/* RCSID("$Id: channels.h,v 1.13 2000/05/30 17:23:37 markus Exp $"); */

#ifndef CHANNELS_H
#define CHANNELS_H

/* Definitions for channel types. */
#define SSH_CHANNEL_FREE		0	/* This channel is free (unused). */
#define SSH_CHANNEL_X11_LISTENER	1	/* Listening for inet X11 conn. */
#define SSH_CHANNEL_PORT_LISTENER	2	/* Listening on a port. */
#define SSH_CHANNEL_OPENING		3	/* waiting for confirmation */
#define SSH_CHANNEL_OPEN		4	/* normal open two-way channel */
#define SSH_CHANNEL_CLOSED		5	/* waiting for close confirmation */
#define SSH_CHANNEL_AUTH_SOCKET		6	/* authentication socket */
#define SSH_CHANNEL_X11_OPEN		7	/* reading first X11 packet */
#define SSH_CHANNEL_INPUT_DRAINING	8	/* sending remaining data to conn */
#define SSH_CHANNEL_OUTPUT_DRAINING	9	/* sending remaining data to app */
#define SSH_CHANNEL_LARVAL		10	/* larval session */
#define SSH_CHANNEL_MAX_TYPE		11

/*
 * Data structure for channel data.  This is iniailized in channel_allocate
 * and cleared in channel_free.
 */
typedef void channel_callback_fn(int id, void *arg);

typedef struct Channel {
	int     type;		/* channel type/state */
	int     self;		/* my own channel identifier */
	int     remote_id;	/* channel identifier for remote peer */
	/* peer can be reached over encrypted connection, via packet-sent */
	int     istate;		/* input from channel (state of receive half) */
	int     ostate;		/* output to channel  (state of transmit half) */
	int     flags;		/* close sent/rcvd */
	int     rfd;		/* read fd */
	int     wfd;		/* write fd */
	int     efd;		/* extended fd */
	int     sock;		/* sock fd */
	Buffer  input;		/* data read from socket, to be sent over
				 * encrypted connection */
	Buffer  output;		/* data received over encrypted connection for
				 * send on socket */
	Buffer  extended;
	char    path[200];	/* path for unix domain sockets, or host name
				 * for forwards */
	int     listening_port;	/* port being listened for forwards */
	int     host_port;	/* remote port to connect for forwards */
	char   *remote_name;	/* remote hostname */

	int	remote_window;
	int	remote_maxpacket;
	int	local_window;
	int	local_window_max;
	int	local_consumed;
	int	local_maxpacket;
	int     extended_usage;

	char   *ctype;		/* type */

	/* callback */
	channel_callback_fn	*cb_fn;
	void	*cb_arg;
	int	cb_event;
	channel_callback_fn	*dettach_user;
}       Channel;

#define CHAN_EXTENDED_IGNORE		0
#define CHAN_EXTENDED_READ		1
#define CHAN_EXTENDED_WRITE		2

void	channel_set_fds(int id, int rfd, int wfd, int efd, int extusage);
void	channel_open(int id);
void	channel_request(int id, char *service, int wantconfirm);
void	channel_request_start(int id, char *service, int wantconfirm);
void	channel_register_callback(int id, int mtype, channel_callback_fn *fn, void *arg);
void	channel_register_cleanup(int id, channel_callback_fn *fn);
void	channel_cancel_cleanup(int id);
Channel	*channel_lookup(int id);

int
channel_new(char *ctype, int type, int rfd, int wfd, int efd,
    int window, int maxpack, int extended_usage, char *remote_name);

void	channel_input_channel_request(int type, int plen);
void	channel_input_close(int type, int plen);
void	channel_input_close_confirmation(int type, int plen);
void	channel_input_data(int type, int plen);
void	channel_input_extended_data(int type, int plen);
void	channel_input_ieof(int type, int plen);
void	channel_input_oclose(int type, int plen);
void	channel_input_open_confirmation(int type, int plen);
void	channel_input_open_failure(int type, int plen);
void	channel_input_port_open(int type, int plen);
void	channel_input_window_adjust(int type, int plen);
void	channel_input_open(int type, int plen);

/* Sets specific protocol options. */
void    channel_set_options(int hostname_in_open);

