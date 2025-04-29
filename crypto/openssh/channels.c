/* $OpenBSD: channels.c,v 1.439 2024/07/25 22:40:08 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file contains functions for generic socket connection forwarding.
 * There is also code for initiating connection forwarding for X11 connections,
 * arbitrary tcp/ip connections, and the authentication agent connection.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 support added by Markus Friedl.
 * Copyright (c) 1999, 2000, 2001, 2002 Markus Friedl.  All rights reserved.
 * Copyright (c) 1999 Dug Song.  All rights reserved.
 * Copyright (c) 1999 Theo de Raadt.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <stdarg.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "ssh.h"
#include "ssh2.h"
#include "ssherr.h"
#include "sshbuf.h"
#include "packet.h"
#include "log.h"
#include "misc.h"
#include "channels.h"
#include "compat.h"
#include "canohost.h"
#include "sshkey.h"
#include "authfd.h"
#include "pathnames.h"
#include "match.h"

/* XXX remove once we're satisfied there's no lurking bugs */
/* #define DEBUG_CHANNEL_POLL 1 */

/* -- agent forwarding */
#define	NUM_SOCKS	10

/* -- X11 forwarding */
/* Maximum number of fake X11 displays to try. */
#define MAX_DISPLAYS  1000

/* Per-channel callback for pre/post IO actions */
typedef void chan_fn(struct ssh *, Channel *c);

/*
 * Data structure for storing which hosts are permitted for forward requests.
 * The local sides of any remote forwards are stored in this array to prevent
 * a corrupt remote server from accessing arbitrary TCP/IP ports on our local
 * network (which might be behind a firewall).
 */
/* XXX: streamlocal wants a path instead of host:port */
/*      Overload host_to_connect; we could just make this match Forward */
/*	XXX - can we use listen_host instead of listen_path? */
struct permission {
	char *host_to_connect;		/* Connect to 'host'. */
	int port_to_connect;		/* Connect to 'port'. */
	char *listen_host;		/* Remote side should listen address. */
	char *listen_path;		/* Remote side should listen path. */
	int listen_port;		/* Remote side should listen port. */
	Channel *downstream;		/* Downstream mux*/
};

/*
 * Stores the forwarding permission state for a single direction (local or
 * remote).
 */
struct permission_set {
	/*
	 * List of all local permitted host/port pairs to allow for the
	 * user.
	 */
	u_int num_permitted_user;
	struct permission *permitted_user;

	/*
	 * List of all permitted host/port pairs to allow for the admin.
	 */
	u_int num_permitted_admin;
	struct permission *permitted_admin;

	/*
	 * If this is true, all opens/listens are permitted.  This is the
	 * case on the server on which we have to trust the client anyway,
	 * and the user could do anything after logging in.
	 */
	int all_permitted;
};

/* Used to record timeouts per channel type */
struct ssh_channel_timeout {
	char *type_pattern;
	int timeout_secs;
};

/* Master structure for channels state */
struct ssh_channels {
	/*
	 * Pointer to an array containing all allocated channels.  The array
	 * is dynamically extended as needed.
	 */
	Channel **channels;

	/*
	 * Size of the channel array.  All slots of the array must always be
	 * initialized (at least the type field); unused slots set to NULL
	 */
	u_int channels_alloc;

	/*
	 * 'channel_pre*' are called just before IO to add any bits
	 * relevant to channels in the c->io_want bitmasks.
	 *
	 * 'channel_post*': perform any appropriate operations for
	 * channels which have c->io_ready events pending.
	 */
	chan_fn **channel_pre;
	chan_fn **channel_post;

	/* -- tcp forwarding */
	struct permission_set local_perms;
	struct permission_set remote_perms;

	/* -- X11 forwarding */

	/* Saved X11 local (client) display. */
	char *x11_saved_display;

	/* Saved X11 authentication protocol name. */
	char *x11_saved_proto;

	/* Saved X11 authentication data.  This is the real data. */
	char *x11_saved_data;
	u_int x11_saved_data_len;

	/* Deadline after which all X11 connections are refused */
	time_t x11_refuse_time;

	/*
	 * Fake X11 authentication data.  This is what the server will be
	 * sending us; we should replace any occurrences of this by the
	 * real data.
	 */
	u_char *x11_fake_data;
	u_int x11_fake_data_len;

	/* AF_UNSPEC or AF_INET or AF_INET6 */
	int IPv4or6;

	/* Channel timeouts by type */
	struct ssh_channel_timeout *timeouts;
	size_t ntimeouts;
	/* Global timeout for all OPEN channels */
	int global_deadline;
	time_t lastused;
};

/* helper */
static void port_open_helper(struct ssh *ssh, Channel *c, char *rtype);
static const char *channel_rfwd_bind_host(const char *listen_host);

/* non-blocking connect helpers */
static int connect_next(struct channel_connect *);
static void channel_connect_ctx_free(struct channel_connect *);
static Channel *rdynamic_connect_prepare(struct ssh *, char *, char *);
static int rdynamic_connect_finish(struct ssh *, Channel *);

/* Setup helper */
static void channel_handler_init(struct ssh_channels *sc);

/* -- channel core */

void
channel_init_channels(struct ssh *ssh)
{
	struct ssh_channels *sc;

	if ((sc = calloc(1, sizeof(*sc))) == NULL)
		fatal_f("allocation failed");
	sc->channels_alloc = 10;
	sc->channels = xcalloc(sc->channels_alloc, sizeof(*sc->channels));
	sc->IPv4or6 = AF_UNSPEC;
	channel_handler_init(sc);

	ssh->chanctxt = sc;
}

Channel *
channel_by_id(struct ssh *ssh, int id)
{
	Channel *c;

	if (id < 0 || (u_int)id >= ssh->chanctxt->channels_alloc) {
		logit_f("%d: bad id", id);
		return NULL;
	}
	c = ssh->chanctxt->channels[id];
	if (c == NULL) {
		logit_f("%d: bad id: channel free", id);
		return NULL;
	}
	return c;
}

Channel *
channel_by_remote_id(struct ssh *ssh, u_int remote_id)
{
	Channel *c;
	u_int i;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		c = ssh->chanctxt->channels[i];
		if (c != NULL && c->have_remote_id && c->remote_id == remote_id)
			return c;
	}
	return NULL;
}

/*
 * Returns the channel if it is allowed to receive protocol messages.
 * Private channels, like listening sockets, may not receive messages.
 */
Channel *
channel_lookup(struct ssh *ssh, int id)
{
	Channel *c;

	if ((c = channel_by_id(ssh, id)) == NULL)
		return NULL;

	switch (c->type) {
	case SSH_CHANNEL_X11_OPEN:
	case SSH_CHANNEL_LARVAL:
	case SSH_CHANNEL_CONNECTING:
	case SSH_CHANNEL_DYNAMIC:
	case SSH_CHANNEL_RDYNAMIC_OPEN:
	case SSH_CHANNEL_RDYNAMIC_FINISH:
	case SSH_CHANNEL_OPENING:
	case SSH_CHANNEL_OPEN:
	case SSH_CHANNEL_ABANDONED:
	case SSH_CHANNEL_MUX_PROXY:
		return c;
	}
	logit("Non-public channel %d, type %d.", id, c->type);
	return NULL;
}

/*
 * Add a timeout for open channels whose c->ctype (or c->xctype if it is set)
 * match type_pattern.
 */
void
channel_add_timeout(struct ssh *ssh, const char *type_pattern,
    int timeout_secs)
{
	struct ssh_channels *sc = ssh->chanctxt;

	if (strcmp(type_pattern, "global") == 0) {
		debug2_f("global channel timeout %d seconds", timeout_secs);
		sc->global_deadline = timeout_secs;
		return;
	}
	debug2_f("channel type \"%s\" timeout %d seconds",
	    type_pattern, timeout_secs);
	sc->timeouts = xrecallocarray(sc->timeouts, sc->ntimeouts,
	    sc->ntimeouts + 1, sizeof(*sc->timeouts));
	sc->timeouts[sc->ntimeouts].type_pattern = xstrdup(type_pattern);
	sc->timeouts[sc->ntimeouts].timeout_secs = timeout_secs;
	sc->ntimeouts++;
}

/* Clears all previously-added channel timeouts */
void
channel_clear_timeouts(struct ssh *ssh)
{
	struct ssh_channels *sc = ssh->chanctxt;
	size_t i;

	debug3_f("clearing");
	for (i = 0; i < sc->ntimeouts; i++)
		free(sc->timeouts[i].type_pattern);
	free(sc->timeouts);
	sc->timeouts = NULL;
	sc->ntimeouts = 0;
}

static int
lookup_timeout(struct ssh *ssh, const char *type)
{
	struct ssh_channels *sc = ssh->chanctxt;
	size_t i;

	for (i = 0; i < sc->ntimeouts; i++) {
		if (match_pattern(type, sc->timeouts[i].type_pattern))
			return sc->timeouts[i].timeout_secs;
	}

	return 0;
}

/*
 * Sets "extended type" of a channel; used by session layer to add additional
 * information about channel types (e.g. shell, login, subsystem) that can then
 * be used to select timeouts.
 * Will reset c->inactive_deadline as a side-effect.
 */
void
channel_set_xtype(struct ssh *ssh, int id, const char *xctype)
{
	Channel *c;

	if ((c = channel_by_id(ssh, id)) == NULL)
		fatal_f("missing channel %d", id);
	if (c->xctype != NULL)
		free(c->xctype);
	c->xctype = xstrdup(xctype);
	/* Type has changed, so look up inactivity deadline again */
	c->inactive_deadline = lookup_timeout(ssh, c->xctype);
	debug2_f("labeled channel %d as %s (inactive timeout %u)", id, xctype,
	    c->inactive_deadline);
}

/*
 * update "last used" time on a channel.
 * NB. nothing else should update lastused except to clear it.
 */
static void
channel_set_used_time(struct ssh *ssh, Channel *c)
{
	ssh->chanctxt->lastused = monotime();
	if (c != NULL)
		c->lastused = ssh->chanctxt->lastused;
}

/*
 * Get the time at which a channel is due to time out for inactivity.
 * Returns 0 if the channel is not due to time out ever.
 */
static time_t
channel_get_expiry(struct ssh *ssh, Channel *c)
{
	struct ssh_channels *sc = ssh->chanctxt;
	time_t expiry = 0, channel_expiry;

	if (sc->lastused != 0 && sc->global_deadline != 0)
		expiry = sc->lastused + sc->global_deadline;
	if (c->lastused != 0 && c->inactive_deadline != 0) {
		channel_expiry = c->lastused + c->inactive_deadline;
		if (expiry == 0 || channel_expiry < expiry)
			expiry = channel_expiry;
	}
	return expiry;
}

/*
 * Register filedescriptors for a channel, used when allocating a channel or
 * when the channel consumer/producer is ready, e.g. shell exec'd
 */
static void
channel_register_fds(struct ssh *ssh, Channel *c, int rfd, int wfd, int efd,
    int extusage, int nonblock, int is_tty)
{
	int val;

	if (rfd != -1)
		(void)fcntl(rfd, F_SETFD, FD_CLOEXEC);
	if (wfd != -1 && wfd != rfd)
		(void)fcntl(wfd, F_SETFD, FD_CLOEXEC);
	if (efd != -1 && efd != rfd && efd != wfd)
		(void)fcntl(efd, F_SETFD, FD_CLOEXEC);

	c->rfd = rfd;
	c->wfd = wfd;
	c->sock = (rfd == wfd) ? rfd : -1;
	c->efd = efd;
	c->extended_usage = extusage;

	if ((c->isatty = is_tty) != 0)
		debug2("channel %d: rfd %d isatty", c->self, c->rfd);
#ifdef _AIX
	/* XXX: Later AIX versions can't push as much data to tty */
	c->wfd_isatty = is_tty || isatty(c->wfd);
#endif

	/* enable nonblocking mode */
	c->restore_block = 0;
	if (nonblock == CHANNEL_NONBLOCK_STDIO) {
		/*
		 * Special handling for stdio file descriptors: do not set
		 * non-blocking mode if they are TTYs. Otherwise prepare to
		 * restore their blocking state on exit to avoid interfering
		 * with other programs that follow.
		 */
		if (rfd != -1 && !isatty(rfd) &&
		    (val = fcntl(rfd, F_GETFL)) != -1 && !(val & O_NONBLOCK)) {
			c->restore_flags[0] = val;
			c->restore_block |= CHANNEL_RESTORE_RFD;
			set_nonblock(rfd);
		}
		if (wfd != -1 && !isatty(wfd) &&
		    (val = fcntl(wfd, F_GETFL)) != -1 && !(val & O_NONBLOCK)) {
			c->restore_flags[1] = val;
			c->restore_block |= CHANNEL_RESTORE_WFD;
			set_nonblock(wfd);
		}
		if (efd != -1 && !isatty(efd) &&
		    (val = fcntl(efd, F_GETFL)) != -1 && !(val & O_NONBLOCK)) {
			c->restore_flags[2] = val;
			c->restore_block |= CHANNEL_RESTORE_EFD;
			set_nonblock(efd);
		}
	} else if (nonblock) {
		if (rfd != -1)
			set_nonblock(rfd);
		if (wfd != -1)
			set_nonblock(wfd);
		if (efd != -1)
			set_nonblock(efd);
	}
	/* channel might be entering a larval state, so reset global timeout */
	channel_set_used_time(ssh, NULL);
}

/*
 * Allocate a new channel object and set its type and socket.
 */
Channel *
channel_new(struct ssh *ssh, char *ctype, int type, int rfd, int wfd, int efd,
    u_int window, u_int maxpack, int extusage, const char *remote_name,
    int nonblock)
{
	struct ssh_channels *sc = ssh->chanctxt;
	u_int i, found = 0;
	Channel *c;
	int r;

	/* Try to find a free slot where to put the new channel. */
	for (i = 0; i < sc->channels_alloc; i++) {
		if (sc->channels[i] == NULL) {
			/* Found a free slot. */
			found = i;
			break;
		}
	}
	if (i >= sc->channels_alloc) {
		/*
		 * There are no free slots. Take last+1 slot and expand
		 * the array.
		 */
		found = sc->channels_alloc;
		if (sc->channels_alloc > CHANNELS_MAX_CHANNELS)
			fatal_f("internal error: channels_alloc %d too big",
			    sc->channels_alloc);
		sc->channels = xrecallocarray(sc->channels, sc->channels_alloc,
		    sc->channels_alloc + 10, sizeof(*sc->channels));
		sc->channels_alloc += 10;
		debug2("channel: expanding %d", sc->channels_alloc);
	}
	/* Initialize and return new channel. */
	c = sc->channels[found] = xcalloc(1, sizeof(Channel));
	if ((c->input = sshbuf_new()) == NULL ||
	    (c->output = sshbuf_new()) == NULL ||
	    (c->extended = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_set_max_size(c->input, CHAN_INPUT_MAX)) != 0)
		fatal_fr(r, "sshbuf_set_max_size");
	c->ostate = CHAN_OUTPUT_OPEN;
	c->istate = CHAN_INPUT_OPEN;
	channel_register_fds(ssh, c, rfd, wfd, efd, extusage, nonblock, 0);
	c->self = found;
	c->type = type;
	c->ctype = ctype;
	c->local_window = window;
	c->local_window_max = window;
	c->local_maxpacket = maxpack;
	c->remote_name = xstrdup(remote_name);
	c->ctl_chan = -1;
	c->delayed = 1;		/* prevent call to channel_post handler */
	c->inactive_deadline = lookup_timeout(ssh, c->ctype);
	TAILQ_INIT(&c->status_confirms);
	debug("channel %d: new %s [%s] (inactive timeout: %u)",
	    found, c->ctype, remote_name, c->inactive_deadline);
	return c;
}

int
channel_close_fd(struct ssh *ssh, Channel *c, int *fdp)
{
	int ret, fd = *fdp;

	if (fd == -1)
		return 0;

	/* restore blocking */
	if (*fdp == c->rfd &&
	    (c->restore_block & CHANNEL_RESTORE_RFD) != 0)
		(void)fcntl(*fdp, F_SETFL, c->restore_flags[0]);
	else if (*fdp == c->wfd &&
	    (c->restore_block & CHANNEL_RESTORE_WFD) != 0)
		(void)fcntl(*fdp, F_SETFL, c->restore_flags[1]);
	else if (*fdp == c->efd &&
	    (c->restore_block & CHANNEL_RESTORE_EFD) != 0)
		(void)fcntl(*fdp, F_SETFL, c->restore_flags[2]);

	if (*fdp == c->rfd) {
		c->io_want &= ~SSH_CHAN_IO_RFD;
		c->io_ready &= ~SSH_CHAN_IO_RFD;
		c->rfd = -1;
		c->pfds[0] = -1;
	}
	if (*fdp == c->wfd) {
		c->io_want &= ~SSH_CHAN_IO_WFD;
		c->io_ready &= ~SSH_CHAN_IO_WFD;
		c->wfd = -1;
		c->pfds[1] = -1;
	}
	if (*fdp == c->efd) {
		c->io_want &= ~SSH_CHAN_IO_EFD;
		c->io_ready &= ~SSH_CHAN_IO_EFD;
		c->efd = -1;
		c->pfds[2] = -1;
	}
	if (*fdp == c->sock) {
		c->io_want &= ~SSH_CHAN_IO_SOCK;
		c->io_ready &= ~SSH_CHAN_IO_SOCK;
		c->sock = -1;
		c->pfds[3] = -1;
	}

	ret = close(fd);
	*fdp = -1; /* probably redundant */
	return ret;
}

/* Close all channel fd/socket. */
static void
channel_close_fds(struct ssh *ssh, Channel *c)
{
	int sock = c->sock, rfd = c->rfd, wfd = c->wfd, efd = c->efd;

	channel_close_fd(ssh, c, &c->sock);
	if (rfd != sock)
		channel_close_fd(ssh, c, &c->rfd);
	if (wfd != sock && wfd != rfd)
		channel_close_fd(ssh, c, &c->wfd);
	if (efd != sock && efd != rfd && efd != wfd)
		channel_close_fd(ssh, c, &c->efd);
}

static void
fwd_perm_clear(struct permission *perm)
{
	free(perm->host_to_connect);
	free(perm->listen_host);
	free(perm->listen_path);
	memset(perm, 0, sizeof(*perm));
}

/* Returns an printable name for the specified forwarding permission list */
static const char *
fwd_ident(int who, int where)
{
	if (who == FORWARD_ADM) {
		if (where == FORWARD_LOCAL)
			return "admin local";
		else if (where == FORWARD_REMOTE)
			return "admin remote";
	} else if (who == FORWARD_USER) {
		if (where == FORWARD_LOCAL)
			return "user local";
		else if (where == FORWARD_REMOTE)
			return "user remote";
	}
	fatal("Unknown forward permission list %d/%d", who, where);
}

/* Returns the forwarding permission list for the specified direction */
static struct permission_set *
permission_set_get(struct ssh *ssh, int where)
{
	struct ssh_channels *sc = ssh->chanctxt;

	switch (where) {
	case FORWARD_LOCAL:
		return &sc->local_perms;
		break;
	case FORWARD_REMOTE:
		return &sc->remote_perms;
		break;
	default:
		fatal_f("invalid forwarding direction %d", where);
	}
}

/* Returns pointers to the specified forwarding list and its element count */
static void
permission_set_get_array(struct ssh *ssh, int who, int where,
    struct permission ***permpp, u_int **npermpp)
{
	struct permission_set *pset = permission_set_get(ssh, where);

	switch (who) {
	case FORWARD_USER:
		*permpp = &pset->permitted_user;
		*npermpp = &pset->num_permitted_user;
		break;
	case FORWARD_ADM:
		*permpp = &pset->permitted_admin;
		*npermpp = &pset->num_permitted_admin;
		break;
	default:
		fatal_f("invalid forwarding client %d", who);
	}
}

/* Adds an entry to the specified forwarding list */
static int
permission_set_add(struct ssh *ssh, int who, int where,
    const char *host_to_connect, int port_to_connect,
    const char *listen_host, const char *listen_path, int listen_port,
    Channel *downstream)
{
	struct permission **permp;
	u_int n, *npermp;

	permission_set_get_array(ssh, who, where, &permp, &npermp);

	if (*npermp >= INT_MAX)
		fatal_f("%s overflow", fwd_ident(who, where));

	*permp = xrecallocarray(*permp, *npermp, *npermp + 1, sizeof(**permp));
	n = (*npermp)++;
#define MAYBE_DUP(s) ((s == NULL) ? NULL : xstrdup(s))
	(*permp)[n].host_to_connect = MAYBE_DUP(host_to_connect);
	(*permp)[n].port_to_connect = port_to_connect;
	(*permp)[n].listen_host = MAYBE_DUP(listen_host);
	(*permp)[n].listen_path = MAYBE_DUP(listen_path);
	(*permp)[n].listen_port = listen_port;
	(*permp)[n].downstream = downstream;
#undef MAYBE_DUP
	return (int)n;
}

static void
mux_remove_remote_forwardings(struct ssh *ssh, Channel *c)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	struct permission *perm;
	int r;
	u_int i;

	for (i = 0; i < pset->num_permitted_user; i++) {
		perm = &pset->permitted_user[i];
		if (perm->downstream != c)
			continue;

		/* cancel on the server, since mux client is gone */
		debug("channel %d: cleanup remote forward for %s:%u",
		    c->self, perm->listen_host, perm->listen_port);
		if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
		    (r = sshpkt_put_cstring(ssh,
		    "cancel-tcpip-forward")) != 0 ||
		    (r = sshpkt_put_u8(ssh, 0)) != 0 ||
		    (r = sshpkt_put_cstring(ssh,
		    channel_rfwd_bind_host(perm->listen_host))) != 0 ||
		    (r = sshpkt_put_u32(ssh, perm->listen_port)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0) {
			fatal_fr(r, "channel %i", c->self);
		}
		fwd_perm_clear(perm); /* unregister */
	}
}

