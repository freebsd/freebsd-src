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

/*
 * We create the following process hierarchy:
 *
 *   runconsoles utility
 *   |-- runconsoles [ttyX]
 *   |   `-- utility primary
 *   |-- runconsoles [ttyY]
 *   |   `-- utility secondary
 *   ...
 *   `-- runconsoles [ttyZ]
 *       `-- utility secondary
 *
 * Whilst the intermediate processes might seem unnecessary, they are important
 * so we can ensure the session leader stays around until the actual program
 * being run and all its children have exited when killing them (and, in the
 * case of our controlling terminal, that nothing in our current session goes
 * on to write to it before then), giving them a chance to clean up the
 * terminal (important if a dialog box is showing).
 *
 * Each of the intermediate processes acquires reaper status, allowing it to
 * kill its descendants, not just a single process group, and wait until all
 * have finished, not just its immediate child.
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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

struct consinfo {
	const char		*name;
	STAILQ_ENTRY(consinfo)	link;
	int			fd;
	/* -1: not started, 0: reaped */
	volatile pid_t		pid;
	volatile int		exitstatus;
};

STAILQ_HEAD(consinfo_list, consinfo);

static struct consinfo_list consinfos;
static struct consinfo *primary_consinfo;
static struct consinfo *controlling_consinfo;

static struct consinfo * volatile first_sigchld_consinfo;

static struct pipe_barrier wait_first_child_barrier;
static struct pipe_barrier wait_all_children_barrier;

static const char primary[] = "primary";
static const char secondary[] = "secondary";

static const struct option longopts[] = {
	{ "help",	no_argument,	NULL,	'h' },
	{ NULL,		0,		NULL,	0 }
};

static void
kill_consoles(int sig)
{
	struct consinfo *consinfo;
	sigset_t set, oset;

	/* Temporarily block signals so PID reading and killing are atomic */
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oset);
	STAILQ_FOREACH(consinfo, &consinfos, link) {
		if (consinfo->pid != -1 && consinfo->pid != 0)
			kill(consinfo->pid, sig);
	}
	sigprocmask(SIG_SETMASK, &oset, NULL);
}

static void
sigalrm_handler(int code __unused)
{
	int saved_errno;

	saved_errno = errno;
	kill_consoles(SIGKILL);
	errno = saved_errno;
}

static void
wait_all_consoles(void)
{
	sigset_t set, oset;
	int error;

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
	pipe_barrier_wait(&wait_all_children_barrier);
	alarm(0);
	sigprocmask(SIG_SETMASK, &oset, NULL);

	if (controlling_consinfo != NULL) {
		error = tcsetpgrp(controlling_consinfo->fd,
		    getpgrp());
		if (error != 0)
			err(EX_OSERR, "could not give up control of %s",
			    controlling_consinfo->name);
	}
}

static void
kill_wait_all_consoles(int sig)
{
	kill_consoles(sig);
	wait_all_consoles();
}

static void
kill_wait_all_consoles_err_exit(int eval __unused)
{
	kill_wait_all_consoles(SIGTERM);
}

static void __dead2
exit_signal_handler(int code)
{
	struct consinfo *consinfo;
	bool started_console;

	started_console = false;
	STAILQ_FOREACH(consinfo, &consinfos, link) {
		if (consinfo->pid != -1) {
			started_console = true;
			break;
		}
	}

	/*
	 * If we haven't yet started a console, don't wait for them, since
	 * we'll never get a SIGCHLD that will wake us up.
	 */
	if (started_console)
		kill_wait_all_consoles(SIGTERM);

	reproduce_signal_death(code);
	exit(EXIT_FAILURE);
}

static void
sigchld_handler_reaped_one(pid_t pid, int status)
{
	struct consinfo *consinfo, *child_consinfo;
	bool others;

	child_consinfo = NULL;
	others = false;
	STAILQ_FOREACH(consinfo, &consinfos, link) {
		/*
		 * NB: No need to check consinfo->pid as the caller is
		 * responsible for passing a valid PID
		 */
		if (consinfo->pid == pid)
			child_consinfo = consinfo;
		else if (consinfo->pid != -1 && consinfo->pid != 0)
			others = true;
	}

	if (child_consinfo == NULL)
		return;

	child_consinfo->pid = 0;
	child_consinfo->exitstatus = status;

	if (first_sigchld_consinfo == NULL) {
		first_sigchld_consinfo = child_consinfo;
		pipe_barrier_ready(&wait_first_child_barrier);
	}

	if (others)
		return;

	pipe_barrier_ready(&wait_all_children_barrier);
}

