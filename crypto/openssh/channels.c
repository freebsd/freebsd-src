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
 *
 * SSH2 support added by Markus Friedl.
 * Copyright (c) 1999,2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: channels.c,v 1.68 2000/09/07 20:40:29 markus Exp $");

#include "ssh.h"
#include "packet.h"
#include "xmalloc.h"
#include "buffer.h"
#include "uidswap.h"
#include "readconf.h"
#include "servconf.h"

#include "channels.h"
#include "nchan.h"
#include "compat.h"

#include "ssh2.h"

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include "key.h"
#include "authfd.h"

/* Maximum number of fake X11 displays to try. */
#define MAX_DISPLAYS  1000

/* Max len of agent socket */
#define MAX_SOCKET_NAME 100

/*
 * Pointer to an array containing all allocated channels.  The array is
 * dynamically extended as needed.
 */
static Channel *channels = NULL;

/*
 * Size of the channel array.  All slots of the array must always be
 * initialized (at least the type field); unused slots are marked with type
 * SSH_CHANNEL_FREE.
 */
static int channels_alloc = 0;

/*
 * Maximum file descriptor value used in any of the channels.  This is
 * updated in channel_allocate.
 */
static int channel_max_fd_value = 0;

/* Name and directory of socket for authentication agent forwarding. */
static char *channel_forwarded_auth_socket_name = NULL;
static char *channel_forwarded_auth_socket_dir = NULL;

/* Saved X11 authentication protocol name. */
char *x11_saved_proto = NULL;

/* Saved X11 authentication data.  This is the real data. */
char *x11_saved_data = NULL;
unsigned int x11_saved_data_len = 0;

/*
 * Fake X11 authentication data.  This is what the server will be sending us;
 * we should replace any occurrences of this by the real data.
 */
char *x11_fake_data = NULL;
unsigned int x11_fake_data_len;

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

/* This is set to true if both sides support SSH_PROTOFLAG_HOST_IN_FWD_OPEN. */
static int have_hostname_in_open = 0;

/* Sets specific protocol options. */

void
channel_set_options(int hostname_in_open)
{
	have_hostname_in_open = hostname_in_open;
}

/*
 * Permits opening to any host/port in SSH_MSG_PORT_OPEN.  This is usually
 * called by the server, because the user could connect to any port anyway,
 * and the server has no way to know but to trust the client anyway.
 */

void
channel_permit_all_opens()
{
	all_opens_permitted = 1;
}

/* lookup channel by id */

Channel *
channel_lookup(int id)
{
	Channel *c;
	if (id < 0 || id > channels_alloc) {
		log("channel_lookup: %d: bad id", id);
		return NULL;
	}
	c = &channels[id];
	if (c->type == SSH_CHANNEL_FREE) {
		log("channel_lookup: %d: bad id: channel free", id);
		return NULL;
	}
	return c;
}

/*
 * Register filedescriptors for a channel, used when allocating a channel or
 * when the channel consumer/producer is ready, e.g. shell exec'd
 */

void
channel_register_fds(Channel *c, int rfd, int wfd, int efd, int extusage)
{
	/* Update the maximum file descriptor value. */
	if (rfd > channel_max_fd_value)
		channel_max_fd_value = rfd;
	if (wfd > channel_max_fd_value)
		channel_max_fd_value = wfd;
	if (efd > channel_max_fd_value)
		channel_max_fd_value = efd;
	/* XXX set close-on-exec -markus */

	c->rfd = rfd;
	c->wfd = wfd;
	c->sock = (rfd == wfd) ? rfd : -1;
	c->efd = efd;
	c->extended_usage = extusage;
	if (rfd != -1)
		set_nonblock(rfd);
	if (wfd != -1)
		set_nonblock(wfd);
	if (efd != -1)
		set_nonblock(efd);
}

/*
 * Allocate a new channel object and set its type and socket. This will cause
 * remote_name to be freed.
 */

int
channel_new(char *ctype, int type, int rfd, int wfd, int efd,
    int window, int maxpack, int extusage, char *remote_name)
{
	int i, found;
	Channel *c;

	/* Do initial allocation if this is the first call. */
	if (channels_alloc == 0) {
		chan_init();
		channels_alloc = 10;
		channels = xmalloc(channels_alloc * sizeof(Channel));
		for (i = 0; i < channels_alloc; i++)
			channels[i].type = SSH_CHANNEL_FREE;
		/*
		 * Kludge: arrange a call to channel_stop_listening if we
		 * terminate with fatal().
		 */
		fatal_add_cleanup((void (*) (void *)) channel_stop_listening, NULL);
	}
	/* Try to find a free slot where to put the new channel. */
	for (found = -1, i = 0; i < channels_alloc; i++)
		if (channels[i].type == SSH_CHANNEL_FREE) {
			/* Found a free slot. */
			found = i;
			break;
		}
	if (found == -1) {
		/* There are no free slots.  Take last+1 slot and expand the array.  */
		found = channels_alloc;
		channels_alloc += 10;
		debug("channel: expanding %d", channels_alloc);
		channels = xrealloc(channels, channels_alloc * sizeof(Channel));
		for (i = found; i < channels_alloc; i++)
			channels[i].type = SSH_CHANNEL_FREE;
	}
	/* Initialize and return new channel number. */
	c = &channels[found];
	buffer_init(&c->input);
	buffer_init(&c->output);
	buffer_init(&c->extended);
	chan_init_iostates(c);
	channel_register_fds(c, rfd, wfd, efd, extusage);
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
	c->cb_fn = NULL;
	c->cb_arg = NULL;
	c->cb_event = 0;
	c->dettach_user = NULL;
	c->input_filter = NULL;
	debug("channel %d: new [%s]", found, remote_name);
	return found;
}
/* old interface XXX */
int
channel_allocate(int type, int sock, char *remote_name)
{
	return channel_new("", type, sock, sock, -1, 0, 0, 0, remote_name);
}


/* Close all channel fd/socket. */

