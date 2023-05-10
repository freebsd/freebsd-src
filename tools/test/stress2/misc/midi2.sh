#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Test threaded access to /dev/midistat.

# "panic: vm_fault_hold: fault on nofault entry, addr: 0x8b352000" seen.
# https://people.freebsd.org/~pho/stress/log/mark089.txt
# Fixed by 351262

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/midi2.c
mycc -o midi2 -Wall -Wextra -O0 -g midi2.c -lpthread || exit 1

$dir/midi2
s=$?
cat /dev/midistat > /dev/null || s=1

rm -rf midi2 midi2.c midi2.core
exit $s

EOF
#include <sys/param.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int share;
static int fd;
static char buf[0xbc59], buf2[0x40eddf];

#define THREADS 4
#define RUNTIME (1 * 60)
#define SYNC 0

static void *
t1(void *data __unused)
{

	atomic_add_int(&share, 1);
	while (share != THREADS)
		;

        pread(fd, buf, 0xbc59, 0x27dbb298LL);
        pread(fd, buf, 0xbc59, 0x104e35c6d22eLL);
        pread(fd, buf2, 0x40eddf, 0x405d1df88cbf41LL);

	return (NULL);
}

int
main(void)
{
	pthread_t tid[THREADS];
	time_t start;
	int i, rc;

	if ((fd = open("/dev/midistat", O_RDONLY)) == -1)
		err(1, "open(/dev/midistat)");
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		share = 0;
		for (i = 0; i < THREADS; i++) {
			if ((rc = pthread_create(&tid[i], NULL, t1, NULL)) !=
			    0)
				errc(1, rc, "pthread_create");
		}

		for (i = 0; i < THREADS; i++) {
			if ((rc = pthread_join(tid[i], NULL)) != 0)
				errc(1, rc, "pthread_join");
		}
	}

	return (0);
}
