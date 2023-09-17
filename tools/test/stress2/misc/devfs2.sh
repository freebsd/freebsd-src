#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Stopped at      devfs_open+0x23f:       pushl   0x14(%ebx)
# db> where
# Tracing pid 46017 tid 100350 td 0xc4c08510
# devfs_open(e6d06a10) at devfs_open+0x23f
# VOP_OPEN_APV(c09edda0,e6d06a10) at VOP_OPEN_APV+0x9b
# vn_open_cred(e6d06b78,e6d06c78,0,c4883900,3,...) at vn_open_cred+0x41e
# vn_open(e6d06b78,e6d06c78,0,3) at vn_open+0x1e
# kern_open(c4c08510,8048887,0,1,0,...) at kern_open+0xb7
# open(c4c08510,e6d06d00) at open+0x18
# syscall(e6d06d38) at syscall+0x252

# Test scenario by kib@freebsd.org

. ../default.cfg

odir=`pwd`
dir=/tmp

cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/devfs2.c
mycc -o devfs2 -Wall devfs2.c -lthr || exit 1
rm -f devfs2.c

./devfs2

rm devfs2
exit 0

EOF
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>

void *
thr1(void *arg)
{
	int fd;
	int i;

	for (i = 0; i < 1024; i++) {
		if ((fd = open("/dev/zero", O_RDONLY)) == -1)
			perror("open /dev/zero");
		close(fd);
	}
	return (0);
}

void *
thr2(void *arg)
{
	int i;
	for (i = 0; i < 1024; i++)
		close(3);
	return (0);
}

int
main()
{
	pthread_t threads[2];
	int i;
	int r;

	if ((r = pthread_create(&threads[0], NULL, thr1, 0)) != 0)
		err(1, "pthread_create(): %s\n", strerror(r));
	if ((r = pthread_create(&threads[1], NULL, thr2, 0)) != 0)
		err(1, "pthread_create(): %s\n", strerror(r));

	for (i = 0; i < 2; i++)
		if ((r = pthread_join(threads[i], NULL)) != 0)
			errc(1, r, "pthread_join(%d)", i);

	return (0);
}
