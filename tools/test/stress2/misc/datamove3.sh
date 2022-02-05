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

# Threaded variation of datamove.sh

# Based on a test scenario by ups and suggestions by kib

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > datamove3.c
mycc -o datamove3 -Wall datamove3.c -lpthread
rm -f datamove3.c

n=5
old=`sysctl vm.old_msync | awk '{print $NF}'`
sysctl vm.old_msync=1
for i in `jot $n`; do
	mkdir -p /tmp/datamove3.dir.$i
	cd /tmp/datamove3.dir.$i
	/tmp/datamove3 &
done
cd /tmp
for i in `jot $n`; do
	wait
done
for i in `jot $n`; do
	rm -rf /tmp/datamove3.dir.$i
done
sysctl vm.old_msync=$old

rm -rf /tmp/datamove3
exit 0
EOF
/*-
 * Copyright (c) 2006, Stephan Uphoff <ups@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct args {
	char *bp;
	int fd1;
	int fd2;
} a[2];

int prepareFile(char *, int *);
void * mapBuffer(void *);
int startIO(int, char *);

int pagesize;

#define FILESIZE (32*1024)
char wbuffer   [FILESIZE];

/* Create a FILESIZE sized file - then remove file data from the cache */
int
prepareFile(char *filename, int *fdp)
{
	int fd;
	int len;
	int status;
	void *addr;

	fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
	if (fd == -1) {
		perror("Creating file");
		return fd;
	}
	len = write(fd, wbuffer, FILESIZE);
	if (len < 0) {
		perror("Write failed");
		return 1;
	}
	status = fsync(fd);
	if (status != 0) {
		perror("fsync failed");
		return 1;
	}
	addr = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		perror("Mmap failed");
		return 1;
	}
	status = msync(addr, FILESIZE, MS_INVALIDATE | MS_SYNC);
	if (status != 0) {
		perror("Msync failed");
		return 1;
	}
	munmap(addr, FILESIZE);

	*fdp = fd;
	return 0;
}

/* mmap a 2 page buffer - first page is from fd1, second page from fd2 */
void *
mapBuffer(void *ar)
{
	void *addr;
	char *buffer;
	int i;

	i = (intptr_t)ar;
	addr = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE, MAP_SHARED, a[i].fd1, 0);
	if (addr == MAP_FAILED) {
		err(1, "Mmap failed");
	}
	buffer = addr;
	addr = mmap(buffer + pagesize, pagesize, PROT_READ | PROT_WRITE, MAP_FIXED |
		    MAP_SHARED, a[i].fd2, 0);

	if (addr == MAP_FAILED) {
		err(1, "Mmap2 failed");
	}
	a[i].bp = buffer;
	sleep(1);
	return (NULL);
}

int
startIO(int fd, char *buffer)
{
	ssize_t len;

	len = write(fd, buffer, 2 * pagesize);
	if (len == -1) {
		warn("startIO(%d, %p): write failed", fd, buffer);
		return 1;
	}
	return 0;
}

int
main(int argc, char *argv[], char *envp[])
{

	int fdA, fdB, fdDelayA, fdDelayB;
	int r, status;
	char *bufferA, *bufferB;
	pid_t pid;
	pthread_t threads[2];

	pagesize = getpagesize();

	if ((prepareFile("A", &fdA))
	    || (prepareFile("B", &fdB))
	    || (prepareFile("DelayA", &fdDelayA))
	    || (prepareFile("DelayB", &fdDelayB)))
		exit(1);

	a[0].fd1 = fdDelayA;
	a[0].fd2 = fdB;

	a[1].fd1 = fdDelayB;
	a[1].fd2 = fdA;

	if ((r = pthread_create(&threads[0], NULL, mapBuffer, (void *)0)) != 0)
		errc(1, r, "pthread_create()");
	if ((r = pthread_create(&threads[1], NULL, mapBuffer, (void *)1)) != 0)
		errc(1, r, "pthread_create()");

	while (a[0].bp == NULL || a[1].bp == NULL)
		pthread_yield();

	bufferA = a[0].bp;
	bufferB = a[1].bp;

	pid = fork();

	if (pid == 0) {
		status = startIO(fdA, bufferA);
		exit(status);
	}
	if (pid == -1) {
		exit(1);
	}
	status = startIO(fdB, bufferB);
	exit(status);

}
