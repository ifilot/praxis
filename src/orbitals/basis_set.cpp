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

#include "basis_set.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

namespace {

double double_factorial(int n) {
    double result = 1.0;
    for(int i = n; i > 1; i -= 2) {
        result *= (double)i;
    }
    return result;
}

inline double ipow(double x, int n) {
    double result = 1.0;
    for(int i = 0; i < n; ++i) {
        result *= x;
    }
    return result;
}

} // namespace

/**
 * @brief Construct primitive and calculate normalization constant such that
 *        <GTO|GTO> = 1 (identical to PyQInt's GTO::calculate_normalization_constant)
 */
GaussianPrimitive::GaussianPrimitive(double _c, double _alpha, int _l, int _m, int _n) :
    c(_c), alpha(_alpha), l(_l), m(_m), n(_n) {

    const int lsum = l + m + n;
    const double nom = std::pow(2.0, 2.0 * lsum + 1.5) * std::pow(alpha, lsum + 1.5);
    const double denom = (l < 1 ? 1.0 : double_factorial(2 * l - 1)) *
                         (m < 1 ? 1.0 : double_factorial(2 * m - 1)) *
                         (n < 1 ? 1.0 : double_factorial(2 * n - 1)) *
                         std::pow(M_PI, 1.5);
    this->norm = std::sqrt(nom / denom);
}

/**
 * @brief Estimate the radius beyond which all primitives are below threshold
 */
double BasisFunction::cutoff_radius(double threshold) const {
    double rmax = 0.0;
    for(const auto& g : this->gtos) {
        const double pref = std::abs(g.c * g.norm);
        if(pref <= 0.0) {
            continue;
        }
        // solve pref * r^L * exp(-alpha r^2) = threshold by bisection on the
        // monotonically decreasing tail
        const int lsum = g.l + g.m + g.n;
        auto f = [&](double r) {
            return pref * ipow(r, lsum) * std::exp(-g.alpha * r * r);
        };
        double lo = std::sqrt(lsum / (2.0 * g.alpha));   // maximum of r^L exp(-a r^2)
        double hi = std::max(lo, 1.0);
        while(f(hi) > threshold && hi < 1e3) {
            hi *= 2.0;
        }
        for(int it = 0; it < 60; ++it) {
            double mid = 0.5 * (lo + hi);
            if(f(mid) > threshold) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        rmax = std::max(rmax, hi);
    }
    return rmax;
}

void BasisSet::add(const BasisFunction& bf) {
    this->functions.push_back(bf);
    const double rc = bf.cutoff_radius();
    this->cutoff2.push_back(rc * rc);
}

double BasisSet::evaluate_function(const BasisFunction& bf, const glm::dvec3& r) {
    const glm::dvec3 d = r - bf.center;
    const double r2 = glm::dot(d, d);

    double sum = 0.0;
    for(const auto& g : bf.gtos) {
        sum += g.c * g.norm * ipow(d.x, g.l) * ipow(d.y, g.m) * ipow(d.z, g.n) *
               std::exp(-g.alpha * r2);
    }
    return sum;
}

double BasisSet::evaluate(const glm::dvec3& r, const std::vector<double>& coeff) const {
    double sum = 0.0;
    const size_t nbf = std::min(coeff.size(), this->functions.size());
    for(size_t i = 0; i < nbf; ++i) {
        if(coeff[i] == 0.0) {
            continue;
        }
        const glm::dvec3 d = r - this->functions[i].center;
        if(glm::dot(d, d) > this->cutoff2[i]) {
            continue;
        }
        sum += coeff[i] * evaluate_function(this->functions[i], r);
    }
    return sum;
}
