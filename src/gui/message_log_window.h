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

#include <QDialog>

class MessageLog;
class QPlainTextEdit;

/**
 * @brief Shows the diagnostic messages collected by MessageLog, live
 */
class MessageLogWindow : public QDialog {
    Q_OBJECT

private:
    MessageLog* log;
    QPlainTextEdit* text;
    qint64 last_seq = 0;

public:
    explicit MessageLogWindow(MessageLog* log, QWidget* parent = nullptr);

private slots:
    void on_message_logged(qint64 seq, const QString& line);
    void copy_all();
    void save();
    void clear();
};
