/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL
/*
 * i386 to clock driver interface.
 * XXX large parts of the driver and its interface are misplaced.
 */
extern int	adjkerntz;
extern int	disable_rtc_set;
extern int	statclock_disable;
extern u_int	timer_freq;
extern int	timer0_max_count;
extern u_int	tsc_freq;
extern int	tsc_is_broken;
extern int	wall_cmos_clock;
#ifdef APIC_IO
extern int	apic_8254_intr;
#endif

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
void	i8254_restore __P((void));

#endif /* _KERNEL */

#endif /* !_MACHINE_CLOCK_H_ */
