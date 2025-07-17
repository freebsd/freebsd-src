#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# killpg(2) version of reaper.sh. No problems seen.

. ../default.cfg

prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>


#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static _Atomic(int) *share;

#define GID 0
#define PARALLEL 10
#define RDY 1
#define MAXP 7000

static void
hand(int i __unused) {
	_exit(0);
}

static void
innerloop(int parallel)
{
	pid_t pids[MAXP];
	struct sigaction sa;
	int i;

	usleep(1000);
	for (i = 0; i < parallel; i++) {
		if ((pids[i] = fork()) == 0) {
			sa.sa_handler = hand;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			if (sigaction(SIGUSR1, &sa, NULL) == -1)
				err(1, "sigaction");
			atomic_fetch_add(&share[RDY], 1);
			setproctitle("child");
			for (;;)
				pause();
			_exit(0); /* never reached */
		}
		if (pids[i] == -1)
			err(1, "fork()");
	}
	for (i = 0; i < parallel; i++) {
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waitpid(%d) in looper", pids[i]);
	}
	_exit(0);
}

static void
looper(void)
{
	struct sigaction sa;
	struct passwd *pw;
	pid_t pids[MAXP];
	int i, parallel;

	setproctitle("looper");
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGUSR1, &sa, NULL) == -1)
		err(1, "sigaction");

	if ((pw = getpwnam("TUSER")) == NULL)
		err(1, "no such user: TUSER");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"TUSER\"");
	endpwent();
	setpgrp(0, 0);
	share[GID] = getpgrp();
	parallel = arc4random() % MAXP + 1;
	parallel = parallel / PARALLEL * PARALLEL;
	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			innerloop(parallel / PARALLEL);
	}
	while (atomic_load(&share[RDY]) != parallel)
		usleep(10000);
	if (killpg(share[GID], SIGUSR1) == -1)
		err(1, "pgkill(%d)", share[GID]);
	for (i = 0; i < 4; i++) {
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waitpid(%d) in looper", pids[i]);
	}
	_exit(0);
}

int
main(void)
{
	size_t len;
	time_t start;
	int lpid, s1;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while (time(NULL) - start < 120) {
		share[GID] = 0;
		share[RDY] = 0;
		if ((lpid = fork()) == 0)
			looper();
		if (waitpid(lpid, &s1, 0) != lpid)
			err(1, "waitpid looper");
	}

	return (0);
}
EOF
sed -i '' "s#TUSER#$testuser#" /tmp/$prog.c
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1
rm /tmp/$prog.c

export MAXSWAPPCT=70
n=1
start=`date +%s`
while true; do
	../testcases/swap/swap -t 2m -i 20 > /dev/null &
	/tmp/$prog & pid=$!
	st=`date +%s`
	while kill -0 $pid > /dev/null 2>&1; do
		e=$((`date +%s` - st))
		if [ $e -ge 120 ]; then
			while pgrep -q swap; do pkill swap; done
		fi
		if [ $e -ge 600 ]; then
			echo "Failed in loop #$n after $e seconds."
			ps -jU$testuser | head -20
			kill $pid
			pkill -U$testuser
			wait
			rm -f /tmp/$prog
			exit 1
		fi
	done
	wait
	[ $((`date +%s` - start)) -ge 300 ] && break
	n=$((n + 1))
done
rm /tmp/$prog
exit 0
