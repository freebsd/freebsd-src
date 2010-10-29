/*
 * wpa_gui - Peers class
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

#include <cstdio>
#include <QImageReader>
#include <QMessageBox>

#include "common/wpa_ctrl.h"
#include "wpagui.h"
#include "stringquery.h"
#include "peers.h"


enum {
	peer_role_address = Qt::UserRole + 1,
	peer_role_type,
	peer_role_uuid,
	peer_role_details,
	peer_role_pri_dev_type,
	peer_role_ssid,
	peer_role_config_methods,
	peer_role_dev_passwd_id,
	peer_role_bss_id
};

/*
 * TODO:
 * - add current AP info (e.g., from WPS) in station mode
 */

enum peer_type {
	PEER_TYPE_ASSOCIATED_STATION,
	PEER_TYPE_AP,
	PEER_TYPE_AP_WPS,
	PEER_TYPE_WPS_PIN_NEEDED,
	PEER_TYPE_WPS_ER_AP,
	PEER_TYPE_WPS_ER_AP_UNCONFIGURED,
	PEER_TYPE_WPS_ER_ENROLLEE,
	PEER_TYPE_WPS_ENROLLEE
};


Peers::Peers(QWidget *parent, const char *, bool, Qt::WFlags)
	: QDialog(parent)
{
	setupUi(this);

	if (QImageReader::supportedImageFormats().contains(QByteArray("svg")))
	{
		default_icon = new QIcon(":/icons/wpa_gui.svg");
		ap_icon = new QIcon(":/icons/ap.svg");
		laptop_icon = new QIcon(":/icons/laptop.svg");
	} else {
		default_icon = new QIcon(":/icons/wpa_gui.png");
		ap_icon = new QIcon(":/icons/ap.png");
		laptop_icon = new QIcon(":/icons/laptop.png");
	}

	peers->setModel(&model);
	peers->setResizeMode(QListView::Adjust);

	peers->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(peers, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(context_menu(const QPoint &)));

	wpagui = NULL;
}


void Peers::setWpaGui(WpaGui *_wpagui)
{
	wpagui = _wpagui;
	update_peers();
}


Peers::~Peers()
{
	delete default_icon;
	delete ap_icon;
	delete laptop_icon;
}


void Peers::languageChange()
{
	retranslateUi(this);
}


QString Peers::ItemType(int type)
{
	QString title;
	switch (type) {
	case PEER_TYPE_ASSOCIATED_STATION:
		title = tr("Associated station");
		break;
	case PEER_TYPE_AP:
		title = tr("AP");
		break;
	case PEER_TYPE_AP_WPS:
		title = tr("WPS AP");
		break;
	case PEER_TYPE_WPS_PIN_NEEDED:
		title = tr("WPS PIN needed");
		break;
	case PEER_TYPE_WPS_ER_AP:
		title = tr("ER: WPS AP");
		break;
	case PEER_TYPE_WPS_ER_AP_UNCONFIGURED:
		title = tr("ER: WPS AP (Unconfigured)");
		break;
	case PEER_TYPE_WPS_ER_ENROLLEE:
		title = tr("ER: WPS Enrollee");
		break;
	case PEER_TYPE_WPS_ENROLLEE:
		title = tr("WPS Enrollee");
		break;
	}
	return title;
}


