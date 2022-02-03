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

# Page fault seen
# http://people.freebsd.org/~pho/stress/log/kostik654.txt
# Fixed by r259521.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > fifo2.c
rm -f /tmp/fifo2
mycc -o fifo2 -Wall -Wextra -O2 -g fifo2.c -lpthread || exit 1
rm -f fifo2.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1
bsdlabel -w md$mdstart auto
newfs $newfs_flags md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint
chmod 777 $mntpoint
mkfifo $mntpoint/f
chmod 777 $mntpoint/f

sleeptime=12
st=`date '+%s'`
while [ $((`date '+%s'` - st)) -lt $((10 * sleeptime)) ]; do
	(cd $mntpoint; /tmp/fifo2) &
	start=`date '+%s'`
	while [ $((`date '+%s'` - start)) -lt $sleeptime ]; do
		pgrep -q fifo2 || break
		sleep .5
	done
	while pkill -9 fifo2; do :; done
	wait
done

for i in `jot 10`; do
	mount | grep -q md${mdstart}$part  && \
		umount $mntpoint > /dev/null 2>&1 &&
		    mdconfig -d -u $mdstart && break
	sleep 10
done
s=0
mount | grep -q md${mdstart}$part &&
    { echo "umount $mntpoint failed"; s=1; }
rm -f /tmp/fifo2
exit $s
EOF
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N (128 * 1024 / (int)sizeof(u_int32_t))
u_int32_t r[N];

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
calls(void *arg __unused)
{
	unsigned long arg1, arg2, arg3, arg4, arg5, arg6, arg7;
	int i;

	for (i = 0;; i++) {
		arg1 = (unsigned long)(void *)"f";
		arg2 = makearg();
		arg3 = makearg();
		arg4 = makearg();
		arg5 = makearg();
		arg6 = makearg();
		arg7 = makearg();

#if 0
		fprintf(stderr, "%2d : syscall(%3d, %lx, %lx, %lx, %lx, %lx, %lx, %lx)\n",
			i, SYS_open, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
		usleep(100000);
#endif
		alarm(1);
		syscall(SYS_open, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	}

	return (0);
}

int
main(void)
{
	struct passwd *pw;
	struct rlimit limit;
	pthread_t cp[50];
	time_t start;
	int e, j;

	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "no such user: nobody");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	limit.rlim_cur = limit.rlim_max = 1000;
	if (setrlimit(RLIMIT_NPTS, &limit) < 0)
		err(1, "setrlimit");

	signal(SIGALRM, hand);
	signal(SIGILL,  hand);
	signal(SIGFPE,  hand);
	signal(SIGSEGV, hand);
	signal(SIGBUS,  hand);
	signal(SIGURG,  hand);
	signal(SIGSYS,  hand);
	signal(SIGTRAP, hand);

	start = time(NULL);
	while ((time(NULL) - start) < 120) {
		if (fork() == 0) {
			for (j = 0; j < 1; j++)
				if ((e = pthread_create(&cp[j], NULL, calls, NULL)) != 0)
					errc(1, e,"pthread_create");

			for (j = 0; j < 1; j++)
				pthread_join(cp[j], NULL);
			_exit(0);
		}
		wait(NULL);
	}

	return (0);
}
