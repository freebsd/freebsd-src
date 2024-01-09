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

# "panic: vm_page_dirty: page is invalid!" seen.
# http://people.freebsd.org/~pho/stress/log/mmap10.txt
# No problems seen after r271681.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap10.c
mycc -o mmap10 -Wall -Wextra -O2 -g mmap10.c -lpthread || exit 1
rm -f mmap10.c

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 2m -i 20 -k > /dev/null)"
ulimit -c 0
sleep `jot -r 1 0 9`
for i in `jot 2`; do
	su $testuser -c /tmp/mmap10 &
done
start=`date +%s`
while pgrep -q mmap10; do
	[ $((`date +%s` - start)) -ge 300 ] && break
	sleep 2
done
while pgrep -q 'mmap10|swap'; do
	pkill -9 mmap10 swap
	sleep 2
done
wait

rm -f /tmp/mmap10 /tmp/mmap10.core
exit 0
EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOOPS 2
#define MMSIZE (192 * 1024 * 1024)
#define N (128 * 1024 / (int)sizeof(u_int32_t))
#define PARALLEL 50

static int debug = 0; /* set to "1" for debug output */
void *p;
u_int32_t r[N];

unsigned long
makearg(void)
{
	unsigned long val;
	unsigned int i;

	val = arc4random();
	i   = arc4random() % 100;
	if (i < 20)
		val = val & 0xff;
	if (i >= 20 && i < 40)
		val = val & 0xffff;
	if (i >= 40 && i < 60)
		val = (unsigned long)(r) | (val & 0xffff);
#if defined(__LP64__)
	if (i >= 60) {
		val = (val << 32) | arc4random();
		if (i > 80)
			val = val & 0x00007fffffffffffUL;
	}
#endif

	return(val);
}

void *
makeptr(void)
{
	unsigned long val;

	if (p != MAP_FAILED && p != NULL)
		val = (unsigned long)p + arc4random();
	else
		val = makearg();
	val = trunc_page(val);

	return ((void *)val);
}

void *
tmmap(void *arg __unused)
{
	size_t len;
	int i, j, fd;

	pthread_set_name_np(pthread_self(), __func__);
	len = MMSIZE;

	for (i = 0, j = 0; i < 100; i++) {
		if ((fd = open("/dev/zero", O_RDWR)) == -1)
			err(1,"open()");

		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE,
		    fd, 0)) != MAP_FAILED) {
			usleep(100);
			munmap(p, len);
			j++;
		}

		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON,
		    -1, 0)) != MAP_FAILED) {
			usleep(100);
			munmap(p, len);
			j++;
		}
		close(fd);
	}
	if (j == 0)
		fprintf(stderr, "FAIL: all mmap(2) calls failed.\n");

	return (NULL);
}

void *
tmlock(void *arg __unused)
{
	size_t len;
	int i, n;

	pthread_set_name_np(pthread_self(), __func__);
	n = 0;
	for (i = 0; i < 200; i++) {
		len = trunc_page(makearg());
		if (mlock(makeptr(), len) == 0)
			n++;
		len = trunc_page(makearg());
		if (arc4random() % 100 < 50)
			if (munlock(makeptr(), len) == 0)
				n++;
	}
	if (debug == 1 && n < 10)
		fprintf(stderr, "Note: tmlock() only succeeded %d times.\n",
		    n);

	return (NULL);
}

void *
tmprotect(void *arg __unused)
{
	size_t len;
	void *addr;
	int i, n, prot;

	pthread_set_name_np(pthread_self(), __func__);
	n = 0;
	for (i = 0; i < 200; i++) {
		addr = makeptr();
		len = trunc_page(makearg());
		prot = makearg();
		if (mprotect(addr, len, prot) == 0)
			n++;
		usleep(1000);
	}
	if (debug == 1 && n < 10)
		fprintf(stderr, "Note: tmprotect() only succeeded %d times.\n",
		    n);

	return (NULL);
}

void *
tmlockall(void *arg __unused)
{
	int flags, i, n;

	pthread_set_name_np(pthread_self(), __func__);
	n = 0;
	for (i = 0; i < 200; i++) {
		flags = makearg();
		if (mlockall(flags) == 0)
			n++;
		usleep(100);
		munlockall();
		usleep(1000);
	}
	if (debug == 1 && n < 10)
		fprintf(stderr, "Note: tmlockall() only succeeded %d times.\n",
		    n);

	return (NULL);
}

void
test(void)
{
	pthread_t tid[4];
	int i, rc;

	if ((rc = pthread_create(&tid[0], NULL, tmmap, NULL)) != 0)
		errc(1, rc, "tmmap()");
	if ((rc = pthread_create(&tid[1], NULL, tmlock, NULL)) != 0)
		errc(1, rc, "tmlock()");
	if ((rc = pthread_create(&tid[2], NULL, tmprotect, NULL)) != 0)
		errc(1, rc, "tmprotect()");
	if ((rc = pthread_create(&tid[3], NULL, tmlockall, NULL)) != 0)
		errc(1, rc, "tmlockall()");

	for (i = 0; i < 100; i++) {
		if (fork() == 0) {
			usleep(10000);
			_exit(0);
		}
		wait(NULL);
	}

	for (i = 0; i < 4; i++)
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join(%d)", i);
	_exit(0);
}

int
main(void)
{
	int i, j;

	for (i = 0; i < N; i++)
		r[i] = arc4random();

	for (i = 0; i < LOOPS; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0)
				test();
		}

		for (j = 0; j < PARALLEL; j++)
			wait(NULL);
	}

	return (0);
}
