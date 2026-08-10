/*
 * wpa_gui - Peers class
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
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
	peer_role_ifname,
	peer_role_pri_dev_type,
	peer_role_ssid,
	peer_role_config_methods,
	peer_role_dev_passwd_id,
	peer_role_bss_id,
	peer_role_selected_method,
	peer_role_selected_pin,
	peer_role_requested_method,
	peer_role_network_id
};

enum selected_method {
	SEL_METHOD_NONE,
	SEL_METHOD_PIN_PEER_DISPLAY,
	SEL_METHOD_PIN_LOCAL_DISPLAY
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
	PEER_TYPE_P2P,
	PEER_TYPE_P2P_CLIENT,
	PEER_TYPE_P2P_GROUP,
	PEER_TYPE_P2P_PERSISTENT_GROUP_GO,
	PEER_TYPE_P2P_PERSISTENT_GROUP_CLIENT,
	PEER_TYPE_P2P_INVITATION,
	PEER_TYPE_WPS_ER_AP,
	PEER_TYPE_WPS_ER_AP_UNCONFIGURED,
	PEER_TYPE_WPS_ER_ENROLLEE,
	PEER_TYPE_WPS_ENROLLEE
};


Peers::Peers(QWidget *parent, const char *, bool, Qt::WindowFlags)
	: QDialog(parent)
{
	setupUi(this);

	if (QImageReader::supportedImageFormats().contains(QByteArray("svg")))
	{
		default_icon = new QIcon(":/icons/wpa_gui.svg");
		ap_icon = new QIcon(":/icons/ap.svg");
		laptop_icon = new QIcon(":/icons/laptop.svg");
		group_icon = new QIcon(":/icons/group.svg");
		invitation_icon = new QIcon(":/icons/invitation.svg");
	} else {
		default_icon = new QIcon(":/icons/wpa_gui.png");
		ap_icon = new QIcon(":/icons/ap.png");
		laptop_icon = new QIcon(":/icons/laptop.png");
		group_icon = new QIcon(":/icons/group.png");
		invitation_icon = new QIcon(":/icons/invitation.png");
	}

	peers->setModel(&model);
	peers->setResizeMode(QListView::Adjust);
	peers->setDragEnabled(false);
	peers->setSelectionMode(QAbstractItemView::NoSelection);

	peers->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(peers, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(context_menu(const QPoint &)));

	wpagui = NULL;
	hide_ap = false;
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
	delete group_icon;
	delete invitation_icon;
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
	case PEER_TYPE_P2P:
		title = tr("P2P Device");
		break;
	case PEER_TYPE_P2P_CLIENT:
		title = tr("P2P Device (group client)");
		break;
	case PEER_TYPE_P2P_GROUP:
		title = tr("P2P Group");
		break;
	case PEER_TYPE_P2P_PERSISTENT_GROUP_GO:
		title = tr("P2P Persistent Group (GO)");
		break;
	case PEER_TYPE_P2P_PERSISTENT_GROUP_CLIENT:
		title = tr("P2P Persistent Group (client)");
		break;
	case PEER_TYPE_P2P_INVITATION:
		title = tr("P2P Invitation");
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

		enum selected_method method = SEL_METHOD_NONE;
		var = ctx_item->data(peer_role_selected_method);
		if (var.isValid())
			method = (enum selected_method) var.toInt();

		if ((type == PEER_TYPE_ASSOCIATED_STATION ||
		     type == PEER_TYPE_AP_WPS ||
		     type == PEER_TYPE_WPS_PIN_NEEDED ||
		     type == PEER_TYPE_WPS_ER_ENROLLEE ||
		     type == PEER_TYPE_WPS_ENROLLEE) &&
		    (config_methods == -1 || (config_methods & 0x010c))) {
			menu->addAction(tr("Enter WPS PIN"), this,
					SLOT(enter_pin()));
		}

		if (type == PEER_TYPE_P2P || type == PEER_TYPE_P2P_CLIENT) {
			menu->addAction(tr("P2P Connect"), this,
					SLOT(ctx_p2p_connect()));
			if (method == SEL_METHOD_NONE &&
			    config_methods > -1 &&
			    config_methods & 0x0080 /* PBC */ &&
			    config_methods != 0x0080)
				menu->addAction(tr("P2P Connect (PBC)"), this,
						SLOT(connect_pbc()));
			if (method == SEL_METHOD_NONE) {
				menu->addAction(tr("P2P Request PIN"), this,
						SLOT(ctx_p2p_req_pin()));
				menu->addAction(tr("P2P Show PIN"), this,
						SLOT(ctx_p2p_show_pin()));
			}

			if (config_methods > -1 && (config_methods & 0x0100)) {
				/* Peer has Keypad */
				menu->addAction(tr("P2P Display PIN"), this,
						SLOT(ctx_p2p_display_pin()));
			}

			if (config_methods > -1 && (config_methods & 0x000c)) {
				/* Peer has Label or Display */
				menu->addAction(tr("P2P Enter PIN"), this,
						SLOT(ctx_p2p_enter_pin()));
			}
		}

		if (type == PEER_TYPE_P2P_GROUP) {
			menu->addAction(tr("Show passphrase"), this,
					SLOT(ctx_p2p_show_passphrase()));
			menu->addAction(tr("Remove P2P Group"), this,
					SLOT(ctx_p2p_remove_group()));
		}

		if (type == PEER_TYPE_P2P_PERSISTENT_GROUP_GO ||
		    type == PEER_TYPE_P2P_PERSISTENT_GROUP_CLIENT ||
		    type == PEER_TYPE_P2P_INVITATION) {
			menu->addAction(tr("Start group"), this,
					SLOT(ctx_p2p_start_persistent()));
		}

		if (type == PEER_TYPE_P2P_PERSISTENT_GROUP_GO ||
		    type == PEER_TYPE_P2P_PERSISTENT_GROUP_CLIENT) {
			menu->addAction(tr("Invite"), this,
					SLOT(ctx_p2p_invite()));
		}

		if (type == PEER_TYPE_P2P_INVITATION) {
			menu->addAction(tr("Ignore"), this,
					SLOT(ctx_p2p_delete()));
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
		menu->addAction(tr("Start P2P discovery"), this,
				SLOT(ctx_p2p_start()));
		menu->addAction(tr("Stop P2P discovery"), this,
				SLOT(ctx_p2p_stop()));
		menu->addAction(tr("P2P listen only"), this,
				SLOT(ctx_p2p_listen()));
		menu->addAction(tr("Start P2P group"), this,
				SLOT(ctx_p2p_start_group()));
		if (hide_ap)
			menu->addAction(tr("Show AP entries"), this,
					SLOT(ctx_show_ap()));
		else
			menu->addAction(tr("Hide AP entries"), this,
					SLOT(ctx_hide_ap()));
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
	addr = ctx_item->data(peer_role_address).toString();
	if (peer_type == PEER_TYPE_WPS_ER_ENROLLEE)
		uuid = ctx_item->data(peer_role_uuid).toString();

	StringQuery input(tr("PIN:"));
	input.setWindowTitle(tr("PIN for ") + ctx_item->text());
	if (input.exec() != QDialog::Accepted)
		return;

	char cmd[100];
	char reply[100];
	size_t reply_len;

	if (peer_type == PEER_TYPE_WPS_ER_ENROLLEE) {
		snprintf(cmd, sizeof(cmd), "WPS_ER_PIN %s %s %s",
			 uuid.toLocal8Bit().constData(),
			 input.get_string().toLocal8Bit().constData(),
			 addr.toLocal8Bit().constData());
	} else {
		snprintf(cmd, sizeof(cmd), "WPS_PIN %s %s",
			 addr.toLocal8Bit().constData(),
			 input.get_string().toLocal8Bit().constData());
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


void Peers::ctx_p2p_start()
{
	char reply[20];
	size_t reply_len;
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest("P2P_FIND", reply, &reply_len) < 0 ||
	    memcmp(reply, "FAIL", 4) == 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to start P2P discovery.");
		msg.exec();
	}
}


void Peers::ctx_p2p_stop()
{
	char reply[20];
	size_t reply_len;
	reply_len = sizeof(reply) - 1;
	wpagui->ctrlRequest("P2P_STOP_FIND", reply, &reply_len);
}


void Peers::ctx_p2p_listen()
{
	char reply[20];
	size_t reply_len;
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest("P2P_LISTEN 3600", reply, &reply_len) < 0 ||
	    memcmp(reply, "FAIL", 4) == 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to start P2P listen.");
		msg.exec();
	}
}


