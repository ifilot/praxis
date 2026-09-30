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

#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

/**
 * @brief Collects the diagnostic messages of the program (qDebug, qInfo,
 *        qWarning, ...) instead of printing them to the terminal
 *
 * Once installed as the Qt message handler, every message is timestamped
 * and kept in memory (the oldest messages are dropped beyond a maximum) so
 * that it can be inspected via Help -> Message log. Warnings and errors are
 * still written to the terminal as well; debug and info messages only when
 * echoing is enabled (--verbose). The handler may be called from any thread.
 */
class MessageLog : public QObject {
    Q_OBJECT

private:
    mutable QMutex mutex;
    QStringList lines;
    qint64 last_seq = 0;                // sequence number of the newest message
    int max_entries;
    bool echo_all = false;

    static QtMessageHandler previous_handler;

public:
    static constexpr int DEFAULT_MAX_ENTRIES = 10000;

    explicit MessageLog(int max_entries = DEFAULT_MAX_ENTRIES, QObject* parent = nullptr);

    /**
     * @brief The log used by the program (lives until the program exits)
     */
    static MessageLog* instance();

    /**
     * @brief Install instance() as the Qt message handler
     */
    static void install();

    /**
     * @brief Also write debug and info messages to the terminal
     */
    void set_echo_all(bool echo);

    /**
     * @brief Store a message and announce it via message_logged()
     */
    void record(QtMsgType type, const QString& category, const QString& message);

    /**
     * @brief All stored messages, oldest first
     *
     * @param seq  receives the sequence number of the newest message
     */
    QStringList entries(qint64* seq = nullptr) const;

    void clear();

    /**
     * @brief Single log line: time, severity, category (unless default) and message
     */
    static QString format_line(QtMsgType type, const QString& category, const QString& message);

signals:
    /**
     * @brief Emitted for every stored message, possibly from another thread;
     *        connect with Qt::QueuedConnection to avoid recursion when the
     *        receiver itself logs
     */
    void message_logged(qint64 seq, const QString& line);

private:
    static void handler(QtMsgType type, const QMessageLogContext& context, const QString& message);
};
