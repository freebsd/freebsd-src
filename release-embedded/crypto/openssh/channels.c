/* $OpenBSD: channels.c,v 1.327 2013/11/08 00:39:15 djm Exp $ */
/* $FreeBSD$ */
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
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdarg.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "packet.h"
#include "log.h"
#include "misc.h"
#include "buffer.h"
#include "channels.h"
#include "compat.h"
#include "canohost.h"
#include "key.h"
#include "authfd.h"
#include "pathnames.h"

/* -- channel core */

/*
 * Pointer to an array containing all allocated channels.  The array is
 * dynamically extended as needed.
 */
static Channel **channels = NULL;

/*
 * Size of the channel array.  All slots of the array must always be
 * initialized (at least the type field); unused slots set to NULL
 */
static u_int channels_alloc = 0;

/*
 * Maximum file descriptor value used in any of the channels.  This is
 * updated in channel_new.
 */
static int channel_max_fd = 0;


/* -- tcp forwarding */

/*
 * Data structure for storing which hosts are permitted for forward requests.
 * The local sides of any remote forwards are stored in this array to prevent
 * a corrupt remote server from accessing arbitrary TCP/IP ports on our local
 * network (which might be behind a firewall).
 */
typedef struct {
	char *host_to_connect;		/* Connect to 'host'. */
	u_short port_to_connect;	/* Connect to 'port'. */
	u_short listen_port;		/* Remote side should listen port number. */
} ForwardPermission;

/* List of all permitted host/port pairs to connect by the user. */
static ForwardPermission *permitted_opens = NULL;

/* List of all permitted host/port pairs to connect by the admin. */
static ForwardPermission *permitted_adm_opens = NULL;

/* Number of permitted host/port pairs in the array permitted by the user. */
static int num_permitted_opens = 0;

/* Number of permitted host/port pair in the array permitted by the admin. */
static int num_adm_permitted_opens = 0;

/* special-case port number meaning allow any port */
#define FWD_PERMIT_ANY_PORT	0

/*
 * If this is true, all opens are permitted.  This is the case on the server
 * on which we have to trust the client anyway, and the user could do
 * anything after logging in anyway.
 */
static int all_opens_permitted = 0;


/* -- X11 forwarding */

/* Maximum number of fake X11 displays to try. */
#define MAX_DISPLAYS  1000

/* Saved X11 local (client) display. */
static char *x11_saved_display = NULL;

/* Saved X11 authentication protocol name. */
static char *x11_saved_proto = NULL;

/* Saved X11 authentication data.  This is the real data. */
static char *x11_saved_data = NULL;
static u_int x11_saved_data_len = 0;

/*
 * Fake X11 authentication data.  This is what the server will be sending us;
 * we should replace any occurrences of this by the real data.
 */
static u_char *x11_fake_data = NULL;
static u_int x11_fake_data_len;


/* -- agent forwarding */

#define	NUM_SOCKS	10

/* AF_UNSPEC or AF_INET or AF_INET6 */
static int IPv4or6 = AF_UNSPEC;

/* helper */
static void port_open_helper(Channel *c, char *rtype);

/* non-blocking connect helpers */
static int connect_next(struct channel_connect *);
static void channel_connect_ctx_free(struct channel_connect *);

/* -- HPN */

static int hpn_disabled = 0;
static u_int buffer_size = CHAN_HPN_MIN_WINDOW_DEFAULT;

/* -- channel core */

Channel *
channel_by_id(int id)
{
	Channel *c;

	if (id < 0 || (u_int)id >= channels_alloc) {
		logit("channel_by_id: %d: bad id", id);
		return NULL;
	}
	c = channels[id];
	if (c == NULL) {
		logit("channel_by_id: %d: bad id: channel free", id);
		return NULL;
	}
	return c;
}

/*
 * Returns the channel if it is allowed to receive protocol messages.
 * Private channels, like listening sockets, may not receive messages.
 */
Channel *
channel_lookup(int id)
{
	Channel *c;

	if ((c = channel_by_id(id)) == NULL)
		return (NULL);

	switch (c->type) {
	case SSH_CHANNEL_X11_OPEN:
	case SSH_CHANNEL_LARVAL:
	case SSH_CHANNEL_CONNECTING:
	case SSH_CHANNEL_DYNAMIC:
	case SSH_CHANNEL_OPENING:
	case SSH_CHANNEL_OPEN:
	case SSH_CHANNEL_INPUT_DRAINING:
	case SSH_CHANNEL_OUTPUT_DRAINING:
	case SSH_CHANNEL_ABANDONED:
		return (c);
	}
	logit("Non-public channel %d, type %d.", id, c->type);
	return (NULL);
}

/*
 * Register filedescriptors for a channel, used when allocating a channel or
 * when the channel consumer/producer is ready, e.g. shell exec'd
 */
static void
channel_register_fds(Channel *c, int rfd, int wfd, int efd,
    int extusage, int nonblock, int is_tty)
{
	/* Update the maximum file descriptor value. */
	channel_max_fd = MAX(channel_max_fd, rfd);
	channel_max_fd = MAX(channel_max_fd, wfd);
	channel_max_fd = MAX(channel_max_fd, efd);

	if (rfd != -1)
		fcntl(rfd, F_SETFD, FD_CLOEXEC);
	if (wfd != -1 && wfd != rfd)
		fcntl(wfd, F_SETFD, FD_CLOEXEC);
	if (efd != -1 && efd != rfd && efd != wfd)
		fcntl(efd, F_SETFD, FD_CLOEXEC);

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
	if (nonblock) {
		if (rfd != -1)
			set_nonblock(rfd);
		if (wfd != -1)
			set_nonblock(wfd);
		if (efd != -1)
			set_nonblock(efd);
	}
}

/*
 * Allocate a new channel object and set its type and socket. This will cause
 * remote_name to be freed.
 */
Channel *
channel_new(char *ctype, int type, int rfd, int wfd, int efd,
    u_int window, u_int maxpack, int extusage, char *remote_name, int nonblock)
{
	int found;
	u_int i;
	Channel *c;

	/* Do initial allocation if this is the first call. */
	if (channels_alloc == 0) {
		channels_alloc = 10;
		channels = xcalloc(channels_alloc, sizeof(Channel *));
		for (i = 0; i < channels_alloc; i++)
			channels[i] = NULL;
	}
	/* Try to find a free slot where to put the new channel. */
	for (found = -1, i = 0; i < channels_alloc; i++)
		if (channels[i] == NULL) {
			/* Found a free slot. */
			found = (int)i;
			break;
		}
	if (found < 0) {
		/* There are no free slots.  Take last+1 slot and expand the array.  */
		found = channels_alloc;
		if (channels_alloc > 10000)
			fatal("channel_new: internal error: channels_alloc %d "
			    "too big.", channels_alloc);
		channels = xrealloc(channels, channels_alloc + 10,
		    sizeof(Channel *));
		channels_alloc += 10;
		debug2("channel: expanding %d", channels_alloc);
		for (i = found; i < channels_alloc; i++)
			channels[i] = NULL;
	}
	/* Initialize and return new channel. */
	c = channels[found] = xcalloc(1, sizeof(Channel));
	buffer_init(&c->input);
	buffer_init(&c->output);
	buffer_init(&c->extended);
	c->path = NULL;
	c->listening_addr = NULL;
	c->listening_port = 0;
	c->ostate = CHAN_OUTPUT_OPEN;
	c->istate = CHAN_INPUT_OPEN;
	c->flags = 0;
	channel_register_fds(c, rfd, wfd, efd, extusage, nonblock, 0);
	c->notbefore = 0;
	c->self = found;
	c->type = type;
	c->ctype = ctype;
	c->dynamic_window = 0;
	c->local_window = window;
	c->local_window_max = window;
	c->local_consumed = 0;
	c->local_maxpacket = maxpack;
	c->remote_id = -1;
	c->remote_name = xstrdup(remote_name);
	c->remote_window = 0;
	c->remote_maxpacket = 0;
	c->force_drain = 0;
	c->single_connection = 0;
	c->detach_user = NULL;
	c->detach_close = 0;
	c->open_confirm = NULL;
	c->open_confirm_ctx = NULL;
	c->input_filter = NULL;
	c->output_filter = NULL;
	c->filter_ctx = NULL;
	c->filter_cleanup = NULL;
	c->ctl_chan = -1;
	c->mux_rcb = NULL;
	c->mux_ctx = NULL;
	c->mux_pause = 0;
	c->delayed = 1;		/* prevent call to channel_post handler */
	TAILQ_INIT(&c->status_confirms);
	debug("channel %d: new [%s]", found, remote_name);
	return c;
}

static int
channel_find_maxfd(void)
{
	u_int i;
	int max = 0;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c != NULL) {
			max = MAX(max, c->rfd);
			max = MAX(max, c->wfd);
			max = MAX(max, c->efd);
		}
	}
	return max;
}

int
channel_close_fd(int *fdp)
{
	int ret = 0, fd = *fdp;

	if (fd != -1) {
		ret = close(fd);
		*fdp = -1;
		if (fd == channel_max_fd)
			channel_max_fd = channel_find_maxfd();
	}
	return ret;
}

/* Close all channel fd/socket. */
static void
channel_close_fds(Channel *c)
{
	channel_close_fd(&c->sock);
	channel_close_fd(&c->rfd);
	channel_close_fd(&c->wfd);
	channel_close_fd(&c->efd);
}

/* Free the channel and close its fd/socket. */
void
channel_free(Channel *c)
{
	char *s;
	u_int i, n;
	struct channel_confirm *cc;

	for (n = 0, i = 0; i < channels_alloc; i++)
		if (channels[i])
			n++;
	debug("channel %d: free: %s, nchannels %u", c->self,
	    c->remote_name ? c->remote_name : "???", n);

	s = channel_open_message();
	debug3("channel %d: status: %s", c->self, s);
	free(s);

	if (c->sock != -1)
		shutdown(c->sock, SHUT_RDWR);
	channel_close_fds(c);
	buffer_free(&c->input);
	buffer_free(&c->output);
	buffer_free(&c->extended);
	free(c->remote_name);
	c->remote_name = NULL;
	free(c->path);
	c->path = NULL;
	free(c->listening_addr);
	c->listening_addr = NULL;
	while ((cc = TAILQ_FIRST(&c->status_confirms)) != NULL) {
		if (cc->abandon_cb != NULL)
			cc->abandon_cb(c, cc->ctx);
		TAILQ_REMOVE(&c->status_confirms, cc, entry);
		bzero(cc, sizeof(*cc));
		free(cc);
	}
	if (c->filter_cleanup != NULL && c->filter_ctx != NULL)
		c->filter_cleanup(c->self, c->filter_ctx);
	channels[c->self] = NULL;
	free(c);
}

void
channel_free_all(void)
{
	u_int i;

	for (i = 0; i < channels_alloc; i++)
		if (channels[i] != NULL)
			channel_free(channels[i]);
}

/*
 * Closes the sockets/fds of all channels.  This is used to close extra file
 * descriptors after a fork.
 */
void
channel_close_all(void)
{
	u_int i;

	for (i = 0; i < channels_alloc; i++)
		if (channels[i] != NULL)
			channel_close_fds(channels[i]);
}

/*
 * Stop listening to channels.
 */
void
channel_stop_listening(void)
{
	u_int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c != NULL) {
			switch (c->type) {
			case SSH_CHANNEL_AUTH_SOCKET:
			case SSH_CHANNEL_PORT_LISTENER:
			case SSH_CHANNEL_RPORT_LISTENER:
			case SSH_CHANNEL_X11_LISTENER:
				channel_close_fd(&c->sock);
				channel_free(c);
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
channel_not_very_much_buffered_data(void)
{
	u_int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c != NULL && c->type == SSH_CHANNEL_OPEN) {
#if 0
			if (!compat20 &&
			    buffer_len(&c->input) > packet_get_maxsize()) {
				debug2("channel %d: big input buffer %d",
				    c->self, buffer_len(&c->input));
				return 0;
			}
#endif
			if (buffer_len(&c->output) > packet_get_maxsize()) {
				debug2("channel %d: big output buffer %u > %u",
				    c->self, buffer_len(&c->output),
				    packet_get_maxsize());
				return 0;
			}
		}
	}
	return 1;
}