void Peers::ctx_p2p_start_group()
{
	char reply[20];
	size_t reply_len;
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest("P2P_GROUP_ADD", reply, &reply_len) < 0 ||
	    memcmp(reply, "FAIL", 4) == 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to start P2P group.");
		msg.exec();
	}
}


void Peers::add_station(QString info)
{
	QStringList lines = info.split(QRegularExpression("\\n"));
	QString name;

	for (QStringList::Iterator it = lines.begin();
	     it != lines.end(); it++) {
		int pos = (*it).indexOf('=') + 1;
		if (pos < 1)
			continue;

		if ((*it).startsWith("wpsDeviceName="))
			name = (*it).mid(pos);
		else if ((*it).startsWith("p2p_device_name="))
			name = (*it).mid(pos);
	}

	if (name.isEmpty())
		name = lines[0];

	QStandardItem *item = new QStandardItem(*laptop_icon, name);
	if (item) {
		/* Remove WPS enrollee entry if one is still pending */
		if (model.rowCount() > 0) {
			QModelIndexList lst = model.match(model.index(0, 0),
							  peer_role_address,
							  lines[0]);
			for (int i = 0; i < lst.size(); i++) {
				QStandardItem *item;
				item = model.itemFromIndex(lst[i]);
				if (item == NULL)
					continue;
				int type = item->data(peer_role_type).toInt();
				if (type == PEER_TYPE_WPS_ENROLLEE) {
					model.removeRow(lst[i].row());
					break;
				}
			}
		}

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
		res = snprintf(cmd, sizeof(cmd), "STA-NEXT %s", reply);
		if (res < 0 || (size_t) res >= sizeof(cmd))
			break;
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


void Peers::add_p2p_group_client(QStandardItem * /*parent*/, QString params)
{
	/*
	 * dev=02:b5:64:63:30:63 iface=02:b5:64:63:30:63 dev_capab=0x0
	 * dev_type=1-0050f204-1 dev_name='Wireless Client'
	 * config_methods=0x8c
	 */

	QStringList items =
		params.split(QRegularExpression(" (?=[^']*('[^']*'[^']*)*$)"));
	QString addr = "";
	QString name = "";
	int config_methods = 0;
	QString dev_type;

	for (int i = 0; i < items.size(); i++) {
		QString str = items.at(i);
		int pos = str.indexOf('=') + 1;
		if (str.startsWith("dev_name='"))
			name = str.section('\'', 1, -2);
		else if (str.startsWith("config_methods="))
			config_methods =
				str.section('=', 1).toInt(0, 0);
		else if (str.startsWith("dev="))
			addr = str.mid(pos);
		else if (str.startsWith("dev_type=") && dev_type.isEmpty())
			dev_type = str.mid(pos);
	}

	QStandardItem *item = find_addr(addr);
	if (item)
		return;

	item = new QStandardItem(*default_icon, name);
	if (item) {
		/* TODO: indicate somehow the relationship to the group owner
		 * (parent) */
		item->setData(addr, peer_role_address);
		item->setData(config_methods, peer_role_config_methods);
		item->setData(PEER_TYPE_P2P_CLIENT, peer_role_type);
		if (!dev_type.isEmpty())
			item->setData(dev_type, peer_role_pri_dev_type);
		item->setData(items.join(QString("\n")), peer_role_details);
		item->setToolTip(ItemType(PEER_TYPE_P2P_CLIENT));
		model.appendRow(item);
	}
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

	if (hide_ap)
		return false;

	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0)
		return false;
	reply[reply_len] = '\0';

	QString bss(reply);
	if (bss.isEmpty() || bss.startsWith("FAIL"))
		return false;

	QString ssid, bssid, flags, wps_name, pri_dev_type;
	int id = -1;

	QStringList lines = bss.split(QRegularExpression("\\n"));
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

		lines = bss.split(QRegularExpression("\\n"));
		for (QStringList::Iterator it = lines.begin();
		     it != lines.end(); it++) {
			if ((*it).startsWith("p2p_group_client:"))
				add_p2p_group_client(item,
						     (*it).mid(18));
		}
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


void Peers::add_persistent(int id, const char *ssid, const char *bssid)
{
	char cmd[100];
	char reply[100];
	size_t reply_len;
	int mode;

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d mode", id);
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0)
		return;
	reply[reply_len] = '\0';
	mode = atoi(reply);

	QString name = ssid;
	name = '[' + name + ']';

	QStandardItem *item = new QStandardItem(*group_icon, name);
	if (!item)
		return;

	int type;
	if (mode == 3)
		type = PEER_TYPE_P2P_PERSISTENT_GROUP_GO;
	else
		type = PEER_TYPE_P2P_PERSISTENT_GROUP_CLIENT;
	item->setData(type, peer_role_type);
	item->setToolTip(ItemType(type));
	item->setData(ssid, peer_role_ssid);
	if (bssid && strcmp(bssid, "any") == 0)
		bssid = NULL;
	if (bssid)
		item->setData(bssid, peer_role_address);
	item->setData(id, peer_role_network_id);
	item->setBackground(Qt::BDiagPattern);

	model.appendRow(item);
}


void Peers::add_persistent_groups()
{
	char buf[2048], *start, *end, *id, *ssid, *bssid, *flags;
	size_t len;

	len = sizeof(buf) - 1;
	if (wpagui->ctrlRequest("LIST_NETWORKS", buf, &len) < 0)
		return;

	buf[len] = '\0';
	start = strchr(buf, '\n');
	if (start == NULL)
		return;
	start++;

	while (*start) {
		bool last = false;
		end = strchr(start, '\n');
		if (end == NULL) {
			last = true;
			end = start;
			while (end[0] && end[1])
				end++;
		}
		*end = '\0';

		id = start;
		ssid = strchr(id, '\t');
		if (ssid == NULL)
			break;
		*ssid++ = '\0';
		bssid = strchr(ssid, '\t');
		if (bssid == NULL)
			break;
		*bssid++ = '\0';
		flags = strchr(bssid, '\t');
		if (flags == NULL)
			break;
		*flags++ = '\0';

		if (strstr(flags, "[DISABLED][P2P-PERSISTENT]"))
			add_persistent(atoi(id), ssid, bssid);

		if (last)
			break;
		start = end + 1;
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
	add_persistent_groups();
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


QStandardItem * Peers::find_addr_type(QString addr, int type)
{
	if (model.rowCount() == 0)
		return NULL;

	QModelIndexList lst = model.match(model.index(0, 0), peer_role_address,
					  addr);
	for (int i = 0; i < lst.size(); i++) {
		QStandardItem *item = model.itemFromIndex(lst[i]);
		if (item->data(peer_role_type).toInt() == type)
			return item;
	}
	return NULL;
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
			add_single_station(addr.toLocal8Bit().constData());
		return;
	}

	if (text.startsWith(AP_STA_DISCONNECTED)) {
		/* AP-STA-DISCONNECTED 02:2a:c4:18:5b:f3 */
		QStringList items = text.split(' ');
		QString addr = items[1];

		if (model.rowCount() == 0)
			return;

		QModelIndexList lst = model.match(model.index(0, 0),
						  peer_role_address, addr, -1);
		for (int i = 0; i < lst.size(); i++) {
			QStandardItem *item = model.itemFromIndex(lst[i]);
			if (item && item->data(peer_role_type).toInt() ==
			    PEER_TYPE_ASSOCIATED_STATION) {
				model.removeRow(lst[i].row());
				break;
			}
		}
		return;
	}

	if (text.startsWith(P2P_EVENT_DEVICE_FOUND)) {
		/*
		 * P2P-DEVICE-FOUND 02:b5:64:63:30:63
		 * p2p_dev_addr=02:b5:64:63:30:63 pri_dev_type=1-0050f204-1
		 * name='Wireless Client' config_methods=0x84 dev_capab=0x21
		 * group_capab=0x0
		 */
		QStringList items =
			text.split(QRegularExpression(" (?=[^']*('[^']*'[^']*)*$)"));
		QString addr = items[1];
		QString name = "";
		QString pri_dev_type;
		int config_methods = 0;
		for (int i = 0; i < items.size(); i++) {
			QString str = items.at(i);
			if (str.startsWith("name='"))
				name = str.section('\'', 1, -2);
			else if (str.startsWith("config_methods="))
				config_methods =
					str.section('=', 1).toInt(0, 0);
			else if (str.startsWith("pri_dev_type="))
				pri_dev_type = str.section('=', 1);
		}

		QStandardItem *item = find_addr(addr);
		if (item) {
			int type = item->data(peer_role_type).toInt();
			if (type == PEER_TYPE_P2P)
				return;
		}

		item = new QStandardItem(*default_icon, name);
		if (item) {
			item->setData(addr, peer_role_address);
			item->setData(config_methods,
				      peer_role_config_methods);
			item->setData(PEER_TYPE_P2P, peer_role_type);
			if (!pri_dev_type.isEmpty())
				item->setData(pri_dev_type,
					      peer_role_pri_dev_type);
			item->setData(items.join(QString("\n")),
				      peer_role_details);
			item->setToolTip(ItemType(PEER_TYPE_P2P));
			model.appendRow(item);
		}

		item = find_addr_type(addr,
				      PEER_TYPE_P2P_PERSISTENT_GROUP_CLIENT);
		if (item)
			item->setBackground(Qt::NoBrush);
	}

	if (text.startsWith(P2P_EVENT_GROUP_STARTED)) {
		/* P2P-GROUP-STARTED wlan0-p2p-0 GO ssid="DIRECT-3F"
		 * passphrase="YOyTkxID" go_dev_addr=02:40:61:c2:f3:b7
		 * [PERSISTENT] */
		QStringList items = text.split(' ');
		if (items.size() < 4)
			return;

		int pos = text.indexOf(" ssid=\"");
		if (pos < 0)
			return;
		QString ssid = text.mid(pos + 7);
		pos = ssid.indexOf(" passphrase=\"");
		if (pos < 0)
			pos = ssid.indexOf(" psk=");
		if (pos >= 0)
			ssid.truncate(pos);
		pos = ssid.lastIndexOf('"');
		if (pos >= 0)
			ssid.truncate(pos);

		QStandardItem *item = new QStandardItem(*group_icon, ssid);
		if (item) {
			item->setData(PEER_TYPE_P2P_GROUP, peer_role_type);
			item->setData(items[1], peer_role_ifname);
			QString details;
			if (items[2] == "GO") {
				details = tr("P2P GO for interface ") +
					items[1];
			} else {
				details = tr("P2P client for interface ") +
					items[1];
			}
			if (text.contains(" [PERSISTENT]"))
				details += "\nPersistent group";
			item->setData(details, peer_role_details);
			item->setToolTip(ItemType(PEER_TYPE_P2P_GROUP));
			model.appendRow(item);
		}
	}

	if (text.startsWith(P2P_EVENT_GROUP_REMOVED)) {
		/* P2P-GROUP-REMOVED wlan0-p2p-0 GO */
		QStringList items = text.split(' ');
		if (items.size() < 2)
			return;

		if (model.rowCount() == 0)
			return;

		QModelIndexList lst = model.match(model.index(0, 0),
						  peer_role_ifname, items[1]);
		for (int i = 0; i < lst.size(); i++)
			model.removeRow(lst[i].row());
		return;
	}

	if (text.startsWith(P2P_EVENT_PROV_DISC_SHOW_PIN)) {
		/* P2P-PROV-DISC-SHOW-PIN 02:40:61:c2:f3:b7 12345670 */
		QStringList items = text.split(' ');
		if (items.size() < 3)
			return;
		QString addr = items[1];
		QString pin = items[2];

		QStandardItem *item = find_addr_type(addr, PEER_TYPE_P2P);
		if (item == NULL)
			return;
		item->setData(SEL_METHOD_PIN_LOCAL_DISPLAY,
			      peer_role_selected_method);
		item->setData(pin, peer_role_selected_pin);
		QVariant var = item->data(peer_role_requested_method);
		if (var.isValid() &&
		    var.toInt() == SEL_METHOD_PIN_LOCAL_DISPLAY) {
			ctx_item = item;
			ctx_p2p_display_pin_pd();
		}
		return;
	}

	if (text.startsWith(P2P_EVENT_PROV_DISC_ENTER_PIN)) {
		/* P2P-PROV-DISC-ENTER-PIN 02:40:61:c2:f3:b7 */
		QStringList items = text.split(' ');
		if (items.size() < 2)
			return;
		QString addr = items[1];

		QStandardItem *item = find_addr_type(addr, PEER_TYPE_P2P);
		if (item == NULL)
			return;
		item->setData(SEL_METHOD_PIN_PEER_DISPLAY,
			      peer_role_selected_method);
		QVariant var = item->data(peer_role_requested_method);
		if (var.isValid() &&
		    var.toInt() == SEL_METHOD_PIN_PEER_DISPLAY) {
			ctx_item = item;
			ctx_p2p_connect();
		}
		return;
	}

	if (text.startsWith(P2P_EVENT_INVITATION_RECEIVED)) {
		/* P2P-INVITATION-RECEIVED sa=02:f0:bc:44:87:62 persistent=4 */
		QStringList items = text.split(' ');
		if (items.size() < 3)
			return;
		if (!items[1].startsWith("sa=") ||
		    !items[2].startsWith("persistent="))
			return;
		QString addr = items[1].mid(3);
		int id = items[2].mid(11).toInt();

		char cmd[100];
		char reply[100];
		size_t reply_len;

		snprintf(cmd, sizeof(cmd), "GET_NETWORK %d ssid", id);
		reply_len = sizeof(reply) - 1;
		if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0)
			return;
		reply[reply_len] = '\0';
		QString name;
		char *pos = strrchr(reply, '"');
		if (pos && reply[0] == '"') {
			*pos = '\0';
			name = reply + 1;
		} else
			name = reply;

		QStandardItem *item;
		item = find_addr_type(addr, PEER_TYPE_P2P_INVITATION);
		if (item)
			model.removeRow(item->row());

		item = new QStandardItem(*invitation_icon, name);
		if (!item)
			return;
		item->setData(PEER_TYPE_P2P_INVITATION, peer_role_type);
		item->setToolTip(ItemType(PEER_TYPE_P2P_INVITATION));
		item->setData(addr, peer_role_address);
		item->setData(id, peer_role_network_id);

		model.appendRow(item);

		enable_persistent(id);

		return;
	}

	if (text.startsWith(P2P_EVENT_INVITATION_RESULT)) {
		/* P2P-INVITATION-RESULT status=1 */
		/* TODO */
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

		QStandardItem *item = find_addr(addr);
		if (item) {
			int type = item->data(peer_role_type).toInt();
			if (type == PEER_TYPE_ASSOCIATED_STATION)
				return; /* already associated */
		}

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


void Peers::ctx_p2p_connect()
{
	if (ctx_item == NULL)
		return;
	QString addr = ctx_item->data(peer_role_address).toString();
	QString arg;
	int config_methods =
		ctx_item->data(peer_role_config_methods).toInt();
	enum selected_method method = SEL_METHOD_NONE;
	QVariant var = ctx_item->data(peer_role_selected_method);
	if (var.isValid())
		method = (enum selected_method) var.toInt();
	if (method == SEL_METHOD_PIN_LOCAL_DISPLAY) {
		arg = ctx_item->data(peer_role_selected_pin).toString();
		char cmd[100];
		char reply[100];
		size_t reply_len;
		snprintf(cmd, sizeof(cmd), "P2P_CONNECT %s %s display",
			 addr.toLocal8Bit().constData(),
			 arg.toLocal8Bit().constData());
		reply_len = sizeof(reply) - 1;
		if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
			QMessageBox msg;
			msg.setIcon(QMessageBox::Warning);
			msg.setText("Failed to initiate P2P connect.");
			msg.exec();
			return;
		}
		QMessageBox::information(this,
					 tr("PIN for ") + ctx_item->text(),
					 tr("Enter the following PIN on the\n"
					    "peer device: ") + arg);
	} else if (method == SEL_METHOD_PIN_PEER_DISPLAY) {
		StringQuery input(tr("PIN from peer display:"));
		input.setWindowTitle(tr("PIN for ") + ctx_item->text());
		if (input.exec() != QDialog::Accepted)
			return;
		arg = input.get_string();
	} else if (config_methods == 0x0080 /* PBC */) {
		arg = "pbc";
	} else {
		StringQuery input(tr("PIN:"));
		input.setWindowTitle(tr("PIN for ") + ctx_item->text());
		if (input.exec() != QDialog::Accepted)
			return;
		arg = input.get_string();
	}

	char cmd[100];
	char reply[100];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "P2P_CONNECT %s %s",
		 addr.toLocal8Bit().constData(),
		 arg.toLocal8Bit().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to initiate P2P connect.");
		msg.exec();
	}
}


void Peers::ctx_p2p_req_pin()
{
	if (ctx_item == NULL)
		return;
	QString addr = ctx_item->data(peer_role_address).toString();
	ctx_item->setData(SEL_METHOD_PIN_PEER_DISPLAY,
			  peer_role_requested_method);

	char cmd[100];
	char reply[100];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "P2P_PROV_DISC %s display",
		 addr.toLocal8Bit().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to request PIN from peer."));
		msg.exec();
	}
}


