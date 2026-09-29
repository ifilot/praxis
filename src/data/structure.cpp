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

#include "structure.h"

/**
 * @brief      Constructs a new instance.
 */
Structure::Structure() {

}

/**
 * @brief      Add an atom to the structure
 *
 * @param[in]  atnr  Atom number
 * @param[in]  x     x coordinate
 * @param[in]  y     y coordinate
 * @param[in]  z     z coordinate
 */
void Structure::add_atom(unsigned int atnr, double x, double y, double z) {
    this->atoms.emplace_back(atnr, x, y, z);
}

/**
 * @brief      Center the structure at the origin
 */
void Structure::center() {
    double sumx = 0.0;
    double sumy = 0.0;
    double sumz = 0.0;
    for(unsigned int i=0; i<this->atoms.size(); i++) {
        sumx += this->atoms[i].x;
    }
    for(unsigned int i=0; i<this->atoms.size(); i++) {
        sumy += this->atoms[i].y;
    }
    for(unsigned int i=0; i<this->atoms.size(); i++) {
        sumz += this->atoms[i].z;
    }

    sumx /= (float)this->atoms.size();
    sumy /= (float)this->atoms.size();
    sumz /= (float)this->atoms.size();

    auto cv = get_center_vector();
    for(unsigned int i=0; i<this->atoms.size(); i++) {
        this->atoms[i].x -= sumx + cv[0];
        this->atoms[i].y -= sumy + cv[1];
        this->atoms[i].z -= sumz + cv[2];
    }
}

/**
 * @brief      Get the largest distance from the origin
 *
 * @return     The largest distance.
 */
QVector3D Structure::get_largest_distance() const {
    unsigned int idx = 0;
    float dist = -1.0f;

    for(unsigned int i=0; i<this->atoms.size(); i++) {
        float vdist = this->atoms[i].get_pos_qtvec().lengthSquared();
        if(vdist > dist) {
            dist = vdist;
            idx = i;
        }
    }

    if(this->atoms.empty()) {
        return QVector3D(0, 0, 0);
    }

    return this->atoms[idx].get_pos_qtvec();
}

/**
 * @brief      Gets the elements in this structure as a string
 *
 * @return     String holding comma seperated list of elements
 */
std::string Structure::get_elements_string() const {
    std::string result;

    for(const auto& item : this->element_types) {
        result += QString("%1 (%2); ").arg(QString(item.first.c_str())).arg(item.second).toStdString();
    }

    // remove last two characters
    result.pop_back();
    result.pop_back();

    return result;
}

/**
 * @brief      Get the centering vector
 *
 * @return     Vector that puts unitcell at the origin
 */
QVector3D Structure::get_center_vector() const {
    return QVector3D(0,0,0);
}

/**
 * @brief      Update data based on contents;
 */
void Structure::update() {
    this->count_elements();
    this->construct_bonds();
}

/**
 * @brief      Count the number of elements
 */
void Structure::count_elements() {
    this->element_types.clear();

    for(const auto& atom : this->atoms) {
        std::string atomname = AtomSettings::get().get_name_from_elnr(atom.atnr);
        auto got = this->element_types.find(atomname);
        if(got != this->element_types.end()) {
            got->second++;
        } else {
            this->element_types.emplace(atomname, 1);
        }
    }
}

/**
 * @brief      Construct the bonds
 */
void Structure::construct_bonds() {
    this->bonds.clear();

    for(unsigned int i=0; i<this->atoms.size(); i++) {
        const auto& atom1 = this->atoms[i];
        for(unsigned int j=i+1; j<this->atoms.size(); j++) {
            const auto& atom2 = this->atoms[j];
            double maxdist2 = AtomSettings::get().get_bond_distance(atom1.atnr, atom2.atnr);

            double dist2 = atom1.dist(atom2);

            // check if atoms are bonded
            if(dist2 < maxdist2) {
                this->bonds.emplace_back(atom1, atom2);
            }
        }
    }
}
