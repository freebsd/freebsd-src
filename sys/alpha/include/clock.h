/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/alpha/include/clock.h,v 1.3.2.1 1999/08/29 15:56:50 peter Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef KERNEL

extern	int	disable_rtc_set;
extern	int	wall_cmos_clock;
extern	int	adjkerntz;

void	DELAY __P((int usec));
int	sysbeep __P((int pitch, int period));
int	acquire_timer2 __P((int mode));
int	release_timer2 __P((void));

#endif /* KERNEL */

#endif /* !_MACHINE_CLOCK_H_ */