/* Free the channel and close its fd/socket. */
void
channel_free(struct ssh *ssh, Channel *c)
{
	struct ssh_channels *sc = ssh->chanctxt;
	char *s;
	u_int i, n;
	Channel *other;
	struct channel_confirm *cc;

	for (n = 0, i = 0; i < sc->channels_alloc; i++) {
		if ((other = sc->channels[i]) == NULL)
			continue;
		n++;
		/* detach from mux client and prepare for closing */
		if (c->type == SSH_CHANNEL_MUX_CLIENT &&
		    other->type == SSH_CHANNEL_MUX_PROXY &&
		    other->mux_ctx == c) {
			other->mux_ctx = NULL;
			other->type = SSH_CHANNEL_OPEN;
			other->istate = CHAN_INPUT_CLOSED;
			other->ostate = CHAN_OUTPUT_CLOSED;
		}
	}
	debug("channel %d: free: %s, nchannels %u", c->self,
	    c->remote_name ? c->remote_name : "???", n);

	if (c->type == SSH_CHANNEL_MUX_CLIENT) {
		mux_remove_remote_forwardings(ssh, c);
		free(c->mux_ctx);
		c->mux_ctx = NULL;
	} else if (c->type == SSH_CHANNEL_MUX_LISTENER) {
		free(c->mux_ctx);
		c->mux_ctx = NULL;
	}

	if (log_level_get() >= SYSLOG_LEVEL_DEBUG3) {
		s = channel_open_message(ssh);
		debug3("channel %d: status: %s", c->self, s);
		free(s);
	}

	channel_close_fds(ssh, c);
	sshbuf_free(c->input);
	sshbuf_free(c->output);
	sshbuf_free(c->extended);
	c->input = c->output = c->extended = NULL;
	free(c->remote_name);
	c->remote_name = NULL;
	free(c->path);
	c->path = NULL;
	free(c->listening_addr);
	c->listening_addr = NULL;
	free(c->xctype);
	c->xctype = NULL;
	while ((cc = TAILQ_FIRST(&c->status_confirms)) != NULL) {
		if (cc->abandon_cb != NULL)
			cc->abandon_cb(ssh, c, cc->ctx);
		TAILQ_REMOVE(&c->status_confirms, cc, entry);
		freezero(cc, sizeof(*cc));
	}
	if (c->filter_cleanup != NULL && c->filter_ctx != NULL)
		c->filter_cleanup(ssh, c->self, c->filter_ctx);
	sc->channels[c->self] = NULL;
	freezero(c, sizeof(*c));
}

void
channel_free_all(struct ssh *ssh)
{
	u_int i;
	struct ssh_channels *sc = ssh->chanctxt;

	for (i = 0; i < sc->channels_alloc; i++)
		if (sc->channels[i] != NULL)
			channel_free(ssh, sc->channels[i]);

	free(sc->channels);
	sc->channels = NULL;
	sc->channels_alloc = 0;

	free(sc->x11_saved_display);
	sc->x11_saved_display = NULL;

	free(sc->x11_saved_proto);
	sc->x11_saved_proto = NULL;

	free(sc->x11_saved_data);
	sc->x11_saved_data = NULL;
	sc->x11_saved_data_len = 0;

	free(sc->x11_fake_data);
	sc->x11_fake_data = NULL;
	sc->x11_fake_data_len = 0;
}

/*
 * Closes the sockets/fds of all channels.  This is used to close extra file
 * descriptors after a fork.
 */
void
channel_close_all(struct ssh *ssh)
{
	u_int i;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++)
		if (ssh->chanctxt->channels[i] != NULL)
			channel_close_fds(ssh, ssh->chanctxt->channels[i]);
}

/*
 * Stop listening to channels.
 */
void
channel_stop_listening(struct ssh *ssh)
{
	u_int i;
	Channel *c;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		c = ssh->chanctxt->channels[i];
		if (c != NULL) {
			switch (c->type) {
			case SSH_CHANNEL_AUTH_SOCKET:
			case SSH_CHANNEL_PORT_LISTENER:
			case SSH_CHANNEL_RPORT_LISTENER:
			case SSH_CHANNEL_X11_LISTENER:
			case SSH_CHANNEL_UNIX_LISTENER:
			case SSH_CHANNEL_RUNIX_LISTENER:
				channel_close_fd(ssh, c, &c->sock);
				channel_free(ssh, c);
				break;
			}
		}
	}
}

/*
 * Returns true if no channel has too much buffered data, and false if one or
 * more channel is overfull.
 */
int
channel_not_very_much_buffered_data(struct ssh *ssh)
{
	u_int i;
	u_int maxsize = ssh_packet_get_maxsize(ssh);
	Channel *c;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		c = ssh->chanctxt->channels[i];
		if (c == NULL || c->type != SSH_CHANNEL_OPEN)
			continue;
		if (sshbuf_len(c->output) > maxsize) {
			debug2("channel %d: big output buffer %zu > %u",
			    c->self, sshbuf_len(c->output), maxsize);
			return 0;
		}
	}
	return 1;
}

/* Returns true if any channel is still open. */
int
channel_still_open(struct ssh *ssh)
{
	u_int i;
	Channel *c;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		c = ssh->chanctxt->channels[i];
		if (c == NULL)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_MUX_LISTENER:
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_AUTH_SOCKET:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_RDYNAMIC_OPEN:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
		case SSH_CHANNEL_ABANDONED:
		case SSH_CHANNEL_UNIX_LISTENER:
		case SSH_CHANNEL_RUNIX_LISTENER:
			continue;
		case SSH_CHANNEL_LARVAL:
			continue;
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_RDYNAMIC_FINISH:
		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_MUX_CLIENT:
		case SSH_CHANNEL_MUX_PROXY:
			return 1;
		default:
			fatal_f("bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	return 0;
}

/* Returns true if a channel with a TTY is open. */
int
channel_tty_open(struct ssh *ssh)
{
	u_int i;
	Channel *c;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		c = ssh->chanctxt->channels[i];
		if (c == NULL || c->type != SSH_CHANNEL_OPEN)
			continue;
		if (c->client_tty)
			return 1;
	}
	return 0;
}

/* Returns the id of an open channel suitable for keepaliving */
int
channel_find_open(struct ssh *ssh)
{
	u_int i;
	Channel *c;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		c = ssh->chanctxt->channels[i];
		if (c == NULL || !c->have_remote_id)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_RDYNAMIC_OPEN:
		case SSH_CHANNEL_RDYNAMIC_FINISH:
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_MUX_LISTENER:
		case SSH_CHANNEL_MUX_CLIENT:
		case SSH_CHANNEL_MUX_PROXY:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
		case SSH_CHANNEL_ABANDONED:
		case SSH_CHANNEL_UNIX_LISTENER:
		case SSH_CHANNEL_RUNIX_LISTENER:
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_AUTH_SOCKET:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
			return i;
		default:
			fatal_f("bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	return -1;
}

/* Returns the state of the channel's extended usage flag */
const char *
channel_format_extended_usage(const Channel *c)
{
	if (c->efd == -1)
		return "closed";

	switch (c->extended_usage) {
	case CHAN_EXTENDED_WRITE:
		return "write";
	case CHAN_EXTENDED_READ:
		return "read";
	case CHAN_EXTENDED_IGNORE:
		return "ignore";
	default:
		return "UNKNOWN";
	}
}

static char *
channel_format_status(const Channel *c)
{
	char *ret = NULL;

	xasprintf(&ret, "t%d [%s] %s%u %s%u i%u/%zu o%u/%zu e[%s]/%zu "
	    "fd %d/%d/%d sock %d cc %d %s%u io 0x%02x/0x%02x",
	    c->type, c->xctype != NULL ? c->xctype : c->ctype,
	    c->have_remote_id ? "r" : "nr", c->remote_id,
	    c->mux_ctx != NULL ? "m" : "nm", c->mux_downstream_id,
	    c->istate, sshbuf_len(c->input),
	    c->ostate, sshbuf_len(c->output),
	    channel_format_extended_usage(c), sshbuf_len(c->extended),
	    c->rfd, c->wfd, c->efd, c->sock, c->ctl_chan,
	    c->have_ctl_child_id ? "c" : "nc", c->ctl_child_id,
	    c->io_want, c->io_ready);
	return ret;
}

/*
 * Returns a message describing the currently open forwarded connections,
 * suitable for sending to the client.  The message contains crlf pairs for
 * newlines.
 */
char *
channel_open_message(struct ssh *ssh)
{
	struct sshbuf *buf;
	Channel *c;
	u_int i;
	int r;
	char *cp, *ret;

	if ((buf = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new");
	if ((r = sshbuf_putf(buf,
	    "The following connections are open:\r\n")) != 0)
		fatal_fr(r, "sshbuf_putf");
	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		c = ssh->chanctxt->channels[i];
		if (c == NULL)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_AUTH_SOCKET:
		case SSH_CHANNEL_ZOMBIE:
		case SSH_CHANNEL_ABANDONED:
		case SSH_CHANNEL_MUX_LISTENER:
		case SSH_CHANNEL_UNIX_LISTENER:
		case SSH_CHANNEL_RUNIX_LISTENER:
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_RDYNAMIC_OPEN:
		case SSH_CHANNEL_RDYNAMIC_FINISH:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_MUX_PROXY:
		case SSH_CHANNEL_MUX_CLIENT:
			cp = channel_format_status(c);
			if ((r = sshbuf_putf(buf, "  #%d %.300s (%s)\r\n",
			    c->self, c->remote_name, cp)) != 0) {
				free(cp);
				fatal_fr(r, "sshbuf_putf");
			}
			free(cp);
			continue;
		default:
			fatal_f("bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	if ((ret = sshbuf_dup_string(buf)) == NULL)
		fatal_f("sshbuf_dup_string");
	sshbuf_free(buf);
	return ret;
}

static void
open_preamble(struct ssh *ssh, const char *where, Channel *c, const char *type)
{
	int r;

	if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_OPEN)) != 0 ||
	    (r = sshpkt_put_cstring(ssh, type)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->self)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->local_window)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->local_maxpacket)) != 0) {
		fatal_r(r, "%s: channel %i: open", where, c->self);
	}
}

void
channel_send_open(struct ssh *ssh, int id)
{
	Channel *c = channel_lookup(ssh, id);
	int r;

	if (c == NULL) {
		logit("channel_send_open: %d: bad id", id);
		return;
	}
	debug2("channel %d: send open", id);
	open_preamble(ssh, __func__, c, c->ctype);
	if ((r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "channel %i", c->self);
}

void
channel_request_start(struct ssh *ssh, int id, char *service, int wantconfirm)
{
	Channel *c = channel_lookup(ssh, id);
	int r;

	if (c == NULL) {
		logit_f("%d: unknown channel id", id);
		return;
	}
	if (!c->have_remote_id)
		fatal_f("channel %d: no remote id", c->self);

	debug2("channel %d: request %s confirm %d", id, service, wantconfirm);
	if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_REQUEST)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
	    (r = sshpkt_put_cstring(ssh, service)) != 0 ||
	    (r = sshpkt_put_u8(ssh, wantconfirm)) != 0) {
		fatal_fr(r, "channel %i", c->self);
	}
}

void
channel_register_status_confirm(struct ssh *ssh, int id,
    channel_confirm_cb *cb, channel_confirm_abandon_cb *abandon_cb, void *ctx)
{
	struct channel_confirm *cc;
	Channel *c;

	if ((c = channel_lookup(ssh, id)) == NULL)
		fatal_f("%d: bad id", id);

	cc = xcalloc(1, sizeof(*cc));
	cc->cb = cb;
	cc->abandon_cb = abandon_cb;
	cc->ctx = ctx;
	TAILQ_INSERT_TAIL(&c->status_confirms, cc, entry);
}

void
channel_register_open_confirm(struct ssh *ssh, int id,
    channel_open_fn *fn, void *ctx)
{
	Channel *c = channel_lookup(ssh, id);

	if (c == NULL) {
		logit_f("%d: bad id", id);
		return;
	}
	c->open_confirm = fn;
	c->open_confirm_ctx = ctx;
}

void
channel_register_cleanup(struct ssh *ssh, int id,
    channel_callback_fn *fn, int do_close)
{
	Channel *c = channel_by_id(ssh, id);

	if (c == NULL) {
		logit_f("%d: bad id", id);
		return;
	}
	c->detach_user = fn;
	c->detach_close = do_close;
}

void
channel_cancel_cleanup(struct ssh *ssh, int id)
{
	Channel *c = channel_by_id(ssh, id);

	if (c == NULL) {
		logit_f("%d: bad id", id);
		return;
	}
	c->detach_user = NULL;
	c->detach_close = 0;
}

void
channel_register_filter(struct ssh *ssh, int id, channel_infilter_fn *ifn,
    channel_outfilter_fn *ofn, channel_filter_cleanup_fn *cfn, void *ctx)
{
	Channel *c = channel_lookup(ssh, id);

	if (c == NULL) {
		logit_f("%d: bad id", id);
		return;
	}
	c->input_filter = ifn;
	c->output_filter = ofn;
	c->filter_ctx = ctx;
	c->filter_cleanup = cfn;
}

void
channel_set_fds(struct ssh *ssh, int id, int rfd, int wfd, int efd,
    int extusage, int nonblock, int is_tty, u_int window_max)
{
	Channel *c = channel_lookup(ssh, id);
	int r;

	if (c == NULL || c->type != SSH_CHANNEL_LARVAL)
		fatal("channel_activate for non-larval channel %d.", id);
	if (!c->have_remote_id)
		fatal_f("channel %d: no remote id", c->self);

	channel_register_fds(ssh, c, rfd, wfd, efd, extusage, nonblock, is_tty);
	c->type = SSH_CHANNEL_OPEN;
	channel_set_used_time(ssh, c);
	c->local_window = c->local_window_max = window_max;

	if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_WINDOW_ADJUST)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->local_window)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "channel %i", c->self);
}

static void
channel_pre_listener(struct ssh *ssh, Channel *c)
{
	c->io_want = SSH_CHAN_IO_SOCK_R;
}

static void
channel_pre_connecting(struct ssh *ssh, Channel *c)
{
	debug3("channel %d: waiting for connection", c->self);
	c->io_want = SSH_CHAN_IO_SOCK_W;
}

static void
channel_pre_open(struct ssh *ssh, Channel *c)
{
	c->io_want = 0;
	if (c->istate == CHAN_INPUT_OPEN &&
	    c->remote_window > 0 &&
	    sshbuf_len(c->input) < c->remote_window &&
	    sshbuf_check_reserve(c->input, CHAN_RBUF) == 0)
		c->io_want |= SSH_CHAN_IO_RFD;
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (sshbuf_len(c->output) > 0) {
			c->io_want |= SSH_CHAN_IO_WFD;
		} else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
			if (CHANNEL_EFD_OUTPUT_ACTIVE(c))
				debug2("channel %d: "
				    "obuf_empty delayed efd %d/(%zu)", c->self,
				    c->efd, sshbuf_len(c->extended));
			else
				chan_obuf_empty(ssh, c);
		}
	}
	/** XXX check close conditions, too */
	if (c->efd != -1 && !(c->istate == CHAN_INPUT_CLOSED &&
	    c->ostate == CHAN_OUTPUT_CLOSED)) {
		if (c->extended_usage == CHAN_EXTENDED_WRITE &&
		    sshbuf_len(c->extended) > 0)
			c->io_want |= SSH_CHAN_IO_EFD_W;
		else if (c->efd != -1 && !(c->flags & CHAN_EOF_SENT) &&
		    (c->extended_usage == CHAN_EXTENDED_READ ||
		    c->extended_usage == CHAN_EXTENDED_IGNORE) &&
		    sshbuf_len(c->extended) < c->remote_window)
			c->io_want |= SSH_CHAN_IO_EFD_R;
	}
	/* XXX: What about efd? races? */
}

/*
 * This is a special state for X11 authentication spoofing.  An opened X11
 * connection (when authentication spoofing is being done) remains in this
 * state until the first packet has been completely read.  The authentication
 * data in that packet is then substituted by the real data if it matches the
 * fake data, and the channel is put into normal mode.
 * XXX All this happens at the client side.
 * Returns: 0 = need more data, -1 = wrong cookie, 1 = ok
 */
static int
x11_open_helper(struct ssh *ssh, struct sshbuf *b)
{
	struct ssh_channels *sc = ssh->chanctxt;
	u_char *ucp;
	u_int proto_len, data_len;

	/* Is this being called after the refusal deadline? */
	if (sc->x11_refuse_time != 0 &&
	    monotime() >= sc->x11_refuse_time) {
		verbose("Rejected X11 connection after ForwardX11Timeout "
		    "expired");
		return -1;
	}

	/* Check if the fixed size part of the packet is in buffer. */
	if (sshbuf_len(b) < 12)
		return 0;

	/* Parse the lengths of variable-length fields. */
	ucp = sshbuf_mutable_ptr(b);
	if (ucp[0] == 0x42) {	/* Byte order MSB first. */
		proto_len = 256 * ucp[6] + ucp[7];
		data_len = 256 * ucp[8] + ucp[9];
	} else if (ucp[0] == 0x6c) {	/* Byte order LSB first. */
		proto_len = ucp[6] + 256 * ucp[7];
		data_len = ucp[8] + 256 * ucp[9];
	} else {
		debug2("Initial X11 packet contains bad byte order byte: 0x%x",
		    ucp[0]);
		return -1;
	}

	/* Check if the whole packet is in buffer. */
	if (sshbuf_len(b) <
	    12 + ((proto_len + 3) & ~3) + ((data_len + 3) & ~3))
		return 0;

	/* Check if authentication protocol matches. */
	if (proto_len != strlen(sc->x11_saved_proto) ||
	    memcmp(ucp + 12, sc->x11_saved_proto, proto_len) != 0) {
		debug2("X11 connection uses different authentication protocol.");
		return -1;
	}
	/* Check if authentication data matches our fake data. */
	if (data_len != sc->x11_fake_data_len ||
	    timingsafe_bcmp(ucp + 12 + ((proto_len + 3) & ~3),
		sc->x11_fake_data, sc->x11_fake_data_len) != 0) {
		debug2("X11 auth data does not match fake data.");
		return -1;
	}
	/* Check fake data length */
	if (sc->x11_fake_data_len != sc->x11_saved_data_len) {
		error("X11 fake_data_len %d != saved_data_len %d",
		    sc->x11_fake_data_len, sc->x11_saved_data_len);
		return -1;
	}
	/*
	 * Received authentication protocol and data match
	 * our fake data. Substitute the fake data with real
	 * data.
	 */
	memcpy(ucp + 12 + ((proto_len + 3) & ~3),
	    sc->x11_saved_data, sc->x11_saved_data_len);
	return 1;
}

void
channel_force_close(struct ssh *ssh, Channel *c, int abandon)
{
	debug3_f("channel %d: forcibly closing", c->self);
	if (c->istate == CHAN_INPUT_OPEN)
		chan_read_failed(ssh, c);
	if (c->istate == CHAN_INPUT_WAIT_DRAIN) {
		sshbuf_reset(c->input);
		chan_ibuf_empty(ssh, c);
	}
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		sshbuf_reset(c->output);
		chan_write_failed(ssh, c);
	}
	if (c->detach_user)
		c->detach_user(ssh, c->self, 1, NULL);
	if (c->efd != -1)
		channel_close_fd(ssh, c, &c->efd);
	if (abandon)
		c->type = SSH_CHANNEL_ABANDONED;
	/* exempt from inactivity timeouts */
	c->inactive_deadline = 0;
	c->lastused = 0;
}

static void
channel_pre_x11_open(struct ssh *ssh, Channel *c)
{
	int ret = x11_open_helper(ssh, c->output);

	/* c->force_drain = 1; */

	if (ret == 1) {
		c->type = SSH_CHANNEL_OPEN;
		channel_set_used_time(ssh, c);
		channel_pre_open(ssh, c);
	} else if (ret == -1) {
		logit("X11 connection rejected because of wrong "
		    "authentication.");
		debug2("X11 rejected %d i%d/o%d",
		    c->self, c->istate, c->ostate);
		channel_force_close(ssh, c, 0);
	}
}

static void
channel_pre_mux_client(struct ssh *ssh, Channel *c)
{
	c->io_want = 0;
	if (c->istate == CHAN_INPUT_OPEN && !c->mux_pause &&
	    sshbuf_check_reserve(c->input, CHAN_RBUF) == 0)
		c->io_want |= SSH_CHAN_IO_RFD;
	if (c->istate == CHAN_INPUT_WAIT_DRAIN) {
		/* clear buffer immediately (discard any partial packet) */
		sshbuf_reset(c->input);
		chan_ibuf_empty(ssh, c);
		/* Start output drain. XXX just kill chan? */
		chan_rcvd_oclose(ssh, c);
	}
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (sshbuf_len(c->output) > 0)
			c->io_want |= SSH_CHAN_IO_WFD;
		else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN)
			chan_obuf_empty(ssh, c);
	}
}

