/*
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *   Single Channel Products:
 *	ABP742 - Bus-Master EISA (240 CDB)
 *
 *   Dual Channel Products:  
 *	ABP752 - Dual Channel Bus-Master EISA (240 CDB Per Channel)
 *
 * Copyright (c) 1997 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/eisa/eisaconf.h>

#include <dev/advansys/advansys.h>

#define EISA_DEVICE_ID_ADVANSYS_740	0x04507400
#define EISA_DEVICE_ID_ADVANSYS_750	0x04507500

#define ADV_EISA_SLOT_OFFSET		0xc00
#define ADV_EISA_OFFSET_CHAN1		0x30
#define ADV_EISA_OFFSET_CHAN2		0x50
#define ADV_EISA_IOSIZE			0x100

#define ADV_EISA_ROM_BIOS_ADDR_REG	0x86
#define ADV_EISA_IRQ_BURST_LEN_REG	0x87
#define 	ADV_EISA_IRQ_MASK	0x07
#define 	ADV_EISA_IRQ_10		0x00
#define 	ADV_EISA_IRQ_11		0x01
#define 	ADV_EISA_IRQ_12		0x02
#define 	ADV_EISA_IRQ_14		0x04
#define 	ADV_EISA_IRQ_15		0x05

#define	ADV_EISA_MAX_DMA_ADDR   (0x07FFFFFFL)
#define	ADV_EISA_MAX_DMA_COUNT  (0x07FFFFFFL)

/* 
 * The overrun buffer shared amongst all EISA adapters.
 */
static	u_int8_t*	overrun_buf;
static	bus_dma_tag_t	overrun_dmat;
static	bus_dmamap_t	overrun_dmamap;
static	bus_addr_t	overrun_physbase;

static const char*
adv_eisa_match(eisa_id_t type)
{
	switch (type & ~0xF) {
	case EISA_DEVICE_ID_ADVANSYS_740:
		return ("AdvanSys ABP-740/742 SCSI adapter");
		break;
	case EISA_DEVICE_ID_ADVANSYS_750:
		return ("AdvanSys ABP-750/752 SCSI adapter");
		break;
	default:
		break;
	}
	return (NULL);
}

static int
adv_eisa_probe(device_t dev)
{
	const char *desc;
	u_int32_t iobase;
	u_int8_t irq;

	desc = adv_eisa_match(eisa_get_id(dev));
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	iobase = (eisa_get_slot(dev) * EISA_SLOT_SIZE) + ADV_EISA_SLOT_OFFSET;

	eisa_add_iospace(dev, iobase, ADV_EISA_IOSIZE, RESVADDR_NONE);
	irq = inb(iobase + ADV_EISA_IRQ_BURST_LEN_REG);
	irq &= ADV_EISA_IRQ_MASK;
	switch (irq) {
	case 0:
	case 1:
	case 2:
	case 4:
	case 5:
	    break;
	default:
	    printf("adv at slot %d: illegal "
		   "irq setting %d\n", eisa_get_slot(dev),
		   irq);
	    return ENXIO;
	}
	eisa_add_intr(dev, irq + 10, EISA_TRIGGER_LEVEL);

	return 0;
}

