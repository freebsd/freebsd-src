/*	$NetBSD: i82365_isa.c,v 1.11 1998/06/09 07:25:00 thorpej Exp $	*/
/* $FreeBSD$ */

/*
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
#include <dev/pccard/pccardchip.h>

#include <dev/pcic/i82365reg.h>
#include <dev/pcic/i82365var.h>
#include <dev/pcic/i82365_isavar.h>

#ifdef PCICISADEBUG
int	pcicisa_debug = 0 /* XXX */ ;
#define	DPRINTF(arg) if (pcicisa_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

static struct isa_pnp_id pcic_ids[] = {
	{PCIC_PNP_82365,		NULL},		/* PNP0E00 */
	{PCIC_PNP_CL_PD6720,		NULL},		/* PNP0E01 */
	{PCIC_PNP_VLSI_82C146,		NULL},		/* PNP0E02 */
	{PCIC_PNP_82365_CARDBUS,	NULL},		/* PNP0E03 */
	{0}
};

int	pcic_isa_probe(device_t dev);
int	pcic_isa_attach(device_t dev);

static struct pccard_chip_functions pcic_isa_functions = {
	pcic_chip_mem_alloc,
	pcic_chip_mem_free,
	pcic_chip_mem_map,
	pcic_chip_mem_unmap,

	pcic_chip_io_alloc,
	pcic_chip_io_free,
	pcic_chip_io_map,
	pcic_chip_io_unmap,

	pcic_isa_chip_intr_establish,
	pcic_isa_chip_intr_disestablish,

	pcic_chip_socket_enable,
	pcic_chip_socket_disable,
};

int
pcic_isa_probe(device_t dev)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int val, found;
	int rid;
	struct resource *res;

	/* Check isapnp ids */
	if (ISA_PNP_PROBE(device_get_parent(dev), dev, pcic_ids) == ENXIO)
		return (ENXIO);

	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, PCIC_IOSIZE,
	    RF_ACTIVE);
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

	/* XXX DO I NEED TO WORRY ABOUT the IRQ? XXX */

	if (!found)
		return (ENXIO);

	return (0);
}

int
pcic_isa_attach(device_t dev)
{
	struct pcic_softc *sc = (struct pcic_softc *) device_get_softc(dev);

	sc->port_rid = 0;
	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->port_rid,
	    0, ~0, PCIC_IOSIZE, RF_ACTIVE);
	if (!sc->port_res) {
		device_printf(dev, "Unable to allocate I/O ports\n");
		goto error;
	}
	
	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    0, ~0, 1 << 13, RF_ACTIVE);
	if (!sc->mem_res) {
		device_printf(dev, "Unable to allocate memory range\n");
		goto error;
	}
	
	sc->subregionmask = (1 << 
	    ((rman_get_end(sc->mem_res) - rman_get_start(sc->mem_res) + 1) / 
		PCIC_MEM_PAGESIZE)) - 1;

	sc->pct = (pccard_chipset_tag_t) & pcic_isa_functions;

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
	    0, ~0, 1, RF_ACTIVE);
	if (!sc->irq_res) {
		device_printf(dev, "Unable to allocate irq\n");
		goto error;
	}
	
#if 0
	/*
	 * allocate an irq.  it will be used by both controllers.  I could
	 * use two different interrupts, but interrupts are relatively
	 * scarce, shareable, and for PCIC controllers, very infrequent.
	 */

	if ((sc->irq = ia->ia_irq) == IRQUNK) {
		if (isa_intr_alloc(ic,
		    PCIC_CSC_INTR_IRQ_VALIDMASK & pcic_isa_intr_alloc_mask,
		    IST_EDGE, &sc->irq)) {
			printf("\n%s: can't allocate interrupt\n",
			    sc->dev.dv_xname);
			return;
		}
		printf(": using irq %d", sc->irq);
	}
#endif
	sc->iot = rman_get_bustag(sc->port_res);
	sc->ioh = rman_get_bushandle(sc->port_res);;
	sc->memt = rman_get_bustag(sc->mem_res);
	sc->memh = rman_get_bushandle(sc->mem_res);;

#if 0 /* Not yet */
	pcic_attach(dev);

	pcic_isa_bus_width_probe (dev, sc->iot, sc->ioh,
	    rman_get_start(sc->port_res), 
	    rman_get_end(sc->port_res) - rman_get_end(sc->port_res) + 1);

	pcic_attach_sockets(sc);
#endif
	return 0;
 error:
	if (sc->port_res)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->port_rid, 
		    sc->port_res);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, 
		    sc->mem_res);
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, 
		    sc->irq_res);
	return ENOMEM;
}

static int
pcic_isa_detach(device_t dev)
{
	return 0;
}



static device_method_t pcic_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcic_isa_probe),
	DEVMETHOD(device_attach,	pcic_isa_attach),
	DEVMETHOD(device_detach,	pcic_isa_detach),

	{ 0, 0 }
};

static driver_t pcic_driver = {
	"pcic",
	pcic_isa_methods,
	sizeof(struct pcic_softc)
};

static devclass_t pcic_devclass;

DRIVER_MODULE(pcic, isa, pcic_driver, pcic_devclass, 0, 0);
