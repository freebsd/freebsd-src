/*-
 * Copyright (c) 1998 - 2003 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/taskqueue.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#ifdef __alpha__
#include <machine/md_var.h>
#endif
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>

/* local vars */
static MALLOC_DEFINE(M_ATAPCI, "ATA PCI", "ATA driver PCI");

/* misc defines */
#define IOMASK			0xfffffffc

/* prototypes */
static int ata_pci_allocate(device_t, struct ata_channel *);
static int ata_pci_dmainit(struct ata_channel *);
static void ata_pci_locknoop(struct ata_channel *, int);

static int
ata_pci_probe(device_t dev)
{
    if (pci_get_class(dev) != PCIC_STORAGE)
	return ENXIO;

    switch (pci_get_vendor(dev)) {
    case ATA_ACARD_ID: 
	return ata_acard_ident(dev);
    case ATA_ACER_LABS_ID:
	return ata_ali_ident(dev);
    case ATA_AMD_ID:
	return ata_amd_ident(dev);
    case ATA_CYRIX_ID:
	return ata_cyrix_ident(dev);
    case ATA_CYPRESS_ID:
	return ata_cypress_ident(dev);
    case ATA_HIGHPOINT_ID: 
	return ata_highpoint_ident(dev);
    case ATA_INTEL_ID:
	return ata_intel_ident(dev);
    case ATA_NATIONAL_ID:
	return ata_national_ident(dev);
    case ATA_NVIDIA_ID:
	return ata_nvidia_ident(dev);
    case ATA_PROMISE_ID:
	return ata_promise_ident(dev);
    case ATA_SERVERWORKS_ID: 
	return ata_serverworks_ident(dev);
    case ATA_SILICON_IMAGE_ID:
	return ata_sii_ident(dev);
    case ATA_SIS_ID:
	return ata_sis_ident(dev);
    case ATA_VIA_ID: 
	return ata_via_ident(dev);

    case 0x16ca:
	if (pci_get_devid(dev) == 0x000116ca) {
	    ata_generic_ident(dev);
	    device_set_desc(dev, "Cenatek Rocket Drive controller");
	    return 0;
	}
	return ENXIO;

    case 0x1042:
	if (pci_get_devid(dev)==0x10001042 || pci_get_devid(dev)==0x10011042) {
	    ata_generic_ident(dev);
	    device_set_desc(dev, 
		"RZ 100? ATA controller !WARNING! buggy HW data loss possible");
	    return 0;
	}
	return ENXIO;

    /* unknown chipset, try generic DMA if it seems possible */
    default:
	if (pci_get_class(dev) == PCIC_STORAGE &&
	    (pci_get_subclass(dev) == PCIS_STORAGE_IDE))
	    return ata_generic_ident(dev);
    }
    return ENXIO;
}

static int
ata_pci_attach(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    u_int8_t class, subclass;
    u_int32_t type, cmd;
    int unit;

    /* set up vendor-specific stuff */
    type = pci_get_devid(dev);
    class = pci_get_class(dev);
    subclass = pci_get_subclass(dev);
    cmd = pci_read_config(dev, PCIR_COMMAND, 2);

    if (!(cmd & PCIM_CMD_PORTEN)) {
	device_printf(dev, "ATA channel disabled by BIOS\n");
	return 0;
    }

    /* do chipset specific setups only needed once */
    if (ATA_MASTERDEV(dev) || pci_read_config(dev, 0x18, 4) & IOMASK)
	ctlr->channels = 2;
    else
	ctlr->channels = 1;
    ctlr->allocate = ata_pci_allocate;
    ctlr->dmainit = ata_pci_dmainit;
    ctlr->locking = ata_pci_locknoop;

#ifdef __sparc64__
    if (!(cmd & PCIM_CMD_BUSMASTEREN)) {
	pci_write_config(dev, PCIR_COMMAND, cmd | PCIM_CMD_BUSMASTEREN, 2);
	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
    }
#endif
    /* if busmastering configured get the I/O resource */
    if ((cmd & PCIM_CMD_BUSMASTEREN) == PCIM_CMD_BUSMASTEREN) {
	int rid = ATA_BMADDR_RID;

	ctlr->r_io1 = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					 0, ~0, 1, RF_ACTIVE);
    }

    ctlr->chipinit(dev);

    /* attach all channels on this controller */
    for (unit = 0; unit < ctlr->channels; unit++)
	device_add_child(dev, "ata", ATA_MASTERDEV(dev) ?
			 unit : devclass_find_free_unit(ata_devclass, 2));

    return bus_generic_attach(dev);
}


static int
ata_pci_print_child(device_t dev, device_t child)
{
    struct ata_channel *ch = device_get_softc(child);
    int retval = 0;

    retval += bus_print_child_header(dev, child);
    retval += printf(": at 0x%lx", rman_get_start(ch->r_io[ATA_IDX_ADDR].res));

    if (ATA_MASTERDEV(dev))
	retval += printf(" irq %d", 14 + ch->unit);
    
    retval += bus_print_child_footer(dev, child);

    return retval;
}

