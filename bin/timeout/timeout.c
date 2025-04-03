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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EXIT_TIMEOUT	124
#define EXIT_INVALID	125
#define EXIT_CMD_ERROR	126
#define EXIT_CMD_NOENT	127

static volatile sig_atomic_t sig_chld = 0;
static volatile sig_atomic_t sig_term = 0;
static volatile sig_atomic_t sig_alrm = 0;
static const char *command = NULL;
static bool verbose = false;

static void __dead2
usage(void)
{
	fprintf(stderr,
		"Usage: %s [-f | --foreground] [-k time | --kill-after time]"
		" [-p | --preserve-status] [-s signal | --signal signal] "
		" [-v | --verbose] <duration> <command> [arg ...]\n",
		getprogname());
	exit(EXIT_FAILURE);
}

static void
logv(const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
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
	switch (signo) {
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
send_sig(pid_t pid, int signo, bool foreground)
{
	struct procctl_reaper_kill rk;

	logv("sending signal %s(%d) to command '%s'",
	     sys_signame[signo], signo, command);
	if (foreground) {
		if (kill(pid, signo) == -1)
			warnx("kill(%d, %s)", (int)pid, sys_signame[signo]);
	} else {
		memset(&rk, 0, sizeof(rk));
		rk.rk_sig = signo;
		if (procctl(P_PID, getpid(), PROC_REAP_KILL, &rk) == -1)
			warnx("procctl(PROC_REAP_KILL)");
		else if (rk.rk_fpid > 0)
			warnx("failed to signal some processes: first pid=%d",
			      (int)rk.rk_fpid);
		logv("signaled %u processes", rk.rk_killed);
	}

	/*
	 * If the child process was stopped by a signal, POSIX.1-2024
	 * requires to send a SIGCONT signal.  However, the standard also
	 * allows to send a SIGCONT regardless of the stop state, as we
	 * are doing here.
	 */
	if (signo != SIGKILL && signo != SIGSTOP && signo != SIGCONT) {
		logv("sending signal %s(%d) to command '%s'",
		     sys_signame[SIGCONT], SIGCONT, command);
		if (foreground) {
			kill(pid, SIGCONT);
		} else {
			memset(&rk, 0, sizeof(rk));
			rk.rk_sig = SIGCONT;
			procctl(P_PID, getpid(), PROC_REAP_KILL, &rk);
		}
	}
}

static void
set_interval(double iv)
{
	struct itimerval tim;

	memset(&tim, 0, sizeof(tim));
	if (iv > 0) {
		tim.it_value.tv_sec = (time_t)iv;
		iv -= (double)(time_t)iv;
		tim.it_value.tv_usec = (suseconds_t)(iv * 1000000UL);
	}

	if (setitimer(ITIMER_REAL, &tim, NULL) == -1)
		err(EXIT_FAILURE, "setitimer()");
}

int
main(int argc, char **argv)
{
	int ch, status, sig;
	int pstat = 0;
	int killsig = SIGTERM;
	size_t i;
	pid_t pid, cpid;
	double first_kill;
	double second_kill = 0;
	bool foreground = false;
	bool preserve = false;
	bool timedout = false;
	bool do_second_kill = false;
	bool child_done = false;
	sigset_t zeromask, allmask, oldmask;
	struct sigaction signals;
	struct procctl_reaper_status info;
	int signums[] = {
		-1,
		SIGTERM,
		SIGINT,
		SIGHUP,
		SIGCHLD,
		SIGALRM,
		SIGQUIT,
	};

	const char optstr[] = "+fhk:ps:v";
	const struct option longopts[] = {
		{ "foreground",      no_argument,       NULL, 'f' },
		{ "help",            no_argument,       NULL, 'h' },
		{ "kill-after",      required_argument, NULL, 'k' },
		{ "preserve-status", no_argument,       NULL, 'p' },
		{ "signal",          required_argument, NULL, 's' },
		{ "verbose",         no_argument,       NULL, 'v' },
		{ NULL,              0,                 NULL,  0  },
	};

	while ((ch = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
		switch (ch) {
		case 'f':
			foreground = true;
			break;
		case 'k':
			do_second_kill = true;
			second_kill = parse_duration(optarg);
			break;
		case 'p':
			preserve = true;
			break;
		case 's':
			killsig = parse_signal(optarg);
			break;
		case 'v':
			verbose = true;
			break;
		case 0:
			break;
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
			err(EXIT_FAILURE, "procctl(PROC_REAP_ACQUIRE)");
	}

	/* Block all signals to avoid racing against the child. */
	sigfillset(&allmask);
	if (sigprocmask(SIG_BLOCK, &allmask, &oldmask) == -1)
		err(EXIT_FAILURE, "sigprocmask()");

	pid = fork();
	if (pid == -1) {
		err(EXIT_FAILURE, "fork()");
	} else if (pid == 0) {
		/*
		 * child process
		 *
		 * POSIX.1-2024 requires that the child process inherit the
		 * same signal dispositions as the timeout(1) utility
		 * inherited, except for the signal to be sent upon timeout.
		 */
		signal(killsig, SIG_DFL);
		if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1)
			err(EXIT_FAILURE, "sigprocmask(oldmask)");

		execvp(argv[0], argv);
		warn("exec(%s)", argv[0]);
		_exit(errno == ENOENT ? EXIT_CMD_NOENT : EXIT_CMD_ERROR);
	}

	/* parent continues here */

	memset(&signals, 0, sizeof(signals));
	sigemptyset(&signals.sa_mask);

	if (killsig != SIGKILL && killsig != SIGSTOP)
		signums[0] = killsig;

	for (i = 0; i < sizeof(signums) / sizeof(signums[0]); i++)
		sigaddset(&signals.sa_mask, signums[i]);

	signals.sa_handler = sig_handler;
	signals.sa_flags = SA_RESTART;

	for (i = 0; i < sizeof(signums) / sizeof(signums[0]); i++) {
		if (signums[i] > 0 &&
		    sigaction(signums[i], &signals, NULL) == -1)
			err(EXIT_FAILURE, "sigaction()");
	}

	/* Don't stop if background child needs TTY */
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	set_interval(first_kill);
	sigemptyset(&zeromask);

	for (;;) {
		sigsuspend(&zeromask);

		if (sig_chld) {
			sig_chld = 0;

			while ((cpid = waitpid(-1, &status, WNOHANG)) != 0) {
				if (cpid < 0) {
					if (errno != EINTR)
						break;
				} else if (cpid == pid) {
					pstat = status;
					child_done = true;
					logv("child terminated: pid=%d, "
					     "exit=%d, signal=%d",
					     (int)pid, WEXITSTATUS(status),
					     WTERMSIG(status));
				} else {
					/*
					 * Collect grandchildren zombies.
					 * Only effective if we're a reaper.
					 */
					logv("collected zombie: pid=%d, "
					     "exit=%d, signal=%d",
					     (int)cpid, WEXITSTATUS(status),
					     WTERMSIG(status));
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
		} else if (sig_alrm || sig_term) {
			if (sig_alrm) {
				sig = killsig;
				sig_alrm = 0;
				timedout = true;
				logv("time limit reached or received SIGALRM");
			} else {
				sig = sig_term;
				sig_term = 0;
				logv("received terminating signal %s(%d)",
				     sys_signame[sig], sig);
			}

			send_sig(pid, sig, foreground);

			if (do_second_kill) {
				set_interval(second_kill);
				do_second_kill = false;
				killsig = SIGKILL;
			}
		}
	}

	if (!foreground)
		procctl(P_PID, getpid(), PROC_REAP_RELEASE, NULL);

	if (timedout && !preserve) {
		pstat = EXIT_TIMEOUT;
	} else {
		if (WIFEXITED(pstat))
			pstat = WEXITSTATUS(pstat);
		else if (WIFSIGNALED(pstat))
			pstat = 128 + WTERMSIG(pstat);
	}

	return (pstat);
}
