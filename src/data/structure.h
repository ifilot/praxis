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

#include <QDebug>
#include <QVector3D>
#include <QMatrix4x4>
#include <QGenericMatrix>
#include <vector>
#include <QString>

#include "atom_settings.h"
#include "atom.h"
#include "bond.h"

/**
 * @brief      This class describes a chemical structure.
 */
class Structure {

private:
    std::vector<Atom> atoms;            // atoms in the structure
    std::vector<Bond> bonds;            // bonds between the atoms
    std::vector<double> radii;          // radii of the atoms
    std::unordered_map<std::string, unsigned int> element_types;    // elements present in the structure

public:
    /**
     * @brief      Constructs a new instance.
     */
    Structure();

    /**
     * @brief      Get all atoms from the structure
     *
     * @return     The atoms.
     */
    inline const auto& get_atoms() const {
        return this->atoms;
    }

    /**
     * @brief      Get all bonds from the structure
     *
     * @return     The atoms.
     */
    inline const auto& get_bonds() const {
        return this->bonds;
    }

    /**
     * @brief      Get specific atom
     *
     * @param[in]  idx   The index
     *
     * @return     The atom.
     */
    inline const Atom& get_atom(unsigned int idx) const {
        return this->atoms[idx];
    }

    /**
     * @brief      Get specific bond
     *
     * @param[in]  idx   The index
     *
     * @return     The bond.
     */
    inline const Bond& get_bond(unsigned int idx) const {
        return this->bonds[idx];
    }

    /**
     * @brief      Gets the atomic radius.
     *
     * @param[in]  idx   The index
     *
     * @return     The radius.
     */
    inline double get_radius(unsigned int idx) const {
        return this->radii[idx];
    }

    /**
     * @brief      Add an atom to the structure
     *
     * @param[in]  atnr  Atom number
     * @param[in]  x     x coordinate
     * @param[in]  y     y coordinate
     * @param[in]  z     z coordinate
     */
    void add_atom(unsigned int atnr, double x, double y, double z);

    /**
     * @brief      Gets the total number of atoms.
     *
     * @return     The number of atoms
     */
    inline size_t get_nr_atoms() const {
        return this->atoms.size();
    }

    /**
     * @brief      Gets the total number of bonds.
     *
     * @return     The number of bonds
     */
    inline size_t get_nr_bonds() const {
        return this->bonds.size();
    }

    ~Structure() {
        qDebug() << "Deleting structure ("
                 << QString("0x%1").arg((size_t)this, 0, 16)
                 << "; " << this->atoms.size() << " atoms ).";
    }

    //********************************************
    // [END BLOCK] DATA GETTERS AND SETTERS
    //********************************************

    /**
     * @brief      Center the structure at the origin
     */
    void center();

    /**
     * @brief      Get the largest distance from the origin
     *
     * @return     The largest distance.
     */
    QVector3D get_largest_distance() const;

    /**
     * @brief      Gets the elements in this structure as a string
     *
     * @return     String holding comma seperated list of elements
     */
    std::string get_elements_string() const;

    /**
     * @brief      Update data based on contents;
     */
    void update();

    /**
     * @brief      Get the centering vector
     *
     * @return     Vector that puts unitcell at the origin
     */
    QVector3D get_center_vector() const;

    /**
     * @brief      Select atom by idx
     *
     * @param[in]  idx   The index
     */
    void select_atom(unsigned int idx);

private:
    /**
     * @brief      Count the number of elements
     */
    void count_elements();

    /**
     * @brief      Construct the bonds
     */
    void construct_bonds();
};
