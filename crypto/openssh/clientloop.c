/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * The main loop for the interactive session (client side).
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
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
 *
 *
 * SSH2 support added by Markus Friedl.
 * Copyright (c) 1999, 2000, 2001 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: clientloop.c,v 1.104 2002/08/22 19:38:42 stevesk Exp $");

#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "xmalloc.h"
#include "packet.h"
#include "buffer.h"
#include "compat.h"
#include "channels.h"
#include "dispatch.h"
#include "buffer.h"
#include "bufaux.h"
#include "key.h"
#include "kex.h"
#include "log.h"
#include "readconf.h"
#include "clientloop.h"
#include "authfd.h"
#include "atomicio.h"
#include "sshtty.h"
#include "misc.h"
#include "readpass.h"

/* import options */
extern Options options;

/* Flag indicating that stdin should be redirected from /dev/null. */
extern int stdin_null_flag;

/*
 * Name of the host we are connecting to.  This is the name given on the
 * command line, or the HostName specified for the user-supplied name in a
 * configuration file.
 */
extern char *host;

/*
 * Flag to indicate that we have received a window change signal which has
 * not yet been processed.  This will cause a message indicating the new
 * window size to be sent to the server a little later.  This is volatile
 * because this is updated in a signal handler.
 */
static volatile sig_atomic_t received_window_change_signal = 0;
static volatile sig_atomic_t received_signal = 0;

/* Flag indicating whether the user\'s terminal is in non-blocking mode. */
static int in_non_blocking_mode = 0;

/* Common data for the client loop code. */
static int quit_pending;	/* Set to non-zero to quit the client loop. */
static int escape_char;		/* Escape character. */
static int escape_pending;	/* Last character was the escape character */
static int last_was_cr;		/* Last character was a newline. */
static int exit_status;		/* Used to store the exit status of the command. */
static int stdin_eof;		/* EOF has been encountered on standard error. */
static Buffer stdin_buffer;	/* Buffer for stdin data. */
static Buffer stdout_buffer;	/* Buffer for stdout data. */
static Buffer stderr_buffer;	/* Buffer for stderr data. */
static u_long stdin_bytes, stdout_bytes, stderr_bytes;
static u_int buffer_high;/* Soft max buffer size. */
static int connection_in;	/* Connection to server (input). */
static int connection_out;	/* Connection to server (output). */
static int need_rekeying;	/* Set to non-zero if rekeying is requested. */
static int session_closed = 0;	/* In SSH2: login session closed. */

static void client_init_dispatch(void);
int	session_ident = -1;

/*XXX*/
extern Kex *xxx_kex;

/* Restores stdin to blocking mode. */

static void
leave_non_blocking(void)
{
	if (in_non_blocking_mode) {
		(void) fcntl(fileno(stdin), F_SETFL, 0);
		in_non_blocking_mode = 0;
		fatal_remove_cleanup((void (*) (void *)) leave_non_blocking, NULL);
	}
}

/* Puts stdin terminal in non-blocking mode. */

static void
enter_non_blocking(void)
{
	in_non_blocking_mode = 1;
	(void) fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);
	fatal_add_cleanup((void (*) (void *)) leave_non_blocking, NULL);
}

/*
 * Signal handler for the window change signal (SIGWINCH).  This just sets a
 * flag indicating that the window has changed.
 */

static void
window_change_handler(int sig)
{
	received_window_change_signal = 1;
	signal(SIGWINCH, window_change_handler);
}

/*
 * Signal handler for signals that cause the program to terminate.  These
 * signals must be trapped to restore terminal modes.
 */

static void
signal_handler(int sig)
{
	received_signal = sig;
	quit_pending = 1;
}

/*
 * Returns current time in seconds from Jan 1, 1970 with the maximum
 * available resolution.
 */

static double
get_current_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}

/*
 * This is called when the interactive is entered.  This checks if there is
 * an EOF coming on stdin.  We must check this explicitly, as select() does
 * not appear to wake up when redirecting from /dev/null.
 */

static void
client_check_initial_eof_on_stdin(void)
{
	int len;
	char buf[1];

	/*
	 * If standard input is to be "redirected from /dev/null", we simply
	 * mark that we have seen an EOF and send an EOF message to the
	 * server. Otherwise, we try to read a single character; it appears
	 * that for some files, such /dev/null, select() never wakes up for
	 * read for this descriptor, which means that we never get EOF.  This
	 * way we will get the EOF if stdin comes from /dev/null or similar.
	 */
	if (stdin_null_flag) {
		/* Fake EOF on stdin. */
		debug("Sending eof.");
		stdin_eof = 1;
		packet_start(SSH_CMSG_EOF);
		packet_send();
	} else {
		enter_non_blocking();

		/* Check for immediate EOF on stdin. */
		len = read(fileno(stdin), buf, 1);
		if (len == 0) {
			/* EOF.  Record that we have seen it and send EOF to server. */
			debug("Sending eof.");
			stdin_eof = 1;
			packet_start(SSH_CMSG_EOF);
			packet_send();
		} else if (len > 0) {
			/*
			 * Got data.  We must store the data in the buffer,
			 * and also process it as an escape character if
			 * appropriate.
			 */
			if ((u_char) buf[0] == escape_char)
				escape_pending = 1;
			else
				buffer_append(&stdin_buffer, buf, 1);
		}
		leave_non_blocking();
	}
}


