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
 * Copyright (c) 1999,2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: clientloop.c,v 1.34 2000/09/07 20:40:30 markus Exp $");

#include "xmalloc.h"
#include "ssh.h"
#include "packet.h"
#include "buffer.h"
#include "readconf.h"

#include "ssh2.h"
#include "compat.h"
#include "channels.h"
#include "dispatch.h"

#include "buffer.h"
#include "bufaux.h"

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
static volatile int received_window_change_signal = 0;

/* Terminal modes, as saved by enter_raw_mode. */
static struct termios saved_tio;

/*
 * Flag indicating whether we are in raw mode.  This is used by
 * enter_raw_mode and leave_raw_mode.
 */
static int in_raw_mode = 0;

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
static unsigned long stdin_bytes, stdout_bytes, stderr_bytes;
static unsigned int buffer_high;/* Soft max buffer size. */
static int max_fd;		/* Maximum file descriptor number in select(). */
static int connection_in;	/* Connection to server (input). */
static int connection_out;	/* Connection to server (output). */


void	client_init_dispatch(void);
int	session_ident = -1;

/* Returns the user\'s terminal to normal mode if it had been put in raw mode. */

void
leave_raw_mode()
{
	if (!in_raw_mode)
		return;
	in_raw_mode = 0;
	if (tcsetattr(fileno(stdin), TCSADRAIN, &saved_tio) < 0)
		perror("tcsetattr");

	fatal_remove_cleanup((void (*) (void *)) leave_raw_mode, NULL);
}

/* Puts the user\'s terminal in raw mode. */

void
enter_raw_mode()
{
	struct termios tio;

	if (tcgetattr(fileno(stdin), &tio) < 0)
		perror("tcgetattr");
	saved_tio = tio;
	tio.c_iflag |= IGNPAR;
	tio.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);
	tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
#ifdef IEXTEN
	tio.c_lflag &= ~IEXTEN;
#endif				/* IEXTEN */
	tio.c_oflag &= ~OPOST;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	if (tcsetattr(fileno(stdin), TCSADRAIN, &tio) < 0)
		perror("tcsetattr");
	in_raw_mode = 1;

	fatal_add_cleanup((void (*) (void *)) leave_raw_mode, NULL);
}

/* Restores stdin to blocking mode. */

void
leave_non_blocking()
{
	if (in_non_blocking_mode) {
		(void) fcntl(fileno(stdin), F_SETFL, 0);
		in_non_blocking_mode = 0;
		fatal_remove_cleanup((void (*) (void *)) leave_non_blocking, NULL);
	}
}

/* Puts stdin terminal in non-blocking mode. */

void
enter_non_blocking()
{
	in_non_blocking_mode = 1;
	(void) fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);
	fatal_add_cleanup((void (*) (void *)) leave_non_blocking, NULL);
}

/*
 * Signal handler for the window change signal (SIGWINCH).  This just sets a
 * flag indicating that the window has changed.
 */

void
window_change_handler(int sig)
{
	received_window_change_signal = 1;
	signal(SIGWINCH, window_change_handler);
}

/*
 * Signal handler for signals that cause the program to terminate.  These
 * signals must be trapped to restore terminal modes.
 */

void
signal_handler(int sig)
{
	if (in_raw_mode)
		leave_raw_mode();
	if (in_non_blocking_mode)
		leave_non_blocking();
	channel_stop_listening();
	packet_close();
	fatal("Killed by signal %d.", sig);
}

/*
 * Returns current time in seconds from Jan 1, 1970 with the maximum
 * available resolution.
 */

double
get_current_time()
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

void
client_check_initial_eof_on_stdin()
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
			if ((unsigned char) buf[0] == escape_char)
				escape_pending = 1;
			else {
				buffer_append(&stdin_buffer, buf, 1);
				stdin_bytes += 1;
			}
		}
		leave_non_blocking();
	}
}


/*
 * Make packets from buffered stdin data, and buffer them for sending to the
 * connection.
 */

void
client_make_packets_from_stdin_data()
{
	unsigned int len;

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

void
client_check_window_change()
{
	struct winsize ws;

	if (! received_window_change_signal)
		return;
	/** XXX race */
	received_window_change_signal = 0;

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) < 0)
		return;

	debug("client_check_window_change: changed");

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

