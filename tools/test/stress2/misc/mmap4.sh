#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
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

# Test overcommit of the file system capacity
# Causes panic: 1 vncache entries remaining
# Fixed in r202529

# Scenario by kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap4.c
mycc -o mmap4 -Wall -O2 mmap4.c
rm -f mmap4.c

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 40m -u $mdstart

newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

/tmp/mmap4 /$mntpoint/file

cd $here
rm -f /tmp/mmap4

while mount | grep -q $mntpoint; do
	sync;sync;sync
	sleep 1
	umount $mntpoint
done

mdconfig -d -u $mdstart
exit 0
EOF
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define STARTADDR 0x0U
#define ADRSPACE  0x06400000U /* 100 Mb */

int
main(int argc, char **argv)
{
	int fd, ps;
	void *p;
	size_t len;
	volatile char *c;
	char *path;

	p = (void *)STARTADDR;
	len = ADRSPACE;

	path = argv[1];
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open()");
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return (1);
		err(1, "mmap(1)");
	}

	c = p;
	ps = getpagesize();
	for (c = p; (void *)c < p + len; c += ps) {
		*c = 1;
	}

	close(fd);

	return (0);
}
