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
 * Copyright (c) 2000 Markus Friedl. All rights reserved.
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
RCSID("$OpenBSD: serverloop.c,v 1.34 2000/10/27 07:32:18 markus Exp $");

#include "xmalloc.h"
#include "ssh.h"
#include "packet.h"
#include "buffer.h"
#include "servconf.h"
#include "pty.h"
#include "channels.h"

#include "compat.h"
#include "ssh2.h"
#include "session.h"
#include "dispatch.h"
#include "auth-options.h"

extern ServerOptions options;

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
static int connection_in;	/* Connection to client (input). */
static int connection_out;	/* Connection to client (output). */
static unsigned int buffer_high;/* "Soft" max buffer size. */
static int max_fd;		/* Max file descriptor number for select(). */

/*
 * This SIGCHLD kludge is used to detect when the child exits.  The server
 * will exit after that, as soon as forwarded connections have terminated.
 */

static pid_t child_pid;			/* Pid of the child. */
static volatile int child_terminated;	/* The child has terminated. */
static volatile int child_wait_status;	/* Status from wait(). */

void	server_init_dispatch(void);

void
sigchld_handler(int sig)
{
	int save_errno = errno;
	pid_t wait_pid;

	debug("Received SIGCHLD.");
	wait_pid = wait((int *) &child_wait_status);
	if (wait_pid != -1) {
		if (wait_pid != child_pid)
			error("Strange, got SIGCHLD and wait returned pid %d but child is %d",
			      wait_pid, child_pid);
		if (WIFEXITED(child_wait_status) ||
		    WIFSIGNALED(child_wait_status))
			child_terminated = 1;
	}
	signal(SIGCHLD, sigchld_handler);
	errno = save_errno;
}
void
sigchld_handler2(int sig)
{
	int save_errno = errno;
	debug("Received SIGCHLD.");
	child_terminated = 1;
	signal(SIGCHLD, sigchld_handler2);
	errno = save_errno;
}

/*
 * Make packets from buffered stderr data, and buffer it for sending
 * to the client.
 */
