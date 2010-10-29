/*
 * wpa_gui - NetworkConfig class
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
#include <QMessageBox>

#include "networkconfig.h"
#include "wpagui.h"

enum {
	AUTH_NONE_OPEN,
	AUTH_NONE_WEP,
	AUTH_NONE_WEP_SHARED,
	AUTH_IEEE8021X,
	AUTH_WPA_PSK,
	AUTH_WPA_EAP,
	AUTH_WPA2_PSK,
	AUTH_WPA2_EAP
};

#define WPA_GUI_KEY_DATA "[key is configured]"


NetworkConfig::NetworkConfig(QWidget *parent, const char *, bool, Qt::WFlags)
	: QDialog(parent)
{
	setupUi(this);

	encrSelect->setEnabled(false);
	connect(authSelect, SIGNAL(activated(int)), this,
		SLOT(authChanged(int)));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(addButton, SIGNAL(clicked()), this, SLOT(addNetwork()));
	connect(encrSelect, SIGNAL(activated(const QString &)), this,
		SLOT(encrChanged(const QString &)));
	connect(removeButton, SIGNAL(clicked()), this, SLOT(removeNetwork()));
	connect(eapSelect, SIGNAL(activated(int)), this,
		SLOT(eapChanged(int)));
	connect(useWpsButton, SIGNAL(clicked()), this, SLOT(useWps()));

	wpagui = NULL;
	new_network = false;
}


NetworkConfig::~NetworkConfig()
{
}


void NetworkConfig::languageChange()
{
	retranslateUi(this);
}


void NetworkConfig::paramsFromScanResults(QTreeWidgetItem *sel)
{
	new_network = true;

	/* SSID BSSID frequency signal flags */
	setWindowTitle(sel->text(0));
	ssidEdit->setText(sel->text(0));

	QString flags = sel->text(4);
	int auth, encr = 0;
	if (flags.indexOf("[WPA2-EAP") >= 0)
		auth = AUTH_WPA2_EAP;
	else if (flags.indexOf("[WPA-EAP") >= 0)
		auth = AUTH_WPA_EAP;
	else if (flags.indexOf("[WPA2-PSK") >= 0)
		auth = AUTH_WPA2_PSK;
	else if (flags.indexOf("[WPA-PSK") >= 0)
		auth = AUTH_WPA_PSK;
	else
		auth = AUTH_NONE_OPEN;

	if (flags.indexOf("-CCMP") >= 0)
		encr = 1;
	else if (flags.indexOf("-TKIP") >= 0)
		encr = 0;
	else if (flags.indexOf("WEP") >= 0) {
		encr = 1;
		if (auth == AUTH_NONE_OPEN)
			auth = AUTH_NONE_WEP;
	} else
		encr = 0;

	authSelect->setCurrentIndex(auth);
	authChanged(auth);
	encrSelect->setCurrentIndex(encr);

	wepEnabled(auth == AUTH_NONE_WEP);

	getEapCapa();

	if (flags.indexOf("[WPS") >= 0)
		useWpsButton->setEnabled(true);
	bssid = sel->text(1);
}


void NetworkConfig::authChanged(int sel)
{
	encrSelect->setEnabled(sel != AUTH_NONE_OPEN && sel != AUTH_NONE_WEP &&
			       sel != AUTH_NONE_WEP_SHARED);
	pskEdit->setEnabled(sel == AUTH_WPA_PSK || sel == AUTH_WPA2_PSK);
	bool eap = sel == AUTH_IEEE8021X || sel == AUTH_WPA_EAP ||
		sel == AUTH_WPA2_EAP;
	eapSelect->setEnabled(eap);
	identityEdit->setEnabled(eap);
	passwordEdit->setEnabled(eap);
	cacertEdit->setEnabled(eap);
	phase2Select->setEnabled(eap);
	if (eap)
		eapChanged(eapSelect->currentIndex());

	while (encrSelect->count())
		encrSelect->removeItem(0);

	if (sel == AUTH_NONE_OPEN || sel == AUTH_NONE_WEP ||
	    sel == AUTH_NONE_WEP_SHARED || sel == AUTH_IEEE8021X) {
		encrSelect->addItem("None");
		encrSelect->addItem("WEP");
		encrSelect->setCurrentIndex(sel == AUTH_NONE_OPEN ? 0 : 1);
	} else {
		encrSelect->addItem("TKIP");
		encrSelect->addItem("CCMP");
		encrSelect->setCurrentIndex((sel == AUTH_WPA2_PSK ||
					     sel == AUTH_WPA2_EAP) ? 1 : 0);
	}

	wepEnabled(sel == AUTH_NONE_WEP || sel == AUTH_NONE_WEP_SHARED);
}


