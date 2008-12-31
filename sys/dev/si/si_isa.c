/*-
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 *
 * Copyright (C) 2000, Peter Wemm <peter@netplex.com.au>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/si/si_isa.c,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_debug_si.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/si/sireg.h>
#include <dev/si/sivar.h>

#include <isa/isavar.h>

/* Look for a valid board at the given mem addr */
static int
si_isa_probe(device_t dev)
{
	struct si_softc *sc;
	int type;
	u_int i, ramsize;
	volatile unsigned char was, *ux;
	volatile unsigned char *maddr;
	unsigned char *paddr;
	int unit;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
					    &sc->sc_mem_rid,
					    0, ~0, SIPROBEALLOC, RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory resource\n");
		return ENXIO;
	}
	paddr = (caddr_t)rman_get_start(sc->sc_mem_res);/* physical */
	maddr = rman_get_virtual(sc->sc_mem_res);	/* in kvm */

	DPRINT((0, DBG_AUTOBOOT, "si%d: probe at virtual=0x%x physical=0x%x\n",
		unit, maddr, paddr));

	/*
	 * this is a lie, but it's easier than trying to handle caching
	 * and ram conflicts in the >1M and <16M region.
	 */
	if ((caddr_t)paddr < (caddr_t)0xA0000 ||
	    (caddr_t)paddr >= (caddr_t)0x100000) {
		device_printf(dev, "maddr (%p) out of range\n", paddr);
		goto fail;
	}

	if (((uintptr_t)paddr & 0x7fff) != 0) {
		device_printf(dev, "maddr (%p) not on 32k boundary\n", paddr);
		goto fail;
	}

	/* Is there anything out there? (0x17 is just an arbitrary number) */
	*maddr = 0x17;
	if (*maddr != 0x17) {
		device_printf(dev, "0x17 check fail at phys %p\n", paddr);
		goto fail;
	}
	/*
	 * Let's look first for a JET ISA card, since that's pretty easy
	 *
	 * All jet hosts are supposed to have this string in the IDROM,
	 * but it's not worth checking on self-IDing busses like PCI.
	 */
	{
		unsigned char *jet_chk_str = "JET HOST BY KEV#";

		for (i = 0; i < strlen(jet_chk_str); i++)
			if (jet_chk_str[i] != *(maddr + SIJETIDSTR + 2 * i))
				goto try_mk2;
	}
	DPRINT((0, DBG_AUTOBOOT|DBG_FAIL, "si%d: JET first check - 0x%x\n",
		unit, (*(maddr+SIJETIDBASE))));
	if (*(maddr+SIJETIDBASE) != (SISPLXID&0xff))
		goto try_mk2;
	DPRINT((0, DBG_AUTOBOOT|DBG_FAIL, "si%d: JET second check - 0x%x\n",
		unit, (*(maddr+SIJETIDBASE+2))));
	if (*(maddr+SIJETIDBASE+2) != ((SISPLXID&0xff00)>>8))
		goto try_mk2;
	/* It must be a Jet ISA or RIO card */
	DPRINT((0, DBG_AUTOBOOT|DBG_FAIL, "si%d: JET id check - 0x%x\n",
		unit, (*(maddr+SIUNIQID))));
	if ((*(maddr+SIUNIQID) & 0xf0) != 0x20)
		goto try_mk2;
	/* It must be a Jet ISA SI/XIO card */
	*(maddr + SIJETCONFIG) = 0;
	type = SIJETISA;
	ramsize = SIJET_RAMSIZE;
	goto got_card;

try_mk2:
	/*
	 * OK, now to see if whatever responded is really an SI card.
	 * Try for a MK II next (SIHOST2)
	 */
	for (i = SIPLSIG; i < SIPLSIG + 8; i++)
		if ((*(maddr+i) & 7) != (~(unsigned char)i & 7))
			goto try_mk1;

	/* It must be an SIHOST2 */
	*(maddr + SIPLRESET) = 0;
	*(maddr + SIPLIRQCLR) = 0;
	*(maddr + SIPLIRQSET) = 0x10;
	type = SIHOST2;
	ramsize = SIHOST2_RAMSIZE;
	goto got_card;

try_mk1:
	/*
	 * Its not a MK II, so try for a MK I (SIHOST)
	 */
	*(maddr+SIRESET) = 0x0;		/* reset the card */
	*(maddr+SIINTCL) = 0x0;		/* clear int */
	*(maddr+SIRAM) = 0x17;
	if (*(maddr+SIRAM) != (unsigned char)0x17)
		goto fail;
	*(maddr+0x7ff8) = 0x17;
	if (*(maddr+0x7ff8) != (unsigned char)0x17) {
		device_printf(dev, "0x17 check fail at phys %p = 0x%x\n",
		    paddr+0x77f8, *(maddr+0x77f8));
		goto fail;
	}

	/* It must be an SIHOST (maybe?) - there must be a better way XXX */
	type = SIHOST;
	ramsize = SIHOST_RAMSIZE;

