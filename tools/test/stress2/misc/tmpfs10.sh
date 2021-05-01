#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# tmpfs(5) name lookup problem seen:

# $ ./tmpfs10.sh
# tmpfs10: unlink(p01193.14729) at loop #2: No such file or directory
# unlink(p01193.14729) succeeded at call #2.
# tmpfs10: unlink(p01186.14409) at loop #2: No such file or directory
# unlink(p01186.14409) succeeded at call #2.
# FAIL
# $

# Fixed in r253967.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > tmpfs10.c
mycc -o tmpfs10 -Wall -Wextra -O2 -g tmpfs10.c || exit 1
rm -f tmpfs10.c
cd $odir

mount | grep -q "on $mntpoint " && umount $mntpoint
mount -t tmpfs tmpfs $mntpoint
cd $mntpoint
/tmp/tmpfs10 ||
{ find $mntpoint ! -type d | xargs ls -il; echo FAIL; }
cd $odir
umount $mntpoint
rm -f /tmp/tmpfs10
exit 0
EOF
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int loop;
int error;

#define PARALLEL 20
#define SIZE 16000

void
test2(void)
{
	int i, j, k;
	pid_t pid;
	char file[128], dir[128];

	loop++;
	pid = getpid();
	sprintf(dir,"%s.%05d", getprogname(), pid);
	if (mkdir(dir, 0770) < 0)
		err(1, "mkdir(%s), %s:%d", dir, __FILE__, __LINE__);

	if (chdir(dir) == -1)
		err(1, "chdir(%s), %s:%d", dir, __FILE__, __LINE__);

	for (j = 0; j < SIZE; j++) {
		sprintf(file,"p%05d.%05d", pid, j);
		if (symlink("/tmp/not/there", file) == -1) {
			if (errno != EINTR) {
				warn("symlink(%s). %s.%d", file, __FILE__, __LINE__);
				break;
			}
		}
	}

	for (i = --j; i >= 0; i--) {
		sprintf(file,"p%05d.%05d", pid, i);
		if (unlink(file) == -1) {
			warn("unlink(%s) at loop #%d", file, loop);
			error++;
			for (k = 0; k < 10; k++) {
				usleep(10000);
				if (unlink(file) == 0) {
					fprintf(stderr,
					    "unlink(%s) succeeded at call #%d.\n",
					    file, k + 2);
					break;
				}
			}
		}
	}

	(void)chdir("..");
	if (rmdir(dir) == -1)
		warn("rmdir(%s), %s:%d", dir, __FILE__, __LINE__);

}

void
test(void)
{
	sleep(arc4random() % 3 + 1);

	test2();
	if (error == 0)
		test2();

	_exit(error);
}

int
main(void)
{
	int e, i, status;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			test();

	e = 0;
	for (i = 0; i < PARALLEL; i++) {
		wait(&status);
		e += WEXITSTATUS(status);
	}

	return (e);
}
