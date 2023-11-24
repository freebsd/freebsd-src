#!/bin/sh

#
# Copyright (c) 2016 Dell EMC Isilon
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

# Demonstrate "wrong handling for suspend".
# https://www.mail-archive.com/freebsd-current@freebsd.org/msg166333.html

# panic: Failed to register NFS lock locally - error=11
# https://people.freebsd.org/~pho/stress/log/kostik897.txt
# Fixed in r302013.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
pgrep -q lockd || { echo "lockd not running."; exit 1; }

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > nfs15lockd.c
mycc -o nfs15lockd -Wall -Wextra -O2 -g nfs15lockd.c -lpthread || exit 1
rm -f nfs15lockd.c
cd $here

mount | grep "on $mntpoint " | grep nfs > /dev/null && umount $mntpoint

[ $# -ne 0 ] &&
    # Problem only seen with lockd
    { echo "Not using lockd"; debug="-o nolockd"; }
mount -t nfs -o tcp -o retrycnt=3 -o soft -o rw $debug \
    $nfs_export $mntpoint
sleep 2

s=0
lockf -t 10 $mntpoint/$$.lock sleep 2 > /tmp/$$.log 2>&1
if grep -q "No locks available" /tmp/$$.log; then
	echo "Is lockd running on the remote host?"
	rm /tmp/$$.log
	s=1
fi

wd=$mntpoint/nfs15lockd-`jot -rc 8 a z | tr -d '\n'`.dir
rm -rf $wd
mkdir $wd

echo "Expect: nfs15lockd exited on signal 6 (core dumped)"
(cd $wd; /tmp/nfs15lockd) &
start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 600 ]; do
	pgrep -q nfs15lockd || break
	sleep 2
done
if pgrep -q nfs15lockd; then
	s=2
	echo "Thread suspension issue:"
	ps -lx | grep -v grep | grep nfs15lockd | grep "T+" | \
		    awk '{print $2}' | while read pid; do
		ps -lp$pid
		procstat -k $pid
		kill -9 $pid
	done
	pkill nfs15lockd
fi
wait
rm -rf $wd

n=0
while mount | grep "on $mntpoint " | grep -q nfs; do
	umount $mntpoint && break
	n=$((n + 1))
	if [ $n -gt 60 ]; then
		fstat -mf $mntpoint
		s=3
		break
	fi
	sleep 2
done 2>&1 | awk '!seen[$0]++'
mount | grep -q "on $mntpoint " && umount -f $mntpoint

rm -f /tmp/nfs15lockd nfs15lockd.core
exit $s
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
		} while (r == -1 && (errno == EDEADLK || errno == EINTR));
		if (r == -1)
			err(1, "lockf(%s, F_LOCK)", file);
		write(fd, "x", 1);
		usleep(arc4random() % 1000);
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
			usleep(1000);
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