/*
 * Make packets from buffered stdin data, and buffer them for sending to the
 * connection.
 */

static void
client_make_packets_from_stdin_data(void)
{
	u_int len;

	/* Send buffered stdin data to the server. */
	while (buffer_len(&stdin_buffer) > 0 &&
	    packet_not_very_much_data_to_write()) {
		len = buffer_len(&stdin_buffer);
		/* Keep the packets at reasonable size. */
		if (len > packet_get_maxsize())
			len = packet_get_maxsize();
		packet_start(SSH_CMSG_STDIN_DATA);
		packet_put_string(buffer_ptr(&stdin_buffer), len);
		packet_send();
		buffer_consume(&stdin_buffer, len);
		stdin_bytes += len;
		/* If we have a pending EOF, send it now. */
		if (stdin_eof && buffer_len(&stdin_buffer) == 0) {
			packet_start(SSH_CMSG_EOF);
			packet_send();
		}
	}
}

/*
 * Checks if the client window has changed, and sends a packet about it to
 * the server if so.  The actual change is detected elsewhere (by a software
 * interrupt on Unix); this just checks the flag and sends a message if
 * appropriate.
 */

static void
client_check_window_change(void)
{
	struct winsize ws;

	if (! received_window_change_signal)
		return;
	/** XXX race */
	received_window_change_signal = 0;

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) < 0)
		return;

	debug2("client_check_window_change: changed");

	if (compat20) {
		channel_request_start(session_ident, "window-change", 0);
		packet_put_int(ws.ws_col);
		packet_put_int(ws.ws_row);
		packet_put_int(ws.ws_xpixel);
		packet_put_int(ws.ws_ypixel);
		packet_send();
	} else {
		packet_start(SSH_CMSG_WINDOW_SIZE);
		packet_put_int(ws.ws_row);
		packet_put_int(ws.ws_col);
		packet_put_int(ws.ws_xpixel);
		packet_put_int(ws.ws_ypixel);
		packet_send();
	}
}

/*
 * Waits until the client can do something (some data becomes available on
 * one of the file descriptors).
 */

static void
client_wait_until_can_do_something(fd_set **readsetp, fd_set **writesetp,
    int *maxfdp, int *nallocp, int rekeying)
{
	/* Add any selections by the channel mechanism. */
	channel_prepare_select(readsetp, writesetp, maxfdp, nallocp, rekeying);

	if (!compat20) {
		/* Read from the connection, unless our buffers are full. */
		if (buffer_len(&stdout_buffer) < buffer_high &&
		    buffer_len(&stderr_buffer) < buffer_high &&
		    channel_not_very_much_buffered_data())
			FD_SET(connection_in, *readsetp);
		/*
		 * Read from stdin, unless we have seen EOF or have very much
		 * buffered data to send to the server.
		 */
		if (!stdin_eof && packet_not_very_much_data_to_write())
			FD_SET(fileno(stdin), *readsetp);

		/* Select stdout/stderr if have data in buffer. */
		if (buffer_len(&stdout_buffer) > 0)
			FD_SET(fileno(stdout), *writesetp);
		if (buffer_len(&stderr_buffer) > 0)
			FD_SET(fileno(stderr), *writesetp);
	} else {
		/* channel_prepare_select could have closed the last channel */
		if (session_closed && !channel_still_open() &&
		    !packet_have_data_to_write()) {
			/* clear mask since we did not call select() */
			memset(*readsetp, 0, *nallocp);
			memset(*writesetp, 0, *nallocp);
			return;
		} else {
			FD_SET(connection_in, *readsetp);
		}
	}

	/* Select server connection if have data to write to the server. */
	if (packet_have_data_to_write())
		FD_SET(connection_out, *writesetp);

	/*
	 * Wait for something to happen.  This will suspend the process until
	 * some selected descriptor can be read, written, or has some other
	 * event pending. Note: if you want to implement SSH_MSG_IGNORE
	 * messages to fool traffic analysis, this might be the place to do
	 * it: just have a random timeout for the select, and send a random
	 * SSH_MSG_IGNORE packet when the timeout expires.
	 */

	if (select((*maxfdp)+1, *readsetp, *writesetp, NULL, NULL) < 0) {
		char buf[100];

		/*
		 * We have to clear the select masks, because we return.
		 * We have to return, because the mainloop checks for the flags
		 * set by the signal handlers.
		 */
		memset(*readsetp, 0, *nallocp);
		memset(*writesetp, 0, *nallocp);

		if (errno == EINTR)
			return;
		/* Note: we might still have data in the buffers. */
		snprintf(buf, sizeof buf, "select: %s\r\n", strerror(errno));
		buffer_append(&stderr_buffer, buf, strlen(buf));
		quit_pending = 1;
	}
}