/* try to decode a socks4 header */
static int
channel_decode_socks4(Channel *c, struct sshbuf *input, struct sshbuf *output)
{
	const u_char *p;
	char *host;
	u_int len, have, i, found, need;
	char username[256];
	struct {
		u_int8_t version;
		u_int8_t command;
		u_int16_t dest_port;
		struct in_addr dest_addr;
	} s4_req, s4_rsp;
	int r;

	debug2("channel %d: decode socks4", c->self);

	have = sshbuf_len(input);
	len = sizeof(s4_req);
	if (have < len)
		return 0;
	p = sshbuf_ptr(input);

	need = 1;
	/* SOCKS4A uses an invalid IP address 0.0.0.x */
	if (p[4] == 0 && p[5] == 0 && p[6] == 0 && p[7] != 0) {
		debug2("channel %d: socks4a request", c->self);
		/* ... and needs an extra string (the hostname) */
		need = 2;
	}
	/* Check for terminating NUL on the string(s) */
	for (found = 0, i = len; i < have; i++) {
		if (p[i] == '\0') {
			found++;
			if (found == need)
				break;
		}
		if (i > 1024) {
			/* the peer is probably sending garbage */
			debug("channel %d: decode socks4: too long",
			    c->self);
			return -1;
		}
	}
	if (found < need)
		return 0;
	if ((r = sshbuf_get(input, &s4_req.version, 1)) != 0 ||
	    (r = sshbuf_get(input, &s4_req.command, 1)) != 0 ||
	    (r = sshbuf_get(input, &s4_req.dest_port, 2)) != 0 ||
	    (r = sshbuf_get(input, &s4_req.dest_addr, 4)) != 0) {
		debug_r(r, "channels %d: decode socks4", c->self);
		return -1;
	}
	have = sshbuf_len(input);
	p = sshbuf_ptr(input);
	if (memchr(p, '\0', have) == NULL) {
		error("channel %d: decode socks4: unterminated user", c->self);
		return -1;
	}
	len = strlen(p);
	debug2("channel %d: decode socks4: user %s/%d", c->self, p, len);
	len++; /* trailing '\0' */
	strlcpy(username, p, sizeof(username));
	if ((r = sshbuf_consume(input, len)) != 0)
		fatal_fr(r, "channel %d: consume", c->self);
	free(c->path);
	c->path = NULL;
	if (need == 1) {			/* SOCKS4: one string */
		host = inet_ntoa(s4_req.dest_addr);
		c->path = xstrdup(host);
	} else {				/* SOCKS4A: two strings */
		have = sshbuf_len(input);
		p = sshbuf_ptr(input);
		if (memchr(p, '\0', have) == NULL) {
			error("channel %d: decode socks4a: host not nul "
			    "terminated", c->self);
			return -1;
		}
		len = strlen(p);
		debug2("channel %d: decode socks4a: host %s/%d",
		    c->self, p, len);
		len++;				/* trailing '\0' */
		if (len > NI_MAXHOST) {
			error("channel %d: hostname \"%.100s\" too long",
			    c->self, p);
			return -1;
		}
		c->path = xstrdup(p);
		if ((r = sshbuf_consume(input, len)) != 0)
			fatal_fr(r, "channel %d: consume", c->self);
	}
	c->host_port = ntohs(s4_req.dest_port);

	debug2("channel %d: dynamic request: socks4 host %s port %u command %u",
	    c->self, c->path, c->host_port, s4_req.command);

	if (s4_req.command != 1) {
		debug("channel %d: cannot handle: %s cn %d",
		    c->self, need == 1 ? "SOCKS4" : "SOCKS4A", s4_req.command);
		return -1;
	}
	s4_rsp.version = 0;			/* vn: 0 for reply */
	s4_rsp.command = 90;			/* cd: req granted */
	s4_rsp.dest_port = 0;			/* ignored */
	s4_rsp.dest_addr.s_addr = INADDR_ANY;	/* ignored */
	if ((r = sshbuf_put(output, &s4_rsp, sizeof(s4_rsp))) != 0)
		fatal_fr(r, "channel %d: append reply", c->self);
	return 1;
}

/* try to decode a socks5 header */
#define SSH_SOCKS5_AUTHDONE	0x1000
#define SSH_SOCKS5_NOAUTH	0x00
#define SSH_SOCKS5_IPV4		0x01
#define SSH_SOCKS5_DOMAIN	0x03
#define SSH_SOCKS5_IPV6		0x04
#define SSH_SOCKS5_CONNECT	0x01
#define SSH_SOCKS5_SUCCESS	0x00

static int
channel_decode_socks5(Channel *c, struct sshbuf *input, struct sshbuf *output)
{
	/* XXX use get/put_u8 instead of trusting struct padding */
	struct {
		u_int8_t version;
		u_int8_t command;
		u_int8_t reserved;
		u_int8_t atyp;
	} s5_req, s5_rsp;
	u_int16_t dest_port;
	char dest_addr[255+1], ntop[INET6_ADDRSTRLEN];
	const u_char *p;
	u_int have, need, i, found, nmethods, addrlen, af;
	int r;

	debug2("channel %d: decode socks5", c->self);
	p = sshbuf_ptr(input);
	if (p[0] != 0x05)
		return -1;
	have = sshbuf_len(input);
	if (!(c->flags & SSH_SOCKS5_AUTHDONE)) {
		/* format: ver | nmethods | methods */
		if (have < 2)
			return 0;
		nmethods = p[1];
		if (have < nmethods + 2)
			return 0;
		/* look for method: "NO AUTHENTICATION REQUIRED" */
		for (found = 0, i = 2; i < nmethods + 2; i++) {
			if (p[i] == SSH_SOCKS5_NOAUTH) {
				found = 1;
				break;
			}
		}
		if (!found) {
			debug("channel %d: method SSH_SOCKS5_NOAUTH not found",
			    c->self);
			return -1;
		}
		if ((r = sshbuf_consume(input, nmethods + 2)) != 0)
			fatal_fr(r, "channel %d: consume", c->self);
		/* version, method */
		if ((r = sshbuf_put_u8(output, 0x05)) != 0 ||
		    (r = sshbuf_put_u8(output, SSH_SOCKS5_NOAUTH)) != 0)
			fatal_fr(r, "channel %d: append reply", c->self);
		c->flags |= SSH_SOCKS5_AUTHDONE;
		debug2("channel %d: socks5 auth done", c->self);
		return 0;				/* need more */
	}
	debug2("channel %d: socks5 post auth", c->self);
	if (have < sizeof(s5_req)+1)
		return 0;			/* need more */
	memcpy(&s5_req, p, sizeof(s5_req));
	if (s5_req.version != 0x05 ||
	    s5_req.command != SSH_SOCKS5_CONNECT ||
	    s5_req.reserved != 0x00) {
		debug2("channel %d: only socks5 connect supported", c->self);
		return -1;
	}
	switch (s5_req.atyp){
	case SSH_SOCKS5_IPV4:
		addrlen = 4;
		af = AF_INET;
		break;
	case SSH_SOCKS5_DOMAIN:
		addrlen = p[sizeof(s5_req)];
		af = -1;
		break;
	case SSH_SOCKS5_IPV6:
		addrlen = 16;
		af = AF_INET6;
		break;
	default:
		debug2("channel %d: bad socks5 atyp %d", c->self, s5_req.atyp);
		return -1;
	}
	need = sizeof(s5_req) + addrlen + 2;
	if (s5_req.atyp == SSH_SOCKS5_DOMAIN)
		need++;
	if (have < need)
		return 0;
	if ((r = sshbuf_consume(input, sizeof(s5_req))) != 0)
		fatal_fr(r, "channel %d: consume", c->self);
	if (s5_req.atyp == SSH_SOCKS5_DOMAIN) {
		/* host string length */
		if ((r = sshbuf_consume(input, 1)) != 0)
			fatal_fr(r, "channel %d: consume", c->self);
	}
	if ((r = sshbuf_get(input, &dest_addr, addrlen)) != 0 ||
	    (r = sshbuf_get(input, &dest_port, 2)) != 0) {
		debug_r(r, "channel %d: parse addr/port", c->self);
		return -1;
	}
	dest_addr[addrlen] = '\0';
	free(c->path);
	c->path = NULL;
	if (s5_req.atyp == SSH_SOCKS5_DOMAIN) {
		if (addrlen >= NI_MAXHOST) {
			error("channel %d: dynamic request: socks5 hostname "
			    "\"%.100s\" too long", c->self, dest_addr);
			return -1;
		}
		c->path = xstrdup(dest_addr);
	} else {
		if (inet_ntop(af, dest_addr, ntop, sizeof(ntop)) == NULL)
			return -1;
		c->path = xstrdup(ntop);
	}
	c->host_port = ntohs(dest_port);

	debug2("channel %d: dynamic request: socks5 host %s port %u command %u",
	    c->self, c->path, c->host_port, s5_req.command);

	s5_rsp.version = 0x05;
	s5_rsp.command = SSH_SOCKS5_SUCCESS;
	s5_rsp.reserved = 0;			/* ignored */
	s5_rsp.atyp = SSH_SOCKS5_IPV4;
	dest_port = 0;				/* ignored */

	if ((r = sshbuf_put(output, &s5_rsp, sizeof(s5_rsp))) != 0 ||
	    (r = sshbuf_put_u32(output, ntohl(INADDR_ANY))) != 0 ||
	    (r = sshbuf_put(output, &dest_port, sizeof(dest_port))) != 0)
		fatal_fr(r, "channel %d: append reply", c->self);
	return 1;
}

Channel *
channel_connect_stdio_fwd(struct ssh *ssh,
    const char *host_to_connect, int port_to_connect,
    int in, int out, int nonblock)
{
	Channel *c;

	debug_f("%s:%d", host_to_connect, port_to_connect);

	c = channel_new(ssh, "stdio-forward", SSH_CHANNEL_OPENING, in, out,
	    -1, CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
	    0, "stdio-forward", nonblock);

	c->path = xstrdup(host_to_connect);
	c->host_port = port_to_connect;
	c->listening_port = 0;
	c->force_drain = 1;

	channel_register_fds(ssh, c, in, out, -1, 0, 1, 0);
	port_open_helper(ssh, c, port_to_connect == PORT_STREAMLOCAL ?
	    "direct-streamlocal@openssh.com" : "direct-tcpip");

	return c;
}

/* dynamic port forwarding */
static void
channel_pre_dynamic(struct ssh *ssh, Channel *c)
{
	const u_char *p;
	u_int have;
	int ret;

	c->io_want = 0;
	have = sshbuf_len(c->input);
	debug2("channel %d: pre_dynamic: have %d", c->self, have);
	/* sshbuf_dump(c->input, stderr); */
	/* check if the fixed size part of the packet is in buffer. */
	if (have < 3) {
		/* need more */
		c->io_want |= SSH_CHAN_IO_RFD;
		return;
	}
	/* try to guess the protocol */
	p = sshbuf_ptr(c->input);
	/* XXX sshbuf_peek_u8? */
	switch (p[0]) {
	case 0x04:
		ret = channel_decode_socks4(c, c->input, c->output);
		break;
	case 0x05:
		ret = channel_decode_socks5(c, c->input, c->output);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret < 0) {
		chan_mark_dead(ssh, c);
	} else if (ret == 0) {
		debug2("channel %d: pre_dynamic: need more", c->self);
		/* need more */
		c->io_want |= SSH_CHAN_IO_RFD;
		if (sshbuf_len(c->output))
			c->io_want |= SSH_CHAN_IO_WFD;
	} else {
		/* switch to the next state */
		c->type = SSH_CHANNEL_OPENING;
		port_open_helper(ssh, c, "direct-tcpip");
	}
}

/* simulate read-error */
static void
rdynamic_close(struct ssh *ssh, Channel *c)
{
	c->type = SSH_CHANNEL_OPEN;
	channel_force_close(ssh, c, 0);
}

/* reverse dynamic port forwarding */
static void
channel_before_prepare_io_rdynamic(struct ssh *ssh, Channel *c)
{
	const u_char *p;
	u_int have, len;
	int r, ret;

	have = sshbuf_len(c->output);
	debug2("channel %d: pre_rdynamic: have %d", c->self, have);
	/* sshbuf_dump(c->output, stderr); */
	/* EOF received */
	if (c->flags & CHAN_EOF_RCVD) {
		if ((r = sshbuf_consume(c->output, have)) != 0)
			fatal_fr(r, "channel %d: consume", c->self);
		rdynamic_close(ssh, c);
		return;
	}
	/* check if the fixed size part of the packet is in buffer. */
	if (have < 3)
		return;
	/* try to guess the protocol */
	p = sshbuf_ptr(c->output);
	switch (p[0]) {
	case 0x04:
		/* switch input/output for reverse forwarding */
		ret = channel_decode_socks4(c, c->output, c->input);
		break;
	case 0x05:
		ret = channel_decode_socks5(c, c->output, c->input);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret < 0) {
		rdynamic_close(ssh, c);
	} else if (ret == 0) {
		debug2("channel %d: pre_rdynamic: need more", c->self);
		/* send socks request to peer */
		len = sshbuf_len(c->input);
		if (len > 0 && len < c->remote_window) {
			if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_DATA)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
			    (r = sshpkt_put_stringb(ssh, c->input)) != 0 ||
			    (r = sshpkt_send(ssh)) != 0) {
				fatal_fr(r, "channel %i: rdynamic", c->self);
			}
			if ((r = sshbuf_consume(c->input, len)) != 0)
				fatal_fr(r, "channel %d: consume", c->self);
			c->remote_window -= len;
		}
	} else if (rdynamic_connect_finish(ssh, c) < 0) {
		/* the connect failed */
		rdynamic_close(ssh, c);
	}
}

/* This is our fake X11 server socket. */
static void
channel_post_x11_listener(struct ssh *ssh, Channel *c)
{
	Channel *nc;
	struct sockaddr_storage addr;
	int r, newsock, oerrno, remote_port;
	socklen_t addrlen;
	char buf[16384], *remote_ipaddr;

	if ((c->io_ready & SSH_CHAN_IO_SOCK_R) == 0)
		return;

	debug("X11 connection requested.");
	addrlen = sizeof(addr);
	newsock = accept(c->sock, (struct sockaddr *)&addr, &addrlen);
	if (c->single_connection) {
		oerrno = errno;
		debug2("single_connection: closing X11 listener.");
		channel_close_fd(ssh, c, &c->sock);
		chan_mark_dead(ssh, c);
		errno = oerrno;
	}
	if (newsock == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK &&
		    errno != ECONNABORTED)
			error("accept: %.100s", strerror(errno));
		if (errno == EMFILE || errno == ENFILE)
			c->notbefore = monotime() + 1;
		return;
	}
	set_nodelay(newsock);
	remote_ipaddr = get_peer_ipaddr(newsock);
	remote_port = get_peer_port(newsock);
	snprintf(buf, sizeof buf, "X11 connection from %.200s port %d",
	    remote_ipaddr, remote_port);

	nc = channel_new(ssh, "x11-connection",
	    SSH_CHANNEL_OPENING, newsock, newsock, -1,
	    c->local_window_max, c->local_maxpacket, 0, buf, 1);
	open_preamble(ssh, __func__, nc, "x11");
	if ((r = sshpkt_put_cstring(ssh, remote_ipaddr)) != 0 ||
	    (r = sshpkt_put_u32(ssh, remote_port)) != 0) {
		fatal_fr(r, "channel %i: reply", c->self);
	}
	if ((r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "channel %i: send", c->self);
	free(remote_ipaddr);
}

static void
port_open_helper(struct ssh *ssh, Channel *c, char *rtype)
{
	char *local_ipaddr = get_local_ipaddr(c->sock);
	int local_port = c->sock == -1 ? 65536 : get_local_port(c->sock);
	char *remote_ipaddr = get_peer_ipaddr(c->sock);
	int remote_port = get_peer_port(c->sock);
	int r;

	if (remote_port == -1) {
		/* Fake addr/port to appease peers that validate it (Tectia) */
		free(remote_ipaddr);
		remote_ipaddr = xstrdup("127.0.0.1");
		remote_port = 65535;
	}

	free(c->remote_name);
	xasprintf(&c->remote_name,
	    "%s: listening port %d for %.100s port %d, "
	    "connect from %.200s port %d to %.100s port %d",
	    rtype, c->listening_port, c->path, c->host_port,
	    remote_ipaddr, remote_port, local_ipaddr, local_port);

	open_preamble(ssh, __func__, c, rtype);
	if (strcmp(rtype, "direct-tcpip") == 0) {
		/* target host, port */
		if ((r = sshpkt_put_cstring(ssh, c->path)) != 0 ||
		    (r = sshpkt_put_u32(ssh, c->host_port)) != 0)
			fatal_fr(r, "channel %i: reply", c->self);
	} else if (strcmp(rtype, "direct-streamlocal@openssh.com") == 0) {
		/* target path */
		if ((r = sshpkt_put_cstring(ssh, c->path)) != 0)
			fatal_fr(r, "channel %i: reply", c->self);
	} else if (strcmp(rtype, "forwarded-streamlocal@openssh.com") == 0) {
		/* listen path */
		if ((r = sshpkt_put_cstring(ssh, c->path)) != 0)
			fatal_fr(r, "channel %i: reply", c->self);
	} else {
		/* listen address, port */
		if ((r = sshpkt_put_cstring(ssh, c->path)) != 0 ||
		    (r = sshpkt_put_u32(ssh, local_port)) != 0)
			fatal_fr(r, "channel %i: reply", c->self);
	}
	if (strcmp(rtype, "forwarded-streamlocal@openssh.com") == 0) {
		/* reserved for future owner/mode info */
		if ((r = sshpkt_put_cstring(ssh, "")) != 0)
			fatal_fr(r, "channel %i: reply", c->self);
	} else {
		/* originator host and port */
		if ((r = sshpkt_put_cstring(ssh, remote_ipaddr)) != 0 ||
		    (r = sshpkt_put_u32(ssh, (u_int)remote_port)) != 0)
			fatal_fr(r, "channel %i: reply", c->self);
	}
	if ((r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "channel %i: send", c->self);
	free(remote_ipaddr);
	free(local_ipaddr);
}

void
channel_set_x11_refuse_time(struct ssh *ssh, time_t refuse_time)
{
	ssh->chanctxt->x11_refuse_time = refuse_time;
}

/*
 * This socket is listening for connections to a forwarded TCP/IP port.
 */
static void
channel_post_port_listener(struct ssh *ssh, Channel *c)
{
	Channel *nc;
	struct sockaddr_storage addr;
	int newsock, nextstate;
	socklen_t addrlen;
	char *rtype;

	if ((c->io_ready & SSH_CHAN_IO_SOCK_R) == 0)
		return;

	debug("Connection to port %d forwarding to %.100s port %d requested.",
	    c->listening_port, c->path, c->host_port);

	if (c->type == SSH_CHANNEL_RPORT_LISTENER) {
		nextstate = SSH_CHANNEL_OPENING;
		rtype = "forwarded-tcpip";
	} else if (c->type == SSH_CHANNEL_RUNIX_LISTENER) {
		nextstate = SSH_CHANNEL_OPENING;
		rtype = "forwarded-streamlocal@openssh.com";
	} else if (c->host_port == PORT_STREAMLOCAL) {
		nextstate = SSH_CHANNEL_OPENING;
		rtype = "direct-streamlocal@openssh.com";
	} else if (c->host_port == 0) {
		nextstate = SSH_CHANNEL_DYNAMIC;
		rtype = "dynamic-tcpip";
	} else {
		nextstate = SSH_CHANNEL_OPENING;
		rtype = "direct-tcpip";
	}

	addrlen = sizeof(addr);
	newsock = accept(c->sock, (struct sockaddr *)&addr, &addrlen);
	if (newsock == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK &&
		    errno != ECONNABORTED)
			error("accept: %.100s", strerror(errno));
		if (errno == EMFILE || errno == ENFILE)
			c->notbefore = monotime() + 1;
		return;
	}
	if (c->host_port != PORT_STREAMLOCAL)
		set_nodelay(newsock);
	nc = channel_new(ssh, rtype, nextstate, newsock, newsock, -1,
	    c->local_window_max, c->local_maxpacket, 0, rtype, 1);
	nc->listening_port = c->listening_port;
	nc->host_port = c->host_port;
	if (c->path != NULL)
		nc->path = xstrdup(c->path);

	if (nextstate != SSH_CHANNEL_DYNAMIC)
		port_open_helper(ssh, nc, rtype);
}

/*
 * This is the authentication agent socket listening for connections from
 * clients.
 */
static void
channel_post_auth_listener(struct ssh *ssh, Channel *c)
{
	Channel *nc;
	int r, newsock;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	if ((c->io_ready & SSH_CHAN_IO_SOCK_R) == 0)
		return;

	addrlen = sizeof(addr);
	newsock = accept(c->sock, (struct sockaddr *)&addr, &addrlen);
	if (newsock == -1) {
		error("accept from auth socket: %.100s", strerror(errno));
		if (errno == EMFILE || errno == ENFILE)
			c->notbefore = monotime() + 1;
		return;
	}
	nc = channel_new(ssh, "agent-connection",
	    SSH_CHANNEL_OPENING, newsock, newsock, -1,
	    c->local_window_max, c->local_maxpacket,
	    0, "accepted auth socket", 1);
	open_preamble(ssh, __func__, nc, "auth-agent@openssh.com");
	if ((r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "channel %i", c->self);
}

static void
channel_post_connecting(struct ssh *ssh, Channel *c)
{
	int err = 0, sock, isopen, r;
	socklen_t sz = sizeof(err);

	if ((c->io_ready & SSH_CHAN_IO_SOCK_W) == 0)
		return;
	if (!c->have_remote_id)
		fatal_f("channel %d: no remote id", c->self);
	/* for rdynamic the OPEN_CONFIRMATION has been sent already */
	isopen = (c->type == SSH_CHANNEL_RDYNAMIC_FINISH);

	if (getsockopt(c->sock, SOL_SOCKET, SO_ERROR, &err, &sz) == -1) {
		err = errno;
		error("getsockopt SO_ERROR failed");
	}

	if (err == 0) {
		/* Non-blocking connection completed */
		debug("channel %d: connected to %s port %d",
		    c->self, c->connect_ctx.host, c->connect_ctx.port);
		channel_connect_ctx_free(&c->connect_ctx);
		c->type = SSH_CHANNEL_OPEN;
		channel_set_used_time(ssh, c);
		if (isopen) {
			/* no message necessary */
		} else {
			if ((r = sshpkt_start(ssh,
			    SSH2_MSG_CHANNEL_OPEN_CONFIRMATION)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->self)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->local_window)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->local_maxpacket)) != 0 ||
			    (r = sshpkt_send(ssh)) != 0)
				fatal_fr(r, "channel %i open confirm", c->self);
		}
		return;
	}
	if (err == EINTR || err == EAGAIN || err == EINPROGRESS)
		return;

	/* Non-blocking connection failed */
	debug("channel %d: connection failed: %s", c->self, strerror(err));

	/* Try next address, if any */
	if ((sock = connect_next(&c->connect_ctx)) == -1) {
		/* Exhausted all addresses for this destination */
		error("connect_to %.100s port %d: failed.",
		    c->connect_ctx.host, c->connect_ctx.port);
		channel_connect_ctx_free(&c->connect_ctx);
		if (isopen) {
			rdynamic_close(ssh, c);
		} else {
			if ((r = sshpkt_start(ssh,
			    SSH2_MSG_CHANNEL_OPEN_FAILURE)) != 0 ||
			    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
			    (r = sshpkt_put_u32(ssh,
			    SSH2_OPEN_CONNECT_FAILED)) != 0 ||
			    (r = sshpkt_put_cstring(ssh, strerror(err))) != 0 ||
			    (r = sshpkt_put_cstring(ssh, "")) != 0 ||
			    (r = sshpkt_send(ssh)) != 0)
				fatal_fr(r, "channel %i: failure", c->self);
			chan_mark_dead(ssh, c);
		}
	}

	/* New non-blocking connection in progress */
	close(c->sock);
	c->sock = c->rfd = c->wfd = sock;
}