static struct resource *
ata_pci_alloc_resource(device_t dev, device_t child, int type, int *rid,
		       u_long start, u_long end, u_long count, u_int flags)
{
    struct ata_pci_controller *controller = device_get_softc(dev);
    int unit = ((struct ata_channel *)device_get_softc(child))->unit;
    struct resource *res = NULL;
    int myrid;

    if (type == SYS_RES_IOPORT) {
	switch (*rid) {
	case ATA_IOADDR_RID:
	    if (ATA_MASTERDEV(dev)) {
		myrid = 0;
		start = (unit ? ATA_SECONDARY : ATA_PRIMARY);
		end = start + ATA_IOSIZE - 1;
		count = ATA_IOSIZE;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
					 SYS_RES_IOPORT, &myrid,
					 start, end, count, flags);
	    }
	    else {
		myrid = 0x10 + 8 * unit;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
					 SYS_RES_IOPORT, &myrid,
					 start, end, count, flags);
	    }
	    break;

	case ATA_ALTADDR_RID:
	    if (ATA_MASTERDEV(dev)) {
		myrid = 0;
		start = (unit ? ATA_SECONDARY : ATA_PRIMARY) + ATA_ALTOFFSET;
		end = start + ATA_ALTIOSIZE - 1;
		count = ATA_ALTIOSIZE;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
					 SYS_RES_IOPORT, &myrid,
					 start, end, count, flags);
	    }
	    else {
		myrid = 0x14 + 8 * unit;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
					 SYS_RES_IOPORT, &myrid,
					 start, end, count, flags);
		if (res) {
			start = rman_get_start(res) + 2;
			end = start + ATA_ALTIOSIZE - 1;
			count = ATA_ALTIOSIZE;
			BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
					     SYS_RES_IOPORT, myrid, res);
			res = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
						 SYS_RES_IOPORT, &myrid,
						 start, end, count, flags);
		}
	    }
	    break;
	}
	return res;
    }

    if (type == SYS_RES_IRQ && *rid == ATA_IRQ_RID) {
		if (ATA_MASTERDEV(dev)) {
#ifdef __alpha__
	    return alpha_platform_alloc_ide_intr(unit);
#else
	    int irq = (unit == 0 ? 14 : 15);
	    
	    return BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
				      SYS_RES_IRQ, rid, irq, irq, 1, flags);
#endif
	}
	else {
	    return controller->r_irq;
	}
    }
    return 0;
}

static int
ata_pci_release_resource(device_t dev, device_t child, int type, int rid,
			 struct resource *r)
{
    int unit = ((struct ata_channel *)device_get_softc(child))->unit;

    if (type == SYS_RES_IOPORT) {
	switch (rid) {
	case ATA_IOADDR_RID:
	    if (ATA_MASTERDEV(dev))
		return BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
					    SYS_RES_IOPORT, 0x0, r);
	    else
		return BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
					    SYS_RES_IOPORT, 0x10 + 8 * unit, r);
	    break;

	case ATA_ALTADDR_RID:
	    if (ATA_MASTERDEV(dev))
		return BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
					    SYS_RES_IOPORT, 0x0, r);
	    else
		return BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
					    SYS_RES_IOPORT, 0x14 + 8 * unit, r);
	    break;
	default:
	    return ENOENT;
	}
    }
    if (type == SYS_RES_IRQ) {
	if (rid != ATA_IRQ_RID)
	    return ENOENT;

	if (ATA_MASTERDEV(dev)) {
#ifdef __alpha__
	    return alpha_platform_release_ide_intr(unit, r);
#else
	    return BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
					SYS_RES_IRQ, rid, r);
#endif
	}
	else  
	    return 0;
    }
    return EINVAL;
}

static int
ata_pci_setup_intr(device_t dev, device_t child, struct resource *irq, 
		   int flags, driver_intr_t *function, void *argument,
		   void **cookiep)
{
    if (ATA_MASTERDEV(dev)) {
#ifdef __alpha__
	return alpha_platform_setup_ide_intr(child, irq, function, argument,
					     cookiep);
#else
	return BUS_SETUP_INTR(device_get_parent(dev), child, irq,
			      flags, function, argument, cookiep);
#endif
    }
    else {
	struct ata_pci_controller *controller = device_get_softc(dev);
	int unit = ((struct ata_channel *)device_get_softc(child))->unit;

	controller->interrupt[unit].function = function;
	controller->interrupt[unit].argument = argument;
	*cookiep = controller;
	return 0;
    }
}

static int
ata_pci_teardown_intr(device_t dev, device_t child, struct resource *irq,
		      void *cookie)
{
    if (ATA_MASTERDEV(dev)) {
#ifdef __alpha__
	return alpha_platform_teardown_ide_intr(child, irq, cookie);
#else
	return BUS_TEARDOWN_INTR(device_get_parent(dev), child, irq, cookie);
#endif
    }
    else {
	struct ata_pci_controller *controller = device_get_softc(dev);
	int unit = ((struct ata_channel *)device_get_softc(child))->unit;

	controller->interrupt[unit].function = NULL;
	controller->interrupt[unit].argument = NULL;
	return 0;
    }
}
    
