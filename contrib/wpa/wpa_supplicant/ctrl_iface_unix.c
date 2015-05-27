/*
 * WPA Supplicant / UNIX domain socket -based control interface
 * Copyright (c) 2004-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/un.h>
#include <sys/stat.h>
#include <grp.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef ANDROID
#include <cutils/sockets.h>
#endif /* ANDROID */

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/list.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "config.h"
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
	struct dl_list list;
	struct sockaddr_un addr;
	socklen_t addrlen;
	int debug_level;
	int errors;
};


struct ctrl_iface_priv {
	struct wpa_supplicant *wpa_s;
	int sock;
	struct dl_list ctrl_dst;
	int android_control_socket;
};


struct ctrl_iface_global_priv {
	struct wpa_global *global;
	int sock;
	struct dl_list ctrl_dst;
	int android_control_socket;
};


static void wpa_supplicant_ctrl_iface_send(struct wpa_supplicant *wpa_s,
					   const char *ifname, int sock,
					   struct dl_list *ctrl_dst,
					   int level, const char *buf,
					   size_t len,
					   struct ctrl_iface_priv *priv,
					   struct ctrl_iface_global_priv *gp);
static int wpas_ctrl_iface_reinit(struct wpa_supplicant *wpa_s,
				  struct ctrl_iface_priv *priv);
static int wpas_ctrl_iface_global_reinit(struct wpa_global *global,
					 struct ctrl_iface_global_priv *priv);


static int wpa_supplicant_ctrl_iface_attach(struct dl_list *ctrl_dst,
					    struct sockaddr_un *from,
					    socklen_t fromlen, int global)
{
	struct wpa_ctrl_dst *dst;
	char addr_txt[200];

	dst = os_zalloc(sizeof(*dst));
	if (dst == NULL)
		return -1;
	os_memcpy(&dst->addr, from, sizeof(struct sockaddr_un));
	dst->addrlen = fromlen;
	dst->debug_level = MSG_INFO;
	dl_list_add(ctrl_dst, &dst->list);
	printf_encode(addr_txt, sizeof(addr_txt),
		      (u8 *) from->sun_path,
		      fromlen - offsetof(struct sockaddr_un, sun_path));
	wpa_printf(MSG_DEBUG, "CTRL_IFACE %smonitor attached %s",
		   global ? "global " : "", addr_txt);
	return 0;
}


static int wpa_supplicant_ctrl_iface_detach(struct dl_list *ctrl_dst,
					    struct sockaddr_un *from,
					    socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst;

	dl_list_for_each(dst, ctrl_dst, struct wpa_ctrl_dst, list) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			char addr_txt[200];
			printf_encode(addr_txt, sizeof(addr_txt),
				      (u8 *) from->sun_path,
				      fromlen -
				      offsetof(struct sockaddr_un, sun_path));
			wpa_printf(MSG_DEBUG, "CTRL_IFACE monitor detached %s",
				   addr_txt);
			dl_list_del(&dst->list);
			os_free(dst);
			return 0;
		}
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

	dl_list_for_each(dst, &priv->ctrl_dst, struct wpa_ctrl_dst, list) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			char addr_txt[200];
			dst->debug_level = atoi(level);
			printf_encode(addr_txt, sizeof(addr_txt),
				      (u8 *) from->sun_path, fromlen -
				      offsetof(struct sockaddr_un, sun_path));
			wpa_printf(MSG_DEBUG, "CTRL_IFACE changed monitor level to %d for %s",
				   dst->debug_level, addr_txt);
			return 0;
		}
	}

	return -1;
}


