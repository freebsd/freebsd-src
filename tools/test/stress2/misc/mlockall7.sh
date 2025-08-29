#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Variation of mmap18.sh.
# "panic: vm_page_unwire: page 0xfffff81038d721f0's wire count is zero" seen:
# https://people.freebsd.org/~pho/stress/log/mlockall7.txt
# Fixed by r328880

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mlockall7.c
mycc -o mlockall7 -Wall -Wextra -O2 mlockall7.c -lpthread || exit 1
rm -f mlockall7.c

/tmp/mlockall7 `[ $# -eq 0 ] && echo 1 || echo $1` || s=1

sleep 2
rm -f /tmp/mlockall7 /tmp/mlockall7.core
exit $s
EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define N 4096
#define PARALLEL 50
#define RUNTIME 180

static u_int32_t r[N];
static void *p;

static unsigned long
makearg(void)
{
	unsigned long val;
	unsigned int i;

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

	return (val);
}

static void *
tmmap(void *arg __unused)
{
	size_t len;
	time_t start;

	pthread_set_name_np(pthread_self(), __func__);
	len = 128LL * 1024 * 1024;

	start = time(NULL);
	while (time(NULL) - start < 60) {
		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON,
		    -1, 0)) != MAP_FAILED) {
			usleep(100);
			munmap(p, len);
		}
	}

	return (NULL);
}

static void *
tmlockall(void *arg __unused)
{
	time_t start;
	int flags;

	pthread_set_name_np(pthread_self(), __func__);
	start = time(NULL);
	while (time(NULL) - start < 60) {
		flags = makearg() & 0xff;
		mlockall(flags);
		usleep(100);
		munlockall();
		usleep(1000);
	}

	return (NULL);
}

static void
test(void)
{
	pthread_t tid[2];
	time_t start;
	int i, rc;

	if ((rc = pthread_create(&tid[0], NULL, tmmap, NULL)) != 0)
		errc(1, rc, "tmmap()");
	if ((rc = pthread_create(&tid[1], NULL, tmlockall, NULL)) != 0)
		errc(1, rc, "tmlock()");

	start = time(NULL);
	while (time(NULL) - start < 60) {
		if (fork() == 0) {
			usleep(10000);
			_exit(0);
		}
		wait(NULL);
	}

	for (i = 0; i < 2; i++)
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join");
	_exit(0);
}

int
testing(unsigned long maxl)
{
	struct passwd *pw;
	struct rlimit rl;
	rlim_t maxlock;
	time_t start;
	int i;

	maxlock = maxl;
	if ((pw = getpwnam("nobody")) == NULL)
		err(1, "failed to resolve nobody");
	if (setgroups(0, NULL) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		err(1, "Can't drop privileges to \"nobody\"");
	endpwent();

	rl.rlim_max = rl.rlim_cur = 0;
	if (setrlimit(RLIMIT_CORE, &rl) == -1)
		warn("setrlimit");

	if (getrlimit(RLIMIT_MEMLOCK, &rl) == -1)
		warn("getrlimit");
	if (maxlock <= 0)
		errx(1, "Argument is %jd", maxlock);
	maxlock = (maxlock / 10 * 8) / PARALLEL * PAGE_SIZE;
	if (maxlock < rl.rlim_cur) {
		rl.rlim_max = rl.rlim_cur = maxlock;
		if (setrlimit(RLIMIT_MEMLOCK, &rl) == -1)
			warn("setrlimit");
	}

	for (i = 0; i < N; i++)
		r[i] = arc4random();

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				test();
		}

		for (i = 0; i < PARALLEL; i++)
			wait(NULL);
	}

	_exit(0);
}

int
main(int argc, char *argv[])
{
	pid_t pid;
	size_t len;
	int i, loops, s, status;
	unsigned long max_wired;
	unsigned int wire_count, wire_count_old;

	s = 0;
	if (argc != 2)
		errx(1, "Usage: %s <loops>", argv[0]);
	loops = atoi(argv[1]);

	len = sizeof(max_wired);
	if (sysctlbyname("vm.max_user_wired", &max_wired, &len, NULL, 0) != 0)
		err(1, "vm.max_user_wired");

	len = sizeof(wire_count);
	if (sysctlbyname("vm.stats.vm.v_user_wire_count", &wire_count, &len,
	    NULL, 0) != 0)
		err(1, "vm.stats.vm.v_user_wire_count");

	for (i = 0; i < loops; i++) {
		wire_count_old = wire_count;

		if ((pid = fork()) == 0)
			testing(max_wired);
		if (waitpid(pid, &status, 0) != pid)
			err(1, "waitpid(%d)", pid);
		if (status != 0)
			errx(1, "Exit status %d from pid %d\n", status, pid);

		len = sizeof(wire_count);
		if (sysctlbyname("vm.stats.vm.v_user_wire_count", &wire_count, &len,
		    NULL, 0) != 0)
			err(1, "vm.stats.vm.v_user_wire_count");
		fprintf(stderr, "vm.stats.vm.v_user_wire_count was %d, is %d. %d\n",
		    wire_count_old, wire_count, wire_count - wire_count_old);
	}

	return (s);
}