/* Returns true if any channel is still open. */
int
channel_still_open(void)
{
	u_int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
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
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
		case SSH_CHANNEL_ABANDONED:
			continue;
		case SSH_CHANNEL_LARVAL:
			if (!compat20)
				fatal("cannot happen: SSH_CHANNEL_LARVAL");
			continue;
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_MUX_CLIENT:
			return 1;
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			if (!compat13)
				fatal("cannot happen: OUT_DRAIN");
			return 1;
		default:
			fatal("channel_still_open: bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	return 0;
}

/* Returns the id of an open channel suitable for keepaliving */
int
channel_find_open(void)
{
	u_int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL || c->remote_id < 0)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_MUX_LISTENER:
		case SSH_CHANNEL_MUX_CLIENT:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
		case SSH_CHANNEL_ABANDONED:
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_AUTH_SOCKET:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
			return i;
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			if (!compat13)
				fatal("cannot happen: OUT_DRAIN");
			return i;
		default:
			fatal("channel_find_open: bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	return -1;
}


/*
 * Returns a message describing the currently open forwarded connections,
 * suitable for sending to the client.  The message contains crlf pairs for
 * newlines.
 */
char *
channel_open_message(void)
{
	Buffer buffer;
	Channel *c;
	char buf[1024], *cp;
	u_int i;

	buffer_init(&buffer);
	snprintf(buf, sizeof buf, "The following connections are open:\r\n");
	buffer_append(&buffer, buf, strlen(buf));
	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
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
		case SSH_CHANNEL_MUX_CLIENT:
		case SSH_CHANNEL_MUX_LISTENER:
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			snprintf(buf, sizeof buf,
			    "  #%d %.300s (t%d r%d i%d/%d o%d/%d fd %d/%d cc %d)\r\n",
			    c->self, c->remote_name,
			    c->type, c->remote_id,
			    c->istate, buffer_len(&c->input),
			    c->ostate, buffer_len(&c->output),
			    c->rfd, c->wfd, c->ctl_chan);
			buffer_append(&buffer, buf, strlen(buf));
			continue;
		default:
			fatal("channel_open_message: bad channel type %d", c->type);
			/* NOTREACHED */
		}
	}
	buffer_append(&buffer, "\0", 1);
	cp = xstrdup(buffer_ptr(&buffer));
	buffer_free(&buffer);
	return cp;
}

void
channel_send_open(int id)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		logit("channel_send_open: %d: bad id", id);
		return;
	}
	debug2("channel %d: send open", id);
	packet_start(SSH2_MSG_CHANNEL_OPEN);
	packet_put_cstring(c->ctype);
	packet_put_int(c->self);
	packet_put_int(c->local_window);
	packet_put_int(c->local_maxpacket);
	packet_send();
}

void
channel_request_start(int id, char *service, int wantconfirm)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		logit("channel_request_start: %d: unknown channel id", id);
		return;
	}
	debug2("channel %d: request %s confirm %d", id, service, wantconfirm);
	packet_start(SSH2_MSG_CHANNEL_REQUEST);
	packet_put_int(c->remote_id);
	packet_put_cstring(service);
	packet_put_char(wantconfirm);
}

void
channel_register_status_confirm(int id, channel_confirm_cb *cb,
    channel_confirm_abandon_cb *abandon_cb, void *ctx)
{
	struct channel_confirm *cc;
	Channel *c;

	if ((c = channel_lookup(id)) == NULL)
		fatal("channel_register_expect: %d: bad id", id);

	cc = xcalloc(1, sizeof(*cc));
	cc->cb = cb;
	cc->abandon_cb = abandon_cb;
	cc->ctx = ctx;
	TAILQ_INSERT_TAIL(&c->status_confirms, cc, entry);
}

void
channel_register_open_confirm(int id, channel_open_fn *fn, void *ctx)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		logit("channel_register_open_confirm: %d: bad id", id);
		return;
	}
	c->open_confirm = fn;
	c->open_confirm_ctx = ctx;
}

void
channel_register_cleanup(int id, channel_callback_fn *fn, int do_close)
{
	Channel *c = channel_by_id(id);

	if (c == NULL) {
		logit("channel_register_cleanup: %d: bad id", id);
		return;
	}
	c->detach_user = fn;
	c->detach_close = do_close;
}

void
channel_cancel_cleanup(int id)
{
	Channel *c = channel_by_id(id);

	if (c == NULL) {
		logit("channel_cancel_cleanup: %d: bad id", id);
		return;
	}
	c->detach_user = NULL;
	c->detach_close = 0;
}

void
channel_register_filter(int id, channel_infilter_fn *ifn,
    channel_outfilter_fn *ofn, channel_filter_cleanup_fn *cfn, void *ctx)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		logit("channel_register_filter: %d: bad id", id);
		return;
	}
	c->input_filter = ifn;
	c->output_filter = ofn;
	c->filter_ctx = ctx;
	c->filter_cleanup = cfn;
}

void
channel_set_fds(int id, int rfd, int wfd, int efd,
    int extusage, int nonblock, int is_tty, u_int window_max)
{
	Channel *c = channel_lookup(id);

	if (c == NULL || c->type != SSH_CHANNEL_LARVAL)
		fatal("channel_activate for non-larval channel %d.", id);
	channel_register_fds(c, rfd, wfd, efd, extusage, nonblock, is_tty);
	c->type = SSH_CHANNEL_OPEN;
	c->local_window = c->local_window_max = window_max;
	packet_start(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
	packet_put_int(c->remote_id);
	packet_put_int(c->local_window);
	packet_send();
}

/*
 * 'channel_pre*' are called just before select() to add any bits relevant to
 * channels in the select bitmasks.
 */
/*
 * 'channel_post*': perform any appropriate operations for channels which
 * have events pending.
 */
typedef void chan_fn(Channel *c, fd_set *readset, fd_set *writeset);
chan_fn *channel_pre[SSH_CHANNEL_MAX_TYPE];
chan_fn *channel_post[SSH_CHANNEL_MAX_TYPE];

/* ARGSUSED */
static void
channel_pre_listener(Channel *c, fd_set *readset, fd_set *writeset)
{
	FD_SET(c->sock, readset);
}

/* ARGSUSED */
static void
channel_pre_connecting(Channel *c, fd_set *readset, fd_set *writeset)
{
	debug3("channel %d: waiting for connection", c->self);
	FD_SET(c->sock, writeset);
}

static void
channel_pre_open_13(Channel *c, fd_set *readset, fd_set *writeset)
{
	if (buffer_len(&c->input) < packet_get_maxsize())
		FD_SET(c->sock, readset);
	if (buffer_len(&c->output) > 0)
		FD_SET(c->sock, writeset);
}

static u_int
channel_tcpwinsz(void)
{
	u_int32_t tcpwinsz;
	socklen_t optsz;
	int ret, sd;
	u_int maxlen;

	/* If we are not on a socket return 128KB. */
	if (!packet_connection_is_on_socket())
		return (128 * 1024);

	tcpwinsz = 0;
	optsz = sizeof(tcpwinsz);
	sd = packet_get_connection_in();
	ret = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &tcpwinsz, &optsz);

	/* Return no more than the maximum buffer size. */
	maxlen = buffer_get_max_len();
	if ((ret == 0) && tcpwinsz > maxlen)
		tcpwinsz = maxlen;
	/* In case getsockopt() failed return a minimum. */
	if (tcpwinsz == 0)
		tcpwinsz = CHAN_TCP_WINDOW_DEFAULT;
	debug2("tcpwinsz: %d for connection: %d", tcpwinsz, sd);
	return (tcpwinsz);
}

static void
channel_pre_open(Channel *c, fd_set *readset, fd_set *writeset)
{
	u_int limit;

	/* Check buffer limits. */
	if (!c->tcpwinsz || c->dynamic_window > 0)
		c->tcpwinsz = channel_tcpwinsz();

	limit = MIN(compat20 ? c->remote_window : packet_get_maxsize(),
	    2 * c->tcpwinsz);

	if (c->istate == CHAN_INPUT_OPEN &&
	    limit > 0 &&
	    buffer_len(&c->input) < limit &&
	    buffer_check_alloc(&c->input, CHAN_RBUF))
		FD_SET(c->rfd, readset);
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (buffer_len(&c->output) > 0) {
			FD_SET(c->wfd, writeset);
		} else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
			if (CHANNEL_EFD_OUTPUT_ACTIVE(c))
				debug2("channel %d: obuf_empty delayed efd %d/(%d)",
				    c->self, c->efd, buffer_len(&c->extended));
			else
				chan_obuf_empty(c);
		}
	}
	/** XXX check close conditions, too */
	if (compat20 && c->efd != -1 && 
	    !(c->istate == CHAN_INPUT_CLOSED && c->ostate == CHAN_OUTPUT_CLOSED)) {
		if (c->extended_usage == CHAN_EXTENDED_WRITE &&
		    buffer_len(&c->extended) > 0)
			FD_SET(c->efd, writeset);
		else if (c->efd != -1 && !(c->flags & CHAN_EOF_SENT) &&
		    (c->extended_usage == CHAN_EXTENDED_READ ||
		    c->extended_usage == CHAN_EXTENDED_IGNORE) &&
		    buffer_len(&c->extended) < c->remote_window)
			FD_SET(c->efd, readset);
	}
	/* XXX: What about efd? races? */
}

