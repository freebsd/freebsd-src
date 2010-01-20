/*-
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL

#define	CLOCK_VECTOR	254

extern uint64_t	ia64_clock_reload;

#endif

#endif /* !_MACHINE_CLOCK_H_ */
