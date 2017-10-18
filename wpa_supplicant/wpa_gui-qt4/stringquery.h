/*
 * wpa_gui - StringQuery class
 * Copyright (c) 2009, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef STRINGQUERY_H
#define STRINGQUERY_H

#include <QDialog>
#include <QLineEdit>
#include <QGridLayout>

class StringQuery : public QDialog
{
	Q_OBJECT

public:
	StringQuery(QString label);
	QString get_string();

private:
	QLineEdit *edit;
};

#endif /* STRINGQUERY_H */
