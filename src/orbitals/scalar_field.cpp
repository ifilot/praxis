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

#include "scalar_field.h"

#include <algorithm>
#include <cmath>

ScalarField::ScalarField(const glm::vec3& _origin, float _spacing, const std::array<unsigned int, 3>& _dims) :
    origin(_origin),
    spacing(_spacing),
    dims(_dims) {
    this->values.resize((size_t)dims[0] * dims[1] * dims[2], 0.0f);
}

glm::vec3 ScalarField::gradient(unsigned int i, unsigned int j, unsigned int k) const {
    auto diff = [&](unsigned int axis) {
        std::array<unsigned int, 3> lo = {i, j, k};
        std::array<unsigned int, 3> hi = {i, j, k};
        if(lo[axis] > 0) {
            lo[axis]--;
        }
        if(hi[axis] + 1 < this->dims[axis]) {
            hi[axis]++;
        }
        const float h = (float)(hi[axis] - lo[axis]) * this->spacing;
        if(h <= 0.0f) {
            return 0.0f;
        }
        return (this->get(hi[0], hi[1], hi[2]) - this->get(lo[0], lo[1], lo[2])) / h;
    };

    return glm::vec3(diff(0), diff(1), diff(2));
}

float ScalarField::max_abs() const {
    float result = 0.0f;
    for(float v : this->values) {
        result = std::max(result, std::abs(v));
    }
    return result;
}