/* ARGSUSED */
static void
channel_pre_input_draining(Channel *c, fd_set *readset, fd_set *writeset)
{
	if (buffer_len(&c->input) == 0) {
		packet_start(SSH_MSG_CHANNEL_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
		c->type = SSH_CHANNEL_CLOSED;
		debug2("channel %d: closing after input drain.", c->self);
	}
}

/* ARGSUSED */
static void
channel_pre_output_draining(Channel *c, fd_set *readset, fd_set *writeset)
{
	if (buffer_len(&c->output) == 0)
		chan_mark_dead(c);
	else
		FD_SET(c->sock, writeset);
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
x11_open_helper(Buffer *b)
{
	u_char *ucp;
	u_int proto_len, data_len;

	/* Check if the fixed size part of the packet is in buffer. */
	if (buffer_len(b) < 12)
		return 0;

	/* Parse the lengths of variable-length fields. */
	ucp = buffer_ptr(b);
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
	if (buffer_len(b) <
	    12 + ((proto_len + 3) & ~3) + ((data_len + 3) & ~3))
		return 0;

	/* Check if authentication protocol matches. */
	if (proto_len != strlen(x11_saved_proto) ||
	    memcmp(ucp + 12, x11_saved_proto, proto_len) != 0) {
		debug2("X11 connection uses different authentication protocol.");
		return -1;
	}
	/* Check if authentication data matches our fake data. */
	if (data_len != x11_fake_data_len ||
	    timingsafe_bcmp(ucp + 12 + ((proto_len + 3) & ~3),
		x11_fake_data, x11_fake_data_len) != 0) {
		debug2("X11 auth data does not match fake data.");
		return -1;
	}
	/* Check fake data length */
	if (x11_fake_data_len != x11_saved_data_len) {
		error("X11 fake_data_len %d != saved_data_len %d",
		    x11_fake_data_len, x11_saved_data_len);
		return -1;
	}
	/*
	 * Received authentication protocol and data match
	 * our fake data. Substitute the fake data with real
	 * data.
	 */
	memcpy(ucp + 12 + ((proto_len + 3) & ~3),
	    x11_saved_data, x11_saved_data_len);
	return 1;
}

static void
channel_pre_x11_open_13(Channel *c, fd_set *readset, fd_set *writeset)
{
	int ret = x11_open_helper(&c->output);

	if (ret == 1) {
		/* Start normal processing for the channel. */
		c->type = SSH_CHANNEL_OPEN;
		channel_pre_open_13(c, readset, writeset);
	} else if (ret == -1) {
		/*
		 * We have received an X11 connection that has bad
		 * authentication information.
		 */
		logit("X11 connection rejected because of wrong authentication.");
		buffer_clear(&c->input);
		buffer_clear(&c->output);
		channel_close_fd(&c->sock);
		c->sock = -1;
		c->type = SSH_CHANNEL_CLOSED;
		packet_start(SSH_MSG_CHANNEL_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
	}
}

static void
channel_pre_x11_open(Channel *c, fd_set *readset, fd_set *writeset)
{
	int ret = x11_open_helper(&c->output);

	/* c->force_drain = 1; */

	if (ret == 1) {
		c->type = SSH_CHANNEL_OPEN;
		channel_pre_open(c, readset, writeset);
	} else if (ret == -1) {
		logit("X11 connection rejected because of wrong authentication.");
		debug2("X11 rejected %d i%d/o%d", c->self, c->istate, c->ostate);
		chan_read_failed(c);
		buffer_clear(&c->input);
		chan_ibuf_empty(c);
		buffer_clear(&c->output);
		/* for proto v1, the peer will send an IEOF */
		if (compat20)
			chan_write_failed(c);
		else
			c->type = SSH_CHANNEL_OPEN;
		debug2("X11 closed %d i%d/o%d", c->self, c->istate, c->ostate);
	}
}

static void
channel_pre_mux_client(Channel *c, fd_set *readset, fd_set *writeset)
{
	if (c->istate == CHAN_INPUT_OPEN && !c->mux_pause &&
	    buffer_check_alloc(&c->input, CHAN_RBUF))
		FD_SET(c->rfd, readset);
	if (c->istate == CHAN_INPUT_WAIT_DRAIN) {
		/* clear buffer immediately (discard any partial packet) */
		buffer_clear(&c->input);
		chan_ibuf_empty(c);
		/* Start output drain. XXX just kill chan? */
		chan_rcvd_oclose(c);
	}
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (buffer_len(&c->output) > 0)
			FD_SET(c->wfd, writeset);
		else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN)
			chan_obuf_empty(c);
	}
}

/* try to decode a socks4 header */
/* ARGSUSED */
static int
channel_decode_socks4(Channel *c, fd_set *readset, fd_set *writeset)
{
	char *p, *host;
	u_int len, have, i, found, need;
	char username[256];
	struct {
		u_int8_t version;
		u_int8_t command;
		u_int16_t dest_port;
		struct in_addr dest_addr;
	} s4_req, s4_rsp;

	debug2("channel %d: decode socks4", c->self);

	have = buffer_len(&c->input);
	len = sizeof(s4_req);
	if (have < len)
		return 0;
	p = buffer_ptr(&c->input);

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
	buffer_get(&c->input, (char *)&s4_req.version, 1);
	buffer_get(&c->input, (char *)&s4_req.command, 1);
	buffer_get(&c->input, (char *)&s4_req.dest_port, 2);
	buffer_get(&c->input, (char *)&s4_req.dest_addr, 4);
	have = buffer_len(&c->input);
	p = buffer_ptr(&c->input);
	len = strlen(p);
	debug2("channel %d: decode socks4: user %s/%d", c->self, p, len);
	len++;					/* trailing '\0' */
	if (len > have)
		fatal("channel %d: decode socks4: len %d > have %d",
		    c->self, len, have);
	strlcpy(username, p, sizeof(username));
	buffer_consume(&c->input, len);

	free(c->path);
	c->path = NULL;
	if (need == 1) {			/* SOCKS4: one string */
		host = inet_ntoa(s4_req.dest_addr);
		c->path = xstrdup(host);
	} else {				/* SOCKS4A: two strings */
		have = buffer_len(&c->input);
		p = buffer_ptr(&c->input);
		len = strlen(p);
		debug2("channel %d: decode socks4a: host %s/%d",
		    c->self, p, len);
		len++;				/* trailing '\0' */
		if (len > have)
			fatal("channel %d: decode socks4a: len %d > have %d",
			    c->self, len, have);
		if (len > NI_MAXHOST) {
			error("channel %d: hostname \"%.100s\" too long",
			    c->self, p);
			return -1;
		}
		c->path = xstrdup(p);
		buffer_consume(&c->input, len);
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
	buffer_append(&c->output, &s4_rsp, sizeof(s4_rsp));
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

/* ARGSUSED */
static int
channel_decode_socks5(Channel *c, fd_set *readset, fd_set *writeset)
{
	struct {
		u_int8_t version;
		u_int8_t command;
		u_int8_t reserved;
		u_int8_t atyp;
	} s5_req, s5_rsp;
	u_int16_t dest_port;
	char dest_addr[255+1], ntop[INET6_ADDRSTRLEN];
	u_char *p;
	u_int have, need, i, found, nmethods, addrlen, af;

	debug2("channel %d: decode socks5", c->self);
	p = buffer_ptr(&c->input);
	if (p[0] != 0x05)
		return -1;
	have = buffer_len(&c->input);
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
		buffer_consume(&c->input, nmethods + 2);
		buffer_put_char(&c->output, 0x05);		/* version */
		buffer_put_char(&c->output, SSH_SOCKS5_NOAUTH);	/* method */
		FD_SET(c->sock, writeset);
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
	buffer_consume(&c->input, sizeof(s5_req));
	if (s5_req.atyp == SSH_SOCKS5_DOMAIN)
		buffer_consume(&c->input, 1);    /* host string length */
	buffer_get(&c->input, &dest_addr, addrlen);
	buffer_get(&c->input, (char *)&dest_port, 2);
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

	buffer_append(&c->output, &s5_rsp, sizeof(s5_rsp));
	buffer_put_int(&c->output, ntohl(INADDR_ANY)); /* bind address */
	buffer_append(&c->output, &dest_port, sizeof(dest_port));
	return 1;
}

Channel *
channel_connect_stdio_fwd(const char *host_to_connect, u_short port_to_connect,
    int in, int out)
{
	Channel *c;

	debug("channel_connect_stdio_fwd %s:%d", host_to_connect,
	    port_to_connect);

	c = channel_new("stdio-forward", SSH_CHANNEL_OPENING, in, out,
	    -1, CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
	    0, "stdio-forward", /*nonblock*/0);

	c->path = xstrdup(host_to_connect);
	c->host_port = port_to_connect;
	c->listening_port = 0;
	c->force_drain = 1;

	channel_register_fds(c, in, out, -1, 0, 1, 0);
	port_open_helper(c, "direct-tcpip");

	return c;
}

/* dynamic port forwarding */
static void
channel_pre_dynamic(Channel *c, fd_set *readset, fd_set *writeset)
{
	u_char *p;
	u_int have;
	int ret;

	have = buffer_len(&c->input);
	debug2("channel %d: pre_dynamic: have %d", c->self, have);
	/* buffer_dump(&c->input); */
	/* check if the fixed size part of the packet is in buffer. */
	if (have < 3) {
		/* need more */
		FD_SET(c->sock, readset);
		return;
	}
	/* try to guess the protocol */
	p = buffer_ptr(&c->input);
	switch (p[0]) {
	case 0x04:
		ret = channel_decode_socks4(c, readset, writeset);
		break;
	case 0x05:
		ret = channel_decode_socks5(c, readset, writeset);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret < 0) {
		chan_mark_dead(c);
	} else if (ret == 0) {
		debug2("channel %d: pre_dynamic: need more", c->self);
		/* need more */
		FD_SET(c->sock, readset);
	} else {
		/* switch to the next state */
		c->type = SSH_CHANNEL_OPENING;
		port_open_helper(c, "direct-tcpip");
	}
}

/* This is our fake X11 server socket. */
/* ARGSUSED */
static void
channel_post_x11_listener(Channel *c, fd_set *readset, fd_set *writeset)
{
	Channel *nc;
	struct sockaddr_storage addr;
	int newsock, oerrno;
	socklen_t addrlen;
	char buf[16384], *remote_ipaddr;
	int remote_port;

	if (FD_ISSET(c->sock, readset)) {
		debug("X11 connection requested.");
		addrlen = sizeof(addr);
		newsock = accept(c->sock, (struct sockaddr *)&addr, &addrlen);
		if (c->single_connection) {
			oerrno = errno;
			debug2("single_connection: closing X11 listener.");
			channel_close_fd(&c->sock);
			chan_mark_dead(c);
			errno = oerrno;
		}
		if (newsock < 0) {
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

		nc = channel_new("accepted x11 socket",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket, 0, buf, 1);
		if (compat20) {
			packet_start(SSH2_MSG_CHANNEL_OPEN);
			packet_put_cstring("x11");
			packet_put_int(nc->self);
			packet_put_int(nc->local_window_max);
			packet_put_int(nc->local_maxpacket);
			/* originator ipaddr and port */
			packet_put_cstring(remote_ipaddr);
			if (datafellows & SSH_BUG_X11FWD) {
				debug2("ssh2 x11 bug compat mode");
			} else {
				packet_put_int(remote_port);
			}
			packet_send();
		} else {
			packet_start(SSH_SMSG_X11_OPEN);
			packet_put_int(nc->self);
			if (packet_get_protocol_flags() &
			    SSH_PROTOFLAG_HOST_IN_FWD_OPEN)
				packet_put_cstring(buf);
			packet_send();
		}
		free(remote_ipaddr);
	}
}

static void
port_open_helper(Channel *c, char *rtype)
{
	int direct;
	char buf[1024];
	char *remote_ipaddr = get_peer_ipaddr(c->sock);
	int remote_port = get_peer_port(c->sock);

	if (remote_port == -1) {
		/* Fake addr/port to appease peers that validate it (Tectia) */
		free(remote_ipaddr);
		remote_ipaddr = xstrdup("127.0.0.1");
		remote_port = 65535;
	}

	direct = (strcmp(rtype, "direct-tcpip") == 0);

	snprintf(buf, sizeof buf,
	    "%s: listening port %d for %.100s port %d, "
	    "connect from %.200s port %d",
	    rtype, c->listening_port, c->path, c->host_port,
	    remote_ipaddr, remote_port);

	free(c->remote_name);
	c->remote_name = xstrdup(buf);

	if (compat20) {
		packet_start(SSH2_MSG_CHANNEL_OPEN);
		packet_put_cstring(rtype);
		packet_put_int(c->self);
		packet_put_int(c->local_window_max);
		packet_put_int(c->local_maxpacket);
		if (direct) {
			/* target host, port */
			packet_put_cstring(c->path);
			packet_put_int(c->host_port);
		} else {
			/* listen address, port */
			packet_put_cstring(c->path);
			packet_put_int(c->listening_port);
		}
		/* originator host and port */
		packet_put_cstring(remote_ipaddr);
		packet_put_int((u_int)remote_port);
		packet_send();
	} else {
		packet_start(SSH_MSG_PORT_OPEN);
		packet_put_int(c->self);
		packet_put_cstring(c->path);
		packet_put_int(c->host_port);
		if (packet_get_protocol_flags() &
		    SSH_PROTOFLAG_HOST_IN_FWD_OPEN)
			packet_put_cstring(c->remote_name);
		packet_send();
	}
	free(remote_ipaddr);
}

static void
channel_set_reuseaddr(int fd)
{
	int on = 1;

	/*
	 * Set socket options.
	 * Allow local port reuse in TIME_WAIT.
	 */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
		error("setsockopt SO_REUSEADDR fd %d: %s", fd, strerror(errno));
}

/*
 * This socket is listening for connections to a forwarded TCP/IP port.
 */
/* ARGSUSED */
static void
channel_post_port_listener(Channel *c, fd_set *readset, fd_set *writeset)
{
	Channel *nc;
	struct sockaddr_storage addr;
	int newsock, nextstate;
	socklen_t addrlen;
	char *rtype;

	if (FD_ISSET(c->sock, readset)) {
		debug("Connection to port %d forwarding "
		    "to %.100s port %d requested.",
		    c->listening_port, c->path, c->host_port);

		if (c->type == SSH_CHANNEL_RPORT_LISTENER) {
			nextstate = SSH_CHANNEL_OPENING;
			rtype = "forwarded-tcpip";
		} else {
			if (c->host_port == 0) {
				nextstate = SSH_CHANNEL_DYNAMIC;
				rtype = "dynamic-tcpip";
			} else {
				nextstate = SSH_CHANNEL_OPENING;
				rtype = "direct-tcpip";
			}
		}

		addrlen = sizeof(addr);
		newsock = accept(c->sock, (struct sockaddr *)&addr, &addrlen);
		if (newsock < 0) {
			if (errno != EINTR && errno != EWOULDBLOCK &&
			    errno != ECONNABORTED)
				error("accept: %.100s", strerror(errno));
			if (errno == EMFILE || errno == ENFILE)
				c->notbefore = monotime() + 1;
			return;
		}
		set_nodelay(newsock);
		nc = channel_new(rtype, nextstate, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket, 0, rtype, 1);
		nc->listening_port = c->listening_port;
		nc->host_port = c->host_port;
		if (c->path != NULL)
			nc->path = xstrdup(c->path);

		if (nextstate != SSH_CHANNEL_DYNAMIC)
			port_open_helper(nc, rtype);
	}
}

/*
 * This is the authentication agent socket listening for connections from
 * clients.
 */
/* ARGSUSED */
static void
channel_post_auth_listener(Channel *c, fd_set *readset, fd_set *writeset)
{
	Channel *nc;
	int newsock;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	if (FD_ISSET(c->sock, readset)) {
		addrlen = sizeof(addr);
		newsock = accept(c->sock, (struct sockaddr *)&addr, &addrlen);
		if (newsock < 0) {
			error("accept from auth socket: %.100s",
			    strerror(errno));
			if (errno == EMFILE || errno == ENFILE)
				c->notbefore = monotime() + 1;
			return;
		}
		nc = channel_new("accepted auth socket",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, "accepted auth socket", 1);
		if (compat20) {
			packet_start(SSH2_MSG_CHANNEL_OPEN);
			packet_put_cstring("auth-agent@openssh.com");
			packet_put_int(nc->self);
			packet_put_int(c->local_window_max);
			packet_put_int(c->local_maxpacket);
		} else {
			packet_start(SSH_SMSG_AGENT_OPEN);
			packet_put_int(nc->self);
		}
		packet_send();
	}
}

/* ARGSUSED */
static void
channel_post_connecting(Channel *c, fd_set *readset, fd_set *writeset)
{
	int err = 0, sock;
	socklen_t sz = sizeof(err);

	if (FD_ISSET(c->sock, writeset)) {
		if (getsockopt(c->sock, SOL_SOCKET, SO_ERROR, &err, &sz) < 0) {
			err = errno;
			error("getsockopt SO_ERROR failed");
		}
		if (err == 0) {
			debug("channel %d: connected to %s port %d",
			    c->self, c->connect_ctx.host, c->connect_ctx.port);
			channel_connect_ctx_free(&c->connect_ctx);
			c->type = SSH_CHANNEL_OPEN;
			if (compat20) {
				packet_start(SSH2_MSG_CHANNEL_OPEN_CONFIRMATION);
				packet_put_int(c->remote_id);
				packet_put_int(c->self);
				packet_put_int(c->local_window);
				packet_put_int(c->local_maxpacket);
			} else {
				packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
				packet_put_int(c->remote_id);
				packet_put_int(c->self);
			}
		} else {
			debug("channel %d: connection failed: %s",
			    c->self, strerror(err));
			/* Try next address, if any */
			if ((sock = connect_next(&c->connect_ctx)) > 0) {
				close(c->sock);
				c->sock = c->rfd = c->wfd = sock;
				channel_max_fd = channel_find_maxfd();
				return;
			}
			/* Exhausted all addresses */
			error("connect_to %.100s port %d: failed.",
			    c->connect_ctx.host, c->connect_ctx.port);
			channel_connect_ctx_free(&c->connect_ctx);
			if (compat20) {
				packet_start(SSH2_MSG_CHANNEL_OPEN_FAILURE);
				packet_put_int(c->remote_id);
				packet_put_int(SSH2_OPEN_CONNECT_FAILED);
				if (!(datafellows & SSH_BUG_OPENFAILURE)) {
					packet_put_cstring(strerror(err));
					packet_put_cstring("");
				}
			} else {
				packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
				packet_put_int(c->remote_id);
			}
			chan_mark_dead(c);
		}
		packet_send();
	}
}

/* ARGSUSED */
static int
channel_handle_rfd(Channel *c, fd_set *readset, fd_set *writeset)
{
	char buf[CHAN_RBUF];
	int len, force;

	force = c->isatty && c->detach_close && c->istate != CHAN_INPUT_CLOSED;
	if (c->rfd != -1 && (force || FD_ISSET(c->rfd, readset))) {
		errno = 0;
		len = read(c->rfd, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR ||
		    ((errno == EAGAIN || errno == EWOULDBLOCK) && !force)))
			return 1;
#ifndef PTY_ZEROREAD
		if (len <= 0) {
#else
		if ((!c->isatty && len <= 0) ||
		    (c->isatty && (len < 0 || (len == 0 && errno != 0)))) {
#endif
			debug2("channel %d: read<=0 rfd %d len %d",
			    c->self, c->rfd, len);
			if (c->type != SSH_CHANNEL_OPEN) {
				debug2("channel %d: not open", c->self);
				chan_mark_dead(c);
				return -1;
			} else if (compat13) {
				buffer_clear(&c->output);
				c->type = SSH_CHANNEL_INPUT_DRAINING;
				debug2("channel %d: input draining.", c->self);
			} else {
				chan_read_failed(c);
			}
			return -1;
		}
		if (c->input_filter != NULL) {
			if (c->input_filter(c, buf, len) == -1) {
				debug2("channel %d: filter stops", c->self);
				chan_read_failed(c);
			}
		} else if (c->datagram) {
			buffer_put_string(&c->input, buf, len);
		} else {
			buffer_append(&c->input, buf, len);
		}
	}
	return 1;
}

/* ARGSUSED */
static int
channel_handle_wfd(Channel *c, fd_set *readset, fd_set *writeset)
{
	struct termios tio;
	u_char *data = NULL, *buf;
	u_int dlen, olen = 0;
	int len;

	/* Send buffered output data to the socket. */
	if (c->wfd != -1 &&
	    FD_ISSET(c->wfd, writeset) &&
	    buffer_len(&c->output) > 0) {
		olen = buffer_len(&c->output);
		if (c->output_filter != NULL) {
			if ((buf = c->output_filter(c, &data, &dlen)) == NULL) {
				debug2("channel %d: filter stops", c->self);
				if (c->type != SSH_CHANNEL_OPEN)
					chan_mark_dead(c);
				else
					chan_write_failed(c);
				return -1;
			}
		} else if (c->datagram) {
			buf = data = buffer_get_string(&c->output, &dlen);
		} else {
			buf = data = buffer_ptr(&c->output);
			dlen = buffer_len(&c->output);
		}

		if (c->datagram) {
			/* ignore truncated writes, datagrams might get lost */
			len = write(c->wfd, buf, dlen);
			free(data);
			if (len < 0 && (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK))
				return 1;
			if (len <= 0) {
				if (c->type != SSH_CHANNEL_OPEN)
					chan_mark_dead(c);
				else
					chan_write_failed(c);
				return -1;
			}
			goto out;
		}
#ifdef _AIX
		/* XXX: Later AIX versions can't push as much data to tty */
		if (compat20 && c->wfd_isatty)
			dlen = MIN(dlen, 8*1024);
#endif

		len = write(c->wfd, buf, dlen);
		if (len < 0 &&
		    (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
			return 1;
		if (len <= 0) {
			if (c->type != SSH_CHANNEL_OPEN) {
				debug2("channel %d: not open", c->self);
				chan_mark_dead(c);
				return -1;
			} else if (compat13) {
				buffer_clear(&c->output);
				debug2("channel %d: input draining.", c->self);
				c->type = SSH_CHANNEL_INPUT_DRAINING;
			} else {
				chan_write_failed(c);
			}
			return -1;
		}
#ifndef BROKEN_TCGETATTR_ICANON
		if (compat20 && c->isatty && dlen >= 1 && buf[0] != '\r') {
			if (tcgetattr(c->wfd, &tio) == 0 &&
			    !(tio.c_lflag & ECHO) && (tio.c_lflag & ICANON)) {
				/*
				 * Simulate echo to reduce the impact of
				 * traffic analysis. We need to match the
				 * size of a SSH2_MSG_CHANNEL_DATA message
				 * (4 byte channel id + buf)
				 */
				packet_send_ignore(4 + len);
				packet_send();
			}
		}
#endif
		buffer_consume(&c->output, len);
	}
 out:
	if (compat20 && olen > 0)
		c->local_consumed += olen - buffer_len(&c->output);
	return 1;
}

static int
channel_handle_efd(Channel *c, fd_set *readset, fd_set *writeset)
{
	char buf[CHAN_RBUF];
	int len;

/** XXX handle drain efd, too */
	if (c->efd != -1) {
		if (c->extended_usage == CHAN_EXTENDED_WRITE &&
		    FD_ISSET(c->efd, writeset) &&
		    buffer_len(&c->extended) > 0) {
			len = write(c->efd, buffer_ptr(&c->extended),
			    buffer_len(&c->extended));
			debug2("channel %d: written %d to efd %d",
			    c->self, len, c->efd);
			if (len < 0 && (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK))
				return 1;
			if (len <= 0) {
				debug2("channel %d: closing write-efd %d",
				    c->self, c->efd);
				channel_close_fd(&c->efd);
			} else {
				buffer_consume(&c->extended, len);
				c->local_consumed += len;
			}
		} else if (c->efd != -1 &&
		    (c->extended_usage == CHAN_EXTENDED_READ ||
		    c->extended_usage == CHAN_EXTENDED_IGNORE) &&
		    (c->detach_close || FD_ISSET(c->efd, readset))) {
			len = read(c->efd, buf, sizeof(buf));
			debug2("channel %d: read %d from efd %d",
			    c->self, len, c->efd);
			if (len < 0 && (errno == EINTR || ((errno == EAGAIN ||
			    errno == EWOULDBLOCK) && !c->detach_close)))
				return 1;
			if (len <= 0) {
				debug2("channel %d: closing read-efd %d",
				    c->self, c->efd);
				channel_close_fd(&c->efd);
			} else {
				if (c->extended_usage == CHAN_EXTENDED_IGNORE) {
					debug3("channel %d: discard efd",
					    c->self);
				} else
					buffer_append(&c->extended, buf, len);
			}
		}
	}
	return 1;
}

static int
channel_check_window(Channel *c)
{
	if (c->type == SSH_CHANNEL_OPEN &&
	    !(c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD)) &&
	    ((c->local_window_max - c->local_window >
	    c->local_maxpacket*3) ||
	    c->local_window < c->local_window_max/2) &&
	    c->local_consumed > 0) {
		u_int addition = 0;

		/* Adjust max window size if we are in a dynamic environment. */
		if (c->dynamic_window && c->tcpwinsz > c->local_window_max) {
			/*
			 * Grow the window somewhat aggressively to maintain
			 * pressure.
			 */
			addition = 1.5 * (c->tcpwinsz - c->local_window_max);
			c->local_window_max += addition;
		}
		packet_start(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
		packet_put_int(c->remote_id);
		packet_put_int(c->local_consumed + addition);
		packet_send();
		debug2("channel %d: window %d sent adjust %d",
		    c->self, c->local_window,
		    c->local_consumed);
		c->local_window += c->local_consumed + addition;
		c->local_consumed = 0;
	}
	return 1;
}

static void
channel_post_open(Channel *c, fd_set *readset, fd_set *writeset)
{
	channel_handle_rfd(c, readset, writeset);
	channel_handle_wfd(c, readset, writeset);
	if (!compat20)
		return;
	channel_handle_efd(c, readset, writeset);
	channel_check_window(c);
}

static u_int
read_mux(Channel *c, u_int need)
{
	char buf[CHAN_RBUF];
	int len;
	u_int rlen;

	if (buffer_len(&c->input) < need) {
		rlen = need - buffer_len(&c->input);
		len = read(c->rfd, buf, MIN(rlen, CHAN_RBUF));
		if (len <= 0) {
			if (errno != EINTR && errno != EAGAIN) {
				debug2("channel %d: ctl read<=0 rfd %d len %d",
				    c->self, c->rfd, len);
				chan_read_failed(c);
				return 0;
			}
		} else
			buffer_append(&c->input, buf, len);
	}
	return buffer_len(&c->input);
}

static void
channel_post_mux_client(Channel *c, fd_set *readset, fd_set *writeset)
{
	u_int need;
	ssize_t len;

	if (!compat20)
		fatal("%s: entered with !compat20", __func__);

	if (c->rfd != -1 && !c->mux_pause && FD_ISSET(c->rfd, readset) &&
	    (c->istate == CHAN_INPUT_OPEN ||
	    c->istate == CHAN_INPUT_WAIT_DRAIN)) {
		/*
		 * Don't not read past the precise end of packets to
		 * avoid disrupting fd passing.
		 */
		if (read_mux(c, 4) < 4) /* read header */
			return;
		need = get_u32(buffer_ptr(&c->input));
#define CHANNEL_MUX_MAX_PACKET	(256 * 1024)
		if (need > CHANNEL_MUX_MAX_PACKET) {
			debug2("channel %d: packet too big %u > %u",
			    c->self, CHANNEL_MUX_MAX_PACKET, need);
			chan_rcvd_oclose(c);
			return;
		}
		if (read_mux(c, need + 4) < need + 4) /* read body */
			return;
		if (c->mux_rcb(c) != 0) {
			debug("channel %d: mux_rcb failed", c->self);
			chan_mark_dead(c);
			return;
		}
	}

	if (c->wfd != -1 && FD_ISSET(c->wfd, writeset) &&
	    buffer_len(&c->output) > 0) {
		len = write(c->wfd, buffer_ptr(&c->output),
		    buffer_len(&c->output));
		if (len < 0 && (errno == EINTR || errno == EAGAIN))
			return;
		if (len <= 0) {
			chan_mark_dead(c);
			return;
		}
		buffer_consume(&c->output, len);
	}
}

static void
channel_post_mux_listener(Channel *c, fd_set *readset, fd_set *writeset)
{
	Channel *nc;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int newsock;
	uid_t euid;
	gid_t egid;

	if (!FD_ISSET(c->sock, readset))
		return;

	debug("multiplexing control connection");

	/*
	 * Accept connection on control socket
	 */
	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);
	if ((newsock = accept(c->sock, (struct sockaddr*)&addr,
	    &addrlen)) == -1) {
		error("%s accept: %s", __func__, strerror(errno));
		if (errno == EMFILE || errno == ENFILE)
			c->notbefore = monotime() + 1;
		return;
	}

	if (getpeereid(newsock, &euid, &egid) < 0) {
		error("%s getpeereid failed: %s", __func__,
		    strerror(errno));
		close(newsock);
		return;
	}
	if ((euid != 0) && (getuid() != euid)) {
		error("multiplex uid mismatch: peer euid %u != uid %u",
		    (u_int)euid, (u_int)getuid());
		close(newsock);
		return;
	}
	nc = channel_new("multiplex client", SSH_CHANNEL_MUX_CLIENT,
	    newsock, newsock, -1, c->local_window_max,
	    c->local_maxpacket, 0, "mux-control", 1);
	nc->mux_rcb = c->mux_rcb;
	debug3("%s: new mux channel %d fd %d", __func__,
	    nc->self, nc->sock);
	/* establish state */
	nc->mux_rcb(nc);
	/* mux state transitions must not elicit protocol messages */
	nc->flags |= CHAN_LOCAL;
}

/* ARGSUSED */
static void
channel_post_output_drain_13(Channel *c, fd_set *readset, fd_set *writeset)
{
	int len;

	/* Send buffered output data to the socket. */
	if (FD_ISSET(c->sock, writeset) && buffer_len(&c->output) > 0) {
		len = write(c->sock, buffer_ptr(&c->output),
			    buffer_len(&c->output));
		if (len <= 0)
			buffer_clear(&c->output);
		else
			buffer_consume(&c->output, len);
	}
}

static void
channel_handler_init_20(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_RPORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_CONNECTING] =		&channel_pre_connecting;
	channel_pre[SSH_CHANNEL_DYNAMIC] =		&channel_pre_dynamic;
	channel_pre[SSH_CHANNEL_MUX_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_MUX_CLIENT] =		&channel_pre_mux_client;

	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_RPORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	channel_post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open;
	channel_post[SSH_CHANNEL_MUX_LISTENER] =	&channel_post_mux_listener;
	channel_post[SSH_CHANNEL_MUX_CLIENT] =		&channel_post_mux_client;
}

static void
channel_handler_init_13(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open_13;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open_13;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_INPUT_DRAINING] =	&channel_pre_input_draining;
	channel_pre[SSH_CHANNEL_OUTPUT_DRAINING] =	&channel_pre_output_draining;
	channel_pre[SSH_CHANNEL_CONNECTING] =		&channel_pre_connecting;
	channel_pre[SSH_CHANNEL_DYNAMIC] =		&channel_pre_dynamic;

	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open;
	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_OUTPUT_DRAINING] =	&channel_post_output_drain_13;
	channel_post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	channel_post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open;
}

static void
channel_handler_init_15(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_CONNECTING] =		&channel_pre_connecting;
	channel_pre[SSH_CHANNEL_DYNAMIC] =		&channel_pre_dynamic;

	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open;
	channel_post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	channel_post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open;
}

static void
channel_handler_init(void)
{
	int i;

	for (i = 0; i < SSH_CHANNEL_MAX_TYPE; i++) {
		channel_pre[i] = NULL;
		channel_post[i] = NULL;
	}
	if (compat20)
		channel_handler_init_20();
	else if (compat13)
		channel_handler_init_13();
	else
		channel_handler_init_15();
}

/* gc dead channels */
static void
channel_garbage_collect(Channel *c)
{
	if (c == NULL)
		return;
	if (c->detach_user != NULL) {
		if (!chan_is_dead(c, c->detach_close))
			return;
		debug2("channel %d: gc: notify user", c->self);
		c->detach_user(c->self, NULL);
		/* if we still have a callback */
		if (c->detach_user != NULL)
			return;
		debug2("channel %d: gc: user detached", c->self);
	}
	if (!chan_is_dead(c, 1))
		return;
	debug2("channel %d: garbage collecting", c->self);
	channel_free(c);
}

static void
channel_handler(chan_fn *ftab[], fd_set *readset, fd_set *writeset,
    time_t *unpause_secs)
{
	static int did_init = 0;
	u_int i, oalloc;
	Channel *c;
	time_t now;

	if (!did_init) {
		channel_handler_init();
		did_init = 1;
	}
	now = monotime();
	if (unpause_secs != NULL)
		*unpause_secs = 0;
	for (i = 0, oalloc = channels_alloc; i < oalloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;
		if (c->delayed) {
			if (ftab == channel_pre)
				c->delayed = 0;
			else
				continue;
		}
		if (ftab[c->type] != NULL) {
			/*
			 * Run handlers that are not paused.
			 */
			if (c->notbefore <= now)
				(*ftab[c->type])(c, readset, writeset);
			else if (unpause_secs != NULL) {
				/*
				 * Collect the time that the earliest
				 * channel comes off pause.
				 */
				debug3("%s: chan %d: skip for %d more seconds",
				    __func__, c->self,
				    (int)(c->notbefore - now));
				if (*unpause_secs == 0 ||
				    (c->notbefore - now) < *unpause_secs)
					*unpause_secs = c->notbefore - now;
			}
		}
		channel_garbage_collect(c);
	}
	if (unpause_secs != NULL && *unpause_secs != 0)
		debug3("%s: first channel unpauses in %d seconds",
		    __func__, (int)*unpause_secs);
}

/*
 * Allocate/update select bitmasks and add any bits relevant to channels in
 * select bitmasks.
 */
void
channel_prepare_select(fd_set **readsetp, fd_set **writesetp, int *maxfdp,
    u_int *nallocp, time_t *minwait_secs, int rekeying)
{
	u_int n, sz, nfdset;

	n = MAX(*maxfdp, channel_max_fd);

	nfdset = howmany(n+1, NFDBITS);
	/* Explicitly test here, because xrealloc isn't always called */
	if (nfdset && SIZE_T_MAX / nfdset < sizeof(fd_mask))
		fatal("channel_prepare_select: max_fd (%d) is too large", n);
	sz = nfdset * sizeof(fd_mask);

	/* perhaps check sz < nalloc/2 and shrink? */
	if (*readsetp == NULL || sz > *nallocp) {
		*readsetp = xrealloc(*readsetp, nfdset, sizeof(fd_mask));
		*writesetp = xrealloc(*writesetp, nfdset, sizeof(fd_mask));
		*nallocp = sz;
	}
	*maxfdp = n;
	memset(*readsetp, 0, sz);
	memset(*writesetp, 0, sz);

	if (!rekeying)
		channel_handler(channel_pre, *readsetp, *writesetp,
		    minwait_secs);
}

/*
 * After select, perform any appropriate operations for channels which have
 * events pending.
 */
void
channel_after_select(fd_set *readset, fd_set *writeset)
{
	channel_handler(channel_post, readset, writeset, NULL);
}


/* If there is data to send to the connection, enqueue some of it now. */
void
channel_output_poll(void)
{
	Channel *c;
	u_int i, len;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;

		/*
		 * We are only interested in channels that can have buffered
		 * incoming data.
		 */
		if (compat13) {
			if (c->type != SSH_CHANNEL_OPEN &&
			    c->type != SSH_CHANNEL_INPUT_DRAINING)
				continue;
		} else {
			if (c->type != SSH_CHANNEL_OPEN)
				continue;
		}
		if (compat20 &&
		    (c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD))) {
			/* XXX is this true? */
			debug3("channel %d: will not send data after close", c->self);
			continue;
		}

		/* Get the amount of buffered data for this channel. */
		if ((c->istate == CHAN_INPUT_OPEN ||
		    c->istate == CHAN_INPUT_WAIT_DRAIN) &&
		    (len = buffer_len(&c->input)) > 0) {
			if (c->datagram) {
				if (len > 0) {
					u_char *data;
					u_int dlen;

					data = buffer_get_string(&c->input,
					    &dlen);
					if (dlen > c->remote_window ||
					    dlen > c->remote_maxpacket) {
						debug("channel %d: datagram "
						    "too big for channel",
						    c->self);
						free(data);
						continue;
					}
					packet_start(SSH2_MSG_CHANNEL_DATA);
					packet_put_int(c->remote_id);
					packet_put_string(data, dlen);
					packet_send();
					c->remote_window -= dlen + 4;
					free(data);
				}
				continue;
			}
			/*
			 * Send some data for the other side over the secure
			 * connection.
			 */
			if (compat20) {
				if (len > c->remote_window)
					len = c->remote_window;
				if (len > c->remote_maxpacket)
					len = c->remote_maxpacket;
			} else {
				if (packet_is_interactive()) {
					if (len > 1024)
						len = 512;
				} else {
					/* Keep the packets at reasonable size. */
					if (len > packet_get_maxsize()/2)
						len = packet_get_maxsize()/2;
				}
			}
			if (len > 0) {
				packet_start(compat20 ?
				    SSH2_MSG_CHANNEL_DATA : SSH_MSG_CHANNEL_DATA);
				packet_put_int(c->remote_id);
				packet_put_string(buffer_ptr(&c->input), len);
				packet_send();
				buffer_consume(&c->input, len);
				c->remote_window -= len;
			}
		} else if (c->istate == CHAN_INPUT_WAIT_DRAIN) {
			if (compat13)
				fatal("cannot happen: istate == INPUT_WAIT_DRAIN for proto 1.3");
			/*
			 * input-buffer is empty and read-socket shutdown:
			 * tell peer, that we will not send more data: send IEOF.
			 * hack for extended data: delay EOF if EFD still in use.
			 */
			if (CHANNEL_EFD_INPUT_ACTIVE(c))
				debug2("channel %d: ibuf_empty delayed efd %d/(%d)",
				    c->self, c->efd, buffer_len(&c->extended));
			else
				chan_ibuf_empty(c);
		}
		/* Send extended data, i.e. stderr */
		if (compat20 &&
		    !(c->flags & CHAN_EOF_SENT) &&
		    c->remote_window > 0 &&
		    (len = buffer_len(&c->extended)) > 0 &&
		    c->extended_usage == CHAN_EXTENDED_READ) {
			debug2("channel %d: rwin %u elen %u euse %d",
			    c->self, c->remote_window, buffer_len(&c->extended),
			    c->extended_usage);
			if (len > c->remote_window)
				len = c->remote_window;
			if (len > c->remote_maxpacket)
				len = c->remote_maxpacket;
			packet_start(SSH2_MSG_CHANNEL_EXTENDED_DATA);
			packet_put_int(c->remote_id);
			packet_put_int(SSH2_EXTENDED_DATA_STDERR);
			packet_put_string(buffer_ptr(&c->extended), len);
			packet_send();
			buffer_consume(&c->extended, len);
			c->remote_window -= len;
			debug2("channel %d: sent ext data %d", c->self, len);
		}
	}
}


