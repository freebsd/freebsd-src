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
RCSID("$OpenBSD: channels.c,v 1.187 2003/03/05 22:33:43 markus Exp $");

#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "packet.h"
#include "xmalloc.h"
#include "log.h"
#include "misc.h"
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
static int channels_alloc = 0;

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

/* List of all permitted host/port pairs to connect. */
static ForwardPermission permitted_opens[SSH_MAX_FORWARDS_PER_DIRECTION];

/* Number of permitted host/port pairs in the array. */
static int num_permitted_opens = 0;
/*
 * If this is true, all opens are permitted.  This is the case on the server
 * on which we have to trust the client anyway, and the user could do
 * anything after logging in anyway.
 */
static int all_opens_permitted = 0;


/* -- X11 forwarding */

/* Maximum number of fake X11 displays to try. */
#define MAX_DISPLAYS  1000

/* Saved X11 authentication protocol name. */
static char *x11_saved_proto = NULL;

/* Saved X11 authentication data.  This is the real data. */
static char *x11_saved_data = NULL;
static u_int x11_saved_data_len = 0;

/*
 * Fake X11 authentication data.  This is what the server will be sending us;
 * we should replace any occurrences of this by the real data.
 */
static char *x11_fake_data = NULL;
static u_int x11_fake_data_len;


/* -- agent forwarding */

#define	NUM_SOCKS	10

/* AF_UNSPEC or AF_INET or AF_INET6 */
static int IPv4or6 = AF_UNSPEC;

/* helper */
static void port_open_helper(Channel *c, char *rtype);

/* -- channel core */

Channel *
channel_lookup(int id)
{
	Channel *c;

	if (id < 0 || id >= channels_alloc) {
		log("channel_lookup: %d: bad id", id);
		return NULL;
	}
	c = channels[id];
	if (c == NULL) {
		log("channel_lookup: %d: bad id: channel free", id);
		return NULL;
	}
	return c;
}

/*
 * Register filedescriptors for a channel, used when allocating a channel or
 * when the channel consumer/producer is ready, e.g. shell exec'd
 */

static void
channel_register_fds(Channel *c, int rfd, int wfd, int efd,
    int extusage, int nonblock)
{
	/* Update the maximum file descriptor value. */
	channel_max_fd = MAX(channel_max_fd, rfd);
	channel_max_fd = MAX(channel_max_fd, wfd);
	channel_max_fd = MAX(channel_max_fd, efd);

	/* XXX set close-on-exec -markus */

	c->rfd = rfd;
	c->wfd = wfd;
	c->sock = (rfd == wfd) ? rfd : -1;
	c->efd = efd;
	c->extended_usage = extusage;

	/* XXX ugly hack: nonblock is only set by the server */
	if (nonblock && isatty(c->rfd)) {
		debug("channel %d: rfd %d isatty", c->self, c->rfd);
		c->isatty = 1;
		if (!isatty(c->wfd)) {
			error("channel %d: wfd %d is not a tty?",
			    c->self, c->wfd);
		}
	} else {
		c->isatty = 0;
	}
	c->wfd_isatty = isatty(c->wfd);

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
	int i, found;
	Channel *c;

	/* Do initial allocation if this is the first call. */
	if (channels_alloc == 0) {
		channels_alloc = 10;
		channels = xmalloc(channels_alloc * sizeof(Channel *));
		for (i = 0; i < channels_alloc; i++)
			channels[i] = NULL;
		fatal_add_cleanup((void (*) (void *)) channel_free_all, NULL);
	}
	/* Try to find a free slot where to put the new channel. */
	for (found = -1, i = 0; i < channels_alloc; i++)
		if (channels[i] == NULL) {
			/* Found a free slot. */
			found = i;
			break;
		}
	if (found == -1) {
		/* There are no free slots.  Take last+1 slot and expand the array.  */
		found = channels_alloc;
		if (channels_alloc > 10000)
			fatal("channel_new: internal error: channels_alloc %d "
			    "too big.", channels_alloc);
		channels = xrealloc(channels,
		    (channels_alloc + 10) * sizeof(Channel *));
		channels_alloc += 10;
		debug2("channel: expanding %d", channels_alloc);
		for (i = found; i < channels_alloc; i++)
			channels[i] = NULL;
	}
	/* Initialize and return new channel. */
	c = channels[found] = xmalloc(sizeof(Channel));
	memset(c, 0, sizeof(Channel));
	buffer_init(&c->input);
	buffer_init(&c->output);
	buffer_init(&c->extended);
	c->ostate = CHAN_OUTPUT_OPEN;
	c->istate = CHAN_INPUT_OPEN;
	c->flags = 0;
	channel_register_fds(c, rfd, wfd, efd, extusage, nonblock);
	c->self = found;
	c->type = type;
	c->ctype = ctype;
	c->local_window = window;
	c->local_window_max = window;
	c->local_consumed = 0;
	c->local_maxpacket = maxpack;
	c->remote_id = -1;
	c->remote_name = remote_name;
	c->remote_window = 0;
	c->remote_maxpacket = 0;
	c->force_drain = 0;
	c->single_connection = 0;
	c->detach_user = NULL;
	c->confirm = NULL;
	c->input_filter = NULL;
	debug("channel %d: new [%s]", found, remote_name);
	return c;
}

