/*
 * wpa_gui - AddInterface class
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef ADDINTERFACE_H
#define ADDINTERFACE_H

#include <QObject>

#include <QDialog>
#include <QTreeWidget>
#include <QVBoxLayout>

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
