/*
 * Linux packet socket monitor
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#ifndef __APPLE__
#include <net/if.h>
#include <netpacket/packet.h>
#endif /* __APPLE__ */

#include "utils/common.h"
#include "utils/eloop.h"
#include "wlantest.h"


#ifdef __APPLE__

int monitor_init(struct wlantest *wt, const char *ifname)
{
	return -1;
}


int monitor_init_wired(struct wlantest *wt, const char *ifname)
{
	return -1;
}


void monitor_deinit(struct wlantest *wt)
{
}

#else /* __APPLE__ */

static void monitor_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wlantest *wt = eloop_ctx;
	u8 buf[3000];
	int len;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		wpa_printf(MSG_INFO, "recv(PACKET): %s", strerror(errno));
		return;
	}

	clear_notes(wt);
	os_free(wt->decrypted);
	wt->decrypted = NULL;
	write_pcap_captured(wt, buf, len);
	wlantest_process(wt, buf, len);
	write_pcapng_captured(wt, buf, len);
}


static void monitor_read_wired(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wlantest *wt = eloop_ctx;
	u8 buf[3000];
	int len;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		wpa_printf(MSG_INFO, "recv(PACKET): %s", strerror(errno));
		return;
	}

	wlantest_process_wired(wt, buf, len);
}


int monitor_init(struct wlantest *wt, const char *ifname)
{
	struct sockaddr_ll ll;

	os_memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = if_nametoindex(ifname);
	if (ll.sll_ifindex == 0) {
		wpa_printf(MSG_ERROR, "Monitor interface '%s' does not exist",
			   ifname);
		return -1;
	}

	wt->monitor_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (wt->monitor_sock < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_PACKET,SOCK_RAW): %s",
			   strerror(errno));
		return -1;
	}

	if (bind(wt->monitor_sock, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		wpa_printf(MSG_ERROR, "bind(PACKET): %s", strerror(errno));
		close(wt->monitor_sock);
		wt->monitor_sock = -1;
		return -1;
	}

	if (eloop_register_read_sock(wt->monitor_sock, monitor_read, wt, NULL))
	{
		wpa_printf(MSG_ERROR, "Could not register monitor read "
			   "socket");
		close(wt->monitor_sock);
		wt->monitor_sock = -1;
		return -1;
	}

	return 0;
}


int monitor_init_wired(struct wlantest *wt, const char *ifname)
{
	struct sockaddr_ll ll;

	os_memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = if_nametoindex(ifname);
	if (ll.sll_ifindex == 0) {
		wpa_printf(MSG_ERROR, "Monitor interface '%s' does not exist",
			   ifname);
		return -1;
	}

	wt->monitor_wired = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (wt->monitor_wired < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_PACKET,SOCK_RAW): %s",
			   strerror(errno));
		return -1;
	}

	if (bind(wt->monitor_wired, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		wpa_printf(MSG_ERROR, "bind(PACKET): %s", strerror(errno));
		close(wt->monitor_wired);
		wt->monitor_wired = -1;
		return -1;
	}

	if (eloop_register_read_sock(wt->monitor_wired, monitor_read_wired,
				     wt, NULL)) {
		wpa_printf(MSG_ERROR, "Could not register monitor read "
			   "socket");
		close(wt->monitor_wired);
		wt->monitor_wired = -1;
		return -1;
	}

	return 0;
}


void monitor_deinit(struct wlantest *wt)
{
	if (wt->monitor_sock >= 0) {
		eloop_unregister_read_sock(wt->monitor_sock);
		close(wt->monitor_sock);
		wt->monitor_sock = -1;
	}

	if (wt->monitor_wired >= 0) {
		eloop_unregister_read_sock(wt->monitor_wired);
		close(wt->monitor_wired);
		wt->monitor_wired = -1;
	}
}

#endif /* __APPLE__ */
