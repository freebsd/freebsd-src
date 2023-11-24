#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Verify that mmap access updates mtime and ctime on files located on a
# tmpfs FS.
# Fixed by r277828 and r277969.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > tmpfs14.c
rm -f /tmp/tmpfs14
mycc -o tmpfs14 -Wall -Wextra -g -O2 tmpfs14.c || exit 1
rm -f tmpfs14.c
cd $odir

mount | grep -q "$mntpoint " && umount -f $mntpoint
mount -t tmpfs tmpfs $mntpoint

(cd $mntpoint; /tmp/tmpfs14) &

sleep .5
set `stat -f "%m %c" $mntpoint/test`
m1=$1
c1=$2
while pgrep -q tmpfs14; do
	set `stat -f "%m %c" $mntpoint/test`
	[ "$m1" != "$1" ] && break
	[ "$c1" != "$2" ] && break
	sleep 1
done
[ "$m1" = "$1" -o "$c1" = "$2" ] &&
    echo "FAIL Unchanged time $m1 $c1 / $1 $2"
wait

while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
rm -f /tmp/tmpfs14
exit 0
EOF

#include <sys/types.h>
#include <sys/mman.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

char *file = "test";

int
main(void)
{
	char *p;
	size_t len;
	int fd, i;

	if ((fd = open(file, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", file);
	if (write(fd, "abcdef", 7) < 0)
		err(1, "write");
	close(fd);

	len = getpagesize();
	for (i = 0; i < 10; i++) {
		sleep(1);
		if ((fd = open(file, O_RDWR | O_CREAT, 0644)) == -1)
			err(1, "open(%s)", file);

		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
			err(1, "mmap");

		p[0] = '0' + i;
		munmap(p, len);
		close(fd);
	}

	return 0;
}