void
channel_close_fds(Channel *c)
{
	if (c->sock != -1) {
		close(c->sock);
		c->sock = -1;
	}
	if (c->rfd != -1) {
		close(c->rfd);
		c->rfd = -1;
	}
	if (c->wfd != -1) {
		close(c->wfd);
		c->wfd = -1;
	}
	if (c->efd != -1) {
		close(c->efd);
		c->efd = -1;
	}
}

/* Free the channel and close its fd/socket. */

void
channel_free(int id)
{
	Channel *c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("channel free: bad local channel %d", id);
	debug("channel_free: channel %d: status: %s", id, channel_open_message());
	if (c->dettach_user != NULL) {
		debug("channel_free: channel %d: dettaching channel user", id);
		c->dettach_user(c->self, NULL);
	}
	if (c->sock != -1)
		shutdown(c->sock, SHUT_RDWR);
	channel_close_fds(c);
	buffer_free(&c->input);
	buffer_free(&c->output);
	buffer_free(&c->extended);
	c->type = SSH_CHANNEL_FREE;
	if (c->remote_name) {
		xfree(c->remote_name);
		c->remote_name = NULL;
	}
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

void
channel_pre_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	FD_SET(c->sock, readset);
}

void
channel_pre_open_13(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->input) < packet_get_maxsize())
		FD_SET(c->sock, readset);
	if (buffer_len(&c->output) > 0)
		FD_SET(c->sock, writeset);
}

void
channel_pre_open_15(Channel *c, fd_set * readset, fd_set * writeset)
{
	/* test whether sockets are 'alive' for read/write */
	if (c->istate == CHAN_INPUT_OPEN)
		if (buffer_len(&c->input) < packet_get_maxsize())
			FD_SET(c->sock, readset);
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (buffer_len(&c->output) > 0) {
			FD_SET(c->sock, writeset);
		} else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
			chan_obuf_empty(c);
		}
	}
}

void
channel_pre_open_20(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (c->istate == CHAN_INPUT_OPEN &&
	    c->remote_window > 0 &&
	    buffer_len(&c->input) < c->remote_window)
		FD_SET(c->rfd, readset);
	if (c->ostate == CHAN_OUTPUT_OPEN ||
	    c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
		if (buffer_len(&c->output) > 0) {
			FD_SET(c->wfd, writeset);
		} else if (c->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
			chan_obuf_empty(c);
		}
	}
	/** XXX check close conditions, too */
	if (c->efd != -1) {
		if (c->extended_usage == CHAN_EXTENDED_WRITE &&
		    buffer_len(&c->extended) > 0)
			FD_SET(c->efd, writeset);
		else if (c->extended_usage == CHAN_EXTENDED_READ &&
		    buffer_len(&c->extended) < c->remote_window)
			FD_SET(c->efd, readset);
	}
}

void
channel_pre_input_draining(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->input) == 0) {
		packet_start(SSH_MSG_CHANNEL_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
		c->type = SSH_CHANNEL_CLOSED;
		debug("Closing channel %d after input drain.", c->self);
	}
}

void
channel_pre_output_draining(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (buffer_len(&c->output) == 0)
		channel_free(c->self);
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
 */
int
x11_open_helper(Channel *c)
{
	unsigned char *ucp;
	unsigned int proto_len, data_len;

	/* Check if the fixed size part of the packet is in buffer. */
	if (buffer_len(&c->output) < 12)
		return 0;

	/* Parse the lengths of variable-length fields. */
	ucp = (unsigned char *) buffer_ptr(&c->output);
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
	if (buffer_len(&c->output) <
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

void
channel_pre_x11_open_13(Channel *c, fd_set * readset, fd_set * writeset)
{
	int ret = x11_open_helper(c);
	if (ret == 1) {
		/* Start normal processing for the channel. */
		c->type = SSH_CHANNEL_OPEN;
		channel_pre_open_13(c, readset, writeset);
	} else if (ret == -1) {
		/*
		 * We have received an X11 connection that has bad
		 * authentication information.
		 */
		log("X11 connection rejected because of wrong authentication.\r\n");
		buffer_clear(&c->input);
		buffer_clear(&c->output);
		close(c->sock);
		c->sock = -1;
		c->type = SSH_CHANNEL_CLOSED;
		packet_start(SSH_MSG_CHANNEL_CLOSE);
		packet_put_int(c->remote_id);
		packet_send();
	}
}

void
channel_pre_x11_open(Channel *c, fd_set * readset, fd_set * writeset)
{
	int ret = x11_open_helper(c);
	if (ret == 1) {
		c->type = SSH_CHANNEL_OPEN;
		if (compat20)
			channel_pre_open_20(c, readset, writeset);
		else
			channel_pre_open_15(c, readset, writeset);
	} else if (ret == -1) {
		debug("X11 rejected %d i%d/o%d", c->self, c->istate, c->ostate);
		chan_read_failed(c);	/** force close? */
		chan_write_failed(c);
		debug("X11 closed %d i%d/o%d", c->self, c->istate, c->ostate);
	}
}

/* This is our fake X11 server socket. */
void
channel_post_x11_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	struct sockaddr addr;
	int newsock, newch;
	socklen_t addrlen;
	char buf[16384], *remote_hostname;
	int remote_port;

	if (FD_ISSET(c->sock, readset)) {
		debug("X11 connection requested.");
		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept: %.100s", strerror(errno));
			return;
		}
		remote_hostname = get_remote_hostname(newsock);
		remote_port = get_peer_port(newsock);
		snprintf(buf, sizeof buf, "X11 connection from %.200s port %d",
		    remote_hostname, remote_port);

		newch = channel_new("x11",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, xstrdup(buf));
		if (compat20) {
			packet_start(SSH2_MSG_CHANNEL_OPEN);
			packet_put_cstring("x11");
			packet_put_int(newch);
			packet_put_int(c->local_window_max);
			packet_put_int(c->local_maxpacket);
			/* originator host and port */
			packet_put_cstring(remote_hostname);
			if (datafellows & SSH_BUG_X11FWD) {
				debug("ssh2 x11 bug compat mode");
			} else {
				packet_put_int(remote_port);
			}
			packet_send();
		} else {
			packet_start(SSH_SMSG_X11_OPEN);
			packet_put_int(newch);
			if (have_hostname_in_open)
				packet_put_string(buf, strlen(buf));
			packet_send();
		}
		xfree(remote_hostname);
	}
}

/*
 * This socket is listening for connections to a forwarded TCP/IP port.
 */
void
channel_post_port_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	struct sockaddr addr;
	int newsock, newch;
	socklen_t addrlen;
	char buf[1024], *remote_hostname;
	int remote_port;

	if (FD_ISSET(c->sock, readset)) {
		debug("Connection to port %d forwarding "
		    "to %.100s port %d requested.",
		    c->listening_port, c->path, c->host_port);
		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept: %.100s", strerror(errno));
			return;
		}
		remote_hostname = get_remote_hostname(newsock);
		remote_port = get_peer_port(newsock);
		snprintf(buf, sizeof buf,
		    "listen port %d for %.100s port %d, "
		    "connect from %.200s port %d",
		    c->listening_port, c->path, c->host_port,
		    remote_hostname, remote_port);
		newch = channel_new("direct-tcpip",
		    SSH_CHANNEL_OPENING, newsock, newsock, -1,
		    c->local_window_max, c->local_maxpacket,
		    0, xstrdup(buf));
		if (compat20) {
			packet_start(SSH2_MSG_CHANNEL_OPEN);
			packet_put_cstring("direct-tcpip");
			packet_put_int(newch);
			packet_put_int(c->local_window_max);
			packet_put_int(c->local_maxpacket);
			/* target host and port */
			packet_put_string(c->path, strlen(c->path));
			packet_put_int(c->host_port);
			/* originator host and port */
			packet_put_cstring(remote_hostname);
			packet_put_int(remote_port);
			packet_send();
		} else {
			packet_start(SSH_MSG_PORT_OPEN);
			packet_put_int(newch);
			packet_put_string(c->path, strlen(c->path));
			packet_put_int(c->host_port);
			if (have_hostname_in_open) {
				packet_put_string(buf, strlen(buf));
			}
			packet_send();
		}
		xfree(remote_hostname);
	}
}

