/* $FreeBSD$ */
/* $NetBSD: mcclock_tlsb.c,v 1.8 1998/05/13 02:50:29 thorpej Exp $ */

/*
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/clockvar.h>
#include <dev/dec/mcclockvar.h>

#include <alpha/tlsb/gbusvar.h>

#include <alpha/tlsb/tlsbreg.h>		/* XXX */

#include <dev/dec/mc146818reg.h>

#define	KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))
/*
 * Registers are 64 bytes apart (and 1 byte wide)
 */
#define	REGSHIFT	6

struct mcclock_tlsb_softc {
	unsigned long regbase;
};

static int	mcclock_tlsb_probe(device_t dev);
static int	mcclock_tlsb_attach(device_t dev);
static void	mcclock_tlsb_write(device_t, u_int, u_int);
static u_int	mcclock_tlsb_read(device_t, u_int);

static device_method_t mcclock_tlsb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mcclock_tlsb_probe),
	DEVMETHOD(device_attach,	mcclock_tlsb_attach),

	/* mcclock interface */
	DEVMETHOD(mcclock_write,	mcclock_tlsb_write),
	DEVMETHOD(mcclock_read,		mcclock_tlsb_read),

	/* clock interface */
	DEVMETHOD(clock_init,		mcclock_init),
	DEVMETHOD(clock_get,		mcclock_get),
	DEVMETHOD(clock_set,		mcclock_set),
	DEVMETHOD(clock_getsecs,	mcclock_getsecs),

	{ 0, 0 }
};

static driver_t mcclock_tlsb_driver = {
	"mcclock", mcclock_tlsb_methods, sizeof(struct mcclock_tlsb_softc),
};

static devclass_t mcclock_devclass;

int
mcclock_tlsb_probe(device_t dev)
{
	device_set_desc(dev, "MC146818A real time clock");
	return 0;
}

int
mcclock_tlsb_attach(device_t dev)
{
	struct mcclock_tlsb_softc *sc = device_get_softc(dev);

	/* XXX Should be bus.h'd, so we can accomodate the kn7aa. */

	sc->regbase = TLSB_GBUS_BASE + gbus_get_offset(dev);

	mcclock_attach(dev);
	return 0;
}

static void
mcclock_tlsb_write(device_t dev, u_int reg, u_int val)
{
	struct mcclock_tlsb_softc *sc = device_get_softc(dev);
	unsigned char *ptr = (unsigned char *)
		KV(sc->regbase + (reg << REGSHIFT));
	*ptr = val;
}

static u_int
mcclock_tlsb_read(device_t dev, u_int reg)
{
	struct mcclock_tlsb_softc *sc = device_get_softc(dev);
	unsigned char *ptr = (unsigned char *)
		KV(sc->regbase + (reg << REGSHIFT));
	return *ptr;
}

DRIVER_MODULE(mcclock, gbus, mcclock_tlsb_driver, mcclock_devclass, 0, 0);
