/*
 *       Copyright (c) 1997 by Simon Shapiro
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 */

/*
 *  dptpci.c:  PCI Bus Attachment for DPT SCSI HBAs
 */

#ident "$FreeBSD$"

#include "opt_devfs.h"
#include "opt_dpt.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <cam/scsi/scsi_all.h>

#include <dev/dpt/dpt.h>
#include <pci/dpt_pci.h>

#define PCI_BASEADR0  PCI_MAP_REG_START      /* I/O Address */
#define PCI_BASEADR1  PCI_MAP_REG_START + 4  /* Mem I/O Address */

#define ISA_PRIMARY_WD_ADDRESS    0x1f8

/* Global variables */

/* Function Prototypes */

static const char    *dpt_pci_probe(pcici_t tag, pcidi_t type);
static void     dpt_pci_attach(pcici_t config_id, int unit);

extern struct cdevsw dpt_cdevsw;

static  struct pci_device dpt_pci_driver =
{
	"dpt",
	dpt_pci_probe,
	dpt_pci_attach,
	&dpt_unit,
	NULL
};

DATA_SET(pcidevice_set, dpt_pci_driver);

/*
 * Probe the PCI device.
 * Some of this work will have to be duplicated in _attach
 * because we do not know for sure how the two relate.
 */

static const char *
dpt_pci_probe(pcici_t tag, pcidi_t type)
{
	u_int32_t  class;

#ifndef PCI_COMMAND_MASTER_ENABLE
#define PCI_COMMAND_MASTER_ENABLE 0x00000004
#endif

#ifndef PCI_SUBCLASS_MASS_STORAGE_SCSI
#define PCI_SUBCLASS_MASS_STORAGE_SCSI 0x00000000
#endif

	class = pci_conf_read(tag, PCI_CLASS_REG);
	if (((type & 0xffff0000) >> 16) == DPT_DEVICE_ID
	 && (class & PCI_CLASS_MASK) == PCI_CLASS_MASS_STORAGE
	 && (class & PCI_SUBCLASS_MASK) == PCI_SUBCLASS_MASS_STORAGE_SCSI)
		return ("DPT Caching SCSI RAID Controller");
	return (NULL);
}

static void
dpt_pci_attach(pcici_t config_id, int unit)
{
	dpt_softc_t	   *dpt;
	vm_offset_t	    vaddr;
#ifdef DPT_ALLOW_MEMIO
	vm_offset_t	    paddr;
#endif
	u_int16_t	    io_base;
	bus_space_tag_t     tag;
	bus_space_handle_t  bsh;
	u_int32_t	    command;
	int		    s;

	vaddr = NULL;
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
#ifdef DPT_ALLOW_MEMIO
	if ((command & PCI_COMMAND_MEM_ENABLE) == 0
	 || (pci_map_mem(config_id, PCI_BASEADR1, &vaddr, &paddr)) == 0)
#endif
		if ((command & PCI_COMMAND_IO_ENABLE) == 0
		 || (pci_map_port(config_id, PCI_BASEADR0, &io_base)) == 0)
			return;

	/*
	 * If the DPT is mapped as an IDE controller,
	 * let it be IDE controller
	 */
	if (io_base == ISA_PRIMARY_WD_ADDRESS - 0x10) {
#ifdef DPT_DEBUG_WARN
		printf("dpt%d: Mapped as an IDE controller.  "
		       "Disabling SCSI setup\n", unit);
#endif
		return;
	}

	/* XXX Should be passed in by parent bus */
	/* XXX Why isn't the 0x10 offset incorporated into the reg defs? */
	if (vaddr != 0) {
		tag = I386_BUS_SPACE_MEM;
		bsh = vaddr + 0x10;
	} else {
		tag = I386_BUS_SPACE_IO;
		bsh = io_base + 0x10;
	}

	if ((dpt = dpt_alloc(unit, tag, bsh)) == NULL)
		return;  /* XXX PCI code should take return status */
	
	/* Allocate a dmatag representing the capabilities of this attachment */
	/* XXX Should be a child of the PCI bus dma tag */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignemnt*/1, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
			       /*nsegments*/BUS_SPACE_UNRESTRICTED,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &dpt->parent_dmat) != 0) {
		dpt_free(dpt);
		return;
	}

	if (pci_map_int(config_id, dpt_intr, (void *)dpt, &cam_imask) == 0) {
		dpt_free(dpt);
		return;
	}

	s = splcam();
	if (dpt_init(dpt) != 0) {
		dpt_free(dpt);
		return;
	}

	/* Register with the XPT */
	dpt_attach(dpt);
	splx(s);
}
