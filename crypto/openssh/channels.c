/*
 * 
 * channels.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Fri Mar 24 16:35:24 1995 ylo
 * 
 * This file contains functions for generic socket connection forwarding.
 * There is also code for initiating connection forwarding for X11 connections,
 * arbitrary tcp/ip connections, and the authentication agent connection.
 * 
 */

#include "includes.h"
RCSID("$Id: channels.c,v 1.38 2000/01/24 20:37:29 markus Exp $");

#include "ssh.h"
#include "packet.h"
#include "xmalloc.h"
#include "buffer.h"
#include "authfd.h"
#include "uidswap.h"
#include "readconf.h"
#include "servconf.h"

#include "channels.h"
#include "nchan.h"
#include "compat.h"

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
	char *host;		/* Host name. */
	u_short port;		/* Port number. */
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

/*
 * Allocate a new channel object and set its type and socket. This will cause
 * remote_name to be freed.
 */

int 
channel_allocate(int type, int sock, char *remote_name)
{
	int i, found;
	Channel *c;

	/* Update the maximum file descriptor value. */
	if (sock > channel_max_fd_value)
		channel_max_fd_value = sock;
	/* XXX set close-on-exec -markus */

	/* Do initial allocation if this is the first call. */
	if (channels_alloc == 0) {
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
	chan_init_iostates(c);
	c->self = found;
	c->type = type;
	c->sock = sock;
	c->remote_id = -1;
	c->remote_name = remote_name;
	debug("channel %d: new [%s]", found, remote_name);
	return found;
}

/* Free the channel and close its socket. */

void 
channel_free(int channel)
{
	if (channel < 0 || channel >= channels_alloc ||
	    channels[channel].type == SSH_CHANNEL_FREE)
		packet_disconnect("channel free: bad local channel %d", channel);

	if (compat13)
		shutdown(channels[channel].sock, SHUT_RDWR);
	close(channels[channel].sock);
	buffer_free(&channels[channel].input);
	buffer_free(&channels[channel].output);
	channels[channel].type = SSH_CHANNEL_FREE;
	if (channels[channel].remote_name) {
		xfree(channels[channel].remote_name);
		channels[channel].remote_name = NULL;
	}
}

/*
 * This is called just before select() to add any bits relevant to channels
 * in the select bitmasks.
 */

void 
channel_prepare_select(fd_set * readset, fd_set * writeset)
{
	int i;
	Channel *ch;
	unsigned char *ucp;
	unsigned int proto_len, data_len;

	for (i = 0; i < channels_alloc; i++) {
		ch = &channels[i];
redo:
		switch (ch->type) {
		case SSH_CHANNEL_X11_LISTENER:
		case SSH_CHANNEL_PORT_LISTENER:
		case SSH_CHANNEL_AUTH_SOCKET:
			FD_SET(ch->sock, readset);
			break;

		case SSH_CHANNEL_OPEN:
			if (compat13) {
				if (buffer_len(&ch->input) < packet_get_maxsize())
					FD_SET(ch->sock, readset);
				if (buffer_len(&ch->output) > 0)
					FD_SET(ch->sock, writeset);
				break;
			}
			/* test whether sockets are 'alive' for read/write */
			if (ch->istate == CHAN_INPUT_OPEN)
				if (buffer_len(&ch->input) < packet_get_maxsize())
					FD_SET(ch->sock, readset);
			if (ch->ostate == CHAN_OUTPUT_OPEN ||
			    ch->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
				if (buffer_len(&ch->output) > 0) {
					FD_SET(ch->sock, writeset);
				} else if (ch->ostate == CHAN_OUTPUT_WAIT_DRAIN) {
					chan_obuf_empty(ch);
				}
			}
			break;

		case SSH_CHANNEL_INPUT_DRAINING:
			if (!compat13)
				fatal("cannot happen: IN_DRAIN");
			if (buffer_len(&ch->input) == 0) {
				packet_start(SSH_MSG_CHANNEL_CLOSE);
				packet_put_int(ch->remote_id);
				packet_send();
				ch->type = SSH_CHANNEL_CLOSED;
				debug("Closing channel %d after input drain.", ch->self);
				break;
			}
			break;

		case SSH_CHANNEL_OUTPUT_DRAINING:
			if (!compat13)
				fatal("cannot happen: OUT_DRAIN");
			if (buffer_len(&ch->output) == 0) {
				channel_free(i);
				break;
			}
			FD_SET(ch->sock, writeset);
			break;

		case SSH_CHANNEL_X11_OPEN:
			/*
			 * This is a special state for X11 authentication
			 * spoofing.  An opened X11 connection (when
			 * authentication spoofing is being done) remains in
			 * this state until the first packet has been
			 * completely read.  The authentication data in that
			 * packet is then substituted by the real data if it
			 * matches the fake data, and the channel is put into
			 * normal mode.
			 */
			/* Check if the fixed size part of the packet is in buffer. */
			if (buffer_len(&ch->output) < 12)
				break;

			/* Parse the lengths of variable-length fields. */
			ucp = (unsigned char *) buffer_ptr(&ch->output);
			if (ucp[0] == 0x42) {	/* Byte order MSB first. */
				proto_len = 256 * ucp[6] + ucp[7];
				data_len = 256 * ucp[8] + ucp[9];
			} else if (ucp[0] == 0x6c) {	/* Byte order LSB first. */
				proto_len = ucp[6] + 256 * ucp[7];
				data_len = ucp[8] + 256 * ucp[9];
			} else {
				debug("Initial X11 packet contains bad byte order byte: 0x%x",
				      ucp[0]);
				ch->type = SSH_CHANNEL_OPEN;
				goto reject;
			}

			/* Check if the whole packet is in buffer. */
			if (buffer_len(&ch->output) <
			    12 + ((proto_len + 3) & ~3) + ((data_len + 3) & ~3))
				break;

			/* Check if authentication protocol matches. */
			if (proto_len != strlen(x11_saved_proto) ||
			    memcmp(ucp + 12, x11_saved_proto, proto_len) != 0) {
				debug("X11 connection uses different authentication protocol.");
				ch->type = SSH_CHANNEL_OPEN;
				goto reject;
			}
			/* Check if authentication data matches our fake data. */
			if (data_len != x11_fake_data_len ||
			    memcmp(ucp + 12 + ((proto_len + 3) & ~3),
				x11_fake_data, x11_fake_data_len) != 0) {
				debug("X11 auth data does not match fake data.");
				ch->type = SSH_CHANNEL_OPEN;
				goto reject;
			}
			/* Check fake data length */
			if (x11_fake_data_len != x11_saved_data_len) {
				error("X11 fake_data_len %d != saved_data_len %d",
				  x11_fake_data_len, x11_saved_data_len);
				ch->type = SSH_CHANNEL_OPEN;
				goto reject;
			}
			/*
			 * Received authentication protocol and data match
			 * our fake data. Substitute the fake data with real
			 * data.
			 */
			memcpy(ucp + 12 + ((proto_len + 3) & ~3),
			       x11_saved_data, x11_saved_data_len);

			/* Start normal processing for the channel. */
			ch->type = SSH_CHANNEL_OPEN;
			goto redo;

	reject:
			/*
			 * We have received an X11 connection that has bad
			 * authentication information.
			 */
			log("X11 connection rejected because of wrong authentication.\r\n");
			buffer_clear(&ch->input);
			buffer_clear(&ch->output);
			if (compat13) {
				close(ch->sock);
				ch->sock = -1;
				ch->type = SSH_CHANNEL_CLOSED;
				packet_start(SSH_MSG_CHANNEL_CLOSE);
				packet_put_int(ch->remote_id);
				packet_send();
			} else {
				debug("X11 rejected %d i%d/o%d", ch->self, ch->istate, ch->ostate);
				chan_read_failed(ch);
				chan_write_failed(ch);
				debug("X11 rejected %d i%d/o%d", ch->self, ch->istate, ch->ostate);
			}
			break;

		case SSH_CHANNEL_FREE:
		default:
			continue;
		}
	}
}

/*
 * After select, perform any appropriate operations for channels which have
 * events pending.
 */

void 
channel_after_select(fd_set * readset, fd_set * writeset)
{
	struct sockaddr addr;
	int newsock, i, newch, len;
	socklen_t addrlen;
	Channel *ch;
	char buf[16384], *remote_hostname;

	/* Loop over all channels... */
	for (i = 0; i < channels_alloc; i++) {
		ch = &channels[i];
		switch (ch->type) {
		case SSH_CHANNEL_X11_LISTENER:
			/* This is our fake X11 server socket. */
			if (FD_ISSET(ch->sock, readset)) {
				debug("X11 connection requested.");
				addrlen = sizeof(addr);
				newsock = accept(ch->sock, &addr, &addrlen);
				if (newsock < 0) {
					error("accept: %.100s", strerror(errno));
					break;
				}
				remote_hostname = get_remote_hostname(newsock);
				snprintf(buf, sizeof buf, "X11 connection from %.200s port %d",
				remote_hostname, get_peer_port(newsock));
				xfree(remote_hostname);
				newch = channel_allocate(SSH_CHANNEL_OPENING, newsock,
							 xstrdup(buf));
				packet_start(SSH_SMSG_X11_OPEN);
				packet_put_int(newch);
				if (have_hostname_in_open)
					packet_put_string(buf, strlen(buf));
				packet_send();
			}
			break;

		case SSH_CHANNEL_PORT_LISTENER:
			/*
			 * This socket is listening for connections to a
			 * forwarded TCP/IP port.
			 */
			if (FD_ISSET(ch->sock, readset)) {
				debug("Connection to port %d forwarding to %.100s port %d requested.",
				      ch->listening_port, ch->path, ch->host_port);
				addrlen = sizeof(addr);
				newsock = accept(ch->sock, &addr, &addrlen);
				if (newsock < 0) {
					error("accept: %.100s", strerror(errno));
					break;
				}
				remote_hostname = get_remote_hostname(newsock);
				snprintf(buf, sizeof buf, "listen port %d for %.100s port %d, connect from %.200s port %d",
					 ch->listening_port, ch->path, ch->host_port,
				remote_hostname, get_peer_port(newsock));
				xfree(remote_hostname);
				newch = channel_allocate(SSH_CHANNEL_OPENING, newsock,
							 xstrdup(buf));
				packet_start(SSH_MSG_PORT_OPEN);
				packet_put_int(newch);
				packet_put_string(ch->path, strlen(ch->path));
				packet_put_int(ch->host_port);
				if (have_hostname_in_open)
					packet_put_string(buf, strlen(buf));
				packet_send();
			}
			break;

		case SSH_CHANNEL_AUTH_SOCKET:
			/*
			 * This is the authentication agent socket listening
			 * for connections from clients.
			 */
			if (FD_ISSET(ch->sock, readset)) {
				addrlen = sizeof(addr);
				newsock = accept(ch->sock, &addr, &addrlen);
				if (newsock < 0) {
					error("accept from auth socket: %.100s", strerror(errno));
					break;
				}
				newch = channel_allocate(SSH_CHANNEL_OPENING, newsock,
					xstrdup("accepted auth socket"));
				packet_start(SSH_SMSG_AGENT_OPEN);
				packet_put_int(newch);
				packet_send();
			}
			break;

		case SSH_CHANNEL_OPEN:
			/*
			 * This is an open two-way communication channel. It
			 * is not of interest to us at this point what kind
			 * of data is being transmitted.
			 */

			/*
			 * Read available incoming data and append it to
			 * buffer; shutdown socket, if read or write failes
			 */
			if (FD_ISSET(ch->sock, readset)) {
				len = read(ch->sock, buf, sizeof(buf));
				if (len <= 0) {
					if (compat13) {
						buffer_consume(&ch->output, buffer_len(&ch->output));
						ch->type = SSH_CHANNEL_INPUT_DRAINING;
						debug("Channel %d status set to input draining.", i);
					} else {
						chan_read_failed(ch);
					}
					break;
				}
				buffer_append(&ch->input, buf, len);
			}
			/* Send buffered output data to the socket. */
			if (FD_ISSET(ch->sock, writeset) && buffer_len(&ch->output) > 0) {
				len = write(ch->sock, buffer_ptr(&ch->output),
					    buffer_len(&ch->output));
				if (len <= 0) {
					if (compat13) {
						buffer_consume(&ch->output, buffer_len(&ch->output));
						debug("Channel %d status set to input draining.", i);
						ch->type = SSH_CHANNEL_INPUT_DRAINING;
					} else {
						chan_write_failed(ch);
					}
					break;
				}
				buffer_consume(&ch->output, len);
			}
			break;

		case SSH_CHANNEL_OUTPUT_DRAINING:
			if (!compat13)
				fatal("cannot happen: OUT_DRAIN");
			/* Send buffered output data to the socket. */
			if (FD_ISSET(ch->sock, writeset) && buffer_len(&ch->output) > 0) {
				len = write(ch->sock, buffer_ptr(&ch->output),
					    buffer_len(&ch->output));
				if (len <= 0)
					buffer_consume(&ch->output, buffer_len(&ch->output));
				else
					buffer_consume(&ch->output, len);
			}
			break;

		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_FREE:
		default:
			continue;
		}
	}
}

/* If there is data to send to the connection, send some of it now. */

void 
channel_output_poll()
{
	int len, i;
	Channel *ch;

	for (i = 0; i < channels_alloc; i++) {
		ch = &channels[i];

		/* We are only interested in channels that can have buffered incoming data. */
		if (compat13) {
			if (ch->type != SSH_CHANNEL_OPEN &&
			    ch->type != SSH_CHANNEL_INPUT_DRAINING)
				continue;
		} else {
			if (ch->type != SSH_CHANNEL_OPEN)
				continue;
			if (ch->istate != CHAN_INPUT_OPEN &&
			    ch->istate != CHAN_INPUT_WAIT_DRAIN)
				continue;
		}

		/* Get the amount of buffered data for this channel. */
		len = buffer_len(&ch->input);
		if (len > 0) {
			/* Send some data for the other side over the secure connection. */
			if (packet_is_interactive()) {
				if (len > 1024)
					len = 512;
			} else {
				/* Keep the packets at reasonable size. */
				if (len > packet_get_maxsize()/2)
					len = packet_get_maxsize()/2;
			}
			packet_start(SSH_MSG_CHANNEL_DATA);
			packet_put_int(ch->remote_id);
			packet_put_string(buffer_ptr(&ch->input), len);
			packet_send();
			buffer_consume(&ch->input, len);
		} else if (ch->istate == CHAN_INPUT_WAIT_DRAIN) {
			if (compat13)
				fatal("cannot happen: istate == INPUT_WAIT_DRAIN for proto 1.3");
			/*
			 * input-buffer is empty and read-socket shutdown:
			 * tell peer, that we will not send more data: send IEOF
			 */
			chan_ibuf_empty(ch);
		}
	}
}

/*
 * This is called when a packet of type CHANNEL_DATA has just been received.
 * The message type has already been consumed, but channel number and data is
 * still there.
 */

void 
channel_input_data(int payload_len)
{
	int id;
	char *data;
	unsigned int data_len;
	Channel *ch;

	/* Get the channel number and verify it. */
	id = packet_get_int();
	if (id < 0 || id >= channels_alloc)
		packet_disconnect("Received data for nonexistent channel %d.", id);
	ch = &channels[id];

	if (ch->type == SSH_CHANNEL_FREE)
		packet_disconnect("Received data for free channel %d.", ch->self);

	/* Ignore any data for non-open channels (might happen on close) */
	if (ch->type != SSH_CHANNEL_OPEN &&
	    ch->type != SSH_CHANNEL_X11_OPEN)
		return;

	/* same for protocol 1.5 if output end is no longer open */
	if (!compat13 && ch->ostate != CHAN_OUTPUT_OPEN)
		return;

	/* Get the data. */
	data = packet_get_string(&data_len);
	packet_integrity_check(payload_len, 4 + 4 + data_len, SSH_MSG_CHANNEL_DATA);
	buffer_append(&ch->output, data, data_len);
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
	Channel *ch;

	for (i = 0; i < channels_alloc; i++) {
		ch = &channels[i];
		if (ch->type == SSH_CHANNEL_OPEN) {
			if (buffer_len(&ch->input) > packet_get_maxsize())
				return 0;
			if (buffer_len(&ch->output) > packet_get_maxsize())
				return 0;
		}
	}
	return 1;
}

/* This is called after receiving CHANNEL_CLOSE/IEOF. */

void 
channel_input_close()
{
	int channel;

	/* Get the channel number and verify it. */
	channel = packet_get_int();
	if (channel < 0 || channel >= channels_alloc ||
	    channels[channel].type == SSH_CHANNEL_FREE)
		packet_disconnect("Received data for nonexistent channel %d.", channel);

	if (!compat13) {
		/* proto version 1.5 overloads CLOSE with IEOF */
		chan_rcvd_ieof(&channels[channel]);
		return;
	}

	/*
	 * Send a confirmation that we have closed the channel and no more
	 * data is coming for it.
	 */
	packet_start(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION);
	packet_put_int(channels[channel].remote_id);
	packet_send();

	/*
	 * If the channel is in closed state, we have sent a close request,
	 * and the other side will eventually respond with a confirmation.
	 * Thus, we cannot free the channel here, because then there would be
	 * no-one to receive the confirmation.  The channel gets freed when
	 * the confirmation arrives.
	 */
	if (channels[channel].type != SSH_CHANNEL_CLOSED) {
		/*
		 * Not a closed channel - mark it as draining, which will
		 * cause it to be freed later.
		 */
		buffer_consume(&channels[channel].input,
			       buffer_len(&channels[channel].input));
		channels[channel].type = SSH_CHANNEL_OUTPUT_DRAINING;
	}
}

/* This is called after receiving CHANNEL_CLOSE_CONFIRMATION/OCLOSE. */

void 
channel_input_close_confirmation()
{
	int channel;

	/* Get the channel number and verify it. */
	channel = packet_get_int();
	if (channel < 0 || channel >= channels_alloc)
		packet_disconnect("Received close confirmation for out-of-range channel %d.",
				  channel);

	if (!compat13) {
		/* proto version 1.5 overloads CLOSE_CONFIRMATION with OCLOSE */
		chan_rcvd_oclose(&channels[channel]);
		return;
	}
	if (channels[channel].type != SSH_CHANNEL_CLOSED)
		packet_disconnect("Received close confirmation for non-closed channel %d (type %d).",
				  channel, channels[channel].type);

	/* Free the channel. */
	channel_free(channel);
}

/* This is called after receiving CHANNEL_OPEN_CONFIRMATION. */

void 
channel_input_open_confirmation()
{
	int channel, remote_channel;

	/* Get the channel number and verify it. */
	channel = packet_get_int();
	if (channel < 0 || channel >= channels_alloc ||
	    channels[channel].type != SSH_CHANNEL_OPENING)
		packet_disconnect("Received open confirmation for non-opening channel %d.",
				  channel);

	/* Get remote side's id for this channel. */
	remote_channel = packet_get_int();

	/* Record the remote channel number and mark that the channel is now open. */
	channels[channel].remote_id = remote_channel;
	channels[channel].type = SSH_CHANNEL_OPEN;
}

/* This is called after receiving CHANNEL_OPEN_FAILURE from the other side. */

void 
channel_input_open_failure()
{
	int channel;

	/* Get the channel number and verify it. */
	channel = packet_get_int();
	if (channel < 0 || channel >= channels_alloc ||
	    channels[channel].type != SSH_CHANNEL_OPENING)
		packet_disconnect("Received open failure for non-opening channel %d.",
				  channel);

	/* Free the channel.  This will also close the socket. */
	channel_free(channel);
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
 * Closes the sockets of all channels.  This is used to close extra file
 * descriptors after a fork.
 */

void 
channel_close_all()
{
	int i;
	for (i = 0; i < channels_alloc; i++) {
		if (channels[i].type != SSH_CHANNEL_FREE)
			close(channels[i].sock);
	}
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
		case SSH_CHANNEL_OPENING:
		case SSH_CHANNEL_OPEN:
		case SSH_CHANNEL_X11_OPEN:
		case SSH_CHANNEL_INPUT_DRAINING:
		case SSH_CHANNEL_OUTPUT_DRAINING:
			snprintf(buf, sizeof buf, "  #%d %.300s (t%d r%d i%d/%d o%d/%d)\r\n",
			    c->self, c->remote_name,
			    c->type, c->remote_id,
			    c->istate, buffer_len(&c->input),
			    c->ostate, buffer_len(&c->output));
			buffer_append(&buffer, buf, strlen(buf));
			continue;
		default:
			fatal("channel_still_open: bad channel type %d", c->type);
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
		ch = channel_allocate(SSH_CHANNEL_PORT_LISTENER, sock,
		    xstrdup("port listener"));
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
channel_request_remote_forwarding(u_short port, const char *host,
				  u_short remote_port)
{
	int payload_len;
	/* Record locally that connection to this host/port is permitted. */
	if (num_permitted_opens >= SSH_MAX_FORWARDS_PER_DIRECTION)
		fatal("channel_request_remote_forwarding: too many forwards");

	permitted_opens[num_permitted_opens].host = xstrdup(host);
	permitted_opens[num_permitted_opens].port = remote_port;
	num_permitted_opens++;

	/* Send the forward request to the remote side. */
	packet_start(SSH_CMSG_PORT_FORWARD_REQUEST);
	packet_put_int(port);
	packet_put_string(host, strlen(host));
	packet_put_int(remote_port);
	packet_send();
	packet_write_wait();

	/*
	 * Wait for response from the remote side.  It will send a disconnect
	 * message on failure, and we will never see it here.
	 */
	packet_read_expect(&payload_len, SSH_SMSG_SUCCESS);
}

/*
 * This is called after receiving CHANNEL_FORWARDING_REQUEST.  This initates
 * listening for the port, and sends back a success reply (or disconnect
 * message if there was an error).  This never returns if there was an error.
 */

void 
channel_input_port_forward_request(int is_root)
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
	 * bind port to localhost only (gateway ports == 0).
	 */
	channel_request_local_forwarding(port, hostname, host_port, 0);

	/* Free the argument string. */
	xfree(hostname);
}

/*
 * This is called after receiving PORT_OPEN message.  This attempts to
 * connect to the given host:port, and sends back CHANNEL_OPEN_CONFIRMATION
 * or CHANNEL_OPEN_FAILURE.
 */

void 
channel_input_port_open(int payload_len)
{
	int remote_channel, sock = 0, newch, i;
	u_short host_port;
	char *host, *originator_string;
	int host_len, originator_len;
	struct addrinfo hints, *ai, *aitop;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];
	int gaierr;

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

	packet_integrity_check(payload_len,
			       4 + 4 + host_len + 4 + originator_len,
			       SSH_MSG_PORT_OPEN);

	/* Check if opening that port is permitted. */
	if (!all_opens_permitted) {
		/* Go trough all permitted ports. */
		for (i = 0; i < num_permitted_opens; i++)
			if (permitted_opens[i].port == host_port &&
			    strcmp(permitted_opens[i].host, host) == 0)
				break;

		/* Check if we found the requested port among those permitted. */
		if (i >= num_permitted_opens) {
			/* The port is not permitted. */
			log("Received request to connect to %.100s:%d, but the request was denied.",
			    host, host_port);
			goto fail;
		}
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", host_port);
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0) {
		error("%.100s: unknown host (%s)", host, gai_strerror(gaierr));
		goto fail;
	}

	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop, sizeof(ntop),
		    strport, sizeof(strport), NI_NUMERICHOST|NI_NUMERICSERV) != 0) {
			error("channel_input_port_open: getnameinfo failed");
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
		goto fail;
	}

	/* Successful connection. */

	/* Allocate a channel for this connection. */
	newch = channel_allocate(SSH_CHANNEL_OPEN, sock, originator_string);
	channels[newch].remote_id = remote_channel;

	/* Send a confirmation to the remote host. */
	packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
	packet_put_int(remote_channel);
	packet_put_int(newch);
	packet_send();

	/* Free the argument string. */
	xfree(host);

	return;

