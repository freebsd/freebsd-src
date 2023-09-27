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

# Consume VM radix nodes

# "panic: default pager with handle" seen with WiP kernel code.
# https://people.freebsd.org/~pho/stress/log/kostik1243.txt

# "panic: ASan: Invalid access, 8-byte read at ..., MallocRedZone(fb)" seen

[ `sysctl vm.swap_total | sed 's/.* //'` -eq 0 ] && exit 0

. ../default.cfg

log=/tmp/radix.log
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/radix.c
mycc -o radix -Wall -Wextra radix.c || exit 1
rm -f radix.c
cd $odir

set -u
trap "rm -f rendezvous" EXIT INT
parallel=1
usermem=`sysctl hw.usermem | sed 's/.* //'`
pagesize=`pagesize`
start=`date +%s`
while true; do
	timeout 20m /tmp/radix $parallel > $log; s=$?
	[ $s -eq 124 ] && { echo "Timed out"; break; }
	[ $s -ne 0 ] && cat $log
	used=`awk '{print $4}' < $log`
	[ -z "$used" ] && break
	[ $((`date +%s` - start)) -gt 300 ] && break
	[ $used -gt $((usermem / pagesize)) ] && break
	[ $parallel -eq 1 ] &&
	    parallel=$((usermem / pagesize / used))
	parallel=$((parallel + 1))
	[ $parallel -gt 10 ] && parallel=10
done
cat /tmp/radix.log

rm -f /tmp/radix $log
exit $s

EOF
/*
   On Wed, 17 Apr 2013 18:57:00 -0500 alc wrote:

   Suppose that I write a program for i386 that creates giant VM objects,
   perhaps, using shm_open() + ftruncate(), and touches pages 0, 1, 8, 9,
   64, 65, 72, 73, 512, 513, 520, 521, 576, 577, 584, 585, 4096, 4097,
   4104, 4105, ... in each of the VM objects. (The sequence would be
   different on amd64.) I could work around the 32-bit address space
   limitation by mmap(2)ing and munmap(2)ing windows onto a VM object.
   Each of the VM objects would have only one less interior node in the
   radix tree than pages. If I create enough of these VM objects, then I
   can consume all of the available pages and an almost equal number of
   interior nodes. (Maybe it's worth writing this program so that some
   experiments could be done?)
*/

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __LP64__
#define	WIDTH	4
#else
#define	WIDTH	3
#endif
#define	N	(int)howmany(sizeof(uint64_t) * NBBY, WIDTH)

typedef uint64_t state_t[N];

static uint64_t pgs;
static int fds[2];
static int parallel;
static volatile sig_atomic_t s1;
static int ps;

static void
init(state_t state)
{
	int i;

	for (i = 0; i < N; i++)
		state[i] = 0;
}

static uint64_t
generator(state_t state)
{
	uint64_t value;
	int i;

	value = 0;
	for (i = 0; i < N; i++)
		value += state[i] << (i * WIDTH);
	for (i = 0; i < N; i++)
		if (state[i] == 0)
			break;
	if (i < N)
		state[i]++;
	for (i--; i >= 0; i--)
		state[i]--;
	return (value);
}

static int
wr(int fd, off_t pno)
{
	off_t len, offset;
	void *p;

	offset = pno * ps;
	len = ps;
	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NOSYNC,
	    fd, offset);
	if (p == MAP_FAILED) {
		if (errno == ENOMEM)
			return (1);
		err(1, "mmap(len 0x%jx, offset 0x%jx). %s:%d", len, offset,
		    __FILE__, __LINE__);
	}
	*(char *)p = 1;
	pgs++;

	return (0);
}

static void
handler(int s __unused)
{
	s1++;
}

static void
ihandler(int s __unused)
{
	_exit(1);
}

static int
radix(void)
{
	FILE *f;
	int r;

	if ((f = popen("vmstat -z | grep RADIX | awk -F',' '{print $3}'", "r")) == NULL)
		err(1, "popen");
	fscanf(f, "%d", &r);
	pclose(f);

	return (r);
}

static void
test(void)
{
	state_t state;
	off_t offset;
	int fd, i;

	signal(SIGHUP, ihandler);
	for (;;) {
		if (access("rendezvous", R_OK) == 0)
			break;
		usleep(2000);
	}

	if ((fd = open("/dev/zero", O_RDWR)) == -1)
		err(1, "open()");

	init(state);
	offset = generator(state);
	do {
		if (wr(fd, offset) != 0)
			break;
		offset = generator(state);
	} while (offset != 0);

	if (write(fds[1], &pgs, sizeof(pgs)) != sizeof(pgs))
		err(1, "ewrite pipe");
	kill(getppid(), SIGHUP);
	for (i = 0; i < 180; i++)
		sleep(1);
	close(fd);

	_exit(0);
}

int
main(int argc, char **argv)
{
	uint64_t pages;
	pid_t *pids;
	int i, r1, r2, rfd;

	if (argc != 2)
		errx(1, "Usage: %s <number of parallel processes>.", argv[0]);
	parallel = atoi(argv[1]);

	ps = getpagesize();
	signal(SIGALRM, ihandler);
	signal(SIGHUP, handler);
	unlink("rendezvous");
	pids = malloc(parallel * sizeof(pid_t));
	if (pipe(fds) == -1)
		err(1, "pipe");
	r1 = radix();
	for (i = 0; i < parallel; i++) {
		if ((pids[i] = fork()) == 0)
			test();
	}
	if ((rfd = open("rendezvous", O_CREAT, 0644)) == -1)
		err(1, "open()");
	close(rfd);
	alarm(300);
	while (s1 != parallel) {
		usleep(10000);
	}
	r2 = radix();
	pages = 0;
	for (i = 0; i < parallel; i++) {
		kill(pids[i], SIGHUP);
		if (read(fds[0], &pgs, sizeof(pgs)) != sizeof(pgs))
			err(1, "read pipe");
		pages += pgs;
	}
	fprintf(stdout, "A total of %jd pages (%.1f MB) touched, %d"
	    " RADIX nodes used, p/r = %.1f, parallel = %d.\n",
	    pages, pages * ps / 1024. / 1024, r2 - r1,
	    pages / (r2 - r1 + 0.), parallel);

	for (i = 0; i < parallel; i++) {
		wait(NULL);
	}
	unlink("rendezvous");
	return (0);
}
