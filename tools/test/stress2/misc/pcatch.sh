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

# The issue makes it possible for applications to get wrong EINTR
# or ERESTART (the later is not directly visible) when doing write
# to the file on suspended UFS volume. I.e. the thread is sleeping
# when fs is suspended, and process is signaled.

# Test scenario mostly by kib.
# Fixed in r275744.

# Deadlock seen:
# https://people.freebsd.org/~pho/stress/log/pcatch.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -z "$DEBUG" ] && exit 0 # Waiting for fix

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pcatch.c
mycc -o pcatch -Wall -Wextra -O0 -g pcatch.c || exit 1
rm -f pcatch.c
cd $here

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint

start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 120 ]; do
	/tmp/pcatch $mntpoint
done

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/pcatch
exit 0
EOF
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
hand_sigaction(int signo __unused, siginfo_t *si __unused, void *c __unused)
{
}

static void
suspend(char *path)
{
        struct statfs s;
        int fd, error;

	if (fork() == 0) {
		if ((error = statfs(path, &s)) != 0)
			err(1, "statfs %s", path);
		fd = open("/dev/ufssuspend", O_RDWR);
		if ((error = ioctl(fd, UFSSUSPEND, &s.f_fsid)) != 0)
			err(1, "UFSSUSPEND");
		sleep(1);
		if ((error = ioctl(fd, UFSRESUME, &s.f_fsid)) != 0)
			err(1, "UFSRESUME");
		_exit(0);
	}
}

static void
test(char *mp)
{
	pid_t pid;
	struct sigaction sa;
	int fd;
	char buf[80], file[80];

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = hand_sigaction;
	if (sigaction(SIGUSR1, &sa, NULL) == -1)
		err(1, "sigaction");

	snprintf(file, sizeof(file), "%s/file", mp);
	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s). %s:%d", file, __FILE__, __LINE__);

	suspend(mp);

	if ((pid = fork()) == 0) {
		if (write(fd, buf, sizeof(buf)) == -1)
			warn("FAIL: write");
		_exit(0);
	}
	usleep(10000);
	if (kill(pid, SIGUSR1) == -1)
		err(1, "kill");
	wait(NULL);
	wait(NULL);
	close(fd);
}

int
main(int argc, char *argv[])
{

	if (argc != 2)
		errx(1, "Usage: %s <UFS mount point>", argv[0]);

	test(argv[1]);

	return (0);
}
