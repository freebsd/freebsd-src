/* $FreeBSD: src/sys/alpha/isa/mcclock_isa.c,v 1.7.2.1 2000/07/04 01:55:55 mjacob Exp $ */
/* $NetBSD: mcclock_tlsb.c,v 1.8 1998/05/13 02:50:29 thorpej Exp $ */

/*
 * Copyright (c) 1997 by Matthew Jacob
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>
#include <machine/clockvar.h>
#include <dev/dec/mcclockvar.h>
#include <dev/dec/mc146818reg.h>

struct mcclock_softc {
	struct resource	*port;
};

static int	mcclock_isa_probe(device_t dev);
static int	mcclock_isa_attach(device_t dev);
static void	mcclock_isa_write(device_t, u_int, u_int);
static u_int	mcclock_isa_read(device_t, u_int);

static device_method_t mcclock_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mcclock_isa_probe),
	DEVMETHOD(device_attach,	mcclock_isa_attach),

	/* mcclock interface */
	DEVMETHOD(mcclock_write,	mcclock_isa_write),
	DEVMETHOD(mcclock_read,		mcclock_isa_read),

	/* clock interface */
	DEVMETHOD(clock_init,		mcclock_init),
	DEVMETHOD(clock_get,		mcclock_get),
	DEVMETHOD(clock_set,		mcclock_set),
	DEVMETHOD(clock_getsecs,	mcclock_getsecs),

	{ 0, 0 }
};

static driver_t mcclock_isa_driver = {
	"mcclock",
	mcclock_isa_methods,
	1,			/* XXX no softc */
};

static devclass_t mcclock_devclass;

int
mcclock_isa_probe(device_t dev)
{
	struct mcclock_softc *sc = device_get_softc(dev);
	int rid;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	rid = 0;
	sc->port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				      0ul, ~0ul, 2, RF_ACTIVE);
	if (!sc->port)
		return ENXIO;

	device_set_desc(dev, "MC146818A real time clock");
	return 0;
}

int
mcclock_isa_attach(device_t dev)
{
	mcclock_attach(dev);
	return 0;
}

static void
mcclock_isa_write(device_t dev, u_int reg, u_int val)
{
	struct mcclock_softc *sc = device_get_softc(dev);
	bus_space_tag_t iot = rman_get_bustag(sc->port);
	bus_space_handle_t ioh = rman_get_bushandle(sc->port);

	bus_space_write_1(iot, ioh, 0, reg);
	bus_space_write_1(iot, ioh, 1, val);
}

static u_int
mcclock_isa_read(device_t dev, u_int reg)
{
	struct mcclock_softc *sc = device_get_softc(dev);
	bus_space_tag_t iot = rman_get_bustag(sc->port);
	bus_space_handle_t ioh = rman_get_bushandle(sc->port);

	bus_space_write_1(iot, ioh, 0, reg);
	return bus_space_read_1(iot, ioh, 1);
}

DRIVER_MODULE(mcclock, isa, mcclock_isa_driver, mcclock_devclass, 0, 0);
