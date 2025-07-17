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

# Test scenario by kib@

[ `uname -m` = "i386" ] || exit 0

. ../default.cfg

grep -q MAP_GUARD /usr/include/sys/mman.h 2>/dev/null || exit 0
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap16.c
mycc -o mmap16 -Wall -Wextra -O2 -g mmap16.c -lpthread || exit 1
rm -f mmap16.c /tmp/mmap16.core

echo "Expect:
    mmap16: mprotect: Permission denied"
/tmp/mmap16 > /dev/null
s=$?

rm -f /tmp/mmap16 /tmp/mmap16.core
exit $s
EOF
/* $Id: map_hole.c,v 1.6 2014/06/16 05:52:03 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

static void
sighandler(int signo, siginfo_t *info, void *uap1)
{
	static char scratch;
	ucontext_t *uap;

	uap = uap1;
	printf("SIG%s(%d) at %p (%%eax %p)\n",
	    signo < sys_nsig ? sys_signame[signo] : "SOME", signo,
	    info->si_addr, (void *)(uintptr_t)uap->uc_mcontext.mc_eax);
	uap->uc_mcontext.mc_eax = (uintptr_t)&scratch;
}

static void
access_addr(char *addr)
{
	char r;

	r = '1';
	printf("accessing %p\n", addr);
	__asm __volatile("movb	%0,(%%eax)" : : "i"(r), "a"(addr) : "memory");
	printf("done\n");
}

static int pagesz;

static void
test_access(char *addr)
{
	struct rusage ru;
	long majflt, minflt;

	if (getrusage(RUSAGE_THREAD, &ru) == -1)
		err(1, "getrusage");
	majflt = ru.ru_majflt;
	minflt = ru.ru_minflt;
	access_addr(addr);
	if (mprotect(addr, pagesz, PROT_READ | PROT_WRITE) == -1)
		warn("mprotect");
	access_addr(addr);
	if (getrusage(RUSAGE_THREAD, &ru) == -1)
		err(1, "getrusage");
	majflt = ru.ru_majflt - majflt;
	minflt = ru.ru_minflt - minflt;
	printf("majflt %ld minflt %ld\n", majflt, minflt);
}

int
main(void)
{
	struct sigaction sa;
	char *addr;
	char cmd[128];

	bzero(&sa, sizeof(sa));
	sa.sa_sigaction = sighandler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL) == -1)
		err(1, "sigaction");
	pagesz = getpagesize();

	printf("MAP_GUARD\n");
	addr = mmap(NULL, pagesz, PROT_NONE, MAP_GUARD, -1, 0);
	if (addr == (char *)MAP_FAILED)
		err(1, "FAIL: mmap(MAP_GUARD)");
	test_access(addr);

	printf("PROT_NONE wire\n");
	addr = mmap(NULL, pagesz, PROT_NONE, MAP_ANON, -1, 0);
	if (addr == (char *)MAP_FAILED)
		err(1, "mmap(PROT_NONE)");
	if (mlock(addr, pagesz) == -1)
		if (errno != ENOMEM)
			err(1, "mlock");
	test_access(addr);

	snprintf(cmd, sizeof(cmd), "procstat -v %d", getpid());
	system(cmd);

	return (0);
}
