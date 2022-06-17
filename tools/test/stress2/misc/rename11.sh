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

# SU problem with "mkdir(d.008740.000987): Too many links" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename11.c
mycc -o rename11 -Wall -Wextra rename11.c || exit 1
rm -f rename11.c

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

[ $# -eq 1 ] && newfs_flags=$1	# Problem only seen with SU
echo newfs $newfs_flags md$mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

mkdir $mntpoint/dir
(
	cd $mntpoint/dir
	/tmp/rename11 || echo FAIL
)

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done

checkfs /dev/md$mdstart; s=$?
mdconfig -d -u $mdstart
rm -rf /tmp/rename11
exit $s
EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOOPS 10
#define PARALLEL 3
#define ND 5000

static void
check(char *name)
{
	struct stat sb;

	if (stat(name, &sb) == -1)
		warn("stat(%s)", name);
	else
		warnx("stat(%s) succeeded!", name);
}

static void
dirs(void)
{
	pid_t pid;
	int i, j;
	char name[80], to[80];

	pid = getpid();

	for (j = 0; j < LOOPS; j++) {
		for (i = 0; i < ND; i++) {
			snprintf(name, sizeof(name), "d.%06d.%06d", pid, i);
			if (mkdir(name, 0700) == -1)
				err(1, "mkdir(%s)", name);
			snprintf(to  , sizeof(to  ), "d2.%06d.%06d", pid, i);
			if (rename(name, to) == -1)
				warn("rename(%s, %s)", name, to);
			if (rename(to, name) == -1)
				warn("rename(%s, %s)", to, name);
			if (rmdir(name) == -1) {
				check(name);
				err(1, "rmdir(%s)", name);
			}
		}
	}

	_exit(0);
}

int
main(void)
{
	int i, r, s;

	r = 0;
	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			dirs();
	}

	for (i = 0; i < PARALLEL; i++) {
		wait(&s);
		r += WEXITSTATUS(s);
	}

	return (r);
}
