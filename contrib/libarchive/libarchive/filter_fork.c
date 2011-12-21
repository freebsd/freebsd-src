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

/* This capability is only available on POSIX systems. */
#if defined(HAVE_PIPE) && defined(HAVE_FCNTL) && \
    (defined(HAVE_FORK) || defined(HAVE_VFORK))

__FBSDID("$FreeBSD: head/lib/libarchive/filter_fork.c 182958 2008-09-12 05:33:00Z kientzle $");

#if defined(HAVE_POLL) && (defined(HAVE_POLL_H) || defined(HAVE_SYS_POLL_H))
#  if defined(HAVE_POLL_H)
#    include <poll.h>
#  elif defined(HAVE_SYS_POLL_H)
#    include <sys/poll.h>
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
	if (stdin_pipe[0] == 1 /* stdout */) {
		if ((tmp = dup(stdin_pipe[0])) == -1)
			goto stdin_opened;
		close(stdin_pipe[0]);
		stdin_pipe[0] = tmp;
	}
	if (pipe(stdout_pipe) == -1)
		goto stdin_opened;
	if (stdout_pipe[1] == 0 /* stdin */) {
		if ((tmp = dup(stdout_pipe[1])) == -1)
			goto stdout_opened;
		close(stdout_pipe[1]);
		stdout_pipe[1] = tmp;
	}

#if HAVE_VFORK
	switch ((child = vfork())) {
#else
	switch ((child = fork())) {
#endif
	case -1:
		goto stdout_opened;
	case 0:
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		if (dup2(stdin_pipe[0], 0 /* stdin */) == -1)
			_exit(254);
		if (stdin_pipe[0] != 0 /* stdin */)
			close(stdin_pipe[0]);
		if (dup2(stdout_pipe[1], 1 /* stdout */) == -1)
			_exit(254);
		if (stdout_pipe[1] != 1 /* stdout */)
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
#if defined(HAVE_POLL) && (defined(HAVE_POLL_H) || defined(HAVE_SYS_POLL_H))
	struct pollfd fds[2];
	int idx;

	idx = 0;
	if (in != -1) {
		fds[idx].fd = in;
		fds[idx].events = POLLOUT;
		++idx;
	}
	if (out != -1) {
		fds[idx].fd = out;
		fds[idx].events = POLLIN;
		++idx;
	}

	poll(fds, idx, -1); /* -1 == INFTIM, wait forever */
#elif defined(HAVE_SELECT)
	fd_set fds_in, fds_out, fds_error;

	FD_ZERO(&fds_in);
	FD_ZERO(&fds_out);
	FD_ZERO(&fds_error);
	if (out != -1) {
		FD_SET(out, &fds_in);
		FD_SET(out, &fds_error);
	}
	if (in != -1) {
		FD_SET(in, &fds_out);
		FD_SET(in, &fds_error);
	}
	select(in < out ? out + 1 : in + 1, &fds_in, &fds_out, &fds_error, NULL);
#else
	sleep(1);
#endif
}

#endif /* defined(HAVE_PIPE) && defined(HAVE_VFORK) && defined(HAVE_FCNTL) */
