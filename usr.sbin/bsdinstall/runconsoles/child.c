/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Jessica Clarke <jrtc27@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/procctl.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>
#include <ttyent.h>
#include <unistd.h>

#include "common.h"
#include "child.h"

/* -1: not started, 0: reaped */
static volatile pid_t grandchild_pid = -1;
static volatile int grandchild_status;

static struct pipe_barrier wait_grandchild_barrier;
static struct pipe_barrier wait_all_descendants_barrier;

static void
kill_descendants(int sig)
{
	struct procctl_reaper_kill rk;

	rk.rk_sig = sig;
	rk.rk_flags = 0;
	procctl(P_PID, getpid(), PROC_REAP_KILL, &rk);
}

static void
sigalrm_handler(int sig __unused)
{
	int saved_errno;

	saved_errno = errno;
	kill_descendants(SIGKILL);
	errno = saved_errno;
}

static void
wait_all_descendants(void)
{
	sigset_t set, oset;

	err_set_exit(NULL);

	/*
	 * We may be run in a context where SIGALRM is blocked; temporarily
	 * unblock so we can SIGKILL. Similarly, SIGCHLD may be blocked, but if
	 * we're waiting on the pipe we need to make sure it's not.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &set, &oset);
	alarm(KILL_TIMEOUT);
	pipe_barrier_wait(&wait_all_descendants_barrier);
	alarm(0);
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

static void
sigchld_handler(int sig __unused)
{
	int status, saved_errno;
	pid_t pid;

	saved_errno = errno;

	while ((void)(pid = waitpid(-1, &status, WNOHANG)),
	    pid != -1 && pid != 0) {
		/* NB: No need to check grandchild_pid due to the pid checks */
		if (pid == grandchild_pid) {
			grandchild_status = status;
			grandchild_pid = 0;
			pipe_barrier_ready(&wait_grandchild_barrier);
		}
	}

	/*
	 * Another process calling kill(..., SIGCHLD) could cause us to get
	 * here before we've spawned the grandchild; only ready when we have no
	 * children if the grandchild has been reaped.
	 */
	if (pid == -1 && errno == ECHILD && grandchild_pid == 0)
		pipe_barrier_ready(&wait_all_descendants_barrier);

	errno = saved_errno;
}

static void
exit_signal_handler(int sig)
{
	int saved_errno;

	/*
	 * If we get killed before we've started the grandchild then just exit
	 * with that signal, otherwise kill all our descendants with that
	 * signal and let the main program pick up the grandchild's death.
	 */
	if (grandchild_pid == -1) {
		reproduce_signal_death(sig);
		exit(EXIT_FAILURE);
	}

	saved_errno = errno;
	kill_descendants(sig);
	errno = saved_errno;
}

static void
kill_wait_all_descendants(int sig)
{
	kill_descendants(sig);
	wait_all_descendants();
}

static void
kill_wait_all_descendants_err_exit(int eval __unused)
{
	kill_wait_all_descendants(SIGTERM);
}

static void __dead2
grandchild_run(const char **argv, const sigset_t *oset)
{
	sig_t orig;

	/* Restore signals */
	orig = signal(SIGALRM, SIG_DFL);
	if (orig == SIG_ERR)
		err(EX_OSERR, "could not restore SIGALRM");
	orig = signal(SIGCHLD, SIG_DFL);
	if (orig == SIG_ERR)
		err(EX_OSERR, "could not restore SIGCHLD");
	orig = signal(SIGTERM, SIG_DFL);
	if (orig == SIG_ERR)
		err(EX_OSERR, "could not restore SIGTERM");
	orig = signal(SIGINT, SIG_DFL);
	if (orig == SIG_ERR)
		err(EX_OSERR, "could not restore SIGINT");
	orig = signal(SIGQUIT, SIG_DFL);
	if (orig == SIG_ERR)
		err(EX_OSERR, "could not restore SIGQUIT");
	orig = signal(SIGPIPE, SIG_DFL);
	if (orig == SIG_ERR)
		err(EX_OSERR, "could not restore SIGPIPE");
	orig = signal(SIGTTOU, SIG_DFL);
	if (orig == SIG_ERR)
		err(EX_OSERR, "could not restore SIGTTOU");

	/* Now safe to unmask signals */
	sigprocmask(SIG_SETMASK, oset, NULL);

	/* Only run with stdin/stdout/stderr */
	closefrom(3);

	/* Ready to execute the requested program */
	execvp(argv[0], __DECONST(char * const *, argv));
	err(EX_OSERR, "cannot execvp %s", argv[0]);
}

static int
wait_grandchild_descendants(void)
{
	pipe_barrier_wait(&wait_grandchild_barrier);

	/*
	 * Once the grandchild itself has exited, kill any lingering
	 * descendants and wait until we've reaped them all.
	 */
	kill_wait_all_descendants(SIGTERM);

	if (grandchild_pid != 0)
		errx(EX_SOFTWARE, "failed to reap grandchild");

	return (grandchild_status);
}