static void
client_suspend_self(Buffer *bin, Buffer *bout, Buffer *berr)
{
	struct winsize oldws, newws;

	/* Flush stdout and stderr buffers. */
	if (buffer_len(bout) > 0)
		atomicio(write, fileno(stdout), buffer_ptr(bout), buffer_len(bout));
	if (buffer_len(berr) > 0)
		atomicio(write, fileno(stderr), buffer_ptr(berr), buffer_len(berr));

	leave_raw_mode();

	/*
	 * Free (and clear) the buffer to reduce the amount of data that gets
	 * written to swap.
	 */
	buffer_free(bin);
	buffer_free(bout);
	buffer_free(berr);

	/* Save old window size. */
	ioctl(fileno(stdin), TIOCGWINSZ, &oldws);

	/* Send the suspend signal to the program itself. */
	kill(getpid(), SIGTSTP);

	/* Check if the window size has changed. */
	if (ioctl(fileno(stdin), TIOCGWINSZ, &newws) >= 0 &&
	    (oldws.ws_row != newws.ws_row ||
	    oldws.ws_col != newws.ws_col ||
	    oldws.ws_xpixel != newws.ws_xpixel ||
	    oldws.ws_ypixel != newws.ws_ypixel))
		received_window_change_signal = 1;

	/* OK, we have been continued by the user. Reinitialize buffers. */
	buffer_init(bin);
	buffer_init(bout);
	buffer_init(berr);

	enter_raw_mode();
}

static void
client_process_net_input(fd_set * readset)
{
	int len;
	char buf[8192];

	/*
	 * Read input from the server, and add any such data to the buffer of
	 * the packet subsystem.
	 */
	if (FD_ISSET(connection_in, readset)) {
		/* Read as much as possible. */
		len = read(connection_in, buf, sizeof(buf));
		if (len == 0) {
			/* Received EOF.  The remote host has closed the connection. */
			snprintf(buf, sizeof buf, "Connection to %.300s closed by remote host.\r\n",
				 host);
			buffer_append(&stderr_buffer, buf, strlen(buf));
			quit_pending = 1;
			return;
		}
		/*
		 * There is a kernel bug on Solaris that causes select to
		 * sometimes wake up even though there is no data available.
		 */
		if (len < 0 && (errno == EAGAIN || errno == EINTR))
			len = 0;

		if (len < 0) {
			/* An error has encountered.  Perhaps there is a network problem. */
			snprintf(buf, sizeof buf, "Read from remote host %.300s: %.100s\r\n",
				 host, strerror(errno));
			buffer_append(&stderr_buffer, buf, strlen(buf));
			quit_pending = 1;
			return;
		}
		packet_process_incoming(buf, len);
	}
}

static void
process_cmdline(void)
{
	void (*handler)(int);
	char *s, *cmd;
	u_short fwd_port, fwd_host_port;
	char buf[1024], sfwd_port[6], sfwd_host_port[6];
	int local = 0;

	leave_raw_mode();
	handler = signal(SIGINT, SIG_IGN);
	cmd = s = read_passphrase("\r\nssh> ", RP_ECHO);
	if (s == NULL)
		goto out;
	while (*s && isspace(*s))
		s++;
	if (*s == 0)
		goto out;
	if (strlen(s) < 2 || s[0] != '-' || !(s[1] == 'L' || s[1] == 'R')) {
		log("Invalid command.");
		goto out;
	}
	if (s[1] == 'L')
		local = 1;
	if (!local && !compat20) {
		log("Not supported for SSH protocol version 1.");
		goto out;
	}
	s += 2;
	while (*s && isspace(*s))
		s++;

	if (sscanf(s, "%5[0-9]:%255[^:]:%5[0-9]",
	    sfwd_port, buf, sfwd_host_port) != 3 &&
	    sscanf(s, "%5[0-9]/%255[^/]/%5[0-9]",
	    sfwd_port, buf, sfwd_host_port) != 3) {
		log("Bad forwarding specification.");
		goto out;
	}
	if ((fwd_port = a2port(sfwd_port)) == 0 ||
	    (fwd_host_port = a2port(sfwd_host_port)) == 0) {
		log("Bad forwarding port(s).");
		goto out;
	}
	if (local) {
		if (channel_setup_local_fwd_listener(fwd_port, buf,
		    fwd_host_port, options.gateway_ports) < 0) {
			log("Port forwarding failed.");
			goto out;
		}
	} else
		channel_request_remote_forwarding(fwd_port, buf,
		    fwd_host_port);
	log("Forwarding port.");
out:
	signal(SIGINT, handler);
	enter_raw_mode();
	if (cmd)
		xfree(cmd);
}

