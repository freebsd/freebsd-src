#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Test scenario idea by kib@

# "panic: double fault" seen due to recursion

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q procfs || mount -t procfs procfs /proc
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > procfs4.c
mycc -o procfs4 -Wall -Wextra -O2 procfs4.c || exit 1
rm -f procfs4.c
cd $here

su $testuser -c /tmp/procfs4
e=$?

rm -f /tmp/procfs4
exit $e
EOF
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOOPS 1000
#define MAXRUN 1200
#define PARALLEL 10

static int debug; /* Set to 1 for debug output */
char *files[] = {
	"cmdline",
	"ctl",
	"dbregs",
	"etype",
	"file",
	"fpregs",
	"map",
	"mem",
	"note",
	"notepg",
	"osrel",
	"regs",
	"rlimit",
	"status"
};

void
test(void)
{
	pid_t p;
	int fd, i, j, n, opens;
	char path[128];

	for (i = 0; i < 64; i++) {
		if ((p = fork()) == 0) {
			setproctitle("Sleeper");
			usleep(20000);
			usleep(arc4random() % 200);
			for (j = 0; j < 10000; j++)
				getpid();
			_exit(0);
		}
		opens = 0;
		setproctitle("load");
		for (j = 0; j < 14; j++) {
			snprintf(path, sizeof(path), "/proc/%d/%s", p, files[j]);
			if ((fd = open(path, O_RDWR)) == -1)
				if ((fd = open(path, O_RDONLY)) == -1)
					continue;

			ioctl(fd, FIONREAD, &n);
			if (ioctl(fd, FIONBIO, &n) != -1)
				opens++;

			close(fd);
		}
		kill(p, SIGHUP);
		if (debug != 0 && opens == 0)
			fprintf(stderr, "No ioctl() calls succeeded.\n");
	}

	for (i = 0; i < 64; i++)
		wait(NULL);

	_exit(0);
}

int
main(void)
{
	time_t start;
	int e, i, j;

	e = 0;
	start = time(NULL);
	for (i = 0; i < LOOPS; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0)
				test();
		}

		for (j = 0; j < PARALLEL; j++)
			wait(NULL);
		usleep(10000);
		if (time(NULL) - start > MAXRUN) {
			fprintf(stderr, "FAIL Timeout\n");
			e = 1;
			break;
		}
	}

	return (e);
}
