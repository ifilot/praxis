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

#include "environment_dialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include "config.h"
#include "icons.h"

EnvironmentDialog::EnvironmentDialog(PythonEnvironment* _environment, QWidget* parent) :
    QDialog(parent),
    environment(_environment) {

    this->setWindowTitle("Python environment");
    this->resize(720, 560);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        "<p>" PROGRAM_NAME " runs its calculations with <b>PyQInt</b> (Hartree-Fock) and "
        "<b>PyDFT</b> (density functional theory) in a private Python environment. This "
        "environment is downloaded and installed automatically; it does not touch any Python "
        "installation you may already have.</p>"
        "<p>Installation requires an internet connection and roughly 300&nbsp;MB of disk space.</p>");
    intro->setWordWrap(true);
    layout->addWidget(intro);

    // status
    auto* status_box = new QGroupBox("Status");
    auto* form = new QFormLayout(status_box);
    this->label_state = new QLabel;
    this->label_pyqint = new QLabel;
    this->label_pydft = new QLabel;
    this->label_pydft->setWordWrap(true);
    this->label_python = new QLabel;
    this->label_location = new QLabel;
    this->label_location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    this->label_location->setWordWrap(true);
    form->addRow("State:", this->label_state);
    form->addRow("PyQInt:", this->label_pyqint);
    form->addRow("PyDFT:", this->label_pydft);
    form->addRow("Python:", this->label_python);
    form->addRow("Location:", this->label_location);
    layout->addWidget(status_box);

    // actions
    auto* actions = new QHBoxLayout;
    this->button_install = new QPushButton(bluecurve_icon("fileimport"), "Install");
    this->button_install->setToolTip("Download Python and install the tested PyQInt and PyDFT versions");
    this->button_pinned = new QPushButton("Use tested versions");
    this->button_pinned->setToolTip(QString("(Re)install the versions this program was tested with "
                                            "(PyQInt %1, PyDFT %2)").arg(PYQINT_PINNED_VERSION, PYDFT_PINNED_VERSION));
    this->button_update = new QPushButton(bluecurve_icon("reload"), "Update to latest");
    this->button_update->setToolTip("Upgrade to the newest PyQInt and PyDFT releases on PyPI "
                                    "(not tested with this version of the GUI)");
    this->button_reset = new QPushButton("Reset environment");
    this->button_reset->setToolTip("Delete the environment completely and install it again");
    actions->addWidget(this->button_install);
    actions->addWidget(this->button_pinned);
    actions->addWidget(this->button_update);
    actions->addWidget(this->button_reset);
    actions->addStretch();
    layout->addLayout(actions);

    // progress
    this->label_step = new QLabel;
    this->label_step->setWordWrap(true);
    this->progress = new QProgressBar;
    this->progress->setTextVisible(false);
    this->progress->setRange(0, 1);
    this->progress->setValue(0);
    layout->addWidget(this->label_step);
    layout->addWidget(this->progress);

    this->log = new QPlainTextEdit;
    this->log->setReadOnly(true);
    this->log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->log->setMaximumBlockCount(5000);
    layout->addWidget(this->log, 1);

    // advanced: interpreter override
    auto* adv = new QGroupBox("Advanced: use your own Python interpreter");
    auto* advl = new QHBoxLayout(adv);
    this->edit_override = new QLineEdit;
    this->edit_override->setPlaceholderText("(managed environment)");
    this->edit_override->setReadOnly(true);
    this->button_override_browse = new QPushButton(bluecurve_icon("folder"), "Browse...");
    this->button_override_clear = new QPushButton("Use managed");
    advl->addWidget(this->edit_override, 1);
    advl->addWidget(this->button_override_browse);
    advl->addWidget(this->button_override_clear);
    adv->setToolTip("For developers: run calculations with an existing interpreter in which PyQInt "
                    "(and optionally PyDFT) is installed.");
    layout->addWidget(adv);

    auto* buttons = new QHBoxLayout;
    this->button_cancel = new QPushButton(bluecurve_icon("stop"), "Cancel operation");
    this->button_close = new QPushButton("Close");
    buttons->addWidget(this->button_cancel);
    buttons->addStretch();
    buttons->addWidget(this->button_close);
    layout->addLayout(buttons);

    connect(this->button_install, &QPushButton::clicked, this, &EnvironmentDialog::start_install);
    connect(this->button_pinned, &QPushButton::clicked, this, [this]() {
        this->log->clear();
        this->environment->update_packages(false);
    });
    connect(this->button_update, &QPushButton::clicked, this, [this]() {
        auto answer = QMessageBox::question(this, "Update PyQInt and PyDFT",
            QString("This installs the newest PyQInt and PyDFT releases, which have not been tested with "
                    "this version of %1. You can always return to the tested versions (PyQInt %2, "
                    "PyDFT %3).\n\nContinue?")
                .arg(PROGRAM_NAME, PYQINT_PINNED_VERSION, PYDFT_PINNED_VERSION));
        if(answer == QMessageBox::Yes) {
            this->log->clear();
            this->environment->update_packages(true);
        }
    });
    connect(this->button_reset, &QPushButton::clicked, this, [this]() {
        auto answer = QMessageBox::question(this, "Reset environment",
            "This deletes the Python environment and installs it again from scratch.\n\nContinue?");
        if(answer == QMessageBox::Yes) {
            this->log->clear();
            this->environment->reset();
        }
    });
    connect(this->button_cancel, &QPushButton::clicked, this->environment, &PythonEnvironment::cancel);
    connect(this->button_close, &QPushButton::clicked, this, &QDialog::accept);

    connect(this->button_override_browse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "Select Python interpreter", QDir::homePath());
        if(!path.isEmpty()) {
            this->apply_override(path);
        }
    });
    connect(this->button_override_clear, &QPushButton::clicked, this, [this]() {
        this->apply_override(QString());
    });

    connect(this->environment, &PythonEnvironment::state_changed, this, &EnvironmentDialog::update_state);
    connect(this->environment, &PythonEnvironment::output, this, &EnvironmentDialog::on_output);
    connect(this->environment, &PythonEnvironment::step_started, this, &EnvironmentDialog::on_step);
    connect(this->environment, &PythonEnvironment::finished, this, &EnvironmentDialog::on_finished);

    this->update_state();
}

