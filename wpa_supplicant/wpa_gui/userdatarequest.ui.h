/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include <stdlib.h>

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
