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

#include "bond_analysis_dialog.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include "anaglyph_widget.h"
#include "data/units.h"
#include "icons.h"
#include "viewer_controller.h"

namespace {

enum Column {
    COL_INDEX,
    COL_LABEL,
    COL_ENERGY,
    COL_OCC,
    COL_VALUE,
    COL_BAR,
    COL_CHARACTER,
    NR_COLUMNS
};

// roles of the bar column
constexpr int ROLE_STRENGTH = Qt::UserRole;         // bonding strength, >0 = bonding
constexpr int ROLE_SCALE = Qt::UserRole + 1;        // strength that fills half the cell
constexpr int ROLE_OCCUPIED = Qt::UserRole + 2;

const QColor COLOR_BONDING(46, 125, 50);
const QColor COLOR_ANTIBONDING(198, 40, 40);

/**
 * @brief Diverging bar: bonding to the right, antibonding to the left
 */
class BarDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        this->initStyleOption(&opt, index);
        opt.text.clear();
        const QWidget* w = opt.widget;
        QStyle* style = w != nullptr ? w->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, w);

        const double strength = index.data(ROLE_STRENGTH).toDouble();
        const double scale = std::max(1e-12, index.data(ROLE_SCALE).toDouble());
        const bool occupied = index.data(ROLE_OCCUPIED).toBool();

        const QRectF r = QRectF(opt.rect).adjusted(6, 4, -6, -4);
        const double cx = r.center().x();
        const double half = r.width() / 2.0;
        const double frac = strength / scale;
        const double len = std::clamp(frac, -1.0, 1.0) * half;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QColor color = strength >= 0 ? COLOR_BONDING : COLOR_ANTIBONDING;
        if(!occupied) {
            color.setAlpha(80);     // virtual orbitals do not contribute to the bond
        }
        const QRectF bar = len >= 0 ? QRectF(cx, r.top(), len, r.height()) : QRectF(cx + len, r.top(), -len, r.height());
        painter->fillRect(bar, color);

        // values beyond the scale are clipped; mark them with an arrow head
        if(std::abs(frac) > 1.0) {
            const double x = len >= 0 ? r.right() : r.left();
            const double d = len >= 0 ? 5.0 : -5.0;
            QPolygonF tip;
            tip << QPointF(x, r.center().y()) << QPointF(x - d, r.top()) << QPointF(x - d, r.bottom());
            painter->setPen(Qt::NoPen);
            painter->setBrush(color.darker(130));
            painter->drawPolygon(tip);
        }

        painter->setPen(QPen(opt.palette.color(QPalette::Mid), 1));
        painter->drawLine(QPointF(cx, opt.rect.top() + 1), QPointF(cx, opt.rect.bottom() - 1));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setWidth(std::max(size.width(), 180));
        return size;
    }
};

QTableWidgetItem* number_item(double value, int decimals) {
    auto* item = new QTableWidgetItem(QString::number(value, 'f', decimals));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

} // namespace