static int
channel_find_maxfd(void)
{
	int i, max = 0;
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
	debug3("channel_close_fds: channel %d: r %d w %d e %d",
	    c->self, c->rfd, c->wfd, c->efd);

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
	int i, n;

	for (n = 0, i = 0; i < channels_alloc; i++)
		if (channels[i])
			n++;
	debug("channel_free: channel %d: %s, nchannels %d", c->self,
	    c->remote_name ? c->remote_name : "???", n);

	s = channel_open_message();
	debug3("channel_free: status: %s", s);
	xfree(s);

	if (c->sock != -1)
		shutdown(c->sock, SHUT_RDWR);
	channel_close_fds(c);
	buffer_free(&c->input);
	buffer_free(&c->output);
	buffer_free(&c->extended);
	if (c->remote_name) {
		xfree(c->remote_name);
		c->remote_name = NULL;
	}
	channels[c->self] = NULL;
	xfree(c);
}

void
channel_free_all(void)
{
	int i;

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
	int i;

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
	int i;
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
				debug2("channel %d: big output buffer %d > %d",
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
	int i;
	Channel *c;

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
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
			continue;
		case SSH_CHANNEL_LARVAL:
			if (!compat20)
				fatal("cannot happen: SSH_CHANNEL_LARVAL");
			continue;
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
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
	int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;
		switch (c->type) {
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_RPORT_LISTENER:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_ZOMBIE:
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
	int i;

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
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_CONNECTING:
		case SSH_CHANNEL_DYNAMIC:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			snprintf(buf, sizeof buf, "  #%d %.300s (t%d r%d i%d/%d o%d/%d fd %d/%d)\r\n",
			    c->self, c->remote_name,
			    c->type, c->remote_id,
			    c->istate, buffer_len(&c->input),
			    c->ostate, buffer_len(&c->output),
			    c->rfd, c->wfd);
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
		log("channel_send_open: %d: bad id", id);
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
		log("channel_request_start: %d: unknown channel id", id);
		return;
	}
	debug("channel %d: request %s", id, service) ;
	packet_start(SSH2_MSG_CHANNEL_REQUEST);
	packet_put_int(c->remote_id);
	packet_put_cstring(service);
	packet_put_char(wantconfirm);
}
void
channel_register_confirm(int id, channel_callback_fn *fn)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		log("channel_register_comfirm: %d: bad id", id);
		return;
	}
	c->confirm = fn;
}
void
channel_register_cleanup(int id, channel_callback_fn *fn)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		log("channel_register_cleanup: %d: bad id", id);
		return;
	}
	c->detach_user = fn;
}
void
channel_cancel_cleanup(int id)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		log("channel_cancel_cleanup: %d: bad id", id);
		return;
	}
	c->detach_user = NULL;
}
void
channel_register_filter(int id, channel_filter_fn *fn)
{
	Channel *c = channel_lookup(id);

	if (c == NULL) {
		log("channel_register_filter: %d: bad id", id);
		return;
	}
	c->input_filter = fn;
}

void
channel_set_fds(int id, int rfd, int wfd, int efd,
    int extusage, int nonblock, u_int window_max)
{
	Channel *c = channel_lookup(id);

	if (c == NULL || c->type != SSH_CHANNEL_LARVAL)
		fatal("channel_activate for non-larval channel %d.", id);
	channel_register_fds(c, rfd, wfd, efd, extusage, nonblock);
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
typedef void chan_fn(Channel *c, fd_set * readset, fd_set * writeset);
chan_fn *channel_pre[SSH_CHANNEL_MAX_TYPE];
chan_fn *channel_post[SSH_CHANNEL_MAX_TYPE];

static void
channel_pre_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	FD_SET(c->sock, readset);
}

static void
channel_pre_connecting(Channel *c, fd_set * readset, fd_set * writeset)
{
	debug3("channel %d: waiting for connection", c->self);
	FD_SET(c->sock, writeset);
}

static void
channel_pre_open_13(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->input) < packet_get_maxsize())
		FD_SET(c->sock, readset);
	if (buffer_len(&c->output) > 0)
		FD_SET(c->sock, writeset);
}

static void
channel_pre_open(Channel *c, fd_set * readset, fd_set * writeset)
{
	u_int limit = compat20 ? c->remote_window : packet_get_maxsize();

	if (c->istate == CHAN_INPUT_OPEN &&
	    limit > 0 &&
	    buffer_len(&c->input) < limit)
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
	if (compat20 && c->efd != -1) {
		if (c->extended_usage == CHAN_EXTENDED_WRITE &&
		    buffer_len(&c->extended) > 0)
			FD_SET(c->efd, writeset);
		else if (!(c->flags & CHAN_EOF_SENT) &&
		    c->extended_usage == CHAN_EXTENDED_READ &&
		    buffer_len(&c->extended) < c->remote_window)
			FD_SET(c->efd, readset);
	}
}

static void
channel_pre_input_draining(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->input) == 0) {
		packet_start(SSH_MSG_CHANNEL_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
		c->type = SSH_CHANNEL_CLOSED;
		debug("channel %d: closing after input drain.", c->self);
	}
}

