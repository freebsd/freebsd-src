#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Test of pipe_poll()
# https://reviews.freebsd.org/D21333

# No problems seen.

# markj@ write:
# A simplified reproducible might be tricky to come up with.  I think this
# would do it:
#
# - Thread W writes 8KB (PIPE_MINDIRECT) of data to a pipe at a time.
# - Thread P poll()s the pipe for POLLIN.
# - Thread R reads 8KB of data from the pipe at a time.
#
# Thread P uses non-blocking poll() (timeout == 0).  When thread P does
# not see POLLIN, it signals the reader and the writer and continues
# polling in a loop.  When thread P sees POLLIN it signals the reader and
# sleeps until the reader returns and wakes it up.  After threads R and W
# finish their respective system calls, they always wait for another
# signal from P before doing anything.
#
# Basically, if all three threads are executing their respective system
# calls, and the reader has drained the writer's data and awoken the
# writer, there is a window where poll() will return POLLIN even though
# all data has been read.  If the reader then attempts to read() from the
# pipe again, it will block and the application appears to be hung.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
rm -f $dir/poll2.c || exit 1
sed '1,/^EOF/d' < $odir/$0 > $dir/poll2.c
mycc -o poll2 -Wall -Wextra -O0 -g poll2.c -lpthread || exit 1

cpuset -l 0 $dir/poll2
s=$?
pkill swap
wait

rm -rf poll2 poll2.c poll2.core
exit $s

EOF
#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile int done, frd, fwr, fpl;
static int fds[2];
static char b1[8192], b2[8192];

#define RUNTIME (2 * 60)
#define LOOP 400000

static void *
wr(void *data __unused)
{
	int i;

	for (i = 0; i < LOOP; i++) {
		pthread_set_name_np(pthread_self(), "wr-idle");
		while (fwr == 0)
			usleep(5);
		pthread_set_name_np(pthread_self(), "wr-act");
		fpl = 1;
		if (write(fds[1], b1, sizeof(b1)) != sizeof(b1))
			err(1, "write");
		fpl = 1;
		fwr = 0;
	}

	return (NULL);
}

static void *
rd(void *data __unused)
{
	int i;

	for (i = 0; i < LOOP; i++) {
		fpl = 1;
		pthread_set_name_np(pthread_self(), "rd-idle");
		while (frd == 0)
			usleep(5);
		pthread_set_name_np(pthread_self(), "rd-act");
		if (read(fds[0], b2, sizeof(b2)) != sizeof(b2))
			err(1, "read");
		frd = 0;
		fpl = 1;
	}
	done = 1;

	return (NULL);
}

static void *
pl(void *data __unused)
{
	struct pollfd pfd;
	int r;

	pfd.fd = fds[0];
	pfd.events = POLLIN;
	while (done == 0) {
		pfd.fd = fds[0];
		pfd.events = POLLIN;
		pthread_set_name_np(pthread_self(), "pl-idle");
		pthread_set_name_np(pthread_self(), "pl-act");
		while (fpl == 0)
			usleep(5);
again:
		if ((r = poll(&pfd, 1, 0)) == -1)
			err(1, "poll");
		if (done == 1)
			return (NULL);
		if (r == 0) {
			frd = fwr = 1;
			goto again;
		} else {
			fpl = 0;
			frd = fwr = 1;
		}
	}

	return (NULL);
}

void
test(void)
{
	pthread_t tid[3];
	int rc;

	if (pipe(fds) == -1)
		err(1, "pipe");
	done = 0;
	fpl = 0;
	frd = 0;
	fwr = 0;
	if ((rc = pthread_create(&tid[0], NULL, rd, NULL)) != 0)
		errc(1, rc, "pthread_create");
	if ((rc = pthread_create(&tid[1], NULL, wr, NULL)) != 0)
		errc(1, rc, "pthread_create");
	if ((rc = pthread_create(&tid[2], NULL, pl, NULL)) != 0)
		errc(1, rc, "pthread_create");

	frd = 1;
	fwr = 1;

	if ((rc = pthread_join(tid[0], NULL)) != 0)
		errc(1, rc, "pthread_join");
	if ((rc = pthread_join(tid[1], NULL)) != 0)
		errc(1, rc, "pthread_join");
	if ((rc = pthread_join(tid[2], NULL)) != 0)
		errc(1, rc, "pthread_join");

	close(fds[0]);
	close(fds[1]);
}

int
main(void)
{
	time_t start;

	alarm(600);
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		test();
	}

	return (0);
}
