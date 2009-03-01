/*
 * wpa_gui - WpaGui class
 * Copyright (c) 2005-2008, Jouni Malinen <j@w1.fi>
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

#ifdef __MINGW32__
/* Need to get getopt() */
#include <unistd.h>
#endif

#ifdef CONFIG_NATIVE_WINDOWS
#include <windows.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include <cstdio>
#include <QMessageBox>
#include <QCloseEvent>
#include <QImageReader>
#include <QSettings>

#include "wpagui.h"
#include "dirent.h"
#include "wpa_ctrl.h"
#include "userdatarequest.h"
#include "networkconfig.h"

#if 1
/* Silence stdout */
#define printf wpagui_printf
static int wpagui_printf(const char *, ...)
{
	return 0;
}
#endif

WpaGui::WpaGui(QApplication *_app, QWidget *parent, const char *, Qt::WFlags)
	: QMainWindow(parent), app(_app)
{
	setupUi(this);

#ifdef CONFIG_NATIVE_WINDOWS
	fileStopServiceAction = new QAction(this);
	fileStopServiceAction->setObjectName("Stop Service");
	fileStopServiceAction->setIconText("Stop Service");
	fileMenu->insertAction(actionWPS, fileStopServiceAction);

	fileStartServiceAction = new QAction(this);
	fileStartServiceAction->setObjectName("Start Service");
	fileStartServiceAction->setIconText("Start Service");
	fileMenu->insertAction(fileStopServiceAction, fileStartServiceAction);

	connect(fileStartServiceAction, SIGNAL(triggered()), this,
		SLOT(startService()));
	connect(fileStopServiceAction, SIGNAL(triggered()), this,
		SLOT(stopService()));

	addInterfaceAction = new QAction(this);
	addInterfaceAction->setIconText("Add Interface");
	fileMenu->insertAction(fileStartServiceAction, addInterfaceAction);

	connect(addInterfaceAction, SIGNAL(triggered()), this,
		SLOT(addInterface()));
#endif /* CONFIG_NATIVE_WINDOWS */

	(void) statusBar();

	/*
	 * Disable WPS tab by default; it will be enabled if wpa_supplicant is
	 * built with WPS support.
	 */
	wpsTab->setEnabled(false);
	wpaguiTab->setTabEnabled(wpaguiTab->indexOf(wpsTab), false);

	connect(fileEventHistoryAction, SIGNAL(triggered()), this,
		SLOT(eventHistory()));
	connect(fileSaveConfigAction, SIGNAL(triggered()), this,
		SLOT(saveConfig()));
	connect(actionWPS, SIGNAL(triggered()), this, SLOT(wpsDialog()));
	connect(fileExitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
	connect(networkAddAction, SIGNAL(triggered()), this,
		SLOT(addNetwork()));
	connect(networkEditAction, SIGNAL(triggered()), this,
		SLOT(editSelectedNetwork()));
	connect(networkRemoveAction, SIGNAL(triggered()), this,
		SLOT(removeSelectedNetwork()));
	connect(networkEnableAllAction, SIGNAL(triggered()), this,
		SLOT(enableAllNetworks()));
	connect(networkDisableAllAction, SIGNAL(triggered()), this,
		SLOT(disableAllNetworks()));
	connect(networkRemoveAllAction, SIGNAL(triggered()), this,
		SLOT(removeAllNetworks()));
	connect(helpIndexAction, SIGNAL(triggered()), this, SLOT(helpIndex()));
	connect(helpContentsAction, SIGNAL(triggered()), this,
		SLOT(helpContents()));
	connect(helpAboutAction, SIGNAL(triggered()), this, SLOT(helpAbout()));
	connect(disconnectButton, SIGNAL(clicked()), this, SLOT(disconnect()));
	connect(scanButton, SIGNAL(clicked()), this, SLOT(scan()));
	connect(connectButton, SIGNAL(clicked()), this, SLOT(connectB()));
	connect(adapterSelect, SIGNAL(activated(const QString&)), this,
		SLOT(selectAdapter(const QString&)));
	connect(networkSelect, SIGNAL(activated(const QString&)), this,
		SLOT(selectNetwork(const QString&)));
	connect(addNetworkButton, SIGNAL(clicked()), this, SLOT(addNetwork()));
	connect(editNetworkButton, SIGNAL(clicked()), this,
		SLOT(editListedNetwork()));
	connect(removeNetworkButton, SIGNAL(clicked()), this,
		SLOT(removeListedNetwork()));
	connect(networkList, SIGNAL(itemSelectionChanged()), this,
		SLOT(updateNetworkDisabledStatus()));
	connect(enableRadioButton, SIGNAL(toggled(bool)), this,
		SLOT(enableListedNetwork(bool)));
	connect(disableRadioButton, SIGNAL(toggled(bool)), this,
		SLOT(disableListedNetwork(bool)));
	connect(scanNetworkButton, SIGNAL(clicked()), this, SLOT(scan()));
	connect(networkList, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
		this, SLOT(editListedNetwork()));
	connect(wpaguiTab, SIGNAL(currentChanged(int)), this,
		SLOT(tabChanged(int)));
	connect(wpsPbcButton, SIGNAL(clicked()), this, SLOT(wpsPbc()));
	connect(wpsPinButton, SIGNAL(clicked()), this, SLOT(wpsGeneratePin()));
	connect(wpsApPinEdit, SIGNAL(textChanged(const QString &)), this,
		SLOT(wpsApPinChanged(const QString &)));
	connect(wpsApPinButton, SIGNAL(clicked()), this, SLOT(wpsApPin()));

	eh = NULL;
	scanres = NULL;
	add_iface = NULL;
	udr = NULL;
	tray_icon = NULL;
	startInTray = false;
	ctrl_iface = NULL;
	ctrl_conn = NULL;
	monitor_conn = NULL;
	msgNotifier = NULL;
	ctrl_iface_dir = strdup("/var/run/wpa_supplicant");

	parse_argv();

	if (app->isSessionRestored()) {
		QSettings settings("wpa_supplicant", "wpa_gui");
		settings.beginGroup("state");
		if (app->sessionId().compare(settings.value("session_id").
					     toString()) == 0)
			startInTray = settings.value("in_tray").toBool();
		settings.endGroup();
	}

	if (QSystemTrayIcon::isSystemTrayAvailable())
		createTrayIcon(startInTray);
	else
		show();

	connectedToService = false;
	textStatus->setText("connecting to wpa_supplicant");
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), SLOT(ping()));
	timer->setSingleShot(FALSE);
	timer->start(1000);

	if (openCtrlConnection(ctrl_iface) < 0) {
		printf("Failed to open control connection to "
		       "wpa_supplicant.\n");
	}

	updateStatus();
	networkMayHaveChanged = true;
	updateNetworks();
}


