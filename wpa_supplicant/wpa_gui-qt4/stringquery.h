/*
 * wpa_gui - StringQuery class
 * Copyright (c) 2009, Atheros Communications
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