static void
channel_pre_output_draining(Channel *c, fd_set * readset, fd_set * writeset)
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
		debug("Initial X11 packet contains bad byte order byte: 0x%x",
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
		debug("X11 connection uses different authentication protocol.");
		return -1;
	}
	/* Check if authentication data matches our fake data. */
	if (data_len != x11_fake_data_len ||
	    memcmp(ucp + 12 + ((proto_len + 3) & ~3),
		x11_fake_data, x11_fake_data_len) != 0) {
		debug("X11 auth data does not match fake data.");
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
channel_pre_x11_open_13(Channel *c, fd_set * readset, fd_set * writeset)
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
		log("X11 connection rejected because of wrong authentication.");
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
channel_pre_x11_open(Channel *c, fd_set * readset, fd_set * writeset)
{
	int ret = x11_open_helper(&c->output);

	/* c->force_drain = 1; */

	if (ret == 1) {
		c->type = SSH_CHANNEL_OPEN;
		channel_pre_open(c, readset, writeset);
	} else if (ret == -1) {
		log("X11 connection rejected because of wrong authentication.");
		debug("X11 rejected %d i%d/o%d", c->self, c->istate, c->ostate);
		chan_read_failed(c);
		buffer_clear(&c->input);
		chan_ibuf_empty(c);
		buffer_clear(&c->output);
		/* for proto v1, the peer will send an IEOF */
		if (compat20)
			chan_write_failed(c);
		else
			c->type = SSH_CHANNEL_OPEN;
		debug("X11 closed %d i%d/o%d", c->self, c->istate, c->ostate);
	}
}

/* try to decode a socks4 header */
static int
channel_decode_socks4(Channel *c, fd_set * readset, fd_set * writeset)
{
	char *p, *host;
	int len, have, i, found;
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
	for (found = 0, i = len; i < have; i++) {
		if (p[i] == '\0') {
			found = 1;
			break;
		}
		if (i > 1024) {
			/* the peer is probably sending garbage */
			debug("channel %d: decode socks4: too long",
			    c->self);
			return -1;
		}
	}
	if (!found)
		return 0;
	buffer_get(&c->input, (char *)&s4_req.version, 1);
	buffer_get(&c->input, (char *)&s4_req.command, 1);
	buffer_get(&c->input, (char *)&s4_req.dest_port, 2);
	buffer_get(&c->input, (char *)&s4_req.dest_addr, 4);
	have = buffer_len(&c->input);
	p = buffer_ptr(&c->input);
	len = strlen(p);
	debug2("channel %d: decode socks4: user %s/%d", c->self, p, len);
	if (len > have)
		fatal("channel %d: decode socks4: len %d > have %d",
		    c->self, len, have);
	strlcpy(username, p, sizeof(username));
	buffer_consume(&c->input, len);
	buffer_consume(&c->input, 1);		/* trailing '\0' */

	host = inet_ntoa(s4_req.dest_addr);
	strlcpy(c->path, host, sizeof(c->path));
	c->host_port = ntohs(s4_req.dest_port);

	debug("channel %d: dynamic request: socks4 host %s port %u command %u",
	    c->self, host, c->host_port, s4_req.command);

	if (s4_req.command != 1) {
		debug("channel %d: cannot handle: socks4 cn %d",
		    c->self, s4_req.command);
		return -1;
	}
	s4_rsp.version = 0;			/* vn: 0 for reply */
	s4_rsp.command = 90;			/* cd: req granted */
	s4_rsp.dest_port = 0;			/* ignored */
	s4_rsp.dest_addr.s_addr = INADDR_ANY;	/* ignored */
	buffer_append(&c->output, (char *)&s4_rsp, sizeof(s4_rsp));
	return 1;
}

/* dynamic port forwarding */
static void
channel_pre_dynamic(Channel *c, fd_set * readset, fd_set * writeset)
{
	u_char *p;
	int have, ret;

	have = buffer_len(&c->input);
	c->delayed = 0;
	debug2("channel %d: pre_dynamic: have %d", c->self, have);
	/* buffer_dump(&c->input); */
	/* check if the fixed size part of the packet is in buffer. */
	if (have < 4) {
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
static void
channel_post_x11_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	Channel *nc;
	struct sockaddr addr;
	int newsock;
	socklen_t addrlen;
	char buf[16384], *remote_ipaddr;
	int remote_port;

	if (FD_ISSET(c->sock, readset)) {
		debug("X11 connection requested.");
		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (c->single_connection) {
			debug("single_connection: closing X11 listener.");
			channel_close_fd(&c->sock);
			chan_mark_dead(c);
		}
		if (newsock < 0) {
			error("accept: %.100s", strerror(errno));
			return;
		}
		set_nodelay(newsock);
		remote_ipaddr = get_peer_ipaddr(newsock);
		remote_port = get_peer_port(newsock);
		snprintf(buf, sizeof buf, "X11 connection from %.200s port %d",
		    remote_ipaddr, remote_port);

		nc = channel_new("accepted x11 socket",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, xstrdup(buf), 1);
		if (compat20) {
			packet_start(SSH2_MSG_CHANNEL_OPEN);
			packet_put_cstring("x11");
			packet_put_int(nc->self);
			packet_put_int(nc->local_window_max);
			packet_put_int(nc->local_maxpacket);
			/* originator ipaddr and port */
			packet_put_cstring(remote_ipaddr);
			if (datafellows & SSH_BUG_X11FWD) {
				debug("ssh2 x11 bug compat mode");
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
		xfree(remote_ipaddr);
	}
}

static void
port_open_helper(Channel *c, char *rtype)
{
	int direct;
	char buf[1024];
	char *remote_ipaddr = get_peer_ipaddr(c->sock);
	u_short remote_port = get_peer_port(c->sock);

	direct = (strcmp(rtype, "direct-tcpip") == 0);

	snprintf(buf, sizeof buf,
	    "%s: listening port %d for %.100s port %d, "
	    "connect from %.200s port %d",
	    rtype, c->listening_port, c->path, c->host_port,
	    remote_ipaddr, remote_port);

	xfree(c->remote_name);
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
		packet_put_int(remote_port);
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
	xfree(remote_ipaddr);
}

/*
 * This socket is listening for connections to a forwarded TCP/IP port.
 */
static void
channel_post_port_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	Channel *nc;
	struct sockaddr addr;
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
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept: %.100s", strerror(errno));
			return;
		}
		set_nodelay(newsock);
		nc = channel_new(rtype,
		    nextstate, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, xstrdup(rtype), 1);
		nc->listening_port = c->listening_port;
		nc->host_port = c->host_port;
		strlcpy(nc->path, c->path, sizeof(nc->path));

		if (nextstate == SSH_CHANNEL_DYNAMIC) {
			/*
			 * do not call the channel_post handler until
			 * this flag has been reset by a pre-handler.
			 * otherwise the FD_ISSET calls might overflow
			 */
			nc->delayed = 1;
		} else {
			port_open_helper(nc, rtype);
		}
	}
}

/*
 * This is the authentication agent socket listening for connections from
 * clients.
 */
static void
channel_post_auth_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	Channel *nc;
	char *name;
	int newsock;
	struct sockaddr addr;
	socklen_t addrlen;

	if (FD_ISSET(c->sock, readset)) {
		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept from auth socket: %.100s", strerror(errno));
			return;
		}
		name = xstrdup("accepted auth socket");
		nc = channel_new("accepted auth socket",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, name, 1);
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

static void
channel_post_connecting(Channel *c, fd_set * readset, fd_set * writeset)
{
	int err = 0;
	socklen_t sz = sizeof(err);

	if (FD_ISSET(c->sock, writeset)) {
		if (getsockopt(c->sock, SOL_SOCKET, SO_ERROR, &err, &sz) < 0) {
			err = errno;
			error("getsockopt SO_ERROR failed");
		}
		if (err == 0) {
			debug("channel %d: connected", c->self);
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
			debug("channel %d: not connected: %s",
			    c->self, strerror(err));
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

static int
channel_handle_rfd(Channel *c, fd_set * readset, fd_set * writeset)
{
	char buf[16*1024];
	int len;

	if (c->rfd != -1 &&
	    FD_ISSET(c->rfd, readset)) {
		len = read(c->rfd, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR || errno == EAGAIN))
			return 1;
		if (len <= 0) {
			debug("channel %d: read<=0 rfd %d len %d",
			    c->self, c->rfd, len);
			if (c->type != SSH_CHANNEL_OPEN) {
				debug("channel %d: not open", c->self);
				chan_mark_dead(c);
				return -1;
			} else if (compat13) {
				buffer_clear(&c->output);
				c->type = SSH_CHANNEL_INPUT_DRAINING;
				debug("channel %d: input draining.", c->self);
			} else {
				chan_read_failed(c);
			}
			return -1;
		}
		if (c->input_filter != NULL) {
			if (c->input_filter(c, buf, len) == -1) {
				debug("channel %d: filter stops", c->self);
				chan_read_failed(c);
			}
		} else {
			buffer_append(&c->input, buf, len);
		}
	}
	return 1;
}
static int
channel_handle_wfd(Channel *c, fd_set * readset, fd_set * writeset)
{
	struct termios tio;
	u_char *data;
	u_int dlen;
	int len;

	/* Send buffered output data to the socket. */
	if (c->wfd != -1 &&
	    FD_ISSET(c->wfd, writeset) &&
	    buffer_len(&c->output) > 0) {
		data = buffer_ptr(&c->output);
		dlen = buffer_len(&c->output);
#ifdef _AIX
		/* XXX: Later AIX versions can't push as much data to tty */ 
		if (compat20 && c->wfd_isatty && dlen > 8*1024)
			dlen = 8*1024;
#endif
		len = write(c->wfd, data, dlen);
		if (len < 0 && (errno == EINTR || errno == EAGAIN))
			return 1;
		if (len <= 0) {
			if (c->type != SSH_CHANNEL_OPEN) {
				debug("channel %d: not open", c->self);
				chan_mark_dead(c);
				return -1;
			} else if (compat13) {
				buffer_clear(&c->output);
				debug("channel %d: input draining.", c->self);
				c->type = SSH_CHANNEL_INPUT_DRAINING;
			} else {
				chan_write_failed(c);
			}
			return -1;
		}
		if (compat20 && c->isatty && dlen >= 1 && data[0] != '\r') {
			if (tcgetattr(c->wfd, &tio) == 0 &&
			    !(tio.c_lflag & ECHO) && (tio.c_lflag & ICANON)) {
				/*
				 * Simulate echo to reduce the impact of
				 * traffic analysis. We need to match the
				 * size of a SSH2_MSG_CHANNEL_DATA message
				 * (4 byte channel id + data)
				 */
				packet_send_ignore(4 + len);
				packet_send();
			}
		}
		buffer_consume(&c->output, len);
		if (compat20 && len > 0) {
			c->local_consumed += len;
		}
	}
	return 1;
}
static int
channel_handle_efd(Channel *c, fd_set * readset, fd_set * writeset)
{
	char buf[16*1024];
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
			if (len < 0 && (errno == EINTR || errno == EAGAIN))
				return 1;
			if (len <= 0) {
				debug2("channel %d: closing write-efd %d",
				    c->self, c->efd);
				channel_close_fd(&c->efd);
			} else {
				buffer_consume(&c->extended, len);
				c->local_consumed += len;
			}
		} else if (c->extended_usage == CHAN_EXTENDED_READ &&
		    FD_ISSET(c->efd, readset)) {
			len = read(c->efd, buf, sizeof(buf));
			debug2("channel %d: read %d from efd %d",
			    c->self, len, c->efd);
			if (len < 0 && (errno == EINTR || errno == EAGAIN))
				return 1;
			if (len <= 0) {
				debug2("channel %d: closing read-efd %d",
				    c->self, c->efd);
				channel_close_fd(&c->efd);
			} else {
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
	    c->local_window < c->local_window_max/2 &&
	    c->local_consumed > 0) {
		packet_start(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
		packet_put_int(c->remote_id);
		packet_put_int(c->local_consumed);
		packet_send();
		debug2("channel %d: window %d sent adjust %d",
		    c->self, c->local_window,
		    c->local_consumed);
		c->local_window += c->local_consumed;
		c->local_consumed = 0;
	}
	return 1;
}

static void
channel_post_open(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (c->delayed)
		return;
	channel_handle_rfd(c, readset, writeset);
	channel_handle_wfd(c, readset, writeset);
	if (!compat20)
		return;
	channel_handle_efd(c, readset, writeset);
	channel_check_window(c);
}

static void
channel_post_output_drain_13(Channel *c, fd_set * readset, fd_set * writeset)
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

	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_RPORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_CONNECTING] =		&channel_post_connecting;
	channel_post[SSH_CHANNEL_DYNAMIC] =		&channel_post_open;
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
		if (!chan_is_dead(c, 0))
			return;
		debug("channel %d: gc: notify user", c->self);
		c->detach_user(c->self, NULL);
		/* if we still have a callback */
		if (c->detach_user != NULL)
			return;
		debug("channel %d: gc: user detached", c->self);
	}
	if (!chan_is_dead(c, 1))
		return;
	debug("channel %d: garbage collecting", c->self);
	channel_free(c);
}

static void
channel_handler(chan_fn *ftab[], fd_set * readset, fd_set * writeset)
{
	static int did_init = 0;
	int i;
	Channel *c;

	if (!did_init) {
		channel_handler_init();
		did_init = 1;
	}
	for (i = 0; i < channels_alloc; i++) {
		c = channels[i];
		if (c == NULL)
			continue;
		if (ftab[c->type] != NULL)
			(*ftab[c->type])(c, readset, writeset);
		channel_garbage_collect(c);
	}
}

/*
 * Allocate/update select bitmasks and add any bits relevant to channels in
 * select bitmasks.
 */
void
channel_prepare_select(fd_set **readsetp, fd_set **writesetp, int *maxfdp,
    int *nallocp, int rekeying)
{
	int n;
	u_int sz;

	n = MAX(*maxfdp, channel_max_fd);

	sz = howmany(n+1, NFDBITS) * sizeof(fd_mask);
	/* perhaps check sz < nalloc/2 and shrink? */
	if (*readsetp == NULL || sz > *nallocp) {
		*readsetp = xrealloc(*readsetp, sz);
		*writesetp = xrealloc(*writesetp, sz);
		*nallocp = sz;
	}
	*maxfdp = n;
	memset(*readsetp, 0, sz);
	memset(*writesetp, 0, sz);

	if (!rekeying)
		channel_handler(channel_pre, *readsetp, *writesetp);
}

/*
 * After select, perform any appropriate operations for channels which have
 * events pending.
 */
void
channel_after_select(fd_set * readset, fd_set * writeset)
{
	channel_handler(channel_post, readset, writeset);
}


/* If there is data to send to the connection, enqueue some of it now. */

void
channel_output_poll(void)
{
	Channel *c;
	int i;
	u_int len;

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

void
channel_input_data(int type, u_int32_t seq, void *ctxt)
{
	int id;
	char *data;
	u_int data_len;
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

	/* same for protocol 1.5 if output end is no longer open */
	if (!compat13 && c->ostate != CHAN_OUTPUT_OPEN)
		return;

	/* Get the data. */
	data = packet_get_string(&data_len);

	if (compat20) {
		if (data_len > c->local_maxpacket) {
			log("channel %d: rcvd big packet %d, maxpack %d",
			    c->self, data_len, c->local_maxpacket);
		}
		if (data_len > c->local_window) {
			log("channel %d: rcvd too much data %d, win %d",
			    c->self, data_len, c->local_window);
			xfree(data);
			return;
		}
		c->local_window -= data_len;
	}
	packet_check_eom();
	buffer_append(&c->output, data, data_len);
	xfree(data);
}

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
		log("channel %d: ext data for non open", id);
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
		log("channel %d: bad ext data", c->self);
		return;
	}
	data = packet_get_string(&data_len);
	packet_check_eom();
	if (data_len > c->local_window) {
		log("channel %d: rcvd too much extended_data %d, win %d",
		    c->self, data_len, c->local_window);
		xfree(data);
		return;
	}
	debug2("channel %d: rcvd ext data %d", c->self, data_len);
	c->local_window -= data_len;
	buffer_append(&c->extended, data, data_len);
	xfree(data);
}

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

