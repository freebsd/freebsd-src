/*-
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2016, 2017, 2019, 2021 The FreeBSD Foundation
 * Copyright (c) 2021 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Portions of this software were developed by Konstantin Belousov
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <x86/cputypes.h>
#include <x86/x86_var.h>
#include <x86/specialreg.h>

#include <machine/cpufunc.h>

#include <x86/linux/linux_x86.h>

int
linux_vdso_tsc_selector_idx(void)
{
	bool amd_cpu;

	if (cpu_feature == 0)
		return (2);	/* should not happen due to RDTSC */

	amd_cpu = (cpu_vendor_id == CPU_VENDOR_AMD ||
	    cpu_vendor_id == CPU_VENDOR_HYGON);

	if ((amd_feature & AMDID_RDTSCP) != 0)
		return (3);
	if ((cpu_feature & CPUID_SSE2) == 0)
		return (2);
	return (amd_cpu ? 1 : 0);
}

int
linux_vdso_cpu_selector_idx(void)
{

	if ((cpu_stdext_feature2 & CPUID_STDEXT2_RDPID) != 0)
		return (LINUX_VDSO_CPU_RDPID);

	return ((amd_feature & AMDID_RDTSCP) == 0 ?
	    LINUX_VDSO_CPU_DEFAULT : LINUX_VDSO_CPU_RDTSCP);
}
