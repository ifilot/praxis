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

#include "mo_diagram_widget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr double DEGENERACY_THRESHOLD = 1e-4;   // Ht
constexpr double MIN_LEVEL_SEPARATION = 14.0;   // pixels; closer levels are drawn side by side
constexpr double WINDOW_BELOW_HOMO = 1.5;       // Ht
constexpr double WINDOW_ABOVE_LUMO = 1.0;       // Ht

void draw_arrow(QPainter& painter, double x, double ybase, double length, bool up) {
    const double y0 = up ? ybase + length / 2.0 : ybase - length / 2.0;
    const double y1 = up ? ybase - length / 2.0 : ybase + length / 2.0;
    const double head = std::min(5.0, length * 0.35);
    const double dir = up ? 1.0 : -1.0;

    painter.drawLine(QPointF(x, y0), QPointF(x, y1));
    QPainterPath tip;
    tip.moveTo(x, y1);
    tip.lineTo(x - head * 0.6, y1 + dir * head);
    tip.lineTo(x + head * 0.6, y1 + dir * head);
    tip.closeSubpath();
    painter.fillPath(tip, painter.pen().color());
}

} // namespace

MODiagramWidget::MODiagramWidget(QWidget* parent) :
    QWidget(parent) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setMouseTracking(false);
}

void MODiagramWidget::set_columns(const std::vector<Column>& _columns) {
    this->columns = _columns;
    this->selected_column = -1;
    this->selected_orbital = -1;
    this->update();
}

void MODiagramWidget::clear() {
    this->columns.clear();
    this->hitboxes.clear();
    this->update();
}

void MODiagramWidget::set_selected(int column, int orbital) {
    this->selected_column = column;
    this->selected_orbital = orbital;
    this->update();
}

void MODiagramWidget::set_show_all(bool _show_all) {
    this->show_all = _show_all;
    this->update();
}

QSize MODiagramWidget::sizeHint() const {
    return QSize(300, 420);
}

QSize MODiagramWidget::minimumSizeHint() const {
    return QSize(160, 200);
}

void MODiagramWidget::energy_window(double* emin, double* emax) const {
    double all_min = 1e30, all_max = -1e30;
    double homo = -1e30, lumo = 1e30;
    for(const auto& col : this->columns) {
        for(size_t i = 0; i < col.energies.size(); ++i) {
            all_min = std::min(all_min, col.energies[i]);
            all_max = std::max(all_max, col.energies[i]);
            if(col.occupations[i] > 0.0) {
                homo = std::max(homo, col.energies[i]);
            } else {
                lumo = std::min(lumo, col.energies[i]);
            }
        }
    }

    if(this->show_all) {
        *emin = all_min;
        *emax = all_max;
    } else {
        *emin = homo > -1e29 ? std::max(all_min, homo - WINDOW_BELOW_HOMO) : all_min;
        *emax = lumo < 1e29 ? std::min(all_max, lumo + WINDOW_ABOVE_LUMO) : all_max;
    }

    if(*emax - *emin < 0.1) {
        *emin -= 0.05;
        *emax += 0.05;
    }
}

void MODiagramWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPalette pal = this->palette();
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = pal.color(QPalette::Highlight);
    QColor muted = fg;
    muted.setAlpha(110);
    const QColor occupied_color = fg;
    const QColor electron_color(0xd6, 0x3b, 0x3b);

    painter.fillRect(this->rect(), pal.color(QPalette::Base));
    this->hitboxes.clear();

    const QFontMetrics fm(this->font());
    if(this->columns.empty()) {
        painter.setPen(muted);
        painter.drawText(this->rect(), Qt::AlignCenter, "Run a calculation to\nshow the orbital diagram");
        return;
    }

    double emin, emax;
    this->energy_window(&emin, &emax);

    const double left = fm.horizontalAdvance("-00.000") + fm.height() + 10;
    const double top = fm.height() * 2.5;
    const double bottom = fm.height() * 2.0;
    const double right = 10;
    const QRectF area(left, top, std::max(10.0, this->width() - left - right),
                      std::max(10.0, this->height() - top - bottom));

    auto ymap = [&](double e) {
        return area.bottom() - area.height() * (e - emin) / (emax - emin);
    };

    // energy axis
    painter.setPen(fg);
    painter.drawLine(QPointF(area.left() - 6, area.top()), QPointF(area.left() - 6, area.bottom()));
    const int nticks = 5;
    for(int t = 0; t <= nticks; ++t) {
        const double e = emin + (emax - emin) * t / nticks;
        const double y = ymap(e);
        painter.drawLine(QPointF(area.left() - 10, y), QPointF(area.left() - 6, y));
        painter.drawText(QRectF(0, y - fm.height() / 2.0, area.left() - 12, fm.height()),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(e, 'f', 3));
    }
    painter.save();
    painter.translate(fm.height() * 0.1, area.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-area.height() / 2.0, 0, area.height(), fm.height()),
                     Qt::AlignCenter, "Orbital energy (Ht)");
    painter.restore();

    // columns
    const double colwidth = area.width() / this->columns.size();
    for(size_t c = 0; c < this->columns.size(); ++c) {
        const Column& col = this->columns[c];
        const double cx0 = area.left() + c * colwidth;

        QFont bold = this->font();
        bold.setBold(true);
        painter.setFont(bold);
        painter.setPen(fg);
        painter.drawText(QRectF(cx0, 2, colwidth, fm.height() * 1.4), Qt::AlignCenter, col.label);
        painter.setFont(this->font());

        // count orbitals outside the window
        int nbelow = 0, nabove = 0;

        // group degenerate levels
        size_t i = 0;
        while(i < col.energies.size()) {
            // group (near-)degenerate levels and levels that would overlap on screen
            size_t j = i + 1;
            while(j < col.energies.size() &&
                  (std::abs(col.energies[j] - col.energies[i]) < DEGENERACY_THRESHOLD ||
                   std::abs(ymap(col.energies[j]) - ymap(col.energies[i])) < MIN_LEVEL_SEPARATION)) {
                j++;
            }

            const double e = col.energies[i];
            if(e < emin - 1e-9) {
                nbelow += (int)(j - i);
                i = j;
                continue;
            }
            if(e > emax + 1e-9) {
                nabove += (int)(j - i);
                i = j;
                continue;
            }

            const int ndeg = (int)(j - i);
            const double slot = std::min(colwidth * 0.8 / ndeg, 60.0);
            const double linew = slot * 0.75;
            const double xstart = cx0 + colwidth / 2.0 - slot * ndeg / 2.0;

            for(int d = 0; d < ndeg; ++d) {
                const int orb = (int)(i + d);
                const double y = ymap(col.energies[orb]);
                const double x0 = xstart + d * slot + (slot - linew) / 2.0;
                const bool occupied = col.occupations[orb] > 0.0;
                const bool selected = (int)c == this->selected_column && orb == this->selected_orbital;

                if(selected) {
                    painter.fillRect(QRectF(x0 - 3, y - 12, linew + 6, 24), QColor(accent.red(), accent.green(), accent.blue(), 60));
                }

                painter.setPen(QPen(selected ? accent : (occupied ? occupied_color : muted), selected ? 3.0 : 2.0));
                painter.drawLine(QPointF(x0, y), QPointF(x0 + linew, y));

                // electrons
                painter.setPen(QPen(electron_color, 1.6));
                const double alen = std::min(18.0, area.height() / 25.0 + 6.0);
                const double occ = col.occupations[orb];
                if(col.spin == "restricted") {
                    if(occ >= 1.0) {
                        draw_arrow(painter, x0 + linew * 0.35, y, alen, true);
                    }
                    if(occ >= 2.0) {
                        draw_arrow(painter, x0 + linew * 0.65, y, alen, false);
                    }
                } else if(occ >= 1.0) {
                    draw_arrow(painter, x0 + linew * 0.5, y, alen, col.spin != "beta");
                }

                this->hitboxes.push_back({(int)c, orb, QRectF(x0 - 4, y - 8, linew + 8, 16)});
            }
            i = j;
        }

        painter.setPen(muted);
        if(nbelow > 0) {
            painter.drawText(QRectF(cx0, area.bottom() + 4, colwidth, fm.height()), Qt::AlignCenter,
                             QString("+ %1 lower").arg(nbelow));
        }
        if(nabove > 0) {
            painter.drawText(QRectF(cx0, top - fm.height() - 2, colwidth, fm.height()), Qt::AlignCenter,
                             QString("+ %1 higher").arg(nabove));
        }
    }
}

void MODiagramWidget::mousePressEvent(QMouseEvent* event) {
    const QPointF pos = event->position();
    for(const auto& hb : this->hitboxes) {
        if(hb.rect.contains(pos)) {
            this->set_selected(hb.column, hb.orbital);
            emit orbital_clicked(hb.column, hb.orbital);
            return;
        }
    }
}