void
channel_input_close_confirmation(int type, u_int32_t seq, void *ctxt)
{
	int id = packet_get_int();
	Channel *c = channel_lookup(id);

	packet_check_eom();
	if (c == NULL)
		packet_disconnect("Received close confirmation for "
		    "out-of-range channel %d.", id);
	if (c->type != SSH_CHANNEL_CLOSED)
		packet_disconnect("Received close confirmation for "
		    "non-closed channel %d (type %d).", id, c->type);
	channel_free(c);
}

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
		if (c->confirm) {
			debug2("callback start");
			c->confirm(c->self, NULL);
			debug2("callback done");
		}
		debug("channel %d: open confirm rwindow %u rmax %u", c->self,
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
		log("channel %d: open failed: %s%s%s", id,
		    reason2txt(reason), msg ? ": ": "", msg ? msg : "");
		if (msg != NULL)
			xfree(msg);
		if (lang != NULL)
			xfree(lang);
	}
	packet_check_eom();
	/* Free the channel.  This will also close the socket. */
	channel_free(c);
}

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

	if (c == NULL || c->type != SSH_CHANNEL_OPEN) {
		log("Received window adjust for "
		    "non-open channel %d.", id);
		return;
	}
	adjust = packet_get_int();
	packet_check_eom();
	debug2("channel %d: rcvd adjust %u", id, adjust);
	c->remote_window += adjust;
}

