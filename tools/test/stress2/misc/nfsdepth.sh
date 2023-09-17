#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

. ../default.cfg

# NFS test with deep directories.

# Only issue seen is:
# nfsdepth: mkdir(d93) l35: Permission denied

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > nfsdepth.c
mycc -o nfsdepth -Wall -Wextra -g nfsdepth.c || exit 1
rm -f nfsdepth.c
cd $odir

mount | grep "$mntpoint" | grep nfs > /dev/null && umount $mntpoint
mount -t nfs -o tcp -o rw -o soft $nfs_export $mntpoint

work=$mntpoint/nfsdepth.`jot -rc 8 a z | tr -d '\n'`.dir
mkdir -p $work
chmod 777 $work

su $testuser -c "cd $work; /tmp/nfsdepth"
s=$?
if [ $s -eq 0 ]; then
	su $testuser -c "rm -rf $work 2>/dev/null"
	rm -rf $work
else
	find $work -ls
fi

umount $mntpoint > /dev/null 2>&1
while mount | grep "$mntpoint" | grep -q nfs; do
	umount $mntpoint > /dev/null 2>&1
done

rm -f /tmp/nfsdepth
exit $s

EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned long actual, size;
volatile int done_testing;
int fail;

#define DEPTH 200
#define PARALLEL 8
#define RUNTIME 180

void
handler(int s __unused)
{
	done_testing = 1;
}

void
mkDir(char *path, int level) {
	int n, r;
	char newPath[MAXPATHLEN + 1];

	n = 0;
	do {
		r = mkdir(path, 0770);
		if (r == -1 && errno == EACCES && n < 10) {
			warn("mkdir(%s) l%d", path, __LINE__);
			n++;
			errno = EAGAIN;
			usleep(10000);
		}
		if (r == -1 && errno == EINTR)
			(void)rmdir(path);
	} while (r == -1 && (errno == EINTR || errno == EAGAIN));

	if (r == -1) {
		warn("mkdir(%s), pid %d, l%d", path, getpid(), __LINE__);
		fail++;
	} else {
		actual++;
		do {
			r = chdir (path);
		if (r == -1 && errno == EACCES && n < 10) {
			warn("chdir(%s) .%d", path, __LINE__);
			n++;
			errno = EAGAIN;
			usleep(10000);
		}
		} while (r == -1 && (errno == EINTR || errno == EAGAIN));
		if (r == -1)
			err(1, "chdir(%s), pid %d, l%d", path, getpid(), __LINE__);
	}

	if (done_testing == 0 && fail == 0 && level < (int)size) {
		sprintf(newPath,"d%d", level + 1);
		mkDir(newPath, level + 1);
	}
}

void
rmDir(char *path, int level) {
	int n, r;
	char newPath[MAXPATHLEN + 1];

	if (level == 0)
		return;

	if (level < (int)actual) {
		sprintf(newPath,"d%d", level+1);
		rmDir(newPath, level+1);
	}
	n = 0;
	do {
		r = chdir ("..");
		if (r == -1 && errno == EACCES && n < 10) {
			warn("chdir(%s) l%d", path, __LINE__);
			n++;
			errno = EAGAIN;
			usleep(10000);
		}
	} while (r == -1 && (errno == EINTR || errno == EAGAIN));
	if (r == -1)
		err(1, "chdir(%s), pid %d,  l%d", "..", getpid(), __LINE__);
	n = 0;
	do {
		r = rmdir(path);
		if (r == -1 && errno == EACCES && n < 10) {
			warn("rmdir(%s) l%d", path, __LINE__);
			n++;
			errno = EAGAIN;
			usleep(10000);
		}
	} while (r == -1 && (errno == EINTR || errno == EAGAIN));
	if (r == -1)
		err(1, "rmdir(%s), pid %d,  l%d", path, getpid(), __LINE__);
}

int
test2(void)
{
	char path[MAXPATHLEN + 1];

	fail = actual = 0;
	umask(0);
	sprintf(path,"p%05d.d%d", getpid(), 1);
	mkDir(path, 1);
	rmDir(path, 1);

	_exit (fail);
}

int
test(void)
{
	pid_t pid;
	time_t start;
	int status;

	size = (arc4random() % DEPTH) + 1;

	signal(SIGHUP, handler);
	start = time(NULL);
	while (time(NULL) - start < RUNTIME && fail == 0) {
		if ((pid = fork()) == 0) {
			done_testing = 0;
			test2();
		}

		status = 0;
		while (wait4(pid, &status, WNOHANG, NULL) != pid) {
			if (kill(pid, SIGHUP) == -1)
				err(1, "kill(%d)", pid);
			usleep(100000 + (arc4random() % 10000));
		}
		if (status != 0)
			fail++;
	}

	_exit (status != 0);
}

int
main(void)
{
	int e, i, pids[PARALLEL], status;

	e = 0;
	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test();
	}
	for (i = 0; i < PARALLEL; i++) {
		waitpid(pids[i], &status, 0);
		e += status == 0 ? 0 : 1;
	}

	return (e);
}