fail:
	/* Free the argument string. */
	xfree(host);

	/* Send refusal to the remote host. */
	packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
	packet_put_int(remote_channel);
	packet_send();
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
		(void) channel_allocate(SSH_CHANNEL_X11_LISTENER, sock,
					xstrdup("X11 inet listener"));
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


/*
 * This is called when SSH_SMSG_X11_OPEN is received.  The packet contains
 * the remote channel number.  We should do whatever we want, and respond
 * with either SSH_MSG_OPEN_CONFIRMATION or SSH_MSG_OPEN_FAILURE.
 */

void 
x11_input_open(int payload_len)
{
	int remote_channel, display_number, sock = 0, newch;
	const char *display;
	char buf[1024], *cp, *remote_host;
	int remote_len;
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr;

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
	packet_integrity_check(payload_len, 4 + remote_len, SSH_SMSG_X11_OPEN);

	/* Try to open a socket for the local X server. */
	display = getenv("DISPLAY");
	if (!display) {
		error("DISPLAY not set.");
		goto fail;
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
			goto fail;
		}
		/* Create a socket. */
		sock = connect_local_xsocket(display_number);
		if (sock < 0)
			goto fail;

		/* OK, we now have a connection to the display. */
		goto success;
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
		goto fail;
	}
	*cp = 0;
	/* buf now contains the host name.  But first we parse the display number. */
	if (sscanf(cp + 1, "%d", &display_number) != 1) {
		error("Could not parse display number from DISPLAY: %.100s",
		      display);
		goto fail;
	}

	/* Look up the host address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(strport, sizeof strport, "%d", 6000 + display_number);
	if ((gaierr = getaddrinfo(buf, strport, &hints, &aitop)) != 0) {
		error("%.100s: unknown host. (%s)", buf, gai_strerror(gaierr));
		goto fail;
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
		debug("connect %.100s port %d: %.100s", buf, 6000 + display_number, 
		    strerror(errno));
		close(sock);
		continue;
	}
	/* Success */
	break;

	} /* (ai = aitop, ai; ai = ai->ai_next) */
	freeaddrinfo(aitop);
	if (!ai) {
		error("connect %.100s port %d: %.100s", buf, 6000 + display_number, 
		    strerror(errno));
		goto fail;
	}
