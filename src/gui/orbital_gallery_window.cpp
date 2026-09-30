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

#include "orbital_gallery_window.h"

#include <algorithm>
#include <cmath>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "anaglyph_widget.h"
#include "data/model.h"
#include "data/structure.h"
#include "icons.h"
#include "orbital_gallery_widget.h"
#include "viewer_controller.h"

namespace {

enum Range {
    RANGE_ALL,
    RANGE_OCCUPIED,
    RANGE_FRONTIER,
    RANGE_CUSTOM,
};

constexpr int MAX_ALL_BY_DEFAULT = 30;      // show all orbitals up to this number
constexpr int FRONTIER_WIDTH = 3;           // HOMO-3 ... LUMO+3

QString slug(const QString& text) {
    QString result = text.simplified().toLower();
    result.replace(QRegularExpression("[^a-z0-9+-]+"), "-");
    result.remove(QRegularExpression("^-+|-+$"));
    return result;
}

} // namespace

OrbitalGalleryWindow::OrbitalGalleryWindow(AnaglyphWidget* _main_viewer, const ViewerController* settings, QWidget* parent) :
    QWidget(parent, Qt::Window),
    main_viewer(_main_viewer) {

    this->setWindowTitle("Orbital gallery");
    this->setWindowIcon(QIcon(":/assets/icons/praxis.png"));
    this->resize(1180, 820);

    this->opacity = settings->get_opacity();
    this->color_positive = settings->get_color_positive();
    this->color_negative = settings->get_color_negative();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(this->build_controls());
    layout->addWidget(this->build_view_toolbar());

    auto* lgallery = new QHBoxLayout;
    lgallery->setSpacing(0);
    this->gallery = new OrbitalGalleryWidget;
    this->scrollbar = new QScrollBar(Qt::Vertical);
    lgallery->addWidget(this->gallery, 1);
    lgallery->addWidget(this->scrollbar);
    layout->addLayout(lgallery, 1);

    auto* lstatus = new QHBoxLayout;
    this->label_status = new QLabel;
    this->progress = new QProgressBar;
    this->progress->setMaximumWidth(200);
    this->progress->setMaximumHeight(14);
    this->progress->setVisible(false);
    auto* hint = new QLabel("Drag to rotate all orbitals · Ctrl + wheel to zoom · double-click to show an orbital in the main window");
    hint->setStyleSheet("color: gray;");
    lstatus->addWidget(this->label_status);
    lstatus->addWidget(this->progress);
    lstatus->addStretch(1);
    lstatus->addWidget(hint);
    layout->addLayout(lstatus);

    // initial isosurface settings follow the main viewer
    {
        const QSignalBlocker b1(this->spin_fraction), b2(this->radio_auto), b3(this->combo_grid);
        this->spin_fraction->setValue((int)std::lround(settings->get_enclosed_fraction() * 100.0f));
        this->spin_isovalue->setValue(settings->get_isovalue());
        this->radio_auto->setChecked(settings->is_auto_isovalue());
        this->radio_manual->setChecked(!settings->is_auto_isovalue());
        this->spin_fraction->setEnabled(settings->is_auto_isovalue());
        this->spin_isovalue->setEnabled(!settings->is_auto_isovalue());
        const int grid = this->combo_grid->findData((double)settings->get_grid_settings().spacing);
        this->combo_grid->setCurrentIndex(grid >= 0 ? grid : 1);
    }

    // orientation and projection follow the main viewer
    this->gallery->set_rotation(this->main_viewer->get_rotation());
    this->combo_projection->setCurrentIndex(this->main_viewer->get_camera_mode() == CameraMode::ORTHOGRAPHIC ? 1 : 0);

    this->watcher = new QFutureWatcher<Build>(this);
    connect(this->watcher, &QFutureWatcher<Build>::finished, this, &OrbitalGalleryWindow::on_build_finished);

    connect(this->gallery, &OrbitalGalleryWidget::layout_changed, this, &OrbitalGalleryWindow::update_scrollbar);
    connect(this->gallery, &OrbitalGalleryWidget::scroll_requested, this, [this](int delta) {
        this->scrollbar->setValue(this->scrollbar->value() + delta);
    });
    connect(this->scrollbar, &QScrollBar::valueChanged, this->gallery, &OrbitalGalleryWidget::set_scroll);
    connect(this->gallery, &OrbitalGalleryWidget::tile_activated, this, [this](int tile) {
        if(tile >= 0 && tile < (int)this->orbitals.size()) {
            emit orbital_activated(this->current_set(), this->orbitals[tile]);
        }
    });
}