/* -- protocol input */

/* ARGSUSED */
void
channel_input_data(int type, u_int32_t seq, void *ctxt)
{
	int id;
	char *data;
	u_int data_len, win_len;
	Channel *c;

	/* Get the channel number and verify it. */
	id = packet_get_int();
	c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("Received data for nonexistent channel %d.", id);

	/* Ignore any data for non-open channels (might happen on close) */
	if (c->type != SSH_CHANNEL_OPEN &&
	    c->type != SSH_CHANNEL_X11_OPEN)
		return;

	/* Get the data. */
	data = packet_get_string_ptr(&data_len);
	win_len = data_len;
	if (c->datagram)
		win_len += 4;  /* string length header */

	/*
	 * Ignore data for protocol > 1.3 if output end is no longer open.
	 * For protocol 2 the sending side is reducing its window as it sends
	 * data, so we must 'fake' consumption of the data in order to ensure
	 * that window updates are sent back.  Otherwise the connection might
	 * deadlock.
	 */
	if (!compat13 && c->ostate != CHAN_OUTPUT_OPEN) {
		if (compat20) {
			c->local_window -= win_len;
			c->local_consumed += win_len;
		}
		return;
	}

	if (compat20) {
		if (win_len > c->local_maxpacket) {
			logit("channel %d: rcvd big packet %d, maxpack %d",
			    c->self, win_len, c->local_maxpacket);
		}
		if (win_len > c->local_window) {
			logit("channel %d: rcvd too much data %d, win %d",
			    c->self, win_len, c->local_window);
			return;
		}
		c->local_window -= win_len;
	}
	if (c->datagram)
		buffer_put_string(&c->output, data, data_len);
	else
		buffer_append(&c->output, data, data_len);
	packet_check_eom();
}