/*
 * This is the authentication agent socket listening for connections from
 * clients.
 */
void
channel_post_auth_listener(Channel *c, fd_set * readset, fd_set * writeset)
{
	struct sockaddr addr;
	int newsock, newch;
	socklen_t addrlen;

	if (FD_ISSET(c->sock, readset)) {
		addrlen = sizeof(addr);
		newsock = accept(c->sock, &addr, &addrlen);
		if (newsock < 0) {
			error("accept from auth socket: %.100s", strerror(errno));
			return;
		}
		newch = channel_allocate(SSH_CHANNEL_OPENING, newsock,
		    xstrdup("accepted auth socket"));
		packet_start(SSH_SMSG_AGENT_OPEN);
		packet_put_int(newch);
		packet_send();
	}
}

int
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
			if (compat13) {
				buffer_consume(&c->output, buffer_len(&c->output));
				c->type = SSH_CHANNEL_INPUT_DRAINING;
				debug("Channel %d status set to input draining.", c->self);
			} else {
				chan_read_failed(c);
			}
			return -1;
		}
		if(c->input_filter != NULL) {
			if (c->input_filter(c, buf, len) == -1) {
				debug("filter stops channel %d", c->self);
				chan_read_failed(c);
			}
		} else {
			buffer_append(&c->input, buf, len);
		}
	}
	return 1;
}
int
channel_handle_wfd(Channel *c, fd_set * readset, fd_set * writeset)
{
	int len;

	/* Send buffered output data to the socket. */
	if (c->wfd != -1 &&
	    FD_ISSET(c->wfd, writeset) &&
	    buffer_len(&c->output) > 0) {
		len = write(c->wfd, buffer_ptr(&c->output),
		    buffer_len(&c->output));
		if (len < 0 && (errno == EINTR || errno == EAGAIN))
			return 1;
		if (len <= 0) {
			if (compat13) {
				buffer_consume(&c->output, buffer_len(&c->output));
				debug("Channel %d status set to input draining.", c->self);
				c->type = SSH_CHANNEL_INPUT_DRAINING;
			} else {
				chan_write_failed(c);
			}
			return -1;
		}
		buffer_consume(&c->output, len);
		if (compat20 && len > 0) {
			c->local_consumed += len;
		}
	}
	return 1;
}
int
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
			debug("channel %d: written %d to efd %d",
			    c->self, len, c->efd);
			if (len > 0) {
				buffer_consume(&c->extended, len);
				c->local_consumed += len;
			}
		} else if (c->extended_usage == CHAN_EXTENDED_READ &&
		    FD_ISSET(c->efd, readset)) {
			len = read(c->efd, buf, sizeof(buf));
			debug("channel %d: read %d from efd %d",
			     c->self, len, c->efd);
			if (len == 0) {
				debug("channel %d: closing efd %d",
				    c->self, c->efd);
				close(c->efd);
				c->efd = -1;
			} else if (len > 0)
				buffer_append(&c->extended, buf, len);
		}
	}
	return 1;
}
int
channel_check_window(Channel *c, fd_set * readset, fd_set * writeset)
{
	if (!(c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD)) &&
	    c->local_window < c->local_window_max/2 &&
	    c->local_consumed > 0) {
		packet_start(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
		packet_put_int(c->remote_id);
		packet_put_int(c->local_consumed);
		packet_send();
		debug("channel %d: window %d sent adjust %d",
		    c->self, c->local_window,
		    c->local_consumed);
		c->local_window += c->local_consumed;
		c->local_consumed = 0;
	}
	return 1;
}

void
channel_post_open_1(Channel *c, fd_set * readset, fd_set * writeset)
{
	channel_handle_rfd(c, readset, writeset);
	channel_handle_wfd(c, readset, writeset);
}

void
channel_post_open_2(Channel *c, fd_set * readset, fd_set * writeset)
{
	channel_handle_rfd(c, readset, writeset);
	channel_handle_wfd(c, readset, writeset);
	channel_handle_efd(c, readset, writeset);
	channel_check_window(c, readset, writeset);
}

