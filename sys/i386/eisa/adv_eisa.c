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

#include "eisa.h"
#if NEISA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/eisa/eisaconf.h>

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

static int	adveisaprobe(void);
static int	adveisaattach(struct eisa_device *e_dev);

/* 
 * The overrun buffer shared amongst all EISA adapters.
 */
static	u_int8_t*	overrun_buf;
static	bus_dma_tag_t	overrun_dmat;
static	bus_dmamap_t	overrun_dmamap;
static	bus_addr_t	overrun_physbase;

static struct eisa_driver adv_eisa_driver =
{
	"adv",
	adveisaprobe,
	adveisaattach,
	/*shutdown*/NULL,
	&adv_unit
};

DATA_SET (eisadriver_set, adv_eisa_driver);

static const char *adveisamatch(eisa_id_t type);

static const char*
adveisamatch(type)
	eisa_id_t type;
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
adveisaprobe(void)
{
	u_int32_t iobase;
	u_int8_t irq;
	struct eisa_device *e_dev = NULL;
	int count;

	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, adveisamatch))) {
		iobase = (e_dev->ioconf.slot * EISA_SLOT_SIZE)
		       + ADV_EISA_SLOT_OFFSET;

		eisa_add_iospace(e_dev, iobase, ADV_EISA_IOSIZE, RESVADDR_NONE);
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
				       "irq setting %d\n", e_dev->ioconf.slot,
					irq);
				continue;
		}
		eisa_add_intr(e_dev, irq + 10);
		eisa_registerdev(e_dev, &adv_eisa_driver);
		count++;
	}
	return count;
}

static int
adveisaattach(struct eisa_device *e_dev)
{
	struct adv_softc *adv;
	struct adv_softc *adv_b;
	resvaddr_t *iospace;
	int unit;
	int irq;
	int error;

	adv_b = NULL;
	unit = e_dev->unit;
	iospace = e_dev->ioconf.ioaddrs.lh_first;

	if (TAILQ_FIRST(&e_dev->ioconf.irqs) == NULL)
		return (-1);

	irq = TAILQ_FIRST(&e_dev->ioconf.irqs)->irq_no;

	if (!iospace)
		return (-1);

	switch (e_dev->id & ~0xF) {
	case EISA_DEVICE_ID_ADVANSYS_750:
		adv_b = adv_alloc(unit, I386_BUS_SPACE_IO,
				  iospace->addr + ADV_EISA_OFFSET_CHAN2);
		if (adv_b == NULL)
			return (-1);
		
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
			return (-1);
		}

		adv_b->init_level++;

		/* FALLTHROUGH */
	case EISA_DEVICE_ID_ADVANSYS_740:
		adv = adv_alloc(unit, I386_BUS_SPACE_IO,
				iospace->addr + ADV_EISA_OFFSET_CHAN1);
		if (adv == NULL) {
			if (adv_b != NULL)
				adv_free(adv_b);
			return (-1);
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
			return (-1);
		}

		adv->init_level++;
		break;
	default: 
		printf("adveisaattach: Unknown device type!\n");
		return (-1);
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
			return (-1);
       		}
		if (bus_dmamem_alloc(overrun_dmat,
				     (void **)&overrun_buf,
				     BUS_DMA_NOWAIT,
				     &overrun_dmamap) != 0) {
			bus_dma_tag_destroy(overrun_dmat);
			adv_free(adv);
			return (-1);
		}
		/* And permanently map it in */  
		bus_dmamap_load(overrun_dmat, overrun_dmamap,
				overrun_buf, ADV_OVERRUN_BSIZE,
				adv_map, &overrun_physbase,
				/*flags*/0);
	}
	
	eisa_reg_start(e_dev);
	if (eisa_reg_iospace(e_dev, iospace)) {
		adv_free(adv);
		if (adv_b != NULL)
			adv_free(adv_b);
		return (-1);
	}

	if (eisa_reg_intr(e_dev, irq, adv_intr, (void *)adv, &cam_imask,
			 /*shared ==*/TRUE)) {
		adv_free(adv);
		if (adv_b != NULL)
			adv_free(adv_b);
		return (-1);
	}
	eisa_reg_end(e_dev);

	/*
	 * Now that we know we own the resources we need, do the 
	 * card initialization.
	 */

	/*
	 * Stop the chip.
	 */
	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
	ADV_OUTW(adv, ADV_CHIP_STATUS, 0);

	adv->chip_version = EISA_REVISION_ID(e_dev->id)
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

		adv_b->chip_version = EISA_REVISION_ID(e_dev->id)
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
	if (eisa_enable_intr(e_dev, irq)) {
		adv_free(adv);
		if (adv_b != NULL)
			adv_free(adv_b);
		eisa_release_intr(e_dev, irq, adv_intr);
		return (-1);
	}

	/* Attach sub-devices - always succeeds */
	adv_attach(adv);
	if (adv_b != NULL)
		adv_attach(adv_b);

	return 0;
}

#endif /* NEISA > 0 */