void
make_packets_from_stderr_data()
{
	int len;

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
void
make_packets_from_stdout_data()
{
	int len;

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

/*
 * Sleep in select() until we can do something.  This will initialize the
 * select masks.  Upon return, the masks will indicate which descriptors
 * have data or can accept data.  Optionally, a maximum time can be specified
 * for the duration of the wait (0 = infinite).
 */
void
wait_until_can_do_something(fd_set * readset, fd_set * writeset,
			    unsigned int max_time_milliseconds)
{
	struct timeval tv, *tvp;
	int ret;

	/* When select fails we restart from here. */
retry_select:

	/* Initialize select() masks. */
	FD_ZERO(readset);
	FD_ZERO(writeset);

	if (compat20) {
		/* wrong: bad condition XXX */
		if (channel_not_very_much_buffered_data())
			FD_SET(connection_in, readset);
	} else {
		/*
		 * Read packets from the client unless we have too much
		 * buffered stdin or channel data.
		 */
		if (buffer_len(&stdin_buffer) < buffer_high &&
		    channel_not_very_much_buffered_data())
			FD_SET(connection_in, readset);
		/*
		 * If there is not too much data already buffered going to
		 * the client, try to get some more data from the program.
		 */
		if (packet_not_very_much_data_to_write()) {
			if (!fdout_eof)
				FD_SET(fdout, readset);
			if (!fderr_eof)
				FD_SET(fderr, readset);
		}
		/*
		 * If we have buffered data, try to write some of that data
		 * to the program.
		 */
		if (fdin != -1 && buffer_len(&stdin_buffer) > 0)
			FD_SET(fdin, writeset);
	}
	/* Set masks for channel descriptors. */
	channel_prepare_select(readset, writeset);

	/*
	 * If we have buffered packet data going to the client, mark that
	 * descriptor.
	 */
	if (packet_have_data_to_write())
		FD_SET(connection_out, writeset);

	/* Update the maximum descriptor number if appropriate. */
	if (channel_max_fd() > max_fd)
		max_fd = channel_max_fd();

	/*
	 * If child has terminated and there is enough buffer space to read
	 * from it, then read as much as is available and exit.
	 */
	if (child_terminated && packet_not_very_much_data_to_write())
		if (max_time_milliseconds == 0)
			max_time_milliseconds = 100;

	if (max_time_milliseconds == 0)
		tvp = NULL;
	else {
		tv.tv_sec = max_time_milliseconds / 1000;
		tv.tv_usec = 1000 * (max_time_milliseconds % 1000);
		tvp = &tv;
	}
	if (tvp!=NULL)
		debug("tvp!=NULL kid %d mili %d", child_terminated, max_time_milliseconds);

	/* Wait for something to happen, or the timeout to expire. */
	ret = select(max_fd + 1, readset, writeset, NULL, tvp);

	if (ret < 0) {
		if (errno != EINTR)
			error("select: %.100s", strerror(errno));
		else
			goto retry_select;
	}
}

/*
 * Processes input from the client and the program.  Input data is stored
 * in buffers and processed later.
 */
void
process_input(fd_set * readset)
{
	int len;
	char buf[16384];

	/* Read and buffer any input data from the client. */
	if (FD_ISSET(connection_in, readset)) {
		len = read(connection_in, buf, sizeof(buf));
		if (len == 0) {
			verbose("Connection closed by remote host.");
			fatal_cleanup();
		} else if (len < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				verbose("Read error from remote host: %.100s", strerror(errno));
				fatal_cleanup();
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
		len = read(fdout, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR || errno == EAGAIN)) {
			/* do nothing */
		} else if (len <= 0) {
			fdout_eof = 1;
		} else {
			buffer_append(&stdout_buffer, buf, len);
			fdout_bytes += len;
		}
	}
	/* Read and buffer any available stderr data from the program. */
	if (!fderr_eof && FD_ISSET(fderr, readset)) {
		len = read(fderr, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR || errno == EAGAIN)) {
			/* do nothing */
		} else if (len <= 0) {
			fderr_eof = 1;
		} else {
			buffer_append(&stderr_buffer, buf, len);
		}
	}
}

/*
 * Sends data from internal buffers to client program stdin.
 */
void
process_output(fd_set * writeset)
{
	int len;

	/* Write buffered data to program stdin. */
	if (!compat20 && fdin != -1 && FD_ISSET(fdin, writeset)) {
		len = write(fdin, buffer_ptr(&stdin_buffer),
		    buffer_len(&stdin_buffer));
		if (len < 0 && (errno == EINTR || errno == EAGAIN)) {
			/* do nothing */
		} else if (len <= 0) {
#ifdef USE_PIPES
			close(fdin);
#else
			if (fdin != fdout)
				close(fdin);
			else
				shutdown(fdin, SHUT_WR); /* We will no longer send. */
#endif
			fdin = -1;
		} else {
			/* Successful write.  Consume the data from the buffer. */
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
void
drain_output()
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

void
process_buffered_input_packets()
{
	dispatch_run(DISPATCH_NONBLOCK, NULL, NULL);
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
	fd_set readset, writeset;
	int wait_status;	/* Status returned by wait(). */
	pid_t wait_pid;		/* pid returned by wait(). */
	int waiting_termination = 0;	/* Have displayed waiting close message. */
	unsigned int max_time_milliseconds;
	unsigned int previous_stdout_buffer_bytes;
	unsigned int stdout_buffer_bytes;
	int type;

	debug("Entering interactive session.");

	/* Initialize the SIGCHLD kludge. */
	child_pid = pid;
	child_terminated = 0;
	signal(SIGCHLD, sigchld_handler);

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

	connection_in = packet_get_connection_in();
	connection_out = packet_get_connection_out();

	previous_stdout_buffer_bytes = 0;

	/* Set approximate I/O buffer size. */
	if (packet_is_interactive())
		buffer_high = 4096;
	else
		buffer_high = 64 * 1024;

	/* Initialize max_fd to the maximum of the known file descriptors. */
	max_fd = fdin;
	if (fdout > max_fd)
		max_fd = fdout;
	if (fderr != -1 && fderr > max_fd)
		max_fd = fderr;
	if (connection_in > max_fd)
		max_fd = connection_in;
	if (connection_out > max_fd)
		max_fd = connection_out;

	/* Initialize Initialize buffers. */
	buffer_init(&stdin_buffer);
	buffer_init(&stdout_buffer);
	buffer_init(&stderr_buffer);

	/*
	 * If we have no separate fderr (which is the case when we have a pty
	 * - there we cannot make difference between data sent to stdout and
	 * stderr), indicate that we have seen an EOF from stderr.  This way
	 * we don\'t need to check the descriptor everywhere.
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
#ifdef USE_PIPES
			close(fdin);
#else
			if (fdin != fdout)
				close(fdin);
			else
				shutdown(fdin, SHUT_WR); /* We will no longer send. */
#endif
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
				xfree(cp);
			}
		}
		/* Sleep in select() until we can do something. */
		wait_until_can_do_something(&readset, &writeset,
					    max_time_milliseconds);

		/* Process any channel events. */
		channel_after_select(&readset, &writeset);

		/* Process input from the client and from program stdout/stderr. */
		process_input(&readset);

		/* Process output to the client and to program stdin. */
		process_output(&writeset);
	}

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

	/* Stop listening for channels; this removes unix domain sockets. */
	channel_stop_listening();

	/* Wait for the child to exit.  Get its exit status. */
	wait_pid = wait(&wait_status);
	if (wait_pid < 0) {
		/*
		 * It is possible that the wait was handled by SIGCHLD
		 * handler.  This may result in either: this call
		 * returning with EINTR, or: this call returning ECHILD.
		 */
		if (child_terminated)
			wait_status = child_wait_status;
		else
			packet_disconnect("wait: %.100s", strerror(errno));
	} else {
		/* Check if it matches the process we forked. */
		if (wait_pid != pid)
			error("Strange, wait returned pid %d, expected %d",
			       wait_pid, pid);
	}

	/* We no longer want our SIGCHLD handler to be called. */
	signal(SIGCHLD, SIG_DFL);

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
			int plen;
			type = packet_read(&plen);
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

void
server_loop2(void)
{
	fd_set readset, writeset;
	int had_channel = 0;
	int status;
	pid_t pid;

	debug("Entering interactive session for SSH2.");

	signal(SIGCHLD, sigchld_handler2);
	child_terminated = 0;
	connection_in = packet_get_connection_in();
	connection_out = packet_get_connection_out();
	max_fd = connection_in;
	if (connection_out > max_fd)
		max_fd = connection_out;
	server_init_dispatch();

	for (;;) {
		process_buffered_input_packets();
		if (!had_channel && channel_still_open())
			had_channel = 1;
		if (had_channel && !channel_still_open()) {
			debug("!channel_still_open.");
			break;
		}
		if (packet_not_very_much_data_to_write())
			channel_output_poll();
		wait_until_can_do_something(&readset, &writeset, 0);
		if (child_terminated) {
			while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
				session_close_by_pid(pid, status);
			child_terminated = 0;
		}
		channel_after_select(&readset, &writeset);
		process_input(&readset);
		process_output(&writeset);
	}
	signal(SIGCHLD, SIG_DFL);
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		session_close_by_pid(pid, status);
	channel_stop_listening();
}

void
server_input_stdin_data(int type, int plen, void *ctxt)
{
	char *data;
	unsigned int data_len;

	/* Stdin data from the client.  Append it to the buffer. */
	/* Ignore any data if the client has closed stdin. */
	if (fdin == -1)
		return;
	data = packet_get_string(&data_len);
	packet_integrity_check(plen, (4 + data_len), type);
	buffer_append(&stdin_buffer, data, data_len);
	memset(data, 0, data_len);
	xfree(data);
}

void
server_input_eof(int type, int plen, void *ctxt)
{
	/*
	 * Eof from the client.  The stdin descriptor to the
	 * program will be closed when all buffered data has
	 * drained.
	 */
	debug("EOF received for stdin.");
	packet_integrity_check(plen, 0, type);
	stdin_eof = 1;
}

void
server_input_window_size(int type, int plen, void *ctxt)
{
	int row = packet_get_int();
	int col = packet_get_int();
	int xpixel = packet_get_int();
	int ypixel = packet_get_int();

	debug("Window change received.");
	packet_integrity_check(plen, 4 * 4, type);
	if (fdin != -1)
		pty_change_window_size(fdin, row, col, xpixel, ypixel);
}

int
input_direct_tcpip(void)
{
	int sock;
	char *target, *originator;
	int target_port, originator_port;

	target = packet_get_string(NULL);
	target_port = packet_get_int();
	originator = packet_get_string(NULL);
	originator_port = packet_get_int();
	packet_done();

	debug("open direct-tcpip: from %s port %d to %s port %d",
	   originator, originator_port, target, target_port);

	/* XXX check permission */
	if (no_port_forwarding_flag || !options.allow_tcp_forwarding) {
		xfree(target);
		xfree(originator);
		return -1;
	}
	sock = channel_connect_to(target, target_port);
	xfree(target);
	xfree(originator);
	if (sock < 0)
		return -1;
	return channel_new("direct-tcpip", SSH_CHANNEL_OPEN,
	    sock, sock, -1, CHAN_TCP_WINDOW_DEFAULT,
	    CHAN_TCP_PACKET_DEFAULT, 0, xstrdup("direct-tcpip"), 1);
}

void
server_input_channel_open(int type, int plen, void *ctxt)
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

	debug("server_input_channel_open: ctype %s rchan %d win %d max %d",
	    ctype, rchan, rwindow, rmaxpack);

	if (strcmp(ctype, "session") == 0) {
		debug("open session");
		packet_done();
		/*
		 * A server session has no fd to read or write
		 * until a CHANNEL_REQUEST for a shell is made,
		 * so we set the type to SSH_CHANNEL_LARVAL.
		 * Additionally, a callback for handling all
		 * CHANNEL_REQUEST messages is registered.
		 */
		id = channel_new(ctype, SSH_CHANNEL_LARVAL,
		    -1, -1, -1, 0, CHAN_SES_PACKET_DEFAULT,
		    0, xstrdup("server-session"), 1);
		if (session_open(id) == 1) {
			channel_register_callback(id, SSH2_MSG_CHANNEL_REQUEST,
			    session_input_channel_req, (void *)0);
			channel_register_cleanup(id, session_close_by_channel);
			c = channel_lookup(id);
		} else {
			debug("session open failed, free channel %d", id);
			channel_free(id);
		}
	} else if (strcmp(ctype, "direct-tcpip") == 0) {
		id = input_direct_tcpip();
		if (id >= 0)
			c = channel_lookup(id);
	}
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
server_init_dispatch_20()
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
	dispatch_set(SSH2_MSG_CHANNEL_REQUEST, &channel_input_channel_request);
	dispatch_set(SSH2_MSG_CHANNEL_WINDOW_ADJUST, &channel_input_window_adjust);
}
void
server_init_dispatch_13()
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
void
server_init_dispatch_15()
{
	server_init_dispatch_13();
	debug("server_init_dispatch_15");
	dispatch_set(SSH_MSG_CHANNEL_CLOSE, &channel_input_ieof);
	dispatch_set(SSH_MSG_CHANNEL_CLOSE_CONFIRMATION, &channel_input_oclose);
}
void
server_init_dispatch()
{
	if (compat20)
		server_init_dispatch_20();
	else if (compat13)
		server_init_dispatch_13();
	else
		server_init_dispatch_15();
}
