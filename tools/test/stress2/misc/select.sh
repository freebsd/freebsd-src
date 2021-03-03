#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# The combination of ualarm() firing before and after the select(2) timeout
# triggers select() to return EINTR a number of times.
# Problem only seen on i386.

# Test scenario suggestion by kib@

# "FAIL n = 2389" seen on r302369, no debug build.
# Fixed by: r302573.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/select.c
mycc -o select -Wall -Wextra -O0 -g select.c -lpthread || exit 1
rm -f select.c
cd $odir

/tmp/select
s=$?

rm -f /tmp/select
exit $s
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_barrier_t barr;
static sig_atomic_t alarms;
static int lines;

#define LINES 128000
#define N 2000 /* also seen fail with N = 20.000 */
#define PARALLEL 16 /* Fails seen with 1 - 16 */
#define RUNTIME (10 * 60)

static void
handler(int i __unused) {
	alarms++;
}

static void
test(void)
{
	struct timeval tv;
	int i, n, r, s;

	r = pthread_barrier_wait(&barr);
	if (r != 0 && r != PTHREAD_BARRIER_SERIAL_THREAD)
	    errc(1, r, "pthread_barrier_wait");

	signal(SIGALRM, handler);
	s = 0;
	for (i = 0; i < lines; i++) {
		alarms = 0;
		if (arc4random() % 100 < 50)
			ualarm(N / 2, 0);
		else
			ualarm(N * 2, 0);
		tv.tv_sec  = 0;
		tv.tv_usec = N;
		n = 0;
		do {
			r = select(1, NULL, NULL, NULL, &tv);
			n++;
		} while (r == -1 && errno == EINTR);
		if (r == -1)
			err(1, "select");
		ualarm(0, 0);
		if (n > 2) {
			fprintf(stderr, "FAIL n = %d, tv = %ld.%06ld\n",
			    n, (long)tv.tv_sec, tv.tv_usec);
			s = 1;
			break;
		}
		if (alarms >  1) {
			fprintf(stderr, "FAIL alarms = %d\n", (int)alarms);
			s = 2;
			break;
		}

	}

	exit(s);
}

int
main(void)
{
	pthread_barrierattr_t attr;
	time_t start;
	int e, i, j, pids[PARALLEL], r, status;

	lines = LINES / PARALLEL;
	if (lines == 0)
		lines = 1;
	e = 0;
	if ((r = pthread_barrierattr_init(&attr)) != 0)
		errc(1, r, "pthread_barrierattr_init");
	if ((r = pthread_barrierattr_setpshared(&attr,
	    PTHREAD_PROCESS_SHARED)) != 0)
		errc(1, r, "pthread_barrierattr_setpshared");
	if ((r = pthread_barrier_init(&barr, &attr, PARALLEL)) != 0)
		errc(1, r, "pthread_barrier_init");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		for (i = 0; i < PARALLEL; i++) {
			waitpid(pids[i], &status, 0);
			e += status == 0 ? 0 : 1;
			if (status != 0) {
				for (j = i + 1; j < PARALLEL; j++)
					kill(pids[j], SIGINT);
			}
		}
	}

	if ((r = pthread_barrier_destroy(&barr)) > 0)
		errc(1, r, "pthread_barrier_destroy");

	return (e);
}
