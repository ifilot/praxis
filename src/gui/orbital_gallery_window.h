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

#include <memory>
#include <vector>

#include <QColor>
#include <QFutureWatcher>
#include <QWidget>
#include <glm/glm.hpp>

#include "calculation/job_result.h"
#include "orbitals/orbital_builder.h"

class AnaglyphWidget;
class OrbitalGalleryWidget;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QScrollBar;
class QSlider;
class QSpinBox;
class Structure;
class ViewerController;

/**
 * @brief Window showing all (or a range of) molecular orbitals side by side
 *
 * The orbitals are evaluated one after the other in the background and
 * appear as soon as they are ready. All orbitals are shown from the same
 * direction and can be exported as one PNG image per orbital.
 */
class OrbitalGalleryWindow : public QWidget {
    Q_OBJECT

public:
    struct Build {
        int generation = 0;
        int tile = 0;
        OrbitalMeshes meshes;
    };

private:
    AnaglyphWidget* main_viewer;

    std::shared_ptr<JobResult> result;
    QString job_dir;
    std::shared_ptr<Structure> structure;
    std::vector<glm::vec3> atom_positions;     // viewer frame (angstrom)
    glm::vec3 offset = glm::vec3(0.0f);
    std::vector<int> orbitals;                  // orbital indices of the tiles

    float opacity = 0.8f;
    QColor color_positive;
    QColor color_negative;

    QFutureWatcher<Build>* watcher;
    int generation = 0;
    int next_tile = 0;

    QComboBox* combo_set;
    QComboBox* combo_range;
    QSpinBox* spin_from;
    QSpinBox* spin_to;
    QRadioButton* radio_auto;
    QRadioButton* radio_manual;
    QSpinBox* spin_fraction;
    QDoubleSpinBox* spin_isovalue;
    QComboBox* combo_grid;
    QComboBox* combo_projection;
    QSlider* slider_size;
    QPushButton* button_export;

    OrbitalGalleryWidget* gallery;
    QScrollBar* scrollbar;
    QLabel* label_status;
    QProgressBar* progress;

public:
    /**
     * @param main_viewer   viewer of the main window (initial orientation)
     * @param settings      initial isosurface settings and colors
     */
    OrbitalGalleryWindow(AnaglyphWidget* main_viewer, const ViewerController* settings, QWidget* parent = nullptr);
    ~OrbitalGalleryWindow();

    /**
     * @brief Show the orbitals of a result
     *
     * @param set_index     orbital set to show initially
     */
    void set_result(std::shared_ptr<JobResult> result, const QString& job_dir, int set_index);

signals:
    /**
     * @brief The user double-clicked an orbital
     */
    void orbital_activated(int set_index, int orbital_index);

private:
    QWidget* build_controls();
    QWidget* build_view_toolbar();

    void update_range_controls();
    void rebuild();
    void start_next();
    void set_orientation(int alignment);
    void update_scrollbar();
    void export_images();

    int current_set() const;
    QString tile_title(int orbital) const;
    QString file_name(const QString& prefix, int orbital) const;

private slots:
    void on_build_finished();
};