void NetworkConfig::eapChanged(int sel)
{
	QString prev_val = phase2Select->currentText();
	while (phase2Select->count())
		phase2Select->removeItem(0);

	QStringList inner;
	inner << "PEAP" << "TTLS" << "FAST";
	if (!inner.contains(eapSelect->itemText(sel)))
		return;

	phase2Select->addItem("[ any ]");

	/* Add special cases based on outer method */
	if (eapSelect->currentText().compare("TTLS") == 0) {
		phase2Select->addItem("PAP");
		phase2Select->addItem("CHAP");
		phase2Select->addItem("MSCHAP");
		phase2Select->addItem("MSCHAPv2");
	} else if (eapSelect->currentText().compare("FAST") == 0)
		phase2Select->addItem("GTC(auth) + MSCHAPv2(prov)");

	/* Add all enabled EAP methods that can be used in the tunnel */
	int i;
	QStringList allowed;
	allowed << "MSCHAPV2" << "MD5" << "GTC" << "TLS" << "OTP" << "SIM"
		<< "AKA";
	for (i = 0; i < eapSelect->count(); i++) {
		if (allowed.contains(eapSelect->itemText(i))) {
			phase2Select->addItem("EAP-" + eapSelect->itemText(i));
		}
	}

	for (i = 0; i < phase2Select->count(); i++) {
		if (phase2Select->itemText(i).compare(prev_val) == 0) {
			phase2Select->setCurrentIndex(i);
			break;
		}
	}
}


