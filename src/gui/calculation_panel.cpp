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

#include "calculation_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "line_plot_widget.h"

namespace {

void set_item_enabled(QComboBox* combo, int index, bool enabled) {
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    if(model != nullptr) {
        auto* item = model->item(index);
        if(item != nullptr) {
            item->setEnabled(enabled);
        }
    }
}

} // namespace

CalculationPanel::CalculationPanel(QWidget* parent) :
    QWidget(parent) {

    auto* layout = new QVBoxLayout(this);

    // ------------------------------------------------------------------
    // molecule
    // ------------------------------------------------------------------
    auto* group_mol = new QGroupBox("Molecule");
    auto* lmol = new QVBoxLayout(group_mol);
    this->label_molecule = new QLabel;
    this->label_molecule->setWordWrap(true);
    this->label_molecule->setTextFormat(Qt::RichText);
    lmol->addWidget(this->label_molecule);
    auto* lmolbuttons = new QHBoxLayout;
    this->button_library = new QPushButton("Library...");
    this->button_library->setToolTip("Choose one of the molecules shipped with the program");
    this->button_open = new QPushButton("Open .xyz...");
    this->button_open->setToolTip("Load a molecule from an .xyz file (coordinates in angstrom)");
    lmolbuttons->addWidget(this->button_library);
    lmolbuttons->addWidget(this->button_open);
    lmol->addLayout(lmolbuttons);
    layout->addWidget(group_mol);

    // ------------------------------------------------------------------
    // calculation settings
    // ------------------------------------------------------------------
    auto* group_calc = new QGroupBox("Calculation");
    auto* form = new QFormLayout(group_calc);

    this->combo_type = new QComboBox;
    this->combo_type->addItem("Single point", (int)JobType::SinglePoint);
    this->combo_type->addItem("Geometry optimization", (int)JobType::GeometryOptimization);
    this->combo_type->setToolTip("Single point: solve the Hartree-Fock equations for the given geometry.\n"
                                 "Geometry optimization: find the geometry with the lowest energy.");
    form->addRow("Type:", this->combo_type);

    this->combo_method = new QComboBox;
    this->combo_method->addItem("Restricted HF (RHF)", (int)HFMethod::Restricted);
    this->combo_method->addItem("Unrestricted HF (UHF)", (int)HFMethod::Unrestricted);
    this->combo_method->setToolTip("RHF: all electrons are paired (closed-shell molecules).\n"
                                   "UHF: separate orbitals for spin-up and spin-down electrons (open-shell).");
    form->addRow("Method:", this->combo_method);

    this->combo_basis = new QComboBox;
    for(const QString& b : JobSpec::available_basis_sets()) {
        this->combo_basis->addItem(JobSpec::basis_set_label(b), b);
    }
    this->combo_basis->setToolTip("Larger basis sets are more accurate but take (much) longer.");
    form->addRow("Basis set:", this->combo_basis);

    this->spin_charge = new QSpinBox;
    this->spin_charge->setRange(-10, 10);
    form->addRow("Charge:", this->spin_charge);

    this->spin_multiplicity = new QSpinBox;
    this->spin_multiplicity->setRange(1, 11);
    this->spin_multiplicity->setToolTip("Spin multiplicity 2S+1: 1 = singlet, 2 = doublet, 3 = triplet, ...");
    form->addRow("Multiplicity:", this->spin_multiplicity);

    this->check_fosterboys = new QCheckBox("Foster-Boys localization");
    this->check_fosterboys->setToolTip("Also construct localized molecular orbitals from the occupied\n"
                                       "canonical orbitals (restricted Hartree-Fock only).");
    form->addRow(this->check_fosterboys);
    layout->addWidget(group_calc);

    // ------------------------------------------------------------------
    // advanced settings
    // ------------------------------------------------------------------
    this->button_advanced = new QPushButton("Advanced settings ▸");
    this->button_advanced->setCheckable(true);
    this->button_advanced->setFlat(true);
    this->button_advanced->setStyleSheet("text-align: left;");
    layout->addWidget(this->button_advanced);

    this->group_advanced = new QGroupBox;
    auto* adv = new QFormLayout(this->group_advanced);
    this->spin_itermax = new QSpinBox;
    this->spin_itermax->setRange(5, 1000);
    this->spin_itermax->setValue(100);
    adv->addRow("Max. SCF iterations:", this->spin_itermax);

    this->combo_tolerance = new QComboBox;
    for(double t : {1e-6, 1e-7, 1e-8, 1e-9, 1e-10, 1e-12}) {
        this->combo_tolerance->addItem(QString::number(t, 'g', 1), t);
    }
    this->combo_tolerance->setCurrentIndex(3);
    this->combo_tolerance->setToolTip("SCF stops when the energy changes less than this (Ht) between iterations");
    adv->addRow("Energy tolerance (Ht):", this->combo_tolerance);

    this->check_diis = new QCheckBox("Use DIIS acceleration");
    this->check_diis->setChecked(true);
    adv->addRow(this->check_diis);

    this->combo_ortho = new QComboBox;
    this->combo_ortho->addItem("Canonical", "canonical");
    this->combo_ortho->addItem("Symmetric", "symmetric");
    adv->addRow("Orthogonalization:", this->combo_ortho);

    this->combo_gtol = new QComboBox;
    for(double t : {1e-3, 1e-4, 1e-5, 1e-6}) {
        this->combo_gtol->addItem(QString::number(t, 'g', 1), t);
    }
    this->combo_gtol->setCurrentIndex(2);
    this->combo_gtol->setToolTip("Geometry optimization stops when the gradient is smaller than this (Ht/bohr)");
    adv->addRow("Gradient tolerance:", this->combo_gtol);
    this->group_advanced->setVisible(false);
    layout->addWidget(this->group_advanced);

    // ------------------------------------------------------------------
    // run
    // ------------------------------------------------------------------
    this->label_validation = new QLabel;
    this->label_validation->setWordWrap(true);
    this->label_validation->setStyleSheet("color: #c62828;");
    layout->addWidget(this->label_validation);

    auto* lrun = new QHBoxLayout;
    this->button_run = new QPushButton("Run calculation");
    this->button_run->setDefault(true);
    QFont bold = this->button_run->font();
    bold.setBold(true);
    this->button_run->setFont(bold);
    this->button_run->setMinimumHeight(32);
    this->button_cancel = new QPushButton("Cancel");
    this->button_cancel->setMinimumHeight(32);
    lrun->addWidget(this->button_run, 1);
    lrun->addWidget(this->button_cancel);
    layout->addLayout(lrun);

    this->label_status = new QLabel;
    this->label_status->setWordWrap(true);
    layout->addWidget(this->label_status);

    this->plot_scf = new LinePlotWidget;
    this->plot_scf->set_labels("SCF convergence", "Iteration", "Energy (Ht)");
    this->plot_scf->setMinimumHeight(160);
    layout->addWidget(this->plot_scf, 1);

    // ------------------------------------------------------------------
    // connections
    // ------------------------------------------------------------------
    connect(this->button_library, &QPushButton::clicked, this, &CalculationPanel::library_requested);
    connect(this->button_open, &QPushButton::clicked, this, &CalculationPanel::open_requested);
    connect(this->button_run, &QPushButton::clicked, this, [this]() {
        emit run_requested(this->get_spec());
    });
    connect(this->button_cancel, &QPushButton::clicked, this, &CalculationPanel::cancel_requested);
    connect(this->button_advanced, &QPushButton::toggled, this, [this](bool on) {
        this->group_advanced->setVisible(on);
        this->button_advanced->setText(on ? "Advanced settings ▾" : "Advanced settings ▸");
    });

    connect(this->spin_charge, &QSpinBox::valueChanged, this, [this]() {
        this->fix_spin_state();
        this->update_molecule_label();
        this->update_state();
    });
    connect(this->spin_multiplicity, &QSpinBox::valueChanged, this, [this](int mult) {
        // open-shell systems require UHF
        if(mult > 1 && this->combo_method->currentData().toInt() == (int)HFMethod::Restricted) {
            this->combo_method->setCurrentIndex(this->combo_method->findData((int)HFMethod::Unrestricted));
        }
        this->update_state();
    });
    for(auto* combo : {this->combo_type, this->combo_method, this->combo_basis}) {
        connect(combo, &QComboBox::currentIndexChanged, this, &CalculationPanel::update_state);
    }
    connect(this->check_fosterboys, &QCheckBox::toggled, this, &CalculationPanel::update_state);

    this->update_molecule_label();
    this->update_state();
}

