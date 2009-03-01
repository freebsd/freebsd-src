/*
 * wpa_gui - AddInterface class
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
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

#ifndef ADDINTERFACE_H
#define ADDINTERFACE_H

#include <QObject>

#include <QtGui/QDialog>
#include <QtGui/QTreeWidget>
#include <QtGui/QVBoxLayout>

class WpaGui;

class AddInterface : public QDialog
{
	Q_OBJECT

public:
	AddInterface(WpaGui *_wpagui, QWidget *parent = 0);

public slots:
	virtual void interfaceSelected(QTreeWidgetItem *sel);

private:
	void addInterfaces();
	bool addRegistryInterface(const QString &ifname);

	QVBoxLayout *vboxLayout;
	QTreeWidget *interfaceWidget;
	WpaGui *wpagui;
};

#endif /* ADDINTERFACE_H */
