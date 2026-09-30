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

#include "main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

#include "anaglyph_widget.h"
#include "bond_analysis_dialog.h"
#include "calculation/job_result.h"
#include "calculation/job_runner.h"
#include "calculation/python_environment.h"
#include "calculation_panel.h"
#include "config.h"
#include "environment_dialog.h"
#include "icons.h"
#include "library_dialog.h"
#include "orbital_gallery_window.h"
#include "results_panel.h"
#include "viewer_controller.h"

namespace {
constexpr const char* MANUAL_URL = "https://ifilot.github.io/pyqint/";
constexpr const char* PYDFT_MANUAL_URL = "https://ifilot.github.io/pydft/";
constexpr const char* GITHUB_URL = "https://github.com/ifilot/pyqint-gui";
constexpr const char* DEFAULT_MOLECULE = ":/assets/molecules/h2o.xyz";
}

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent) {

    this->setWindowTitle(QString("%1 %2").arg(PROGRAM_NAME, PROGRAM_VERSION));
    this->setWindowIcon(QIcon(":/assets/icons/pyqint-gui.png"));
    this->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);

    this->environment = new PythonEnvironment(this);
    this->runner = new JobRunner(this->environment, this);

    // central 3D viewer
    this->viewer = new AnaglyphWidget(this);
    this->setCentralWidget(this->viewer);
    this->viewer_controller = new ViewerController(this->viewer, this);

    this->build_docks();
    this->build_menu();
    this->build_statusbar();

    // environment
    connect(this->environment, &PythonEnvironment::state_changed, this, &MainWindow::on_environment_state_changed);
    connect(this->environment, &PythonEnvironment::finished, this, &MainWindow::on_environment_finished);

    // jobs
    connect(this->calculation_panel, &CalculationPanel::run_requested, this, &MainWindow::run_job);
    connect(this->calculation_panel, &CalculationPanel::cancel_requested, this->runner, &JobRunner::cancel);
    connect(this->calculation_panel, &CalculationPanel::library_requested, this, &MainWindow::open_library);
    connect(this->calculation_panel, &CalculationPanel::open_requested, this, &MainWindow::open_xyz);
    connect(this->runner, &JobRunner::started, this, &MainWindow::on_job_started);
    connect(this->runner, &JobRunner::output, this, &MainWindow::append_log);
    connect(this->runner, &JobRunner::stage_changed, this, &MainWindow::on_job_stage);
    connect(this->runner, &JobRunner::scf_iteration, this->calculation_panel, &CalculationPanel::on_scf_iteration);
    connect(this->runner, &JobRunner::optimization_step, this->calculation_panel, &CalculationPanel::on_optimization_step);
    connect(this->runner, &JobRunner::finished, this, &MainWindow::on_job_finished);

    // results -> viewer
    connect(this->results_panel, &ResultsPanel::orbital_selected, this->viewer_controller, &ViewerController::show_orbital);
    connect(this->results_panel, &ResultsPanel::orbital_hidden, this->viewer_controller, &ViewerController::hide_orbital);
    connect(this->results_panel, &ResultsPanel::isovalue_changed, this->viewer_controller, &ViewerController::set_isovalue);
    connect(this->results_panel, &ResultsPanel::fraction_changed, this->viewer_controller, &ViewerController::set_enclosed_fraction);
    connect(this->results_panel, &ResultsPanel::grid_spacing_changed, this->viewer_controller, &ViewerController::set_grid_spacing);
    connect(this->results_panel, &ResultsPanel::opacity_changed, this->viewer_controller, &ViewerController::set_opacity);
    connect(this->results_panel, &ResultsPanel::trajectory_frame_selected, this->viewer_controller,
            [this](int frame) { this->viewer_controller->show_trajectory_frame((size_t)frame); });
    connect(this->results_panel, &ResultsPanel::gallery_requested, this, &MainWindow::show_gallery);
    connect(this->results_panel, &ResultsPanel::localization_requested, this, &MainWindow::localize_orbitals);
    connect(this->results_panel, &ResultsPanel::bonding_analysis_requested, this, &MainWindow::show_bonding_analysis);
    connect(this->viewer_controller, &ViewerController::orbital_shown, this->results_panel, &ResultsPanel::on_orbital_shown);
    connect(this->viewer_controller, &ViewerController::busy_changed, this, &MainWindow::on_viewer_busy);

    // start with a simple molecule once OpenGL is available
    connect(this->viewer, &AnaglyphWidget::opengl_ready, this, [this]() {
        if(this->calculation_panel->get_molecule().empty()) {
            try {
                this->load_molecule(Molecule::from_xyz_file(DEFAULT_MOLECULE));
            } catch(const std::exception& e) {
                this->append_log(QString("Cannot load default molecule: %1\n").arg(e.what()));
            }
        }
    });

    // restore window layout
    QSettings settings;
    this->resize(1400, 860);
    this->restoreGeometry(settings.value("window/geometry").toByteArray());
    this->restoreState(settings.value("window/state").toByteArray());

    // check the Python environment in the background
    QTimer::singleShot(0, this->environment, &PythonEnvironment::check);
}

