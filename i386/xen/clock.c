/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)clock.c	7.2 (Berkeley) 5/12/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* #define DELAYDEBUG */
/*
 * Routines to handle clock hardware.
 */

#include "opt_ddb.h"
#include "opt_clock.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/sysctl.h>
#include <sys/cons.h>
#include <sys/power.h>

#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#if defined(SMP)
#include <machine/smp.h>
#endif
#include <machine/specialreg.h>
#include <machine/timerreg.h>

#include <x86/isa/icu.h>
#include <x86/isa/isa.h>
#include <isa/rtc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <xen/hypervisor.h>
#include <xen/xen-os.h>
#include <machine/xen/xenfunc.h>
#include <xen/interface/vcpu.h>
#include <machine/cpu.h>
#include <xen/xen_intr.h>

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y)	(!((y) % 4))
#define	DAYSPERYEAR	(28+30*4+31*7)

#ifndef TIMER_FREQ
#define	TIMER_FREQ	1193182
#endif

#ifdef CYC2NS_SCALE_FACTOR
#undef	CYC2NS_SCALE_FACTOR
#endif
#define CYC2NS_SCALE_FACTOR	10

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

struct mtx clock_lock;
#define	RTC_LOCK_INIT							\
	mtx_init(&clock_lock, "clk", NULL, MTX_SPIN | MTX_NOPROFILE)
#define	RTC_LOCK	mtx_lock_spin(&clock_lock)
#define	RTC_UNLOCK	mtx_unlock_spin(&clock_lock)
#define	NS_PER_TICK	(1000000000ULL/hz)

int adjkerntz;		/* local offset from GMT in seconds */
int clkintr_pending;
int pscnt = 1;
int psdiv = 1;
int wall_cmos_clock;
u_int timer_freq = TIMER_FREQ;
static u_long cyc2ns_scale; 
static uint64_t processed_system_time;	/* stime (ns) at last processing. */

extern volatile uint64_t xen_timer_last_time;

#define do_div(n,base) ({ \
        unsigned long __upper, __low, __high, __mod, __base; \
        __base = (base); \
        __asm("":"=a" (__low), "=d" (__high):"A" (n)); \
        __upper = __high; \
        if (__high) { \
                __upper = __high % (__base); \
                __high = __high / (__base); \
        } \
        __asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (__base), "0" (__low), "1" (__upper)); \
        __asm("":"=A" (n):"a" (__low),"d" (__high)); \
        __mod; \
})


/* convert from cycles(64bits) => nanoseconds (64bits)
 *  basic equation:
 *		ns = cycles / (freq / ns_per_sec)
 *		ns = cycles * (ns_per_sec / freq)
 *		ns = cycles * (10^9 / (cpu_mhz * 10^6))
 *		ns = cycles * (10^3 / cpu_mhz)
 *
 *	Then we use scaling math (suggested by george@mvista.com) to get:
 *		ns = cycles * (10^3 * SC / cpu_mhz) / SC
 *		ns = cycles * cyc2ns_scale / SC
 *
 *	And since SC is a constant power of two, we can convert the div
 *  into a shift.   
 *			-johnstul@us.ibm.com "math is hard, lets go shopping!"
 */
static inline void set_cyc2ns_scale(unsigned long cpu_mhz)
{
	cyc2ns_scale = (1000 << CYC2NS_SCALE_FACTOR)/cpu_mhz;
}

static inline unsigned long long cycles_2_ns(unsigned long long cyc)
{
	return ((cyc * cyc2ns_scale) >> CYC2NS_SCALE_FACTOR);
}

static uint32_t
getit(void)
{
	return (xen_timer_last_time);
}


/*
 * XXX: timer needs more SMP work.
 */