/* ARGSUSED */
void
channel_input_extended_data(int type, u_int32_t seq, void *ctxt)
{
	int id;
	char *data;
	u_int data_len, tcode;
	Channel *c;

	/* Get the channel number and verify it. */
	id = packet_get_int();
	c = channel_lookup(id);

	if (c == NULL)
		packet_disconnect("Received extended_data for bad channel %d.", id);
	if (c->type != SSH_CHANNEL_OPEN) {
		logit("channel %d: ext data for non open", id);
		return;
	}
	if (c->flags & CHAN_EOF_RCVD) {
		if (datafellows & SSH_BUG_EXTEOF)
			debug("channel %d: accepting ext data after eof", id);
		else
			packet_disconnect("Received extended_data after EOF "
			    "on channel %d.", id);
	}
	tcode = packet_get_int();
	if (c->efd == -1 ||
	    c->extended_usage != CHAN_EXTENDED_WRITE ||
	    tcode != SSH2_EXTENDED_DATA_STDERR) {
		logit("channel %d: bad ext data", c->self);
		return;
	}
	data = packet_get_string(&data_len);
	packet_check_eom();
	if (data_len > c->local_window) {
		logit("channel %d: rcvd too much extended_data %d, win %d",
		    c->self, data_len, c->local_window);
		free(data);
		return;
	}
	debug2("channel %d: rcvd ext data %d", c->self, data_len);
	c->local_window -= data_len;
	buffer_append(&c->extended, data, data_len);
	free(data);
}

