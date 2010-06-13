/*
 * WPA Supplicant / UNIX domain socket -based control interface
 * Copyright (c) 2004-2005, Jouni Malinen <j@w1.fi>
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

#include "includes.h"
#include <sys/un.h>
#include <sys/stat.h>
#include <grp.h>
#include <stddef.h>

#include "common.h"
#include "eloop.h"
#include "config.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface.h"

/* Per-interface ctrl_iface */

/**
 * struct wpa_ctrl_dst - Internal data structure of control interface monitors
 *
 * This structure is used to store information about registered control
 * interface monitors into struct wpa_supplicant. This data is private to
 * ctrl_iface_unix.c and should not be touched directly from other files.
 */
struct wpa_ctrl_dst {
	struct wpa_ctrl_dst *next;
	struct sockaddr_un addr;
	socklen_t addrlen;
	int debug_level;
	int errors;
};


struct ctrl_iface_priv {
	struct wpa_supplicant *wpa_s;
	int sock;
	struct wpa_ctrl_dst *ctrl_dst;
};


static void wpa_supplicant_ctrl_iface_send(struct ctrl_iface_priv *priv,
					   int level, const char *buf,
					   size_t len);


static int wpa_supplicant_ctrl_iface_attach(struct ctrl_iface_priv *priv,
					    struct sockaddr_un *from,
					    socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst;

	dst = os_zalloc(sizeof(*dst));
	if (dst == NULL)
		return -1;
	os_memcpy(&dst->addr, from, sizeof(struct sockaddr_un));
	dst->addrlen = fromlen;
	dst->debug_level = MSG_INFO;
	dst->next = priv->ctrl_dst;
	priv->ctrl_dst = dst;
	wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor attached",
		    (u8 *) from->sun_path,
		    fromlen - offsetof(struct sockaddr_un, sun_path));
	return 0;
}


static int wpa_supplicant_ctrl_iface_detach(struct ctrl_iface_priv *priv,
					    struct sockaddr_un *from,
					    socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst, *prev = NULL;

	dst = priv->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			if (prev == NULL)
				priv->ctrl_dst = dst->next;
			else
				prev->next = dst->next;
			os_free(dst);
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor detached",
				    (u8 *) from->sun_path,
				    fromlen -
				    offsetof(struct sockaddr_un, sun_path));
			return 0;
		}
		prev = dst;
		dst = dst->next;
	}
	return -1;
}


static int wpa_supplicant_ctrl_iface_level(struct ctrl_iface_priv *priv,
					   struct sockaddr_un *from,
					   socklen_t fromlen,
					   char *level)
{
	struct wpa_ctrl_dst *dst;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE LEVEL %s", level);

	dst = priv->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE changed monitor "
				    "level", (u8 *) from->sun_path,
				    fromlen -
				    offsetof(struct sockaddr_un, sun_path));
			dst->debug_level = atoi(level);
			return 0;
		}
		dst = dst->next;
	}

	return -1;
}


static void wpa_supplicant_ctrl_iface_receive(int sock, void *eloop_ctx,
					      void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct ctrl_iface_priv *priv = sock_ctx;
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply = NULL;
	size_t reply_len = 0;
	int new_attached = 0;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';

	if (os_strcmp(buf, "ATTACH") == 0) {
		if (wpa_supplicant_ctrl_iface_attach(priv, &from, fromlen))
			reply_len = 1;
		else {
			new_attached = 1;
			reply_len = 2;
		}
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (wpa_supplicant_ctrl_iface_detach(priv, &from, fromlen))
			reply_len = 1;
		else
			reply_len = 2;
	} else if (os_strncmp(buf, "LEVEL ", 6) == 0) {
		if (wpa_supplicant_ctrl_iface_level(priv, &from, fromlen,
						    buf + 6))
			reply_len = 1;
		else
			reply_len = 2;
	} else {
		reply = wpa_supplicant_ctrl_iface_process(wpa_s, buf,
							  &reply_len);
	}

	if (reply) {
		sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
		       fromlen);
		os_free(reply);
	} else if (reply_len == 1) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
	} else if (reply_len == 2) {
		sendto(sock, "OK\n", 3, 0, (struct sockaddr *) &from,
		       fromlen);
	}

	if (new_attached)
		eapol_sm_notify_ctrl_attached(wpa_s->eapol);
}


