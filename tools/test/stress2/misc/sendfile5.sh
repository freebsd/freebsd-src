#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# sendfile(2) test by kib@
# Deadlock seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
file=`basename $diskimage`
dir=`dirname $diskimage`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > sendfile5.c
mycc -o sendfile5 -Wall -Wextra -O2 sendfile5.c
rm -f sendfile5.c
need=1024
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print int($4 / 1024)}'` \
    -lt $need ] &&
    printf "Need %d MB on %s.\n" $need `dirname $diskimage` && exit 0
dd if=/dev/zero of=$diskimage bs=1m count=$need status=none
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

kldstat | grep -q tmpfs.ko || loaded=1
mount -t tmpfs tmpfs $mntpoint
echo "Testing tmpfs(5)"
cp $diskimage $mntpoint
/tmp/sendfile5 $mntpoint/$file
umount $mntpoint
[ $loaded ] && kldunload tmpfs.ko

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
echo "Testing FFS"
cp $diskimage $mntpoint
/tmp/sendfile5 $mntpoint/$file
umount $mntpoint
mdconfig -d -u $mdstart

mount -t nullfs $dir $mntpoint
echo "Testing nullfs(5)"
/tmp/sendfile5 $mntpoint/$file
umount $mntpoint

rm -f /tmp/sendfile5 $diskimage
exit
EOF
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	const char *from_name;
	char *buf;
	int sv[2];
	struct stat st;
	off_t written, pos;
	int child, error, from, status;

	if (argc != 2)
		errx(1, "Usage: %s from", argv[0]);
	from_name = argv[1];

	from = open(from_name, O_RDONLY);
	if (from == -1)
		err(1, "open read %s", from_name);

	error = fstat(from, &st);
	if (error == -1)
		err(1, "stat %s", from_name);

	error = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
	if (error == -1)
		err(1, "socketpair");

	child = fork();
	if (child == -1)
		err(1, "fork");
	else if (child != 0) {
		close(sv[1]);
		pos = 0;
		for (;;) {
			error = sendfile(from, sv[0], pos, st.st_size - pos,
			    NULL, &written, 0);
			if (error == -1) {
				if (errno != EAGAIN)
					err(1, "sendfile");
			}
			pos += written;
			if (pos == st.st_size)
				break;
		}
		close(sv[0]);
		waitpid(child, &status, 0);
	} else {
		close(sv[0]);
		buf = malloc(st.st_size);
		if (buf == NULL)
			err(1, "malloc %jd", st.st_size);
		pos = 0;
		for (;;) {
			written = 413;
			if (written > st.st_size - pos)
				written = st.st_size - pos;
#if 0
			written = st.st_size - pos;
			if (written > 1000)
				written = 1000;
			written = arc4random_uniform(written) + 1;
#endif
			error = read(sv[1], buf + pos, written);
			if (error == -1)
				err(1, "read");
			else if (error == 0)
				errx(1, "short read");
			pos += error;
			if (pos == st.st_size)
				break;
		}
		close(sv[1]);
		_exit(0);
	}

	return (0);
}
