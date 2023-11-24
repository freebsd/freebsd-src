#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# msdosfs rename scenario
# "Invalid long filename entry" seen from fsck

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

[ -x /sbin/mount_msdosfs ] || exit 0
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/msdos8.c
cc -o msdos8 -Wall -Wextra -O0 -g msdos8.c || exit 1
rm -f msdos8.c
cd $odir
log=/tmp/msdos8.sh.log
mount | grep "$mntpoint" | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart
gpart create -s bsd md$mdstart > /dev/null
gpart add -t freebsd-ufs md$mdstart > /dev/null
part=a
newfs_msdos /dev/md${mdstart}$part #> /dev/null
mount -t msdosfs /dev/md${mdstart}$part $mntpoint || exit 1

(cd $mntpoint; /tmp/msdos8)
s=$?

while mount | grep "$mntpoint" | grep -q md$mdstart; do
	umount $mntpoint || sleep 1
done
fsck -t msdosfs -y /dev/md${mdstart}$part > $log 2>&1
if egrep -q "BAD|INCONSISTENCY|MODIFIED" $log; then
	cat $log
	s=1

	mount -t msdosfs /dev/md${mdstart}$part $mntpoint || exit 1
	ls -lR $mntpoint
	umount $mntpoint
fi
mdconfig -d -u $mdstart
rm /tmp/msdos8 $log
s=0	# Ignore for now
exit $s
EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

# define PARALLEL 10

static unsigned long size;

static void
test(void)
{
	pid_t pid;
	int fd, i, j;
	char file1[128], file2[128];

	pid = getpid();
	for (i = 0; i < (int)size; i++) {
		sprintf(file1,"p%05d.%05d", pid, i);
		if ((fd = open(file1, O_RDONLY|O_CREAT, 0660)) == -1)
			err(1, "openat(%s), %s:%d", file1, __FILE__,
			    __LINE__);
		close(fd);
	}
	for (j = 0; j < 100; j++) {
		for (i = 0; i < (int)size; i++) {
			sprintf(file1,"p%05d.%05d", pid, i);
			sprintf(file2,"p%05d.%05d.togo", pid, i);
			if (rename(file1, file2) == -1)
				err(1, "rename(%s, %s). %s:%d", file1,
				    file2, __FILE__, __LINE__);
		}
		for (i = 0; i < (int)size; i++) {
			sprintf(file1,"p%05d.%05d", pid, i);
			sprintf(file2,"p%05d.%05d.togo", pid, i);
			if (rename(file2, file1) == -1)
				err(1, "rename(%s, %s). %s:%d", file2,
				    file1, __FILE__, __LINE__);
		}
	}

	for (i = 0; i < (int)size; i++) {
		sprintf(file1,"p%05d.%05d", pid, i);
		if (unlink(file1) == -1)
			err(1, "unlink(%s), %s:%d", file1, __FILE__,
			    __LINE__);
	}
	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int e, i, status;

	e = 0;
	size = 5;
	start = time(NULL);
	while ((time(NULL) - start) < 60 && e == 0) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
			if (pids[i] == -1)
				err(1, "fork()");
		}
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			if (WIFSIGNALED(status))
				fprintf(stderr, "pid %d exit signal %d\n",
				    pids[i], WTERMSIG(status));
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}