static int
channel_handle_rfd(struct ssh *ssh, Channel *c)
{
	char buf[CHAN_RBUF];
	ssize_t len;
	int r, force;
	size_t nr = 0, have, avail, maxlen = CHANNEL_MAX_READ;
	int pty_zeroread = 0;

#ifdef PTY_ZEROREAD
	/* Bug on AIX: read(1) can return 0 for a non-closed fd */
	pty_zeroread = c->isatty;
#endif

	force = c->isatty && c->detach_close && c->istate != CHAN_INPUT_CLOSED;

	if (!force && (c->io_ready & SSH_CHAN_IO_RFD) == 0)
		return 1;
	if ((avail = sshbuf_avail(c->input)) == 0)
		return 1; /* Shouldn't happen */

	/*
	 * For "simple" channels (i.e. not datagram or filtered), we can
	 * read directly to the channel buffer.
	 */
	if (!pty_zeroread && c->input_filter == NULL && !c->datagram) {
		/* Only OPEN channels have valid rwin */
		if (c->type == SSH_CHANNEL_OPEN) {
			if ((have = sshbuf_len(c->input)) >= c->remote_window)
				return 1; /* shouldn't happen */
			if (maxlen > c->remote_window - have)
				maxlen = c->remote_window - have;
		}
		if (maxlen > avail)
			maxlen = avail;
		if ((r = sshbuf_read(c->rfd, c->input, maxlen, &nr)) != 0) {
			if (errno == EINTR || (!force &&
			    (errno == EAGAIN || errno == EWOULDBLOCK)))
				return 1;
			debug2("channel %d: read failed rfd %d maxlen %zu: %s",
			    c->self, c->rfd, maxlen, ssh_err(r));
			goto rfail;
		}
		if (nr != 0)
			channel_set_used_time(ssh, c);
		return 1;
	}

	errno = 0;
	len = read(c->rfd, buf, sizeof(buf));
	/* fixup AIX zero-length read with errno set to look more like errors */
	if (pty_zeroread && len == 0 && errno != 0)
		len = -1;
	if (len == -1 && (errno == EINTR ||
	    ((errno == EAGAIN || errno == EWOULDBLOCK) && !force)))
		return 1;
	if (len < 0 || (!pty_zeroread && len == 0)) {
		debug2("channel %d: read<=0 rfd %d len %zd: %s",
		    c->self, c->rfd, len,
		    len == 0 ? "closed" : strerror(errno));
 rfail:
		if (c->type != SSH_CHANNEL_OPEN) {
			debug2("channel %d: not open", c->self);
			chan_mark_dead(ssh, c);
			return -1;
		} else {
			chan_read_failed(ssh, c);
		}
		return -1;
	}
	channel_set_used_time(ssh, c);
	if (c->input_filter != NULL) {
		if (c->input_filter(ssh, c, buf, len) == -1) {
			debug2("channel %d: filter stops", c->self);
			chan_read_failed(ssh, c);
		}
	} else if (c->datagram) {
		if ((r = sshbuf_put_string(c->input, buf, len)) != 0)
			fatal_fr(r, "channel %i: put datagram", c->self);
	} else if ((r = sshbuf_put(c->input, buf, len)) != 0)
		fatal_fr(r, "channel %i: put data", c->self);

	return 1;
}

static int
channel_handle_wfd(struct ssh *ssh, Channel *c)
{
	struct termios tio;
	u_char *data = NULL, *buf; /* XXX const; need filter API change */
	size_t dlen, olen = 0;
	int r, len;

	if ((c->io_ready & SSH_CHAN_IO_WFD) == 0)
		return 1;
	if (sshbuf_len(c->output) == 0)
		return 1;

	/* Send buffered output data to the socket. */
	olen = sshbuf_len(c->output);
	if (c->output_filter != NULL) {
		if ((buf = c->output_filter(ssh, c, &data, &dlen)) == NULL) {
			debug2("channel %d: filter stops", c->self);
			if (c->type != SSH_CHANNEL_OPEN)
				chan_mark_dead(ssh, c);
			else
				chan_write_failed(ssh, c);
			return -1;
		}
	} else if (c->datagram) {
		if ((r = sshbuf_get_string(c->output, &data, &dlen)) != 0)
			fatal_fr(r, "channel %i: get datagram", c->self);
		buf = data;
	} else {
		buf = data = sshbuf_mutable_ptr(c->output);
		dlen = sshbuf_len(c->output);
	}

	if (c->datagram) {
		/* ignore truncated writes, datagrams might get lost */
		len = write(c->wfd, buf, dlen);
		free(data);
		if (len == -1 && (errno == EINTR || errno == EAGAIN ||
		    errno == EWOULDBLOCK))
			return 1;
		if (len <= 0)
			goto write_fail;
		goto out;
	}

#ifdef _AIX
	/* XXX: Later AIX versions can't push as much data to tty */
	if (c->wfd_isatty)
		dlen = MINIMUM(dlen, 8*1024);
#endif

	len = write(c->wfd, buf, dlen);
	if (len == -1 &&
	    (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
		return 1;
	if (len <= 0) {
 write_fail:
		if (c->type != SSH_CHANNEL_OPEN) {
			debug2("channel %d: not open", c->self);
			chan_mark_dead(ssh, c);
			return -1;
		} else {
			chan_write_failed(ssh, c);
		}
		return -1;
	}
	channel_set_used_time(ssh, c);
#ifndef BROKEN_TCGETATTR_ICANON
	if (c->isatty && dlen >= 1 && buf[0] != '\r') {
		if (tcgetattr(c->wfd, &tio) == 0 &&
		    !(tio.c_lflag & ECHO) && (tio.c_lflag & ICANON)) {
			/*
			 * Simulate echo to reduce the impact of
			 * traffic analysis. We need to match the
			 * size of a SSH2_MSG_CHANNEL_DATA message
			 * (4 byte channel id + buf)
			 */
			if ((r = sshpkt_msg_ignore(ssh, 4+len)) != 0 ||
			    (r = sshpkt_send(ssh)) != 0)
				fatal_fr(r, "channel %i: ignore", c->self);
		}
	}
#endif /* BROKEN_TCGETATTR_ICANON */
	if ((r = sshbuf_consume(c->output, len)) != 0)
		fatal_fr(r, "channel %i: consume", c->self);
 out:
	c->local_consumed += olen - sshbuf_len(c->output);

	return 1;
}

static int
channel_handle_efd_write(struct ssh *ssh, Channel *c)
{
	int r;
	ssize_t len;

	if ((c->io_ready & SSH_CHAN_IO_EFD_W) == 0)
		return 1;
	if (sshbuf_len(c->extended) == 0)
		return 1;

	len = write(c->efd, sshbuf_ptr(c->extended),
	    sshbuf_len(c->extended));
	debug2("channel %d: written %zd to efd %d", c->self, len, c->efd);
	if (len == -1 && (errno == EINTR || errno == EAGAIN ||
	    errno == EWOULDBLOCK))
		return 1;
	if (len <= 0) {
		debug2("channel %d: closing write-efd %d", c->self, c->efd);
		channel_close_fd(ssh, c, &c->efd);
	} else {
		if ((r = sshbuf_consume(c->extended, len)) != 0)
			fatal_fr(r, "channel %i: consume", c->self);
		c->local_consumed += len;
		channel_set_used_time(ssh, c);
	}
	return 1;
}

static int
channel_handle_efd_read(struct ssh *ssh, Channel *c)
{
	char buf[CHAN_RBUF];
	ssize_t len;
	int r, force;

	force = c->isatty && c->detach_close && c->istate != CHAN_INPUT_CLOSED;

	if (!force && (c->io_ready & SSH_CHAN_IO_EFD_R) == 0)
		return 1;

	len = read(c->efd, buf, sizeof(buf));
	debug2("channel %d: read %zd from efd %d", c->self, len, c->efd);
	if (len == -1 && (errno == EINTR || ((errno == EAGAIN ||
	    errno == EWOULDBLOCK) && !force)))
		return 1;
	if (len <= 0) {
		debug2("channel %d: closing read-efd %d", c->self, c->efd);
		channel_close_fd(ssh, c, &c->efd);
		return 1;
	}
	channel_set_used_time(ssh, c);
	if (c->extended_usage == CHAN_EXTENDED_IGNORE)
		debug3("channel %d: discard efd", c->self);
	else if ((r = sshbuf_put(c->extended, buf, len)) != 0)
		fatal_fr(r, "channel %i: append", c->self);
	return 1;
}

static int
channel_handle_efd(struct ssh *ssh, Channel *c)
{
	if (c->efd == -1)
		return 1;

	/** XXX handle drain efd, too */

	if (c->extended_usage == CHAN_EXTENDED_WRITE)
		return channel_handle_efd_write(ssh, c);
	else if (c->extended_usage == CHAN_EXTENDED_READ ||
	    c->extended_usage == CHAN_EXTENDED_IGNORE)
		return channel_handle_efd_read(ssh, c);

	return 1;
}

static int
channel_check_window(struct ssh *ssh, Channel *c)
{
	int r;

	if (c->type == SSH_CHANNEL_OPEN &&
	    !(c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD)) &&
	    ((c->local_window_max - c->local_window >
	    c->local_maxpacket*3) ||
	    c->local_window < c->local_window_max/2) &&
	    c->local_consumed > 0) {
		if (!c->have_remote_id)
			fatal_f("channel %d: no remote id", c->self);
		if ((r = sshpkt_start(ssh,
		    SSH2_MSG_CHANNEL_WINDOW_ADJUST)) != 0 ||
		    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
		    (r = sshpkt_put_u32(ssh, c->local_consumed)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0) {
			fatal_fr(r, "channel %i", c->self);
		}
		debug2("channel %d: window %d sent adjust %d", c->self,
		    c->local_window, c->local_consumed);
		c->local_window += c->local_consumed;
		c->local_consumed = 0;
	}
	return 1;
}

static void
channel_post_open(struct ssh *ssh, Channel *c)
{
	channel_handle_rfd(ssh, c);
	channel_handle_wfd(ssh, c);
	channel_handle_efd(ssh, c);
	channel_check_window(ssh, c);
}

static u_int
read_mux(struct ssh *ssh, Channel *c, u_int need)
{
	char buf[CHAN_RBUF];
	ssize_t len;
	u_int rlen;
	int r;

	if (sshbuf_len(c->input) < need) {
		rlen = need - sshbuf_len(c->input);
		len = read(c->rfd, buf, MINIMUM(rlen, CHAN_RBUF));
		if (len == -1 && (errno == EINTR || errno == EAGAIN))
			return sshbuf_len(c->input);
		if (len <= 0) {
			debug2("channel %d: ctl read<=0 rfd %d len %zd",
			    c->self, c->rfd, len);
			chan_read_failed(ssh, c);
			return 0;
		} else if ((r = sshbuf_put(c->input, buf, len)) != 0)
			fatal_fr(r, "channel %i: append", c->self);
	}
	return sshbuf_len(c->input);
}

static void
channel_post_mux_client_read(struct ssh *ssh, Channel *c)
{
	u_int need;

	if ((c->io_ready & SSH_CHAN_IO_RFD) == 0)
		return;
	if (c->istate != CHAN_INPUT_OPEN && c->istate != CHAN_INPUT_WAIT_DRAIN)
		return;
	if (c->mux_pause)
		return;

	/*
	 * Don't not read past the precise end of packets to
	 * avoid disrupting fd passing.
	 */
	if (read_mux(ssh, c, 4) < 4) /* read header */
		return;
	/* XXX sshbuf_peek_u32 */
	need = PEEK_U32(sshbuf_ptr(c->input));
#define CHANNEL_MUX_MAX_PACKET	(256 * 1024)
	if (need > CHANNEL_MUX_MAX_PACKET) {
		debug2("channel %d: packet too big %u > %u",
		    c->self, CHANNEL_MUX_MAX_PACKET, need);
		chan_rcvd_oclose(ssh, c);
		return;
	}
	if (read_mux(ssh, c, need + 4) < need + 4) /* read body */
		return;
	if (c->mux_rcb(ssh, c) != 0) {
		debug("channel %d: mux_rcb failed", c->self);
		chan_mark_dead(ssh, c);
		return;
	}
}

static void
channel_post_mux_client_write(struct ssh *ssh, Channel *c)
{
	ssize_t len;
	int r;

	if ((c->io_ready & SSH_CHAN_IO_WFD) == 0)
		return;
	if (sshbuf_len(c->output) == 0)
		return;

	len = write(c->wfd, sshbuf_ptr(c->output), sshbuf_len(c->output));
	if (len == -1 && (errno == EINTR || errno == EAGAIN))
		return;
	if (len <= 0) {
		chan_mark_dead(ssh, c);
		return;
	}
	if ((r = sshbuf_consume(c->output, len)) != 0)
		fatal_fr(r, "channel %i: consume", c->self);
}

static void
channel_post_mux_client(struct ssh *ssh, Channel *c)
{
	channel_post_mux_client_read(ssh, c);
	channel_post_mux_client_write(ssh, c);
}

static void
channel_post_mux_listener(struct ssh *ssh, Channel *c)
{
	Channel *nc;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int newsock;
	uid_t euid;
	gid_t egid;

	if ((c->io_ready & SSH_CHAN_IO_SOCK_R) == 0)
		return;

	debug("multiplexing control connection");

	/*
	 * Accept connection on control socket
	 */
	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);
	if ((newsock = accept(c->sock, (struct sockaddr*)&addr,
	    &addrlen)) == -1) {
		error_f("accept: %s", strerror(errno));
		if (errno == EMFILE || errno == ENFILE)
			c->notbefore = monotime() + 1;
		return;
	}

	if (getpeereid(newsock, &euid, &egid) == -1) {
		error_f("getpeereid failed: %s", strerror(errno));
		close(newsock);
		return;
	}
	if ((euid != 0) && (getuid() != euid)) {
		error("multiplex uid mismatch: peer euid %u != uid %u",
		    (u_int)euid, (u_int)getuid());
		close(newsock);
		return;
	}
	nc = channel_new(ssh, "mux-control", SSH_CHANNEL_MUX_CLIENT,
	    newsock, newsock, -1, c->local_window_max,
	    c->local_maxpacket, 0, "mux-control", 1);
	nc->mux_rcb = c->mux_rcb;
	debug3_f("new mux channel %d fd %d", nc->self, nc->sock);
	/* establish state */
	nc->mux_rcb(ssh, nc);
	/* mux state transitions must not elicit protocol messages */
	nc->flags |= CHAN_LOCAL;
}

static void
channel_handler_init(struct ssh_channels *sc)
{
	chan_fn **pre, **post;

	if ((pre = calloc(SSH_CHANNEL_MAX_TYPE, sizeof(*pre))) == NULL ||
	    (post = calloc(SSH_CHANNEL_MAX_TYPE, sizeof(*post))) == NULL)
		fatal_f("allocation failed");

	pre[SSH_CHANNEL_OPEN] =			&channel_pre_open;
	pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open;
	pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	pre[SSH_CHANNEL_RPORT_LISTENER] =	&channel_pre_listener;
	pre[SSH_CHANNEL_UNIX_LISTENER] =	&channel_pre_listener;
	pre[SSH_CHANNEL_RUNIX_LISTENER] =	&channel_pre_listener;
	pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	pre[SSH_CHANNEL_CONNECTING] =		&channel_pre_connecting;
	pre[SSH_CHANNEL_DYNAMIC] =		&channel_pre_dynamic;
	pre[SSH_CHANNEL_RDYNAMIC_FINISH] =	&channel_pre_connecting;
	pre[SSH_CHANNEL_MUX_LISTENER] =		&channel_pre_listener;
	pre[SSH_CHANNEL_MUX_CLIENT] =		&channel_pre_mux_client;

	post[SSH_CHANNEL_OPEN] =		&channel_post_open;
	post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	post[SSH_CHANNEL_RPORT_LISTENER] =	&channel_post_port_listener;
	post[SSH_CHANNEL_UNIX_LISTENER] =	&channel_post_port_listener;
	post[SSH_CHANNEL_RUNIX_LISTENER] =	&channel_post_port_listener;
	post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open;
	post[SSH_CHANNEL_RDYNAMIC_FINISH] =	&channel_post_connecting;
	post[SSH_CHANNEL_MUX_LISTENER] =	&channel_post_mux_listener;
	post[SSH_CHANNEL_MUX_CLIENT] =		&channel_post_mux_client;

	sc->channel_pre = pre;
	sc->channel_post = post;
}

/* gc dead channels */
static void
channel_garbage_collect(struct ssh *ssh, Channel *c)
{
	if (c == NULL)
		return;
	if (c->detach_user != NULL) {
		if (!chan_is_dead(ssh, c, c->detach_close))
			return;

		debug2("channel %d: gc: notify user", c->self);
		c->detach_user(ssh, c->self, 0, NULL);
		/* if we still have a callback */
		if (c->detach_user != NULL)
			return;
		debug2("channel %d: gc: user detached", c->self);
	}
	if (!chan_is_dead(ssh, c, 1))
		return;
	debug2("channel %d: garbage collecting", c->self);
	channel_free(ssh, c);
}

enum channel_table { CHAN_PRE, CHAN_POST };

static void
channel_handler(struct ssh *ssh, int table, struct timespec *timeout)
{
	struct ssh_channels *sc = ssh->chanctxt;
	chan_fn **ftab = table == CHAN_PRE ? sc->channel_pre : sc->channel_post;
	u_int i, oalloc;
	Channel *c;
	time_t now;

	now = monotime();
	for (i = 0, oalloc = sc->channels_alloc; i < oalloc; i++) {
		c = sc->channels[i];
		if (c == NULL)
			continue;
		/* Try to keep IO going while rekeying */
		if (ssh_packet_is_rekeying(ssh) && c->type != SSH_CHANNEL_OPEN)
			continue;
		if (c->delayed) {
			if (table == CHAN_PRE)
				c->delayed = 0;
			else
				continue;
		}
		if (ftab[c->type] != NULL) {
			if (table == CHAN_PRE && c->type == SSH_CHANNEL_OPEN &&
			    channel_get_expiry(ssh, c) != 0 &&
			    now >= channel_get_expiry(ssh, c)) {
				/* channel closed for inactivity */
				verbose("channel %d: closing after %u seconds "
				    "of inactivity", c->self,
				    c->inactive_deadline);
				channel_force_close(ssh, c, 1);
			} else if (c->notbefore <= now) {
				/* Run handlers that are not paused. */
				(*ftab[c->type])(ssh, c);
				/* inactivity timeouts must interrupt poll() */
				if (timeout != NULL &&
				    c->type == SSH_CHANNEL_OPEN &&
				    channel_get_expiry(ssh, c) != 0) {
					ptimeout_deadline_monotime(timeout,
					    channel_get_expiry(ssh, c));
				}
			} else if (timeout != NULL) {
				/*
				 * Arrange for poll() wakeup when channel pause
				 * timer expires.
				 */
				ptimeout_deadline_monotime(timeout,
				    c->notbefore);
			}
		}
		channel_garbage_collect(ssh, c);
	}
}

/*
 * Create sockets before preparing IO.
 * This is necessary for things that need to happen after reading
 * the network-input but need to be completed before IO event setup, e.g.
 * because they may create new channels.
 */
static void
channel_before_prepare_io(struct ssh *ssh)
{
	struct ssh_channels *sc = ssh->chanctxt;
	Channel *c;
	u_int i, oalloc;

	for (i = 0, oalloc = sc->channels_alloc; i < oalloc; i++) {
		c = sc->channels[i];
		if (c == NULL)
			continue;
		if (c->type == SSH_CHANNEL_RDYNAMIC_OPEN)
			channel_before_prepare_io_rdynamic(ssh, c);
	}
}

static void
dump_channel_poll(const char *func, const char *what, Channel *c,
    u_int pollfd_offset, struct pollfd *pfd)
{
#ifdef DEBUG_CHANNEL_POLL
	debug3("%s: channel %d: %s r%d w%d e%d s%d c->pfds [ %d %d %d %d ] "
	    "io_want 0x%02x io_ready 0x%02x pfd[%u].fd=%d "
	    "pfd.ev 0x%02x pfd.rev 0x%02x", func, c->self, what,
	    c->rfd, c->wfd, c->efd, c->sock,
	    c->pfds[0], c->pfds[1], c->pfds[2], c->pfds[3],
	    c->io_want, c->io_ready,
	    pollfd_offset, pfd->fd, pfd->events, pfd->revents);
#endif
}

/* Prepare pollfd entries for a single channel */
static void
channel_prepare_pollfd(Channel *c, u_int *next_pollfd,
    struct pollfd *pfd, u_int npfd)
{
	u_int ev, p = *next_pollfd;

	if (c == NULL)
		return;
	if (p + 4 > npfd) {
		/* Shouldn't happen */
		fatal_f("channel %d: bad pfd offset %u (max %u)",
		    c->self, p, npfd);
	}
	c->pfds[0] = c->pfds[1] = c->pfds[2] = c->pfds[3] = -1;
	/*
	 * prepare c->rfd
	 *
	 * This is a special case, since c->rfd might be the same as
	 * c->wfd, c->efd and/or c->sock. Handle those here if they want
	 * IO too.
	 */
	if (c->rfd != -1) {
		ev = 0;
		if ((c->io_want & SSH_CHAN_IO_RFD) != 0)
			ev |= POLLIN;
		/* rfd == wfd */
		if (c->wfd == c->rfd) {
			if ((c->io_want & SSH_CHAN_IO_WFD) != 0)
				ev |= POLLOUT;
		}
		/* rfd == efd */
		if (c->efd == c->rfd) {
			if ((c->io_want & SSH_CHAN_IO_EFD_R) != 0)
				ev |= POLLIN;
			if ((c->io_want & SSH_CHAN_IO_EFD_W) != 0)
				ev |= POLLOUT;
		}
		/* rfd == sock */
		if (c->sock == c->rfd) {
			if ((c->io_want & SSH_CHAN_IO_SOCK_R) != 0)
				ev |= POLLIN;
			if ((c->io_want & SSH_CHAN_IO_SOCK_W) != 0)
				ev |= POLLOUT;
		}
		/* Pack a pfd entry if any event armed for this fd */
		if (ev != 0) {
			c->pfds[0] = p;
			pfd[p].fd = c->rfd;
			pfd[p].events = ev;
			dump_channel_poll(__func__, "rfd", c, p, &pfd[p]);
			p++;
		}
	}
	/* prepare c->wfd if wanting IO and not already handled above */
	if (c->wfd != -1 && c->rfd != c->wfd) {
		ev = 0;
		if ((c->io_want & SSH_CHAN_IO_WFD))
			ev |= POLLOUT;
		/* Pack a pfd entry if any event armed for this fd */
		if (ev != 0) {
			c->pfds[1] = p;
			pfd[p].fd = c->wfd;
			pfd[p].events = ev;
			dump_channel_poll(__func__, "wfd", c, p, &pfd[p]);
			p++;
		}
	}
	/* prepare c->efd if wanting IO and not already handled above */
	if (c->efd != -1 && c->rfd != c->efd) {
		ev = 0;
		if ((c->io_want & SSH_CHAN_IO_EFD_R) != 0)
			ev |= POLLIN;
		if ((c->io_want & SSH_CHAN_IO_EFD_W) != 0)
			ev |= POLLOUT;
		/* Pack a pfd entry if any event armed for this fd */
		if (ev != 0) {
			c->pfds[2] = p;
			pfd[p].fd = c->efd;
			pfd[p].events = ev;
			dump_channel_poll(__func__, "efd", c, p, &pfd[p]);
			p++;
		}
	}
	/* prepare c->sock if wanting IO and not already handled above */
	if (c->sock != -1 && c->rfd != c->sock) {
		ev = 0;
		if ((c->io_want & SSH_CHAN_IO_SOCK_R) != 0)
			ev |= POLLIN;
		if ((c->io_want & SSH_CHAN_IO_SOCK_W) != 0)
			ev |= POLLOUT;
		/* Pack a pfd entry if any event armed for this fd */
		if (ev != 0) {
			c->pfds[3] = p;
			pfd[p].fd = c->sock;
			pfd[p].events = 0;
			dump_channel_poll(__func__, "sock", c, p, &pfd[p]);
			p++;
		}
	}
	*next_pollfd = p;
}

/* * Allocate/prepare poll structure */
void
channel_prepare_poll(struct ssh *ssh, struct pollfd **pfdp, u_int *npfd_allocp,
    u_int *npfd_activep, u_int npfd_reserved, struct timespec *timeout)
{
	struct ssh_channels *sc = ssh->chanctxt;
	u_int i, oalloc, p, npfd = npfd_reserved;

	channel_before_prepare_io(ssh); /* might create a new channel */
	/* clear out I/O flags from last poll */
	for (i = 0; i < sc->channels_alloc; i++) {
		if (sc->channels[i] == NULL)
			continue;
		sc->channels[i]->io_want = sc->channels[i]->io_ready = 0;
	}
	/* Allocate 4x pollfd for each channel (rfd, wfd, efd, sock) */
	if (sc->channels_alloc >= (INT_MAX / 4) - npfd_reserved)
		fatal_f("too many channels"); /* shouldn't happen */
	npfd += sc->channels_alloc * 4;
	if (npfd > *npfd_allocp) {
		*pfdp = xrecallocarray(*pfdp, *npfd_allocp,
		    npfd, sizeof(**pfdp));
		*npfd_allocp = npfd;
	}
	*npfd_activep = npfd_reserved;
	oalloc = sc->channels_alloc;

	channel_handler(ssh, CHAN_PRE, timeout);

	if (oalloc != sc->channels_alloc) {
		/* shouldn't happen */
		fatal_f("channels_alloc changed during CHAN_PRE "
		    "(was %u, now %u)", oalloc, sc->channels_alloc);
	}

	/* Prepare pollfd */
	p = npfd_reserved;
	for (i = 0; i < sc->channels_alloc; i++)
		channel_prepare_pollfd(sc->channels[i], &p, *pfdp, npfd);
	*npfd_activep = p;
}

static void
fd_ready(Channel *c, int p, struct pollfd *pfds, u_int npfd, int fd,
    const char *what, u_int revents_mask, u_int ready)
{
	struct pollfd *pfd = &pfds[p];

	if (fd == -1)
		return;
	if (p == -1 || (u_int)p >= npfd)
		fatal_f("channel %d: bad pfd %d (max %u)", c->self, p, npfd);
	dump_channel_poll(__func__, what, c, p, pfd);
	if (pfd->fd != fd) {
		fatal("channel %d: inconsistent %s fd=%d pollfd[%u].fd %d "
		    "r%d w%d e%d s%d", c->self, what, fd, p, pfd->fd,
		    c->rfd, c->wfd, c->efd, c->sock);
	}
	if ((pfd->revents & POLLNVAL) != 0) {
		fatal("channel %d: invalid %s pollfd[%u].fd %d r%d w%d e%d s%d",
		    c->self, what, p, pfd->fd, c->rfd, c->wfd, c->efd, c->sock);
	}
	if ((pfd->revents & (revents_mask|POLLHUP|POLLERR)) != 0)
		c->io_ready |= ready & c->io_want;
}

/*
 * After poll, perform any appropriate operations for channels which have
 * events pending.
 */
void
channel_after_poll(struct ssh *ssh, struct pollfd *pfd, u_int npfd)
{
	struct ssh_channels *sc = ssh->chanctxt;
	u_int i;
	int p;
	Channel *c;

#ifdef DEBUG_CHANNEL_POLL
	for (p = 0; p < (int)npfd; p++) {
		if (pfd[p].revents == 0)
			continue;
		debug_f("pfd[%u].fd %d rev 0x%04x",
		    p, pfd[p].fd, pfd[p].revents);
	}
#endif

	/* Convert pollfd into c->io_ready */
	for (i = 0; i < sc->channels_alloc; i++) {
		c = sc->channels[i];
		if (c == NULL)
			continue;
		/* if rfd is shared with efd/sock then wfd should be too */
		if (c->rfd != -1 && c->wfd != -1 && c->rfd != c->wfd &&
		    (c->rfd == c->efd || c->rfd == c->sock)) {
			/* Shouldn't happen */
			fatal_f("channel %d: unexpected fds r%d w%d e%d s%d",
			    c->self, c->rfd, c->wfd, c->efd, c->sock);
		}
		c->io_ready = 0;
		/* rfd, potentially shared with wfd, efd and sock */
		if (c->rfd != -1 && (p = c->pfds[0]) != -1) {
			fd_ready(c, p, pfd, npfd, c->rfd,
			    "rfd", POLLIN, SSH_CHAN_IO_RFD);
			if (c->rfd == c->wfd) {
				fd_ready(c, p, pfd, npfd, c->wfd,
				    "wfd/r", POLLOUT, SSH_CHAN_IO_WFD);
			}
			if (c->rfd == c->efd) {
				fd_ready(c, p, pfd, npfd, c->efd,
				    "efdr/r", POLLIN, SSH_CHAN_IO_EFD_R);
				fd_ready(c, p, pfd, npfd, c->efd,
				    "efdw/r", POLLOUT, SSH_CHAN_IO_EFD_W);
			}
			if (c->rfd == c->sock) {
				fd_ready(c, p, pfd, npfd, c->sock,
				    "sockr/r", POLLIN, SSH_CHAN_IO_SOCK_R);
				fd_ready(c, p, pfd, npfd, c->sock,
				    "sockw/r", POLLOUT, SSH_CHAN_IO_SOCK_W);
			}
			dump_channel_poll(__func__, "rfd", c, p, pfd);
		}
		/* wfd */
		if (c->wfd != -1 && c->wfd != c->rfd &&
		    (p = c->pfds[1]) != -1) {
			fd_ready(c, p, pfd, npfd, c->wfd,
			    "wfd", POLLOUT, SSH_CHAN_IO_WFD);
			dump_channel_poll(__func__, "wfd", c, p, pfd);
		}
		/* efd */
		if (c->efd != -1 && c->efd != c->rfd &&
		    (p = c->pfds[2]) != -1) {
			fd_ready(c, p, pfd, npfd, c->efd,
			    "efdr", POLLIN, SSH_CHAN_IO_EFD_R);
			fd_ready(c, p, pfd, npfd, c->efd,
			    "efdw", POLLOUT, SSH_CHAN_IO_EFD_W);
			dump_channel_poll(__func__, "efd", c, p, pfd);
		}
		/* sock */
		if (c->sock != -1 && c->sock != c->rfd &&
		    (p = c->pfds[3]) != -1) {
			fd_ready(c, p, pfd, npfd, c->sock,
			    "sockr", POLLIN, SSH_CHAN_IO_SOCK_R);
			fd_ready(c, p, pfd, npfd, c->sock,
			    "sockw", POLLOUT, SSH_CHAN_IO_SOCK_W);
			dump_channel_poll(__func__, "sock", c, p, pfd);
		}
	}
	channel_handler(ssh, CHAN_POST, NULL);
}

/*
 * Enqueue data for channels with open or draining c->input.
 * Returns non-zero if a packet was enqueued.
 */
static int
channel_output_poll_input_open(struct ssh *ssh, Channel *c)
{
	size_t len, plen;
	const u_char *pkt;
	int r;

	if ((len = sshbuf_len(c->input)) == 0) {
		if (c->istate == CHAN_INPUT_WAIT_DRAIN) {
			/*
			 * input-buffer is empty and read-socket shutdown:
			 * tell peer, that we will not send more data:
			 * send IEOF.
			 * hack for extended data: delay EOF if EFD still
			 * in use.
			 */
			if (CHANNEL_EFD_INPUT_ACTIVE(c))
				debug2("channel %d: "
				    "ibuf_empty delayed efd %d/(%zu)",
				    c->self, c->efd, sshbuf_len(c->extended));
			else
				chan_ibuf_empty(ssh, c);
		}
		return 0;
	}

	if (!c->have_remote_id)
		fatal_f("channel %d: no remote id", c->self);

	if (c->datagram) {
		/* Check datagram will fit; drop if not */
		if ((r = sshbuf_get_string_direct(c->input, &pkt, &plen)) != 0)
			fatal_fr(r, "channel %i: get datagram", c->self);
		/*
		 * XXX this does tail-drop on the datagram queue which is
		 * usually suboptimal compared to head-drop. Better to have
		 * backpressure at read time? (i.e. read + discard)
		 */
		if (plen > c->remote_window || plen > c->remote_maxpacket) {
			debug("channel %d: datagram too big", c->self);
			return 0;
		}
		/* Enqueue it */
		if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_DATA)) != 0 ||
		    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
		    (r = sshpkt_put_string(ssh, pkt, plen)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0)
			fatal_fr(r, "channel %i: send datagram", c->self);
		c->remote_window -= plen;
		return 1;
	}

	/* Enqueue packet for buffered data. */
	if (len > c->remote_window)
		len = c->remote_window;
	if (len > c->remote_maxpacket)
		len = c->remote_maxpacket;
	if (len == 0)
		return 0;
	if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_DATA)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
	    (r = sshpkt_put_string(ssh, sshbuf_ptr(c->input), len)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "channel %i: send data", c->self);
	if ((r = sshbuf_consume(c->input, len)) != 0)
		fatal_fr(r, "channel %i: consume", c->self);
	c->remote_window -= len;
	return 1;
}

