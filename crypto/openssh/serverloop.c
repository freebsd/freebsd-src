/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Sun Sep 10 00:30:37 1995 ylo
 * Server main loop for handling the interactive session.
 */

#include "includes.h"
#include "xmalloc.h"
#include "ssh.h"
#include "packet.h"
#include "buffer.h"
#include "servconf.h"
#include "pty.h"

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

static int child_pid;			/* Pid of the child. */
static volatile int child_terminated;	/* The child has terminated. */
static volatile int child_wait_status;	/* Status from wait(). */

void 
sigchld_handler(int sig)
{
	int save_errno = errno;
	int wait_pid;
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

/*
 * Process any buffered packets that have been received from the client.
 */
void 
process_buffered_input_packets()
{
	int type;
	char *data;
	unsigned int data_len;
	int row, col, xpixel, ypixel;
	int payload_len;

	/* Process buffered packets from the client. */
	while ((type = packet_read_poll(&payload_len)) != SSH_MSG_NONE) {
		switch (type) {
		case SSH_CMSG_STDIN_DATA:
			/* Stdin data from the client.  Append it to the buffer. */
			/* Ignore any data if the client has closed stdin. */
			if (fdin == -1)
				break;
			data = packet_get_string(&data_len);
			packet_integrity_check(payload_len, (4 + data_len), type);
			buffer_append(&stdin_buffer, data, data_len);
			memset(data, 0, data_len);
			xfree(data);
			break;

		case SSH_CMSG_EOF:
			/*
			 * Eof from the client.  The stdin descriptor to the
			 * program will be closed when all buffered data has
			 * drained.
			 */
			debug("EOF received for stdin.");
			packet_integrity_check(payload_len, 0, type);
			stdin_eof = 1;
			break;

		case SSH_CMSG_WINDOW_SIZE:
			debug("Window change received.");
			packet_integrity_check(payload_len, 4 * 4, type);
			row = packet_get_int();
			col = packet_get_int();
			xpixel = packet_get_int();
			ypixel = packet_get_int();
			if (fdin != -1)
				pty_change_window_size(fdin, row, col, xpixel, ypixel);
			break;

		case SSH_MSG_PORT_OPEN:
			debug("Received port open request.");
			channel_input_port_open(payload_len);
			break;

		case SSH_MSG_CHANNEL_OPEN_CONFIRMATION:
			debug("Received channel open confirmation.");
			packet_integrity_check(payload_len, 4 + 4, type);
			channel_input_open_confirmation();
			break;

		case SSH_MSG_CHANNEL_OPEN_FAILURE:
			debug("Received channel open failure.");
			packet_integrity_check(payload_len, 4, type);
			channel_input_open_failure();
			break;

		case SSH_MSG_CHANNEL_DATA:
			channel_input_data(payload_len);
			break;

		case SSH_MSG_CHANNEL_CLOSE:
			debug("Received channel close.");
			packet_integrity_check(payload_len, 4, type);
			channel_input_close();
			break;

		case SSH_MSG_CHANNEL_CLOSE_CONFIRMATION:
			debug("Received channel close confirmation.");
			packet_integrity_check(payload_len, 4, type);
			channel_input_close_confirmation();
			break;

		default:
			/*
			 * In this phase, any unexpected messages cause a
			 * protocol error.  This is to ease debugging; also,
			 * since no confirmations are sent messages,
			 * unprocessed unknown messages could cause strange
			 * problems.  Any compatible protocol extensions must
			 * be negotiated before entering the interactive
			 * session.
			 */
			packet_disconnect("Protocol error during session: type %d",
					  type);
		}
	}
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

	/*
	 * Read packets from the client unless we have too much buffered
	 * stdin or channel data.
	 */
	if (buffer_len(&stdin_buffer) < 4096 &&
	    channel_not_very_much_buffered_data())
		FD_SET(connection_in, readset);

	/*
	 * If there is not too much data already buffered going to the
	 * client, try to get some more data from the program.
	 */
	if (packet_not_very_much_data_to_write()) {
		if (!fdout_eof)
			FD_SET(fdout, readset);
		if (!fderr_eof)
			FD_SET(fderr, readset);
	}
	FD_ZERO(writeset);

	/* Set masks for channel descriptors. */
	channel_prepare_select(readset, writeset);

	/*
	 * If we have buffered packet data going to the client, mark that
	 * descriptor.
	 */
	if (packet_have_data_to_write())
		FD_SET(connection_out, writeset);

	/* If we have buffered data, try to write some of that data to the
	   program. */
	if (fdin != -1 && buffer_len(&stdin_buffer) > 0)
		FD_SET(fdin, writeset);

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
		}
		/*
		 * There is a kernel bug on Solaris that causes select to
		 * sometimes wake up even though there is no data available.
		 */
		if (len < 0 && errno == EAGAIN)
			len = 0;

		if (len < 0) {
			verbose("Read error from remote host: %.100s", strerror(errno));
			fatal_cleanup();
		}
		/* Buffer any received data. */
		packet_process_incoming(buf, len);
	}
	/* Read and buffer any available stdout data from the program. */
	if (!fdout_eof && FD_ISSET(fdout, readset)) {
		len = read(fdout, buf, sizeof(buf));
		if (len <= 0)
			fdout_eof = 1;
		else {
			buffer_append(&stdout_buffer, buf, len);
			fdout_bytes += len;
		}
	}
	/* Read and buffer any available stderr data from the program. */
	if (!fderr_eof && FD_ISSET(fderr, readset)) {
		len = read(fderr, buf, sizeof(buf));
		if (len <= 0)
			fderr_eof = 1;
		else
			buffer_append(&stderr_buffer, buf, len);
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
	if (fdin != -1 && FD_ISSET(fdin, writeset)) {
		len = write(fdin, buffer_ptr(&stdin_buffer),
		    buffer_len(&stdin_buffer));
		if (len <= 0) {
#ifdef USE_PIPES
			close(fdin);
#else
			if (fdout == -1)
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

/*
 * Performs the interactive session.  This handles data transmission between
 * the client and the program.  Note that the notion of stdin, stdout, and
 * stderr in this function is sort of reversed: this function writes to
 * stdin (of the child program), and reads from stdout and stderr (of the
 * child program).
 */
void 
server_loop(int pid, int fdin_arg, int fdout_arg, int fderr_arg)
{
	int wait_status, wait_pid;	/* Status and pid returned by wait(). */
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

	/* Main loop of the server for the interactive session mode. */
	for (;;) {
		fd_set readset, writeset;

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
			if (fdout == -1)
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
				goto quit;
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

quit:
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