OrbitalGalleryWindow::~OrbitalGalleryWindow() {
    this->generation++;
}

// ---------------------------------------------------------------------------
// user interface
// ---------------------------------------------------------------------------
QWidget* OrbitalGalleryWindow::build_controls() {
    auto* w = new QWidget;
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);

    l->addWidget(new QLabel("Orbitals:"));
    this->combo_set = new QComboBox;
    l->addWidget(this->combo_set);

    this->combo_range = new QComboBox;
    this->combo_range->addItem("All orbitals", RANGE_ALL);
    this->combo_range->addItem("Occupied orbitals", RANGE_OCCUPIED);
    this->combo_range->addItem(QString("Frontier (HOMO-%1 ... LUMO+%1)").arg(FRONTIER_WIDTH), RANGE_FRONTIER);
    this->combo_range->addItem("Custom range", RANGE_CUSTOM);
    l->addWidget(this->combo_range);

    this->spin_from = new QSpinBox;
    this->spin_to = new QSpinBox;
    this->spin_from->setPrefix("from #");
    this->spin_to->setPrefix("to #");
    l->addWidget(this->spin_from);
    l->addWidget(this->spin_to);

    l->addSpacing(16);
    this->radio_auto = new QRadioButton("Enclose");
    this->radio_auto->setToolTip("Choose the isovalue of each orbital such that the surface encloses this fraction of its density");
    this->spin_fraction = new QSpinBox;
    this->spin_fraction->setRange(10, 99);
    this->spin_fraction->setValue(90);
    this->spin_fraction->setSuffix(" %");
    this->radio_manual = new QRadioButton("Isovalue");
    this->radio_manual->setToolTip("Use the same isovalue for all orbitals");
    this->spin_isovalue = new QDoubleSpinBox;
    this->spin_isovalue->setDecimals(4);
    this->spin_isovalue->setRange(0.0005, 5.0);
    this->spin_isovalue->setSingleStep(0.005);
    this->spin_isovalue->setValue(0.05);
    auto* group = new QButtonGroup(w);
    group->addButton(this->radio_auto);
    group->addButton(this->radio_manual);
    this->radio_auto->setChecked(true);
    this->spin_isovalue->setEnabled(false);
    l->addWidget(this->radio_auto);
    l->addWidget(this->spin_fraction);
    l->addWidget(this->radio_manual);
    l->addWidget(this->spin_isovalue);

    l->addSpacing(16);
    l->addWidget(new QLabel("Grid:"));
    this->combo_grid = new QComboBox;
    this->combo_grid->addItem("Coarse (fast)", 0.20);
    this->combo_grid->addItem("Normal", 0.12);
    this->combo_grid->addItem("Fine (slow)", 0.07);
    this->combo_grid->setCurrentIndex(1);
    l->addWidget(this->combo_grid);

    l->addStretch(1);
    this->button_export = new QPushButton(bluecurve_icon("fileexport"), "Export images...");
    this->button_export->setToolTip("Save every orbital in the gallery as a separate PNG image");
    l->addWidget(this->button_export);

    connect(this->combo_set, &QComboBox::currentIndexChanged, this, [this]() {
        this->update_range_controls();
        this->rebuild();
    });
    connect(this->combo_range, &QComboBox::currentIndexChanged, this, [this]() {
        this->update_range_controls();
        this->rebuild();
    });
    for(QSpinBox* spin : {this->spin_from, this->spin_to}) {
        connect(spin, &QSpinBox::editingFinished, this, &OrbitalGalleryWindow::rebuild);
    }
    connect(this->radio_auto, &QRadioButton::toggled, this, [this](bool automatic) {
        this->spin_fraction->setEnabled(automatic);
        this->spin_isovalue->setEnabled(!automatic);
        this->rebuild();
    });
    connect(this->spin_fraction, &QSpinBox::editingFinished, this, &OrbitalGalleryWindow::rebuild);
    connect(this->spin_isovalue, &QDoubleSpinBox::editingFinished, this, &OrbitalGalleryWindow::rebuild);
    connect(this->combo_grid, &QComboBox::currentIndexChanged, this, &OrbitalGalleryWindow::rebuild);
    connect(this->button_export, &QPushButton::clicked, this, &OrbitalGalleryWindow::export_images);

    return w;
}

