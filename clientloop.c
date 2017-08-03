/* $OpenBSD: clientloop.c,v 1.291 2017/03/10 05:01:13 djm Exp $ */
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

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "ssh.h"
#include "ssh1.h"
#include "ssh2.h"
#include "packet.h"
#include "buffer.h"
#include "compat.h"
#include "channels.h"
#include "dispatch.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "myproposal.h"
#include "log.h"
#include "misc.h"
#include "readconf.h"
#include "clientloop.h"
#include "sshconnect.h"
#include "authfd.h"
#include "atomicio.h"
#include "sshpty.h"
#include "match.h"
#include "msg.h"
#include "ssherr.h"
#include "hostfile.h"

/* import options */
extern Options options;

/* Flag indicating that stdin should be redirected from /dev/null. */
extern int stdin_null_flag;

/* Flag indicating that no shell has been requested */
extern int no_shell_flag;

/* Flag indicating that ssh should daemonise after authentication is complete */
extern int fork_after_authentication_flag;

/* Control socket */
extern int muxserver_sock; /* XXX use mux_client_cleanup() instead */

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

/* Flag indicating whether the user's terminal is in non-blocking mode. */
static int in_non_blocking_mode = 0;

/* Time when backgrounded control master using ControlPersist should exit */
static time_t control_persist_exit_time = 0;

/* Common data for the client loop code. */
volatile sig_atomic_t quit_pending; /* Set non-zero to quit the loop. */
static int escape_char1;	/* Escape character. (proto1 only) */
static int escape_pending1;	/* Last character was an escape (proto1 only) */
static int last_was_cr;		/* Last character was a newline. */
static int exit_status;		/* Used to store the command exit status. */
static int stdin_eof;		/* EOF has been encountered on stderr. */
static Buffer stdin_buffer;	/* Buffer for stdin data. */
static Buffer stdout_buffer;	/* Buffer for stdout data. */
static Buffer stderr_buffer;	/* Buffer for stderr data. */
static u_int buffer_high;	/* Soft max buffer size. */
static int connection_in;	/* Connection to server (input). */
static int connection_out;	/* Connection to server (output). */
static int need_rekeying;	/* Set to non-zero if rekeying is requested. */
static int session_closed;	/* In SSH2: login session closed. */
static u_int x11_refuse_time;	/* If >0, refuse x11 opens after this time. */

static void client_init_dispatch(void);
int	session_ident = -1;

/* Track escape per proto2 channel */
struct escape_filter_ctx {
	int escape_pending;
	int escape_char;
};

/* Context for channel confirmation replies */
struct channel_reply_ctx {
	const char *request_type;
	int id;
	enum confirm_action action;
};

/* Global request success/failure callbacks */
struct global_confirm {
	TAILQ_ENTRY(global_confirm) entry;
	global_confirm_cb *cb;
	void *ctx;
	int ref_count;
};
TAILQ_HEAD(global_confirms, global_confirm);
static struct global_confirms global_confirms =
    TAILQ_HEAD_INITIALIZER(global_confirms);

void ssh_process_session2_setup(int, int, int, Buffer *);

/* Restores stdin to blocking mode. */

static void
leave_non_blocking(void)
{
	if (in_non_blocking_mode) {
		unset_nonblock(fileno(stdin));
		in_non_blocking_mode = 0;
	}
}

/* Puts stdin terminal in non-blocking mode. */

static void
enter_non_blocking(void)
{
	in_non_blocking_mode = 1;
	set_nonblock(fileno(stdin));
}

/*
 * Signal handler for the window change signal (SIGWINCH).  This just sets a
 * flag indicating that the window has changed.
 */
/*ARGSUSED */
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
/*ARGSUSED */
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
 * Sets control_persist_exit_time to the absolute time when the
 * backgrounded control master should exit due to expiry of the
 * ControlPersist timeout.  Sets it to 0 if we are not a backgrounded
 * control master process, or if there is no ControlPersist timeout.
 */
static void
set_control_persist_exit_time(void)
{
	if (muxserver_sock == -1 || !options.control_persist
	    || options.control_persist_timeout == 0) {
		/* not using a ControlPersist timeout */
		control_persist_exit_time = 0;
	} else if (channel_still_open()) {
		/* some client connections are still open */
		if (control_persist_exit_time > 0)
			debug2("%s: cancel scheduled exit", __func__);
		control_persist_exit_time = 0;
	} else if (control_persist_exit_time <= 0) {
		/* a client connection has recently closed */
		control_persist_exit_time = monotime() +
			(time_t)options.control_persist_timeout;
		debug2("%s: schedule exit in %d seconds", __func__,
		    options.control_persist_timeout);
	}
	/* else we are already counting down to the timeout */
}

#define SSH_X11_VALID_DISPLAY_CHARS ":/.-_"
static int
client_x11_display_valid(const char *display)
{
	size_t i, dlen;

	if (display == NULL)
		return 0;

	dlen = strlen(display);
	for (i = 0; i < dlen; i++) {
		if (!isalnum((u_char)display[i]) &&
		    strchr(SSH_X11_VALID_DISPLAY_CHARS, display[i]) == NULL) {
			debug("Invalid character '%c' in DISPLAY", display[i]);
			return 0;
		}
	}
	return 1;
}

#define SSH_X11_PROTO		"MIT-MAGIC-COOKIE-1"
#define X11_TIMEOUT_SLACK	60
int
client_x11_get_proto(const char *display, const char *xauth_path,
    u_int trusted, u_int timeout, char **_proto, char **_data)
{
	char cmd[1024], line[512], xdisplay[512];
	char xauthfile[PATH_MAX], xauthdir[PATH_MAX];
	static char proto[512], data[512];
	FILE *f;
	int got_data = 0, generated = 0, do_unlink = 0, r;
	struct stat st;
	u_int now, x11_timeout_real;

	*_proto = proto;
	*_data = data;
	proto[0] = data[0] = xauthfile[0] = xauthdir[0] = '\0';

	if (!client_x11_display_valid(display)) {
		if (display != NULL)
			logit("DISPLAY \"%s\" invalid; disabling X11 forwarding",
			    display);
		return -1;
	}
	if (xauth_path != NULL && stat(xauth_path, &st) == -1) {
		debug("No xauth program.");
		xauth_path = NULL;
	}

	if (xauth_path != NULL) {
		/*
		 * Handle FamilyLocal case where $DISPLAY does
		 * not match an authorization entry.  For this we
		 * just try "xauth list unix:displaynum.screennum".
		 * XXX: "localhost" match to determine FamilyLocal
		 *      is not perfect.
		 */
		if (strncmp(display, "localhost:", 10) == 0) {
			if ((r = snprintf(xdisplay, sizeof(xdisplay), "unix:%s",
			    display + 10)) < 0 ||
			    (size_t)r >= sizeof(xdisplay)) {
				error("%s: display name too long", __func__);
				return -1;
			}
			display = xdisplay;
		}
		if (trusted == 0) {
			/*
			 * Generate an untrusted X11 auth cookie.
			 *
			 * The authentication cookie should briefly outlive
			 * ssh's willingness to forward X11 connections to
			 * avoid nasty fail-open behaviour in the X server.
			 */
			mktemp_proto(xauthdir, sizeof(xauthdir));
			if (mkdtemp(xauthdir) == NULL) {
				error("%s: mkdtemp: %s",
				    __func__, strerror(errno));
				return -1;
			}
			do_unlink = 1;
			if ((r = snprintf(xauthfile, sizeof(xauthfile),
			    "%s/xauthfile", xauthdir)) < 0 ||
			    (size_t)r >= sizeof(xauthfile)) {
				error("%s: xauthfile path too long", __func__);
				unlink(xauthfile);
				rmdir(xauthdir);
				return -1;
			}

			if (timeout >= UINT_MAX - X11_TIMEOUT_SLACK)
				x11_timeout_real = UINT_MAX;
			else
				x11_timeout_real = timeout + X11_TIMEOUT_SLACK;
			if ((r = snprintf(cmd, sizeof(cmd),
			    "%s -f %s generate %s " SSH_X11_PROTO
			    " untrusted timeout %u 2>" _PATH_DEVNULL,
			    xauth_path, xauthfile, display,
			    x11_timeout_real)) < 0 ||
			    (size_t)r >= sizeof(cmd))
				fatal("%s: cmd too long", __func__);
			debug2("%s: %s", __func__, cmd);
			if (x11_refuse_time == 0) {
				now = monotime() + 1;
				if (UINT_MAX - timeout < now)
					x11_refuse_time = UINT_MAX;
				else
					x11_refuse_time = now + timeout;
				channel_set_x11_refuse_time(x11_refuse_time);
			}
			if (system(cmd) == 0)
				generated = 1;
		}

		/*
		 * When in untrusted mode, we read the cookie only if it was
		 * successfully generated as an untrusted one in the step
		 * above.
		 */
		if (trusted || generated) {
			snprintf(cmd, sizeof(cmd),
			    "%s %s%s list %s 2>" _PATH_DEVNULL,
			    xauth_path,
			    generated ? "-f " : "" ,
			    generated ? xauthfile : "",
			    display);
			debug2("x11_get_proto: %s", cmd);
			f = popen(cmd, "r");
			if (f && fgets(line, sizeof(line), f) &&
			    sscanf(line, "%*s %511s %511s", proto, data) == 2)
				got_data = 1;
			if (f)
				pclose(f);
		}
	}

	if (do_unlink) {
		unlink(xauthfile);
		rmdir(xauthdir);
	}

	/* Don't fall back to fake X11 data for untrusted forwarding */
	if (!trusted && !got_data) {
		error("Warning: untrusted X11 forwarding setup failed: "
		    "xauth key data not generated");
		return -1;
	}

	/*
	 * If we didn't get authentication data, just make up some
	 * data.  The forwarding code will check the validity of the
	 * response anyway, and substitute this data.  The X11
	 * server, however, will ignore this fake data and use
	 * whatever authentication mechanisms it was using otherwise
	 * for the local connection.
	 */
	if (!got_data) {
		u_int8_t rnd[16];
		u_int i;

		logit("Warning: No xauth data; "
		    "using fake authentication data for X11 forwarding.");
		strlcpy(proto, SSH_X11_PROTO, sizeof proto);
		arc4random_buf(rnd, sizeof(rnd));
		for (i = 0; i < sizeof(rnd); i++) {
			snprintf(data + 2 * i, sizeof data - 2 * i, "%02x",
			    rnd[i]);
		}
	}

	return 0;
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
			/*
			 * EOF.  Record that we have seen it and send
			 * EOF to server.
			 */
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
			if ((u_char) buf[0] == escape_char1)
				escape_pending1 = 1;
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

	debug2("client_check_window_change: changed");

	if (compat20) {
		channel_send_window_changes();
	} else {
		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) < 0)
			return;
		packet_start(SSH_CMSG_WINDOW_SIZE);
		packet_put_int((u_int)ws.ws_row);
		packet_put_int((u_int)ws.ws_col);
		packet_put_int((u_int)ws.ws_xpixel);
		packet_put_int((u_int)ws.ws_ypixel);
		packet_send();
	}
}

