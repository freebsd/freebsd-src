#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Test EVFILT_VNODE. Found page fault in knlist_add+0x39
# Test scenario by kib@

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kevent5.c
mycc -o kevent5 -Wall -Wextra -O2 -g kevent5.c || exit 1
rm -f kevent5.c

[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR
/tmp/kevent5 kevent5.xxx kevent5.yyy
s=$?

rm -f /tmp/kevent5 kevent.xxx kevent.yyy

exit $s
EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/event.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

volatile u_int *share;

static char *file1, *file2;

#define N 1000
#define SYNC 0

static void
test(void) {

	struct kevent ev[2];
	struct timespec ts;
	int fd, kq, n;

	if ((fd = open(file1, O_RDONLY, 0)) == -1)
		err(1, "open(%s)(2)", file1);
	atomic_add_int(&share[SYNC], 1);

	if ((kq = kqueue()) < 0)
		err(1, "kqueue()");

	n = 0;
	EV_SET(&ev[n], fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
		NOTE_DELETE, 0, 0);
	n++;

	ts.tv_sec  = 5;
	ts.tv_nsec = 0;

	if (kevent(kq, ev, n, NULL, 0, &ts) < 0)
		err(1, "kevent()");

	memset(&ev, 0, sizeof(ev));
	n = kevent(kq, NULL, 0, ev, 1, NULL);
	close(fd);
	close(kq);

/* Once the rendezvous file is gone create a new kevent */

	if ((fd = open(file2, O_RDONLY, 0)) == -1)
		err(1, "open(%s)(2)", file2);

	if ((kq = kqueue()) < 0)
		err(1, "kqueue()");

	n = 0;
	EV_SET(&ev[n], fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
		NOTE_DELETE, 0, 0);
	n++;

	if (kevent(kq, ev, n, NULL, 0, NULL) < 0)
		err(1, "kevent()");

	memset(&ev, 0, sizeof(ev));
	n = kevent(kq, NULL, 0, ev, 1, &ts);
	close(fd);
	close(kq);
}

int
main(int argc, char **argv) {
	size_t len;
	int e, fd, i, j, status;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <rendezvous file> <tail file>\n",
		    argv[0]);
		return (1);
	}
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	e = 0;
	file1 = argv[1];
	file2 = argv[2];

	for (j = 0; j < 100 && e == 0; j++) {
		if ((fd = open(file1, O_CREAT | O_TRUNC | O_RDWR, 0660)) ==
		    -1)
			err(1, "open(%s)", file1);
		close(fd);
		if ((fd = open(file2, O_CREAT | O_TRUNC | O_RDWR, 0660)) ==
		    -1)
			err(1, "open(%s)", file2);
		close(fd);

		share[SYNC] = 0;
		for (i = 0; i < N; i++) {
			if (fork() == 0) {
				test();
				return (0);
			}
		}

		while (share[SYNC] != N)
			usleep(200);

		sleep(1);
		if (unlink(file1) == -1)
			err(1, "unlink(%s). %s:%d\n", file1, __FILE__,
			    __LINE__);
		sleep(2);
		if (unlink(file2) == -1)
			err(1, "unlink(%s). %s:%d\n", file2, __FILE__,
			    __LINE__);

		for (i = 0; i < N; i++) {
			if (wait(&status) == -1)
				err(1, "wait(), %s:%d", __FILE__, __LINE__);
			if (status != 0)
				e = 1;
		}
	}

	return (e);
}