/*
 * Enqueue data for channels with open c->extended in read mode.
 * Returns non-zero if a packet was enqueued.
 */
static int
channel_output_poll_extended_read(struct ssh *ssh, Channel *c)
{
	size_t len;
	int r;

	if ((len = sshbuf_len(c->extended)) == 0)
		return 0;

	debug2("channel %d: rwin %u elen %zu euse %d", c->self,
	    c->remote_window, sshbuf_len(c->extended), c->extended_usage);
	if (len > c->remote_window)
		len = c->remote_window;
	if (len > c->remote_maxpacket)
		len = c->remote_maxpacket;
	if (len == 0)
		return 0;
	if (!c->have_remote_id)
		fatal_f("channel %d: no remote id", c->self);
	if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_EXTENDED_DATA)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
	    (r = sshpkt_put_u32(ssh, SSH2_EXTENDED_DATA_STDERR)) != 0 ||
	    (r = sshpkt_put_string(ssh, sshbuf_ptr(c->extended), len)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "channel %i: data", c->self);
	if ((r = sshbuf_consume(c->extended, len)) != 0)
		fatal_fr(r, "channel %i: consume", c->self);
	c->remote_window -= len;
	debug2("channel %d: sent ext data %zu", c->self, len);
	return 1;
}

/*
 * If there is data to send to the connection, enqueue some of it now.
 * Returns non-zero if data was enqueued.
 */
int
channel_output_poll(struct ssh *ssh)
{
	struct ssh_channels *sc = ssh->chanctxt;
	Channel *c;
	u_int i;
	int ret = 0;

	for (i = 0; i < sc->channels_alloc; i++) {
		c = sc->channels[i];
		if (c == NULL)
			continue;

		/*
		 * We are only interested in channels that can have buffered
		 * incoming data.
		 */
		if (c->type != SSH_CHANNEL_OPEN)
			continue;
		if ((c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD))) {
			/* XXX is this true? */
			debug3("channel %d: will not send data after close",
			    c->self);
			continue;
		}

		/* Get the amount of buffered data for this channel. */
		if (c->istate == CHAN_INPUT_OPEN ||
		    c->istate == CHAN_INPUT_WAIT_DRAIN)
			ret |= channel_output_poll_input_open(ssh, c);
		/* Send extended data, i.e. stderr */
		if (!(c->flags & CHAN_EOF_SENT) &&
		    c->extended_usage == CHAN_EXTENDED_READ)
			ret |= channel_output_poll_extended_read(ssh, c);
	}
	return ret;
}

/* -- mux proxy support  */

/*
 * When multiplexing channel messages for mux clients we have to deal
 * with downstream messages from the mux client and upstream messages
 * from the ssh server:
 * 1) Handling downstream messages is straightforward and happens
 *    in channel_proxy_downstream():
 *    - We forward all messages (mostly) unmodified to the server.
 *    - However, in order to route messages from upstream to the correct
 *      downstream client, we have to replace the channel IDs used by the
 *      mux clients with a unique channel ID because the mux clients might
 *      use conflicting channel IDs.
 *    - so we inspect and change both SSH2_MSG_CHANNEL_OPEN and
 *      SSH2_MSG_CHANNEL_OPEN_CONFIRMATION messages, create a local
 *      SSH_CHANNEL_MUX_PROXY channel and replace the mux clients ID
 *      with the newly allocated channel ID.
 * 2) Upstream messages are received by matching SSH_CHANNEL_MUX_PROXY
 *    channels and processed by channel_proxy_upstream(). The local channel ID
 *    is then translated back to the original mux client ID.
 * 3) In both cases we need to keep track of matching SSH2_MSG_CHANNEL_CLOSE
 *    messages so we can clean up SSH_CHANNEL_MUX_PROXY channels.
 * 4) The SSH_CHANNEL_MUX_PROXY channels also need to closed when the
 *    downstream mux client are removed.
 * 5) Handling SSH2_MSG_CHANNEL_OPEN messages from the upstream server
 *    requires more work, because they are not addressed to a specific
 *    channel. E.g. client_request_forwarded_tcpip() needs to figure
 *    out whether the request is addressed to the local client or a
 *    specific downstream client based on the listen-address/port.
 * 6) Agent and X11-Forwarding have a similar problem and are currently
 *    not supported as the matching session/channel cannot be identified
 *    easily.
 */

/*
 * receive packets from downstream mux clients:
 * channel callback fired on read from mux client, creates
 * SSH_CHANNEL_MUX_PROXY channels and translates channel IDs
 * on channel creation.
 */