static void wpa_supplicant_ctrl_iface_receive(int sock, void *eloop_ctx,
					      void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct ctrl_iface_priv *priv = sock_ctx;
	char buf[4096];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply = NULL, *reply_buf = NULL;
	size_t reply_len = 0;
	int new_attached = 0;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",
			   strerror(errno));
		return;
	}
	buf[res] = '\0';

	if (os_strcmp(buf, "ATTACH") == 0) {
		if (wpa_supplicant_ctrl_iface_attach(&priv->ctrl_dst, &from,
						     fromlen, 0))
			reply_len = 1;
		else {
			new_attached = 1;
			reply_len = 2;
		}
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (wpa_supplicant_ctrl_iface_detach(&priv->ctrl_dst, &from,
						     fromlen))
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
		reply_buf = wpa_supplicant_ctrl_iface_process(wpa_s, buf,
							      &reply_len);
		reply = reply_buf;
	}

	if (!reply && reply_len == 1) {
		reply = "FAIL\n";
		reply_len = 5;
	} else if (!reply && reply_len == 2) {
		reply = "OK\n";
		reply_len = 3;
	}

	if (reply) {
		if (sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
			   fromlen) < 0) {
			int _errno = errno;
			wpa_dbg(wpa_s, MSG_DEBUG,
				"ctrl_iface sendto failed: %d - %s",
				_errno, strerror(_errno));
			if (_errno == ENOBUFS || _errno == EAGAIN) {
				/*
				 * The socket send buffer could be full. This
				 * may happen if client programs are not
				 * receiving their pending messages. Close and
				 * reopen the socket as a workaround to avoid
				 * getting stuck being unable to send any new
				 * responses.
				 */
				sock = wpas_ctrl_iface_reinit(wpa_s, priv);
				if (sock < 0) {
					wpa_dbg(wpa_s, MSG_DEBUG, "Failed to reinitialize ctrl_iface socket");
				}
			}
			if (new_attached) {
				wpa_dbg(wpa_s, MSG_DEBUG, "Failed to send response to ATTACH - detaching");
				new_attached = 0;
				wpa_supplicant_ctrl_iface_detach(
					&priv->ctrl_dst, &from, fromlen);
			}
		}
	}
	os_free(reply_buf);

	if (new_attached)
		eapol_sm_notify_ctrl_attached(wpa_s->eapol);
}


