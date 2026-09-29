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

#include <QDialog>

#include "calculation/python_environment.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

/**
 * @brief Install, update, repair or override the Python environment
 */
class EnvironmentDialog : public QDialog {
    Q_OBJECT

private:
    PythonEnvironment* environment;

    QLabel* label_state;
    QLabel* label_pyqint;
    QLabel* label_python;
    QLabel* label_location;
    QLabel* label_step;
    QProgressBar* progress;
    QPlainTextEdit* log;

    QPushButton* button_install;
    QPushButton* button_update;
    QPushButton* button_pinned;
    QPushButton* button_reset;
    QPushButton* button_cancel;
    QPushButton* button_close;

    QLineEdit* edit_override;
    QPushButton* button_override_browse;
    QPushButton* button_override_clear;

public:
    explicit EnvironmentDialog(PythonEnvironment* environment, QWidget* parent = nullptr);

    /**
     * @brief Start installing straight away (first-run experience)
     */
    void start_install();

private:
    void update_state();
    void apply_override(const QString& path);

private slots:
    void on_output(const QString& text);
    void on_step(const QString& description, int step, int nsteps);
    void on_finished(bool success, const QString& message);
};
