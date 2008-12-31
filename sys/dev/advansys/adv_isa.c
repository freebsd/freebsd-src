/*-
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *   Connectivity Products:
 *	ABP510/5150 - Bus-Master ISA (240 CDB) *
 *	ABP5140 - Bus-Master ISA PnP (16 CDB) * **
 *	ABP5142 - Bus-Master ISA PnP with floppy (16 CDB) ***
 *
 *   Single Channel Products:
 *	ABP542 - Bus-Master ISA with floppy (240 CDB)
 *	ABP842 - Bus-Master VL (240 CDB) 
 *
 *   Dual Channel Products:  
 *	ABP852 - Dual Channel Bus-Master VL (240 CDB Per Channel)
 *
 *    * This board has been shipped by HP with the 4020i CD-R drive.
 *      The board has no BIOS so it cannot control a boot device, but 
 *      it can control any secondary SCSI device.
 *   ** This board has been sold by SIIG as the i540 SpeedMaster.
 *  *** This board has been sold by SIIG as the i542 SpeedMaster.
 *
 * Copyright (c) 1996, 1997 Justin T. Gibbs.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/advansys/adv_isa.c,v 1.31.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h> 
#include <sys/module.h> 
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h> 
#include <sys/rman.h> 

#include <isa/isavar.h>

#include <dev/advansys/advansys.h>

#include <cam/scsi/scsi_all.h>

#define ADV_ISA_MAX_DMA_ADDR    (0x00FFFFFFL)
#define ADV_ISA_MAX_DMA_COUNT   (0x00FFFFFFL)

#define ADV_VL_MAX_DMA_ADDR     (0x07FFFFFFL)
#define ADV_VL_MAX_DMA_COUNT    (0x07FFFFFFL)

/*
 * The overrun buffer shared amongst all ISA/VL adapters.
 */
static	u_int8_t*	overrun_buf;
static	bus_dma_tag_t	overrun_dmat;
static	bus_dmamap_t	overrun_dmamap;
static	bus_addr_t	overrun_physbase;

/* Possible port addresses an ISA or VL adapter can live at */
static u_int16_t adv_isa_ioports[] =
{
	0x100,
	0x110,	/* First selection in BIOS setup */
	0x120,
	0x130,	/* Second selection in BIOS setup */
	0x140,
	0x150,	/* Third selection in BIOS setup */
	0x190,	/* Fourth selection in BIOS setup */
	0x210,	/* Fifth selection in BIOS setup */
	0x230,	/* Sixth selection in BIOS setup */
	0x250,	/* Seventh selection in BIOS setup */
	0x330 	/* Eighth and default selection in BIOS setup */
};

#define MAX_ISA_IOPORT_INDEX (sizeof(adv_isa_ioports)/sizeof(u_int16_t) - 1)

static	int	adv_isa_probe(device_t dev);
static  int	adv_isa_attach(device_t dev);
static	void	adv_set_isapnp_wait_for_key(void);
static	int	adv_get_isa_dma_channel(struct adv_softc *adv);
static	int	adv_set_isa_dma_settings(struct adv_softc *adv);

