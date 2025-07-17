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

# Scenario that causes "panic: brelse: free buffer onto another queue???"
# Idea for scenario by kib@. Fixed in r203818

# When UFS partition is full, then some high load causes
# panic: brelse: free buffer onto another queue???

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > fragments.c
rm -f /tmp/fragments
mycc -o fragments -Wall -Wextra -O2 -g fragments.c
rm -f fragments.c
cd $here

mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags -m 0 md$mdstart > /dev/null 2>&1
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

cd $mntpoint
su $testuser -c "/tmp/fragments"
cd $here

umount $mntpoint
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

rm -f /tmp/fragments
exit
EOF
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOOPS 600
#define PARALLEL 8

static	pid_t pid;
static	char *buf;

static volatile sig_atomic_t stop;

void
handler(int i __unused) {
	stop = 1;
}

void
cleanup(int n)
{
	int i, j, start;
	int nb = 0;
	char file[128];
	struct statfs sbuf;
	struct stat sb;

	if (n == -1) {
		for (i = 0; i < LOOPS; i++) {
			sprintf(file,"t%05d", i);
			unlink(file);
		}
		return;
	}

	start = arc4random() % n;
	for (i = 0; i < LOOPS; i++) {
		j = (start + i) % LOOPS;
		sprintf(file,"t%05d", j);
		if (stat(file, &sb) != 0)
			continue;

		if (sb.st_size == 0) {
			unlink(file);
			continue;
		}
		if (truncate(file, 0) == 0) {
			nb++;
			continue;
		}
		if (nb > 10)
			break;
	}

	for (i = 0; i < 10; i++) {
		if (statfs(".", &sbuf) < 0)
			err(1, "statfs(%s)", ".");

		if (sbuf.f_bfree > 8)
			return;
	}

	for (i = 0; i < LOOPS; i++) {
		j = (start + i) % LOOPS;
		sprintf(file,"t%05d", j);
		if (unlink(file) == 0) {
			return;
		}
	}
}

void
fragments(void)
{
	int i, len;
	char file[128];
	int fd;

	for (i = 0;; i++) {
		sprintf(file,"d%d/f%05d.%05d", i/1000, pid, i);

		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0) {
			if (errno != ENOSPC)
				warn("open(%s)", file);
			break;
		}

		len =  2 * 1024;
		if (write(fd, buf, len) != len) {
		}

		close(fd);
	}
}

void
blocks(void)
{
	int i, len;
	char file[128];
	int fd;

	for (i = 0;; i++) {
		sprintf(file,"d%d/b%05d.%05d", i/1000, pid, i);

		if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0) {
			if (errno != ENOSPC)
				warn("open(%s)", file);
			break;
		}

		len =  16 * 1024;
		if (write(fd, buf, len) != len) {
		}

		close(fd);
	}
}

void
setup(void)
{
	int i;
	char file[128];

	for (i = 0; i < 300; i++) {
		sprintf(file,"d%d", i);
		if (mkdir(file, 0700) == -1)
			warn("mkdir(%s)", file);
	}

	blocks();
	fragments();

	for (i = 0;i < 8; i++) {
		sprintf(file,"d%d/b%05d.%05d", i/1000, pid, i);
		unlink(file);
	}
	for (i = 0;i < 1; i++) {
		sprintf(file,"d%d/f%05d.%05d", i/1000, pid, i);
		unlink(file);
	}

}

int
test(void)
{
	int i, len, n;
	char file[128];
	int fd;

	for (i = 0; i < LOOPS; i++) {
		sprintf(file,"t%05d", i);

		if ((fd = open(file, O_RDWR | O_CREAT | O_EXCL, 0600)) < 0) {
			continue;
		}
//		n = arc4random() % (12 + 1);
		n = 0;
                len = (arc4random() % (16 * 1024) + 1) + n * 16;
		while (len > 0) {
			if (write(fd, buf, len) == len)
				break;
			len = len / 2;
			usleep(1000);
		}
		close(fd);
		if (len == 0) {
			cleanup(i);
		}
	}

	exit(0);

	return (0);
}

int
main()
{
	int i, j, status;

	pid = getpid();
	if ((buf = malloc(12 * 16 * 1024)) == NULL)
		err(1, "malloc()");

	setup();
	signal(SIGALRM, handler);
	alarm(30 * 60);
	for (j = 0; j < 50 && stop == 0; j++) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				test();
		}
		for (i = 0; i < PARALLEL; i++)
			wait(&status);
		cleanup(-1);
	}
        return (0);
}
