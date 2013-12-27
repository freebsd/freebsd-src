/* $OpenBSD: serverloop.c,v 1.168 2013/07/12 00:19:59 djm Exp $ */
/* $FreeBSD$ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Server main loop for handling the interactive session.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 support by Markus Friedl.
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
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
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdarg.h>

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "packet.h"
#include "buffer.h"
#include "log.h"
#include "servconf.h"
#include "canohost.h"
#include "sshpty.h"
#include "channels.h"
#include "compat.h"
#include "ssh1.h"
#include "ssh2.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "hostfile.h"
#include "auth.h"
#include "session.h"
#include "dispatch.h"
#include "auth-options.h"
#include "serverloop.h"
#include "misc.h"
#include "roaming.h"

extern ServerOptions options;

/* XXX */
extern Kex *xxx_kex;
extern Authctxt *the_authctxt;
extern int use_privsep;

static Buffer stdin_buffer;	/* Buffer for stdin data. */
static Buffer stdout_buffer;	/* Buffer for stdout data. */
static Buffer stderr_buffer;	/* Buffer for stderr data. */
static int fdin;		/* Descriptor for stdin (for writing) */
static int fdout;		/* Descriptor for stdout (for reading);
				   May be same number as fdin. */
static int fderr;		/* Descriptor for stderr.  May be -1. */
static long stdin_bytes = 0;	/* Number of bytes written to stdin. */
static long stdout_bytes = 0;	/* Number of stdout bytes sent to client. */
static long stderr_bytes = 0;	/* Number of stderr bytes sent to client. */
static long fdout_bytes = 0;	/* Number of stdout bytes read from program. */
static int stdin_eof = 0;	/* EOF message received from client. */
static int fdout_eof = 0;	/* EOF encountered reading from fdout. */
static int fderr_eof = 0;	/* EOF encountered readung from fderr. */
static int fdin_is_tty = 0;	/* fdin points to a tty. */
static int connection_in;	/* Connection to client (input). */
static int connection_out;	/* Connection to client (output). */
static int connection_closed = 0;	/* Connection to client closed. */
static u_int buffer_high;	/* "Soft" max buffer size. */
static int no_more_sessions = 0; /* Disallow further sessions. */

/*
 * This SIGCHLD kludge is used to detect when the child exits.  The server
 * will exit after that, as soon as forwarded connections have terminated.
 */

static volatile sig_atomic_t child_terminated = 0;	/* The child has terminated. */

/* Cleanup on signals (!use_privsep case only) */
static volatile sig_atomic_t received_sigterm = 0;

/* prototypes */
static void server_init_dispatch(void);

/*
 * we write to this pipe if a SIGCHLD is caught in order to avoid
 * the race between select() and child_terminated
 */
static int notify_pipe[2];
static void
notify_setup(void)
{
	if (pipe(notify_pipe) < 0) {
		error("pipe(notify_pipe) failed %s", strerror(errno));
	} else if ((fcntl(notify_pipe[0], F_SETFD, FD_CLOEXEC) == -1) ||
	    (fcntl(notify_pipe[1], F_SETFD, FD_CLOEXEC) == -1)) {
		error("fcntl(notify_pipe, F_SETFD) failed %s", strerror(errno));
		close(notify_pipe[0]);
		close(notify_pipe[1]);
	} else {
		set_nonblock(notify_pipe[0]);
		set_nonblock(notify_pipe[1]);
		return;
	}
	notify_pipe[0] = -1;	/* read end */
	notify_pipe[1] = -1;	/* write end */
}
static void
notify_parent(void)
{
	if (notify_pipe[1] != -1)
		(void)write(notify_pipe[1], "", 1);
}
static void
notify_prepare(fd_set *readset)
{
	if (notify_pipe[0] != -1)
		FD_SET(notify_pipe[0], readset);
}
static void
notify_done(fd_set *readset)
{
	char c;

	if (notify_pipe[0] != -1 && FD_ISSET(notify_pipe[0], readset))
		while (read(notify_pipe[0], &c, 1) != -1)
			debug2("notify_done: reading");
}

/*ARGSUSED*/
static void
sigchld_handler(int sig)
{
	int save_errno = errno;
	child_terminated = 1;
#ifndef _UNICOS
	mysignal(SIGCHLD, sigchld_handler);
#endif
	notify_parent();
	errno = save_errno;
}

/*ARGSUSED*/
static void
sigterm_handler(int sig)
{
	received_sigterm = sig;
}

/*
 * Make packets from buffered stderr data, and buffer it for sending
 * to the client.
 */
static void
make_packets_from_stderr_data(void)
{
	u_int len;

	/* Send buffered stderr data to the client. */
	while (buffer_len(&stderr_buffer) > 0 &&
	    packet_not_very_much_data_to_write()) {
		len = buffer_len(&stderr_buffer);
		if (packet_is_interactive()) {
			if (len > 512)
				len = 512;
		} else {
			/* Keep the packets at reasonable size. */
			if (len > packet_get_maxsize())
				len = packet_get_maxsize();
		}
		packet_start(SSH_SMSG_STDERR_DATA);
		packet_put_string(buffer_ptr(&stderr_buffer), len);
		packet_send();
		buffer_consume(&stderr_buffer, len);
		stderr_bytes += len;
	}
}