static char * wpa_supplicant_ctrl_iface_path(struct wpa_supplicant *wpa_s)
{
	char *buf;
	size_t len;
	char *pbuf, *dir = NULL;
	int res;

	if (wpa_s->conf->ctrl_interface == NULL)
		return NULL;

	pbuf = os_strdup(wpa_s->conf->ctrl_interface);
	if (pbuf == NULL)
		return NULL;
	if (os_strncmp(pbuf, "DIR=", 4) == 0) {
		char *gid_str;
		dir = pbuf + 4;
		gid_str = os_strstr(dir, " GROUP=");
		if (gid_str)
			*gid_str = '\0';
	} else
		dir = pbuf;

	len = os_strlen(dir) + os_strlen(wpa_s->ifname) + 2;
	buf = os_malloc(len);
	if (buf == NULL) {
		os_free(pbuf);
		return NULL;
	}

	res = os_snprintf(buf, len, "%s/%s", dir, wpa_s->ifname);
	if (os_snprintf_error(len, res)) {
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


static void wpa_supplicant_ctrl_iface_msg_cb(void *ctx, int level, int global,
					     const char *txt, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (wpa_s == NULL)
		return;

	if (global != 2 && wpa_s->global->ctrl_iface) {
		struct ctrl_iface_global_priv *priv = wpa_s->global->ctrl_iface;
		if (!dl_list_empty(&priv->ctrl_dst)) {
			wpa_supplicant_ctrl_iface_send(wpa_s, global ? NULL :
						       wpa_s->ifname,
						       priv->sock,
						       &priv->ctrl_dst,
						       level, txt, len, NULL,
						       priv);
		}
	}

	if (wpa_s->ctrl_iface == NULL)
		return;
	wpa_supplicant_ctrl_iface_send(wpa_s, NULL, wpa_s->ctrl_iface->sock,
				       &wpa_s->ctrl_iface->ctrl_dst,
				       level, txt, len, wpa_s->ctrl_iface,
				       NULL);
}


static int wpas_ctrl_iface_open_sock(struct wpa_supplicant *wpa_s,
				     struct ctrl_iface_priv *priv)
{
	struct sockaddr_un addr;
	char *fname = NULL;
	gid_t gid = 0;
	int gid_set = 0;
	char *buf, *dir = NULL, *gid_str = NULL;
	struct group *grp;
	char *endp;
	int flags;

	buf = os_strdup(wpa_s->conf->ctrl_interface);
	if (buf == NULL)
		goto fail;
#ifdef ANDROID
	os_snprintf(addr.sun_path, sizeof(addr.sun_path), "wpa_%s",
		    wpa_s->conf->ctrl_interface);
	priv->sock = android_get_control_socket(addr.sun_path);
	if (priv->sock >= 0) {
		priv->android_control_socket = 1;
		goto havesock;
	}
#endif /* ANDROID */
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
			wpa_printf(MSG_ERROR, "mkdir[ctrl_interface=%s]: %s",
				   dir, strerror(errno));
			goto fail;
		}
	}

#ifdef ANDROID
	/*
	 * wpa_supplicant is started from /init.*.rc on Android and that seems
	 * to be using umask 0077 which would leave the control interface
	 * directory without group access. This breaks things since Wi-Fi
	 * framework assumes that this directory can be accessed by other
	 * applications in the wifi group. Fix this by adding group access even
	 * if umask value would prevent this.
	 */
	if (chmod(dir, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "CTRL: Could not chmod directory: %s",
			   strerror(errno));
		/* Try to continue anyway */
	}
#endif /* ANDROID */

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
		wpa_printf(MSG_ERROR, "chown[ctrl_interface=%s,gid=%d]: %s",
			   dir, (int) gid, strerror(errno));
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
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
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
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   fname, strerror(errno));
				goto fail;
			}
			if (bind(priv->sock, (struct sockaddr *) &addr,
				 sizeof(addr)) < 0) {
				wpa_printf(MSG_ERROR, "supp-ctrl-iface-init: bind(PF_UNIX): %s",
					   strerror(errno));
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
		wpa_printf(MSG_ERROR, "chown[ctrl_interface=%s,gid=%d]: %s",
			   fname, (int) gid, strerror(errno));
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "chmod[ctrl_interface=%s]: %s",
			   fname, strerror(errno));
		goto fail;
	}
	os_free(fname);

#ifdef ANDROID
havesock:
#endif /* ANDROID */

	/*
	 * Make socket non-blocking so that we don't hang forever if
	 * target dies unexpectedly.
	 */
	flags = fcntl(priv->sock, F_GETFL);
	if (flags >= 0) {
		flags |= O_NONBLOCK;
		if (fcntl(priv->sock, F_SETFL, flags) < 0) {
			wpa_printf(MSG_INFO, "fcntl(ctrl, O_NONBLOCK): %s",
				   strerror(errno));
			/* Not fatal, continue on.*/
		}
	}

	eloop_register_read_sock(priv->sock, wpa_supplicant_ctrl_iface_receive,
				 wpa_s, priv);
	wpa_msg_register_cb(wpa_supplicant_ctrl_iface_msg_cb);

	os_free(buf);
	return 0;

fail:
	if (priv->sock >= 0) {
		close(priv->sock);
		priv->sock = -1;
	}
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	os_free(buf);
	return -1;
}


struct ctrl_iface_priv *
wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	struct ctrl_iface_priv *priv;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;
	dl_list_init(&priv->ctrl_dst);
	priv->wpa_s = wpa_s;
	priv->sock = -1;

	if (wpa_s->conf->ctrl_interface == NULL)
		return priv;

	if (wpas_ctrl_iface_open_sock(wpa_s, priv) < 0) {
		os_free(priv);
		return NULL;
	}

	return priv;
}


