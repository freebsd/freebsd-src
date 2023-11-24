#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

# Variation of newfs4.sh, using a swap backed MD disk

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > newfs5.c
mycc -o newfs5 -Wall -Wextra newfs5.c
rm -f newfs5.c
cd $odir

mount | grep "$mntpoint" | grep md$mdstart > /dev/null && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

blocksize="-b 65536"
opt="-O2 -U"
size=9	# Gb
mdconfig -a -t swap -s ${size}g -u $mdstart
newfs $blocksize $opt md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cd $mntpoint
truncate -s 2g f1
truncate -s 2g f2
truncate -s 2g f3
truncate -s 2g f4
/tmp/newfs5 f1 &
/tmp/newfs5 f2 &
/tmp/newfs5 f3 &
/tmp/newfs5 f4 &
wait

while mount | grep "$mntpoint" | grep -q md$mdstart; do
	umount -f $mntpoint || sleep 1
done
checkfs /dev/md$mdstart; s=$?

mdconfig -d -u $mdstart
rm -f $diskimage
rm -f /tmp/newfs5
exit $s

EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Perform random IO operations on a file */

int
main(int argc, char **argv)
{
	struct stat sb;
	char buf[256];
	off_t bp, maxb;
	int fd;
	long i;

	if (argc != 2) {
		fprintf(stderr, "Usage %s: file\n", argv[0]);
		return (1);
	}
	if ((fd = open(argv[1], O_RDWR)) == -1)
		err(1, "open(%s)", argv[1]);
	if (fstat(fd, &sb) == -1)
		err(1, "fstatf(stdin)");
	maxb = sb.st_size - sizeof(buf);

	for (i = 0; i < 10000; i++) {
		bp = arc4random();
		bp = (bp << 31 | arc4random()) % maxb;

		if (lseek(fd, bp, 0) == -1)
			err(1, "lseek()");
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write()");
	}
	close(fd);

	return (0);
}
