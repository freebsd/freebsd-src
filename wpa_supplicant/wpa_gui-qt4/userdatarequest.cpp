/*
 * wpa_gui - UserDataRequest class
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "userdatarequest.h"
#include "wpagui.h"
#include "common/wpa_ctrl.h"


UserDataRequest::UserDataRequest(QWidget *parent, const char *, bool,
				 Qt::WindowFlags)
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
		queryField->setText(tr("Password: "));
		queryEdit->setEchoMode(QLineEdit::Password);
	} else if (strcmp(tmp, "NEW_PASSWORD") == 0) {
		queryField->setText(tr("New password: "));
		queryEdit->setEchoMode(QLineEdit::Password);
	} else if (strcmp(tmp, "IDENTITY") == 0)
		queryField->setText(tr("Identity: "));
	else if (strcmp(tmp, "PASSPHRASE") == 0) {
		queryField->setText(tr("Private key passphrase: "));
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
	wpagui->ctrlRequest(cmd.toLocal8Bit().constData(), reply, &reply_len);
	accept();
}