/* process the characters one by one */
static int
process_escapes(Buffer *bin, Buffer *bout, Buffer *berr, char *buf, int len)
{
	char string[1024];
	pid_t pid;
	int bytes = 0;
	u_int i;
	u_char ch;
	char *s;

	for (i = 0; i < len; i++) {
		/* Get one character at a time. */
		ch = buf[i];

		if (escape_pending) {
			/* We have previously seen an escape character. */
			/* Clear the flag now. */
			escape_pending = 0;

			/* Process the escaped character. */
			switch (ch) {
			case '.':
				/* Terminate the connection. */
				snprintf(string, sizeof string, "%c.\r\n", escape_char);
				buffer_append(berr, string, strlen(string));

				quit_pending = 1;
				return -1;

			case 'Z' - 64:
				/* Suspend the program. */
				/* Print a message to that effect to the user. */
				snprintf(string, sizeof string, "%c^Z [suspend ssh]\r\n", escape_char);
				buffer_append(berr, string, strlen(string));

				/* Restore terminal modes and suspend. */
				client_suspend_self(bin, bout, berr);

				/* We have been continued. */
				continue;

			case 'R':
				if (compat20) {
					if (datafellows & SSH_BUG_NOREKEY)
						log("Server does not support re-keying");
					else
						need_rekeying = 1;
				}
				continue;

			case '&':
				/*
				 * Detach the program (continue to serve connections,
				 * but put in background and no more new connections).
				 */
				/* Restore tty modes. */
				leave_raw_mode();

				/* Stop listening for new connections. */
				channel_stop_listening();

				snprintf(string, sizeof string,
				    "%c& [backgrounded]\n", escape_char);
				buffer_append(berr, string, strlen(string));

				/* Fork into background. */
				pid = fork();
				if (pid < 0) {
					error("fork: %.100s", strerror(errno));
					continue;
				}
				if (pid != 0) {	/* This is the parent. */
					/* The parent just exits. */
					exit(0);
				}
				/* The child continues serving connections. */
				if (compat20) {
					buffer_append(bin, "\004", 1);
					/* fake EOF on stdin */
					return -1;
				} else if (!stdin_eof) {
					/*
					 * Sending SSH_CMSG_EOF alone does not always appear
					 * to be enough.  So we try to send an EOF character
					 * first.
					 */
					packet_start(SSH_CMSG_STDIN_DATA);
					packet_put_string("\004", 1);
					packet_send();
					/* Close stdin. */
					stdin_eof = 1;
					if (buffer_len(bin) == 0) {
						packet_start(SSH_CMSG_EOF);
						packet_send();
					}
				}
				continue;

			case '?':
				snprintf(string, sizeof string,
"%c?\r\n\
Supported escape sequences:\r\n\
%c.  - terminate connection\r\n\
%cC  - open a command line\r\n\
%cR  - Request rekey (SSH protocol 2 only)\r\n\
%c^Z - suspend ssh\r\n\
%c#  - list forwarded connections\r\n\
%c&  - background ssh (when waiting for connections to terminate)\r\n\
%c?  - this message\r\n\
%c%c  - send the escape character by typing it twice\r\n\
(Note that escapes are only recognized immediately after newline.)\r\n",
				    escape_char, escape_char, escape_char, escape_char,
				    escape_char, escape_char, escape_char, escape_char,
				    escape_char, escape_char);
				buffer_append(berr, string, strlen(string));
				continue;

			case '#':
				snprintf(string, sizeof string, "%c#\r\n", escape_char);
				buffer_append(berr, string, strlen(string));
				s = channel_open_message();
				buffer_append(berr, s, strlen(s));
				xfree(s);
				continue;

			case 'C':
				process_cmdline();
				continue;

			default:
				if (ch != escape_char) {
					buffer_put_char(bin, escape_char);
					bytes++;
				}
				/* Escaped characters fall through here */
				break;
			}
		} else {
			/*
			 * The previous character was not an escape char. Check if this
			 * is an escape.
			 */
			if (last_was_cr && ch == escape_char) {
				/* It is. Set the flag and continue to next character. */
				escape_pending = 1;
				continue;
			}
		}

		/*
		 * Normal character.  Record whether it was a newline,
		 * and append it to the buffer.
		 */
		last_was_cr = (ch == '\r' || ch == '\n');
		buffer_put_char(bin, ch);
		bytes++;
	}
	return bytes;
}

static void
client_process_input(fd_set * readset)
{
	int len;
	char buf[8192];

	/* Read input from stdin. */
	if (FD_ISSET(fileno(stdin), readset)) {
		/* Read as much as possible. */
		len = read(fileno(stdin), buf, sizeof(buf));
		if (len < 0 && (errno == EAGAIN || errno == EINTR))
			return;		/* we'll try again later */
		if (len <= 0) {
			/*
			 * Received EOF or error.  They are treated
			 * similarly, except that an error message is printed
			 * if it was an error condition.
			 */
			if (len < 0) {
				snprintf(buf, sizeof buf, "read: %.100s\r\n", strerror(errno));
				buffer_append(&stderr_buffer, buf, strlen(buf));
			}
			/* Mark that we have seen EOF. */
			stdin_eof = 1;
			/*
			 * Send an EOF message to the server unless there is
			 * data in the buffer.  If there is data in the
			 * buffer, no message will be sent now.  Code
			 * elsewhere will send the EOF when the buffer
			 * becomes empty if stdin_eof is set.
			 */
			if (buffer_len(&stdin_buffer) == 0) {
				packet_start(SSH_CMSG_EOF);
				packet_send();
			}
		} else if (escape_char == SSH_ESCAPECHAR_NONE) {
			/*
			 * Normal successful read, and no escape character.
			 * Just append the data to buffer.
			 */
			buffer_append(&stdin_buffer, buf, len);
		} else {
			/*
			 * Normal, successful read.  But we have an escape character
			 * and have to process the characters one by one.
			 */
			if (process_escapes(&stdin_buffer, &stdout_buffer,
			    &stderr_buffer, buf, len) == -1)
				return;
		}
	}
}