/*
 * Make packets from buffered stdout data, and buffer it for sending to the
 * client.
 */
static void
make_packets_from_stdout_data(void)
{
	u_int len;

	/* Send buffered stdout data to the client. */
	while (buffer_len(&stdout_buffer) > 0 &&
	    packet_not_very_much_data_to_write()) {
		len = buffer_len(&stdout_buffer);
		if (packet_is_interactive()) {
			if (len > 512)
				len = 512;
		} else {
			/* Keep the packets at reasonable size. */
			if (len > packet_get_maxsize())
				len = packet_get_maxsize();
		}
		packet_start(SSH_SMSG_STDOUT_DATA);
		packet_put_string(buffer_ptr(&stdout_buffer), len);
		packet_send();
		buffer_consume(&stdout_buffer, len);
		stdout_bytes += len;
	}
}

static void
client_alive_check(void)
{
	int channel_id;

	/* timeout, check to see how many we have had */
	if (packet_inc_alive_timeouts() > options.client_alive_count_max) {
		logit("Timeout, client not responding.");
		cleanup_exit(255);
	}

	/*
	 * send a bogus global/channel request with "wantreply",
	 * we should get back a failure
	 */
	if ((channel_id = channel_find_open()) == -1) {
		packet_start(SSH2_MSG_GLOBAL_REQUEST);
		packet_put_cstring("keepalive@openssh.com");
		packet_put_char(1);	/* boolean: want reply */
	} else {
		channel_request_start(channel_id, "keepalive@openssh.com", 1);
	}
	packet_send();
}

/*
 * Sleep in select() until we can do something.  This will initialize the
 * select masks.  Upon return, the masks will indicate which descriptors
 * have data or can accept data.  Optionally, a maximum time can be specified
 * for the duration of the wait (0 = infinite).
 */
static void
wait_until_can_do_something(fd_set **readsetp, fd_set **writesetp, int *maxfdp,
    u_int *nallocp, u_int64_t max_time_milliseconds)
{
	struct timeval tv, *tvp;
	int ret;
	time_t minwait_secs = 0;
	int client_alive_scheduled = 0;
	int program_alive_scheduled = 0;

	/* Allocate and update select() masks for channel descriptors. */
	channel_prepare_select(readsetp, writesetp, maxfdp, nallocp,
	    &minwait_secs, 0);

	if (minwait_secs != 0)
		max_time_milliseconds = MIN(max_time_milliseconds,
		    (u_int)minwait_secs * 1000);

	/*
	 * if using client_alive, set the max timeout accordingly,
	 * and indicate that this particular timeout was for client
	 * alive by setting the client_alive_scheduled flag.
	 *
	 * this could be randomized somewhat to make traffic
	 * analysis more difficult, but we're not doing it yet.
	 */
	if (compat20 &&
	    max_time_milliseconds == 0 && options.client_alive_interval) {
		client_alive_scheduled = 1;
		max_time_milliseconds = options.client_alive_interval * 1000;
	}

	if (compat20) {
#if 0
		/* wrong: bad condition XXX */
		if (channel_not_very_much_buffered_data())
#endif
		FD_SET(connection_in, *readsetp);
	} else {
		/*
		 * Read packets from the client unless we have too much
		 * buffered stdin or channel data.
		 */
		if (buffer_len(&stdin_buffer) < buffer_high &&
		    channel_not_very_much_buffered_data())
			FD_SET(connection_in, *readsetp);
		/*
		 * If there is not too much data already buffered going to
		 * the client, try to get some more data from the program.
		 */
		if (packet_not_very_much_data_to_write()) {
			program_alive_scheduled = child_terminated;
			if (!fdout_eof)
				FD_SET(fdout, *readsetp);
			if (!fderr_eof)
				FD_SET(fderr, *readsetp);
		}
		/*
		 * If we have buffered data, try to write some of that data
		 * to the program.
		 */
		if (fdin != -1 && buffer_len(&stdin_buffer) > 0)
			FD_SET(fdin, *writesetp);
	}
	notify_prepare(*readsetp);

	/*
	 * If we have buffered packet data going to the client, mark that
	 * descriptor.
	 */
	if (packet_have_data_to_write())
		FD_SET(connection_out, *writesetp);

	/*
	 * If child has terminated and there is enough buffer space to read
	 * from it, then read as much as is available and exit.
	 */
	if (child_terminated && packet_not_very_much_data_to_write())
		if (max_time_milliseconds == 0 || client_alive_scheduled)
			max_time_milliseconds = 100;

	if (max_time_milliseconds == 0)
		tvp = NULL;
	else {
		tv.tv_sec = max_time_milliseconds / 1000;
		tv.tv_usec = 1000 * (max_time_milliseconds % 1000);
		tvp = &tv;
	}

	/* Wait for something to happen, or the timeout to expire. */
	ret = select((*maxfdp)+1, *readsetp, *writesetp, NULL, tvp);

	if (ret == -1) {
		memset(*readsetp, 0, *nallocp);
		memset(*writesetp, 0, *nallocp);
		if (errno != EINTR)
			error("select: %.100s", strerror(errno));
	} else {
		if (ret == 0 && client_alive_scheduled)
			client_alive_check();
		if (!compat20 && program_alive_scheduled && fdin_is_tty) {
			if (!fdout_eof)
				FD_SET(fdout, *readsetp);
			if (!fderr_eof)
				FD_SET(fderr, *readsetp);
		}
	}

	notify_done(*readsetp);
}

