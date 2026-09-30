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

#include "viewer_controller.h"

#include <QtConcurrent/QtConcurrent>

#include "anaglyph_widget.h"
#include "data/frame.h"
#include "data/model.h"
#include "data/structure.h"

ViewerController::ViewerController(AnaglyphWidget* _widget, QObject* parent) :
    QObject(parent),
    widget(_widget) {
    this->watcher = new QFutureWatcher<OrbitalBuild>(this);
    connect(this->watcher, &QFutureWatcher<OrbitalBuild>::finished, this, &ViewerController::on_build_finished);
}

void ViewerController::set_structure(const Molecule& mol, bool fit_camera) {
    this->molecule = mol;
    this->structure = mol.to_structure(&this->offset);

    this->atom_positions.clear();
    float radius = 0.0f;
    for(const auto& atom : this->structure->get_atoms()) {
        const glm::vec3 p((float)atom.x, (float)atom.y, (float)atom.z);
        this->atom_positions.push_back(p);
        radius = std::max(radius, glm::length(p));
    }

    if(fit_camera) {
        this->widget->fit_camera(radius);
    }
}

void ViewerController::show_molecule(const Molecule& mol, bool fit_camera) {
    this->result.reset();
    this->hide_orbital();
    this->set_structure(mol, fit_camera);
    this->update_frame();
}

void ViewerController::show_result(std::shared_ptr<JobResult> _result, bool fit_camera) {
    this->result = std::move(_result);
    this->set_index = -1;
    this->orbital_index = -1;
    this->field.reset();
    this->meshes = OrbitalMeshes();
    this->generation++;
    this->set_structure(this->result->get_molecule(), fit_camera);
    this->update_frame();
}

void ViewerController::show_trajectory_frame(size_t frame) {
    if(!this->result) {
        return;
    }

    // orbitals belong to the final geometry only
    this->hide_orbital();
    this->set_structure(this->result->get_trajectory_molecule(frame), false);
    this->update_frame();
}

void ViewerController::show_orbital(int _set_index, int _orbital_index) {
    if(!this->result ||
       _set_index < 0 || _set_index >= (int)this->result->orbital_sets.size() ||
       _orbital_index < 0 || _orbital_index >= (int)this->result->orbital_sets[_set_index].size()) {
        this->hide_orbital();
        return;
    }

    // make sure the final geometry is shown
    if(this->molecule.size() != this->result->positions.size() || this->set_index < 0) {
        this->set_structure(this->result->get_molecule(), false);
    }

    this->set_index = _set_index;
    this->orbital_index = _orbital_index;
    this->rebuild(false);
}

void ViewerController::hide_orbital() {
    this->generation++;     // discard pending builds
    this->set_index = -1;
    this->orbital_index = -1;
    this->field.reset();
    this->meshes = OrbitalMeshes();
    if(this->structure) {
        this->update_frame();
    }
    emit busy_changed(false, QString());
}

void ViewerController::set_isovalue(float _isovalue, bool automatic) {
    this->isovalue = std::abs(_isovalue);
    this->auto_isovalue = automatic;
    if(this->field) {
        this->rebuild(true);
    }
}

void ViewerController::set_enclosed_fraction(float fraction) {
    this->enclosed_fraction = std::clamp(fraction, 0.05f, 0.999f);
    if(this->field && this->auto_isovalue) {
        this->rebuild(true);
    }
}

void ViewerController::set_grid_spacing(float spacing) {
    this->grid.spacing = spacing;
    if(this->set_index >= 0) {
        this->rebuild(false);
    }
}

void ViewerController::set_opacity(float _opacity) {
    this->opacity = std::clamp(_opacity, 0.05f, 1.0f);
    this->update_frame();
}

void ViewerController::set_colors(const QColor& positive, const QColor& negative) {
    this->color_positive = positive;
    this->color_negative = negative;
    this->update_frame();
}

void ViewerController::set_highlighted_atoms(const std::vector<int>& atoms) {
    this->highlighted_atoms = atoms;
    if(this->structure) {
        this->update_frame();
    }
}

void ViewerController::rebuild(bool reuse_field) {
    if(!this->result || this->set_index < 0) {
        return;
    }

    const int gen = ++this->generation;

    auto basis = std::static_pointer_cast<const BasisSet>(this->result->basis);
    const auto coeff = this->result->orbital_sets[this->set_index].coefficients[this->orbital_index];
    const auto positions = this->atom_positions;
    const glm::vec3 shift = this->offset;
    const OrbitalGridSettings settings = this->grid;
    const bool automatic = this->auto_isovalue;
    const float fraction = this->enclosed_fraction;
    const float iso = this->isovalue;
    std::shared_ptr<ScalarField> existing = reuse_field ? this->field : nullptr;

    emit busy_changed(true, existing ? "Constructing isosurface..." : "Evaluating orbital on grid...");

    this->watcher->setFuture(QtConcurrent::run([=]() {
        OrbitalBuild build;
        build.generation = gen;
        build.field = existing;
        if(!build.field) {
            OrbitalBuilder builder(basis, positions, shift);
            build.field = builder.build_field(coeff, settings);
        }
        const float value = automatic ? OrbitalBuilder::suggest_isovalue(*build.field, fraction) : iso;
        build.meshes = OrbitalBuilder::build_meshes(*build.field, value);
        return build;
    }));
}

void ViewerController::on_build_finished() {
    OrbitalBuild build = this->watcher->result();
    if(build.generation != this->generation) {
        return;     // superseded by a newer request
    }

    this->field = build.field;
    this->meshes = std::move(build.meshes);
    this->isovalue = this->meshes.isovalue;
    this->update_frame();

    const int ntri = (int)((this->meshes.positive.indices.size() + this->meshes.negative.indices.size()) / 3);
    emit busy_changed(false, QString());
    emit orbital_shown(this->isovalue, ntri);
}

void ViewerController::update_frame() {
    if(!this->structure) {
        this->widget->set_frame(nullptr);
        return;
    }

    auto frame = std::make_shared<Frame>(this->structure, this->molecule.get_name().toStdString());

    auto add_mesh = [&](const IsoMesh& mesh, const QColor& color) {
        if(mesh.empty()) {
            return;
        }
        auto model = std::make_shared<Model>(mesh.positions, mesh.normals, mesh.indices);
        model->set_color(QVector4D(color.redF(), color.greenF(), color.blueF(), this->opacity));
        frame->add_model(model);
    };

    if(this->set_index >= 0) {
        add_mesh(this->meshes.positive, this->color_positive);
        add_mesh(this->meshes.negative, this->color_negative);
    }

    for(int idx : this->highlighted_atoms) {
        if(idx < 0 || idx >= (int)this->atom_positions.size()) {
            continue;
        }
        const float radius = AtomSettings::get().get_atom_radius_from_elnr(this->structure->get_atom(idx).atnr);
        const IsoMesh halo = sphere_mesh(this->atom_positions[idx], radius * 1.35f + 0.15f);
        auto model = std::make_shared<Model>(halo.positions, halo.normals, halo.indices);
        model->set_color(QVector4D(this->color_highlight.redF(), this->color_highlight.greenF(),
                                   this->color_highlight.blueF(), 0.45f));
        frame->add_model(model);
    }

    this->widget->set_frame(frame);
}