void Peers::ctx_p2p_show_pin()
{
	if (ctx_item == NULL)
		return;
	QString addr = ctx_item->data(peer_role_address).toString();
	ctx_item->setData(SEL_METHOD_PIN_LOCAL_DISPLAY,
			  peer_role_requested_method);

	char cmd[100];
	char reply[100];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "P2P_PROV_DISC %s keypad",
		 addr.toLocal8Bit().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to request peer to enter PIN."));
		msg.exec();
	}
}


void Peers::ctx_p2p_display_pin()
{
	if (ctx_item == NULL)
		return;
	QString addr = ctx_item->data(peer_role_address).toString();

	char cmd[100];
	char reply[100];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "P2P_CONNECT %s pin",
		 addr.toLocal8Bit().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to initiate P2P connect.");
		msg.exec();
		return;
	}
	reply[reply_len] = '\0';
	QMessageBox::information(this,
				 tr("PIN for ") + ctx_item->text(),
				 tr("Enter the following PIN on the\n"
				    "peer device: ") + reply);
}


void Peers::ctx_p2p_display_pin_pd()
{
	if (ctx_item == NULL)
		return;
	QString addr = ctx_item->data(peer_role_address).toString();
	QString arg = ctx_item->data(peer_role_selected_pin).toString();

	char cmd[100];
	char reply[100];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "P2P_CONNECT %s %s display",
		 addr.toLocal8Bit().constData(),
		 arg.toLocal8Bit().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to initiate P2P connect.");
		msg.exec();
		return;
	}
	reply[reply_len] = '\0';
	QMessageBox::information(this,
				 tr("PIN for ") + ctx_item->text(),
				 tr("Enter the following PIN on the\n"
				    "peer device: ") + arg);
}