static int wpas_ctrl_iface_reinit(struct wpa_supplicant *wpa_s,
				  struct ctrl_iface_priv *priv)
{
	int res;

	if (priv->sock <= 0)
		return -1;

	/*
	 * On Android, the control socket being used may be the socket
	 * that is created when wpa_supplicant is started as a /init.*.rc
	 * service. Such a socket is maintained as a key-value pair in
	 * Android's environment. Closing this control socket would leave us
	 * in a bad state with an invalid socket descriptor.
	 */
	if (priv->android_control_socket)
		return priv->sock;

	eloop_unregister_read_sock(priv->sock);
	close(priv->sock);
	priv->sock = -1;
	res = wpas_ctrl_iface_open_sock(wpa_s, priv);
	if (res < 0)
		return -1;
	return priv->sock;
}


void wpa_supplicant_ctrl_iface_deinit(struct ctrl_iface_priv *priv)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (priv->sock > -1) {
		char *fname;
		char *buf, *dir = NULL;
		eloop_unregister_read_sock(priv->sock);
		if (!dl_list_empty(&priv->ctrl_dst)) {
			/*
			 * Wait before closing the control socket if
			 * there are any attached monitors in order to allow
			 * them to receive any pending messages.
			 */
			wpa_printf(MSG_DEBUG, "CTRL_IFACE wait for attached "
				   "monitors to receive messages");
			os_sleep(0, 100000);
		}
		close(priv->sock);
		priv->sock = -1;
		fname = wpa_supplicant_ctrl_iface_path(priv->wpa_s);
		if (fname) {
			unlink(fname);
			os_free(fname);
		}

		if (priv->wpa_s->conf->ctrl_interface == NULL)
			goto free_dst;
		buf = os_strdup(priv->wpa_s->conf->ctrl_interface);
		if (buf == NULL)
			goto free_dst;
		if (os_strncmp(buf, "DIR=", 4) == 0) {
			char *gid_str;
			dir = buf + 4;
			gid_str = os_strstr(dir, " GROUP=");
			if (gid_str)
				*gid_str = '\0';
		} else
			dir = buf;

		if (rmdir(dir) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   dir, strerror(errno));
			}
		}
		os_free(buf);
	}

free_dst:
	dl_list_for_each_safe(dst, prev, &priv->ctrl_dst, struct wpa_ctrl_dst,
			      list)
		os_free(dst);
	os_free(priv);
}


/**
 * wpa_supplicant_ctrl_iface_send - Send a control interface packet to monitors
 * @ifname: Interface name for global control socket or %NULL
 * @sock: Local socket fd
 * @ctrl_dst: List of attached listeners
 * @level: Priority level of the message
 * @buf: Message data
 * @len: Message length
 *
 * Send a packet to all monitor programs attached to the control interface.
 */
static void wpa_supplicant_ctrl_iface_send(struct wpa_supplicant *wpa_s,
					   const char *ifname, int sock,
					   struct dl_list *ctrl_dst,
					   int level, const char *buf,
					   size_t len,
					   struct ctrl_iface_priv *priv,
					   struct ctrl_iface_global_priv *gp)
{
	struct wpa_ctrl_dst *dst, *next;
	char levelstr[10];
	int idx, res;
	struct msghdr msg;
	struct iovec io[5];

	if (sock < 0 || dl_list_empty(ctrl_dst))
		return;

	res = os_snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	if (os_snprintf_error(sizeof(levelstr), res))
		return;
	idx = 0;
	if (ifname) {
		io[idx].iov_base = "IFNAME=";
		io[idx].iov_len = 7;
		idx++;
		io[idx].iov_base = (char *) ifname;
		io[idx].iov_len = os_strlen(ifname);
		idx++;
		io[idx].iov_base = " ";
		io[idx].iov_len = 1;
		idx++;
	}
	io[idx].iov_base = levelstr;
	io[idx].iov_len = os_strlen(levelstr);
	idx++;
	io[idx].iov_base = (char *) buf;
	io[idx].iov_len = len;
	idx++;
	os_memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = idx;