static char * wpa_supplicant_ctrl_iface_path(struct wpa_supplicant *wpa_s)
{
	char *buf;
	size_t len;
	char *pbuf, *dir = NULL, *gid_str = NULL;
	int res;

	if (wpa_s->conf->ctrl_interface == NULL)
		return NULL;

	pbuf = os_strdup(wpa_s->conf->ctrl_interface);
	if (pbuf == NULL)
		return NULL;
	if (os_strncmp(pbuf, "DIR=", 4) == 0) {
		dir = pbuf + 4;
		gid_str = os_strstr(dir, " GROUP=");
		if (gid_str) {
			*gid_str = '\0';
			gid_str += 7;
		}
	} else
		dir = pbuf;

	len = os_strlen(dir) + os_strlen(wpa_s->ifname) + 2;
	buf = os_malloc(len);
	if (buf == NULL) {
		os_free(pbuf);
		return NULL;
	}

	res = os_snprintf(buf, len, "%s/%s", dir, wpa_s->ifname);
	if (res < 0 || (size_t) res >= len) {
		os_free(pbuf);
		os_free(buf);
		return NULL;
	}
#ifdef __CYGWIN__
	{
		/* Windows/WinPcap uses interface names that are not suitable
		 * as a file name - convert invalid chars to underscores */
		char *pos = buf;
		while (*pos) {
			if (*pos == '\\')
				*pos = '_';
			pos++;
		}
	}
#endif /* __CYGWIN__ */
	os_free(pbuf);
	return buf;
}


static void wpa_supplicant_ctrl_iface_msg_cb(void *ctx, int level,
					     const char *txt, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s == NULL || wpa_s->ctrl_iface == NULL)
		return;
	wpa_supplicant_ctrl_iface_send(wpa_s->ctrl_iface, level, txt, len);
}


