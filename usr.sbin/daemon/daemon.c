/*-
 * Copyright (c) 1999 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From BSDI: daemon.c,v 1.2 1996/08/15 01:11:09 jch Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <login_cap.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void dummy_sighandler(int);
static void restrict_process(const char *);
static int  wait_child(pid_t pid, sigset_t *mask);
static void usage(void);

int
main(int argc, char *argv[])
{
	struct pidfh *pfh = NULL;
	sigset_t mask, oldmask;
	int ch, nochdir, noclose, restart;
	const char *pidfile, *user;
	pid_t otherpid, pid;

	nochdir = noclose = 1;
	restart = 0;
	pidfile = user = NULL;
	while ((ch = getopt(argc, argv, "cfp:ru:")) != -1) {
		switch (ch) {
		case 'c':
			nochdir = 0;
			break;
		case 'f':
			noclose = 0;
			break;
		case 'p':
			pidfile = optarg;
			break;
		case 'r':
			restart = 1;
			break;
		case 'u':
			user = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	pfh = NULL;
	/*
	 * Try to open the pidfile before calling daemon(3),
	 * to be able to report the error intelligently
	 */
	if (pidfile != NULL) {
		pfh = pidfile_open(pidfile, 0600, &otherpid);
		if (pfh == NULL) {
			if (errno == EEXIST) {
				errx(3, "process already running, pid: %d",
				    otherpid);
			}
			err(2, "pidfile ``%s''", pidfile);
		}
	}

	if (daemon(nochdir, noclose) == -1)
		err(1, NULL);

	/*
	 * If the pidfile or restart option is specified the daemon
	 * executes the command in a forked process and wait on child
	 * exit to remove the pidfile or restart the command. Normally
	 * we don't want the monitoring daemon to be terminated
	 * leaving the running process and the stale pidfile, so we
	 * catch SIGTERM and forward it to the children expecting to
	 * get SIGCHLD eventually.
	 */
	pid = -1;
	if (pidfile != NULL || restart) {
		/*
		 * Restore default action for SIGTERM in case the
		 * parent process decided to ignore it.
		 */
		if (signal(SIGTERM, SIG_DFL) == SIG_ERR)
			err(1, "signal");
		/*
		 * Because SIGCHLD is ignored by default, setup dummy handler
		 * for it, so we can mask it.
		 */
		if (signal(SIGCHLD, dummy_sighandler) == SIG_ERR)
			err(1, "signal");
		/*
		 * Block interesting signals.
		 */
		sigemptyset(&mask);
		sigaddset(&mask, SIGTERM);
		sigaddset(&mask, SIGCHLD);
		if (sigprocmask(SIG_SETMASK, &mask, &oldmask) == -1)
			err(1, "sigprocmask");
		/*
		 * Try to protect against pageout kill. Ignore the
		 * error, madvise(2) will fail only if a process does
		 * not have superuser privileges.
		 */
		(void)madvise(NULL, 0, MADV_PROTECT);
restart:
		/*
		 * Spawn a child to exec the command, so in the parent
		 * we could wait for it to exit and remove pidfile.
		 */
		pid = fork();
		if (pid == -1) {
			pidfile_remove(pfh);
			err(1, "fork");
		}
	}
	if (pid <= 0) {
		if (pid == 0) {
			/* Restore old sigmask in the child. */
			if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1)
				err(1, "sigprocmask");
		}
		/* Now that we are the child, write out the pid. */
		pidfile_write(pfh);

		if (user != NULL)
			restrict_process(user);

		execvp(argv[0], argv);

		/*
		 * execvp() failed -- report the error. The child is
		 * now running, so the exit status doesn't matter.
		 */
		err(1, "%s", argv[0]);
	}
	setproctitle("%s[%d]", argv[0], pid);
	if (wait_child(pid, &mask) == 0 && restart) {
		sleep(1);
		goto restart;
	}
	pidfile_remove(pfh);
	exit(0); /* Exit status does not matter. */
}

static void
dummy_sighandler(int sig __unused)
{
	/* Nothing to do. */
}

static void
restrict_process(const char *user)
{
	struct passwd *pw = NULL;

	pw = getpwnam(user);
	if (pw == NULL)
		errx(1, "unknown user: %s", user);

	if (setusercontext(NULL, pw, pw->pw_uid, LOGIN_SETALL) != 0)
		errx(1, "failed to set user environment");
}

static int
wait_child(pid_t pid, sigset_t *mask)
{
	int terminate, signo;

	terminate = 0;
	for (;;) {
		if (sigwait(mask, &signo) == -1) {
			warn("sigwaitinfo");
			return (-1);
		}
		switch (signo) {
		case SIGCHLD:
			if (waitpid(pid, NULL, WNOHANG) == -1) {
				warn("waitpid");
				return (-1);
			}
			return (terminate);
		case SIGTERM:
			terminate = 1;
			if (kill(pid, signo) == -1) {
				warn("kill");
				return (-1);
			}
			continue;
		default:
			warnx("sigwaitinfo: invalid signal: %d", signo);
			return (-1);
		}
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: daemon [-cfr] [-p pidfile] [-u user] command "
		"arguments ...\n");
	exit(1);
}
