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
#include <QStringList>

#include "data/molecule.h"

enum class JobType {
    SinglePoint,
    GeometryOptimization,
};

enum class HFMethod {
    Restricted,
    Unrestricted,
};

/**
 * @brief Everything needed to set up a PyQInt calculation
 */
struct JobSpec {
    Molecule molecule;

    JobType type = JobType::SinglePoint;
    HFMethod method = HFMethod::Restricted;
    QString basis = "sto3g";
    int charge = 0;
    int multiplicity = 1;

    // SCF settings
    int itermax = 100;
    double tolerance = 1e-9;
    bool use_diis = true;
    QString ortho = "canonical";

    // geometry optimization
    double gtol = 1e-5;

    // Foster-Boys localization (restricted only)
    bool foster_boys = false;
    int fb_seed = 42;
    int fb_runners = 1;

    /**
     * @brief Number of electrons
     */
    inline int nelec() const {
        return this->molecule.nuclear_charge() - this->charge;
    }

    /**
     * @brief Check whether the settings make sense
     *
     * @return empty string when valid, otherwise a human-readable reason
     */
    QString validate() const;

    /**
     * @brief Short human-readable description, e.g. "RHF/sto3g single point"
     */
    QString description() const;

    /**
     * @brief Basis sets shipped with PyQInt (file names without .json)
     */
    static QStringList available_basis_sets();

    /**
     * @brief Label shown in the user interface for a basis set name
     */
    static QString basis_set_label(const QString& name);
};
