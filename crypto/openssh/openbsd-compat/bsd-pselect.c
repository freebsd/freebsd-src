/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2021 Darren Tucker (dtucker at dtucker net).
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
#ifndef HAVE_PSELECT

#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "misc.h"      /* for set_nonblock */

#ifndef HAVE_SIGHANDLER_T
typedef void (*sighandler_t)(int);
#endif

static sighandler_t saved_sighandler[_NSIG];

/*
 * Set up the descriptors.  Because they are close-on-exec, in the case
 * where sshd's re-exec fails notify_pipe will still point to a descriptor
 * that was closed by the exec attempt but if that descriptor has been
 * reopened then we'll attempt to use that.  Ensure that notify_pipe is
 * outside of the range used by sshd re-exec but within NFDBITS (so we don't
 * need to expand the fd_sets).
 */
#define REEXEC_MIN_FREE_FD (STDERR_FILENO + 4)
static int
pselect_notify_setup_fd(int *fd)
{
	int r;

	if ((r = fcntl(*fd, F_DUPFD, REEXEC_MIN_FREE_FD)) < 0 ||
	    fcntl(r, F_SETFD, FD_CLOEXEC) < 0 || r >= FD_SETSIZE)
		return -1;
	(void)close(*fd);
	return (*fd = r);
}

/*
 * we write to this pipe if a SIGCHLD is caught in order to avoid
 * the race between select() and child_terminated
 */
static pid_t notify_pid;
static int notify_pipe[2];
static void
pselect_notify_setup(void)
{
	static int initialized;

	if (initialized && notify_pid == getpid())
		return;
	if (notify_pid == 0)
		debug3_f("initializing");
	else {
		debug3_f("pid changed, reinitializing");
		if (notify_pipe[0] != -1)
			close(notify_pipe[0]);
		if (notify_pipe[1] != -1)
			close(notify_pipe[1]);
	}
	if (pipe(notify_pipe) == -1) {
		error("pipe(notify_pipe) failed %s", strerror(errno));
	} else if (pselect_notify_setup_fd(&notify_pipe[0]) == -1 ||
	    pselect_notify_setup_fd(&notify_pipe[1]) == -1) {
		error("fcntl(notify_pipe, ...) failed %s", strerror(errno));
		close(notify_pipe[0]);
		close(notify_pipe[1]);
	} else {
		set_nonblock(notify_pipe[0]);
		set_nonblock(notify_pipe[1]);
		notify_pid = getpid();
		debug3_f("pid %d saved %d pipe0 %d pipe1 %d", getpid(),
		    notify_pid, notify_pipe[0], notify_pipe[1]);
		initialized = 1;
		return;
	}
	notify_pipe[0] = -1;    /* read end */
	notify_pipe[1] = -1;    /* write end */
}
static void
pselect_notify_parent(void)
{
	if (notify_pipe[1] != -1)
		(void)write(notify_pipe[1], "", 1);
}
static void
pselect_notify_prepare(fd_set *readset)
{
	if (notify_pipe[0] != -1)
		FD_SET(notify_pipe[0], readset);
}
static void
pselect_notify_done(fd_set *readset)
{
	char c;

	if (notify_pipe[0] != -1 && FD_ISSET(notify_pipe[0], readset)) {
		while (read(notify_pipe[0], &c, 1) != -1)
			debug2_f("reading");
		FD_CLR(notify_pipe[0], readset);
	}
}

/*ARGSUSED*/
static void
pselect_sig_handler(int sig)
{
	int save_errno = errno;

	pselect_notify_parent();
	if (saved_sighandler[sig] != NULL)
		(*saved_sighandler[sig])(sig);  /* call original handler */
	errno = save_errno;
}

/*
 * A minimal implementation of pselect(2), built on top of select(2).
 */

int
pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    const struct timespec *timeout, const sigset_t *mask)
{
	int ret, sig, saved_errno, unmasked = 0;
	sigset_t osig;
	struct sigaction sa, osa;
	struct timeval tv, *tvp = NULL;

	if (timeout != NULL) {
		tv.tv_sec = timeout->tv_sec;
		tv.tv_usec = timeout->tv_nsec / 1000;
		tvp = &tv;
	}
	if (mask == NULL)  /* no signal mask, just call select */
		return select(nfds, readfds, writefds, exceptfds, tvp);

	/* For each signal we're unmasking, install our handler if needed. */
	for (sig = 0; sig < _NSIG; sig++) {
		if (sig == SIGKILL || sig == SIGSTOP || sigismember(mask, sig))
			continue;
		if (sigaction(sig, NULL, &sa) == 0 &&
		    sa.sa_handler != SIG_IGN && sa.sa_handler != SIG_DFL) {
			unmasked = 1;
			if (sa.sa_handler == pselect_sig_handler)
				continue;
			sa.sa_handler = pselect_sig_handler;
			if (sigaction(sig, &sa, &osa) == 0) {
				debug3_f("installing signal handler for %s, "
				    "previous %p", strsignal(sig),
				     osa.sa_handler);
				saved_sighandler[sig] = osa.sa_handler;
			}
		}
	}
	if (unmasked) {
		pselect_notify_setup();
		pselect_notify_prepare(readfds);
		nfds = MAX(nfds, notify_pipe[0]);
	}

	/* Unmask signals, call select then restore signal mask. */
	sigprocmask(SIG_SETMASK, mask, &osig);
	ret = select(nfds, readfds, writefds, exceptfds, tvp);
	saved_errno = errno;
	sigprocmask(SIG_SETMASK, &osig, NULL);

	if (unmasked)
		pselect_notify_done(readfds);
	errno = saved_errno;
	return ret;
}
#endif