void NetworkConfig::addNetwork()
{
	char reply[10], cmd[256];
	size_t reply_len;
	int id;
	int psklen = pskEdit->text().length();
	int auth = authSelect->currentIndex();

	if (auth == AUTH_WPA_PSK || auth == AUTH_WPA2_PSK) {
		if (psklen < 8 || psklen > 64) {
			QMessageBox::warning(
				this,
				tr("WPA Pre-Shared Key Error"),
				tr("WPA-PSK requires a passphrase of 8 to 63 "
				   "characters\n"
				   "or 64 hex digit PSK"));
			pskEdit->setFocus();
			return;
		}
	}

	if (idstrEdit->isEnabled() && !idstrEdit->text().isEmpty()) {
		QRegExp rx("^(\\w|-)+$");
		if (rx.indexIn(idstrEdit->text()) < 0) {
			QMessageBox::warning(
				this, tr("Network ID Error"),
				tr("Network ID String contains non-word "
				   "characters.\n"
				   "It must be a simple string, "
				   "without spaces, containing\n"
				   "only characters in this range: "
				   "[A-Za-z0-9_-]\n"));
			idstrEdit->setFocus();
			return;
		}
	}

	if (wpagui == NULL)
		return;

	memset(reply, 0, sizeof(reply));
	reply_len = sizeof(reply) - 1;

	if (new_network) {
		wpagui->ctrlRequest("ADD_NETWORK", reply, &reply_len);
		if (reply[0] == 'F') {
			QMessageBox::warning(this, "wpa_gui",
					     tr("Failed to add "
						"network to wpa_supplicant\n"
						"configuration."));
			return;
		}
		id = atoi(reply);
	} else
		id = edit_network_id;

	setNetworkParam(id, "ssid", ssidEdit->text().toAscii().constData(),
			true);

	const char *key_mgmt = NULL, *proto = NULL, *pairwise = NULL;
	switch (auth) {
	case AUTH_NONE_OPEN:
	case AUTH_NONE_WEP:
	case AUTH_NONE_WEP_SHARED:
		key_mgmt = "NONE";
		break;
	case AUTH_IEEE8021X:
		key_mgmt = "IEEE8021X";
		break;
	case AUTH_WPA_PSK:
		key_mgmt = "WPA-PSK";
		proto = "WPA";
		break;
	case AUTH_WPA_EAP:
		key_mgmt = "WPA-EAP";
		proto = "WPA";
		break;
	case AUTH_WPA2_PSK:
		key_mgmt = "WPA-PSK";
		proto = "WPA2";
		break;
	case AUTH_WPA2_EAP:
		key_mgmt = "WPA-EAP";
		proto = "WPA2";
		break;
	}

	if (auth == AUTH_NONE_WEP_SHARED)
		setNetworkParam(id, "auth_alg", "SHARED", false);
	else
		setNetworkParam(id, "auth_alg", "OPEN", false);

	if (auth == AUTH_WPA_PSK || auth == AUTH_WPA_EAP ||
	    auth == AUTH_WPA2_PSK || auth == AUTH_WPA2_EAP) {
		int encr = encrSelect->currentIndex();
		if (encr == 0)
			pairwise = "TKIP";
		else
			pairwise = "CCMP";
	}

	if (proto)
		setNetworkParam(id, "proto", proto, false);
	if (key_mgmt)
		setNetworkParam(id, "key_mgmt", key_mgmt, false);
	if (pairwise) {
		setNetworkParam(id, "pairwise", pairwise, false);
		setNetworkParam(id, "group", "TKIP CCMP WEP104 WEP40", false);
	}
	if (pskEdit->isEnabled() &&
	    strcmp(pskEdit->text().toAscii().constData(),
		   WPA_GUI_KEY_DATA) != 0)
		setNetworkParam(id, "psk",
				pskEdit->text().toAscii().constData(),
				psklen != 64);
	if (eapSelect->isEnabled()) {
		const char *eap =
			eapSelect->currentText().toAscii().constData();
		setNetworkParam(id, "eap", eap, false);
		if (strcmp(eap, "SIM") == 0 || strcmp(eap, "AKA") == 0)
			setNetworkParam(id, "pcsc", "", true);
		else
			setNetworkParam(id, "pcsc", "NULL", false);
	}
	if (phase2Select->isEnabled()) {
		QString eap = eapSelect->currentText();
		QString inner = phase2Select->currentText();
		char phase2[32];
		phase2[0] = '\0';
		if (eap.compare("PEAP") == 0) {
			if (inner.startsWith("EAP-"))
				snprintf(phase2, sizeof(phase2), "auth=%s",
					 inner.right(inner.size() - 4).
					 toAscii().constData());
		} else if (eap.compare("TTLS") == 0) {
			if (inner.startsWith("EAP-"))
				snprintf(phase2, sizeof(phase2), "autheap=%s",
					 inner.right(inner.size() - 4).
					 toAscii().constData());
			else
				snprintf(phase2, sizeof(phase2), "auth=%s",
					 inner.toAscii().constData());
		} else if (eap.compare("FAST") == 0) {
			const char *provisioning = NULL;
			if (inner.startsWith("EAP-")) {
				snprintf(phase2, sizeof(phase2), "auth=%s",
					 inner.right(inner.size() - 4).
					 toAscii().constData());
				provisioning = "fast_provisioning=2";
			} else if (inner.compare("GTC(auth) + MSCHAPv2(prov)")
				   == 0) {
				snprintf(phase2, sizeof(phase2),
					 "auth=GTC auth=MSCHAPV2");
				provisioning = "fast_provisioning=1";
			} else
				provisioning = "fast_provisioning=3";
			if (provisioning) {
				char blob[32];
				setNetworkParam(id, "phase1", provisioning,
						true);
				snprintf(blob, sizeof(blob),
					 "blob://fast-pac-%d", id);
				setNetworkParam(id, "pac_file", blob, true);
			}
		}
		if (phase2[0])
			setNetworkParam(id, "phase2", phase2, true);
		else
			setNetworkParam(id, "phase2", "NULL", false);
	} else
		setNetworkParam(id, "phase2", "NULL", false);
	if (identityEdit->isEnabled() && identityEdit->text().length() > 0)
		setNetworkParam(id, "identity",
				identityEdit->text().toAscii().constData(),
				true);
	else
		setNetworkParam(id, "identity", "NULL", false);
	if (passwordEdit->isEnabled() && passwordEdit->text().length() > 0 &&
	    strcmp(passwordEdit->text().toAscii().constData(),
		   WPA_GUI_KEY_DATA) != 0)
		setNetworkParam(id, "password",
				passwordEdit->text().toAscii().constData(),
				true);
	else if (passwordEdit->text().length() == 0)
		setNetworkParam(id, "password", "NULL", false);
	if (cacertEdit->isEnabled() && cacertEdit->text().length() > 0)
		setNetworkParam(id, "ca_cert",
				cacertEdit->text().toAscii().constData(),
				true);
	else
		setNetworkParam(id, "ca_cert", "NULL", false);
	writeWepKey(id, wep0Edit, 0);
	writeWepKey(id, wep1Edit, 1);
	writeWepKey(id, wep2Edit, 2);
	writeWepKey(id, wep3Edit, 3);

	if (wep0Radio->isEnabled() && wep0Radio->isChecked())
		setNetworkParam(id, "wep_tx_keyidx", "0", false);
	else if (wep1Radio->isEnabled() && wep1Radio->isChecked())
		setNetworkParam(id, "wep_tx_keyidx", "1", false);
	else if (wep2Radio->isEnabled() && wep2Radio->isChecked())
		setNetworkParam(id, "wep_tx_keyidx", "2", false);
	else if (wep3Radio->isEnabled() && wep3Radio->isChecked())
		setNetworkParam(id, "wep_tx_keyidx", "3", false);

	if (idstrEdit->isEnabled() && idstrEdit->text().length() > 0)
		setNetworkParam(id, "id_str",
				idstrEdit->text().toAscii().constData(),
				true);
	else
		setNetworkParam(id, "id_str", "NULL", false);

	if (prioritySpinBox->isEnabled()) {
		QString prio;
		prio = prio.setNum(prioritySpinBox->value());
		setNetworkParam(id, "priority", prio.toAscii().constData(),
				false);
	}

	snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %d", id);
	reply_len = sizeof(reply);
	wpagui->ctrlRequest(cmd, reply, &reply_len);
	if (strncmp(reply, "OK", 2) != 0) {
		QMessageBox::warning(this, "wpa_gui",
				     tr("Failed to enable "
					"network in wpa_supplicant\n"
					"configuration."));
		/* Network was added, so continue anyway */
	}
	wpagui->triggerUpdate();
	wpagui->ctrlRequest("SAVE_CONFIG", reply, &reply_len);

	close();
}