int
channel_proxy_downstream(struct ssh *ssh, Channel *downstream)
{
	Channel *c = NULL;
	struct sshbuf *original = NULL, *modified = NULL;
	const u_char *cp;
	char *ctype = NULL, *listen_host = NULL;
	u_char type;
	size_t have;
	int ret = -1, r;
	u_int id, remote_id, listen_port;

	/* sshbuf_dump(downstream->input, stderr); */
	if ((r = sshbuf_get_string_direct(downstream->input, &cp, &have))
	    != 0) {
		error_fr(r, "parse");
		return -1;
	}
	if (have < 2) {
		error_f("short message");
		return -1;
	}
	type = cp[1];
	/* skip padlen + type */
	cp += 2;
	have -= 2;
	if (ssh_packet_log_type(type))
		debug3_f("channel %u: down->up: type %u",
		    downstream->self, type);

	switch (type) {
	case SSH2_MSG_CHANNEL_OPEN:
		if ((original = sshbuf_from(cp, have)) == NULL ||
		    (modified = sshbuf_new()) == NULL) {
			error_f("alloc");
			goto out;
		}
		if ((r = sshbuf_get_cstring(original, &ctype, NULL)) != 0 ||
		    (r = sshbuf_get_u32(original, &id)) != 0) {
			error_fr(r, "parse");
			goto out;
		}
		c = channel_new(ssh, "mux-proxy", SSH_CHANNEL_MUX_PROXY,
		    -1, -1, -1, 0, 0, 0, ctype, 1);
		c->mux_ctx = downstream;	/* point to mux client */
		c->mux_downstream_id = id;	/* original downstream id */
		if ((r = sshbuf_put_cstring(modified, ctype)) != 0 ||
		    (r = sshbuf_put_u32(modified, c->self)) != 0 ||
		    (r = sshbuf_putb(modified, original)) != 0) {
			error_fr(r, "compose");
			channel_free(ssh, c);
			goto out;
		}
		break;
	case SSH2_MSG_CHANNEL_OPEN_CONFIRMATION:
		/*
		 * Almost the same as SSH2_MSG_CHANNEL_OPEN, except then we
		 * need to parse 'remote_id' instead of 'ctype'.
		 */
		if ((original = sshbuf_from(cp, have)) == NULL ||
		    (modified = sshbuf_new()) == NULL) {
			error_f("alloc");
			goto out;
		}
		if ((r = sshbuf_get_u32(original, &remote_id)) != 0 ||
		    (r = sshbuf_get_u32(original, &id)) != 0) {
			error_fr(r, "parse");
			goto out;
		}
		c = channel_new(ssh, "mux-proxy", SSH_CHANNEL_MUX_PROXY,
		    -1, -1, -1, 0, 0, 0, "mux-down-connect", 1);
		c->mux_ctx = downstream;	/* point to mux client */
		c->mux_downstream_id = id;
		c->remote_id = remote_id;
		c->have_remote_id = 1;
		if ((r = sshbuf_put_u32(modified, remote_id)) != 0 ||
		    (r = sshbuf_put_u32(modified, c->self)) != 0 ||
		    (r = sshbuf_putb(modified, original)) != 0) {
			error_fr(r, "compose");
			channel_free(ssh, c);
			goto out;
		}
		break;
	case SSH2_MSG_GLOBAL_REQUEST:
		if ((original = sshbuf_from(cp, have)) == NULL) {
			error_f("alloc");
			goto out;
		}
		if ((r = sshbuf_get_cstring(original, &ctype, NULL)) != 0) {
			error_fr(r, "parse");
			goto out;
		}
		if (strcmp(ctype, "tcpip-forward") != 0) {
			error_f("unsupported request %s", ctype);
			goto out;
		}
		if ((r = sshbuf_get_u8(original, NULL)) != 0 ||
		    (r = sshbuf_get_cstring(original, &listen_host, NULL)) != 0 ||
		    (r = sshbuf_get_u32(original, &listen_port)) != 0) {
			error_fr(r, "parse");
			goto out;
		}
		if (listen_port > 65535) {
			error_f("tcpip-forward for %s: bad port %u",
			    listen_host, listen_port);
			goto out;
		}
		/* Record that connection to this host/port is permitted. */
		permission_set_add(ssh, FORWARD_USER, FORWARD_LOCAL, "<mux>",
		    -1, listen_host, NULL, (int)listen_port, downstream);
		break;
	case SSH2_MSG_CHANNEL_CLOSE:
		if (have < 4)
			break;
		remote_id = PEEK_U32(cp);
		if ((c = channel_by_remote_id(ssh, remote_id)) != NULL) {
			if (c->flags & CHAN_CLOSE_RCVD)
				channel_free(ssh, c);
			else
				c->flags |= CHAN_CLOSE_SENT;
		}
		break;
	}
	if (modified) {
		if ((r = sshpkt_start(ssh, type)) != 0 ||
		    (r = sshpkt_putb(ssh, modified)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0) {
			error_fr(r, "send");
			goto out;
		}
	} else {
		if ((r = sshpkt_start(ssh, type)) != 0 ||
		    (r = sshpkt_put(ssh, cp, have)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0) {
			error_fr(r, "send");
			goto out;
		}
	}
	ret = 0;
 out:
	free(ctype);
	free(listen_host);
	sshbuf_free(original);
	sshbuf_free(modified);
	return ret;
}

/*
 * receive packets from upstream server and de-multiplex packets
 * to correct downstream:
 * implemented as a helper for channel input handlers,
 * replaces local (proxy) channel ID with downstream channel ID.
 */
int
channel_proxy_upstream(Channel *c, int type, u_int32_t seq, struct ssh *ssh)
{
	struct sshbuf *b = NULL;
	Channel *downstream;
	const u_char *cp = NULL;
	size_t len;
	int r;

	/*
	 * When receiving packets from the peer we need to check whether we
	 * need to forward the packets to the mux client. In this case we
	 * restore the original channel id and keep track of CLOSE messages,
	 * so we can cleanup the channel.
	 */
	if (c == NULL || c->type != SSH_CHANNEL_MUX_PROXY)
		return 0;
	if ((downstream = c->mux_ctx) == NULL)
		return 0;
	switch (type) {
	case SSH2_MSG_CHANNEL_CLOSE:
	case SSH2_MSG_CHANNEL_DATA:
	case SSH2_MSG_CHANNEL_EOF:
	case SSH2_MSG_CHANNEL_EXTENDED_DATA:
	case SSH2_MSG_CHANNEL_OPEN_CONFIRMATION:
	case SSH2_MSG_CHANNEL_OPEN_FAILURE:
	case SSH2_MSG_CHANNEL_WINDOW_ADJUST:
	case SSH2_MSG_CHANNEL_SUCCESS:
	case SSH2_MSG_CHANNEL_FAILURE:
	case SSH2_MSG_CHANNEL_REQUEST:
		break;
	default:
		debug2_f("channel %u: unsupported type %u", c->self, type);
		return 0;
	}
	if ((b = sshbuf_new()) == NULL) {
		error_f("alloc reply");
		goto out;
	}
	/* get remaining payload (after id) */
	cp = sshpkt_ptr(ssh, &len);
	if (cp == NULL) {
		error_f("no packet");
		goto out;
	}
	/* translate id and send to muxclient */
	if ((r = sshbuf_put_u8(b, 0)) != 0 ||	/* padlen */
	    (r = sshbuf_put_u8(b, type)) != 0 ||
	    (r = sshbuf_put_u32(b, c->mux_downstream_id)) != 0 ||
	    (r = sshbuf_put(b, cp, len)) != 0 ||
	    (r = sshbuf_put_stringb(downstream->output, b)) != 0) {
		error_fr(r, "compose muxclient");
		goto out;
	}
	/* sshbuf_dump(b, stderr); */
	if (ssh_packet_log_type(type))
		debug3_f("channel %u: up->down: type %u", c->self, type);
 out:
	/* update state */
	switch (type) {
	case SSH2_MSG_CHANNEL_OPEN_CONFIRMATION:
		/* record remote_id for SSH2_MSG_CHANNEL_CLOSE */
		if (cp && len > 4) {
			c->remote_id = PEEK_U32(cp);
			c->have_remote_id = 1;
		}
		break;
	case SSH2_MSG_CHANNEL_CLOSE:
		if (c->flags & CHAN_CLOSE_SENT)
			channel_free(ssh, c);
		else
			c->flags |= CHAN_CLOSE_RCVD;
		break;
	}
	sshbuf_free(b);
	return 1;
}

/* -- protocol input */

/* Parse a channel ID from the current packet */
static int
channel_parse_id(struct ssh *ssh, const char *where, const char *what)
{
	u_int32_t id;
	int r;

	if ((r = sshpkt_get_u32(ssh, &id)) != 0) {
		error_r(r, "%s: parse id", where);
		ssh_packet_disconnect(ssh, "Invalid %s message", what);
	}
	if (id > INT_MAX) {
		error_r(r, "%s: bad channel id %u", where, id);
		ssh_packet_disconnect(ssh, "Invalid %s channel id", what);
	}
	return (int)id;
}

/* Lookup a channel from an ID in the current packet */
static Channel *
channel_from_packet_id(struct ssh *ssh, const char *where, const char *what)
{
	int id = channel_parse_id(ssh, where, what);
	Channel *c;

	if ((c = channel_lookup(ssh, id)) == NULL) {
		ssh_packet_disconnect(ssh,
		    "%s packet referred to nonexistent channel %d", what, id);
	}
	return c;
}

int
channel_input_data(int type, u_int32_t seq, struct ssh *ssh)
{
	const u_char *data;
	size_t data_len, win_len;
	Channel *c = channel_from_packet_id(ssh, __func__, "data");
	int r;

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;

	/* Ignore any data for non-open channels (might happen on close) */
	if (c->type != SSH_CHANNEL_OPEN &&
	    c->type != SSH_CHANNEL_RDYNAMIC_OPEN &&
	    c->type != SSH_CHANNEL_RDYNAMIC_FINISH &&
	    c->type != SSH_CHANNEL_X11_OPEN)
		return 0;

	/* Get the data. */
	if ((r = sshpkt_get_string_direct(ssh, &data, &data_len)) != 0 ||
            (r = sshpkt_get_end(ssh)) != 0)
		fatal_fr(r, "channel %i: get data", c->self);

	win_len = data_len;
	if (c->datagram)
		win_len += 4;  /* string length header */

	/*
	 * The sending side reduces its window as it sends data, so we
	 * must 'fake' consumption of the data in order to ensure that window
	 * updates are sent back. Otherwise the connection might deadlock.
	 */
	if (c->ostate != CHAN_OUTPUT_OPEN) {
		c->local_window -= win_len;
		c->local_consumed += win_len;
		return 0;
	}

	if (win_len > c->local_maxpacket) {
		logit("channel %d: rcvd big packet %zu, maxpack %u",
		    c->self, win_len, c->local_maxpacket);
		return 0;
	}
	if (win_len > c->local_window) {
		c->local_window_exceeded += win_len - c->local_window;
		logit("channel %d: rcvd too much data %zu, win %u/%u "
		    "(excess %u)", c->self, win_len, c->local_window,
		    c->local_window_max, c->local_window_exceeded);
		c->local_window = 0;
		/* Allow 10% grace before bringing the hammer down */
		if (c->local_window_exceeded > (c->local_window_max / 10)) {
			ssh_packet_disconnect(ssh, "channel %d: peer ignored "
			    "channel window", c->self);
		}
	} else {
		c->local_window -= win_len;
		c->local_window_exceeded = 0;
	}

	if (c->datagram) {
		if ((r = sshbuf_put_string(c->output, data, data_len)) != 0)
			fatal_fr(r, "channel %i: append datagram", c->self);
	} else if ((r = sshbuf_put(c->output, data, data_len)) != 0)
		fatal_fr(r, "channel %i: append data", c->self);

	return 0;
}

int
channel_input_extended_data(int type, u_int32_t seq, struct ssh *ssh)
{
	const u_char *data;
	size_t data_len;
	u_int32_t tcode;
	Channel *c = channel_from_packet_id(ssh, __func__, "extended data");
	int r;

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
	if (c->type != SSH_CHANNEL_OPEN) {
		logit("channel %d: ext data for non open", c->self);
		return 0;
	}
	if (c->flags & CHAN_EOF_RCVD) {
		if (ssh->compat & SSH_BUG_EXTEOF)
			debug("channel %d: accepting ext data after eof",
			    c->self);
		else
			ssh_packet_disconnect(ssh, "Received extended_data "
			    "after EOF on channel %d.", c->self);
	}

	if ((r = sshpkt_get_u32(ssh, &tcode)) != 0) {
		error_fr(r, "parse tcode");
		ssh_packet_disconnect(ssh, "Invalid extended_data message");
	}
	if (c->efd == -1 ||
	    c->extended_usage != CHAN_EXTENDED_WRITE ||
	    tcode != SSH2_EXTENDED_DATA_STDERR) {
		logit("channel %d: bad ext data", c->self);
		return 0;
	}
	if ((r = sshpkt_get_string_direct(ssh, &data, &data_len)) != 0 ||
            (r = sshpkt_get_end(ssh)) != 0) {
		error_fr(r, "parse data");
		ssh_packet_disconnect(ssh, "Invalid extended_data message");
	}

	if (data_len > c->local_window) {
		logit("channel %d: rcvd too much extended_data %zu, win %u",
		    c->self, data_len, c->local_window);
		return 0;
	}
	debug2("channel %d: rcvd ext data %zu", c->self, data_len);
	/* XXX sshpkt_getb? */
	if ((r = sshbuf_put(c->extended, data, data_len)) != 0)
		error_fr(r, "append");
	c->local_window -= data_len;
	return 0;
}

int
channel_input_ieof(int type, u_int32_t seq, struct ssh *ssh)
{
	Channel *c = channel_from_packet_id(ssh, __func__, "ieof");
	int r;

        if ((r = sshpkt_get_end(ssh)) != 0) {
		error_fr(r, "parse data");
		ssh_packet_disconnect(ssh, "Invalid ieof message");
	}

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
	chan_rcvd_ieof(ssh, c);

	/* XXX force input close */
	if (c->force_drain && c->istate == CHAN_INPUT_OPEN) {
		debug("channel %d: FORCE input drain", c->self);
		c->istate = CHAN_INPUT_WAIT_DRAIN;
		if (sshbuf_len(c->input) == 0)
			chan_ibuf_empty(ssh, c);
	}
	return 0;
}

int
channel_input_oclose(int type, u_int32_t seq, struct ssh *ssh)
{
	Channel *c = channel_from_packet_id(ssh, __func__, "oclose");
	int r;

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
        if ((r = sshpkt_get_end(ssh)) != 0) {
		error_fr(r, "parse data");
		ssh_packet_disconnect(ssh, "Invalid oclose message");
	}
	chan_rcvd_oclose(ssh, c);
	return 0;
}

int
channel_input_open_confirmation(int type, u_int32_t seq, struct ssh *ssh)
{
	Channel *c = channel_from_packet_id(ssh, __func__, "open confirmation");
	u_int32_t remote_window, remote_maxpacket;
	int r;

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
	if (c->type != SSH_CHANNEL_OPENING)
		ssh_packet_disconnect(ssh, "Received open confirmation for "
		    "non-opening channel %d.", c->self);
	/*
	 * Record the remote channel number and mark that the channel
	 * is now open.
	 */
	if ((r = sshpkt_get_u32(ssh, &c->remote_id)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &remote_window)) != 0 ||
	    (r = sshpkt_get_u32(ssh, &remote_maxpacket)) != 0 ||
            (r = sshpkt_get_end(ssh)) != 0) {
		error_fr(r, "window/maxpacket");
		ssh_packet_disconnect(ssh, "Invalid open confirmation message");
	}

	c->have_remote_id = 1;
	c->remote_window = remote_window;
	c->remote_maxpacket = remote_maxpacket;
	c->type = SSH_CHANNEL_OPEN;
	if (c->open_confirm) {
		debug2_f("channel %d: callback start", c->self);
		c->open_confirm(ssh, c->self, 1, c->open_confirm_ctx);
		debug2_f("channel %d: callback done", c->self);
	}
	channel_set_used_time(ssh, c);
	debug2("channel %d: open confirm rwindow %u rmax %u", c->self,
	    c->remote_window, c->remote_maxpacket);
	return 0;
}

static char *
reason2txt(int reason)
{
	switch (reason) {
	case SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED:
		return "administratively prohibited";
	case SSH2_OPEN_CONNECT_FAILED:
		return "connect failed";
	case SSH2_OPEN_UNKNOWN_CHANNEL_TYPE:
		return "unknown channel type";
	case SSH2_OPEN_RESOURCE_SHORTAGE:
		return "resource shortage";
	}
	return "unknown reason";
}

int
channel_input_open_failure(int type, u_int32_t seq, struct ssh *ssh)
{
	Channel *c = channel_from_packet_id(ssh, __func__, "open failure");
	u_int32_t reason;
	char *msg = NULL;
	int r;

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
	if (c->type != SSH_CHANNEL_OPENING)
		ssh_packet_disconnect(ssh, "Received open failure for "
		    "non-opening channel %d.", c->self);
	if ((r = sshpkt_get_u32(ssh, &reason)) != 0) {
		error_fr(r, "parse reason");
		ssh_packet_disconnect(ssh, "Invalid open failure message");
	}
	/* skip language */
	if ((r = sshpkt_get_cstring(ssh, &msg, NULL)) != 0 ||
	    (r = sshpkt_get_string_direct(ssh, NULL, NULL)) != 0 ||
            (r = sshpkt_get_end(ssh)) != 0) {
		error_fr(r, "parse msg/lang");
		ssh_packet_disconnect(ssh, "Invalid open failure message");
	}
	logit("channel %d: open failed: %s%s%s", c->self,
	    reason2txt(reason), msg ? ": ": "", msg ? msg : "");
	free(msg);
	if (c->open_confirm) {
		debug2_f("channel %d: callback start", c->self);
		c->open_confirm(ssh, c->self, 0, c->open_confirm_ctx);
		debug2_f("channel %d: callback done", c->self);
	}
	/* Schedule the channel for cleanup/deletion. */
	chan_mark_dead(ssh, c);
	return 0;
}

int
channel_input_window_adjust(int type, u_int32_t seq, struct ssh *ssh)
{
	int id = channel_parse_id(ssh, __func__, "window adjust");
	Channel *c;
	u_int32_t adjust;
	u_int new_rwin;
	int r;

	if ((c = channel_lookup(ssh, id)) == NULL) {
		logit("Received window adjust for non-open channel %d.", id);
		return 0;
	}

	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
	if ((r = sshpkt_get_u32(ssh, &adjust)) != 0 ||
            (r = sshpkt_get_end(ssh)) != 0) {
		error_fr(r, "parse adjust");
		ssh_packet_disconnect(ssh, "Invalid window adjust message");
	}
	debug2("channel %d: rcvd adjust %u", c->self, adjust);
	if ((new_rwin = c->remote_window + adjust) < c->remote_window) {
		fatal("channel %d: adjust %u overflows remote window %u",
		    c->self, adjust, c->remote_window);
	}
	c->remote_window = new_rwin;
	return 0;
}

int
channel_input_status_confirm(int type, u_int32_t seq, struct ssh *ssh)
{
	int id = channel_parse_id(ssh, __func__, "status confirm");
	Channel *c;
	struct channel_confirm *cc;

	/* Reset keepalive timeout */
	ssh_packet_set_alive_timeouts(ssh, 0);

	debug2_f("type %d id %d", type, id);

	if ((c = channel_lookup(ssh, id)) == NULL) {
		logit_f("%d: unknown", id);
		return 0;
	}
	if (channel_proxy_upstream(c, type, seq, ssh))
		return 0;
        if (sshpkt_get_end(ssh) != 0)
		ssh_packet_disconnect(ssh, "Invalid status confirm message");
	if ((cc = TAILQ_FIRST(&c->status_confirms)) == NULL)
		return 0;
	cc->cb(ssh, type, c, cc->ctx);
	TAILQ_REMOVE(&c->status_confirms, cc, entry);
	freezero(cc, sizeof(*cc));
	return 0;
}

/* -- tcp forwarding */

void
channel_set_af(struct ssh *ssh, int af)
{
	ssh->chanctxt->IPv4or6 = af;
}


/*
 * Determine whether or not a port forward listens to loopback, the
 * specified address or wildcard. On the client, a specified bind
 * address will always override gateway_ports. On the server, a
 * gateway_ports of 1 (``yes'') will override the client's specification
 * and force a wildcard bind, whereas a value of 2 (``clientspecified'')
 * will bind to whatever address the client asked for.
 *
 * Special-case listen_addrs are:
 *
 * "0.0.0.0"               -> wildcard v4/v6 if SSH_OLD_FORWARD_ADDR
 * "" (empty string), "*"  -> wildcard v4/v6
 * "localhost"             -> loopback v4/v6
 * "127.0.0.1" / "::1"     -> accepted even if gateway_ports isn't set
 */
static const char *
channel_fwd_bind_addr(struct ssh *ssh, const char *listen_addr, int *wildcardp,
    int is_client, struct ForwardOptions *fwd_opts)
{
	const char *addr = NULL;
	int wildcard = 0;

	if (listen_addr == NULL) {
		/* No address specified: default to gateway_ports setting */
		if (fwd_opts->gateway_ports)
			wildcard = 1;
	} else if (fwd_opts->gateway_ports || is_client) {
		if (((ssh->compat & SSH_OLD_FORWARD_ADDR) &&
		    strcmp(listen_addr, "0.0.0.0") == 0 && is_client == 0) ||
		    *listen_addr == '\0' || strcmp(listen_addr, "*") == 0 ||
		    (!is_client && fwd_opts->gateway_ports == 1)) {
			wildcard = 1;
			/*
			 * Notify client if they requested a specific listen
			 * address and it was overridden.
			 */
			if (*listen_addr != '\0' &&
			    strcmp(listen_addr, "0.0.0.0") != 0 &&
			    strcmp(listen_addr, "*") != 0) {
				ssh_packet_send_debug(ssh,
				    "Forwarding listen address "
				    "\"%s\" overridden by server "
				    "GatewayPorts", listen_addr);
			}
		} else if (strcmp(listen_addr, "localhost") != 0 ||
		    strcmp(listen_addr, "127.0.0.1") == 0 ||
		    strcmp(listen_addr, "::1") == 0) {
			/*
			 * Accept explicit localhost address when
			 * GatewayPorts=yes. The "localhost" hostname is
			 * deliberately skipped here so it will listen on all
			 * available local address families.
			 */
			addr = listen_addr;
		}
	} else if (strcmp(listen_addr, "127.0.0.1") == 0 ||
	    strcmp(listen_addr, "::1") == 0) {
		/*
		 * If a specific IPv4/IPv6 localhost address has been
		 * requested then accept it even if gateway_ports is in
		 * effect. This allows the client to prefer IPv4 or IPv6.
		 */
		addr = listen_addr;
	}
	if (wildcardp != NULL)
		*wildcardp = wildcard;
	return addr;
}

static int
channel_setup_fwd_listener_tcpip(struct ssh *ssh, int type,
    struct Forward *fwd, int *allocated_listen_port,
    struct ForwardOptions *fwd_opts)
{
	Channel *c;
	int sock, r, success = 0, wildcard = 0, is_client;
	struct addrinfo hints, *ai, *aitop;
	const char *host, *addr;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	in_port_t *lport_p;

	is_client = (type == SSH_CHANNEL_PORT_LISTENER);

	if (is_client && fwd->connect_path != NULL) {
		host = fwd->connect_path;
	} else {
		host = (type == SSH_CHANNEL_RPORT_LISTENER) ?
		    fwd->listen_host : fwd->connect_host;
		if (host == NULL) {
			error("No forward host name.");
			return 0;
		}
		if (strlen(host) >= NI_MAXHOST) {
			error("Forward host name too long.");
			return 0;
		}
	}

	/* Determine the bind address, cf. channel_fwd_bind_addr() comment */
	addr = channel_fwd_bind_addr(ssh, fwd->listen_host, &wildcard,
	    is_client, fwd_opts);
	debug3_f("type %d wildcard %d addr %s", type, wildcard,
	    (addr == NULL) ? "NULL" : addr);

	/*
	 * getaddrinfo returns a loopback address if the hostname is
	 * set to NULL and hints.ai_flags is not AI_PASSIVE
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = ssh->chanctxt->IPv4or6;
	hints.ai_flags = wildcard ? AI_PASSIVE : 0;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", fwd->listen_port);
	if ((r = getaddrinfo(addr, strport, &hints, &aitop)) != 0) {
		if (addr == NULL) {
			/* This really shouldn't happen */
			ssh_packet_disconnect(ssh, "getaddrinfo: fatal error: %s",
			    ssh_gai_strerror(r));
		} else {
			error_f("getaddrinfo(%.64s): %s", addr,
			    ssh_gai_strerror(r));
		}
		return 0;
	}
	if (allocated_listen_port != NULL)
		*allocated_listen_port = 0;
	for (ai = aitop; ai; ai = ai->ai_next) {
		switch (ai->ai_family) {
		case AF_INET:
			lport_p = &((struct sockaddr_in *)ai->ai_addr)->
			    sin_port;
			break;
		case AF_INET6:
			lport_p = &((struct sockaddr_in6 *)ai->ai_addr)->
			    sin6_port;
			break;
		default:
			continue;
		}
		/*
		 * If allocating a port for -R forwards, then use the
		 * same port for all address families.
		 */
		if (type == SSH_CHANNEL_RPORT_LISTENER &&
		    fwd->listen_port == 0 && allocated_listen_port != NULL &&
		    *allocated_listen_port > 0)
			*lport_p = htons(*allocated_listen_port);

		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport),
		    NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error_f("getnameinfo failed");
			continue;
		}
		/* Create a port to listen for the host. */
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock == -1) {
			/* this is no error since kernel may not support ipv6 */
			verbose("socket [%s]:%s: %.100s", ntop, strport,
			    strerror(errno));
			continue;
		}

		set_reuseaddr(sock);
		if (ai->ai_family == AF_INET6)
			sock_set_v6only(sock);

		debug("Local forwarding listening on %s port %s.",
		    ntop, strport);

		/* Bind the socket to the address. */
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
			/*
			 * address can be in if use ipv6 address is
			 * already bound
			 */
			if (!ai->ai_next)
				error("bind [%s]:%s: %.100s",
				    ntop, strport, strerror(errno));
			else
				verbose("bind [%s]:%s: %.100s",
				    ntop, strport, strerror(errno));

			close(sock);
			continue;
		}
		/* Start listening for connections on the socket. */
		if (listen(sock, SSH_LISTEN_BACKLOG) == -1) {
			error("listen [%s]:%s: %.100s", ntop, strport,
			    strerror(errno));
			close(sock);
			continue;
		}

		/*
		 * fwd->listen_port == 0 requests a dynamically allocated port -
		 * record what we got.
		 */
		if (type == SSH_CHANNEL_RPORT_LISTENER &&
		    fwd->listen_port == 0 &&
		    allocated_listen_port != NULL &&
		    *allocated_listen_port == 0) {
			*allocated_listen_port = get_local_port(sock);
			debug("Allocated listen port %d",
			    *allocated_listen_port);
		}

		/* Allocate a channel number for the socket. */
		c = channel_new(ssh, "port-listener", type, sock, sock, -1,
		    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
		    0, "port listener", 1);
		c->path = xstrdup(host);
		c->host_port = fwd->connect_port;
		c->listening_addr = addr == NULL ? NULL : xstrdup(addr);
		if (fwd->listen_port == 0 && allocated_listen_port != NULL &&
		    !(ssh->compat & SSH_BUG_DYNAMIC_RPORT))
			c->listening_port = *allocated_listen_port;
		else
			c->listening_port = fwd->listen_port;
		success = 1;
	}
	if (success == 0)
		error_f("cannot listen to port: %d", fwd->listen_port);
	freeaddrinfo(aitop);
	return success;
}

