#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# Test scenario by Mark Johnston <markj@FreeBSD.org>
# "physwr DL /tmp/graid1_2 /dev/mirror/test" seen.
# Fixed by r307691.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/graid1_2.c
mycc -o graid1_2 -Wall -Wextra -O0 -g graid1_2.c || exit 1
rm -f graid1_2.c
cd $odir

gmirror load > /dev/null 2>&1 && unload=1
[ -c /dev/mirror/test ] && { gmirror stop test; gmirror destroy test; }
old=`sysctl -n kern.geom.mirror.debug`
sysctl kern.geom.mirror.debug=-1 | grep -q -- -1 ||
    sysctl kern.geom.mirror.debug=$old > /dev/null

md1=$mdstart
md2=$((mdstart + 1))
s=0
size=$((128 * 1024))

for u in $md1 $md2; do
	dd if=/dev/zero of=/tmp/graid1_2_di$u bs=$size count=1 status=none
	[ -c /dev/md$u ] && mdconfig -d -u $u
	mdconfig -a -t vnode -f /tmp/graid1_2_di$u -u $u
done
gmirror label test /dev/md$md1 /dev/md$md2 || exit 1
[ "`sysctl -in kern.geom.mirror.launch_mirror_before_timeout`" = "0" ] &&
    sleep $((`sysctl -n kern.geom.mirror.timeout` + 1))
[ -c /dev/mirror/test ] || exit 1

for i in `jot 150`; do /tmp/graid1_2 /dev/mirror/test; done &

sleep 5
start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 300 ]; do
	gmirror rebuild test /dev/md$md1
	sleep 2
	n=0
	while ps -lx | grep -v grep | grep graid1_2 | grep -q D; do
		opid=$pid
		pid=`pgrep graid1_2`
		[ -z "$pid" -o "$pid" != "$opid" ] && n=0
		sleep 1
		n=$((n + 1))
		if [ $n -gt 180 ]; then
			echo FAIL
			ps -lx | grep -v grep | grep graid1_2 | grep D
			exit 1
		fi
	done
done
kill $! 2>/dev/null
pkill graid1_2
wait

while mount | grep $mntpoint | grep -q /mirror/; do
	umount $mntpoint || sleep 1
done
gmirror stop test || s=2
[ $unload ] && gmirror unload

for u in $md2 $md1; do
	mdconfig -d -u $u || s=4
done
rm -f /tmp/graid1_2 /tmp/graid1_2_di*
exit $s
EOF
/* Write last sector on disk */
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static char buf[512];

int
main(int argc __unused, char *argv[])
{
	time_t start;
	int fd;

	if ((fd = open(argv[1], O_RDWR)) == -1)
		err(1, "open(%s)", argv[1]);
	start = time(NULL);
	while (time(NULL) - start < 2) {
			if (lseek(fd, 254 * sizeof(buf), SEEK_SET) == -1)
			err(1, "seek");
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write");
	}
	close(fd);

	return (0);
}
