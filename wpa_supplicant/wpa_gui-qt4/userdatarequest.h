/*
 * wpa_gui - UserDataRequest class
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef USERDATAREQUEST_H
#define USERDATAREQUEST_H

#include <QObject>
#include "ui_userdatarequest.h"

class WpaGui;

class UserDataRequest : public QDialog, public Ui::UserDataRequest
{
	Q_OBJECT

public:
	UserDataRequest(QWidget *parent = 0, const char *name = 0,
			bool modal = false, Qt::WindowFlags fl = 0);
	~UserDataRequest();

	int setParams(WpaGui *_wpagui, const char *reqMsg);

public slots:
	virtual void sendReply();

protected slots:
	virtual void languageChange();

private:
	WpaGui *wpagui;
	int networkid;
	QString field;
};

#endif /* USERDATAREQUEST_H */