void Peers::context_menu(const QPoint &pos)
{
	QMenu *menu = new QMenu;
	if (menu == NULL)
		return;

	QModelIndex idx = peers->indexAt(pos);
	if (idx.isValid()) {
		ctx_item = model.itemFromIndex(idx);
		int type = ctx_item->data(peer_role_type).toInt();
		menu->addAction(Peers::ItemType(type))->setEnabled(false);
		menu->addSeparator();

		int config_methods = -1;
		QVariant var = ctx_item->data(peer_role_config_methods);
		if (var.isValid())
			config_methods = var.toInt();

		if ((type == PEER_TYPE_ASSOCIATED_STATION ||
		     type == PEER_TYPE_AP_WPS ||
		     type == PEER_TYPE_WPS_PIN_NEEDED ||
		     type == PEER_TYPE_WPS_ER_ENROLLEE ||
		     type == PEER_TYPE_WPS_ENROLLEE) &&
		    (config_methods == -1 || (config_methods & 0x010c))) {
			menu->addAction(tr("Enter WPS PIN"), this,
					SLOT(enter_pin()));
		}

		if (type == PEER_TYPE_AP_WPS) {
			menu->addAction(tr("Connect (PBC)"), this,
					SLOT(connect_pbc()));
		}

		if ((type == PEER_TYPE_ASSOCIATED_STATION ||
		     type == PEER_TYPE_WPS_ER_ENROLLEE ||
		     type == PEER_TYPE_WPS_ENROLLEE) &&
		    config_methods >= 0 && (config_methods & 0x0080)) {
			menu->addAction(tr("Enroll (PBC)"), this,
					SLOT(connect_pbc()));
		}

		if (type == PEER_TYPE_WPS_ER_AP) {
			menu->addAction(tr("Learn Configuration"), this,
					SLOT(learn_ap_config()));
		}

		menu->addAction(tr("Properties"), this, SLOT(properties()));
	} else {
		ctx_item = NULL;
		menu->addAction(QString(tr("Refresh")), this,
				SLOT(ctx_refresh()));
	}

	menu->exec(peers->mapToGlobal(pos));
}


void Peers::enter_pin()
{
	if (ctx_item == NULL)
		return;

	int peer_type = ctx_item->data(peer_role_type).toInt();
	QString uuid;
	QString addr;
	if (peer_type == PEER_TYPE_WPS_ER_ENROLLEE)
		uuid = ctx_item->data(peer_role_uuid).toString();
	else
		addr = ctx_item->data(peer_role_address).toString();

	StringQuery input(tr("PIN:"));
	input.setWindowTitle(tr("PIN for ") + ctx_item->text());
	if (input.exec() != QDialog::Accepted)
		return;

	char cmd[100];
	char reply[100];
	size_t reply_len;

	if (peer_type == PEER_TYPE_WPS_ER_ENROLLEE) {
		snprintf(cmd, sizeof(cmd), "WPS_ER_PIN %s %s",
			 uuid.toAscii().constData(),
			 input.get_string().toAscii().constData());
	} else {
		snprintf(cmd, sizeof(cmd), "WPS_PIN %s %s",
			 addr.toAscii().constData(),
			 input.get_string().toAscii().constData());
	}
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to set the WPS PIN."));
		msg.exec();
	}
}


void Peers::ctx_refresh()
{
	update_peers();
}


void Peers::add_station(QString info)
{
	QStringList lines = info.split(QRegExp("\\n"));
	QString name;

	for (QStringList::Iterator it = lines.begin();
	     it != lines.end(); it++) {
		int pos = (*it).indexOf('=') + 1;
		if (pos < 1)
			continue;

		if ((*it).startsWith("wpsDeviceName="))
			name = (*it).mid(pos);
	}

	if (name.isEmpty())
		name = lines[0];

	QStandardItem *item = new QStandardItem(*laptop_icon, name);
	if (item) {
		item->setData(lines[0], peer_role_address);
		item->setData(PEER_TYPE_ASSOCIATED_STATION,
			      peer_role_type);
		item->setData(info, peer_role_details);
		item->setToolTip(ItemType(PEER_TYPE_ASSOCIATED_STATION));
		model.appendRow(item);
	}
}


void Peers::add_stations()
{
	char reply[2048];
	size_t reply_len;
	char cmd[30];
	int res;

	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest("STA-FIRST", reply, &reply_len) < 0)
		return;

	do {
		reply[reply_len] = '\0';
		QString info(reply);
		char *txt = reply;
		while (*txt != '\0' && *txt != '\n')
			txt++;
		*txt++ = '\0';
		if (strncmp(reply, "FAIL", 4) == 0 ||
		    strncmp(reply, "UNKNOWN", 7) == 0)
			break;

		add_station(info);

		reply_len = sizeof(reply) - 1;
		snprintf(cmd, sizeof(cmd), "STA-NEXT %s", reply);
		res = wpagui->ctrlRequest(cmd, reply, &reply_len);
	} while (res >= 0);
}


