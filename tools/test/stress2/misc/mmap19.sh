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

# panic: pmap active 0xfffff801d22cdae8" seen.
# Variation of  mmap18.sh.
# http://people.freebsd.org/~pho/stress/log/kostik712.txt
# Fixed by r271000.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap19.c
mycc -o mmap19 -Wall -Wextra -O2 mmap19.c -lpthread || exit 1
rm -f mmap19.c

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 2m -i 20 -k)"
rnd=`od -An -N1 -t u1 /dev/random | sed 's/ //g'`
sleep $((rnd % 10))
s=0
for i in `jot 2`; do
	/tmp/mmap19 || s=$?
done
while pgrep -q swap; do
	pkill -9 swap
done

rm -f /tmp/mmap19 /tmp/mmap19.core
exit $s
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

#define LOOPS 50
#define N (128 * 1024 / (int)sizeof(u_int32_t))
#define PARALLEL 50

u_int32_t r[N];
void *p;

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
	int i, fd;

	pthread_set_name_np(pthread_self(), __func__);
	len = 1LL * 1024 * 1024 * 1024;

	for (i = 0; i < 100; i++) {
		if ((fd = open("/dev/zero", O_RDWR)) == -1)
			err(1,"open()");

		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE,
		    fd, 0)) != MAP_FAILED) {
			usleep(100);
			munmap(p, len);
		}

		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1,
		    0)) != MAP_FAILED) {
			usleep(100);
			munmap(p, len);
		}
		close(fd);
	}

	return (NULL);
}

void *
tmprotect(void *arg __unused)
{
	void *addr;
	size_t len;
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
	if (n < 10)
		fprintf(stderr, "Note: tmprotect() only succeeded %d times.\n",
		    n);

	return (NULL);
}

void
test(void)
{
	pthread_t tid[2];
	int i, rc;

	if ((rc = pthread_create(&tid[0], NULL, tmmap, NULL)) != 0)
		errc(1, rc, "tmmap()");
	if ((rc = pthread_create(&tid[1], NULL, tmprotect, NULL)) != 0)
		errc(1, rc, "tmprotect()");

	for (i = 0; i < 100; i++) {
		if (fork() == 0) {
			usleep(10000);
			_exit(0);
		}
		wait(NULL);
	}

	alarm(120);
	for (i = 0; i < 2; i++)
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join(%d)", i);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	struct rlimit rl;
	int e, i, j, status;

	rl.rlim_max = rl.rlim_cur = 0;
	if (setrlimit(RLIMIT_CORE, &rl) == -1)
		warn("setrlimit");

	for (i = 0; i < N; i++)
		r[i] = arc4random();

	e = 0;
	start = time(NULL);
	for (i = 0; i < LOOPS && e == 0; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if ((pids[j] = fork()) == 0)
				test();
		}

		for (j = 0; j < PARALLEL; j++) {
			if (waitpid(pids[j], &status, 0) != pids[j])
				err(1, "waitpid(%d)", pids[j]);
			if (status != 0 && e == 0 && status != SIGSEGV && status != EFAULT)
				e = status;
		}
		if (time(NULL) - start > 1200) {
			fprintf(stderr, "Timed out.");
			break;
		}
	}

	return (e);
}
