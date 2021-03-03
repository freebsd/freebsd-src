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

# This problem was seen with WiP kernel code:
# https://people.freebsd.org/~pho/stress/log/kostik1210.txt

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > context2.c
mycc -o context2 -Wall -Wextra -O2 context2.c -lpthread || exit 1
rm -f context2.c
[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 10m -i 20)" > \
    /dev/null 2>&1
for i in `jot 4`; do
	/tmp/context2 &
done
wait
while pgrep -q swap; do
	pkill -9 swap
done
rm -f /tmp/context2
exit 0
EOF
/*
 * Inspired by lmbench-3.0-a9/src/lat_ctx.c
 * Pass a token thru pipes to NTHREADS+1 threads in a circular list.
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NTHREADS 64
#define RUNTIME 300

pid_t pid[NTHREADS];
int fds[NTHREADS+1][2];

void *
thr_routine(void *arg)
{
	int i;
	int token;

	i = (long)arg;
	for (;;) {
		if (read(fds[i][0], &token, sizeof(token)) != sizeof(token))
			err(1, "read pipe 2");
		token++;
		if (write(fds[i+1][1], &token, sizeof(token)) != sizeof(token))
			err(1, "write pipe 1");
	}
	return (0);
}

int
main(void)
{
	pthread_t threads[NTHREADS];
	time_t start;
	long arg;
	int i, r, token;

	for (i = 0; i < NTHREADS + 1; i++) {
		if (pipe(fds[i]) == -1)
			err(1, "pipe");
	}

	for (i = 0; i < NTHREADS; i++) {
		arg = i;
		if ((r = pthread_create(&threads[i], NULL, thr_routine,
		    (void *)arg)) != 0)
			errc(1, r, "pthread_create(): %s\n", strerror(r));
	}

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		token = 0;
		if (write(fds[0][1], &token, sizeof(token)) != sizeof(token))
			err(1, "write pipe 2");
		if (read(fds[NTHREADS][0], &token, sizeof(token)) !=
		    sizeof(token))
			err(1, "read pipe 1");
	}

	for (i = 0; i < NTHREADS; i++)
		if ((r = pthread_cancel(threads[i])) != 0)
			errc(1, r, "pthread_cancel(%d)", i);
	for (i = 0; i < NTHREADS; i++)
		if ((r = pthread_join(threads[i], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", i);

        return (0);
}