/* ARGSUSED */
void
channel_input_ieof(int type, u_int32_t seq, void *ctxt)
{
	int id;
	Channel *c;

	id = packet_get_int();
	packet_check_eom();
	c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("Received ieof for nonexistent channel %d.", id);
	chan_rcvd_ieof(c);

	/* XXX force input close */
	if (c->force_drain && c->istate == CHAN_INPUT_OPEN) {
		debug("channel %d: FORCE input drain", c->self);
		c->istate = CHAN_INPUT_WAIT_DRAIN;
		if (buffer_len(&c->input) == 0)
			chan_ibuf_empty(c);
	}

}

/* ARGSUSED */
void
channel_input_close(int type, u_int32_t seq, void *ctxt)
{
	int id;
	Channel *c;

	id = packet_get_int();
	packet_check_eom();
	c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("Received close for nonexistent channel %d.", id);

	/*
	 * Send a confirmation that we have closed the channel and no more
	 * data is coming for it.
	 */
	packet_start(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION);
	packet_put_int(c->remote_id);
	packet_send();

	/*
	 * If the channel is in closed state, we have sent a close request,
	 * and the other side will eventually respond with a confirmation.
	 * Thus, we cannot free the channel here, because then there would be
	 * no-one to receive the confirmation.  The channel gets freed when
	 * the confirmation arrives.
	 */
	if (c->type != SSH_CHANNEL_CLOSED) {
		/*
		 * Not a closed channel - mark it as draining, which will
		 * cause it to be freed later.
		 */
		buffer_clear(&c->input);
		c->type = SSH_CHANNEL_OUTPUT_DRAINING;
	}
}

/* proto version 1.5 overloads CLOSE_CONFIRMATION with OCLOSE */
/* ARGSUSED */
void
channel_input_oclose(int type, u_int32_t seq, void *ctxt)
{
	int id = packet_get_int();
	Channel *c = channel_lookup(id);

	packet_check_eom();
	if (c == NULL)
		packet_disconnect("Received oclose for nonexistent channel %d.", id);
	chan_rcvd_oclose(c);
}

/* ARGSUSED */
void
channel_input_close_confirmation(int type, u_int32_t seq, void *ctxt)
{
	int id = packet_get_int();
	Channel *c = channel_lookup(id);

	packet_check_eom();
	if (c == NULL)
		packet_disconnect("Received close confirmation for "
		    "out-of-range channel %d.", id);
	if (c->type != SSH_CHANNEL_CLOSED && c->type != SSH_CHANNEL_ABANDONED)
		packet_disconnect("Received close confirmation for "
		    "non-closed channel %d (type %d).", id, c->type);
	channel_free(c);
}

/* ARGSUSED */
void
channel_input_open_confirmation(int type, u_int32_t seq, void *ctxt)
{
	int id, remote_id;
	Channel *c;

	id = packet_get_int();
	c = channel_lookup(id);

	if (c==NULL || c->type != SSH_CHANNEL_OPENING)
		packet_disconnect("Received open confirmation for "
		    "non-opening channel %d.", id);
	remote_id = packet_get_int();
	/* Record the remote channel number and mark that the channel is now open. */
	c->remote_id = remote_id;
	c->type = SSH_CHANNEL_OPEN;

	if (compat20) {
		c->remote_window = packet_get_int();
		c->remote_maxpacket = packet_get_int();
		if (c->open_confirm) {
			debug2("callback start");
			c->open_confirm(c->self, 1, c->open_confirm_ctx);
			debug2("callback done");
		}
		debug2("channel %d: open confirm rwindow %u rmax %u", c->self,
		    c->remote_window, c->remote_maxpacket);
	}
	packet_check_eom();
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

/* ARGSUSED */
void
channel_input_open_failure(int type, u_int32_t seq, void *ctxt)
{
	int id, reason;
	char *msg = NULL, *lang = NULL;
	Channel *c;

	id = packet_get_int();
	c = channel_lookup(id);

	if (c==NULL || c->type != SSH_CHANNEL_OPENING)
		packet_disconnect("Received open failure for "
		    "non-opening channel %d.", id);
	if (compat20) {
		reason = packet_get_int();
		if (!(datafellows & SSH_BUG_OPENFAILURE)) {
			msg  = packet_get_string(NULL);
			lang = packet_get_string(NULL);
		}
		logit("channel %d: open failed: %s%s%s", id,
		    reason2txt(reason), msg ? ": ": "", msg ? msg : "");
		free(msg);
		free(lang);
		if (c->open_confirm) {
			debug2("callback start");
			c->open_confirm(c->self, 0, c->open_confirm_ctx);
			debug2("callback done");
		}
	}
	packet_check_eom();
	/* Schedule the channel for cleanup/deletion. */
	chan_mark_dead(c);
}

/* ARGSUSED */
void
channel_input_window_adjust(int type, u_int32_t seq, void *ctxt)
{
	Channel *c;
	int id;
	u_int adjust;

	if (!compat20)
		return;

	/* Get the channel number and verify it. */
	id = packet_get_int();
	c = channel_lookup(id);

	if (c == NULL) {
		logit("Received window adjust for non-open channel %d.", id);
		return;
	}
	adjust = packet_get_int();
	packet_check_eom();
	debug2("channel %d: rcvd adjust %u", id, adjust);
	c->remote_window += adjust;
}

/* ARGSUSED */
void
channel_input_port_open(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	u_short host_port;
	char *host, *originator_string;
	int remote_id;

	remote_id = packet_get_int();
	host = packet_get_string(NULL);
	host_port = packet_get_int();

	if (packet_get_protocol_flags() & SSH_PROTOFLAG_HOST_IN_FWD_OPEN) {
		originator_string = packet_get_string(NULL);
	} else {
		originator_string = xstrdup("unknown (remote did not supply name)");
	}
	packet_check_eom();
	c = channel_connect_to(host, host_port,
	    "connected socket", originator_string);
	free(originator_string);
	free(host);
	if (c == NULL) {
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
		packet_send();
	} else
		c->remote_id = remote_id;
}

/* ARGSUSED */
void
channel_input_status_confirm(int type, u_int32_t seq, void *ctxt)
{
	Channel *c;
	struct channel_confirm *cc;
	int id;

	/* Reset keepalive timeout */
	packet_set_alive_timeouts(0);

	id = packet_get_int();
	packet_check_eom();

	debug2("channel_input_status_confirm: type %d id %d", type, id);

	if ((c = channel_lookup(id)) == NULL) {
		logit("channel_input_status_confirm: %d: unknown", id);
		return;
	}	
	;
	if ((cc = TAILQ_FIRST(&c->status_confirms)) == NULL)
		return;
	cc->cb(type, c, cc->ctx);
	TAILQ_REMOVE(&c->status_confirms, cc, entry);
	bzero(cc, sizeof(*cc));
	free(cc);
}

/* -- tcp forwarding */

void
channel_set_af(int af)
{
	IPv4or6 = af;
}

void
channel_set_hpn(int disabled, u_int buf_size)
{
	hpn_disabled = disabled;
	buffer_size = buf_size;
	debug("HPN Disabled: %d, HPN Buffer Size: %d",
	    hpn_disabled, buffer_size);
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
 */
static const char *
channel_fwd_bind_addr(const char *listen_addr, int *wildcardp,
    int is_client, int gateway_ports)
{
	const char *addr = NULL;
	int wildcard = 0;

	if (listen_addr == NULL) {
		/* No address specified: default to gateway_ports setting */
		if (gateway_ports)
			wildcard = 1;
	} else if (gateway_ports || is_client) {
		if (((datafellows & SSH_OLD_FORWARD_ADDR) &&
		    strcmp(listen_addr, "0.0.0.0") == 0 && is_client == 0) ||
		    *listen_addr == '\0' || strcmp(listen_addr, "*") == 0 ||
		    (!is_client && gateway_ports == 1))
			wildcard = 1;
		else if (strcmp(listen_addr, "localhost") != 0)
			addr = listen_addr;
	}
	if (wildcardp != NULL)
		*wildcardp = wildcard;
	return addr;
}

static int
channel_setup_fwd_listener(int type, const char *listen_addr,
    u_short listen_port, int *allocated_listen_port,
    const char *host_to_connect, u_short port_to_connect, int gateway_ports)
{
	Channel *c;
	int sock, r, success = 0, wildcard = 0, is_client;
	struct addrinfo hints, *ai, *aitop;
	const char *host, *addr;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	in_port_t *lport_p;

	host = (type == SSH_CHANNEL_RPORT_LISTENER) ?
	    listen_addr : host_to_connect;
	is_client = (type == SSH_CHANNEL_PORT_LISTENER);

	if (host == NULL) {
		error("No forward host name.");
		return 0;
	}
	if (strlen(host) >= NI_MAXHOST) {
		error("Forward host name too long.");
		return 0;
	}

	/* Determine the bind address, cf. channel_fwd_bind_addr() comment */
	addr = channel_fwd_bind_addr(listen_addr, &wildcard,
	    is_client, gateway_ports);
	debug3("channel_setup_fwd_listener: type %d wildcard %d addr %s",
	    type, wildcard, (addr == NULL) ? "NULL" : addr);

	/*
	 * getaddrinfo returns a loopback address if the hostname is
	 * set to NULL and hints.ai_flags is not AI_PASSIVE
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_flags = wildcard ? AI_PASSIVE : 0;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", listen_port);
	if ((r = getaddrinfo(addr, strport, &hints, &aitop)) != 0) {
		if (addr == NULL) {
			/* This really shouldn't happen */
			packet_disconnect("getaddrinfo: fatal error: %s",
			    ssh_gai_strerror(r));
		} else {
			error("channel_setup_fwd_listener: "
			    "getaddrinfo(%.64s): %s", addr,
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
		if (type == SSH_CHANNEL_RPORT_LISTENER && listen_port == 0 &&
		    allocated_listen_port != NULL && *allocated_listen_port > 0)
			*lport_p = htons(*allocated_listen_port);

		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("channel_setup_fwd_listener: getnameinfo failed");
			continue;
		}
		/* Create a port to listen for the host. */
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) {
			/* this is no error since kernel may not support ipv6 */
			verbose("socket: %.100s", strerror(errno));
			continue;
		}

		channel_set_reuseaddr(sock);
		if (ai->ai_family == AF_INET6)
			sock_set_v6only(sock);

		debug("Local forwarding listening on %s port %s.",
		    ntop, strport);

		/* Bind the socket to the address. */
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			/* address can be in use ipv6 address is already bound */
			if (!ai->ai_next)
				error("bind: %.100s", strerror(errno));
			else
				verbose("bind: %.100s", strerror(errno));

			close(sock);
			continue;
		}
		/* Start listening for connections on the socket. */
		if (listen(sock, SSH_LISTEN_BACKLOG) < 0) {
			error("listen: %.100s", strerror(errno));
			close(sock);
			continue;
		}

		/*
		 * listen_port == 0 requests a dynamically allocated port -
		 * record what we got.
		 */
		if (type == SSH_CHANNEL_RPORT_LISTENER && listen_port == 0 &&
		    allocated_listen_port != NULL &&
		    *allocated_listen_port == 0) {
			*allocated_listen_port = get_sock_port(sock, 1);
			debug("Allocated listen port %d",
			    *allocated_listen_port);
		}

		/*
		 * Allocate a channel number for the socket.  Explicitly test
		 * for hpn disabled option.  If true use smaller window size.
		 */
		if (hpn_disabled)
			c = channel_new("port listener", type, sock, sock, -1,
			    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
			    0, "port listener", 1);
		else
			c = channel_new("port listener", type, sock, sock, -1,
			    buffer_size, CHAN_TCP_PACKET_DEFAULT,
			    0, "port listener", 1);
		c->path = xstrdup(host);
		c->host_port = port_to_connect;
		c->listening_addr = addr == NULL ? NULL : xstrdup(addr);
		if (listen_port == 0 && allocated_listen_port != NULL &&
		    !(datafellows & SSH_BUG_DYNAMIC_RPORT))
			c->listening_port = *allocated_listen_port;
		else
			c->listening_port = listen_port;
		success = 1;
	}
	if (success == 0)
		error("channel_setup_fwd_listener: cannot listen to port: %d",
		    listen_port);
	freeaddrinfo(aitop);
	return success;
}