BondAnalysisDialog::BondAnalysisDialog(std::shared_ptr<JobResult> _result, int set_index,
                                       const ViewerController* settings, QWidget* parent) :
    QDialog(parent),
    result(std::move(_result)) {

    this->setWindowTitle("Orbital bonding analysis");
    this->resize(1280, 780);
    this->setSizeGripEnabled(true);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel("Which molecular orbitals bond or antibond two atoms? Each orbital is "
                             "projected onto the basis functions of the two atoms. Click two atoms in the "
                             "3D view or choose them below; select an orbital in the table to show it.");
    intro->setWordWrap(true);
    layout->addWidget(intro);

    // ------------------------------------------------------------------
    // controls
    // ------------------------------------------------------------------
    auto* lctrl = new QHBoxLayout;
    lctrl->addWidget(new QLabel("Orbitals:"));
    this->combo_set = new QComboBox;
    for(const auto& set : this->result->orbital_sets) {
        this->combo_set->addItem(set.label);
    }
    this->combo_set->setCurrentIndex(std::clamp(set_index, 0, this->combo_set->count() - 1));
    lctrl->addWidget(this->combo_set);

    lctrl->addSpacing(16);
    lctrl->addWidget(new QLabel("Atoms:"));
    this->combo_a = new QComboBox;
    this->combo_b = new QComboBox;
    QPixmap swatch(12, 12);
    swatch.fill(settings->get_color_highlight());
    for(size_t i = 0; i < this->result->elements.size(); ++i) {
        this->combo_a->addItem(QIcon(swatch), this->atom_label((int)i));
        this->combo_b->addItem(QIcon(swatch), this->atom_label((int)i));
    }
    lctrl->addWidget(this->combo_a);
    lctrl->addWidget(new QLabel("–"));
    lctrl->addWidget(this->combo_b);
    this->label_distance = new QLabel;
    lctrl->addWidget(this->label_distance);

    lctrl->addSpacing(16);
    lctrl->addWidget(new QLabel("Quantity:"));
    this->combo_kind = new QComboBox;
    this->combo_kind->addItem("MOHP (Hamilton population)", (int)PopulationAnalysis::Kind::Hamilton);
    this->combo_kind->addItem("MOOP (overlap population)", (int)PopulationAnalysis::Kind::Overlap);
    this->combo_kind->addItem("MOBI (bond index)", (int)PopulationAnalysis::Kind::BondIndex);
    lctrl->addWidget(this->combo_kind);
    lctrl->addStretch(1);
    layout->addLayout(lctrl);

    // ------------------------------------------------------------------
    // viewer and table
    // ------------------------------------------------------------------
    auto* splitter = new QSplitter(Qt::Horizontal);
    this->viewer = new AnaglyphWidget;
    this->viewer->setMinimumWidth(380);
    this->controller = new ViewerController(this->viewer, this);
    this->controller->set_colors(settings->get_color_positive(), settings->get_color_negative());
    this->controller->set_grid_spacing(settings->get_grid_settings().spacing);
    this->controller->set_enclosed_fraction(settings->get_enclosed_fraction());
    this->controller->set_isovalue(settings->get_isovalue(), settings->is_auto_isovalue());
    this->controller->set_opacity(std::min(settings->get_opacity(), 0.7f));
    splitter->addWidget(this->viewer);

    this->table = new QTableWidget;
    this->table->setColumnCount(NR_COLUMNS);
    this->table->verticalHeader()->setVisible(false);
    this->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->table->setSelectionMode(QAbstractItemView::SingleSelection);
    this->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->table->setItemDelegateForColumn(COL_BAR, new BarDelegate(this->table));
    this->table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->table->horizontalHeader()->setSectionResizeMode(COL_BAR, QHeaderView::Stretch);
    splitter->addWidget(this->table);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({560, 680});
    layout->addWidget(splitter, 1);

    // ------------------------------------------------------------------
    // summary, explanation and buttons
    // ------------------------------------------------------------------
    this->label_summary = new QLabel;
    this->label_summary->setWordWrap(true);
    this->label_summary->setTextFormat(Qt::RichText);
    layout->addWidget(this->label_summary);

    this->label_explanation = new QLabel;
    this->label_explanation->setWordWrap(true);
    this->label_explanation->setTextFormat(Qt::RichText);
    this->label_explanation->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(this->label_explanation);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton* button_copy = buttons->addButton("Copy table", QDialogButtonBox::ActionRole);
    button_copy->setIcon(bluecurve_icon("edit-copy"));
    button_copy->setToolTip("Copy the table as tab-separated text (e.g. to paste into a spreadsheet)");
    layout->addWidget(buttons);

    // ------------------------------------------------------------------
    // connections
    // ------------------------------------------------------------------
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(button_copy, &QPushButton::clicked, this, &BondAnalysisDialog::copy_table);
    connect(this->combo_set, &QComboBox::currentIndexChanged, this, &BondAnalysisDialog::recalculate);
    connect(this->combo_a, &QComboBox::currentIndexChanged, this, &BondAnalysisDialog::recalculate);
    connect(this->combo_b, &QComboBox::currentIndexChanged, this, &BondAnalysisDialog::recalculate);
    connect(this->combo_kind, &QComboBox::currentIndexChanged, this, &BondAnalysisDialog::recalculate);
    connect(this->table, &QTableWidget::itemSelectionChanged, this, &BondAnalysisDialog::show_selected_orbital);
    connect(this->viewer, &AnaglyphWidget::atom_clicked, this, &BondAnalysisDialog::on_atom_clicked);

    const auto pair = default_pair(*this->result);
    {
        const QSignalBlocker b1(this->combo_a), b2(this->combo_b);
        this->combo_a->setCurrentIndex(pair.first);
        this->combo_b->setCurrentIndex(pair.second);
    }

    // the viewer can only show the molecule once OpenGL is initialized
    connect(this->viewer, &AnaglyphWidget::opengl_ready, this, [this]() {
        this->controller->show_result(this->result);
        this->recalculate();
    });
}

