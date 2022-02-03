#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# fcntl(2) locking scenario. No problems seen.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > fcntl.c
mycc -o fcntl -Wall -Wextra -O0 -g fcntl.c || exit 1
rm -f fcntl.c

mkdir -p $RUNDIR
cd $RUNDIR
/tmp/fcntl
status=$?

rm -f /tmp/fcntl
exit $status
EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PARALLEL 16
#define N 4096

static volatile sig_atomic_t completed;
const char name[] = "work";
int fd;

static void
ahandler(int s __unused)
{
	unlink(name);
	_exit(1);
}

static void
handler(int s __unused)
{
	completed++;
}

void
add(int n, int increment)
{
        struct flock fl;
	off_t pos;
	long val, oval __unused;
	int r;

	pos = n * sizeof(val);
	memset(&fl, 0, sizeof(fl));
        fl.l_start = pos;
        fl.l_len = sizeof(val);
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;

	while (fcntl(fd, F_SETLK, &fl) < 0) {
		if (errno != EAGAIN)
			err(1, "F_SETLK (child)");
		usleep(100);
	}

	if (lseek(fd, pos, SEEK_SET) == -1)
		err(1, "lseek");
	oval = 999999;
	while ((r = read(fd, &val, sizeof(val)) != sizeof(val))) {
		if (r == -1 && errno != EAGAIN)
			err(1, "read");
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek");
	}
	oval = val;
	val = val + increment;
#if defined(DEBUG)
	fprintf(stderr, "add(%d, %d) @ pos %ld: %ld = %ld + %d\n",
	    n, increment, (long)pos, val, oval, increment);
#endif
	if (lseek(fd, pos, SEEK_SET) == -1)
		err(1, "lseek");
	while ((r = write(fd, &val, sizeof(val)) != sizeof(val))) {
		if (r == -1 && errno != EAGAIN)
			err(1, "write");
		if (lseek(fd, pos, SEEK_SET) == -1)
			err(1, "lseek");
	}

        fl.l_type = F_UNLCK;
        if (fcntl(fd, F_SETLK, &fl) < 0)
                err(1, "F_UNLCK");

}

void
up(void)
{
	int flags, i;

	/* Need to re-open after a fork() */
	close(fd);
	if ((fd = open(name, O_RDWR)) == -1)
		err(1, "open(%s)", name);
        if ((flags = fcntl(fd, F_GETFL)) == -1)
		err(1, "fcntl(%d, T_GETFL)", fd);
        flags |= O_NONBLOCK;
        if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		err(1, "fcntl(%d, T_SETFL, %d)", fd, flags);

	for (i = 0; i < N; i++)
		add(i, 1);

	kill(getppid(), SIGHUP);
	while (access("work", R_OK) == 0)
		usleep(100);

	_exit(0);
}

void
down(void)
{
	int flags, i;

	close(fd);
	if ((fd = open(name, O_RDWR)) == -1)
		err(1, "open(%s)", name);
        if ((flags = fcntl(fd, F_GETFL)) == -1)
		err(1, "fcntl(%d, T_GETFL)", fd);
        flags |= O_NONBLOCK;
        if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		err(1, "fcntl(%d, T_SETFL, %d)", fd, flags);

	for (i = 0; i < N; i++)
		add(i, -1);

	kill(getppid(), SIGHUP);
	while (access("work", R_OK) == 0)
		usleep(100);

	_exit(0);
}

int
main(void)
{
	int flags, i;
	long val, sum;
	off_t len;

	signal(SIGHUP, handler);
	signal(SIGALRM, ahandler);
	alarm(300);
	if ((fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", name);
	len = N * sizeof(val);
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");

        if ((flags = fcntl(fd, F_GETFL)) == -1)
		err(1, "fcntl(%d, T_GETFL)", fd);
        flags |= O_NONBLOCK;
        if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		err(1, "fcntl(%d, T_SETFL, %d)", fd, flags);

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			up();
	}
	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			down();
	}

	while (completed != PARALLEL * 2)
		usleep(200);

	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek");
	sum = 0;
	for (i = 0; i < N; i++) {
		if (read(fd, &val, sizeof(val)) != sizeof(val))
			err(1, "Final read");
		if (val != 0)
			fprintf(stderr, "index %d: %ld\n", i, val);
		sum += val;
	}
	if (sum != 0)
		fprintf(stderr, "FAIL\n");
	unlink(name);

	for (i = 0; i < PARALLEL; i++) {
		wait(NULL);
		wait(NULL);
	}

	close(fd);

	return (sum != 0);
}
