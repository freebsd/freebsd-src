/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 *	$Id: clock.h,v 1.16 1996/08/02 21:16:13 bde Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#include "opt_cpu.h"

#if defined(I586_CPU) || defined(I686_CPU)

/*
 * When we update the clock, we also update this bias value which is
 * automatically subtracted in microtime().  We assume that CPU_THISTICKLEN()
 * has been called at some point in the past, so that an appropriate value is
 * set up in i586_last_tick.  (This works even if we are not being called
 * from hardclock because hardclock will have run before and will made the
 * call.)
 */
#define CPU_CLOCKUPDATE(otime, ntime) \
	do { \
	if (i586_ctr_freq != 0) { \
		disable_intr(); \
		i586_ctr_bias = i586_last_tick; \
		*(otime) = *(ntime); \
		enable_intr(); \
	} else { \
		*(otime) = *(ntime); \
	} \
	} while(0)

#define	CPU_THISTICKLEN(dflt) cpu_thisticklen(dflt)
#else
#define CPU_CLOCKUPDATE(otime, ntime) \
		(*(otime) = *(ntime))
#define CPU_THISTICKLEN(dflt) dflt
#endif

#define	I586_CTR_COMULTIPLIER_SHIFT	20
#define	I586_CTR_MULTIPLIER_SHIFT	32

#if defined(KERNEL) && !defined(LOCORE)
#include <sys/cdefs.h>
#include <machine/frame.h>

/*
 * i386 to clock driver interface.
 * XXX almost all of it is misplaced.  i586 stuff is done in isa/clock.c
 * and isa stuff is done in i386/microtime.s and i386/support.s.
 */
extern int	adjkerntz;
extern int	disable_rtc_set;
extern int	statclock_disable;
extern int	wall_cmos_clock;

#if defined(I586_CPU) || defined(I686_CPU)
extern u_int	i586_ctr_bias;
extern u_int	i586_ctr_comultiplier;
extern u_int	i586_ctr_freq;
extern u_int	i586_ctr_multiplier;
extern long long i586_last_tick;
extern unsigned long i586_avg_tick;
#endif
extern int 	timer0_max_count;
extern u_int 	timer0_overflow_threshold;
extern u_int 	timer0_prescaler_count;


#if defined(I586_CPU) || defined(I686_CPU)
static __inline u_long 
cpu_thisticklen(u_long dflt)
{
	long long old;
	long len;

	if (i586_ctr_freq != 0) {
		old = i586_last_tick;
		i586_last_tick = rdtsc();
		len = ((i586_last_tick - old) * i586_ctr_multiplier)
			>> I586_CTR_MULTIPLIER_SHIFT;
		i586_avg_tick = i586_avg_tick * 15 / 16 + len / 16;
	}
	return dflt;
}
#endif

/*
 * Driver to clock driver interface.
 */
void	DELAY __P((int usec));
int	acquire_timer0 __P((int rate,
			    void (*function)(struct clockframe *frame)));
int	acquire_timer2 __P((int mode));
int	release_timer0 __P((void));
int	release_timer2 __P((void));
#ifndef PC98
int	rtcin __P((int val));
#else
int	acquire_timer1 __P((int mode));
int	release_timer1 __P((void));
void	rtc_serialcombit __P((int i));
void	rtc_serialcom __P((int i));
void	rtc_outb __P((int val));
#endif
int	sysbeep __P((int pitch, int period));

#endif /* KERNEL && !LOCORE */

#endif /* !_MACHINE_CLOCK_H_ */
