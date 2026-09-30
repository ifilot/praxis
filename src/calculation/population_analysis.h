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

#include <QString>
#include <vector>

#include "job_result.h"

/**
 * @brief Orbital-resolved population analysis for a pair of atoms
 *
 * For molecular orbital k and atoms A and B the population is
 *
 *     X_k = f * sum_{i in A} sum_{j in B} C_ik M_ij C_jk
 *
 * with M the Fock (MOHP), overlap (MOOP) or density (MOBI) matrix and
 * f = 2 for restricted orbitals (spin degeneracy) and f = 1 for the alpha
 * and beta orbitals of an unrestricted calculation. This is identical to
 * pyqint.PopulationAnalysis.mohp(), moop() and mobi(), and extends it to
 * unrestricted calculations using the spin-resolved matrices.
 */
namespace PopulationAnalysis {

enum class Kind {
    Hamilton,       // MOHP: negative = bonding
    Overlap,        // MOOP: positive = bonding
    BondIndex,      // MOBI: positive = bonding
};

struct PairPopulation {
    std::vector<double> values;     // one per orbital of the set
    double occupied_sum = 0.0;      // sum over the occupied orbitals
    QString matrix_key;             // matrix used, e.g. "fock"
    double spin_factor = 2.0;
};

/**
 * @brief Indices of the basis functions centered on an atom
 */
std::vector<int> basis_functions_on_atom(const BasisSet& basis, int atom);

/**
 * @brief Evaluate the population of all orbitals of a set for a pair of atoms
 *
 * @throws std::runtime_error when the atoms are invalid or identical, or the
 *         required matrix is not stored in the result
 */
PairPopulation evaluate(const JobResult& result, int set_index, int atom_a, int atom_b, Kind kind);

/**
 * @brief Short name, e.g. "MOHP"
 */
QString short_name(Kind kind);

/**
 * @brief Full name, e.g. "Molecular orbital Hamilton population"
 */
QString long_name(Kind kind);

/**
 * @brief Name of the PyQInt method, e.g. "mohp"
 */
QString pyqint_method(Kind kind);

/**
 * @brief +1 when positive values indicate bonding, -1 otherwise (MOHP)
 */
int bonding_sign(Kind kind);

/**
 * @brief Unit of the values ("Ht" for MOHP, empty otherwise)
 */
QString unit(Kind kind);

} // namespace PopulationAnalysis
