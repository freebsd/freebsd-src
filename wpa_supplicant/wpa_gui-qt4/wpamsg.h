/*
 * wpa_gui - WpaMsg class for storing event messages
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPAMSG_H
#define WPAMSG_H

#include <QDateTime>
#include <QList>

class WpaMsg {
public:
	WpaMsg(const QString &_msg, int _priority = 2)
		: msg(_msg), priority(_priority)
	{
		timestamp = QDateTime::currentDateTime();
	}

	QString getMsg() const { return msg; }
	int getPriority() const { return priority; }
	QDateTime getTimestamp() const { return timestamp; }

private:
	QString msg;
	int priority;
	QDateTime timestamp;
};

typedef QList<WpaMsg> WpaMsgList;

#endif /* WPAMSG_H */
