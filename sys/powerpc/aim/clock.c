/*
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/timetc.h>
#include <sys/interrupt.h>

#include <dev/ofw/openfirm.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#if 0 /* XXX */
#include "adb.h"
#else
#define	NADB	0
#endif

/*
 * Initially we assume a processor with a bus frequency of 12.5 MHz.
 */
static u_long ticks_per_sec = 12500000;
static u_long		ns_per_tick = 80;
static long		ticks_per_intr;
static volatile u_long	lasttb;

#define	SECDAY		86400
#define	DIFF19041970	2082844800

#if NADB > 0
extern int adb_read_date_time __P((int *));
extern int adb_set_date_time __P((int));
#endif

static int		clockinitted = 0;

static timecounter_get_t	powerpc_get_timecount;

static struct timecounter	powerpc_timecounter = {
	powerpc_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"powerpc"		/* name */
};

SYSCTL_OPAQUE(_debug, OID_AUTO, powerpc_timecounter, CTLFLAG_RD, 
	&powerpc_timecounter, sizeof(powerpc_timecounter), "S,timecounter", "");

void
inittodr(time_t base)
{
	time_t		deltat;
	u_int		rtc_time;
	struct timespec	ts;

	/*
	 * If we can't read from RTC, use the fs time.
	 */
#if NADB > 0
	if (adb_read_date_time(&rtc_time) < 0)
#endif
	{
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
		return;
	}
	clockinitted = 1;
	ts.tv_sec = rtc_time - DIFF19041970;

	deltat = ts.tv_sec - base;
	if (deltat < 0) {
		deltat = -deltat;
	}
	if (deltat < 2 * SECDAY) {
		tc_setclock(&ts);
		return;
	}

	printf("WARNING: clock %s %d days",
	    ts.tv_sec < base ? "lost" : "gained", (int)(deltat / SECDAY));

	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Similar to the above
 */
void
resettodr()
{
#if NADB > 0
	u_int	rtc_time;

	if (clockinitted) {
		rtc_time = time.tv_sec + DIFF19041970;
		adb_set_date_time(rtc_time);
	}
#endif
}

void
decr_intr(struct clockframe *frame)
{
	u_long	tb;
	long	tick;
	int	nticks;

	/*
	 * Check whether we are initialized.
	 */
	if (!ticks_per_intr)
		return;

	/*
	 * Based on the actual time delay since the last decrementer reload,
	 * we arrange for earlier interrupt next time.
	 */
	__asm ("mftb %0; mfdec %1" : "=r"(tb), "=r"(tick));
	for (nticks = 0; tick < 0; nticks++)
		tick += ticks_per_intr;
	__asm __volatile ("mtdec %0" :: "r"(tick));
	/*
	 * lasttb is used during microtime. Set it to the virtual
	 * start of this tick interval.
	 */
	lasttb = tb + tick - ticks_per_intr;

	/*
	 * This probably needs some kind of locking.
	 */

	intrcnt[CNT_CLOCK]++;
	{
		int msr;

		nticks += tickspending;
		tickspending = 0;

#if 0
		/*
		 * Reenable interrupts
		 */
		msr = mfmsr();
		mtmsr(msr | PSL_EE);
#endif
		
		/*
		 * Do standard timer interrupt stuff.
		 * Do softclock stuff only on the last iteration.
		 */
		while (--nticks > 0) {
			hardclock(frame);
		}
		hardclock(frame);
	}
}

void
cpu_initclocks(void)
{
	int qhandle, phandle;
	char name[32];
	unsigned int msr;
	
	/*
	 * Get this info during autoconf?				XXX
	 */
	for (qhandle = OF_peer(0); qhandle; qhandle = phandle) {
		if (OF_getprop(qhandle, "device_type", name, sizeof name) >= 0
		    && !strcmp(name, "cpu")
		    && OF_getprop(qhandle, "timebase-frequency",
				  &ticks_per_sec, sizeof ticks_per_sec) >= 0) {
			/*
			 * Should check for correct CPU here?		XXX
			 */
			msr = mfmsr();
			mtmsr(msr & ~PSL_EE);

			powerpc_timecounter.tc_frequency = ticks_per_sec;
			tc_init(&powerpc_timecounter);

			ns_per_tick = 1000000000 / ticks_per_sec;
			ticks_per_intr = ticks_per_sec / hz;
			__asm __volatile ("mftb %0" : "=r"(lasttb));
			mtdec(ticks_per_intr);

			mtmsr(msr);

			break;
		}
		if ((phandle = OF_child(qhandle)))
			continue;
		while (qhandle) {
			if ((phandle = OF_peer(qhandle)))
				break;
			qhandle = OF_parent(qhandle);
		}
	}
	if (!phandle)
		panic("no cpu node");
}

static __inline u_quad_t
mftb(void)
{
	u_long		scratch;
	u_quad_t	tb;
	
	__asm ("1: mftbu %0; mftb %0+1; mftbu %1; cmpw 0,%0,%1; bne 1b"
	      : "=r"(tb), "=r"(scratch));
	return tb;
}

static unsigned
powerpc_get_timecount(struct timecounter *tc)
{
	return mftb();
}

/*
 * Wait for about n microseconds (at least!).
 */
void
delay(int n)
{
	u_quad_t	tb;
	u_long		tbh, tbl, scratch;
	
	tb = mftb();
	tb += (n * 1000 + ns_per_tick - 1) / ns_per_tick;
	tbh = tb >> 32;
	tbl = tb;
	__asm ("1: mftbu %0; cmplw %0,%1; blt 1b; bgt 2f;"
	       "mftb %0; cmplw %0,%2; blt 1b; 2:"
	       :: "r"(scratch), "r"(tbh), "r"(tbl));
}

/*
 * Nothing to do.
 */
void
setstatclockrate(int arg)
{

	/* Do nothing */
}