static int
client_global_request_reply(int type, u_int32_t seq, void *ctxt)
{
	struct global_confirm *gc;

	if ((gc = TAILQ_FIRST(&global_confirms)) == NULL)
		return 0;
	if (gc->cb != NULL)
		gc->cb(type, seq, gc->ctx);
	if (--gc->ref_count <= 0) {
		TAILQ_REMOVE(&global_confirms, gc, entry);
		explicit_bzero(gc, sizeof(*gc));
		free(gc);
	}

	packet_set_alive_timeouts(0);
	return 0;
}

static void
server_alive_check(void)
{
	if (packet_inc_alive_timeouts() > options.server_alive_count_max) {
		logit("Timeout, server %s not responding.", host);
		cleanup_exit(255);
	}
	packet_start(SSH2_MSG_GLOBAL_REQUEST);
	packet_put_cstring("keepalive@openssh.com");
	packet_put_char(1);     /* boolean: want reply */
	packet_send();
	/* Insert an empty placeholder to maintain ordering */
	client_register_global_confirm(NULL, NULL);
}

/*
 * Waits until the client can do something (some data becomes available on
 * one of the file descriptors).
 */
static void
client_wait_until_can_do_something(fd_set **readsetp, fd_set **writesetp,
    int *maxfdp, u_int *nallocp, int rekeying)
{
	struct timeval tv, *tvp;
	int timeout_secs;
	time_t minwait_secs = 0, server_alive_time = 0, now = monotime();
	int ret;

	/* Add any selections by the channel mechanism. */
	channel_prepare_select(readsetp, writesetp, maxfdp, nallocp,
	    &minwait_secs, rekeying);

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
	 * event pending, or a timeout expires.
	 */

	timeout_secs = INT_MAX; /* we use INT_MAX to mean no timeout */
	if (options.server_alive_interval > 0 && compat20) {
		timeout_secs = options.server_alive_interval;
		server_alive_time = now + options.server_alive_interval;
	}
	if (options.rekey_interval > 0 && compat20 && !rekeying)
		timeout_secs = MINIMUM(timeout_secs, packet_get_rekey_timeout());
	set_control_persist_exit_time();
	if (control_persist_exit_time > 0) {
		timeout_secs = MINIMUM(timeout_secs,
			control_persist_exit_time - now);
		if (timeout_secs < 0)
			timeout_secs = 0;
	}
	if (minwait_secs != 0)
		timeout_secs = MINIMUM(timeout_secs, (int)minwait_secs);
	if (timeout_secs == INT_MAX)
		tvp = NULL;
	else {
		tv.tv_sec = timeout_secs;
		tv.tv_usec = 0;
		tvp = &tv;
	}

	ret = select((*maxfdp)+1, *readsetp, *writesetp, NULL, tvp);
	if (ret < 0) {
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
	} else if (ret == 0) {
		/*
		 * Timeout.  Could have been either keepalive or rekeying.
		 * Keepalive we check here, rekeying is checked in clientloop.
		 */
		if (server_alive_time != 0 && server_alive_time <= monotime())
			server_alive_check();
	}

}

static void
client_suspend_self(Buffer *bin, Buffer *bout, Buffer *berr)
{
	/* Flush stdout and stderr buffers. */
	if (buffer_len(bout) > 0)
		atomicio(vwrite, fileno(stdout), buffer_ptr(bout),
		    buffer_len(bout));
	if (buffer_len(berr) > 0)
		atomicio(vwrite, fileno(stderr), buffer_ptr(berr),
		    buffer_len(berr));

	leave_raw_mode(options.request_tty == REQUEST_TTY_FORCE);

	/*
	 * Free (and clear) the buffer to reduce the amount of data that gets
	 * written to swap.
	 */
	buffer_free(bin);
	buffer_free(bout);
	buffer_free(berr);

	/* Send the suspend signal to the program itself. */
	kill(getpid(), SIGTSTP);

	/* Reset window sizes in case they have changed */
	received_window_change_signal = 1;

	/* OK, we have been continued by the user. Reinitialize buffers. */
	buffer_init(bin);
	buffer_init(bout);
	buffer_init(berr);

	enter_raw_mode(options.request_tty == REQUEST_TTY_FORCE);
}

static void
client_process_net_input(fd_set *readset)
{
	int len;
	char buf[SSH_IOBUFSZ];

	/*
	 * Read input from the server, and add any such data to the buffer of
	 * the packet subsystem.
	 */
	if (FD_ISSET(connection_in, readset)) {
		/* Read as much as possible. */
		len = read(connection_in, buf, sizeof(buf));
		if (len == 0) {
			/*
			 * Received EOF.  The remote host has closed the
			 * connection.
			 */
			snprintf(buf, sizeof buf,
			    "Connection to %.300s closed by remote host.\r\n",
			    host);
			buffer_append(&stderr_buffer, buf, strlen(buf));
			quit_pending = 1;
			return;
		}
		/*
		 * There is a kernel bug on Solaris that causes select to
		 * sometimes wake up even though there is no data available.
		 */
		if (len < 0 &&
		    (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK))
			len = 0;

		if (len < 0) {
			/*
			 * An error has encountered.  Perhaps there is a
			 * network problem.
			 */
			snprintf(buf, sizeof buf,
			    "Read from remote host %.300s: %.100s\r\n",
			    host, strerror(errno));
			buffer_append(&stderr_buffer, buf, strlen(buf));
			quit_pending = 1;
			return;
		}
		packet_process_incoming(buf, len);
	}
}

static void
client_status_confirm(int type, Channel *c, void *ctx)
{
	struct channel_reply_ctx *cr = (struct channel_reply_ctx *)ctx;
	char errmsg[256];
	int tochan;

	/*
	 * If a TTY was explicitly requested, then a failure to allocate
	 * one is fatal.
	 */
	if (cr->action == CONFIRM_TTY &&
	    (options.request_tty == REQUEST_TTY_FORCE ||
	    options.request_tty == REQUEST_TTY_YES))
		cr->action = CONFIRM_CLOSE;

	/* XXX supress on mux _client_ quietmode */
	tochan = options.log_level >= SYSLOG_LEVEL_ERROR &&
	    c->ctl_chan != -1 && c->extended_usage == CHAN_EXTENDED_WRITE;

	if (type == SSH2_MSG_CHANNEL_SUCCESS) {
		debug2("%s request accepted on channel %d",
		    cr->request_type, c->self);
	} else if (type == SSH2_MSG_CHANNEL_FAILURE) {
		if (tochan) {
			snprintf(errmsg, sizeof(errmsg),
			    "%s request failed\r\n", cr->request_type);
		} else {
			snprintf(errmsg, sizeof(errmsg),
			    "%s request failed on channel %d",
			    cr->request_type, c->self);
		}
		/* If error occurred on primary session channel, then exit */
		if (cr->action == CONFIRM_CLOSE && c->self == session_ident)
			fatal("%s", errmsg);
		/*
		 * If error occurred on mux client, append to
		 * their stderr.
		 */
		if (tochan) {
			buffer_append(&c->extended, errmsg,
			    strlen(errmsg));
		} else
			error("%s", errmsg);
		if (cr->action == CONFIRM_TTY) {
			/*
			 * If a TTY allocation error occurred, then arrange
			 * for the correct TTY to leave raw mode.
			 */
			if (c->self == session_ident)
				leave_raw_mode(0);
			else
				mux_tty_alloc_failed(c);
		} else if (cr->action == CONFIRM_CLOSE) {
			chan_read_failed(c);
			chan_write_failed(c);
		}
	}
	free(cr);
}

