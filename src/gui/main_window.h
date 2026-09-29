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

#include <QMainWindow>

#include "calculation/job_spec.h"

class AnaglyphWidget;
class CalculationPanel;
class EnvironmentDialog;
class JobRunner;
class PythonEnvironment;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class ResultsPanel;
class ViewerController;

class MainWindow : public QMainWindow {
    Q_OBJECT

private:
    PythonEnvironment* environment;
    JobRunner* runner;

    AnaglyphWidget* viewer;
    ViewerController* viewer_controller;
    CalculationPanel* calculation_panel;
    ResultsPanel* results_panel;
    QPlainTextEdit* output_log;

    QPushButton* status_environment;
    QLabel* status_busy;
    QProgressBar* status_progress;

    EnvironmentDialog* environment_dialog = nullptr;
    bool first_check_done = false;

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Open a file given on the command line (.xyz or result .json)
     */
    void open_file(const QString& path);

    /**
     * @brief Bring a tab of the results panel to the front
     */
    void show_results_tab(const QString& name);

protected:
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

private:
    void build_menu();
    void build_docks();
    void build_statusbar();

    void load_molecule(const Molecule& mol);
    void load_result(const QString& filename, const QString& job_dir);
    void append_log(const QString& text);
    void set_stereo(const QString& name);

private slots:
    void open_library();
    void open_xyz();
    void open_result();
    void save_xyz();
    void save_image();
    void open_jobs_folder();
    void show_environment_dialog();
    void show_about();

    void on_environment_state_changed();
    void on_environment_finished(bool success, const QString& message);

    void run_job(const JobSpec& spec);
    void on_job_started(const QString& job_dir);
    void on_job_stage(const QString& stage);
    void on_job_finished(bool success, const QString& message, const QString& result_file);

    void on_viewer_busy(bool busy, const QString& message);
};
