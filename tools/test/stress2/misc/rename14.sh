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

# "panic: handle_workitem_remove: DIRCHG and worklist not empty." seen:
# https://people.freebsd.org/~pho/stress/log/rename14.txt
# Fixed by r356714

# Based on a syzkaller scenario reported by tuexen@freebsd.org

# "panic: journal_jremref: Lost inodedep":
# https://people.freebsd.org/~pho/stress/log/log0279.txt

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed -e '1,/^EOF/d' < $odir/$0 > $dir/rename14.c
mycc -o rename14 -Wall -Wextra -O0 -g rename14.c -lpthread || exit 1
rm -f rename14.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
[ `jot -r 1 0 1` -eq 1 ] && opt="-U -n" || opt="-j -n"
newfs -j -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

s=0
(cd /mnt; /tmp/rename14)
[ -f rename14.core -a $s -eq 0 ] &&
    { ls -l rename14.core; mv rename14.core $dir; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
checkfs /dev/md$mdstart || s=2
mdconfig -d -u $mdstart
rm -rf $dir/rename14
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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static sig_atomic_t done_testing;
static volatile u_int *share;

#define PARALLEL 10
#define RUNTIME (3 * 60)
#define SYNC 0

static void *
t1(void *data __unused)
{
	char n1[80];

	snprintf(n1, sizeof(n1), "file1.%d", getpid());
        while (done_testing == 0) {
		rmdir(n1);
		usleep(50000);
        }

        return (NULL);
}

static void *
t2(void *data __unused)
{
	time_t start;
	char n0[80], n00[80], n1[80];

	snprintf(n0, sizeof(n0), "file0.%d", getpid());
	snprintf(n00, sizeof(n00), "file0.%d/file0", getpid());
	snprintf(n1, sizeof(n1), "file1.%d", getpid());
	if (mkdir(n0, 0) == -1)
		err(1, "mkdir(%s)", n0);

	start = time(NULL);
	while (time(NULL) - start < 60) {
		if (mkdir(n00, 0) == -1)
			err(1, "mkdir(%s)", n00);
		if (rename(n00, n1) == -1)
			err(1, "rename(i%s, %s)", n00, n1);
        }
	if (rmdir(n0) == -1)
		err(1, "rmdir(%s)", n0);
	done_testing = 1;

        return (NULL);
}

static void
test(void)
{
        pthread_t tid[2];
        int r;

	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	done_testing = 0;
	if ((r = pthread_create(&tid[0], NULL, t1, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&tid[1], NULL, t2, NULL)) != 0)
		errc(1, r, "pthread_create");

	if ((r = pthread_join(tid[0], NULL)) != 0)
		errc(1, r, "pthread_join");
	if ((r = pthread_join(tid[1], NULL)) != 0)
		errc(1, r, "pthread_join");

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

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		share[SYNC] = 0;
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
