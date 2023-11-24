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

# Variation of kevent5.sh
# OOM seen: https://people.freebsd.org/~pho/stress/log/mark018.txt

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/kevent11.c
mycc -o kevent11 -Wall -Wextra -O0 -g kevent11.c || exit 1
rm -f kevent11.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -h -l 100 > /dev/null) &
cd $mntpoint
$dir/kevent11
s=$?
[ -f kevent11.core -a $s -eq 0 ] &&
    { ls -l kevent11.core; mv kevent11.core /tmp; s=1; }
cd $odir
while pkill -9 swap; do :; done
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf $dir/kevent11
exit $s

EOF
#include <sys/param.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;
static char *file = "file";

#define PARALLEL 256
#define RUNTIME (3 * 60)
#define SYNC 0

static void
test(void)
{
        struct kevent ev[2];
        struct timespec ts;
        int kq, fd, n;

	if ((fd = open(file, O_RDONLY, 0)) == -1)
		err(1, "open(%s). %s:%d", file, __func__, __LINE__);

	if ((kq = kqueue()) < 0)
		err(1, "kqueue()");

	n = 0;
	EV_SET(&ev[n], fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
		NOTE_DELETE, 0, 0);
	n++;

	if (kevent(kq, ev, n, NULL, 0, NULL) < 0)
		err(1, "kevent()");

	setproctitle("presync");
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		usleep(10000);
	setproctitle("postsync");

	memset(&ev, 0, sizeof(ev));
	n = kevent(kq, NULL, 0, ev, 1, &ts);
	close(fd);
	close(kq);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int e, fd, i, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		if ((fd = open(file, O_CREAT | O_TRUNC | O_RDWR, 0660)) ==
		    -1)
			err(1, "open(%s)", file);
		close(fd);
		share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {

			if ((pids[i] = fork()) == 0)
				test();
			if (pids[i] == -1)
				err(1, "fork()");
		}
		while (share[SYNC] != PARALLEL)
			usleep(10000);
		if (unlink(file) == -1)
			err(1, "unlink(%s). %s:%d\n", file,
				    __FILE__, __LINE__);
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			if (status != 0) {
				if (WIFSIGNALED(status))
					fprintf(stderr,
					    "pid %d exit signal %d\n",
					    pids[i], WTERMSIG(status));
			}
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}