void NetworkConfig::setWpaGui(WpaGui *_wpagui)
{
	wpagui = _wpagui;
}


int NetworkConfig::setNetworkParam(int id, const char *field,
				   const char *value, bool quote)
{
	char reply[10], cmd[256];
	size_t reply_len;
	snprintf(cmd, sizeof(cmd), "SET_NETWORK %d %s %s%s%s",
		 id, field, quote ? "\"" : "", value, quote ? "\"" : "");
	reply_len = sizeof(reply);
	wpagui->ctrlRequest(cmd, reply, &reply_len);
	return strncmp(reply, "OK", 2) == 0 ? 0 : -1;
}


void NetworkConfig::encrChanged(const QString &)
{
}


void NetworkConfig::wepEnabled(bool enabled)
{
	wep0Edit->setEnabled(enabled);
	wep1Edit->setEnabled(enabled);
	wep2Edit->setEnabled(enabled);
	wep3Edit->setEnabled(enabled);
	wep0Radio->setEnabled(enabled);
	wep1Radio->setEnabled(enabled);
	wep2Radio->setEnabled(enabled);
	wep3Radio->setEnabled(enabled);
}


void NetworkConfig::writeWepKey(int network_id, QLineEdit *edit, int id)
{
	char buf[10];
	bool hex;
	const char *txt, *pos;
	size_t len;

	if (!edit->isEnabled() || edit->text().isEmpty())
		return;

	/*
	 * Assume hex key if only hex characters are present and length matches
	 * with 40, 104, or 128-bit key
	 */
	txt = edit->text().toAscii().constData();
	if (strcmp(txt, WPA_GUI_KEY_DATA) == 0)
		return;
	len = strlen(txt);
	if (len == 0)
		return;
	pos = txt;
	hex = true;
	while (*pos) {
		if (!((*pos >= '0' && *pos <= '9') ||
		      (*pos >= 'a' && *pos <= 'f') ||
		      (*pos >= 'A' && *pos <= 'F'))) {
			hex = false;
			break;
		}
		pos++;
	}
	if (hex && len != 10 && len != 26 && len != 32)
		hex = false;
	snprintf(buf, sizeof(buf), "wep_key%d", id);
	setNetworkParam(network_id, buf, txt, !hex);
}


