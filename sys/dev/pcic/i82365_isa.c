/*	$NetBSD: i82365_isasubr.c,v 1.3 1999/10/15 06:07:27 haya Exp $	*/
/*	$NetBSD: i82365_isa.c,v 1.11 1998/06/09 07:25:00 thorpej Exp $	*/
/* $FreeBSD$ */

/*
 * Copyright (c) 1998 Bill Sommerfeld.  All rights reserved.
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <isa/isavar.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/pcic/i82365reg.h>
#include <dev/pcic/i82365var.h>

#include "power_if.h"
#include "card_if.h"

/*****************************************************************************
 * Configurable parameters.
 *****************************************************************************/

/*
 * Default I/O allocation range.  If both are set to non-zero, these
 * values will be used instead.  Otherwise, the code attempts to probe
 * the bus width.  Systems with 10 address bits should use 0x300 and 0xff.
 * Systems with 12 address bits (most) should use 0x400 and 0xbff.
 */

#ifndef PCIC_ISA_ALLOC_IOBASE
#define	PCIC_ISA_ALLOC_IOBASE		0
#endif

#ifndef PCIC_ISA_ALLOC_IOSIZE
#define	PCIC_ISA_ALLOC_IOSIZE		0
#endif

int	pcic_isa_alloc_iobase = PCIC_ISA_ALLOC_IOBASE;
int	pcic_isa_alloc_iosize = PCIC_ISA_ALLOC_IOSIZE;


/*
 * Default IRQ allocation bitmask.  This defines the range of allowable
 * IRQs for PCCARD slots.  Useful if order of probing would screw up other
 * devices, or if PCIC hardware/cards have trouble with certain interrupt
 * lines.
 *
 * We disable IRQ 10 by default, since some common laptops (namely, the
 * NEC Versa series) reserve IRQ 10 for the docking station SCSI interface.
 */

#ifndef PCIC_ISA_INTR_ALLOC_MASK
#define	PCIC_ISA_INTR_ALLOC_MASK	0xfbff
#endif

int	pcic_isa_intr_alloc_mask = PCIC_ISA_INTR_ALLOC_MASK;

/*****************************************************************************
 * End of configurable parameters.
 *****************************************************************************/

#define PCICISADEBUG 1

#ifdef PCICISADEBUG
int	pcicisa_debug = PCICISADEBUG;
#define	DPRINTF(arg) if (pcicisa_debug) printf arg;
#define	DEVPRINTF(arg) if (pcicisa_debug) device_printf arg;
#else
#define	DPRINTF(arg)
#define	DEVPRINTF(arg)
#endif

static struct isa_pnp_id pcic_ids[] = {
	{PCIC_PNP_ACTIONTEC,		NULL},		/* AEI0218 */
	{PCIC_PNP_IBM3765,		NULL},		/* IBM3765 */
	{PCIC_PNP_82365,		NULL},		/* PNP0E00 */
	{PCIC_PNP_CL_PD6720,		NULL},		/* PNP0E01 */
	{PCIC_PNP_VLSI_82C146,		NULL},		/* PNP0E02 */
	{PCIC_PNP_82365_CARDBUS,	NULL},		/* PNP0E03 */
	{PCIC_PNP_SCM_SWAPBOX,		NULL},		/* SCM0469 */
	{0}
};

static void
pcic_isa_bus_width_probe (device_t dev)
{
	struct pcic_softc *sc = PCIC_SOFTC(dev);
	bus_space_handle_t ioh_high;
	int i, iobuswidth, tmp1, tmp2;
	int rid;
	u_long base;
	u_int32_t length;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct resource *r;

	base = rman_get_start(sc->port_res);
	length = rman_get_size(sc->port_res);
	iot = sc->iot;
	ioh = sc->ioh;

	/*
	 * figure out how wide the isa bus is.  Do this by checking if the
	 * pcic controller is mirrored 0x400 above where we expect it to be.
	 */

	iobuswidth = 12;
	rid = 1;
	r = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, base + 0x400,
	    base + 0x400 + length, length, RF_ACTIVE);
	if (!r) {
		printf("Can't allocated mirror area for pcic bus width probe\n");
		return;
	}
	ioh_high = rman_get_bushandle(r);
	for (i = 0; i < PCIC_NSLOTS; i++) {
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP) {
			/*
			 * read the ident flags from the normal space and
			 * from the mirror, and compare them
			 */

			bus_space_write_1(iot, ioh, PCIC_REG_INDEX,
			    sc->handle[i].sock + PCIC_IDENT);
			tmp1 = bus_space_read_1(iot, ioh, PCIC_REG_DATA);

			bus_space_write_1(iot, ioh_high, PCIC_REG_INDEX,
			    sc->handle[i].sock + PCIC_IDENT);
			tmp2 = bus_space_read_1(iot, ioh_high, PCIC_REG_DATA);

			if (tmp1 == tmp2)
				iobuswidth = 10;
		}
	}
	bus_release_resource(dev, SYS_RES_IOPORT, rid, r);

	/*
	 * XXX mycroft recommends I/O space range 0x400-0xfff .  I should put
	 * this in a header somewhere
	 */

	/*
	 * XXX some hardware doesn't seem to grok addresses in 0x400 range--
	 * apparently missing a bit or more of address lines. (e.g.
	 * CIRRUS_PD672X with Linksys EthernetCard ne2000 clone in TI
	 * TravelMate 5000--not clear which is at fault)
	 * 
	 * Add a kludge to detect 10 bit wide buses and deal with them,
	 * and also a config file option to override the probe.
	 */

	if (iobuswidth == 10) {
		sc->iobase = 0x300;
		sc->iosize = 0x0ff;
	} else {
#if 0
		/*
		 * This is what we'd like to use, but...
		 */
		sc->iobase = 0x400;
		sc->iosize = 0xbff;
#else
		/*
		 * ...the above bus width probe doesn't always work.
		 * So, experimentation has shown the following range
		 * to not lose on systems that 0x300-0x3ff loses on
		 * (e.g. the NEC Versa 6030X).
		 */
		sc->iobase = 0x330;
		sc->iosize = 0x0cf;
#endif
	}

	DEVPRINTF((dev, "bus_space_alloc range 0x%04lx-0x%04lx (probed)\n",
	    (long) sc->iobase, (long) sc->iobase + sc->iosize));

	if (pcic_isa_alloc_iobase && pcic_isa_alloc_iosize) {
		sc->iobase = pcic_isa_alloc_iobase;
		sc->iosize = pcic_isa_alloc_iosize;

		DEVPRINTF((dev, "bus_space_alloc range 0x%04lx-0x%04lx "
		    "(config override)\n", (long) sc->iobase,
		    (long) sc->iobase + sc->iosize));
	}
}

