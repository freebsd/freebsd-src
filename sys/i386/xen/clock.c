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

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <isa/rtc.h>

#include <machine/xen/xen_intr.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenfunc.h>
#include <xen/interface/vcpu.h>
#include <machine/cpu.h>

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

int adjkerntz;		/* local offset from GMT in seconds */
int clkintr_pending;
int pscnt = 1;
int psdiv = 1;
int statclock_disable;
int wall_cmos_clock;
u_int timer_freq = TIMER_FREQ;
static int independent_wallclock;
static int xen_disable_rtc_set;
static u_long cached_gtm;	/* cached quotient for TSC -> microseconds */
static u_long cyc2ns_scale; 
static struct timespec shadow_tv;
static uint32_t shadow_tv_version;	/* XXX: lazy locking */
static uint64_t processed_system_time;	/* stime (ns) at last processing. */

static	const u_char daysinmonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};

SYSCTL_INT(_machdep, OID_AUTO, independent_wallclock,
    CTLFLAG_RW, &independent_wallclock, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, xen_disable_rtc_set,
    CTLFLAG_RW, &xen_disable_rtc_set, 1, "");


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


/* These are peridically updated in shared_info, and then copied here. */
struct shadow_time_info {
	uint64_t tsc_timestamp;     /* TSC at last update of time vals.  */
	uint64_t system_timestamp;  /* Time, in nanosecs, since boot.    */
	uint32_t tsc_to_nsec_mul;
	uint32_t tsc_to_usec_mul;
	int tsc_shift;
	uint32_t version;
};
static DEFINE_PER_CPU(uint64_t, processed_system_time);
static DEFINE_PER_CPU(struct shadow_time_info, shadow_time);


#define NS_PER_TICK (1000000000ULL/hz)

#define rdtscll(val) \
    __asm__ __volatile__("rdtsc" : "=A" (val))


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
	return (cyc * cyc2ns_scale) >> CYC2NS_SCALE_FACTOR;
}

/*
 * Scale a 64-bit delta by scaling and multiplying by a 32-bit fraction,
 * yielding a 64-bit result.
 */
static inline uint64_t 
scale_delta(uint64_t delta, uint32_t mul_frac, int shift)
{
	uint64_t product;
	uint32_t tmp1, tmp2;

	if ( shift < 0 )
		delta >>= -shift;
	else
		delta <<= shift;

	__asm__ (
		"mul  %5       ; "
		"mov  %4,%%eax ; "
		"mov  %%edx,%4 ; "
		"mul  %5       ; "
		"add  %4,%%eax ; "
		"xor  %5,%5    ; "
		"adc  %5,%%edx ; "
		: "=A" (product), "=r" (tmp1), "=r" (tmp2)
		: "a" ((uint32_t)delta), "1" ((uint32_t)(delta >> 32)), "2" (mul_frac) );

	return product;
}

static uint64_t get_nsec_offset(struct shadow_time_info *shadow)
{
	uint64_t now, delta;
	rdtscll(now);
	delta = now - shadow->tsc_timestamp;
	return scale_delta(delta, shadow->tsc_to_nsec_mul, shadow->tsc_shift);
}

static void update_wallclock(void)
{
	shared_info_t *s = HYPERVISOR_shared_info;

	do {
		shadow_tv_version = s->wc_version;
		rmb();
		shadow_tv.tv_sec  = s->wc_sec;
		shadow_tv.tv_nsec = s->wc_nsec;
		rmb();
	}
	while ((s->wc_version & 1) | (shadow_tv_version ^ s->wc_version));

}

/*
 * Reads a consistent set of time-base values from Xen, into a shadow data
 * area. Must be called with the xtime_lock held for writing.
 */