success:
	/* We have successfully obtained a connection to the real X display. */

	/* Allocate a channel for this connection. */
	if (x11_saved_proto == NULL)
		newch = channel_allocate(SSH_CHANNEL_OPEN, sock, remote_host);
	else
		newch = channel_allocate(SSH_CHANNEL_X11_OPEN, sock, remote_host);
	channels[newch].remote_id = remote_channel;

	/* Send a confirmation to the remote host. */
	packet_start(SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
	packet_put_int(remote_channel);
	packet_put_int(newch);
	packet_send();

	return;

fail:
	/* Send refusal to the remote host. */
	packet_start(SSH_MSG_CHANNEL_OPEN_FAILURE);
	packet_put_int(remote_channel);
	packet_send();
}

/*
 * Requests forwarding of X11 connections, generates fake authentication
 * data, and enables authentication spoofing.
 */

void 
x11_request_forwarding_with_spoofing(const char *proto, const char *data)
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
	packet_start(SSH_CMSG_X11_REQUEST_FORWARDING);
	packet_put_string(proto, strlen(proto));
	packet_put_string(new_data, strlen(new_data));
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
 * This if called to process SSH_CMSG_AGENT_REQUEST_FORWARDING on the server.
 * This starts forwarding authentication requests.
 */

void 
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
	if (mkdtemp(channel_forwarded_auth_socket_dir) == NULL)
		packet_disconnect("mkdtemp: %.100s", strerror(errno));
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
}

/* This is called to process an SSH_SMSG_AGENT_OPEN message. */

void 
auth_input_open_request()
{
	int remch, sock, newch;
	char *dummyname;

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
