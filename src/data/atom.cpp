/**************************************************************************
 *   This file is part of MANAGLYPH.                                      *
 *                                                                        *
 *   Author: Ivo Filot <ivo@ivofilot.nl>                                  *
 *                                                                        *
 *   MANAGLYPH is free software:                                          *
 *   you can redistribute it and/or modify it under the terms of the      *
 *   GNU General Public License as published by the Free Software         *
 *   Foundation, either version 3 of the License, or (at your option)     *
 *   any later version.                                                   *
 *                                                                        *
 *   MANAGLYPH is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "atom.h"
#include <cmath>

Atom::Atom(unsigned int _atnr, double _x, double _y, double _z) :
atnr(_atnr),
x(_x),
y(_y),
z(_z) {

}

/**
 * @brief      Distance between two atoms
 *
 * @param[in]  other  The other atom
 *
 * @return     Distance
 */
double Atom::dist(const Atom& other) const {
    return std::sqrt(this->dist2(other));
}

/**
 * @brief      Squared distance between two atoms
 *
 * @param[in]  other  The other atom
 *
 * @return     Squared distance
 */
double Atom::dist2(const Atom& other) const {
    return (this->x - other.x) * (this->x - other.x) +
           (this->y - other.y) * (this->y - other.y) +
           (this->z - other.z) * (this->z - other.z);
}
