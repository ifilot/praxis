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

#pragma once

#include <QVector3D>
#include <array>

enum {
    ATOM_CENTRAL_UNITCELL,
    ATOM_EXPANSION_XY,
    ATOM_EXPANSION_Z
};

class Atom {
public:
    unsigned int atnr;
    double x,y,z;

    Atom(unsigned int _atnr, double _x, double _y, double _z);

    /**
     * @brief      Gets the position as a QVector3D vector.
     *
     * @return     The position.
     */
    inline QVector3D get_pos_qtvec() const {
        return QVector3D(this->x, this->y, this->z);
    }

    /**
     * @brief      Distance between two atoms
     *
     * @param[in]  other  The other atom
     *
     * @return     Distance
     */
    double dist(const Atom& other) const;

    /**
     * @brief      Squared distance between two atoms
     *
     * @param[in]  other  The other atom
     *
     * @return     Squared distance
     */
    double dist2(const Atom& other) const;

    /**
     * @brief      Translate an atom
     *
     * @param[in]  dx    translation x
     * @param[in]  dy    translation y
     * @param[in]  dz    translation z
     */
    inline void translate(double dx, double dy, double dz) {
        this->x += dx;
        this->y += dy;
        this->z += dz;
    }

    /**
     * @brief      Select this atom
     */
    void select_atom();

private:
};
