/*
 * FreeBSD, EISA product support functions
 * 
 *
 * Copyright (c) 1994-1998, 2000, 2001 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *
 * $Id: //depot/aic7xxx/freebsd/dev/aic7xxx/ahc_eisa.c#9 $
 *
 * $FreeBSD$
 */

#include <dev/aic7xxx/aic7xxx_osm.h>

#include <dev/eisa/eisaconf.h>

static int
aic7770_probe(device_t dev)
{
	struct	 aic7770_identity *entry;
	struct	 resource *regs;
	uint32_t iobase;
	bus_space_handle_t bsh;
	bus_space_tag_t	tag;
	u_int	 irq;
	u_int	 intdef;
	u_int	 hcntrl;
	int	 shared;
	int	 rid;
	int	 error;

	entry = aic7770_find_device(eisa_get_id(dev));
	if (entry == NULL)
		return (ENXIO);
	device_set_desc(dev, entry->name);

	iobase = (eisa_get_slot(dev) * EISA_SLOT_SIZE) + AHC_EISA_SLOT_OFFSET;

	eisa_add_iospace(dev, iobase, AHC_EISA_IOSIZE, RESVADDR_NONE);

	rid = 0;
	regs = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				0, ~0, 1, RF_ACTIVE);
	if (regs == NULL) {
		device_printf(dev, "Unable to map I/O space?!\n");
		return ENOMEM;
	}

	tag = rman_get_bustag(regs);
	bsh = rman_get_bushandle(regs);
	error = 0;

	/* Pause the card preseving the IRQ type */
	hcntrl = bus_space_read_1(tag, bsh, HCNTRL) & IRQMS;
	bus_space_write_1(tag, bsh, HCNTRL, hcntrl | PAUSE);
	while ((bus_space_read_1(tag, bsh, HCNTRL) & PAUSE) == 0)
		;

	/* Make sure we have a valid interrupt vector */
	intdef = bus_space_read_1(tag, bsh, INTDEF);
	shared = (intdef & EDGE_TRIG) ? EISA_TRIGGER_EDGE : EISA_TRIGGER_LEVEL;
	irq = intdef & VECTOR;
	switch (irq) {
	case 9: 
	case 10:
	case 11:
	case 12:
	case 14:
	case 15:
		break;
	default:
		printf("aic7770 at slot %d: illegal irq setting %d\n",
		       eisa_get_slot(dev), intdef);
		error = ENXIO;
	}

	if (error == 0)
		eisa_add_intr(dev, irq, shared);

	bus_release_resource(dev, SYS_RES_IOPORT, rid, regs);
	return (error);
}

static int
aic7770_attach(device_t dev)
{
	struct	 aic7770_identity *entry;
	struct	 ahc_softc *ahc;
	char	*name;
	int	 error;

	entry = aic7770_find_device(eisa_get_id(dev));
	if (entry == NULL)
		return (ENXIO);

	/*
	 * Allocate a softc for this card and
	 * set it up for attachment by our
	 * common detect routine.
	 */
	name = malloc(strlen(device_get_nameunit(dev)) + 1, M_DEVBUF, M_NOWAIT);
	if (name == NULL)
		return (ENOMEM);
	strcpy(name, device_get_nameunit(dev));
	ahc = ahc_alloc(dev, name);
	if (ahc == NULL)
		return (ENOMEM);

	ahc_set_unit(ahc, device_get_unit(dev));

	/* Allocate a dmatag for our SCB DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
				   /*boundary*/0,
				   /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
				   /*highaddr*/BUS_SPACE_MAXADDR,
				   /*filter*/NULL, /*filterarg*/NULL,
				   /*maxsize*/MAXBSIZE, /*nsegments*/AHC_NSEG,
				   /*maxsegsz*/AHC_MAXTRANSFER_SIZE,
				   /*flags*/BUS_DMA_ALLOCNOW,
				   &ahc->parent_dmat);

	if (error != 0) {
		printf("ahc_eisa_attach: Could not allocate DMA tag "
		       "- error %d\n", error);
		ahc_free(ahc);
		return (ENOMEM);
	}
	ahc->dev_softc = dev;
	error = aic7770_config(ahc, entry, /*unused ioport arg*/0);
	if (error != 0) {
		ahc_free(ahc);
		return (error);
	}

	ahc_attach(ahc);
	return (0);
}

int
aic7770_map_registers(struct ahc_softc *ahc, u_int unused_ioport_arg)
{
	struct	resource *regs;
	int	rid;

	rid = 0;
	regs = bus_alloc_resource(ahc->dev_softc, SYS_RES_IOPORT,
				  &rid, 0, ~0, 1, RF_ACTIVE);
	if (regs == NULL) {
		device_printf(ahc->dev_softc, "Unable to map I/O space?!\n");
		return ENOMEM;
	}
	ahc->platform_data->regs_res_type = SYS_RES_IOPORT;
	ahc->platform_data->regs_res_id = rid,
	ahc->platform_data->regs = regs;
	ahc->tag = rman_get_bustag(regs);
	ahc->bsh = rman_get_bushandle(regs);
	return (0);
}

int
aic7770_map_int(struct ahc_softc *ahc, int irq)
{
	int zero;

	zero = 0;
	ahc->platform_data->irq =
	    bus_alloc_resource(ahc->dev_softc, SYS_RES_IRQ, &zero,
			       0, ~0, 1, RF_ACTIVE);
	if (ahc->platform_data->irq == NULL)
		return (ENOMEM);
	ahc->platform_data->irq_res_type = SYS_RES_IRQ;
	return (ahc_map_int(ahc));
}

static device_method_t ahc_eisa_device_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aic7770_probe),
	DEVMETHOD(device_attach,	aic7770_attach),
	DEVMETHOD(device_detach,	ahc_detach),
	{ 0, 0 }
};

static driver_t ahc_eisa_driver = {
	"ahc",
	ahc_eisa_device_methods,
	sizeof(struct ahc_softc)
};

DRIVER_MODULE(ahc_eisa, eisa, ahc_eisa_driver, ahc_devclass, 0, 0);
MODULE_DEPEND(ahc_eisa, ahc, 1, 1, 1);
MODULE_VERSION(ahc_eisa, 1);
