/*-
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <x86/ifunc.h>
#include <errno.h>
#include <sched.h>
#include "libc_private.h"

static int
sched_getcpu_sys(void)
{
	return (__sys_sched_getcpu());
}

static int
sched_getcpu_rdpid(void)
{
	register_t res;

	__asm("rdpid	%0" : "=r" (res));
	return ((int)res);
}

static int
sched_getcpu_rdtscp(void)
{
	int res;

	__asm("rdtscp" : "=c" (res) : : "eax", "edx");
	return (res);
}

DEFINE_UIFUNC(, int, sched_getcpu, (void))
{
	u_int amd_feature, cpu_exthigh, p[4];

	if ((cpu_stdext_feature2 & CPUID_STDEXT2_RDPID) != 0)
		return (sched_getcpu_rdpid);

	amd_feature = 0;
	if (cpu_feature != 0) {
		do_cpuid(0x80000000, p);
		cpu_exthigh = p[0];
		if (cpu_exthigh >= 0x80000001) {
			do_cpuid(0x80000001, p);
			amd_feature = p[3];
		}
	}

	return ((amd_feature & AMDID_RDTSCP) == 0 ?
	    sched_getcpu_sys : sched_getcpu_rdtscp);
}