QWidget* OrbitalGalleryWindow::build_view_toolbar() {
    auto* bar = new QToolBar;
    bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    bar->addWidget(new QLabel("View:  "));

    const std::vector<std::tuple<QString, CameraAlignment, QString>> views = {
        {"Default", CameraAlignment::DEFAULT, "Default oblique view"},
        {"Top", CameraAlignment::TOP, "Look down the z-axis"},
        {"Bottom", CameraAlignment::BOTTOM, "Look up the z-axis"},
        {"Front", CameraAlignment::FRONT, "Look along the y-axis"},
        {"Back", CameraAlignment::BACK, "Look along the negative y-axis"},
        {"Left", CameraAlignment::LEFT, "Look along the x-axis"},
        {"Right", CameraAlignment::RIGHT, "Look along the negative x-axis"},
        {"Face-on", CameraAlignment::FACE_ON, "Look perpendicular to the plane of the molecule\n"
                                                "(ideal for the π orbitals of planar molecules)"},
        {"Edge-on", CameraAlignment::EDGE_ON, "Look along the plane of the molecule"},
    };
    for(const auto& v : views) {
        QAction* action = bar->addAction(std::get<0>(v));
        if(std::get<1>(v) == CameraAlignment::DEFAULT) {
            action->setIcon(bluecurve_icon("go-home"));
        }
        action->setToolTip(std::get<2>(v));
        const int alignment = (int)std::get<1>(v);
        connect(action, &QAction::triggered, this, [this, alignment]() { this->set_orientation(alignment); });
    }
    QAction* action_main = bar->addAction(bluecurve_icon("camera-photo"), "Main viewer");
    action_main->setToolTip("Use the orientation of the molecule in the main window");
    connect(action_main, &QAction::triggered, this, [this]() {
        this->gallery->set_rotation(this->main_viewer->get_rotation());
    });

    bar->addSeparator();
    this->combo_projection = new QComboBox;
    this->combo_projection->addItem("Perspective");
    this->combo_projection->addItem("Orthographic");
    bar->addWidget(this->combo_projection);
    connect(this->combo_projection, &QComboBox::currentIndexChanged, this, [this](int idx) {
        this->gallery->set_camera_mode(idx == 1 ? CameraMode::ORTHOGRAPHIC : CameraMode::PERSPECTIVE);
    });

    bar->addSeparator();
    bar->addWidget(new QLabel(" Tile size: "));
    this->slider_size = new QSlider(Qt::Horizontal);
    this->slider_size->setRange(120, 480);
    this->slider_size->setValue(QSettings().value("gallery/tile_size", 240).toInt());
    this->slider_size->setMaximumWidth(160);
    bar->addWidget(this->slider_size);
    connect(this->slider_size, &QSlider::valueChanged, this, [this](int v) {
        this->gallery->set_tile_size(v);
        QSettings().setValue("gallery/tile_size", v);
    });

    return bar;
}

void OrbitalGalleryWindow::update_scrollbar() {
    const int page = std::max(1, this->gallery->height());
    this->scrollbar->setPageStep(page);
    this->scrollbar->setSingleStep(40);
    this->scrollbar->setRange(0, std::max(0, this->gallery->content_height() - page));
    this->gallery->set_scroll(this->scrollbar->value());
}

void OrbitalGalleryWindow::update_range_controls() {
    const int s = this->current_set();
    const bool custom = this->combo_range->currentData().toInt() == RANGE_CUSTOM;
    this->spin_from->setVisible(custom);
    this->spin_to->setVisible(custom);
    if(!this->result || s < 0) {
        return;
    }
    const int n = (int)this->result->orbital_sets[s].size();
    const QSignalBlocker b1(this->spin_from), b2(this->spin_to);
    this->spin_from->setRange(1, n);
    this->spin_to->setRange(1, n);
    if(this->spin_to->value() < this->spin_from->value()) {
        this->spin_to->setValue(n);
    }
}

