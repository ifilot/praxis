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

#include <memory>

#include <QDialog>

#include "calculation/job_result.h"
#include "calculation/population_analysis.h"

class AnaglyphWidget;
class QComboBox;
class QLabel;
class QTableWidget;
class ViewerController;

/**
 * @brief Orbital-resolved bonding analysis (MOHP, MOOP, MOBI) for a pair of atoms
 *
 * Shows the molecule with the two selected atoms highlighted, a table with
 * the contribution of every molecular orbital to the interaction between
 * the two atoms, and the isosurface of the selected orbital.
 */
class BondAnalysisDialog : public QDialog {
    Q_OBJECT

private:
    std::shared_ptr<JobResult> result;
    PopulationAnalysis::PairPopulation population;
    int next_pick = 0;      // which atom (A or B) the next click in the viewer sets

    AnaglyphWidget* viewer;
    ViewerController* controller;

    QComboBox* combo_set;
    QComboBox* combo_a;
    QComboBox* combo_b;
    QComboBox* combo_kind;
    QLabel* label_distance;
    QTableWidget* table;
    QLabel* label_summary;
    QLabel* label_explanation;

public:
    /**
     * @param result        result of a calculation
     * @param set_index     orbital set to analyse initially
     * @param settings      isosurface settings and colors of the main viewer
     */
    BondAnalysisDialog(std::shared_ptr<JobResult> result, int set_index,
                       const ViewerController* settings, QWidget* parent = nullptr);

    /**
     * @brief Select the pair of atoms to analyse
     */
    void set_atoms(int atom_a, int atom_b);

    /**
     * @brief Default pair: the bonded pair of the heaviest atoms
     */
    static std::pair<int, int> default_pair(const JobResult& result);

private:
    void recalculate();
    void update_summary();
    void show_selected_orbital();
    void on_atom_clicked(int atom);
    void copy_table();

    PopulationAnalysis::Kind kind() const;
    QString atom_label(int atom) const;
};
