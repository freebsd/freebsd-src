/* $FreeBSD$ */
/* $NetBSD: mcclock_ioasic.c,v 1.8 1997/09/02 13:20:14 thorpej Exp $ */

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
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/clockvar.h>
#include <dev/dec/mcclockvar.h>

#include <alpha/tlsb/gbusvar.h>

#include <alpha/tc/tcreg.h>
#include <alpha/tc/tcvar.h>
#include <alpha/tc/tcdevs.h>
#include <alpha/tc/ioasicreg.h>
#include <alpha/tc/ioasicvar.h>

#include <dev/dec/mc146818reg.h>



struct mcclock_ioasic_clockdatum {
	u_char	datum;
	char	pad[3];
};

#define	KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))
/*
 * Registers are 64 bytes apart (and 1 byte wide)
 */
#define	REGSHIFT	6

struct mcclock_ioasic_softc {
	unsigned long regbase;
	struct mcclock_ioasic_clockdatum *sc_dp;
};

static int	mcclock_ioasic_probe(device_t dev);
static int	mcclock_ioasic_attach(device_t dev);
static void	mcclock_ioasic_write(device_t, u_int, u_int);
static u_int	mcclock_ioasic_read(device_t, u_int);

static device_method_t mcclock_ioasic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mcclock_ioasic_probe),
	DEVMETHOD(device_attach,	mcclock_ioasic_attach),

	/* mcclock interface */
	DEVMETHOD(mcclock_write,	mcclock_ioasic_write),
	DEVMETHOD(mcclock_read,		mcclock_ioasic_read),

	/* clock interface */
	DEVMETHOD(clock_init,		mcclock_init),
	DEVMETHOD(clock_get,		mcclock_get),
	DEVMETHOD(clock_set,		mcclock_set),

	{ 0, 0 }
};

static driver_t mcclock_ioasic_driver = {
	"mcclock",
	mcclock_ioasic_methods,
	sizeof(struct mcclock_ioasic_softc),
};

static devclass_t mcclock_devclass;

int
mcclock_ioasic_probe(device_t dev)
{
	device_set_desc(dev, "MC146818A real time clock");
	return 0;
}

static int
mcclock_ioasic_attach(device_t dev)
{
	struct mcclock_ioasic_softc *sc = device_get_softc(dev);
	struct ioasic_dev *ioasic = device_get_ivars(dev);
	sc->sc_dp = (struct mcclock_ioasic_clockdatum *)ioasic->iada_addr;
	mcclock_attach(dev);
	return 0;
}

void
mcclock_ioasic_write(device_t dev, u_int reg, u_int datum)
{
	struct mcclock_ioasic_softc *sc = device_get_softc(dev);
	sc->sc_dp[reg].datum = datum;
}

u_int
mcclock_ioasic_read(device_t dev, u_int reg)
{
	struct mcclock_ioasic_softc *sc = device_get_softc(dev);
	return (sc->sc_dp[reg].datum);
}

DRIVER_MODULE(mcclock, ioasic, mcclock_ioasic_driver, mcclock_devclass, 0, 0);
