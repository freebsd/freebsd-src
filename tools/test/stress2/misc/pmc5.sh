#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# pmc fuzz test

. ../default.cfg

kldstat -v | grep -q hwpmc  || { kldload hwpmc; loaded=1; }
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/pmc5.c
mycc -o pmc5 -Wall -Wextra -O0 -g pmc5.c -lpmc -lpthread || exit 1
rm -f pmc5.c

for i in `jot 100`; do
	./pmc5
done > /dev/null 2>&1

rm -rf pmc5 pmc5.core
[ $loaded ] && kldunload hwpmc
exit 0

EOF
#include <sys/param.h>
#include <sys/event.h>
#include <sys/mman.h>

#include <machine/atomic.h>

#include <err.h>
#include <pmc.h>
#include <pmclog.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile u_int *share;
static int fd1[2];
static int kq;
#define THREADS 20
#define SYNC 0

void *
test(void *arg __unused)
{

	void *rfd;
	char *cmdline[] = { "/usr/bin/true", NULL };

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != THREADS)
		;
	if ((rfd = pmclog_open(kq)) == NULL)
		err(1, "pmclog_open(%d)", kq);
//	if (pmc_configure_logfile(fd1[1]) < 0)
	if (pmc_configure_logfile(kq) < 0)
		err(1, "ERROR: Cannot configure log file");
	sleep(1);
	usleep(arc4random() % 20000);
        if (execve(cmdline[0], cmdline, NULL) == -1)
		err(1, "execve");

	return (0);
}

int
main(void)
{
	struct kevent ev[3];
	pthread_t tid[THREADS];
	size_t len;
	int i, n, rc;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	if (pmc_init() == -1)
		err(1, "pmc_init");

	if (pipe(fd1) == -1)
		err(1, "pipe()");

	if ((kq = kqueue()) < 0)
		err(1, "kqueue(). %s:%d", __FILE__, __LINE__);

	n = 0;
	EV_SET(&ev[n], fd1[1], EVFILT_WRITE,
		    EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, 0);
	n++;

	if (kevent(kq, ev, n, NULL, 0, NULL) < 0)
		err(1, "kevent(). %s:%d", __FILE__, __LINE__);
	n = 0;
	EV_SET(&ev[n], fd1[1], EVFILT_WRITE,
		    EV_DELETE, 0, 0, 0);
	n++;
	if (kevent(kq, ev, n, NULL, 0, NULL) < 0)
		warn("kevent(). %s:%d", __FILE__, __LINE__);

	for (i = 0; i < THREADS; i++) {
		if ((rc = pthread_create(&tid[i], NULL, test, NULL)) != 0)
			errc(1, rc, "test()");
	}
	for (i = 0; i < THREADS; i++) {
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join(%d)", i);
	}

	return (0);
}