struct ctrl_iface_priv *
wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	struct ctrl_iface_priv *priv;
	struct sockaddr_un addr;
	char *fname = NULL;
	gid_t gid = 0;
	int gid_set = 0;
	char *buf, *dir = NULL, *gid_str = NULL;
	struct group *grp;
	char *endp;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;
	priv->wpa_s = wpa_s;
	priv->sock = -1;

	if (wpa_s->conf->ctrl_interface == NULL)
		return priv;

	buf = os_strdup(wpa_s->conf->ctrl_interface);
	if (buf == NULL)
		goto fail;
	if (os_strncmp(buf, "DIR=", 4) == 0) {
		dir = buf + 4;
		gid_str = os_strstr(dir, " GROUP=");
		if (gid_str) {
			*gid_str = '\0';
			gid_str += 7;
		}
	} else {
		dir = buf;
		gid_str = wpa_s->conf->ctrl_interface_group;
	}

	if (mkdir(dir, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			perror("mkdir[ctrl_interface]");
			goto fail;
		}
	}

	if (gid_str) {
		grp = getgrnam(gid_str);
		if (grp) {
			gid = grp->gr_gid;
			gid_set = 1;
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d"
				   " (from group name '%s')",
				   (int) gid, gid_str);
		} else {
			/* Group name not found - try to parse this as gid */
			gid = strtol(gid_str, &endp, 10);
			if (*gid_str == '\0' || *endp != '\0') {
				wpa_printf(MSG_ERROR, "CTRL: Invalid group "
					   "'%s'", gid_str);
				goto fail;
			}
			gid_set = 1;
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
				   (int) gid);
		}
	}

	if (gid_set && chown(dir, -1, gid) < 0) {
		perror("chown[ctrl_interface]");
		goto fail;
	}

	/* Make sure the group can enter and read the directory */
	if (gid_set &&
	    chmod(dir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP) < 0) {
		wpa_printf(MSG_ERROR, "CTRL: chmod[ctrl_interface]: %s",
			   strerror(errno));
		goto fail;
	}

	if (os_strlen(dir) + 1 + os_strlen(wpa_s->ifname) >=
	    sizeof(addr.sun_path)) {
		wpa_printf(MSG_ERROR, "ctrl_iface path limit exceeded");
		goto fail;
	}

	priv->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (priv->sock < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = wpa_supplicant_ctrl_iface_path(wpa_s);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(priv->sock, (struct sockaddr *) &addr,
			    sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				perror("unlink[ctrl_iface]");
				wpa_printf(MSG_ERROR, "Could not unlink "
					   "existing ctrl_iface socket '%s'",
					   fname);
				goto fail;
			}
			if (bind(priv->sock, (struct sockaddr *) &addr,
				 sizeof(addr)) < 0) {
				perror("bind(PF_UNIX)");
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (gid_set && chown(fname, -1, gid) < 0) {
		perror("chown[ctrl_interface/ifname]");
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		perror("chmod[ctrl_interface/ifname]");
		goto fail;
	}
	os_free(fname);

	eloop_register_read_sock(priv->sock, wpa_supplicant_ctrl_iface_receive,
				 wpa_s, priv);
	wpa_msg_register_cb(wpa_supplicant_ctrl_iface_msg_cb);

	os_free(buf);
	return priv;

fail:
	if (priv->sock >= 0)
		close(priv->sock);
	os_free(priv);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	os_free(buf);
	return NULL;
}


void wpa_supplicant_ctrl_iface_deinit(struct ctrl_iface_priv *priv)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (priv->sock > -1) {
		char *fname;
		char *buf, *dir = NULL, *gid_str = NULL;
		eloop_unregister_read_sock(priv->sock);
		if (priv->ctrl_dst) {
			/*
			 * Wait a second before closing the control socket if
			 * there are any attached monitors in order to allow
			 * them to receive any pending messages.
			 */
			wpa_printf(MSG_DEBUG, "CTRL_IFACE wait for attached "
				   "monitors to receive messages");
			os_sleep(1, 0);
		}
		close(priv->sock);
		priv->sock = -1;
		fname = wpa_supplicant_ctrl_iface_path(priv->wpa_s);
		if (fname) {
			unlink(fname);
			os_free(fname);
		}

		buf = os_strdup(priv->wpa_s->conf->ctrl_interface);
		if (buf == NULL)
			goto free_dst;
		if (os_strncmp(buf, "DIR=", 4) == 0) {
			dir = buf + 4;
			gid_str = os_strstr(dir, " GROUP=");
			if (gid_str) {
				*gid_str = '\0';
				gid_str += 7;
			}
		} else
			dir = buf;

		if (rmdir(dir) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				perror("rmdir[ctrl_interface]");
			}
		}
		os_free(buf);
	}

free_dst:
	dst = priv->ctrl_dst;
	while (dst) {
		prev = dst;
		dst = dst->next;
		os_free(prev);
	}
	os_free(priv);
}


/**
 * wpa_supplicant_ctrl_iface_send - Send a control interface packet to monitors
 * @priv: Pointer to private data from wpa_supplicant_ctrl_iface_init()
 * @level: Priority level of the message
 * @buf: Message data
 * @len: Message length
 *
 * Send a packet to all monitor programs attached to the control interface.
 */
