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

#include <vector>

#include <QDialog>

#include "data/molecule.h"

class QLineEdit;
class QTreeWidget;

/**
 * @brief Select one of the molecules shipped with the program
 */
class LibraryDialog : public QDialog {
    Q_OBJECT

private:
    std::vector<Molecule> molecules;
    QLineEdit* filter;
    QTreeWidget* list;

public:
    explicit LibraryDialog(QWidget* parent = nullptr);

    /**
     * @brief The selected molecule (empty when nothing is selected)
     */
    Molecule selected_molecule() const;

    /**
     * @brief All molecules in the library, sorted by number of atoms
     */
    static std::vector<Molecule> load_library();

private:
    void apply_filter(const QString& text);
};
