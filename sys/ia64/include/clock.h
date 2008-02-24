/*-
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/ia64/include/clock.h,v 1.11 2006/10/02 12:59:57 phk Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL

#define	CLOCK_VECTOR	254

extern uint64_t	ia64_clock_reload;
extern uint64_t	itc_frequency;

int sysbeep(int pitch, int period);

#endif

#endif /* !_MACHINE_CLOCK_H_ */
