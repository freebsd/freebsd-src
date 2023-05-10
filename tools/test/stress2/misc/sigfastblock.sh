#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Konstantin Belousov
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

# sigfastblock test scenario by Konstantin Belousov <kib@FreeBSD.org>

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -p` != "amd64" ] && exit 0

cat > /tmp/sigfastblock.c <<EOF
/* $Id: sigfastblock1.c,v 1.4 2020/02/16 19:53:14 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __ILP32
#define	BITNESS	"32"
#else
#define	BITNESS	"64"
#endif

int block;

static void
sighandler(int signo, siginfo_t *si, void *ucp __unused)
{
	printf("block %#x sig %d si_code %d si_pid %d\n",
	    block, signo, si->si_code, si->si_pid);
}

int
main(void)
{
	struct timespec rqts;
	struct sigaction sa;
	int error, val;
	size_t valsize;
	pid_t child;

	valsize = sizeof(val);
	error = sysctlbyname("kern.elf" BITNESS ".sigfastblock", &val,
	    &valsize, NULL, 0);
	if (error != 0)
		err(1, "sigfastblock sysctl");
	if (val != 0)
		errx(1, "sigfastblock use in rtld must be turned off");

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sighandler;
	sa.sa_flags = SA_SIGINFO;
	error = sigaction(SIGUSR1, &sa, NULL);

	child = fork();
	if (child == -1)
		err(1, "fork");
	if (child == 0) {
		sleep(2);
		kill(getppid(), SIGUSR1);
		_exit(0);
	}

	error = __sys_sigfastblock(SIGFASTBLOCK_SETPTR, &block);
	if (error != 0)
		err(1, "sigfastblock setptr");

	printf("Registered block at %p\n", &block);
	block = SIGFASTBLOCK_INC;
	rqts.tv_sec = 4;
	rqts.tv_nsec = 0;
	errno = 0;
	error = nanosleep(&rqts, 0);
	printf("nanosleep 1 returned %d, errno %d, block %#x\n",
	    error, errno, block);
	raise(SIGUSR1);
	errno = 0;
	error = nanosleep(&rqts, 0);
	printf("nanosleep 2 returned %d, errno %d, block %#x\n",
	    error, errno, block);

	errno = 0;
	block -= SIGFASTBLOCK_INC;
	error = __sys_sigfastblock(SIGFASTBLOCK_UNBLOCK, NULL);
	printf("unblock returned %d, errno %d, block %#x\n",
	    error, errno, block);
}

EOF
mycc -o /tmp/sigfastblock -Wall -Wextra -O2 -g /tmp/sigfastblock.c || exit 1

echo "Expect:
Registered block at 0x2030c0
nanosleep 1 returned -1, errno 4, block 0x11
nanosleep 2 returned 0, errno 0, block 0x11
unblock returned 0, errno 0, block 0
block 0 sig 30 si_code 65543 si_pid 1151
block 0 sig 30 si_code 65537 si_pid 1152
"

cd /tmp
sysctl kern.elf64.sigfastblock=0
sysctl kern.elf32.sigfastblock=0
./sigfastblock; s=$?
sysctl kern.elf64.sigfastblock=1
sysctl kern.elf32.sigfastblock=1

rm -f /tmp/sigfastblock.c /tmp/sigfastblock
exit $s
