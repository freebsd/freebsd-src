/*
 * wpa_gui - UserDataRequest class
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

#include "userdatarequest.h"
#include "wpagui.h"
#include "wpa_ctrl.h"


UserDataRequest::UserDataRequest(QWidget *parent, const char *, bool,
				 Qt::WFlags)
	: QDialog(parent)
{
	setupUi(this);

	connect(buttonOk, SIGNAL(clicked()), this, SLOT(sendReply()));
	connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
	connect(queryEdit, SIGNAL(returnPressed()), this, SLOT(sendReply()));
}


UserDataRequest::~UserDataRequest()
{
}


void UserDataRequest::languageChange()
{
	retranslateUi(this);
}


int UserDataRequest::setParams(WpaGui *_wpagui, const char *reqMsg)
{
	char *tmp, *pos, *pos2;
	wpagui = _wpagui;
	tmp = strdup(reqMsg);
	if (tmp == NULL)
		return -1;
	pos = strchr(tmp, '-');
	if (pos == NULL) {
		free(tmp);
		return -1;
	}
	*pos++ = '\0';
	field = tmp;
	pos2 = strchr(pos, ':');
	if (pos2 == NULL) {
		free(tmp);
		return -1;
	}
	*pos2++ = '\0';

	networkid = atoi(pos);
	queryInfo->setText(pos2);
	if (strcmp(tmp, "PASSWORD") == 0) {
		queryField->setText("Password: ");
		queryEdit->setEchoMode(QLineEdit::Password);
	} else if (strcmp(tmp, "NEW_PASSWORD") == 0) {
		queryField->setText("New password: ");
		queryEdit->setEchoMode(QLineEdit::Password);
	} else if (strcmp(tmp, "IDENTITY") == 0)
		queryField->setText("Identity: ");
	else if (strcmp(tmp, "PASSPHRASE") == 0) {
		queryField->setText("Private key passphrase: ");
		queryEdit->setEchoMode(QLineEdit::Password);
	} else
		queryField->setText(field + ":");
	free(tmp);

	return 0;
}


void UserDataRequest::sendReply()
{
	char reply[10];
	size_t reply_len = sizeof(reply);

	if (wpagui == NULL) {
		reject();
		return;
	}

	QString cmd = QString(WPA_CTRL_RSP) + field + '-' +
		QString::number(networkid) + ':' +
		queryEdit->text();
	wpagui->ctrlRequest(cmd.ascii(), reply, &reply_len);
	accept();
}
