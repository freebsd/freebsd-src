/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / UNIX domain socket -based control interface
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
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <netinet/in.h>

#include "hostapd.h"
#include "eloop.h"
#include "config.h"
#include "eapol_sm.h"
#include "ieee802_1x.h"
#include "wpa.h"
#include "radius_client.h"
#include "ieee802_11.h"
#include "ctrl_iface.h"
#include "sta_info.h"


struct wpa_ctrl_dst {
	struct wpa_ctrl_dst *next;
	struct sockaddr_un addr;
	socklen_t addrlen;
	int debug_level;
	int errors;
};


static int hostapd_ctrl_iface_attach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst;

	dst = malloc(sizeof(*dst));
	if (dst == NULL)
		return -1;
	memset(dst, 0, sizeof(*dst));
	memcpy(&dst->addr, from, sizeof(struct sockaddr_un));
	dst->addrlen = fromlen;
	dst->debug_level = MSG_INFO;
	dst->next = hapd->ctrl_dst;
	hapd->ctrl_dst = dst;
	wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor attached",
		    (u8 *) from->sun_path, fromlen);
	return 0;
}


static int hostapd_ctrl_iface_detach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst, *prev = NULL;

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    memcmp(from->sun_path, dst->addr.sun_path, fromlen) == 0) {
			if (prev == NULL)
				hapd->ctrl_dst = dst->next;
			else
				prev->next = dst->next;
			free(dst);
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor detached",
				    (u8 *) from->sun_path, fromlen);
			return 0;
		}
		prev = dst;
		dst = dst->next;
	}
	return -1;
}


static int hostapd_ctrl_iface_level(struct hostapd_data *hapd,
				    struct sockaddr_un *from,
				    socklen_t fromlen,
				    char *level)
{
	struct wpa_ctrl_dst *dst;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE LEVEL %s", level);

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    memcmp(from->sun_path, dst->addr.sun_path, fromlen) == 0) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE changed monitor "
				    "level", (u8 *) from->sun_path, fromlen);
			dst->debug_level = atoi(level);
			return 0;
		}
		dst = dst->next;
	}

	return -1;
}


static int hostapd_ctrl_iface_sta_mib(struct hostapd_data *hapd,
				      struct sta_info *sta,
				      char *buf, size_t buflen)
{
	int len, res;

	if (sta == NULL) {
		return snprintf(buf, buflen, "FAIL\n");
	}

	len = 0;
	len += snprintf(buf + len, buflen - len, MACSTR "\n",
			MAC2STR(sta->addr));

	res = ieee802_11_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = wpa_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = ieee802_1x_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;

	return len;
}


static int hostapd_ctrl_iface_sta_first(struct hostapd_data *hapd,
					char *buf, size_t buflen)
{
	return hostapd_ctrl_iface_sta_mib(hapd, hapd->sta_list, buf, buflen);
}


static int hostapd_ctrl_iface_sta(struct hostapd_data *hapd,
				  const char *txtaddr,
				  char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];

	if (hwaddr_aton(txtaddr, addr))
		return snprintf(buf, buflen, "FAIL\n");
	return hostapd_ctrl_iface_sta_mib(hapd, ap_get_sta(hapd, addr),
					  buf, buflen);
}


static int hostapd_ctrl_iface_sta_next(struct hostapd_data *hapd,
				       const char *txtaddr,
				       char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	if (hwaddr_aton(txtaddr, addr) ||
	    (sta = ap_get_sta(hapd, addr)) == NULL)
		return snprintf(buf, buflen, "FAIL\n");
	return hostapd_ctrl_iface_sta_mib(hapd, sta->next, buf, buflen);
}