void
i8254_init(void)
{

	RTC_LOCK_INIT;
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (timer_freq / hz)
 * Note: timer had better have been programmed before this is first used!
 */
void
DELAY(int n)
{
	int delta, ticks_left;
	uint32_t tick, prev_tick;
#ifdef DELAYDEBUG
	int getit_calls = 1;
	int n1;
	static int state = 0;

	if (state == 0) {
		state = 1;
		for (n1 = 1; n1 <= 10000000; n1 *= 10)
			DELAY(n1);
		state = 2;
	}
	if (state == 1)
		printf("DELAY(%d)...", n);
#endif
	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 *
	 * However, if ddb is active then use a fake counter since reading
	 * the i8254 counter involves acquiring a lock.  ddb must not go
	 * locking for many reasons, but it calls here for at least atkbd
	 * input.
	 */
	prev_tick = getit();

	n -= 0;			/* XXX actually guess no initial overhead */
	/*
	 * Calculate (n * (timer_freq / 1e6)) without using floating point
	 * and without any avoidable overflows.
	 */
	if (n <= 0)
		ticks_left = 0;
	else if (n < 256)
		/*
		 * Use fixed point to avoid a slow division by 1000000.
		 * 39099 = 1193182 * 2^15 / 10^6 rounded to nearest.
		 * 2^15 is the first power of 2 that gives exact results
		 * for n between 0 and 256.
		 */
		ticks_left = ((u_int)n * 39099 + (1 << 15) - 1) >> 15;
	else
		/*
		 * Don't bother using fixed point, although gcc-2.7.2
		 * generates particularly poor code for the long long
		 * division, since even the slow way will complete long
		 * before the delay is up (unless we're interrupted).
		 */
		ticks_left = ((u_int)n * (long long)timer_freq + 999999)
			/ 1000000;

	while (ticks_left > 0) {
		tick = getit();
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		delta = tick - prev_tick;
		prev_tick = tick;
		if (delta < 0) {
			/*
			 * Guard against timer0_max_count being wrong.
			 * This shouldn't happen in normal operation,
			 * but it may happen if set_timer_freq() is
			 * traced.
			 */
			/* delta += timer0_max_count; ??? */
			if (delta < 0)
				delta = 0;
		}
		ticks_left -= delta;
	}
#ifdef DELAYDEBUG
	if (state == 1)
		printf(" %d calls to getit() at %d usec each\n",
		       getit_calls, (n + 5) / getit_calls);
#endif
}

void
startrtclock()
{
	uint64_t __cpu_khz;
	uint32_t cpu_khz;
	struct vcpu_time_info *info;

	__cpu_khz = 1000000ULL << 32;
	info = &HYPERVISOR_shared_info->vcpu_info[0].time;

	(void)do_div(__cpu_khz, info->tsc_to_system_mul);
	if ( info->tsc_shift < 0 )
		cpu_khz = __cpu_khz << -info->tsc_shift;
	else
		cpu_khz = __cpu_khz >> info->tsc_shift;

	printf("Xen reported: %u.%03u MHz processor.\n", 
	       cpu_khz / 1000, cpu_khz % 1000);

	/* (10^6 * 2^32) / cpu_hz = (10^3 * 2^32) / cpu_khz =
	   (2^32 * 1 / (clocks/us)) */

	set_cyc2ns_scale(cpu_khz/1000);
	tsc_freq = cpu_khz * 1000;
}

/*
 * RTC support routines
 */


static __inline int
readrtc(int port)
{
	return(bcd2bin(rtcin(port)));
}


#ifdef XEN_PRIVILEGED_GUEST

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
static void
domu_inittodr(time_t base)
{
	unsigned long   sec;
	int		s, y;
	struct timespec ts;

	update_wallclock();
	add_uptime_to_wallclock();
	
	RTC_LOCK;
	
	if (base) {
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}

	sec += tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	y = time_second - shadow_tv.tv_sec;
	if (y <= -2 || y >= 2) {
		/* badly off, adjust it */
		tc_setclock(&shadow_tv);
	}
	RTC_UNLOCK;
}

/*
 * Write system time back to RTC.  
 */
static void
domu_resettodr(void)
{
	unsigned long tm;
	int s;
	dom0_op_t op;
	struct shadow_time_info *shadow;
	struct pcpu *pc;

	pc = pcpu_find(smp_processor_id());
	shadow = &pc->pc_shadow_time;
	if (xen_disable_rtc_set)
		return;
	
	s = splclock();
	tm = time_second;
	splx(s);
	
	tm -= tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);
	
	if ((xen_start_info->flags & SIF_INITDOMAIN) &&
	    !independent_wallclock)
	{
		op.cmd = DOM0_SETTIME;
		op.u.settime.secs        = tm;
		op.u.settime.nsecs       = 0;
		op.u.settime.system_time = shadow->system_timestamp;
		HYPERVISOR_dom0_op(&op);
		update_wallclock();
		add_uptime_to_wallclock();
	} else if (independent_wallclock) {
		/* notyet */
		;
	}		
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	unsigned long	sec, days;
	int		year, month;
	int		y, m, s;
	struct timespec ts;

	if (!(xen_start_info->flags & SIF_INITDOMAIN)) {
	        domu_inittodr(base);
		return;
	}

	if (base) {
		s = splclock();
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
		splx(s);
	}

	/* Look if we have a RTC present and the time is valid */
	if (!(rtcin(RTC_STATUSD) & RTCSD_PWR))
		goto wrong_time;

	/* wait for time update to complete */
	/* If RTCSA_TUP is zero, we have at least 244us before next update */
	s = splhigh();
	while (rtcin(RTC_STATUSA) & RTCSA_TUP) {
		splx(s);
		s = splhigh();
	}

	days = 0;
#ifdef USE_RTC_CENTURY
	year = readrtc(RTC_YEAR) + readrtc(RTC_CENTURY) * 100;
#else
	year = readrtc(RTC_YEAR) + 1900;
	if (year < 1970)
		year += 100;
#endif
	if (year < 1970) {
		splx(s);
		goto wrong_time;
	}
	month = readrtc(RTC_MONTH);
	for (m = 1; m < month; m++)
		days += daysinmonth[m-1];
	if ((month > 2) && LEAPYEAR(year))
		days ++;
	days += readrtc(RTC_DAY) - 1;
	for (y = 1970; y < year; y++)
		days += DAYSPERYEAR + LEAPYEAR(y);
	sec = ((( days * 24 +
		  readrtc(RTC_HRS)) * 60 +
		readrtc(RTC_MIN)) * 60 +
	       readrtc(RTC_SEC));
	/* sec now contains the number of seconds, since Jan 1 1970,
	   in the local time zone */

	sec += tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	y = time_second - sec;
	if (y <= -2 || y >= 2) {
		/* badly off, adjust it */
		ts.tv_sec = sec;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}
	splx(s);
	return;

 wrong_time:
	printf("Invalid time in real time clock.\n");
	printf("Check and reset the date immediately!\n");
}


