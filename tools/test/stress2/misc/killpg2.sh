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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;

#define CONT 0
#define GID 1
#define SYNC 2
#define MAXP 10000

static void
hand(int i __unused) {
	_exit(0);
}

static void
looper(void)
{
	struct sigaction sa;
	time_t start;
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
	share[SYNC] = 1;
	start = time(NULL);
	while (time(NULL) - start < 120) {
		parallel = arc4random() % MAXP + 1;
		for (i = 0; i < parallel; i++) {
			if ((pids[i] = fork()) == 0) {
				sa.sa_handler = hand;
				sigemptyset(&sa.sa_mask);
				sa.sa_flags = 0;
				if (sigaction(SIGUSR1, &sa, NULL) == -1)
					err(1, "sigaction");
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
	}
	_exit(0);
}

static void
killer(void)
{
	int e, gid;

	while (share[SYNC] == 0)
		;
	gid = share[GID];
	while (share[CONT] == 1) {
		usleep(arc4random() % 50000);
		gid = share[GID];
		if (gid != 0) {
			e = 0;
			while (e == 0) {
				if (killpg(gid, SIGUSR1) == -1) {
					e = 1;
					if (errno != ESRCH)
						err(1, "pgkill(%d)", gid);
				}
				usleep(5000 + arc4random() % 5000);
			}
		}
	}
	_exit(0);
}

int
main(void)
{
	size_t len;
	time_t start;
	int lpid, kpid, s1, s2;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while (time(NULL) - start < 120) {
		share[CONT] = 1;
		share[GID] = 0;
		share[SYNC] = 0;
		if ((lpid = fork()) == 0)
			looper();
		usleep(arc4random() % 100000);
		if ((kpid = fork()) == 0)
			killer();

		if (waitpid(lpid, &s1, 0) != lpid)
			err(1, "waitpid looper");
		usleep(arc4random() % 1000);
		share[CONT] = 0;
		waitpid(kpid, &s2, 0);
	}

	return (0);
}
EOF
sed -i '' "s#TUSER#$testuser#" /tmp/$prog.c
cc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c || exit 1
rm /tmp/$prog.c

n=1
start=`date +%s`
while true; do
	/tmp/$prog
	for i in `jot 50`; do
		pgrep -q $prog || break
		sleep .5
	done
	if pgrep -q $prog; then
		e=$((`date +%s` - start))
		echo "Failed in loop #$n after $e seconds."
		pgrep "$prog|timeout" | xargs ps -jp
		pkill $prog
		rm -f /tmp/$prog
		exit 1
	fi
	[ $((`date +%s` - start)) -ge 600 ] && break
	n=$((n + 1))
done
rm /tmp/$prog
exit 0
