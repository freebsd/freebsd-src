#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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
# ftruncate+mmap+fsync fails for small maps
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=225586

# Original test scenario by tris_vern@hotmail.com

# Fixed in r328773:
# On pageout, in vnode generic pager, for partially dirty page, only
# clear dirty bits for completely invalid blocks.

. ../default.cfg

cat > /tmp/mmap33.c <<EOF
#include <sys/mman.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main (int argc, char *argv[])
{
	size_t i, size1, size2;
	int fd;
	char *data;
	char *filename;
	char pattern = 0x01;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s filename size1 size2\n", argv[0]);
		exit(1);
	}

	filename = argv[1];
	size1 = atoi(argv[2]);
	size2 = atoi(argv[3]);

	fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, 0644);
	for (i = 0; i < size1; i++)
		write(fd, &pattern, 1);
	close(fd);

	fd = open(filename, O_RDWR, 0644);
	if (fd == -1)
		err(1, "open(%s)", filename);
	if (ftruncate(fd, size2) == -1)
		err(1, "ftruncate()");
	data = mmap(NULL, size2, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		err(1, "mmap()");
	memset(data, 0xFF, size2);

	if (munmap(data, size2) == -1)
		err(1, "munmap");
	close(fd);

	return (0);
}
EOF
cc -o /tmp/mmap33 -Wall -Wextra -O2 -g /tmp/mmap33.c || exit 1
rm /tmp/mmap33.c

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

file=file
odir=`pwd`
cd $mntpoint
/tmp/mmap33 $file 1024 511
s=$?
sum1=`md5 < $mntpoint/$file`
[ -f mmap33.core -a $s -eq 0 ] &&
    { ls -l mmap33.core; mv mmap33.core /tmp; s=1; }
cd $odir
umount $mntpoint
mount /dev/md$mdstart $mntpoint
# This fails for truncate size < 512
sum2=`md5 < $mntpoint/$file`
[ $sum1 = $sum2 ] ||
    { s=2; echo "md5 fingerprint differs."; }
umount $mntpoint

mdconfig -d -u $mdstart
rm /tmp/mmap33
exit $s