	dl_list_for_each_safe(dst, next, ctrl_dst, struct wpa_ctrl_dst, list) {
		int _errno;
		char addr_txt[200];

		if (level < dst->debug_level)
			continue;

		printf_encode(addr_txt, sizeof(addr_txt),
			      (u8 *) dst->addr.sun_path, dst->addrlen -
			      offsetof(struct sockaddr_un, sun_path));
		msg.msg_name = (void *) &dst->addr;
		msg.msg_namelen = dst->addrlen;
		if (sendmsg(sock, &msg, MSG_DONTWAIT) >= 0) {
			wpa_printf(MSG_DEBUG, "CTRL_IFACE monitor sent successfully to %s",
				   addr_txt);
			dst->errors = 0;
			continue;
		}

		_errno = errno;
		wpa_printf(MSG_DEBUG, "CTRL_IFACE monitor[%s]: %d - %s",
			   addr_txt, errno, strerror(errno));
		dst->errors++;

		if (dst->errors > 10 || _errno == ENOENT || _errno == EPERM) {
			wpa_printf(MSG_INFO, "CTRL_IFACE: Detach monitor %s that cannot receive messages",
				addr_txt);
			wpa_supplicant_ctrl_iface_detach(ctrl_dst, &dst->addr,
							 dst->addrlen);
		}

		if (_errno == ENOBUFS || _errno == EAGAIN) {
			/*
			 * The socket send buffer could be full. This may happen
			 * if client programs are not receiving their pending
			 * messages. Close and reopen the socket as a workaround
			 * to avoid getting stuck being unable to send any new
			 * responses.
			 */
			if (priv)
				sock = wpas_ctrl_iface_reinit(wpa_s, priv);
			else if (gp)
				sock = wpas_ctrl_iface_global_reinit(
					wpa_s->global, gp);
			else
				break;
			if (sock < 0) {
				wpa_dbg(wpa_s, MSG_DEBUG,
					"Failed to reinitialize ctrl_iface socket");
				break;
			}
		}
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
			wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",
				   strerror(errno));
			continue;
		}
		buf[res] = '\0';

		if (os_strcmp(buf, "ATTACH") == 0) {
			/* handle ATTACH signal of first monitor interface */
			if (!wpa_supplicant_ctrl_iface_attach(&priv->ctrl_dst,
							      &from, fromlen,
							      0)) {
				if (sendto(priv->sock, "OK\n", 3, 0,
					   (struct sockaddr *) &from, fromlen) <
				    0) {
					wpa_printf(MSG_DEBUG, "ctrl_iface sendto failed: %s",
						   strerror(errno));
				}
				/* OK to continue */
				return;
			} else {
				if (sendto(priv->sock, "FAIL\n", 5, 0,
					   (struct sockaddr *) &from, fromlen) <
				    0) {
					wpa_printf(MSG_DEBUG, "ctrl_iface sendto failed: %s",
						   strerror(errno));
				}
			}
		} else {
			/* return FAIL for all other signals */
			if (sendto(priv->sock, "FAIL\n", 5, 0,
				   (struct sockaddr *) &from, fromlen) < 0) {
				wpa_printf(MSG_DEBUG,
					   "ctrl_iface sendto failed: %s",
					   strerror(errno));
			}
		}
	}
}


/* Global ctrl_iface */