// ---------------------------------------------------------------------------
// contents
// ---------------------------------------------------------------------------
void OrbitalGalleryWindow::set_result(std::shared_ptr<JobResult> _result, const QString& _job_dir, int set_index) {
    this->result = std::move(_result);
    this->job_dir = _job_dir;

    const Molecule mol = this->result->get_molecule();
    this->structure = mol.to_structure(&this->offset);
    this->atom_positions.clear();
    float radius = 0.0f;
    for(const auto& atom : this->structure->get_atoms()) {
        const glm::vec3 p((float)atom.x, (float)atom.y, (float)atom.z);
        this->atom_positions.push_back(p);
        radius = std::max(radius, glm::length(p));
    }

    // leave room for the orbitals around the molecule; all tiles use the
    // same scale such that the orbitals can be compared
    this->gallery->set_camera_distance(2.25f * (radius + 1.9f));
    this->setWindowTitle(QString("Orbital gallery - %1 (%2)").arg(this->result->molecule_name, mol.formula()));

    {
        const QSignalBlocker b1(this->combo_set), b2(this->combo_range);
        this->combo_set->clear();
        for(const auto& set : this->result->orbital_sets) {
            this->combo_set->addItem(set.label);
        }
        this->combo_set->setCurrentIndex(std::clamp(set_index, 0, (int)this->result->orbital_sets.size() - 1));

        const int n = (int)this->result->orbital_sets[this->current_set()].size();
        this->combo_range->setCurrentIndex(n <= MAX_ALL_BY_DEFAULT ? RANGE_ALL : RANGE_FRONTIER);
        this->spin_from->setRange(1, n);
        this->spin_to->setRange(1, n);
        this->spin_from->setValue(1);
        this->spin_to->setValue(n);
    }
    this->update_range_controls();
    this->rebuild();
}

int OrbitalGalleryWindow::current_set() const {
    return this->combo_set->currentIndex();
}

QString OrbitalGalleryWindow::tile_title(int orbital) const {
    const OrbitalSet& set = this->result->orbital_sets[this->current_set()];
    const QString label = set.frontier_label(orbital);
    return label.isEmpty() ? QString("MO %1").arg(orbital + 1) : QString("MO %1 · %2").arg(orbital + 1).arg(label);
}

void OrbitalGalleryWindow::rebuild() {
    this->generation++;
    const int s = this->current_set();
    if(!this->result || s < 0) {
        this->gallery->clear();
        return;
    }

    // orbitals to show
    const OrbitalSet& set = this->result->orbital_sets[s];
    const int n = (int)set.size();
    const int homo = set.homo();
    int first = 0, last = n - 1;
    switch(this->combo_range->currentData().toInt()) {
        case RANGE_OCCUPIED:
            last = homo;
            break;
        case RANGE_FRONTIER:
            first = std::max(0, homo - FRONTIER_WIDTH);
            last = std::min(n - 1, homo + 1 + FRONTIER_WIDTH);
            break;
        case RANGE_CUSTOM:
            first = this->spin_from->value() - 1;
            last = this->spin_to->value() - 1;
            break;
        default:
            break;
    }

    this->orbitals.clear();
    std::vector<OrbitalGalleryWidget::Tile> tiles;
    for(int i = std::max(0, first); i <= std::min(n - 1, last); ++i) {
        this->orbitals.push_back(i);
        tiles.push_back({this->tile_title(i), QString(), nullptr});
    }

    auto placeholder = std::make_shared<Frame>(this->structure, std::string());
    this->gallery->set_tiles(std::move(tiles), placeholder);

    this->next_tile = 0;
    this->progress->setRange(0, (int)this->orbitals.size());
    this->progress->setValue(0);
    this->start_next();
}

void OrbitalGalleryWindow::start_next() {
    const int total = (int)this->orbitals.size();
    this->button_export->setEnabled(this->next_tile >= total && total > 0);
    if(this->next_tile >= total) {
        this->progress->setVisible(false);
        this->label_status->setText(QString("%1 orbitals").arg(total));
        return;
    }

    this->progress->setVisible(true);
    this->progress->setValue(this->next_tile);
    this->label_status->setText(QString("Evaluating orbital %1 of %2...").arg(this->next_tile + 1).arg(total));

    const int gen = this->generation;
    const int tile = this->next_tile;
    auto basis = std::static_pointer_cast<const BasisSet>(this->result->basis);
    const auto coeff = this->result->orbital_sets[this->current_set()].coefficients[this->orbitals[tile]];
    const auto positions = this->atom_positions;
    const glm::vec3 shift = this->offset;
    OrbitalGridSettings grid;
    grid.spacing = (float)this->combo_grid->currentData().toDouble();
    const bool automatic = this->radio_auto->isChecked();
    const float fraction = this->spin_fraction->value() / 100.0f;
    const float iso = (float)this->spin_isovalue->value();

    this->watcher->setFuture(QtConcurrent::run([=]() {
        Build build;
        build.generation = gen;
        build.tile = tile;
        OrbitalBuilder builder(basis, positions, shift);
        const auto field = builder.build_field(coeff, grid);
        const float value = automatic ? OrbitalBuilder::suggest_isovalue(*field, fraction) : iso;
        build.meshes = OrbitalBuilder::build_meshes(*field, value);
        return build;
    }));
}

