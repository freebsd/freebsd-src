/*-
 * Copyright (c) 2000 Matthew N. Dodd <winter@jurai.net>
 * All rights reserved.
 *
 * Copyright (c) 1997 Simon Shapiro
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <cam/scsi/scsi_all.h>

#include <dev/dpt/dpt.h>

#define	DPT_VENDOR_ID		0x1044
#define	DPT_DEVICE_ID		0xa400

#define	DPT_PCI_IOADDR		PCIR_MAPS		/* I/O Address */
#define	DPT_PCI_MEMADDR		(PCIR_MAPS + 4)		/* Mem I/O Address */

#define	ISA_PRIMARY_WD_ADDRESS	0x1f8

static int	dpt_pci_probe	(device_t);
static int	dpt_pci_attach	(device_t);

static int
dpt_pci_probe (device_t dev)
{
	if ((pci_get_vendor(dev) == DPT_VENDOR_ID) &&
	    (pci_get_device(dev) == DPT_DEVICE_ID)) {
		device_set_desc(dev, "DPT Caching SCSI RAID Controller");
		return (0);
	}
	return (ENXIO);
}

static int
dpt_pci_attach (device_t dev)
{
	dpt_softc_t *	dpt;
	struct resource *io = 0;
	struct resource *irq = 0;
	int		s;
	int		rid;
	void *		ih;
	int		error = 0;

	int		iotype = 0;
	u_int32_t	command;

	command = pci_read_config(dev, PCIR_COMMAND, /*bytes*/1);

#ifdef DPT_ALLOW_MMIO
	if ((command & PCIM_CMD_MEMEN) != 0) {
		rid = DPT_PCI_MEMADDR;
		iotype = SYS_RES_MEMORY;
		io = bus_alloc_resource(dev, iotype, &rid, 0, ~0, 1, RF_ACTIVE);
	}
#endif
	if (io == NULL && (command &  PCIM_CMD_PORTEN) != 0) {
		rid = DPT_PCI_IOADDR;
		iotype = SYS_RES_IOPORT;
		io = bus_alloc_resource(dev, iotype, &rid, 0, ~0, 1, RF_ACTIVE);
	}

	if (io == NULL) {
		device_printf(dev, "can't allocate register resources\n");
		error = ENOMEM;
		goto bad;
	}

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				 RF_ACTIVE | RF_SHAREABLE);
	if (!irq) {
		device_printf(dev, "No irq?!\n");
		error = ENOMEM;
		goto bad;
	}

	/* Ensure busmastering is enabled */
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, /*bytes*/1);

	if (rman_get_start(io) == (ISA_PRIMARY_WD_ADDRESS - 0x10)) {
#ifdef DPT_DEBUG_WARN
		device_printf(dev, "Mapped as an IDE controller.  "
				   "Disabling SCSI setup\n");
#endif
		error = ENXIO;
		goto bad;
	}

	/* Device registers are offset 0x10 into the register window.  FEH */
	dpt = dpt_alloc(dev, rman_get_bustag(io), rman_get_bushandle(io) + 0x10);
	if (dpt == NULL) {
		error = ENXIO;
		goto bad;
	}

	/* Allocate a dmatag representing the capabilities of this attachment */
	/* XXX Should be a child of the PCI bus dma tag */
	if (bus_dma_tag_create(	/* parent    */	NULL,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_32BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	BUS_SPACE_MAXSIZE_32BIT,
				/* nsegments */	~0,
				/* maxsegsz  */	BUS_SPACE_MAXSIZE_32BIT,
				/* flags     */	0,
				&dpt->parent_dmat) != 0) {
		dpt_free(dpt);
		error = ENXIO;
		goto bad;
	}

	s = splcam();

	if (dpt_init(dpt) != 0) {
		dpt_free(dpt);
		error = ENXIO;
		goto bad;
	}

	/* Register with the XPT */
	dpt_attach(dpt);

	splx(s);

	if (bus_setup_intr(dev, irq, INTR_TYPE_CAM | INTR_ENTROPY, dpt_intr,
		 	   dpt, &ih)) {
		device_printf(dev, "Unable to register interrupt handler\n");
		error = ENXIO;
		goto bad;
	}

	return (error);

bad:
	if (io)
		bus_release_resource(dev, iotype, 0, io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);

	return (error);
}

static device_method_t dpt_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         dpt_pci_probe),
	DEVMETHOD(device_attach,        dpt_pci_attach),

	{ 0, 0 }
};

static driver_t dpt_pci_driver = {
	"dpt",
	dpt_pci_methods,
	sizeof(dpt_softc_t),
};

static devclass_t dpt_devclass;

DRIVER_MODULE(dpt, pci, dpt_pci_driver, dpt_devclass, 0, 0);
