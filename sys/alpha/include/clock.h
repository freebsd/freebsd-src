/*
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 *	$Id: clock.h,v 1.34 1998/03/05 21:45:42 tegge Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef KERNEL

extern	int	wall_cmos_clock;
extern	int	adjkerntz;

void	DELAY __P((int usec));
int	sysbeep __P((int pitch, int period));

#endif /* KERNEL */

#endif /* !_MACHINE_CLOCK_H_ */