static void
client_process_output(fd_set * writeset)
{
	int len;
	char buf[100];

	/* Write buffered output to stdout. */
	if (FD_ISSET(fileno(stdout), writeset)) {
		/* Write as much data as possible. */
		len = write(fileno(stdout), buffer_ptr(&stdout_buffer),
		    buffer_len(&stdout_buffer));
		if (len <= 0) {
			if (errno == EINTR || errno == EAGAIN)
				len = 0;
			else {
				/*
				 * An error or EOF was encountered.  Put an
				 * error message to stderr buffer.
				 */
				snprintf(buf, sizeof buf, "write stdout: %.50s\r\n", strerror(errno));
				buffer_append(&stderr_buffer, buf, strlen(buf));
				quit_pending = 1;
				return;
			}
		}
		/* Consume printed data from the buffer. */
		buffer_consume(&stdout_buffer, len);
		stdout_bytes += len;
	}
	/* Write buffered output to stderr. */
	if (FD_ISSET(fileno(stderr), writeset)) {
		/* Write as much data as possible. */
		len = write(fileno(stderr), buffer_ptr(&stderr_buffer),
		    buffer_len(&stderr_buffer));
		if (len <= 0) {
			if (errno == EINTR || errno == EAGAIN)
				len = 0;
			else {
				/* EOF or error, but can't even print error message. */
				quit_pending = 1;
				return;
			}
		}
		/* Consume printed characters from the buffer. */
		buffer_consume(&stderr_buffer, len);
		stderr_bytes += len;
	}
}

/*
 * Get packets from the connection input buffer, and process them as long as
 * there are packets available.
 *
 * Any unknown packets received during the actual
 * session cause the session to terminate.  This is
 * intended to make debugging easier since no
 * confirmations are sent.  Any compatible protocol
 * extensions must be negotiated during the
 * preparatory phase.
 */

static void
client_process_buffered_input_packets(void)
{
	dispatch_run(DISPATCH_NONBLOCK, &quit_pending, compat20 ? xxx_kex : NULL);
}

/* scan buf[] for '~' before sending data to the peer */

static int
simple_escape_filter(Channel *c, char *buf, int len)
{
	/* XXX we assume c->extended is writeable */
	return process_escapes(&c->input, &c->output, &c->extended, buf, len);
}

static void
client_channel_closed(int id, void *arg)
{
	if (id != session_ident)
		error("client_channel_closed: id %d != session_ident %d",
		    id, session_ident);
	channel_cancel_cleanup(id);
	session_closed = 1;
	if (in_raw_mode())
		leave_raw_mode();
}

/*
 * Implements the interactive session with the server.  This is called after
 * the user has been authenticated, and a command has been started on the
 * remote host.  If escape_char != SSH_ESCAPECHAR_NONE, it is the character
 * used as an escape character for terminating or suspending the session.
 */