WpaGui::~WpaGui()
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

	if (add_iface) {
		add_iface->close();
		delete add_iface;
		add_iface = NULL;
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


void WpaGui::languageChange()
{
	retranslateUi(this);
}


void WpaGui::parse_argv()
{
	int c;
	for (;;) {
		c = getopt(qApp->argc(), qApp->argv(), "i:p:t");
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
		case 't':
			startInTray = true;
			break;
		}
	}
}


int WpaGui::openCtrlConnection(const char *ifname)
{
	char *cfile;
	int flen;
	char buf[2048], *pos, *pos2;
	size_t len;

	if (ifname) {
		if (ifname != ctrl_iface) {
			free(ctrl_iface);
			ctrl_iface = strdup(ifname);
		}
	} else {
#ifdef CONFIG_CTRL_IFACE_UDP
		free(ctrl_iface);
		ctrl_iface = strdup("udp");
#endif /* CONFIG_CTRL_IFACE_UDP */
#ifdef CONFIG_CTRL_IFACE_UNIX
		struct dirent *dent;
		DIR *dir = opendir(ctrl_iface_dir);
		free(ctrl_iface);
		ctrl_iface = NULL;
		if (dir) {
			while ((dent = readdir(dir))) {
#ifdef _DIRENT_HAVE_D_TYPE
				/* Skip the file if it is not a socket.
				 * Also accept DT_UNKNOWN (0) in case
				 * the C library or underlying file
				 * system does not support d_type. */
				if (dent->d_type != DT_SOCK &&
				    dent->d_type != DT_UNKNOWN)
					continue;
#endif /* _DIRENT_HAVE_D_TYPE */

				if (strcmp(dent->d_name, ".") == 0 ||
				    strcmp(dent->d_name, "..") == 0)
					continue;
				printf("Selected interface '%s'\n",
				       dent->d_name);
				ctrl_iface = strdup(dent->d_name);
				break;
			}
			closedir(dir);
		}
#endif /* CONFIG_CTRL_IFACE_UNIX */
#ifdef CONFIG_CTRL_IFACE_NAMED_PIPE
		struct wpa_ctrl *ctrl;
		int ret;

		free(ctrl_iface);
		ctrl_iface = NULL;

		ctrl = wpa_ctrl_open(NULL);
		if (ctrl) {
			len = sizeof(buf) - 1;
			ret = wpa_ctrl_request(ctrl, "INTERFACES", 10, buf,
					       &len, NULL);
			if (ret >= 0) {
				connectedToService = true;
				buf[len] = '\0';
				pos = strchr(buf, '\n');
				if (pos)
					*pos = '\0';
				ctrl_iface = strdup(buf);
			}
			wpa_ctrl_close(ctrl);
		}
#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */
	}

	if (ctrl_iface == NULL) {
#ifdef CONFIG_NATIVE_WINDOWS
		static bool first = true;
		if (first && !serviceRunning()) {
			first = false;
			if (QMessageBox::warning(
				    this, qAppName(),
				    "wpa_supplicant service is not running.\n"
				    "Do you want to start it?",
				    QMessageBox::Yes | QMessageBox::No) ==
			    QMessageBox::Yes)
				startService();
		}
#endif /* CONFIG_NATIVE_WINDOWS */
		return -1;
	}

#ifdef CONFIG_CTRL_IFACE_UNIX
	flen = strlen(ctrl_iface_dir) + strlen(ctrl_iface) + 2;
	cfile = (char *) malloc(flen);
	if (cfile == NULL)
		return -1;
	snprintf(cfile, flen, "%s/%s", ctrl_iface_dir, ctrl_iface);
#else /* CONFIG_CTRL_IFACE_UNIX */
	flen = strlen(ctrl_iface) + 1;
	cfile = (char *) malloc(flen);
	if (cfile == NULL)
		return -1;
	snprintf(cfile, flen, "%s", ctrl_iface);
#endif /* CONFIG_CTRL_IFACE_UNIX */

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

#if defined(CONFIG_CTRL_IFACE_UNIX) || defined(CONFIG_CTRL_IFACE_UDP)
	msgNotifier = new QSocketNotifier(wpa_ctrl_get_fd(monitor_conn),
					  QSocketNotifier::Read, this);
	connect(msgNotifier, SIGNAL(activated(int)), SLOT(receiveMsgs()));
#endif

	adapterSelect->clear();
	adapterSelect->addItem(ctrl_iface);
	adapterSelect->setCurrentIndex(0);

	len = sizeof(buf) - 1;
	if (wpa_ctrl_request(ctrl_conn, "INTERFACES", 10, buf, &len, NULL) >=
	    0) {
		buf[len] = '\0';
		pos = buf;
		while (*pos) {
			pos2 = strchr(pos, '\n');
			if (pos2)
				*pos2 = '\0';
			if (strcmp(pos, ctrl_iface) != 0)
				adapterSelect->addItem(pos);
			if (pos2)
				pos = pos2 + 1;
			else
				break;
		}
	}

	len = sizeof(buf) - 1;
	if (wpa_ctrl_request(ctrl_conn, "GET_CAPABILITY eap", 18, buf, &len,
			     NULL) >= 0) {
		buf[len] = '\0';

		QString res(buf);
		QStringList types = res.split(QChar(' '));
		bool wps = types.contains("WSC");
		actionWPS->setEnabled(wps);
		wpsTab->setEnabled(wps);
		wpaguiTab->setTabEnabled(wpaguiTab->indexOf(wpsTab), wps);
	}

	return 0;
}


