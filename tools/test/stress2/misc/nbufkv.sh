#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Test scenario with a 20G files on two UFS2 FSs with 64k/64k
# Test program will hang (deadlock) in "nbufkv"

# Test scenario by John-Mark Gurney <jmg at funkthat dot com>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=`dirname $diskimage`
d1=$dir/diskimage1
d2=$dir/diskimage2
rm -f $d1 $d2

size=20	# G
avail=$((`sysctl -n hw.physmem` / 1024 / 1024 / 1024))
[ $((size * 2)) -gt $avail ] && size=$((avail / 2))
[ `df -k $dir | tail -1 | awk '{print $4}'` -lt \
    $((size * 2  * 1024 * 1024)) ] &&
    echo "Not enough disk space on $dir." && exit 0

odir=`pwd`

USE_TIMEOUT=1
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > nbufkv.c
mycc -o nbufkv -Wall nbufkv.c
rm -f nbufkv.c
cd $odir

u1=$mdstart
u2=$((u1 + 1))
mp1=$mntpoint
mp2=${mntpoint}2
[ -d $mp1 ] || mkdir $mp1
[ -d $mp2 ] || mkdir $mp2
dd if=/dev/zero of=$d1 bs=1m count=${size}k status=none || exit 1
cp $d1 $d2 || exit 1

mount | grep -q /dev/md$u2 && umount -f /dev/md$u2
mount | grep -q /dev/md$u1 && umount -f /dev/md$u1
[ -c /dev/md$u2 ] && mdconfig -d -u $u2
[ -c /dev/md$u1 ] && mdconfig -d -u $u1

mdconfig -a -t vnode -f $d1 -u $u1 || exit 1
newfs -b 65536 -f 65536 -O2 md$u1 > /dev/null

mdconfig -a -t vnode -f $d2 -u $u2 || exit 1
newfs -b 65536 -f 65536 -O2 md$u2 > /dev/null

mount /dev/md$u1 $mp1
mount /dev/md$u2 $mp2

/tmp/nbufkv $mp1 &
/tmp/nbufkv $mp2 &
wait

umount /dev/md$u2
umount /dev/md$u1

mount | grep -q /dev/md$u2 && umount -f /dev/md$u2
mount | grep -q /dev/md$u1 && umount -f /dev/md$u1

mdconfig -d -u $u2
mdconfig -d -u $u1

rm -rf $d1 $d2 /tmp/nbufkv
exit
EOF
#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
handler(int i) {
	fprintf(stderr, "FAIL. Timerout.\n");
	_exit(0);
}

void
work(int fd, size_t n)
{
	int i;

	for (i = 0; i < 128 * 1024; i++) {
		n = n - PAGE_SIZE;
		if (lseek(fd, n , SEEK_SET) == -1)
			err(1, "lseek()");
		if (write(fd, "1", 1) != 1)
			err(1, "write()");
	}

}

int
main(int argc, char **argv)
{

	int fd;
	off_t len;
	char path[128];

	len = 20;
	len = len * 1024 * 1024 * 1024;

	sprintf(path, "%s/nbufkv.%06d", argv[1], getpid());
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0640)) == -1)
		err(1,"open(%s)", path);
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");

	signal(SIGALRM, handler);
	alarm(1200);
	work(fd, len);

	close(fd);
	if (unlink(path) == -1)
		err(1, "unlink(%s)", path);

	return (0);
}
