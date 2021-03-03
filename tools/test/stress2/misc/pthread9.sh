#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Thread suspend deadlock seen:
# https://people.freebsd.org/~pho/stress/log/pthread9.txt

# Test scenario by Conrad Meyer.
# Fixed by r283320.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pthread9.c
mycc -o pthread9 -Wall -Wextra -O2 pthread9.c -lpthread || exit 1
rm -f pthread9.c

status=0
if ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1; then
	mount -t nfs -o nfsv3,tcp,nolockd,retrycnt=3,intr $nfs_export \
	    $mntpoint || exit 1
	sleep .5
	echo "Expect core dumps"
	(cd $mntpoint; /tmp/pthread9) &
	sleep 200
	if pgrep -q pthread9; then
		echo FAIL
		procstat -k `pgrep pthread9 | grep -v $!`
		status=1
	fi
	rm -f $mntpoint/pthread9.core
	umount -f $mntpoint
	wait
fi

rm -f /tmp/pthread9 /tmp/pthread9.core
exit $status
EOF
#include <sys/types.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#define	__NP__
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOOPS 50
#define RUNTIME 180

volatile u_int go;
int fd;
char file[] = "pthread9.file";

static void *
t1(void *data __unused)
{
	int i;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	while (go == 0)
		pthread_yield();

	atomic_add_int(&go, 1);
	for (i = 0; i < 100; i++)
		if (ftruncate(fd, 0) == -1)
			err(1, "truncate");

	return (NULL);
}

static void *
t2(void *data __unused)
{
	int i;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	while (go == 0)
		pthread_yield();

	atomic_add_int(&go, 1);
	for (i = 0; i < 100; i++)
		if (ftruncate(fd, 0) == -1)
			err(1, "truncate");

	return (NULL);
}

static void *
t3(void *data __unused)
{

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	while (go != 3)
		pthread_yield();
	abort();

	return (NULL);
}

int
test(void)
{
	pthread_t tid[3];
	int i, rc;

	go = 0;
	if ((rc = pthread_create(&tid[0], NULL, t1, NULL)) != 0)
		errc(1, rc, "pthread_create");
	if ((rc = pthread_create(&tid[1], NULL, t2, NULL)) != 0)
		errc(1, rc, "pthread_create");
	if ((rc = pthread_create(&tid[2], NULL, t3, NULL)) != 0)
		errc(1, rc, "pthread_create");
	usleep(200);
	atomic_add_int(&go, 1);

	for (i = 0; i < 3; i++) {
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join");
	}

	_exit(0);
}

int
main(void)
{
	time_t start;

	if ((fd = open(file, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "open(%s)", file);

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if (fork() == 0)
			test();
		wait(NULL);
	}
	close(fd);
	unlink(file);

	return (0);
}