void
channel_post_output_drain_13(Channel *c, fd_set * readset, fd_set * writeset)
{
	int len;
	/* Send buffered output data to the socket. */
	if (FD_ISSET(c->sock, writeset) && buffer_len(&c->output) > 0) {
		len = write(c->sock, buffer_ptr(&c->output),
			    buffer_len(&c->output));
		if (len <= 0)
			buffer_consume(&c->output, buffer_len(&c->output));
		else
			buffer_consume(&c->output, len);
	}
}

void
channel_handler_init_20(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open_20;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;

	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open_2;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
}

void
channel_handler_init_13(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open_13;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open_13;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_INPUT_DRAINING] =	&channel_pre_input_draining;
	channel_pre[SSH_CHANNEL_OUTPUT_DRAINING] =	&channel_pre_output_draining;

	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open_1;
	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_OUTPUT_DRAINING] =	&channel_post_output_drain_13;
}

void
channel_handler_init_15(void)
{
	channel_pre[SSH_CHANNEL_OPEN] =			&channel_pre_open_15;
	channel_pre[SSH_CHANNEL_X11_OPEN] =		&channel_pre_x11_open;
	channel_pre[SSH_CHANNEL_X11_LISTENER] =		&channel_pre_listener;
	channel_pre[SSH_CHANNEL_PORT_LISTENER] =	&channel_pre_listener;
	channel_pre[SSH_CHANNEL_AUTH_SOCKET] =		&channel_pre_listener;

	channel_post[SSH_CHANNEL_X11_LISTENER] =	&channel_post_x11_listener;
	channel_post[SSH_CHANNEL_PORT_LISTENER] =	&channel_post_port_listener;
	channel_post[SSH_CHANNEL_AUTH_SOCKET] =		&channel_post_auth_listener;
	channel_post[SSH_CHANNEL_OPEN] =		&channel_post_open_1;
}

void
channel_handler_init(void)
{
	int i;
	for(i = 0; i < SSH_CHANNEL_MAX_TYPE; i++) {
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

void
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
		c = &channels[i];
		if (c->type == SSH_CHANNEL_FREE)
			continue;
		if (ftab[c->type] == NULL)
			continue;
		(*ftab[c->type])(c, readset, writeset);
		chan_delete_if_full_closed(c);
	}
}

void
channel_prepare_select(fd_set * readset, fd_set * writeset)
{
	channel_handler(channel_pre, readset, writeset);
}

void
channel_after_select(fd_set * readset, fd_set * writeset)
{
	channel_handler(channel_post, readset, writeset);
}

/* If there is data to send to the connection, send some of it now. */

void
channel_output_poll()
{
	int len, i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = &channels[i];

		/* We are only interested in channels that can have buffered incoming data. */
		if (compat13) {
			if (c->type != SSH_CHANNEL_OPEN &&
			    c->type != SSH_CHANNEL_INPUT_DRAINING)
				continue;
		} else {
			if (c->type != SSH_CHANNEL_OPEN)
				continue;
			if (c->istate != CHAN_INPUT_OPEN &&
			    c->istate != CHAN_INPUT_WAIT_DRAIN)
				continue;
		}
		if (compat20 &&
		    (c->flags & (CHAN_CLOSE_SENT|CHAN_CLOSE_RCVD))) {
			debug("channel: %d: no data after CLOSE", c->self);
			continue;
		}

		/* Get the amount of buffered data for this channel. */
		len = buffer_len(&c->input);
		if (len > 0) {
			/* Send some data for the other side over the secure connection. */
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
			 * tell peer, that we will not send more data: send IEOF
			 */
			chan_ibuf_empty(c);
		}
		/* Send extended data, i.e. stderr */
		if (compat20 &&
		    c->remote_window > 0 &&
		    (len = buffer_len(&c->extended)) > 0 &&
		    c->extended_usage == CHAN_EXTENDED_READ) {
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
		}
	}
}

/*
 * This is called when a packet of type CHANNEL_DATA has just been received.
 * The message type has already been consumed, but channel number and data is
 * still there.
 */

void
channel_input_data(int type, int plen)
{
	int id;
	char *data;
	unsigned int data_len;
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
	packet_done();

	if (compat20){
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
	}else{
		packet_integrity_check(plen, 4 + 4 + data_len, type);
	}
	buffer_append(&c->output, data, data_len);
	xfree(data);
}
void
channel_input_extended_data(int type, int plen)
{
	int id;
	int tcode;
	char *data;
	unsigned int data_len;
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
	tcode = packet_get_int();
	if (c->efd == -1 ||
	    c->extended_usage != CHAN_EXTENDED_WRITE ||
	    tcode != SSH2_EXTENDED_DATA_STDERR) {
		log("channel %d: bad ext data", c->self);
		return;
	}
	data = packet_get_string(&data_len);
	packet_done();
	if (data_len > c->local_window) {
		log("channel %d: rcvd too much extended_data %d, win %d",
		    c->self, data_len, c->local_window);
		xfree(data);
		return;
	}
	debug("channel %d: rcvd ext data %d", c->self, data_len);
	c->local_window -= data_len;
	buffer_append(&c->extended, data, data_len);
	xfree(data);
}


/*
 * Returns true if no channel has too much buffered data, and false if one or
 * more channel is overfull.
 */

int
channel_not_very_much_buffered_data()
{
	unsigned int i;
	Channel *c;

	for (i = 0; i < channels_alloc; i++) {
		c = &channels[i];
		if (c->type == SSH_CHANNEL_OPEN) {
			if (!compat20 && buffer_len(&c->input) > packet_get_maxsize()) {
				debug("channel %d: big input buffer %d",
				    c->self, buffer_len(&c->input));
				return 0;
			}
			if (buffer_len(&c->output) > packet_get_maxsize()) {
				debug("channel %d: big output buffer %d",
				    c->self, buffer_len(&c->output));
				return 0;
			}
		}
	}
	return 1;
}

void
channel_input_ieof(int type, int plen)
{
	int id;
	Channel *c;

	packet_integrity_check(plen, 4, type);

	id = packet_get_int();
	c = channel_lookup(id);
	if (c == NULL)
		packet_disconnect("Received ieof for nonexistent channel %d.", id);
	chan_rcvd_ieof(c);
}

