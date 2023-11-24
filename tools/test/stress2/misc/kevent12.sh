#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Bug 228858 - panic when delivering knote to a process who has opened a
# kqueue() is dying
# Page fault seen: https://people.freebsd.org/~pho/stress/log/mark052.txt
# Fixed by r340897.

# Test scenario based on analysis by siddharthtuli gmail com

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kevent12.c
mycc -o kevent12 -Wall -Wextra -O2 -g kevent12.c || exit 1
rm -f kevent12.c
cd $odir

(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -l 100 > /dev/null 2>&1) &
pid=$!
/tmp/kevent12
while pgrep -q swap; do
	pkill -9 swap
done
wait $pid

rm -f /tmp/kevent12
exit 0
EOF
#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 64
#define RUNTIME (5 * 60)

static pid_t parent;
static int kq;

static void
hand(int i __unused) {	/* handler */
	kill(parent, SIGINT);
	_exit(1);
}

static void
init_kq()
{
	kq = kqueue();
	if (kq == -1)
		err(1, "kqueue");
}

static void
add_watch(pid_t pid)
{
	struct kevent kev;
	char msg[64];

	bzero(&kev, sizeof(kev));
	kev.ident = pid;
	kev.flags = EV_ADD | EV_ENABLE;
	kev.filter = EVFILT_PROC;
	kev.fflags = NOTE_EXIT | NOTE_EXEC | NOTE_TRACK | NOTE_TRACKERR;

	for (;;) {
		int res = kevent(kq, &kev, 1, NULL, 0, NULL);
		if (res == -1) {
			if (errno == EINTR)
				continue;
			if (errno == ESRCH)
				break;

			snprintf(msg, sizeof(msg),
				 "kevent - add watch for pid %u", pid);
			err(1, "%s", msg);
		}
		else
			break;
	}
}

static void
polling()
{
	struct kevent kev[10];
#if defined(DEBUG)
	pid_t pid;
	int i;
#endif

	for (;;) {
		bzero(&kev, sizeof(kev));
		if (arc4random() % 100 < 10) {
			signal(SIGALRM, hand);
			ualarm(10000, 0);
		}
		int res = kevent(kq, NULL, 0, kev,
				 sizeof(kev) / sizeof(kev[0]), NULL);
		if (res == -1) {
			if (errno == EINTR)
				continue;

			if (errno == ESRCH)
				continue;

			err(1, "kevent");
		}

#if defined(DEBUG)
		for (i = 0; i < res; i++) {
			pid = kev[i].ident;
			if (kev[i].fflags & NOTE_CHILD) {
				add_watch(pid);
				printf("%u - new process, parent %u\n", pid,
				    (unsigned int)kev[i].data);
			}
			if (kev[i].fflags & NOTE_FORK) {
				printf("%u forked\n", pid);
			}
			if (kev[i].fflags & NOTE_EXEC) {
				printf("%u called exec\n", pid);
			}
			if (kev[i].fflags & NOTE_EXIT) {
				printf("%u exited\n", pid);
				if (parent == pid)
					return;
			}
			if (kev[i].fflags & NOTE_TRACK) {
				printf("%u forked - track\n", pid);
			}
			if (kev[i].fflags & NOTE_TRACKERR) {
				fprintf(stderr, "%u - track error\n", pid);
			}
		}
#endif
	}
}

void
churn(void)
{
	pid_t pid;
	time_t  start;
	char *cmdline[] = { "/usr/bin/true", NULL };

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if ((pid = fork()) == -1)
			err(1, "fork");
		if (pid == 0) {
			if (execve(cmdline[0], cmdline, NULL) == -1)
				err(1, "execve");
		}
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid(%d):%d", pid, __LINE__);
	}

	_exit(0);
}

int
test(void)
{
	if ((parent = fork()) == 0)
		churn();

	init_kq();
	add_watch(parent);
	polling();
	if (waitpid(parent, NULL, 0) != parent)
		err(1, "waitpid(%d):%d", parent, __LINE__);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int i;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		for (i = 0; i < PARALLEL; i++)
			if (waitpid(pids[i], NULL, 0) != pids[i])
				err(1, "waitpid(%d):%d", pids[i], __LINE__);
	}

	return (0);
}