static int
adv_isa_probe(device_t dev)
{
	int	port_index;
	int	max_port_index;
	u_long	iobase, iocount, irq;
	int	user_iobase = 0;
	int	rid = 0;
	void	*ih;
	struct resource	*iores, *irqres;

	/*
	 * We don't know of any PnP ID's for these cards.
	 */
	if (isa_get_logicalid(dev) != 0)
		return (ENXIO);

	/*
	 * Default to scanning all possible device locations.
	 */
	port_index = 0;
	max_port_index = MAX_ISA_IOPORT_INDEX;

	if (bus_get_resource(dev, SYS_RES_IOPORT, 0, &iobase, &iocount) == 0) {
		user_iobase = 1;
		for (;port_index <= max_port_index; port_index++)
			if (iobase <= adv_isa_ioports[port_index])
				break;
		if ((port_index > max_port_index)
		 || (iobase != adv_isa_ioports[port_index])) {
			if (bootverbose)
			    printf("adv%d: Invalid baseport of 0x%lx specified. "
				"Nearest valid baseport is 0x%x.  Failing "
				"probe.\n", device_get_unit(dev), iobase,
				(port_index <= max_port_index) ?
					adv_isa_ioports[port_index] :
					adv_isa_ioports[max_port_index]);
			return ENXIO;
		}
		max_port_index = port_index;
	}

	/* Perform the actual probing */
	adv_set_isapnp_wait_for_key();
	for (;port_index <= max_port_index; port_index++) {
		u_int16_t port_addr = adv_isa_ioports[port_index];
		bus_size_t maxsegsz;
		bus_size_t maxsize;
		bus_addr_t lowaddr;
		int error;
		struct adv_softc *adv;

		if (port_addr == 0)
			/* Already been attached */
			continue;
		
		if (bus_set_resource(dev, SYS_RES_IOPORT, 0, port_addr, 1))
			continue;

		/* XXX what is the real portsize? */
		iores = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid,
					       RF_ACTIVE);
		if (iores == NULL)
			continue;

		if (adv_find_signature(rman_get_bustag(iores),
				       rman_get_bushandle(iores)) == 0) {
			bus_release_resource(dev, SYS_RES_IOPORT, 0, iores);
			continue;
		}

		/*
		 * Got one.  Now allocate our softc
		 * and see if we can initialize the card.
		 */
		adv = adv_alloc(dev, rman_get_bustag(iores),
				rman_get_bushandle(iores));
		if (adv == NULL) {
			bus_release_resource(dev, SYS_RES_IOPORT, 0, iores);
			break;
		}

		/*
		 * Stop the chip.
		 */
		ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
		ADV_OUTW(adv, ADV_CHIP_STATUS, 0);
		/*
		 * Determine the chip version.
		 */
		adv->chip_version = ADV_INB(adv, ADV_NONEISA_CHIP_REVISION);
		if ((adv->chip_version >= ADV_CHIP_MIN_VER_VL)
		    && (adv->chip_version <= ADV_CHIP_MAX_VER_VL)) {
			adv->type = ADV_VL;
			maxsegsz = ADV_VL_MAX_DMA_COUNT;
			maxsize = BUS_SPACE_MAXSIZE_32BIT;
			lowaddr = ADV_VL_MAX_DMA_ADDR;
			bus_delete_resource(dev, SYS_RES_DRQ, 0);
		} else if ((adv->chip_version >= ADV_CHIP_MIN_VER_ISA)
			   && (adv->chip_version <= ADV_CHIP_MAX_VER_ISA)) {
			if (adv->chip_version >= ADV_CHIP_MIN_VER_ISA_PNP) {
				adv->type = ADV_ISAPNP;
				ADV_OUTB(adv, ADV_REG_IFC,
					 ADV_IFC_INIT_DEFAULT);
			} else {
				adv->type = ADV_ISA;
			}
			maxsegsz = ADV_ISA_MAX_DMA_COUNT;
			maxsize = BUS_SPACE_MAXSIZE_24BIT;
			lowaddr = ADV_ISA_MAX_DMA_ADDR;
			adv->isa_dma_speed = ADV_DEF_ISA_DMA_SPEED;
			adv->isa_dma_channel = adv_get_isa_dma_channel(adv);
			bus_set_resource(dev, SYS_RES_DRQ, 0,
					 adv->isa_dma_channel, 1);
		} else {
			panic("advisaprobe: Unknown card revision\n");
		}

		/*
		 * Allocate a parent dmatag for all tags created
		 * by the MI portions of the advansys driver
		 */
		/* XXX Should be a child of the ISA bus dma tag */ 
		error = bus_dma_tag_create(
				/* parent	*/ NULL,
				/* alignemnt	*/ 1,
				/* boundary	*/ 0,
				/* lowaddr	*/ lowaddr,
				/* highaddr	*/ BUS_SPACE_MAXADDR,
				/* filter	*/ NULL,
				/* filterarg	*/ NULL,
				/* maxsize	*/ maxsize,
				/* nsegments	*/ ~0,
				/* maxsegsz	*/ maxsegsz,
				/* flags	*/ 0,
				/* lockfunc	*/ busdma_lock_mutex,
				/* lockarg	*/ &Giant,
				&adv->parent_dmat); 

		if (error != 0) {
			printf("%s: Could not allocate DMA tag - error %d\n",
			       adv_name(adv), error); 
			adv_free(adv); 
			bus_release_resource(dev, SYS_RES_IOPORT, 0, iores);
			break;
		}

		adv->init_level += 2;

		if (overrun_buf == NULL) {
			/* Need to allocate our overrun buffer */
			if (bus_dma_tag_create(
				/* parent	*/ adv->parent_dmat,
				/* alignment	*/ 8,
				/* boundary	*/ 0,
				/* lowaddr	*/ ADV_ISA_MAX_DMA_ADDR,
				/* highaddr	*/ BUS_SPACE_MAXADDR,
				/* filter	*/ NULL,
				/* filterarg	*/ NULL,
				/* maxsize	*/ ADV_OVERRUN_BSIZE,
				/* nsegments	*/ 1,
				/* maxsegsz	*/ BUS_SPACE_MAXSIZE_32BIT,
				/* flags	*/ 0,
				/* lockfunc	*/ NULL,
				/* lockarg	*/ NULL,
				&overrun_dmat) != 0) {
				adv_free(adv);
				bus_release_resource(dev, SYS_RES_IOPORT, 0,
						     iores);
				break;
			}
			if (bus_dmamem_alloc(overrun_dmat,
					     (void **)&overrun_buf,
					     BUS_DMA_NOWAIT,
					     &overrun_dmamap) != 0) {
				bus_dma_tag_destroy(overrun_dmat);
				adv_free(adv);
				bus_release_resource(dev, SYS_RES_IOPORT, 0,
						     iores);
				break;
			}
			/* And permanently map it in */  
			bus_dmamap_load(overrun_dmat, overrun_dmamap,
					overrun_buf, ADV_OVERRUN_BSIZE,
					adv_map, &overrun_physbase,
					/*flags*/0);
		}

		adv->overrun_physbase = overrun_physbase;

		if (adv_init(adv) != 0) {
			bus_dmamap_unload(overrun_dmat, overrun_dmamap);
			bus_dmamem_free(overrun_dmat, overrun_buf,
			    overrun_dmamap);
			bus_dma_tag_destroy(overrun_dmat);
			adv_free(adv);
			bus_release_resource(dev, SYS_RES_IOPORT, 0, iores);
			break;
		}

		switch (adv->type) {
		case ADV_ISAPNP:
			if (adv->chip_version == ADV_CHIP_VER_ASYN_BUG) {
				adv->bug_fix_control
				    |= ADV_BUG_FIX_ASYN_USE_SYN;
				adv->fix_asyn_xfer = ~0;
			}
			/* Fall Through */
		case ADV_ISA:
			adv->max_dma_count = ADV_ISA_MAX_DMA_COUNT;
			adv->max_dma_addr = ADV_ISA_MAX_DMA_ADDR;
			adv_set_isa_dma_settings(adv);
			break;

		case ADV_VL:
			adv->max_dma_count = ADV_VL_MAX_DMA_COUNT;
			adv->max_dma_addr = ADV_VL_MAX_DMA_ADDR;
			break;
		default:
			panic("advisaprobe: Invalid card type\n");
		}
			
		/* Determine our IRQ */
		if (bus_get_resource(dev, SYS_RES_IRQ, 0, &irq, NULL))
			bus_set_resource(dev, SYS_RES_IRQ, 0,
					 adv_get_chip_irq(adv), 1);
		else
			adv_set_chip_irq(adv, irq);

		irqres = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
						RF_ACTIVE);
		if (irqres == NULL ||
		    bus_setup_intr(dev, irqres, INTR_TYPE_CAM|INTR_ENTROPY,
		        NULL, adv_intr, adv, &ih)) {
			bus_dmamap_unload(overrun_dmat, overrun_dmamap);
			bus_dmamem_free(overrun_dmat, overrun_buf,
			    overrun_dmamap);
			bus_dma_tag_destroy(overrun_dmat);
			adv_free(adv);
			bus_release_resource(dev, SYS_RES_IOPORT, 0, iores);
			break;
		}

		/* Mark as probed */
		adv_isa_ioports[port_index] = 0;
		return 0;
	}

	if (user_iobase)
		bus_set_resource(dev, SYS_RES_IOPORT, 0, iobase, iocount);
	else
		bus_delete_resource(dev, SYS_RES_IOPORT, 0);

	return ENXIO;
}

