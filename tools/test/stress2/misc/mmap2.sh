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

#   Stress mmap by having at most 100 threads mapping random areas within
#   a 100 Mb range.

# Test scenario by kib@

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mmap2.c
mycc -o mmap2 -Wall -g mmap2.c -lpthread
rm -f mmap2.c

start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 600 ]; do
	./mmap2
done
rm -f ./mmap2*
exit

EOF
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

static void
work(int nr)
{
	int fd, m;
	void *p;
	size_t left, len;
	char path[128];

	p = (void *)STARTADDR + trunc_page(arc4random() % ADRSPACE);
	left = ADRSPACE - (size_t)p + STARTADDR;
	len = trunc_page(arc4random() % left) + PAGE_SIZE;
	fd = -1;

	if (arc4random() % 100 < 90)
		sprintf(path, "/tmp/mmap.%06d.%04d", getpid(), nr);
	else
		sprintf(path, "/dev/zero");
	if (arc4random() % 2 == 0) {
		if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
			err(1,"open()");
		if (ftruncate(fd, len) == -1)
			err(1, "ftruncate");
		if (arc4random() % 2 == 0) {
			if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
					MAP_FAILED) {
				if (errno == ENOMEM)
					return;
				err(1, "mmap()");
			}
		} else {
			if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) ==
					MAP_FAILED) {
				if (errno == ENOMEM)
					return;
				err(1, "mmap()");
			}
		}
		if (fd > 0 && strcmp(path, "/dev/zero"))
			if (unlink(path) == -1)
				err(1, "unlink(%s)", path);
	} else {
		if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0)) == MAP_FAILED) {
			if (errno == ENOMEM)
				return;
			err(1, "mmap()");
		}
		strcpy(path, "anon");
	}
#if 0
	printf("nr = %d, %-14s, start = %p, end = %p, len = 0x%08x, (%5d pages)\n",
		nr, path, p, p + len, len, len>>PAGE_SHIFT);
#endif

	*(int *)p = 1;

	if (arc4random() % 2 == 0) {
		m = arc4random() % 10;
		if (madvise(p, len, m) == -1)
			warn("madvise(%p, %zd, %d)", p, len, m);
	}
	if (arc4random() %2 == 0)
		if (mprotect(p, trunc_page(arc4random() % len), PROT_READ) == -1 )
			err(1, "mprotect failed with error:");
	if (arc4random() % 2 == 0) {
		if (arc4random() %2 == 0) {
			if (msync(p, 0, MS_SYNC) == -1)
				err(1, "msync(%p)", p);
		} else {
			if (msync(p, 0, MS_INVALIDATE) == -1)
				err(1, "msync(%p)", p);
		}
	}
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

//	printf("Address start 0x%x, address end 0x%x, pages %d\n",
//		STARTADDR, STARTADDR + ADRSPACE, ADRSPACE>>PAGE_SHIFT);
	n = arc4random() % THREADS + 1;
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
