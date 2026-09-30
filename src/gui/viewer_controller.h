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

#include <memory>
#include <vector>

#include <QColor>
#include <QFutureWatcher>
#include <QObject>
#include <glm/glm.hpp>

#include "calculation/job_result.h"
#include "data/molecule.h"
#include "orbitals/orbital_builder.h"

class AnaglyphWidget;
class Structure;

/**
 * @brief Decides what is shown in the 3D viewer
 *
 * Shows a molecule, optionally together with the isosurfaces of a molecular
 * orbital. Orbital grids are evaluated in a background thread; the grid is
 * cached such that changing the isovalue only re-runs marching cubes.
 */
class ViewerController : public QObject {
    Q_OBJECT

public:
    struct OrbitalBuild {
        std::shared_ptr<ScalarField> field;
        OrbitalMeshes meshes;
        int generation = 0;
    };

private:
    AnaglyphWidget* widget;

    Molecule molecule;
    std::shared_ptr<Structure> structure;
    glm::vec3 offset = glm::vec3(0.0f);
    std::vector<glm::vec3> atom_positions;      // viewer frame

    std::shared_ptr<JobResult> result;

    // orbital state
    int set_index = -1;
    int orbital_index = -1;
    std::shared_ptr<ScalarField> field;
    OrbitalMeshes meshes;
    OrbitalGridSettings grid;
    float isovalue = 0.05f;
    bool auto_isovalue = true;
    float enclosed_fraction = 0.90f;
    float opacity = 0.80f;
    QColor color_positive = QColor(30, 120, 220);
    QColor color_negative = QColor(230, 75, 50);

    std::vector<int> highlighted_atoms;
    QColor color_highlight = QColor(255, 193, 7);

    QFutureWatcher<OrbitalBuild>* watcher;
    int generation = 0;

public:
    explicit ViewerController(AnaglyphWidget* widget, QObject* parent = nullptr);

    /**
     * @brief Show a molecule without any orbitals
     */
    void show_molecule(const Molecule& mol, bool fit_camera = true);

    /**
     * @brief Show the final geometry of a calculation
     */
    void show_result(std::shared_ptr<JobResult> result, bool fit_camera = true);

    /**
     * @brief Show a frame of a geometry optimization trajectory
     */
    void show_trajectory_frame(size_t frame);

    /**
     * @brief Show an orbital of the current result
     */
    void show_orbital(int set_index, int orbital_index);

    void hide_orbital();

    void set_isovalue(float isovalue, bool automatic);
    void set_enclosed_fraction(float fraction);
    void set_grid_spacing(float spacing);
    void set_opacity(float opacity);
    void set_colors(const QColor& positive, const QColor& negative);

    /**
     * @brief Draw a translucent halo around these atoms
     */
    void set_highlighted_atoms(const std::vector<int>& atoms);

    inline const QColor& get_color_highlight() const {
        return this->color_highlight;
    }

    inline const OrbitalGridSettings& get_grid_settings() const {
        return this->grid;
    }

    inline bool is_auto_isovalue() const {
        return this->auto_isovalue;
    }

    inline float get_enclosed_fraction() const {
        return this->enclosed_fraction;
    }

    inline float get_opacity() const {
        return this->opacity;
    }

    inline const QColor& get_color_positive() const {
        return this->color_positive;
    }

    inline const QColor& get_color_negative() const {
        return this->color_negative;
    }

    inline float get_isovalue() const {
        return this->isovalue;
    }

    inline const Molecule& get_molecule() const {
        return this->molecule;
    }

signals:
    void busy_changed(bool busy, const QString& message);
    void orbital_shown(float isovalue, int nr_triangles);

private:
    void set_structure(const Molecule& mol, bool fit_camera);
    void rebuild(bool reuse_field);
    void update_frame();

private slots:
    void on_build_finished();
};
