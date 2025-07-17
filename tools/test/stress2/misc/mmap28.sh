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

# 'panic: vnode_pager_generic_getpages: page 0xc3350b0c offset beyond vp
# 0xcc187000 size' seen.
# https://people.freebsd.org/~pho/stress/log/mmap28.txt
# This was introduced by r292373.
#
# A page fault is seen on a non INVARIANTS kernel w/ r292373,
# whereas this test runs as expected on r292372.
# https://people.freebsd.org/~pho/stress/log/mmap28-2.txt
# https://people.freebsd.org/~pho/stress/log/mmap28-3.txt
# To repeat, run this test with "sysctl vfs.ffs.use_buf_pager=0".
# Fixed by r307626

# Test scenario refinement by kib@

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mmap28.c
mycc -o mmap28 -Wall -Wextra -g -O0 mmap28.c || exit 1
rm -f mmap28.c
cd $odir

(cd /tmp; ./mmap28)

rm -f /tmp/mmap28 /tmp/mmap28.0* /tmp/mmap28.core
exit 0

EOF
#include <sys/param.h>
#include <sys/mman.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ADRSPACE  (256 * 1024 * 1024 )
#define STARTADDR 0x50000000U

static void
work(void)
{
	size_t indx, left, len;
	int fd, rfd;
	int i;
	char *p;
	char path[128];
	volatile char val __unused;

	if ((rfd = open("/dev/random", O_RDONLY)) == -1)
		err(1, "open(/dev/random)");

	snprintf(path, sizeof(path), "/tmp/mmap28.%06d", 0);
	if ((fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1,"open(%s)", path);

	p = (void *)STARTADDR + trunc_page(arc4random() % ADRSPACE);
	left = ADRSPACE - (size_t)p + STARTADDR;
	len = trunc_page(arc4random() % left) + PAGE_SIZE;

	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");

	if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return;
		err(1, "mmap()");
	}

	/*
	   Truncating the mapped file triggers a panic when accessed beyond
	   EOF.
	 */
	if (ftruncate(fd, len / 2) == -1)
		err(1, "ftruncate(%s)", path);

	for (i = 0; i < 1000; i++) {
		if (read(rfd, &indx, sizeof(indx)) != sizeof(indx))
			err(1, "read(random)");
		val = p[indx % len];
	}
	close(rfd);

	if (munmap(p, len) == -1)
		err(1, "munmap(%p)", p);
	close(fd);
	unlink(path);
}

int
main(void)
{

	work();

	return (0);
}
