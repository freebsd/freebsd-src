/*
 * wpa_gui - Application startup
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

#ifdef CONFIG_NATIVE_WINDOWS
#include <winsock.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <QApplication>
#include "wpagui.h"


class WpaGuiApp : public QApplication
{
public:
	WpaGuiApp(int &argc, char **argv);

#ifndef QT_NO_SESSIONMANAGER
	virtual void saveState(QSessionManager &manager);
#endif

	WpaGui *w;
};

WpaGuiApp::WpaGuiApp(int &argc, char **argv) : QApplication(argc, argv)
{
}

#ifndef QT_NO_SESSIONMANAGER
void WpaGuiApp::saveState(QSessionManager &manager)
{
	QApplication::saveState(manager);
	w->saveState();
}
#endif


int main(int argc, char *argv[])
{
	WpaGuiApp app(argc, argv);
	WpaGui w(&app);
	int ret;

#ifdef CONFIG_NATIVE_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
		/* printf("Could not find a usable WinSock.dll\n"); */
		return -1;
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	app.w = &w;

	ret = app.exec();

#ifdef CONFIG_NATIVE_WINDOWS
	WSACleanup();
#endif /* CONFIG_NATIVE_WINDOWS */

	return ret;
}
