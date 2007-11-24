/*
 * WPA Supplicant / main() function for UNIX like OSes and MinGW
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "common.h"
#include "wpa_supplicant_i.h"


extern const char *wpa_supplicant_version;
extern const char *wpa_supplicant_license;
#ifndef CONFIG_NO_STDOUT_DEBUG
extern const char *wpa_supplicant_full_license;
#endif /* CONFIG_NO_STDOUT_DEBUG */

extern struct wpa_driver_ops *wpa_supplicant_drivers[];


static void usage(void)
{
	int i;
	printf("%s\n\n%s\n"
	       "usage:\n"
	       "  wpa_supplicant [-BddehLqqvwW] [-P<pid file>] "
	       "[-g<global ctrl>] \\\n"
	       "        -i<ifname> -c<config file> [-C<ctrl>] [-D<driver>] "
	       "[-p<driver_param>] \\\n"
	       "        [-N -i<ifname> -c<conf> [-C<ctrl>] [-D<driver>] "
	       "[-p<driver_param>] ...]\n"
	       "\n"
	       "drivers:\n",
	       wpa_supplicant_version, wpa_supplicant_license);

	for (i = 0; wpa_supplicant_drivers[i]; i++) {
		printf("  %s = %s\n",
		       wpa_supplicant_drivers[i]->name,
		       wpa_supplicant_drivers[i]->desc);
	}

#ifndef CONFIG_NO_STDOUT_DEBUG
	printf("options:\n"
	       "  -B = run daemon in the background\n"
	       "  -c = Configuration file\n"
	       "  -C = ctrl_interface parameter (only used if -c is not)\n"
	       "  -i = interface name\n"
	       "  -d = increase debugging verbosity (-dd even more)\n"
	       "  -D = driver name\n"
	       "  -g = global ctrl_interface\n"
	       "  -K = include keys (passwords, etc.) in debug output\n"
	       "  -t = include timestamp in debug messages\n"
	       "  -h = show this help text\n"
	       "  -L = show license (GPL and BSD)\n"
	       "  -p = driver parameters\n"
	       "  -P = PID file\n"
	       "  -q = decrease debugging verbosity (-qq even less)\n"
	       "  -v = show version\n"
	       "  -w = wait for interface to be added, if needed\n"
	       "  -W = wait for a control interface monitor before starting\n"
	       "  -N = start describing new interface\n");

	printf("example:\n"
	       "  wpa_supplicant -Dwext -iwlan0 -c/etc/wpa_supplicant.conf\n");
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


static void license(void)
{
#ifndef CONFIG_NO_STDOUT_DEBUG
	printf("%s\n\n%s\n",
	       wpa_supplicant_version, wpa_supplicant_full_license);
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


static void wpa_supplicant_fd_workaround(void)
{
	int s, i;
	/* When started from pcmcia-cs scripts, wpa_supplicant might start with
	 * fd 0, 1, and 2 closed. This will cause some issues because many
	 * places in wpa_supplicant are still printing out to stdout. As a
	 * workaround, make sure that fd's 0, 1, and 2 are not used for other
	 * sockets. */
	for (i = 0; i < 3; i++) {
		s = open("/dev/null", O_RDWR);
		if (s > 2) {
			close(s);
			break;
		}
	}
}


int main(int argc, char *argv[])
{
	int c, i;
	struct wpa_interface *ifaces, *iface;
	int iface_count, exitcode;
	struct wpa_params params;
	struct wpa_global *global;

#ifdef CONFIG_NATIVE_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
		printf("Could not find a usable WinSock.dll\n");
		return -1;
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	memset(&params, 0, sizeof(params));
	params.wpa_debug_level = MSG_INFO;

	iface = ifaces = malloc(sizeof(struct wpa_interface));
	if (ifaces == NULL)
		return -1;
	memset(iface, 0, sizeof(*iface));
	iface_count = 1;

	wpa_supplicant_fd_workaround();

	for (;;) {
		c = getopt(argc, argv, "Bc:C:D:dg:hi:KLNp:P:qtvwW");
		if (c < 0)
			break;
		switch (c) {
		case 'B':
			params.daemonize++;
			break;
		case 'c':
			iface->confname = optarg;
			break;
		case 'C':
			iface->ctrl_interface = optarg;
			break;
		case 'D':
			iface->driver = optarg;
			break;
		case 'd':
#ifdef CONFIG_NO_STDOUT_DEBUG
			printf("Debugging disabled with "
			       "CONFIG_NO_STDOUT_DEBUG=y build time "
			       "option.\n");
			return -1;
#else /* CONFIG_NO_STDOUT_DEBUG */
			params.wpa_debug_level--;
			break;
#endif /* CONFIG_NO_STDOUT_DEBUG */
		case 'g':
			params.ctrl_interface = optarg;
			break;
		case 'h':
			usage();
			return -1;
		case 'i':
			iface->ifname = optarg;
			break;
		case 'K':
			params.wpa_debug_show_keys++;
			break;
		case 'L':
			license();
			return -1;
		case 'p':
			iface->driver_param = optarg;
			break;
		case 'P':
			params.pid_file = rel2abs_path(optarg);
			break;
		case 'q':
			params.wpa_debug_level++;
			break;
		case 't':
			params.wpa_debug_timestamp++;
			break;
		case 'v':
			printf("%s\n", wpa_supplicant_version);
			return -1;
		case 'w':
			params.wait_for_interface++;
			break;
		case 'W':
			params.wait_for_monitor++;
			break;
		case 'N':
			iface_count++;
			iface = realloc(ifaces, iface_count *
					sizeof(struct wpa_interface));
			if (iface == NULL) {
				free(ifaces);
				return -1;
			}
			ifaces = iface;
			iface = &ifaces[iface_count - 1]; 
			memset(iface, 0, sizeof(*iface));
			break;
		default:
			usage();
			return -1;
		}
	}

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
			if (iface_count == 1 && params.ctrl_interface)
				break;
			usage();
			return -1;
		}
		if (wpa_supplicant_add_iface(global, &ifaces[i]) == NULL)
			exitcode = -1;
	}

	if (exitcode == 0)
		exitcode = wpa_supplicant_run(global);

	wpa_supplicant_deinit(global);

	free(ifaces);
	free(params.pid_file);

#ifdef CONFIG_NATIVE_WINDOWS
	WSACleanup();
#endif /* CONFIG_NATIVE_WINDOWS */

	return exitcode;
}