/*
 * Write system time back to RTC
 */
void
resettodr()
{
	unsigned long	tm;
	int		y, m, s;

	if (!(xen_start_info->flags & SIF_INITDOMAIN)) {
	        domu_resettodr();
		return;
	}
	       
	if (xen_disable_rtc_set)
		return;

	s = splclock();
	tm = time_second;
	splx(s);

	/* Disable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_HALT | RTCSB_24HR);

	/* Calculate local time to put in RTC */

	tm -= tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	writertc(RTC_SEC, bin2bcd(tm%60)); tm /= 60;	/* Write back Seconds */
	writertc(RTC_MIN, bin2bcd(tm%60)); tm /= 60;	/* Write back Minutes */
	writertc(RTC_HRS, bin2bcd(tm%24)); tm /= 24;	/* Write back Hours   */

	/* We have now the days since 01-01-1970 in tm */
	writertc(RTC_WDAY, (tm + 4) % 7 + 1);		/* Write back Weekday */
	for (y = 1970, m = DAYSPERYEAR + LEAPYEAR(y);
	     tm >= m;
	     y++,      m = DAYSPERYEAR + LEAPYEAR(y))
		tm -= m;

	/* Now we have the years in y and the day-of-the-year in tm */
	writertc(RTC_YEAR, bin2bcd(y%100));		/* Write back Year    */
#ifdef USE_RTC_CENTURY
	writertc(RTC_CENTURY, bin2bcd(y/100));		/* ... and Century    */
#endif
	for (m = 0; ; m++) {
		int ml;

		ml = daysinmonth[m];
		if (m == 1 && LEAPYEAR(y))
			ml++;
		if (tm < ml)
			break;
		tm -= ml;
	}

	writertc(RTC_MONTH, bin2bcd(m + 1));            /* Write back Month   */
	writertc(RTC_DAY, bin2bcd(tm + 1));             /* Write back Month Day */

	/* Reenable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_24HR);
	rtcin(RTC_INTR);
}
#endif

/*
 * Start clocks running.
 */
void
cpu_initclocks(void)
{
	cpu_initclocks_bsp();
}

/* Return system time offset by ticks */
uint64_t
get_system_time(int ticks)
{
    return (processed_system_time + (ticks * NS_PER_TICK));
}

int
timer_spkr_acquire(void)
{

	return (0);
}

int
timer_spkr_release(void)
{

	return (0);
}

void
timer_spkr_setfreq(int freq)
{

}

