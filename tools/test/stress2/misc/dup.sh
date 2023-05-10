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

# Test scenario for "D20947: Check and avoid overflow when incrementing
# fp->f_count in fget_unlocked() and fhold()".

# OOM killing seen. Cap files and procs for now.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
rm -f /tmp/dup /tmp/dup.c || exit 1
sed '1,/^EOF/d' < $odir/$0 > $dir/dup.c
mycc -o dup  -Wall -Wextra dup.c || exit 1
rm -f dup.c
cd $odir

/tmp/dup; s=$?

rm -f /tmp/dup
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static volatile u_int *share;
#define N 10240
#define CAP_FILES 50000
#define CAP_PROCS 1000
#define SYNC 0

int
main(void)
{
	struct stat st;
	pid_t pid[N];
	size_t len;
	int fd, fd2, i, j, last;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");
	if ((fd = open("/dev/zero", O_RDONLY)) == -1)
		err(1, "open(/dev/zero)");
	i = 0;
	for (;;) {
		if ((fd2 = dup(fd)) == -1) {
			if (errno == EMFILE)
				break;
			err(1, "dup()");
		}
		last = fd2;
		if (++i == CAP_FILES)
			break;
	}
#if defined(DEBUG)
	fprintf(stderr, "i = %d\n", i);
#endif

	for (i = 0; i < N; i++) {
		if ((pid[i] = fork()) == 0) {
			if (fstat(last, &st) == -1)
				err(1, "stat(%s)", "/dev/zero");
			while(share[SYNC] == 0)
				usleep(100000);
			_exit(0);
		}
		if (pid[i] == -1) {
			warn("fork()");
			i--;
			break;
		}
		if (i + 1 == CAP_PROCS)
			break;
	}
	share[SYNC] = 1;
	for (j = 0; j < i; j++) {
		if (waitpid(pid[j], NULL, 0) != pid[j])
			err(1, "waitpid(%d), index %d", pid[j], j);
	}

	return (0);
}
