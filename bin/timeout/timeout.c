/*-
 * Copyright (c) 2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2014 Vsevolod Stakhov <vsevolod@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#include <sys/cdefs.h>
#include <sys/procctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EXIT_TIMEOUT 124
#define EXIT_INVALID 125
#define EXIT_CMD_ERROR 126
#define EXIT_CMD_NOENT 127

static volatile sig_atomic_t sig_chld = 0;
static volatile sig_atomic_t sig_term = 0;
static volatile sig_atomic_t sig_alrm = 0;
static volatile sig_atomic_t sig_ign = 0;
static const char *command = NULL;
static bool verbose = false;

static void
usage(void)
{

	fprintf(stderr, "Usage: %s [-k time | --kill-after time]"
		" [-s sig | --signal sig] [-v | --verbose] [--foreground]"
		" [--preserve-status] <duration> <command> <arg ...>\n",
		getprogname());

	exit(EXIT_FAILURE);
}

static double
parse_duration(const char *duration)
{
	double ret;
	char *suffix;

	ret = strtod(duration, &suffix);
	if (suffix == duration)
		errx(EXIT_INVALID, "duration is not a number");

	if (*suffix == '\0')
		return (ret);

	if (suffix[1] != '\0')
		errx(EXIT_INVALID, "duration unit suffix too long");

	switch (*suffix) {
	case 's':
		break;
	case 'm':
		ret *= 60;
		break;
	case 'h':
		ret *= 60 * 60;
		break;
	case 'd':
		ret *= 60 * 60 * 24;
		break;
	default:
		errx(EXIT_INVALID, "duration unit suffix invalid");
	}

	if (ret < 0 || ret >= 100000000UL)
		errx(EXIT_INVALID, "duration out of range");

	return (ret);
}

static int
parse_signal(const char *str)
{
	int sig, i;
	const char *errstr;

	sig = strtonum(str, 1, sys_nsig - 1, &errstr);

	if (errstr == NULL)
		return (sig);

	if (strncasecmp(str, "SIG", 3) == 0)
		str += 3;

	for (i = 1; i < sys_nsig; i++) {
		if (strcasecmp(str, sys_signame[i]) == 0)
			return (i);
	}

	errx(EXIT_INVALID, "invalid signal");
}

static void
sig_handler(int signo)
{
	if (sig_ign != 0 && signo == sig_ign) {
		sig_ign = 0;
		return;
	}

	switch (signo) {
	case 0:
	case SIGINT:
	case SIGHUP:
	case SIGQUIT:
	case SIGTERM:
		sig_term = signo;
		break;
	case SIGCHLD:
		sig_chld = 1;
		break;
	case SIGALRM:
		sig_alrm = 1;
		break;
	}
}

static void
send_sig(pid_t pid, int signo)
{
	if (verbose) {
		warnx("sending signal %s(%d) to command '%s'",
		sys_signame[signo], signo, command);
	}
	kill(pid, signo);
}

static void
set_interval(double iv)
{
	struct itimerval tim;

	memset(&tim, 0, sizeof(tim));
	tim.it_value.tv_sec = (time_t)iv;
	iv -= (double)tim.it_value.tv_sec;
	tim.it_value.tv_usec = (suseconds_t)(iv * 1000000UL);

	if (setitimer(ITIMER_REAL, &tim, NULL) == -1)
		err(EXIT_FAILURE, "setitimer()");
}

int
main(int argc, char **argv)
{
	int ch;
	int foreground, preserve;
	int pstat, status;
	int killsig = SIGTERM;
	size_t i;
	pid_t pid, cpid;
	double first_kill;
	double second_kill;
	bool timedout = false;
	bool do_second_kill = false;
	bool child_done = false;
	struct sigaction signals;
	struct procctl_reaper_status info;
	struct procctl_reaper_kill killemall;
	int signums[] = {
		-1,
		SIGTERM,
		SIGINT,
		SIGHUP,
		SIGCHLD,
		SIGALRM,
		SIGQUIT,
	};

	foreground = preserve = 0;
	second_kill = 0;

	const struct option longopts[] = {
		{ "preserve-status", no_argument,       &preserve,    1 },
		{ "foreground",      no_argument,       &foreground,  1 },
		{ "kill-after",      required_argument, NULL,        'k'},
		{ "signal",          required_argument, NULL,        's'},
		{ "help",            no_argument,       NULL,        'h'},
		{ "verbose",         no_argument,       NULL,        'v'},
		{ NULL,              0,                 NULL,         0 }
	};

	while ((ch = getopt_long(argc, argv, "+k:s:vh", longopts, NULL)) != -1) {
		switch (ch) {
			case 'k':
				do_second_kill = true;
				second_kill = parse_duration(optarg);
				break;
			case 's':
				killsig = parse_signal(optarg);
				break;
			case 'v':
				verbose = true;
				break;
			case 0:
				break;
			case 'h':
			default:
				usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	first_kill = parse_duration(argv[0]);
	argc--;
	argv++;
	command = argv[0];

	if (!foreground) {
		/* Acquire a reaper */
		if (procctl(P_PID, getpid(), PROC_REAP_ACQUIRE, NULL) == -1)
			err(EXIT_FAILURE, "Fail to acquire the reaper");
	}

	memset(&signals, 0, sizeof(signals));
	sigemptyset(&signals.sa_mask);

	if (killsig != SIGKILL && killsig != SIGSTOP)
		signums[0] = killsig;

	for (i = 0; i < sizeof(signums) / sizeof(signums[0]); i++)
		sigaddset(&signals.sa_mask, signums[i]);

	signals.sa_handler = sig_handler;
	signals.sa_flags = SA_RESTART;

	for (i = 0; i < sizeof(signums) / sizeof(signums[0]); i++) {
		if (signums[i] != -1 && signums[i] != 0 &&
		    sigaction(signums[i], &signals, NULL) == -1)
			err(EXIT_FAILURE, "sigaction()");
	}

	/* Don't stop if background child needs TTY */
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	pid = fork();
	if (pid == -1)
		err(EXIT_FAILURE, "fork()");
	else if (pid == 0) {
		/* child process */
		signal(SIGTTIN, SIG_DFL);
		signal(SIGTTOU, SIG_DFL);

		execvp(argv[0], argv);
		warn("exec(%s)", argv[0]);
		_exit(errno == ENOENT ? EXIT_CMD_NOENT : EXIT_CMD_ERROR);
	}

	if (sigprocmask(SIG_BLOCK, &signals.sa_mask, NULL) == -1)
		err(EXIT_FAILURE, "sigprocmask()");

	/* parent continues here */
	set_interval(first_kill);

	for (;;) {
		sigemptyset(&signals.sa_mask);
		sigsuspend(&signals.sa_mask);

		if (sig_chld) {
			sig_chld = 0;

			while ((cpid = waitpid(-1, &status, WNOHANG)) != 0) {
				if (cpid < 0) {
					if (errno == EINTR)
						continue;
					else
						break;
				} else if (cpid == pid) {
					pstat = status;
					child_done = true;
				}
			}
			if (child_done) {
				if (foreground) {
					break;
				} else {
					procctl(P_PID, getpid(),
					    	PROC_REAP_STATUS, &info);
					if (info.rs_children == 0)
						break;
				}
			}
		} else if (sig_alrm) {
			sig_alrm = 0;

			timedout = true;
			if (!foreground) {
				killemall.rk_sig = killsig;
				killemall.rk_flags = 0;
				procctl(P_PID, getpid(), PROC_REAP_KILL,
				    &killemall);
			} else
				send_sig(pid, killsig);

			if (do_second_kill) {
				set_interval(second_kill);
				do_second_kill = false;
				sig_ign = killsig;
				killsig = SIGKILL;
			} else
				break;

		} else if (sig_term) {
			if (!foreground) {
				killemall.rk_sig = sig_term;
				killemall.rk_flags = 0;
				procctl(P_PID, getpid(), PROC_REAP_KILL,
				    &killemall);
			} else
				send_sig(pid, sig_term);

			if (do_second_kill) {
				set_interval(second_kill);
				do_second_kill = false;
				sig_ign = killsig;
				killsig = SIGKILL;
			} else
				break;
		}
	}

	while (!child_done && wait(&pstat) == -1) {
		if (errno != EINTR)
			err(EXIT_FAILURE, "waitpid()");
	}

	if (!foreground)
		procctl(P_PID, getpid(), PROC_REAP_RELEASE, NULL);

	if (WEXITSTATUS(pstat))
		pstat = WEXITSTATUS(pstat);
	else if (WIFSIGNALED(pstat))
		pstat = 128 + WTERMSIG(pstat);

	if (timedout && !preserve)
		pstat = EXIT_TIMEOUT;

	return (pstat);
}
