/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/


enum {
    AUTH_NONE = 0,
    AUTH_IEEE8021X = 1,
    AUTH_WPA_PSK = 2,
    AUTH_WPA_EAP = 3,
    AUTH_WPA2_PSK = 4,
    AUTH_WPA2_EAP = 5
};

void NetworkConfig::init()
{
    wpagui = NULL;
    new_network = false;
}

void NetworkConfig::paramsFromScanResults(Q3ListViewItem *sel)
{
    new_network = true;

    /* SSID BSSID frequency signal flags */
    setCaption(sel->text(0));
    ssidEdit->setText(sel->text(0));
    
    QString flags = sel->text(4);
    int auth, encr = 0;
    if (flags.find("[WPA2-EAP") >= 0)
	auth = AUTH_WPA2_EAP;
    else if (flags.find("[WPA-EAP") >= 0)
	auth = AUTH_WPA_EAP;
    else if (flags.find("[WPA2-PSK") >= 0)
	auth = AUTH_WPA2_PSK;
    else if (flags.find("[WPA-PSK") >= 0)
	auth = AUTH_WPA_PSK;
    else
	auth = AUTH_NONE;
    
    if (flags.find("-CCMP") >= 0)
	encr = 1;
    else if (flags.find("-TKIP") >= 0)
	encr = 0;
    else if (flags.find("WEP") >= 0)
	encr = 1;
    else
	encr = 0;
 
    authSelect->setCurrentItem(auth);
    authChanged(auth);
    encrSelect->setCurrentItem(encr);

    getEapCapa();
}


void NetworkConfig::authChanged(int sel)
{
    pskEdit->setEnabled(sel == AUTH_WPA_PSK || sel == AUTH_WPA2_PSK);
    bool eap = sel == AUTH_IEEE8021X || sel == AUTH_WPA_EAP ||
	       sel == AUTH_WPA2_EAP;
    eapSelect->setEnabled(eap);
    identityEdit->setEnabled(eap);
    passwordEdit->setEnabled(eap);
    cacertEdit->setEnabled(eap);
   
    while (encrSelect->count())
	encrSelect->removeItem(0);
    
    if (sel == AUTH_NONE || sel == AUTH_IEEE8021X) {
	encrSelect->insertItem("None");
	encrSelect->insertItem("WEP");
	encrSelect->setCurrentItem(sel == AUTH_NONE ? 0 : 1);
    } else {
	encrSelect->insertItem("TKIP");
	encrSelect->insertItem("CCMP");
	encrSelect->setCurrentItem((sel == AUTH_WPA2_PSK ||
				    sel == AUTH_WPA2_EAP) ? 1 : 0);
    }
    
    wepEnabled(sel == AUTH_IEEE8021X);
}


