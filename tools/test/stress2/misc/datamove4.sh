#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# Variation of the datamove2.sh, using TMPFS
# Deadlock seen
# https://people.freebsd.org/~pho/stress/log/datamove4.txt

# panic: elf32_putnote: Note type 10 changed as we read it (2236 > 2220)...
# https://people.freebsd.org/~pho/stress/log/datamove4-2.txt
# Fixed by r288944.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > datamove4.c
mycc -o datamove4 -Wall -Wextra -O2 -g datamove4.c
rm -f datamove4.c

mount | grep -q "$mntpoint " && umount $mntpoint
mount -t tmpfs tmpfs $mntpoint
chmod 777 $mntpoint

for i in `jot 5`; do
	su $testuser -c "cd $mntpoint; /tmp/datamove4"
done
while mount | grep -q $mntpoint; do
	umount -f $mntpoint > /dev/null 2>&1
done

rm -rf /tmp/datamove4
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int	prepareFile(char *filename, int *fdp);
int	mapBuffer  (char **bufferp, int fd1, int fd2);
int	startIO    (int fd, char *buffer);

int	pagesize;

#define FILESIZE (32*1024)
char	wbuffer   [FILESIZE];

/* Create a FILESIZE sized file - then remove file data from the cache */
int
prepareFile(char *filename, int *fdp)
{
	int	fd;
	int	len;
	int	status;
	void	*addr;

	fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
	if (fd == -1) {
		perror(filename);
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
	if (munmap(addr, FILESIZE) == -1) {
		perror("munmap failed");
		return 1;
	}

	*fdp = fd;
	return 0;
}

/* mmap a 2 page buffer - first page is from fd1, second page from fd2 */
int
mapBuffer(char **bufferp, int fd1, int fd2)
{
	void *addr;
	char *buffer;

	addr = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
	if (addr == MAP_FAILED) {
		perror("Mmap failed");
		return 1;
	}
	buffer = addr;
	addr = mmap(buffer + pagesize, pagesize, PROT_READ | PROT_WRITE, MAP_FIXED |
		    MAP_SHARED, fd2, 0);

	if (addr == MAP_FAILED) {
		perror("Mmap2 failed");
		return 1;
	}
	*bufferp = buffer;
	return 0;
}

void
unmapBuffer(char *bufferp)
{
	if (munmap(bufferp, pagesize * 2) == -1)
		err(1, "unmap 1. buffer");
	/*
	   The following unmaps something random, which could trigger:
	   Program received signal SIGSEGV, Segmentation fault.
	   free (cp=0x28070000) at /usr/src/libexec/rtld-elf/malloc.c:311
	*/

#if 0
	if (munmap(bufferp + pagesize * 2, pagesize * 2) == -1)
		err(1, "unmap 2. buffer");
#endif
}

int
startIO(int fd, char *buffer)
{
	ssize_t	len;

	len = write(fd, buffer, 2 * pagesize);
	if (len == -1) {
		perror("write failed");
		return 1;
	}
	return 0;
}

int
main()
{

	int	fdA, fdB, fdDelayA, fdDelayB;
	int	status;
	int	i;
	char	*bufferA, *bufferB;
	pid_t	pid;

	pagesize = getpagesize();

	for (i = 0; i < 1000; i++) {
		if ((prepareFile("A", &fdA))
		    || (prepareFile("B", &fdB))
		    || (prepareFile("DelayA", &fdDelayA))
		    || (prepareFile("DelayB", &fdDelayB))
		    || (mapBuffer(&bufferA, fdDelayA, fdB))
		    || (mapBuffer(&bufferB, fdDelayB, fdA)))
			exit(1);

		pid = fork();

		if (pid == 0) {
			status = startIO(fdA, bufferA);
			exit(status);
		}
		if (pid == -1) {
			perror("fork");
			exit(1);
		}
		status = startIO(fdB, bufferB);
		if (wait(&status) == -1)
			err(1, "wait");

		close(fdA);
		close(fdB);
		close(fdDelayA);
		close(fdDelayB);
		unmapBuffer(bufferA);
		unmapBuffer(bufferB);
		unlink("A");
		unlink("B");
		unlink("DelayA");
		unlink("DelayB");
	}
	exit(status);

}
