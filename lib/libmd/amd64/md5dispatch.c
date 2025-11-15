/*-
 * Copyright (c) 2024 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/md5.h>

#include <machine/cpufunc.h>
#include <machine/specialreg.h>
#include <stdint.h>
#include <string.h>
#include <x86/ifunc.h>

extern void _libmd_md5block_baseline(MD5_CTX *, const void *, size_t);
extern void _libmd_md5block_bmi1(MD5_CTX *, const void *, size_t);
extern void _libmd_md5block_avx512(MD5_CTX *, const void *, size_t);

DEFINE_UIFUNC(, void, _libmd_md5block, (MD5_CTX *, const void *, size_t))
{
	if ((cpu_stdext_feature & (CPUID_STDEXT_AVX512F | CPUID_STDEXT_AVX512VL))
	    == (CPUID_STDEXT_AVX512F | CPUID_STDEXT_AVX512VL)) {
		u_int regs[4];
		char cpu_vendor[12];

		do_cpuid(0, regs);
		((u_int *)&cpu_vendor)[0] = regs[1];
		((u_int *)&cpu_vendor)[1] = regs[3];
		((u_int *)&cpu_vendor)[2] = regs[2];

		/* the AVX-512 kernel performs poorly on AMD */
		if (memcmp(cpu_vendor, AMD_VENDOR_ID, sizeof(cpu_vendor)) != 0)
			return (_libmd_md5block_avx512);
	}

	if (cpu_stdext_feature & CPUID_STDEXT_BMI1)
		return (_libmd_md5block_bmi1);
	else
		return (_libmd_md5block_baseline);
}
