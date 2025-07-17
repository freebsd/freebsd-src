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

# Problem seen with atomic assignment of f_offset. Fixed in r238029.

# Test scenario by kib@

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > f_offset.c
mycc -o f_offset -Wall -Wextra -O2 f_offset.c -lpthread
rm -f f_offset.c

/tmp/f_offset

rm -f /tmp/f_offset
exit 0
EOF
/*
   Description by kib:
To really exercise the race conditions, all the following items must
be fulfilled simultaneously:
1. you use 32bit host, i.e. i386
2. you operate on the file offsets larger than 4GB (but see below)
3. there are several threads or processes that operate on the same
   file descriptor simultaneously.

Please note that the normal fork(2) causes file descriptor table
copy, so only rfork(2) call with RFFDG flag unset causes sharing. Or,
multi-threading can be used.
 */

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int errors, fd;
char file[128];

#define START 0x100000000ULL
#define N 1000000

void *
t1(void *arg __unused)
{
	int i;
	off_t offset;

	offset = START + 2;

	for (i = 0; i < N; i++) {
		if (lseek(fd, offset, SEEK_SET) == -1)
			err(1, "lseek error");
	}

	return (0);
}

void *
t2(void *arg __unused)
{
	int i;
	off_t offset;

	offset = 1;

	for (i = 0; i < N; i++) {
		if (lseek(fd, offset, SEEK_SET) == -1)
			err(1, "lseek error");
	}
	return (0);
}
void *
t3(void *arg __unused)
{
	int i;
	off_t offset;

	offset = 1;

	for (i = 0; i < N; i++) {
		if ((offset = lseek(fd, 0, SEEK_CUR)) == -1)
			err(1, "lseek error");
		if (offset != 1 && offset != START + 2)
			fprintf(stderr, "FAIL #%d offset = %10jd (0x%09jx)\n",
					errors++, offset, offset);
	}

	return (0);
}

int
main(void)
{
	pthread_t threads[3];
	int r;
	int i;
	off_t offset;

	snprintf(file, sizeof(file), "file.%06d", getpid());
	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
		err(1, "%s", file);

	offset = 1;
	if (lseek(fd, offset, SEEK_SET) == -1)
		err(1, "lseek error");

	for (i = 0; i < 20 && errors < 10; i++) {
		if ((r = pthread_create(&threads[0], NULL, t1, 0)) != 0)
			errc(1, r, "pthread_create()");
		if ((r = pthread_create(&threads[1], NULL, t2, 0)) != 0)
			errc(1, r, "pthread_create()");
		if ((r = pthread_create(&threads[2], NULL, t3, 0)) != 0)
			errc(1, r, "pthread_create()");

		if ((r = pthread_join(threads[0], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", 0);
		if ((r = pthread_join(threads[1], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", 1);
		if ((r = pthread_join(threads[2], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", 2);
	}
	close(fd);
	if (unlink(file) == -1)
		err(3, "unlink(%s)", file);

	return (0);
}
