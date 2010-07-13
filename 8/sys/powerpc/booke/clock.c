/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/ktr.h>
#include <sys/pcpu.h>
#include <sys/timetc.h>
#include <sys/interrupt.h>

#include <machine/clock.h>
#include <machine/platform.h>
#include <machine/psl.h>
#include <machine/spr.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/md_var.h>

/*
 * Initially we assume a processor with a bus frequency of 12.5 MHz.
 */
u_int tickspending;
u_long ns_per_tick = 80;
static u_long ticks_per_sec = 12500000;
static long ticks_per_intr;

#define	DIFF19041970	2082844800

static timecounter_get_t decr_get_timecount;

static struct timecounter	decr_timecounter = {
	decr_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"decrementer"		/* name */
};

void
decr_intr(struct trapframe *frame)
{

	/*
	 * Check whether we are initialized.
	 */
	if (!ticks_per_intr)
		return;

	/*
	 * Interrupt handler must reset DIS to avoid getting another
	 * interrupt once EE is enabled.
	 */
	mtspr(SPR_TSR, TSR_DIS);

	CTR1(KTR_INTR, "%s: DEC interrupt", __func__);

	if (PCPU_GET(cpuid) == 0)
		hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	else
		hardclock_cpu(TRAPF_USERMODE(frame));

	statclock(TRAPF_USERMODE(frame));
	if (profprocs != 0)
		profclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
}

void
cpu_initclocks(void)
{

	decr_tc_init();
	stathz = hz;
	profhz = hz;
}

void
decr_init(void)
{
	struct cpuref cpu;
	unsigned int msr;

	if (platform_smp_get_bsp(&cpu) != 0)
		platform_smp_first_cpu(&cpu);
	ticks_per_sec = platform_timebase_freq(&cpu);

	msr = mfmsr();
	mtmsr(msr & ~(PSL_EE));

	ns_per_tick = 1000000000 / ticks_per_sec;
	ticks_per_intr = ticks_per_sec / hz;

	mtdec(ticks_per_intr);

	mtspr(SPR_DECAR, ticks_per_intr);
	mtspr(SPR_TCR, mfspr(SPR_TCR) | TCR_DIE | TCR_ARE);

	set_cputicker(mftb, ticks_per_sec, 0);

	mtmsr(msr);
}

#ifdef SMP
void
decr_ap_init(void)
{

	/* Set auto-reload value and enable DEC interrupts in TCR */
	mtspr(SPR_DECAR, ticks_per_intr);
	mtspr(SPR_TCR, mfspr(SPR_TCR) | TCR_DIE | TCR_ARE);

	CTR2(KTR_INTR, "%s: set TCR=%p", __func__, mfspr(SPR_TCR));
}
#endif

void
decr_tc_init(void)
{

	decr_timecounter.tc_frequency = ticks_per_sec;
	tc_init(&decr_timecounter);
}

static unsigned
decr_get_timecount(struct timecounter *tc)
{
	quad_t tb;

	tb = mftb();
	return (tb);
}

/*
 * Wait for about n microseconds (at least!).
 */
void
DELAY(int n)
{
	u_quad_t start, end, now;

	start = mftb();
	end = start + (u_quad_t)ticks_per_sec / (1000000ULL / n);
	do {
		now = mftb();
	} while (now < end || (now > start && end < start));
}

/*
 * Nothing to do.
 */
void
cpu_startprofclock(void)
{

	/* Do nothing */
}

void
cpu_stopprofclock(void)
{

}