QString BondAnalysisDialog::atom_label(int atom) const {
    return QString("%1%2").arg(this->result->elements[atom]).arg(atom + 1);
}

PopulationAnalysis::Kind BondAnalysisDialog::kind() const {
    return (PopulationAnalysis::Kind)this->combo_kind->currentData().toInt();
}

std::pair<int, int> BondAnalysisDialog::default_pair(const JobResult& result) {
    const int n = (int)result.positions.size();
    if(n < 2) {
        return {0, 0};
    }

    // among the (roughly) bonded pairs, prefer the heaviest atoms, then the shortest distance
    std::pair<int, int> best = {0, 1};
    int best_z = -1;
    double best_d = 1e30;
    for(int i = 0; i < n; ++i) {
        for(int j = i + 1; j < n; ++j) {
            const double d = glm::length(result.positions[i] - result.positions[j]) * BOHR_TO_ANGSTROM;
            if(d > 1.9) {
                continue;
            }
            const int z = result.atomic_numbers[i] + result.atomic_numbers[j];
            if(z > best_z || (z == best_z && d < best_d)) {
                best = {i, j};
                best_z = z;
                best_d = d;
            }
        }
    }
    return best;
}

void BondAnalysisDialog::set_atoms(int atom_a, int atom_b) {
    const QSignalBlocker b1(this->combo_a), b2(this->combo_b);
    this->combo_a->setCurrentIndex(atom_a);
    this->combo_b->setCurrentIndex(atom_b);
    this->recalculate();
}

void BondAnalysisDialog::on_atom_clicked(int atom) {
    // clicks alternately set atom A and atom B
    QComboBox* target = this->next_pick == 0 ? this->combo_a : this->combo_b;
    QComboBox* other = this->next_pick == 0 ? this->combo_b : this->combo_a;
    if(atom == other->currentIndex()) {
        return;
    }
    target->setCurrentIndex(atom);
    this->next_pick = 1 - this->next_pick;
}

