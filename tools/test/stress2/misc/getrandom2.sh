#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm
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

# getrandom(2) DoS scenario.

# panic: pmap_growkernel: no memory to grow kernel
# cpuid = 8
# time = 1582102582
# KDB: stack backtrace:
# db_trace_self_wrapper() at db_trace_self_wrapper+0x2b/frame 0xfffffe03e6992450
# vpanic() at vpanic+0x185/frame 0xfffffe03e69924b0
# panic() at panic+0x43/frame 0xfffffe03e6992510
# pmap_growkernel() at pmap_growkernel+0x2d4/frame 0xfffffe03e6992550
# vm_map_insert() at vm_map_insert+0x296/frame 0xfffffe03e69925f0
# vm_map_find() at vm_map_find+0x617/frame 0xfffffe03e69926d0
# kva_import() at kva_import+0x3c/frame 0xfffffe03e6992710
# vmem_try_fetch() at vmem_try_fetch+0xde/frame 0xfffffe03e6992760
# vmem_xalloc() at vmem_xalloc+0x4bb/frame 0xfffffe03e69927e0
# kva_import_domain() at kva_import_domain+0x36/frame 0xfffffe03e6992810
# vmem_try_fetch() at vmem_try_fetch+0xde/frame 0xfffffe03e6992860
# vmem_xalloc() at vmem_xalloc+0x4bb/frame 0xfffffe03e69928e0
# vmem_alloc() at vmem_alloc+0x8a/frame 0xfffffe03e6992930
# kmem_malloc_domainset() at kmem_malloc_domainset+0x92/frame 0xfffffe03e69929a0
# malloc() at malloc+0x162/frame 0xfffffe03e69929f0
# read_random_uio() at read_random_uio+0xa5/frame 0xfffffe03e6992a40
# sys_getrandom() at sys_getrandom+0x7b/frame 0xfffffe03e6992ac0
# amd64_syscall() at amd64_syscall+0x183/frame 0xfffffe03e6992bf0
# fast_syscall_common() at fast_syscall_common+0x101/frame 0xfffffe03e6992bf0
# --- syscall (563, FreeBSD ELF64, sys_getrandom), rip = 0x80041899a, rsp = 0x7ffffffc3cb8, rbp = 0x7ffffffc3cd0 ---
# KDB: enter: panic
# [ thread pid 12095 tid 186584 ]
# Stopped at      kdb_enter+0x37: movq    $0,0x1084916(%rip)
# db> x/s version
# version:        FreeBSD 13.0-CURRENT #0 r358094: Wed Feb 19 06:25:16 CET 2020\012    pho@t2.osted.lan:/usr/src/sys/amd64/compile/PHO\012
# db>

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/getrandom2.c
mycc -o getrandom2 -Wall -Wextra -O0 -g getrandom2.c || exit 1
rm -f getrandom2.c
cd $odir

cd /tmp
$dir/getrandom2
s=$?
[ -f getrandom2.core -a $s -eq 0 ] &&
    { ls -l getrandom2.core; s=1; }
cd $odir

rm -rf $dir/getrandom2
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static size_t mx;
static _Atomic(int) *share;
static int parallel;
static char *bp;

#define PARALLEL 40000   /* Arbitrary cap */
#define SYNC 0

static void
test(void)
{
	int i;

	alarm(180);
	(void)atomic_fetch_add(&share[SYNC], 1);
	while (atomic_load(&share[SYNC]) != parallel)
		usleep(200000);
	for (i = 0; i < 10; i++)
		getrandom(bp, mx, 0);
//		close(66);

	_exit(0);
}

int
main(void)
{
	pid_t *pids;
	struct rlimit rlp;
	size_t len;
        size_t f, vsz;
        u_int pages;
	int e, i, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	if (getrlimit(RLIMIT_NPROC, &rlp) < 0)
		err(1, "getrlimit");
	parallel = rlp.rlim_cur / 100 * 80;
	if (parallel > PARALLEL)
		parallel = PARALLEL;
	pids = calloc(parallel, sizeof(pid_t));

        vsz = sizeof(pages);
        if (sysctlbyname("vm.stats.vm.v_free_count", &pages, &vsz, NULL, 0) != 0)
                err(1, "sysctl(vm.stats.vm.v_free_count)");
	f = pages;
	f *= PAGE_SIZE;

	if (getrlimit(RLIMIT_DATA, &rlp) < 0)
		err(1,"getrlimit");
	mx = rlp.rlim_cur;
	if (mx > f / parallel)
		mx = f / parallel;
	if ((bp = mmap(NULL, mx, PROT_READ | PROT_WRITE, MAP_ANON, -1,
	    0)) == MAP_FAILED)
		err(1, "mmap");
	for (;;) {
		if (getrandom(bp, mx, 0) != -1)
			break;
		mx = mx / 2;
	}
	printf("Max getrandom() buffer size is %zu, %d threads\n", mx,
	    parallel);
	for (i = 0; i < parallel; i++) {
		if ((pids[i] = fork()) == 0)
			test();
		if (pids[i] == -1)
			err(1, "fork()");
	}
	for (i = 0; i < parallel; i++) {
		if (waitpid(pids[i], &status, 0) == -1)
			err(1, "waitpid(%d)", pids[i]);
		if (status != 0) {
			if (WIFSIGNALED(status))
				fprintf(stderr,
				    "pid %d exit signal %d\n",
				    pids[i], WTERMSIG(status));
		}
		e += status == 0 ? 0 : 1;
	}

	return (e);
}
