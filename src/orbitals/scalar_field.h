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

#include <array>
#include <cstddef>
#include <vector>
#include <glm/glm.hpp>

/**
 * @brief Scalar field sampled on a regular, axis-aligned grid
 *
 * Grid point (i,j,k) is located at origin + spacing * (i,j,k). Values are
 * stored with i running fastest.
 */
class ScalarField {
private:
    glm::vec3 origin;
    float spacing;
    std::array<unsigned int, 3> dims;
    std::vector<float> values;

public:
    ScalarField(const glm::vec3& _origin, float _spacing, const std::array<unsigned int, 3>& _dims);

    inline const glm::vec3& get_origin() const {
        return this->origin;
    }

    inline float get_spacing() const {
        return this->spacing;
    }

    inline const std::array<unsigned int, 3>& get_dims() const {
        return this->dims;
    }

    inline size_t index(unsigned int i, unsigned int j, unsigned int k) const {
        return ((size_t)k * this->dims[1] + j) * this->dims[0] + i;
    }

    inline float get(unsigned int i, unsigned int j, unsigned int k) const {
        return this->values[this->index(i, j, k)];
    }

    inline void set(unsigned int i, unsigned int j, unsigned int k, float v) {
        this->values[this->index(i, j, k)] = v;
    }

    inline glm::vec3 position(unsigned int i, unsigned int j, unsigned int k) const {
        return this->origin + this->spacing * glm::vec3((float)i, (float)j, (float)k);
    }

    inline size_t size() const {
        return this->values.size();
    }

    inline std::vector<float>& data() {
        return this->values;
    }

    inline const std::vector<float>& data() const {
        return this->values;
    }

    /**
     * @brief Gradient at a grid point using central differences (one-sided
     *        at the boundaries)
     */
    glm::vec3 gradient(unsigned int i, unsigned int j, unsigned int k) const;

    /**
     * @brief Maximum absolute value on the grid
     */
    float max_abs() const;
};