void Peers::ctx_p2p_enter_pin()
{
	if (ctx_item == NULL)
		return;
	QString addr = ctx_item->data(peer_role_address).toString();
	QString arg;

	StringQuery input(tr("PIN from peer:"));
	input.setWindowTitle(tr("PIN for ") + ctx_item->text());
	if (input.exec() != QDialog::Accepted)
		return;
	arg = input.get_string();

	char cmd[100];
	char reply[100];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "P2P_CONNECT %s %s keypad",
		 addr.toLocal8Bit().constData(),
		 arg.toLocal8Bit().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to initiate P2P connect.");
		msg.exec();
	}
}


void Peers::ctx_p2p_remove_group()
{
	if (ctx_item == NULL)
		return;
	char cmd[100];
	char reply[100];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "P2P_GROUP_REMOVE %s",
		 ctx_item->data(peer_role_ifname).toString().toLocal8Bit().
		 constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to remove P2P Group.");
		msg.exec();
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

	var = ctx_item->data(peer_role_selected_method);
	if (var.isValid()) {
		enum selected_method method =
			(enum selected_method) var.toInt();
		switch (method) {
		case SEL_METHOD_NONE:
			break;
		case SEL_METHOD_PIN_PEER_DISPLAY:
			info += tr("Selected Method: PIN on peer display\n");
			break;
		case SEL_METHOD_PIN_LOCAL_DISPLAY:
			info += tr("Selected Method: PIN on local display\n");
			break;
		}
	}

	var = ctx_item->data(peer_role_selected_pin);
	if (var.isValid()) {
		info += tr("PIN to enter on peer: ") + var.toString() + "\n";
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
			 ctx_item->data(peer_role_uuid).toString().toLocal8Bit().
			 constData());
	} else if (peer_type == PEER_TYPE_P2P ||
		   peer_type == PEER_TYPE_P2P_CLIENT) {
		snprintf(cmd, sizeof(cmd), "P2P_CONNECT %s pbc",
			 ctx_item->data(peer_role_address).toString().
			 toLocal8Bit().constData());
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
		 uuid.toLocal8Bit().constData(),
		 input.get_string().toLocal8Bit().constData());
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to start learning AP configuration."));
		msg.exec();
	}
}