static int
ata_pci_allocate(device_t dev, struct ata_channel *ch)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct resource *io = NULL, *altio = NULL;
    int i, rid;

    rid = ATA_IOADDR_RID;
    io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
			    0, ~0, ATA_IOSIZE, RF_ACTIVE);
    if (!io)
	return ENXIO;

    rid = ATA_ALTADDR_RID;
    altio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
			       0, ~0, ATA_ALTIOSIZE, RF_ACTIVE);
    if (!altio) {
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, io);
	return ENXIO;
    }

    for (i = ATA_DATA; i <= ATA_STATUS; i ++) {
	ch->r_io[i].res = io;
	ch->r_io[i].offset = i;
    }
    ch->r_io[ATA_ALTSTAT].res = altio;
    ch->r_io[ATA_ALTSTAT].offset = 0;
    ch->r_io[ATA_IDX_ADDR].res = io;

    if (ctlr->r_io1) {
	for (i = ATA_BMCMD_PORT; i <= ATA_BMDTP_PORT; i++) {
	    ch->r_io[i].res = ctlr->r_io1;
	    ch->r_io[i].offset = (i - ATA_BMCMD_PORT)+(ch->unit * ATA_BMIOSIZE);
	}

	/* if simplex controller, only allow DMA on primary channel */
	ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, ATA_IDX_INB(ch, ATA_BMSTAT_PORT) &
		     (ATA_BMSTAT_DMA_MASTER | ATA_BMSTAT_DMA_SLAVE));
	if (ch->unit > 0 &&
	    (ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_DMA_SIMPLEX))
	    device_printf(dev, "simplex device, DMA on primary only\n");
	else 
	    ctlr->dmainit(ch);
    }
    return 0;
}

static int
ata_pci_dmastart(struct ata_channel *ch, caddr_t data, int32_t count, int dir)
{
    int error;

    if ((error = ata_dmastart(ch, data, count, dir)))
	return error;
    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, (ATA_IDX_INB(ch, ATA_BMSTAT_PORT) | 
		 (ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR)));
    ATA_IDX_OUTL(ch, ATA_BMDTP_PORT, ch->dma->mdmatab);
    ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
		 (dir ? ATA_BMCMD_WRITE_READ : 0) | ATA_BMCMD_START_STOP);
    return 0;
}

static int
ata_pci_dmastop(struct ata_channel *ch)
{
    int error;

    error = ATA_IDX_INB(ch, ATA_BMSTAT_PORT);
    ATA_IDX_OUTB(ch, ATA_BMCMD_PORT, 
		 ATA_IDX_INB(ch, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR);
    ata_dmastop(ch);
    return error;
}

static int
ata_pci_dmainit(struct ata_channel *ch)
{
    int error;

    if ((error = ata_dmainit(ch)))
	return error;

    ch->dma->start = ata_pci_dmastart;
    ch->dma->stop = ata_pci_dmastop;
    return 0;
}

static void
ata_pci_locknoop(struct ata_channel *ch, int flags)
{
}

static device_method_t ata_pci_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,		ata_pci_probe),
    DEVMETHOD(device_attach,		ata_pci_attach),
    DEVMETHOD(device_detach,		ata_pci_attach),
    DEVMETHOD(device_shutdown,		bus_generic_shutdown),
    DEVMETHOD(device_suspend,		bus_generic_suspend),
    DEVMETHOD(device_resume,		bus_generic_resume),

    /* bus methods */
    DEVMETHOD(bus_print_child,		ata_pci_print_child),
    DEVMETHOD(bus_alloc_resource,	ata_pci_alloc_resource),
    DEVMETHOD(bus_release_resource,	ata_pci_release_resource),
    DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,		ata_pci_setup_intr),
    DEVMETHOD(bus_teardown_intr,	ata_pci_teardown_intr),
    { 0, 0 }
};

static driver_t ata_pci_driver = {
    "atapci",
    ata_pci_methods,
    sizeof(struct ata_pci_controller),
};

static devclass_t ata_pci_devclass;

DRIVER_MODULE(atapci, pci, ata_pci_driver, ata_pci_devclass, 0, 0);

static int
ata_pcisub_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int count, error, i;

    /* find channel number on this controller */
    device_get_children(device_get_parent(dev), &children, &count);
    for (i = 0; i < count; i++) {
	if (children[i] == dev)
	    ch->unit = i;
    }
    free(children, M_TEMP);

    if ((error = ctlr->allocate(dev, ch)))
	return error;

    ch->device[MASTER].setmode = ctlr->setmode;
    ch->device[SLAVE].setmode = ctlr->setmode;
    ch->locking = ctlr->locking;
    return ata_probe(dev);
}

static device_method_t ata_pcisub_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,	ata_pcisub_probe),
    DEVMETHOD(device_attach,	ata_attach),
    DEVMETHOD(device_detach,	ata_detach),
    DEVMETHOD(device_suspend,	ata_suspend),
    DEVMETHOD(device_resume,	ata_resume),
    { 0, 0 }
};

static driver_t ata_pcisub_driver = {
    "ata",
    ata_pcisub_methods,
    sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, atapci, ata_pcisub_driver, ata_devclass, 0, 0);
