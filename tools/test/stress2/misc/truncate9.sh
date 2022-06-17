#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

# Truncate test scenario. No problems seen.

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > truncate9.c
mycc -o truncate9 -Wall -Wextra truncate9.c || exit 1
rm -f truncate9.c
set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -l 100) > /dev/null &
cd $mntpoint
for i in `jot 5`; do
	/tmp/truncate9 &
	pids="$pids $!"
done
for i in $pids; do
	wait $i
done
while pkill swap; do :; done
wait
cd $odir

umount $mntpoint
mdconfig -d -u $mdstart
rm /tmp/truncate9
exit $s
EOF
#include <sys/mman.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int
main (void)
{
	off_t opos, pos;
	time_t start;
	int fd, i;
	char file[80], *cp;

	snprintf(file, sizeof(file), "file.%d", getpid());
	start = time(NULL);
	if ((fd = open(file, O_RDWR | O_TRUNC | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", file);
	close(fd);
	while (time(NULL) - start < 120) {
		if ((fd = open(file, O_RDWR)) == -1)
			err(1, "open(%s)", file);
		pos = arc4random();
		opos = pos = (pos << 12) | arc4random();
		if (ftruncate(fd, pos) == -1)
			err(1, "ftruncate()");
		if ((cp = mmap(NULL, pos, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
			err(1, "mmap");
		cp[0] = 1;
		cp[pos - 1] = 2;
		for (i = 0; i < 10; i++) {
			while (pos >= opos) {
				pos = arc4random();
				pos = (pos << 12) | arc4random();
			}
			if (ftruncate(fd, pos) == -1)
				err(1, "ftruncate()");
			cp[0] = 1;
			cp[pos - 1] = 2;
		}

		if (munmap(cp, opos) == -1)
			err(1, "munmap");
		close(fd);
	}
	unlink(file);

	return (0);
}
