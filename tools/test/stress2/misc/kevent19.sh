#!/bin/sh

# A kqueuex(KQUEUE_CPONFORK) test scenario

set -u
prog=$(basename "$0" .sh)

cat > /tmp/$prog.c <<EOF
/* \$Id: kqfork.c,v 1.4 2025/08/19 19:42:16 kostik Exp kostik $ */

#include <sys/param.h>
#include <sys/event.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef KQUEUE_CPONFORK
#define	KQUEUE_CPONFORK	0x2
#endif

static pid_t pid_pipe_beat;
static pid_t pid_controller;

static void
sighup_handler(int sig __unused)
{
	kill(pid_pipe_beat, SIGKILL);
	_exit(1);
}

static void
pipe_beat(int wp)
{
	static const char a[1] = { 'a' };
	ssize_t s;

	for (;;) {
		sleep(1);
		s = write(wp, a, 1);
		if (s < 0)
			err(1, "pipe write");
		if (s == 0)
			errx(1, "short pipe write");
	}
}

static void
worker(int kq, int rp)
{
	struct kevent ev[1];
	char a[1];
	ssize_t s;
	int n;

	for (;;) {
		n = kevent(kq, NULL, 0, ev, nitems(ev), NULL);
		if (n == -1) {
			kill(pid_controller, SIGHUP);
			err(1, "kqueue");
		}
		if (n == 0)
			continue; // XXXKIB
		switch (ev[0].filter) {
		case EVFILT_TIMER:
			printf("tick\n");
			break;
		case EVFILT_READ:
			if (ev[0].ident != (uintptr_t)rp) {
				kill(pid_controller, SIGHUP);
				errx(1, "unknown read ident %d\n", (int)ev[0].ident);
			}
			s = read(rp, a, sizeof(a));
			if (s == -1) {
				kill(pid_controller, SIGHUP);
				err(1, "read");
			}
			if (s == 0) {
				kill(pid_controller, SIGHUP);
				errx(1, "EOF");
			}
			printf("%c\n", a[0]);
			break;
		default:
			kill(pid_controller, SIGHUP);
			errx(1, "unknown fiter %d\n", ev[0].filter);
			break;
		}
	}
}

static void
usage(void)
{
	fprintf(stderr, "Usage: kqfork [fork]\n");
	exit(2);
}

int
main(int argc, char *argv[])
{
	struct kevent ev[2];
	struct sigaction sa;
	int kq, n, pp[2];
	pid_t pid_worker;
	bool do_fork;

	do_fork = false;
	if (argc != 1 && argc != 2)
		usage();
	if (argc == 2) {
		if (strcmp(argv[1], "fork") != 0)
			usage();
		do_fork = true;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sighup_handler;
	if (sigaction(SIGHUP, &sa, NULL) == -1)
		err(1, "sigaction(SIGHUP)");

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP;
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
		err(1, "sigaction(SIGCHLD)");

	if (pipe(pp) == -1)
		err(1, "pipe");

	pid_pipe_beat = fork();
	if (pid_pipe_beat == -1)
		err(1, "fork");
	if (pid_pipe_beat == 0) {
		close(pp[0]);
		pipe_beat(pp[1]);
	}

	kq = kqueuex(do_fork ? KQUEUE_CPONFORK : 0);
	if (kq == -1)
		err(1, "kqueuex");

	EV_SET(&ev[0], 1, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, 1, NULL);
	EV_SET(&ev[1], pp[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
	n = kevent(kq, ev, nitems(ev), NULL, 0, NULL);
	if (n == -1) {
		kill(pid_pipe_beat, SIGKILL);
		err(1, "kevent reg");
	}
	if (n != 0) {
		kill(pid_pipe_beat, SIGKILL);
		errx(1, "kevent reg %d", n);
	}

	pid_controller = getpid();

	if (do_fork) {
		pid_worker = fork();
		if (pid_worker == -1) {
			kill(pid_pipe_beat, SIGKILL);
			err(1, "fork");
		}
		if (pid_worker == 0) {
			close(pp[1]);
			worker(kq, pp[0]);
		}

		for (;;)
			pause();
	} else {
		worker(kq, pp[0]);
	}
	exit(0); // unreachable
}
EOF
cc -o /tmp/$prog -Wall -Wextra -O2 /tmp/$prog.c || exit 1

echo "--> No fork"
timeout 4s /tmp/$prog
echo "--> fork"
timeout 4s /tmp/$prog fork

rm -f /tmp/$prog.c $prog
exit 0