static void __get_time_values_from_xen(void)
{
	shared_info_t           *s = HYPERVISOR_shared_info;
	struct vcpu_time_info   *src;
	struct shadow_time_info *dst;

	src = &s->vcpu_info[smp_processor_id()].time;
	dst = &per_cpu(shadow_time, smp_processor_id());

	do {
		dst->version = src->version;
		rmb();
		dst->tsc_timestamp     = src->tsc_timestamp;
		dst->system_timestamp  = src->system_time;
		dst->tsc_to_nsec_mul   = src->tsc_to_system_mul;
		dst->tsc_shift         = src->tsc_shift;
		rmb();
	}
	while ((src->version & 1) | (dst->version ^ src->version));

	dst->tsc_to_usec_mul = dst->tsc_to_nsec_mul / 1000;
}

static inline int time_values_up_to_date(int cpu)
{
	struct vcpu_time_info   *src;
	struct shadow_time_info *dst;

	src = &HYPERVISOR_shared_info->vcpu_info[cpu].time; 
	dst = &per_cpu(shadow_time, cpu); 

	rmb();
	return (dst->version == src->version);
}

static	unsigned xen_get_timecount(struct timecounter *tc);

static struct timecounter xen_timecounter = {
	xen_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"ixen",			/* name */
	0			/* quality */
};

static void 
clkintr(struct trapframe *frame)
{
	int64_t delta_cpu, delta;
	int cpu = smp_processor_id();
	struct shadow_time_info *shadow = &per_cpu(shadow_time, cpu);

	do {
		__get_time_values_from_xen();
		
		delta = delta_cpu = 
			shadow->system_timestamp + get_nsec_offset(shadow);
		delta     -= processed_system_time;
		delta_cpu -= per_cpu(processed_system_time, cpu);

	} while (!time_values_up_to_date(cpu));
	
	if (unlikely(delta < (int64_t)0) || unlikely(delta_cpu < (int64_t)0)) {
		printf("Timer ISR: Time went backwards: %lld\n", delta);
		return;
	}
	
	/* Process elapsed ticks since last call. */
	if (delta >= NS_PER_TICK) {
		processed_system_time += (delta / NS_PER_TICK) * NS_PER_TICK;
		per_cpu(processed_system_time, cpu) += (delta_cpu / NS_PER_TICK) * NS_PER_TICK;
	}
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));

	/*
	 * Take synchronised time from Xen once a minute if we're not
	 * synchronised ourselves, and we haven't chosen to keep an independent
	 * time base.
	 */
	
	if (shadow_tv_version != HYPERVISOR_shared_info->wc_version) {
		update_wallclock();
		tc_setclock(&shadow_tv);
	}
	
	/* XXX TODO */
}

