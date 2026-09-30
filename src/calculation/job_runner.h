/**************************************************************************
 *   This file is part of PRAXIS.                                         *
 *                                                                        *
 *   Author: Ivo Filot <ivo@ivofilot.nl>                                  *
 *                                                                        *
 *   PRAXIS is free software:                                             *
 *   you can redistribute it and/or modify it under the terms of the      *
 *   GNU General Public License as published by the Free Software         *
 *   Foundation, either version 3 of the License, or (at your option)     *
 *   any later version.                                                   *
 *                                                                        *
 *   PRAXIS is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>

#include "job_spec.h"

class PythonEnvironment;

/**
 * @brief Runs a single PyQInt or PyDFT job in a separate Python process
 *
 * Each job gets its own directory containing the generated job.py script,
 * the helper module, the molecule and, after completion, result.json and
 * the full output log.
 */
class JobRunner : public QObject {
    Q_OBJECT

private:
    PythonEnvironment* environment;
    QProcess* process = nullptr;
    QString job_dir;
    QString result_file;
    QString log_filename;
    QString stdout_buffer;
    QString log;
    QElapsedTimer timer;
    bool cancelled = false;
    int optimization_step_nr = 0;

public:
    explicit JobRunner(PythonEnvironment* environment, QObject* parent = nullptr);

    /**
     * @brief Default folder in which job directories are created
     */
    static QString default_jobs_directory();

    /**
     * @brief Folder in which job directories are created (configurable)
     */
    static QString jobs_directory();

    /**
     * @brief Start a job
     *
     * @return empty string on success, otherwise the reason for not starting
     */
    QString start(const JobSpec& spec);

    /**
     * @brief Add Foster-Boys localized orbitals to an existing (restricted)
     *        result without repeating the Hartree-Fock calculation
     *
     * Writes localize.py next to the result file and runs it; the result
     * file is updated in place and the output goes to localize.log.
     *
     * @return empty string on success, otherwise the reason for not starting
     */
    QString start_localization(const QString& result_file, int seed = 42, int runners = 1);

    inline bool is_running() const {
        return this->process != nullptr;
    }

    inline const QString& get_job_directory() const {
        return this->job_dir;
    }

public slots:
    void cancel();

signals:
    void started(const QString& job_dir);
    void output(const QString& text);
    void stage_changed(const QString& stage);
    void scf_iteration(int iteration, double energy);
    void optimization_step(int step, double energy);
    void finished(bool success, const QString& message, const QString& result_file);

private:
    QString check_can_start() const;
    void launch(const QString& script);
    void process_line(const QString& line);
    void write_log();

private slots:
    void on_stdout();
    void on_stderr();
    void on_finished(int exit_code, QProcess::ExitStatus status);
    void on_error(QProcess::ProcessError error);
};
