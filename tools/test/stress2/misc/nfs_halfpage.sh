#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Konstantin Belousov <kib@FreeBSD.org>
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

# Regression test for:
# Mark pages after EOF as clean after pageout.
# https://reviews.freebsd.org/D11697
# Committed as r321580 + r321581.

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/nfs_halfpage.c
mycc -o nfs_halfpage -Wall -Wextra -O0 -g nfs_halfpage.c || exit 1
rm -f nfs_halfpage.c
cd $odir

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

mount | grep "$mntpoint" | grep -q nfs && umount $mntpoint
mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw $nfs_export $mntpoint

file=$mntpoint/nfs_halfpage.file
/tmp/nfs_halfpage $file

echo "Reboot now to trigger syncing disks loop."
sleep 60

rm $file
for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && s=1 || s=0
rm -f /tmp/nfs_halfpage

exit $s
EOF
/* $Id: nfs_halfpage.c,v 1.2 2017/07/23 09:36:23 kostik Exp kostik $ */

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <err.h>
#include <unistd.h>

int
main(int argc __unused, char *argv[])
{
	char *m;
	int error, fd, pgsz, sz;

	pgsz = getpagesize();
	fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fd == -1)
		err(1, "open %s", argv[1]);
	sz = pgsz / 4;
	error = lseek(fd, sz, SEEK_SET);
	if (error == -1)
		err(1, "lseek");
	error = write(fd, "a", 1);
	if (error == -1)
		err(1, "write");
	else if (error != 1)
		errx(1, "short write");
	m = mmap(NULL, sz + 1, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (m == MAP_FAILED)
		err(1, "mmap");
	m[0] = 'x';
}
