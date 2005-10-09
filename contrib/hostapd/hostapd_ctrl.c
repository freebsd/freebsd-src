/*
 * hostapd - hostapd control interface library
 * Copyright (c) 2004, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include "hostapd_ctrl.h"


struct hostapd_ctrl {
	int s;
	struct sockaddr_un local;
	struct sockaddr_un dest;
};


struct hostapd_ctrl * hostapd_ctrl_open(const char *ctrl_path)
{
	struct hostapd_ctrl *ctrl;
	static int counter = 0;

	ctrl = malloc(sizeof(*ctrl));
	if (ctrl == NULL)
		return NULL;
	memset(ctrl, 0, sizeof(*ctrl));

	ctrl->s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (ctrl->s < 0) {
		free(ctrl);
		return NULL;
	}

	ctrl->local.sun_family = AF_UNIX;
	snprintf(ctrl->local.sun_path, sizeof(ctrl->local.sun_path),
		 "/tmp/hostapd_ctrl_%d-%d", getpid(), counter++);
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

	return ctrl;
}


void hostapd_ctrl_close(struct hostapd_ctrl *ctrl)
{
	unlink(ctrl->local.sun_path);
	close(ctrl->s);
	free(ctrl);
}


int hostapd_ctrl_request(struct hostapd_ctrl *ctrl, char *cmd, size_t cmd_len,
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


static int hostapd_ctrl_attach_helper(struct hostapd_ctrl *ctrl, int attach)
{
	char buf[10];
	int ret;
	size_t len = 10;

	ret = hostapd_ctrl_request(ctrl, attach ? "ATTACH" : "DETACH", 6,
			       buf, &len, NULL);
	if (ret < 0)
		return ret;
	if (len == 3 && memcmp(buf, "OK\n", 3) == 0)
		return 0;
	return -1;
}


int hostapd_ctrl_attach(struct hostapd_ctrl *ctrl)
{
	return hostapd_ctrl_attach_helper(ctrl, 1);
}


int hostapd_ctrl_detach(struct hostapd_ctrl *ctrl)
{
	return hostapd_ctrl_attach_helper(ctrl, 0);
}


int hostapd_ctrl_recv(struct hostapd_ctrl *ctrl, char *reply,
		      size_t *reply_len)
{
	int res;

	res = recv(ctrl->s, reply, *reply_len, 0);
	if (res < 0)
		return res;
	*reply_len = res;
	return 0;
}


int hostapd_ctrl_pending(struct hostapd_ctrl *ctrl)
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


int hostapd_ctrl_get_fd(struct hostapd_ctrl *ctrl)
{
	return ctrl->s;
}
