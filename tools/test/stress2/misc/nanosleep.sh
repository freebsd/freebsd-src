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

# A simplistic regression test for r200510:

[ `sysctl -n kern.hz` -lt 1000 ] && exit 0

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > nanosleep.c
mycc -o nanosleep -Wall -Wextra nanosleep.c || exit 1
rm -f nanosleep.c

/tmp/nanosleep && s=0 || s=$?

rm -f /tmp/nanosleep
exit $s
EOF
#include <sys/limits.h>
#include <sys/time.h>

#include <err.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>

#define N 20000

int
main(void)
{
	struct timespec rmt, rqt;
	struct timespec finish, start, res;
	long m;
	int i;

	m = LONG_MAX;
	for (i = 0; i < 100; i++) {
		rqt.tv_sec  = 0;
		rqt.tv_nsec = N;
		clock_gettime(CLOCK_REALTIME_PRECISE, &start);
		if (nanosleep(&rqt, &rmt) == -1)
			err(1, "nanosleep");
		clock_gettime(CLOCK_REALTIME_PRECISE, &finish);
		timespecsub(&finish, &start, &res);
		if (res.tv_nsec < N)
			errx(1, "Short sleep: %ld", res.tv_nsec);
//		fprintf(stderr, "asked for %f, got %f\n", (double)N / 1e9,
//		    (double)res.tv_nsec / 1e9);
		if (m > res.tv_nsec)
			m = res.tv_nsec;
	}
	if (m > 2 * N) {
		fprintf(stderr, "nanosleep(%fs). Best value is %fs.\n",
		    (double)N / 1e9, (double)m / 1e9);
		errx(1, "FAIL");
	}

	return (0);
}
