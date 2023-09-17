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

# Test scenario by kib@

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kevent4.c
mycc -o kevent4 -Wall kevent4.c -pthread || exit 1
rm -f kevent4.c

cd $odir
export runRUNTIME=3m
(cd ..; ./run.sh > /dev/null 2>&1) &
rpid=$!

(cd $RUNDIR; /tmp/kevent4 $rpid > /dev/null) &

sleep 120
kill $rpid
../tools/killall.sh > /dev/null 2>&1
kill $!
wait
rm -f /tmp/kevent4

exit
EOF
#include <sys/types.h>
#include <sys/event.h>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef true
# define true 1
#endif

static int kq;

static void
err(const char *msg, int err_no)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(err_no));
	exit(1);
}

static void
init_kq()
{
	kq = kqueue();
	if (kq == -1)
		err("kqueue", errno);
}

static void
add_watch(pid_t pid)
{
	struct kevent kev;

	bzero(&kev, sizeof(kev));
	kev.ident = pid;
	kev.flags = EV_ADD | EV_ENABLE;
	kev.filter = EVFILT_PROC;
	kev.fflags = NOTE_EXIT | NOTE_FORK | NOTE_EXEC | NOTE_TRACK;

	while (true) {
		int res = kevent(kq, &kev, 1, NULL, 0, NULL);
		if (res == -1) {
			if (errno == EINTR)
				continue;
			if (errno == ESRCH)
				break;

			int err_no = errno;
			char msg[64];
			snprintf(msg, sizeof(msg),
				 "kevent - add watch for pid %u", pid);
			err(msg, err_no);
		}
		else
			break;
	}
}

static void polling()
{
	struct kevent kev[10];
	pid_t pid;
	int i;

	while (true) {
		bzero(&kev, sizeof(kev));
		int res = kevent(kq, NULL, 0, kev,
				 sizeof(kev) / sizeof(kev[0]), NULL);
		if (res == -1) {
			if (errno == EINTR)
				continue;

			if (errno == ESRCH)
				continue;

			err("kevent", errno);
		}

		for (i = 0; i < res; i++) {
			pid = kev[i].ident;
			if (kev[i].fflags & NOTE_CHILD) {
				add_watch(pid);
				printf("%u - new process, parent %u\n", pid,
				    (unsigned int)kev[i].data);
			}
			if (kev[i].fflags & NOTE_FORK) {
				printf("%u forked\n", pid);
			}
			if (kev[i].fflags & NOTE_EXEC) {
				printf("%u called exec\n", pid);
			}
			if (kev[i].fflags & NOTE_EXIT) {
				printf("%u exited\n", pid);
			}
			if (kev[i].fflags & NOTE_TRACK) {
				printf("%u forked - track\n", pid);
			}
			if (kev[i].fflags & NOTE_TRACKERR) {
				fprintf(stderr, "%u - track error\n", pid);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "pid ?\n");
		return (2);
	}
	pid_t parent = atoi(argv[1]);

	init_kq();
	add_watch(parent);
	polling();

	return (0);
}