/*
 * Processes input from the client and the program.  Input data is stored
 * in buffers and processed later.
 */
static void
process_input(fd_set *readset)
{
	int len;
	char buf[16384];

	/* Read and buffer any input data from the client. */
	if (FD_ISSET(connection_in, readset)) {
		int cont = 0;
		len = roaming_read(connection_in, buf, sizeof(buf), &cont);
		if (len == 0) {
			if (cont)
				return;
			verbose("Connection closed by %.100s",
			    get_remote_ipaddr());
			connection_closed = 1;
			if (compat20)
				return;
			cleanup_exit(255);
		} else if (len < 0) {
			if (errno != EINTR && errno != EAGAIN &&
			    errno != EWOULDBLOCK) {
				verbose("Read error from remote host "
				    "%.100s: %.100s",
				    get_remote_ipaddr(), strerror(errno));
				cleanup_exit(255);
			}
		} else {
			/* Buffer any received data. */
			packet_process_incoming(buf, len);
		}
	}
	if (compat20)
		return;

	/* Read and buffer any available stdout data from the program. */
	if (!fdout_eof && FD_ISSET(fdout, readset)) {
		errno = 0;
		len = read(fdout, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR || ((errno == EAGAIN ||
		    errno == EWOULDBLOCK) && !child_terminated))) {
			/* do nothing */
#ifndef PTY_ZEROREAD
		} else if (len <= 0) {
#else
		} else if ((!isatty(fdout) && len <= 0) ||
		    (isatty(fdout) && (len < 0 || (len == 0 && errno != 0)))) {
#endif
			fdout_eof = 1;
		} else {
			buffer_append(&stdout_buffer, buf, len);
			fdout_bytes += len;
		}
	}
	/* Read and buffer any available stderr data from the program. */
	if (!fderr_eof && FD_ISSET(fderr, readset)) {
		errno = 0;
		len = read(fderr, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR || ((errno == EAGAIN ||
		    errno == EWOULDBLOCK) && !child_terminated))) {
			/* do nothing */
#ifndef PTY_ZEROREAD
		} else if (len <= 0) {
#else
		} else if ((!isatty(fderr) && len <= 0) ||
		    (isatty(fderr) && (len < 0 || (len == 0 && errno != 0)))) {
#endif
			fderr_eof = 1;
		} else {
			buffer_append(&stderr_buffer, buf, len);
		}
	}
}

/*
 * Sends data from internal buffers to client program stdin.
 */
