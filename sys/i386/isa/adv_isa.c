/*
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
 *
 *      $Id: adv_isa.c,v 1.8 1998/12/22 18:14:12 gibbs Exp $
 */

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/malloc.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

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

static	int	advisaprobe(struct isa_device *id);
static  int	advisaattach(struct isa_device *id);
static	void	adv_set_isapnp_wait_for_key(void);
static	int	adv_get_isa_dma_channel(struct adv_softc *adv);
static	int	adv_set_isa_dma_settings(struct adv_softc *adv);

void	adv_isa_intr(void *unit);

struct isa_driver advdriver =
{
	advisaprobe,
	advisaattach,
	"adv"
};

static int
advisaprobe(struct isa_device *id)
{
	int	port_index;
	int	max_port_index;

	/*
	 * Default to scanning all possible device locations.
	 */
	port_index = 0;
	max_port_index = MAX_ISA_IOPORT_INDEX;

	if (id->id_iobase > 0) {
		for (;port_index <= max_port_index; port_index++)
			if (id->id_iobase <= adv_isa_ioports[port_index])
				break;
		if ((port_index > max_port_index)
		 || (id->id_iobase != adv_isa_ioports[port_index])) {
			printf("adv%d: Invalid baseport of 0x%x specified. "
				"Neerest valid baseport is 0x%x.  Failing "
				"probe.\n", id->id_unit, id->id_iobase,
				(port_index <= max_port_index) ?
					adv_isa_ioports[port_index] :
					adv_isa_ioports[max_port_index]);
			return 0;
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

		if (port_addr == 0)
			/* Already been attached */
			continue;
		id->id_iobase = port_addr;
		if (haveseen_isadev(id, CC_IOADDR | CC_QUIET))
			continue;

		if (adv_find_signature(I386_BUS_SPACE_IO, port_addr)) {
			/*
			 * Got one.  Now allocate our softc
			 * and see if we can initialize the card.
			 */
			struct adv_softc *adv;
			adv = adv_alloc(id->id_unit, I386_BUS_SPACE_IO,
					port_addr);
			if (adv == NULL)
				return (0);

			adv_unit++;

			id->id_iobase = adv->bsh;

			/*
			 * Stop the chip.
			 */
			ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
			ADV_OUTW(adv, ADV_CHIP_STATUS, 0);
			/*
			 * Determine the chip version.
			 */
			adv->chip_version = ADV_INB(adv,
						    ADV_NONEISA_CHIP_REVISION);
			if ((adv->chip_version >= ADV_CHIP_MIN_VER_VL)
			 && (adv->chip_version <= ADV_CHIP_MAX_VER_VL)) {
				adv->type = ADV_VL;
				maxsegsz = ADV_VL_MAX_DMA_COUNT;
				maxsize = BUS_SPACE_MAXSIZE_32BIT;
				lowaddr = ADV_VL_MAX_DMA_ADDR;
				id->id_drq = -1;				
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
				adv->isa_dma_channel =
				    adv_get_isa_dma_channel(adv);
				id->id_drq = adv->isa_dma_channel;
			} else {
				panic("advisaprobe: Unknown card revision\n");
			}

			/*
			 * Allocate a parent dmatag for all tags created
			 * by the MI portions of the advansys driver
			 */
			/* XXX Should be a child of the ISA bus dma tag */ 
			error =
			    bus_dma_tag_create(/*parent*/NULL,
					       /*alignemnt*/0,
					       /*boundary*/0,
					       lowaddr,
					       /*highaddr*/BUS_SPACE_MAXADDR,
					       /*filter*/NULL,
					       /*filterarg*/NULL,
					       maxsize,
					       /*nsegs*/BUS_SPACE_UNRESTRICTED,
					       maxsegsz,
					       /*flags*/0,
					       &adv->parent_dmat); 
 
			if (error != 0) {
				printf("%s: Could not allocate DMA tag - error %d\n",
				       adv_name(adv), error); 
				adv_free(adv); 
				return (0); 
			}

			adv->init_level++;

			if (overrun_buf == NULL) {
				/* Need to allocate our overrun buffer */
				if (bus_dma_tag_create(adv->parent_dmat,
						       /*alignment*/8,
						       /*boundary*/0,
						       ADV_ISA_MAX_DMA_ADDR,
						       BUS_SPACE_MAXADDR,
						       /*filter*/NULL,
						       /*filterarg*/NULL,
						       ADV_OVERRUN_BSIZE,
						       /*nsegments*/1,
						       BUS_SPACE_MAXSIZE_32BIT,
						       /*flags*/0,
						       &overrun_dmat) != 0) {
					adv_free(adv);
					return (0);
        			}
				if (bus_dmamem_alloc(overrun_dmat,
						     (void **)&overrun_buf,
						     BUS_DMA_NOWAIT,
						     &overrun_dmamap) != 0) {
					bus_dma_tag_destroy(overrun_dmat);
					adv_free(adv);
					return (0);
				}
				/* And permanently map it in */  
				bus_dmamap_load(overrun_dmat, overrun_dmamap,
						overrun_buf, ADV_OVERRUN_BSIZE,
                        			adv_map, &overrun_physbase,
						/*flags*/0);
			}

			adv->overrun_physbase = overrun_physbase;
			
			if (adv_init(adv) != 0) {
				adv_free(adv);
				return (0);
			}

			switch (adv->type) {
			case ADV_ISAPNP:
				if (adv->chip_version == ADV_CHIP_VER_ASYN_BUG){
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
			if (id->id_irq == 0 /* irq ? */)
				id->id_irq = 1 << adv_get_chip_irq(adv);
			else
				adv_set_chip_irq(adv, ffs(id->id_irq) - 1);

			id->id_intr = adv_isa_intr;
			
			/* Mark as probed */
			adv_isa_ioports[port_index] = 0;
			return 1;
		}
	}

	return 0;
}

static int
advisaattach(struct isa_device *id)
{
	struct adv_softc *adv;

	adv = advsoftcs[id->id_unit];
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
	return;                 
}

/*
 * Handle an ISA interrupt.
 * XXX should go away as soon as ISA interrupt handlers
 * take a (void *) arg.
 */
static void
adv_isa_intr(void *unit)
{
	struct adv_softc *arg = advsoftcs[(int)unit];
	adv_intr((void *)arg);
}
