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

# Looping thread seen:
# https://people.freebsd.org/~pho/stress/log/kostik855.txt
# Fixed by r293197.

# Again on i386:
# https://people.freebsd.org/~pho/stress/log/kostik867.txt
# Fixed by r295716.

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > posix_fadvise3.c
mycc -o posix_fadvise3 -Wall -Wextra -O2 -g posix_fadvise3.c

data=/tmp/posix_fadvise3.data
dd if=/dev/zero of=$data bs=1m count=64 status=none
/tmp/posix_fadvise3

rm $data
truncate -s 64m $data
/tmp/posix_fadvise3

rm -f /tmp/posix_fadvise3 posix_fadvise3.c $data
exit
EOF
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LOOPS 10000
#define N (128 * 1024 / (int)sizeof(u_int32_t))

u_int32_t r[N];

unsigned long
makearg(void)
{
	unsigned int i;
	unsigned long val;

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

int
main(void)
{
	off_t len, offset;
	int advise, fd, i, j;

	if ((fd = open("/tmp/posix_fadvise3.data", O_RDONLY)) == -1)
		err(1, "open()");
	offset = 0;
	len = 0x7fffffffffffffff;
	advise = 4;
	if (posix_fadvise(fd, offset, len, advise) == -1)
		warn("posix_fadvise");
	close(fd);

	for (i = 0; i < LOOPS; i++) {
		for (j = 0; j < N; j++)
			r[j] = arc4random();
		if ((fd = open("/tmp/posix_fadvise3.data", O_RDONLY)) == -1)
			err(1, "open()");
		offset = makearg();
		len = makearg();
		advise = arc4random() % 6;
		if (posix_fadvise(fd, offset, len, advise) == -1)
			warn("posix_fadvise");
		close(fd);
	}

	return (0);
}