got_card:
	DPRINT((0, DBG_AUTOBOOT, "si%d: found type %d card, try memory test\n",
		unit, type));
	/* Try the acid test */
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++, ux++)
		*ux = (unsigned char)(i&0xff);
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++, ux++) {
		if ((was = *ux) != (unsigned char)(i&0xff)) {
			device_printf(dev,
			    "memtest fail at phys %p, was %x should be %x\n",
			    paddr + i, was, i & 0xff);
			goto fail;
		}
	}

	/* clear out the RAM */
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++)
		*ux++ = 0;
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++) {
		if ((was = *ux++) != 0) {
			device_printf(dev, "clear fail at phys %p, was %x\n",
			    paddr + i, was);
			goto fail;
		}
	}

	/*
	 * Success, we've found a valid board, now fill in
	 * the adapter structure.
	 */
	switch (type) {
	case SIHOST2:
		switch (isa_get_irq(dev)) {
		case 11:
		case 12:
		case 15:
			break;
		default:
			device_printf(dev,
			    "bad IRQ value - %d (11, 12, 15 allowed)\n",
			    isa_get_irq(dev));
			goto fail;
		}
		sc->sc_memsize = SIHOST2_MEMSIZE;
		break;
	case SIHOST:
		switch (isa_get_irq(dev)) {
		case 11:
		case 12:
		case 15:
			break;
		default:
			device_printf(dev,
			    "bad IRQ value - %d (11, 12, 15 allowed)\n",
			    isa_get_irq(dev));
			goto fail;
		}
		sc->sc_memsize = SIHOST_MEMSIZE;
		break;
	case SIJETISA:
		switch (isa_get_irq(dev)) {
		case 9:
		case 10:
		case 11:
		case 12:
		case 15:
			break;
		default:
			device_printf(dev,
			    "bad IRQ value - %d (9, 10, 11, 12, 15 allowed)\n",
			    isa_get_irq(dev));
			goto fail;
		}
		sc->sc_memsize = SIJETISA_MEMSIZE;
		break;
	case SIMCA:		/* MCA */
	default:
		device_printf(dev, "card type %d not supported\n", type);
		goto fail;
	}
	sc->sc_type = type;
	bus_release_resource(dev, SYS_RES_MEMORY,
			     sc->sc_mem_rid, sc->sc_mem_res);
	sc->sc_mem_res = 0;
	return (0);		/* success! */

fail:
	if (sc->sc_mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->sc_mem_rid, sc->sc_mem_res);
		sc->sc_mem_res = 0;
	}
	return(EINVAL);
}

static int
si_isa_attach(device_t dev)
{
	int error;
	void *ih;
	struct si_softc *sc;

	error = 0;
	ih = NULL;
	sc = device_get_softc(dev);

	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
						&sc->sc_mem_rid,
						RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "couldn't map memory\n");
		goto fail;
	}
	sc->sc_paddr = (caddr_t)rman_get_start(sc->sc_mem_res);
	sc->sc_maddr = rman_get_virtual(sc->sc_mem_res);

	sc->sc_irq_rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, 
						&sc->sc_irq_rid,
						RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "couldn't allocate interrupt\n");
		goto fail;
	}
	sc->sc_irq = rman_get_start(sc->sc_irq_res);
	error = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_TTY,
			       NULL, si_intr, sc, &ih);
	if (error) {
		device_printf(dev, "couldn't activate interrupt\n");
		goto fail;
	}

	error = siattach(dev);
	if (error)
		goto fail;
	return (0);		/* success */

fail:
	if (error == 0)
		error = ENXIO;
	if (sc->sc_irq_res) {
		if (ih)
			bus_teardown_intr(dev, sc->sc_irq_res, ih);
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->sc_irq_rid, sc->sc_irq_res);
		sc->sc_irq_res = 0;
	}
	if (sc->sc_mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->sc_mem_rid, sc->sc_mem_res);
		sc->sc_mem_res = 0;
	}
	return (error);
}

static device_method_t si_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		si_isa_probe),
	DEVMETHOD(device_attach,	si_isa_attach),

	{ 0, 0 }
};

static driver_t si_isa_driver = {
	"si",
	si_isa_methods,
	sizeof(struct si_softc),
};

DRIVER_MODULE(si, isa, si_isa_driver, si_devclass, 0, 0);
