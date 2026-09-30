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

#include "python_environment.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include "config.h"

namespace {

constexpr const char* VERIFY_MARKER_PYQINT = "PYQINT_VERSION=";
constexpr const char* VERIFY_MARKER_PYDFT = "PYDFT_VERSION=";
constexpr const char* VERIFY_MARKER_PYDFT_ERROR = "PYDFT_ERROR=";
constexpr const char* VERIFY_MARKER_PYTHON = "PYTHON_VERSION=";
constexpr const char* VERIFY_DESCRIPTION = "Verifying installation";

#ifdef Q_OS_WIN
constexpr const char* UV_BINARY = "uv.exe";
#else
constexpr const char* UV_BINARY = "uv";
#endif

} // namespace

PythonEnvironment::PythonEnvironment(QObject* parent) :
    QObject(parent) {}

QString PythonEnvironment::root_directory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("python");
}

QString PythonEnvironment::env_directory() {
    return QDir(root_directory()).filePath("venv");
}

QString PythonEnvironment::python_executable() const {
    const QString override = interpreter_override();
    if(!override.isEmpty()) {
        return override;
    }

#ifdef Q_OS_WIN
    return QDir(env_directory()).filePath("Scripts/python.exe");
#else
    return QDir(env_directory()).filePath("bin/python");
#endif
}

QString PythonEnvironment::uv_executable() {
    // explicit setting takes precedence
    QSettings settings;
    const QString configured = settings.value("python/uv_path").toString();
    if(!configured.isEmpty() && QFileInfo(configured).isExecutable()) {
        return configured;
    }

    // shipped next to the executable (Windows installer, Linux builds) or
    // inside the application bundle (macOS)
    const QDir appdir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        appdir.filePath(UV_BINARY),
        appdir.filePath(QString("uv/") + UV_BINARY),
        appdir.filePath(QString("../Resources/") + UV_BINARY),
    };
    for(const auto& c : candidates) {
        if(QFileInfo(c).isExecutable()) {
            return QFileInfo(c).canonicalFilePath();
        }
    }

    // fall back to a uv on the PATH (developer setups)
    return QStandardPaths::findExecutable("uv");
}

QString PythonEnvironment::interpreter_override() {
    QSettings settings;
    return settings.value("python/interpreter_override").toString();
}

void PythonEnvironment::set_interpreter_override(const QString& path) {
    QSettings settings;
    if(path.isEmpty()) {
        settings.remove("python/interpreter_override");
    } else {
        settings.setValue("python/interpreter_override", path);
    }
}

QProcessEnvironment PythonEnvironment::process_environment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // isolate from user site-packages and any activated environments
    env.insert("PYTHONNOUSERSITE", "1");
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("PYTHONUTF8", "1");
    env.remove("PYTHONHOME");
    env.remove("PYTHONPATH");
    env.remove("VIRTUAL_ENV");
    env.remove("CONDA_PREFIX");

    // matplotlib may be imported by PyQInt; never try to open a window
    env.insert("MPLBACKEND", "Agg");

    return env;
}

QProcessEnvironment PythonEnvironment::uv_environment() const {
    QProcessEnvironment env = this->process_environment();
    const QDir root(root_directory());
    env.insert("UV_PYTHON_INSTALL_DIR", root.filePath("interpreters"));
    env.insert("UV_CACHE_DIR", root.filePath("cache"));
    env.insert("UV_NO_CONFIG", "1");
    env.insert("UV_LINK_MODE", "copy");
    env.insert("UV_NO_PROGRESS", "1");
    env.remove("UV_PYTHON");
    env.remove("UV_INDEX_URL");
    return env;
}

QString PythonEnvironment::state_label(State s) {
    switch(s) {
        case State::Unknown:    return "Not checked";
        case State::Checking:   return "Checking...";
        case State::Missing:    return "Not installed";
        case State::Installing: return "Installing...";
        case State::Ready:      return "Ready";
        case State::Broken:     return "Broken";
    }
    return QString();
}

void PythonEnvironment::set_state(State s) {
    if(this->state != s) {
        this->state = s;
        emit state_changed(s);
    }
}

QStringList PythonEnvironment::verify_arguments() const {
    // PyQInt is required; PyDFT is optional, hence imported inside a
    // try-block (written with escape sequences to keep the command on one line)
    return {
        "-c",
        QString("import sys, numpy, scipy, pyqint; "
                "print('%1' + pyqint.__version__); "
                "exec('try:\\n import pydft\\n print(\\'%2\\' + pydft.__version__)\\n"
                "except Exception as e:\\n print(\\'%3\\' + repr(e))'); "
                "print('%4' + sys.version.split()[0])")
            .arg(VERIFY_MARKER_PYQINT, VERIFY_MARKER_PYDFT, VERIFY_MARKER_PYDFT_ERROR, VERIFY_MARKER_PYTHON)
    };
}

