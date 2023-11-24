#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# Fatal trap -4186856: UNKNOWN while in kernel mode
# cpuid = 1; apic id = 01
# error code              = 0xfbafcf8c
# instruction pointer     = 0x79e4:0x4
# stack pointer           = 0x28:0xffc0aff0
# frame pointer           = 0x28:0x204620d4
# code segment            = base 0x0, limit 0x0, type 0x0
#                         = DPL 0, pres 0, def32 0, gran 0
# processor eflags        = trace trap,  at 0x3b/frame 0xffc0340c
# KDB: enter: panic
# [ thread pid 15631 tid 114622 ]
# Stopped at      kdb_enter+0x34: movl    $0,kdb_why
# db>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
prog=sigreturn3

cat > /tmp/$prog.c <<EOF
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N 4096
#define RUNTIME 120
#define THREADS 1

static uint32_t r[N];

static void
hand(int i __unused) {	/* handler */
	exit(1);
}

static void *
churn(void *arg __unused)
{
	time_t start;

	pthread_set_name_np(pthread_self(), __func__);
	start = time(NULL);
	while (time(NULL) - start < 10) {
		arc4random_buf(r, sizeof(r));
		usleep(100);
	}
	return(NULL);
}

static void *
calls(void *arg __unused)
{
	time_t start;
	int i;

	start = time(NULL);
	for (i = 0; time(NULL) - start < 10; i++) {
		arc4random_buf(r, sizeof(r));
		alarm(1);
		syscall(SYS_sigreturn, r);
	}

	return (NULL);
}

int
main(int argc, char **argv)
{
	struct passwd *pw;
	struct rlimit limit;
	pid_t pid;
	pthread_t rp, cp[THREADS];
	time_t start;
	int e, j;

	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "failed to resolve nobody");

	if (getenv("USE_ROOT") && argc == 2)
		fprintf(stderr, "Running syscall4 as root for %s.\n",
				argv[1]);
	else {
		if (setgroups(1, &pw->pw_gid) ||
		    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
		    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
			err(1, "Can't drop privileges to \"nobody\"");
		endpwent();
	}

	limit.rlim_cur = limit.rlim_max = 1000;
#if defined(RLIMIT_NPTS)
	if (setrlimit(RLIMIT_NPTS, &limit) < 0)
		err(1, "setrlimit");
#endif

	signal(SIGALRM, hand);
	signal(SIGILL,  hand);
	signal(SIGFPE,  hand);
	signal(SIGSEGV, hand);
	signal(SIGBUS,  hand);
	signal(SIGURG,  hand);
	signal(SIGSYS,  hand);
	signal(SIGTRAP, hand);

	if (daemon(1, 1) == -1)
		err(1, "daemon()");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		if ((pid = fork()) == 0) {
			if ((e = pthread_create(&rp, NULL, churn, NULL)) != 0)
			errc(1, e, "pthread_create");
			for (j = 0; j < THREADS; j++)
				if ((e = pthread_create(&cp[j], NULL, calls,
				    NULL)) != 0)
					errc(1, e, "pthread_create");
			for (j = 0; j < THREADS; j++)
				pthread_join(cp[j], NULL);

			if ((e = pthread_kill(rp, SIGINT)) != 0)
				errc(1, e, "pthread_kill");
			if ((e = pthread_join(rp, NULL)) != 0)
				errc(1, e, "pthread_join");
			_exit(0);
		}
		waitpid(pid, NULL, 0);
	}

	return (0);
}
EOF

cd /tmp
cc -o $prog -Wall -Wextra -O0 $prog.c -lpthread || exit 1
start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	./$prog > /dev/null 2>&1
	date +%T
done
rm -f /tmp/$prog /tmp/$ptog.c /tmp/$prog.core
exit 0
