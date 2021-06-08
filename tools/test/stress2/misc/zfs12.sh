#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Peter Holm
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

# lseek(SEEK_HOLE): finds hole
# Test scenario suggestion by: kib@
# Related to:
# Bug 256205 - ZFS: data corruption with SEEK_HOLE/SEEK_DATA on dirty files ...

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko; loaded=1; } ||
    exit 0

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > zfs12.c
mycc -o zfs12 -Wall -Wextra -O0 -g zfs12.c || exit 1
rm -f zfs12.c
cc -o /tmp/lsholes -Wall -Wextra -O2 $here/../tools/lsholes.c | exit 1

mp1=/stress2_tank/test
u1=$mdstart
u2=$((u1 + 1))

set -e
mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 2g -u $u1
mdconfig -s 2g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank md$u1 md$u2
zfs create stress2_tank/test
set +e

(cd $here/../testcases/swap; ./swap -t 2m -i 20 -l 100 -h > /dev/null) &
file1=$mp1/file		# zfs path
sleep 20
/tmp/zfs12 $file1
/tmp/lsholes $file1
s=$?
while pkill swap; do sleep 1; done
wait

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank
mdconfig -d -u $u1
mdconfig -d -u $u2
rm -f /tmp/zfs12 /tmp/lsholes
[ $loaded ] && kldunload zfs.ko
exit $s
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

#define SIZ  (500UL * 1024 * 1024)

int
main(int argc __unused, char *argv[])
{
	off_t hole;
	size_t len;
	int fd;
	char *p, *path;

	len = SIZ;

	path = argv[1];
	if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) == -1)
		err(1,"open()");
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return (1);
		err(1, "mmap(1)");
	}
	p[1 * 1024] = 1;
	p[2 * 1024] = 1;
	p[4 * 1024] = 1;

	if (msync(p, len, MS_SYNC | MS_INVALIDATE) == -1)
		err(1, "msync()");

	if ((hole = lseek(fd, 0, SEEK_HOLE)) == -1)
		err(1, "lseek(SEEK_HOLE)");
	if (hole != SIZ)
		printf("--> hole = %jd, file size=%jd\n",
		    (intmax_t)hole, (intmax_t)SIZ);
	close(fd);

	return (hole == SIZ ? 0 : 1);
}