void Peers::add_single_station(const char *addr)
{
	char reply[2048];
	size_t reply_len;
	char cmd[30];

	reply_len = sizeof(reply) - 1;
	snprintf(cmd, sizeof(cmd), "STA %s", addr);
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0)
		return;

	reply[reply_len] = '\0';
	QString info(reply);
	char *txt = reply;
	while (*txt != '\0' && *txt != '\n')
		txt++;
	*txt++ = '\0';
	if (strncmp(reply, "FAIL", 4) == 0 ||
	    strncmp(reply, "UNKNOWN", 7) == 0)
		return;

	add_station(info);
}


void Peers::remove_bss(int id)
{
	if (model.rowCount() == 0)
		return;

	QModelIndexList lst = model.match(model.index(0, 0), peer_role_bss_id,
					  id);
	if (lst.size() == 0)
		return;
	model.removeRow(lst[0].row());
}


bool Peers::add_bss(const char *cmd)
{
	char reply[2048];
	size_t reply_len;

	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0)
		return false;
	reply[reply_len] = '\0';

	QString bss(reply);
	if (bss.isEmpty() || bss.startsWith("FAIL"))
		return false;

	QString ssid, bssid, flags, wps_name, pri_dev_type;
	int id = -1;

	QStringList lines = bss.split(QRegExp("\\n"));
	for (QStringList::Iterator it = lines.begin();
	     it != lines.end(); it++) {
		int pos = (*it).indexOf('=') + 1;
		if (pos < 1)
			continue;

		if ((*it).startsWith("bssid="))
			bssid = (*it).mid(pos);
		else if ((*it).startsWith("id="))
			id = (*it).mid(pos).toInt();
		else if ((*it).startsWith("flags="))
			flags = (*it).mid(pos);
		else if ((*it).startsWith("ssid="))
			ssid = (*it).mid(pos);
		else if ((*it).startsWith("wps_device_name="))
			wps_name = (*it).mid(pos);
		else if ((*it).startsWith("wps_primary_device_type="))
			pri_dev_type = (*it).mid(pos);
	}

	QString name = wps_name;
	if (name.isEmpty())
		name = ssid + "\n" + bssid;

	QStandardItem *item = new QStandardItem(*ap_icon, name);
	if (item) {
		item->setData(bssid, peer_role_address);
		if (id >= 0)
			item->setData(id, peer_role_bss_id);
		int type;
		if (flags.contains("[WPS"))
			type = PEER_TYPE_AP_WPS;
		else
			type = PEER_TYPE_AP;
		item->setData(type, peer_role_type);

		for (int i = 0; i < lines.size(); i++) {
			if (lines[i].length() > 60) {
				lines[i].remove(60, lines[i].length());
				lines[i] += "..";
			}
		}
		item->setToolTip(ItemType(type));
		item->setData(lines.join("\n"), peer_role_details);
		if (!pri_dev_type.isEmpty())
			item->setData(pri_dev_type,
				      peer_role_pri_dev_type);
		if (!ssid.isEmpty())
			item->setData(ssid, peer_role_ssid);
		model.appendRow(item);
	}

	return true;
}


void Peers::add_scan_results()
{
	int index;
	char cmd[20];

	index = 0;
	while (wpagui) {
		snprintf(cmd, sizeof(cmd), "BSS %d", index++);
		if (index > 1000)
			break;

		if (!add_bss(cmd))
			break;
	}
}


void Peers::update_peers()
{
	model.clear();
	if (wpagui == NULL)
		return;

	char reply[20];
	size_t replylen = sizeof(reply) - 1;
	wpagui->ctrlRequest("WPS_ER_START", reply, &replylen);

	add_stations();
	add_scan_results();
}


