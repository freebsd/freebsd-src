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


#ifdef __MINGW32__
/* Need to get getopt() */
#include <unistd.h>
#endif


void WpaGui::init()
{
    eh = NULL;
    scanres = NULL;
    udr = NULL;
    ctrl_iface = NULL;
    ctrl_conn = NULL;
    monitor_conn = NULL;
    msgNotifier = NULL;
    ctrl_iface_dir = strdup("/var/run/wpa_supplicant");
    
    parse_argv();

    textStatus->setText("connecting to wpa_supplicant");
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(ping()));
    timer->start(1000, FALSE);
    
    if (openCtrlConnection(ctrl_iface) < 0) {
	printf("Failed to open control connection to wpa_supplicant.\n");
    }
    
    updateStatus();
    networkMayHaveChanged = true;
    updateNetworks();
}


void WpaGui::destroy()
{
    delete msgNotifier;

    if (monitor_conn) {
	wpa_ctrl_detach(monitor_conn);
	wpa_ctrl_close(monitor_conn);
	monitor_conn = NULL;
    }
    if (ctrl_conn) {
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
    }
    
    if (eh) {
	eh->close();
	delete eh;
	eh = NULL;
    }
    
    if (scanres) {
	scanres->close();
	delete scanres;
	scanres = NULL;
    }
    
    if (udr) {
	udr->close();
	delete udr;
	udr = NULL;
    }
    
    free(ctrl_iface);
    ctrl_iface = NULL;
    
    free(ctrl_iface_dir);
    ctrl_iface_dir = NULL;
}


void WpaGui::parse_argv()
{
    int c;
    for (;;) {
	c = getopt(qApp->argc(), qApp->argv(), "i:p:");
	if (c < 0)
	    break;
	switch (c) {
	case 'i':
	    free(ctrl_iface);
	    ctrl_iface = strdup(optarg);
	    break;
	case 'p':
	    free(ctrl_iface_dir);
	    ctrl_iface_dir = strdup(optarg);
	    break;
	}
    }
}


int WpaGui::openCtrlConnection(const char *ifname)
{
    char *cfile;
    int flen;

    if (ifname) {
	if (ifname != ctrl_iface) {
	    free(ctrl_iface);
	    ctrl_iface = strdup(ifname);
	}
    } else {
#ifdef CONFIG_CTRL_IFACE_UDP
	free(ctrl_iface);
	ctrl_iface = strdup("udp");
#else /* CONFIG_CTRL_IFACE_UDP */
	struct dirent *dent;
	DIR *dir = opendir(ctrl_iface_dir);
	free(ctrl_iface);
	ctrl_iface = NULL;
	if (dir) {
	    while ((dent = readdir(dir))) {
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
		    continue;
		printf("Selected interface '%s'\n", dent->d_name);
		ctrl_iface = strdup(dent->d_name);
		break;
	    }
	    closedir(dir);
	}
#endif /* CONFIG_CTRL_IFACE_UDP */
    }
    
    if (ctrl_iface == NULL)
	return -1;

    flen = strlen(ctrl_iface_dir) + strlen(ctrl_iface) + 2;
    cfile = (char *) malloc(flen);
    if (cfile == NULL)
	return -1;
    snprintf(cfile, flen, "%s/%s", ctrl_iface_dir, ctrl_iface);

    if (ctrl_conn) {
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
    }

    if (monitor_conn) {
	delete msgNotifier;
	msgNotifier = NULL;
	wpa_ctrl_detach(monitor_conn);
	wpa_ctrl_close(monitor_conn);
	monitor_conn = NULL;
    }

    printf("Trying to connect to '%s'\n", cfile);
    ctrl_conn = wpa_ctrl_open(cfile);
    if (ctrl_conn == NULL) {
	free(cfile);
	return -1;
    }
    monitor_conn = wpa_ctrl_open(cfile);
    free(cfile);
    if (monitor_conn == NULL) {
	wpa_ctrl_close(ctrl_conn);
	return -1;
    }
    if (wpa_ctrl_attach(monitor_conn)) {
	printf("Failed to attach to wpa_supplicant\n");
	wpa_ctrl_close(monitor_conn);
	monitor_conn = NULL;
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
	return -1;
    }

    msgNotifier = new QSocketNotifier(wpa_ctrl_get_fd(monitor_conn),
				      QSocketNotifier::Read, this);
    connect(msgNotifier, SIGNAL(activated(int)), SLOT(receiveMsgs()));

    adapterSelect->clear();
    adapterSelect->insertItem(ctrl_iface);
    adapterSelect->setCurrentItem(0);

    return 0;
}


static void wpa_gui_msg_cb(char *msg, size_t)
{
    /* This should not happen anymore since two control connections are used. */
    printf("missed message: %s\n", msg);
}


