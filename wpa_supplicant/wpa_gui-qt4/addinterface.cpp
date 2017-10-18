/*
 * wpa_gui - AddInterface class
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <cstdio>
#include "common/wpa_ctrl.h"

#include <QMessageBox>

#include "wpagui.h"
#include "addinterface.h"

#ifdef CONFIG_NATIVE_WINDOWS
#include <windows.h>

#ifndef WPA_KEY_ROOT
#define WPA_KEY_ROOT HKEY_LOCAL_MACHINE
#endif
#ifndef WPA_KEY_PREFIX
#define WPA_KEY_PREFIX TEXT("SOFTWARE\\wpa_supplicant")
#endif
#endif /* CONFIG_NATIVE_WINDOWS */


AddInterface::AddInterface(WpaGui *_wpagui, QWidget *parent)
	: QDialog(parent), wpagui(_wpagui)
{
	setWindowTitle(tr("Select network interface to add"));
	resize(400, 200);
	vboxLayout = new QVBoxLayout(this);

	interfaceWidget = new QTreeWidget(this);
	interfaceWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	interfaceWidget->setUniformRowHeights(true);
	interfaceWidget->setSortingEnabled(true);
	interfaceWidget->setColumnCount(3);
	interfaceWidget->headerItem()->setText(0, tr("driver"));
	interfaceWidget->headerItem()->setText(1, tr("interface"));
	interfaceWidget->headerItem()->setText(2, tr("description"));
	interfaceWidget->setItemsExpandable(false);
	interfaceWidget->setRootIsDecorated(false);
	vboxLayout->addWidget(interfaceWidget);

	connect(interfaceWidget,
		SIGNAL(itemActivated(QTreeWidgetItem *, int)), this,
		SLOT(interfaceSelected(QTreeWidgetItem *)));

	addInterfaces();
}


void AddInterface::addInterfaces()
{
#ifdef CONFIG_CTRL_IFACE_NAMED_PIPE
	struct wpa_ctrl *ctrl;
	int ret;
	char buf[2048];
	size_t len;

	ctrl = wpa_ctrl_open(NULL);
	if (ctrl == NULL)
		return;

	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, "INTERFACE_LIST", 14, buf, &len, NULL);
	if (ret < 0) {
		wpa_ctrl_close(ctrl);
		return;
	}
	buf[len] = '\0';

	wpa_ctrl_close(ctrl);

	QString ifaces(buf);
	QStringList lines = ifaces.split(QRegExp("\\n"));
	for (QStringList::Iterator it = lines.begin();
	     it != lines.end(); it++) {
		QStringList arg = (*it).split(QChar('\t'));
		if (arg.size() < 3)
			continue;
		QTreeWidgetItem *item = new QTreeWidgetItem(interfaceWidget);
		if (!item)
			break;

		item->setText(0, arg[0]);
		item->setText(1, arg[1]);
		item->setText(2, arg[2]);
	}

	interfaceWidget->resizeColumnToContents(0);
	interfaceWidget->resizeColumnToContents(1);
	interfaceWidget->resizeColumnToContents(2);
#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */
}


#ifdef CONFIG_NATIVE_WINDOWS
bool AddInterface::addRegistryInterface(const QString &ifname)
{
	HKEY hk, ihk;
	LONG ret;
	int id, tmp;
	TCHAR name[10];
	DWORD val, i;

	ret = RegOpenKeyEx(WPA_KEY_ROOT, WPA_KEY_PREFIX TEXT("\\interfaces"),
			   0, KEY_ENUMERATE_SUB_KEYS | KEY_CREATE_SUB_KEY,
			   &hk);
	if (ret != ERROR_SUCCESS)
		return false;

	id = -1;

	for (i = 0; ; i++) {
		TCHAR name[255];
		DWORD namelen;

		namelen = 255;
		ret = RegEnumKeyEx(hk, i, name, &namelen, NULL, NULL, NULL,
				   NULL);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS)
			break;

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = '\0';

#ifdef UNICODE
		QString s((QChar *) name, namelen);
#else /* UNICODE */
		QString s(name);
#endif /* UNICODE */
		tmp = s.toInt();
		if (tmp > id)
			id = tmp;
	}

	id += 1;

#ifdef UNICODE
	wsprintf(name, L"%04d", id);
#else /* UNICODE */
	os_snprintf(name, sizeof(name), "%04d", id);
#endif /* UNICODE */
	ret = RegCreateKeyEx(hk, name, 0, NULL, 0, KEY_WRITE, NULL, &ihk,
			     NULL);
	RegCloseKey(hk);
	if (ret != ERROR_SUCCESS)
		return false;

#ifdef UNICODE
	RegSetValueEx(ihk, TEXT("adapter"), 0, REG_SZ,
		      (LPBYTE) ifname.unicode(),
		      (ifname.length() + 1) * sizeof(TCHAR));

#else /* UNICODE */
	RegSetValueEx(ihk, TEXT("adapter"), 0, REG_SZ,
		      (LPBYTE) ifname.toLocal8Bit(), ifname.length() + 1);
#endif /* UNICODE */
	RegSetValueEx(ihk, TEXT("config"), 0, REG_SZ,
		      (LPBYTE) TEXT("default"), 8 * sizeof(TCHAR));
	RegSetValueEx(ihk, TEXT("ctrl_interface"), 0, REG_SZ,
		      (LPBYTE) TEXT(""), 1 * sizeof(TCHAR));
	val = 1;
	RegSetValueEx(ihk, TEXT("skip_on_error"), 0, REG_DWORD, (LPBYTE) &val,
		      sizeof(val));

	RegCloseKey(ihk);
	return true;
}
#endif /* CONFIG_NATIVE_WINDOWS */


void AddInterface::interfaceSelected(QTreeWidgetItem *sel)
{
	if (!sel)
		return;

#ifdef CONFIG_CTRL_IFACE_NAMED_PIPE
	struct wpa_ctrl *ctrl;
	int ret;
	char buf[20], cmd[256];
	size_t len;

	/*
	 * INTERFACE_ADD <ifname>TAB<confname>TAB<driver>TAB<ctrl_interface>TAB
	 * <driver_param>TAB<bridge_name>
	 */
	snprintf(cmd, sizeof(cmd),
		 "INTERFACE_ADD %s\t%s\t%s\t%s\t%s\t%s",
		 sel->text(1).toLocal8Bit().constData(),
		 "default",
		 sel->text(0).toLocal8Bit().constData(),
		 "yes", "", "");
	cmd[sizeof(cmd) - 1] = '\0';

	ctrl = wpa_ctrl_open(NULL);
	if (ctrl == NULL)
		return;

	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, &len, NULL);
	wpa_ctrl_close(ctrl);

	if (ret < 0) {
		QMessageBox::warning(this, "wpa_gui",
				     tr("Add interface command could not be "
					"completed."));
		return;
	}

	buf[len] = '\0';
	if (buf[0] != 'O' || buf[1] != 'K') {
		QMessageBox::warning(this, "wpa_gui",
				     tr("Failed to add the interface."));
		return;
	}

#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */

#ifdef CONFIG_NATIVE_WINDOWS
	if (!addRegistryInterface(sel->text(1))) {
		QMessageBox::information(this, "wpa_gui",
					 tr("Failed to add the interface into "
					    "registry."));
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	wpagui->selectAdapter(sel->text(1));
	close();
}
