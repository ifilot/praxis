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

#include <QRectF>
#include <QString>
#include <QWidget>

/**
 * @brief Molecular orbital energy level diagram
 *
 * Shows one column per orbital set (e.g. alpha and beta for UHF), with
 * degenerate levels drawn side by side and electrons as arrows. Clicking a
 * level selects the corresponding orbital.
 */
class MODiagramWidget : public QWidget {
    Q_OBJECT

public:
    struct Column {
        QString label;
        QString spin;                   // "restricted", "alpha" or "beta"
        std::vector<double> energies;
        std::vector<double> occupations;
    };

private:
    struct LevelRect {
        int column;
        int orbital;
        QRectF rect;
    };

    std::vector<Column> columns;
    std::vector<LevelRect> hitboxes;
    int selected_column = -1;
    int selected_orbital = -1;
    bool show_all = false;

public:
    explicit MODiagramWidget(QWidget* parent = nullptr);

    void set_columns(const std::vector<Column>& columns);
    void clear();
    void set_selected(int column, int orbital);
    void set_show_all(bool show_all);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void orbital_clicked(int column, int orbital);

private:
    void energy_window(double* emin, double* emax) const;
};