int
channel_cancel_rport_listener(const char *host, u_short port)
{
	u_int i;
	int found = 0;

	for (i = 0; i < channels_alloc; i++) {
		Channel *c = channels[i];
		if (c == NULL || c->type != SSH_CHANNEL_RPORT_LISTENER)
			continue;
		if (strcmp(c->path, host) == 0 && c->listening_port == port) {
			debug2("%s: close channel %d", __func__, i);
			channel_free(c);
			found = 1;
		}
	}

	return (found);
}

int
channel_cancel_lport_listener(const char *lhost, u_short lport,
    int cport, int gateway_ports)
{
	u_int i;
	int found = 0;
	const char *addr = channel_fwd_bind_addr(lhost, NULL, 1, gateway_ports);

	for (i = 0; i < channels_alloc; i++) {
		Channel *c = channels[i];
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
			debug2("%s: close channel %d", __func__, i);
			channel_free(c);
			found = 1;
		}
	}

	return (found);
}

/* protocol local port fwd, used by ssh (and sshd in v1) */
int
channel_setup_local_fwd_listener(const char *listen_host, u_short listen_port,
    const char *host_to_connect, u_short port_to_connect, int gateway_ports)
{
	return channel_setup_fwd_listener(SSH_CHANNEL_PORT_LISTENER,
	    listen_host, listen_port, NULL, host_to_connect, port_to_connect,
	    gateway_ports);
}

/* protocol v2 remote port fwd, used by sshd */
int
channel_setup_remote_fwd_listener(const char *listen_address,
    u_short listen_port, int *allocated_listen_port, int gateway_ports)
{
	return channel_setup_fwd_listener(SSH_CHANNEL_RPORT_LISTENER,
	    listen_address, listen_port, allocated_listen_port,
	    NULL, 0, gateway_ports);
}

/*
 * Translate the requested rfwd listen host to something usable for
 * this server.
 */
static const char *
channel_rfwd_bind_host(const char *listen_host)
{
	if (listen_host == NULL) {
		if (datafellows & SSH_BUG_RFWD_ADDR)
			return "127.0.0.1";
		else
			return "localhost";
	} else if (*listen_host == '\0' || strcmp(listen_host, "*") == 0) {
		if (datafellows & SSH_BUG_RFWD_ADDR)
			return "0.0.0.0";
		else
			return "";
	} else
		return listen_host;
}

/*
 * Initiate forwarding of connections to port "port" on remote host through
 * the secure channel to host:port from local side.
 * Returns handle (index) for updating the dynamic listen port with
 * channel_update_permitted_opens().
 */
int
channel_request_remote_forwarding(const char *listen_host, u_short listen_port,
    const char *host_to_connect, u_short port_to_connect)
{
	int type, success = 0, idx = -1;

	/* Send the forward request to the remote side. */
	if (compat20) {
		packet_start(SSH2_MSG_GLOBAL_REQUEST);
		packet_put_cstring("tcpip-forward");
		packet_put_char(1);		/* boolean: want reply */
		packet_put_cstring(channel_rfwd_bind_host(listen_host));
		packet_put_int(listen_port);
		packet_send();
		packet_write_wait();
		/* Assume that server accepts the request */
		success = 1;
	} else {
		packet_start(SSH_CMSG_PORT_FORWARD_REQUEST);
		packet_put_int(listen_port);
		packet_put_cstring(host_to_connect);
		packet_put_int(port_to_connect);
		packet_send();
		packet_write_wait();

		/* Wait for response from the remote side. */
		type = packet_read();
		switch (type) {
		case SSH_SMSG_SUCCESS:
			success = 1;
			break;
		case SSH_SMSG_FAILURE:
			break;
		default:
			/* Unknown packet */
			packet_disconnect("Protocol error for port forward request:"
			    "received packet type %d.", type);
		}
	}
	if (success) {
		/* Record that connection to this host/port is permitted. */
		permitted_opens = xrealloc(permitted_opens,
		    num_permitted_opens + 1, sizeof(*permitted_opens));
		idx = num_permitted_opens++;
		permitted_opens[idx].host_to_connect = xstrdup(host_to_connect);
		permitted_opens[idx].port_to_connect = port_to_connect;
		permitted_opens[idx].listen_port = listen_port;
	}
	return (idx);
}

/*
 * Request cancellation of remote forwarding of connection host:port from
 * local side.
 */
int
channel_request_rforward_cancel(const char *host, u_short port)
{
	int i;

	if (!compat20)
		return -1;

	for (i = 0; i < num_permitted_opens; i++) {
		if (permitted_opens[i].host_to_connect != NULL &&
		    permitted_opens[i].listen_port == port)
			break;
	}
	if (i >= num_permitted_opens) {
		debug("%s: requested forward not found", __func__);
		return -1;
	}
	packet_start(SSH2_MSG_GLOBAL_REQUEST);
	packet_put_cstring("cancel-tcpip-forward");
	packet_put_char(0);
	packet_put_cstring(channel_rfwd_bind_host(host));
	packet_put_int(port);
	packet_send();

	permitted_opens[i].listen_port = 0;
	permitted_opens[i].port_to_connect = 0;
	free(permitted_opens[i].host_to_connect);
	permitted_opens[i].host_to_connect = NULL;

	return 0;
}

/*
 * This is called after receiving CHANNEL_FORWARDING_REQUEST.  This initates
 * listening for the port, and sends back a success reply (or disconnect
 * message if there was an error).
 */
int
channel_input_port_forward_request(int is_root, int gateway_ports)
{
	u_short port, host_port;
	int success = 0;
	char *hostname;

	/* Get arguments from the packet. */
	port = packet_get_int();
	hostname = packet_get_string(NULL);
	host_port = packet_get_int();

#ifndef HAVE_CYGWIN
	/*
	 * Check that an unprivileged user is not trying to forward a
	 * privileged port.
	 */
	if (port < IPPORT_RESERVED && !is_root)
		packet_disconnect(
		    "Requested forwarding of port %d but user is not root.",
		    port);
	if (host_port == 0)
		packet_disconnect("Dynamic forwarding denied.");
#endif

	/* Initiate forwarding */
	success = channel_setup_local_fwd_listener(NULL, port, hostname,
	    host_port, gateway_ports);

	/* Free the argument string. */
	free(hostname);

	return (success ? 0 : -1);
}

/*
 * Permits opening to any host/port if permitted_opens[] is empty.  This is
 * usually called by the server, because the user could connect to any port
 * anyway, and the server has no way to know but to trust the client anyway.
 */
void
channel_permit_all_opens(void)
{
	if (num_permitted_opens == 0)
		all_opens_permitted = 1;
}

void
channel_add_permitted_opens(char *host, int port)
{
	debug("allow port forwarding to host %s port %d", host, port);

	permitted_opens = xrealloc(permitted_opens,
	    num_permitted_opens + 1, sizeof(*permitted_opens));
	permitted_opens[num_permitted_opens].host_to_connect = xstrdup(host);
	permitted_opens[num_permitted_opens].port_to_connect = port;
	num_permitted_opens++;

	all_opens_permitted = 0;
}

/*
 * Update the listen port for a dynamic remote forward, after
 * the actual 'newport' has been allocated. If 'newport' < 0 is
 * passed then they entry will be invalidated.
 */
void
channel_update_permitted_opens(int idx, int newport)
{
	if (idx < 0 || idx >= num_permitted_opens) {
		debug("channel_update_permitted_opens: index out of range:"
		    " %d num_permitted_opens %d", idx, num_permitted_opens);
		return;
	}
	debug("%s allowed port %d for forwarding to host %s port %d",
	    newport > 0 ? "Updating" : "Removing",
	    newport,
	    permitted_opens[idx].host_to_connect,
	    permitted_opens[idx].port_to_connect);
	if (newport >= 0)  {
		permitted_opens[idx].listen_port = 
		    (datafellows & SSH_BUG_DYNAMIC_RPORT) ? 0 : newport;
	} else {
		permitted_opens[idx].listen_port = 0;
		permitted_opens[idx].port_to_connect = 0;
		free(permitted_opens[idx].host_to_connect);
		permitted_opens[idx].host_to_connect = NULL;
	}
}

int
channel_add_adm_permitted_opens(char *host, int port)
{
	debug("config allows port forwarding to host %s port %d", host, port);

	permitted_adm_opens = xrealloc(permitted_adm_opens,
	    num_adm_permitted_opens + 1, sizeof(*permitted_adm_opens));
	permitted_adm_opens[num_adm_permitted_opens].host_to_connect
	     = xstrdup(host);
	permitted_adm_opens[num_adm_permitted_opens].port_to_connect = port;
	return ++num_adm_permitted_opens;
}

void
channel_disable_adm_local_opens(void)
{
	channel_clear_adm_permitted_opens();
	permitted_adm_opens = xmalloc(sizeof(*permitted_adm_opens));
	permitted_adm_opens[num_adm_permitted_opens].host_to_connect = NULL;
	num_adm_permitted_opens = 1;
}

void
channel_clear_permitted_opens(void)
{
	int i;

	for (i = 0; i < num_permitted_opens; i++)
		free(permitted_opens[i].host_to_connect);
	free(permitted_opens);
	permitted_opens = NULL;
	num_permitted_opens = 0;
}

void
channel_clear_adm_permitted_opens(void)
{
	int i;

	for (i = 0; i < num_adm_permitted_opens; i++)
		free(permitted_adm_opens[i].host_to_connect);
	free(permitted_adm_opens);
	permitted_adm_opens = NULL;
	num_adm_permitted_opens = 0;
}

void
channel_print_adm_permitted_opens(void)
{
	int i;

	printf("permitopen");
	if (num_adm_permitted_opens == 0) {
		printf(" any\n");
		return;
	}
	for (i = 0; i < num_adm_permitted_opens; i++)
		if (permitted_adm_opens[i].host_to_connect == NULL)
			printf(" none");
		else
			printf(" %s:%d", permitted_adm_opens[i].host_to_connect,
			    permitted_adm_opens[i].port_to_connect);
	printf("\n");
}

/* returns port number, FWD_PERMIT_ANY_PORT or -1 on error */
int
permitopen_port(const char *p)
{
	int port;

	if (strcmp(p, "*") == 0)
		return FWD_PERMIT_ANY_PORT;
	if ((port = a2port(p)) > 0)
		return port;
	return -1;
}

static int
port_match(u_short allowedport, u_short requestedport)
{
	if (allowedport == FWD_PERMIT_ANY_PORT ||
	    allowedport == requestedport)
		return 1;
	return 0;
}