void OrbitalGalleryWindow::on_build_finished() {
    Build build = this->watcher->result();
    if(build.generation != this->generation) {
        return;     // superseded by a newer request
    }

    auto frame = std::make_shared<Frame>(this->structure, std::string());
    for(const auto* mesh : {&build.meshes.positive, &build.meshes.negative}) {
        if(mesh->empty()) {
            continue;
        }
        const QColor& color = mesh == &build.meshes.positive ? this->color_positive : this->color_negative;
        auto model = std::make_shared<Model>(mesh->positions, mesh->normals, mesh->indices);
        model->set_color(QVector4D(color.redF(), color.greenF(), color.blueF(), this->opacity));
        frame->add_model(model);
    }

    const OrbitalSet& set = this->result->orbital_sets[this->current_set()];
    const int orbital = this->orbitals[build.tile];
    const QString subtitle = QString("%1 Ht · occ. %2 · ±%3")
        .arg(set.energies[orbital], 0, 'f', 4)
        .arg(set.occupations[orbital], 0, 'f', 0)
        .arg(build.meshes.isovalue, 0, 'f', 3);
    this->gallery->set_tile(build.tile, frame, subtitle);

    this->next_tile++;
    this->start_next();
}

void OrbitalGalleryWindow::set_orientation(int alignment) {
    std::vector<QVector3D> positions;
    for(const auto& p : this->atom_positions) {
        positions.emplace_back(p.x, p.y, p.z);
    }
    this->gallery->set_rotation(Scene::alignment_rotation((CameraAlignment)alignment, positions));
}

// ---------------------------------------------------------------------------
// export
// ---------------------------------------------------------------------------
QString OrbitalGalleryWindow::file_name(const QString& prefix, int orbital) const {
    const OrbitalSet& set = this->result->orbital_sets[this->current_set()];
    const int width = QString::number(set.size()).size();
    QString name = QString("%1_%2_mo%3").arg(prefix, slug(set.label)).arg(orbital + 1, width, 10, QChar('0'));
    const QString label = set.frontier_label(orbital);
    if(!label.isEmpty()) {
        name += "_" + slug(label);
    }
    return name + ".png";
}

