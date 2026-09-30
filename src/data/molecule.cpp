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

#include "molecule.h"

#include <map>
#include <stdexcept>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include "atom_settings.h"
#include "structure.h"

Molecule::Molecule(const QString& _name) :
    name(_name) {}

void Molecule::add_atom(const QString& element, const glm::dvec3& position) {
    this->atoms.push_back({element, position});
}

Molecule Molecule::from_xyz_string(const QString& content, const QString& fallback_name) {
    QStringList lines = content.split(QRegularExpression("\r?\n"));

    // skip leading empty lines
    while(!lines.empty() && lines.front().trimmed().isEmpty()) {
        lines.pop_front();
    }

    if(lines.size() < 2) {
        throw std::runtime_error("File is too short to be a valid .xyz file.");
    }

    bool ok = false;
    const int nratoms = lines[0].trimmed().toInt(&ok);
    if(!ok || nratoms <= 0) {
        throw std::runtime_error("First line of an .xyz file should contain the number of atoms.");
    }

    if(lines.size() < nratoms + 2) {
        throw std::runtime_error(QString("Expected %1 atoms but the file ends after %2 lines.")
                                 .arg(nratoms).arg(lines.size()).toStdString());
    }

    QString title = lines[1].trimmed();
    Molecule mol(title.isEmpty() ? fallback_name : title);

    for(int i = 0; i < nratoms; ++i) {
        const QString& line = lines[i + 2];
        const QStringList pieces = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if(pieces.size() < 4) {
            throw std::runtime_error(QString("Invalid atom line %1: '%2'").arg(i + 3).arg(line).toStdString());
        }

        // normalize element symbol capitalization (e.g. "CL" -> "Cl")
        QString el = pieces[0].toLower();
        if(!el.isEmpty()) {
            el[0] = el[0].toUpper();
        }

        if(AtomSettings::get().get_atom_elnr(el.toStdString()) == 0) {
            throw std::runtime_error(QString("Unknown element '%1' on line %2.").arg(pieces[0]).arg(i + 3).toStdString());
        }

        bool okx, oky, okz;
        const double x = pieces[1].toDouble(&okx);
        const double y = pieces[2].toDouble(&oky);
        const double z = pieces[3].toDouble(&okz);
        if(!(okx && oky && okz)) {
            throw std::runtime_error(QString("Invalid coordinates on line %1: '%2'").arg(i + 3).arg(line).toStdString());
        }

        mol.add_atom(el, glm::dvec3(x, y, z));
    }

    return mol;
}

Molecule Molecule::from_xyz_file(const QString& path) {
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("Cannot open %1").arg(path).toStdString());
    }
    const QString content = QString::fromUtf8(f.readAll());
    return from_xyz_string(content, QFileInfo(path).completeBaseName());
}

int Molecule::nuclear_charge() const {
    int sum = 0;
    for(const auto& atom : this->atoms) {
        sum += (int)AtomSettings::get().get_atom_elnr(atom.element.toStdString());
    }
    return sum;
}

QString Molecule::formula() const {
    std::map<QString, int> count;
    for(const auto& atom : this->atoms) {
        count[atom.element]++;
    }

    auto term = [](const QString& el, int n) {
        return n > 1 ? el + QString::number(n) : el;
    };

    // Hill system: C first, then H, then alphabetical; no carbon -> alphabetical
    QString result;
    if(count.count("C")) {
        result += term("C", count["C"]);
        count.erase("C");
        if(count.count("H")) {
            result += term("H", count["H"]);
            count.erase("H");
        }
    }
    for(const auto& item : count) {
        result += term(item.first, item.second);
    }
    return result;
}

QString Molecule::formula_html() const {
    QString result = this->formula();
    result.replace(QRegularExpression("(\\d+)"), "<sub>\\1</sub>");
    return result;
}

glm::dvec3 Molecule::centroid() const {
    glm::dvec3 sum(0.0);
    if(this->atoms.empty()) {
        return sum;
    }
    for(const auto& atom : this->atoms) {
        sum += atom.position;
    }
    return sum / (double)this->atoms.size();
}

QString Molecule::to_xyz() const {
    QString result;
    QTextStream out(&result);
    out << this->atoms.size() << "\n" << this->name << "\n";
    for(const auto& atom : this->atoms) {
        out << QString("%1 %2 %3 %4\n")
               .arg(atom.element, -2)
               .arg(atom.position.x, 14, 'f', 8)
               .arg(atom.position.y, 14, 'f', 8)
               .arg(atom.position.z, 14, 'f', 8);
    }
    return result;
}

std::shared_ptr<Structure> Molecule::to_structure(glm::vec3* offset) const {
    auto structure = std::make_shared<Structure>();
    const glm::dvec3 ctr = this->centroid();

    for(const auto& atom : this->atoms) {
        const glm::dvec3 p = atom.position - ctr;
        structure->add_atom(AtomSettings::get().get_atom_elnr(atom.element.toStdString()), p.x, p.y, p.z);
    }
    structure->update();

    if(offset != nullptr) {
        *offset = glm::vec3(ctr);
    }

    return structure;
}
