/*
 * WPA Supplicant - wpa_supplicant control interface library
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <sys/socket.h>
#include <sys/un.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "wpa_ctrl.h"
#ifdef CONFIG_NATIVE_WINDOWS
#include "common.h"
#endif /* CONFIG_NATIVE_WINDOWS */


struct wpa_ctrl {
	int s;
#ifdef CONFIG_CTRL_IFACE_UDP
	struct sockaddr_in local;
	struct sockaddr_in dest;
#else /* CONFIG_CTRL_IFACE_UDP */
	struct sockaddr_un local;
	struct sockaddr_un dest;
#endif /* CONFIG_CTRL_IFACE_UDP */
};


struct wpa_ctrl * wpa_ctrl_open(const char *ctrl_path)
{
	struct wpa_ctrl *ctrl;
#ifndef CONFIG_CTRL_IFACE_UDP
	static int counter = 0;
#endif /* CONFIG_CTRL_IFACE_UDP */

	ctrl = malloc(sizeof(*ctrl));
	if (ctrl == NULL)
		return NULL;
	memset(ctrl, 0, sizeof(*ctrl));

#ifdef CONFIG_CTRL_IFACE_UDP
	ctrl->s = socket(PF_INET, SOCK_DGRAM, 0);
	if (ctrl->s < 0) {
		perror("socket");
		free(ctrl);
		return NULL;
	}

	ctrl->local.sin_family = AF_INET;
	ctrl->local.sin_addr.s_addr = htonl((127 << 24) | 1);
	if (bind(ctrl->s, (struct sockaddr *) &ctrl->local,
		 sizeof(ctrl->local)) < 0) {
		close(ctrl->s);
		free(ctrl);
		return NULL;
	}

	ctrl->dest.sin_family = AF_INET;
	ctrl->dest.sin_addr.s_addr = htonl((127 << 24) | 1);
	ctrl->dest.sin_port = htons(9877);
	if (connect(ctrl->s, (struct sockaddr *) &ctrl->dest,
		    sizeof(ctrl->dest)) < 0) {
		perror("connect");
		close(ctrl->s);
		free(ctrl);
		return NULL;
	}
#else /* CONFIG_CTRL_IFACE_UDP */
	ctrl->s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (ctrl->s < 0) {
		free(ctrl);
		return NULL;
	}

	ctrl->local.sun_family = AF_UNIX;
	snprintf(ctrl->local.sun_path, sizeof(ctrl->local.sun_path),
		 "/tmp/wpa_ctrl_%d-%d", getpid(), counter++);
	if (bind(ctrl->s, (struct sockaddr *) &ctrl->local,
		    sizeof(ctrl->local.sun_family) +
		 strlen(ctrl->local.sun_path)) < 0) {
		close(ctrl->s);
		free(ctrl);
		return NULL;
	}

	ctrl->dest.sun_family = AF_UNIX;
	strncpy(ctrl->dest.sun_path, ctrl_path, sizeof(ctrl->dest.sun_path));
	if (connect(ctrl->s, (struct sockaddr *) &ctrl->dest,
		    sizeof(ctrl->dest.sun_family) +
		    strlen(ctrl->dest.sun_path)) < 0) {
		close(ctrl->s);
		unlink(ctrl->local.sun_path);
		free(ctrl);
		return NULL;
	}
#endif /* CONFIG_CTRL_IFACE_UDP */

	return ctrl;
}


void wpa_ctrl_close(struct wpa_ctrl *ctrl)
{
#ifndef CONFIG_CTRL_IFACE_UDP
	unlink(ctrl->local.sun_path);
#endif /* CONFIG_CTRL_IFACE_UDP */
	close(ctrl->s);
	free(ctrl);
}


int wpa_ctrl_request(struct wpa_ctrl *ctrl, char *cmd, size_t cmd_len,
		     char *reply, size_t *reply_len,
		     void (*msg_cb)(char *msg, size_t len))
{
	struct timeval tv;
	int res;
	fd_set rfds;

	if (send(ctrl->s, cmd, cmd_len, 0) < 0)
		return -1;

	for (;;) {
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(ctrl->s, &rfds);
		res = select(ctrl->s + 1, &rfds, NULL, NULL, &tv);
		if (FD_ISSET(ctrl->s, &rfds)) {
			res = recv(ctrl->s, reply, *reply_len, 0);
			if (res < 0)
				return res;
			if (res > 0 && reply[0] == '<') {
				/* This is an unsolicited message from
				 * wpa_supplicant, not the reply to the
				 * request. Use msg_cb to report this to the
				 * caller. */
				if (msg_cb) {
					/* Make sure the message is nul
					 * terminated. */
					if (res == *reply_len)
						res = (*reply_len) - 1;
					reply[res] = '\0';
					msg_cb(reply, res);
				}
				continue;
			}
			*reply_len = res;
			break;
		} else {
			return -2;
		}
	}
	return 0;
}


static int wpa_ctrl_attach_helper(struct wpa_ctrl *ctrl, int attach)
{
	char buf[10];
	int ret;
	size_t len = 10;

	ret = wpa_ctrl_request(ctrl, attach ? "ATTACH" : "DETACH", 6,
			       buf, &len, NULL);
	if (ret < 0)
		return ret;
	if (len == 3 && memcmp(buf, "OK\n", 3) == 0)
		return 0;
	return -1;
}


int wpa_ctrl_attach(struct wpa_ctrl *ctrl)
{
	return wpa_ctrl_attach_helper(ctrl, 1);
}


int wpa_ctrl_detach(struct wpa_ctrl *ctrl)
{
	return wpa_ctrl_attach_helper(ctrl, 0);
}


int wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len)
{
	int res;

	res = recv(ctrl->s, reply, *reply_len, 0);
	if (res < 0)
		return res;
	*reply_len = res;
	return 0;
}


int wpa_ctrl_pending(struct wpa_ctrl *ctrl)
{
	struct timeval tv;
	int res;
	fd_set rfds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(ctrl->s, &rfds);
	res = select(ctrl->s + 1, &rfds, NULL, NULL, &tv);
	return FD_ISSET(ctrl->s, &rfds);
}


int wpa_ctrl_get_fd(struct wpa_ctrl *ctrl)
{
	return ctrl->s;
}