JobSpec CalculationPanel::get_spec() const {
    JobSpec spec;
    spec.molecule = this->molecule;
    spec.type = (JobType)this->combo_type->currentData().toInt();
    spec.method = (HFMethod)this->combo_method->currentData().toInt();
    spec.basis = this->combo_basis->currentData().toString();
    spec.charge = this->spin_charge->value();
    spec.multiplicity = this->spin_multiplicity->value();
    spec.foster_boys = this->check_fosterboys->isChecked() && this->check_fosterboys->isEnabled();
    spec.itermax = this->spin_itermax->value();
    spec.tolerance = this->combo_tolerance->currentData().toDouble();
    spec.use_diis = this->check_diis->isChecked();
    spec.ortho = this->combo_ortho->currentData().toString();
    spec.gtol = this->combo_gtol->currentData().toDouble();
    return spec;
}

void CalculationPanel::set_molecule(const Molecule& mol) {
    this->molecule = mol;
    {
        QSignalBlocker blocker(this->spin_charge);
        this->spin_charge->setValue(0);
    }
    this->fix_spin_state();
    this->update_molecule_label();
    this->update_state();
    this->plot_scf->clear();
    this->label_status->clear();
}

void CalculationPanel::load_settings(const JobResult& result) {
    const QJsonObject& job = result.job;
    auto select = [](QComboBox* combo, const QVariant& data) {
        const int idx = combo->findData(data);
        if(idx >= 0) {
            combo->setCurrentIndex(idx);
        }
    };

    // the result itself is authoritative; the echoed job settings fill in the rest
    select(this->combo_method, (int)(result.is_unrestricted() ? HFMethod::Unrestricted : HFMethod::Restricted));
    select(this->combo_type, (int)(result.optimization ? JobType::GeometryOptimization : JobType::SinglePoint));
    if(job.contains("basis")) {
        select(this->combo_basis, job["basis"].toString());
    }
    {
        QSignalBlocker blocker(this->spin_charge);
        this->spin_charge->setValue(result.charge);
    }
    this->spin_multiplicity->setValue(result.multiplicity);
    this->check_fosterboys->setChecked(job["foster_boys"].toBool(false));
    if(job.contains("itermax")) {
        this->spin_itermax->setValue(job["itermax"].toInt());
    }
    if(job.contains("use_diis")) {
        this->check_diis->setChecked(job["use_diis"].toBool());
    }
    select(this->combo_ortho, job["ortho"].toString("canonical"));

    this->update_molecule_label();
    this->update_state();
}

