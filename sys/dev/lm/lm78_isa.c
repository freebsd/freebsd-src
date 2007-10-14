/*	$FreeBSD$	*/
/*	$OpenBSD: lm78_isa.c,v 1.2 2007/07/01 21:48:57 cnst Exp $	*/

/*-
 * Copyright (c) 2005, 2006 Mark Kettenis
 * Copyright (c) 2007 Constantine A. Murenin, Google Summer of Code
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <sys/systm.h>

#include <sys/sensors.h>

#include <dev/lm/lm78var.h>

/* ISA registers */
#define LMC_ADDR	0x05
#define LMC_DATA	0x06

extern struct cfdriver lm_cd;

#if defined(LMDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

struct lm_isa_softc {
	struct lm_softc sc_lmsc;

	struct resource *sc_iores;
	int sc_iorid;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

static int lm_isa_probe(struct device *);
static int lm_isa_attach(struct device *);
static int lm_isa_detach(struct device *);
u_int8_t lm_isa_readreg(struct lm_softc *, int);
void lm_isa_writereg(struct lm_softc *, int, int);

static device_method_t lm_isa_methods[] = {
	/* Methods from the device interface */
	DEVMETHOD(device_probe,		lm_isa_probe),
	DEVMETHOD(device_attach,	lm_isa_attach),
	DEVMETHOD(device_detach,	lm_isa_detach),

	/* Terminate method list */
	{ 0, 0 }
};

static driver_t lm_isa_driver = {
	"lm",
	lm_isa_methods,
	sizeof (struct lm_isa_softc)
};

static devclass_t lm_devclass;

DRIVER_MODULE(lm, isa, lm_isa_driver, lm_devclass, NULL, NULL);

int
lm_isa_probe(struct device *dev)
{
	struct lm_isa_softc *sc = device_get_softc(dev);
	struct resource *iores;
	int iorid = 0;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int banksel, vendid, chipid, addr;

	iores = bus_alloc_resource(dev, SYS_RES_IOPORT, &iorid,
	    0ul, ~0ul, 8, RF_ACTIVE);
	if (iores == NULL) {
		DPRINTF(("%s: can't map i/o space\n", __func__));
		return (1);
	}
	iot = rman_get_bustag(iores);
	ioh = rman_get_bushandle(iores);

	/* Probe for Winbond chips. */
	bus_space_write_1(iot, ioh, LMC_ADDR, WB_BANKSEL);
	banksel = bus_space_read_1(iot, ioh, LMC_DATA);
	bus_space_write_1(iot, ioh, LMC_ADDR, WB_VENDID);
	vendid = bus_space_read_1(iot, ioh, LMC_DATA);
	if (((banksel & 0x80) && vendid == (WB_VENDID_WINBOND >> 8)) ||
	    (!(banksel & 0x80) && vendid == (WB_VENDID_WINBOND & 0xff)))
		goto found;

	/* Probe for ITE chips (and don't attach if we find one). */
	bus_space_write_1(iot, ioh, LMC_ADDR, 0x58 /*ITD_CHIPID*/);
	vendid = bus_space_read_1(iot, ioh, LMC_DATA);
	if (vendid == 0x90 /*IT_ID_IT87*/)
		goto notfound;

	/*
	 * Probe for National Semiconductor LM78/79/81.
	 *
	 * XXX This assumes the address has not been changed from the
	 * power up default.  This is probably a reasonable
	 * assumption, and if it isn't true, we should be able to
	 * access the chip using the serial bus.
	 */
	bus_space_write_1(iot, ioh, LMC_ADDR, LM_SBUSADDR);
	addr = bus_space_read_1(iot, ioh, LMC_DATA);
	if ((addr & 0xfc) == 0x2c) {
		bus_space_write_1(iot, ioh, LMC_ADDR, LM_CHIPID);
		chipid = bus_space_read_1(iot, ioh, LMC_DATA);

		switch (chipid & LM_CHIPID_MASK) {
		case LM_CHIPID_LM78:
		case LM_CHIPID_LM78J:
		case LM_CHIPID_LM79:
		case LM_CHIPID_LM81:
			goto found;
		}
	}

 notfound:
	bus_release_resource(dev, SYS_RES_IOPORT, iorid, iores);

	return (1);

 found:
	/* Bus-independent probe */
	sc->sc_lmsc.sc_dev = dev;
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_lmsc.lm_writereg = lm_isa_writereg;
	sc->sc_lmsc.lm_readreg = lm_isa_readreg;
	lm_probe(&sc->sc_lmsc);

	bus_release_resource(dev, SYS_RES_IOPORT, iorid, iores);
	sc->sc_iot = 0;
	sc->sc_ioh = 0;

	return (0);
}

int
lm_isa_attach(struct device *dev)
{
	struct lm_isa_softc *sc = device_get_softc(dev);
#ifdef notyet
	struct lm_softc *lmsc;
	int i;
	u_int8_t sbusaddr;
#endif

	sc->sc_iores = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->sc_iorid,
	    0ul, ~0ul, 8, RF_ACTIVE);
	if (sc->sc_iores == NULL) {
		device_printf(dev, "can't map i/o space\n");
		return (1);
	}
	sc->sc_iot = rman_get_bustag(sc->sc_iores);
	sc->sc_ioh = rman_get_bushandle(sc->sc_iores);

	/* Bus-independent attachment */
	lm_attach(&sc->sc_lmsc);

#ifdef notyet
	/*
	 * Most devices supported by this driver can attach to iic(4)
	 * as well.  However, we prefer to attach them to isa(4) since
	 * that causes less overhead and is more reliable.  We look
	 * through all previously attached devices, and if we find an
	 * identical chip at the same serial bus address, we stop
	 * updating its sensors and mark them as invalid.
	 */

	sbusaddr = lm_isa_readreg(&sc->sc_lmsc, LM_SBUSADDR);
	if (sbusaddr == 0)
		return (0);

	for (i = 0; i < lm_cd.cd_ndevs; i++) {
		lmsc = lm_cd.cd_devs[i];
		if (lmsc == &sc->sc_lmsc)
			continue;
		if (lmsc && lmsc->sbusaddr == sbusaddr &&
		    lmsc->chipid == sc->sc_lmsc.chipid)
			config_detach(&lmsc->sc_dev, 0);
	}
#endif
	return (0);
}

int
lm_isa_detach(struct device *dev)
{
	struct lm_isa_softc *sc = device_get_softc(dev);
	int error;

	/* Bus-independent detachment */
	error = lm_detach(&sc->sc_lmsc);
	if (error)
		return (error);

	error = bus_release_resource(dev, SYS_RES_IOPORT, sc->sc_iorid,
	    sc->sc_iores);
	if (error)
		return (error);

	return (0);
}

u_int8_t
lm_isa_readreg(struct lm_softc *lmsc, int reg)
{
	struct lm_isa_softc *sc = (struct lm_isa_softc *)lmsc;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LMC_ADDR, reg);
	return (bus_space_read_1(sc->sc_iot, sc->sc_ioh, LMC_DATA));
}

void
lm_isa_writereg(struct lm_softc *lmsc, int reg, int val)
{
	struct lm_isa_softc *sc = (struct lm_isa_softc *)lmsc;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LMC_ADDR, reg);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, LMC_DATA, val);
}
