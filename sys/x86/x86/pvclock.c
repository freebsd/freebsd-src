/*-
 * Copyright (c) 2009 Adrian Chadd
 * Copyright (c) 2012 Spectra Logic Corporation
 * Copyright (c) 2014 Bryan Venteicher
 * All rights reserved.
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
#include <sys/systm.h>
#include <sys/clock.h>
#include <sys/limits.h>
#include <sys/proc.h>

#include <machine/atomic.h>
#include <machine/md_var.h>
#include <machine/pvclock.h>

/*
 * Last system time. This is used to guarantee a monotonically non-decreasing
 * clock for the kernel codepath and approximate the same for the vDSO codepath.
 * In theory, this should be unnecessary absent hypervisor bug(s) and/or what
 * should be rare cases where TSC jitter may still be visible despite the
 * hypervisor's best efforts.
 */
static volatile uint64_t pvclock_last_systime;

static uint64_t		 pvclock_getsystime(struct pvclock *pvc);
static void		 pvclock_read_time_info(
    struct pvclock_vcpu_time_info *ti, uint64_t *ns, uint8_t *flags);
static void		 pvclock_read_wall_clock(struct pvclock_wall_clock *wc,
    struct timespec *ts);
static u_int		 pvclock_tc_get_timecount(struct timecounter *tc);

void
pvclock_resume(void)
{
	atomic_store_rel_64(&pvclock_last_systime, 0);
}

uint64_t
pvclock_tsc_freq(struct pvclock_vcpu_time_info *ti)
{
	uint64_t freq;

	freq = (1000000000ULL << 32) / ti->tsc_to_system_mul;
	if (ti->tsc_shift < 0)
		freq <<= -ti->tsc_shift;
	else
		freq >>= ti->tsc_shift;
	return (freq);
}

/*
 * Scale a 64-bit delta by scaling and multiplying by a 32-bit fraction,
 * yielding a 64-bit result.
 */
static inline uint64_t
pvclock_scale_delta(uint64_t delta, uint32_t mul_frac, int shift)
{
	uint64_t product;

	if (shift < 0)
		delta >>= -shift;
	else
		delta <<= shift;
#if defined(__i386__)
	{
		uint32_t tmp1, tmp2;

		/**
		 * For i386, the formula looks like:
		 *
		 *   lower = (mul_frac * (delta & UINT_MAX)) >> 32
		 *   upper = mul_frac * (delta >> 32)
		 *   product = lower + upper
		 */
		__asm__ (
			"mul  %5       ; "
			"mov  %4,%%eax ; "
			"mov  %%edx,%4 ; "
			"mul  %5       ; "
			"xor  %5,%5    ; "
			"add  %4,%%eax ; "
			"adc  %5,%%edx ; "
			: "=A" (product), "=r" (tmp1), "=r" (tmp2)
			: "a" ((uint32_t)delta), "1" ((uint32_t)(delta >> 32)),
			  "2" (mul_frac) );
	}
#elif defined(__amd64__)
	{
		unsigned long tmp;

		__asm__ (
			"mulq %[mul_frac] ; shrd $32, %[hi], %[lo]"
			: [lo]"=a" (product), [hi]"=d" (tmp)
			: "0" (delta), [mul_frac]"rm"((uint64_t)mul_frac));
	}
#else
#error "pvclock: unsupported x86 architecture?"
#endif
	return (product);
}

static void
pvclock_read_time_info(struct pvclock_vcpu_time_info *ti,
    uint64_t *ns, uint8_t *flags)
{
	uint64_t delta;
	uint32_t version;

	do {
		version = atomic_load_acq_32(&ti->version);
		delta = rdtsc_ordered() - ti->tsc_timestamp;
		*ns = ti->system_time + pvclock_scale_delta(delta,
		    ti->tsc_to_system_mul, ti->tsc_shift);
		*flags = ti->flags;
		atomic_thread_fence_acq();
	} while ((ti->version & 1) != 0 || ti->version != version);
}