void
channel_input_close(int type, int plen)
{
	int id;
	Channel *c;

	packet_integrity_check(plen, 4, type);

	id = packet_get_int();
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
		buffer_consume(&c->input, buffer_len(&c->input));
		c->type = SSH_CHANNEL_OUTPUT_DRAINING;
	}
}

/* proto version 1.5 overloads CLOSE_CONFIRMATION with OCLOSE */
void
channel_input_oclose(int type, int plen)
{
	int id = packet_get_int();
	Channel *c = channel_lookup(id);
	packet_integrity_check(plen, 4, type);
	if (c == NULL)
		packet_disconnect("Received oclose for nonexistent channel %d.", id);
	chan_rcvd_oclose(c);
}

void
channel_input_close_confirmation(int type, int plen)
{
	int id = packet_get_int();
	Channel *c = channel_lookup(id);

	packet_done();
	if (c == NULL)
		packet_disconnect("Received close confirmation for "
		    "out-of-range channel %d.", id);
	if (c->type != SSH_CHANNEL_CLOSED)
		packet_disconnect("Received close confirmation for "
		    "non-closed channel %d (type %d).", id, c->type);
	channel_free(c->self);
}

void
channel_input_open_confirmation(int type, int plen)
{
	int id, remote_id;
	Channel *c;

	if (!compat20)
		packet_integrity_check(plen, 4 + 4, type);

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
		packet_done();
		if (c->cb_fn != NULL && c->cb_event == type) {
			debug("callback start");
			c->cb_fn(c->self, c->cb_arg);
			debug("callback done");
		}
		debug("channel %d: open confirm rwindow %d rmax %d", c->self,
		    c->remote_window, c->remote_maxpacket);
	}
}

void
channel_input_open_failure(int type, int plen)
{
	int id;
	Channel *c;

	if (!compat20)
		packet_integrity_check(plen, 4, type);

	id = packet_get_int();
	c = channel_lookup(id);

	if (c==NULL || c->type != SSH_CHANNEL_OPENING)
		packet_disconnect("Received open failure for "
		    "non-opening channel %d.", id);
	if (compat20) {
		int reason = packet_get_int();
		char *msg  = packet_get_string(NULL);
		char *lang  = packet_get_string(NULL);
		log("channel_open_failure: %d: reason %d: %s", id, reason, msg);
		packet_done();
		xfree(msg);
		xfree(lang);
	}
	/* Free the channel.  This will also close the socket. */
	channel_free(id);
}

void
channel_input_channel_request(int type, int plen)
{
	int id;
	Channel *c;

	id = packet_get_int();
	c = channel_lookup(id);

	if (c == NULL ||
	    (c->type != SSH_CHANNEL_OPEN && c->type != SSH_CHANNEL_LARVAL))
		packet_disconnect("Received request for "
		    "non-open channel %d.", id);
	if (c->cb_fn != NULL && c->cb_event == type) {
		debug("callback start");
		c->cb_fn(c->self, c->cb_arg);
		debug("callback done");
	} else {
		char *service = packet_get_string(NULL);
		debug("channel: %d rcvd request for %s", c->self, service);
debug("cb_fn %p cb_event %d", c->cb_fn , c->cb_event);
		xfree(service);
	}
}

void
channel_input_window_adjust(int type, int plen)
{
	Channel *c;
	int id, adjust;

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
	packet_done();
	debug("channel %d: rcvd adjust %d", id, adjust);
	c->remote_window += adjust;
}

/*
 * Stops listening for channels, and removes any unix domain sockets that we
 * might have.
 */

void
channel_stop_listening()
{
	int i;
	for (i = 0; i < channels_alloc; i++) {
		switch (channels[i].type) {
		case SSH_CHANNEL_AUTH_SOCKET:
			close(channels[i].sock);
			remove(channels[i].path);
			channel_free(i);
			break;
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_X11_LISTENER:
			close(channels[i].sock);
			channel_free(i);
			break;
		default:
			break;
		}
	}
}

/*
 * Closes the sockets/fds of all channels.  This is used to close extra file
 * descriptors after a fork.
 */

void
channel_close_all()
{
	int i;
	for (i = 0; i < channels_alloc; i++)
		if (channels[i].type != SSH_CHANNEL_FREE)
			channel_close_fds(&channels[i]);
}

/* Returns the maximum file descriptor number used by the channels. */

int
channel_max_fd()
{
	return channel_max_fd_value;
}

/* Returns true if any channel is still open. */

int
channel_still_open()
{
	unsigned int i;
	for (i = 0; i < channels_alloc; i++)
		switch (channels[i].type) {
		case SSH_CHANNEL_FREE:
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_AUTH_SOCKET:
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
			fatal("channel_still_open: bad channel type %d", channels[i].type);
			/* NOTREACHED */
		}
	return 0;
}

/*
 * Returns a message describing the currently open forwarded connections,
 * suitable for sending to the client.  The message contains crlf pairs for
 * newlines.
 */

char *
channel_open_message()
{
	Buffer buffer;
	int i;
	char buf[512], *cp;

	buffer_init(&buffer);
	snprintf(buf, sizeof buf, "The following connections are open:\r\n");
	buffer_append(&buffer, buf, strlen(buf));
	for (i = 0; i < channels_alloc; i++) {
		Channel *c = &channels[i];
		switch (c->type) {
		case SSH_CHANNEL_FREE:
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_CLOSED:
		case SSH_CHANNEL_AUTH_SOCKET:
			continue;
		case SSH_CHANNEL_LARVAL:
		case SSH_CHANNEL_OPENING:
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

/*
 * Initiate forwarding of connections to local port "port" through the secure
 * channel to host:port from remote side.
 */

void
channel_request_local_forwarding(u_short port, const char *host,
				 u_short host_port, int gateway_ports)
{
	int success, ch, sock, on = 1;
	struct addrinfo hints, *ai, *aitop;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	struct linger linger;

	if (strlen(host) > sizeof(channels[0].path) - 1)
		packet_disconnect("Forward host name too long.");

	/*
	 * getaddrinfo returns a loopback address if the hostname is
	 * set to NULL and hints.ai_flags is not AI_PASSIVE
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_flags = gateway_ports ? AI_PASSIVE : 0;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", port);
	if (getaddrinfo(NULL, strport, &hints, &aitop) != 0)
		packet_disconnect("getaddrinfo: fatal error");

	success = 0;
	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("channel_request_local_forwarding: getnameinfo failed");
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
		 * Set socket options.  We would like the socket to disappear
		 * as soon as it has been closed for whatever reason.
		 */
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));
		linger.l_onoff = 1;
		linger.l_linger = 5;
		setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *)&linger, sizeof(linger));
		debug("Local forwarding listening on %s port %s.", ntop, strport);

		/* Bind the socket to the address. */
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			/* address can be in use ipv6 address is already bound */
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
		ch = channel_new(
		    "port listener", SSH_CHANNEL_PORT_LISTENER,
		    sock, sock, -1,
		    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT,
		    0, xstrdup("port listener"));
		strlcpy(channels[ch].path, host, sizeof(channels[ch].path));
		channels[ch].host_port = host_port;
		channels[ch].listening_port = port;
		success = 1;
	}
	if (success == 0)
		packet_disconnect("cannot listen port: %d", port);
	freeaddrinfo(aitop);
}