void
client_wait_until_can_do_something(fd_set * readset, fd_set * writeset)
{
	/*debug("client_wait_until_can_do_something"); */

	/* Initialize select masks. */
	FD_ZERO(readset);
	FD_ZERO(writeset);

	if (!compat20) {
		/* Read from the connection, unless our buffers are full. */
		if (buffer_len(&stdout_buffer) < buffer_high &&
		    buffer_len(&stderr_buffer) < buffer_high &&
		    channel_not_very_much_buffered_data())
			FD_SET(connection_in, readset);
		/*
		 * Read from stdin, unless we have seen EOF or have very much
		 * buffered data to send to the server.
		 */
		if (!stdin_eof && packet_not_very_much_data_to_write())
			FD_SET(fileno(stdin), readset);

		/* Select stdout/stderr if have data in buffer. */
		if (buffer_len(&stdout_buffer) > 0)
			FD_SET(fileno(stdout), writeset);
		if (buffer_len(&stderr_buffer) > 0)
			FD_SET(fileno(stderr), writeset);
	} else {
		FD_SET(connection_in, readset);
	}

	/* Add any selections by the channel mechanism. */
	channel_prepare_select(readset, writeset);

	/* Select server connection if have data to write to the server. */
	if (packet_have_data_to_write())
		FD_SET(connection_out, writeset);

/* move UP XXX */
	/* Update maximum file descriptor number, if appropriate. */
	if (channel_max_fd() > max_fd)
		max_fd = channel_max_fd();

	/*
	 * Wait for something to happen.  This will suspend the process until
	 * some selected descriptor can be read, written, or has some other
	 * event pending. Note: if you want to implement SSH_MSG_IGNORE
	 * messages to fool traffic analysis, this might be the place to do
	 * it: just have a random timeout for the select, and send a random
	 * SSH_MSG_IGNORE packet when the timeout expires.
	 */

	if (select(max_fd + 1, readset, writeset, NULL, NULL) < 0) {
		char buf[100];
		/* Some systems fail to clear these automatically. */
		FD_ZERO(readset);
		FD_ZERO(writeset);
		if (errno == EINTR)
			return;
		/* Note: we might still have data in the buffers. */
		snprintf(buf, sizeof buf, "select: %s\r\n", strerror(errno));
		buffer_append(&stderr_buffer, buf, strlen(buf));
		stderr_bytes += strlen(buf);
		quit_pending = 1;
	}
}

void
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

void
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
/*debug("read connection_in len %d", len); XXX */
		if (len == 0) {
			/* Received EOF.  The remote host has closed the connection. */
			snprintf(buf, sizeof buf, "Connection to %.300s closed by remote host.\r\n",
				 host);
			buffer_append(&stderr_buffer, buf, strlen(buf));
			stderr_bytes += strlen(buf);
			quit_pending = 1;
			return;
		}
		/*
		 * There is a kernel bug on Solaris that causes select to
		 * sometimes wake up even though there is no data available.
		 */
		if (len < 0 && errno == EAGAIN)
			len = 0;

		if (len < 0) {
			/* An error has encountered.  Perhaps there is a network problem. */
			snprintf(buf, sizeof buf, "Read from remote host %.300s: %.100s\r\n",
				 host, strerror(errno));
			buffer_append(&stderr_buffer, buf, strlen(buf));
			stderr_bytes += strlen(buf);
			quit_pending = 1;
			return;
		}
		packet_process_incoming(buf, len);
	}
}

