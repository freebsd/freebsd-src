/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 *	$Id: clock.h,v 1.31 1998/02/01 22:45:23 bde Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#define	TSC_COMULTIPLIER_SHIFT	20
#define	TSC_MULTIPLIER_SHIFT	32

#ifdef KERNEL
/*
 * i386 to clock driver interface.
 * XXX almost all of it is misplaced.  i586 stuff is done in isa/clock.c
 * and isa stuff is done in i386/microtime.s and i386/support.s.
 */
extern int	adjkerntz;
extern int	disable_rtc_set;
extern int	statclock_disable;
extern u_int	timer_freq;
extern int	timer0_max_count;
extern u_int	tsc_freq;
extern int	wall_cmos_clock;

/*
 * Driver to clock driver interface.
 */
struct clockframe;

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
#endif
int	sysbeep __P((int pitch, int period));

#endif /* KERNEL */

#endif /* !_MACHINE_CLOCK_H_ */