QStringList PythonEnvironment::pinned_packages() {
    return {QString("pyqint==%1").arg(PYQINT_PINNED_VERSION), QString("pydft==%1").arg(PYDFT_PINNED_VERSION)};
}

QString PythonEnvironment::programs_label() const {
    QString result = QString("PyQInt %1").arg(this->pyqint_version);
    if(!this->pydft_version.isEmpty()) {
        result += QString(" · PyDFT %1").arg(this->pydft_version);
    }
    return result;
}

void PythonEnvironment::parse_verify_output(const QString& text) {
    for(const QString& line : text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts)) {
        if(line.startsWith(VERIFY_MARKER_PYQINT)) {
            this->pyqint_version = line.mid(QString(VERIFY_MARKER_PYQINT).size()).trimmed();
        } else if(line.startsWith(VERIFY_MARKER_PYDFT)) {
            this->pydft_version = line.mid(QString(VERIFY_MARKER_PYDFT).size()).trimmed();
        } else if(line.startsWith(VERIFY_MARKER_PYDFT_ERROR)) {
            this->pydft_error = line.mid(QString(VERIFY_MARKER_PYDFT_ERROR).size()).trimmed();
        } else if(line.startsWith(VERIFY_MARKER_PYTHON)) {
            this->python_version = line.mid(QString(VERIFY_MARKER_PYTHON).size()).trimmed();
        }
    }
}

void PythonEnvironment::check() {
    if(this->is_busy()) {
        return;
    }

    this->pyqint_version.clear();
    this->pydft_version.clear();
    this->pydft_error.clear();
    this->python_version.clear();

    const QString python = this->python_executable();
    if(!QFileInfo(python).isExecutable()) {
        this->last_error = interpreter_override().isEmpty() ?
            "The Python environment has not been installed yet." :
            QString("The Python interpreter %1 does not exist.").arg(python);
        this->set_state(interpreter_override().isEmpty() ? State::Missing : State::Broken);
        emit finished(false, this->last_error);
        return;
    }

    this->set_state(State::Checking);
    this->run_steps({{VERIFY_DESCRIPTION, python, this->verify_arguments()}});
}

void PythonEnvironment::install() {
    if(this->is_busy()) {
        return;
    }

    if(!interpreter_override().isEmpty()) {
        this->last_error = "An interpreter override is active; remove it in the settings to use the managed environment.";
        emit finished(false, this->last_error);
        return;
    }

    const QString uv = uv_executable();
    if(uv.isEmpty()) {
        this->last_error = QString("The uv executable, used to install Python, PyQInt and PyDFT, was not found "
                                   "(looked in %1 and on the PATH).")
                               .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));
        this->set_state(State::Missing);
        emit finished(false, this->last_error);
        return;
    }

    QDir().mkpath(root_directory());

    this->set_state(State::Installing);
    this->run_steps({
        {QString("Downloading Python %1").arg(PYTHON_MANAGED_VERSION), uv,
         {"python", "install", PYTHON_MANAGED_VERSION}},
        {"Creating virtual environment", uv,
         {"venv", "--python", PYTHON_MANAGED_VERSION, "--python-preference", "only-managed",
          "--allow-existing", env_directory()}},
        {QString("Installing PyQInt %1 and PyDFT %2").arg(PYQINT_PINNED_VERSION, PYDFT_PINNED_VERSION), uv,
         QStringList{"pip", "install", "--python", this->python_executable()} + pinned_packages()},
        {VERIFY_DESCRIPTION, this->python_executable(), this->verify_arguments()},
    });
}

void PythonEnvironment::update_packages(bool latest) {
    if(this->is_busy()) {
        return;
    }

    const QString uv = uv_executable();
    if(uv.isEmpty() || !interpreter_override().isEmpty()) {
        this->last_error = uv.isEmpty() ? "Cannot find the uv executable." :
                                          "Updating is not possible while an interpreter override is active.";
        emit finished(false, this->last_error);
        return;
    }

    if(!QFileInfo(this->python_executable()).exists()) {
        this->install();
        return;
    }

    QStringList args = {"pip", "install", "--python", this->python_executable()};
    if(latest) {
        args << "--upgrade" << "pyqint" << "pydft";
    } else {
        args << pinned_packages();
    }

    this->set_state(State::Installing);
    this->run_steps({
        {latest ? QString("Upgrading PyQInt and PyDFT to the latest versions") :
                  QString("Installing PyQInt %1 and PyDFT %2").arg(PYQINT_PINNED_VERSION, PYDFT_PINNED_VERSION), uv, args},
        {VERIFY_DESCRIPTION, this->python_executable(), this->verify_arguments()},
    });
}