void BondAnalysisDialog::recalculate() {
    const int s = this->combo_set->currentIndex();
    const int a = this->combo_a->currentIndex();
    const int b = this->combo_b->currentIndex();
    if(s < 0 || a < 0 || b < 0) {
        return;
    }

    this->controller->set_highlighted_atoms({a, b});
    const double d = glm::length(this->result->positions[a] - this->result->positions[b]) * BOHR_TO_ANGSTROM;
    this->label_distance->setText(QString("  d = %1 Å").arg(d, 0, 'f', 3));

    // remember the selected orbital
    const auto rows = this->table->selectionModel()->selectedRows();
    const int previous = rows.isEmpty() ? -1 : rows.front().row();

    const QSignalBlocker blocker(this->table);
    this->table->setRowCount(0);
    try {
        this->population = PopulationAnalysis::evaluate(*this->result, s, a, b, this->kind());
    } catch(const std::exception& e) {
        this->population = PopulationAnalysis::PairPopulation();
        this->label_summary->setText(QString("<span style='color:#c62828'>%1</span>").arg(QString(e.what()).toHtmlEscaped()));
        this->label_explanation->clear();
        this->controller->hide_orbital();
        return;
    }

    const OrbitalSet& set = this->result->orbital_sets[s];
    const PopulationAnalysis::Kind k = this->kind();
    const int sign = PopulationAnalysis::bonding_sign(k);
    const QString unit = PopulationAnalysis::unit(k);

    this->table->setHorizontalHeaderLabels({
        "#", "", "Energy (Ht)", "Occ.",
        PopulationAnalysis::short_name(k) + (unit.isEmpty() ? QString() : QString(" (%1)").arg(unit)),
        "◂ antibonding · bonding ▸", "Character"
    });

    // bars are scaled to the occupied orbitals; virtual orbitals may be clipped
    double scale = 0.0, scale_all = 0.0;
    for(size_t i = 0; i < set.size(); ++i) {
        scale_all = std::max(scale_all, std::abs(this->population.values[i]));
        if(set.occupations[i] > 0.0) {
            scale = std::max(scale, std::abs(this->population.values[i]));
        }
    }
    if(scale < 1e-8) {
        scale = scale_all;
    }
    const double threshold = std::max(1e-4, 0.03 * scale);

    this->table->setRowCount((int)set.size());
    int best = -1;
    double best_strength = 0.0;
    for(int i = 0; i < (int)set.size(); ++i) {
        const double v = this->population.values[i];
        const double strength = sign * v;
        const bool occupied = set.occupations[i] > 0.0;

        auto* idx = new QTableWidgetItem(QString::number(i + 1));
        idx->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        this->table->setItem(i, COL_INDEX, idx);
        this->table->setItem(i, COL_LABEL, new QTableWidgetItem(set.frontier_label(i)));
        this->table->setItem(i, COL_ENERGY, number_item(set.energies[i], 4));
        this->table->setItem(i, COL_OCC, number_item(set.occupations[i], 0));
        this->table->setItem(i, COL_VALUE, number_item(v, 4));

        auto* bar = new QTableWidgetItem;
        bar->setData(ROLE_STRENGTH, strength);
        bar->setData(ROLE_SCALE, scale);
        bar->setData(ROLE_OCCUPIED, occupied);
        bar->setToolTip(QString("%1 = %2").arg(PopulationAnalysis::short_name(k)).arg(v, 0, 'g', 8));
        this->table->setItem(i, COL_BAR, bar);

        QString character = "nonbonding";
        QColor color = this->palette().color(QPalette::PlaceholderText);
        if(std::abs(v) >= threshold) {
            character = strength > 0 ? "bonding" : "antibonding";
            color = strength > 0 ? COLOR_BONDING : COLOR_ANTIBONDING;
        }
        auto* item = new QTableWidgetItem(character);
        item->setForeground(color);
        this->table->setItem(i, COL_CHARACTER, item);

        if(occupied) {
            QFont f = this->table->font();
            f.setBold(true);
            for(int c : {COL_INDEX, COL_LABEL, COL_ENERGY, COL_OCC, COL_VALUE}) {
                this->table->item(i, c)->setFont(f);
            }
            if(strength > best_strength) {
                best_strength = strength;
                best = i;
            }
        }
    }

    this->update_summary();

    // keep the selection; initially show the most strongly bonding orbital
    int row = previous >= 0 && previous < (int)set.size() ? previous : (best >= 0 ? best : std::max(0, set.homo()));
    this->table->selectRow(row);
    this->table->scrollToItem(this->table->item(row, 0), QAbstractItemView::PositionAtCenter);
    this->show_selected_orbital();
}