void EnvironmentDialog::start_install() {
    this->log->clear();
    this->environment->install();
}

void EnvironmentDialog::apply_override(const QString& path) {
    PythonEnvironment::set_interpreter_override(path);
    this->log->clear();
    this->environment->check();
    this->update_state();
}

void EnvironmentDialog::update_state() {
    using State = PythonEnvironment::State;
    const State state = this->environment->get_state();
    const bool busy = this->environment->is_busy();
    const bool override = !PythonEnvironment::interpreter_override().isEmpty();
    const bool has_uv = !PythonEnvironment::uv_executable().isEmpty();

    QString state_str = PythonEnvironment::state_label(state);
    if(state == State::Ready) {
        state_str = "<span style='color:#2e7d32'><b>Ready</b></span>";
    } else if(state == State::Broken || state == State::Missing) {
        state_str = QString("<span style='color:#c62828'><b>%1</b></span>").arg(state_str);
    }
    this->label_state->setText(state_str);

    QString pyqint = this->environment->get_pyqint_version();
    if(pyqint.isEmpty()) {
        pyqint = "-";
    } else if(pyqint != PYQINT_PINNED_VERSION) {
        pyqint += QString(" (tested version: %1)").arg(PYQINT_PINNED_VERSION);
    } else {
        pyqint += " (tested version)";
    }
    this->label_pyqint->setText(pyqint);

    QString pydft = this->environment->get_pydft_version();
    if(state != State::Ready) {
        pydft = "-";
    } else if(pydft.isEmpty()) {
        pydft = "<span style='color:#c62828'>not installed: DFT calculations are unavailable";
        if(!this->environment->get_pydft_error().isEmpty()) {
            pydft += QString(" (%1)").arg(this->environment->get_pydft_error().toHtmlEscaped());
        }
        pydft += override ? "</span>" : ". Click <i>Use tested versions</i> to install it.</span>";
    } else if(pydft != PYDFT_PINNED_VERSION) {
        pydft += QString(" (tested version: %1)").arg(PYDFT_PINNED_VERSION);
    } else {
        pydft += " (tested version)";
    }
    this->label_pydft->setText(pydft);
    this->label_python->setText(this->environment->get_python_version().isEmpty() ? "-" :
                                this->environment->get_python_version());
    this->label_location->setText(QDir::toNativeSeparators(this->environment->python_executable()));

    this->edit_override->setText(PythonEnvironment::interpreter_override());

    const bool managed_actions = !busy && !override && has_uv;
    this->button_install->setEnabled(managed_actions && state != State::Ready);
    this->button_install->setText(state == State::Broken ? "Repair" : "Install");
    this->button_pinned->setEnabled(managed_actions && state == State::Ready);
    this->button_update->setEnabled(managed_actions && state == State::Ready);
    this->button_reset->setEnabled(managed_actions);
    this->button_cancel->setEnabled(busy);
    this->button_override_browse->setEnabled(!busy);
    this->button_override_clear->setEnabled(!busy && override);

    if(!has_uv && !override) {
        this->label_step->setText(QString("<span style='color:#c62828'>The uv executable that installs Python and "
                                  "PyQInt/PyDFT was not found next to the program (%1) or on the PATH. The installers "
                                  "include it; for a build from source, re-run the build (CMake option "
                                  "PYQINT_GUI_FETCH_UV) or run <tt>scripts/fetch-uv.sh</tt>. Alternatively, "
                                  "select your own Python interpreter with PyQInt below.</span>")
                                  .arg(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toHtmlEscaped()));
    }

    if(!busy) {
        this->progress->setRange(0, 1);
        this->progress->setValue(state == State::Ready ? 1 : 0);
    }
}

void EnvironmentDialog::on_output(const QString& text) {
    this->log->moveCursor(QTextCursor::End);
    this->log->insertPlainText(text);
    this->log->verticalScrollBar()->setValue(this->log->verticalScrollBar()->maximum());
}

void EnvironmentDialog::on_step(const QString& description, int step, int nsteps) {
    this->label_step->setText(QString("Step %1 of %2: %3...").arg(step).arg(nsteps).arg(description));
    this->progress->setRange(0, 0);     // busy indicator
}

void EnvironmentDialog::on_finished(bool success, const QString& message) {
    this->label_step->setText(success ? QString("<span style='color:#2e7d32'>%1</span>").arg(message) :
                                        QString("<span style='color:#c62828'>%1</span>").arg(message));
    this->update_state();
}
