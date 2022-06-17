#!/bin/sh

#
# Copyright (c) 2011-2013 Peter Holm
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

# Usage: syscall4.sh [syscall number]
#	Without an argument random syscall numbers are tested.
#	With an argument only the specified syscall number is tested.

# Sample problems found:
# Thread stuck in stopprof.
# http://people.freebsd.org/~pho/stress/log/kostik732.txt
# Fixed by r275121.

# panic: td 0xcbe1ac40 is not suspended.
# https://people.freebsd.org/~pho/stress/log/kostik807.txt
# Fixed by r282944.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > syscall4.c
sed -i '' -e "s#MNTPOINT#$mntpoint#" syscall4.c
rm -f /tmp/syscall4
mycc -o syscall4 -Wall -Wextra -O2 -g syscall4.c -lpthread || exit 1
rm -f syscall4.c

kldstat -v | grep -q sysvmsg  || $stress2tools/kldload.sh sysvmsg
kldstat -v | grep -q sysvsem  || $stress2tools/kldload.sh sysvsem
kldstat -v | grep -q sysvshm  || $stress2tools/kldload.sh sysvshm
kldstat -v | grep -q aio      || $stress2tools/kldload.sh aio
kldstat -v | grep -q mqueuefs || $stress2tools/kldload.sh mqueuefs

mount | grep -q "on $mntpoint " && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

[ -z "$noswap" ] &&
    daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 10m -i 20 -k)" > \
    /dev/null
sleeptime=${sleeptime:-12}
st=`date '+%s'`
while [ $((`date '+%s'` - st)) -lt $((10 * sleeptime)) ]; do
	(cd $mntpoint; /tmp/syscall4 $* 1>>stdout 2>>stderr) &
	start=`date '+%s'`
	while [ $((`date '+%s'` - start)) -lt $sleeptime ]; do
		pgrep syscall4 > /dev/null || break
		sleep .5
	done
	while pkill -9 syscall4; do :; done
	wait
	ipcs | grep nobody | awk '/^(q|m|s)/ {print " -" $1, $2}' |
	    xargs -L 1 ipcrm
done
while pkill -9 swap; do :; done
while pkill -9 syscall4; do :; done

for i in `jot 10`; do
	mount | grep -q md$mdstart  && \
		umount $mntpoint && mdconfig -d -u $mdstart && break
	sleep 10
done
if mount | grep -q md$mdstart; then
	fstat $mntpoint
	echo "umount $mntpoint failed"
	exit 1
fi
rm -f /tmp/syscall4
exit 0
EOF
#include <sys/types.h>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <pthread.h>
#if defined(__FreeBSD__)
#include <pthread_np.h>
#define	__NP__
#endif
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ignore[] = {
	SYS_syscall,
	SYS_exit,
	SYS_fork,
	11,			/* 11 is obsolete execv */
	SYS_reboot,
	SYS_vfork,
	109,			/* 109 is old sigblock */
	111,			/* 111 is old sigsuspend */
	SYS_shutdown,
	SYS___syscall,
	216,			/* custom syscall */
	SYS_rfork,
	SYS_mac_syscall,
};

static int fd[900], fds[2], kq, socketpr[2];
#ifndef nitems
#define nitems(x) (sizeof((x)) / sizeof((x)[0]))
#endif
#define N 4096
#define MAGIC 1664
#define RUNTIME 120
#define THREADS 50

static uint32_t r[N];
static int magic1, syscallno, magic2;

static int
random_int(int mi, int ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

static void
hand(int i __unused) {	/* handler */
	exit(1);
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
	FTS *fts;
	FTSENT *p;
	time_t start;
	int ftsoptions, i, numfiles;
	char *args[] = {
	    "/dev",
	    "/proc",
	    "MNTPOINT",
	    "mnt2",
	    ".",
	    NULL,
	};

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	numfiles = 0;
	ftsoptions = FTS_PHYSICAL;
	start = time(NULL);
	while (time(NULL) - start < 2) {
		for (i = 0; i < N; i++)
			r[i] = arc4random();

		if (pipe(fds) == -1)
			err(1, "pipe()");
		if (socketpair(PF_UNIX, SOCK_SEQPACKET, 0, socketpr) == -1)
			err(1, "socketpair()");
		kq = kqueue();

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
			if (numfiles++ < 10) {
				fprintf(stderr, "%d: pts_path = %s\n",
				    numfiles, p->fts_path);
			}
		}

		if (fts_close(fts) == -1)
			warn("fts_close()");
		sleep(1);
		close(socketpr[0]);
		close(socketpr[1]);
		close(fds[0]);
		close(fds[1]);
		close(kq);
	}
	return(NULL);
}

static void *
calls(void *arg __unused)
{
	time_t start;
	int i, j, num;
	unsigned long arg1, arg2, arg3, arg4, arg5, arg6, arg7;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif
	start = time(NULL);
	for (i = 0; time(NULL) - start < 10; i++) {
		num = syscallno;
		while (num == 0) {
			num = random_int(0, SYS_MAXSYSCALL);
			for (j = 0; j < (int)nitems(ignore); j++)
				if (num == ignore[j]) {
					num = 0;
					break;
				}
		}
		arg1 = makearg();
		arg2 = makearg();
		arg3 = makearg();
		arg4 = makearg();
		arg5 = makearg();
		arg6 = makearg();
		arg7 = makearg();

#if 0		/* Debug mode */
		fprintf(stderr, "%2d : syscall(%3d, %lx, %lx, %lx, %lx, %lx,"
		   " %lx, %lx)\n",
			i, num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
		sleep(2);
#endif
		alarm(1);
		syscall(num, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
		num = 0;
		if (magic1 != MAGIC || magic2 != MAGIC)
			exit(1);
	}

	return (NULL);
}

int
main(int argc, char **argv)
{
	struct passwd *pw;
	struct rlimit limit;
	pthread_t rp, cp[THREADS];
	time_t start;
	int e, j;

	magic1 = magic2 = MAGIC;
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

	if (argc > 2) {
		fprintf(stderr, "usage: %s [syscall-num]\n", argv[0]);
		exit(1);
	}
	if (argc == 2) {
		syscallno = atoi(argv[1]);
		for (j = 0; j < (int)nitems(ignore); j++)
			if (syscallno == ignore[j])
				errx(0, "syscall #%d is on the ignore list.",
				    syscallno);
	}

	if (daemon(1, 1) == -1)
		err(1, "daemon()");

	system("touch aaa bbb ccc; mkdir -p ddd");
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		if (fork() == 0) {
			if ((e = pthread_create(&rp, NULL, test, NULL)) != 0)
				errc(1, e, "pthread_create");
			usleep(1000);
			for (j = 0; j < THREADS; j++)
				if ((e = pthread_create(&cp[j], NULL, calls,
				    NULL)) != 0)
					errc(1, e, "pthread_create");
			for (j = 0; j < THREADS; j++)
				pthread_join(cp[j], NULL);

			if ((e = pthread_kill(rp, SIGINT)) != 0)
				errc(1, e, "pthread_kill");
			exit(0);
		}
		wait(NULL);
		usleep(10000);
	}

	return (0);
}