static int key_value_isset(const char *reply, size_t reply_len)
{
    return reply_len > 0 && (reply_len < 4 || memcmp(reply, "FAIL", 4) != 0);
}


void NetworkConfig::paramsFromConfig(int network_id)
{
	int i, res;

	edit_network_id = network_id;
	getEapCapa();

	char reply[1024], cmd[256], *pos;
	size_t reply_len;

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d ssid", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 &&
	    reply_len >= 2 && reply[0] == '"') {
		reply[reply_len] = '\0';
		pos = strchr(reply + 1, '"');
		if (pos)
			*pos = '\0';
		ssidEdit->setText(reply + 1);
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d proto", network_id);
	reply_len = sizeof(reply) - 1;
	int wpa = 0;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0) {
		reply[reply_len] = '\0';
		if (strstr(reply, "RSN") || strstr(reply, "WPA2"))
			wpa = 2;
		else if (strstr(reply, "WPA"))
			wpa = 1;
	}

	int auth = AUTH_NONE_OPEN, encr = 0;
	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d key_mgmt", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0) {
		reply[reply_len] = '\0';
		if (strstr(reply, "WPA-EAP"))
			auth = wpa & 2 ? AUTH_WPA2_EAP : AUTH_WPA_EAP;
		else if (strstr(reply, "WPA-PSK"))
			auth = wpa & 2 ? AUTH_WPA2_PSK : AUTH_WPA_PSK;
		else if (strstr(reply, "IEEE8021X")) {
			auth = AUTH_IEEE8021X;
			encr = 1;
		}
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d pairwise", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0) {
		reply[reply_len] = '\0';
		if (strstr(reply, "CCMP") && auth != AUTH_NONE_OPEN &&
		    auth != AUTH_NONE_WEP && auth != AUTH_NONE_WEP_SHARED)
			encr = 1;
		else if (strstr(reply, "TKIP"))
			encr = 0;
		else if (strstr(reply, "WEP"))
			encr = 1;
		else
			encr = 0;
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d psk", network_id);
	reply_len = sizeof(reply) - 1;
	res = wpagui->ctrlRequest(cmd, reply, &reply_len);
	if (res >= 0 && reply_len >= 2 && reply[0] == '"') {
		reply[reply_len] = '\0';
		pos = strchr(reply + 1, '"');
		if (pos)
			*pos = '\0';
		pskEdit->setText(reply + 1);
	} else if (res >= 0 && key_value_isset(reply, reply_len)) {
		pskEdit->setText(WPA_GUI_KEY_DATA);
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d identity", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 &&
	    reply_len >= 2 && reply[0] == '"') {
		reply[reply_len] = '\0';
		pos = strchr(reply + 1, '"');
		if (pos)
			*pos = '\0';
		identityEdit->setText(reply + 1);
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d password", network_id);
	reply_len = sizeof(reply) - 1;
	res = wpagui->ctrlRequest(cmd, reply, &reply_len);
	if (res >= 0 && reply_len >= 2 && reply[0] == '"') {
		reply[reply_len] = '\0';
		pos = strchr(reply + 1, '"');
		if (pos)
			*pos = '\0';
		passwordEdit->setText(reply + 1);
	} else if (res >= 0 && key_value_isset(reply, reply_len)) {
		passwordEdit->setText(WPA_GUI_KEY_DATA);
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d ca_cert", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 &&
	    reply_len >= 2 && reply[0] == '"') {
		reply[reply_len] = '\0';
		pos = strchr(reply + 1, '"');
		if (pos)
			*pos = '\0';
		cacertEdit->setText(reply + 1);
	}

	enum { NO_INNER, PEAP_INNER, TTLS_INNER, FAST_INNER } eap = NO_INNER;
	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d eap", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 &&
	    reply_len >= 1) {
		reply[reply_len] = '\0';
		for (i = 0; i < eapSelect->count(); i++) {
			if (eapSelect->itemText(i).compare(reply) == 0) {
				eapSelect->setCurrentIndex(i);
				if (strcmp(reply, "PEAP") == 0)
					eap = PEAP_INNER;
				else if (strcmp(reply, "TTLS") == 0)
					eap = TTLS_INNER;
				else if (strcmp(reply, "FAST") == 0)
					eap = FAST_INNER;
				break;
			}
		}
	}

	if (eap != NO_INNER) {
		snprintf(cmd, sizeof(cmd), "GET_NETWORK %d phase2",
			 network_id);
		reply_len = sizeof(reply) - 1;
		if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 &&
		    reply_len >= 1) {
			reply[reply_len] = '\0';
			eapChanged(eapSelect->currentIndex());
		} else
			eap = NO_INNER;
	}

	char *val;
	val = reply + 1;
	while (*(val + 1))
		val++;
	if (*val == '"')
		*val = '\0';

	switch (eap) {
	case PEAP_INNER:
		if (strncmp(reply, "\"auth=", 6))
			break;
		val = reply + 2;
		memcpy(val, "EAP-", 4);
		break;
	case TTLS_INNER:
		if (strncmp(reply, "\"autheap=", 9) == 0) {
			val = reply + 5;
			memcpy(val, "EAP-", 4);
		} else if (strncmp(reply, "\"auth=", 6) == 0)
			val = reply + 6;
		break;
	case FAST_INNER:
		if (strncmp(reply, "\"auth=", 6))
			break;
		if (strcmp(reply + 6, "GTC auth=MSCHAPV2") == 0) {
			val = (char *) "GTC(auth) + MSCHAPv2(prov)";
			break;
		}
		val = reply + 2;
		memcpy(val, "EAP-", 4);
		break;
	case NO_INNER:
		break;
	}

	for (i = 0; i < phase2Select->count(); i++) {
		if (phase2Select->itemText(i).compare(val) == 0) {
			phase2Select->setCurrentIndex(i);
			break;
		}
	}

	for (i = 0; i < 4; i++) {
		QLineEdit *wepEdit;
		switch (i) {
		default:
		case 0:
			wepEdit = wep0Edit;
			break;
		case 1:
			wepEdit = wep1Edit;
			break;
		case 2:
			wepEdit = wep2Edit;
			break;
		case 3:
			wepEdit = wep3Edit;
			break;
		}
		snprintf(cmd, sizeof(cmd), "GET_NETWORK %d wep_key%d",
			 network_id, i);
		reply_len = sizeof(reply) - 1;
		res = wpagui->ctrlRequest(cmd, reply, &reply_len);
		if (res >= 0 && reply_len >= 2 && reply[0] == '"') {
			reply[reply_len] = '\0';
			pos = strchr(reply + 1, '"');
			if (pos)
				*pos = '\0';
			if (auth == AUTH_NONE_OPEN || auth == AUTH_IEEE8021X) {
				if (auth == AUTH_NONE_OPEN)
					auth = AUTH_NONE_WEP;
				encr = 1;
			}

			wepEdit->setText(reply + 1);
		} else if (res >= 0 && key_value_isset(reply, reply_len)) {
			if (auth == AUTH_NONE_OPEN || auth == AUTH_IEEE8021X) {
				if (auth == AUTH_NONE_OPEN)
					auth = AUTH_NONE_WEP;
				encr = 1;
			}
			wepEdit->setText(WPA_GUI_KEY_DATA);
		}
	}

	if (auth == AUTH_NONE_WEP) {
		snprintf(cmd, sizeof(cmd), "GET_NETWORK %d auth_alg",
			 network_id);
		reply_len = sizeof(reply) - 1;
		if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0) {
			reply[reply_len] = '\0';
			if (strcmp(reply, "SHARED") == 0)
				auth = AUTH_NONE_WEP_SHARED;
		}
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d wep_tx_keyidx", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 1)
	{
		reply[reply_len] = '\0';
		switch (atoi(reply)) {
		case 0:
			wep0Radio->setChecked(true);
			break;
		case 1:
			wep1Radio->setChecked(true);
			break;
		case 2:
			wep2Radio->setChecked(true);
			break;
		case 3:
			wep3Radio->setChecked(true);
			break;
		}
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d id_str", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 &&
	    reply_len >= 2 && reply[0] == '"') {
		reply[reply_len] = '\0';
		pos = strchr(reply + 1, '"');
		if (pos)
			*pos = '\0';
		idstrEdit->setText(reply + 1);
	}

	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d priority", network_id);
	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 1)
	{
		reply[reply_len] = '\0';
		prioritySpinBox->setValue(atoi(reply));
	}

	authSelect->setCurrentIndex(auth);
	authChanged(auth);
	encrSelect->setCurrentIndex(encr);
	wepEnabled(auth == AUTH_NONE_WEP || auth == AUTH_NONE_WEP_SHARED);

	removeButton->setEnabled(true);
	addButton->setText("Save");
}


