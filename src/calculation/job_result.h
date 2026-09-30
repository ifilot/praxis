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

#include <map>
#include <memory>
#include <vector>

#include <QJsonObject>
#include <QString>
#include <glm/glm.hpp>

#include "data/molecule.h"
#include "orbitals/basis_set.h"

struct DenseMatrix {
    QString label;
    int rows = 0;
    int cols = 0;
    std::vector<double> data;       // row-major

    inline double operator()(int i, int j) const {
        return this->data[(size_t)i * this->cols + j];
    }
};

struct OrbitalSet {
    QString label;                  // e.g. "Canonical", "Foster-Boys"
    QString spin;                   // "restricted", "alpha" or "beta"
    std::vector<double> energies;   // Ht
    std::vector<double> occupations;
    std::vector<std::vector<double>> coefficients;  // [orbital][basis function]

    inline size_t size() const {
        return this->energies.size();
    }

    /**
     * @brief Index of the highest occupied orbital (-1 if none)
     */
    int homo() const;

    /**
     * @brief "HOMO", "LUMO", "HOMO-1", "LUMO+2", ... for orbitals close to
     *        the frontier, empty otherwise
     */
    QString frontier_label(int index) const;
};

struct OptimizationTrajectory {
    bool success = false;
    QString message;
    std::vector<double> energies;                   // Ht, per energy evaluation
    std::vector<std::vector<glm::dvec3>> frames;    // bohr
    std::vector<std::vector<glm::dvec3>> forces;    // Ht/bohr
};

struct LocalizationInfo {
    bool present = false;
    double r2start = 0.0;
    double r2final = 0.0;
    int iterations = 0;
};

/**
 * @brief Parsed contents of a result.json file
 */
class JobResult {
public:
    int schema = 0;
    QString pyqint_version;
    QString pydft_version;          // empty for Hartree-Fock results
    QString python_version;
    QString created;
    QJsonObject job;                // job settings as echoed by the script

    // molecule (final geometry)
    QString molecule_name;
    int charge = 0;
    std::vector<QString> elements;
    std::vector<int> atomic_numbers;
    std::vector<glm::dvec3> positions;      // bohr

    // basis set
    std::shared_ptr<BasisSet> basis;

    // SCF
    QString method;                 // "rhf", "uhf" or "rks" (Kohn-Sham DFT)
    QString functional;             // DFT only, e.g. "svwn5"
    bool converged = true;          // false when the program reports no convergence
    int nelec = 0;
    int nalpha = 0;
    int nbeta = 0;
    int multiplicity = 1;
    std::vector<double> scf_energies;
    std::vector<std::pair<QString, double>> energy_components;  // ordered

    std::vector<OrbitalSet> orbital_sets;
    std::vector<std::pair<QString, DenseMatrix>> matrices;      // ordered

    std::vector<double> mulliken;
    std::vector<double> lowdin;

    std::vector<std::pair<QString, double>> timing;

    LocalizationInfo localization;
    std::unique_ptr<OptimizationTrajectory> optimization;

    /**
     * @brief Load from a result.json file
     *
     * @throws std::runtime_error on failure
     */
    static std::shared_ptr<JobResult> load(const QString& filename);

    /**
     * @brief Parse from JSON data
     *
     * @throws std::runtime_error on failure
     */
    static std::shared_ptr<JobResult> parse(const QByteArray& data);

    /**
     * @brief Total energy (Ht)
     */
    double total_energy() const;

    /**
     * @brief Final molecule geometry in angstrom
     */
    Molecule get_molecule() const;

    /**
     * @brief Geometry of a frame of the optimization trajectory (angstrom)
     */
    Molecule get_trajectory_molecule(size_t frame) const;

    inline bool is_unrestricted() const {
        return this->method == "uhf";
    }

    inline bool is_dft() const {
        return this->method == "rks";
    }

    /**
     * @brief Human readable level of theory, e.g. "Restricted Hartree-Fock"
     */
    QString method_label() const;

    /**
     * @brief Matrix by key (e.g. "fock", "overlap"); nullptr when absent
     */
    const DenseMatrix* find_matrix(const QString& key) const;

    /**
     * @brief Index of the orbital set with this label (-1 when absent)
     */
    int find_orbital_set(const QString& label) const;

    /**
     * @brief Human readable description of an energy component key
     */
    static QString energy_component_label(const QString& key);
};