static void wpa_gui_msg_cb(char *msg, size_t)
{
	/* This should not happen anymore since two control connections are
	 * used. */
	printf("missed message: %s\n", msg);
}


int WpaGui::ctrlRequest(const char *cmd, char *buf, size_t *buflen)
{
	int ret;

	if (ctrl_conn == NULL)
		return -3;
	ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), buf, buflen,
			       wpa_gui_msg_cb);
	if (ret == -2)
		printf("'%s' command timed out.\n", cmd);
	else if (ret < 0)
		printf("'%s' command failed.\n", cmd);

	return ret;
}


void WpaGui::updateStatus()
{
	char buf[2048], *start, *end, *pos;
	size_t len;

	pingsToStatusUpdate = 10;

	len = sizeof(buf) - 1;
	if (ctrl_conn == NULL || ctrlRequest("STATUS", buf, &len) < 0) {
		textStatus->setText("Could not get status from "
				    "wpa_supplicant");
		textAuthentication->clear();
		textEncryption->clear();
		textSsid->clear();
		textBssid->clear();
		textIpAddress->clear();

#ifdef CONFIG_NATIVE_WINDOWS
		static bool first = true;
		if (first && connectedToService &&
		    (ctrl_iface == NULL || *ctrl_iface == '\0')) {
			first = false;
			if (QMessageBox::information(
				    this, qAppName(),
				    "No network interfaces in use.\n"
				    "Would you like to add one?",
				    QMessageBox::Yes | QMessageBox::No) ==
			    QMessageBox::Yes)
				addInterface();
		}
#endif /* CONFIG_NATIVE_WINDOWS */
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
		} else {
			encr.append(group_cipher);
			encr.append(" [group key only]");
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
	int was_selected = -1;
	bool current = false;

	if (!networkMayHaveChanged)
		return;

	if (networkList->currentRow() >= 0)
		was_selected = networkList->currentRow();

	networkSelect->clear();
	networkList->clear();

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
		networkSelect->addItem(network);
		networkList->addItem(network);

		if (strstr(flags, "[CURRENT]")) {
			networkSelect->setCurrentIndex(networkSelect->count() -
						      1);
			current = true;
		} else if (first_active < 0 &&
			   strstr(flags, "[DISABLED]") == NULL)
			first_active = networkSelect->count() - 1;

		if (last)
			break;
		start = end + 1;
	}

	if (networkSelect->count() > 1)
		networkSelect->addItem("Select any network");

	if (!current && first_active >= 0)
		networkSelect->setCurrentIndex(first_active);

	if (was_selected >= 0 && networkList->count() > 0) {
		if (was_selected < networkList->count())
			networkList->setCurrentRow(was_selected);
		else
			networkList->setCurrentRow(networkList->count() - 1);
	}
	else
		networkList->setCurrentRow(networkSelect->currentIndex());

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
			   "Copyright (c) 2003-2008,\n"
			   "Jouni Malinen <j@w1.fi>\n"
			   "and contributors.\n"
			   "\n"
			   "This program is free software. You can\n"
			   "distribute it and/or modify it under the terms "
			   "of\n"
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
	stopWpsRun(false);
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

#ifdef CONFIG_CTRL_IFACE_NAMED_PIPE
	/*
	 * QSocketNotifier cannot be used with Windows named pipes, so use a
	 * timer to check for received messages for now. This could be
	 * optimized be doing something specific to named pipes or Windows
	 * events, but it is not clear what would be the best way of doing that
	 * in Qt.
	 */
	receiveMsgs();
#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */

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

#ifndef CONFIG_CTRL_IFACE_NAMED_PIPE
	/* Use less frequent pings and status updates when the main window is
	 * hidden (running in taskbar). */
	int interval = isHidden() ? 5000 : 1000;
	if (timer->interval() != interval)
		timer->setInterval(interval);
#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */
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
	else if (str_match(pos, WPA_EVENT_SCAN_RESULTS) && scanres)
		scanres->updateResults();
	else if (str_match(pos, WPA_EVENT_DISCONNECTED))
		showTrayMessage(QSystemTrayIcon::Information, 3,
				"Disconnected from network.");
	else if (str_match(pos, WPA_EVENT_CONNECTED)) {
		showTrayMessage(QSystemTrayIcon::Information, 3,
				"Connection to network established.");
		QTimer::singleShot(5 * 1000, this, SLOT(showTrayStatus()));
		stopWpsRun(true);
	} else if (str_match(pos, WPS_EVENT_AP_AVAILABLE_PBC)) {
		showTrayMessage(QSystemTrayIcon::Information, 3,
				"Wi-Fi Protected Setup (WPS) AP\n"
				"in active PBC mode found.");
		wpsStatusText->setText("WPS AP in active PBC mode found");
		wpaguiTab->setCurrentWidget(wpsTab);
		wpsInstructions->setText("Press the PBC button on the screen "
					 "to start registration");
	} else if (str_match(pos, WPS_EVENT_AP_AVAILABLE_PIN)) {
		showTrayMessage(QSystemTrayIcon::Information, 3,
				"Wi-Fi Protected Setup (WPS) AP\n"
				" in active PIN mode found.");
		wpsStatusText->setText("WPS AP with recently selected "
				       "registrar");
		wpaguiTab->setCurrentWidget(wpsTab);
	} else if (str_match(pos, WPS_EVENT_AP_AVAILABLE)) {
		showTrayMessage(QSystemTrayIcon::Information, 3,
				"Wi-Fi Protected Setup (WPS)\n"
				"AP detected.");
		wpsStatusText->setText("WPS AP detected");
		wpaguiTab->setCurrentWidget(wpsTab);
	} else if (str_match(pos, WPS_EVENT_OVERLAP)) {
		showTrayMessage(QSystemTrayIcon::Information, 3,
				"Wi-Fi Protected Setup (WPS)\n"
				"PBC mode overlap detected.");
		wpsStatusText->setText("PBC mode overlap detected");
		wpsInstructions->setText("More than one AP is currently in "
					 "active WPS PBC mode. Wait couple of "
					 "minutes and try again");
		wpaguiTab->setCurrentWidget(wpsTab);
	} else if (str_match(pos, WPS_EVENT_CRED_RECEIVED)) {
		wpsStatusText->setText("Network configuration received");
		wpaguiTab->setCurrentWidget(wpsTab);
	} else if (str_match(pos, WPA_EVENT_EAP_METHOD)) {
		if (strstr(pos, "(WSC)"))
			wpsStatusText->setText("Registration started");
	} else if (str_match(pos, WPS_EVENT_M2D)) {
		wpsStatusText->setText("Registrar does not yet know PIN");
	} else if (str_match(pos, WPS_EVENT_FAIL)) {
		wpsStatusText->setText("Registration failed");
	} else if (str_match(pos, WPS_EVENT_SUCCESS)) {
		wpsStatusText->setText("Registration succeeded");
	}
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

	while (monitor_conn && wpa_ctrl_pending(monitor_conn) > 0) {
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

	if (cmd.startsWith("Select any")) {
		cmd = "any";
	} else {
		int pos = cmd.indexOf(':');
		if (pos < 0) {
			printf("Invalid selectNetwork '%s'\n",
			       cmd.toAscii().constData());
			return;
		}
		cmd.truncate(pos);
	}
	cmd.prepend("SELECT_NETWORK ");
	ctrlRequest(cmd.toAscii().constData(), reply, &reply_len);
	triggerUpdate();
	stopWpsRun(false);
}


void WpaGui::enableNetwork(const QString &sel)
{
	QString cmd(sel);
	char reply[10];
	size_t reply_len = sizeof(reply);

	if (!cmd.startsWith("all")) {
		int pos = cmd.indexOf(':');
		if (pos < 0) {
			printf("Invalid enableNetwork '%s'\n",
			       cmd.toAscii().constData());
			return;
		}
		cmd.truncate(pos);
	}
	cmd.prepend("ENABLE_NETWORK ");
	ctrlRequest(cmd.toAscii().constData(), reply, &reply_len);
	triggerUpdate();
}


void WpaGui::disableNetwork(const QString &sel)
{
	QString cmd(sel);
	char reply[10];
	size_t reply_len = sizeof(reply);

	if (!cmd.startsWith("all")) {
		int pos = cmd.indexOf(':');
		if (pos < 0) {
			printf("Invalid disableNetwork '%s'\n",
			       cmd.toAscii().constData());
			return;
		}
		cmd.truncate(pos);
	}
	cmd.prepend("DISABLE_NETWORK ");
	ctrlRequest(cmd.toAscii().constData(), reply, &reply_len);
	triggerUpdate();
}


void WpaGui::editNetwork(const QString &sel)
{
	QString cmd(sel);
	int id = -1;

	if (!cmd.startsWith("Select any")) {
		int pos = sel.indexOf(':');
		if (pos < 0) {
			printf("Invalid editNetwork '%s'\n",
			       cmd.toAscii().constData());
			return;
		}
		cmd.truncate(pos);
		id = cmd.toInt();
	}

	NetworkConfig *nc = new NetworkConfig();
	if (nc == NULL)
		return;
	nc->setWpaGui(this);

	if (id >= 0)
		nc->paramsFromConfig(id);
	else
		nc->newNetwork();

	nc->show();
	nc->exec();
}


void WpaGui::editSelectedNetwork()
{
	if (networkSelect->count() < 1) {
		QMessageBox::information(this, "No Networks",
			                 "There are no networks to edit.\n");
		return;
	}
	QString sel(networkSelect->currentText());
	editNetwork(sel);
}


void WpaGui::editListedNetwork()
{
	if (networkList->currentRow() < 0) {
		QMessageBox::information(this, "Select A Network",
					 "Select a network from the list to"
					 " edit it.\n");
		return;
	}
	QString sel(networkList->currentItem()->text());
	editNetwork(sel);
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


void WpaGui::removeNetwork(const QString &sel)
{
	QString cmd(sel);
	char reply[10];
	size_t reply_len = sizeof(reply);

	if (cmd.startsWith("Select any"))
		return;

	if (!cmd.startsWith("all")) {
		int pos = cmd.indexOf(':');
		if (pos < 0) {
			printf("Invalid removeNetwork '%s'\n",
			       cmd.toAscii().constData());
			return;
		}
		cmd.truncate(pos);
	}
	cmd.prepend("REMOVE_NETWORK ");
	ctrlRequest(cmd.toAscii().constData(), reply, &reply_len);
	triggerUpdate();
}


void WpaGui::removeSelectedNetwork()
{
	if (networkSelect->count() < 1) {
		QMessageBox::information(this, "No Networks",
			                 "There are no networks to remove.\n");
		return;
	}
	QString sel(networkSelect->currentText());
	removeNetwork(sel);
}


void WpaGui::removeListedNetwork()
{
	if (networkList->currentRow() < 0) {
		QMessageBox::information(this, "Select A Network",
					 "Select a network from the list to"
					 " remove it.\n");
		return;
	}
	QString sel(networkList->currentItem()->text());
	removeNetwork(sel);
}


void WpaGui::enableAllNetworks()
{
	QString sel("all");
	enableNetwork(sel);
}


void WpaGui::disableAllNetworks()
{
	QString sel("all");
	disableNetwork(sel);
}


void WpaGui::removeAllNetworks()
{
	QString sel("all");
	removeNetwork(sel);
}


int WpaGui::getNetworkDisabled(const QString &sel)
{
	QString cmd(sel);
	char reply[10];
	size_t reply_len = sizeof(reply) - 1;
	int pos = cmd.indexOf(':');
	if (pos < 0) {
		printf("Invalid getNetworkDisabled '%s'\n",
		       cmd.toAscii().constData());
		return -1;
	}
	cmd.truncate(pos);
	cmd.prepend("GET_NETWORK ");
	cmd.append(" disabled");

	if (ctrlRequest(cmd.toAscii().constData(), reply, &reply_len) >= 0
	    && reply_len >= 1) {
		reply[reply_len] = '\0';
		if (!str_match(reply, "FAIL"))
			return atoi(reply);
	}

	return -1;
}


void WpaGui::updateNetworkDisabledStatus()
{
	if (networkList->currentRow() < 0)
		return;

	QString sel(networkList->currentItem()->text());

	switch (getNetworkDisabled(sel)) {
	case 0:
		if (!enableRadioButton->isChecked())
			enableRadioButton->setChecked(true);
		return;
	case 1:
		if (!disableRadioButton->isChecked())
			disableRadioButton->setChecked(true);
		return;
	}
}


void WpaGui::enableListedNetwork(bool enabled)
{
	if (networkList->currentRow() < 0 || !enabled)
		return;

	QString sel(networkList->currentItem()->text());

	if (getNetworkDisabled(sel) == 1)
		enableNetwork(sel);
}


void WpaGui::disableListedNetwork(bool disabled)
{
	if (networkList->currentRow() < 0 || !disabled)
		return;

	QString sel(networkList->currentItem()->text());

	if (getNetworkDisabled(sel) == 0)
		disableNetwork(sel);
}


void WpaGui::saveConfig()
{
	char buf[10];
	size_t len;

	len = sizeof(buf) - 1;
	ctrlRequest("SAVE_CONFIG", buf, &len);

	buf[len] = '\0';

	if (str_match(buf, "FAIL"))
		QMessageBox::warning(this, "Failed to save configuration",
			             "The configuration could not be saved.\n"
				     "\n"
				     "The update_config=1 configuration option\n"
				     "must be used for configuration saving to\n"
				     "be permitted.\n");
	else
		QMessageBox::information(this, "Saved configuration",
			                 "The current configuration was saved."
					 "\n");
}


void WpaGui::selectAdapter( const QString & sel )
{
	if (openCtrlConnection(sel.toAscii().constData()) < 0)
		printf("Failed to open control connection to "
		       "wpa_supplicant.\n");
	updateStatus();
	updateNetworks();
}


void WpaGui::createTrayIcon(bool trayOnly)
{
	QApplication::setQuitOnLastWindowClosed(false);

	tray_icon = new QSystemTrayIcon(this);
	tray_icon->setToolTip(qAppName() + " - wpa_supplicant user interface");
	if (QImageReader::supportedImageFormats().contains(QByteArray("svg")))
		tray_icon->setIcon(QIcon(":/icons/wpa_gui.svg"));
	else
		tray_icon->setIcon(QIcon(":/icons/wpa_gui.png"));

	connect(tray_icon,
		SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));

	ackTrayIcon = false;

	tray_menu = new QMenu(this);

	disconnectAction = new QAction("&Disconnect", this);
	reconnectAction = new QAction("Re&connect", this);
	connect(disconnectAction, SIGNAL(triggered()), this,
		SLOT(disconnect()));
	connect(reconnectAction, SIGNAL(triggered()), this,
		SLOT(connectB()));
	tray_menu->addAction(disconnectAction);
	tray_menu->addAction(reconnectAction);
	tray_menu->addSeparator();

	eventAction = new QAction("&Event History", this);
	scanAction = new QAction("Scan &Results", this);
	statAction = new QAction("S&tatus", this);
	connect(eventAction, SIGNAL(triggered()), this, SLOT(eventHistory()));
	connect(scanAction, SIGNAL(triggered()), this, SLOT(scan()));
	connect(statAction, SIGNAL(triggered()), this, SLOT(showTrayStatus()));
	tray_menu->addAction(eventAction);
	tray_menu->addAction(scanAction);
	tray_menu->addAction(statAction);
	tray_menu->addSeparator();

	showAction = new QAction("&Show Window", this);
	hideAction = new QAction("&Hide Window", this);
	quitAction = new QAction("&Quit", this);
	connect(showAction, SIGNAL(triggered()), this, SLOT(show()));
	connect(hideAction, SIGNAL(triggered()), this, SLOT(hide()));
	connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
	tray_menu->addAction(showAction);
	tray_menu->addAction(hideAction);
	tray_menu->addSeparator();
	tray_menu->addAction(quitAction);

	tray_icon->setContextMenu(tray_menu);

	tray_icon->show();

	if (!trayOnly)
		show();
	inTray = trayOnly;
}


void WpaGui::showTrayMessage(QSystemTrayIcon::MessageIcon type, int sec,
			     const QString & msg)
{
	if (!QSystemTrayIcon::supportsMessages())
		return;

	if (isVisible() || !tray_icon || !tray_icon->isVisible())
		return;

	tray_icon->showMessage(qAppName(), msg, type, sec * 1000);
}


void WpaGui::trayActivated(QSystemTrayIcon::ActivationReason how)
 {
	switch (how) {
	/* use close() here instead of hide() and allow the
	 * custom closeEvent handler take care of children */
	case QSystemTrayIcon::Trigger:
		ackTrayIcon = true;
		if (isVisible()) {
			close();
			inTray = true;
		} else {
			show();
			inTray = false;
		}
		break;
	case QSystemTrayIcon::MiddleClick:
		showTrayStatus();
		break;
	default:
		break;
	}
}


void WpaGui::showTrayStatus()
{
	char buf[2048];
	size_t len;

	len = sizeof(buf) - 1;
	if (ctrlRequest("STATUS", buf, &len) < 0)
		return;
	buf[len] = '\0';

	QString msg, status(buf);

	QStringList lines = status.split(QRegExp("\\n"));
	for (QStringList::Iterator it = lines.begin();
	     it != lines.end(); it++) {
		int pos = (*it).indexOf('=') + 1;
		if (pos < 1)
			continue;

		if ((*it).startsWith("bssid="))
			msg.append("BSSID:\t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("ssid="))
			msg.append("SSID: \t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("pairwise_cipher="))
			msg.append("PAIR: \t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("group_cipher="))
			msg.append("GROUP:\t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("key_mgmt="))
			msg.append("AUTH: \t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("wpa_state="))
			msg.append("STATE:\t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("ip_address="))
			msg.append("IP:   \t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("Supplicant PAE state="))
			msg.append("PAE:  \t" + (*it).mid(pos) + "\n");
		else if ((*it).startsWith("EAP state="))
			msg.append("EAP:  \t" + (*it).mid(pos) + "\n");
	}

	if (!msg.isEmpty())
		showTrayMessage(QSystemTrayIcon::Information, 10, msg);
}


void WpaGui::closeEvent(QCloseEvent *event)
{
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

	if (tray_icon && !ackTrayIcon) {
		/* give user a visual hint that the tray icon exists */
		if (QSystemTrayIcon::supportsMessages()) {
			hide();
			showTrayMessage(QSystemTrayIcon::Information, 3,
					qAppName() + " will keep running in "
					"the system tray.");
		} else {
			QMessageBox::information(this, qAppName() + " systray",
						 "The program will keep "
						 "running in the system "
						 "tray.");
		}
		ackTrayIcon = true;
	}

	event->accept();
}


void WpaGui::wpsDialog()
{
	wpaguiTab->setCurrentWidget(wpsTab);
}


void WpaGui::tabChanged(int index)
{
	if (index != 2)
		return;

	if (wpsRunning)
		return;

	wpsApPinEdit->setEnabled(!bssFromScan.isEmpty());
	if (bssFromScan.isEmpty())
		wpsApPinButton->setEnabled(false);
}


void WpaGui::wpsPbc()
{
	char reply[20];
	size_t reply_len = sizeof(reply);

	if (ctrlRequest("WPS_PBC", reply, &reply_len) < 0)
		return;

	wpsPinEdit->setEnabled(false);
	if (wpsStatusText->text().compare("WPS AP in active PBC mode found")) {
		wpsInstructions->setText("Press the push button on the AP to "
					 "start the PBC mode.");
	} else {
		wpsInstructions->setText("If you have not yet done so, press "
					 "the push button on the AP to start "
					 "the PBC mode.");
	}
	wpsStatusText->setText("Waiting for Registrar");
	wpsRunning = true;
}


void WpaGui::wpsGeneratePin()
{
	char reply[20];
	size_t reply_len = sizeof(reply) - 1;

	if (ctrlRequest("WPS_PIN any", reply, &reply_len) < 0)
		return;

	reply[reply_len] = '\0';

	wpsPinEdit->setText(reply);
	wpsPinEdit->setEnabled(true);
	wpsInstructions->setText("Enter the generated PIN into the Registrar "
				 "(either the internal one in the AP or an "
				 "external one).");
	wpsStatusText->setText("Waiting for Registrar");
	wpsRunning = true;
}


void WpaGui::setBssFromScan(const QString &bssid)
{
	bssFromScan = bssid;
	wpsApPinEdit->setEnabled(!bssFromScan.isEmpty());
	wpsApPinButton->setEnabled(wpsApPinEdit->text().length() == 8);
	wpsStatusText->setText("WPS AP selected from scan results");
	wpsInstructions->setText("If you want to use an AP device PIN, e.g., "
				 "from a label in the device, enter the eight "
				 "digit AP PIN and click Use AP PIN button.");
}


void WpaGui::wpsApPinChanged(const QString &text)
{
	wpsApPinButton->setEnabled(text.length() == 8);
}


void WpaGui::wpsApPin()
{
	char reply[20];
	size_t reply_len = sizeof(reply);

	QString cmd("WPS_REG " + bssFromScan + " " + wpsApPinEdit->text());
	if (ctrlRequest(cmd.toAscii().constData(), reply, &reply_len) < 0)
		return;

	wpsStatusText->setText("Waiting for AP/Enrollee");
	wpsRunning = true;
}


void WpaGui::stopWpsRun(bool success)
{
	if (wpsRunning)
		wpsStatusText->setText(success ? "Connected to the network" :
				       "Stopped");
	else
		wpsStatusText->setText("");
	wpsPinEdit->setEnabled(false);
	wpsInstructions->setText("");
	wpsRunning = false;
	bssFromScan = "";
	wpsApPinEdit->setEnabled(false);
	wpsApPinButton->setEnabled(false);
}


#ifdef CONFIG_NATIVE_WINDOWS

#ifndef WPASVC_NAME
#define WPASVC_NAME TEXT("wpasvc")
#endif

class ErrorMsg : public QMessageBox {
public:
	ErrorMsg(QWidget *parent, DWORD last_err = GetLastError());
	void showMsg(QString msg);
private:
	DWORD err;
};

ErrorMsg::ErrorMsg(QWidget *parent, DWORD last_err) :
	QMessageBox(parent), err(last_err)
{
	setWindowTitle("wpa_gui error");
	setIcon(QMessageBox::Warning);
}

void ErrorMsg::showMsg(QString msg)
{
	LPTSTR buf;

	setText(msg);
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			  FORMAT_MESSAGE_FROM_SYSTEM,
			  NULL, err, 0, (LPTSTR) (void *) &buf,
			  0, NULL) > 0) {
		QString msg = QString::fromWCharArray(buf);
		setInformativeText(QString("[%1] %2").arg(err).arg(msg));
		LocalFree(buf);
	} else {
		setInformativeText(QString("[%1]").arg(err));
	}

	exec();
}


void WpaGui::startService()
{
	SC_HANDLE svc, scm;

	scm = OpenSCManager(0, 0, SC_MANAGER_CONNECT);
	if (!scm) {
		ErrorMsg(this).showMsg("OpenSCManager failed");
		return;
	}

	svc = OpenService(scm, WPASVC_NAME, SERVICE_START);
	if (!svc) {
		ErrorMsg(this).showMsg("OpenService failed");
		CloseServiceHandle(scm);
		return;
	}

	if (!StartService(svc, 0, NULL)) {
		ErrorMsg(this).showMsg("Failed to start wpa_supplicant "
				       "service");
	}

	CloseServiceHandle(svc);
	CloseServiceHandle(scm);
}


void WpaGui::stopService()
{
	SC_HANDLE svc, scm;
	SERVICE_STATUS status;

	scm = OpenSCManager(0, 0, SC_MANAGER_CONNECT);
	if (!scm) {
		ErrorMsg(this).showMsg("OpenSCManager failed");
		return;
	}

	svc = OpenService(scm, WPASVC_NAME, SERVICE_STOP);
	if (!svc) {
		ErrorMsg(this).showMsg("OpenService failed");
		CloseServiceHandle(scm);
		return;
	}

	if (!ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
		ErrorMsg(this).showMsg("Failed to stop wpa_supplicant "
				       "service");
	}

	CloseServiceHandle(svc);
	CloseServiceHandle(scm);
}


bool WpaGui::serviceRunning()
{
	SC_HANDLE svc, scm;
	SERVICE_STATUS status;
	bool running = false;

	scm = OpenSCManager(0, 0, SC_MANAGER_CONNECT);
	if (!scm) {
		printf("OpenSCManager failed: %d\n", (int) GetLastError());
		return false;
	}

	svc = OpenService(scm, WPASVC_NAME, SERVICE_QUERY_STATUS);
	if (!svc) {
		printf("OpenService failed: %d\n\n", (int) GetLastError());
		CloseServiceHandle(scm);
		return false;
	}

	if (QueryServiceStatus(svc, &status)) {
		if (status.dwCurrentState != SERVICE_STOPPED)
			running = true;
	}

	CloseServiceHandle(svc);
	CloseServiceHandle(scm);

	return running;
}

#endif /* CONFIG_NATIVE_WINDOWS */


void WpaGui::addInterface()
{
	if (add_iface) {
		add_iface->close();
		delete add_iface;
	}
	add_iface = new AddInterface(this, this);
	add_iface->show();
	add_iface->exec();
}


void WpaGui::saveState()
{
	QSettings settings("wpa_supplicant", "wpa_gui");
	settings.beginGroup("state");
	settings.setValue("session_id", app->sessionId());
	settings.setValue("in_tray", inTray);
	settings.endGroup();
}