QStandardItem * Peers::find_addr(QString addr)
{
	if (model.rowCount() == 0)
		return NULL;

	QModelIndexList lst = model.match(model.index(0, 0), peer_role_address,
					  addr);
	if (lst.size() == 0)
		return NULL;
	return model.itemFromIndex(lst[0]);
}


QStandardItem * Peers::find_uuid(QString uuid)
{
	if (model.rowCount() == 0)
		return NULL;

	QModelIndexList lst = model.match(model.index(0, 0), peer_role_uuid,
					  uuid);
	if (lst.size() == 0)
		return NULL;
	return model.itemFromIndex(lst[0]);
}


void Peers::event_notify(WpaMsg msg)
{
	QString text = msg.getMsg();

	if (text.startsWith(WPS_EVENT_PIN_NEEDED)) {
		/*
		 * WPS-PIN-NEEDED 5a02a5fa-9199-5e7c-bc46-e183d3cb32f7
		 * 02:2a:c4:18:5b:f3
		 * [Wireless Client|Company|cmodel|123|12345|1-0050F204-1]
		 */
		QStringList items = text.split(' ');
		QString uuid = items[1];
		QString addr = items[2];
		QString name = "";

		QStandardItem *item = find_addr(addr);
		if (item)
			return;

		int pos = text.indexOf('[');
		if (pos >= 0) {
			int pos2 = text.lastIndexOf(']');
			if (pos2 >= pos) {
				items = text.mid(pos + 1, pos2 - pos - 1).
					split('|');
				name = items[0];
				items.append(addr);
			}
		}

		item = new QStandardItem(*laptop_icon, name);
		if (item) {
			item->setData(addr, peer_role_address);
			item->setData(PEER_TYPE_WPS_PIN_NEEDED,
				      peer_role_type);
			item->setToolTip(ItemType(PEER_TYPE_WPS_PIN_NEEDED));
			item->setData(items.join("\n"), peer_role_details);
			item->setData(items[5], peer_role_pri_dev_type);
			model.appendRow(item);
		}
		return;
	}

	if (text.startsWith(AP_STA_CONNECTED)) {
		/* AP-STA-CONNECTED 02:2a:c4:18:5b:f3 */
		QStringList items = text.split(' ');
		QString addr = items[1];
		QStandardItem *item = find_addr(addr);
		if (item == NULL || item->data(peer_role_type).toInt() !=
		    PEER_TYPE_ASSOCIATED_STATION)
			add_single_station(addr.toAscii().constData());
		return;
	}

	if (text.startsWith(AP_STA_DISCONNECTED)) {
		/* AP-STA-DISCONNECTED 02:2a:c4:18:5b:f3 */
		QStringList items = text.split(' ');
		QString addr = items[1];

		if (model.rowCount() == 0)
			return;

		QModelIndexList lst = model.match(model.index(0, 0),
						  peer_role_address, addr);
		for (int i = 0; i < lst.size(); i++) {
			QStandardItem *item = model.itemFromIndex(lst[i]);
			if (item && item->data(peer_role_type).toInt() ==
			    PEER_TYPE_ASSOCIATED_STATION)
				model.removeRow(lst[i].row());
		}
		return;
	}

	if (text.startsWith(WPS_EVENT_ER_AP_ADD)) {
		/*
		 * WPS-ER-AP-ADD 87654321-9abc-def0-1234-56789abc0002
		 * 02:11:22:33:44:55 pri_dev_type=6-0050F204-1 wps_state=1
		 * |Very friendly name|Company|Long description of the model|
		 * WAP|http://w1.fi/|http://w1.fi/hostapd/
		 */
		QStringList items = text.split(' ');
		if (items.size() < 5)
			return;
		QString uuid = items[1];
		QString addr = items[2];
		QString pri_dev_type = items[3].mid(13);
		int wps_state = items[4].mid(10).toInt();

		int pos = text.indexOf('|');
		if (pos < 0)
			return;
		items = text.mid(pos + 1).split('|');
		if (items.size() < 1)
			return;

		QStandardItem *item = find_uuid(uuid);
		if (item)
			return;

		item = new QStandardItem(*ap_icon, items[0]);
		if (item) {
			item->setData(uuid, peer_role_uuid);
			item->setData(addr, peer_role_address);
			int type = wps_state == 2 ? PEER_TYPE_WPS_ER_AP:
				PEER_TYPE_WPS_ER_AP_UNCONFIGURED;
			item->setData(type, peer_role_type);
			item->setToolTip(ItemType(type));
			item->setData(pri_dev_type, peer_role_pri_dev_type);
			item->setData(items.join(QString("\n")),
				      peer_role_details);
			model.appendRow(item);
		}

		return;
	}

	if (text.startsWith(WPS_EVENT_ER_AP_REMOVE)) {
		/* WPS-ER-AP-REMOVE 87654321-9abc-def0-1234-56789abc0002 */
		QStringList items = text.split(' ');
		if (items.size() < 2)
			return;
		if (model.rowCount() == 0)
			return;

		QModelIndexList lst = model.match(model.index(0, 0),
						  peer_role_uuid, items[1]);
		for (int i = 0; i < lst.size(); i++) {
			QStandardItem *item = model.itemFromIndex(lst[i]);
			if (item &&
			    (item->data(peer_role_type).toInt() ==
			     PEER_TYPE_WPS_ER_AP ||
			     item->data(peer_role_type).toInt() ==
			     PEER_TYPE_WPS_ER_AP_UNCONFIGURED))
				model.removeRow(lst[i].row());
		}
		return;
	}

	if (text.startsWith(WPS_EVENT_ER_ENROLLEE_ADD)) {
		/*
		 * WPS-ER-ENROLLEE-ADD 2b7093f1-d6fb-5108-adbb-bea66bb87333
		 * 02:66:a0:ee:17:27 M1=1 config_methods=0x14d dev_passwd_id=0
		 * pri_dev_type=1-0050F204-1
		 * |Wireless Client|Company|cmodel|123|12345|
		 */
		QStringList items = text.split(' ');
		if (items.size() < 3)
			return;
		QString uuid = items[1];
		QString addr = items[2];
		QString pri_dev_type = items[6].mid(13);
		int config_methods = -1;
		int dev_passwd_id = -1;

		for (int i = 3; i < items.size(); i++) {
			int pos = items[i].indexOf('=') + 1;
			if (pos < 1)
				continue;
			QString val = items[i].mid(pos);
			if (items[i].startsWith("config_methods=")) {
				config_methods = val.toInt(0, 0);
			} else if (items[i].startsWith("dev_passwd_id=")) {
				dev_passwd_id = val.toInt();
			}
		}

		int pos = text.indexOf('|');
		if (pos < 0)
			return;
		items = text.mid(pos + 1).split('|');
		if (items.size() < 1)
			return;
		QString name = items[0];
		if (name.length() == 0)
			name = addr;

		remove_enrollee_uuid(uuid);

		QStandardItem *item;
		item = new QStandardItem(*laptop_icon, name);
		if (item) {
			item->setData(uuid, peer_role_uuid);
			item->setData(addr, peer_role_address);
			item->setData(PEER_TYPE_WPS_ER_ENROLLEE,
				      peer_role_type);
			item->setToolTip(ItemType(PEER_TYPE_WPS_ER_ENROLLEE));
			item->setData(items.join(QString("\n")),
				      peer_role_details);
			item->setData(pri_dev_type, peer_role_pri_dev_type);
			if (config_methods >= 0)
				item->setData(config_methods,
					      peer_role_config_methods);
			if (dev_passwd_id >= 0)
				item->setData(dev_passwd_id,
					      peer_role_dev_passwd_id);
			model.appendRow(item);
		}

		return;
	}

	if (text.startsWith(WPS_EVENT_ER_ENROLLEE_REMOVE)) {
		/*
		 * WPS-ER-ENROLLEE-REMOVE 2b7093f1-d6fb-5108-adbb-bea66bb87333
		 * 02:66:a0:ee:17:27
		 */
		QStringList items = text.split(' ');
		if (items.size() < 2)
			return;
		remove_enrollee_uuid(items[1]);
		return;
	}

	if (text.startsWith(WPS_EVENT_ENROLLEE_SEEN)) {
		/* TODO: need to time out this somehow or remove on successful
		 * WPS run, etc. */
		/*
		 * WPS-ENROLLEE-SEEN 02:00:00:00:01:00
		 * 572cf82f-c957-5653-9b16-b5cfb298abf1 1-0050F204-1 0x80 4 1
		 * [Wireless Client]
		 * (MAC addr, UUID-E, pri dev type, config methods,
		 * dev passwd id, request type, [dev name])
		 */
		QStringList items = text.split(' ');
		if (items.size() < 7)
			return;
		QString addr = items[1];
		QString uuid = items[2];
		QString pri_dev_type = items[3];
		int config_methods = items[4].toInt(0, 0);
		int dev_passwd_id = items[5].toInt();
		QString name;

		int pos = text.indexOf('[');
		if (pos >= 0) {
			int pos2 = text.lastIndexOf(']');
			if (pos2 >= pos) {
				QStringList items2 =
					text.mid(pos + 1, pos2 - pos - 1).
					split('|');
				name = items2[0];
			}
		}
		if (name.isEmpty())
			name = addr;

		QStandardItem *item;

		item = find_uuid(uuid);
		if (item) {
			QVariant var = item->data(peer_role_config_methods);
			QVariant var2 = item->data(peer_role_dev_passwd_id);
			if ((var.isValid() && config_methods != var.toInt()) ||
			    (var2.isValid() && dev_passwd_id != var2.toInt()))
				remove_enrollee_uuid(uuid);
			else
				return;
		}

		item = new QStandardItem(*laptop_icon, name);
		if (item) {
			item->setData(uuid, peer_role_uuid);
			item->setData(addr, peer_role_address);
			item->setData(PEER_TYPE_WPS_ENROLLEE,
				      peer_role_type);
			item->setToolTip(ItemType(PEER_TYPE_WPS_ENROLLEE));
			item->setData(items.join(QString("\n")),
				      peer_role_details);
			item->setData(pri_dev_type, peer_role_pri_dev_type);
			item->setData(config_methods,
				      peer_role_config_methods);
			item->setData(dev_passwd_id, peer_role_dev_passwd_id);
			model.appendRow(item);
		}

		return;
	}

	if (text.startsWith(WPA_EVENT_BSS_ADDED)) {
		/* CTRL-EVENT-BSS-ADDED 34 00:11:22:33:44:55 */
		QStringList items = text.split(' ');
		if (items.size() < 2)
			return;
		char cmd[20];
		snprintf(cmd, sizeof(cmd), "BSS ID-%d", items[1].toInt());
		add_bss(cmd);
		return;
	}

	if (text.startsWith(WPA_EVENT_BSS_REMOVED)) {
		/* CTRL-EVENT-BSS-REMOVED 34 00:11:22:33:44:55 */
		QStringList items = text.split(' ');
		if (items.size() < 2)
			return;
		remove_bss(items[1].toInt());
		return;
	}
}