void Peers::ctx_hide_ap()
{
	hide_ap = true;

	if (model.rowCount() == 0)
		return;

	do {
		QModelIndexList lst;
		lst = model.match(model.index(0, 0),
				  peer_role_type, PEER_TYPE_AP);
		if (lst.size() == 0) {
			lst = model.match(model.index(0, 0),
					  peer_role_type, PEER_TYPE_AP_WPS);
			if (lst.size() == 0)
				break;
		}

		model.removeRow(lst[0].row());
	} while (1);
}


void Peers::ctx_show_ap()
{
	hide_ap = false;
	add_scan_results();
}


void Peers::ctx_p2p_show_passphrase()
{
	char reply[64];
	size_t reply_len;

	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest("P2P_GET_PASSPHRASE", reply, &reply_len) < 0 ||
	    memcmp(reply, "FAIL", 4) == 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText("Failed to get P2P group passphrase.");
		msg.exec();
	} else {
		reply[reply_len] = '\0';
		QMessageBox::information(this, tr("Passphrase"),
					 tr("P2P group passphrase:\n") +
					 reply);
	}
}


void Peers::ctx_p2p_start_persistent()
{
	if (ctx_item == NULL)
		return;

	char cmd[100];
	char reply[100];
	size_t reply_len;

	snprintf(cmd, sizeof(cmd), "P2P_GROUP_ADD persistent=%d",
		 ctx_item->data(peer_role_network_id).toInt());
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0 ||
	    memcmp(reply, "FAIL", 4) == 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to start persistent P2P Group."));
		msg.exec();
	} else if (ctx_item->data(peer_role_type).toInt() ==
		   PEER_TYPE_P2P_INVITATION)
		model.removeRow(ctx_item->row());
}


