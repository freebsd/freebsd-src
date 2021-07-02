#!/bin/sh

# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=227041

# panic: [pmc,2749] (ri21, rc1) waiting too long for pmc to be free
# cpuid = 23
# time = 1624293771
# KDB: stack backtrace:
# db_trace_self_wrapper() at db_trace_self_wrapper+0x2b/frame 0xfffffe01ab083850
# vpanic() at vpanic+0x181/frame 0xfffffe01ab0838a0
# panic() at panic+0x43/frame 0xfffffe01ab083900
# pmc_wait_for_pmc_idle() at pmc_wait_for_pmc_idle+0xa2/frame 0xfffffe01ab083930
# pmc_release_pmc_descriptor() at pmc_release_pmc_descriptor+0x20b/frame 0xfffffe01ab083980
# pmc_syscall_handler() at pmc_syscall_handler+0x4bd/frame 0xfffffe01ab083ac0
# amd64_syscall() at amd64_syscall+0x762/frame 0xfffffe01ab083bf0
# fast_syscall_common() at fast_syscall_common+0xf8/frame 0xfffffe01ab083bf0
# --- syscall (0, FreeBSD ELF64, nosys), rip = 0x8009c154a, rsp = 0x7fffffffe4d8, rbp = 0x7fffffffe500 ---
# KDB: enter: panic
# [ thread pid 3123 tid 100454 ]
# Stopped at      kdb_enter+0x37: movq    $0,0x1285d1e(%rip)
# db> x/s version
# version: FreeBSD 14.0-CURRENT #1 ufs-n247476-f77f86ecfea: Mon Jun 21 08:40:40 CEST 2021
# pho@t2.osted.lan:/var/tmp/deviant3/sys/amd64/compile/PHO
# db>

. ../default.cfg
cat > /tmp/pmc-crash.c <<EOF
/*
	Copyright (c) 2018 Dominic Dwyer <dom@itsallbroken.com>
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.

	clang -lpmc pmc-crash.c -o pmc-crash
*/

#include <sys/types.h>
#include <errno.h>
#include <pmc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	if (pmc_init() != 0)
	{
		printf("pmc_init %s", strerror(errno));
		exit(-1);
	}

	pmc_id_t pmc_id = 0;
	if (pmc_allocate("inst_retired.any", PMC_MODE_TC, 0, PMC_CPU_ANY,
	    &pmc_id, 1024) != 0)
	{
		printf("pmc_allocate %s", strerror(errno));
		exit(-1);
	}

	printf("attaching to self\n");
	if (pmc_attach(pmc_id, 0) != 0)
	{
		printf("pmc_attach %s", strerror(errno));
		exit(-1);
	}

	if (pmc_start(pmc_id) != 0)
	{
		printf("pmc_start %s", strerror(errno));
		exit(-1);
	}

	pmc_value_t v = 0;
	for (int i = 0; i < 100; i++)
	{
		pmc_read(pmc_id, &v);
	}
	pmc_stop(pmc_id);

	printf("success, detaching...\n");
	pmc_detach(pmc_id, 0);
	pmc_release(pmc_id);

	printf("ok?");
}
EOF
mycc -o /tmp/pmc-crash -Wall -Wextra -O0 /tmp/pmc-crash.c -lpmc || exit
kldstat -v | grep -q hwpmc  || { kldload hwpmc; loaded=1; }
here=`pwd`
cd /tmp
./pmc-crash; s=$?
cd $here
rm -f /tmp/pmc-crash /tmp/pmc-crash.core /tmp/pmc-crash.c
[ $loaded ] && kldunload hwpmc
exit $s
