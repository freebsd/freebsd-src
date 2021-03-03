#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# selfd regression test attempt for r285310. Not reproduced.
# Watchdog fired seen: https://people.freebsd.org/~pho/stress/log/selfd.txt

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/selfd.c
mycc -o selfd -Wall -Wextra -O0 -g selfd.c -lpthread || exit 1
rm -f selfd.c
cd $odir

rm -rf /tmp/stressX.control
daemon sh -c "(cd ../testcases/swap; ./swap -t 10m -i 20 -l 100)" > \
    /dev/null 2>&1
sleep 2

/tmp/selfd
s=$?

while pgrep -q swap; do
	pkill -9 swap
done

rm -rf /tmp/selfd
exit $s

EOF
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static pthread_barrier_t barr;

#define PARALLEL 16
#define PIPES 32
#define RUNTIME (5 * 60)

static void
handler(int s __unused)
{
}

static void
test(void)
{
	fd_set rset, tmpl;
	struct sigaction sa;
	struct timeval timeout;
	time_t start;
	int fds[PIPES][2], i, n, r;

	r = pthread_barrier_wait(&barr);
	if (r != 0 && r != PTHREAD_BARRIER_SERIAL_THREAD)
	    errc(1, r, "pthread_barrier_wait");

	FD_ZERO(&tmpl);
	for (i = 0; i < PIPES; i++) {
		if (pipe(fds[i]) == -1)
			err(1, "pipe()");
		FD_SET(fds[i][0], &tmpl);
	}
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) == -1)
		err(1, "sigaction");
	start = time(NULL);
	while ((time(NULL) - start) < 10) {
		rset = tmpl;
		timeout.tv_sec  = 0;
		timeout.tv_usec = arc4random() % 10000 + 100;
		n = arc4random() % PIPES;
		ualarm(arc4random() % 10000 + 100, 0);
		write(fds[n][1], "a", 1);
		if ((n = select(PIPES, &rset, NULL, NULL, &timeout)) < 0)
			if (errno != EINTR)
				err(1, "select()");
		ualarm(0, 0);
	}

	_exit(0);

}

int
main(void)
{
	pthread_barrierattr_t attr;
	time_t start;
	int i, r;

	if ((r = pthread_barrierattr_init(&attr)) != 0)
		errc(1, r, "pthread_barrierattr_init");
	if ((r = pthread_barrierattr_setpshared(&attr,
	    PTHREAD_PROCESS_SHARED)) != 0)
		errc(1, r, "pthread_barrierattr_setpshared");
	if ((r = pthread_barrier_init(&barr, &attr, PARALLEL)) != 0)
		errc(1, r, "pthread_barrier_init");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		for (i = 0; i < PARALLEL; i++)
			if (fork() == 0)
				test();
		for (i = 0; i < PARALLEL; i++)
			wait(NULL);
	}

	return (0);
}
