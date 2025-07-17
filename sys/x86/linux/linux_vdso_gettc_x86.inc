/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2016, 2017, 2019 The FreeBSD Foundation
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

static inline u_int
rdtsc_low(const struct vdso_timehands *th)
{
	u_int rv;

	__asm __volatile("rdtsc; shrd %%cl, %%edx, %0"
	    : "=a" (rv) : "c" (th->th_x86_shift) : "edx");
	return (rv);
}

static inline u_int
rdtscp_low(const struct vdso_timehands *th)
{
	u_int rv;

	__asm __volatile("rdtscp; movl %%edi,%%ecx; shrd %%cl, %%edx, %0"
	    : "=a" (rv) : "D" (th->th_x86_shift) : "ecx", "edx");
	return (rv);
}

static u_int
rdtsc_low_mb_lfence(const struct vdso_timehands *th)
{
	lfence();
	return (rdtsc_low(th));
}

static u_int
rdtsc_low_mb_mfence(const struct vdso_timehands *th)
{
	mfence();
	return (rdtsc_low(th));
}

static u_int
rdtsc_low_mb_none(const struct vdso_timehands *th)
{
	return (rdtsc_low(th));
}

static u_int
rdtsc32_mb_lfence(void)
{
	lfence();
	return (rdtsc32());
}

static u_int
rdtsc32_mb_mfence(void)
{
	mfence();
	return (rdtsc32());
}

static u_int
rdtsc32_mb_none(void)
{
	return (rdtsc32());
}

static u_int
rdtscp32_(void)
{
	return (rdtscp32());
}

struct tsc_selector_tag {
	u_int (*ts_rdtsc32)(void);
	u_int (*ts_rdtsc_low)(const struct vdso_timehands *);
};

static const struct tsc_selector_tag tsc_selector[] = {
	[0] = {				/* Intel, LFENCE */
		.ts_rdtsc32 =	rdtsc32_mb_lfence,
		.ts_rdtsc_low =	rdtsc_low_mb_lfence,
	},
	[1] = {				/* AMD, MFENCE */
		.ts_rdtsc32 =	rdtsc32_mb_mfence,
		.ts_rdtsc_low =	rdtsc_low_mb_mfence,
	},
	[2] = {				/* No SSE2 */
		.ts_rdtsc32 = rdtsc32_mb_none,
		.ts_rdtsc_low = rdtsc_low_mb_none,
	},
	[3] = {				/* RDTSCP */
		.ts_rdtsc32 =	rdtscp32_,
		.ts_rdtsc_low =	rdtscp_low,
	},
};

static u_int
__vdso_gettc_rdtsc_low(const struct vdso_timehands *th)
{

	return (tsc_selector[kern_tsc_selector].ts_rdtsc_low(th));
}

static u_int
__vdso_gettc_rdtsc32(void)
{

	return (tsc_selector[kern_tsc_selector].ts_rdtsc32());
}

int
__vdso_gettc(const struct vdso_timehands *th, u_int *tc)
{

	switch (th->th_algo) {
	case VDSO_TH_ALGO_X86_TSC:
		*tc = th->th_x86_shift > 0 ? __vdso_gettc_rdtsc_low(th) :
		    __vdso_gettc_rdtsc32();
		return (0);
	case VDSO_TH_ALGO_X86_HPET:
		/* TODO */
	default:
		return (ENOSYS);
	}
}
