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

#include "job_spec.h"

QString JobSpec::validate() const {
    if(this->molecule.empty()) {
        return "No molecule has been loaded.";
    }

    const int ne = this->nelec();
    if(ne <= 0) {
        return QString("The molecule has %1 electrons; at least one electron is required.").arg(ne);
    }

    if(this->multiplicity < 1 || this->multiplicity > ne + 1) {
        return QString("A multiplicity of %1 is impossible for %2 electrons.").arg(this->multiplicity).arg(ne);
    }

    // parity: number of unpaired electrons (multiplicity - 1) must match
    // the parity of the number of electrons
    if((ne - (this->multiplicity - 1)) % 2 != 0) {
        return QString("A multiplicity of %1 is incompatible with %2 electrons "
                       "(an %3 number of electrons requires an %4 multiplicity).")
            .arg(this->multiplicity).arg(ne)
            .arg(ne % 2 == 0 ? "even" : "odd")
            .arg(ne % 2 == 0 ? "odd" : "even");
    }

    if(this->method == HFMethod::Restricted && this->multiplicity != 1) {
        return "Restricted Hartree-Fock requires a closed-shell system (multiplicity 1). "
               "Use unrestricted Hartree-Fock for open-shell systems.";
    }

    if(this->type == JobType::GeometryOptimization && this->method != HFMethod::Restricted) {
        return "PyQInt only supports geometry optimizations with restricted Hartree-Fock.";
    }

    if(this->foster_boys && this->method != HFMethod::Restricted) {
        return "Foster-Boys localization is only available for restricted Hartree-Fock.";
    }

    if(this->type == JobType::GeometryOptimization && this->molecule.size() < 2) {
        return "A geometry optimization requires at least two atoms.";
    }

    return QString();
}

QString JobSpec::description() const {
    QString method_str = this->method == HFMethod::Restricted ? "RHF" : "UHF";
    QString type_str = this->type == JobType::SinglePoint ? "single point" : "geometry optimization";
    QString result = QString("%1/%2 %3").arg(method_str, basis_set_label(this->basis), type_str);
    if(this->foster_boys) {
        result += " + Foster-Boys";
    }
    return result;
}

QStringList JobSpec::available_basis_sets() {
    return {"sto3g", "sto6g", "p321", "p631", "aug-cc-pVDZ", "aug-cc-pVTZ", "aug-cc-pVQZ"};
}

QString JobSpec::basis_set_label(const QString& name) {
    if(name == "sto3g") return "STO-3G";
    if(name == "sto6g") return "STO-6G";
    if(name == "p321") return "3-21G";
    if(name == "p631") return "6-31G";
    return name;
}