static void
sigchld_handler(int code __unused)
{
	int status, saved_errno;
	pid_t pid;

	saved_errno = errno;
	while ((void)(pid = waitpid(-1, &status, WNOHANG)),
	    pid != -1 && pid != 0)
		sigchld_handler_reaped_one(pid, status);
	errno = saved_errno;
}

static const char *
read_primary_console(void)
{
	char *buf, *p, *cons;
	size_t len;
	int error;

	/*
	 * NB: Format is "cons,...cons,/cons,...cons,", with the list before
	 * the / being the set of configured consoles, and the list after being
	 * the list of available consoles.
	 */
	error = sysctlbyname("kern.console", NULL, &len, NULL, 0);
	if (error == -1)
		err(EX_OSERR, "could not read kern.console length");
	buf = malloc(len);
	if (buf == NULL)
		err(EX_OSERR, "could not allocate kern.console buffer");
	error = sysctlbyname("kern.console", buf, &len, NULL, 0);
	if (error == -1)
		err(EX_OSERR, "could not read kern.console");

	/* Truncate at / to get just the configured consoles */
	p = strchr(buf, '/');
	if (p == NULL)
		errx(EX_OSERR, "kern.console malformed: no / found");
	*p = '\0';

	/*
	 * Truncate at , to get just the first configured console, the primary
	 * ("high level") one.
	 */
	p = strchr(buf, ',');
	if (p != NULL)
		*p = '\0';

	if (*buf != '\0')
		cons = strdup(buf);
	else
		cons = NULL;

	free(buf);

	return (cons);
}

static void
read_consoles(void)
{
	const char *primary_console;
	struct consinfo *consinfo;
	int fd, error, flags;
	struct ttyent *tty;
	char *dev, *name;
	pid_t pgrp;

	primary_console = read_primary_console();

	STAILQ_INIT(&consinfos);
	while ((tty = getttyent()) != NULL) {
		if ((tty->ty_status & TTY_ON) == 0)
			continue;

		/*
		 * Only use the first VTY; starting on others is pointless as
		 * they're multiplexed, and they get used to show the install
		 * log and start a shell.
		 */
		if (strncmp(tty->ty_name, "ttyv", 4) == 0 &&
		    strcmp(tty->ty_name + 4, "0") != 0)
			continue;

		consinfo = malloc(sizeof(struct consinfo));
		if (consinfo == NULL)
			err(EX_OSERR, "could not allocate consinfo");

		asprintf(&dev, "/dev/%s", tty->ty_name);
		if (dev == NULL)
			err(EX_OSERR, "could not allocate dev path");

		name = dev + 5;
		fd = open(dev, O_RDWR | O_NONBLOCK);
		if (fd == -1)
			err(EX_IOERR, "could not open %s", dev);

		flags = fcntl(fd, F_GETFL);
		if (flags == -1)
			err(EX_IOERR, "could not get flags for %s", dev);

		error = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		if (error == -1)
			err(EX_IOERR, "could not set flags for %s", dev);

		if (tcgetsid(fd) != -1) {
			/*
			 * No need to check controlling session is ours as
			 * tcgetsid fails with ENOTTY if not.
			 */
			pgrp = tcgetpgrp(fd);
			if (pgrp == -1)
				err(EX_IOERR, "could not get pgrp of %s",
				    dev);
			else if (pgrp != getpgrp())
				errx(EX_IOERR, "%s controlled by another group",
				    dev);

			if (controlling_consinfo != NULL)
				errx(EX_OSERR,
				    "multiple controlling terminals %s and %s",
				    controlling_consinfo->name, name);

			controlling_consinfo = consinfo;
		}

		consinfo->name = name;
		consinfo->pid = -1;
		consinfo->fd = fd;
		consinfo->exitstatus = -1;
		STAILQ_INSERT_TAIL(&consinfos, consinfo, link);

		if (primary_console != NULL &&
		    strcmp(consinfo->name, primary_console) == 0)
			primary_consinfo = consinfo;
	}

	endttyent();
	free(__DECONST(char *, primary_console));

	if (STAILQ_EMPTY(&consinfos))
		errx(EX_OSERR, "no consoles found");

	if (primary_consinfo == NULL) {
		warnx("no primary console found, using first");
		primary_consinfo = STAILQ_FIRST(&consinfos);
	}
}