void
channel_input_port_open(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	u_short host_port;
	char *host, *originator_string;
	int remote_id, sock = -1;

	remote_id = packet_get_int();
	host = packet_get_string(NULL);
	host_port = packet_get_int();

	if (packet_get_protocol_flags() & SSH_PROTOFLAG_HOST_IN_FWD_OPEN) {
		originator_string = packet_get_string(NULL);
	} else {
		originator_string = xstrdup("unknown (remote did not supply name)");
	}
	packet_check_eom();
	sock = channel_connect_to(host, host_port);
	if (sock != -1) {
		c = channel_new("connected socket",
		    SSH_CHANNEL_CONNECTING, sock, sock, -1, 0, 0, 0,
		    originator_string, 1);
		c->remote_id = remote_id;
	}
	if (c == NULL) {
		xfree(originator_string);
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
		packet_send();
	}
	xfree(host);
}


/* -- tcp forwarding */

void
channel_set_af(int af)
{
	IPv4or6 = af;
}

static int
channel_setup_fwd_listener(int type, const char *listen_addr, u_short listen_port,
    const char *host_to_connect, u_short port_to_connect, int gateway_ports)
{
	Channel *c;
	int success, sock, on = 1;
	struct addrinfo hints, *ai, *aitop;
	const char *host;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];

	success = 0;
	host = (type == SSH_CHANNEL_RPORT_LISTENER) ?
	    listen_addr : host_to_connect;

	if (host == NULL) {
		error("No forward host name.");
		return success;
	}
	if (strlen(host) > SSH_CHANNEL_PATH_LEN - 1) {
		error("Forward host name too long.");
		return success;
	}

	/*
	 * getaddrinfo returns a loopback address if the hostname is
	 * set to NULL and hints.ai_flags is not AI_PASSIVE
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_flags = gateway_ports ? AI_PASSIVE : 0;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", listen_port);
	if (getaddrinfo(NULL, strport, &hints, &aitop) != 0)
		packet_disconnect("getaddrinfo: fatal error");

	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("channel_setup_fwd_listener: getnameinfo failed");
			continue;
		}
		/* Create a port to listen for the host. */
		sock = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock < 0) {
			/* this is no error since kernel may not support ipv6 */
			verbose("socket: %.100s", strerror(errno));
			continue;
		}
		/*
		 * Set socket options.
		 * Allow local port reuse in TIME_WAIT.
		 */
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on,
		    sizeof(on)) == -1)
			error("setsockopt SO_REUSEADDR: %s", strerror(errno));

		debug("Local forwarding listening on %s port %s.", ntop, strport);

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
		if (listen(sock, 5) < 0) {
			error("listen: %.100s", strerror(errno));
			close(sock);
			continue;
		}
		/* Allocate a channel number for the socket. */
		c = channel_new("port listener", type, sock, sock, -1,
		    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
		    0, xstrdup("port listener"), 1);
		strlcpy(c->path, host, sizeof(c->path));
		c->host_port = port_to_connect;
		c->listening_port = listen_port;
		success = 1;
	}
	if (success == 0)
		error("channel_setup_fwd_listener: cannot listen to port: %d",
		    listen_port);
	freeaddrinfo(aitop);
	return success;
}

