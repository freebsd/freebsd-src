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

extern	int	disable_rtc_set;
extern	int	wall_cmos_clock;
extern	int	adjkerntz;

int	sysbeep __P((int pitch, int period));
int	acquire_timer2 __P((int mode));
int	release_timer2 __P((void));

#endif

#endif /* !_MACHINE_CLOCK_H_ */
