/*
 * wpa_gui - Peers class
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PEERS_H
#define PEERS_H

#include <QObject>
#include <QStandardItemModel>
#include "wpamsg.h"
#include "ui_peers.h"

class WpaGui;

class Peers : public QDialog, public Ui::Peers
{
	Q_OBJECT

public:
	Peers(QWidget *parent = 0, const char *name = 0,
		    bool modal = false, Qt::WindowFlags fl = 0);
	~Peers();
	void setWpaGui(WpaGui *_wpagui);
	void event_notify(WpaMsg msg);

public slots:
	virtual void context_menu(const QPoint &pos);
	virtual void enter_pin();
	virtual void connect_pbc();
	virtual void learn_ap_config();
	virtual void ctx_refresh();
	virtual void ctx_p2p_start();
	virtual void ctx_p2p_stop();
	virtual void ctx_p2p_listen();
	virtual void ctx_p2p_start_group();
	virtual void ctx_p2p_remove_group();
	virtual void ctx_p2p_connect();
	virtual void ctx_p2p_req_pin();
	virtual void ctx_p2p_show_pin();
	virtual void ctx_p2p_display_pin();
	virtual void ctx_p2p_display_pin_pd();
	virtual void ctx_p2p_enter_pin();
	virtual void properties();
	virtual void ctx_hide_ap();
	virtual void ctx_show_ap();
	virtual void ctx_p2p_show_passphrase();
	virtual void ctx_p2p_start_persistent();
	virtual void ctx_p2p_invite();
	virtual void ctx_p2p_delete();

protected slots:
	virtual void languageChange();
	virtual void closeEvent(QCloseEvent *event);

private:
	void add_station(QString info);
	void add_stations();
	void add_single_station(const char *addr);
	bool add_bss(const char *cmd);
	void remove_bss(int id);
	void add_scan_results();
	void add_persistent(int id, const char *ssid, const char *bssid);
	void add_persistent_groups();
	void update_peers();
	QStandardItem * find_addr(QString addr);
	QStandardItem * find_addr_type(QString addr, int type);
	void add_p2p_group_client(QStandardItem *parent, QString params);
	QStandardItem * find_uuid(QString uuid);
	void done(int r);
	void remove_enrollee_uuid(QString uuid);
	QString ItemType(int type);
	void enable_persistent(int id);

	WpaGui *wpagui;
	QStandardItemModel model;
	QIcon *default_icon;
	QIcon *ap_icon;
	QIcon *laptop_icon;
	QIcon *group_icon;
	QIcon *invitation_icon;
	QStandardItem *ctx_item;

	bool hide_ap;
};

#endif /* PEERS_H */
