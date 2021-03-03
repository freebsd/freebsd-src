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

# "panic: smp_tlb_shootdown: interrupts disabled" seen.
# http://people.freebsd.org/~pho/stress/log/freebsd4_sigreturn.txt
# Fixed in r251033.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sigreturn.c
mycc -o sigreturn -Wall -Wextra sigreturn.c -lpthread || exit 1
rm -f sigreturn.c
cd $odir

/tmp/sigreturn

rm -f /tmp/sigreturn ./sigreturn.core
exit
EOF
#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

unsigned long buf[] = {
0xe8243489, 0x000e7cc9, 0x85204e8b, 0xc0940fc9, 0x0fc2950f, 0x5589d2b6,
0x56bf0fe4, 0xe855890e, 0x890fd285, 0x0000014a, 0x850fc084, 0x00000142,
0x891c468b, 0xd1ff2404, 0x00012ee9, 0x04b3e800, 0x388bffff, 0xe8243489,
0xfffefbd9, 0xff04a4e8, 0xe93889ff, 0x00000343, 0x0fc08566, 0x00031f84,
0x46bf0f00, 0x2404890e, 0x042444c7, 0x00000003, 0xfefb90e8, 0x85c789ff,
0x0d880fff, 0x89000002, 0x03e083f8, 0x7402f883, 0xf04d8b0e, 0x3903e183,
0xe1850fc8, 0xf6000002, 0x74080c46, 0x24348908, 0x0e7c2ce8, 0xf0458b00,
0xf931c189, 0x7408c1f6, 0xf7e7832b, 0x0908e083, 0x4ebf0ff8, 0x2444890e,
0x240c8908, 0x042444c7, 0x00000004, 0xfefb34e8, 0x0fc085ff, 0x0001b388,
0xf0458b00, 0x7404c4f6, 0x46bf0f1f, 0x2404890e, 0x082444c7, 0x00000000,
0x042444c7, 0x00000000, 0xff0898e8, 0xf0458bff, 0x237508a8, 0xc7243489,
0x000c2444, 0xc7000000, 0x00082444, 0xc7000000, 0x00042444, 0xe8000000,
0x000e86ad, 0xa9f0458b, 0x00100000, 0xbf0f1c74, 0x04890e46, 0x2444c724,
0x00000108, 0x2444c700, 0x00000204, 0xfabbe800, 0xbf0ffffe, 0x45c70e7e,
0xffffffe8, 0xe445c7ff, 0x00000000, 0xc76634eb, 0x00200c46, 0xffe845c7
};

#if !defined(SYS_freebsd4_sigreturn)
#define SYS_freebsd4_sigreturn 344
#endif

static void
hand(int i __unused) {	/* handler */
	_exit(1);
}

void *
calls(void *arg __unused)
{
	alarm(1);
	syscall(SYS_freebsd4_sigreturn, buf);

	return (0);
}

int
main(void)
{
	pthread_t cp;
	int e;

	signal(SIGALRM, hand);
	signal(SIGILL,  hand);
	signal(SIGFPE,  hand);
	signal(SIGSEGV, hand);
	signal(SIGBUS,  hand);
	signal(SIGURG,  hand);
	signal(SIGSYS,  hand);
	signal(SIGTRAP, hand);

	if (fork() == 0) {
		if ((e = pthread_create(&cp, NULL, calls, NULL)) != 0)
			errc(1, e, "pthread_create");

		pthread_join(cp, NULL);
		_exit(0);
	}
	wait(NULL);

	return (0);
}
