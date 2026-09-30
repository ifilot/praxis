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

#include "orbital_builder.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include <QtConcurrent/QtConcurrent>

#include "data/units.h"

OrbitalBuilder::OrbitalBuilder(std::shared_ptr<const BasisSet> _basis,
                               const std::vector<glm::vec3>& atom_positions,
                               const glm::vec3& _offset) :
    basis(std::move(_basis)),
    offset(_offset),
    box_min(0.0f),
    box_max(0.0f) {

    if(!atom_positions.empty()) {
        this->box_min = atom_positions.front();
        this->box_max = atom_positions.front();
        for(const auto& p : atom_positions) {
            this->box_min = glm::min(this->box_min, p);
            this->box_max = glm::max(this->box_max, p);
        }
    }
}

std::unique_ptr<ScalarField> OrbitalBuilder::build_field(const std::vector<double>& coefficients,
                                                         const OrbitalGridSettings& settings) const {
    const float spacing = std::max(settings.spacing, 0.01f);
    const glm::vec3 lo = this->box_min - glm::vec3(settings.padding);
    const glm::vec3 hi = this->box_max + glm::vec3(settings.padding);

    std::array<unsigned int, 3> dims;
    for(int a = 0; a < 3; ++a) {
        dims[a] = std::max(2u, (unsigned int)std::ceil((hi[a] - lo[a]) / spacing) + 1u);
    }

    auto field = std::make_unique<ScalarField>(lo, spacing, dims);

    // evaluate slice by slice in parallel
    std::vector<unsigned int> slices(dims[2]);
    std::iota(slices.begin(), slices.end(), 0u);

    ScalarField* fptr = field.get();
    const BasisSet* bptr = this->basis.get();
    const glm::vec3 shift = this->offset;

    QtConcurrent::blockingMap(slices, [fptr, bptr, &coefficients, &dims, shift](unsigned int k) {
        for(unsigned int j = 0; j < dims[1]; ++j) {
            for(unsigned int i = 0; i < dims[0]; ++i) {
                const glm::dvec3 r = glm::dvec3(fptr->position(i, j, k) + shift) * ANGSTROM_TO_BOHR;
                fptr->set(i, j, k, (float)bptr->evaluate(r, coefficients));
            }
        }
    });

    return field;
}

OrbitalMeshes OrbitalBuilder::build_meshes(const ScalarField& field, float isovalue) {
    OrbitalMeshes meshes;
    meshes.isovalue = std::abs(isovalue);

    // build both lobes concurrently
    QFuture<IsoMesh> fpos = QtConcurrent::run([&field, isovalue]() {
        return marching_cubes(field, std::abs(isovalue));
    });
    meshes.negative = marching_cubes(field, -std::abs(isovalue));
    meshes.positive = fpos.result();

    return meshes;
}

float OrbitalBuilder::suggest_isovalue(const ScalarField& field, float fraction) {
    std::vector<float> dens(field.data().size());
    std::transform(field.data().begin(), field.data().end(), dens.begin(),
                   [](float v) { return v * v; });
    std::sort(dens.begin(), dens.end(), std::greater<float>());

    const double total = std::accumulate(dens.begin(), dens.end(), 0.0);
    if(total <= 0.0) {
        return 0.05f;
    }

    double cumsum = 0.0;
    for(float d : dens) {
        cumsum += d;
        if(cumsum >= fraction * total) {
            return std::sqrt(d);
        }
    }
    return std::sqrt(dens.back());
}
