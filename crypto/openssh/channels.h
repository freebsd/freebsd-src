/* RCSID("$Id: channels.h,v 1.6 1999/11/24 19:53:45 markus Exp $"); */

#ifndef CHANNELS_H
#define CHANNELS_H

/* Definitions for channel types. */
#define SSH_CHANNEL_FREE		0	/* This channel is free (unused). */
#define SSH_CHANNEL_X11_LISTENER	1	/* Listening for inet X11 conn. */
#define SSH_CHANNEL_PORT_LISTENER	2	/* Listening on a port. */
#define SSH_CHANNEL_OPENING		3	/* waiting for confirmation */
#define SSH_CHANNEL_OPEN		4	/* normal open two-way channel */
#define SSH_CHANNEL_CLOSED		5	/* waiting for close confirmation */
/*	SSH_CHANNEL_AUTH_FD		6    	   authentication fd */
#define SSH_CHANNEL_AUTH_SOCKET		7	/* authentication socket */
/*	SSH_CHANNEL_AUTH_SOCKET_FD	8    	   connection to auth socket */
#define SSH_CHANNEL_X11_OPEN		9	/* reading first X11 packet */
#define SSH_CHANNEL_INPUT_DRAINING	10	/* sending remaining data to conn */
#define SSH_CHANNEL_OUTPUT_DRAINING	11	/* sending remaining data to app */

/*
 * Data structure for channel data.  This is iniailized in channel_allocate
 * and cleared in channel_free.
 */

typedef struct Channel {
	int     type;		/* channel type/state */
	int     self;		/* my own channel identifier */
	int     remote_id;	/* channel identifier for remote peer */
	/* peer can be reached over encrypted connection, via packet-sent */
	int     istate;		/* input from channel (state of receive half) */
	int     ostate;		/* output to channel  (state of transmit half) */
	int     sock;		/* data socket, linked to this channel */
	Buffer  input;		/* data read from socket, to be sent over
				 * encrypted connection */
	Buffer  output;		/* data received over encrypted connection for
				 * send on socket */
	char    path[200];	/* path for unix domain sockets, or host name
				 * for forwards */
	int     listening_port;	/* port being listened for forwards */
	int     host_port;	/* remote port to connect for forwards */
	char   *remote_name;	/* remote hostname */
}       Channel;
#endif
