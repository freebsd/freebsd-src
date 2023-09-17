#!/bin/sh

#
# Copyright (c) 2010 Peter Holm <pho@FreeBSD.org>
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

# "panic: handle_workitem_freeblocks: inode 447489 block count 336" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart

newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > truncate5.c
mycc -o truncate5 -Wall -Wextra -O2 truncate5.c
rm -f truncate5.c

export RUNDIR=$mntpoint/stressX
[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR

/tmp/truncate5

cd $here
rm -f /tmp/truncate5

while mount | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done

checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
exit $s
EOF
#include <fcntl.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define SIZ 4096
char buf[SIZ];

int
main()
{
	int fd, i, j;
	char name[128];
	off_t len = 21474837416LL;
	off_t pos;

	srand48(getpid());
	sprintf(name, "%05d.%05d", getpid(), 0);
	fd = open(name, O_WRONLY | O_CREAT, 0666);
	if (ftruncate(fd, len) == -1)
		err(1, "ftruncate");
	unlink(name);
	sleep(1);

	for (i = 0; i < 100; i++) {
		for (j = 0; j < 100; j++) {
			pos = lrand48() % (len - sizeof(buf));
			if (lseek(fd, pos, SEEK_SET) == -1)
				err(1, "lseek");
			if (write(fd, buf, sizeof(buf)) != sizeof(buf))
				err(1, "write");
		}
	}
	close(fd);

	return (0);
}