int
client_loop(int have_pty, int escape_char_arg, int ssh2_chan_id)
{
	fd_set *readset = NULL, *writeset = NULL;
	double start_time, total_time;
	int max_fd = 0, max_fd2 = 0, len, rekeying = 0, nalloc = 0;
	char buf[100];

	debug("Entering interactive session.");

	start_time = get_current_time();

	/* Initialize variables. */
	escape_pending = 0;
	last_was_cr = 1;
	exit_status = -1;
	stdin_eof = 0;
	buffer_high = 64 * 1024;
	connection_in = packet_get_connection_in();
	connection_out = packet_get_connection_out();
	max_fd = MAX(connection_in, connection_out);

	if (!compat20) {
		/* enable nonblocking unless tty */
		if (!isatty(fileno(stdin)))
			set_nonblock(fileno(stdin));
		if (!isatty(fileno(stdout)))
			set_nonblock(fileno(stdout));
		if (!isatty(fileno(stderr)))
			set_nonblock(fileno(stderr));
		max_fd = MAX(max_fd, fileno(stdin));
		max_fd = MAX(max_fd, fileno(stdout));
		max_fd = MAX(max_fd, fileno(stderr));
	}
	stdin_bytes = 0;
	stdout_bytes = 0;
	stderr_bytes = 0;
	quit_pending = 0;
	escape_char = escape_char_arg;

	/* Initialize buffers. */
	buffer_init(&stdin_buffer);
	buffer_init(&stdout_buffer);
	buffer_init(&stderr_buffer);

	client_init_dispatch();

	/* Set signal handlers to restore non-blocking mode.  */
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	if (have_pty)
		signal(SIGWINCH, window_change_handler);

	if (have_pty)
		enter_raw_mode();

	if (compat20) {
		session_ident = ssh2_chan_id;
		if (escape_char != SSH_ESCAPECHAR_NONE)
			channel_register_filter(session_ident,
			    simple_escape_filter);
		if (session_ident != -1)
			channel_register_cleanup(session_ident,
			    client_channel_closed);
	} else {
		/* Check if we should immediately send eof on stdin. */
		client_check_initial_eof_on_stdin();
	}

	/* Main loop of the client for the interactive session mode. */
	while (!quit_pending) {

		/* Process buffered packets sent by the server. */
		client_process_buffered_input_packets();

		if (compat20 && session_closed && !channel_still_open())
			break;

		rekeying = (xxx_kex != NULL && !xxx_kex->done);

		if (rekeying) {
			debug("rekeying in progress");
		} else {
			/*
			 * Make packets of buffered stdin data, and buffer
			 * them for sending to the server.
			 */
			if (!compat20)
				client_make_packets_from_stdin_data();

			/*
			 * Make packets from buffered channel data, and
			 * enqueue them for sending to the server.
			 */
			if (packet_not_very_much_data_to_write())
				channel_output_poll();

			/*
			 * Check if the window size has changed, and buffer a
			 * message about it to the server if so.
			 */
			client_check_window_change();

			if (quit_pending)
				break;
		}
		/*
		 * Wait until we have something to do (something becomes
		 * available on one of the descriptors).
		 */
		max_fd2 = max_fd;
		client_wait_until_can_do_something(&readset, &writeset,
		    &max_fd2, &nalloc, rekeying);

		if (quit_pending)
			break;

		/* Do channel operations unless rekeying in progress. */
		if (!rekeying) {
			channel_after_select(readset, writeset);

			if (need_rekeying) {
				debug("user requests rekeying");
				xxx_kex->done = 0;
				kex_send_kexinit(xxx_kex);
				need_rekeying = 0;
			}
		}

		/* Buffer input from the connection.  */
		client_process_net_input(readset);

		if (quit_pending)
			break;

		if (!compat20) {
			/* Buffer data from stdin */
			client_process_input(readset);
			/*
			 * Process output to stdout and stderr.  Output to
			 * the connection is processed elsewhere (above).
			 */
			client_process_output(writeset);
		}

		/* Send as much buffered packet data as possible to the sender. */
		if (FD_ISSET(connection_out, writeset))
			packet_write_poll();
	}
	if (readset)
		xfree(readset);
	if (writeset)
		xfree(writeset);

	/* Terminate the session. */

	/* Stop watching for window change. */
	if (have_pty)
		signal(SIGWINCH, SIG_DFL);

	channel_free_all();

	if (have_pty)
		leave_raw_mode();

	/* restore blocking io */
	if (!isatty(fileno(stdin)))
		unset_nonblock(fileno(stdin));
	if (!isatty(fileno(stdout)))
		unset_nonblock(fileno(stdout));
	if (!isatty(fileno(stderr)))
		unset_nonblock(fileno(stderr));

	if (received_signal) {
		if (in_non_blocking_mode)	/* XXX */
			leave_non_blocking();
		fatal("Killed by signal %d.", (int) received_signal);
	}

	/*
	 * In interactive mode (with pseudo tty) display a message indicating
	 * that the connection has been closed.
	 */
	if (have_pty && options.log_level != SYSLOG_LEVEL_QUIET) {
		snprintf(buf, sizeof buf, "Connection to %.64s closed.\r\n", host);
		buffer_append(&stderr_buffer, buf, strlen(buf));
	}

	/* Output any buffered data for stdout. */
	while (buffer_len(&stdout_buffer) > 0) {
		len = write(fileno(stdout), buffer_ptr(&stdout_buffer),
		    buffer_len(&stdout_buffer));
		if (len <= 0) {
			error("Write failed flushing stdout buffer.");
			break;
		}
		buffer_consume(&stdout_buffer, len);
		stdout_bytes += len;
	}

	/* Output any buffered data for stderr. */
	while (buffer_len(&stderr_buffer) > 0) {
		len = write(fileno(stderr), buffer_ptr(&stderr_buffer),
		    buffer_len(&stderr_buffer));
		if (len <= 0) {
			error("Write failed flushing stderr buffer.");
			break;
		}
		buffer_consume(&stderr_buffer, len);
		stderr_bytes += len;
	}

	/* Clear and free any buffers. */
	memset(buf, 0, sizeof(buf));
	buffer_free(&stdin_buffer);
	buffer_free(&stdout_buffer);
	buffer_free(&stderr_buffer);

	/* Report bytes transferred, and transfer rates. */
	total_time = get_current_time() - start_time;
	debug("Transferred: stdin %lu, stdout %lu, stderr %lu bytes in %.1f seconds",
	    stdin_bytes, stdout_bytes, stderr_bytes, total_time);
	if (total_time > 0)
		debug("Bytes per second: stdin %.1f, stdout %.1f, stderr %.1f",
		    stdin_bytes / total_time, stdout_bytes / total_time,
		    stderr_bytes / total_time);

	/* Return the exit status of the program. */
	debug("Exit status %d", exit_status);
	return exit_status;
}