// ---------------------------------------------------------------------------
// user interface construction
// ---------------------------------------------------------------------------
void MainWindow::build_docks() {
    // calculation settings (left)
    this->calculation_panel = new CalculationPanel;
    auto* scroll = new QScrollArea;
    scroll->setWidget(this->calculation_panel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* dock_calc = new QDockWidget("Calculation", this);
    dock_calc->setObjectName("dock_calculation");
    dock_calc->setWidget(scroll);
    dock_calc->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock_calc->setMinimumWidth(300);
    this->addDockWidget(Qt::LeftDockWidgetArea, dock_calc);

    // results (right)
    this->results_panel = new ResultsPanel;
    auto* dock_results = new QDockWidget("Results", this);
    dock_results->setObjectName("dock_results");
    dock_results->setWidget(this->results_panel);
    dock_results->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock_results->setMinimumWidth(430);
    this->addDockWidget(Qt::RightDockWidgetArea, dock_results);

    // output (bottom)
    this->output_log = new QPlainTextEdit;
    this->output_log->setReadOnly(true);
    this->output_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->output_log->setMaximumBlockCount(20000);
    this->output_log->setPlaceholderText("Output of the calculations appears here.");
    auto* dock_log = new QDockWidget("Output", this);
    dock_log->setObjectName("dock_output");
    dock_log->setWidget(this->output_log);
    this->addDockWidget(Qt::BottomDockWidgetArea, dock_log);
    this->resizeDocks({dock_log}, {160}, Qt::Vertical);
}

void MainWindow::build_menu() {
    // ------------------------------------------------------------------
    // File
    // ------------------------------------------------------------------
    QMenu* menu_file = this->menuBar()->addMenu("&File");

    QAction* action_library = menu_file->addAction(bluecurve_icon("accessories-dictionary"), "Molecule &library...");
    action_library->setShortcut(QKeySequence("Ctrl+L"));
    connect(action_library, &QAction::triggered, this, &MainWindow::open_library);

    QAction* action_open = menu_file->addAction(bluecurve_icon("document-open"), "&Open molecule (.xyz)...");
    action_open->setShortcut(QKeySequence::Open);
    connect(action_open, &QAction::triggered, this, &MainWindow::open_xyz);

    QAction* action_open_result = menu_file->addAction(bluecurve_icon("fileimport"), "Open &result...");
    action_open_result->setShortcut(QKeySequence("Ctrl+R"));
    connect(action_open_result, &QAction::triggered, this, &MainWindow::open_result);

    QAction* action_save = menu_file->addAction(bluecurve_icon("document-save"), "&Save molecule as .xyz...");
    action_save->setShortcut(QKeySequence::Save);
    connect(action_save, &QAction::triggered, this, &MainWindow::save_xyz);

    QAction* action_image = menu_file->addAction(bluecurve_icon("image-x-generic"), "Save &image...");
    action_image->setShortcut(QKeySequence("Ctrl+I"));
    connect(action_image, &QAction::triggered, this, &MainWindow::save_image);

    menu_file->addSeparator();
    QAction* action_jobs = menu_file->addAction(bluecurve_icon("folder"), "Show &job folders");
    connect(action_jobs, &QAction::triggered, this, &MainWindow::open_jobs_folder);

    menu_file->addSeparator();
    QAction* action_quit = menu_file->addAction(bluecurve_icon("application-exit"), "&Quit");
    action_quit->setShortcut(QKeySequence::Quit);
    connect(action_quit, &QAction::triggered, this, &QMainWindow::close);

    // ------------------------------------------------------------------
    // View
    // ------------------------------------------------------------------
    QMenu* menu_view = this->menuBar()->addMenu("&View");

    // the entries of this menu keep their own (non-Bluecurve) icons
    QMenu* menu_projection = menu_view->addMenu(bluecurve_icon("display-capplet"), "&Projection");
    auto* group_projection = new QActionGroup(this);
    const std::vector<std::pair<QString, QString>> projections = {
        {"Two-dimensional", "no_stereo_flat"},
        {"Anaglyph (red/cyan)", "stereo_anaglyph_red_cyan"},
        {"Interlaced rows (left first)", "stereo_interlaced_rows_lr"},
        {"Interlaced rows (right first)", "stereo_interlaced_rows_rl"},
        {"Interlaced columns (left first)", "stereo_interlaced_columns_lr"},
        {"Interlaced columns (right first)", "stereo_interlaced_columns_rl"},
        {"Checkerboard (left first)", "stereo_interlaced_checkerboard_lr"},
        {"Checkerboard (right first)", "stereo_interlaced_checkerboard_rl"},
    };
    for(const auto& p : projections) {
        QString icon = p.second == "no_stereo_flat" ? QString(":/assets/icons/two_dimensional_32.png") :
            QString(":/assets/icons/%1_32.png").arg(QString(p.second).replace("stereo_", ""));
        QAction* action = menu_projection->addAction(QIcon(icon), p.first);
        action->setCheckable(true);
        action->setChecked(p.second == "no_stereo_flat");
        group_projection->addAction(action);
        const QString name = p.second;
        connect(action, &QAction::triggered, this, [this, name]() { this->set_stereo(name); });
        if(p.second == "no_stereo_flat") {
            menu_projection->addSeparator();
        }
    }

    QMenu* menu_camera = menu_view->addMenu(bluecurve_icon("camera-photo"), "&Camera");
    auto* group_camera = new QActionGroup(this);
    QAction* action_persp = menu_camera->addAction("&Perspective");
    QAction* action_ortho = menu_camera->addAction("&Orthographic");
    for(QAction* a : {action_persp, action_ortho}) {
        a->setCheckable(true);
        group_camera->addAction(a);
    }
    action_persp->setChecked(true);
    connect(action_persp, &QAction::triggered, this, [this]() { this->viewer->set_camera_mode((int)CameraMode::PERSPECTIVE); });
    connect(action_ortho, &QAction::triggered, this, [this]() { this->viewer->set_camera_mode((int)CameraMode::ORTHOGRAPHIC); });
    menu_camera->addSeparator();

    const std::vector<std::pair<QString, CameraAlignment>> alignments = {
        {"Default view", CameraAlignment::DEFAULT},
        {"Top", CameraAlignment::TOP}, {"Bottom", CameraAlignment::BOTTOM},
        {"Left", CameraAlignment::LEFT}, {"Right", CameraAlignment::RIGHT},
        {"Front", CameraAlignment::FRONT}, {"Back", CameraAlignment::BACK},
        {"Face-on (perpendicular to the molecule)", CameraAlignment::FACE_ON},
        {"Edge-on (along the plane of the molecule)", CameraAlignment::EDGE_ON},
    };
    for(const auto& al : alignments) {
        QAction* action = menu_camera->addAction(al.first);
        if(al.second == CameraAlignment::DEFAULT) {
            action->setIcon(bluecurve_icon("go-home"));
        }
        const int dir = (int)al.second;
        connect(action, &QAction::triggered, this, [this, dir]() { this->viewer->set_camera_alignment(dir); });
    }
    menu_camera->addSeparator();
    QAction* action_reset_pan = menu_camera->addAction(bluecurve_icon("zoom-best-fit"), "Reset panning");
    connect(action_reset_pan, &QAction::triggered, this->viewer, &AnaglyphWidget::reset_panning);

    QAction* action_axes = menu_view->addAction("Show coordinate &axes");
    action_axes->setCheckable(true);
    action_axes->setChecked(true);
    connect(action_axes, &QAction::toggled, this->viewer, &AnaglyphWidget::set_show_axes);

    menu_view->addSeparator();
    for(QDockWidget* dock : this->findChildren<QDockWidget*>()) {
        menu_view->addAction(dock->toggleViewAction());
    }

    // ------------------------------------------------------------------
    // Analysis
    // ------------------------------------------------------------------
    QMenu* menu_analysis = this->menuBar()->addMenu("&Analysis");
    this->action_gallery = menu_analysis->addAction(bluecurve_icon("gnome-stock-insert-table"), "Orbital &gallery...");
    this->action_gallery->setShortcut(QKeySequence("Ctrl+G"));
    this->action_gallery->setStatusTip("Show all molecular orbitals side by side and export them as images");
    connect(this->action_gallery, &QAction::triggered, this, &MainWindow::show_gallery);

    this->action_localize = menu_analysis->addAction(bluecurve_icon("gnome-run"), "&Localize orbitals (Foster-Boys)");
    this->action_localize->setStatusTip("Add Foster-Boys localized orbitals to the current result without repeating the calculation");
    connect(this->action_localize, &QAction::triggered, this, &MainWindow::localize_orbitals);

    menu_analysis->addSeparator();
    this->action_bonding = menu_analysis->addAction(bluecurve_icon("accessories-calculator"), "Orbital &bonding analysis (MOHP)...");
    this->action_bonding->setStatusTip("Orbital-resolved Hamilton, overlap and bond-index populations for a pair of atoms");
    connect(this->action_bonding, &QAction::triggered, this, &MainWindow::show_bonding_analysis);

    // ------------------------------------------------------------------
    // Python
    // ------------------------------------------------------------------
    QMenu* menu_python = this->menuBar()->addMenu("&Python");
    QAction* action_env = menu_python->addAction(bluecurve_icon("preferences-system"), "&Manage environment...");
    connect(action_env, &QAction::triggered, this, &MainWindow::show_environment_dialog);

    // ------------------------------------------------------------------
    // Help
    // ------------------------------------------------------------------
    QMenu* menu_help = this->menuBar()->addMenu("&Help");
    QAction* action_manual = menu_help->addAction(bluecurve_icon("help-contents"), "PyQInt &manual");
    connect(action_manual, &QAction::triggered, this, []() { QDesktopServices::openUrl(QUrl(MANUAL_URL)); });
    QAction* action_pydft_manual = menu_help->addAction(bluecurve_icon("help-contents"), "PyDFT m&anual");
    connect(action_pydft_manual, &QAction::triggered, this, []() { QDesktopServices::openUrl(QUrl(PYDFT_MANUAL_URL)); });
    QAction* action_github = menu_help->addAction(bluecurve_icon("icon-globe"), QString("%1 on &GitHub").arg(PROGRAM_NAME));
    connect(action_github, &QAction::triggered, this, []() { QDesktopServices::openUrl(QUrl(GITHUB_URL)); });
    menu_help->addSeparator();
    QAction* action_about = menu_help->addAction(bluecurve_icon("help-about"), "&About");
    connect(action_about, &QAction::triggered, this, &MainWindow::show_about);
}

void MainWindow::build_statusbar() {
    this->status_environment = new QPushButton;
    this->status_environment->setFlat(true);
    this->status_environment->setToolTip("Click to manage the Python environment");
    connect(this->status_environment, &QPushButton::clicked, this, &MainWindow::show_environment_dialog);

    this->status_busy = new QLabel;
    this->status_progress = new QProgressBar;
    this->status_progress->setRange(0, 0);
    this->status_progress->setMaximumWidth(120);
    this->status_progress->setMaximumHeight(14);
    this->status_progress->setVisible(false);

    this->statusBar()->addWidget(this->status_busy);
    this->statusBar()->addWidget(this->status_progress);
    this->statusBar()->addPermanentWidget(this->status_environment);

    this->on_environment_state_changed();
}

// ---------------------------------------------------------------------------
// molecules and results
// ---------------------------------------------------------------------------
void MainWindow::load_molecule(const Molecule& mol) {
    this->calculation_panel->set_molecule(mol);
    this->results_panel->clear();
    this->result_file.clear();
    if(this->gallery != nullptr) {
        this->gallery->close();
    }
    this->update_analysis_actions();
    this->viewer_controller->show_molecule(mol);
    this->statusBar()->showMessage(QString("Loaded %1 (%2)").arg(mol.get_name(), mol.formula()), 5000);
}

void MainWindow::load_result(const QString& filename, const QString& job_dir, bool fit_camera) {
    std::shared_ptr<JobResult> result;
    try {
        result = JobResult::load(filename);
    } catch(const std::exception& e) {
        QMessageBox::critical(this, "Cannot open result", QString::fromStdString(e.what()));
        return;
    }

    // show the geometry the result belongs to in the calculation panel
    this->calculation_panel->set_molecule(result->get_molecule());
    this->calculation_panel->load_settings(*result);
    this->viewer_controller->show_result(result, fit_camera);
    this->results_panel->set_result(result, job_dir);
    this->result_file = QFileInfo(filename).absoluteFilePath();
    this->update_analysis_actions();

    if(this->gallery != nullptr && this->gallery->isVisible()) {
        this->gallery->set_result(result, job_dir, this->results_panel->current_set());
    }
}

void MainWindow::update_analysis_actions() {
    const auto& result = this->results_panel->get_result();
    const bool available = this->environment->is_ready() && !this->runner->is_running();
    const bool can_localize = result && !result->is_unrestricted() && result->find_orbital_set("Foster-Boys") < 0;

    if(this->action_gallery != nullptr) {
        this->action_gallery->setEnabled((bool)result);
        this->action_localize->setEnabled(can_localize && available);
        this->action_bonding->setEnabled(result && result->positions.size() >= 2);
    }
    this->results_panel->set_localization_available(available);
}

QWidget* MainWindow::show_tool_window(const QString& name) {
    if(!this->results_panel->get_result()) {
        return nullptr;
    }
    if(name == "gallery") {
        this->show_gallery();
        return this->gallery;
    }
    if(name == "bonding") {
        this->show_bonding_analysis();
        return this->findChild<BondAnalysisDialog*>();
    }
    return nullptr;
}

void MainWindow::show_gallery() {
    const auto& result = this->results_panel->get_result();
    if(!result) {
        return;
    }

    if(this->gallery == nullptr) {
        this->gallery = new OrbitalGalleryWindow(this->viewer, this->viewer_controller, this);
        connect(this->gallery, &OrbitalGalleryWindow::orbital_activated, this, [this](int set, int orbital) {
            this->results_panel->select_orbital(set, orbital);
            this->results_panel->show_tab("orbitals");
            this->raise();
            this->activateWindow();
        });
    }
    if(!this->gallery->isVisible()) {
        this->gallery->set_result(result, this->results_panel->get_job_dir(), this->results_panel->current_set());
    }
    this->gallery->show();
    this->gallery->raise();
    this->gallery->activateWindow();
}

void MainWindow::show_bonding_analysis() {
    const auto& result = this->results_panel->get_result();
    if(!result || result->positions.size() < 2) {
        return;
    }
    auto* dialog = new BondAnalysisDialog(result, this->results_panel->current_set(), this->viewer_controller, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void MainWindow::localize_orbitals() {
    const auto& result = this->results_panel->get_result();
    if(!result || result->is_unrestricted() || this->result_file.isEmpty()) {
        return;
    }

    const QString err = this->runner->start_localization(this->result_file);
    if(!err.isEmpty()) {
        QMessageBox::warning(this, "Cannot localize orbitals", err);
        return;
    }
    this->localization_running = true;
    this->append_log(QString("Constructing Foster-Boys orbitals from the stored canonical orbitals "
                             "(the %1 calculation is not repeated).\n\n").arg(result->is_dft() ? "DFT" : "Hartree-Fock"));
}

void MainWindow::open_file(const QString& path) {
    if(path.endsWith(".json", Qt::CaseInsensitive)) {
        this->load_result(path, QFileInfo(path).absolutePath());
        return;
    }
    try {
        this->load_molecule(Molecule::from_xyz_file(path));
    } catch(const std::exception& e) {
        QMessageBox::critical(this, "Cannot open molecule", QString::fromStdString(e.what()));
    }
}

void MainWindow::show_results_tab(const QString& name) {
    this->results_panel->show_tab(name);
}

void MainWindow::open_library() {
    LibraryDialog dialog(this);
    if(dialog.exec() == QDialog::Accepted) {
        const Molecule mol = dialog.selected_molecule();
        if(!mol.empty()) {
            this->load_molecule(mol);
        }
    }
}

void MainWindow::open_xyz() {
    QSettings settings;
    const QString dir = settings.value("paths/last_xyz", QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(this, "Open molecule", dir, "XYZ files (*.xyz);;All files (*)");
    if(path.isEmpty()) {
        return;
    }
    settings.setValue("paths/last_xyz", QFileInfo(path).absolutePath());
    this->open_file(path);
}

void MainWindow::open_result() {
    const QString path = QFileDialog::getOpenFileName(this, "Open result", JobRunner::jobs_directory(),
                                                      "PyQInt-GUI results (result.json *.json);;All files (*)");
    if(!path.isEmpty()) {
        this->load_result(path, QFileInfo(path).absolutePath());
    }
}

void MainWindow::save_xyz() {
    const Molecule& mol = this->viewer_controller->get_molecule();
    if(mol.empty()) {
        return;
    }
    QSettings settings;
    const QString dir = settings.value("paths/last_xyz", QDir::homePath()).toString();
    const QString path = QFileDialog::getSaveFileName(this, "Save molecule", QDir(dir).filePath(mol.get_name() + ".xyz"),
                                                      "XYZ files (*.xyz)");
    if(path.isEmpty()) {
        return;
    }
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Cannot save", QString("Cannot write %1").arg(path));
        return;
    }
    f.write(mol.to_xyz().toUtf8());
    settings.setValue("paths/last_xyz", QFileInfo(path).absolutePath());
}

void MainWindow::save_image() {
    QSettings settings;
    const QString dir = settings.value("paths/last_image", QDir::homePath()).toString();
    const QString path = QFileDialog::getSaveFileName(this, "Save image",
        QDir(dir).filePath(this->viewer_controller->get_molecule().get_name() + ".png"),
        "PNG images (*.png);;JPEG images (*.jpg)");
    if(path.isEmpty()) {
        return;
    }
    const QImage image = this->viewer->grabFramebuffer();
    if(!image.save(path)) {
        QMessageBox::critical(this, "Cannot save image", QString("Cannot write %1").arg(path));
        return;
    }
    settings.setValue("paths/last_image", QFileInfo(path).absolutePath());
    this->statusBar()->showMessage(QString("Saved %1").arg(QDir::toNativeSeparators(path)), 5000);
}

void MainWindow::open_jobs_folder() {
    const QString dir = JobRunner::jobs_directory();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

// ---------------------------------------------------------------------------
// Python environment
// ---------------------------------------------------------------------------
void MainWindow::show_environment_dialog() {
    if(this->environment_dialog == nullptr) {
        this->environment_dialog = new EnvironmentDialog(this->environment, this);
    }
    this->environment_dialog->show();
    this->environment_dialog->raise();
    this->environment_dialog->activateWindow();
}

void MainWindow::on_environment_state_changed() {
    using State = PythonEnvironment::State;
    const State state = this->environment->get_state();

    QString text;
    switch(state) {
        case State::Ready:
            text = QString("● %1").arg(this->environment->programs_label());
            this->status_environment->setStyleSheet("color: #2e7d32;");
            break;
        case State::Missing:
        case State::Broken:
            text = QString("● Python environment: %1").arg(PythonEnvironment::state_label(state).toLower());
            this->status_environment->setStyleSheet("color: #c62828;");
            break;
        default:
            text = QString("○ Python environment: %1").arg(PythonEnvironment::state_label(state).toLower());
            this->status_environment->setStyleSheet(QString());
    }
    this->status_environment->setText(text);
    this->calculation_panel->set_environment_ready(this->environment->is_ready(), this->environment->has_pydft());
    this->update_analysis_actions();
}

void MainWindow::on_environment_finished(bool success, const QString& message) {
    using State = PythonEnvironment::State;
    this->on_environment_state_changed();

    // first start: offer to install the environment
    if(!this->first_check_done) {
        this->first_check_done = true;
        const State state = this->environment->get_state();
        if(!success && (state == State::Missing || state == State::Broken) &&
           PythonEnvironment::interpreter_override().isEmpty()) {
            auto answer = QMessageBox::question(this, "Set up PyQInt and PyDFT",
                QString("<p>%1 needs a Python environment with PyQInt and PyDFT to perform calculations.</p>"
                        "<p>It will now download Python, PyQInt %2 and PyDFT %3 (about 300&nbsp;MB) into "
                        "a private folder. This only has to be done once and does not affect other "
                        "Python installations on this computer.</p><p>Install now?</p>")
                    .arg(PROGRAM_NAME, PYQINT_PINNED_VERSION, PYDFT_PINNED_VERSION));
            if(answer == QMessageBox::Yes) {
                this->show_environment_dialog();
                this->environment_dialog->start_install();
            }
        } else if(!success) {
            this->append_log(message + "\n");
        }
    }
}

// ---------------------------------------------------------------------------
// jobs
// ---------------------------------------------------------------------------
void MainWindow::run_job(const JobSpec& spec) {
    const QString err = this->runner->start(spec);
    if(!err.isEmpty()) {
        QMessageBox::warning(this, "Cannot start calculation", err);
    }
}

void MainWindow::on_job_started(const QString& job_dir) {
    this->calculation_panel->set_running(true);
    this->output_log->clear();
    this->append_log(QString("Job folder: %1\n\n").arg(QDir::toNativeSeparators(job_dir)));
    this->calculation_panel->set_status("Starting Python...");
    this->status_busy->setText("Calculation running...");
    this->status_progress->setVisible(true);
    this->update_analysis_actions();
}

void MainWindow::on_job_stage(const QString& stage) {
    static const std::map<QString, QString> labels = {
        {"scf", "Solving the Hartree-Fock equations..."},
        {"dft", "Building the integration grid and solving the Kohn-Sham equations..."},
        {"optimization", "Optimizing the geometry..."},
        {"localization", "Localizing orbitals (Foster-Boys)..."},
        {"export", "Collecting results..."},
        {"done", "Done."},
    };
    auto it = labels.find(stage);
    if(it != labels.end()) {
        this->calculation_panel->set_status(it->second);
        this->status_busy->setText(it->second);
    }
}

void MainWindow::on_job_finished(bool success, const QString& message, const QString& result_file) {
    const bool localization = this->localization_running;
    this->localization_running = false;

    this->calculation_panel->set_running(false);
    this->calculation_panel->set_status(message);
    this->status_busy->clear();
    this->status_progress->setVisible(false);
    this->statusBar()->showMessage(message, 8000);
    this->update_analysis_actions();

    if(success && localization) {
        // keep the camera; show the new orbitals
        this->load_result(result_file, this->runner->get_job_directory(), false);
        const auto& result = this->results_panel->get_result();
        const int fb = result ? result->find_orbital_set("Foster-Boys") : -1;
        if(fb >= 0) {
            this->results_panel->select_orbital(fb, std::max(0, result->orbital_sets[fb].homo()));
            this->results_panel->show_tab("orbitals");
        }
    } else if(success) {
        this->load_result(result_file, this->runner->get_job_directory());
    } else {
        QMessageBox::warning(this, "Calculation failed",
                             message + "\n\nThe output panel at the bottom of the window shows the details.");
    }
}

void MainWindow::on_viewer_busy(bool busy, const QString& message) {
    if(this->runner->is_running()) {
        return;     // the job status takes precedence
    }
    this->status_busy->setText(message);
    this->status_progress->setVisible(busy);
}

// ---------------------------------------------------------------------------
// misc
// ---------------------------------------------------------------------------
void MainWindow::append_log(const QString& text) {
    this->output_log->moveCursor(QTextCursor::End);
    this->output_log->insertPlainText(text);
    this->output_log->verticalScrollBar()->setValue(this->output_log->verticalScrollBar()->maximum());
}

void MainWindow::set_stereo(const QString& name) {
    this->viewer->set_stereo(name);
}

void MainWindow::show_about() {
    QMessageBox::about(this, QString("About %1").arg(PROGRAM_NAME),
        QString("<h3>%1 %2</h3>"
                "<p>Graphical user interface for the educational electronic-structure programs "
                "<a href='%3'>PyQInt</a> (Hartree-Fock) and <a href='%8'>PyDFT</a> (density "
                "functional theory).</p>"
                "<p>Source code, releases and issue tracker: <a href='%7'>%7</a></p>"
                "<p>Author: Ivo Filot<br>License: GNU General Public License v3<br>"
                "Icons: Bluecurve icon theme (Red Hat, GPL)</p>"
                "<p><small>Build %4 &middot; tested with PyQInt %5 and PyDFT %9 &middot; Qt %6</small></p>")
            .arg(PROGRAM_NAME, PROGRAM_VERSION, MANUAL_URL, GIT_HASH, PYQINT_PINNED_VERSION, qVersion(), GITHUB_URL,
                 PYDFT_MANUAL_URL, PYDFT_PINNED_VERSION));
}

void MainWindow::moveEvent(QMoveEvent* event) {
    QMainWindow::moveEvent(event);
    this->viewer->window_move_event();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if(this->runner->is_running()) {
        auto answer = QMessageBox::question(this, "Calculation running",
                                            "A calculation is still running. Stop it and quit?");
        if(answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        this->runner->cancel();
    }
    if(this->environment->is_busy()) {
        this->environment->cancel();
    }

    QSettings settings;
    settings.setValue("window/geometry", this->saveGeometry());
    settings.setValue("window/state", this->saveState());
    QMainWindow::closeEvent(event);
}
