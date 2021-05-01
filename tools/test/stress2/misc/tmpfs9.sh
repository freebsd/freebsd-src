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

# Regression test: exec returning EIO problem scenario for tmpfs

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > tmpfs9.c
mycc -o tmpfs9 -Wall -Wextra -O2 tmpfs9.c
rm -f tmpfs9.c
cd $here

mount | grep $mntpoint | grep -q tmpfs && umount -f $mntpoint
mount -t tmpfs tmpfs $mntpoint
chmod 777 $mntpoint

cp /usr/bin/true $mntpoint
su $testuser -c "/tmp/tmpfs9 $mntpoint" &

while kill -0 $! 2>/dev/null; do
	../testcases/swap/swap -t 2m -i 40 -h
done
wait

while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
rm -f /tmp/tmpfs9
exit
EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N 5000
#define PARALLEL 10
const char path[] = "./true";

void test(void)
{
	pid_t p;
	int i;

	for (i = 0; i < N; i++) {
		if ((p = fork()) == 0)
			if (execl(path, path, (char *)0) == -1)
				err(1, "exec(%s)", path);
		if (p > 0)
			wait(NULL);
	}
	_exit(0);
}

int
main(int argc, char **argv)
{
	int i, status;

	if (argc != 2)
		errx(1, "Usage: %s <path>", argv[0]);
	if (chdir(argv[1]) == -1)
		err(1, "chdir(%s)", argv[1]);

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			test();
	}

	for (i = 0; i < PARALLEL; i++) {
		wait(&status);
		if (status != 0)
			return (1);
	}

	return (0);
}