/*********/

static void
client_input_stdout_data(int type, u_int32_t seq, void *ctxt)
{
	u_int data_len;
	char *data = packet_get_string(&data_len);
	packet_check_eom();
	buffer_append(&stdout_buffer, data, data_len);
	memset(data, 0, data_len);
	xfree(data);
}
static void
client_input_stderr_data(int type, u_int32_t seq, void *ctxt)
{
	u_int data_len;
	char *data = packet_get_string(&data_len);
	packet_check_eom();
	buffer_append(&stderr_buffer, data, data_len);
	memset(data, 0, data_len);
	xfree(data);
}
static void
client_input_exit_status(int type, u_int32_t seq, void *ctxt)
{
	exit_status = packet_get_int();
	packet_check_eom();
	/* Acknowledge the exit. */
	packet_start(SSH_CMSG_EXIT_CONFIRMATION);
	packet_send();
	/*
	 * Must wait for packet to be sent since we are
	 * exiting the loop.
	 */
	packet_write_wait();
	/* Flag that we want to exit. */
	quit_pending = 1;
}

static Channel *
client_request_forwarded_tcpip(const char *request_type, int rchan)
{
	Channel *c = NULL;
	char *listen_address, *originator_address;
	int listen_port, originator_port;
	int sock;

	/* Get rest of the packet */
	listen_address = packet_get_string(NULL);
	listen_port = packet_get_int();
	originator_address = packet_get_string(NULL);
	originator_port = packet_get_int();
	packet_check_eom();

	debug("client_request_forwarded_tcpip: listen %s port %d, originator %s port %d",
	    listen_address, listen_port, originator_address, originator_port);

	sock = channel_connect_by_listen_address(listen_port);
	if (sock < 0) {
		xfree(originator_address);
		xfree(listen_address);
		return NULL;
	}
	c = channel_new("forwarded-tcpip",
	    SSH_CHANNEL_CONNECTING, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_WINDOW_DEFAULT, 0,
	    xstrdup(originator_address), 1);
	xfree(originator_address);
	xfree(listen_address);
	return c;
}

static Channel *
client_request_x11(const char *request_type, int rchan)
{
	Channel *c = NULL;
	char *originator;
	int originator_port;
	int sock;

	if (!options.forward_x11) {
		error("Warning: ssh server tried X11 forwarding.");
		error("Warning: this is probably a break in attempt by a malicious server.");
		return NULL;
	}
	originator = packet_get_string(NULL);
	if (datafellows & SSH_BUG_X11FWD) {
		debug2("buggy server: x11 request w/o originator_port");
		originator_port = 0;
	} else {
		originator_port = packet_get_int();
	}
	packet_check_eom();
	/* XXX check permission */
	debug("client_request_x11: request from %s %d", originator,
	    originator_port);
	xfree(originator);
	sock = x11_connect_display();
	if (sock < 0)
		return NULL;
	c = channel_new("x11",
	    SSH_CHANNEL_X11_OPEN, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT, 0,
	    xstrdup("x11"), 1);
	c->force_drain = 1;
	return c;
}

static Channel *
client_request_agent(const char *request_type, int rchan)
{
	Channel *c = NULL;
	int sock;

	if (!options.forward_agent) {
		error("Warning: ssh server tried agent forwarding.");
		error("Warning: this is probably a break in attempt by a malicious server.");
		return NULL;
	}
	sock =  ssh_get_authentication_socket();
	if (sock < 0)
		return NULL;
	c = channel_new("authentication agent connection",
	    SSH_CHANNEL_OPEN, sock, sock, -1,
	    CHAN_X11_WINDOW_DEFAULT, CHAN_TCP_WINDOW_DEFAULT, 0,
	    xstrdup("authentication agent connection"), 1);
	c->force_drain = 1;
	return c;
}