#if 0
static int
pcic_isa_check(device_t dev, u_int16_t addr)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int val, found;
	int rid;
	struct resource *res;

	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, addr, addr,
	    PCIC_IOSIZE, RF_ACTIVE);
	if (!res)
		return(ENXIO);
	iot = rman_get_bustag(res);
	ioh = rman_get_bushandle(res);
	found = 0;

	/*
	 * this could be done with a loop, but it would violate the
	 * abstraction
	 */
	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C0SA + PCIC_IDENT);
	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);
	if (pcic_ident_ok(val))
		found++;

	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C0SB + PCIC_IDENT);
	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);
	if (pcic_ident_ok(val))
		found++;

	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C1SA + PCIC_IDENT);
	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);
	if (pcic_ident_ok(val))
		found++;

	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C1SB + PCIC_IDENT);
	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);
	if (pcic_ident_ok(val))
		found++;

	bus_release_resource(dev, SYS_RES_IOPORT, rid, res);

	return (found);
}
#endif

static int
pcic_isa_probe(device_t dev)
{
	int error;
	struct resource *res;
	int rid;
	int i;
	u_long mem;

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, pcic_ids);
	if (error == ENXIO)
		return (ENXIO);

	/* If we had some other problem. */
	if (!(error == 0 || error == ENOENT))
		return (error);

	/* If we have the resources we need then we're good to go. */
	if (bus_get_resource_start(dev, SYS_RES_IOPORT, 0) == 0)
		return (ENXIO);
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_ACTIVE);
	if (res == NULL) {
		/*
		 * No IRQ specified, find one.  This can be due to the PnP
		 * data not specifying any IRQ
		 */
		for (i = 0; i < 16; i++) {
			if (((1 << i) & PCIC_INTR_IRQ_VALIDMASK) == 0)
				continue;
			res = bus_alloc_resource(dev, SYS_RES_IRQ,
			    &rid, i, i, 1, RF_ACTIVE);
			if (res != NULL)
				break;
		}
		if (res == NULL)
			return (ENXIO);
		mem = rman_get_start(res);
		bus_release_resource(dev, SYS_RES_IRQ, rid, res);	
		bus_set_resource(dev, SYS_RES_IRQ, 0, i, 1);
	} else {
		bus_release_resource(dev, SYS_RES_IRQ, rid, res);
	}
	/* XXX This might not be needed in future, get it directly from
	 * XXX parent */
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 
	    1 << 13, RF_ACTIVE);
	if (res == NULL) {
		/*
		 * We failed to get memory.  Since this XXX comment above
		 * indicates that this is transient, we try to get a hunk
		 * of memory in the isa hole.  Sure would be nice if there
		 * were some MI constants for this.
		 */
		res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 
		    0xa0000, 0xdffff, 1 << 13, RF_ACTIVE);
		if (res != NULL) {
			mem = rman_get_start(res);
			bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
			bus_set_resource(dev, SYS_RES_MEMORY, 0, mem, 1 << 13);
		}
	} else {
		bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
	}
	if (res == NULL) {
		device_printf(dev, "Cannot allocate mem\n");
		return ENOMEM;
	}
	return (0);
}

static int
pcic_isa_attach(device_t dev)
{
	int err = 0;

	if ((err = pcic_attach(dev)) == 0)
		pcic_isa_bus_width_probe (dev);
	return err;
}

static int
pcic_isa_detach(device_t dev)
{
	pcic_detach(dev);
	return 0;
}

static device_method_t pcic_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcic_isa_probe),
	DEVMETHOD(device_attach,	pcic_isa_attach),
	DEVMETHOD(device_detach,	pcic_isa_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	pcic_suspend),
	DEVMETHOD(device_resume,	pcic_resume),

	/* Bus Interface */
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	pcic_alloc_resource),
	DEVMETHOD(bus_release_resource,	pcic_release_resource),
	DEVMETHOD(bus_activate_resource, pcic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pcic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	pcic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	pcic_teardown_intr),

	/* pccard/cardbus interface */
	DEVMETHOD(card_set_res_flags, pcic_set_res_flags),
	DEVMETHOD(card_set_memory_offset, pcic_set_memory_offset),

	/* Power Interface */
	DEVMETHOD(power_enable_socket,	pcic_enable_socket),
	DEVMETHOD(power_disable_socket,	pcic_disable_socket),
	{ 0, 0 }
};

static driver_t pcic_driver = {
	"pcic",
	pcic_isa_methods,
	sizeof(struct pcic_softc)
};

static devclass_t pcic_devclass;

DRIVER_MODULE(pcic, isa, pcic_driver, pcic_devclass, 0, 0);
