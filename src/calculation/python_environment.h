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

#pragma once

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

/**
 * @brief Manages the private Python environment in which PyQInt runs
 *
 * The environment is created with uv (https://docs.astral.sh/uv/), which is
 * shipped alongside the executable. uv downloads a standalone Python build
 * and installs the pinned PyQInt version from PyPI into a virtual
 * environment in the user's application data folder. Nothing is installed
 * system-wide and the user's own Python installation (if any) is ignored.
 *
 * Developers can bypass the managed environment by setting an interpreter
 * override in the settings dialog.
 */
class PythonEnvironment : public QObject {
    Q_OBJECT

public:
    enum class State {
        Unknown,        // not yet checked
        Checking,       // check in progress
        Missing,        // no environment present
        Installing,     // install / update in progress
        Ready,          // PyQInt can be imported
        Broken,         // environment present but PyQInt cannot be imported
    };
    Q_ENUM(State)

private:
    struct Step {
        QString description;
        QString program;
        QStringList arguments;
    };

    State state = State::Unknown;
    QString pyqint_version;
    QString python_version;
    QString last_error;

    QProcess* process = nullptr;
    QList<Step> steps;
    int current_step = 0;
    QString step_output;

public:
    explicit PythonEnvironment(QObject* parent = nullptr);

    /**
     * @brief Folder holding everything managed by this class
     */
    static QString root_directory();

    /**
     * @brief Folder of the virtual environment
     */
    static QString env_directory();

    /**
     * @brief Interpreter used to run jobs (override or managed environment)
     */
    QString python_executable() const;

    /**
     * @brief Location of the uv executable (empty when not found)
     */
    static QString uv_executable();

    /**
     * @brief Interpreter override set by the user (empty when none)
     */
    static QString interpreter_override();
    static void set_interpreter_override(const QString& path);

    /**
     * @brief Environment variables for processes run in this environment
     */
    QProcessEnvironment process_environment() const;

    inline State get_state() const {
        return this->state;
    }

    inline bool is_ready() const {
        return this->state == State::Ready;
    }

    inline bool is_busy() const {
        return this->state == State::Checking || this->state == State::Installing;
    }

    inline const QString& get_pyqint_version() const {
        return this->pyqint_version;
    }

    inline const QString& get_python_version() const {
        return this->python_version;
    }

    inline const QString& get_last_error() const {
        return this->last_error;
    }

    static QString state_label(State s);

public slots:
    /**
     * @brief Verify that PyQInt can be imported (asynchronous)
     */
    void check();

    /**
     * @brief Create the environment and install the pinned PyQInt version
     */
    void install();

    /**
     * @brief Install the pinned PyQInt version or upgrade to the latest one
     */
    void update_pyqint(bool latest);

    /**
     * @brief Remove the environment completely and reinstall it
     */
    void reset();

    /**
     * @brief Abort a running install / update
     */
    void cancel();

signals:
    void state_changed(PythonEnvironment::State state);
    void output(const QString& text);
    void step_started(const QString& description, int step, int nsteps);
    void finished(bool success, const QString& message);

private:
    void set_state(State s);
    void run_steps(const QList<Step>& steps);
    void run_next_step();
    QStringList verify_arguments() const;
    void parse_verify_output(const QString& text);
    QProcessEnvironment uv_environment() const;

private slots:
    void on_process_output();
    void on_process_finished(int exit_code, QProcess::ExitStatus status);
    void on_process_error(QProcess::ProcessError error);
};