/* XXXX move to generic input handler */
static void
client_input_channel_open(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	char *ctype;
	int rchan;
	u_int rmaxpack, rwindow, len;

	ctype = packet_get_string(&len);
	rchan = packet_get_int();
	rwindow = packet_get_int();
	rmaxpack = packet_get_int();

	debug("client_input_channel_open: ctype %s rchan %d win %d max %d",
	    ctype, rchan, rwindow, rmaxpack);

	if (strcmp(ctype, "forwarded-tcpip") == 0) {
		c = client_request_forwarded_tcpip(ctype, rchan);
	} else if (strcmp(ctype, "x11") == 0) {
		c = client_request_x11(ctype, rchan);
	} else if (strcmp(ctype, "auth-agent@openssh.com") == 0) {
		c = client_request_agent(ctype, rchan);
	}
/* XXX duplicate : */
	if (c != NULL) {
		debug("confirm %s", ctype);
		c->remote_id = rchan;
		c->remote_window = rwindow;
		c->remote_maxpacket = rmaxpack;
		if (c->type != SSH_CHANNEL_CONNECTING) {
			packet_start(SSH2_MSG_CHANNEL_OPEN_CONFIRMATION);
			packet_put_int(c->remote_id);
			packet_put_int(c->self);
			packet_put_int(c->local_window);
			packet_put_int(c->local_maxpacket);
			packet_send();
		}
	} else {
		debug("failure %s", ctype);
		packet_start(SSH2_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(rchan);
		packet_put_int(SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED);
		if (!(datafellows & SSH_BUG_OPENFAILURE)) {
			packet_put_cstring("open failed");
			packet_put_cstring("");
		}
		packet_send();
	}
	xfree(ctype);
}
static void
client_input_channel_req(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	int id, reply, success = 0;
	char *rtype;

	id = packet_get_int();
	rtype = packet_get_string(NULL);
	reply = packet_get_char();

	debug("client_input_channel_req: channel %d rtype %s reply %d",
	    id, rtype, reply);

	if (session_ident == -1) {
		error("client_input_channel_req: no channel %d", session_ident);
	} else if (id != session_ident) {
		error("client_input_channel_req: channel %d: wrong channel: %d",
		    session_ident, id);
	}
	c = channel_lookup(id);
	if (c == NULL) {
		error("client_input_channel_req: channel %d: unknown channel", id);
	} else if (strcmp(rtype, "exit-status") == 0) {
		success = 1;
		exit_status = packet_get_int();
		packet_check_eom();
	}
	if (reply) {
		packet_start(success ?
		    SSH2_MSG_CHANNEL_SUCCESS : SSH2_MSG_CHANNEL_FAILURE);
		packet_put_int(c->remote_id);
		packet_send();
	}
	xfree(rtype);
}
static void
client_input_global_request(int type, u_int32_t seq, void *ctxt)
{
	char *rtype;
	int want_reply;
	int success = 0;

	rtype = packet_get_string(NULL);
	want_reply = packet_get_char();
	debug("client_input_global_request: rtype %s want_reply %d", rtype, want_reply);
	if (want_reply) {
		packet_start(success ?
		    SSH2_MSG_REQUEST_SUCCESS : SSH2_MSG_REQUEST_FAILURE);
		packet_send();
		packet_write_wait();
	}
	xfree(rtype);
}

static void
client_init_dispatch_20(void)
{
	dispatch_init(&dispatch_protocol_error);

	dispatch_set(SSH2_MSG_CHANNEL_CLOSE, &channel_input_oclose);
	dispatch_set(SSH2_MSG_CHANNEL_DATA, &channel_input_data);
	dispatch_set(SSH2_MSG_CHANNEL_EOF, &channel_input_ieof);
	dispatch_set(SSH2_MSG_CHANNEL_EXTENDED_DATA, &channel_input_extended_data);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN, &client_input_channel_open);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN_CONFIRMATION, &channel_input_open_confirmation);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN_FAILURE, &channel_input_open_failure);
	dispatch_set(SSH2_MSG_CHANNEL_REQUEST, &client_input_channel_req);
	dispatch_set(SSH2_MSG_CHANNEL_WINDOW_ADJUST, &channel_input_window_adjust);
	dispatch_set(SSH2_MSG_GLOBAL_REQUEST, &client_input_global_request);

	/* rekeying */
	dispatch_set(SSH2_MSG_KEXINIT, &kex_input_kexinit);

	/* global request reply messages */
	dispatch_set(SSH2_MSG_REQUEST_FAILURE, &client_global_request_reply);
	dispatch_set(SSH2_MSG_REQUEST_SUCCESS, &client_global_request_reply);
}
static void
client_init_dispatch_13(void)
{
	dispatch_init(NULL);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE, &channel_input_close);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION, &channel_input_close_confirmation);
	dispatch_set(SSH_MSG_CHANNEL_DATA, &channel_input_data);
	dispatch_set(SSH_MSG_CHANNEL_OPEN_CONFIRMATION, &channel_input_open_confirmation);
	dispatch_set(SSH_MSG_CHANNEL_OPEN_FAILURE, &channel_input_open_failure);
	dispatch_set(SSH_MSG_PORT_OPEN, &channel_input_port_open);
	dispatch_set(SSH_SMSG_EXITSTATUS, &client_input_exit_status);
	dispatch_set(SSH_SMSG_STDERR_DATA, &client_input_stderr_data);
	dispatch_set(SSH_SMSG_STDOUT_DATA, &client_input_stdout_data);

	dispatch_set(SSH_SMSG_AGENT_OPEN, options.forward_agent ?
	    &auth_input_open_request : &deny_input_open);
	dispatch_set(SSH_SMSG_X11_OPEN, options.forward_x11 ?
	    &x11_input_open : &deny_input_open);
}
static void
client_init_dispatch_15(void)
{
	client_init_dispatch_13();
	dispatch_set(SSH_MSG_CHANNEL_CLOSE, &channel_input_ieof);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION, & channel_input_oclose);
}
static void
client_init_dispatch(void)
{
	if (compat20)
		client_init_dispatch_20();
	else if (compat13)
		client_init_dispatch_13();
	else
		client_init_dispatch_15();
}