static void
client_abandon_status_confirm(Channel *c, void *ctx)
{
	free(ctx);
}

void
client_expect_confirm(int id, const char *request,
    enum confirm_action action)
{
	struct channel_reply_ctx *cr = xcalloc(1, sizeof(*cr));

	cr->request_type = request;
	cr->action = action;

	channel_register_status_confirm(id, client_status_confirm,
	    client_abandon_status_confirm, cr);
}

void
client_register_global_confirm(global_confirm_cb *cb, void *ctx)
{
	struct global_confirm *gc, *last_gc;

	/* Coalesce identical callbacks */
	last_gc = TAILQ_LAST(&global_confirms, global_confirms);
	if (last_gc && last_gc->cb == cb && last_gc->ctx == ctx) {
		if (++last_gc->ref_count >= INT_MAX)
			fatal("%s: last_gc->ref_count = %d",
			    __func__, last_gc->ref_count);
		return;
	}

	gc = xcalloc(1, sizeof(*gc));
	gc->cb = cb;
	gc->ctx = ctx;
	gc->ref_count = 1;
	TAILQ_INSERT_TAIL(&global_confirms, gc, entry);
}

static void
process_cmdline(void)
{
	void (*handler)(int);
	char *s, *cmd;
	int ok, delete = 0, local = 0, remote = 0, dynamic = 0;
	struct Forward fwd;

	memset(&fwd, 0, sizeof(fwd));

	leave_raw_mode(options.request_tty == REQUEST_TTY_FORCE);
	handler = signal(SIGINT, SIG_IGN);
	cmd = s = read_passphrase("\r\nssh> ", RP_ECHO);
	if (s == NULL)
		goto out;
	while (isspace((u_char)*s))
		s++;
	if (*s == '-')
		s++;	/* Skip cmdline '-', if any */
	if (*s == '\0')
		goto out;

	if (*s == 'h' || *s == 'H' || *s == '?') {
		logit("Commands:");
		logit("      -L[bind_address:]port:host:hostport    "
		    "Request local forward");
		logit("      -R[bind_address:]port:host:hostport    "
		    "Request remote forward");
		logit("      -D[bind_address:]port                  "
		    "Request dynamic forward");
		logit("      -KL[bind_address:]port                 "
		    "Cancel local forward");
		logit("      -KR[bind_address:]port                 "
		    "Cancel remote forward");
		logit("      -KD[bind_address:]port                 "
		    "Cancel dynamic forward");
		if (!options.permit_local_command)
			goto out;
		logit("      !args                                  "
		    "Execute local command");
		goto out;
	}

	if (*s == '!' && options.permit_local_command) {
		s++;
		ssh_local_cmd(s);
		goto out;
	}

	if (*s == 'K') {
		delete = 1;
		s++;
	}
	if (*s == 'L')
		local = 1;
	else if (*s == 'R')
		remote = 1;
	else if (*s == 'D')
		dynamic = 1;
	else {
		logit("Invalid command.");
		goto out;
	}

	if (delete && !compat20) {
		logit("Not supported for SSH protocol version 1.");
		goto out;
	}

	while (isspace((u_char)*++s))
		;

	/* XXX update list of forwards in options */
	if (delete) {
		/* We pass 1 for dynamicfwd to restrict to 1 or 2 fields. */
		if (!parse_forward(&fwd, s, 1, 0)) {
			logit("Bad forwarding close specification.");
			goto out;
		}
		if (remote)
			ok = channel_request_rforward_cancel(&fwd) == 0;
		else if (dynamic)
			ok = channel_cancel_lport_listener(&fwd,
			    0, &options.fwd_opts) > 0;
		else
			ok = channel_cancel_lport_listener(&fwd,
			    CHANNEL_CANCEL_PORT_STATIC,
			    &options.fwd_opts) > 0;
		if (!ok) {
			logit("Unknown port forwarding.");
			goto out;
		}
		logit("Canceled forwarding.");
	} else {
		if (!parse_forward(&fwd, s, dynamic, remote)) {
			logit("Bad forwarding specification.");
			goto out;
		}
		if (local || dynamic) {
			if (!channel_setup_local_fwd_listener(&fwd,
			    &options.fwd_opts)) {
				logit("Port forwarding failed.");
				goto out;
			}
		} else {
			if (channel_request_remote_forwarding(&fwd) < 0) {
				logit("Port forwarding failed.");
				goto out;
			}
		}
		logit("Forwarding port.");
	}

out:
	signal(SIGINT, handler);
	enter_raw_mode(options.request_tty == REQUEST_TTY_FORCE);
	free(cmd);
	free(fwd.listen_host);
	free(fwd.listen_path);
	free(fwd.connect_host);
	free(fwd.connect_path);
}

/* reasons to suppress output of an escape command in help output */
#define SUPPRESS_NEVER		0	/* never suppress, always show */
#define SUPPRESS_PROTO1		1	/* don't show in protocol 1 sessions */
#define SUPPRESS_MUXCLIENT	2	/* don't show in mux client sessions */
#define SUPPRESS_MUXMASTER	4	/* don't show in mux master sessions */
#define SUPPRESS_SYSLOG		8	/* don't show when logging to syslog */
struct escape_help_text {
	const char *cmd;
	const char *text;
	unsigned int flags;
};
static struct escape_help_text esc_txt[] = {
    {".",  "terminate session", SUPPRESS_MUXMASTER},
    {".",  "terminate connection (and any multiplexed sessions)",
	SUPPRESS_MUXCLIENT},
    {"B",  "send a BREAK to the remote system", SUPPRESS_PROTO1},
    {"C",  "open a command line", SUPPRESS_MUXCLIENT},
    {"R",  "request rekey", SUPPRESS_PROTO1},
    {"V/v",  "decrease/increase verbosity (LogLevel)", SUPPRESS_MUXCLIENT},
    {"^Z", "suspend ssh", SUPPRESS_MUXCLIENT},
    {"#",  "list forwarded connections", SUPPRESS_NEVER},
    {"&",  "background ssh (when waiting for connections to terminate)",
	SUPPRESS_MUXCLIENT},
    {"?", "this message", SUPPRESS_NEVER},
};

static void
print_escape_help(Buffer *b, int escape_char, int protocol2, int mux_client,
    int using_stderr)
{
	unsigned int i, suppress_flags;
	char string[1024];

	snprintf(string, sizeof string, "%c?\r\n"
	    "Supported escape sequences:\r\n", escape_char);
	buffer_append(b, string, strlen(string));

	suppress_flags = (protocol2 ? 0 : SUPPRESS_PROTO1) |
	    (mux_client ? SUPPRESS_MUXCLIENT : 0) |
	    (mux_client ? 0 : SUPPRESS_MUXMASTER) |
	    (using_stderr ? 0 : SUPPRESS_SYSLOG);

	for (i = 0; i < sizeof(esc_txt)/sizeof(esc_txt[0]); i++) {
		if (esc_txt[i].flags & suppress_flags)
			continue;
		snprintf(string, sizeof string, " %c%-3s - %s\r\n",
		    escape_char, esc_txt[i].cmd, esc_txt[i].text);
		buffer_append(b, string, strlen(string));
	}

	snprintf(string, sizeof string,
	    " %c%c   - send the escape character by typing it twice\r\n"
	    "(Note that escapes are only recognized immediately after "
	    "newline.)\r\n", escape_char, escape_char);
	buffer_append(b, string, strlen(string));
}

/* 
 * Process the characters one by one, call with c==NULL for proto1 case.
 */
