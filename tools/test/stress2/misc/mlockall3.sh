#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# mlockall(2) test scenario.
# "panic: vm_page_unwire: page xxx's wire count is zero" seen.
# http://people.freebsd.org/~pho/stress/log/mlockall.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mlockall3.c
mycc -o mlockall3 -Wall -Wextra mlockall3.c -lpthread || exit 1
rm -f mlockall3.c
cd $odir

/tmp/mlockall3

killall mlockall3 > /dev/null 2>&1
rm -f /tmp/mlockall3
exit

EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define N (128 * 1024 / (int)sizeof(u_int32_t))
u_int32_t r[N];

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

unsigned long
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

void *
calls(void *arg __unused)
{
	int i;
	unsigned long arg1, arg2, arg3;

	usleep(1000);
	for (i = 0; i < 500; i++) {
		arg1 = makearg();
		arg2 = makearg();
		arg3 = makearg();

		alarm(1);
		syscall(SYS_mlockall, arg1, arg2, arg3);
	}

	return (0);
}

int
main(void)
{
	struct passwd *pw;
	pid_t pid;
	pthread_t cp[50];
	int e, i, j;

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

	alarm(180);
	for (i = 0; i < 8000; i++) {
		if ((pid = fork()) == 0) {
			for (j = 0; j < N; j++)
				r[j] = arc4random();
			for (j = 0; j < 50; j++)
				if ((e = pthread_create(&cp[j], NULL, calls, NULL)) != 0)
					errc(1, e, "pthread_create");

			for (j = 0; j < 50; j++)
				pthread_join(cp[j], NULL);
			_exit(0);
		}
		if (pid == -1)
			err(1, "fork()");
		wait(NULL);
	}

	return (0);
}
