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

#include "message_log.h"

#include <cstdio>
#include <cstdlib>

#include <QMutexLocker>
#include <QTime>

QtMessageHandler MessageLog::previous_handler = nullptr;

MessageLog::MessageLog(int _max_entries, QObject* parent) :
    QObject(parent),
    max_entries(_max_entries) {}

MessageLog* MessageLog::instance() {
    // deliberately never destroyed: messages can still arrive during shutdown
    static MessageLog* log = new MessageLog;
    return log;
}

void MessageLog::install() {
    instance();
    previous_handler = qInstallMessageHandler(&MessageLog::handler);
}

void MessageLog::set_echo_all(bool echo) {
    QMutexLocker lock(&this->mutex);
    this->echo_all = echo;
}

void MessageLog::record(QtMsgType type, const QString& category, const QString& message) {
    const QString line = format_line(type, category, message);
    qint64 seq = 0;
    {
        QMutexLocker lock(&this->mutex);
        this->lines.append(line);
        while(this->lines.size() > this->max_entries) {
            this->lines.removeFirst();
        }
        seq = ++this->last_seq;
    }
    emit message_logged(seq, line);
}

QStringList MessageLog::entries(qint64* seq) const {
    QMutexLocker lock(&this->mutex);
    if(seq != nullptr) {
        *seq = this->last_seq;
    }
    return this->lines;
}

void MessageLog::clear() {
    QMutexLocker lock(&this->mutex);
    this->lines.clear();
}

QString MessageLog::format_line(QtMsgType type, const QString& category, const QString& message) {
    const char* severity = "debug";
    switch(type) {
        case QtDebugMsg:    severity = "debug"; break;
        case QtInfoMsg:     severity = "info"; break;
        case QtWarningMsg:  severity = "warning"; break;
        case QtCriticalMsg: severity = "critical"; break;
        case QtFatalMsg:    severity = "fatal"; break;
    }
    QString line = QString("%1  %2").arg(QTime::currentTime().toString("hh:mm:ss.zzz"), QString(severity).leftJustified(9));
    if(!category.isEmpty() && category != "default") {
        line += QString("[%1] ").arg(category);
    }
    return line + message;
}

void MessageLog::handler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    MessageLog* log = instance();
    log->record(type, QString::fromLatin1(context.category), message);

    bool echo = type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg;
    if(!echo) {
        QMutexLocker lock(&log->mutex);
        echo = log->echo_all;
    }
    if(echo) {
        if(previous_handler != nullptr) {
            previous_handler(type, context, message);       // aborts for fatal messages
        } else {
            std::fprintf(stderr, "%s\n", qPrintable(message));
        }
    }
    if(type == QtFatalMsg) {
        std::abort();
    }
}