void
child_leader_run(const char *name, int fd, bool new_session, const char **argv,
    const sigset_t *oset, struct pipe_barrier *start_children_barrier)
{
	struct pipe_barrier start_grandchild_barrier;
	pid_t pid, sid, pgid;
	struct sigaction sa;
	int error, status;
	sigset_t set;

	setproctitle("%s [%s]", getprogname(), name);

	error = procctl(P_PID, getpid(), PROC_REAP_ACQUIRE, NULL);
	if (error != 0)
		err(EX_OSERR, "could not acquire reaper status");

	/*
	 * Set up our own signal handlers for everything the parent overrides
	 * other than SIGPIPE and SIGTTOU which we leave as ignored, since we
	 * also use pipe-based synchronisation and want to be able to print
	 * errors.
	 */
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigchld_handler;
	sigfillset(&sa.sa_mask);
	error = sigaction(SIGCHLD, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGCHLD handler");
	sa.sa_handler = sigalrm_handler;
	error = sigaction(SIGALRM, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGALRM handler");
	sa.sa_handler = exit_signal_handler;
	error = sigaction(SIGTERM, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGTERM handler");
	error = sigaction(SIGINT, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGINT handler");
	error = sigaction(SIGQUIT, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGQUIT handler");

	/*
	 * Now safe to unmask signals. Note that creating the barriers used by
	 * the SIGCHLD handler with signals unmasked is safe since they won't
	 * be used if the grandchild hasn't been forked (and reaped), which
	 * comes later.
	 */
	sigprocmask(SIG_SETMASK, oset, NULL);

	error = pipe_barrier_init(&start_grandchild_barrier);
	if (error != 0)
		err(EX_OSERR, "could not create start grandchild barrier");

	error = pipe_barrier_init(&wait_grandchild_barrier);
	if (error != 0)
		err(EX_OSERR, "could not create wait grandchild barrier");

	error = pipe_barrier_init(&wait_all_descendants_barrier);
	if (error != 0)
		err(EX_OSERR, "could not create wait all descendants barrier");

	/*
	 * Create a new session if this is on a different terminal to
	 * the current one, otherwise just create a new process group to keep
	 * things as similar as possible between the two cases.
	 */
	if (new_session) {
		sid = setsid();
		pgid = sid;
		if (sid == -1)
			err(EX_OSERR, "could not create session");
	} else {
		sid = -1;
		pgid = getpid();
		error = setpgid(0, pgid);
		if (error == -1)
			err(EX_OSERR, "could not create process group");
	}

	/* Wait until parent is ready for us to start */
	pipe_barrier_destroy_ready(start_children_barrier);
	pipe_barrier_wait(start_children_barrier);

	/*
	 * Use the console for stdin/stdout/stderr.
	 *
	 * NB: dup2(2) is a no-op if the two fds are equal, and the call to
	 * closefrom(2) later in the grandchild will close the fd if it isn't
	 * one of stdin/stdout/stderr already. This means we do not need to
	 * handle that special case differently.
	 */
	error = dup2(fd, STDIN_FILENO);
	if (error == -1)
		err(EX_IOERR, "could not dup %s to stdin", name);
	error = dup2(fd, STDOUT_FILENO);
	if (error == -1)
		err(EX_IOERR, "could not dup %s to stdout", name);
	error = dup2(fd, STDERR_FILENO);
	if (error == -1)
		err(EX_IOERR, "could not dup %s to stderr", name);

	/*
	 * If we created a new session, make the console our controlling
	 * terminal. Either way, also make this the foreground process group.
	 */
	if (new_session) {
		error = tcsetsid(STDIN_FILENO, sid);
		if (error != 0)
			err(EX_IOERR, "could not set session for %s", name);
	} else {
		error = tcsetpgrp(STDIN_FILENO, pgid);
		if (error != 0)
			err(EX_IOERR, "could not set process group for %s",
			    name);
	}

	/*
	 * Temporarily block signals again; forking, setting grandchild_pid and
	 * calling err_set_exit need to all be atomic for similar reasons as
	 * the parent when forking us.
	 */
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, NULL);
	pid = fork();
	if (pid == -1)
		err(EX_OSERR, "could not fork");

	if (pid == 0) {
		/*
		 * We need to destroy the ready ends so we don't block these
		 * child leader-only self-pipes, and might as well destroy the
		 * wait ends too given we're not going to use them.
		 */
		pipe_barrier_destroy(&wait_grandchild_barrier);
		pipe_barrier_destroy(&wait_all_descendants_barrier);

		/* Wait until the parent has put us in a new process group */
		pipe_barrier_destroy_ready(&start_grandchild_barrier);
		pipe_barrier_wait(&start_grandchild_barrier);
		grandchild_run(argv, oset);
	}

	grandchild_pid = pid;

	/*
	 * Now the grandchild exists make sure to clean it up, and any of its
	 * descendants, on exit.
	 */
	err_set_exit(kill_wait_all_descendants_err_exit);

	sigprocmask(SIG_SETMASK, oset, NULL);

	/* Start the grandchild and wait for it and its descendants to exit */
	pipe_barrier_ready(&start_grandchild_barrier);

	status = wait_grandchild_descendants();

	if (WIFSIGNALED(status))
		reproduce_signal_death(WTERMSIG(status));

	if (WIFEXITED(status))
		exit(WEXITSTATUS(status));

	exit(EXIT_FAILURE);
}
