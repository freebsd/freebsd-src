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

# Demonstrate livelock with "umount -f" seen both with UFS and MSDOS

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

D=$diskimage
dd if=/dev/zero of=$D bs=1m count=1k status=none || exit 1

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > umountf3.c
mycc -o umountf3 -Wall umountf3.c
rm -f umountf3.c
cd $odir

mount | grep "$mntpoint" | grep md$mdstart > /dev/null && umount $mntpoint
mdconfig -l | grep md$mdstart > /dev/null &&  mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart
newfs md$mdstart > /dev/null 2>&1
mount /dev/md$mdstart $mntpoint
export RUNDIR=$mntpoint/stressX
for i in `jot 25`; do
	(cd /$mntpoint; /tmp/umountf3) &
done
sleep $((4 * 60))
echo "umount -f $mntpoint"
umount -f $mntpoint
while pkill umountf3; do :; done
wait
mdconfig -d -u $mdstart
rm -f $D /tmp/umountf3
exit
EOF

#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned long size = 1024 * 1024 * 2;

int
main(int argc, char **argv)
{
	int buf[1024], index, to, n;
	int fd;
	char file[128];

	sprintf(file,"p%06d", getpid());
	if ((fd = open(file, O_CREAT | O_RDWR, 0666)) == -1)
		err(1, "creat(%s)", file);

	to = sizeof(buf);
	for (;;) {
		index = 0;
		while (index < size) {
			if (index + to > size)
				to = size - index;
			index += to;
			if (write(fd, buf, to) != to)
				err(1, "write(%s), %s:%d", file, __FILE__, __LINE__);
		}

		if (lseek(fd, 0, 0) == -1)
			err(1, "lseek");
		index = 0;
		while (index < size) {
			if (index + to > size)
				to = size - index;
			if ((n = read(fd, buf, to)) != to)
				err(1, "rw read(%d, %d, %d). %s.%d", n, to, index,  __FILE__, __LINE__);
			index += to;
		}
		if (lseek(fd, 0, 0) == -1)
			err(1, "lseek");
	}

	if (close(fd) == -1)
		err(1, "close(%s), %s:%d", file, __FILE__, __LINE__);
	if (unlink(file) == -1)
		err(1, "unlink(%s), %s:%d", file, __FILE__, __LINE__);
	return (0);
}
