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

#include "population_analysis.h"

#include <stdexcept>

namespace PopulationAnalysis {

std::vector<int> basis_functions_on_atom(const BasisSet& basis, int atom) {
    std::vector<int> result;
    for(size_t i = 0; i < basis.size(); ++i) {
        if(basis[i].atom == atom) {
            result.push_back((int)i);
        }
    }
    return result;
}

PairPopulation evaluate(const JobResult& result, int set_index, int atom_a, int atom_b, Kind kind) {
    const int natoms = (int)result.positions.size();
    if(atom_a < 0 || atom_a >= natoms || atom_b < 0 || atom_b >= natoms) {
        throw std::runtime_error("Invalid atom index.");
    }
    if(atom_a == atom_b) {
        throw std::runtime_error("Population analysis requires two distinct atoms.");
    }
    if(set_index < 0 || set_index >= (int)result.orbital_sets.size()) {
        throw std::runtime_error("Invalid orbital set.");
    }

    const OrbitalSet& set = result.orbital_sets[set_index];
    const QString suffix = set.spin == "alpha" ? "_alpha" : (set.spin == "beta" ? "_beta" : "");

    PairPopulation pop;
    pop.spin_factor = suffix.isEmpty() ? 2.0 : 1.0;
    switch(kind) {
        case Kind::Hamilton:  pop.matrix_key = "fock" + suffix; break;
        case Kind::Overlap:   pop.matrix_key = "overlap"; break;
        case Kind::BondIndex: pop.matrix_key = "density" + suffix; break;
    }

    const DenseMatrix* M = result.find_matrix(pop.matrix_key);
    const int nbf = (int)result.basis->size();
    if(M == nullptr || M->rows != nbf || M->cols != nbf) {
        throw std::runtime_error(QString("The result file does not contain the matrix '%1'.")
                                 .arg(pop.matrix_key).toStdString());
    }

    const std::vector<int> ia = basis_functions_on_atom(*result.basis, atom_a);
    const std::vector<int> ib = basis_functions_on_atom(*result.basis, atom_b);

    pop.values.resize(set.size(), 0.0);
    for(size_t k = 0; k < set.size(); ++k) {
        const std::vector<double>& c = set.coefficients[k];
        double sum = 0.0;
        for(int i : ia) {
            for(int j : ib) {
                sum += c[i] * (*M)(i, j) * c[j];
            }
        }
        pop.values[k] = pop.spin_factor * sum;
        if(set.occupations[k] > 0.0) {
            pop.occupied_sum += pop.values[k];
        }
    }

    return pop;
}

QString short_name(Kind kind) {
    switch(kind) {
        case Kind::Hamilton:  return "MOHP";
        case Kind::Overlap:   return "MOOP";
        case Kind::BondIndex: return "MOBI";
    }
    return QString();
}

QString long_name(Kind kind) {
    switch(kind) {
        case Kind::Hamilton:  return "Molecular orbital Hamilton population";
        case Kind::Overlap:   return "Molecular orbital overlap population";
        case Kind::BondIndex: return "Molecular orbital bond index";
    }
    return QString();
}

QString pyqint_method(Kind kind) {
    return short_name(kind).toLower();
}

int bonding_sign(Kind kind) {
    return kind == Kind::Hamilton ? -1 : 1;
}

QString unit(Kind kind) {
    return kind == Kind::Hamilton ? "Ht" : QString();
}

} // namespace PopulationAnalysis
