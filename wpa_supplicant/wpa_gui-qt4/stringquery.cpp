/*
 * wpa_gui - StringQuery class
 * Copyright (c) 2009, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <cstdio>
#include <QLabel>

#include "stringquery.h"


StringQuery::StringQuery(QString label)
{
	edit = new QLineEdit;
	edit->setFocus();
	QGridLayout *layout = new QGridLayout;
	layout->addWidget(new QLabel(label), 0, 0);
	layout->addWidget(edit, 0, 1);
	setLayout(layout);

	connect(edit, SIGNAL(returnPressed()), this, SLOT(accept()));
}


QString StringQuery::get_string()
{
	return edit->text();
}
