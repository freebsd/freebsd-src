#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Simple O_EXLOCK test scenario.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > exlock.c
mycc -o exlock -Wall -Wextra exlock.c || exit 1
rm -f exlock.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "cd $mntpoint; /tmp/exlock"

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/exlock
exit
EOF
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

char buf[128];

#define PARALLEL 3

static void
tst(char *file, int n)
{
	int fd, i;

	for (i = 0; i < (int)sizeof(buf); i++)
		buf[i] = '0' + n;

	for (i = 0; i < 1024 * 1024; i++) {
		if ((fd = open(file, O_RDWR | O_CREAT | O_APPEND | O_EXLOCK,
		    0644)) == -1)
			err(1, "open(%s)", file);
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write");
		close(fd);
	}

	_exit(0);
}
static void
test(void)
{
	int i;
	char file[80];

	snprintf(file, sizeof(file), "f06%d", getpid());

	for (i = 0; i < 3; i++)
		if (fork() == 0)
			tst(file, i);
	for (i = 0; i < 3; i++)
		wait(NULL);

	unlink(file);

	_exit(0);
}

int
main(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			test();
	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}
