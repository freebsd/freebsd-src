/*
 * wpa_gui - ScanResults class
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
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
		    bool modal = false, Qt::WindowFlags fl = Qt::Widget);
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
