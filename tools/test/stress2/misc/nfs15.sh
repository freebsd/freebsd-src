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

# Test effort to reproduce reported: "observed a hang in a multi-threaded
# process that had hit an assertion failure and was attempting to dump core".
# Problem never seen.

# Most interesting load are:
# - coredumping of the multithreaded program with the current dir on NFS,
#   which also accesses NFS files;
# - advisory locking tests on NFS files, while e.g. sending SIGSTOP/SIGCONT
#   to the test programs.

# See also pthread9.sh

# https://people.freebsd.org/~pho/stress/log/kostik897.txt
# Fixed in r302013.

# "panic: mutex sleepq chain not owned at subr_sleepqueue.c:1009" seen:
# https://people.freebsd.org/~pho/stress/log/kostik914.txt
# Fixed in r302328.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

USE_TIMEOUT=1
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > nfs15.c
mycc -o nfs15 -Wall -Wextra -O2 -g nfs15.c -lpthread || exit 1
rm -f nfs15.c
cd $here

mount | grep "on $mntpoint " | grep nfs > /dev/null && umount $mntpoint

mount -t nfs -o tcp -o retrycnt=3 -o intr,soft -o rw -o nolockd \
    $nfs_export $mntpoint || exit 1
sleep 2
wd=$mntpoint/nfs15.dir
rm -rf $wd
mkdir $wd

(cd $wd; /tmp/nfs15)
rm -rf $wd

while mount | grep "on $mntpoint " | grep -q nfs; do
	umount $mntpoint || sleep 1
done

rm -f /tmp/nfs15
exit 0
EOF
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PARALLEL 4
#define RUNTIME 300
#define SYNC 0

volatile u_int *share;

static void *
t1(void *data __unused)
{
	atomic_add_int(&share[SYNC], 1);
	usleep(arc4random() % 8000);
	raise(SIGABRT);

	return (NULL);
}

static void *
t2(void *data __unused)
{
	int fd, i, r;
	char file[80];

	for (i = 0; i < 100; i++) {
		atomic_add_int(&share[SYNC], 1);
		snprintf(file, sizeof(file), "file.%06d", i);
		if ((fd = open(file, O_WRONLY | O_CREAT | O_APPEND,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s)", file);
		do {
			r = lockf(fd, F_LOCK, 0);
		} while (r == -1 && errno == EDEADLK);
		if (r == -1)
			err(1, "lockf(%s, F_LOCK)", file);
		write(fd, "x", 1);
		if (lseek(fd, 0, SEEK_SET) == -1)
			err(1, "lseek");
		if (lockf(fd, F_ULOCK, 0) == -1)
			err(1, "lockf(%s, F_ULOCK)", file);
		close(fd);
	}

	return (NULL);
}

int
test(void)
{
	pthread_t tid[3];
	int i, rc;

	for (i = 0; i < 10; i++) {
		if ((rc = pthread_create(&tid[0], NULL, t2, NULL)) == -1)
			errc(1, rc, "pthread_create");
		if ((rc = pthread_create(&tid[1], NULL, t2, NULL)) == -1)
			errc(1, rc, "pthread_create");
		if ((rc = pthread_create(&tid[2], NULL, t1, NULL)) == -1)
			errc(1, rc, "pthread_create");

		if ((rc = pthread_join(tid[0], NULL)) == -1)
			errc(1, rc, "pthread_join");
		if ((rc = pthread_join(tid[1], NULL)) == -1)
			errc(1, rc, "pthread_join");
		if ((rc = pthread_join(tid[2], NULL)) == -1)
			errc(1, rc, "pthread_join");
	}

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
        int i, status;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}

		for(;;) {
			if (share[SYNC] > 0)
				atomic_add_int(&share[SYNC], -1);
			for (i = 0; i < PARALLEL; i++)
				kill(pids[i], SIGSTOP);
			usleep(100 + arc4random() % 1000);
			for (i = 0; i < PARALLEL; i++)
				kill(pids[i], SIGCONT);
			usleep(100 + arc4random() % 400);
			if (share[SYNC] == 0) { /* If all procs are done */
				usleep(500);
				if (share[SYNC] == 0)
					break;
			}
		}

		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) != pids[i])
				err(1, "waitpid");
		}
	}

	return (0);
}
