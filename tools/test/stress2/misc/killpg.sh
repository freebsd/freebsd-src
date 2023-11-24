#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
# All rights reserved.
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

# Regression test for r241859:
# Return EPERM if processes were found but they were unable to be signaled.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > killpg.c
mycc -o killpg -Wall -Wextra killpg.c || exit 1
rm -f killpg.c

/tmp/killpg

rm -f /tmp/killpg
exit 0
EOF
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pid_t gid, pid;

#define LOOPS 100

static void
hand(int i __unused) {	/* handler */
	_exit(0);
}

static void
nlooper(void)
{
	int i;

	setproctitle("nlooper");
	setpgrp(0, getpid());
	for (i = 0; i < LOOPS; i++) {
		if ((pid = fork()) == 0) {
			signal(SIGINFO, hand);
			if (arc4random() % 100 < 10)
				usleep(arc4random() % 100);
			_exit(0);
		}
		wait(NULL);
	}
	_exit(0);
}

static void
looper(void)
{
	int i;

	setproctitle("looper");
	setpgrp(0, getpid());
	for (i = 0; i < LOOPS; i++) {
		if ((pid = fork()) == 0) {
			signal(SIGINFO, hand);
			if (arc4random() % 100 < 10)
				usleep(arc4random() % 100);
			_exit(0);
		}
		wait(NULL);
	}
	unlink("cont");
	_exit(0);
}

static void
killer(void)
{
	struct passwd *pw;

	setproctitle("killer");
	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "no such user: nobody");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	if ((gid = fork()) == 0)
		nlooper();	/* nobody looper */

	sleep(1);
	while (access("cont", R_OK) == 0) {
		if (killpg(gid, SIGINFO) == -1) {
			if (errno == EPERM)
				continue;
			warn("FAIL killpg");
		}
	}
	_exit(0);
}

int
main(void)
{
	int fd;

	if ((fd = open("cont", O_RDWR | O_CREAT, 0666)) == -1)
		err(1, "creat");
	close(fd);
	signal(SIGINFO, SIG_IGN);

	if ((gid = fork()) == 0)
		looper();
	if (fork() == 0)
		killer();

	wait(NULL);
	wait(NULL);

	return (0);
}
