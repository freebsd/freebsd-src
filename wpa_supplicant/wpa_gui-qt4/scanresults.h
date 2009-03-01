/*
 * wpa_gui - ScanResults class
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

#ifndef SCANRESULTS_H
#define SCANRESULTS_H

#include <QObject>
#include "ui_scanresults.h"

class WpaGui;

class ScanResults : public QDialog, public Ui::ScanResults
{
	Q_OBJECT

public:
	ScanResults(QWidget *parent = 0, const char *name = 0,
		    bool modal = false, Qt::WFlags fl = 0);
	~ScanResults();

public slots:
	virtual void setWpaGui(WpaGui *_wpagui);
	virtual void updateResults();
	virtual void scanRequest();
	virtual void getResults();
	virtual void bssSelected(QTreeWidgetItem *sel);

protected slots:
	virtual void languageChange();

private:
	WpaGui *wpagui;
};

#endif /* SCANRESULTS_H */