static void
start_console(struct consinfo *consinfo, const char **argv,
    char *primary_secondary, struct pipe_barrier *start_barrier,
    const sigset_t *oset)
{
	pid_t pid;

	if (consinfo == primary_consinfo)
		strcpy(primary_secondary, primary);
	else
		strcpy(primary_secondary, secondary);

	fprintf(stderr, "Starting %s installer on %s\n", primary_secondary,
	    consinfo->name);

	pid = fork();
	if (pid == -1)
		err(EX_OSERR, "could not fork");

	if (pid == 0) {
		/* Redundant for the first fork but not subsequent ones */
		err_set_exit(NULL);

		/*
		 * We need to destroy the ready ends so we don't block these
		 * parent-only self-pipes, and might as well destroy the wait
		 * ends too given we're not going to use them.
		 */
		pipe_barrier_destroy(&wait_first_child_barrier);
		pipe_barrier_destroy(&wait_all_children_barrier);

		child_leader_run(consinfo->name, consinfo->fd,
		    consinfo != controlling_consinfo, argv, oset,
		    start_barrier);
	}

	consinfo->pid = pid;

	/*
	 * We have at least one child now so make sure we kill children on
	 * exit. We also must not do this until we have at least one since
	 * otherwise we will never receive a SIGCHLD that will ready the pipe
	 * barrier and thus we will wait forever.
	 */
	err_set_exit(kill_wait_all_consoles_err_exit);
}

