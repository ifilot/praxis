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

#include <string>
#include <vector>
#include <glm/glm.hpp>

/**
 * @brief Primitive Cartesian Gaussian c * N * x^l y^m z^n exp(-alpha r^2)
 *
 * The normalization constant N is computed on construction using the same
 * expression as PyQInt so that orbitals evaluated here are identical to the
 * ones PyQInt would produce.
 */
struct GaussianPrimitive {
    double c = 0.0;         // contraction coefficient (as stored by PyQInt)
    double alpha = 0.0;     // exponent
    int l = 0, m = 0, n = 0;
    double norm = 0.0;      // normalization constant

    GaussianPrimitive() = default;
    GaussianPrimitive(double _c, double _alpha, int _l, int _m, int _n);
};

/**
 * @brief Contracted Gaussian function centered on an atom
 */
struct BasisFunction {
    glm::dvec3 center;      // bohr
    int atom = -1;          // index of the atom this function belongs to
    std::string label;      // e.g. "O1 px"
    std::vector<GaussianPrimitive> gtos;

    /**
     * @brief Largest distance (bohr) from the center where this function can
     *        still exceed the threshold (used for screening)
     */
    double cutoff_radius(double threshold = 1e-8) const;
};

class BasisSet {
private:
    std::vector<BasisFunction> functions;
    std::vector<double> cutoff2;    // squared cutoff radii for screening

public:
    BasisSet() = default;

    void add(const BasisFunction& bf);

    inline size_t size() const {
        return this->functions.size();
    }

    inline const BasisFunction& operator[](size_t i) const {
        return this->functions[i];
    }

    inline const auto& get_functions() const {
        return this->functions;
    }

    /**
     * @brief Evaluate the linear combination sum_i coeff[i] * phi_i(r)
     *
     * @param r        position (bohr)
     * @param coeff    coefficients (one per basis function)
     */
    double evaluate(const glm::dvec3& r, const std::vector<double>& coeff) const;

    /**
     * @brief Evaluate a single basis function at position r (bohr)
     */
    static double evaluate_function(const BasisFunction& bf, const glm::dvec3& r);
};