static void wpa_supplicant_ctrl_iface_send(struct ctrl_iface_priv *priv,
					   int level, const char *buf,
					   size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	char levelstr[10];
	int idx, res;
	struct msghdr msg;
	struct iovec io[2];

	dst = priv->ctrl_dst;
	if (priv->sock < 0 || dst == NULL)
		return;

	res = os_snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	if (res < 0 || (size_t) res >= sizeof(levelstr))
		return;
	io[0].iov_base = levelstr;
	io[0].iov_len = os_strlen(levelstr);
	io[1].iov_base = (char *) buf;
	io[1].iov_len = len;
	os_memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = 2;

	idx = 0;
	while (dst) {
		next = dst->next;
		if (level >= dst->debug_level) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor send",
				    (u8 *) dst->addr.sun_path, dst->addrlen -
				    offsetof(struct sockaddr_un, sun_path));
			msg.msg_name = (void *) &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(priv->sock, &msg, 0) < 0) {
				int _errno = errno;
				wpa_printf(MSG_INFO, "CTRL_IFACE monitor[%d]: "
					   "%d - %s",
					   idx, errno, strerror(errno));
				dst->errors++;
				if (dst->errors > 10 || _errno == ENOENT) {
					wpa_supplicant_ctrl_iface_detach(
						priv, &dst->addr,
						dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
		dst = next;
	}
}


void wpa_supplicant_ctrl_iface_wait(struct ctrl_iface_priv *priv)
{
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);

	for (;;) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE - %s - wait for monitor to "
			   "attach", priv->wpa_s->ifname);
		eloop_wait_for_read_sock(priv->sock);

		res = recvfrom(priv->sock, buf, sizeof(buf) - 1, 0,
			       (struct sockaddr *) &from, &fromlen);
		if (res < 0) {
			perror("recvfrom(ctrl_iface)");
			continue;
		}
		buf[res] = '\0';

		if (os_strcmp(buf, "ATTACH") == 0) {
			/* handle ATTACH signal of first monitor interface */
			if (!wpa_supplicant_ctrl_iface_attach(priv, &from,
							      fromlen)) {
				sendto(priv->sock, "OK\n", 3, 0,
				       (struct sockaddr *) &from, fromlen);
				/* OK to continue */
				return;
			} else {
				sendto(priv->sock, "FAIL\n", 5, 0,
				       (struct sockaddr *) &from, fromlen);
			}
		} else {
			/* return FAIL for all other signals */
			sendto(priv->sock, "FAIL\n", 5, 0,
			       (struct sockaddr *) &from, fromlen);
		}
	}
}


/* Global ctrl_iface */

struct ctrl_iface_global_priv {
	struct wpa_global *global;
	int sock;
};


static void wpa_supplicant_global_ctrl_iface_receive(int sock, void *eloop_ctx,
						     void *sock_ctx)
{
	struct wpa_global *global = eloop_ctx;
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	size_t reply_len;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';

	reply = wpa_supplicant_global_ctrl_iface_process(global, buf,
							 &reply_len);

	if (reply) {
		sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
		       fromlen);
		os_free(reply);
	} else if (reply_len) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
	}
}


struct ctrl_iface_global_priv *
wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	struct ctrl_iface_global_priv *priv;
	struct sockaddr_un addr;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;
	priv->global = global;
	priv->sock = -1;

	if (global->params.ctrl_interface == NULL)
		return priv;

	wpa_printf(MSG_DEBUG, "Global control interface '%s'",
		   global->params.ctrl_interface);

	priv->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (priv->sock < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	os_strlcpy(addr.sun_path, global->params.ctrl_interface,
		   sizeof(addr.sun_path));
	if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind(PF_UNIX)");
		if (connect(priv->sock, (struct sockaddr *) &addr,
			    sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(global->params.ctrl_interface) < 0) {
				perror("unlink[ctrl_iface]");
				wpa_printf(MSG_ERROR, "Could not unlink "
					   "existing ctrl_iface socket '%s'",
					   global->params.ctrl_interface);
				goto fail;
			}
			if (bind(priv->sock, (struct sockaddr *) &addr,
				 sizeof(addr)) < 0) {
				perror("bind(PF_UNIX)");
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'",
				   global->params.ctrl_interface);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore",
				   global->params.ctrl_interface);
			goto fail;
		}
	}

	eloop_register_read_sock(priv->sock,
				 wpa_supplicant_global_ctrl_iface_receive,
				 global, NULL);

	return priv;

fail:
	if (priv->sock >= 0)
		close(priv->sock);
	os_free(priv);
	return NULL;
}


void
wpa_supplicant_global_ctrl_iface_deinit(struct ctrl_iface_global_priv *priv)
{
	if (priv->sock >= 0) {
		eloop_unregister_read_sock(priv->sock);
		close(priv->sock);
	}
	if (priv->global->params.ctrl_interface)
		unlink(priv->global->params.ctrl_interface);
	os_free(priv);
}