void NetworkConfig::addNetwork()
{
    char reply[10], cmd[256];
    size_t reply_len;
    int id;
    int psklen = pskEdit->text().length();
    int auth = authSelect->currentItem();

    if (auth == AUTH_WPA_PSK || auth == AUTH_WPA2_PSK) {
	if (psklen < 8 || psklen > 64) {
	    QMessageBox::warning(this, "wpa_gui", "WPA-PSK requires a passphrase "
				 "of 8 to 63 characters\n"
				 "or 64 hex digit PSK");
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
	    QMessageBox::warning(this, "wpa_gui", "Failed to add network to wpa_supplicant\n"
				 "configuration.");
	    return;
	}
	id = atoi(reply);
    } else {
	id = edit_network_id;
    }

    setNetworkParam(id, "ssid", ssidEdit->text().ascii(), true);
    
    char *key_mgmt = NULL, *proto = NULL, *pairwise = NULL;
    switch (auth) {
    case AUTH_NONE:
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
    
    if (auth == AUTH_WPA_PSK || auth == AUTH_WPA_EAP ||
	auth == AUTH_WPA2_PSK || auth == AUTH_WPA2_EAP) {
	int encr = encrSelect->currentItem();
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
    if (pskEdit->isEnabled())
	setNetworkParam(id, "psk", pskEdit->text().ascii(), psklen != 64);
    if (eapSelect->isEnabled())
	setNetworkParam(id, "eap", eapSelect->currentText().ascii(), false);
    if (identityEdit->isEnabled())
	setNetworkParam(id, "identity", identityEdit->text().ascii(), true);
    if (passwordEdit->isEnabled())
	setNetworkParam(id, "password", passwordEdit->text().ascii(), true);
    if (cacertEdit->isEnabled())
	setNetworkParam(id, "ca_cert", cacertEdit->text().ascii(), true);
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

    snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %d", id);
    reply_len = sizeof(reply);
    wpagui->ctrlRequest(cmd, reply, &reply_len);
    if (strncmp(reply, "OK", 2) != 0) {
	QMessageBox::warning(this, "wpa_gui", "Failed to enable network in wpa_supplicant\n"
			     "configuration.");
	/* Network was added, so continue anyway */
    }
    wpagui->triggerUpdate();
    wpagui->ctrlRequest("SAVE_CONFIG", reply, &reply_len);

    close();
}


void NetworkConfig::setWpaGui( WpaGui *_wpagui )
{
    wpagui = _wpagui;
}


int NetworkConfig::setNetworkParam(int id, const char *field, const char *value, bool quote)
{
    char reply[10], cmd[256];
    size_t reply_len;
    snprintf(cmd, sizeof(cmd), "SET_NETWORK %d %s %s%s%s",
	     id, field, quote ? "\"" : "", value, quote ? "\"" : "");
    reply_len = sizeof(reply);
    wpagui->ctrlRequest(cmd, reply, &reply_len);
    return strncmp(reply, "OK", 2) == 0 ? 0 : -1;
}


void NetworkConfig::encrChanged( const QString &sel )
{
    wepEnabled(sel.find("WEP") == 0);
}


void NetworkConfig::wepEnabled( bool enabled )
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


void NetworkConfig::writeWepKey( int network_id, QLineEdit *edit, int id )
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
    txt = edit->text().ascii();
    len = strlen(txt);
    if (len == 0)
	return;
    pos = txt;
    hex = true;
    while (*pos) {
	if (!((*pos >= '0' && *pos <= '9') || (*pos >= 'a' && *pos <= 'f') ||
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


void NetworkConfig::paramsFromConfig( int network_id )
{
    int i;

    edit_network_id = network_id;
    getEapCapa();
    
    char reply[1024], cmd[256], *pos;
    size_t reply_len;
    
    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d ssid", network_id);
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 2 &&
	reply[0] == '"') {
	reply[reply_len] = '\0';
	pos = strchr(reply + 1, '"');
	if (pos)
	    *pos = '\0';
	ssidEdit->setText(reply + 1);
    }
    
    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d proto", network_id);
    reply_len = sizeof(reply);
    int wpa = 0;
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0) {
	reply[reply_len] = '\0';
	if (strstr(reply, "RSN") || strstr(reply, "WPA2"))
	    wpa = 2;
	else if (strstr(reply, "WPA"))
	    wpa = 1;
    }

    int auth = AUTH_NONE, encr = 0;
    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d key_mgmt", network_id);
    reply_len = sizeof(reply);
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
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0) {
	reply[reply_len] = '\0';
	if (strstr(reply, "CCMP"))
	    encr = 1;
	else if (strstr(reply, "TKIP"))
	    encr = 0;
	else if (strstr(reply, "WEP"))
	    encr = 1;
	else
	    encr = 0;
    }

    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d psk", network_id);
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 2 &&
	reply[0] == '"') {
	reply[reply_len] = '\0';
	pos = strchr(reply + 1, '"');
	if (pos)
	    *pos = '\0';
	pskEdit->setText(reply + 1);
    }

    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d identity", network_id);
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 2 &&
	reply[0] == '"') {
	reply[reply_len] = '\0';
	pos = strchr(reply + 1, '"');
	if (pos)
	    *pos = '\0';
	identityEdit->setText(reply + 1);
    }

    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d password", network_id);
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 2 &&
	reply[0] == '"') {
	reply[reply_len] = '\0';
	pos = strchr(reply + 1, '"');
	if (pos)
	    *pos = '\0';
	passwordEdit->setText(reply + 1);
    }

    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d ca_cert", network_id);
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 2 &&
	reply[0] == '"') {
	reply[reply_len] = '\0';
	pos = strchr(reply + 1, '"');
	if (pos)
	    *pos = '\0';
	cacertEdit->setText(reply + 1);
    }

    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d eap", network_id);
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 1) {
	reply[reply_len] = '\0';
	for (i = 0; i < eapSelect->count(); i++) {
	    if (eapSelect->text(i).compare(reply) == 0) {
		eapSelect->setCurrentItem(i);
		break;
	    }
	}
    }

    for (i = 0; i < 4; i++) {
	snprintf(cmd, sizeof(cmd), "GET_NETWORK %d wep_key%d", network_id, i);
	reply_len = sizeof(reply);
	if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 2 &&
	    reply[0] == '"') {
	    reply[reply_len] = '\0';
	    pos = strchr(reply + 1, '"');
	    if (pos)
		*pos = '\0';
	    if (auth == AUTH_NONE || auth == AUTH_IEEE8021X)
		encr = 1;

	    switch (i) {
	    case 0:
		wep0Edit->setText(reply + 1);
		break;
	    case 1:
		wep1Edit->setText(reply + 1);
		break;
	    case 2:
		wep2Edit->setText(reply + 1);
		break;
	    case 3:
		wep3Edit->setText(reply + 1);
		break;
	    }
	}
    }

    snprintf(cmd, sizeof(cmd), "GET_NETWORK %d wep_tx_keyidx", network_id);
    reply_len = sizeof(reply);
    if (wpagui->ctrlRequest(cmd, reply, &reply_len) >= 0 && reply_len >= 1) {
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

    authSelect->setCurrentItem(auth);
    authChanged(auth);
    encrSelect->setCurrentItem(encr);
    if (auth == AUTH_NONE || auth == AUTH_IEEE8021X)
	wepEnabled(encr == 1);

    removeButton->setEnabled(true);
    addButton->setText("Save");
}


void NetworkConfig::removeNetwork()
{
    char reply[10], cmd[256];
    size_t reply_len;
    
    if (QMessageBox::information(this, "wpa_gui",
				 "This will permanently remove the network\n"
				 "from the configuration. Do you really want\n"
				 "to remove this network?", "Yes", "No") != 0)
	return;
    
    snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", edit_network_id);
    reply_len = sizeof(reply);
    wpagui->ctrlRequest(cmd, reply, &reply_len);
    if (strncmp(reply, "OK", 2) != 0) {
	QMessageBox::warning(this, "wpa_gui",
			     "Failed to remove network from wpa_supplicant\n"
			     "configuration.");
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
    QStringList types = QStringList::split(QChar(' '), res);
    eapSelect->insertStringList(types);
}
