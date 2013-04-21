/*-
 * Copyright (C) 2012 Margarida Gouveia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/platform.h>
#include <machine/intr_machdep.h>
#include <machine/resource.h>

#include <powerpc/wii/wii_picreg.h>

#include "pic_if.h"

static int	wiipic_probe(device_t);
static int	wiipic_attach(device_t);
static void	wiipic_dispatch(device_t, struct trapframe *);
static void	wiipic_enable(device_t, unsigned int, unsigned int);
static void	wiipic_eoi(device_t, unsigned int);
static void	wiipic_mask(device_t, unsigned int);
static void	wiipic_unmask(device_t, unsigned int);

struct wiipic_softc {
	device_t		 sc_dev;
	struct resource		*sc_rres;
	bus_space_tag_t		 sc_bt;
	bus_space_handle_t	 sc_bh;
	int			 sc_rrid;
	int			 sc_vector[WIIPIC_NIRQ];
};

static device_method_t wiipic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wiipic_probe),
	DEVMETHOD(device_attach,	wiipic_attach),

	/* PIC interface */
	DEVMETHOD(pic_dispatch,		wiipic_dispatch),
	DEVMETHOD(pic_enable,		wiipic_enable),
	DEVMETHOD(pic_eoi,		wiipic_eoi),
	DEVMETHOD(pic_mask,		wiipic_mask),
	DEVMETHOD(pic_unmask,		wiipic_unmask),

        DEVMETHOD_END
};

static driver_t wiipic_driver = {
	"wiipic",
	wiipic_methods,
	sizeof(struct wiipic_softc)
};

static devclass_t wiipic_devclass;

DRIVER_MODULE(wiipic, wiibus, wiipic_driver, wiipic_devclass, 0, 0);

static __inline uint32_t
wiipic_imr_read(struct wiipic_softc *sc)
{

	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, WIIPIC_IMR));
}

static __inline void
wiipic_imr_write(struct wiipic_softc *sc, uint32_t imr)
{

	bus_space_write_4(sc->sc_bt, sc->sc_bh, WIIPIC_IMR, imr);
}

static __inline uint32_t
wiipic_icr_read(struct wiipic_softc *sc)
{

	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, WIIPIC_ICR));
}

static __inline void
wiipic_icr_write(struct wiipic_softc *sc, uint32_t icr)
{

	bus_space_write_4(sc->sc_bt, sc->sc_bh, WIIPIC_ICR, icr);
}

static int
wiipic_probe(device_t dev)
{
        device_set_desc(dev, "Nintendo Wii PIC");

        return (BUS_PROBE_NOWILDCARD);
}

static int
wiipic_attach(device_t dev)
{
	struct wiipic_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_rrid, RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		device_printf(dev, "could not alloc mem resource\n");
		return (ENXIO);
	}
	sc->sc_bt = rman_get_bustag(sc->sc_rres);
	sc->sc_bh = rman_get_bushandle(sc->sc_rres);

	/* Turn off all interrupts */
	wiipic_imr_write(sc, 0x00000000);
	wiipic_icr_write(sc, 0xffffffff);

	powerpc_register_pic(dev, 0, WIIPIC_NIRQ, 0, FALSE);

	return (0);
}

static void
wiipic_dispatch(device_t dev, struct trapframe *tf)
{
	struct wiipic_softc *sc;
	uint32_t irq;

	sc = device_get_softc(dev);
	irq = wiipic_icr_read(sc) & wiipic_imr_read(sc);
	if (irq == 0)
		return;
	irq = ffs(irq) - 1;
	KASSERT(irq < WIIPIC_NIRQ, ("bogus irq %d", irq));
	powerpc_dispatch_intr(sc->sc_vector[irq], tf);
}

static void
wiipic_enable(device_t dev, unsigned int irq, unsigned int vector)
{
	struct wiipic_softc *sc;

	KASSERT(irq < WIIPIC_NIRQ, ("bogus irq %d", irq));
	sc = device_get_softc(dev);
	sc->sc_vector[irq] = vector;
	wiipic_unmask(dev, irq);
}

static void
wiipic_eoi(device_t dev, unsigned int irq)
{
	struct wiipic_softc *sc;
	uint32_t icr;

	sc = device_get_softc(dev);
	icr = wiipic_icr_read(sc);
	icr |= (1 << irq);
	wiipic_icr_write(sc, icr);
}

static void
wiipic_mask(device_t dev, unsigned int irq)
{
	struct wiipic_softc *sc;
	uint32_t imr;

	sc = device_get_softc(dev);
	imr = wiipic_imr_read(sc);
	imr &= ~(1 << irq);
	wiipic_imr_write(sc, imr);
}

static void
wiipic_unmask(device_t dev, unsigned int irq)
{
	struct wiipic_softc *sc;
	uint32_t imr;

	sc = device_get_softc(dev);
	imr = wiipic_imr_read(sc);
	imr |= (1 << irq);
	wiipic_imr_write(sc, imr);
}