static void wpa_supplicant_global_ctrl_iface_receive(int sock, void *eloop_ctx,
						     void *sock_ctx)
{
	struct wpa_global *global = eloop_ctx;
	struct ctrl_iface_global_priv *priv = sock_ctx;
	char buf[4096];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply = NULL, *reply_buf = NULL;
	size_t reply_len;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",
			   strerror(errno));
		return;
	}
	buf[res] = '\0';

	if (os_strcmp(buf, "ATTACH") == 0) {
		if (wpa_supplicant_ctrl_iface_attach(&priv->ctrl_dst, &from,
						     fromlen, 1))
			reply_len = 1;
		else
			reply_len = 2;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (wpa_supplicant_ctrl_iface_detach(&priv->ctrl_dst, &from,
						     fromlen))
			reply_len = 1;
		else
			reply_len = 2;
	} else {
		reply_buf = wpa_supplicant_global_ctrl_iface_process(
			global, buf, &reply_len);
		reply = reply_buf;
	}

	if (!reply && reply_len == 1) {
		reply = "FAIL\n";
		reply_len = 5;
	} else if (!reply && reply_len == 2) {
		reply = "OK\n";
		reply_len = 3;
	}

	if (reply) {
		if (sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
			   fromlen) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface sendto failed: %s",
				strerror(errno));
		}
	}
	os_free(reply_buf);
}


static int wpas_global_ctrl_iface_open_sock(struct wpa_global *global,
					    struct ctrl_iface_global_priv *priv)
{
	struct sockaddr_un addr;
	const char *ctrl = global->params.ctrl_interface;
	int flags;

	wpa_printf(MSG_DEBUG, "Global control interface '%s'", ctrl);

#ifdef ANDROID
	if (os_strncmp(ctrl, "@android:", 9) == 0) {
		priv->sock = android_get_control_socket(ctrl + 9);
		if (priv->sock < 0) {
			wpa_printf(MSG_ERROR, "Failed to open Android control "
				   "socket '%s'", ctrl + 9);
			goto fail;
		}
		wpa_printf(MSG_DEBUG, "Using Android control socket '%s'",
			   ctrl + 9);
		priv->android_control_socket = 1;
		goto havesock;
	}

	if (os_strncmp(ctrl, "@abstract:", 10) != 0) {
		/*
		 * Backwards compatibility - try to open an Android control
		 * socket and if that fails, assume this was a UNIX domain
		 * socket instead.
		 */
		priv->sock = android_get_control_socket(ctrl);
		if (priv->sock >= 0) {
			wpa_printf(MSG_DEBUG,
				   "Using Android control socket '%s'",
				   ctrl);
			priv->android_control_socket = 1;
			goto havesock;
		}
	}
#endif /* ANDROID */

	priv->sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (priv->sock < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;

	if (os_strncmp(ctrl, "@abstract:", 10) == 0) {
		addr.sun_path[0] = '\0';
		os_strlcpy(addr.sun_path + 1, ctrl + 10,
			   sizeof(addr.sun_path) - 1);
		if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) <
		    0) {
			wpa_printf(MSG_ERROR, "supp-global-ctrl-iface-init: "
				   "bind(PF_UNIX;%s) failed: %s",
				   ctrl, strerror(errno));
			goto fail;
		}
		wpa_printf(MSG_DEBUG, "Using Abstract control socket '%s'",
			   ctrl + 10);
		goto havesock;
	}

	os_strlcpy(addr.sun_path, ctrl, sizeof(addr.sun_path));
	if (bind(priv->sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_INFO, "supp-global-ctrl-iface-init(%s) (will try fixup): bind(PF_UNIX): %s",
			   ctrl, strerror(errno));
		if (connect(priv->sock, (struct sockaddr *) &addr,
			    sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(ctrl) < 0) {
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   ctrl, strerror(errno));
				goto fail;
			}
			if (bind(priv->sock, (struct sockaddr *) &addr,
				 sizeof(addr)) < 0) {
				wpa_printf(MSG_ERROR, "supp-glb-iface-init: bind(PF_UNIX;%s): %s",
					   ctrl, strerror(errno));
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'",
				   ctrl);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore",
				   ctrl);
			goto fail;
		}
	}

	wpa_printf(MSG_DEBUG, "Using UNIX control socket '%s'", ctrl);

	if (global->params.ctrl_interface_group) {
		char *gid_str = global->params.ctrl_interface_group;
		gid_t gid = 0;
		struct group *grp;
		char *endp;

		grp = getgrnam(gid_str);
		if (grp) {
			gid = grp->gr_gid;
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
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
				   (int) gid);
		}
		if (chown(ctrl, -1, gid) < 0) {
			wpa_printf(MSG_ERROR,
				   "chown[global_ctrl_interface=%s,gid=%d]: %s",
				   ctrl, (int) gid, strerror(errno));
			goto fail;
		}

		if (chmod(ctrl, S_IRWXU | S_IRWXG) < 0) {
			wpa_printf(MSG_ERROR,
				   "chmod[global_ctrl_interface=%s]: %s",
				   ctrl, strerror(errno));
			goto fail;
		}
	} else {
		if (chmod(ctrl, S_IRWXU) < 0) {
			wpa_printf(MSG_DEBUG,
				   "chmod[global_ctrl_interface=%s](S_IRWXU): %s",
				   ctrl, strerror(errno));
			/* continue anyway since group change was not required
			 */
		}
	}

