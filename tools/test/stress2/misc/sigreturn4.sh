#!/bin/sh

# panic: vm_fault_lookup: fault on nofault entry, addr: 0
# cpuid = 2
# time = 1661698922
# KDB: stack backtrace:
# db_trace_self_wrapper(b,2931e740,2931e742,ffc0ddb8,190431,...) at db_trace_self_wrapper+0x28/frame 0xffc0dd24
# vpanic(150acba,ffc0dd60,ffc0dd60,ffc0de20,12cc155,...) at vpanic+0xf4/frame 0xffc0dd40
# panic(150acba,14ec1ab,0,146253d,1430,...) at panic+0x14/frame 0xffc0dd54
# vm_fault(1e360c8,0,4,0,0) at vm_fault+0x1725/frame 0xffc0de20
# vm_fault_trap(1e360c8,3b,4,0,0,0) at vm_fault_trap+0x52/frame 0xffc0de48
# trap_pfault(3b,0,0) at trap_pfault+0x176/frame 0xffc0de94
# trap(ffc0df6c,8,28,28,19156000,...) at trap+0x2d9/frame 0xffc0df60
# calltrap() at 0xffc031ef/frame 0xffc0df60
# --- trap 0xc, eip = 0x3b, esp = 0xffc0dfac, ebp = 0xffc0340c ---
# (null)() at 0x3b/frame 0xffc0340c
# KDB: enter: panic
# [ thread pid 54680 tid 102765 ]
# Stopped at      kdb_enter+0x34: movl    $0,kdb_why
# db> x/s version
# version:        FreeBSD 14.0-CURRENT #0 main-n257606-9ea2716b775-dirty: Thu Aug 25 10:47:45 CEST 2022
# pho@mercat1.netperf.freebsd.org:/media/ob
# j/usr/src/i386.i386/sys/PHO\012
# db> show proc
# Process 54680 (date) at 0x28905d50:
#  state: NORMAL
#  uid: 0  gids: 0, 0, 5
#  parent: pid 785 at 0x26c14000
#  ABI: FreeBSD ELF32
#  flag: 0x10004002  flag2: 0
#  arguments: date +%s
#  reaper: 0x18c710a4 reapsubtree: 1
#  sigparent: 20
#  vmspace: 0x29332100
#    (map 0x29332100)
#    (map.pmap 0x29332174)
#    (pmap 0x293321b0)
#  threads: 1
# 102765                   Run     CPU 2               date
# db>

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
prog=sigreturn4

cat > /tmp/$prog.c <<EOF
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <ucontext.h>
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

#define RUNTIME 120
#define THREADS 1

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

static long
random_long(long mi, long ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

static void
flip(void *ap, size_t len)
{
	unsigned char *cp;
	int byte;
	unsigned char bit, buf, mask, old __unused;

	cp = (unsigned char *)ap;
	byte = random_long(0, len);
	bit = random_long(0,7);
	mask = ~(1 << bit);
	buf = cp[byte];
	old = cp[byte];
	buf = (buf & mask) | (~buf & ~mask);
	cp[byte] = buf;
}

static void *
churn(void *arg __unused)
{
	time_t start;

	start = time(NULL);
	while (time(NULL) - start < 10) {
		usleep(100);
	}
	return(NULL);
}

static void *
calls(void *arg __unused)
{
	time_t start;
	ucontext_t uc;
	int i, n;

	start = time(NULL);
	for (i = 0; time(NULL) - start < 10; i++) {
		n = 0;
		if (getcontext(&uc) == -1)
			err(1, "getcontext");
		n++;

		if (n == 1) {
			flip(&uc, sizeof(uc));
			alarm(1);
			if (sigreturn(&uc) == -1)
				err(1, "sigreturn()");
		} else
			break;
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
		fprintf(stderr, "Running sigreturn4 as root for %s.\n",
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
