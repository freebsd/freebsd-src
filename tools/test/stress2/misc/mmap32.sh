#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# Bug 223732 - mmap(2) causes unkillable denial of service with specific
# flags
# Test scenario inspired by:  Arto Pekkanen <aksyom@gmail.com>

# Fixed by r326098.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap32.c
mycc -o mmap32 -Wall -Wextra -O0 -g mmap32.c || exit 1
rm -f mmap32.c

$dir/mmap32
s=$?
[ -f mmap32.core -a $s -eq 0 ] &&
    { ls -l mmap32.core; mv mmap32.core /tmp; s=1; }

rm -rf $dir/mmap32
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define N 4096
static int debug; /* set to 1 for debug output */
static uint32_t r[N];

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

static void
fuzz(int arg, void *addr, size_t len, int prot, int flags, int fd,
    off_t offset)
{
	time_t start;
	void *vp;
	int n;

	setproctitle("arg%d", arg);
	n = 0;
	start = time(NULL);
	while (time(NULL) - start < 10) {
		switch (arg) {
		case 1:
			addr = (void *)makearg();
			break;
		case 2:
			len = makearg();
			break;
		case 3:
			prot = makearg();
			break;
		case 4:
			flags = makearg();
			break;
		case 5:
			fd = makearg();
			break;
		case 6:
			offset = makearg() & 0xffff;
			break;
		case 34:
			prot = makearg();
			flags = makearg();
			break;
		default:
			errx(1, "Bad argument %d to %s", arg, __func__);
		}
		vp = mmap(addr, len, prot, flags, fd, offset);
		if (vp != MAP_FAILED) {
			munmap(vp, len);
			n++;
		}
	}
	if (debug != 0 &&n == 0 && arg != 5)
		fprintf(stderr, "%s(%d) failed\n", __func__, arg);
	exit(0);
}

int
main(void)
{
	off_t offset;
	pid_t pid;
	size_t len;
	struct rlimit rl;
	time_t start;
	void *addr, *vp;
	int e, flags, fd, i, prot, status;

	e = 0;

	rl.rlim_max = rl.rlim_cur = 0;
	if (setrlimit(RLIMIT_CORE, &rl) == -1)
		warn("setrlimit");
	addr = 0;
	len = PAGE_SIZE;
	prot = PROT_READ | PROT_WRITE;
	flags = MAP_ANON | MAP_SHARED;
	fd = -1;
	offset = 0;
	vp = mmap(addr, len, prot, flags, fd, offset);
	if (vp == MAP_FAILED)
		err(1, "initail mmap");
	munmap(vp, len);

	start = time(NULL);
	while (time(NULL) - start < 120) {
		for (i = 0; i < N; i++)
			r[i] = arc4random();
		for (i = 0; i < 6; i++) {
			if ((pid = fork()) == 0)
				fuzz(i + 1, addr, len, prot, flags, fd,
				    offset);
			if (waitpid(pid, &status, 0) != pid)
				err(1, "waitpid %d", pid);
			if (status != 0) {
				if (WIFSIGNALED(status))
					fprintf(stderr,
					    "pid %d exit signal %d\n",
					    pid, WTERMSIG(status));
			}
			e += status == 0 ? 0 : 1;
		}
		if ((pid = fork()) == 0)
			fuzz(34, addr, len, prot, flags, fd, offset);
		if (waitpid(pid, &status, 0) != pid)
			err(1, "waitpid %d", pid);
		if (status != 0) {
			if (WIFSIGNALED(status))
				fprintf(stderr,
				    "pid %d exit signal %d\n",
				    pid, WTERMSIG(status));
		}
		e += status == 0 ? 0 : 1;
	}

	return (e);
}
