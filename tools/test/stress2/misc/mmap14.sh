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

# Simplified version of mmap10.sh with focus on core dumps
# Deadlock seen:
# http://people.freebsd.org/~pho/stress/log/kostik673.txt
# No issues seen with r272060.

# panic: vm_reserv_populate: reserv 0xfffff807cbd46300 is already promoted
# http://people.freebsd.org/~pho/stress/log/kostik764.txt
# Fixed by r280238

# panic: vm_reserv_break: reserv 0xfffff807cbe9af00 is full
# https://people.freebsd.org/~pho/stress/log/alan006.txt

# panic: vm_page_dirty: page is invalid!
# https://people.freebsd.org/~pho/stress/log/kostik818.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap14.c
mycc -o mmap14 -Wall -Wextra -O2 -g mmap14.c -lpthread || exit 1
rm -f mmap14.c

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 2m -i 20 -k -h)"
sleep `jot -r 1 1 10`
wire=$((`sysctl -n vm.max_user_wired` - \
    `sysctl -n vm.stats.vm.v_user_wire_count`))
for i in `jot 2`; do
	/tmp/mmap14 $wire
done
while pgrep -q swap; do
	pkill -9 swap
done

rm -f /tmp/mmap14 /tmp/mmap14.core
exit 0
EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOOPS 2
#define N (128 * 1024 / (int)sizeof(u_int32_t))
#define PARALLEL 50

static void *p;
static u_int32_t r[N];

static unsigned long
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

static void *
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

static void *
tmmap(void *arg __unused)
{
	size_t len;
	int i, fd;

	pthread_set_name_np(pthread_self(), __func__);
	len = 1LL * 1024 * 1024 * 1024;

	for (i = 0; i < 100; i++) {
		if ((fd = open("/dev/zero", O_RDWR)) == -1)
			err(1,"open()");

		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
		    0)) != MAP_FAILED) {
			usleep(100);
			munmap(p, len);
		}

		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0)) !=
		    MAP_FAILED) {
			usleep(100);
			munmap(p, len);
		}
		close(fd);
	}

	return (NULL);
}

static void *
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
	if (n < 10)
		fprintf(stderr, "Note: tmlock() only succeeded %d times.\n",
		    n);

	return (NULL);
}

static void
test(void)
{
	pthread_t tid[4];
	int i, rc;

	if ((rc = pthread_create(&tid[0], NULL, tmmap, NULL)) != 0)
		errc(1, rc, "tmmap()");
	if ((rc = pthread_create(&tid[1], NULL, tmlock, NULL)) != 0)
		errc(1, rc, "tmlock()");

	for (i = 0; i < 100; i++) {
		if (fork() == 0) {
			usleep(10000);
			_exit(0);
		}
		wait(NULL);
	}

	raise(SIGSEGV);

	for (i = 0; i < 2; i++)
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join(%d)", i);
	_exit(0);
}

int
main(int argc, char *argv[])
{
	struct rlimit rl;
	rlim_t maxlock;
	int i, j;

	if (argc != 2) {
		fprintf(stderr, "Usage:%s <max pages to lock.>\n", argv[0]);
		exit(1);
	}
	if (getrlimit(RLIMIT_MEMLOCK, &rl) == -1)
		warn("getrlimit");
	maxlock = atol(argv[1]);
	if (maxlock == 0)
		errx(1, "Argument is zero");
	maxlock = (maxlock / 10 * 8) / PARALLEL * PAGE_SIZE;
	if (maxlock < rl.rlim_cur) {
		rl.rlim_max = rl.rlim_cur = maxlock;
		if (setrlimit(RLIMIT_MEMLOCK, &rl) == -1)
			warn("setrlimit");
	}

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