static void hostapd_ctrl_iface_receive(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	const int reply_size = 4096;
	int reply_len;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';
	wpa_hexdump_ascii(MSG_DEBUG, "RX ctrl_iface", (u8 *) buf, res);

	reply = malloc(reply_size);
	if (reply == NULL) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
		return;
	}

	memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (strcmp(buf, "PING") == 0) {
		memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (strcmp(buf, "MIB") == 0) {
		reply_len = ieee802_11_get_mib(hapd, reply, reply_size);
		if (reply_len >= 0) {
			res = wpa_get_mib(hapd, reply + reply_len,
					  reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = ieee802_1x_get_mib(hapd, reply + reply_len,
						 reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = radius_client_get_mib(hapd->radius,
						    reply + reply_len,
						    reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
	} else if (strcmp(buf, "STA-FIRST") == 0) {
		reply_len = hostapd_ctrl_iface_sta_first(hapd, reply,
							 reply_size);
	} else if (strncmp(buf, "STA ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_sta(hapd, buf + 4, reply,
						   reply_size);
	} else if (strncmp(buf, "STA-NEXT ", 9) == 0) {
		reply_len = hostapd_ctrl_iface_sta_next(hapd, buf + 9, reply,
							reply_size);
	} else if (strcmp(buf, "ATTACH") == 0) {
		if (hostapd_ctrl_iface_attach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (strcmp(buf, "DETACH") == 0) {
		if (hostapd_ctrl_iface_detach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (strncmp(buf, "LEVEL ", 6) == 0) {
		if (hostapd_ctrl_iface_level(hapd, &from, fromlen,
						    buf + 6))
			reply_len = -1;
	} else {
		memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}
	sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
	free(reply);
}


static char * hostapd_ctrl_iface_path(struct hostapd_data *hapd)
{
	char *buf;
	size_t len;

	if (hapd->conf->ctrl_interface == NULL)
		return NULL;

	len = strlen(hapd->conf->ctrl_interface) + strlen(hapd->conf->iface) +
		2;
	buf = malloc(len);
	if (buf == NULL)
		return NULL;

	snprintf(buf, len, "%s/%s",
		 hapd->conf->ctrl_interface, hapd->conf->iface);
	return buf;
}


int hostapd_ctrl_iface_init(struct hostapd_data *hapd)
{
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	hapd->ctrl_sock = -1;

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	if (mkdir(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			perror("mkdir[ctrl_interface]");
			goto fail;
		}
	}

	if (chown(hapd->conf->ctrl_interface, 0,
		  hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface]");
		return -1;
	}

	if (strlen(hapd->conf->ctrl_interface) + 1 + strlen(hapd->conf->iface)
	    >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	fname = hostapd_ctrl_iface_path(hapd);
	if (fname == NULL)
		goto fail;
	strncpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind(PF_UNIX)");
		goto fail;
	}

	if (chown(fname, 0, hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface/ifname]");
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		perror("chmod[ctrl_interface/ifname]");
		goto fail;
	}
	free(fname);

	hapd->ctrl_sock = s;
	eloop_register_read_sock(s, hostapd_ctrl_iface_receive, hapd,
				 NULL);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		free(fname);
	}
	return -1;
}


void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (hapd->ctrl_sock > -1) {
		char *fname;
		eloop_unregister_read_sock(hapd->ctrl_sock);
		close(hapd->ctrl_sock);
		hapd->ctrl_sock = -1;
		fname = hostapd_ctrl_iface_path(hapd);
		if (fname)
			unlink(fname);
		free(fname);

		if (rmdir(hapd->conf->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				perror("rmdir[ctrl_interface]");
			}
		}
	}

	dst = hapd->ctrl_dst;
	while (dst) {
		prev = dst;
		dst = dst->next;
		free(prev);
	}
}


void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
			     char *buf, size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	struct msghdr msg;
	int idx;
	struct iovec io[2];
	char levelstr[10];

	dst = hapd->ctrl_dst;
	if (hapd->ctrl_sock < 0 || dst == NULL)
		return;

	snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	io[0].iov_base = levelstr;
	io[0].iov_len = strlen(levelstr);
	io[1].iov_base = buf;
	io[1].iov_len = len;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = 2;

	idx = 0;
	while (dst) {
		next = dst->next;
		if (level >= dst->debug_level) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor send",
				    (u8 *) dst->addr.sun_path, dst->addrlen);
			msg.msg_name = &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(hapd->ctrl_sock, &msg, 0) < 0) {
				fprintf(stderr, "CTRL_IFACE monitor[%d]: ",
					idx);
				perror("sendmsg");
				dst->errors++;
				if (dst->errors > 10) {
					hostapd_ctrl_iface_detach(
						hapd, &dst->addr,
						dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
		dst = next;
	}
}