void OrbitalGalleryWindow::export_images() {
    if(!this->result || this->orbitals.empty()) {
        return;
    }

    QSettings settings;
    const QString default_dir = !this->job_dir.isEmpty() ? QDir(this->job_dir).filePath("orbitals") :
        settings.value("paths/last_image", QDir::homePath()).toString();

    // options
    QDialog dialog(this);
    dialog.setWindowTitle("Export orbital images");
    auto* form = new QFormLayout(&dialog);
    auto* info = new QLabel(QString("Saves the %1 orbitals shown in the gallery as separate PNG images, "
                                    "all seen from the current direction.").arg(this->orbitals.size()));
    info->setWordWrap(true);
    form->addRow(info);

    auto* ldir = new QHBoxLayout;
    auto* edit_dir = new QLineEdit(QDir::toNativeSeparators(settings.value("gallery/export_dir", default_dir).toString()));
    edit_dir->setMinimumWidth(320);
    auto* button_browse = new QPushButton(bluecurve_icon("folder"), "Browse...");
    ldir->addWidget(edit_dir, 1);
    ldir->addWidget(button_browse);
    form->addRow("Folder:", ldir);
    connect(button_browse, &QPushButton::clicked, &dialog, [&dialog, edit_dir]() {
        const QString dir = QFileDialog::getExistingDirectory(&dialog, "Export folder", edit_dir->text());
        if(!dir.isEmpty()) {
            edit_dir->setText(QDir::toNativeSeparators(dir));
        }
    });

    auto* edit_prefix = new QLineEdit(slug(this->result->molecule_name).isEmpty() ? "molecule" : slug(this->result->molecule_name));
    form->addRow("File name prefix:", edit_prefix);

    auto* combo_size = new QComboBox;
    for(int px : {512, 1024, 2048}) {
        combo_size->addItem(QString("%1 × %1 pixels").arg(px), px);
    }
    combo_size->setCurrentIndex(std::max(0, combo_size->findData(settings.value("gallery/export_size", 1024).toInt())));
    form->addRow("Image size:", combo_size);

    auto* check_white = new QCheckBox("White background");
    check_white->setChecked(settings.value("gallery/export_white", true).toBool());
    form->addRow(check_white);
    auto* check_label = new QCheckBox("Print orbital number and energy in the image");
    check_label->setChecked(settings.value("gallery/export_label", true).toBool());
    form->addRow(check_label);

    auto* example = new QLabel;
    example->setStyleSheet("color: gray;");
    form->addRow("Example:", example);
    auto update_example = [this, example, edit_prefix]() {
        example->setText(this->file_name(edit_prefix->text().trimmed(), this->orbitals.front()));
    };
    connect(edit_prefix, &QLineEdit::textChanged, &dialog, update_example);
    update_example();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if(dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString dir = QDir::fromNativeSeparators(edit_dir->text().trimmed());
    const QString prefix = edit_prefix->text().trimmed().isEmpty() ? QString("molecule") : edit_prefix->text().trimmed();
    const int pixels = combo_size->currentData().toInt();
    const bool white = check_white->isChecked();
    const bool label = check_label->isChecked();
    settings.setValue("gallery/export_dir", dir);
    settings.setValue("gallery/export_size", pixels);
    settings.setValue("gallery/export_white", white);
    settings.setValue("gallery/export_label", label);

    if(!QDir().mkpath(dir)) {
        QMessageBox::critical(this, "Export failed", QString("Cannot create the folder %1").arg(QDir::toNativeSeparators(dir)));
        return;
    }

    QProgressDialog progress_dialog("Saving images...", "Cancel", 0, (int)this->orbitals.size(), this);
    progress_dialog.setWindowModality(Qt::WindowModal);
    progress_dialog.setMinimumDuration(300);

    const QColor background = white ? QColor(Qt::white) : QColor(235, 235, 235);
    int saved = 0;
    for(int t = 0; t < (int)this->orbitals.size(); ++t) {
        progress_dialog.setValue(t);
        if(progress_dialog.wasCanceled()) {
            break;
        }

        QImage image = this->gallery->render_tile(t, pixels, background);
        if(image.isNull()) {
            continue;
        }

        if(label) {
            const auto& tile = this->gallery->tile(t);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            QFont font = this->font();
            font.setPixelSize(std::max(12, pixels / 28));
            painter.setFont(font);
            painter.setPen(QColor(30, 30, 30));
            const int margin = pixels / 40;
            QFont bold = font;
            bold.setBold(true);
            painter.setFont(bold);
            painter.drawText(QRect(margin, margin, pixels - 2 * margin, pixels), Qt::AlignLeft | Qt::AlignTop, tile.title);
            painter.setFont(font);
            painter.setPen(QColor(80, 80, 80));
            painter.drawText(QRect(margin, margin + QFontMetrics(bold).height(), pixels - 2 * margin, pixels),
                             Qt::AlignLeft | Qt::AlignTop, tile.subtitle);
        }

        const QString path = QDir(dir).filePath(this->file_name(prefix, this->orbitals[t]));
        if(!image.save(path)) {
            QMessageBox::critical(this, "Export failed", QString("Cannot write %1").arg(QDir::toNativeSeparators(path)));
            return;
        }
        saved++;
    }
    progress_dialog.setValue((int)this->orbitals.size());

    QMessageBox box(QMessageBox::Information, "Export finished",
                    QString("Saved %1 images to\n%2").arg(saved).arg(QDir::toNativeSeparators(dir)),
                    QMessageBox::Ok, this);
    QPushButton* button_open = box.addButton("Open folder", QMessageBox::ActionRole);
    box.exec();
    if(box.clickedButton() == button_open) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    }
}