static uint32_t
getit(void)
{
	struct shadow_time_info *shadow;
	shadow = &per_cpu(shadow_time, smp_processor_id());
	__get_time_values_from_xen();
	return shadow->system_timestamp + get_nsec_offset(shadow);
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


/*
 * Restore all the timers non-atomically (XXX: should be atomically).
 *
 * This function is called from pmtimer_resume() to restore all the timers.
 * This should not be necessary, but there are broken laptops that do not
 * restore all the timers on resume.
 */
void
timer_restore(void)
{
	/* Get timebases for new environment. */ 
	__get_time_values_from_xen();

	/* Reset our own concept of passage of system time. */
	processed_system_time = per_cpu(shadow_time, 0).system_timestamp;
	per_cpu(processed_system_time, 0) = processed_system_time;
}

void
startrtclock()
{
	unsigned long long alarm;
	uint64_t __cpu_khz;
	uint32_t cpu_khz;
	struct vcpu_time_info *info;

	/* initialize xen values */
	__get_time_values_from_xen();
	processed_system_time = per_cpu(shadow_time, 0).system_timestamp;
	per_cpu(processed_system_time, 0) = processed_system_time;

	__cpu_khz = 1000000ULL << 32;
	info = &HYPERVISOR_shared_info->vcpu_info[0].time;

	do_div(__cpu_khz, info->tsc_to_system_mul);
	if ( info->tsc_shift < 0 )
		cpu_khz = __cpu_khz << -info->tsc_shift;
	else
		cpu_khz = __cpu_khz >> info->tsc_shift;

	printf("Xen reported: %u.%03u MHz processor.\n", 
	       cpu_khz / 1000, cpu_khz % 1000);

	/* (10^6 * 2^32) / cpu_hz = (10^3 * 2^32) / cpu_khz =
	   (2^32 * 1 / (clocks/us)) */
	{	
		unsigned long eax=0, edx=1000;
		__asm__("divl %2"
			:"=a" (cached_gtm), "=d" (edx)
			:"r" (cpu_khz),
			"0" (eax), "1" (edx));
	}

	set_cyc2ns_scale(cpu_khz/1000);
	tsc_freq = cpu_khz * 1000;

        timer_freq = xen_timecounter.tc_frequency = 1000000000LL;
        tc_init(&xen_timecounter);


	rdtscll(alarm);
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

	shadow = &per_cpu(shadow_time, smp_processor_id());
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

static struct vcpu_set_periodic_timer xen_set_periodic_tick;

/*
 * Start clocks running.
 */
void
cpu_initclocks(void)
{
	int time_irq;

	xen_set_periodic_tick.period_ns = NS_PER_TICK;

	HYPERVISOR_vcpu_op(VCPUOP_set_periodic_timer, 0,
			   &xen_set_periodic_tick);

        if ((time_irq = bind_virq_to_irqhandler(VIRQ_TIMER, 0, "clk", 
						(driver_filter_t *)clkintr, NULL,
						INTR_TYPE_CLK | INTR_FAST)) < 0) {
		panic("failed to register clock interrupt\n");
	}

	/* should fast clock be enabled ? */
	
}

int
ap_cpu_initclocks(int cpu)
{
	int time_irq;

	xen_set_periodic_tick.period_ns = NS_PER_TICK;

	HYPERVISOR_vcpu_op(VCPUOP_set_periodic_timer, cpu,
			   &xen_set_periodic_tick);

        if ((time_irq = bind_virq_to_irqhandler(VIRQ_TIMER, cpu, "clk", 
						(driver_filter_t *)clkintr, NULL,
						INTR_TYPE_CLK | INTR_FAST)) < 0) {
		panic("failed to register clock interrupt\n");
	}

	return (0);
}


void
cpu_startprofclock(void)
{

    	printf("cpu_startprofclock: profiling clock is not supported\n");
}

void
cpu_stopprofclock(void)
{

    	printf("cpu_stopprofclock: profiling clock is not supported\n");
}
#define NSEC_PER_USEC 1000

static uint32_t
xen_get_timecount(struct timecounter *tc)
{	
	uint64_t clk;
	struct shadow_time_info *shadow;
	shadow = &per_cpu(shadow_time, smp_processor_id());

	__get_time_values_from_xen();
	
        clk = shadow->system_timestamp + get_nsec_offset(shadow);

	return (uint32_t)((clk / NS_PER_TICK) * NS_PER_TICK);

}

/* Return system time offset by ticks */
uint64_t
get_system_time(int ticks)
{
    return processed_system_time + (ticks * NS_PER_TICK);
}

/*
 * Track behavior of cur_timer->get_offset() functionality in timer_tsc.c
 */

#if 0
static uint32_t
xen_get_offset(void)
{
	register unsigned long eax, edx;

	/* Read the Time Stamp Counter */

	rdtsc(eax,edx);

	/* .. relative to previous jiffy (32 bits is enough) */
	eax -= shadow_tsc_stamp;

	/*
	 * Time offset = (tsc_low delta) * cached_gtm
	 *             = (tsc_low delta) * (usecs_per_clock)
	 *             = (tsc_low delta) * (usecs_per_jiffy / clocks_per_jiffy)
	 *
	 * Using a mull instead of a divl saves up to 31 clock cycles
	 * in the critical path.
	 */

	__asm__("mull %2"
		:"=a" (eax), "=d" (edx)
		:"rm" (cached_gtm),
		"0" (eax));

	/* our adjusted time offset in microseconds */
	return edx;
}
#endif
void
idle_block(void)
{

	__get_time_values_from_xen();
	PANIC_IF(HYPERVISOR_set_timer_op(processed_system_time + NS_PER_TICK) != 0);
	HYPERVISOR_sched_op(SCHEDOP_block, 0);
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


	
	
