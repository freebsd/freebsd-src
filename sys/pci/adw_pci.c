/*
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *	ABP940UW - Bus-Master PCI Ultra-Wide (240 CDB)
 *
 * Copyright (c) 1998 Justin Gibbs.
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
 * $FreeBSD: src/sys/pci/adw_pci.c,v 1.8 1999/08/28 00:50:41 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <cam/cam.h>
#include <cam/scsi/scsi_all.h>

#include <dev/advansys/adwvar.h>
#include <dev/advansys/adwlib.h>
#include <dev/advansys/adwmcode.h>

#define PCI_BASEADR0	PCI_MAP_REG_START	/* I/O Address */
#define PCI_BASEADR1	PCI_MAP_REG_START + 4	/* Mem I/O Address */

#define	PCI_DEVICE_ID_ADVANSYS_3550	0x230010CD

#define ADW_PCI_MAX_DMA_ADDR    (0xFFFFFFFFUL)
#define ADW_PCI_MAX_DMA_COUNT   (0xFFFFFFFFUL)

static const char* adwpciprobe(pcici_t tag, pcidi_t type);
static void adwpciattach(pcici_t config_id, int unit);

static struct  pci_device adw_pci_driver = {
	"adw",
        adwpciprobe,
        adwpciattach,
        &adw_unit,
	NULL
};

COMPAT_PCI_DRIVER (adw_pci, adw_pci_driver);

static const char*
adwpciprobe(pcici_t tag, pcidi_t type)
{
	switch (type) {
	case PCI_DEVICE_ID_ADVANSYS_3550:
		return ("AdvanSys ASC3550 SCSI controller");
	default:
		break;
	}
	return (NULL);
}

static void
adwpciattach(pcici_t config_id, int unit)
{
	u_int32_t	   id;
	u_int32_t	   command;
	vm_offset_t	   vaddr;
#ifdef ADW_ALLOW_MEMIO
	vm_offset_t	   paddr;
#endif
	u_int16_t	   io_port;
	bus_space_tag_t    tag;
	bus_space_handle_t bsh;
	struct adw_softc  *adw;
	int		   error;
 
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
	vaddr = 0;
#ifdef ADW_ALLOW_MEMIO
	if ((command & PCI_COMMAND_MEM_ENABLE) == 0
	 || (pci_map_mem(config_id, PCI_BASEADR1, &vaddr, &paddr)) == 0)
#endif
		if ((command & PCI_COMMAND_IO_ENABLE) == 0
		 || (pci_map_port(config_id, PCI_BASEADR0, &io_port)) == 0)
			return;

	/* XXX Should be passed in by parent bus */
	/* XXX Why isn't the 0x10 offset incorporated into the reg defs? */
	if (vaddr != 0) {
		tag = I386_BUS_SPACE_MEM;
		bsh = vaddr;
	} else {
		tag = I386_BUS_SPACE_IO;
		bsh = io_port;
	}


	if (adw_find_signature(tag, bsh) == 0)
		return;

	adw = adw_alloc(unit, tag, bsh);
	if (adw == NULL)
		return;

	/* Allocate a dmatag for our transfer DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
				   /*boundary*/0,
				   /*lowaddr*/ADW_PCI_MAX_DMA_ADDR,
				   /*highaddr*/BUS_SPACE_MAXADDR,
				   /*filter*/NULL, /*filterarg*/NULL,
				   /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
				   /*nsegments*/BUS_SPACE_UNRESTRICTED,
				   /*maxsegsz*/ADW_PCI_MAX_DMA_COUNT,
				   /*flags*/0,
				   &adw->parent_dmat);

	adw->init_level++;
 
	if (error != 0) {
		printf("%s: Could not allocate DMA tag - error %d\n",
		       adw_name(adw), error);
		adw_free(adw);
		return;
	}

	adw->init_level++;

	if (adw_init(adw) != 0) {
		adw_free(adw);
		return;
	}

	/*
	 * If the PCI Configuration Command Register "Parity Error Response
	 * Control" Bit was clear (0), then set the microcode variable
	 * 'control_flag' CONTROL_FLAG_IGNORE_PERR flag to tell the microcode
	 * to ignore DMA parity errors.
	 */
	if ((command & PCIM_CMD_PERRESPEN) == 0)
		adw_lram_write_16(adw, ADW_MC_CONTROL_FLAG,
				  adw_lram_read_16(adw, ADW_MC_CONTROL_FLAG)
				  | ADW_MC_CONTROL_IGN_PERR);

	if ((pci_map_int(config_id, adw_intr, (void *)adw, &cam_imask)) == 0) {
		adw_free(adw);
		return;
	}
	
	adw_attach(adw);
}
