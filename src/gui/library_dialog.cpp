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

#include "library_dialog.h"

#include <algorithm>
#include <stdexcept>

#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

LibraryDialog::LibraryDialog(QWidget* parent) :
    QDialog(parent) {

    this->setWindowTitle("Molecule library");
    this->resize(460, 520);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Select a molecule to load. Small molecules calculate fastest."));

    this->filter = new QLineEdit;
    this->filter->setPlaceholderText("Filter by name or formula...");
    this->filter->setClearButtonEnabled(true);
    layout->addWidget(this->filter);

    this->list = new QTreeWidget;
    this->list->setColumnCount(3);
    this->list->setHeaderLabels({"Name", "Formula", "Atoms"});
    this->list->setRootIsDecorated(false);
    this->list->setAlternatingRowColors(true);
    this->list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(this->list, 1);

    this->molecules = load_library();
    for(size_t i = 0; i < this->molecules.size(); ++i) {
        const Molecule& mol = this->molecules[i];
        auto* item = new QTreeWidgetItem({mol.get_name(), mol.formula(), QString::number(mol.size())});
        item->setData(0, Qt::UserRole, (int)i);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        this->list->addTopLevelItem(item);
    }
    if(this->list->topLevelItemCount() > 0) {
        this->list->setCurrentItem(this->list->topLevelItem(0));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this->list, &QTreeWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(this->filter, &QLineEdit::textChanged, this, &LibraryDialog::apply_filter);
}

std::vector<Molecule> LibraryDialog::load_library() {
    std::vector<Molecule> result;
    const QDir dir(":/assets/molecules");
    for(const QString& f : dir.entryList({"*.xyz"}, QDir::Files, QDir::Name)) {
        try {
            result.push_back(Molecule::from_xyz_file(dir.filePath(f)));
        } catch(const std::exception& e) {
            qWarning() << "Cannot load library molecule" << f << ":" << e.what();
        }
    }

    std::stable_sort(result.begin(), result.end(), [](const Molecule& a, const Molecule& b) {
        return a.size() < b.size();
    });
    return result;
}

Molecule LibraryDialog::selected_molecule() const {
    const auto* item = this->list->currentItem();
    if(item == nullptr || item->isHidden()) {
        return Molecule();
    }
    return this->molecules[item->data(0, Qt::UserRole).toInt()];
}

void LibraryDialog::apply_filter(const QString& text) {
    QTreeWidgetItem* first_visible = nullptr;
    for(int i = 0; i < this->list->topLevelItemCount(); ++i) {
        auto* item = this->list->topLevelItem(i);
        const bool match = text.isEmpty() ||
                           item->text(0).contains(text, Qt::CaseInsensitive) ||
                           item->text(1).contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
        if(match && first_visible == nullptr) {
            first_visible = item;
        }
    }
    if(first_visible != nullptr && (this->list->currentItem() == nullptr || this->list->currentItem()->isHidden())) {
        this->list->setCurrentItem(first_visible);
    }
}
