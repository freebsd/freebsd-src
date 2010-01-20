/*
 * WPA Supplicant - Windows/NDIS driver interface - event processing
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <string.h>
#include <sys/unistd.h>
#include <sys/types.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "common.h"
#include "driver.h"
#include "wpa_supplicant.h"
#include "l2_packet.h"
#include "eloop.h"
#include "wpa.h"

/* Keep this event processing in a separate file and without WinPcap headers to
 * avoid conflicts with some of the header files. */
struct _ADAPTER;
typedef struct _ADAPTER * LPADAPTER;
#include "driver_ndis.h"


void wpa_driver_ndis_event_connect(struct wpa_driver_ndis_data *drv);
void wpa_driver_ndis_event_disconnect(struct wpa_driver_ndis_data *drv);
void wpa_driver_ndis_event_media_specific(struct wpa_driver_ndis_data *drv,
					  const u8 *data, size_t data_len);


enum event_types { EVENT_CONNECT, EVENT_DISCONNECT,
		   EVENT_MEDIA_SPECIFIC };

/* Event data:
 * enum event_types (as int, i.e., 4 octets)
 * InstanceName length (1 octet)
 * InstanceName (variable len)
 * data length (1 octet, optional)
 * data (variable len, optional)
 */


static void wpa_driver_ndis_event_cb(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wpa_driver_ndis_data *drv = eloop_ctx;
	u8 buf[512], *pos, *data = NULL;
	int res, i, desc_len;
	enum event_types type;
	unsigned char instance_len;
	char *instance;
	size_t data_len = 0;

	res = recv(sock, buf, sizeof(buf), 0);
	if (res < 0) {
		perror("wpa_driver_ndis_event_cb - recv");
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "NDIS: received event data", buf, res);
	if (res < sizeof(int) + 1)
		return;
	type = *((int *) buf);
	pos = buf + sizeof(int);
	wpa_printf(MSG_DEBUG, "NDIS: event - type %d", type);
	instance_len = *pos++;
	if (instance_len > buf + res - pos) {
		wpa_printf(MSG_DEBUG, "NDIS: event InstanceName overflow");
		return;
	}
	instance = pos;
	pos += instance_len;
	wpa_hexdump_ascii(MSG_MSGDUMP, "NDIS: event InstanceName",
			  instance, instance_len);

	if (buf + res - pos > 1) {
		data_len = *pos++;
		if (data_len > buf + res - pos) {
			wpa_printf(MSG_DEBUG, "NDIS: event data overflow");
			return;
		}
		data = pos;
		wpa_hexdump(MSG_MSGDUMP, "NDIS: event data", data, data_len);
	}

	if (drv->adapter_desc) {
		desc_len = strlen(drv->adapter_desc);
		if (instance_len < desc_len ||
		    strncmp(drv->adapter_desc, instance, desc_len)) {
			wpa_printf(MSG_DEBUG, "NDIS: ignored event for "
				   "another adapter");
			return;
		}

		/* InstanceName:
		 * <driver desc> #<num>
		 * <driver desc> #<num> - <intermediate drv name> Miniport
		 */
		for (i = desc_len + 1; i < instance_len; i++) {
			if (instance[i] == '-') {
				wpa_printf(MSG_DEBUG, "NDIS: ignored event "
					   "for intermediate miniport");
				return;
			}
		}
	}

	switch (type) {
	case EVENT_CONNECT:
		wpa_driver_ndis_event_connect(drv);
		break;
	case EVENT_DISCONNECT:
		wpa_driver_ndis_event_disconnect(drv);
		break;
	case EVENT_MEDIA_SPECIFIC:
		wpa_driver_ndis_event_media_specific(drv, data, data_len);
		break;
	}
}


int wpa_driver_register_event_cb(struct wpa_driver_ndis_data *drv)
{
	struct sockaddr_in addr;

	drv->event_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->event_sock < 0) {
		perror("socket");
		return -1;
	}

	/* These events are received from an external program, ndis_events,
	 * which is converting WMI events to more "UNIX-like" input for
	 * wpa_supplicant, i.e., UDP packets that can be received through the
	 * eloop mechanism. */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((127 << 24) | 1);
	addr.sin_port = htons(9876);
	if (bind(drv->event_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
	{
		perror("bind");
		close(drv->event_sock);
		drv->event_sock = -1;
		return -1;
	}

	eloop_register_read_sock(drv->event_sock, wpa_driver_ndis_event_cb,
				 drv, NULL);

	return 0;
}