void NetworkConfig::removeNetwork()
{
	char reply[10], cmd[256];
	size_t reply_len;

	if (QMessageBox::information(
		    this, "wpa_gui",
		    tr("This will permanently remove the network\n"
		       "from the configuration. Do you really want\n"
		       "to remove this network?"),
		    tr("Yes"), tr("No")) != 0)
		return;

	snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", edit_network_id);
	reply_len = sizeof(reply);
	wpagui->ctrlRequest(cmd, reply, &reply_len);
	if (strncmp(reply, "OK", 2) != 0) {
		QMessageBox::warning(this, "wpa_gui",
				     tr("Failed to remove network from "
					"wpa_supplicant\n"
					"configuration."));
	} else {
		wpagui->triggerUpdate();
		wpagui->ctrlRequest("SAVE_CONFIG", reply, &reply_len);
	}

	close();
}


void NetworkConfig::newNetwork()
{
	new_network = true;
	getEapCapa();
}


void NetworkConfig::getEapCapa()
{
	char reply[256];
	size_t reply_len;

	if (wpagui == NULL)
		return;

	reply_len = sizeof(reply) - 1;
	if (wpagui->ctrlRequest("GET_CAPABILITY eap", reply, &reply_len) < 0)
		return;
	reply[reply_len] = '\0';

	QString res(reply);
	QStringList types = res.split(QChar(' '));
	eapSelect->insertItems(-1, types);
}


void NetworkConfig::useWps()
{
	if (wpagui == NULL)
		return;
	wpagui->setBssFromScan(bssid);
	wpagui->wpsDialog();
	close();
}
