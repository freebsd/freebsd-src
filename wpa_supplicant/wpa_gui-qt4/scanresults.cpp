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

#include <cstdio>

#include "scanresults.h"
#include "wpagui.h"
#include "networkconfig.h"


ScanResults::ScanResults(QWidget *parent, const char *, bool, Qt::WFlags)
	: QDialog(parent)
{
	setupUi(this);

	connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(scanButton, SIGNAL(clicked()), this, SLOT(scanRequest()));
	connect(scanResultsWidget,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this,
		SLOT(bssSelected(QTreeWidgetItem *)));

	wpagui = NULL;
	scanResultsWidget->setItemsExpandable(FALSE);
	scanResultsWidget->setRootIsDecorated(FALSE);
}


ScanResults::~ScanResults()
{
}


void ScanResults::languageChange()
{
	retranslateUi(this);
}


void ScanResults::setWpaGui(WpaGui *_wpagui)
{
	wpagui = _wpagui;
	updateResults();
}


void ScanResults::updateResults()
{
	char reply[2048];
	size_t reply_len;
	int index;
	char cmd[20];

	scanResultsWidget->clear();

	index = 0;
	while (wpagui) {
		snprintf(cmd, sizeof(cmd), "BSS %d", index++);
		if (index > 1000)
			break;

		reply_len = sizeof(reply) - 1;
		if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0)
			break;
		reply[reply_len] = '\0';

		QString bss(reply);
		if (bss.isEmpty() || bss.startsWith("FAIL"))
			break;

		QString ssid, bssid, freq, signal, flags;

		QStringList lines = bss.split(QRegExp("\\n"));
		for (QStringList::Iterator it = lines.begin();
		     it != lines.end(); it++) {
			int pos = (*it).indexOf('=') + 1;
			if (pos < 1)
				continue;

			if ((*it).startsWith("bssid="))
				bssid = (*it).mid(pos);
			else if ((*it).startsWith("freq="))
				freq = (*it).mid(pos);
			else if ((*it).startsWith("qual="))
				signal = (*it).mid(pos);
			else if ((*it).startsWith("flags="))
				flags = (*it).mid(pos);
			else if ((*it).startsWith("ssid="))
				ssid = (*it).mid(pos);
		}

		QTreeWidgetItem *item = new QTreeWidgetItem(scanResultsWidget);
		if (item) {
			item->setText(0, ssid);
			item->setText(1, bssid);
			item->setText(2, freq);
			item->setText(3, signal);
			item->setText(4, flags);
		}

		if (bssid.isEmpty())
			break;
	}
}


void ScanResults::scanRequest()
{
	char reply[10];
	size_t reply_len = sizeof(reply);
    
	if (wpagui == NULL)
		return;
    
	wpagui->ctrlRequest("SCAN", reply, &reply_len);
}


void ScanResults::getResults()
{
	updateResults();
}


void ScanResults::bssSelected(QTreeWidgetItem *sel)
{
	NetworkConfig *nc = new NetworkConfig();
	if (nc == NULL)
		return;
	nc->setWpaGui(wpagui);
	nc->paramsFromScanResults(sel);
	nc->show();
	nc->exec();
}