/*
 * Initiate forwarding of connections to port "port" on remote host through
 * the secure channel to host:port from local side.
 */

void
channel_request_remote_forwarding(u_short listen_port, const char *host_to_connect,
				  u_short port_to_connect)
{
	int payload_len;
	/* Record locally that connection to this host/port is permitted. */
	if (num_permitted_opens >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("channel_request_remote_forwarding: too many forwards");

	permitted_opens[num_permitted_opens].host_to_connect = xstrdup(host_to_connect);
	permitted_opens[num_permitted_opens].port_to_connect = port_to_connect;
	permitted_opens[num_permitted_opens].listen_port = listen_port;
	num_permitted_opens++;

	/* Send the forward request to the remote side. */
	if (compat20) {
		const char *address_to_bind = "0.0.0.0";
		packet_start(SSH2_MSG_GLOBAL_REQUEST);
		packet_put_cstring("tcpip-forward");
		packet_put_char(0);			/* boolean: want reply */
		packet_put_cstring(address_to_bind);
		packet_put_int(listen_port);
	} else {
		packet_start(SSH_CMSG_PORT_FORWARD_REQUEST);
		packet_put_int(listen_port);
		packet_put_cstring(host_to_connect);
		packet_put_int(port_to_connect);
		packet_send();
		packet_write_wait();
		/*
		 * Wait for response from the remote side.  It will send a disconnect
		 * message on failure, and we will never see it here.
		 */
		packet_read_expect(&payload_len, SSH_SMSG_SUCCESS);
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

	/*
	 * Check that an unprivileged user is not trying to forward a
	 * privileged port.
	 */
	if (port < IPPORT_RESERVED && !is_root)
		packet_disconnect("Requested forwarding of port %d but user is not root.",
				  port);
	/*
	 * Initiate forwarding,
	 */
	channel_request_local_forwarding(port, hostname, host_port, gateway_ports);

	/* Free the argument string. */
	xfree(hostname);
}

/* XXX move to aux.c */
int
channel_connect_to(const char *host, u_short host_port)
{
	struct addrinfo hints, *ai, *aitop;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	int gaierr;
	int sock = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", host_port);
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0) {
		error("%.100s: unknown host (%s)", host, gai_strerror(gaierr));
		return -1;
	}
	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("channel_connect_to: getnameinfo failed");
			continue;
		}
		/* Create the socket. */
		sock = socket(ai->ai_family, SOCK_STREAM, 0);
		if (sock < 0) {
			error("socket: %.100s", strerror(errno));
			continue;
		}
		/* Connect to the host/port. */
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			error("connect %.100s port %s: %.100s", ntop, strport,
			    strerror(errno));
			close(sock);
			continue;	/* fail -- try next */	
		}
		break; /* success */

	}
	freeaddrinfo(aitop);
	if (!ai) {
		error("connect %.100s port %d: failed.", host, host_port);	
		return -1;
	}
	/* success */
	return sock;
}
/*
 * This is called after receiving PORT_OPEN message.  This attempts to
 * connect to the given host:port, and sends back CHANNEL_OPEN_CONFIRMATION
 * or CHANNEL_OPEN_FAILURE.
 */

void
channel_input_port_open(int type, int plen)
{
	u_short host_port;
	char *host, *originator_string;
	int remote_channel, sock = -1, newch, i, denied;
	unsigned int host_len, originator_len;

	/* Get remote channel number. */
	remote_channel = packet_get_int();

	/* Get host name to connect to. */
	host = packet_get_string(&host_len);

	/* Get port to connect to. */
	host_port = packet_get_int();

	/* Get remote originator name. */
	if (have_hostname_in_open) {
		originator_string = packet_get_string(&originator_len);
		originator_len += 4;	/* size of packet_int */
	} else {
		originator_string = xstrdup("unknown (remote did not supply name)");
		originator_len = 0;	/* no originator supplied */
	}

	packet_integrity_check(plen,
	    4 + 4 + host_len + 4 + originator_len, SSH_MSG_PORT_OPEN);

	/* Check if opening that port is permitted. */
	denied = 0;
	if (!all_opens_permitted) {
		/* Go trough all permitted ports. */
		for (i = 0; i < num_permitted_opens; i++)
			if (permitted_opens[i].port_to_connect == host_port &&
			    strcmp(permitted_opens[i].host_to_connect, host) == 0)
				break;

		/* Check if we found the requested port among those permitted. */
		if (i >= num_permitted_opens) {
			/* The port is not permitted. */
			log("Received request to connect to %.100s:%d, but the request was denied.",
			    host, host_port);
			denied = 1;
		}
	}
	sock = denied ? -1 : channel_connect_to(host, host_port);
	if (sock > 0) {
		/* Allocate a channel for this connection. */
		newch = channel_allocate(SSH_CHANNEL_OPEN, sock, originator_string);
		channels[newch].remote_id = remote_channel;

		packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(remote_channel);
		packet_put_int(newch);
		packet_send();
	} else {
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_channel);
		packet_send();
	}
	xfree(host);
}