static int
process_escapes(Channel *c, Buffer *bin, Buffer *bout, Buffer *berr,
    char *buf, int len)
{
	char string[1024];
	pid_t pid;
	int bytes = 0;
	u_int i;
	u_char ch;
	char *s;
	int *escape_pendingp, escape_char;
	struct escape_filter_ctx *efc;

	if (c == NULL) {
		escape_pendingp = &escape_pending1;
		escape_char = escape_char1;
	} else {
		if (c->filter_ctx == NULL)
			return 0;
		efc = (struct escape_filter_ctx *)c->filter_ctx;
		escape_pendingp = &efc->escape_pending;
		escape_char = efc->escape_char;
	}
	
	if (len <= 0)
		return (0);

	for (i = 0; i < (u_int)len; i++) {
		/* Get one character at a time. */
		ch = buf[i];

		if (*escape_pendingp) {
			/* We have previously seen an escape character. */
			/* Clear the flag now. */
			*escape_pendingp = 0;

			/* Process the escaped character. */
			switch (ch) {
			case '.':
				/* Terminate the connection. */
				snprintf(string, sizeof string, "%c.\r\n",
				    escape_char);
				buffer_append(berr, string, strlen(string));

				if (c && c->ctl_chan != -1) {
					chan_read_failed(c);
					chan_write_failed(c);
					if (c->detach_user)
						c->detach_user(c->self, NULL);
					c->type = SSH_CHANNEL_ABANDONED;
					buffer_clear(&c->input);
					chan_ibuf_empty(c);
					return 0;
				} else
					quit_pending = 1;
				return -1;

			case 'Z' - 64:
				/* XXX support this for mux clients */
				if (c && c->ctl_chan != -1) {
					char b[16];
 noescape:
					if (ch == 'Z' - 64)
						snprintf(b, sizeof b, "^Z");
					else
						snprintf(b, sizeof b, "%c", ch);
					snprintf(string, sizeof string,
					    "%c%s escape not available to "
					    "multiplexed sessions\r\n",
					    escape_char, b);
					buffer_append(berr, string,
					    strlen(string));
					continue;
				}
				/* Suspend the program. Inform the user */
				snprintf(string, sizeof string,
				    "%c^Z [suspend ssh]\r\n", escape_char);
				buffer_append(berr, string, strlen(string));

				/* Restore terminal modes and suspend. */
				client_suspend_self(bin, bout, berr);

				/* We have been continued. */
				continue;

			case 'B':
				if (compat20) {
					snprintf(string, sizeof string,
					    "%cB\r\n", escape_char);
					buffer_append(berr, string,
					    strlen(string));
					channel_request_start(c->self,
					    "break", 0);
					packet_put_int(1000);
					packet_send();
				}
				continue;

			case 'R':
				if (compat20) {
					if (datafellows & SSH_BUG_NOREKEY)
						logit("Server does not "
						    "support re-keying");
					else
						need_rekeying = 1;
				}
				continue;

			case 'V':
				/* FALLTHROUGH */
			case 'v':
				if (c && c->ctl_chan != -1)
					goto noescape;
				if (!log_is_on_stderr()) {
					snprintf(string, sizeof string,
					    "%c%c [Logging to syslog]\r\n",
					     escape_char, ch);
					buffer_append(berr, string,
					    strlen(string));
					continue;
				}
				if (ch == 'V' && options.log_level >
				    SYSLOG_LEVEL_QUIET)
					log_change_level(--options.log_level);
				if (ch == 'v' && options.log_level <
				    SYSLOG_LEVEL_DEBUG3)
					log_change_level(++options.log_level);
				snprintf(string, sizeof string,
				    "%c%c [LogLevel %s]\r\n", escape_char, ch,
				    log_level_name(options.log_level));
				buffer_append(berr, string, strlen(string));
				continue;

			case '&':
				if (c && c->ctl_chan != -1)
					goto noescape;
				/*
				 * Detach the program (continue to serve
				 * connections, but put in background and no
				 * more new connections).
				 */
				/* Restore tty modes. */
				leave_raw_mode(
				    options.request_tty == REQUEST_TTY_FORCE);

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
					 * Sending SSH_CMSG_EOF alone does not
					 * always appear to be enough.  So we
					 * try to send an EOF character first.
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
				print_escape_help(berr, escape_char, compat20,
				    (c && c->ctl_chan != -1),
				    log_is_on_stderr());
				continue;

			case '#':
				snprintf(string, sizeof string, "%c#\r\n",
				    escape_char);
				buffer_append(berr, string, strlen(string));
				s = channel_open_message();
				buffer_append(berr, s, strlen(s));
				free(s);
				continue;

			case 'C':
				if (c && c->ctl_chan != -1)
					goto noescape;
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
			 * The previous character was not an escape char.
			 * Check if this is an escape.
			 */
			if (last_was_cr && ch == escape_char) {
				/*
				 * It is. Set the flag and continue to
				 * next character.
				 */
				*escape_pendingp = 1;
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
client_process_input(fd_set *readset)
{
	int len;
	char buf[SSH_IOBUFSZ];

	/* Read input from stdin. */
	if (FD_ISSET(fileno(stdin), readset)) {
		/* Read as much as possible. */
		len = read(fileno(stdin), buf, sizeof(buf));
		if (len < 0 &&
		    (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK))
			return;		/* we'll try again later */
		if (len <= 0) {
			/*
			 * Received EOF or error.  They are treated
			 * similarly, except that an error message is printed
			 * if it was an error condition.
			 */
			if (len < 0) {
				snprintf(buf, sizeof buf, "read: %.100s\r\n",
				    strerror(errno));
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
		} else if (escape_char1 == SSH_ESCAPECHAR_NONE) {
			/*
			 * Normal successful read, and no escape character.
			 * Just append the data to buffer.
			 */
			buffer_append(&stdin_buffer, buf, len);
		} else {
			/*
			 * Normal, successful read.  But we have an escape
			 * character and have to process the characters one
			 * by one.
			 */
			if (process_escapes(NULL, &stdin_buffer,
			    &stdout_buffer, &stderr_buffer, buf, len) == -1)
				return;
		}
	}
}

static void
client_process_output(fd_set *writeset)
{
	int len;
	char buf[100];

	/* Write buffered output to stdout. */
	if (FD_ISSET(fileno(stdout), writeset)) {
		/* Write as much data as possible. */
		len = write(fileno(stdout), buffer_ptr(&stdout_buffer),
		    buffer_len(&stdout_buffer));
		if (len <= 0) {
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				len = 0;
			else {
				/*
				 * An error or EOF was encountered.  Put an
				 * error message to stderr buffer.
				 */
				snprintf(buf, sizeof buf,
				    "write stdout: %.50s\r\n", strerror(errno));
				buffer_append(&stderr_buffer, buf, strlen(buf));
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
			if (errno == EINTR || errno == EAGAIN ||
			    errno == EWOULDBLOCK)
				len = 0;
			else {
				/*
				 * EOF or error, but can't even print
				 * error message.
				 */
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

static void
client_process_buffered_input_packets(void)
{
	dispatch_run(DISPATCH_NONBLOCK, &quit_pending, active_state);
}

/* scan buf[] for '~' before sending data to the peer */

/* Helper: allocate a new escape_filter_ctx and fill in its escape char */
void *
client_new_escape_filter_ctx(int escape_char)
{
	struct escape_filter_ctx *ret;

	ret = xcalloc(1, sizeof(*ret));
	ret->escape_pending = 0;
	ret->escape_char = escape_char;
	return (void *)ret;
}

/* Free the escape filter context on channel free */
void
client_filter_cleanup(int cid, void *ctx)
{
	free(ctx);
}

int
client_simple_escape_filter(Channel *c, char *buf, int len)
{
	if (c->extended_usage != CHAN_EXTENDED_WRITE)
		return 0;

	return process_escapes(c, &c->input, &c->output, &c->extended,
	    buf, len);
}

static void
client_channel_closed(int id, void *arg)
{
	channel_cancel_cleanup(id);
	session_closed = 1;
	leave_raw_mode(options.request_tty == REQUEST_TTY_FORCE);
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
	int r, max_fd = 0, max_fd2 = 0, len;
	u_int64_t ibytes, obytes;
	u_int nalloc = 0;
	char buf[100];

	debug("Entering interactive session.");

	if (options.control_master &&
	    !option_clear_or_none(options.control_path)) {
		debug("pledge: id");
		if (pledge("stdio rpath wpath cpath unix inet dns recvfd proc exec id tty",
		    NULL) == -1)
			fatal("%s pledge(): %s", __func__, strerror(errno));

	} else if (options.forward_x11 || options.permit_local_command) {
		debug("pledge: exec");
		if (pledge("stdio rpath wpath cpath unix inet dns proc exec tty",
		    NULL) == -1)
			fatal("%s pledge(): %s", __func__, strerror(errno));

	} else if (options.update_hostkeys) {
		debug("pledge: filesystem full");
		if (pledge("stdio rpath wpath cpath unix inet dns proc tty",
		    NULL) == -1)
			fatal("%s pledge(): %s", __func__, strerror(errno));

	} else if (!option_clear_or_none(options.proxy_command) ||
	    fork_after_authentication_flag) {
		debug("pledge: proc");
		if (pledge("stdio cpath unix inet dns proc tty", NULL) == -1)
			fatal("%s pledge(): %s", __func__, strerror(errno));

	} else {
		debug("pledge: network");
		if (pledge("stdio unix inet dns tty", NULL) == -1)
			fatal("%s pledge(): %s", __func__, strerror(errno));
	}

	start_time = get_current_time();

	/* Initialize variables. */
	escape_pending1 = 0;
	last_was_cr = 1;
	exit_status = -1;
	stdin_eof = 0;
	buffer_high = 64 * 1024;
	connection_in = packet_get_connection_in();
	connection_out = packet_get_connection_out();
	max_fd = MAXIMUM(connection_in, connection_out);

	if (!compat20) {
		/* enable nonblocking unless tty */
		if (!isatty(fileno(stdin)))
			set_nonblock(fileno(stdin));
		if (!isatty(fileno(stdout)))
			set_nonblock(fileno(stdout));
		if (!isatty(fileno(stderr)))
			set_nonblock(fileno(stderr));
		max_fd = MAXIMUM(max_fd, fileno(stdin));
		max_fd = MAXIMUM(max_fd, fileno(stdout));
		max_fd = MAXIMUM(max_fd, fileno(stderr));
	}
	quit_pending = 0;
	escape_char1 = escape_char_arg;

	/* Initialize buffers. */
	buffer_init(&stdin_buffer);
	buffer_init(&stdout_buffer);
	buffer_init(&stderr_buffer);

	client_init_dispatch();

	/*
	 * Set signal handlers, (e.g. to restore non-blocking mode)
	 * but don't overwrite SIG_IGN, matches behaviour from rsh(1)
	 */
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, signal_handler);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, signal_handler);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		signal(SIGQUIT, signal_handler);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, signal_handler);
	signal(SIGWINCH, window_change_handler);

	if (have_pty)
		enter_raw_mode(options.request_tty == REQUEST_TTY_FORCE);

	if (compat20) {
		session_ident = ssh2_chan_id;
		if (session_ident != -1) {
			if (escape_char_arg != SSH_ESCAPECHAR_NONE) {
				channel_register_filter(session_ident,
				    client_simple_escape_filter, NULL,
				    client_filter_cleanup,
				    client_new_escape_filter_ctx(
				    escape_char_arg));
			}
			channel_register_cleanup(session_ident,
			    client_channel_closed, 0);
		}
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

		if (ssh_packet_is_rekeying(active_state)) {
			debug("rekeying in progress");
		} else if (need_rekeying) {
			/* manual rekey request */
			debug("need rekeying");
			if ((r = kex_start_rekex(active_state)) != 0)
				fatal("%s: kex_start_rekex: %s", __func__,
				    ssh_err(r));
			need_rekeying = 0;
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
		    &max_fd2, &nalloc, ssh_packet_is_rekeying(active_state));

		if (quit_pending)
			break;

		/* Do channel operations unless rekeying in progress. */
		if (!ssh_packet_is_rekeying(active_state))
			channel_after_select(readset, writeset);

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

		/*
		 * Send as much buffered packet data as possible to the
		 * sender.
		 */
		if (FD_ISSET(connection_out, writeset))
			packet_write_poll();

		/*
		 * If we are a backgrounded control master, and the
		 * timeout has expired without any active client
		 * connections, then quit.
		 */
		if (control_persist_exit_time > 0) {
			if (monotime() >= control_persist_exit_time) {
				debug("ControlPersist timeout expired");
				break;
			}
		}
	}
	free(readset);
	free(writeset);

	/* Terminate the session. */

	/* Stop watching for window change. */
	signal(SIGWINCH, SIG_DFL);

	if (compat20) {
		packet_start(SSH2_MSG_DISCONNECT);
		packet_put_int(SSH2_DISCONNECT_BY_APPLICATION);
		packet_put_cstring("disconnected by user");
		packet_put_cstring(""); /* language tag */
		packet_send();
		packet_write_wait();
	}

	channel_free_all();

	if (have_pty)
		leave_raw_mode(options.request_tty == REQUEST_TTY_FORCE);

	/* restore blocking io */
	if (!isatty(fileno(stdin)))
		unset_nonblock(fileno(stdin));
	if (!isatty(fileno(stdout)))
		unset_nonblock(fileno(stdout));
	if (!isatty(fileno(stderr)))
		unset_nonblock(fileno(stderr));

	/*
	 * If there was no shell or command requested, there will be no remote
	 * exit status to be returned.  In that case, clear error code if the
	 * connection was deliberately terminated at this end.
	 */
	if (no_shell_flag && received_signal == SIGTERM) {
		received_signal = 0;
		exit_status = 0;
	}

	if (received_signal)
		fatal("Killed by signal %d.", (int) received_signal);

	/*
	 * In interactive mode (with pseudo tty) display a message indicating
	 * that the connection has been closed.
	 */
	if (have_pty && options.log_level != SYSLOG_LEVEL_QUIET) {
		snprintf(buf, sizeof buf,
		    "Connection to %.64s closed.\r\n", host);
		buffer_append(&stderr_buffer, buf, strlen(buf));
	}

	/* Output any buffered data for stdout. */
	if (buffer_len(&stdout_buffer) > 0) {
		len = atomicio(vwrite, fileno(stdout),
		    buffer_ptr(&stdout_buffer), buffer_len(&stdout_buffer));
		if (len < 0 || (u_int)len != buffer_len(&stdout_buffer))
			error("Write failed flushing stdout buffer.");
		else
			buffer_consume(&stdout_buffer, len);
	}

	/* Output any buffered data for stderr. */
	if (buffer_len(&stderr_buffer) > 0) {
		len = atomicio(vwrite, fileno(stderr),
		    buffer_ptr(&stderr_buffer), buffer_len(&stderr_buffer));
		if (len < 0 || (u_int)len != buffer_len(&stderr_buffer))
			error("Write failed flushing stderr buffer.");
		else
			buffer_consume(&stderr_buffer, len);
	}

	/* Clear and free any buffers. */
	explicit_bzero(buf, sizeof(buf));
	buffer_free(&stdin_buffer);
	buffer_free(&stdout_buffer);
	buffer_free(&stderr_buffer);

	/* Report bytes transferred, and transfer rates. */
	total_time = get_current_time() - start_time;
	packet_get_bytes(&ibytes, &obytes);
	verbose("Transferred: sent %llu, received %llu bytes, in %.1f seconds",
	    (unsigned long long)obytes, (unsigned long long)ibytes, total_time);
	if (total_time > 0)
		verbose("Bytes per second: sent %.1f, received %.1f",
		    obytes / total_time, ibytes / total_time);
	/* Return the exit status of the program. */
	debug("Exit status %d", exit_status);
	return exit_status;
}

/*********/

static int
client_input_stdout_data(int type, u_int32_t seq, void *ctxt)
{
	u_int data_len;
	char *data = packet_get_string(&data_len);
	packet_check_eom();
	buffer_append(&stdout_buffer, data, data_len);
	explicit_bzero(data, data_len);
	free(data);
	return 0;
}
static int
client_input_stderr_data(int type, u_int32_t seq, void *ctxt)
{
	u_int data_len;
	char *data = packet_get_string(&data_len);
	packet_check_eom();
	buffer_append(&stderr_buffer, data, data_len);
	explicit_bzero(data, data_len);
	free(data);
	return 0;
}
static int
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
	return 0;
}

static int
client_input_agent_open(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	int r, remote_id, sock;

	/* Read the remote channel number from the message. */
	remote_id = packet_get_int();
	packet_check_eom();

	/*
	 * Get a connection to the local authentication agent (this may again
	 * get forwarded).
	 */
	if ((r = ssh_get_authentication_socket(&sock)) != 0 &&
	    r != SSH_ERR_AGENT_NOT_PRESENT)
		debug("%s: ssh_get_authentication_socket: %s",
		    __func__, ssh_err(r));


	/*
	 * If we could not connect the agent, send an error message back to
	 * the server. This should never happen unless the agent dies,
	 * because authentication forwarding is only enabled if we have an
	 * agent.
	 */
	if (sock >= 0) {
		c = channel_new("", SSH_CHANNEL_OPEN, sock, sock,
		    -1, 0, 0, 0, "authentication agent connection", 1);
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
	return 0;
}

static Channel *
client_request_forwarded_tcpip(const char *request_type, int rchan,
    u_int rwindow, u_int rmaxpack)
{
	Channel *c = NULL;
	struct sshbuf *b = NULL;
	char *listen_address, *originator_address;
	u_short listen_port, originator_port;
	int r;

	/* Get rest of the packet */
	listen_address = packet_get_string(NULL);
	listen_port = packet_get_int();
	originator_address = packet_get_string(NULL);
	originator_port = packet_get_int();
	packet_check_eom();

	debug("%s: listen %s port %d, originator %s port %d", __func__,
	    listen_address, listen_port, originator_address, originator_port);

	c = channel_connect_by_listen_address(listen_address, listen_port,
	    "forwarded-tcpip", originator_address);

	if (c != NULL && c->type == SSH_CHANNEL_MUX_CLIENT) {
		if ((b = sshbuf_new()) == NULL) {
			error("%s: alloc reply", __func__);
			goto out;
		}
		/* reconstruct and send to muxclient */
		if ((r = sshbuf_put_u8(b, 0)) != 0 ||	/* padlen */
		    (r = sshbuf_put_u8(b, SSH2_MSG_CHANNEL_OPEN)) != 0 ||
		    (r = sshbuf_put_cstring(b, request_type)) != 0 ||
		    (r = sshbuf_put_u32(b, rchan)) != 0 ||
		    (r = sshbuf_put_u32(b, rwindow)) != 0 ||
		    (r = sshbuf_put_u32(b, rmaxpack)) != 0 ||
		    (r = sshbuf_put_cstring(b, listen_address)) != 0 ||
		    (r = sshbuf_put_u32(b, listen_port)) != 0 ||
		    (r = sshbuf_put_cstring(b, originator_address)) != 0 ||
		    (r = sshbuf_put_u32(b, originator_port)) != 0 ||
		    (r = sshbuf_put_stringb(&c->output, b)) != 0) {
			error("%s: compose for muxclient %s", __func__,
			    ssh_err(r));
			goto out;
		}
	}

 out:
	sshbuf_free(b);
	free(originator_address);
	free(listen_address);
	return c;
}

static Channel *
client_request_forwarded_streamlocal(const char *request_type, int rchan)
{
	Channel *c = NULL;
	char *listen_path;

	/* Get the remote path. */
	listen_path = packet_get_string(NULL);
	/* XXX: Skip reserved field for now. */
	if (packet_get_string_ptr(NULL) == NULL)
		fatal("%s: packet_get_string_ptr failed", __func__);
	packet_check_eom();

	debug("%s: %s", __func__, listen_path);

	c = channel_connect_by_listen_path(listen_path,
	    "forwarded-streamlocal@openssh.com", "forwarded-streamlocal");
	free(listen_path);
	return c;
}

static Channel *
client_request_x11(const char *request_type, int rchan)
{
	Channel *c = NULL;
	char *originator;
	u_short originator_port;
	int sock;

	if (!options.forward_x11) {
		error("Warning: ssh server tried X11 forwarding.");
		error("Warning: this is probably a break-in attempt by a "
		    "malicious server.");
		return NULL;
	}
	if (x11_refuse_time != 0 && (u_int)monotime() >= x11_refuse_time) {
		verbose("Rejected X11 connection after ForwardX11Timeout "
		    "expired");
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
	free(originator);
	sock = x11_connect_display();
	if (sock < 0)
		return NULL;
	c = channel_new("x11",
	    SSH_CHANNEL_X11_OPEN, sock, sock, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT, 0, "x11", 1);
	c->force_drain = 1;
	return c;
}

static Channel *
client_request_agent(const char *request_type, int rchan)
{
	Channel *c = NULL;
	int r, sock;

	if (!options.forward_agent) {
		error("Warning: ssh server tried agent forwarding.");
		error("Warning: this is probably a break-in attempt by a "
		    "malicious server.");
		return NULL;
	}
	if ((r = ssh_get_authentication_socket(&sock)) != 0) {
		if (r != SSH_ERR_AGENT_NOT_PRESENT)
			debug("%s: ssh_get_authentication_socket: %s",
			    __func__, ssh_err(r));
		return NULL;
	}
	c = channel_new("authentication agent connection",
	    SSH_CHANNEL_OPEN, sock, sock, -1,
	    CHAN_X11_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0,
	    "authentication agent connection", 1);
	c->force_drain = 1;
	return c;
}

int
client_request_tun_fwd(int tun_mode, int local_tun, int remote_tun)
{
	Channel *c;
	int fd;

	if (tun_mode == SSH_TUNMODE_NO)
		return 0;

	if (!compat20) {
		error("Tunnel forwarding is not supported for protocol 1");
		return -1;
	}

	debug("Requesting tun unit %d in mode %d", local_tun, tun_mode);

	/* Open local tunnel device */
	if ((fd = tun_open(local_tun, tun_mode)) == -1) {
		error("Tunnel device open failed.");
		return -1;
	}

	c = channel_new("tun", SSH_CHANNEL_OPENING, fd, fd, -1,
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0, "tun", 1);
	c->datagram = 1;

#if defined(SSH_TUN_FILTER)
	if (options.tun_open == SSH_TUNMODE_POINTOPOINT)
		channel_register_filter(c->self, sys_tun_infilter,
		    sys_tun_outfilter, NULL, NULL);
#endif

	packet_start(SSH2_MSG_CHANNEL_OPEN);
	packet_put_cstring("tun@openssh.com");
	packet_put_int(c->self);
	packet_put_int(c->local_window_max);
	packet_put_int(c->local_maxpacket);
	packet_put_int(tun_mode);
	packet_put_int(remote_tun);
	packet_send();

	return 0;
}

/* XXXX move to generic input handler */
static int
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
		c = client_request_forwarded_tcpip(ctype, rchan, rwindow,
		    rmaxpack);
	} else if (strcmp(ctype, "forwarded-streamlocal@openssh.com") == 0) {
		c = client_request_forwarded_streamlocal(ctype, rchan);
	} else if (strcmp(ctype, "x11") == 0) {
		c = client_request_x11(ctype, rchan);
	} else if (strcmp(ctype, "auth-agent@openssh.com") == 0) {
		c = client_request_agent(ctype, rchan);
	}
	if (c != NULL && c->type == SSH_CHANNEL_MUX_CLIENT) {
		debug3("proxied to downstream: %s", ctype);
	} else if (c != NULL) {
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
	free(ctype);
	return 0;
}

static int
client_input_channel_req(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	int exitval, id, reply, success = 0;
	char *rtype;

	id = packet_get_int();
	c = channel_lookup(id);
	if (channel_proxy_upstream(c, type, seq, ctxt))
		return 0;
	rtype = packet_get_string(NULL);
	reply = packet_get_char();

	debug("client_input_channel_req: channel %d rtype %s reply %d",
	    id, rtype, reply);

	if (id == -1) {
		error("client_input_channel_req: request for channel -1");
	} else if (c == NULL) {
		error("client_input_channel_req: channel %d: "
		    "unknown channel", id);
	} else if (strcmp(rtype, "eow@openssh.com") == 0) {
		packet_check_eom();
		chan_rcvd_eow(c);
	} else if (strcmp(rtype, "exit-status") == 0) {
		exitval = packet_get_int();
		if (c->ctl_chan != -1) {
			mux_exit_message(c, exitval);
			success = 1;
		} else if (id == session_ident) {
			/* Record exit value of local session */
			success = 1;
			exit_status = exitval;
		} else {
			/* Probably for a mux channel that has already closed */
			debug("%s: no sink for exit-status on channel %d",
			    __func__, id);
		}
		packet_check_eom();
	}
	if (reply && c != NULL && !(c->flags & CHAN_CLOSE_SENT)) {
		packet_start(success ?
		    SSH2_MSG_CHANNEL_SUCCESS : SSH2_MSG_CHANNEL_FAILURE);
		packet_put_int(c->remote_id);
		packet_send();
	}
	free(rtype);
	return 0;
}

struct hostkeys_update_ctx {
	/* The hostname and (optionally) IP address string for the server */
	char *host_str, *ip_str;

	/*
	 * Keys received from the server and a flag for each indicating
	 * whether they already exist in known_hosts.
	 * keys_seen is filled in by hostkeys_find() and later (for new
	 * keys) by client_global_hostkeys_private_confirm().
	 */
	struct sshkey **keys;
	int *keys_seen;
	size_t nkeys;

	size_t nnew;

	/*
	 * Keys that are in known_hosts, but were not present in the update
	 * from the server (i.e. scheduled to be deleted).
	 * Filled in by hostkeys_find().
	 */
	struct sshkey **old_keys;
	size_t nold;
};

static void
hostkeys_update_ctx_free(struct hostkeys_update_ctx *ctx)
{
	size_t i;

	if (ctx == NULL)
		return;
	for (i = 0; i < ctx->nkeys; i++)
		sshkey_free(ctx->keys[i]);
	free(ctx->keys);
	free(ctx->keys_seen);
	for (i = 0; i < ctx->nold; i++)
		sshkey_free(ctx->old_keys[i]);
	free(ctx->old_keys);
	free(ctx->host_str);
	free(ctx->ip_str);
	free(ctx);
}

static int
hostkeys_find(struct hostkey_foreach_line *l, void *_ctx)
{
	struct hostkeys_update_ctx *ctx = (struct hostkeys_update_ctx *)_ctx;
	size_t i;
	struct sshkey **tmp;

	if (l->status != HKF_STATUS_MATCHED || l->key == NULL ||
	    l->key->type == KEY_RSA1)
		return 0;

	/* Mark off keys we've already seen for this host */
	for (i = 0; i < ctx->nkeys; i++) {
		if (sshkey_equal(l->key, ctx->keys[i])) {
			debug3("%s: found %s key at %s:%ld", __func__,
			    sshkey_ssh_name(ctx->keys[i]), l->path, l->linenum);
			ctx->keys_seen[i] = 1;
			return 0;
		}
	}
	/* This line contained a key that not offered by the server */
	debug3("%s: deprecated %s key at %s:%ld", __func__,
	    sshkey_ssh_name(l->key), l->path, l->linenum);
	if ((tmp = reallocarray(ctx->old_keys, ctx->nold + 1,
	    sizeof(*ctx->old_keys))) == NULL)
		fatal("%s: reallocarray failed nold = %zu",
		    __func__, ctx->nold);
	ctx->old_keys = tmp;
	ctx->old_keys[ctx->nold++] = l->key;
	l->key = NULL;

	return 0;
}

static void
update_known_hosts(struct hostkeys_update_ctx *ctx)
{
	int r, was_raw = 0;
	int loglevel = options.update_hostkeys == SSH_UPDATE_HOSTKEYS_ASK ?
	    SYSLOG_LEVEL_INFO : SYSLOG_LEVEL_VERBOSE;
	char *fp, *response;
	size_t i;

	for (i = 0; i < ctx->nkeys; i++) {
		if (ctx->keys_seen[i] != 2)
			continue;
		if ((fp = sshkey_fingerprint(ctx->keys[i],
		    options.fingerprint_hash, SSH_FP_DEFAULT)) == NULL)
			fatal("%s: sshkey_fingerprint failed", __func__);
		do_log2(loglevel, "Learned new hostkey: %s %s",
		    sshkey_type(ctx->keys[i]), fp);
		free(fp);
	}
	for (i = 0; i < ctx->nold; i++) {
		if ((fp = sshkey_fingerprint(ctx->old_keys[i],
		    options.fingerprint_hash, SSH_FP_DEFAULT)) == NULL)
			fatal("%s: sshkey_fingerprint failed", __func__);
		do_log2(loglevel, "Deprecating obsolete hostkey: %s %s",
		    sshkey_type(ctx->old_keys[i]), fp);
		free(fp);
	}
	if (options.update_hostkeys == SSH_UPDATE_HOSTKEYS_ASK) {
		if (get_saved_tio() != NULL) {
			leave_raw_mode(1);
			was_raw = 1;
		}
		response = NULL;
		for (i = 0; !quit_pending && i < 3; i++) {
			free(response);
			response = read_passphrase("Accept updated hostkeys? "
			    "(yes/no): ", RP_ECHO);
			if (strcasecmp(response, "yes") == 0)
				break;
			else if (quit_pending || response == NULL ||
			    strcasecmp(response, "no") == 0) {
				options.update_hostkeys = 0;
				break;
			} else {
				do_log2(loglevel, "Please enter "
				    "\"yes\" or \"no\"");
			}
		}
		if (quit_pending || i >= 3 || response == NULL)
			options.update_hostkeys = 0;
		free(response);
		if (was_raw)
			enter_raw_mode(1);
	}

	/*
	 * Now that all the keys are verified, we can go ahead and replace
	 * them in known_hosts (assuming SSH_UPDATE_HOSTKEYS_ASK didn't
	 * cancel the operation).
	 */
	if (options.update_hostkeys != 0 &&
	    (r = hostfile_replace_entries(options.user_hostfiles[0],
	    ctx->host_str, ctx->ip_str, ctx->keys, ctx->nkeys,
	    options.hash_known_hosts, 0,
	    options.fingerprint_hash)) != 0)
		error("%s: hostfile_replace_entries failed: %s",
		    __func__, ssh_err(r));
}

static void
client_global_hostkeys_private_confirm(int type, u_int32_t seq, void *_ctx)
{
	struct ssh *ssh = active_state; /* XXX */
	struct hostkeys_update_ctx *ctx = (struct hostkeys_update_ctx *)_ctx;
	size_t i, ndone;
	struct sshbuf *signdata;
	int r;
	const u_char *sig;
	size_t siglen;

	if (ctx->nnew == 0)
		fatal("%s: ctx->nnew == 0", __func__); /* sanity */
	if (type != SSH2_MSG_REQUEST_SUCCESS) {
		error("Server failed to confirm ownership of "
		    "private host keys");
		hostkeys_update_ctx_free(ctx);
		return;
	}
	if ((signdata = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	/* Don't want to accidentally accept an unbound signature */
	if (ssh->kex->session_id_len == 0)
		fatal("%s: ssh->kex->session_id_len == 0", __func__);
	/*
	 * Expect a signature for each of the ctx->nnew private keys we
	 * haven't seen before. They will be in the same order as the
	 * ctx->keys where the corresponding ctx->keys_seen[i] == 0.
	 */
	for (ndone = i = 0; i < ctx->nkeys; i++) {
		if (ctx->keys_seen[i])
			continue;
		/* Prepare data to be signed: session ID, unique string, key */
		sshbuf_reset(signdata);
		if ( (r = sshbuf_put_cstring(signdata,
		    "hostkeys-prove-00@openssh.com")) != 0 ||
		    (r = sshbuf_put_string(signdata, ssh->kex->session_id,
		    ssh->kex->session_id_len)) != 0 ||
		    (r = sshkey_puts(ctx->keys[i], signdata)) != 0)
			fatal("%s: failed to prepare signature: %s",
			    __func__, ssh_err(r));
		/* Extract and verify signature */
		if ((r = sshpkt_get_string_direct(ssh, &sig, &siglen)) != 0) {
			error("%s: couldn't parse message: %s",
			    __func__, ssh_err(r));
			goto out;
		}
		if ((r = sshkey_verify(ctx->keys[i], sig, siglen,
		    sshbuf_ptr(signdata), sshbuf_len(signdata), 0)) != 0) {
			error("%s: server gave bad signature for %s key %zu",
			    __func__, sshkey_type(ctx->keys[i]), i);
			goto out;
		}
		/* Key is good. Mark it as 'seen' */
		ctx->keys_seen[i] = 2;
		ndone++;
	}
	if (ndone != ctx->nnew)
		fatal("%s: ndone != ctx->nnew (%zu / %zu)", __func__,
		    ndone, ctx->nnew);  /* Shouldn't happen */
	ssh_packet_check_eom(ssh);

	/* Make the edits to known_hosts */
	update_known_hosts(ctx);
 out:
	hostkeys_update_ctx_free(ctx);
}

/*
 * Returns non-zero if the key is accepted by HostkeyAlgorithms.
 * Made slightly less trivial by the multiple RSA signature algorithm names.
 */
static int
key_accepted_by_hostkeyalgs(const struct sshkey *key)
{
	const char *ktype = sshkey_ssh_name(key);
	const char *hostkeyalgs = options.hostkeyalgorithms != NULL ?
	    options.hostkeyalgorithms : KEX_DEFAULT_PK_ALG;

	if (key == NULL || key->type == KEY_UNSPEC)
		return 0;
	if (key->type == KEY_RSA &&
	    (match_pattern_list("rsa-sha2-256", hostkeyalgs, 0) == 1 ||
	    match_pattern_list("rsa-sha2-512", hostkeyalgs, 0) == 1))
		return 1;
	return match_pattern_list(ktype, hostkeyalgs, 0) == 1;
}

/*
 * Handle hostkeys-00@openssh.com global request to inform the client of all
 * the server's hostkeys. The keys are checked against the user's
 * HostkeyAlgorithms preference before they are accepted.
 */
static int
client_input_hostkeys(void)
{
	struct ssh *ssh = active_state; /* XXX */
	const u_char *blob = NULL;
	size_t i, len = 0;
	struct sshbuf *buf = NULL;
	struct sshkey *key = NULL, **tmp;
	int r;
	char *fp;
	static int hostkeys_seen = 0; /* XXX use struct ssh */
	extern struct sockaddr_storage hostaddr; /* XXX from ssh.c */
	struct hostkeys_update_ctx *ctx = NULL;

	if (hostkeys_seen)
		fatal("%s: server already sent hostkeys", __func__);
	if (options.update_hostkeys == SSH_UPDATE_HOSTKEYS_ASK &&
	    options.batch_mode)
		return 1; /* won't ask in batchmode, so don't even try */
	if (!options.update_hostkeys || options.num_user_hostfiles <= 0)
		return 1;

	ctx = xcalloc(1, sizeof(*ctx));
	while (ssh_packet_remaining(ssh) > 0) {
		sshkey_free(key);
		key = NULL;
		if ((r = sshpkt_get_string_direct(ssh, &blob, &len)) != 0) {
			error("%s: couldn't parse message: %s",
			    __func__, ssh_err(r));
			goto out;
		}
		if ((r = sshkey_from_blob(blob, len, &key)) != 0) {
			error("%s: parse key: %s", __func__, ssh_err(r));
			goto out;
		}
		fp = sshkey_fingerprint(key, options.fingerprint_hash,
		    SSH_FP_DEFAULT);
		debug3("%s: received %s key %s", __func__,
		    sshkey_type(key), fp);
		free(fp);

		if (!key_accepted_by_hostkeyalgs(key)) {
			debug3("%s: %s key not permitted by HostkeyAlgorithms",
			    __func__, sshkey_ssh_name(key));
			continue;
		}
		/* Skip certs */
		if (sshkey_is_cert(key)) {
			debug3("%s: %s key is a certificate; skipping",
			    __func__, sshkey_ssh_name(key));
			continue;
		}
		/* Ensure keys are unique */
		for (i = 0; i < ctx->nkeys; i++) {
			if (sshkey_equal(key, ctx->keys[i])) {
				error("%s: received duplicated %s host key",
				    __func__, sshkey_ssh_name(key));
				goto out;
			}
		}
		/* Key is good, record it */
		if ((tmp = reallocarray(ctx->keys, ctx->nkeys + 1,
		    sizeof(*ctx->keys))) == NULL)
			fatal("%s: reallocarray failed nkeys = %zu",
			    __func__, ctx->nkeys);
		ctx->keys = tmp;
		ctx->keys[ctx->nkeys++] = key;
		key = NULL;
	}

	if (ctx->nkeys == 0) {
		debug("%s: server sent no hostkeys", __func__);
		goto out;
	}

	if ((ctx->keys_seen = calloc(ctx->nkeys,
	    sizeof(*ctx->keys_seen))) == NULL)
		fatal("%s: calloc failed", __func__);

	get_hostfile_hostname_ipaddr(host,
	    options.check_host_ip ? (struct sockaddr *)&hostaddr : NULL,
	    options.port, &ctx->host_str,
	    options.check_host_ip ? &ctx->ip_str : NULL);

	/* Find which keys we already know about. */
	if ((r = hostkeys_foreach(options.user_hostfiles[0], hostkeys_find,
	    ctx, ctx->host_str, ctx->ip_str,
	    HKF_WANT_PARSE_KEY|HKF_WANT_MATCH)) != 0) {
		error("%s: hostkeys_foreach failed: %s", __func__, ssh_err(r));
		goto out;
	}

	/* Figure out if we have any new keys to add */
	ctx->nnew = 0;
	for (i = 0; i < ctx->nkeys; i++) {
		if (!ctx->keys_seen[i])
			ctx->nnew++;
	}

	debug3("%s: %zu keys from server: %zu new, %zu retained. %zu to remove",
	    __func__, ctx->nkeys, ctx->nnew, ctx->nkeys - ctx->nnew, ctx->nold);

	if (ctx->nnew == 0 && ctx->nold != 0) {
		/* We have some keys to remove. Just do it. */
		update_known_hosts(ctx);
	} else if (ctx->nnew != 0) {
		/*
		 * We have received hitherto-unseen keys from the server.
		 * Ask the server to confirm ownership of the private halves.
		 */
		debug3("%s: asking server to prove ownership for %zu keys",
		    __func__, ctx->nnew);
		if ((r = sshpkt_start(ssh, SSH2_MSG_GLOBAL_REQUEST)) != 0 ||
		    (r = sshpkt_put_cstring(ssh,
		    "hostkeys-prove-00@openssh.com")) != 0 ||
		    (r = sshpkt_put_u8(ssh, 1)) != 0) /* bool: want reply */
			fatal("%s: cannot prepare packet: %s",
			    __func__, ssh_err(r));
		if ((buf = sshbuf_new()) == NULL)
			fatal("%s: sshbuf_new", __func__);
		for (i = 0; i < ctx->nkeys; i++) {
			if (ctx->keys_seen[i])
				continue;
			sshbuf_reset(buf);
			if ((r = sshkey_putb(ctx->keys[i], buf)) != 0)
				fatal("%s: sshkey_putb: %s",
				    __func__, ssh_err(r));
			if ((r = sshpkt_put_stringb(ssh, buf)) != 0)
				fatal("%s: sshpkt_put_string: %s",
				    __func__, ssh_err(r));
		}
		if ((r = sshpkt_send(ssh)) != 0)
			fatal("%s: sshpkt_send: %s", __func__, ssh_err(r));
		client_register_global_confirm(
		    client_global_hostkeys_private_confirm, ctx);
		ctx = NULL;  /* will be freed in callback */
	}

	/* Success */
 out:
	hostkeys_update_ctx_free(ctx);
	sshkey_free(key);
	sshbuf_free(buf);
	/*
	 * NB. Return success for all cases. The server doesn't need to know
	 * what the client does with its hosts file.
	 */
	return 1;
}

static int
client_input_global_request(int type, u_int32_t seq, void *ctxt)
{
	char *rtype;
	int want_reply;
	int success = 0;

	rtype = packet_get_cstring(NULL);
	want_reply = packet_get_char();
	debug("client_input_global_request: rtype %s want_reply %d",
	    rtype, want_reply);
	if (strcmp(rtype, "hostkeys-00@openssh.com") == 0)
		success = client_input_hostkeys();
	if (want_reply) {
		packet_start(success ?
		    SSH2_MSG_REQUEST_SUCCESS : SSH2_MSG_REQUEST_FAILURE);
		packet_send();
		packet_write_wait();
	}
	free(rtype);
	return 0;
}

void
client_session2_setup(int id, int want_tty, int want_subsystem,
    const char *term, struct termios *tiop, int in_fd, Buffer *cmd, char **env)
{
	int len;
	Channel *c = NULL;

	debug2("%s: id %d", __func__, id);

	if ((c = channel_lookup(id)) == NULL)
		fatal("client_session2_setup: channel %d: unknown channel", id);

	packet_set_interactive(want_tty,
	    options.ip_qos_interactive, options.ip_qos_bulk);

	if (want_tty) {
		struct winsize ws;

		/* Store window size in the packet. */
		if (ioctl(in_fd, TIOCGWINSZ, &ws) < 0)
			memset(&ws, 0, sizeof(ws));

		channel_request_start(id, "pty-req", 1);
		client_expect_confirm(id, "PTY allocation", CONFIRM_TTY);
		packet_put_cstring(term != NULL ? term : "");
		packet_put_int((u_int)ws.ws_col);
		packet_put_int((u_int)ws.ws_row);
		packet_put_int((u_int)ws.ws_xpixel);
		packet_put_int((u_int)ws.ws_ypixel);
		if (tiop == NULL)
			tiop = get_saved_tio();
		tty_make_modes(-1, tiop);
		packet_send();
		/* XXX wait for reply */
		c->client_tty = 1;
	}

	/* Transfer any environment variables from client to server */
	if (options.num_send_env != 0 && env != NULL) {
		int i, j, matched;
		char *name, *val;

		debug("Sending environment.");
		for (i = 0; env[i] != NULL; i++) {
			/* Split */
			name = xstrdup(env[i]);
			if ((val = strchr(name, '=')) == NULL) {
				free(name);
				continue;
			}
			*val++ = '\0';

			matched = 0;
			for (j = 0; j < options.num_send_env; j++) {
				if (match_pattern(name, options.send_env[j])) {
					matched = 1;
					break;
				}
			}
			if (!matched) {
				debug3("Ignored env %s", name);
				free(name);
				continue;
			}

			debug("Sending env %s = %s", name, val);
			channel_request_start(id, "env", 0);
			packet_put_cstring(name);
			packet_put_cstring(val);
			packet_send();
			free(name);
		}
	}

	len = buffer_len(cmd);
	if (len > 0) {
		if (len > 900)
			len = 900;
		if (want_subsystem) {
			debug("Sending subsystem: %.*s",
			    len, (u_char*)buffer_ptr(cmd));
			channel_request_start(id, "subsystem", 1);
			client_expect_confirm(id, "subsystem", CONFIRM_CLOSE);
		} else {
			debug("Sending command: %.*s",
			    len, (u_char*)buffer_ptr(cmd));
			channel_request_start(id, "exec", 1);
			client_expect_confirm(id, "exec", CONFIRM_CLOSE);
		}
		packet_put_string(buffer_ptr(cmd), buffer_len(cmd));
		packet_send();
	} else {
		channel_request_start(id, "shell", 1);
		client_expect_confirm(id, "shell", CONFIRM_CLOSE);
		packet_send();
	}
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
	dispatch_set(SSH2_MSG_CHANNEL_SUCCESS, &channel_input_status_confirm);
	dispatch_set(SSH2_MSG_CHANNEL_FAILURE, &channel_input_status_confirm);
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
	    &client_input_agent_open : &deny_input_open);
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

void
client_stop_mux(void)
{
	if (options.control_path != NULL && muxserver_sock != -1)
		unlink(options.control_path);
	/*
	 * If we are in persist mode, or don't have a shell, signal that we
	 * should close when all active channels are closed.
	 */
	if (options.control_persist || no_shell_flag) {
		session_closed = 1;
		setproctitle("[stopped mux]");
	}
}

/* client specific fatal cleanup */
void
cleanup_exit(int i)
{
	leave_raw_mode(options.request_tty == REQUEST_TTY_FORCE);
	leave_non_blocking();
	if (options.control_path != NULL && muxserver_sock != -1)
		unlink(options.control_path);
	ssh_kill_proxy_command();
	_exit(i);
}