/* protocol local port fwd, used by ssh (and sshd in v1) */
int
channel_setup_local_fwd_listener(u_short listen_port,
    const char *host_to_connect, u_short port_to_connect, int gateway_ports)
{
	return channel_setup_fwd_listener(SSH_CHANNEL_PORT_LISTENER,
	    NULL, listen_port, host_to_connect, port_to_connect, gateway_ports);
}

/* protocol v2 remote port fwd, used by sshd */
int
channel_setup_remote_fwd_listener(const char *listen_address,
    u_short listen_port, int gateway_ports)
{
	return channel_setup_fwd_listener(SSH_CHANNEL_RPORT_LISTENER,
	    listen_address, listen_port, NULL, 0, gateway_ports);
}

/*
 * Initiate forwarding of connections to port "port" on remote host through
 * the secure channel to host:port from local side.
 */

void
channel_request_remote_forwarding(u_short listen_port,
    const char *host_to_connect, u_short port_to_connect)
{
	int type, success = 0;

	/* Record locally that connection to this host/port is permitted. */
	if (num_permitted_opens >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("channel_request_remote_forwarding: too many forwards");

	/* Send the forward request to the remote side. */
	if (compat20) {
		const char *address_to_bind = "0.0.0.0";
		packet_start(SSH2_MSG_GLOBAL_REQUEST);
		packet_put_cstring("tcpip-forward");
		packet_put_char(1);			/* boolean: want reply */
		packet_put_cstring(address_to_bind);
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
			log("Warning: Server denied remote port forwarding.");
			break;
		default:
			/* Unknown packet */
			packet_disconnect("Protocol error for port forward request:"
			    "received packet type %d.", type);
		}
	}
	if (success) {
		permitted_opens[num_permitted_opens].host_to_connect = xstrdup(host_to_connect);
		permitted_opens[num_permitted_opens].port_to_connect = port_to_connect;
		permitted_opens[num_permitted_opens].listen_port = listen_port;
		num_permitted_opens++;
	}
}

