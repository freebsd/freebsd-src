#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Konstantin Belousov <kib@FreeBSD.org>
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

# Test scenario for "D12023: Make WRFSBASE and WRGSBASE functional."

[ `uname -m` = "amd64" ] || exit 0

. ../default.cfg

cat > /tmp/rdgsbase.c <<EOF
/*
Below is the updated version of the test program.  It is probably most
useful to run it in parallel with the normal stress jobs.  Please note
that the program starts two CPU-intensive threads per core, so the
whole test load would be run 4x slower.

Also, it is very useful to try to run hwpmc measurements in parallel, at
least for some time.  E.g.,
	pmcstat -S instructions -T
in a terminal in parallel with other activities is enough.  I do not need
any numbers from hwpmc, just the fact that the driver causes the series of
NMI to adjust the load for testing.
*/

/* $Id: rdgsbase.c,v 1.9 2017/08/13 16:22:01 kostik Exp kostik $ */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <machine/sysarch.h>
#include <machine/specialreg.h>
#include <machine/cpufunc.h>

void
hand(int i __unused) {  /* handler */
	_exit(0);
}

static void *
rungs(void *arg __unused)
{
	volatile char x[1024];
	unsigned i;
	uint64_t y, oldbase;

	oldbase = rdgsbase();
	for (i = 0;;) {
		wrgsbase((uintptr_t)&x[i]);
		if (rdgsbase() != (uintptr_t)&x[i]) {
			wrgsbase(oldbase);
			printf("bug1 %lx %lx\n", rdgsbase(), (uintptr_t)&x[i]);
			exit(1);
		}
		sysarch(AMD64_GET_GSBASE, &y);
		if (y != (uintptr_t)&x[i]) {
			wrgsbase(oldbase);
			printf("bug2 %lx %lx\n", y, (uintptr_t)&x[i]);
			exit(1);
		}
		i++;
		if (i >= nitems(x))
			i = 0;
	}
	return (NULL);
}

static void *
runfs(void *arg __unused)
{
	volatile char x[1024];
	unsigned i;
	uint64_t y, oldbase;

	oldbase = rdfsbase();
	for (i = 0;;) {
		wrfsbase((uintptr_t)&x[i]);
		if (rdfsbase() != (uintptr_t)&x[i]) {
			wrfsbase(oldbase);
			printf("bug3 %lx %lx\n", rdfsbase(), (uintptr_t)&x[i]);
			exit(1);
		}
		sysarch(AMD64_GET_FSBASE, &y);
		if (y != (uintptr_t)&x[i]) {
			wrfsbase(oldbase);
			printf("bug4 %lx %lx\n", y, (uintptr_t)&x[i]);
			exit(1);
		}
		i++;
		if (i > nitems(x))
			i = 0;
	}
	return (NULL);
}

static void
start(int nthreads)
{
	pthread_t thrs[nthreads * 2];
	int error, i;

	for (i = 0; i < nthreads; i++) {
		error = pthread_create(&thrs[i], NULL, rungs, NULL);
		if (error != 0)
			errc(1, error, "pthread_create");
	}
	for (; i < 2 * nthreads; i++) {
		error = pthread_create(&thrs[i], NULL, runfs, NULL);
		if (error != 0)
			errc(1, error, "pthread_create");
	}
}

int
main(void)
{
	static const int mib[2] = {CTL_HW, HW_NCPU};
	int error, nthreads;
	u_int p[4];
	size_t len;

	do_cpuid(0, p);
	if (p[0] < 0x7) {
		fprintf(stderr, "CPU does not support extended functions\n");
		return (1);
	}
	cpuid_count(0x7, 0x0, p);
	if ((p[1] & CPUID_STDEXT_FSGSBASE) == 0) {
		fprintf(stderr, "CPU does not support RDGSBASE\n");
		return (0);
	}

	len = sizeof(nthreads);
	error = sysctl(mib, nitems(mib), &nthreads, &len, NULL, 0);
	if (error == -1)
		err(1, "sysctl hw.ncpu");
	signal(SIGALRM, hand);
	alarm(10);
	start(nthreads);
	for (;;)
		pause();
}
EOF

mycc -o /tmp/rdgsbase /tmp/rdgsbase.c -lpthread || exit 1
rm /tmp/rdgsbase.c

(cd /tmp; /tmp/rdgsbase)
s=$?

rm -f /tmp/rdgsbase /tmp/rdgsbase.core
exit $s