void Peers::closeEvent(QCloseEvent *)
{
	if (wpagui) {
		char reply[20];
		size_t replylen = sizeof(reply) - 1;
		wpagui->ctrlRequest("WPS_ER_STOP", reply, &replylen);
	}
}


void Peers::done(int r)
{
	QDialog::done(r);
	close();
}


void Peers::remove_enrollee_uuid(QString uuid)
{
	if (model.rowCount() == 0)
		return;

	QModelIndexList lst = model.match(model.index(0, 0),
					  peer_role_uuid, uuid);
	for (int i = 0; i < lst.size(); i++) {
		QStandardItem *item = model.itemFromIndex(lst[i]);
		if (item == NULL)
			continue;
		int type = item->data(peer_role_type).toInt();
		if (type == PEER_TYPE_WPS_ER_ENROLLEE ||
		    type == PEER_TYPE_WPS_ENROLLEE)
			model.removeRow(lst[i].row());
	}
}


void Peers::properties()
{
	if (ctx_item == NULL)
		return;

	QMessageBox msg(this);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setDefaultButton(QMessageBox::Ok);
	msg.setEscapeButton(QMessageBox::Ok);
	msg.setWindowTitle(tr("Peer Properties"));

	int type = ctx_item->data(peer_role_type).toInt();
	QString title = Peers::ItemType(type);

	msg.setText(title + QString("\n") + tr("Name: ") + ctx_item->text());

	QVariant var;
	QString info;

	var = ctx_item->data(peer_role_address);
	if (var.isValid())
		info += tr("Address: ") + var.toString() + QString("\n");

	var = ctx_item->data(peer_role_uuid);
	if (var.isValid())
		info += tr("UUID: ") + var.toString() + QString("\n");

	var = ctx_item->data(peer_role_pri_dev_type);
	if (var.isValid())
		info += tr("Primary Device Type: ") + var.toString() +
			QString("\n");

	var = ctx_item->data(peer_role_ssid);
	if (var.isValid())
		info += tr("SSID: ") + var.toString() + QString("\n");

	var = ctx_item->data(peer_role_config_methods);
	if (var.isValid()) {
		int methods = var.toInt();
		info += tr("Configuration Methods: ");
		if (methods & 0x0001)
			info += tr("[USBA]");
		if (methods & 0x0002)
			info += tr("[Ethernet]");
		if (methods & 0x0004)
			info += tr("[Label]");
		if (methods & 0x0008)
			info += tr("[Display]");
		if (methods & 0x0010)
			info += tr("[Ext. NFC Token]");
		if (methods & 0x0020)
			info += tr("[Int. NFC Token]");
		if (methods & 0x0040)
			info += tr("[NFC Interface]");
		if (methods & 0x0080)
			info += tr("[Push Button]");
		if (methods & 0x0100)
			info += tr("[Keypad]");
		info += "\n";
	}

	var = ctx_item->data(peer_role_dev_passwd_id);
	if (var.isValid()) {
		info += tr("Device Password ID: ") + var.toString();
		switch (var.toInt()) {
		case 0:
			info += tr(" (Default PIN)");
			break;
		case 1:
			info += tr(" (User-specified PIN)");
			break;
		case 2:
			info += tr(" (Machine-specified PIN)");
			break;
		case 3:
			info += tr(" (Rekey)");
			break;
		case 4:
			info += tr(" (Push Button)");
			break;
		case 5:
			info += tr(" (Registrar-specified)");
			break;
		}
		info += "\n";
	}

	msg.setInformativeText(info);

	var = ctx_item->data(peer_role_details);
	if (var.isValid())
		msg.setDetailedText(var.toString());

	msg.exec();
}


void Peers::connect_pbc()
{
	if (ctx_item == NULL)
		return;

	char cmd[100];
	char reply[100];
	size_t reply_len;

	int peer_type = ctx_item->data(peer_role_type).toInt();
	if (peer_type == PEER_TYPE_WPS_ER_ENROLLEE) {
		snprintf(cmd, sizeof(cmd), "WPS_ER_PBC %s",
			 ctx_item->data(peer_role_uuid).toString().toAscii().
			 constData());
	} else {
		snprintf(cmd, sizeof(cmd), "WPS_PBC");
	}
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to start WPS PBC."));
		msg.exec();
	}
}


void Peers::learn_ap_config()
{
	if (ctx_item == NULL)
		return;

	QString uuid = ctx_item->data(peer_role_uuid).toString();

	StringQuery input(tr("AP PIN:"));
	input.setWindowTitle(tr("AP PIN for ") + ctx_item->text());
	if (input.exec() != QDialog::Accepted)
		return;

	char cmd[100];
	char reply[100];
	size_t reply_len;

	snprintf(cmd, sizeof(cmd), "WPS_ER_LEARN %s %s",
		 uuid.toAscii().constData(),
		 input.get_string().toAscii().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to start learning AP configuration."));
		msg.exec();
	}
}
