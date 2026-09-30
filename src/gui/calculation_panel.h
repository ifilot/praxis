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

#include <QJsonObject>
#include <QWidget>

#include "calculation/job_result.h"
#include "calculation/job_spec.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QSpinBox;
class LinePlotWidget;

/**
 * @brief Set up and launch a calculation
 */
class CalculationPanel : public QWidget {
    Q_OBJECT

private:
    Molecule molecule;
    bool running = false;
    bool environment_ready = false;
    bool pydft_available = false;

    QLabel* label_molecule;
    QPushButton* button_library;
    QPushButton* button_open;

    QFormLayout* form_calc;
    QComboBox* combo_theory;
    QComboBox* combo_type;
    QComboBox* combo_method;
    QComboBox* combo_functional;
    QComboBox* combo_basis;
    QSpinBox* spin_charge;
    QSpinBox* spin_multiplicity;
    QCheckBox* check_fosterboys;

    QPushButton* button_advanced;
    QGroupBox* group_advanced;
    QFormLayout* form_advanced;
    QSpinBox* spin_itermax;
    QComboBox* combo_tolerance;
    QCheckBox* check_diis;
    QComboBox* combo_ortho;
    QComboBox* combo_gtol;
    QComboBox* combo_grid;
    QLabel* label_dft_note;

    QLabel* label_validation;
    QPushButton* button_run;
    QPushButton* button_cancel;
    QLabel* label_status;
    LinePlotWidget* plot_scf;

public:
    explicit CalculationPanel(QWidget* parent = nullptr);

    JobSpec get_spec() const;

    inline const Molecule& get_molecule() const {
        return this->molecule;
    }

public slots:
    void set_molecule(const Molecule& molecule);

    /**
     * @brief Restore the settings of an earlier job (as stored in result.json)
     */
    void load_settings(const JobResult& result);
    void set_running(bool running);
    void set_environment_ready(bool ready, bool pydft_available);
    void set_status(const QString& text);
    void on_scf_iteration(int iteration, double energy);
    void on_optimization_step(int step, double energy);

signals:
    void run_requested(const JobSpec& spec);
    void cancel_requested();
    void library_requested();
    void open_requested();

private:
    void update_molecule_label();
    void update_state();
    void fix_spin_state();
    bool is_dft() const;
};