/* Try to start non-blocking connect to next host in cctx list */
static int
connect_next(struct channel_connect *cctx)
{
	int sock, saved_errno;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];

	for (; cctx->ai; cctx->ai = cctx->ai->ai_next) {
		if (cctx->ai->ai_family != AF_INET &&
		    cctx->ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(cctx->ai->ai_addr, cctx->ai->ai_addrlen,
		    ntop, sizeof(ntop), strport, sizeof(strport),
		    NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("connect_next: getnameinfo failed");
			continue;
		}
		if ((sock = socket(cctx->ai->ai_family, cctx->ai->ai_socktype,
		    cctx->ai->ai_protocol)) == -1) {
			if (cctx->ai->ai_next == NULL)
				error("socket: %.100s", strerror(errno));
			else
				verbose("socket: %.100s", strerror(errno));
			continue;
		}
		if (set_nonblock(sock) == -1)
			fatal("%s: set_nonblock(%d)", __func__, sock);
		if (connect(sock, cctx->ai->ai_addr,
		    cctx->ai->ai_addrlen) == -1 && errno != EINPROGRESS) {
			debug("connect_next: host %.100s ([%.100s]:%s): "
			    "%.100s", cctx->host, ntop, strport,
			    strerror(errno));
			saved_errno = errno;
			close(sock);
			errno = saved_errno;
			continue;	/* fail -- try next */
		}
		debug("connect_next: host %.100s ([%.100s]:%s) "
		    "in progress, fd=%d", cctx->host, ntop, strport, sock);
		cctx->ai = cctx->ai->ai_next;
		set_nodelay(sock);
		return sock;
	}
	return -1;
}

static void
channel_connect_ctx_free(struct channel_connect *cctx)
{
	free(cctx->host);
	if (cctx->aitop)
		freeaddrinfo(cctx->aitop);
	bzero(cctx, sizeof(*cctx));
	cctx->host = NULL;
	cctx->ai = cctx->aitop = NULL;
}

/* Return CONNECTING channel to remote host, port */
static Channel *
connect_to(const char *host, u_short port, char *ctype, char *rname)
{
	struct addrinfo hints;
	int gaierr;
	int sock = -1;
	char strport[NI_MAXSERV];
	struct channel_connect cctx;
	Channel *c;

	memset(&cctx, 0, sizeof(cctx));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", port);
	if ((gaierr = getaddrinfo(host, strport, &hints, &cctx.aitop)) != 0) {
		error("connect_to %.100s: unknown host (%s)", host,
		    ssh_gai_strerror(gaierr));
		return NULL;
	}

	cctx.host = xstrdup(host);
	cctx.port = port;
	cctx.ai = cctx.aitop;

	if ((sock = connect_next(&cctx)) == -1) {
		error("connect to %.100s port %d failed: %s",
		    host, port, strerror(errno));
		channel_connect_ctx_free(&cctx);
		return NULL;
	}
	c = channel_new(ctype, SSH_CHANNEL_CONNECTING, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0, rname, 1);
	c->connect_ctx = cctx;
	return c;
}

Channel *
channel_connect_by_listen_address(u_short listen_port, char *ctype, char *rname)
{
	int i;

	for (i = 0; i < num_permitted_opens; i++) {
		if (permitted_opens[i].host_to_connect != NULL &&
		    port_match(permitted_opens[i].listen_port, listen_port)) {
			return connect_to(
			    permitted_opens[i].host_to_connect,
			    permitted_opens[i].port_to_connect, ctype, rname);
		}
	}
	error("WARNING: Server requests forwarding for unknown listen_port %d",
	    listen_port);
	return NULL;
}

/* Check if connecting to that port is permitted and connect. */
Channel *
channel_connect_to(const char *host, u_short port, char *ctype, char *rname)
{
	int i, permit, permit_adm = 1;

	permit = all_opens_permitted;
	if (!permit) {
		for (i = 0; i < num_permitted_opens; i++)
			if (permitted_opens[i].host_to_connect != NULL &&
			    port_match(permitted_opens[i].port_to_connect, port) &&
			    strcmp(permitted_opens[i].host_to_connect, host) == 0)
				permit = 1;
	}

	if (num_adm_permitted_opens > 0) {
		permit_adm = 0;
		for (i = 0; i < num_adm_permitted_opens; i++)
			if (permitted_adm_opens[i].host_to_connect != NULL &&
			    port_match(permitted_adm_opens[i].port_to_connect, port) &&
			    strcmp(permitted_adm_opens[i].host_to_connect, host)
			    == 0)
				permit_adm = 1;
	}

	if (!permit || !permit_adm) {
		logit("Received request to connect to host %.100s port %d, "
		    "but the request was denied.", host, port);
		return NULL;
	}
	return connect_to(host, port, ctype, rname);
}

void
channel_send_window_changes(void)
{
	u_int i;
	struct winsize ws;

	for (i = 0; i < channels_alloc; i++) {
		if (channels[i] == NULL || !channels[i]->client_tty ||
		    channels[i]->type != SSH_CHANNEL_OPEN)
			continue;
		if (ioctl(channels[i]->rfd, TIOCGWINSZ, &ws) < 0)
			continue;
		channel_request_start(i, "window-change", 0);
		packet_put_int((u_int)ws.ws_col);
		packet_put_int((u_int)ws.ws_row);
		packet_put_int((u_int)ws.ws_xpixel);
		packet_put_int((u_int)ws.ws_ypixel);
		packet_send();
	}
}

/* -- X11 forwarding */

/*
 * Creates an internet domain socket for listening for X11 connections.
 * Returns 0 and a suitable display number for the DISPLAY variable
 * stored in display_numberp , or -1 if an error occurs.
 */
int
x11_create_display_inet(int x11_display_offset, int x11_use_localhost,
    int single_connection, u_int *display_numberp, int **chanids)
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
		hints.ai_family = IPv4or6;
		hints.ai_flags = x11_use_localhost ? 0: AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		snprintf(strport, sizeof strport, "%d", port);
		if ((gaierr = getaddrinfo(NULL, strport, &hints, &aitop)) != 0) {
			error("getaddrinfo: %.100s", ssh_gai_strerror(gaierr));
			return -1;
		}
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;
			sock = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol);
			if (sock < 0) {
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
				channel_set_reuseaddr(sock);
			if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
				debug2("bind port %d: %.100s", port, strerror(errno));
				close(sock);

				for (n = 0; n < num_socks; n++) {
					close(socks[n]);
				}
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
		if (listen(sock, SSH_LISTEN_BACKLOG) < 0) {
			error("listen: %.100s", strerror(errno));
			close(sock);
			return -1;
		}
	}

	/* Allocate a channel for each socket. */
	*chanids = xcalloc(num_socks + 1, sizeof(**chanids));
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		if (hpn_disabled)
			nc = channel_new("x11 listener",
			    SSH_CHANNEL_X11_LISTENER, sock, sock, -1,
			    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
			    0, "X11 inet listener", 1);
		else
			nc = channel_new("x11 listener",
			    SSH_CHANNEL_X11_LISTENER, sock, sock, -1,
			    buffer_size, CHAN_X11_PACKET_DEFAULT,
			    0, "X11 inet listener", 1);
		nc->single_connection = single_connection;
		(*chanids)[n] = nc->self;
	}
	(*chanids)[n] = -1;

	/* Return the display number for the DISPLAY environment variable. */
	*display_numberp = display_number;
	return (0);
}

static int
connect_local_xsocket_path(const char *pathname)
{
	int sock;
	struct sockaddr_un addr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		error("socket: %.100s", strerror(errno));
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

int
x11_connect_display(void)
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

	/* Check if the display is from launchd. */
#ifdef __APPLE__
	if (strncmp(display, "/tmp/launch", 11) == 0) {
		sock = connect_local_xsocket_path(display);
		if (sock < 0)
			return -1;

		/* OK, we now have a connection to the display. */
		return sock;
	}
#endif
	/*
	 * Check if it is a unix domain socket.  Unix domain displays are in
	 * one of the following formats: unix:d[.s], :d[.s], ::d[.s]
	 */
	if (strncmp(display, "unix:", 5) == 0 ||
	    display[0] == ':') {
		/* Connect to the unix domain socket. */
		if (sscanf(strrchr(display, ':') + 1, "%u", &display_number) != 1) {
			error("Could not parse display number from DISPLAY: %.100s",
			    display);
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
	/* buf now contains the host name.  But first we parse the display number. */
	if (sscanf(cp + 1, "%u", &display_number) != 1) {
		error("Could not parse display number from DISPLAY: %.100s",
		    display);
		return -1;
	}

	/* Look up the host address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
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
		if (sock < 0) {
			debug2("socket: %.100s", strerror(errno));
			continue;
		}
		/* Connect it to the display. */
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
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
		error("connect %.100s port %u: %.100s", buf, 6000 + display_number,
		    strerror(errno));
		return -1;
	}
	set_nodelay(sock);
	return sock;
}

/*
 * This is called when SSH_SMSG_X11_OPEN is received.  The packet contains
 * the remote channel number.  We should do whatever we want, and respond
 * with either SSH_MSG_OPEN_CONFIRMATION or SSH_MSG_OPEN_FAILURE.
 */

/* ARGSUSED */
void
x11_input_open(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	int remote_id, sock = 0;
	char *remote_host;

	debug("Received X11 open request.");

	remote_id = packet_get_int();

	if (packet_get_protocol_flags() & SSH_PROTOFLAG_HOST_IN_FWD_OPEN) {
		remote_host = packet_get_string(NULL);
	} else {
		remote_host = xstrdup("unknown (remote did not supply name)");
	}
	packet_check_eom();

	/* Obtain a connection to the real X display. */
	sock = x11_connect_display();
	if (sock != -1) {
		/* Allocate a channel for this connection. */
		c = channel_new("connected x11 socket",
		    SSH_CHANNEL_X11_OPEN, sock, sock, -1, 0, 0, 0,
		    remote_host, 1);
		c->remote_id = remote_id;
		c->force_drain = 1;
	}
	free(remote_host);
	if (c == NULL) {
		/* Send refusal to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
	} else {
		/* Send a confirmation to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(remote_id);
		packet_put_int(c->self);
	}
	packet_send();
}

/* dummy protocol handler that denies SSH-1 requests (agent/x11) */
/* ARGSUSED */
void
deny_input_open(int type, u_int32_t seq, void *ctxt)
{
	int rchan = packet_get_int();

	switch (type) {
	case SSH_SMSG_AGENT_OPEN:
		error("Warning: ssh server tried agent forwarding.");
		break;
	case SSH_SMSG_X11_OPEN:
		error("Warning: ssh server tried X11 forwarding.");
		break;
	default:
		error("deny_input_open: type %d", type);
		break;
	}
	error("Warning: this is probably a break-in attempt by a malicious server.");
	packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
	packet_put_int(rchan);
	packet_send();
}

/*
 * Requests forwarding of X11 connections, generates fake authentication
 * data, and enables authentication spoofing.
 * This should be called in the client only.
 */
void
x11_request_forwarding_with_spoofing(int client_session_id, const char *disp,
    const char *proto, const char *data, int want_reply)
{
	u_int data_len = (u_int) strlen(data) / 2;
	u_int i, value;
	char *new_data;
	int screen_number;
	const char *cp;
	u_int32_t rnd = 0;

	if (x11_saved_display == NULL)
		x11_saved_display = xstrdup(disp);
	else if (strcmp(disp, x11_saved_display) != 0) {
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

	if (x11_saved_proto == NULL) {
		/* Save protocol name. */
		x11_saved_proto = xstrdup(proto);
		/*
		 * Extract real authentication data and generate fake data
		 * of the same length.
		 */
		x11_saved_data = xmalloc(data_len);
		x11_fake_data = xmalloc(data_len);
		for (i = 0; i < data_len; i++) {
			if (sscanf(data + 2 * i, "%2x", &value) != 1)
				fatal("x11_request_forwarding: bad "
				    "authentication data: %.100s", data);
			if (i % 4 == 0)
				rnd = arc4random();
			x11_saved_data[i] = value;
			x11_fake_data[i] = rnd & 0xff;
			rnd >>= 8;
		}
		x11_saved_data_len = data_len;
		x11_fake_data_len = data_len;
	}

	/* Convert the fake data into hex. */
	new_data = tohex(x11_fake_data, data_len);

	/* Send the request packet. */
	if (compat20) {
		channel_request_start(client_session_id, "x11-req", want_reply);
		packet_put_char(0);	/* XXX bool single connection */
	} else {
		packet_start(SSH_CMSG_X11_REQUEST_FORWARDING);
	}
	packet_put_cstring(proto);
	packet_put_cstring(new_data);
	packet_put_int(screen_number);
	packet_send();
	packet_write_wait();
	free(new_data);
}


/* -- agent forwarding */

/* Sends a message to the server to request authentication fd forwarding. */

void
auth_request_forwarding(void)
{
	packet_start(SSH_CMSG_AGENT_REQUEST_FORWARDING);
	packet_send();
	packet_write_wait();
}