/* process the characters one by one */
int
process_escapes(Buffer *bin, Buffer *bout, Buffer *berr, char *buf, int len)
{
	char string[1024];
	pid_t pid;
	int bytes = 0;
	unsigned int i;
	unsigned char ch;
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
				/*stderr_bytes += strlen(string); XXX*/

				quit_pending = 1;
				return -1;

			case 'Z' - 64:
				/* Suspend the program. */
				/* Print a message to that effect to the user. */
				snprintf(string, sizeof string, "%c^Z [suspend ssh]\r\n", escape_char);
				buffer_append(berr, string, strlen(string));
				/*stderr_bytes += strlen(string); XXX*/

				/* Restore terminal modes and suspend. */
				client_suspend_self(bin, bout, berr);

				/* We have been continued. */
				continue;

			case '&':
				/* XXX does not work yet with proto 2 */
				if (compat20)
					continue;
				/*
				 * Detach the program (continue to serve connections,
				 * but put in background and no more new connections).
				 */
				if (!stdin_eof) {
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
				/* Restore tty modes. */
				leave_raw_mode();

				/* Stop listening for new connections. */
				channel_stop_listening();

				printf("%c& [backgrounded]\n", escape_char);

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
				continue; /*XXX ? */

			case '?':
				snprintf(string, sizeof string,
"%c?\r\n\
Supported escape sequences:\r\n\
~.  - terminate connection\r\n\
~^Z - suspend ssh\r\n\
~#  - list forwarded connections\r\n\
~&  - background ssh (when waiting for connections to terminate)\r\n\
~?  - this message\r\n\
~~  - send the escape character by typing it twice\r\n\
(Note that escapes are only recognized immediately after newline.)\r\n",
					 escape_char);
				buffer_append(berr, string, strlen(string));
				continue;

			case '#':
				snprintf(string, sizeof string, "%c#\r\n", escape_char);
				buffer_append(berr, string, strlen(string));
				s = channel_open_message();
				buffer_append(berr, s, strlen(s));
				xfree(s);
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

void
client_process_input(fd_set * readset)
{
	int ret;
	int len;
	char buf[8192];

	/* Read input from stdin. */
	if (FD_ISSET(fileno(stdin), readset)) {
		/* Read as much as possible. */
		len = read(fileno(stdin), buf, sizeof(buf));
		if (len <= 0) {
			/*
			 * Received EOF or error.  They are treated
			 * similarly, except that an error message is printed
			 * if it was an error condition.
			 */
			if (len < 0) {
				snprintf(buf, sizeof buf, "read: %.100s\r\n", strerror(errno));
				buffer_append(&stderr_buffer, buf, strlen(buf));
				stderr_bytes += strlen(buf);
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
		} else if (escape_char == -1) {
			/*
			 * Normal successful read, and no escape character.
			 * Just append the data to buffer.
			 */
			buffer_append(&stdin_buffer, buf, len);
			stdin_bytes += len;
		} else {
			/*
			 * Normal, successful read.  But we have an escape character
			 * and have to process the characters one by one.
			 */
			ret = process_escapes(&stdin_buffer, &stdout_buffer, &stderr_buffer, buf, len);
			if (ret == -1)
				return;
			stdout_bytes += ret;
		}
	}
}

void
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
			if (errno == EAGAIN)
				len = 0;
			else {
				/*
				 * An error or EOF was encountered.  Put an
				 * error message to stderr buffer.
				 */
				snprintf(buf, sizeof buf, "write stdout: %.50s\r\n", strerror(errno));
				buffer_append(&stderr_buffer, buf, strlen(buf));
				stderr_bytes += strlen(buf);
				quit_pending = 1;
				return;
			}
		}
		/* Consume printed data from the buffer. */
		buffer_consume(&stdout_buffer, len);
	}
	/* Write buffered output to stderr. */
	if (FD_ISSET(fileno(stderr), writeset)) {
		/* Write as much data as possible. */
		len = write(fileno(stderr), buffer_ptr(&stderr_buffer),
		    buffer_len(&stderr_buffer));
		if (len <= 0) {
			if (errno == EAGAIN)
				len = 0;
			else {
				/* EOF or error, but can't even print error message. */
				quit_pending = 1;
				return;
			}
		}
		/* Consume printed characters from the buffer. */
		buffer_consume(&stderr_buffer, len);
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

void
client_process_buffered_input_packets()
{
	dispatch_run(DISPATCH_NONBLOCK, &quit_pending);
}

/* scan buf[] for '~' before sending data to the peer */

int
simple_escape_filter(Channel *c, char *buf, int len)
{
	/* XXX we assume c->extended is writeable */
	return process_escapes(&c->input, &c->output, &c->extended, buf, len);
}

/*
 * Implements the interactive session with the server.  This is called after
 * the user has been authenticated, and a command has been started on the
 * remote host.  If escape_char != -1, it is the character used as an escape
 * character for terminating or suspending the session.
 */

int
client_loop(int have_pty, int escape_char_arg, int ssh2_chan_id)
{
	extern Options options;
	double start_time, total_time;
	int len;
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
	max_fd = connection_in;
	if (connection_out > max_fd)
		max_fd = connection_out;
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
	signal(SIGPIPE, SIG_IGN);
	if (have_pty)
		signal(SIGWINCH, window_change_handler);

	if (have_pty)
		enter_raw_mode();

	/* Check if we should immediately send eof on stdin. */
	if (!compat20)
		client_check_initial_eof_on_stdin();

	if (compat20 && escape_char != -1)
		channel_register_filter(ssh2_chan_id, simple_escape_filter);

	/* Main loop of the client for the interactive session mode. */
	while (!quit_pending) {
		fd_set readset, writeset;

		/* Process buffered packets sent by the server. */
		client_process_buffered_input_packets();

		if (compat20 && !channel_still_open()) {
			debug("!channel_still_open.");
			break;
		}

		/*
		 * Make packets of buffered stdin data, and buffer them for
		 * sending to the server.
		 */
		if (!compat20)
			client_make_packets_from_stdin_data();

		/*
		 * Make packets from buffered channel data, and buffer them
		 * for sending to the server.
		 */
		if (packet_not_very_much_data_to_write())
			channel_output_poll();

		/*
		 * Check if the window size has changed, and buffer a message
		 * about it to the server if so.
		 */
		client_check_window_change();

		if (quit_pending)
			break;

		/*
		 * Wait until we have something to do (something becomes
		 * available on one of the descriptors).
		 */
		client_wait_until_can_do_something(&readset, &writeset);

		if (quit_pending)
			break;

		/* Do channel operations. */
		channel_after_select(&readset, &writeset);

		/* Buffer input from the connection.  */
		client_process_net_input(&readset);

		if (quit_pending)
			break;

		if (!compat20) {
			/* Buffer data from stdin */
			client_process_input(&readset);
			/*
			 * Process output to stdout and stderr.  Output to
			 * the connection is processed elsewhere (above).
			 */
			client_process_output(&writeset);
		}

		/* Send as much buffered packet data as possible to the sender. */
		if (FD_ISSET(connection_out, &writeset))
			packet_write_poll();
	}

	/* Terminate the session. */

	/* Stop watching for window change. */
	if (have_pty)
		signal(SIGWINCH, SIG_DFL);

	/* Stop listening for connections. */
	channel_stop_listening();

	/*
	 * In interactive mode (with pseudo tty) display a message indicating
	 * that the connection has been closed.
	 */
	if (have_pty && options.log_level != SYSLOG_LEVEL_QUIET) {
		snprintf(buf, sizeof buf, "Connection to %.64s closed.\r\n", host);
		buffer_append(&stderr_buffer, buf, strlen(buf));
		stderr_bytes += strlen(buf);
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
	}

	if (have_pty)
		leave_raw_mode();

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

void
client_input_stdout_data(int type, int plen)
{
	unsigned int data_len;
	char *data = packet_get_string(&data_len);
	packet_integrity_check(plen, 4 + data_len, type);
	buffer_append(&stdout_buffer, data, data_len);
	stdout_bytes += data_len;
	memset(data, 0, data_len);
	xfree(data);
}
void
client_input_stderr_data(int type, int plen)
{
	unsigned int data_len;
	char *data = packet_get_string(&data_len);
	packet_integrity_check(plen, 4 + data_len, type);
	buffer_append(&stderr_buffer, data, data_len);
	stdout_bytes += data_len;
	memset(data, 0, data_len);
	xfree(data);
}
void
client_input_exit_status(int type, int plen)
{
	packet_integrity_check(plen, 4, type);
	exit_status = packet_get_int();
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

/* XXXX move to generic input handler */
void
client_input_channel_open(int type, int plen)
{
	Channel *c = NULL;
	char *ctype;
	int id;
	unsigned int len;
	int rchan;
	int rmaxpack;
	int rwindow;

	ctype = packet_get_string(&len);
	rchan = packet_get_int();
	rwindow = packet_get_int();
	rmaxpack = packet_get_int();

	debug("client_input_channel_open: ctype %s rchan %d win %d max %d",
	    ctype, rchan, rwindow, rmaxpack);

	if (strcmp(ctype, "x11") == 0) {
		int sock;
		char *originator;
		int originator_port;
		originator = packet_get_string(NULL);
		if (datafellows & SSH_BUG_X11FWD) {
			debug("buggy server: x11 request w/o originator_port");
			originator_port = 0;
		} else {
			originator_port = packet_get_int();
		}
		packet_done();
		/* XXX check permission */
		xfree(originator);
		/* XXX move to channels.c */
		sock = x11_connect_display();
		if (sock >= 0) {
			id = channel_new("x11", SSH_CHANNEL_X11_OPEN,
			    sock, sock, -1, CHAN_X11_WINDOW_DEFAULT,
			    CHAN_X11_PACKET_DEFAULT, 0, xstrdup("x11"));
			c = channel_lookup(id);
		}
	}
/* XXX duplicate : */
	if (c != NULL) {
		debug("confirm %s", ctype);
		c->remote_id = rchan;
		c->remote_window = rwindow;
		c->remote_maxpacket = rmaxpack;

		packet_start(SSH2_MSG_CHANNEL_OPEN_CONFIRMATION);
		packet_put_int(c->remote_id);
		packet_put_int(c->self);
		packet_put_int(c->local_window);
		packet_put_int(c->local_maxpacket);
		packet_send();
	} else {
		debug("failure %s", ctype);
		packet_start(SSH2_MSG_CHANNEL_OPEN_FAILURE);
		packet_put_int(rchan);
		packet_put_int(SSH2_OPEN_ADMINISTRATIVELY_PROHIBITED);
		packet_put_cstring("bla bla");
		packet_put_cstring("");
		packet_send();
	}
	xfree(ctype);
}

void
client_init_dispatch_20()
{
	dispatch_init(&dispatch_protocol_error);
	dispatch_set(SSH2_MSG_CHANNEL_CLOSE, &channel_input_oclose);
	dispatch_set(SSH2_MSG_CHANNEL_DATA, &channel_input_data);
	dispatch_set(SSH2_MSG_CHANNEL_EOF, &channel_input_ieof);
	dispatch_set(SSH2_MSG_CHANNEL_EXTENDED_DATA, &channel_input_extended_data);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN, &client_input_channel_open);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN_CONFIRMATION, &channel_input_open_confirmation);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN_FAILURE, &channel_input_open_failure);
	dispatch_set(SSH2_MSG_CHANNEL_REQUEST, &channel_input_channel_request);
	dispatch_set(SSH2_MSG_CHANNEL_WINDOW_ADJUST, &channel_input_window_adjust);
}
void
client_init_dispatch_13()
{
	dispatch_init(NULL);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE, &channel_input_close);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION, &channel_input_close_confirmation);
	dispatch_set(SSH_MSG_CHANNEL_DATA, &channel_input_data);
	dispatch_set(SSH_MSG_CHANNEL_OPEN_CONFIRMATION, &channel_input_open_confirmation);
	dispatch_set(SSH_MSG_CHANNEL_OPEN_FAILURE, &channel_input_open_failure);
	dispatch_set(SSH_MSG_PORT_OPEN, &channel_input_port_open);
	dispatch_set(SSH_SMSG_AGENT_OPEN, &auth_input_open_request);
	dispatch_set(SSH_SMSG_EXITSTATUS, &client_input_exit_status);
	dispatch_set(SSH_SMSG_STDERR_DATA, &client_input_stderr_data);
	dispatch_set(SSH_SMSG_STDOUT_DATA, &client_input_stdout_data);
	dispatch_set(SSH_SMSG_X11_OPEN, &x11_input_open);
}
void
client_init_dispatch_15()
{
	client_init_dispatch_13();
	dispatch_set(SSH_MSG_CHANNEL_CLOSE, &channel_input_ieof);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION, & channel_input_oclose);
}
void
client_init_dispatch()
{
	if (compat20)
		client_init_dispatch_20();
	else if (compat13)
		client_init_dispatch_13();
	else
		client_init_dispatch_15();
}