static int
adv_isa_attach(device_t dev)
{
	struct adv_softc *adv = device_get_softc(dev);

	return (adv_attach(adv));
}

static int
adv_get_isa_dma_channel(struct adv_softc *adv)
{
	int channel;

	channel = ADV_INW(adv, ADV_CONFIG_LSW) & ADV_CFG_LSW_ISA_DMA_CHANNEL;  
	if (channel == 0x03)
		return (0);
	else if (channel == 0x00)
		return (7);
	return (channel + 4);
}

static int
adv_set_isa_dma_settings(struct adv_softc *adv)
{
	u_int16_t cfg_lsw;
	u_int8_t  value;

	if ((adv->isa_dma_channel >= 5) && (adv->isa_dma_channel <= 7)) { 
	        if (adv->isa_dma_channel == 7)
			value = 0x00;
		else     
			value = adv->isa_dma_channel - 4;
		cfg_lsw = ADV_INW(adv, ADV_CONFIG_LSW)
			& ~ADV_CFG_LSW_ISA_DMA_CHANNEL;  
		cfg_lsw |= value;
		ADV_OUTW(adv, ADV_CONFIG_LSW, cfg_lsw);

		adv->isa_dma_speed &= 0x07;
		adv_set_bank(adv, 1);
		ADV_OUTB(adv, ADV_DMA_SPEED, adv->isa_dma_speed);
		adv_set_bank(adv, 0);
		isa_dmacascade(adv->isa_dma_channel);
	}
	return (0);
}

static void
adv_set_isapnp_wait_for_key(void)
{
	static	int isapnp_wait_set = 0;
	if (isapnp_wait_set == 0) {
		outb(ADV_ISA_PNP_PORT_ADDR, 0x02);
		outb(ADV_ISA_PNP_PORT_WRITE, 0x02);
		isapnp_wait_set++;
	}
}

static device_method_t adv_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		adv_isa_probe),
	DEVMETHOD(device_attach,	adv_isa_attach),
	{ 0, 0 }
};

static driver_t adv_isa_driver = {
	"adv", adv_isa_methods, sizeof(struct adv_softc)
};

static devclass_t adv_isa_devclass;
DRIVER_MODULE(adv, isa, adv_isa_driver, adv_isa_devclass, 0, 0);
MODULE_DEPEND(adv, isa, 1, 1, 1);
