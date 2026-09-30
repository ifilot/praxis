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

#include <QString>
#include <QWidget>

/**
 * @brief Minimal line plot (y versus index) drawn with QPainter
 */
class LinePlotWidget : public QWidget {
    Q_OBJECT

private:
    std::vector<double> values;
    QString title;
    QString xlabel;
    QString ylabel;
    int highlight = -1;
    int x_offset = 0;       // label of the first data point
    bool skip_outliers = true;

public:
    explicit LinePlotWidget(QWidget* parent = nullptr);

    void set_data(const std::vector<double>& values);
    void append(double value);
    void clear();

    void set_labels(const QString& title, const QString& xlabel, const QString& ylabel);
    void set_highlight(int index);
    void set_x_offset(int offset);

    inline size_t size() const {
        return this->values.size();
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void point_clicked(int index);

private:
    QRectF plot_area() const;
    void y_range(double* ymin, double* ymax) const;
};
