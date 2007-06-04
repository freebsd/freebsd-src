/*-
 * Copyright (c) 2007 Joerg Sonnenberger
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

__FBSDID("$FreeBSD$");

#if defined(HAVE_POLL)
#  if defined(HAVE_POLL_H)
#    include <poll.h>
#  endif
#elif defined(HAVE_SELECT)
#  if defined(HAVE_SYS_SELECT_H)
#    include <sys/select.h>
#  elif defined(HAVE_UNISTD_H)
#    include <unistd.h>
#  endif
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "filter_fork.h"

pid_t
__archive_create_child(const char *path, int *child_stdin, int *child_stdout)
{
	pid_t child;
	int stdin_pipe[2], stdout_pipe[2], tmp;

	if (pipe(stdin_pipe) == -1)
		goto state_allocated;
	if (stdin_pipe[0] == STDOUT_FILENO) {
		if ((tmp = dup(stdin_pipe[0])) == -1)
			goto stdin_opened;
		close(stdin_pipe[0]);
		stdin_pipe[0] = tmp;
	}
	if (pipe(stdout_pipe) == -1)
		goto stdin_opened;
	if (stdout_pipe[1] == STDIN_FILENO) {
		if ((tmp = dup(stdout_pipe[1])) == -1)
			goto stdout_opened;
		close(stdout_pipe[1]);
		stdout_pipe[1] = tmp;
	}

	switch ((child = vfork())) {
	case -1:
		goto stdout_opened;
	case 0:
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		if (dup2(stdin_pipe[0], STDIN_FILENO) == -1)
			_exit(254);
		if (stdin_pipe[0] != STDIN_FILENO)
			close(stdin_pipe[0]);
		if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1)
			_exit(254);
		if (stdout_pipe[1] != STDOUT_FILENO)
			close(stdout_pipe[1]);
		execlp(path, path, (char *)NULL);
		_exit(254);		
	default:
		close(stdin_pipe[0]);
		close(stdout_pipe[1]);

		*child_stdin = stdin_pipe[1];
		fcntl(*child_stdin, F_SETFL, O_NONBLOCK);
		*child_stdout = stdout_pipe[0];
		fcntl(*child_stdout, F_SETFL, O_NONBLOCK);
	}

	return child;

stdout_opened:
	close(stdout_pipe[0]);
	close(stdout_pipe[1]);
stdin_opened:
	close(stdin_pipe[0]);
	close(stdin_pipe[1]);
state_allocated:
	return -1;
}

void
__archive_check_child(int in, int out)
{
#if defined(HAVE_POLL)
	struct pollfd fds[2];

	fds[0].fd = in;
	fds[0].events = POLLOUT;
	fds[1].fd = out;
	fds[1].events = POLLIN;

	poll(fds, 2, -1); /* -1 == INFTIM, wait forever */
#elif defined(HAVE_SELECT)
	fd_set fds_in, fds_out, fds_error;

	FD_ZERO(&fds_in);
	FD_SET(out, &fds_in);
	FD_ZERO(&fds_out);
	FD_SET(in, &fds_out);
	FD_ZERO(&fds_error);
	FD_SET(in, &fds_error);
	FD_SET(out, &fds_error);
	select(in < out ? out + 1 : in + 1, &fds_in, &fds_out, &fds_error, NULL);
#else
	sleep(1);
#endif
}
