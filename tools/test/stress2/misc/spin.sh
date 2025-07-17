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

# Demonstrate starvation: Thread stuck in "ufs" for minutes.
# Only seen with >= 16 CPUs.
# Not a problem with 4BSD.
# http://people.freebsd.org/~pho/stress/log/spin.txt
# Fixed by r273966.

. ../default.cfg

timeout=1200

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > spin.c
mycc -o spin -Wall -Wextra -O0 spin.c || exit 1
rm -f spin.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cpus=`sysctl hw.ncpu | sed 's/.*: //'`
(cd $mntpoint; /tmp/spin $((cpus + 1))) &
error=0
n=0
while kill -0 $! 2>/dev/null; do
	sleep 1
	if [ $((n += 1)) -gt $timeout ]; then
		echo FAIL
		ps -l | grep -v sed | sed -n '1p;/ufs/p'
		pkill spin
		error=1
	fi
done
wait

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/spin
exit $error
EOF
#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
work(void)
{

	while (access("rendezvous", R_OK) != 0)
		;

	_exit(0);
}

int
main(int argc, char **argv)
{
	int fd, i, parallel;

	if (argc == 2)
		parallel = atoi(argv[1]);
	else
		errx(1, "Usage: %s <cpus>", argv[0]);

	for (i = 0; i < parallel; i++) {
		if (fork() == 0)
			work();
	}

	/* open(2) blocked on "ufs" for minutes */
	if ((fd = open("rendezvous", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(fd);

	for (i = 0; i < parallel; i++)
		wait(NULL);

	if (unlink("rendezvous") == -1)
		err(1, "unlink()");

	return (0);
}
