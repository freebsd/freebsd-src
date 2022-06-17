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

# "ffs_fsync: dirty" seen:
# http://people.freebsd.org/~pho/stress/log/ffs_sync.txt

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > ffs_sync.c
mycc -o ffs_sync -Wall -Wextra ffs_sync.c || exit 1
rm -f ffs_sync.c
cd $here

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 4g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

for i in `jot 3`; do
	su $testuser -c "cd $mntpoint; /tmp/ffs_sync" &
	sleep 60
	killall -q ffs_sync
	killall -q ffs_sync
done

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
rm -f /tmp/ffs_sync
mdconfig -d -u $mdstart
exit
EOF
#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define PARALLEL    12
#define NRW       1000
#define NFTS     10000
#define NMKDIR    6000
#define NSYMLINK 20000

void
slinktest(void)
{
        int i, j;
        pid_t pid;
        char file[128];

	setproctitle("slink");
        pid = getpid();
        for (j = 0; j < NSYMLINK; j++) {
                sprintf(file,"p%05d.%05d", pid, j);
                if (symlink("/tmp/not/there", file) == -1) {
                        if (errno != EINTR)
                                warn("symlink(%s). %s.%d", file, __FILE__, __LINE__);
                }
        }

        for (i = --j; i >= 0; i--) {
                sprintf(file,"p%05d.%05d", pid, i);
                if (unlink(file) == -1)
                        err(3, "unlink(%s)", file);
        }

	_exit(0);
}

void
mktest(void)
{
	int i;
	char path[80];

	setproctitle("mkdir");
	sprintf(path, "d%06d", getpid());
	if (mkdir(path, 0770) == -1)
		err(1, "mkdir(%s)", path);
	chdir(path);

	sprintf(path, "d");
	for (i = 0; i < NMKDIR; i++) {
		if (mkdir(path, 0770) == -1) {
			warn("mkdir(%s),  %s:%d", path, __FILE__, __LINE__);
		} else
			chdir(path);

	}
	for (i = 0; i < NMKDIR; i++) {
		chdir("..");
		rmdir(path);
	}
	chdir("..");

	_exit(0);
}

void
rwtest(void)
{
	int fd, i, j;
	char buf[80], file[80];

	setproctitle("rw");
	for (i = 0; i < NRW; i++) {
		sprintf(file, "f%06d.%06d", getpid(), i);
		if ((fd = open(file, O_CREAT | O_EXCL, 0644)) == -1)
			err(1, "open(%s)", file);
		for (j = 0; j < 1024; j++)
			write(fd, buf, sizeof(buf));
		lseek(fd, 0, SEEK_SET);
		for (j = 0; j < 1024; j++)
			read(fd, buf, sizeof(buf));
		close(fd);
	}
	for (i = 0; i < NRW; i++) {
		sprintf(file, "f%06d.%06d", getpid(), i);
		if (unlink(file) == -1)
			warn("unlink(%s)", file);
	}
	_exit(0);
}

void
slink(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			slinktest();
}

void
mk(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			mktest();
}

void
rw(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			rwtest();
}

void
ftstest(void)
{
	FTS *fts;
	FTSENT *p;
	int ftsoptions, i;
	char *args[2];

	setproctitle("fts");
	ftsoptions = FTS_PHYSICAL;
	args[0] = ".";
	args[1] = 0;

	for (i = 0; i < NFTS; i++) {
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		while ((p = fts_read(fts)) != NULL)
				;

		if (errno != 0 && errno != ENOENT)
			err(1, "fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close()");
	}

	_exit(0);
}

void
fts(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			ftstest();
}

int
main(void)
{
	int i;

	slink();
	mk();
	rw();
	fts();

	for (i = 0; i < PARALLEL; i++) {
		wait(NULL);
		wait(NULL);
		wait(NULL);
		wait(NULL);
	}

	return (0);
}