/*
 * Creates an internet domain socket for listening for X11 connections.
 * Returns a suitable value for the DISPLAY variable, or NULL if an error
 * occurs.
 */

#define	NUM_SOCKS	10

char *
x11_create_display_inet(int screen_number, int x11_display_offset)
{
	int display_number, sock;
	u_short port;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr, n, num_socks = 0, socks[NUM_SOCKS];
	char display[512];
	char hostname[MAXHOSTNAMELEN];

	for (display_number = x11_display_offset;
	     display_number < MAX_DISPLAYS;
	     display_number++) {
		port = 6000 + display_number;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = IPv4or6;
		hints.ai_flags = AI_PASSIVE;		/* XXX loopback only ? */
		hints.ai_socktype = SOCK_STREAM;
		snprintf(strport, sizeof strport, "%d", port);
		if ((gaierr = getaddrinfo(NULL, strport, &hints, &aitop)) != 0) {
			error("getaddrinfo: %.100s", gai_strerror(gaierr));
			return NULL;
		}
		for (ai = aitop; ai; ai = ai->ai_next) {
			if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
				continue;
			sock = socket(ai->ai_family, SOCK_STREAM, 0);
			if (sock < 0) {
				error("socket: %.100s", strerror(errno));
				return NULL;
			}
			if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
				debug("bind port %d: %.100s", port, strerror(errno));
				shutdown(sock, SHUT_RDWR);
				close(sock);
				for (n = 0; n < num_socks; n++) {
					shutdown(socks[n], SHUT_RDWR);
					close(socks[n]);
				}
				num_socks = 0;
				break;
			}
			socks[num_socks++] = sock;
			if (num_socks == NUM_SOCKS)
				break;
		}
		if (num_socks > 0)
			break;
	}
	if (display_number >= MAX_DISPLAYS) {
		error("Failed to allocate internet-domain X11 display socket.");
		return NULL;
	}
	/* Start listening for connections on the socket. */
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		if (listen(sock, 5) < 0) {
			error("listen: %.100s", strerror(errno));
			shutdown(sock, SHUT_RDWR);
			close(sock);
			return NULL;
		}
	}

	/* Set up a suitable value for the DISPLAY variable. */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		fatal("gethostname: %.100s", strerror(errno));
	snprintf(display, sizeof display, "%.400s:%d.%d", hostname,
		 display_number, screen_number);

	/* Allocate a channel for each socket. */
	for (n = 0; n < num_socks; n++) {
		sock = socks[n];
		(void) channel_new("x11 listener",
		    SSH_CHANNEL_X11_LISTENER, sock, sock, -1,
		    CHAN_X11_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT,
		    0, xstrdup("X11 inet listener"));
	}

	/* Return a suitable value for the DISPLAY environment variable. */
	return xstrdup(display);
}

#ifndef X_UNIX_PATH
#define X_UNIX_PATH "/tmp/.X11-unix/X"
#endif

static
int
connect_local_xsocket(unsigned int dnr)
{
	static const char *const x_sockets[] = {
		X_UNIX_PATH "%u",
		"/var/X/.X11-unix/X" "%u",
		"/usr/spool/sockets/X11/" "%u",
		NULL
	};
	int sock;
	struct sockaddr_un addr;
	const char *const * path;

	for (path = x_sockets; *path; ++path) {
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock < 0)
			error("socket: %.100s", strerror(errno));
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		snprintf(addr.sun_path, sizeof addr.sun_path, *path, dnr);
		if (connect(sock, (struct sockaddr *) & addr, sizeof(addr)) == 0)
			return sock;
		close(sock);
	}
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
	strncpy(buf, display, sizeof(buf));
	buf[sizeof(buf) - 1] = 0;
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
	return sock;
}

/*
 * This is called when SSH_SMSG_X11_OPEN is received.  The packet contains
 * the remote channel number.  We should do whatever we want, and respond
 * with either SSH_MSG_OPEN_CONFIRMATION or SSH_MSG_OPEN_FAILURE.
 */

void
x11_input_open(int type, int plen)
{
	int remote_channel, sock = 0, newch;
	char *remote_host;
	unsigned int remote_len;

	/* Get remote channel number. */
	remote_channel = packet_get_int();

	/* Get remote originator name. */
	if (have_hostname_in_open) {
		remote_host = packet_get_string(&remote_len);
		remote_len += 4;
	} else {
		remote_host = xstrdup("unknown (remote did not supply name)");
		remote_len = 0;
	}

	debug("Received X11 open request.");
	packet_integrity_check(plen, 4 + remote_len, SSH_SMSG_X11_OPEN);

	/* Obtain a connection to the real X display. */
	sock = x11_connect_display();
	if (sock == -1) {
		/* Send refusal to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remote_channel);
		packet_send();
	} else {
		/* Allocate a channel for this connection. */
		newch = channel_allocate(
		     (x11_saved_proto == NULL) ?
		     SSH_CHANNEL_OPEN : SSH_CHANNEL_X11_OPEN,
		     sock, remote_host);
		channels[newch].remote_id = remote_channel;

		/* Send a confirmation to the remote host. */
		packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(remote_channel);
		packet_put_int(newch);
		packet_send();
	}
}

/*
 * Requests forwarding of X11 connections, generates fake authentication
 * data, and enables authentication spoofing.
 */

