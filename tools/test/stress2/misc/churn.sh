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

# VM test. No problems seen.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/churn.c
mycc -o churn -Wall -Wextra -O0 -g churn.c -lpthread || exit 1
rm -f churn.c

/tmp/churn `sysctl -n hw.ncpu` `sysctl -n hw.usermem`
s=$?

rm -rf /tmp/churn
exit $?

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#define	__NP__
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define ARRAYSIZE (4 * 1024)
#define RUNTIME (10 * 60)
#define SYNC 0

volatile u_int *share;
long mem, parallel;
int done;

struct {
	void *addr;
	long pages;
	volatile u_int busy;
} v[ARRAYSIZE];

void *
test2(void *arg __unused)
{
	long i, j, n;
	volatile char *cp;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	while (done == 0) {
		n = 0;
		for (i = 0; i < ARRAYSIZE; i++) {
			if (v[i].pages == 0)
				continue;
			atomic_add_int(&v[i].busy, 1);
			if (v[i].busy != 1) {
				atomic_add_int(&v[i].busy, -1);
				continue;
			}
			cp = v[i].addr;
			for (j = 0; j < v[i].pages; j++)
				cp[j * PAGE_SIZE] = 1;
			atomic_add_int(&v[i].busy, -1);
			n++;
		}
		if (n == 0) {
			usleep(10000);
		}
	}

	return (0);
}

void
test(void)
{
	pthread_t tp[2];
	size_t len;
	time_t start;
	long i, j, k, size;
	int r;
	volatile char *cp;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != (volatile u_int)parallel)
		;

	if ((r = pthread_create(&tp[0], NULL, test2, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&tp[1], NULL, test2, NULL)) != 0)
		errc(1, r, "pthread_create");
	size = 0;
	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (k = 0; k < ARRAYSIZE; k++) {
			i = arc4random() % ARRAYSIZE;
			if (v[i].pages != 0)
				break;
		}
		if (v[i].addr != NULL) {
			atomic_add_int(&v[i].busy, 1);
			if (v[i].busy != 1) {
				atomic_add_int(&v[i].busy, -1);
				continue;
			}
			if (munmap(v[i].addr, v[i].pages * PAGE_SIZE) == -1)
				err(1, "munmap(%p, %ld)", v[i].addr, v[i].pages);
			v[i].addr = NULL;
			size -= v[i].pages;
			v[i].pages = 0;
			atomic_add_int(&v[i].busy, -1);
		}
		if (size < mem) {
			j = round_page((arc4random() % (mem / 10)) + 1);
			len = j * PAGE_SIZE;
			if ((v[i].addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
			    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED) {
				v[i].addr = NULL;
				continue;
			}
			atomic_add_int(&v[i].busy, 1);
			v[i].pages = j;
			size += j;
			assert(size > 0);
			cp = v[i].addr;
			for (k = 0; k < j * PAGE_SIZE; k += PAGE_SIZE)
				cp[k] = 1;
			atomic_add_int(&v[i].busy, -1);
		}
	}
	done = 1;
	pthread_join(tp[0], NULL);
	pthread_join(tp[1], NULL);

	_exit(0);
}

int
main(int argc, char *argv[])
{
	size_t len;
	int e, i, *pids, status;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <memory>\n", argv[0]);
		_exit(1);
	}

	parallel = atol(argv[1]);
	mem = atol(argv[2]) / PAGE_SIZE;
	mem = mem / parallel;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	pids = malloc(sizeof(void *) * parallel);
	for (i = 0; i < parallel; i++) {
		if ((pids[i] = fork()) == 0)
		test();
	}
	for (i = 0; i < parallel; i++) {
		waitpid(pids[i], &status, 0);
		e += status == 0 ? 0 : 1;
	}

	return (e);
}