/*
 * This is called after receiving CHANNEL_FORWARDING_REQUEST.  This initates
 * listening for the port, and sends back a success reply (or disconnect
 * message if there was an error).  This never returns if there was an error.
 */

void
channel_input_port_forward_request(int is_root, int gateway_ports)
{
	u_short port, host_port;
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
		packet_disconnect("Requested forwarding of port %d but user is not root.",
				  port);
#endif
	/* Initiate forwarding */
	channel_setup_local_fwd_listener(port, hostname, host_port, gateway_ports);

	/* Free the argument string. */
	xfree(hostname);
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
	if (num_permitted_opens >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("channel_request_remote_forwarding: too many forwards");
	debug("allow port forwarding to host %s port %d", host, port);

	permitted_opens[num_permitted_opens].host_to_connect = xstrdup(host);
	permitted_opens[num_permitted_opens].port_to_connect = port;
	num_permitted_opens++;

	all_opens_permitted = 0;
}

void
channel_clear_permitted_opens(void)
{
	int i;

	for (i = 0; i < num_permitted_opens; i++)
		xfree(permitted_opens[i].host_to_connect);
	num_permitted_opens = 0;

}


/* return socket to remote host, port */
static int
connect_to(const char *host, u_short port)
{
	struct addrinfo hints, *ai, *aitop;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	int gaierr;
	int sock = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", port);
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0) {
		error("connect_to %.100s: unknown host (%s)", host,
		    gai_strerror(gaierr));
		return -1;
	}
	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("connect_to: getnameinfo failed");
			continue;
		}
		sock = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock < 0) {
			if (ai->ai_next == NULL)
				error("socket: %.100s", strerror(errno));
			else
				verbose("socket: %.100s", strerror(errno));
			continue;
		}
		if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
			fatal("connect_to: F_SETFL: %s", strerror(errno));
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0 &&
		    errno != EINPROGRESS) {
			error("connect_to %.100s port %s: %.100s", ntop, strport,
			    strerror(errno));
			close(sock);
			continue;	/* fail -- try next */
		}
		break; /* success */

	}
	freeaddrinfo(aitop);
	if (!ai) {
		error("connect_to %.100s port %d: failed.", host, port);
		return -1;
	}
	/* success */
	set_nodelay(sock);
	return sock;
}

int
channel_connect_by_listen_address(u_short listen_port)
{
	int i;

	for (i = 0; i < num_permitted_opens; i++)
		if (permitted_opens[i].listen_port == listen_port)
			return connect_to(
			    permitted_opens[i].host_to_connect,
			    permitted_opens[i].port_to_connect);
	error("WARNING: Server requests forwarding for unknown listen_port %d",
	    listen_port);
	return -1;
}

/* Check if connecting to that port is permitted and connect. */
int
channel_connect_to(const char *host, u_short port)
{
	int i, permit;

	permit = all_opens_permitted;
	if (!permit) {
		for (i = 0; i < num_permitted_opens; i++)
			if (permitted_opens[i].port_to_connect == port &&
			    strcmp(permitted_opens[i].host_to_connect, host) == 0)
				permit = 1;

	}
	if (!permit) {
		log("Received request to connect to host %.100s port %d, "
		    "but the request was denied.", host, port);
		return -1;
	}
	return connect_to(host, port);
}

/* -- X11 forwarding */

/*
 * Creates an internet domain socket for listening for X11 connections.
 * Returns 0 and a suitable display number for the DISPLAY variable
 * stored in display_numberp , or -1 if an error occurs.
 */
int
x11_create_display_inet(int x11_display_offset, int x11_use_localhost,
    int single_connection, u_int *display_numberp)
{
	Channel *nc = NULL;
	int display_number, sock;
	u_short port;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr, n, num_socks = 0, socks[NUM_SOCKS];

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
			error("getaddrinfo: %.100s", gai_strerror(gaierr));
			return -1;
		}
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;
			sock = socket(ai->ai_family, SOCK_STREAM, 0);
			if (sock < 0) {
				if ((errno != EINVAL) && (errno != EAFNOSUPPORT)) {
					error("socket: %.100s", strerror(errno));
					return -1;
				} else {
					debug("x11_create_display_inet: Socket family %d not supported",
						 ai->ai_family);
					continue;
				}
			}
#ifdef IPV6_V6ONLY
			if (ai->ai_family == AF_INET6) {
				int on = 1;
				if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0)
					error("setsockopt IPV6_V6ONLY: %.100s", strerror(errno));
			}
#endif
			if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
				debug("bind port %d: %.100s", port, strerror(errno));
				close(sock);

				if (ai->ai_next)
					continue;

				for (n = 0; n < num_socks; n++) {
					close(socks[n]);
				}
				num_socks = 0;
				break;
			}
			socks[num_socks++] = sock;
#ifndef DONT_TRY_OTHER_AF
			if (num_socks == NUM_SOCKS)
				break;
#else
			if (x11_use_localhost) {
				if (num_socks == NUM_SOCKS)
					break;
			} else {
				break;
			}
#endif
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
		if (listen(sock, 5) < 0) {
			error("listen: %.100s", strerror(errno));
			close(sock);
			return -1;
		}
	}

	/* Allocate a channel for each socket. */
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		nc = channel_new("x11 listener",
		    SSH_CHANNEL_X11_LISTENER, sock, sock, -1,
		    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
		    0, xstrdup("X11 inet listener"), 1);
		nc->single_connection = single_connection;
	}

	/* Return the display number for the DISPLAY environment variable. */
	*display_numberp = display_number;
	return (0);
}