void PythonEnvironment::reset() {
    if(this->is_busy()) {
        return;
    }

    emit output(QString("Removing %1\n").arg(env_directory()));
    QDir env(env_directory());
    if(env.exists() && !env.removeRecursively()) {
        this->last_error = QString("Could not remove %1. Is a calculation still running?").arg(env_directory());
        this->set_state(State::Broken);
        emit finished(false, this->last_error);
        return;
    }

    this->set_state(State::Missing);
    this->install();
}

void PythonEnvironment::cancel() {
    if(this->process != nullptr) {
        this->steps.clear();
        this->process->kill();
    }
}

void PythonEnvironment::run_steps(const QList<Step>& _steps) {
    this->steps = _steps;
    this->current_step = 0;
    this->run_next_step();
}

void PythonEnvironment::run_next_step() {
    if(this->current_step >= this->steps.size()) {
        return;
    }

    const Step& step = this->steps[this->current_step];
    emit step_started(step.description, this->current_step + 1, this->steps.size());
    emit output(QString("> %1 %2\n").arg(QDir::toNativeSeparators(step.program), step.arguments.join(' ')));

    this->step_output.clear();

    this->process = new QProcess(this);
    this->process->setProcessEnvironment(this->uv_environment());
    this->process->setProcessChannelMode(QProcess::MergedChannels);
    this->process->setWorkingDirectory(QDir::homePath());
    connect(this->process, &QProcess::readyReadStandardOutput, this, &PythonEnvironment::on_process_output);
    connect(this->process, &QProcess::finished, this, &PythonEnvironment::on_process_finished);
    connect(this->process, &QProcess::errorOccurred, this, &PythonEnvironment::on_process_error);
    this->process->start(step.program, step.arguments);
}

void PythonEnvironment::on_process_output() {
    if(this->process == nullptr) {
        return;
    }
    const QString text = QString::fromUtf8(this->process->readAllStandardOutput());
    this->step_output += text;
    emit output(text);
}

void PythonEnvironment::on_process_error(QProcess::ProcessError error) {
    if(error != QProcess::FailedToStart || this->process == nullptr) {
        return;     // other errors are followed by finished()
    }

    const QString program = this->process->program();
    this->process->deleteLater();
    this->process = nullptr;
    this->steps.clear();

    this->last_error = QString("Could not start %1").arg(QDir::toNativeSeparators(program));
    emit output(this->last_error + "\n");
    this->set_state(State::Broken);
    emit finished(false, this->last_error);
}

void PythonEnvironment::on_process_finished(int exit_code, QProcess::ExitStatus status) {
    if(this->process == nullptr) {
        return;
    }
    this->process->deleteLater();
    this->process = nullptr;

    // cancelled
    if(this->steps.isEmpty()) {
        this->last_error = "Operation cancelled.";
        this->set_state(State::Broken);
        emit finished(false, this->last_error);
        return;
    }

    const Step step = this->steps[this->current_step];
    const bool is_verify = step.description == VERIFY_DESCRIPTION;

    if(status != QProcess::NormalExit || exit_code != 0) {
        this->steps.clear();
        this->last_error = is_verify ?
            "PyQInt could not be imported in the Python environment." :
            QString("Step '%1' failed (exit code %2).").arg(step.description).arg(exit_code);
        emit output(this->last_error + "\n");
        this->set_state(State::Broken);
        emit finished(false, this->last_error);
        return;
    }

    if(is_verify) {
        this->parse_verify_output(this->step_output);
    }

    this->current_step++;
    if(this->current_step < this->steps.size()) {
        this->run_next_step();
        return;
    }

    // all steps completed
    this->steps.clear();
    this->last_error.clear();
    this->set_state(State::Ready);
    QString msg = QString("%1 ready (Python %2).").arg(this->programs_label(), this->python_version);
    if(this->pydft_version.isEmpty()) {
        msg += " PyDFT is not installed; DFT calculations are not available.";
    }
    emit finished(true, msg);
}
