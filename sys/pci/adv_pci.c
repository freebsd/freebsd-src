/*
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *   Connectivity Products:
 *	ABP920   - Bus-Master PCI (16 CDB)
 *	ABP930   - Bus-Master PCI (16 CDB) *
 *	ABP930U  - Bus-Master PCI Ultra (16 CDB)
 *	ABP930UA - Bus-Master PCI Ultra (16 CDB)
 *	ABP960   - Bus-Master PCI MAC/PC (16 CDB) **
 *	ABP960U  - Bus-Master PCI MAC/PC Ultra (16 CDB)
 *
 *   Single Channel Products:
 *	ABP940 - Bus-Master PCI (240 CDB)
 *	ABP940U - Bus-Master PCI Ultra (240 CDB)
 *	ABP970 - Bus-Master PCI MAC/PC (240 CDB)
 *	ABP970U - Bus-Master PCI MAC/PC Ultra (240 CDB)
 *
 *   Dual Channel Products:  
 *	ABP950 - Dual Channel Bus-Master PCI (240 CDB Per Channel)
 *
 *   Footnotes:
 *	 * This board has been sold by SIIG as the Fast SCSI Pro PCI.
 *	** This board has been sold by Iomega as a Jaz Jet PCI adapter. 
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

#include <pci.h>
#if NPCI > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/advansys/advansys.h>

#define PCI_BASEADR0	PCI_MAP_REG_START	/* I/O Address */
#define PCI_BASEADR1	PCI_MAP_REG_START + 4	/* Mem I/O Address */

#define	PCI_DEVICE_ID_ADVANSYS_1200A	0x110010CD
#define	PCI_DEVICE_ID_ADVANSYS_1200B	0x120010CD
#define	PCI_DEVICE_ID_ADVANSYS_ULTRA	0x130010CD
#define	PCI_DEVICE_REV_ADVANSYS_3150	0x02
#define	PCI_DEVICE_REV_ADVANSYS_3050	0x03

#define ADV_PCI_MAX_DMA_ADDR    (0xFFFFFFFFL)
#define ADV_PCI_MAX_DMA_COUNT   (0xFFFFFFFFL)

static const char* advpciprobe(pcici_t tag, pcidi_t type);
static void advpciattach(pcici_t config_id, int unit);

/* 
 * The overrun buffer shared amongst all PCI adapters.
 */
static  u_int8_t*	overrun_buf;
static	bus_dma_tag_t	overrun_dmat;
static	bus_dmamap_t	overrun_dmamap;
static	bus_addr_t	overrun_physbase;

static struct  pci_device adv_pci_driver = {
	"adv",
        advpciprobe,
        advpciattach,
        &adv_unit,
	NULL
};

DATA_SET (pcidevice_set, adv_pci_driver);

static const char*
advpciprobe(pcici_t tag, pcidi_t type)
{
	int rev = pci_conf_read(tag, PCI_CLASS_REG) & 0xff;
	switch (type) {
	case PCI_DEVICE_ID_ADVANSYS_1200A:
		return ("AdvanSys ASC1200A SCSI controller");
	case PCI_DEVICE_ID_ADVANSYS_1200B:
		return ("AdvanSys ASC1200B SCSI controller");
	case PCI_DEVICE_ID_ADVANSYS_ULTRA:
		if (rev == PCI_DEVICE_REV_ADVANSYS_3150)
			return ("AdvanSys ASC3150 Ultra SCSI controller");
		else
			return ("AdvanSys ASC3050 Ultra SCSI controller");
		break;
	default:
		break;
	}
	return (NULL);
}