static int
connect_local_xsocket(u_int dnr)
{
	int sock;
	struct sockaddr_un addr;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		error("socket: %.100s", strerror(errno));
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof addr.sun_path, _PATH_UNIX_X, dnr);
	if (connect(sock, (struct sockaddr *) & addr, sizeof(addr)) == 0)
		return sock;
	close(sock);
	error("connect %.100s: %.100s", addr.sun_path, strerror(errno));
	return -1;
}

int
x11_connect_display(void)
{
	int display_number, sock = 0;
	const char *display;
	char buf[1024], *cp;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;

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

	/*
	 * Check if it is a unix domain socket.  Unix domain displays are in
	 * one of the following formats: unix:d[.s], :d[.s], ::d[.s]
	 */
	if (strncmp(display, "unix:", 5) == 0 ||
	    display[0] == ':') {
		/* Connect to the unix domain socket. */
		if (sscanf(strrchr(display, ':') + 1, "%d", &display_number) != 1) {
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
	if (sscanf(cp + 1, "%d", &display_number) != 1) {
		error("Could not parse display number from DISPLAY: %.100s",
		    display);
		return -1;
	}

	/* Look up the host address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", 6000 + display_number);
	if ((gaierr = getaddrinfo(buf, strport, &hints, &aitop)) != 0) {
		error("%.100s: unknown host. (%s)", buf, gai_strerror(gaierr));
		return -1;
	}
	for (ai = aitop; ai; ai = ai->ai_next) {
		/* Create a socket. */
		sock = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock < 0) {
			debug("socket: %.100s", strerror(errno));
			continue;
		}
		/* Connect it to the display. */
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			debug("connect %.100s port %d: %.100s", buf,
			    6000 + display_number, strerror(errno));
			close(sock);
			continue;
		}
		/* Success */
		break;
	}
	freeaddrinfo(aitop);
	if (!ai) {
		error("connect %.100s port %d: %.100s", buf, 6000 + display_number,
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
	if (c == NULL) {
		/* Send refusal to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
		xfree(remote_host);
	} else {
		/* Send a confirmation to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(remote_id);
		packet_put_int(c->self);
	}
	packet_send();
}

/* dummy protocol handler that denies SSH-1 requests (agent/x11) */
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
	error("Warning: this is probably a break in attempt by a malicious server.");
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
x11_request_forwarding_with_spoofing(int client_session_id,
    const char *proto, const char *data)
{
	u_int data_len = (u_int) strlen(data) / 2;
	u_int i, value, len;
	char *new_data;
	int screen_number;
	const char *cp;
	u_int32_t rand = 0;

	cp = getenv("DISPLAY");
	if (cp)
		cp = strchr(cp, ':');
	if (cp)
		cp = strchr(cp, '.');
	if (cp)
		screen_number = atoi(cp + 1);
	else
		screen_number = 0;

	/* Save protocol name. */
	x11_saved_proto = xstrdup(proto);

	/*
	 * Extract real authentication data and generate fake data of the
	 * same length.
	 */
	x11_saved_data = xmalloc(data_len);
	x11_fake_data = xmalloc(data_len);
	for (i = 0; i < data_len; i++) {
		if (sscanf(data + 2 * i, "%2x", &value) != 1)
			fatal("x11_request_forwarding: bad authentication data: %.100s", data);
		if (i % 4 == 0)
			rand = arc4random();
		x11_saved_data[i] = value;
		x11_fake_data[i] = rand & 0xff;
		rand >>= 8;
	}
	x11_saved_data_len = data_len;
	x11_fake_data_len = data_len;

	/* Convert the fake data into hex. */
	len = 2 * data_len + 1;
	new_data = xmalloc(len);
	for (i = 0; i < data_len; i++)
		snprintf(new_data + 2 * i, len - 2 * i,
		    "%02x", (u_char) x11_fake_data[i]);

	/* Send the request packet. */
	if (compat20) {
		channel_request_start(client_session_id, "x11-req", 0);
		packet_put_char(0);	/* XXX bool single connection */
	} else {
		packet_start(SSH_CMSG_X11_REQUEST_FORWARDING);
	}
	packet_put_cstring(proto);
	packet_put_cstring(new_data);
	packet_put_int(screen_number);
	packet_send();
	packet_write_wait();
	xfree(new_data);
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

/* This is called to process an SSH_SMSG_AGENT_OPEN message. */

void
auth_input_open_request(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	int remote_id, sock;
	char *name;

	/* Read the remote channel number from the message. */
	remote_id = packet_get_int();
	packet_check_eom();

	/*
	 * Get a connection to the local authentication agent (this may again
	 * get forwarded).
	 */
	sock = ssh_get_authentication_socket();

	/*
	 * If we could not connect the agent, send an error message back to
	 * the server. This should never happen unless the agent dies,
	 * because authentication forwarding is only enabled if we have an
	 * agent.
	 */
	if (sock >= 0) {
		name = xstrdup("authentication agent connection");
		c = channel_new("", SSH_CHANNEL_OPEN, sock, sock,
		    -1, 0, 0, 0, name, 1);
		c->remote_id = remote_id;
		c->force_drain = 1;
	}
	if (c == NULL) {
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_id);
	} else {
		/* Send a confirmation to the remote host. */
		debug("Forwarding authentication connection.");
		packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(remote_id);
		packet_put_int(c->self);
	}
	packet_send();
}
