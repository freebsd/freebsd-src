#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# Test based on scenario by Babcia Padlina

# "panic: mutex pipe mutex not owned at sys_pipe.c:1769" seen:
# https://people.freebsd.org/~pho/stress/log/kevent13.txt
# Fixed by r235640.

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kevent13.c
cc -o kevent13 -Wall kevent13.c -lpthread
rm -f kevent13.c

[ -d "$RUNDIR" ] || mkdir -p $RUNDIR
cd $RUNDIR
start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	for i in `jot 10`; do
		/tmp/kevent13 &
		/tmp/kevent13 &
		wait
	done
done

rm -f /tmp/kevent13
exit 0
EOF
/*
 * 29.08.2009, babcia padlina
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/param.h>
#include <sys/timespec.h>
#include <unistd.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOOPS 200000

struct kevent kev[2], ke[2];
struct timespec timeout;
volatile int go;
int fd[2], kq;

void
do_thread(void) {
	int i;

	go = 1;
	for (i = 0; i < LOOPS; i++) {
		if (pipe(fd) < 0)
			err(1, "pipe");
		memset(&kev, 0, sizeof(kev));
		EV_SET(&kev[0], fd[0], EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
		EV_SET(&kev[1], fd[1], EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, NULL);

		if (kevent(kq, kev, 2, ke, 2, &timeout) < 0)
			err(1, "kevent");

		close(fd[0]);
		close(fd[1]);
	}
}

void
do_thread2(void) {
	int i;

	while (go == 0)
		usleep(10);
	for (i = 0; i < LOOPS; i++) {
		if (arc4random() % 2 == 0) {
			close(fd[0]);
			close(fd[1]);
		} else {
			close(fd[1]);
			close(fd[0]);
		}
	}
}

int
main(void) {
	pthread_t pth, pth2;
	int r;

	if ((kq = kqueue()) < 0)
		err(1, "kqueue");

	if ((r = pthread_create(&pth, NULL, (void *)do_thread, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&pth2, NULL, (void *)do_thread2, NULL)) != 0)
		errc(1, r, "pthread_create");

	timeout.tv_sec = 0;
	timeout.tv_nsec = 1;

	if ((r = pthread_join(pth, NULL)) != 0)
		errc(1, r, "pthread_join");
	if ((r = pthread_join(pth2, NULL)) != 0)
		errc(1, r, "pthread_join");

	return (0);
}
