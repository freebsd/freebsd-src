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

# Rename(2) test with msdosfs(5)
# Test scenario by kib@

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

[ -x /sbin/mount_msdosfs ] || exit 0
dir=/tmp
odir=`pwd`
cd $dir
cat > /tmp/msdos14.c <<EOF
/* $Id: rename.c,v 1.2 2021/08/22 15:35:38 kostik Exp kostik $ */

#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	struct stat sb;
	uint64_t x;
	int error, fd;
	char from[64], to[64];

	for (x = 0;; x++) {
		snprintf(from, sizeof(from), "x.%" PRIu64 ".from", x);
		snprintf(to, sizeof(to), "x.%" PRIu64 ".to", x);

		fd = open(from, O_CREAT | O_TRUNC | O_EXCL, 0666);
		if (fd == -1)
			err(1, "open %s", from);
		close(fd);
		error = rename(from, to);
		if (error == -1)
			err(1, "rename %s %s", from, to);
		error = stat(to, &sb);
		if (error == -1)
			err(1, "stat %s", to);
		error = unlink(to);
		if (error == -1)
			err(1, "unlink %s", to);
	}
}

EOF
cc -o msdos14	 -Wall -Wextra -O2 msdos14.c || exit 1
rm -f msdos14.c
cd $odir
log=/tmp/msdos14sh..log
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

set -e
mdconfig -a -t swap -s 4g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos -b 1024 /dev/md${mdstart}$part > /dev/null
mount -t msdosfs /dev/md${mdstart}$part $mntpoint
set +e

cp /tmp/msdos14 $mntpoint
cd $mntpoint

(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -l 100) > /dev/null &
sleep 2
timeout 5m ./msdos14
while pkill swap; do :; done
wait
cd $odir

while mount | grep "$mntpoint" | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done
fsck -t msdosfs -y /dev/md${mdstart}$part > $log 2>&1
if egrep -q "BAD|INCONSISTENCY|MODIFIED" $log; then
	echo "fsck issues:"
	cat $log
	s=1

	mount -t msdosfs /dev/md${mdstart}$part $mntpoint || exit 1
	ls -lR $mntpoint
	umount $mntpoint
fi
mdconfig -d -u $mdstart
rm /tmp/msdos14 $log
exit $s
