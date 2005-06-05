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
RCSID("$OpenBSD: clientloop.c,v 1.136 2005/03/10 22:01:05 deraadt Exp $");

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
#include "sshpty.h"
#include "misc.h"
#include "monitor_fdpass.h"
#include "match.h"
#include "msg.h"

/* import options */
extern Options options;

/* Flag indicating that stdin should be redirected from /dev/null. */
extern int stdin_null_flag;

/* Flag indicating that no shell has been requested */
extern int no_shell_flag;

/* Control socket */
extern int control_fd;

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
static int server_alive_timeouts = 0;

static void client_init_dispatch(void);
int	session_ident = -1;

struct confirm_ctx {
	int want_tty;
	int want_subsys;
	Buffer cmd;
	char *term;
	struct termios tio;
	char **env;
};

/*XXX*/
extern Kex *xxx_kex;

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

	debug2("client_check_window_change: changed");

	if (compat20) {
		channel_send_window_changes();
	} else {
		if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) < 0)
			return;
		packet_start(SSH_CMSG_WINDOW_SIZE);
		packet_put_int(ws.ws_row);
		packet_put_int(ws.ws_col);
		packet_put_int(ws.ws_xpixel);
		packet_put_int(ws.ws_ypixel);
		packet_send();
	}
}

static void
client_global_request_reply(int type, u_int32_t seq, void *ctxt)
{
	server_alive_timeouts = 0;
	client_global_request_reply_fwd(type, seq, ctxt);
}

static void
server_alive_check(void)
{
	if (++server_alive_timeouts > options.server_alive_count_max)
		packet_disconnect("Timeout, server not responding.");
	packet_start(SSH2_MSG_GLOBAL_REQUEST);
	packet_put_cstring("keepalive@openssh.com");
	packet_put_char(1);     /* boolean: want reply */
	packet_send();
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
	int ret;

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

	if (control_fd != -1)
		FD_SET(control_fd, *readsetp);

	/*
	 * Wait for something to happen.  This will suspend the process until
	 * some selected descriptor can be read, written, or has some other
	 * event pending.
	 */

	if (options.server_alive_interval == 0 || !compat20)
		tvp = NULL;
	else {
		tv.tv_sec = options.server_alive_interval;
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
	} else if (ret == 0)
		server_alive_check();
}

