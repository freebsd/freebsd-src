#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# Regression test for https://reviews.freebsd.org/D20784
# "Fix mutual exclusion in pipe_direct_write()"
# https://people.freebsd.org/~pho/stress/log/mkfifo8.txt

# Reported by syzbot+21811cc0a89b2a87a9e7@syzkaller.appspotmail.com
# Test scenario suggestion by markj@
# Fixed by r349546

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mkfifo8.c
mycc -o mkfifo8 -Wall -Wextra -O0 -g mkfifo8.c || exit 1
rm -f mkfifo8.c
cd $odir

set -e
mount | grep -q "on $mntpoint " && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags /dev/md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
$dir/mkfifo8; s=$?
cd $odir

while mount | grep -q "on $mntpoint "; do
        umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -rf /tmp/mkfifo8
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define ACT 1
#define NPROCS 64
#define PARALLEL 4
#define RUNTIME 60
#define SIZ 8192
#define SYNC 0

static volatile u_int *share;
static int fd;
static char cp[SIZ];

static void
tw(void)
{
	int r;

        atomic_add_int(&share[ACT], 1);
	r = write(fd, cp, SIZ);
	if (r == -1)
		warn("write");

	_exit(0);
}

static void
tr(void)
{
	int i, r;
	char cp[SIZ];

	while (share[ACT] < NPROCS / 2)
		usleep(10);
	for (i = 0; i < NPROCS; i++) {
		r = read(fd, cp, SIZ);
		if (r == -1)
			warn("read");
	}

	_exit(0);
}

static void
test(void)
{
        pid_t pid[NPROCS + 1];
        int i;
        char file[80];

        atomic_add_int(&share[SYNC], 1);
        while (share[SYNC] != PARALLEL)
                ;

        snprintf(file, sizeof(file), "fifo.%d", getpid());
	if (mkfifo(file, DEFFILEMODE) == -1)
                err(1, "mkfifo(%s)", file);
        if ((fd = open(file, O_RDWR)) == -1)
                err(1, "open(%s)", file);

	for (i = 0; i < NPROCS; i++) {
		if ((pid[i] = fork()) == 0)
			tw();
	}
	if ((pid[NPROCS] = fork()) == 0)
		tr();

	for (i = 0; i < NPROCS + 1; i++) {
		if (waitpid(pid[i], NULL, 0) != pid[i])
			err(1, "waitpid");
	}
        close(fd);
        unlink(file);

        _exit(0);
}

int
main(void)
{
        pid_t pids[PARALLEL];
        size_t len;
        time_t start;
        int e, i, status;

        e = 0;
        len = PAGE_SIZE;
        if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
            MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
                err(1, "mmap");

	for (i = 0; i < SIZ; i += PAGE_SIZE)
		cp[i] = 1;
        start = time(NULL);
        while ((time(NULL) - start) < RUNTIME && e == 0) {
                share[SYNC] = 0;
                share[ACT] = 0;
                for (i = 0; i < PARALLEL; i++) {
                        if ((pids[i] = fork()) == 0)
                                test();
                        if (pids[i] == -1)
                                err(1, "fork()");
                }
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
