/*-
 * SPDX-License-Identifier: BSD-4-Clause AND BSD-2-Clause
 *
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: clock.c,v 1.9 2000/01/19 02:52:19 msaitoh Exp $
 */
/*
 * Copyright (C) 2001 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/vdso.h>

#include <dev/ofw/openfirm.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/smp.h>

/*
 * Initially we assume a processor with a bus frequency of 12.5 MHz.
 */
static int		initialized = 0;
static uint64_t		ps_per_tick = 80000;
static u_long		ticks_per_sec = 12500000;
static u_long		*decr_counts[MAXCPU];

static int		decr_et_start(struct eventtimer *et,
    sbintime_t first, sbintime_t period);
static int		decr_et_stop(struct eventtimer *et);
static timecounter_get_t	decr_get_timecount;
static uint32_t decr_vdso_timehands(struct vdso_timehands *vdso_th,
    struct timecounter *tc);
#ifdef COMPAT_FREEBSD32
static uint32_t decr_vdso_timehands32(struct vdso_timehands32 *vdso_th32,
    struct timecounter *tc);
#endif

struct decr_state {
	int	mode;	/* 0 - off, 1 - periodic, 2 - one-shot. */
	int32_t	div;	/* Periodic divisor. */
};
DPCPU_DEFINE_STATIC(struct decr_state, decr_state);

static struct eventtimer	decr_et;
static struct timecounter	decr_tc = {
	.tc_get_timecount = 		decr_get_timecount,
	.tc_counter_mask = 		~0u,
	.tc_name = 			"timebase",
	.tc_quality = 			1000,
	.tc_fill_vdso_timehands = 	decr_vdso_timehands,
#ifdef COMPAT_FREEBSD32
	.tc_fill_vdso_timehands32 = 	decr_vdso_timehands32,
#endif
};

/*
 * Decrementer interrupt handler.
 */
void
decr_intr(struct trapframe *frame)
{
	struct decr_state *s = DPCPU_PTR(decr_state);
	int		nticks = 0;
	int32_t		val;

	if (!initialized)
		return;

	(*decr_counts[curcpu])++;

#ifdef BOOKE
	/*
	 * Interrupt handler must reset DIS to avoid getting another
	 * interrupt once EE is enabled.
	 */
	mtspr(SPR_TSR, TSR_DIS);
#endif

	if (s->mode == 1) {
		/*
		 * Based on the actual time delay since the last decrementer
		 * reload, we arrange for earlier interrupt next time.
		 */
		__asm ("mfdec %0" : "=r"(val));
		while (val < 0) {
			val += s->div;
			nticks++;
		}
		mtdec(val);
	} else if (s->mode == 2) {
		nticks = 1;
		decr_et_stop(NULL);
	} else if (s->mode == 0) {
		/* Potemkin timer ran out without an event. Just reset it. */
		decr_et_stop(NULL);
	}

	while (nticks-- > 0) {
		if (decr_et.et_active)
			decr_et.et_event_cb(&decr_et, decr_et.et_arg);
	}
}

void
cpu_initclocks(void)
{

	decr_tc_init();
	cpu_initclocks_bsp();
}

/*
 * BSP early initialization.
 */
void
decr_init(void)
{
	struct cpuref cpu;
	char buf[32];

	/*
	 * Check the BSP's timebase frequency. Sometimes we can't find the BSP,
	 * so fall back to the first CPU in this case.
	 */
	if (platform_smp_get_bsp(&cpu) != 0)
		platform_smp_first_cpu(&cpu);
	ticks_per_sec = platform_timebase_freq(&cpu);
	ps_per_tick = 1000000000000 / ticks_per_sec;

	set_cputicker(mftb, ticks_per_sec, false);
	snprintf(buf, sizeof(buf), "cpu%d:decrementer", curcpu);
	intrcnt_add(buf, &decr_counts[curcpu]);
	decr_et_stop(NULL);
	initialized = 1;
}

#ifdef SMP
/*
 * AP early initialization.
 */
void
decr_ap_init(void)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "cpu%d:decrementer", curcpu);
	intrcnt_add(buf, &decr_counts[curcpu]);
	decr_et_stop(NULL);
}
#endif

/*
 * Final initialization.
 */
void
decr_tc_init(void)
{

	decr_tc.tc_frequency = ticks_per_sec;
	tc_init(&decr_tc);
	decr_et.et_name = "decrementer";
	decr_et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT |
	    ET_FLAGS_PERCPU;
	decr_et.et_quality = 1000;
	decr_et.et_frequency = ticks_per_sec;
	decr_et.et_min_period = (0x00000002LLU << 32) / ticks_per_sec;
	decr_et.et_max_period = (0x7fffffffLLU << 32) / ticks_per_sec;
	decr_et.et_start = decr_et_start;
	decr_et.et_stop = decr_et_stop;
	decr_et.et_priv = NULL;
	et_register(&decr_et);
}

uint32_t
decr_vdso_timehands(struct vdso_timehands *vdso_th, struct timecounter *tc)
{
	vdso_th->th_algo = VDSO_TH_ALGO_PPC_TB;
	bzero(vdso_th->th_res, sizeof(vdso_th->th_res));
	return (initialized == 1);
}

#ifdef COMPAT_FREEBSD32
uint32_t
decr_vdso_timehands32(struct vdso_timehands32 *vdso_th32,
    struct timecounter *tc)
{
	vdso_th32->th_algo = VDSO_TH_ALGO_PPC_TB;
	bzero(vdso_th32->th_res, sizeof(vdso_th32->th_res));
	return (initialized == 1);
}
#endif

/*
 * Event timer start method.
 */
static int
decr_et_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct decr_state *s = DPCPU_PTR(decr_state);
	uint32_t fdiv;
#ifdef BOOKE
	uint32_t tcr;
#endif

	if (period != 0) {
		s->mode = 1;
		s->div = (decr_et.et_frequency * period) >> 32;
	} else {
		s->mode = 2;
		s->div = 0;
	}
	if (first != 0)
		fdiv = (decr_et.et_frequency * first) >> 32;
	else
		fdiv = s->div;

#ifdef BOOKE
	tcr = mfspr(SPR_TCR);
	tcr |= TCR_DIE;
	if (s->mode == 1) {
		mtspr(SPR_DECAR, s->div);
		tcr |= TCR_ARE;
	} else
		tcr &= ~TCR_ARE;
	mtdec(fdiv);
	mtspr(SPR_TCR, tcr);
#else
	mtdec(fdiv);
#endif

	return (0);
}

/*
 * Event timer stop method.
 */
static int
decr_et_stop(struct eventtimer *et)
{
	struct decr_state *s = DPCPU_PTR(decr_state);
#ifdef BOOKE
	uint32_t tcr;
#endif

	s->mode = 0;
	s->div = 0x7fffffff;
#ifdef BOOKE
	tcr = mfspr(SPR_TCR);
	tcr &= ~(TCR_DIE | TCR_ARE);
	mtspr(SPR_TCR, tcr);
#else
	mtdec(s->div);
#endif
	return (0);
}

/*
 * Timecounter get method.
 */
static unsigned
decr_get_timecount(struct timecounter *tc)
{
	return (mftb());
}

/*
 * Wait for about n microseconds (at least!).
 */
void
DELAY(int n)
{
	volatile u_quad_t	tb;
	u_quad_t		ttb;

	TSENTER();
	tb = mftb();
	ttb = tb + howmany((uint64_t)n * 1000000, ps_per_tick);
	nop_prio_vlow();
	while (tb < ttb)
		tb = mftb();
	nop_prio_medium();
	TSEXIT();
}
