#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Test case for vfs.lookup_shared=1 that shows possible name cache
# inconsistency:

# $ ls -l /tmp/file.05015?
# ls: /tmp/file.050150: No such file or directory
# $ fsdb -r /dev/ad4s1e
# ** /dev/ad4s1e (NO WRITE)
# Examining file system `/dev/ad4s1e'
# Last Mounted on /tmp
# current inode: directory
# I=2 MODE=41777 SIZE=5120
#         BTIME=May  7 05:54:47 2006 [0 nsec]
#         MTIME=Apr  2 11:27:36 2009 [0 nsec]
#         CTIME=Apr  2 11:27:36 2009 [0 nsec]
#         ATIME=Apr  2 12:00:30 2009 [0 nsec]
# OWNER=root GRP=wheel LINKCNT=35 FLAGS=0 BLKCNT=c GEN=65f71df4
# fsdb (inum: 2)> lookup file.050150
# component `file.050150': current inode: regular file
# I=198 MODE=100600 SIZE=0
#         BTIME=Apr  2 11:24:33 2009 [0 nsec]
#         MTIME=Apr  2 11:24:33 2009 [0 nsec]
#         CTIME=Apr  2 11:24:33 2009 [0 nsec]
#         ATIME=Apr  2 11:24:33 2009 [0 nsec]
# OWNER=pho GRP=wheel LINKCNT=1 FLAGS=0 BLKCNT=0 GEN=1deaab3a
# fsdb (inum: 198)> quit
# $

# Consistency is restored by a umount + mount of the FS

# Observations:
#    No problems seen with vfs.lookup_shared=0.
#    Does not fail in a "private" subdirectory

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > namecache.c
mycc -o namecache -Wall namecache.c
rm -f namecache.c

#dir=/tmp/namecache.dir	# No problems seen
dir=/tmp
[ -d $dir ] || mkdir -p $dir
cd $dir

start=`date '+%s'`
for i in `jot 30`; do
	for j in `jot 10`; do
		/tmp/namecache &
	done

	for j in `jot 10`; do
		wait
	done
	[ $((`date '+%s'` - start)) -gt 1200 ] && break
done

if ls -l $dir/file.0* 2>&1 | egrep "file.0[0-9]" | grep -q "No such file"; then
	echo FAIL
	echo "ls -l $dir/file.0*"
	ls -l $dir/file.0*
fi

rm -f /tmp/namecache # /$dir/file.0*
exit
EOF
/* Test scenario for possible name cache problem */

#include <sys/types.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char path[MAXPATHLEN+1];
static char buf[64 * 1024];

void
pm(void)
{
	int fd, n;
	int space = sizeof(buf);
	struct stat statb;
	off_t base;
	struct dirent *dp;
	char *bp = buf;

	if ((fd = open(".", O_RDONLY)) == -1)
		err(1, "open(%s)", ".");

	do {
		if ((n = getdirentries(fd, bp, space, &base)) == -1)
			err(1, "getdirentries");
		space = space - n;
		bp   = bp + n;
	} while (n != 0);
	close(fd);

	bp = buf;
	dp = (struct dirent *)bp;
	for (;;) {
		if (strcmp(path, dp->d_name) == 0) {

			if (stat(dp->d_name, &statb) == -1) {
				warn("stat(%s)", dp->d_name);
				printf("name: %-10s, inode %7ju, "
				    "type %2d, namelen %d, d_reclen %d\n",
				    dp->d_name, (uintmax_t)dp->d_fileno, dp->d_type,
				    dp->d_namlen, dp->d_reclen);
				fflush(stdout);
			} else {
				printf("stat(%s) succeeded!\n", path);
				fflush(stdout);
			}

		}
		bp = bp + dp->d_reclen;
		dp = (struct dirent *)bp;
		if (dp->d_reclen <= 0)
			break;
	}
}

static void
reader(void) {
	int fd;

	if ((fd = open(path, O_RDWR, 0600)) < 0) {
		warn("open(%s). %s:%d", path, __FILE__, __LINE__);
		pm();
		exit(1);
	}
	close(fd);
	return;
}

static void
writer(void) {
	int fd;

	if ((fd = open(path, O_RDWR, 0600)) < 0) {
		warn("open(%s). %s:%d", path, __FILE__, __LINE__);
		pm();
		exit(1);
	}
	close(fd);
	return;
}

int
main(int argc, char **argv)
{
	pid_t pid;
	int fd, i, status;

	for (i = 0; i < 10000; i++) {
		if (sprintf(path, "file.0%d", getpid()) < 0)
			err(1, "sprintf()");
		if ((fd = open(path, O_CREAT | O_RDWR, 0600)) == -1)
			err(1, "open(%s)", path);
		close(fd);

		if ((pid = fork()) == 0) {
			writer();
			exit(EXIT_SUCCESS);

		} else if (pid > 0) {
			reader();
			if (waitpid(pid, &status, 0) == -1)
				warn("waitpid(%d)", pid);
		} else
			err(1, "fork(), %s:%d",  __FILE__, __LINE__);

		if (unlink(path) == -1)
			err(1, "unlink(%s). %s:%d", path, __FILE__, __LINE__);
	}
	return (0);
}