static void
process_output(fd_set *writeset)
{
	struct termios tio;
	u_char *data;
	u_int dlen;
	int len;

	/* Write buffered data to program stdin. */
	if (!compat20 && fdin != -1 && FD_ISSET(fdin, writeset)) {
		data = buffer_ptr(&stdin_buffer);
		dlen = buffer_len(&stdin_buffer);
		len = write(fdin, data, dlen);
		if (len < 0 &&
		    (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
			/* do nothing */
		} else if (len <= 0) {
			if (fdin != fdout)
				close(fdin);
			else
				shutdown(fdin, SHUT_WR); /* We will no longer send. */
			fdin = -1;
		} else {
			/* Successful write. */
			if (fdin_is_tty && dlen >= 1 && data[0] != '\r' &&
			    tcgetattr(fdin, &tio) == 0 &&
			    !(tio.c_lflag & ECHO) && (tio.c_lflag & ICANON)) {
				/*
				 * Simulate echo to reduce the impact of
				 * traffic analysis
				 */
				packet_send_ignore(len);
				packet_send();
			}
			/* Consume the data from the buffer. */
			buffer_consume(&stdin_buffer, len);
			/* Update the count of bytes written to the program. */
			stdin_bytes += len;
		}
	}
	/* Send any buffered packet data to the client. */
	if (FD_ISSET(connection_out, writeset))
		packet_write_poll();
}

/*
 * Wait until all buffered output has been sent to the client.
 * This is used when the program terminates.
 */
static void
drain_output(void)
{
	/* Send any buffered stdout data to the client. */
	if (buffer_len(&stdout_buffer) > 0) {
		packet_start(SSH_SMSG_STDOUT_DATA);
		packet_put_string(buffer_ptr(&stdout_buffer),
				  buffer_len(&stdout_buffer));
		packet_send();
		/* Update the count of sent bytes. */
		stdout_bytes += buffer_len(&stdout_buffer);
	}
	/* Send any buffered stderr data to the client. */
	if (buffer_len(&stderr_buffer) > 0) {
		packet_start(SSH_SMSG_STDERR_DATA);
		packet_put_string(buffer_ptr(&stderr_buffer),
				  buffer_len(&stderr_buffer));
		packet_send();
		/* Update the count of sent bytes. */
		stderr_bytes += buffer_len(&stderr_buffer);
	}
	/* Wait until all buffered data has been written to the client. */
	packet_write_wait();
}

static void
process_buffered_input_packets(void)
{
	dispatch_run(DISPATCH_NONBLOCK, NULL, compat20 ? xxx_kex : NULL);
}

/*
 * Performs the interactive session.  This handles data transmission between
 * the client and the program.  Note that the notion of stdin, stdout, and
 * stderr in this function is sort of reversed: this function writes to
 * stdin (of the child program), and reads from stdout and stderr (of the
 * child program).
 */
void
server_loop(pid_t pid, int fdin_arg, int fdout_arg, int fderr_arg)
{
	fd_set *readset = NULL, *writeset = NULL;
	int max_fd = 0;
	u_int nalloc = 0;
	int wait_status;	/* Status returned by wait(). */
	pid_t wait_pid;		/* pid returned by wait(). */
	int waiting_termination = 0;	/* Have displayed waiting close message. */
	u_int64_t max_time_milliseconds;
	u_int previous_stdout_buffer_bytes;
	u_int stdout_buffer_bytes;
	int type;

	debug("Entering interactive session.");

	/* Initialize the SIGCHLD kludge. */
	child_terminated = 0;
	mysignal(SIGCHLD, sigchld_handler);

	if (!use_privsep) {
		signal(SIGTERM, sigterm_handler);
		signal(SIGINT, sigterm_handler);
		signal(SIGQUIT, sigterm_handler);
	}

	/* Initialize our global variables. */
	fdin = fdin_arg;
	fdout = fdout_arg;
	fderr = fderr_arg;

	/* nonblocking IO */
	set_nonblock(fdin);
	set_nonblock(fdout);
	/* we don't have stderr for interactive terminal sessions, see below */
	if (fderr != -1)
		set_nonblock(fderr);

	if (!(datafellows & SSH_BUG_IGNOREMSG) && isatty(fdin))
		fdin_is_tty = 1;

	connection_in = packet_get_connection_in();
	connection_out = packet_get_connection_out();

	notify_setup();

	previous_stdout_buffer_bytes = 0;

	/* Set approximate I/O buffer size. */
	if (packet_is_interactive())
		buffer_high = 4096;
	else
		buffer_high = 64 * 1024;

#if 0
	/* Initialize max_fd to the maximum of the known file descriptors. */
	max_fd = MAX(connection_in, connection_out);
	max_fd = MAX(max_fd, fdin);
	max_fd = MAX(max_fd, fdout);
	if (fderr != -1)
		max_fd = MAX(max_fd, fderr);
#endif

	/* Initialize Initialize buffers. */
	buffer_init(&stdin_buffer);
	buffer_init(&stdout_buffer);
	buffer_init(&stderr_buffer);

	/*
	 * If we have no separate fderr (which is the case when we have a pty
	 * - there we cannot make difference between data sent to stdout and
	 * stderr), indicate that we have seen an EOF from stderr.  This way
	 * we don't need to check the descriptor everywhere.
	 */
	if (fderr == -1)
		fderr_eof = 1;

	server_init_dispatch();

	/* Main loop of the server for the interactive session mode. */
	for (;;) {

		/* Process buffered packets from the client. */
		process_buffered_input_packets();

		/*
		 * If we have received eof, and there is no more pending
		 * input data, cause a real eof by closing fdin.
		 */
		if (stdin_eof && fdin != -1 && buffer_len(&stdin_buffer) == 0) {
			if (fdin != fdout)
				close(fdin);
			else
				shutdown(fdin, SHUT_WR); /* We will no longer send. */
			fdin = -1;
		}
		/* Make packets from buffered stderr data to send to the client. */
		make_packets_from_stderr_data();

		/*
		 * Make packets from buffered stdout data to send to the
		 * client. If there is very little to send, this arranges to
		 * not send them now, but to wait a short while to see if we
		 * are getting more data. This is necessary, as some systems
		 * wake up readers from a pty after each separate character.
		 */
		max_time_milliseconds = 0;
		stdout_buffer_bytes = buffer_len(&stdout_buffer);
		if (stdout_buffer_bytes != 0 && stdout_buffer_bytes < 256 &&
		    stdout_buffer_bytes != previous_stdout_buffer_bytes) {
			/* try again after a while */
			max_time_milliseconds = 10;
		} else {
			/* Send it now. */
			make_packets_from_stdout_data();
		}
		previous_stdout_buffer_bytes = buffer_len(&stdout_buffer);

		/* Send channel data to the client. */
		if (packet_not_very_much_data_to_write())
			channel_output_poll();

		/*
		 * Bail out of the loop if the program has closed its output
		 * descriptors, and we have no more data to send to the
		 * client, and there is no pending buffered data.
		 */
		if (fdout_eof && fderr_eof && !packet_have_data_to_write() &&
		    buffer_len(&stdout_buffer) == 0 && buffer_len(&stderr_buffer) == 0) {
			if (!channel_still_open())
				break;
			if (!waiting_termination) {
				const char *s = "Waiting for forwarded connections to terminate...\r\n";
				char *cp;
				waiting_termination = 1;
				buffer_append(&stderr_buffer, s, strlen(s));

				/* Display list of open channels. */
				cp = channel_open_message();
				buffer_append(&stderr_buffer, cp, strlen(cp));
				free(cp);
			}
		}
		max_fd = MAX(connection_in, connection_out);
		max_fd = MAX(max_fd, fdin);
		max_fd = MAX(max_fd, fdout);
		max_fd = MAX(max_fd, fderr);
		max_fd = MAX(max_fd, notify_pipe[0]);

		/* Sleep in select() until we can do something. */
		wait_until_can_do_something(&readset, &writeset, &max_fd,
		    &nalloc, max_time_milliseconds);

		if (received_sigterm) {
			logit("Exiting on signal %d", (int)received_sigterm);
			/* Clean up sessions, utmp, etc. */
			cleanup_exit(255);
		}

		/* Process any channel events. */
		channel_after_select(readset, writeset);

		/* Process input from the client and from program stdout/stderr. */
		process_input(readset);

		/* Process output to the client and to program stdin. */
		process_output(writeset);
	}
	free(readset);
	free(writeset);

	/* Cleanup and termination code. */

	/* Wait until all output has been sent to the client. */
	drain_output();

	debug("End of interactive session; stdin %ld, stdout (read %ld, sent %ld), stderr %ld bytes.",
	    stdin_bytes, fdout_bytes, stdout_bytes, stderr_bytes);

	/* Free and clear the buffers. */
	buffer_free(&stdin_buffer);
	buffer_free(&stdout_buffer);
	buffer_free(&stderr_buffer);

	/* Close the file descriptors. */
	if (fdout != -1)
		close(fdout);
	fdout = -1;
	fdout_eof = 1;
	if (fderr != -1)
		close(fderr);
	fderr = -1;
	fderr_eof = 1;
	if (fdin != -1)
		close(fdin);
	fdin = -1;

	channel_free_all();

	/* We no longer want our SIGCHLD handler to be called. */
	mysignal(SIGCHLD, SIG_DFL);

	while ((wait_pid = waitpid(-1, &wait_status, 0)) < 0)
		if (errno != EINTR)
			packet_disconnect("wait: %.100s", strerror(errno));
	if (wait_pid != pid)
		error("Strange, wait returned pid %ld, expected %ld",
		    (long)wait_pid, (long)pid);

	/* Check if it exited normally. */
	if (WIFEXITED(wait_status)) {
		/* Yes, normal exit.  Get exit status and send it to the client. */
		debug("Command exited with status %d.", WEXITSTATUS(wait_status));
		packet_start(SSH_SMSG_EXITSTATUS);
		packet_put_int(WEXITSTATUS(wait_status));
		packet_send();
		packet_write_wait();

		/*
		 * Wait for exit confirmation.  Note that there might be
		 * other packets coming before it; however, the program has
		 * already died so we just ignore them.  The client is
		 * supposed to respond with the confirmation when it receives
		 * the exit status.
		 */
		do {
			type = packet_read();
		}
		while (type != SSH_CMSG_EXIT_CONFIRMATION);

		debug("Received exit confirmation.");
		return;
	}
	/* Check if the program terminated due to a signal. */
	if (WIFSIGNALED(wait_status))
		packet_disconnect("Command terminated on signal %d.",
				  WTERMSIG(wait_status));

	/* Some weird exit cause.  Just exit. */
	packet_disconnect("wait returned status %04x.", wait_status);
	/* NOTREACHED */
}

static void
collect_children(void)
{
	pid_t pid;
	sigset_t oset, nset;
	int status;

	/* block SIGCHLD while we check for dead children */
	sigemptyset(&nset);
	sigaddset(&nset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nset, &oset);
	if (child_terminated) {
		debug("Received SIGCHLD.");
		while ((pid = waitpid(-1, &status, WNOHANG)) > 0 ||
		    (pid < 0 && errno == EINTR))
			if (pid > 0)
				session_close_by_pid(pid, status);
		child_terminated = 0;
	}
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

void
server_loop2(Authctxt *authctxt)
{
	fd_set *readset = NULL, *writeset = NULL;
	int rekeying = 0, max_fd;
	u_int nalloc = 0;
	u_int64_t rekey_timeout_ms = 0;

	debug("Entering interactive session for SSH2.");

	mysignal(SIGCHLD, sigchld_handler);
	child_terminated = 0;
	connection_in = packet_get_connection_in();
	connection_out = packet_get_connection_out();

	if (!use_privsep) {
		signal(SIGTERM, sigterm_handler);
		signal(SIGINT, sigterm_handler);
		signal(SIGQUIT, sigterm_handler);
	}

	notify_setup();

	max_fd = MAX(connection_in, connection_out);
	max_fd = MAX(max_fd, notify_pipe[0]);

	server_init_dispatch();

	for (;;) {
		process_buffered_input_packets();

		rekeying = (xxx_kex != NULL && !xxx_kex->done);

		if (!rekeying && packet_not_very_much_data_to_write())
			channel_output_poll();
		if (options.rekey_interval > 0 && compat20 && !rekeying)
			rekey_timeout_ms = packet_get_rekey_timeout() * 1000;
		else
			rekey_timeout_ms = 0;

		wait_until_can_do_something(&readset, &writeset, &max_fd,
		    &nalloc, rekey_timeout_ms);

		if (received_sigterm) {
			logit("Exiting on signal %d", (int)received_sigterm);
			/* Clean up sessions, utmp, etc. */
			cleanup_exit(255);
		}

		collect_children();
		if (!rekeying) {
			channel_after_select(readset, writeset);
			if (packet_need_rekeying()) {
				debug("need rekeying");
				xxx_kex->done = 0;
				kex_send_kexinit(xxx_kex);
			}
		}
		process_input(readset);
		if (connection_closed)
			break;
		process_output(writeset);
	}
	collect_children();

	free(readset);
	free(writeset);

	/* free all channels, no more reads and writes */
	channel_free_all();

	/* free remaining sessions, e.g. remove wtmp entries */
	session_destroy_all(NULL);
}

static void
server_input_keep_alive(int type, u_int32_t seq, void *ctxt)
{
	debug("Got %d/%u for keepalive", type, seq);
	/*
	 * reset timeout, since we got a sane answer from the client.
	 * even if this was generated by something other than
	 * the bogus CHANNEL_REQUEST we send for keepalives.
	 */
	packet_set_alive_timeouts(0);
}

static void
server_input_stdin_data(int type, u_int32_t seq, void *ctxt)
{
	char *data;
	u_int data_len;

	/* Stdin data from the client.  Append it to the buffer. */
	/* Ignore any data if the client has closed stdin. */
	if (fdin == -1)
		return;
	data = packet_get_string(&data_len);
	packet_check_eom();
	buffer_append(&stdin_buffer, data, data_len);
	memset(data, 0, data_len);
	free(data);
}

static void
server_input_eof(int type, u_int32_t seq, void *ctxt)
{
	/*
	 * Eof from the client.  The stdin descriptor to the
	 * program will be closed when all buffered data has
	 * drained.
	 */
	debug("EOF received for stdin.");
	packet_check_eom();
	stdin_eof = 1;
}

static void
server_input_window_size(int type, u_int32_t seq, void *ctxt)
{
	u_int row = packet_get_int();
	u_int col = packet_get_int();
	u_int xpixel = packet_get_int();
	u_int ypixel = packet_get_int();

	debug("Window change received.");
	packet_check_eom();
	if (fdin != -1)
		pty_change_window_size(fdin, row, col, xpixel, ypixel);
}

static Channel *
server_request_direct_tcpip(void)
{
	Channel *c = NULL;
	char *target, *originator;
	u_short target_port, originator_port;

	target = packet_get_string(NULL);
	target_port = packet_get_int();
	originator = packet_get_string(NULL);
	originator_port = packet_get_int();
	packet_check_eom();

	debug("server_request_direct_tcpip: originator %s port %d, target %s "
	    "port %d", originator, originator_port, target, target_port);

	/* XXX fine grained permissions */
	if ((options.allow_tcp_forwarding & FORWARD_LOCAL) != 0 &&
	    !no_port_forwarding_flag) {
		c = channel_connect_to(target, target_port,
		    "direct-tcpip", "direct-tcpip");
	} else {
		logit("refused local port forward: "
		    "originator %s port %d, target %s port %d",
		    originator, originator_port, target, target_port);
	}

	free(originator);
	free(target);

	return c;
}

static Channel *
server_request_tun(void)
{
	Channel *c = NULL;
	int mode, tun;
	int sock;

	mode = packet_get_int();
	switch (mode) {
	case SSH_TUNMODE_POINTOPOINT:
	case SSH_TUNMODE_ETHERNET:
		break;
	default:
		packet_send_debug("Unsupported tunnel device mode.");
		return NULL;
	}
	if ((options.permit_tun & mode) == 0) {
		packet_send_debug("Server has rejected tunnel device "
		    "forwarding");
		return NULL;
	}

	tun = packet_get_int();
	if (forced_tun_device != -1) {
		if (tun != SSH_TUNID_ANY && forced_tun_device != tun)
			goto done;
		tun = forced_tun_device;
	}
	sock = tun_open(tun, mode);
	if (sock < 0)
		goto done;
	if (options.hpn_disabled)
		c = channel_new("tun", SSH_CHANNEL_OPEN, sock, sock, -1,
		    CHAN_TCP_WINDOW_DEFAULT, CHAN_TCP_PACKET_DEFAULT, 0,
		    "tun", 1);
	else
		c = channel_new("tun", SSH_CHANNEL_OPEN, sock, sock, -1,
		    options.hpn_buffer_size, CHAN_TCP_PACKET_DEFAULT, 0,
		    "tun", 1);
	c->datagram = 1;
#if defined(SSH_TUN_FILTER)
	if (mode == SSH_TUNMODE_POINTOPOINT)
		channel_register_filter(c->self, sys_tun_infilter,
		    sys_tun_outfilter, NULL, NULL);
#endif

 done:
	if (c == NULL)
		packet_send_debug("Failed to open the tunnel device.");
	return c;
}

static Channel *
server_request_session(void)
{
	Channel *c;

	debug("input_session_request");
	packet_check_eom();

	if (no_more_sessions) {
		packet_disconnect("Possible attack: attempt to open a session "
		    "after additional sessions disabled");
	}

	/*
	 * A server session has no fd to read or write until a
	 * CHANNEL_REQUEST for a shell is made, so we set the type to
	 * SSH_CHANNEL_LARVAL.  Additionally, a callback for handling all
	 * CHANNEL_REQUEST messages is registered.
	 */
	c = channel_new("session", SSH_CHANNEL_LARVAL,
	    -1, -1, -1, /*window size*/0, CHAN_SES_PACKET_DEFAULT,
	    0, "server-session", 1);
	if (!options.hpn_disabled && options.tcp_rcv_buf_poll)
		c->dynamic_window = 1;
	if (session_open(the_authctxt, c->self) != 1) {
		debug("session open failed, free channel %d", c->self);
		channel_free(c);
		return NULL;
	}
	channel_register_cleanup(c->self, session_close_by_channel, 0);
	return c;
}

static void
server_input_channel_open(int type, u_int32_t seq, void *ctxt)
{
	Channel *c = NULL;
	char *ctype;
	int rchan;
	u_int rmaxpack, rwindow, len;

	ctype = packet_get_string(&len);
	rchan = packet_get_int();
	rwindow = packet_get_int();
	rmaxpack = packet_get_int();

	debug("server_input_channel_open: ctype %s rchan %d win %d max %d",
	    ctype, rchan, rwindow, rmaxpack);

	if (strcmp(ctype, "session") == 0) {
		c = server_request_session();
	} else if (strcmp(ctype, "direct-tcpip") == 0) {
		c = server_request_direct_tcpip();
	} else if (strcmp(ctype, "tun@openssh.com") == 0) {
		c = server_request_tun();
	}
	if (c != NULL) {
		debug("server_input_channel_open: confirm %s", ctype);
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
		debug("server_input_channel_open: failure %s", ctype);
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
}

static void
server_input_global_request(int type, u_int32_t seq, void *ctxt)
{
	char *rtype;
	int want_reply;
	int success = 0, allocated_listen_port = 0;

	rtype = packet_get_string(NULL);
	want_reply = packet_get_char();
	debug("server_input_global_request: rtype %s want_reply %d", rtype, want_reply);

	/* -R style forwarding */
	if (strcmp(rtype, "tcpip-forward") == 0) {
		struct passwd *pw;
		char *listen_address;
		u_short listen_port;

		pw = the_authctxt->pw;
		if (pw == NULL || !the_authctxt->valid)
			fatal("server_input_global_request: no/invalid user");
		listen_address = packet_get_string(NULL);
		listen_port = (u_short)packet_get_int();
		debug("server_input_global_request: tcpip-forward listen %s port %d",
		    listen_address, listen_port);

		/* check permissions */
		if ((options.allow_tcp_forwarding & FORWARD_REMOTE) == 0 ||
		    no_port_forwarding_flag ||
		    (!want_reply && listen_port == 0)
#ifndef NO_IPPORT_RESERVED_CONCEPT
		    || (listen_port != 0 && listen_port < IPPORT_RESERVED &&
                    pw->pw_uid != 0)
#endif
		    ) {
			success = 0;
			packet_send_debug("Server has disabled port forwarding.");
		} else {
			/* Start listening on the port */
			success = channel_setup_remote_fwd_listener(
			    listen_address, listen_port,
			    &allocated_listen_port, options.gateway_ports);
		}
		free(listen_address);
	} else if (strcmp(rtype, "cancel-tcpip-forward") == 0) {
		char *cancel_address;
		u_short cancel_port;

		cancel_address = packet_get_string(NULL);
		cancel_port = (u_short)packet_get_int();
		debug("%s: cancel-tcpip-forward addr %s port %d", __func__,
		    cancel_address, cancel_port);

		success = channel_cancel_rport_listener(cancel_address,
		    cancel_port);
		free(cancel_address);
	} else if (strcmp(rtype, "no-more-sessions@openssh.com") == 0) {
		no_more_sessions = 1;
		success = 1;
	}
	if (want_reply) {
		packet_start(success ?
		    SSH2_MSG_REQUEST_SUCCESS : SSH2_MSG_REQUEST_FAILURE);
		if (success && allocated_listen_port > 0)
			packet_put_int(allocated_listen_port);
		packet_send();
		packet_write_wait();
	}
	free(rtype);
}

static void
server_input_channel_req(int type, u_int32_t seq, void *ctxt)
{
	Channel *c;
	int id, reply, success = 0;
	char *rtype;

	id = packet_get_int();
	rtype = packet_get_string(NULL);
	reply = packet_get_char();

	debug("server_input_channel_req: channel %d request %s reply %d",
	    id, rtype, reply);

	if ((c = channel_lookup(id)) == NULL)
		packet_disconnect("server_input_channel_req: "
		    "unknown channel %d", id);
	if (!strcmp(rtype, "eow@openssh.com")) {
		packet_check_eom();
		chan_rcvd_eow(c);
	} else if ((c->type == SSH_CHANNEL_LARVAL ||
	    c->type == SSH_CHANNEL_OPEN) && strcmp(c->ctype, "session") == 0)
		success = session_input_channel_req(c, rtype);
	if (reply) {
		packet_start(success ?
		    SSH2_MSG_CHANNEL_SUCCESS : SSH2_MSG_CHANNEL_FAILURE);
		packet_put_int(c->remote_id);
		packet_send();
	}
	free(rtype);
}

static void
server_init_dispatch_20(void)
{
	debug("server_init_dispatch_20");
	dispatch_init(&dispatch_protocol_error);
	dispatch_set(SSH2_MSG_CHANNEL_CLOSE, &channel_input_oclose);
	dispatch_set(SSH2_MSG_CHANNEL_DATA, &channel_input_data);
	dispatch_set(SSH2_MSG_CHANNEL_EOF, &channel_input_ieof);
	dispatch_set(SSH2_MSG_CHANNEL_EXTENDED_DATA, &channel_input_extended_data);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN, &server_input_channel_open);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN_CONFIRMATION, &channel_input_open_confirmation);
	dispatch_set(SSH2_MSG_CHANNEL_OPEN_FAILURE, &channel_input_open_failure);
	dispatch_set(SSH2_MSG_CHANNEL_REQUEST, &server_input_channel_req);
	dispatch_set(SSH2_MSG_CHANNEL_WINDOW_ADJUST, &channel_input_window_adjust);
	dispatch_set(SSH2_MSG_GLOBAL_REQUEST, &server_input_global_request);
	/* client_alive */
	dispatch_set(SSH2_MSG_CHANNEL_SUCCESS, &server_input_keep_alive);
	dispatch_set(SSH2_MSG_CHANNEL_FAILURE, &server_input_keep_alive);
	dispatch_set(SSH2_MSG_REQUEST_SUCCESS, &server_input_keep_alive);
	dispatch_set(SSH2_MSG_REQUEST_FAILURE, &server_input_keep_alive);
	/* rekeying */
	dispatch_set(SSH2_MSG_KEXINIT, &kex_input_kexinit);
}
static void
server_init_dispatch_13(void)
{
	debug("server_init_dispatch_13");
	dispatch_init(NULL);
	dispatch_set(SSH_CMSG_EOF, &server_input_eof);
	dispatch_set(SSH_CMSG_STDIN_DATA, &server_input_stdin_data);
	dispatch_set(SSH_CMSG_WINDOW_SIZE, &server_input_window_size);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE, &channel_input_close);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION, &channel_input_close_confirmation);
	dispatch_set(SSH_MSG_CHANNEL_DATA, &channel_input_data);
	dispatch_set(SSH_MSG_CHANNEL_OPEN_CONFIRMATION, &channel_input_open_confirmation);
	dispatch_set(SSH_MSG_CHANNEL_OPEN_FAILURE, &channel_input_open_failure);
	dispatch_set(SSH_MSG_PORT_OPEN, &channel_input_port_open);
}
static void
server_init_dispatch_15(void)
{
	server_init_dispatch_13();
	debug("server_init_dispatch_15");
	dispatch_set(SSH_MSG_CHANNEL_CLOSE, &channel_input_ieof);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION, &channel_input_oclose);
}
static void
server_init_dispatch(void)
{
	if (compat20)
		server_init_dispatch_20();
	else if (compat13)
		server_init_dispatch_13();
	else
		server_init_dispatch_15();
}
