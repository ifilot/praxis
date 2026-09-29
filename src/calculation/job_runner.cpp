/**************************************************************************
 *   This file is part of PYQINT-GUI.                                     *
 *                                                                        *
 *   Author: Ivo Filot <ivo@ivofilot.nl>                                  *
 *                                                                        *
 *   PYQINT-GUI is free software:                                         *
 *   you can redistribute it and/or modify it under the terms of the      *
 *   GNU General Public License as published by the Free Software         *
 *   Foundation, either version 3 of the License, or (at your option)     *
 *   any later version.                                                   *
 *                                                                        *
 *   PYQINT-GUI is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "job_runner.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include "job_script_writer.h"
#include "python_environment.h"

namespace {

const QString PROGRESS_TAG = "@@PYQINT-GUI ";

const QRegularExpression RE_SCF_ITERATION(R"(Iteration:\s*(\d+)\s*\|\s*Energy:\s*([-+0-9.eE]+))");
const QRegularExpression RE_GEOMOPT_STEP(R"(GEOMETRY OPTIMIZATION STEP\s+(\d+))");
const QRegularExpression RE_GEOMOPT_TOTAL(R"(^\s*TOTAL:\s*([-+0-9.eE]+))");

QString sanitize(const QString& name) {
    QString result = name.simplified().toLower();
    result.replace(QRegularExpression("[^a-z0-9_-]+"), "-");
    result.remove(QRegularExpression("^-+|-+$"));
    return result.isEmpty() ? "molecule" : result.left(40);
}

} // namespace

JobRunner::JobRunner(PythonEnvironment* _environment, QObject* parent) :
    QObject(parent),
    environment(_environment) {}

QString JobRunner::default_jobs_directory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("jobs");
}

QString JobRunner::jobs_directory() {
    QSettings settings;
    return settings.value("jobs/directory", default_jobs_directory()).toString();
}

QString JobRunner::start(const JobSpec& spec) {
    if(this->is_running()) {
        return "Another calculation is still running.";
    }

    if(!this->environment->is_ready()) {
        return "The Python environment is not ready. Open Python → Manage environment to install it.";
    }

    const QString invalid = spec.validate();
    if(!invalid.isEmpty()) {
        return invalid;
    }

    // create a unique job directory
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QString dirname = QString("%1-%2").arg(stamp, sanitize(spec.molecule.get_name()));
    QDir root(jobs_directory());
    int suffix = 1;
    while(root.exists(dirname)) {
        dirname = QString("%1-%2-%3").arg(stamp, sanitize(spec.molecule.get_name())).arg(++suffix);
    }
    this->job_dir = root.filePath(dirname);

    const QString err = JobScriptWriter::write_job_directory(spec, this->job_dir);
    if(!err.isEmpty()) {
        return err;
    }

    this->stdout_buffer.clear();
    this->log.clear();
    this->cancelled = false;
    this->optimization_step_nr = 0;

    this->process = new QProcess(this);
    this->process->setWorkingDirectory(this->job_dir);
    this->process->setProcessEnvironment(this->environment->process_environment());
    connect(this->process, &QProcess::readyReadStandardOutput, this, &JobRunner::on_stdout);
    connect(this->process, &QProcess::readyReadStandardError, this, &JobRunner::on_stderr);
    connect(this->process, &QProcess::finished, this, &JobRunner::on_finished);
    connect(this->process, &QProcess::errorOccurred, this, &JobRunner::on_error);

    this->timer.start();
    this->process->start(this->environment->python_executable(),
                         {"-u", JobScriptWriter::SCRIPT_FILENAME});

    emit started(this->job_dir);
    return QString();
}

void JobRunner::cancel() {
    if(this->process != nullptr) {
        this->cancelled = true;
        this->process->kill();
    }
}

void JobRunner::process_line(const QString& line) {
    if(line.startsWith(PROGRESS_TAG)) {
        const QJsonObject msg = QJsonDocument::fromJson(line.mid(PROGRESS_TAG.size()).toUtf8()).object();
        emit stage_changed(msg["stage"].toString());
        return;     // do not clutter the log with progress markers
    }

    auto m = RE_SCF_ITERATION.match(line);
    if(m.hasMatch()) {
        emit scf_iteration(m.captured(1).toInt(), m.captured(2).toDouble());
    }

    m = RE_GEOMOPT_STEP.match(line);
    if(m.hasMatch()) {
        this->optimization_step_nr = m.captured(1).toInt();
    }

    m = RE_GEOMOPT_TOTAL.match(line);
    if(m.hasMatch() && this->optimization_step_nr > 0) {
        emit optimization_step(this->optimization_step_nr, m.captured(1).toDouble());
    }

    const QString text = line + "\n";
    this->log += text;
    emit output(text);
}

void JobRunner::on_stdout() {
    if(this->process == nullptr) {
        return;
    }

    this->stdout_buffer += QString::fromUtf8(this->process->readAllStandardOutput());
    this->stdout_buffer.remove('\r');

    int pos;
    while((pos = this->stdout_buffer.indexOf('\n')) >= 0) {
        this->process_line(this->stdout_buffer.left(pos));
        this->stdout_buffer.remove(0, pos + 1);
    }
}

void JobRunner::on_stderr() {
    if(this->process == nullptr) {
        return;
    }
    const QString text = QString::fromUtf8(this->process->readAllStandardError());
    this->log += text;
    emit output(text);
}

void JobRunner::write_log() {
    QFile f(QDir(this->job_dir).filePath("output.log"));
    if(f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(this->log.toUtf8());
    }
}

void JobRunner::on_error(QProcess::ProcessError error) {
    if(error != QProcess::FailedToStart || this->process == nullptr) {
        return;
    }

    const QString python = this->process->program();
    this->process->deleteLater();
    this->process = nullptr;

    const QString msg = QString("Could not start the Python interpreter %1").arg(QDir::toNativeSeparators(python));
    this->log += msg + "\n";
    this->write_log();
    emit finished(false, msg, QString());
}

void JobRunner::on_finished(int exit_code, QProcess::ExitStatus status) {
    if(this->process == nullptr) {
        return;
    }

    // flush remaining output
    this->on_stdout();
    if(!this->stdout_buffer.isEmpty()) {
        this->process_line(this->stdout_buffer);
        this->stdout_buffer.clear();
    }
    this->on_stderr();

    this->process->deleteLater();
    this->process = nullptr;

    const double elapsed = this->timer.elapsed() / 1000.0;
    const QString result_file = QDir(this->job_dir).filePath(JobScriptWriter::RESULT_FILENAME);

    QString msg;
    bool success = false;
    if(this->cancelled) {
        msg = "Calculation cancelled.";
    } else if(status != QProcess::NormalExit) {
        msg = "The Python process crashed.";
    } else if(exit_code != 0) {
        msg = QString("The calculation failed (exit code %1); see the output log for details.").arg(exit_code);
    } else if(!QFileInfo::exists(result_file)) {
        msg = "The calculation finished but did not produce a result file.";
    } else {
        success = true;
        msg = QString("Calculation finished in %1 s.").arg(elapsed, 0, 'f', 1);
    }

    this->log += "\n" + msg + "\n";
    this->write_log();

    emit finished(success, msg, success ? result_file : QString());
}