/*
 * Allocate a new channel object and set its type and socket.  Remote_name
 * must have been allocated with xmalloc; this will free it when the channel
 * is freed.
 */
int     channel_allocate(int type, int sock, char *remote_name);

/* Free the channel and close its socket. */
void    channel_free(int channel);

/* Add any bits relevant to channels in select bitmasks. */
void    channel_prepare_select(fd_set * readset, fd_set * writeset);

/*
 * After select, perform any appropriate operations for channels which have
 * events pending.
 */
void    channel_after_select(fd_set * readset, fd_set * writeset);

/* If there is data to send to the connection, send some of it now. */
void    channel_output_poll(void);

/* Returns true if no channel has too much buffered data. */
int     channel_not_very_much_buffered_data(void);

/* This closes any sockets that are listening for connections; this removes
   any unix domain sockets. */
void    channel_stop_listening(void);

/*
 * Closes the sockets of all channels.  This is used to close extra file
 * descriptors after a fork.
 */
void    channel_close_all(void);

/* Returns the maximum file descriptor number used by the channels. */
int     channel_max_fd(void);

/* Returns true if there is still an open channel over the connection. */
int     channel_still_open(void);

/*
 * Returns a string containing a list of all open channels.  The list is
 * suitable for displaying to the user.  It uses crlf instead of newlines.
 * The caller should free the string with xfree.
 */
char   *channel_open_message(void);

/*
 * Initiate forwarding of connections to local port "port" through the secure
 * channel to host:port from remote side.  This never returns if there was an
 * error.
 */
void
channel_request_local_forwarding(u_short port, const char *host,
    u_short remote_port, int gateway_ports);

/*
 * Initiate forwarding of connections to port "port" on remote host through
 * the secure channel to host:port from local side.  This never returns if
 * there was an error.  This registers that open requests for that port are
 * permitted.
 */
void
channel_request_remote_forwarding(u_short port, const char *host,
    u_short remote_port);

/*
 * Permits opening to any host/port in SSH_MSG_PORT_OPEN.  This is usually
 * called by the server, because the user could connect to any port anyway,
 * and the server has no way to know but to trust the client anyway.
 */
void    channel_permit_all_opens(void);

/*
 * This is called after receiving CHANNEL_FORWARDING_REQUEST.  This initates
 * listening for the port, and sends back a success reply (or disconnect
 * message if there was an error).  This never returns if there was an error.
 */
void    channel_input_port_forward_request(int is_root, int gateway_ports);

/*
 * Creates a port for X11 connections, and starts listening for it. Returns
 * the display name, or NULL if an error was encountered.
 */
char   *x11_create_display(int screen);

/*
 * Creates an internet domain socket for listening for X11 connections.
 * Returns a suitable value for the DISPLAY variable, or NULL if an error
 * occurs.
 */
char   *x11_create_display_inet(int screen, int x11_display_offset);

/*
 * This is called when SSH_SMSG_X11_OPEN is received.  The packet contains
 * the remote channel number.  We should do whatever we want, and respond
 * with either SSH_MSG_OPEN_CONFIRMATION or SSH_MSG_OPEN_FAILURE.
 */
void    x11_input_open(int type, int plen);

/*
 * Requests forwarding of X11 connections.  This should be called on the
 * client only.
 */
void    x11_request_forwarding(void);

/*
 * Requests forwarding for X11 connections, with authentication spoofing.
 * This should be called in the client only.
 */
void
x11_request_forwarding_with_spoofing(int client_session_id,
    const char *proto, const char *data);

/* Sends a message to the server to request authentication fd forwarding. */
void    auth_request_forwarding(void);

/*
 * Returns the name of the forwarded authentication socket.  Returns NULL if
 * there is no forwarded authentication socket.  The returned value points to
 * a static buffer.
 */
char   *auth_get_socket_name(void);

/*
 * This is called to process SSH_CMSG_AGENT_REQUEST_FORWARDING on the server.
 * This starts forwarding authentication requests.
 */
int     auth_input_request_forwarding(struct passwd * pw);

/* This is called to process an SSH_SMSG_AGENT_OPEN message. */
void    auth_input_open_request(int type, int plen);

/* XXX */
int	channel_connect_to(const char *host, u_short host_port);
int	x11_connect_display(void);

#endif