static void
client_suspend_self(Buffer *bin, Buffer *bout, Buffer *berr)
{
	/* Flush stdout and stderr buffers. */
	if (buffer_len(bout) > 0)
		atomicio(vwrite, fileno(stdout), buffer_ptr(bout), buffer_len(bout));
	if (buffer_len(berr) > 0)
		atomicio(vwrite, fileno(stderr), buffer_ptr(berr), buffer_len(berr));

	leave_raw_mode();

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
client_subsystem_reply(int type, u_int32_t seq, void *ctxt)
{
	int id;
	Channel *c;

	id = packet_get_int();
	packet_check_eom();

	if ((c = channel_lookup(id)) == NULL) {
		error("%s: no channel for id %d", __func__, id);
		return;
	}

	if (type == SSH2_MSG_CHANNEL_SUCCESS)
		debug2("Request suceeded on channel %d", id);
	else if (type == SSH2_MSG_CHANNEL_FAILURE) {
		error("Request failed on channel %d", id);
		channel_free(c);
	}
}

static void
client_extra_session2_setup(int id, void *arg)
{
	struct confirm_ctx *cctx = arg;
	Channel *c;
	int i;

	if (cctx == NULL)
		fatal("%s: cctx == NULL", __func__);
	if ((c = channel_lookup(id)) == NULL)
		fatal("%s: no channel for id %d", __func__, id);

	client_session2_setup(id, cctx->want_tty, cctx->want_subsys,
	    cctx->term, &cctx->tio, c->rfd, &cctx->cmd, cctx->env,
	    client_subsystem_reply);

	c->confirm_ctx = NULL;
	buffer_free(&cctx->cmd);
	xfree(cctx->term);
	if (cctx->env != NULL) {
		for (i = 0; cctx->env[i] != NULL; i++)
			xfree(cctx->env[i]);
		xfree(cctx->env);
	}
	xfree(cctx);
}

static void
client_process_control(fd_set * readset)
{
	Buffer m;
	Channel *c;
	int client_fd, new_fd[3], ver, i, allowed;
	socklen_t addrlen;
	struct sockaddr_storage addr;
	struct confirm_ctx *cctx;
	char *cmd;
	u_int len, env_len, command, flags;
	uid_t euid;
	gid_t egid;

	/*
	 * Accept connection on control socket
	 */
	if (control_fd == -1 || !FD_ISSET(control_fd, readset))
		return;

	memset(&addr, 0, sizeof(addr));
	addrlen = sizeof(addr);
	if ((client_fd = accept(control_fd,
	    (struct sockaddr*)&addr, &addrlen)) == -1) {
		error("%s accept: %s", __func__, strerror(errno));
		return;
	}

	if (getpeereid(client_fd, &euid, &egid) < 0) {
		error("%s getpeereid failed: %s", __func__, strerror(errno));
		close(client_fd);
		return;
	}
	if ((euid != 0) && (getuid() != euid)) {
		error("control mode uid mismatch: peer euid %u != uid %u",
		    (u_int) euid, (u_int) getuid());
		close(client_fd);
		return;
	}

	unset_nonblock(client_fd);

	/* Read command */
	buffer_init(&m);
	if (ssh_msg_recv(client_fd, &m) == -1) {
		error("%s: client msg_recv failed", __func__);
		close(client_fd);
		buffer_free(&m);
		return;
	}
	if ((ver = buffer_get_char(&m)) != 1) {
		error("%s: wrong client version %d", __func__, ver);
		buffer_free(&m);
		close(client_fd);
		return;
	}

	allowed = 1;
	command = buffer_get_int(&m);
	flags = buffer_get_int(&m);

	buffer_clear(&m);

	switch (command) {
	case SSHMUX_COMMAND_OPEN:
		if (options.control_master == 2)
			allowed = ask_permission("Allow shared connection "
			    "to %s? ", host);
		/* continue below */
		break;
	case SSHMUX_COMMAND_TERMINATE:
		if (options.control_master == 2)
			allowed = ask_permission("Terminate shared connection "
			    "to %s? ", host);
		if (allowed)
			quit_pending = 1;
		/* FALLTHROUGH */
	case SSHMUX_COMMAND_ALIVE_CHECK:
		/* Reply for SSHMUX_COMMAND_TERMINATE and ALIVE_CHECK */
		buffer_clear(&m);
		buffer_put_int(&m, allowed);
		buffer_put_int(&m, getpid());
		if (ssh_msg_send(client_fd, /* version */1, &m) == -1) {
			error("%s: client msg_send failed", __func__);
			close(client_fd);
			buffer_free(&m);
			return;
		}
		buffer_free(&m);
		close(client_fd);
		return;
	default:
		error("Unsupported command %d", command);
		buffer_free(&m);
		close(client_fd);
		return;
	}

	/* Reply for SSHMUX_COMMAND_OPEN */
	buffer_clear(&m);
	buffer_put_int(&m, allowed);
	buffer_put_int(&m, getpid());
	if (ssh_msg_send(client_fd, /* version */1, &m) == -1) {
		error("%s: client msg_send failed", __func__);
		close(client_fd);
		buffer_free(&m);
		return;
	}

	if (!allowed) {
		error("Refused control connection");
		close(client_fd);
		buffer_free(&m);
		return;
	}

	buffer_clear(&m);
	if (ssh_msg_recv(client_fd, &m) == -1) {
		error("%s: client msg_recv failed", __func__);
		close(client_fd);
		buffer_free(&m);
		return;
	}
	if ((ver = buffer_get_char(&m)) != 1) {
		error("%s: wrong client version %d", __func__, ver);
		buffer_free(&m);
		close(client_fd);
		return;
	}

	cctx = xmalloc(sizeof(*cctx));
	memset(cctx, 0, sizeof(*cctx));
	cctx->want_tty = (flags & SSHMUX_FLAG_TTY) != 0;
	cctx->want_subsys = (flags & SSHMUX_FLAG_SUBSYS) != 0;
	cctx->term = buffer_get_string(&m, &len);

	cmd = buffer_get_string(&m, &len);
	buffer_init(&cctx->cmd);
	buffer_append(&cctx->cmd, cmd, strlen(cmd));

	env_len = buffer_get_int(&m);
	env_len = MIN(env_len, 4096);
	debug3("%s: receiving %d env vars", __func__, env_len);
	if (env_len != 0) {
		cctx->env = xmalloc(sizeof(*cctx->env) * (env_len + 1));
		for (i = 0; i < env_len; i++)
			cctx->env[i] = buffer_get_string(&m, &len);
		cctx->env[i] = NULL;
	}

	debug2("%s: accepted tty %d, subsys %d, cmd %s", __func__,
	    cctx->want_tty, cctx->want_subsys, cmd);

	/* Gather fds from client */
	new_fd[0] = mm_receive_fd(client_fd);
	new_fd[1] = mm_receive_fd(client_fd);
	new_fd[2] = mm_receive_fd(client_fd);

	debug2("%s: got fds stdin %d, stdout %d, stderr %d", __func__,
	    new_fd[0], new_fd[1], new_fd[2]);

	/* Try to pick up ttymodes from client before it goes raw */
	if (cctx->want_tty && tcgetattr(new_fd[0], &cctx->tio) == -1)
		error("%s: tcgetattr: %s", __func__, strerror(errno));

	/* This roundtrip is just for synchronisation of ttymodes */
	buffer_clear(&m);
	if (ssh_msg_send(client_fd, /* version */1, &m) == -1) {
		error("%s: client msg_send failed", __func__);
		close(client_fd);
		close(new_fd[0]);
		close(new_fd[1]);
		close(new_fd[2]);
		buffer_free(&m);
		xfree(cctx->term);
		if (env_len != 0) {
			for (i = 0; i < env_len; i++)
				xfree(cctx->env[i]);
			xfree(cctx->env);
		}
		return;
	}
	buffer_free(&m);

	/* enable nonblocking unless tty */
	if (!isatty(new_fd[0]))
		set_nonblock(new_fd[0]);
	if (!isatty(new_fd[1]))
		set_nonblock(new_fd[1]);
	if (!isatty(new_fd[2]))
		set_nonblock(new_fd[2]);

	set_nonblock(client_fd);

	c = channel_new("session", SSH_CHANNEL_OPENING,
	    new_fd[0], new_fd[1], new_fd[2],
	    CHAN_SES_WINDOW_DEFAULT, CHAN_SES_PACKET_DEFAULT,
	    CHAN_EXTENDED_WRITE, "client-session", /*nonblock*/0);

	/* XXX */
	c->ctl_fd = client_fd;

	debug3("%s: channel_new: %d", __func__, c->self);

	channel_send_open(c->self);
	channel_register_confirm(c->self, client_extra_session2_setup, cctx);
}

static void
process_cmdline(void)
{
	void (*handler)(int);
	char *s, *cmd, *cancel_host;
	int delete = 0;
	int local = 0;
	u_short cancel_port;
	Forward fwd;

	leave_raw_mode();
	handler = signal(SIGINT, SIG_IGN);
	cmd = s = read_passphrase("\r\nssh> ", RP_ECHO);
	if (s == NULL)
		goto out;
	while (*s && isspace(*s))
		s++;
	if (*s == '-')
		s++;	/* Skip cmdline '-', if any */
	if (*s == '\0')
		goto out;

	if (*s == 'h' || *s == 'H' || *s == '?') {
		logit("Commands:");
		logit("      -Lport:host:hostport    Request local forward");
		logit("      -Rport:host:hostport    Request remote forward");
		logit("      -KRhostport             Cancel remote forward");
		goto out;
	}

	if (*s == 'K') {
		delete = 1;
		s++;
	}
	if (*s != 'L' && *s != 'R') {
		logit("Invalid command.");
		goto out;
	}
	if (*s == 'L')
		local = 1;
	if (local && delete) {
		logit("Not supported.");
		goto out;
	}
	if ((!local || delete) && !compat20) {
		logit("Not supported for SSH protocol version 1.");
		goto out;
	}

	s++;
	while (*s && isspace(*s))
		s++;

	if (delete) {
		cancel_port = 0;
		cancel_host = hpdelim(&s);	/* may be NULL */
		if (s != NULL) {
			cancel_port = a2port(s);
			cancel_host = cleanhostname(cancel_host);
		} else {
			cancel_port = a2port(cancel_host);
			cancel_host = NULL;
		}
		if (cancel_port == 0) {
			logit("Bad forwarding close port");
			goto out;
		}
		channel_request_rforward_cancel(cancel_host, cancel_port);
	} else {
		if (!parse_forward(&fwd, s)) {
			logit("Bad forwarding specification.");
			goto out;
		}
		if (local) {
			if (channel_setup_local_fwd_listener(fwd.listen_host,
			    fwd.listen_port, fwd.connect_host,
			    fwd.connect_port, options.gateway_ports) < 0) {
				logit("Port forwarding failed.");
				goto out;
			}
		} else {
			channel_request_remote_forwarding(fwd.listen_host,
			    fwd.listen_port, fwd.connect_host,
			    fwd.connect_port);
		}

		logit("Forwarding port.");
	}

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

			case 'B':
				if (compat20) {
					snprintf(string, sizeof string,
					    "%cB\r\n", escape_char);
					buffer_append(berr, string,
					    strlen(string));
					channel_request_start(session_ident,
					    "break", 0);
					packet_put_int(1000);
					packet_send();
				}
				continue;

			case 'R':
				if (compat20) {
					if (datafellows & SSH_BUG_NOREKEY)
						logit("Server does not support re-keying");
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
%cB  - send a BREAK to the remote system\r\n\
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
				    escape_char, escape_char, escape_char);
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
	channel_cancel_cleanup(id);
	session_closed = 1;
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
	int max_fd = 0, max_fd2 = 0, len, rekeying = 0;
	u_int nalloc = 0;
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
	if (control_fd != -1)
		max_fd = MAX(max_fd, control_fd);

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
			if (need_rekeying || packet_need_rekeying()) {
				debug("need rekeying");
				xxx_kex->done = 0;
				kex_send_kexinit(xxx_kex);
				need_rekeying = 0;
			}
		}

		/* Buffer input from the connection.  */
		client_process_net_input(readset);

		/* Accept control connections.  */
		client_process_control(readset);

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
static void
client_input_agent_open(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	int remote_id, sock;

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
	    originator_address, 1);
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
	    CHAN_TCP_WINDOW_DEFAULT, CHAN_X11_PACKET_DEFAULT, 0, "x11", 1);
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
	    "authentication agent connection", 1);
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
	int exitval, id, reply, success = 0;
	char *rtype;

	id = packet_get_int();
	rtype = packet_get_string(NULL);
	reply = packet_get_char();

	debug("client_input_channel_req: channel %d rtype %s reply %d",
	    id, rtype, reply);

	if (id == -1) {
		error("client_input_channel_req: request for channel -1");
	} else if ((c = channel_lookup(id)) == NULL) {
		error("client_input_channel_req: channel %d: unknown channel", id);
	} else if (strcmp(rtype, "exit-status") == 0) {
		exitval = packet_get_int();
		if (id == session_ident) {
			success = 1;
			exit_status = exitval;
		} else if (c->ctl_fd == -1) {
			error("client_input_channel_req: unexpected channel %d",
			    session_ident);
		} else {
			atomicio(vwrite, c->ctl_fd, &exitval, sizeof(exitval));
			success = 1;
		}
		packet_check_eom();
	}
	if (reply) {
		packet_start(success ?
		    SSH2_MSG_CHANNEL_SUCCESS : SSH2_MSG_CHANNEL_FAILURE);
		packet_put_int(id);
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
	debug("client_input_global_request: rtype %s want_reply %d",
	    rtype, want_reply);
	if (want_reply) {
		packet_start(success ?
		    SSH2_MSG_REQUEST_SUCCESS : SSH2_MSG_REQUEST_FAILURE);
		packet_send();
		packet_write_wait();
	}
	xfree(rtype);
}