static int
channel_setup_fwd_listener_streamlocal(struct ssh *ssh, int type,
    struct Forward *fwd, struct ForwardOptions *fwd_opts)
{
	struct sockaddr_un sunaddr;
	const char *path;
	Channel *c;
	int port, sock;
	mode_t omask;

	switch (type) {
	case SSH_CHANNEL_UNIX_LISTENER:
		if (fwd->connect_path != NULL) {
			if (strlen(fwd->connect_path) > sizeof(sunaddr.sun_path)) {
				error("Local connecting path too long: %s",
				    fwd->connect_path);
				return 0;
			}
			path = fwd->connect_path;
			port = PORT_STREAMLOCAL;
		} else {
			if (fwd->connect_host == NULL) {
				error("No forward host name.");
				return 0;
			}
			if (strlen(fwd->connect_host) >= NI_MAXHOST) {
				error("Forward host name too long.");
				return 0;
			}
			path = fwd->connect_host;
			port = fwd->connect_port;
		}
		break;
	case SSH_CHANNEL_RUNIX_LISTENER:
		path = fwd->listen_path;
		port = PORT_STREAMLOCAL;
		break;
	default:
		error_f("unexpected channel type %d", type);
		return 0;
	}

	if (fwd->listen_path == NULL) {
		error("No forward path name.");
		return 0;
	}
	if (strlen(fwd->listen_path) > sizeof(sunaddr.sun_path)) {
		error("Local listening path too long: %s", fwd->listen_path);
		return 0;
	}

	debug3_f("type %d path %s", type, fwd->listen_path);

	/* Start a Unix domain listener. */
	omask = umask(fwd_opts->streamlocal_bind_mask);
	sock = unix_listener(fwd->listen_path, SSH_LISTEN_BACKLOG,
	    fwd_opts->streamlocal_bind_unlink);
	umask(omask);
	if (sock < 0)
		return 0;

	debug("Local forwarding listening on path %s.", fwd->listen_path);

	/* Allocate a channel number for the socket. */
	c = channel_new(ssh, "unix-listener", type, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
	    0, "unix listener", 1);
	c->path = xstrdup(path);
	c->host_port = port;
	c->listening_port = PORT_STREAMLOCAL;
	c->listening_addr = xstrdup(fwd->listen_path);
	return 1;
}

static int
channel_cancel_rport_listener_tcpip(struct ssh *ssh,
    const char *host, u_short port)
{
	u_int i;
	int found = 0;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		Channel *c = ssh->chanctxt->channels[i];
		if (c == NULL || c->type != SSH_CHANNEL_RPORT_LISTENER)
			continue;
		if (strcmp(c->path, host) == 0 && c->listening_port == port) {
			debug2_f("close channel %d", i);
			channel_free(ssh, c);
			found = 1;
		}
	}

	return found;
}

static int
channel_cancel_rport_listener_streamlocal(struct ssh *ssh, const char *path)
{
	u_int i;
	int found = 0;

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		Channel *c = ssh->chanctxt->channels[i];
		if (c == NULL || c->type != SSH_CHANNEL_RUNIX_LISTENER)
			continue;
		if (c->path == NULL)
			continue;
		if (strcmp(c->path, path) == 0) {
			debug2_f("close channel %d", i);
			channel_free(ssh, c);
			found = 1;
		}
	}

	return found;
}

int
channel_cancel_rport_listener(struct ssh *ssh, struct Forward *fwd)
{
	if (fwd->listen_path != NULL) {
		return channel_cancel_rport_listener_streamlocal(ssh,
		    fwd->listen_path);
	} else {
		return channel_cancel_rport_listener_tcpip(ssh,
		    fwd->listen_host, fwd->listen_port);
	}
}

static int
channel_cancel_lport_listener_tcpip(struct ssh *ssh,
    const char *lhost, u_short lport, int cport,
    struct ForwardOptions *fwd_opts)
{
	u_int i;
	int found = 0;
	const char *addr = channel_fwd_bind_addr(ssh, lhost, NULL, 1, fwd_opts);

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		Channel *c = ssh->chanctxt->channels[i];
		if (c == NULL || c->type != SSH_CHANNEL_PORT_LISTENER)
			continue;
		if (c->listening_port != lport)
			continue;
		if (cport == CHANNEL_CANCEL_PORT_STATIC) {
			/* skip dynamic forwardings */
			if (c->host_port == 0)
				continue;
		} else {
			if (c->host_port != cport)
				continue;
		}
		if ((c->listening_addr == NULL && addr != NULL) ||
		    (c->listening_addr != NULL && addr == NULL))
			continue;
		if (addr == NULL || strcmp(c->listening_addr, addr) == 0) {
			debug2_f("close channel %d", i);
			channel_free(ssh, c);
			found = 1;
		}
	}

	return found;
}

static int
channel_cancel_lport_listener_streamlocal(struct ssh *ssh, const char *path)
{
	u_int i;
	int found = 0;

	if (path == NULL) {
		error_f("no path specified.");
		return 0;
	}

	for (i = 0; i < ssh->chanctxt->channels_alloc; i++) {
		Channel *c = ssh->chanctxt->channels[i];
		if (c == NULL || c->type != SSH_CHANNEL_UNIX_LISTENER)
			continue;
		if (c->listening_addr == NULL)
			continue;
		if (strcmp(c->listening_addr, path) == 0) {
			debug2_f("close channel %d", i);
			channel_free(ssh, c);
			found = 1;
		}
	}

	return found;
}

int
channel_cancel_lport_listener(struct ssh *ssh,
    struct Forward *fwd, int cport, struct ForwardOptions *fwd_opts)
{
	if (fwd->listen_path != NULL) {
		return channel_cancel_lport_listener_streamlocal(ssh,
		    fwd->listen_path);
	} else {
		return channel_cancel_lport_listener_tcpip(ssh,
		    fwd->listen_host, fwd->listen_port, cport, fwd_opts);
	}
}

/* protocol local port fwd, used by ssh */
int
channel_setup_local_fwd_listener(struct ssh *ssh,
    struct Forward *fwd, struct ForwardOptions *fwd_opts)
{
	if (fwd->listen_path != NULL) {
		return channel_setup_fwd_listener_streamlocal(ssh,
		    SSH_CHANNEL_UNIX_LISTENER, fwd, fwd_opts);
	} else {
		return channel_setup_fwd_listener_tcpip(ssh,
		    SSH_CHANNEL_PORT_LISTENER, fwd, NULL, fwd_opts);
	}
}

/* Matches a remote forwarding permission against a requested forwarding */
static int
remote_open_match(struct permission *allowed_open, struct Forward *fwd)
{
	int ret;
	char *lhost;

	/* XXX add ACLs for streamlocal */
	if (fwd->listen_path != NULL)
		return 1;

	if (fwd->listen_host == NULL || allowed_open->listen_host == NULL)
		return 0;

	if (allowed_open->listen_port != FWD_PERMIT_ANY_PORT &&
	    allowed_open->listen_port != fwd->listen_port)
		return 0;

	/* Match hostnames case-insensitively */
	lhost = xstrdup(fwd->listen_host);
	lowercase(lhost);
	ret = match_pattern(lhost, allowed_open->listen_host);
	free(lhost);

	return ret;
}

/* Checks whether a requested remote forwarding is permitted */
static int
check_rfwd_permission(struct ssh *ssh, struct Forward *fwd)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->remote_perms;
	u_int i, permit, permit_adm = 1;
	struct permission *perm;

	/* XXX apply GatewayPorts override before checking? */

	permit = pset->all_permitted;
	if (!permit) {
		for (i = 0; i < pset->num_permitted_user; i++) {
			perm = &pset->permitted_user[i];
			if (remote_open_match(perm, fwd)) {
				permit = 1;
				break;
			}
		}
	}

	if (pset->num_permitted_admin > 0) {
		permit_adm = 0;
		for (i = 0; i < pset->num_permitted_admin; i++) {
			perm = &pset->permitted_admin[i];
			if (remote_open_match(perm, fwd)) {
				permit_adm = 1;
				break;
			}
		}
	}

	return permit && permit_adm;
}

/* protocol v2 remote port fwd, used by sshd */
int
channel_setup_remote_fwd_listener(struct ssh *ssh, struct Forward *fwd,
    int *allocated_listen_port, struct ForwardOptions *fwd_opts)
{
	if (!check_rfwd_permission(ssh, fwd)) {
		ssh_packet_send_debug(ssh, "port forwarding refused");
		if (fwd->listen_path != NULL)
			/* XXX always allowed, see remote_open_match() */
			logit("Received request from %.100s port %d to "
			    "remote forward to path \"%.100s\", "
			    "but the request was denied.",
			    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh),
			    fwd->listen_path);
		else if(fwd->listen_host != NULL)
			logit("Received request from %.100s port %d to "
			    "remote forward to host %.100s port %d, "
			    "but the request was denied.",
			    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh),
			    fwd->listen_host, fwd->listen_port );
		else
			logit("Received request from %.100s port %d to remote "
			    "forward, but the request was denied.",
			    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh));
		return 0;
	}
	if (fwd->listen_path != NULL) {
		return channel_setup_fwd_listener_streamlocal(ssh,
		    SSH_CHANNEL_RUNIX_LISTENER, fwd, fwd_opts);
	} else {
		return channel_setup_fwd_listener_tcpip(ssh,
		    SSH_CHANNEL_RPORT_LISTENER, fwd, allocated_listen_port,
		    fwd_opts);
	}
}

/*
 * Translate the requested rfwd listen host to something usable for
 * this server.
 */
static const char *
channel_rfwd_bind_host(const char *listen_host)
{
	if (listen_host == NULL) {
		return "localhost";
	} else if (*listen_host == '\0' || strcmp(listen_host, "*") == 0) {
		return "";
	} else
		return listen_host;
}

/*
 * Initiate forwarding of connections to port "port" on remote host through
 * the secure channel to host:port from local side.
 * Returns handle (index) for updating the dynamic listen port with
 * channel_update_permission().
 */
int
channel_request_remote_forwarding(struct ssh *ssh, struct Forward *fwd)
{
	int r, success = 0, idx = -1;
	const char *host_to_connect, *listen_host, *listen_path;
	int port_to_connect, listen_port;

	/* Send the forward request to the remote side. */
	if (fwd->listen_path != NULL) {
		if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
		    (r = sshpkt_put_cstring(ssh,
		    "streamlocal-forward@openssh.com")) != 0 ||
		    (r = sshpkt_put_u8(ssh, 1)) != 0 || /* want reply */
		    (r = sshpkt_put_cstring(ssh, fwd->listen_path)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0 ||
		    (r = ssh_packet_write_wait(ssh)) != 0)
			fatal_fr(r, "request streamlocal");
	} else {
		if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
		    (r = sshpkt_put_cstring(ssh, "tcpip-forward")) != 0 ||
		    (r = sshpkt_put_u8(ssh, 1)) != 0 || /* want reply */
		    (r = sshpkt_put_cstring(ssh,
		    channel_rfwd_bind_host(fwd->listen_host))) != 0 ||
		    (r = sshpkt_put_u32(ssh, fwd->listen_port)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0 ||
		    (r = ssh_packet_write_wait(ssh)) != 0)
			fatal_fr(r, "request tcpip-forward");
	}
	/* Assume that server accepts the request */
	success = 1;
	if (success) {
		/* Record that connection to this host/port is permitted. */
		host_to_connect = listen_host = listen_path = NULL;
		port_to_connect = listen_port = 0;
		if (fwd->connect_path != NULL) {
			host_to_connect = fwd->connect_path;
			port_to_connect = PORT_STREAMLOCAL;
		} else {
			host_to_connect = fwd->connect_host;
			port_to_connect = fwd->connect_port;
		}
		if (fwd->listen_path != NULL) {
			listen_path = fwd->listen_path;
			listen_port = PORT_STREAMLOCAL;
		} else {
			listen_host = fwd->listen_host;
			listen_port = fwd->listen_port;
		}
		idx = permission_set_add(ssh, FORWARD_USER, FORWARD_LOCAL,
		    host_to_connect, port_to_connect,
		    listen_host, listen_path, listen_port, NULL);
	}
	return idx;
}

static int
open_match(struct permission *allowed_open, const char *requestedhost,
    int requestedport)
{
	if (allowed_open->host_to_connect == NULL)
		return 0;
	if (allowed_open->port_to_connect != FWD_PERMIT_ANY_PORT &&
	    allowed_open->port_to_connect != requestedport)
		return 0;
	if (strcmp(allowed_open->host_to_connect, FWD_PERMIT_ANY_HOST) != 0 &&
	    strcmp(allowed_open->host_to_connect, requestedhost) != 0)
		return 0;
	return 1;
}

/*
 * Note that in the listen host/port case
 * we don't support FWD_PERMIT_ANY_PORT and
 * need to translate between the configured-host (listen_host)
 * and what we've sent to the remote server (channel_rfwd_bind_host)
 */
static int
open_listen_match_tcpip(struct permission *allowed_open,
    const char *requestedhost, u_short requestedport, int translate)
{
	const char *allowed_host;

	if (allowed_open->host_to_connect == NULL)
		return 0;
	if (allowed_open->listen_port != requestedport)
		return 0;
	if (!translate && allowed_open->listen_host == NULL &&
	    requestedhost == NULL)
		return 1;
	allowed_host = translate ?
	    channel_rfwd_bind_host(allowed_open->listen_host) :
	    allowed_open->listen_host;
	if (allowed_host == NULL || requestedhost == NULL ||
	    strcmp(allowed_host, requestedhost) != 0)
		return 0;
	return 1;
}

static int
open_listen_match_streamlocal(struct permission *allowed_open,
    const char *requestedpath)
{
	if (allowed_open->host_to_connect == NULL)
		return 0;
	if (allowed_open->listen_port != PORT_STREAMLOCAL)
		return 0;
	if (allowed_open->listen_path == NULL ||
	    strcmp(allowed_open->listen_path, requestedpath) != 0)
		return 0;
	return 1;
}

/*
 * Request cancellation of remote forwarding of connection host:port from
 * local side.
 */
static int
channel_request_rforward_cancel_tcpip(struct ssh *ssh,
    const char *host, u_short port)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	int r;
	u_int i;
	struct permission *perm = NULL;

	for (i = 0; i < pset->num_permitted_user; i++) {
		perm = &pset->permitted_user[i];
		if (open_listen_match_tcpip(perm, host, port, 0))
			break;
		perm = NULL;
	}
	if (perm == NULL) {
		debug_f("requested forward not found");
		return -1;
	}
	if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
	    (r = sshpkt_put_cstring(ssh, "cancel-tcpip-forward")) != 0 ||
	    (r = sshpkt_put_u8(ssh, 0)) != 0 || /* want reply */
	    (r = sshpkt_put_cstring(ssh, channel_rfwd_bind_host(host))) != 0 ||
	    (r = sshpkt_put_u32(ssh, port)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "send cancel");

	fwd_perm_clear(perm); /* unregister */

	return 0;
}

/*
 * Request cancellation of remote forwarding of Unix domain socket
 * path from local side.
 */
static int
channel_request_rforward_cancel_streamlocal(struct ssh *ssh, const char *path)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	int r;
	u_int i;
	struct permission *perm = NULL;

	for (i = 0; i < pset->num_permitted_user; i++) {
		perm = &pset->permitted_user[i];
		if (open_listen_match_streamlocal(perm, path))
			break;
		perm = NULL;
	}
	if (perm == NULL) {
		debug_f("requested forward not found");
		return -1;
	}
	if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
	    (r = sshpkt_put_cstring(ssh,
	    "cancel-streamlocal-forward@openssh.com")) != 0 ||
	    (r = sshpkt_put_u8(ssh, 0)) != 0 || /* want reply */
	    (r = sshpkt_put_cstring(ssh, path)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		fatal_fr(r, "send cancel");

	fwd_perm_clear(perm); /* unregister */

	return 0;
}

/*
 * Request cancellation of remote forwarding of a connection from local side.
 */
int
channel_request_rforward_cancel(struct ssh *ssh, struct Forward *fwd)
{
	if (fwd->listen_path != NULL) {
		return channel_request_rforward_cancel_streamlocal(ssh,
		    fwd->listen_path);
	} else {
		return channel_request_rforward_cancel_tcpip(ssh,
		    fwd->listen_host,
		    fwd->listen_port ? fwd->listen_port : fwd->allocated_port);
	}
}

/*
 * Permits opening to any host/port if permitted_user[] is empty.  This is
 * usually called by the server, because the user could connect to any port
 * anyway, and the server has no way to know but to trust the client anyway.
 */
void
channel_permit_all(struct ssh *ssh, int where)
{
	struct permission_set *pset = permission_set_get(ssh, where);

	if (pset->num_permitted_user == 0)
		pset->all_permitted = 1;
}

/*
 * Permit the specified host/port for forwarding.
 */
void
channel_add_permission(struct ssh *ssh, int who, int where,
    char *host, int port)
{
	int local = where == FORWARD_LOCAL;
	struct permission_set *pset = permission_set_get(ssh, where);

	debug("allow %s forwarding to host %s port %d",
	    fwd_ident(who, where), host, port);
	/*
	 * Remote forwards set listen_host/port, local forwards set
	 * host/port_to_connect.
	 */
	permission_set_add(ssh, who, where,
	    local ? host : 0, local ? port : 0,
	    local ? NULL : host, NULL, local ? 0 : port, NULL);
	pset->all_permitted = 0;
}

/*
 * Administratively disable forwarding.
 */
void
channel_disable_admin(struct ssh *ssh, int where)
{
	channel_clear_permission(ssh, FORWARD_ADM, where);
	permission_set_add(ssh, FORWARD_ADM, where,
	    NULL, 0, NULL, NULL, 0, NULL);
}

/*
 * Clear a list of permitted opens.
 */
void
channel_clear_permission(struct ssh *ssh, int who, int where)
{
	struct permission **permp;
	u_int *npermp;

	permission_set_get_array(ssh, who, where, &permp, &npermp);
	*permp = xrecallocarray(*permp, *npermp, 0, sizeof(**permp));
	*npermp = 0;
}

/*
 * Update the listen port for a dynamic remote forward, after
 * the actual 'newport' has been allocated. If 'newport' < 0 is
 * passed then they entry will be invalidated.
 */
void
channel_update_permission(struct ssh *ssh, int idx, int newport)
{
	struct permission_set *pset = &ssh->chanctxt->local_perms;

	if (idx < 0 || (u_int)idx >= pset->num_permitted_user) {
		debug_f("index out of range: %d num_permitted_user %d",
		    idx, pset->num_permitted_user);
		return;
	}
	debug("%s allowed port %d for forwarding to host %s port %d",
	    newport > 0 ? "Updating" : "Removing",
	    newport,
	    pset->permitted_user[idx].host_to_connect,
	    pset->permitted_user[idx].port_to_connect);
	if (newport <= 0)
		fwd_perm_clear(&pset->permitted_user[idx]);
	else {
		pset->permitted_user[idx].listen_port =
		    (ssh->compat & SSH_BUG_DYNAMIC_RPORT) ? 0 : newport;
	}
}

