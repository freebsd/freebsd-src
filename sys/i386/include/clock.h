/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 *	$Id: clock.h,v 1.6 1995/11/29 19:57:16 wollman Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef I586_CPU

#define I586_CYCLECTR(x) \
	__asm __volatile(".byte 0x0f, 0x31" : "=A" (x))

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
	if(i586_ctr_rate) { \
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

#define		I586_CTR_RATE_SHIFT	8

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
#ifdef I586_CPU
extern unsigned	i586_ctr_rate;	/* fixed point */
extern long long i586_last_tick;
extern long long i586_ctr_bias;
#endif
extern int 	timer0_max_count;
extern u_int 	timer0_overflow_threshold;
extern u_int 	timer0_prescaler_count;

#ifdef I586_CPU
void	calibrate_cyclecounter __P((void));
#endif

#ifdef I586_CPU
static __inline u_long 
cpu_thisticklen(u_long dflt)
{
	long long old;
	long rv;
	
	if (i586_ctr_rate) {
		old = i586_last_tick;
		I586_CYCLECTR(i586_last_tick);
		rv = ((i586_last_tick - old) << I586_CTR_RATE_SHIFT)
			/ i586_ctr_rate;
	} else {
		rv = dflt;
	}
	return rv;
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
int	sysbeep __P((int pitch, int period));

#endif /* KERNEL && !LOCORE */

#endif /* !_MACHINE_CLOCK_H_ */
