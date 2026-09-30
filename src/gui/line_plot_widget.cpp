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

#include "line_plot_widget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

LinePlotWidget::LinePlotWidget(QWidget* parent) :
    QWidget(parent) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void LinePlotWidget::set_data(const std::vector<double>& _values) {
    this->values = _values;
    this->highlight = -1;
    this->update();
}

void LinePlotWidget::append(double value) {
    this->values.push_back(value);
    this->update();
}

void LinePlotWidget::clear() {
    this->values.clear();
    this->highlight = -1;
    this->update();
}

void LinePlotWidget::set_labels(const QString& _title, const QString& _xlabel, const QString& _ylabel) {
    this->title = _title;
    this->xlabel = _xlabel;
    this->ylabel = _ylabel;
    this->update();
}

void LinePlotWidget::set_highlight(int index) {
    this->highlight = index;
    this->update();
}

void LinePlotWidget::set_x_offset(int offset) {
    this->x_offset = offset;
    this->update();
}

QSize LinePlotWidget::sizeHint() const {
    return QSize(320, 200);
}

QSize LinePlotWidget::minimumSizeHint() const {
    return QSize(160, 120);
}

QRectF LinePlotWidget::plot_area() const {
    const QFontMetrics fm(this->font());
    const double left = fm.horizontalAdvance("-0000.0000") + fm.height() + 8;
    const double top = fm.height() * 1.8;
    const double bottom = fm.height() * 2.6;
    const double right = 12;
    return QRectF(left, top, std::max(10.0, this->width() - left - right),
                  std::max(10.0, this->height() - top - bottom));
}

void LinePlotWidget::y_range(double* ymin, double* ymax) const {
    // the first SCF iterations are often far above the converged energy;
    // leave out points that would squash the interesting part of the plot
    size_t start = 0;
    if(this->skip_outliers && this->values.size() > 4) {
        const double last = this->values.back();
        double spread = 0.0;
        for(size_t i = this->values.size() / 2; i < this->values.size(); ++i) {
            spread = std::max(spread, std::abs(this->values[i] - last));
        }
        spread = std::max(spread, 1e-3);
        while(start < this->values.size() - 3 && std::abs(this->values[start] - last) > 50.0 * spread) {
            start++;
        }
    }

    *ymin = *std::min_element(this->values.begin() + start, this->values.end());
    *ymax = *std::max_element(this->values.begin() + start, this->values.end());
    if(*ymax - *ymin < 1e-9) {
        *ymin -= 0.5e-3;
        *ymax += 0.5e-3;
    }
    const double pad = 0.08 * (*ymax - *ymin);
    *ymin -= pad;
    *ymax += pad;
}

void LinePlotWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPalette pal = this->palette();
    const QColor fg = pal.color(QPalette::WindowText);
    QColor grid = fg;
    grid.setAlpha(40);
    const QColor accent = pal.color(QPalette::Highlight);

    painter.fillRect(this->rect(), pal.color(QPalette::Base));

    const QRectF area = this->plot_area();
    const QFontMetrics fm(this->font());

    // title
    painter.setPen(fg);
    QFont bold = this->font();
    bold.setBold(true);
    painter.setFont(bold);
    painter.drawText(QRectF(0, 2, this->width(), fm.height() * 1.5), Qt::AlignCenter, this->title);
    painter.setFont(this->font());

    if(this->values.empty()) {
        painter.setPen(grid.darker());
        painter.drawText(area, Qt::AlignCenter, "No data");
        return;
    }

    double ymin, ymax;
    this->y_range(&ymin, &ymax);
    const size_t n = this->values.size();
    const double xmax = std::max<double>(1.0, (double)n - 1.0);

    auto map = [&](double x, double y) {
        return QPointF(area.left() + area.width() * x / xmax,
                       area.bottom() - area.height() * (y - ymin) / (ymax - ymin));
    };

    // y grid and tick labels
    const int nticks = 4;
    int decimals = std::clamp((int)std::ceil(-std::log10((ymax - ymin) / nticks)) + 1, 0, 8);
    for(int t = 0; t <= nticks; ++t) {
        const double y = ymin + (ymax - ymin) * t / nticks;
        const QPointF p = map(0, y);
        painter.setPen(grid);
        painter.drawLine(QPointF(area.left(), p.y()), QPointF(area.right(), p.y()));
        painter.setPen(fg);
        painter.drawText(QRectF(0, p.y() - fm.height() / 2.0, area.left() - 6, fm.height()),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'f', decimals));
    }

    // x tick labels
    const int xstep = std::max(1, (int)std::ceil(n / 8.0));
    for(size_t i = 0; i < n; i += xstep) {
        const QPointF p = map((double)i, ymin);
        painter.setPen(grid);
        painter.drawLine(QPointF(p.x(), area.top()), QPointF(p.x(), area.bottom()));
        painter.setPen(fg);
        painter.drawText(QRectF(p.x() - 30, area.bottom() + 2, 60, fm.height()),
                         Qt::AlignHCenter | Qt::AlignTop, QString::number(i + this->x_offset));
    }

    // axis labels
    painter.drawText(QRectF(area.left(), area.bottom() + fm.height() + 2, area.width(), fm.height()),
                     Qt::AlignCenter, this->xlabel);
    painter.save();
    painter.translate(fm.height() * 0.2, area.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-area.height() / 2.0, 0, area.height(), fm.height()), Qt::AlignCenter, this->ylabel);
    painter.restore();

    // frame
    painter.setPen(QPen(fg, 1.0));
    painter.drawRect(area);

    // data (clipped to the plot area)
    painter.save();
    painter.setClipRect(area.adjusted(-4, -4, 4, 4));
    QPainterPath path;
    for(size_t i = 0; i < n; ++i) {
        const QPointF p = map((double)i, this->values[i]);
        if(i == 0) {
            path.moveTo(p);
        } else {
            path.lineTo(p);
        }
    }
    painter.setPen(QPen(accent, 2.0));
    painter.drawPath(path);

    painter.setBrush(accent);
    painter.setPen(Qt::NoPen);
    for(size_t i = 0; i < n; ++i) {
        painter.drawEllipse(map((double)i, this->values[i]), 2.5, 2.5);
    }

    if(this->highlight >= 0 && this->highlight < (int)n) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(fg, 2.0));
        painter.drawEllipse(map((double)this->highlight, this->values[this->highlight]), 6.0, 6.0);
    }
    painter.restore();
}

void LinePlotWidget::mousePressEvent(QMouseEvent* event) {
    if(this->values.empty()) {
        return;
    }
    const QRectF area = this->plot_area();
    const double xmax = std::max<double>(1.0, (double)this->values.size() - 1.0);
    const double x = (event->position().x() - area.left()) / area.width() * xmax;
    const int idx = std::clamp((int)std::lround(x), 0, (int)this->values.size() - 1);
    emit point_clicked(idx);
}