havesock:

	/*
	 * Make socket non-blocking so that we don't hang forever if
	 * target dies unexpectedly.
	 */
	flags = fcntl(priv->sock, F_GETFL);
	if (flags >= 0) {
		flags |= O_NONBLOCK;
		if (fcntl(priv->sock, F_SETFL, flags) < 0) {
			wpa_printf(MSG_INFO, "fcntl(ctrl, O_NONBLOCK): %s",
				   strerror(errno));
			/* Not fatal, continue on.*/
		}
	}

	eloop_register_read_sock(priv->sock,
				 wpa_supplicant_global_ctrl_iface_receive,
				 global, priv);

	return 0;

fail:
	if (priv->sock >= 0) {
		close(priv->sock);
		priv->sock = -1;
	}
	return -1;
}


struct ctrl_iface_global_priv *
wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	struct ctrl_iface_global_priv *priv;

	priv = os_zalloc(sizeof(*priv));
	if (priv == NULL)
		return NULL;
	dl_list_init(&priv->ctrl_dst);
	priv->global = global;
	priv->sock = -1;

	if (global->params.ctrl_interface == NULL)
		return priv;

	if (wpas_global_ctrl_iface_open_sock(global, priv) < 0) {
		os_free(priv);
		return NULL;
	}

	wpa_msg_register_cb(wpa_supplicant_ctrl_iface_msg_cb);

	return priv;
}


static int wpas_ctrl_iface_global_reinit(struct wpa_global *global,
					 struct ctrl_iface_global_priv *priv)
{
	int res;

	if (priv->sock <= 0)
		return -1;

	/*
	 * On Android, the control socket being used may be the socket
	 * that is created when wpa_supplicant is started as a /init.*.rc
	 * service. Such a socket is maintained as a key-value pair in
	 * Android's environment. Closing this control socket would leave us
	 * in a bad state with an invalid socket descriptor.
	 */
	if (priv->android_control_socket)
		return priv->sock;

	eloop_unregister_read_sock(priv->sock);
	close(priv->sock);
	priv->sock = -1;
	res = wpas_global_ctrl_iface_open_sock(global, priv);
	if (res < 0)
		return -1;
	return priv->sock;
}


void
wpa_supplicant_global_ctrl_iface_deinit(struct ctrl_iface_global_priv *priv)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (priv->sock >= 0) {
		eloop_unregister_read_sock(priv->sock);
		close(priv->sock);
	}
	if (priv->global->params.ctrl_interface)
		unlink(priv->global->params.ctrl_interface);
	dl_list_for_each_safe(dst, prev, &priv->ctrl_dst, struct wpa_ctrl_dst,
			      list)
		os_free(dst);
	os_free(priv);
}