void
x11_request_forwarding_with_spoofing(int client_session_id,
    const char *proto, const char *data)
{
	unsigned int data_len = (unsigned int) strlen(data) / 2;
	unsigned int i, value;
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
	new_data = xmalloc(2 * data_len + 1);
	for (i = 0; i < data_len; i++)
		sprintf(new_data + 2 * i, "%02x", (unsigned char) x11_fake_data[i]);

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

/* Sends a message to the server to request authentication fd forwarding. */

void
auth_request_forwarding()
{
	packet_start(SSH_CMSG_AGENT_REQUEST_FORWARDING);
	packet_send();
	packet_write_wait();
}

/*
 * Returns the name of the forwarded authentication socket.  Returns NULL if
 * there is no forwarded authentication socket.  The returned value points to
 * a static buffer.
 */

char *
auth_get_socket_name()
{
	return channel_forwarded_auth_socket_name;
}

/* removes the agent forwarding socket */

void
cleanup_socket(void)
{
	remove(channel_forwarded_auth_socket_name);
	rmdir(channel_forwarded_auth_socket_dir);
}

/*
 * This is called to process SSH_CMSG_AGENT_REQUEST_FORWARDING on the server.
 * This starts forwarding authentication requests.
 */

int
auth_input_request_forwarding(struct passwd * pw)
{
	int sock, newch;
	struct sockaddr_un sunaddr;

	if (auth_get_socket_name() != NULL)
		fatal("Protocol error: authentication forwarding requested twice.");

	/* Temporarily drop privileged uid for mkdir/bind. */
	temporarily_use_uid(pw->pw_uid);

	/* Allocate a buffer for the socket name, and format the name. */
	channel_forwarded_auth_socket_name = xmalloc(MAX_SOCKET_NAME);
	channel_forwarded_auth_socket_dir = xmalloc(MAX_SOCKET_NAME);
	strlcpy(channel_forwarded_auth_socket_dir, "/tmp/ssh-XXXXXXXX", MAX_SOCKET_NAME);

	/* Create private directory for socket */
	if (mkdtemp(channel_forwarded_auth_socket_dir) == NULL) {
		packet_send_debug("Agent forwarding disabled: mkdtemp() failed: %.100s",
		    strerror(errno));
		restore_uid();
		xfree(channel_forwarded_auth_socket_name);
		xfree(channel_forwarded_auth_socket_dir);
		channel_forwarded_auth_socket_name = NULL;
		channel_forwarded_auth_socket_dir = NULL;
		return 0;
	}
	snprintf(channel_forwarded_auth_socket_name, MAX_SOCKET_NAME, "%s/agent.%d",
		 channel_forwarded_auth_socket_dir, (int) getpid());

	if (atexit(cleanup_socket) < 0) {
		int saved = errno;
		cleanup_socket();
		packet_disconnect("socket: %.100s", strerror(saved));
	}
	/* Create the socket. */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		packet_disconnect("socket: %.100s", strerror(errno));

	/* Bind it to the name. */
	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	strncpy(sunaddr.sun_path, channel_forwarded_auth_socket_name,
		sizeof(sunaddr.sun_path));

	if (bind(sock, (struct sockaddr *) & sunaddr, sizeof(sunaddr)) < 0)
		packet_disconnect("bind: %.100s", strerror(errno));

	/* Restore the privileged uid. */
	restore_uid();

	/* Start listening on the socket. */
	if (listen(sock, 5) < 0)
		packet_disconnect("listen: %.100s", strerror(errno));

	/* Allocate a channel for the authentication agent socket. */
	newch = channel_allocate(SSH_CHANNEL_AUTH_SOCKET, sock,
				 xstrdup("auth socket"));
	strlcpy(channels[newch].path, channel_forwarded_auth_socket_name,
	    sizeof(channels[newch].path));
	return 1;
}

/* This is called to process an SSH_SMSG_AGENT_OPEN message. */

void
auth_input_open_request(int type, int plen)
{
	int remch, sock, newch;
	char *dummyname;

	packet_integrity_check(plen, 4, type);

	/* Read the remote channel number from the message. */
	remch = packet_get_int();

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
	if (sock < 0) {
		packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(remch);
		packet_send();
		return;
	}
	debug("Forwarding authentication connection.");

	/*
	 * Dummy host name.  This will be freed when the channel is freed; it
	 * will still be valid in the packet_put_string below since the
	 * channel cannot yet be freed at that point.
	 */
	dummyname = xstrdup("authentication agent connection");

	newch = channel_allocate(SSH_CHANNEL_OPEN, sock, dummyname);
	channels[newch].remote_id = remch;

	/* Send a confirmation to the remote host. */
	packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
	packet_put_int(remch);
	packet_put_int(newch);
	packet_send();
}

void
channel_start_open(int id)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_open: %d: bad id", id);
		return;
	}
	debug("send channel open %d", id);
	packet_start(SSH2_MSG_CHANNEL_OPEN);
	packet_put_cstring(c->ctype);
	packet_put_int(c->self);
	packet_put_int(c->local_window);
	packet_put_int(c->local_maxpacket);
}
void
channel_open(int id)
{
	/* XXX REMOVE ME */
	channel_start_open(id);
	packet_send();
}
void
channel_request(int id, char *service, int wantconfirm)
{
	channel_request_start(id, service, wantconfirm);
	packet_send();
	debug("channel request %d: %s", id, service) ;
}
void
channel_request_start(int id, char *service, int wantconfirm)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_request: %d: bad id", id);
		return;
	}
	packet_start(SSH2_MSG_CHANNEL_REQUEST);
	packet_put_int(c->remote_id);
	packet_put_cstring(service);
	packet_put_char(wantconfirm);
}
void
channel_register_callback(int id, int mtype, channel_callback_fn *fn, void *arg)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_register_callback: %d: bad id", id);
		return;
	}
	c->cb_event = mtype;
	c->cb_fn = fn;
	c->cb_arg = arg;
}
void
channel_register_cleanup(int id, channel_callback_fn *fn)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_register_cleanup: %d: bad id", id);
		return;
	}
	c->dettach_user = fn;
}
void
channel_cancel_cleanup(int id)
{
	Channel *c = channel_lookup(id);
	if (c == NULL) {
		log("channel_cancel_cleanup: %d: bad id", id);
		return;
	}
	c->dettach_user = NULL;
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
channel_set_fds(int id, int rfd, int wfd, int efd, int extusage)
{
	Channel *c = channel_lookup(id);
	if (c == NULL || c->type != SSH_CHANNEL_LARVAL)
		fatal("channel_activate for non-larval channel %d.", id);

	channel_register_fds(c, rfd, wfd, efd, extusage);
	c->type = SSH_CHANNEL_OPEN;
	/* XXX window size? */
	c->local_window = c->local_window_max = c->local_maxpacket * 2;
	packet_start(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
	packet_put_int(c->remote_id);
	packet_put_int(c->local_window);
	packet_send();
}