void
client_session2_setup(int id, int want_tty, int want_subsystem,
    const char *term, struct termios *tiop, int in_fd, Buffer *cmd, char **env,
    dispatch_fn *subsys_repl)
{
	int len;
	Channel *c = NULL;

	debug2("%s: id %d", __func__, id);

	if ((c = channel_lookup(id)) == NULL)
		fatal("client_session2_setup: channel %d: unknown channel", id);

	if (want_tty) {
		struct winsize ws;
		struct termios tio;

		/* Store window size in the packet. */
		if (ioctl(in_fd, TIOCGWINSZ, &ws) < 0)
			memset(&ws, 0, sizeof(ws));

		channel_request_start(id, "pty-req", 0);
		packet_put_cstring(term != NULL ? term : "");
		packet_put_int(ws.ws_col);
		packet_put_int(ws.ws_row);
		packet_put_int(ws.ws_xpixel);
		packet_put_int(ws.ws_ypixel);
		tio = get_saved_tio();
		tty_make_modes(-1, tiop != NULL ? tiop : &tio);
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
			debug("Sending subsystem: %.*s", len, (u_char*)buffer_ptr(cmd));
			channel_request_start(id, "subsystem", subsys_repl != NULL);
			if (subsys_repl != NULL) {
				/* register callback for reply */
				/* XXX we assume that client_loop has already been called */
				dispatch_set(SSH2_MSG_CHANNEL_FAILURE, subsys_repl);
				dispatch_set(SSH2_MSG_CHANNEL_SUCCESS, subsys_repl);
			}
		} else {
			debug("Sending command: %.*s", len, (u_char*)buffer_ptr(cmd));
			channel_request_start(id, "exec", 0);
		}
		packet_put_string(buffer_ptr(cmd), buffer_len(cmd));
		packet_send();
	} else {
		channel_request_start(id, "shell", 0);
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

/* client specific fatal cleanup */
void
cleanup_exit(int i)
{
	leave_raw_mode();
	leave_non_blocking();
	if (options.control_path != NULL && control_fd != -1)
		unlink(options.control_path);
	_exit(i);
}