void Peers::ctx_p2p_invite()
{
	if (ctx_item == NULL)
		return;

	char cmd[100];
	char reply[100];
	size_t reply_len;

	snprintf(cmd, sizeof(cmd), "P2P_INVITE persistent=%d",
		 ctx_item->data(peer_role_network_id).toInt());
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) < 0 ||
	    memcmp(reply, "FAIL", 4) == 0) {
		QMessageBox msg;
		msg.setIcon(QMessageBox::Warning);
		msg.setText(tr("Failed to invite peer to start persistent "
			       "P2P Group."));
		msg.exec();
	}
}


void Peers::ctx_p2p_delete()
{
	if (ctx_item == NULL)
		return;
	model.removeRow(ctx_item->row());
}


void Peers::enable_persistent(int id)
{
	if (model.rowCount() == 0)
		return;

	QModelIndexList lst = model.match(model.index(0, 0),
					  peer_role_network_id, id);
	for (int i = 0; i < lst.size(); i++) {
		QStandardItem *item = model.itemFromIndex(lst[i]);
		int type = item->data(peer_role_type).toInt();
		if (type == PEER_TYPE_P2P_PERSISTENT_GROUP_GO ||
		    type == PEER_TYPE_P2P_PERSISTENT_GROUP_CLIENT)
			item->setBackground(Qt::NoBrush);
	}
}