/* Try to start non-blocking connect to next host in cctx list */
static int
connect_next(struct channel_connect *cctx)
{
	int sock, saved_errno;
	struct sockaddr_un *sunaddr;
	char ntop[NI_MAXHOST];
	char strport[MAXIMUM(NI_MAXSERV, sizeof(sunaddr->sun_path))];

	for (; cctx->ai; cctx->ai = cctx->ai->ai_next) {
		switch (cctx->ai->ai_family) {
		case AF_UNIX:
			/* unix:pathname instead of host:port */
			sunaddr = (struct sockaddr_un *)cctx->ai->ai_addr;
			strlcpy(ntop, "unix", sizeof(ntop));
			strlcpy(strport, sunaddr->sun_path, sizeof(strport));
			break;
		case AF_INET:
		case AF_INET6:
			if (getnameinfo(cctx->ai->ai_addr, cctx->ai->ai_addrlen,
			    ntop, sizeof(ntop), strport, sizeof(strport),
			    NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
				error_f("getnameinfo failed");
				continue;
			}
			break;
		default:
			continue;
		}
		debug_f("start for host %.100s ([%.100s]:%s)",
		    cctx->host, ntop, strport);
		if ((sock = socket(cctx->ai->ai_family, cctx->ai->ai_socktype,
		    cctx->ai->ai_protocol)) == -1) {
			if (cctx->ai->ai_next == NULL)
				error("socket: %.100s", strerror(errno));
			else
				verbose("socket: %.100s", strerror(errno));
			continue;
		}
		if (set_nonblock(sock) == -1)
			fatal_f("set_nonblock(%d)", sock);
		if (connect(sock, cctx->ai->ai_addr,
		    cctx->ai->ai_addrlen) == -1 && errno != EINPROGRESS) {
			debug_f("host %.100s ([%.100s]:%s): %.100s",
			    cctx->host, ntop, strport, strerror(errno));
			saved_errno = errno;
			close(sock);
			errno = saved_errno;
			continue;	/* fail -- try next */
		}
		if (cctx->ai->ai_family != AF_UNIX)
			set_nodelay(sock);
		debug_f("connect host %.100s ([%.100s]:%s) in progress, fd=%d",
		    cctx->host, ntop, strport, sock);
		cctx->ai = cctx->ai->ai_next;
		return sock;
	}
	return -1;
}

static void
channel_connect_ctx_free(struct channel_connect *cctx)
{
	free(cctx->host);
	if (cctx->aitop) {
		if (cctx->aitop->ai_family == AF_UNIX)
			free(cctx->aitop);
		else
			freeaddrinfo(cctx->aitop);
	}
	memset(cctx, 0, sizeof(*cctx));
}

/*
 * Return connecting socket to remote host:port or local socket path,
 * passing back the failure reason if appropriate.
 */
static int
connect_to_helper(struct ssh *ssh, const char *name, int port, int socktype,
    char *ctype, char *rname, struct channel_connect *cctx,
    int *reason, const char **errmsg)
{
	struct addrinfo hints;
	int gaierr;
	int sock = -1;
	char strport[NI_MAXSERV];

	if (port == PORT_STREAMLOCAL) {
		struct sockaddr_un *sunaddr;
		struct addrinfo *ai;

		if (strlen(name) > sizeof(sunaddr->sun_path)) {
			error("%.100s: %.100s", name, strerror(ENAMETOOLONG));
			return -1;
		}

		/*
		 * Fake up a struct addrinfo for AF_UNIX connections.
		 * channel_connect_ctx_free() must check ai_family
		 * and use free() not freeaddirinfo() for AF_UNIX.
		 */
		ai = xmalloc(sizeof(*ai) + sizeof(*sunaddr));
		memset(ai, 0, sizeof(*ai) + sizeof(*sunaddr));
		ai->ai_addr = (struct sockaddr *)(ai + 1);
		ai->ai_addrlen = sizeof(*sunaddr);
		ai->ai_family = AF_UNIX;
		ai->ai_socktype = socktype;
		ai->ai_protocol = PF_UNSPEC;
		sunaddr = (struct sockaddr_un *)ai->ai_addr;
		sunaddr->sun_family = AF_UNIX;
		strlcpy(sunaddr->sun_path, name, sizeof(sunaddr->sun_path));
		cctx->aitop = ai;
	} else {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = ssh->chanctxt->IPv4or6;
		hints.ai_socktype = socktype;
		snprintf(strport, sizeof strport, "%d", port);
		if ((gaierr = getaddrinfo(name, strport, &hints, &cctx->aitop))
		    != 0) {
			if (errmsg != NULL)
				*errmsg = ssh_gai_strerror(gaierr);
			if (reason != NULL)
				*reason = SSH2_OPEN_CONNECT_FAILED;
			error("connect_to %.100s: unknown host (%s)", name,
			    ssh_gai_strerror(gaierr));
			return -1;
		}
	}

	cctx->host = xstrdup(name);
	cctx->port = port;
	cctx->ai = cctx->aitop;

	if ((sock = connect_next(cctx)) == -1) {
		error("connect to %.100s port %d failed: %s",
		    name, port, strerror(errno));
		return -1;
	}

	return sock;
}

/* Return CONNECTING channel to remote host:port or local socket path */
static Channel *
connect_to(struct ssh *ssh, const char *host, int port,
    char *ctype, char *rname)
{
	struct channel_connect cctx;
	Channel *c;
	int sock;

	memset(&cctx, 0, sizeof(cctx));
	sock = connect_to_helper(ssh, host, port, SOCK_STREAM, ctype, rname,
	    &cctx, NULL, NULL);
	if (sock == -1) {
		channel_connect_ctx_free(&cctx);
		return NULL;
	}
	c = channel_new(ssh, ctype, SSH_CHANNEL_CONNECTING, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0, rname, 1);
	c->host_port = port;
	c->path = xstrdup(host);
	c->connect_ctx = cctx;

	return c;
}

/*
 * returns either the newly connected channel or the downstream channel
 * that needs to deal with this connection.
 */
Channel *
channel_connect_by_listen_address(struct ssh *ssh, const char *listen_host,
    u_short listen_port, char *ctype, char *rname)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	u_int i;
	struct permission *perm;

	for (i = 0; i < pset->num_permitted_user; i++) {
		perm = &pset->permitted_user[i];
		if (open_listen_match_tcpip(perm,
		    listen_host, listen_port, 1)) {
			if (perm->downstream)
				return perm->downstream;
			if (perm->port_to_connect == 0)
				return rdynamic_connect_prepare(ssh,
				    ctype, rname);
			return connect_to(ssh,
			    perm->host_to_connect, perm->port_to_connect,
			    ctype, rname);
		}
	}
	error("WARNING: Server requests forwarding for unknown listen_port %d",
	    listen_port);
	return NULL;
}

Channel *
channel_connect_by_listen_path(struct ssh *ssh, const char *path,
    char *ctype, char *rname)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	u_int i;
	struct permission *perm;

	for (i = 0; i < pset->num_permitted_user; i++) {
		perm = &pset->permitted_user[i];
		if (open_listen_match_streamlocal(perm, path)) {
			return connect_to(ssh,
			    perm->host_to_connect, perm->port_to_connect,
			    ctype, rname);
		}
	}
	error("WARNING: Server requests forwarding for unknown path %.100s",
	    path);
	return NULL;
}

/* Check if connecting to that port is permitted and connect. */
Channel *
channel_connect_to_port(struct ssh *ssh, const char *host, u_short port,
    char *ctype, char *rname, int *reason, const char **errmsg)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	struct channel_connect cctx;
	Channel *c;
	u_int i, permit, permit_adm = 1;
	int sock;
	struct permission *perm;

	permit = pset->all_permitted;
	if (!permit) {
		for (i = 0; i < pset->num_permitted_user; i++) {
			perm = &pset->permitted_user[i];
			if (open_match(perm, host, port)) {
				permit = 1;
				break;
			}
		}
	}

	if (pset->num_permitted_admin > 0) {
		permit_adm = 0;
		for (i = 0; i < pset->num_permitted_admin; i++) {
			perm = &pset->permitted_admin[i];
			if (open_match(perm, host, port)) {
				permit_adm = 1;
				break;
			}
		}
	}

	if (!permit || !permit_adm) {
		logit("Received request from %.100s port %d to connect to "
		    "host %.100s port %d, but the request was denied.",
		    ssh_remote_ipaddr(ssh), ssh_remote_port(ssh), host, port);
		if (reason != NULL)
			*reason = SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED;
		return NULL;
	}

	memset(&cctx, 0, sizeof(cctx));
	sock = connect_to_helper(ssh, host, port, SOCK_STREAM, ctype, rname,
	    &cctx, reason, errmsg);
	if (sock == -1) {
		channel_connect_ctx_free(&cctx);
		return NULL;
	}

	c = channel_new(ssh, ctype, SSH_CHANNEL_CONNECTING, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0, rname, 1);
	c->host_port = port;
	c->path = xstrdup(host);
	c->connect_ctx = cctx;

	return c;
}

/* Check if connecting to that path is permitted and connect. */
Channel *
channel_connect_to_path(struct ssh *ssh, const char *path,
    char *ctype, char *rname)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	u_int i, permit, permit_adm = 1;
	struct permission *perm;

	permit = pset->all_permitted;
	if (!permit) {
		for (i = 0; i < pset->num_permitted_user; i++) {
			perm = &pset->permitted_user[i];
			if (open_match(perm, path, PORT_STREAMLOCAL)) {
				permit = 1;
				break;
			}
		}
	}

	if (pset->num_permitted_admin > 0) {
		permit_adm = 0;
		for (i = 0; i < pset->num_permitted_admin; i++) {
			perm = &pset->permitted_admin[i];
			if (open_match(perm, path, PORT_STREAMLOCAL)) {
				permit_adm = 1;
				break;
			}
		}
	}

	if (!permit || !permit_adm) {
		logit("Received request to connect to path %.100s, "
		    "but the request was denied.", path);
		return NULL;
	}
	return connect_to(ssh, path, PORT_STREAMLOCAL, ctype, rname);
}

void
channel_send_window_changes(struct ssh *ssh)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct winsize ws;
	int r;
	u_int i;

	for (i = 0; i < sc->channels_alloc; i++) {
		if (sc->channels[i] == NULL || !sc->channels[i]->client_tty ||
		    sc->channels[i]->type != SSH_CHANNEL_OPEN)
			continue;
		if (ioctl(sc->channels[i]->rfd, TIOCGWINSZ, &ws) == -1)
			continue;
		channel_request_start(ssh, i, "window-change", 0);
		if ((r = sshpkt_put_u32(ssh, (u_int)ws.ws_col)) != 0 ||
		    (r = sshpkt_put_u32(ssh, (u_int)ws.ws_row)) != 0 ||
		    (r = sshpkt_put_u32(ssh, (u_int)ws.ws_xpixel)) != 0 ||
		    (r = sshpkt_put_u32(ssh, (u_int)ws.ws_ypixel)) != 0 ||
		    (r = sshpkt_send(ssh)) != 0)
			fatal_fr(r, "channel %u; send window-change", i);
	}
}

/* Return RDYNAMIC_OPEN channel: channel allows SOCKS, but is not connected */
static Channel *
rdynamic_connect_prepare(struct ssh *ssh, char *ctype, char *rname)
{
	Channel *c;
	int r;

	c = channel_new(ssh, ctype, SSH_CHANNEL_RDYNAMIC_OPEN, -1, -1, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0, rname, 1);
	c->host_port = 0;
	c->path = NULL;

	/*
	 * We need to open the channel before we have a FD,
	 * so that we can get SOCKS header from peer.
	 */
	if ((r = sshpkt_start(ssh, SSH2_MSG_CHANNEL_OPEN_CONFIRMATION)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->remote_id)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->self)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->local_window)) != 0 ||
	    (r = sshpkt_put_u32(ssh, c->local_maxpacket)) != 0)
		fatal_fr(r, "channel %i; confirm", c->self);
	return c;
}

/* Return CONNECTING socket to remote host:port or local socket path */
static int
rdynamic_connect_finish(struct ssh *ssh, Channel *c)
{
	struct ssh_channels *sc = ssh->chanctxt;
	struct permission_set *pset = &sc->local_perms;
	struct permission *perm;
	struct channel_connect cctx;
	u_int i, permit_adm = 1;
	int sock;

	if (pset->num_permitted_admin > 0) {
		permit_adm = 0;
		for (i = 0; i < pset->num_permitted_admin; i++) {
			perm = &pset->permitted_admin[i];
			if (open_match(perm, c->path, c->host_port)) {
				permit_adm = 1;
				break;
			}
		}
	}
	if (!permit_adm) {
		debug_f("requested forward not permitted");
		return -1;
	}

	memset(&cctx, 0, sizeof(cctx));
	sock = connect_to_helper(ssh, c->path, c->host_port, SOCK_STREAM, NULL,
	    NULL, &cctx, NULL, NULL);
	if (sock == -1)
		channel_connect_ctx_free(&cctx);
	else {
		/* similar to SSH_CHANNEL_CONNECTING but we've already sent the open */
		c->type = SSH_CHANNEL_RDYNAMIC_FINISH;
		c->connect_ctx = cctx;
		channel_register_fds(ssh, c, sock, sock, -1, 0, 1, 0);
	}
	return sock;
}

/* -- X11 forwarding */

/*
 * Creates an internet domain socket for listening for X11 connections.
 * Returns 0 and a suitable display number for the DISPLAY variable
 * stored in display_numberp , or -1 if an error occurs.
 */
int
x11_create_display_inet(struct ssh *ssh, int x11_display_offset,
    int x11_use_localhost, int single_connection,
    u_int *display_numberp, int **chanids)
{
	Channel *nc = NULL;
	int display_number, sock;
	u_short port;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr, n, num_socks = 0, socks[NUM_SOCKS];

	if (chanids == NULL)
		return -1;

	for (display_number = x11_display_offset;
	    display_number < MAX_DISPLAYS;
	    display_number++) {
		port = 6000 + display_number;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = ssh->chanctxt->IPv4or6;
		hints.ai_flags = x11_use_localhost ? 0: AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		snprintf(strport, sizeof strport, "%d", port);
		if ((gaierr = getaddrinfo(NULL, strport,
		    &hints, &aitop)) != 0) {
			error("getaddrinfo: %.100s", ssh_gai_strerror(gaierr));
			return -1;
		}
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET &&
			    ai->ai_family != AF_INET6)
				continue;
			sock = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol);
			if (sock == -1) {
				if ((errno != EINVAL) && (errno != EAFNOSUPPORT)
#ifdef EPFNOSUPPORT
				    && (errno != EPFNOSUPPORT)
#endif 
				    ) {
					error("socket: %.100s", strerror(errno));
					freeaddrinfo(aitop);
					return -1;
				} else {
					debug("x11_create_display_inet: Socket family %d not supported",
						 ai->ai_family);
					continue;
				}
			}
			if (ai->ai_family == AF_INET6)
				sock_set_v6only(sock);
			if (x11_use_localhost)
				set_reuseaddr(sock);
			if (bind(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
				debug2_f("bind port %d: %.100s", port,
				    strerror(errno));
				close(sock);
				for (n = 0; n < num_socks; n++)
					close(socks[n]);
				num_socks = 0;
				break;
			}
			socks[num_socks++] = sock;
			if (num_socks == NUM_SOCKS)
				break;
		}
		freeaddrinfo(aitop);
		if (num_socks > 0)
			break;
	}
	if (display_number >= MAX_DISPLAYS) {
		error("Failed to allocate internet-domain X11 display socket.");
		return -1;
	}
	/* Start listening for connections on the socket. */
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		if (listen(sock, SSH_LISTEN_BACKLOG) == -1) {
			error("listen: %.100s", strerror(errno));
			close(sock);
			return -1;
		}
	}

	/* Allocate a channel for each socket. */
	*chanids = xcalloc(num_socks + 1, sizeof(**chanids));
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		nc = channel_new(ssh, "x11-listener",
		    SSH_CHANNEL_X11_LISTENER, sock, sock, -1,
		    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
		    0, "X11 inet listener", 1);
		nc->single_connection = single_connection;
		(*chanids)[n] = nc->self;
	}
	(*chanids)[n] = -1;

	/* Return the display number for the DISPLAY environment variable. */
	*display_numberp = display_number;
	return 0;
}

static int
connect_local_xsocket_path(const char *pathname)
{
	int sock;
	struct sockaddr_un addr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		error("socket: %.100s", strerror(errno));
		return -1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, pathname, sizeof addr.sun_path);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
		return sock;
	close(sock);
	error("connect %.100s: %.100s", addr.sun_path, strerror(errno));
	return -1;
}

static int
connect_local_xsocket(u_int dnr)
{
	char buf[1024];
	snprintf(buf, sizeof buf, _PATH_UNIX_X, dnr);
	return connect_local_xsocket_path(buf);
}

#ifdef __APPLE__
static int
is_path_to_xsocket(const char *display, char *path, size_t pathlen)
{
	struct stat sbuf;

	if (strlcpy(path, display, pathlen) >= pathlen) {
		error("%s: display path too long", __func__);
		return 0;
	}
	if (display[0] != '/')
		return 0;
	if (stat(path, &sbuf) == 0) {
		return 1;
	} else {
		char *dot = strrchr(path, '.');
		if (dot != NULL) {
			*dot = '\0';
			if (stat(path, &sbuf) == 0) {
				return 1;
			}
		}
	}
	return 0;
}
#endif

int
x11_connect_display(struct ssh *ssh)
{
	u_int display_number;
	const char *display;
	char buf[1024], *cp;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr, sock = 0;

	/* Try to open a socket for the local X server. */
	display = getenv("DISPLAY");
	if (!display) {
		error("DISPLAY not set.");
		return -1;
	}
	/*
	 * Now we decode the value of the DISPLAY variable and make a
	 * connection to the real X server.
	 */

#ifdef __APPLE__
	/* Check if display is a path to a socket (as set by launchd). */
	{
		char path[PATH_MAX];

		if (is_path_to_xsocket(display, path, sizeof(path))) {
			debug("x11_connect_display: $DISPLAY is launchd");

			/* Create a socket. */
			sock = connect_local_xsocket_path(path);
			if (sock < 0)
				return -1;

			/* OK, we now have a connection to the display. */
			return sock;
		}
	}
#endif
	/*
	 * Check if it is a unix domain socket.  Unix domain displays are in
	 * one of the following formats: unix:d[.s], :d[.s], ::d[.s]
	 */
	if (strncmp(display, "unix:", 5) == 0 ||
	    display[0] == ':') {
		/* Connect to the unix domain socket. */
		if (sscanf(strrchr(display, ':') + 1, "%u",
		    &display_number) != 1) {
			error("Could not parse display number from DISPLAY: "
			    "%.100s", display);
			return -1;
		}
		/* Create a socket. */
		sock = connect_local_xsocket(display_number);
		if (sock < 0)
			return -1;

		/* OK, we now have a connection to the display. */
		return sock;
	}
	/*
	 * Connect to an inet socket.  The DISPLAY value is supposedly
	 * hostname:d[.s], where hostname may also be numeric IP address.
	 */
	strlcpy(buf, display, sizeof(buf));
	cp = strchr(buf, ':');
	if (!cp) {
		error("Could not find ':' in DISPLAY: %.100s", display);
		return -1;
	}
	*cp = 0;
	/*
	 * buf now contains the host name.  But first we parse the
	 * display number.
	 */
	if (sscanf(cp + 1, "%u", &display_number) != 1) {
		error("Could not parse display number from DISPLAY: %.100s",
		    display);
		return -1;
	}

	/* Look up the host address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = ssh->chanctxt->IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%u", 6000 + display_number);
	if ((gaierr = getaddrinfo(buf, strport, &hints, &aitop)) != 0) {
		error("%.100s: unknown host. (%s)", buf,
		ssh_gai_strerror(gaierr));
		return -1;
	}
	for (ai = aitop; ai; ai = ai->ai_next) {
		/* Create a socket. */
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock == -1) {
			debug2("socket: %.100s", strerror(errno));
			continue;
		}
		/* Connect it to the display. */
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
			debug2("connect %.100s port %u: %.100s", buf,
			    6000 + display_number, strerror(errno));
			close(sock);
			continue;
		}
		/* Success */
		break;
	}
	freeaddrinfo(aitop);
	if (!ai) {
		error("connect %.100s port %u: %.100s", buf,
		    6000 + display_number, strerror(errno));
		return -1;
	}
	set_nodelay(sock);
	return sock;
}

/*
 * Requests forwarding of X11 connections, generates fake authentication
 * data, and enables authentication spoofing.
 * This should be called in the client only.
 */
void
x11_request_forwarding_with_spoofing(struct ssh *ssh, int client_session_id,
    const char *disp, const char *proto, const char *data, int want_reply)
{
	struct ssh_channels *sc = ssh->chanctxt;
	u_int data_len = (u_int) strlen(data) / 2;
	u_int i, value;
	const char *cp;
	char *new_data;
	int r, screen_number;

	if (sc->x11_saved_display == NULL)
		sc->x11_saved_display = xstrdup(disp);
	else if (strcmp(disp, sc->x11_saved_display) != 0) {
		error("x11_request_forwarding_with_spoofing: different "
		    "$DISPLAY already forwarded");
		return;
	}

	cp = strchr(disp, ':');
	if (cp)
		cp = strchr(cp, '.');
	if (cp)
		screen_number = (u_int)strtonum(cp + 1, 0, 400, NULL);
	else
		screen_number = 0;

	if (sc->x11_saved_proto == NULL) {
		/* Save protocol name. */
		sc->x11_saved_proto = xstrdup(proto);

		/* Extract real authentication data. */
		sc->x11_saved_data = xmalloc(data_len);
		for (i = 0; i < data_len; i++) {
			if (sscanf(data + 2 * i, "%2x", &value) != 1) {
				fatal("x11_request_forwarding: bad "
				    "authentication data: %.100s", data);
			}
			sc->x11_saved_data[i] = value;
		}
		sc->x11_saved_data_len = data_len;

		/* Generate fake data of the same length. */
		sc->x11_fake_data = xmalloc(data_len);
		arc4random_buf(sc->x11_fake_data, data_len);
		sc->x11_fake_data_len = data_len;
	}

	/* Convert the fake data into hex. */
	new_data = tohex(sc->x11_fake_data, data_len);

	/* Send the request packet. */
	channel_request_start(ssh, client_session_id, "x11-req", want_reply);
	if ((r = sshpkt_put_u8(ssh, 0)) != 0 || /* bool: single connection */
	    (r = sshpkt_put_cstring(ssh, proto)) != 0 ||
	    (r = sshpkt_put_cstring(ssh, new_data)) != 0 ||
	    (r = sshpkt_put_u32(ssh, screen_number)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0 ||
	    (r = ssh_packet_write_wait(ssh)) != 0)
		fatal_fr(r, "send x11-req");
	free(new_data);
}
