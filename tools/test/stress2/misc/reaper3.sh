#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# procctl(2) fuzz test

prog=`basename ${0%.sh}`
cat > /tmp/$prog.c <<EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/procctl.h>

#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;

#define PARALLEL 25
#define RUNTIME 120

static long
random_long(long mi, long ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

static void
flip(void *ap, size_t len)
{
	unsigned char *cp;
	int byte;
	unsigned char bit, buf, mask;

	cp = (unsigned char *)ap;
	byte = random_long(0, len);
	bit = random_long(0,7);
	mask = ~(1 << bit);
	buf = cp[byte];
	buf = (buf & mask) | (~buf & ~mask);
	cp[byte] = buf;
}

static void *
tr(void *arg __unused)
{
	for (;;)
		pause();

	return (NULL);
}

static void
test(void) {
	pthread_t thr;
	struct procctl_reaper_kill killemall;
	pid_t pid;
	time_t start;
	int data[20], debug, e, n, m;

	debug = 0; /* set to 1 for debug output */
	n = m = 0;
	if (procctl(P_PID, getpid(), PROC_REAP_ACQUIRE, NULL) == -1)
		err(EXIT_FAILURE, "Fail to acquire the reaper");
	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		m++;
		share[0] = 0;
		if ((pid = fork()) == 0) {
			e = pthread_create(&thr, NULL, tr, NULL);
			if (e != 0)
				errc(1, e, "pthread_create");
			share[0] = 1;
			setproctitle("child");
			usleep(arc4random() % 200);
			if (arc4random() % 100 < 2)
				raise(SIGSEGV);
			_exit(0);
		}
		arc4random_buf(data, sizeof(data));
		while (share[0] == 0)
			usleep(10);
		killemall.rk_sig = SIGTERM;
		killemall.rk_flags = 0;
		flip(&killemall, sizeof(killemall));
		if (procctl(P_PID, getpid(), PROC_REAP_KILL,
		    &killemall) == 0)
			n++;
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid()");
	}
	if (debug == 1)	
		fprintf(stderr, "n = %d out of %d\n", n, m);
	_exit(0);
}

int
main(void) {
	pid_t pids[PARALLEL];
	size_t len;
	int i;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test();
	}
	for (i = 0; i < PARALLEL; i++)
		if (waitpid(pids[i], NULL, 0) != pids[i])
			err(1, "waitpid()");
}
EOF
cc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c -lpthread || exit 1
rm /tmp/$prog.c

here=`pwd`
cd /tmp
./$prog; s=$?
cd $here

rm -f /tmp/$prog /tmp/$prog.core
exit $s 