static void
advpciattach(pcici_t config_id, int unit)
{
	u_int16_t	io_port;
	struct		adv_softc *adv;
	u_int32_t	id;
	u_int32_t	command;
	int		error;
 
	/*
	 * Determine the chip version.
	 */
	id = pci_cfgread(config_id, PCI_ID_REG, /*bytes*/4);
	command = pci_cfgread(config_id, PCIR_COMMAND, /*bytes*/1);

	/*
	 * These cards do not allow memory mapped accesses, so we must
	 * ensure that I/O accesses are available or we won't be able
	 * to talk to them.
	 */
	if ((command & (PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN))
	 != (PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN)) {
		command |= PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN;
		pci_cfgwrite(config_id, PCIR_COMMAND, command, /*bytes*/1);
	}

	/*
	 * Early chips can't handle non-zero latency timer settings.
	 */
	if (id == PCI_DEVICE_ID_ADVANSYS_1200A
	 || id == PCI_DEVICE_ID_ADVANSYS_1200B) {
		pci_cfgwrite(config_id, PCIR_LATTIMER, /*value*/0, /*bytes*/1);
	}


	if (pci_map_port(config_id, PCI_BASEADR0, &io_port) == 0)
		return;

	if (adv_find_signature(I386_BUS_SPACE_IO, io_port) == 0)
		return;

	adv = adv_alloc(unit, I386_BUS_SPACE_IO, io_port);
	if (adv == NULL)
		return;

	/* Allocate a dmatag for our transfer DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
				   /*boundary*/0,
				   /*lowaddr*/ADV_PCI_MAX_DMA_ADDR,
				   /*highaddr*/BUS_SPACE_MAXADDR,
				   /*filter*/NULL, /*filterarg*/NULL,
				   /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
				   /*nsegments*/BUS_SPACE_UNRESTRICTED,
				   /*maxsegsz*/ADV_PCI_MAX_DMA_COUNT,
				   /*flags*/0,
				   &adv->parent_dmat);
 
	if (error != 0) {
		printf("%s: Could not allocate DMA tag - error %d\n",
		       adv_name(adv), error);
		adv_free(adv);
		return;
	}

	adv->init_level++;

	if (overrun_buf == NULL) {
		/* Need to allocate our overrun buffer */
		if (bus_dma_tag_create(adv->parent_dmat,
				       /*alignment*/8, /*boundary*/0,
				       ADV_PCI_MAX_DMA_ADDR, BUS_SPACE_MAXADDR,
				       /*filter*/NULL, /*filterarg*/NULL,
				       ADV_OVERRUN_BSIZE, /*nsegments*/1,
				       BUS_SPACE_MAXSIZE_32BIT, /*flags*/0,
				       &overrun_dmat) != 0) {
			bus_dma_tag_destroy(adv->parent_dmat);
			adv_free(adv);
			return;
       		}
		if (bus_dmamem_alloc(overrun_dmat,
				     (void **)&overrun_buf,
				     BUS_DMA_NOWAIT,
				     &overrun_dmamap) != 0) {
			bus_dma_tag_destroy(overrun_dmat);
			bus_dma_tag_destroy(adv->parent_dmat);
			adv_free(adv);
			return;
		}
		/* And permanently map it in */  
		bus_dmamap_load(overrun_dmat, overrun_dmamap,
				overrun_buf, ADV_OVERRUN_BSIZE,
				adv_map, &overrun_physbase,
				/*flags*/0);
	}

	adv->overrun_physbase = overrun_physbase;
			
	/*
	 * Stop the chip.
	 */
	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
	ADV_OUTW(adv, ADV_CHIP_STATUS, 0);

	adv->chip_version = ADV_INB(adv, ADV_NONEISA_CHIP_REVISION);
	adv->type = ADV_PCI;
	
	/*
	 * Setup active negation and signal filtering.
	 */
	{
		u_int8_t extra_cfg;

		if (adv->chip_version >= ADV_CHIP_VER_PCI_ULTRA_3150)
			adv->type |= ADV_ULTRA;
		if (adv->chip_version == ADV_CHIP_VER_PCI_ULTRA_3150)
			extra_cfg = ADV_IFC_ACT_NEG | ADV_IFC_SLEW_RATE;
		else if (adv->chip_version == ADV_CHIP_VER_PCI_ULTRA_3050)
			extra_cfg = ADV_IFC_ACT_NEG | ADV_IFC_WR_EN_FILTER;
		else
			extra_cfg = ADV_IFC_ACT_NEG | ADV_IFC_SLEW_RATE;
		ADV_OUTB(adv, ADV_REG_IFC, extra_cfg);
	}

	if (adv_init(adv) != 0) {
		adv_free(adv);
		return;
	}

	adv->max_dma_count = ADV_PCI_MAX_DMA_COUNT;
	adv->max_dma_addr = ADV_PCI_MAX_DMA_ADDR;

#if CC_DISABLE_PCI_PARITY_INT
	{
		u_int16_t config_msw;

		config_msw = ADV_INW(adv, ADV_CONFIG_MSW);
		config_msw &= 0xFFC0;
		ADV_OUTW(adv, ADV_CONFIG_MSW, config_msw); 
	}
#endif
 
	if (id == PCI_DEVICE_ID_ADVANSYS_1200A
	 || id == PCI_DEVICE_ID_ADVANSYS_1200B) {
		adv->bug_fix_control |= ADV_BUG_FIX_IF_NOT_DWB;
		adv->bug_fix_control |= ADV_BUG_FIX_ASYN_USE_SYN;
		adv->fix_asyn_xfer = ~0;
	}

	if ((pci_map_int(config_id, adv_intr, (void *)adv, &cam_imask)) == 0) {
		adv_free(adv);
		return;
	}
	
	adv_attach(adv);
}

#endif /* NPCI > 0 */