int WpaGui::ctrlRequest(const char *cmd, char *buf, size_t *buflen)
{
    int ret;
    
    if (ctrl_conn == NULL)
	return -3;
    ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), buf, buflen,
			   wpa_gui_msg_cb);
    if (ret == -2) {
	printf("'%s' command timed out.\n", cmd);
    } else if (ret < 0) {
	printf("'%s' command failed.\n", cmd);
    }
    
    return ret;
}


void WpaGui::updateStatus()
{
    char buf[2048], *start, *end, *pos;
    size_t len;

    pingsToStatusUpdate = 10;

    len = sizeof(buf) - 1;
    if (ctrl_conn == NULL || ctrlRequest("STATUS", buf, &len) < 0) {
	textStatus->setText("Could not get status from wpa_supplicant");
	textAuthentication->clear();
	textEncryption->clear();
	textSsid->clear();
	textBssid->clear();
	textIpAddress->clear();
	return;
    }
    
    buf[len] = '\0';
    
    bool auth_updated = false, ssid_updated = false;
    bool bssid_updated = false, ipaddr_updated = false;
    bool status_updated = false;
    char *pairwise_cipher = NULL, *group_cipher = NULL;
    
    start = buf;
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
	
	pos = strchr(start, '=');
	if (pos) {
	    *pos++ = '\0';
	    if (strcmp(start, "bssid") == 0) {
		bssid_updated = true;
		textBssid->setText(pos);
	    } else if (strcmp(start, "ssid") == 0) {
		ssid_updated = true;
		textSsid->setText(pos);
	    } else if (strcmp(start, "ip_address") == 0) {
		ipaddr_updated = true;
		textIpAddress->setText(pos);
	    } else if (strcmp(start, "wpa_state") == 0) {
		status_updated = true;
		textStatus->setText(pos);
	    } else if (strcmp(start, "key_mgmt") == 0) {
		auth_updated = true;
		textAuthentication->setText(pos);
		/* TODO: could add EAP status to this */
	    } else if (strcmp(start, "pairwise_cipher") == 0) {
		pairwise_cipher = pos;
	    } else if (strcmp(start, "group_cipher") == 0) {
		group_cipher = pos;
	    }
	}
	
	if (last)
	    break;
	start = end + 1;
    }
    
    if (pairwise_cipher || group_cipher) {
	QString encr;
	if (pairwise_cipher && group_cipher &&
	    strcmp(pairwise_cipher, group_cipher) != 0) {
	    encr.append(pairwise_cipher);
	    encr.append(" + ");
	    encr.append(group_cipher);
	} else if (pairwise_cipher) {
	    encr.append(pairwise_cipher);
	} else if (group_cipher) {
	    encr.append(group_cipher);
	    encr.append(" [group key only]");
	} else {
	    encr.append("?");
	}
	textEncryption->setText(encr);
    } else
	textEncryption->clear();

    if (!status_updated)
	textStatus->clear();
    if (!auth_updated)
	textAuthentication->clear();
    if (!ssid_updated)
	textSsid->clear();
    if (!bssid_updated)
	textBssid->clear();
    if (!ipaddr_updated)
	textIpAddress->clear();
}


void WpaGui::updateNetworks()
{
    char buf[2048], *start, *end, *id, *ssid, *bssid, *flags;
    size_t len;
    int first_active = -1;
    bool selected = false;

    if (!networkMayHaveChanged)
	return;

    networkSelect->clear();

    if (ctrl_conn == NULL)
	return;
    
    len = sizeof(buf) - 1;
    if (ctrlRequest("LIST_NETWORKS", buf, &len) < 0)
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
	
	QString network(id);
	network.append(": ");
	network.append(ssid);
	networkSelect->insertItem(network);
	
	if (strstr(flags, "[CURRENT]")) {
	    networkSelect->setCurrentItem(networkSelect->count() - 1);
	    selected = true;
	} else if (first_active < 0 && strstr(flags, "[DISABLED]") == NULL)
	    first_active = networkSelect->count() - 1;
	
	if (last)
	    break;
	start = end + 1;
    }

    if (!selected && first_active >= 0)
	networkSelect->setCurrentItem(first_active);

    networkMayHaveChanged = false;
}


void WpaGui::helpIndex()
{
    printf("helpIndex\n");
}


void WpaGui::helpContents()
{
    printf("helpContents\n");
}


void WpaGui::helpAbout()
{
    QMessageBox::about(this, "wpa_gui for wpa_supplicant",
		       "Copyright (c) 2003-2005,\n"
		       "Jouni Malinen <jkmaline@cc.hut.fi>\n"
		       "and contributors.\n"
		       "\n"
		       "This program is free software. You can\n"
		       "distribute it and/or modify it under the terms of\n"
		       "the GNU General Public License version 2.\n"
		       "\n"
		       "Alternatively, this software may be distributed\n"
		       "under the terms of the BSD license.\n"
		       "\n"
		       "This product includes software developed\n"
		       "by the OpenSSL Project for use in the\n"
		       "OpenSSL Toolkit (http://www.openssl.org/)\n");
}


void WpaGui::disconnect()
{
    char reply[10];
    size_t reply_len = sizeof(reply);
    ctrlRequest("DISCONNECT", reply, &reply_len);
}