static void
start_consoles(int argc, char **argv)
{
	struct pipe_barrier start_barrier;
	struct consinfo *consinfo;
	char *primary_secondary;
	const char **newargv;
	struct sigaction sa;
	sigset_t set, oset;
	int error, i;

	error = pipe_barrier_init(&start_barrier);
	if (error != 0)
		err(EX_OSERR, "could not create start children barrier");

	error = pipe_barrier_init(&wait_first_child_barrier);
	if (error != 0)
		err(EX_OSERR, "could not create wait first child barrier");

	error = pipe_barrier_init(&wait_all_children_barrier);
	if (error != 0)
		err(EX_OSERR, "could not create wait all children barrier");

	/*
	 * About to start children, so use our SIGCHLD handler to get notified
	 * when we need to stop. Once the first child has started we will have
	 * registered kill_wait_all_consoles_err_exit which needs our SIGALRM handler to
	 * SIGKILL the children on timeout; do it up front so we can err if it
	 * fails beforehand.
	 *
	 * Also set up our SIGTERM (and SIGINT and SIGQUIT if we're keeping
	 * control of this terminal) handler before we start children so we can
	 * clean them up when signalled.
	 */
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sa.sa_handler = sigchld_handler;
	sigfillset(&sa.sa_mask);
	error = sigaction(SIGCHLD, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGCHLD handler");
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigalrm_handler;
	error = sigaction(SIGALRM, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGALRM handler");
	sa.sa_handler = exit_signal_handler;
	error = sigaction(SIGTERM, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not enable SIGTERM handler");
	if (controlling_consinfo == NULL) {
		error = sigaction(SIGINT, &sa, NULL);
		if (error != 0)
			err(EX_OSERR, "could not enable SIGINT handler");
		error = sigaction(SIGQUIT, &sa, NULL);
		if (error != 0)
			err(EX_OSERR, "could not enable SIGQUIT handler");
	}

	/*
	 * Ignore SIGINT/SIGQUIT in parent if a child leader will take control
	 * of this terminal so only it gets them, and ignore SIGPIPE in parent,
	 * and child until unblocked, since we're using pipes internally as
	 * synchronisation barriers between parent and children.
	 *
	 * Also ignore SIGTTOU so we can print errors if needed after the child
	 * has started.
	 */
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;
	if (controlling_consinfo != NULL) {
		error = sigaction(SIGINT, &sa, NULL);
		if (error != 0)
			err(EX_OSERR, "could not ignore SIGINT");
		error = sigaction(SIGQUIT, &sa, NULL);
		if (error != 0)
			err(EX_OSERR, "could not ignore SIGQUIT");
	}
	error = sigaction(SIGPIPE, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not ignore SIGPIPE");
	error = sigaction(SIGTTOU, &sa, NULL);
	if (error != 0)
		err(EX_OSERR, "could not ignore SIGTTOU");

	/*
	 * Create a fresh copy of the argument array and perform %-substitution;
	 * a literal % will be replaced with primary_secondary, and any other
	 * string that starts % will have the leading % removed (thus arguments
	 * that should start with a % should be escaped with an additional %).
	 *
	 * Having all % arguments use primary_secondary means that copying
	 * either "primary" or "secondary" to it will yield the final argument
	 * array for the child in constant time, regardless of how many appear.
	 */
	newargv = malloc(((size_t)argc + 1) * sizeof(char *));
	if (newargv == NULL)
		err(EX_OSERR, "could not allocate newargv");

	primary_secondary = malloc(MAX(sizeof(primary), sizeof(secondary)));
	if (primary_secondary == NULL)
		err(EX_OSERR, "could not allocate primary_secondary");

	newargv[0] = argv[0];
	for (i = 1; i < argc; ++i) {
		switch (argv[i][0]) {
		case '%':
			if (argv[i][1] == '\0')
				newargv[i] = primary_secondary;
			else
				newargv[i] = argv[i] + 1;
			break;
		default:
			newargv[i] = argv[i];
			break;
		}
	}
	newargv[argc] = NULL;

	/*
	 * Temporarily block signals. The parent needs forking, assigning
	 * consinfo->pid and, for the first iteration, calling err_set_exit, to
	 * be atomic, and the child leader shouldn't have signals re-enabled
	 * until it has configured its signal handlers appropriately as the
	 * current ones are for the parent's handling of children.
	 */
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oset);
	STAILQ_FOREACH(consinfo, &consinfos, link)
		start_console(consinfo, newargv, primary_secondary,
		    &start_barrier, &oset);
	sigprocmask(SIG_SETMASK, &oset, NULL);

	/* Now ready for children to start */
	pipe_barrier_ready(&start_barrier);
}

static int
wait_consoles(void)
{
	pipe_barrier_wait(&wait_first_child_barrier);

	/*
	 * Once one of our children has exited, kill off the rest and wait for
	 * them all to exit. This will also set the foreground process group of
	 * the controlling terminal back to ours if it's one of the consoles.
	 */
	kill_wait_all_consoles(SIGTERM);

	if (first_sigchld_consinfo == NULL)
		errx(EX_SOFTWARE, "failed to find first child that exited");

	return (first_sigchld_consinfo->exitstatus);
}

static void __dead2
usage(void)
{
	fprintf(stderr, "usage: %s utility [argument ...]", getprogname());
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	int ch, status;

	while ((ch = getopt_long(argc, argv, "+h", longopts, NULL)) != -1) {
		switch (ch) {
		case 'h':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	/*
	 * Gather the list of enabled consoles from /etc/ttys, ignoring VTYs
	 * other than ttyv0 since they're used for other purposes when the
	 * installer is running, and there would be no point having multiple
	 * copies on each of the multiplexed virtual consoles anyway.
	 */
	read_consoles();

	/*
	 * Start the installer on all the consoles. Do not print after this
	 * point until our process group is in the foreground again unless
	 * necessary (we ignore SIGTTOU so we can print errors, but don't want
	 * to garble a child's output).
	 */
	start_consoles(argc, argv);

	/*
	 * Wait for one of the installers to exit, kill the rest, become the
	 * foreground process group again and get the exit code of the first
	 * child to exit.
	 */
	status = wait_consoles();

	/*
	 * Reproduce the exit code of the first child to exit, including
	 * whether it was a fatal signal or normal termination.
	 */
	if (WIFSIGNALED(status))
		reproduce_signal_death(WTERMSIG(status));

	if (WIFEXITED(status))
		return (WEXITSTATUS(status));

	return (EXIT_FAILURE);
}
