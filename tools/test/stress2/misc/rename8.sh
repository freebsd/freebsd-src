#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Cache inconsistency seen on "to" file for rename(2).

# Scenario by jhb@

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > rename8.c
mycc -o rename8 -Wall -Wextra -O2 rename8.c
rm -f rename8.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "cd $mntpoint; mkdir r; /tmp/rename8 r"
ls -li $mntpoint/r | egrep -v "^total"

for i in `jot 10`; do
        mount | grep -q md$mdstart  && \
                umount $mntpoint && mdconfig -d -u $mdstart && break
	sleep 1
done
if mount | grep -q md$mdstart; then
	fuser $mntpoint
        echo "umount $mntpoint failed"
        exit 1
fi

mdconfig -l | grep -q md$mdstart &&
	mdconfig -d -u $mdstart
rm -f /tmp/rename8
exit
EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static char *always, *file1, *file2;
static ino_t always_ino;

static void
usage(void)
{
	fprintf(stderr, "Usage: rename_race <dir>\n");
	exit(1);
}

static void
child(void)
{
	struct stat sb;

	/* Exit as soon as our parent exits. */
	while (getppid() != 1) {
		stat(file1, &sb);
	}
	exit(0);
}

static void
create_file(const char *path)
{
	int fd;

	fd = open(path, O_CREAT, 0666);
	if (fd < 0)
		err(1, "open(%s)", path);
	close(fd);
}

int
main(int ac, char **av)
{
	struct stat sb, sb2;
	pid_t pid;
	int i, r;

	if (ac != 2)
		usage();
	if (stat(av[1], &sb) != 0)
		err(1, "stat(%s)", av[1]);
	if (!S_ISDIR(sb.st_mode))
		errx(1, "%s not a directory", av[1]);

	asprintf(&always, "%s/file.always", av[1]);
	asprintf(&file1, "%s/file1", av[1]);
	asprintf(&file2, "%s/file2", av[1]);

	create_file(always);
	if (stat(always, &sb) != 0)
		err(1, "stat(%s)", always);
	always_ino = sb.st_ino;

	pid = fork();
	if (pid < 0)
		err(1, "fork");
	if (pid == 0)
		child();
	r = 0;
	for (i = 0; i < 100000; i++) {
		if (unlink(file1) < 0 && errno != ENOENT)
			err(1, "unlink(%s)", file1);
		if (link(always, file1) < 0)
			err(1, "link(%s, %s)", always, file1);
		create_file(file2);
		if (stat(file2, &sb2) < 0)
			err(1, "stat(%s)", file2);
		if (rename(file2, file1) < 0)
			err(1, "rename(%s, %s)", file2, file1);
		if (stat(file1, &sb) < 0)
			err(1, "stat(%s)", file1);
		if (sb.st_ino != sb2.st_ino ||
		    sb.st_ino == always_ino) {
			printf("FAIL. Bad stat: always: %ju file1: %ju (should be %ju)\n",
			    (uintmax_t)always_ino, (uintmax_t)sb.st_ino,
			    (uintmax_t)sb2.st_ino);
			r = EXIT_FAILURE;
			break;
		}
	}
	kill(pid, SIGINT);
	wait(NULL);
	if (r == 0) {
		unlink(always);
		unlink(file1);
		unlink(file2);
	}
	return (r);
}