void BondAnalysisDialog::update_summary() {
    const int s = this->combo_set->currentIndex();
    const OrbitalSet& set = this->result->orbital_sets[s];
    const PopulationAnalysis::Kind k = this->kind();
    const QString name = PopulationAnalysis::short_name(k);
    const QString unit = PopulationAnalysis::unit(k);
    const QString a = this->atom_label(this->combo_a->currentIndex());
    const QString b = this->atom_label(this->combo_b->currentIndex());
    const double sum = this->population.occupied_sum;
    const double strength = PopulationAnalysis::bonding_sign(k) * sum;

    QString verdict = "no net interaction";
    if(std::abs(sum) > 1e-4) {
        verdict = strength > 0 ? "<b style='color:#2e7d32'>net bonding</b>" : "<b style='color:#c62828'>net antibonding</b>";
    }
    QString spin_note;
    if(set.spin == "alpha" || set.spin == "beta") {
        spin_note = QString(" (%1 electrons only)").arg(set.spin == "alpha" ? "α" : "β");
    }
    this->label_summary->setText(QString("<b>Sum over the occupied orbitals%1:</b> %2 = %3%4 &rarr; %5 between %6 and %7.")
        .arg(spin_note, name).arg(sum, 0, 'f', 4).arg(unit.isEmpty() ? QString() : " " + unit)
        .arg(verdict, a, b));

    // formula, sign convention and the equivalent PyQInt call
    const QString factor = this->population.spin_factor == 2.0 ? "2 " : QString();
    QString matrix, meaning;
    switch(k) {
        case PopulationAnalysis::Kind::Hamilton:
            matrix = QString("F<sub>μν</sub>") + (spin_note.isEmpty() ? "" : QString("<sup>%1</sup>").arg(set.spin == "alpha" ? "α" : "β"));
            meaning = "The Fock matrix element F<sub>μν</sub> is (mostly) negative between overlapping basis functions, "
                      "so a <b>negative</b> MOHP means that the orbital lowers the energy through the A–B interaction "
                      "(bonding); a positive MOHP means antibonding.";
            break;
        case PopulationAnalysis::Kind::Overlap:
            matrix = "S<sub>μν</sub>";
            meaning = "A <b>positive</b> MOOP means constructive overlap between A and B (bonding), a negative "
                      "MOOP destructive overlap (antibonding). The sum over the occupied orbitals is the Mulliken "
                      "overlap population of the pair.";
            break;
        case PopulationAnalysis::Kind::BondIndex:
            matrix = QString("P<sub>μν</sub>") + (spin_note.isEmpty() ? "" : QString("<sup>%1</sup>").arg(set.spin == "alpha" ? "α" : "β"));
            meaning = "A <b>positive</b> MOBI indicates bonding, a negative MOBI antibonding.";
            break;
    }

    QString code;
    if(set.spin == "restricted") {
        const QString res = set.label.startsWith("Foster") ? "FosterBoys(res).run()" : "res";
        code = QString("<br>In PyQInt: <code>PopulationAnalysis(%1).%2(%3, %4)</code> (atoms are counted from 0).")
            .arg(res, PopulationAnalysis::pyqint_method(k))
            .arg(this->combo_a->currentIndex()).arg(this->combo_b->currentIndex());
    } else {
        code = "<br>PyQInt's PopulationAnalysis only supports restricted calculations; here the α and β "
               "orbitals are analysed separately with the spin-resolved matrices.";
    }

    this->label_explanation->setText(QString("<span style='color:gray'>%1<sub>k</sub> = %2Σ<sub>μ∈A</sub> Σ<sub>ν∈B</sub> "
                                             "C<sub>μk</sub> %3 C<sub>νk</sub>. %4 Only occupied orbitals (bold) "
                                             "contribute to the bond; the sum over them does not change when the "
                                             "orbitals are localized.%5</span>")
        .arg(name, factor, matrix, meaning, code));
}

void BondAnalysisDialog::show_selected_orbital() {
    const auto rows = this->table->selectionModel()->selectedRows();
    if(rows.isEmpty()) {
        this->controller->hide_orbital();
        return;
    }
    this->controller->show_orbital(this->combo_set->currentIndex(), rows.front().row());
}

void BondAnalysisDialog::copy_table() {
    QStringList lines;
    const PopulationAnalysis::Kind k = this->kind();
    lines << QString("# %1 between %2 and %3 (%4 orbitals)")
             .arg(PopulationAnalysis::long_name(k), this->atom_label(this->combo_a->currentIndex()),
                  this->atom_label(this->combo_b->currentIndex()), this->combo_set->currentText());
    lines << QString("orbital\tlabel\tenergy (Ht)\toccupation\t%1\tcharacter").arg(PopulationAnalysis::short_name(k));
    for(int i = 0; i < this->table->rowCount(); ++i) {
        lines << QString("%1\t%2\t%3\t%4\t%5\t%6")
                 .arg(i + 1)
                 .arg(this->table->item(i, COL_LABEL)->text())
                 .arg(this->result->orbital_sets[this->combo_set->currentIndex()].energies[i], 0, 'f', 8)
                 .arg(this->table->item(i, COL_OCC)->text())
                 .arg(this->population.values[i], 0, 'g', 10)
                 .arg(this->table->item(i, COL_CHARACTER)->text());
    }
    QApplication::clipboard()->setText(lines.join("\n") + "\n");
}