void
client_input_channel_req(int id, void *arg)
{
	Channel *c = NULL;
	unsigned int len;
	int success = 0;
	int reply;
	char *rtype;

	rtype = packet_get_string(&len);
	reply = packet_get_char();

	debug("client_input_channel_req: rtype %s reply %d", rtype, reply);

	c = channel_lookup(id);
	if (c == NULL)
		fatal("session_input_channel_req: channel %d: bad channel", id);

	if (session_ident == -1) {
		error("client_input_channel_req: no channel %d", id);
	} else if (id != session_ident) {
		error("client_input_channel_req: bad channel %d != %d",
		    id, session_ident);
	} else if (strcmp(rtype, "exit-status") == 0) {
		success = 1;
		exit_status = packet_get_int();
		packet_done();
	}
	if (reply) {
		packet_start(success ?
		    SSH2_MSG_CHANNEL_SUCCESS : SSH2_MSG_CHANNEL_FAILURE);
		packet_put_int(c->remote_id);
		packet_send();
	}
	xfree(rtype);
}

void
client_set_session_ident(int id)
{
	debug("client_set_session_ident: id %d", id);
	session_ident = id;
	channel_register_callback(id, SSH2_MSG_CHANNEL_REQUEST,
	    client_input_channel_req, (void *)0);
}
