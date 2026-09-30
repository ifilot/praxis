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

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "scalar_field.h"

/**
 * @brief Indexed triangle mesh
 */
struct IsoMesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;

    inline bool empty() const {
        return this->indices.empty();
    }
};

/**
 * @brief Construct the isosurface of a scalar field
 *
 * Vertices are shared between adjacent cubes (keyed on the grid edge on which
 * they lie), normals are derived from the gradient of the field and point
 * away from the enclosed region. For a positive isovalue the enclosed region
 * is where the field exceeds the isovalue; for a negative isovalue it is
 * where the field is below the isovalue. Triangles are wound
 * counter-clockwise when viewed from the outside.
 *
 * @param field     scalar field
 * @param isovalue  isovalue
 * @return IsoMesh
 */
IsoMesh marching_cubes(const ScalarField& field, float isovalue);

/**
 * @brief Closed, outward-oriented sphere mesh (e.g. to highlight an atom)
 *
 * @param center    center of the sphere
 * @param radius    radius
 * @param stacks    number of divisions from pole to pole (>= 2)
 * @param slices    number of divisions around the polar axis (>= 3)
 */
IsoMesh sphere_mesh(const glm::vec3& center, float radius, unsigned int stacks = 16, unsigned int slices = 24);
