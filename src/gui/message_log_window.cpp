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

#include "message_log_window.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextStream>
#include <QVBoxLayout>

#include "icons.h"
#include "util/message_log.h"

MessageLogWindow::MessageLogWindow(MessageLog* _log, QWidget* parent) :
    QDialog(parent),
    log(_log) {

    this->setWindowTitle("Message log");
    this->resize(900, 500);
    this->setSizeGripEnabled(true);

    auto* layout = new QVBoxLayout(this);

    this->text = new QPlainTextEdit;
    this->text->setReadOnly(true);
    this->text->setLineWrapMode(QPlainTextEdit::NoWrap);
    this->text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->text->setMaximumBlockCount(MessageLog::DEFAULT_MAX_ENTRIES);
    this->text->setPlaceholderText("Diagnostic messages of the program (rendering, file loading, ...) appear here.");
    layout->addWidget(this->text);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton* button_copy = buttons->addButton("Copy", QDialogButtonBox::ActionRole);
    button_copy->setIcon(bluecurve_icon("edit-copy"));
    QPushButton* button_save = buttons->addButton("Save...", QDialogButtonBox::ActionRole);
    button_save->setIcon(bluecurve_icon("document-save"));
    QPushButton* button_clear = buttons->addButton("Clear", QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(button_copy, &QPushButton::clicked, this, &MessageLogWindow::copy_all);
    connect(button_save, &QPushButton::clicked, this, &MessageLogWindow::save);
    connect(button_clear, &QPushButton::clicked, this, &MessageLogWindow::clear);

    // subscribe before taking the snapshot; messages already in the snapshot are skipped
    connect(this->log, &MessageLog::message_logged, this, &MessageLogWindow::on_message_logged, Qt::QueuedConnection);
    const QStringList lines = this->log->entries(&this->last_seq);
    this->text->setPlainText(lines.join('\n'));
    this->text->verticalScrollBar()->setValue(this->text->verticalScrollBar()->maximum());
}

void MessageLogWindow::on_message_logged(qint64 seq, const QString& line) {
    if(seq <= this->last_seq) {
        return;
    }
    this->last_seq = seq;
    QScrollBar* bar = this->text->verticalScrollBar();
    const bool at_end = bar->value() == bar->maximum();
    this->text->appendPlainText(line);
    if(at_end) {
        bar->setValue(bar->maximum());
    }
}

void MessageLogWindow::copy_all() {
    QApplication::clipboard()->setText(this->text->toPlainText());
}

void MessageLogWindow::save() {
    const QString path = QFileDialog::getSaveFileName(this, "Save message log",
        QDir::home().filePath("praxis.log"), "Log files (*.log *.txt);;All files (*)");
    if(path.isEmpty()) {
        return;
    }
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Cannot save message log", QString("Cannot write %1").arg(path));
        return;
    }
    QTextStream(&file) << this->text->toPlainText() << '\n';
}

void MessageLogWindow::clear() {
    this->log->clear();
    this->text->clear();
}
