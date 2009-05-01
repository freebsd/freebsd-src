/*-
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/powerpc/include/clock.h,v 1.13.8.1 2009/04/15 03:14:26 kensmith Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL

struct trapframe;

int	sysbeep(int pitch, int period);
int	acquire_timer2(int mode);
int	release_timer2(void);

void	decr_intr(struct trapframe *);

#endif

#endif /* !_MACHINE_CLOCK_H_ */