static void
pvclock_read_wall_clock(struct pvclock_wall_clock *wc, struct timespec *ts)
{
	uint32_t version;

	do {
		version = atomic_load_acq_32(&wc->version);
		ts->tv_sec = wc->sec;
		ts->tv_nsec = wc->nsec;
		atomic_thread_fence_acq();
	} while ((wc->version & 1) != 0 || wc->version != version);
}

static uint64_t
pvclock_getsystime(struct pvclock *pvc)
{
	uint64_t now, last, ret;
	uint8_t flags;

	critical_enter();
	pvclock_read_time_info(&pvc->timeinfos[curcpu], &now, &flags);
	ret = now;
	if ((flags & PVCLOCK_FLAG_TSC_STABLE) == 0) {
		last = atomic_load_acq_64(&pvclock_last_systime);
		do {
			if (last > now) {
				ret = last;
				break;
			}
		} while (!atomic_fcmpset_rel_64(&pvclock_last_systime, &last,
		    now));
	}
	critical_exit();
	return (ret);
}

/*
 * NOTE: Transitional-only; this should be removed after 'dev/xen/timer/timer.c'
 * has been migrated to the 'struct pvclock' API.
 */
uint64_t
pvclock_get_timecount(struct pvclock_vcpu_time_info *ti)
{
	uint64_t now, last, ret;
	uint8_t flags;

	pvclock_read_time_info(ti, &now, &flags);
	ret = now;
	if ((flags & PVCLOCK_FLAG_TSC_STABLE) == 0) {
		last = atomic_load_acq_64(&pvclock_last_systime);
		do {
			if (last > now) {
				ret = last;
				break;
			}
		} while (!atomic_fcmpset_rel_64(&pvclock_last_systime, &last,
		    now));
	}
	return (ret);
}

/*
 * NOTE: Transitional-only; this should be removed after 'dev/xen/timer/timer.c'
 * has been migrated to the 'struct pvclock' API.
 */
void
pvclock_get_wallclock(struct pvclock_wall_clock *wc, struct timespec *ts)
{
	pvclock_read_wall_clock(wc, ts);
}

static u_int
pvclock_tc_get_timecount(struct timecounter *tc)
{
	struct pvclock *pvc = tc->tc_priv;

	return (pvclock_getsystime(pvc) & UINT_MAX);
}

void
pvclock_gettime(struct pvclock *pvc, struct timespec *ts)
{
	struct timespec system_ts;
	uint64_t system_ns;

	pvclock_read_wall_clock(pvc->get_wallclock(pvc->get_wallclock_arg), ts);
	system_ns = pvclock_getsystime(pvc);
	system_ts.tv_sec = system_ns / 1000000000ULL;
	system_ts.tv_nsec = system_ns % 1000000000ULL;
	timespecadd(ts, &system_ts, ts);
}

void
pvclock_init(struct pvclock *pvc, device_t dev, const char *tc_name,
    int tc_quality, u_int tc_flags)
{
	KASSERT(((uintptr_t)pvc->timeinfos & PAGE_MASK) == 0,
	    ("Specified time info page(s) address is not page-aligned."));

	/* Set up timecounter and timecounter-supporting members: */
	pvc->tc.tc_get_timecount = pvclock_tc_get_timecount;
	pvc->tc.tc_poll_pps = NULL;
	pvc->tc.tc_counter_mask = ~0U;
	pvc->tc.tc_frequency = 1000000000ULL;
	pvc->tc.tc_name = tc_name;
	pvc->tc.tc_quality = tc_quality;
	pvc->tc.tc_flags = tc_flags;
	pvc->tc.tc_priv = pvc;
	pvc->tc.tc_fill_vdso_timehands = NULL;
#ifdef COMPAT_FREEBSD32
	pvc->tc.tc_fill_vdso_timehands32 = NULL;
#endif

	/* Register timecounter: */
	tc_init(&pvc->tc);

	/*
	 * Register wallclock:
	 *     The RTC registration API expects a resolution in microseconds;
	 *     pvclock's 1ns resolution is rounded up to 1us.
	 */
	clock_register(dev, 1);
}

int
pvclock_destroy(struct pvclock *pvc)
{
	/*
	 * Not currently possible since there is no teardown counterpart of
	 * 'tc_init()'.
	 */
	return (EBUSY);
}
