/*
 * wpa_gui - WpaMsg class for storing event messages
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef WPAMSG_H
#define WPAMSG_H

#include <QDateTime>
#include <QLinkedList>

class WpaMsg {
public:
	WpaMsg() {}
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

typedef QLinkedList<WpaMsg> WpaMsgList;

#endif /* WPAMSG_H */