static int
adv_eisa_attach(device_t dev)
{
	struct adv_softc *adv;
	struct adv_softc *adv_b;
	struct resource *io;
	struct resource *irq;
	int rid, error;
	void *ih;

	adv_b = NULL;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				0, ~0, 1, RF_ACTIVE);
	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		return ENOMEM;
	}

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (!irq) {
		device_printf(dev, "No irq?!\n");
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
		return ENOMEM;

	}

	switch (eisa_get_id(dev) & ~0xF) {
	case EISA_DEVICE_ID_ADVANSYS_750:
		adv_b = adv_alloc(dev, rman_get_bustag(io),
				  rman_get_bushandle(io) + ADV_EISA_OFFSET_CHAN2);
		if (adv_b == NULL)
			goto bad;
		
		/*
		 * Allocate a parent dmatag for all tags created
		 * by the MI portions of the advansys driver
		 */
		/* XXX Should be a child of the PCI bus dma tag */
		error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
					   /*boundary*/0,
					   /*lowaddr*/ADV_EISA_MAX_DMA_ADDR,
					   /*highaddr*/BUS_SPACE_MAXADDR,
					   /*filter*/NULL, /*filterarg*/NULL,
					   /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
					   /*nsegments*/BUS_SPACE_UNRESTRICTED,
					   /*maxsegsz*/ADV_EISA_MAX_DMA_COUNT,
					   /*flags*/0,
					   &adv_b->parent_dmat);
 
		if (error != 0) {
			printf("%s: Could not allocate DMA tag - error %d\n",
			       adv_name(adv_b), error);
			adv_free(adv_b);
			goto bad;
		}

		adv_b->init_level++;

		/* FALLTHROUGH */
	case EISA_DEVICE_ID_ADVANSYS_740:
		adv = adv_alloc(dev, rman_get_bustag(io),
				rman_get_bushandle(io) + ADV_EISA_OFFSET_CHAN1);
		if (adv == NULL) {
			if (adv_b != NULL)
				adv_free(adv_b);
			goto bad;
		}

		/*
		 * Allocate a parent dmatag for all tags created
		 * by the MI portions of the advansys driver
		 */
		/* XXX Should be a child of the PCI bus dma tag */
		error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
					   /*boundary*/0,
					   /*lowaddr*/ADV_EISA_MAX_DMA_ADDR,
					   /*highaddr*/BUS_SPACE_MAXADDR,
					   /*filter*/NULL, /*filterarg*/NULL,
					   /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
					   /*nsegments*/BUS_SPACE_UNRESTRICTED,
					   /*maxsegsz*/ADV_EISA_MAX_DMA_COUNT,
					   /*flags*/0,
					   &adv->parent_dmat);
 
		if (error != 0) {
			printf("%s: Could not allocate DMA tag - error %d\n",
			       adv_name(adv), error);
			adv_free(adv);
			goto bad;
		}

		adv->init_level++;
		break;
	default: 
		printf("adveisaattach: Unknown device type!\n");
		goto bad;
		break;
	}

	if (overrun_buf == NULL) {
		/* Need to allocate our overrun buffer */
		if (bus_dma_tag_create(adv->parent_dmat,
				       /*alignment*/8,
				       /*boundary*/0,
				       ADV_EISA_MAX_DMA_ADDR,
				       BUS_SPACE_MAXADDR,
				       /*filter*/NULL,
				       /*filterarg*/NULL,
				       ADV_OVERRUN_BSIZE,
				       /*nsegments*/1,
				       BUS_SPACE_MAXSIZE_32BIT,
				       /*flags*/0,
				       &overrun_dmat) != 0) {
			adv_free(adv);
			goto bad;
       		}
		if (bus_dmamem_alloc(overrun_dmat,
				     (void **)&overrun_buf,
				     BUS_DMA_NOWAIT,
				     &overrun_dmamap) != 0) {
			bus_dma_tag_destroy(overrun_dmat);
			adv_free(adv);
			goto bad;
		}
		/* And permanently map it in */  
		bus_dmamap_load(overrun_dmat, overrun_dmamap,
				overrun_buf, ADV_OVERRUN_BSIZE,
				adv_map, &overrun_physbase,
				/*flags*/0);
	}
	
	/*
	 * Now that we know we own the resources we need, do the 
	 * card initialization.
	 */

	/*
	 * Stop the chip.
	 */
	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
	ADV_OUTW(adv, ADV_CHIP_STATUS, 0);

	adv->chip_version = EISA_REVISION_ID(eisa_get_id(dev))
			  + ADV_CHIP_MIN_VER_EISA - 1;

	if (adv_init(adv) != 0) {
		adv_free(adv);
		if (adv_b != NULL)
			adv_free(adv_b);
		return(-1);
	}

	adv->max_dma_count = ADV_EISA_MAX_DMA_COUNT;
	adv->max_dma_addr = ADV_EISA_MAX_DMA_ADDR;

	if (adv_b != NULL) {
		/*
		 * Stop the chip.
		 */
		ADV_OUTB(adv_b, ADV_CHIP_CTRL, ADV_CC_HALT);
		ADV_OUTW(adv_b, ADV_CHIP_STATUS, 0);

		adv_b->chip_version = EISA_REVISION_ID(eisa_get_id(dev))
				    + ADV_CHIP_MIN_VER_EISA - 1;

		if (adv_init(adv_b) != 0) {
			adv_free(adv_b);
		} else {
			adv_b->max_dma_count = ADV_EISA_MAX_DMA_COUNT;
			adv_b->max_dma_addr = ADV_EISA_MAX_DMA_ADDR;
		}
	}

	/*
	 * Enable our interrupt handler.
	 */
	bus_setup_intr(dev, irq, INTR_TYPE_CAM, adv_intr, adv, &ih);

	/* Attach sub-devices - always succeeds */
	adv_attach(adv);
	if (adv_b != NULL)
		adv_attach(adv_b);

	return 0;

 bad:
	bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	bus_release_resource(dev, SYS_RES_IRQ, 0, irq);
	return -1;
}

static device_method_t adv_eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		adv_eisa_probe),
	DEVMETHOD(device_attach,	adv_eisa_attach),
	{ 0, 0 }
};

static driver_t adv_eisa_driver = {
	"adv", adv_eisa_methods, sizeof(struct adv_softc)
};

static devclass_t adv_eisa_devclass;
DRIVER_MODULE(adv, eisa, adv_eisa_driver, adv_eisa_devclass, 0, 0);