void CalculationPanel::fix_spin_state() {
    // choose the lowest multiplicity compatible with the number of electrons
    const int ne = this->molecule.nuclear_charge() - this->spin_charge->value();
    const int mult = this->spin_multiplicity->value();
    if((ne - (mult - 1)) % 2 != 0) {
        this->spin_multiplicity->setValue(ne % 2 == 0 ? 1 : 2);
    }
}

void CalculationPanel::update_molecule_label() {
    if(this->molecule.empty()) {
        this->label_molecule->setText("<i>No molecule loaded</i>");
        return;
    }
    const int ne = this->molecule.nuclear_charge() - this->spin_charge->value();
    this->label_molecule->setText(QString("<b>%1</b><br>%2 &middot; %3 atoms &middot; %4 electrons")
        .arg(this->molecule.get_name().toHtmlEscaped(), this->molecule.formula())
        .arg(this->molecule.size()).arg(ne));
}

void CalculationPanel::update_state() {
    const bool restricted = this->combo_method->currentData().toInt() == (int)HFMethod::Restricted;

    // options that only make sense for restricted Hartree-Fock
    this->check_fosterboys->setEnabled(restricted && !this->running);
    set_item_enabled(this->combo_type, 1, restricted);
    if(!restricted && this->combo_type->currentIndex() == 1) {
        this->combo_type->setCurrentIndex(0);
    }

    const bool geomopt = this->combo_type->currentData().toInt() == (int)JobType::GeometryOptimization;
    this->combo_gtol->setEnabled(geomopt);

    const QString problem = this->get_spec().validate();
    this->label_validation->setText(problem);
    this->label_validation->setVisible(!problem.isEmpty() && !this->molecule.empty());

    if(!this->environment_ready && !this->running) {
        this->button_run->setToolTip("The Python environment is not ready yet (see Python → Manage environment)");
    } else {
        this->button_run->setToolTip(QString());
    }

    this->button_run->setEnabled(!this->running && problem.isEmpty() && this->environment_ready);
    this->button_cancel->setEnabled(this->running);

    for(QWidget* w : std::initializer_list<QWidget*>{
            this->button_library, this->button_open, this->combo_type, this->combo_method,
            this->combo_basis, this->spin_charge, this->spin_multiplicity, this->group_advanced}) {
        w->setEnabled(!this->running);
    }
}

void CalculationPanel::set_running(bool _running) {
    this->running = _running;
    if(_running) {
        this->plot_scf->clear();
        this->plot_scf->set_labels("SCF convergence", "Iteration", "Energy (Ht)");
        this->plot_scf->set_x_offset(0);
    }
    this->update_state();
}

void CalculationPanel::set_environment_ready(bool ready) {
    this->environment_ready = ready;
    this->update_state();
}

void CalculationPanel::set_status(const QString& text) {
    this->label_status->setText(text);
}

void CalculationPanel::on_scf_iteration(int iteration, double energy) {
    // a new SCF cycle starts (e.g. during a geometry optimization)
    if(iteration == 0) {
        this->plot_scf->clear();
    }
    this->plot_scf->append(energy);
    this->label_status->setText(QString("SCF iteration %1: E = %2 Ht").arg(iteration).arg(energy, 0, 'f', 8));
}

void CalculationPanel::on_optimization_step(int step, double energy) {
    if(step == 1) {
        this->plot_scf->clear();
        this->plot_scf->set_labels("Geometry optimization", "Step", "Energy (Ht)");
        this->plot_scf->set_x_offset(1);
    }
    this->plot_scf->append(energy);
    this->label_status->setText(QString("Optimization step %1: E = %2 Ht").arg(step).arg(energy, 0, 'f', 8));
}