void WpaGui::scan()
{
    if (scanres) {
	scanres->close();
	delete scanres;
    }

    scanres = new ScanResults();
    if (scanres == NULL)
	return;
    scanres->setWpaGui(this);
    scanres->show();
    scanres->exec();
}


void WpaGui::eventHistory()
{
    if (eh) {
	eh->close();
	delete eh;
    }

    eh = new EventHistory();
    if (eh == NULL)
	return;
    eh->addEvents(msgs);
    eh->show();
    eh->exec();
}


void WpaGui::ping()
{
    char buf[10];
    size_t len;
    
    if (scanres && !scanres->isVisible()) {
	delete scanres;
	scanres = NULL;
    }
    
    if (eh && !eh->isVisible()) {
	delete eh;
	eh = NULL;
    }
    
    if (udr && !udr->isVisible()) {
	delete udr;
	udr = NULL;
    }
    
    len = sizeof(buf) - 1;
    if (ctrlRequest("PING", buf, &len) < 0) {
	printf("PING failed - trying to reconnect\n");
	if (openCtrlConnection(ctrl_iface) >= 0) {
	    printf("Reconnected successfully\n");
	    pingsToStatusUpdate = 0;
	}
    }

    pingsToStatusUpdate--;
    if (pingsToStatusUpdate <= 0) {
	updateStatus();
	updateNetworks();
    }
}


static int str_match(const char *a, const char *b)
{
    return strncmp(a, b, strlen(b)) == 0;
}


void WpaGui::processMsg(char *msg)
{
    char *pos = msg, *pos2;
    int priority = 2;
    
    if (*pos == '<') {
	/* skip priority */
	pos++;
	priority = atoi(pos);
	pos = strchr(pos, '>');
	if (pos)
	    pos++;
	else
	    pos = msg;
    }

    WpaMsg wm(pos, priority);
    if (eh)
	eh->addEvent(wm);
    msgs.append(wm);
    while (msgs.count() > 100)
	msgs.pop_front();
    
    /* Update last message with truncated version of the event */
    if (strncmp(pos, "CTRL-", 5) == 0) {
	pos2 = strchr(pos, str_match(pos, WPA_CTRL_REQ) ? ':' : ' ');
	if (pos2)
	    pos2++;
	else
	    pos2 = pos;
    } else
	pos2 = pos;
    QString lastmsg = pos2;
    lastmsg.truncate(40);
    textLastMessage->setText(lastmsg);
    
    pingsToStatusUpdate = 0;
    networkMayHaveChanged = true;
    
    if (str_match(pos, WPA_CTRL_REQ))
	processCtrlReq(pos + strlen(WPA_CTRL_REQ));
}


void WpaGui::processCtrlReq(const char *req)
{
    if (udr) {
	udr->close();
	delete udr;
    }
    udr = new UserDataRequest();
    if (udr == NULL)
	return;
    if (udr->setParams(this, req) < 0) {
	delete udr;
	udr = NULL;
	return;
    }
    udr->show();
    udr->exec();
}


void WpaGui::receiveMsgs()
{
    char buf[256];
    size_t len;
    
    while (wpa_ctrl_pending(monitor_conn)) {
	len = sizeof(buf) - 1;
	if (wpa_ctrl_recv(monitor_conn, buf, &len) == 0) {
	    buf[len] = '\0';
	    processMsg(buf);
	}
    }
}


void WpaGui::connectB()
{
    char reply[10];
    size_t reply_len = sizeof(reply);
    ctrlRequest("REASSOCIATE", reply, &reply_len);
}


void WpaGui::selectNetwork( const QString &sel )
{
    QString cmd(sel);
    char reply[10];
    size_t reply_len = sizeof(reply);
    
    int pos = cmd.find(':');
    if (pos < 0) {
	printf("Invalid selectNetwork '%s'\n", cmd.ascii());
	return;
    }
    cmd.truncate(pos);
    cmd.prepend("SELECT_NETWORK ");
    ctrlRequest(cmd.ascii(), reply, &reply_len);
}


void WpaGui::editNetwork()
{
    QString sel(networkSelect->currentText());
    int pos = sel.find(':');
    if (pos < 0) {
	printf("Invalid selectNetwork '%s'\n", sel.ascii());
	return;
    }
    sel.truncate(pos);
    
    NetworkConfig *nc = new NetworkConfig();
    if (nc == NULL)
	return;
    nc->setWpaGui(this);
    
    nc->paramsFromConfig(sel.toInt());
    nc->show();
    nc->exec();
}


void WpaGui::triggerUpdate()
{
    updateStatus();
    networkMayHaveChanged = true;
    updateNetworks();
}


void WpaGui::addNetwork()
{
    NetworkConfig *nc = new NetworkConfig();
    if (nc == NULL)
	return;
    nc->setWpaGui(this);
    nc->newNetwork();
    nc->show();
    nc->exec();
}
