#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Variation of mmap2.sh with focus on random arguments for mprotect()
# https://people.freebsd.org/~pho/stress/log/kostik209.txt

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mmap3.c
mycc -o mmap3 -Wall mmap3.c -lpthread || exit 1
rm -f mmap3.c

start=`date '+%s'`
while [ `date '+%s'` -lt $((start + 5 * 60)) ]; do
	./mmap3
done
echo "Expect Segmentation faults"
trap "ls /tmp/mmap3* 2>/dev/null | grep -E 'mmap3\.[0-9]{6}\.[0-9]{4}$' | \
    xargs rm -v" EXIT INT
start=`date '+%s'`
while [ `date '+%s'` -lt $((start + 5 * 60)) ]; do
	./mmap3 random
done
rm -f /tmp/mmap3 mmap3.core
exit

EOF
/*
   Stress mmap by having max 100 threads mapping random areas within
   a 100 Mb range.
 */
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define THREADS 100
#define STARTADDR 0x50000000U
#define ADRSPACE  0x06400000U /* 100 Mb */

static int ra;

void
trash(void *p)
{
	unsigned long v;

	mprotect(p, 0x570e3d38, 0x2c8fd54f);
	if (ra) {
		v = arc4random();
#if defined(__LP64__)
		v = v << 32 | arc4random();
#endif
		madvise((void *)v, arc4random(), arc4random());
		mprotect((void *)v, arc4random(), arc4random());
		msync((void *)v, arc4random(), arc4random());
	}

}

void
work(int nr)
{
	int fd, m;
	void *p;
	size_t len;
	char path[128];

	p = (void *)STARTADDR;
	len = ADRSPACE;

	sprintf(path, "/tmp/mmap3.%06d.%04d", getpid(), nr);
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open()");
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return;
		err(1, "mmap()");
	}
	if (unlink(path) == -1)
		err(1, "unlink(%s)", path);

	trash(p);

	m = arc4random() % 10;
	if (madvise(p, len, m) == -1)
		warn("madvise(%p, %zd, %d)", p, len, m);
	if (mprotect(p, trunc_page(arc4random() % len), PROT_READ) == -1 )
		err(1, "mprotect failed with error:");
	if (msync(p, 0, MS_SYNC) == -1)
		err(1, "msync(%p)", p);
	if (munmap(p, len) == -1)
		err(1, "munmap(%p)", p);
	close(fd);
}

void *
thr(void *arg)
{
	int i;

	for (i = 0; i < 512; i++) {
		work(*(int *)arg);
	}
	return (0);
}

int
main(int argc, char **argv)
{
	pthread_t threads[THREADS];
	int nr[THREADS];
	int i, n, r;

	n  = arc4random() % 14 + 5;
	ra = argc != 1;
//	printf("Address start 0x%x, address end 0x%x, pages %d, n %d\n",
//		STARTADDR, STARTADDR + ADRSPACE, ADRSPACE>>PAGE_SHIFT, n);
	for (i = 0; i < n; i++) {
		nr[i] = i;
		if ((r = pthread_create(&threads[i], NULL, thr, (void *)&nr[i])) != 0)
			errc(1, r, "pthread_create()");
	}

	for (i = 0; i < n; i++) {
		if ((r = pthread_join(threads[i], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", i);
	}

	return (0);
}
