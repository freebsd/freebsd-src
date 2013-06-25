/*
 * WPA Supplicant / WinMain() function for Windows-based applications
 * Copyright (c) 2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "wpa_supplicant_i.h"

#ifdef _WIN32_WCE
#define CMDLINE LPWSTR
#else /* _WIN32_WCE */
#define CMDLINE LPSTR
#endif /* _WIN32_WCE */


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		   CMDLINE lpCmdLine, int nShowCmd)
{
	int i;
	struct wpa_interface *ifaces, *iface;
	int iface_count, exitcode = -1;
	struct wpa_params params;
	struct wpa_global *global;

	if (os_program_init())
		return -1;

	os_memset(&params, 0, sizeof(params));
	params.wpa_debug_level = MSG_MSGDUMP;
	params.wpa_debug_file_path = "\\Temp\\wpa_supplicant-log.txt";
	params.wpa_debug_show_keys = 1;

	iface = ifaces = os_zalloc(sizeof(struct wpa_interface));
	if (ifaces == NULL)
		return -1;
	iface_count = 1;

	iface->confname = "default";
	iface->driver = "ndis";
	iface->ifname = "";

	exitcode = 0;
	global = wpa_supplicant_init(&params);
	if (global == NULL) {
		printf("Failed to initialize wpa_supplicant\n");
		exitcode = -1;
	}

	for (i = 0; exitcode == 0 && i < iface_count; i++) {
		if ((ifaces[i].confname == NULL &&
		     ifaces[i].ctrl_interface == NULL) ||
		    ifaces[i].ifname == NULL) {
			if (iface_count == 1 && (params.ctrl_interface ||
						 params.dbus_ctrl_interface))
				break;
			exitcode = -1;
			break;
		}
		if (wpa_supplicant_add_iface(global, &ifaces[i]) == NULL)
			exitcode = -1;
	}

	if (exitcode == 0)
		exitcode = wpa_supplicant_run(global);

	wpa_supplicant_deinit(global);

	os_free(ifaces);

	os_program_deinit();

	return exitcode;
}
