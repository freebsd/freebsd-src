/* $Id$ */
/* $NetBSD: mcclock.c,v 1.11 1998/04/19 07:50:25 jonathan Exp $ */

/*
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

#include <dev/dec/clockvar.h>
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


void	mcclock_init __P((struct device *));
void	mcclock_get __P((struct device *, time_t, struct clocktime *));
void	mcclock_set __P((struct device *, struct clocktime *));

const struct clockfns mcclock_clockfns = {
	mcclock_init, mcclock_get, mcclock_set,
};

#define	mc146818_write(dev, reg, datum)					\
	    (*(dev)->sc_busfns->mc_bf_write)(dev, reg, datum)
#define	mc146818_read(dev, reg)						\
	    (*(dev)->sc_busfns->mc_bf_read)(dev, reg)

void
mcclock_attach(sc, busfns)
	struct mcclock_softc *sc;
	const struct mcclock_busfns *busfns;
{
	sc->sc_busfns = busfns;

	/* Turn interrupts off, just in case. */
	mc146818_write(sc, MC_REGB, MC_REGB_BINARY | MC_REGB_24HR);

	clockattach(sc->sc_dev, &mcclock_clockfns);
}

void
mcclock_init(dev)
	device_t dev;
{
	struct mcclock_softc *sc = device_get_softc(dev);

	mc146818_write(sc, MC_REGA, MC_BASE_32_KHz | MC_DEFAULTRATE);
	mc146818_write(sc, MC_REGB,
	    MC_REGB_PIE | MC_REGB_SQWE | MC_REGB_BINARY | MC_REGB_24HR);
}

/*
 * Get the time of day, based on the clock's value and/or the base value.
 */
void
mcclock_get(dev, base, ct)
	struct device *dev;
	time_t base;
	struct clocktime *ct;
{
	struct mcclock_softc *sc = device_get_softc(dev);
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(sc, &regs)
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
mcclock_set(dev, ct)
	struct device *dev;
	struct clocktime *ct;
{
	struct mcclock_softc *sc = device_get_softc(dev);
	mc_todregs regs;
	int s;

	s = splclock();
	MC146818_GETTOD(sc, &regs);
	splx(s);

	regs[MC_SEC] = ct->sec;
	regs[MC_MIN] = ct->min;
	regs[MC_HOUR] = ct->hour;
	regs[MC_DOW] = ct->dow;
	regs[MC_DOM] = ct->day;
	regs[MC_MONTH] = ct->mon;
	regs[MC_YEAR] = ct->year;

	s = splclock();
	MC146818_PUTTOD(sc, &regs);
	splx(s);
}
