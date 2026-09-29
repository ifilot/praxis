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
#include <glm/glm.hpp>

#include "basis_set.h"
#include "marching_cubes.h"
#include "scalar_field.h"

struct OrbitalGridSettings {
    float padding = 3.0f;       // extra space around the molecule (angstrom)
    float spacing = 0.10f;      // grid spacing (angstrom)
};

struct OrbitalMeshes {
    IsoMesh positive;
    IsoMesh negative;
    float isovalue = 0.0f;
};

/**
 * @brief Evaluates molecular orbitals on a grid and builds their isosurfaces
 *
 * All coordinates handed to and returned from this class are in the frame of
 * the 3D viewer: angstrom, shifted by -offset with respect to the coordinates
 * of the calculation. The basis set itself is in bohr (as PyQInt).
 */
class OrbitalBuilder {
private:
    std::shared_ptr<const BasisSet> basis;
    glm::vec3 offset;           // viewer frame = molecule frame (angstrom) - offset
    glm::vec3 box_min;          // bounding box in viewer frame (angstrom)
    glm::vec3 box_max;

public:
    /**
     * @param basis             basis set (bohr)
     * @param atom_positions    atom positions in the viewer frame (angstrom)
     * @param offset            translation applied to go to the viewer frame (angstrom)
     */
    OrbitalBuilder(std::shared_ptr<const BasisSet> basis,
                   const std::vector<glm::vec3>& atom_positions,
                   const glm::vec3& offset);

    /**
     * @brief Evaluate the orbital with the given coefficients on a grid
     */
    std::unique_ptr<ScalarField> build_field(const std::vector<double>& coefficients,
                                             const OrbitalGridSettings& settings) const;

    /**
     * @brief Construct the positive and negative isosurfaces of a field
     */
    static OrbitalMeshes build_meshes(const ScalarField& field, float isovalue);

    /**
     * @brief Isovalue such that the enclosed volume holds the given fraction
     *        of the orbital density |psi|^2 (on the grid)
     */
    static float suggest_isovalue(const ScalarField& field, float fraction = 0.90f);
};
