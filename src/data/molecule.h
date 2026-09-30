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

#include <memory>
#include <vector>

#include <QString>
#include <glm/glm.hpp>

class Structure;

struct MoleculeAtom {
    QString element;
    glm::dvec3 position;    // angstrom
};

/**
 * @brief Molecular geometry as used for setting up a calculation
 *
 * Coordinates are stored in angstrom (as in .xyz files); conversion to bohr
 * is left to PyQInt.
 */
class Molecule {
private:
    QString name;
    std::vector<MoleculeAtom> atoms;

public:
    Molecule() = default;
    explicit Molecule(const QString& _name);

    /**
     * @brief Parse a molecule from the contents of an .xyz file
     *
     * @throws std::runtime_error on malformed input
     */
    static Molecule from_xyz_string(const QString& content, const QString& fallback_name);

    /**
     * @brief Load a molecule from an .xyz file (also works for Qt resources)
     *
     * @throws std::runtime_error when the file cannot be read or parsed
     */
    static Molecule from_xyz_file(const QString& path);

    inline const QString& get_name() const {
        return this->name;
    }

    inline void set_name(const QString& _name) {
        this->name = _name;
    }

    inline const std::vector<MoleculeAtom>& get_atoms() const {
        return this->atoms;
    }

    inline size_t size() const {
        return this->atoms.size();
    }

    inline bool empty() const {
        return this->atoms.empty();
    }

    void add_atom(const QString& element, const glm::dvec3& position);

    /**
     * @brief Sum of the nuclear charges
     */
    int nuclear_charge() const;

    /**
     * @brief Atomic number of an element symbol
     */
    static int atomic_number(const QString& element);

    /**
     * @brief Hill-ordered chemical formula (e.g. "CH4", "H2O")
     */
    QString formula() const;

    /**
     * @brief Chemical formula as HTML with subscripts, e.g. "C<sub>6</sub>H<sub>6</sub>"
     */
    QString formula_html() const;

    /**
     * @brief Geometric center (angstrom)
     */
    glm::dvec3 centroid() const;

    /**
     * @brief Serialize as .xyz file contents
     */
    QString to_xyz() const;

    /**
     * @brief Build a structure for the viewer, centered at the origin
     *
     * @param offset  (output) centroid that was subtracted (angstrom)
     */
    std::shared_ptr<Structure> to_structure(glm::vec3* offset = nullptr) const;
};
