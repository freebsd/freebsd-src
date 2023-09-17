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

# Threaded syscall(2) fuzz test inspired by the iknowthis test suite
# by Tavis Ormandy <taviso  cmpxchg8b com>

# kevent(2) with random arguments.
# Spinning threads seen.
# Fixed in r255877.

# "panic: softclock_call_cc: act 0xfffff801219a0840 0" seen:
# https://people.freebsd.org/~pho/stress/log/kevent7.txt
# Fixed by r315289

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

ulimit -t 200
odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > kevent7.c
rm -f /tmp/kevent7
mycc -o kevent7 -Wall -Wextra -O2 -g kevent7.c -lpthread || exit 1
rm -f kevent7.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

for i in `jot 5`; do
	(cd $mntpoint; /tmp/kevent7 $* < /dev/null) &
	sleep 60
	while pgrep -q kevent7; do
		pkill -9 kevent7
		sleep 1
	done
done

for i in `jot 5`; do
	mount | grep -q md$mdstart  && \
		umount $mntpoint && mdconfig -d -u $mdstart && break
	sleep 10
done
if mount | grep -q md$mdstart; then
	fstat $mntpoint
	echo "umount $mntpoint failed"
	exit 1
fi
rm -f /tmp/kevent7
exit 0
EOF
#include <sys/types.h>
#include <sys/event.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THREADS 50

int fd[900], fds[2], socketpr[2];
#define N (128 * 1024 / (int)sizeof(u_int32_t))
static u_int32_t r[N];
static int syscallno;

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

static unsigned long
makearg(void)
{
	unsigned int i;
	unsigned long val;

	val = arc4random();
	i   = arc4random() % 100;
	if (i < 20)
		val = val & 0xff;
	if (i >= 20 && i < 40)
		val = val & 0xffff;
	if (i >= 40 && i < 60)
		val = (unsigned long)(r) | (val & 0xffff);
#if defined(__LP64__)
	if (i >= 60) {
		val = (val << 32) | arc4random();
		if (i > 80)
			val = val & 0x00007fffffffffffUL;
	}
#endif

	return(val);
}

static void *
test(void *arg __unused)
{

	FTS		*fts;
	FTSENT		*p;
	int		ftsoptions;
	int i;
	char		*args[5];

	ftsoptions = FTS_PHYSICAL;
	args[0] = "/dev";
	args[1] = "/proc";
	args[2] = "/usr/compat/linux/proc";
	args[3] = ".";
	args[4] = 0;

	for (;;) {
		for (i = 0; i < N; i++)
			r[i] = arc4random();
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		i = 0;
		while ((p = fts_read(fts)) != NULL) {
			if (fd[i] > 0)
				close(fd[i]);
			if ((fd[i] = open(p->fts_path, O_RDWR)) == -1)
				if ((fd[i] = open(p->fts_path, O_WRONLY)) ==
				    -1)
					if ((fd[i] = open(p->fts_path,
					    O_RDONLY)) == -1)
						continue;
			i++;
			i = i % nitems(fd);
		}

		if (fts_close(fts) == -1)
			if (errno != ENOTDIR)
				warn("fts_close()");
		if (pipe(fds) == -1)
			err(1, "pipe()");
		if (socketpair(PF_UNIX, SOCK_SEQPACKET, 0, socketpr) == -1)
			err(1, "socketpair()");
		sleep(1);
		close(socketpr[0]);
		close(socketpr[1]);
		close(fds[0]);
		close(fds[1]);
	}
	return(0);
}

static void *
calls(void *arg __unused)
{
	unsigned long arg1, arg2, arg3, arg4, arg5, arg6, arg7;
	int i, kq, num;

	if ((kq = kqueue()) < 0)
		err(1, "kqueue()");
	for (i = 0; i < 1000; i++) {
		if (i == 0)
			usleep(1000);
		num = syscallno;
		arg1 = makearg();
		arg2 = makearg();
		arg3 = makearg();
		arg4 = makearg();
		arg5 = makearg();
		arg6 = makearg();
		arg7 = makearg();

#if 0
		fprintf(stderr, "%2d : syscall(%3d, %lx, %lx, %lx, %lx, %lx,"
		   " %lx, %lx)\n",
			i, num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
#endif
		alarm(1);
		syscall(num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
		num = 0;
	}
	close(kq);

	return (0);
}

int
main(void)
{
	struct passwd *pw;
	time_t start;
	pthread_t rp, cp[THREADS];
	int e, j, n;

	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "no such user: nobody");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	signal(SIGALRM, hand);
	signal(SIGILL,  hand);
	signal(SIGFPE,  hand);
	signal(SIGSEGV, hand);
	signal(SIGBUS,  hand);
	signal(SIGURG,  hand);
	signal(SIGSYS,  hand);
	signal(SIGTRAP, hand);

	syscallno = SYS_kevent;

	n = 0;
	start = time(NULL);
	while (time(NULL) - start < 120) {
		if (fork() == 0) {
			if ((e = pthread_create(&rp, NULL, test, NULL)) != 0)
				errc(1, e, "pthread_create");
			usleep(1000);
			for (j = 0; j < THREADS; j++) {
				if ((e = pthread_create(&cp[j], NULL, calls,
				    NULL)) != 0)
					errc(1, e, "pthread_create");
			}
			for (j = 0; j < THREADS; j++)
				pthread_join(cp[j], NULL);
			if ((e = pthread_kill(rp, SIGINT)) != 0)
				errc(1, e, "pthread_kill");
			_exit(0);
		}
		wait(NULL);
		if (n++ > 5000)
			break;
	}

	return (0);
}
