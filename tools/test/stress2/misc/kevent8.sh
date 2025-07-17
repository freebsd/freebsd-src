#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Out of VM seen due to missing close of kqueue handle.
# Fixed in 256849

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
ulimit -k 5000 || { echo FAIL; exit 1; }

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kevent8.c
mycc -o kevent8 -Wall -Wextra -O2 -g kevent8.c -lpthread || exit 1
rm -f kevent8.c
cd $odir

mount | grep "on $mntpoint " | grep -q md$mdstart && umount -f $mntpoint
mdconfig -l | grep -q $mdstart && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

su $testuser -c "(cd $mntpoint; /tmp/kevent8)" &
for i in `jot 99`; do
	sleep 1
	kill -0 $! 2>/dev/null || break
done
umount -f $mntpoint
pkill kevent8
wait

while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/kevent8

exit
EOF
#include <sys/types.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PARALLEL 64

static int fd;
static char path[80];

static void *
spin(void *arg __unused)
{
	int i;
	setproctitle("spin");

	for (i= 0;; i++) {
		snprintf(path, sizeof(path), "file.%06d.%d", getpid(), i);
		if ((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0622)) ==
		    -1)
			err(1, "creat()");
		close(fd);
		fd = 0;
		unlink(path);
	}
	return (NULL);
}

static void *
test(void *arg __unused)
{
	int kq;
	int i, n;
	struct kevent ev;
	struct timespec ts;

	for (i = 0; i < 500000; i++) {
		if ((kq = kqueue()) < 0)
			if (errno != ENOMEM)
				err(1, "kqueue()");

		n = 0;
		memset(&ev, 0, sizeof(ev));
		EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
			NOTE_DELETE|NOTE_RENAME|NOTE_EXTEND, 0, 0);
		n++;

		if (kevent(kq, &ev, n, NULL, 0, NULL) < 0)
			continue;	/* Note: missing close(kq)! */

		memset(&ev, 0, sizeof(ev));
		ts.tv_sec  = 0;
		ts.tv_nsec = 1000000;
		if ((n = kevent(kq, NULL, 0, &ev, 1, &ts)) == -1)
			err(1, "kevent()");

		close(kq);
	}
	return (NULL);
}

int
main(void)
{
	pthread_t cp[PARALLEL], sp;
	int e, i;

	if ((e = pthread_create(&sp, NULL, spin, NULL)) != 0)
		errc(1, e, "pthread_create");

	for (i = 0; i < PARALLEL; i++) {
		if ((e = pthread_create(&cp[i], NULL, test, NULL)) != 0)
			errc(1, e, "pthread_create");
	}

	for (i = 0; i < PARALLEL; i++)
		pthread_join(cp[i], NULL);

	close(fd);

	return (0);
}
