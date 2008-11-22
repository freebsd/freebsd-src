/* $NetBSD: mcclock.c,v 1.11 1998/04/19 07:50:25 jonathan Exp $ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/clockvar.h>
#include <dev/dec/mcclockvar.h>
#include <dev/dec/mc146818reg.h>

/*
 * XXX rate is machine-dependent.
 */
#ifdef __alpha__
#define MC_DEFAULTRATE MC_RATE_1024_Hz
#endif
#ifdef __pmax__
#define MC_DEFAULTRATE MC_RATE_256_Hz
#endif

void
mcclock_attach(device_t dev)
{
	/* Turn interrupts off, just in case. */
	MCCLOCK_WRITE(dev, MC_REGB, MC_REGB_BINARY | MC_REGB_24HR);

	clockattach(dev);
}

void
mcclock_init(device_t dev)
{
	MCCLOCK_WRITE(dev, MC_REGA, MC_BASE_32_KHz | MC_DEFAULTRATE);
	MCCLOCK_WRITE(dev, MC_REGB,
	    MC_REGB_PIE | MC_REGB_SQWE | MC_REGB_BINARY | MC_REGB_24HR);
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
void
mcclock_get(device_t dev, time_t base, struct clocktime *ct)
{
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(dev, &regs)
	splx(s);

	ct->sec = regs[MC_SEC];
	ct->min = regs[MC_MIN];
	ct->hour = regs[MC_HOUR];
	ct->dow = regs[MC_DOW];
	ct->day = regs[MC_DOM];
	ct->mon = regs[MC_MONTH];
	ct->year = regs[MC_YEAR];
}

/*
 * Reset the TODR based on the time value.
 */
void
mcclock_set(device_t dev, struct clocktime *ct)
{
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(dev, &regs);
	splx(s);

	regs[MC_SEC] = ct->sec;
	regs[MC_MIN] = ct->min;
	regs[MC_HOUR] = ct->hour;
	regs[MC_DOW] = ct->dow;
	regs[MC_DOM] = ct->day;
	regs[MC_MONTH] = ct->mon;
	regs[MC_YEAR] = ct->year;

	s = splclock();
	MC146818_PUTTOD(dev, &regs);
	splx(s);
}

int
mcclock_getsecs(device_t dev, int *secp)
{
	int timeout = 100000000;
	int sec;
	int s;

	s = splclock();
	for (;;) {
		if (!(MCCLOCK_READ(dev, MC_REGA) & MC_REGA_UIP)) {
			sec = MCCLOCK_READ(dev, MC_SEC);
			break;
		}
		if (--timeout == 0)
			goto fail;
	}

	splx(s);
	*secp = sec;
	return 0;

 fail:
	splx(s);
	return ETIMEDOUT;
}
