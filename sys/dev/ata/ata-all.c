/*-
 * Copyright (c) 1998,1999,2000,2001 Søren Schmidt
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
 *
 * $FreeBSD$
 */

#include "card.h"
#include "pci.h"
#include "opt_global.h"
#include "opt_isa.h"
#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/disk.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>
#include <sys/sysctl.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif
#include <isa/isavar.h>
#include <isa/isareg.h>
#ifdef __alpha__
#include <machine/md_var.h>
#endif
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/atapi-all.h>

/* misc defines */
#define IOMASK			0xfffffffc
#define ATA_IOADDR_RID		0
#define ATA_ALTADDR_RID		1
#define ATA_BMADDR_RID		2
#define ATA_IRQ_RID		0
#define ATA_MASTERDEV(dev)	((pci_get_progif(dev) & 0x80) && \
				 (pci_get_progif(dev) & 0x05) != 0x05)

/* prototypes */
static int ata_probe(device_t);
static int ata_attach(device_t);
static int ata_detach(device_t);
static int ata_resume(device_t);
static void ata_boot_attach(void);
static void ata_intr(void *);
static int ata_getparam(struct ata_softc *, int, u_int8_t);
static int ata_service(struct ata_softc *);
static char *active2str(int);
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);

/* local vars */
static devclass_t ata_devclass;
static devclass_t ata_pci_devclass;
static struct intr_config_hook *ata_delayed_attach = NULL;
static char ata_conf[256];
static MALLOC_DEFINE(M_ATA, "ATA generic", "ATA driver generic layer");

#ifdef DEV_ISA
static struct isa_pnp_id ata_ids[] = {
    {0x0006d041,	"Generic ESDI/IDE/ATA controller"},	/* PNP0600 */
    {0x0106d041,	"Plus Hardcard II"},			/* PNP0601 */
    {0x0206d041,	"Plus Hardcard IIXL/EZ"},		/* PNP0602 */
    {0x0306d041,	"Generic ATA"},				/* PNP0603 */
    {0}
};

static int
ata_isa_probe(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);
    struct resource *io;
    int rid;
    u_long tmp;

    /* check isapnp ids */
    if (ISA_PNP_PROBE(device_get_parent(dev), dev, ata_ids) == ENXIO)
	return ENXIO;
    
    /* allocate the io port range to get the start address */
    rid = ATA_IOADDR_RID;
    io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
			    ATA_IOSIZE, RF_ACTIVE);
    if (!io)
	return ENOMEM;

    /* set the altport range */
    if (bus_get_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID, &tmp, &tmp)) {
	bus_set_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID,
			 rman_get_start(io) + ATA_ALTOFFSET, ATA_ALTIOSIZE);
    }

    bus_release_resource(dev, SYS_RES_IOPORT, rid, io);
    scp->channel = 0;
    scp->flags |= ATA_USE_16BIT;
    return ata_probe(dev);
}

static device_method_t ata_isa_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,	ata_isa_probe),
    DEVMETHOD(device_attach,	ata_attach),
    DEVMETHOD(device_resume,	ata_resume),
    { 0, 0 }
};

static driver_t ata_isa_driver = {
    "ata",
    ata_isa_methods,
    sizeof(struct ata_softc),
};

DRIVER_MODULE(ata, isa, ata_isa_driver, ata_devclass, 0, 0);
#endif

#if NCARD > 0
static int
ata_pccard_probe(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);
    struct resource *io;
    int rid, len, start, end;
    u_long tmp;

    /* allocate the io range to get start and length */
    rid = ATA_IOADDR_RID;
    len = bus_get_resource_count(dev, SYS_RES_IOPORT, rid);
    io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
			    ATA_IOSIZE, RF_ACTIVE);
    if (!io)
	return ENOMEM;

    /* reallocate the io address to only cover the io ports */
    start = rman_get_start(io);
    end = start + ATA_IOSIZE - 1;
    bus_release_resource(dev, SYS_RES_IOPORT, rid, io);
    io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
			    start, end, ATA_IOSIZE, RF_ACTIVE);
    bus_release_resource(dev, SYS_RES_IOPORT, rid, io);

    /* 
     * if we got more than the default ATA_IOSIZE ports, this is likely
     * a pccard system where the altio ports are located at offset 14
     * otherwise its the normal altio offset
     */
    if (bus_get_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID, &tmp, &tmp)) {
	if (len > ATA_IOSIZE) {
	    bus_set_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID,
			     start + ATA_PCCARD_ALTOFFSET, ATA_ALTIOSIZE);
	}
	else {
	    bus_set_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID, 
			     start + ATA_ALTOFFSET, ATA_ALTIOSIZE);
	}
    }
    else
	return ENOMEM;

    scp->channel = 0;
    scp->flags |= (ATA_USE_16BIT | ATA_NO_SLAVE);
    return ata_probe(dev);
}

static device_method_t ata_pccard_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,	ata_pccard_probe),
    DEVMETHOD(device_attach,	ata_attach),
    DEVMETHOD(device_detach,	ata_detach),
    { 0, 0 }
};

static driver_t ata_pccard_driver = {
    "ata",
    ata_pccard_methods,
    sizeof(struct ata_softc),
};

DRIVER_MODULE(ata, pccard, ata_pccard_driver, ata_devclass, 0, 0);
#endif

#if NPCI > 0
struct ata_pci_softc {
    struct resource *bmio;
    int bmaddr;
    struct resource *irq;
    int irqcnt;
};

int
ata_find_dev(device_t dev, u_int32_t type, u_int32_t revid)
{
    device_t *children, child;
    int nchildren, i;

    if (device_get_children(device_get_parent(dev), &children, &nchildren))
	return 0;

    for (i = 0; i < nchildren; i++) {
	child = children[i];

	/* check that it's on the same silicon and the device we want */
	if (pci_get_slot(dev) == pci_get_slot(child) &&
	    pci_get_vendor(child) == (type & 0xffff) &&
	    pci_get_device(child) == ((type & 0xffff0000) >> 16) &&
	    pci_get_revid(child) >= revid) {
	    free(children, M_TEMP);
	    return 1;
	}
    }
    free(children, M_TEMP);
    return 0;
}

static const char *
ata_pci_match(device_t dev)
{
    if (pci_get_class(dev) != PCIC_STORAGE)
	return NULL;

    switch (pci_get_devid(dev)) {
    /* supported chipsets */
    case 0x12308086:
	return "Intel PIIX ATA controller";

    case 0x70108086:
	return "Intel PIIX3 ATA controller";

    case 0x71118086:
    case 0x71998086:
	return "Intel PIIX4 ATA33 controller";

    case 0x24218086:
	return "Intel ICH0 ATA33 controller";

    case 0x24118086:
	return "Intel ICH ATA66 controller";

    case 0x244b8086:
	return "Intel ICH2 ATA100 controller";

    case 0x522910b9:
	if (pci_get_revid(dev) < 0x20)
	    return "AcerLabs Aladdin ATA controller";
	else
	    return "AcerLabs Aladdin ATA33 controller";

    case 0x05711106: 
	if (ata_find_dev(dev, 0x05861106, 0))
	    return "VIA 82C586 ATA33 controller";
	if (ata_find_dev(dev, 0x05961106, 0x12))
	    return "VIA 82C596 ATA66 controller";
	if (ata_find_dev(dev, 0x05961106, 0))
	    return "VIA 82C596 ATA33 controller";
	if (ata_find_dev(dev, 0x06861106, 0x40))
	    return "VIA 82C686 ATA100 controller";
	if (ata_find_dev(dev, 0x06861106, 0))
	    return "VIA 82C686 ATA66 controller";
	return "VIA Apollo ATA controller";

    case 0x55131039:
	return "SiS 5591 ATA33 controller";

    case 0x06491095:
	return "CMD 649 ATA100 controller";

    case 0x06481095:
	return "CMD 648 ATA66 controller";

    case 0x06461095:
	return "CMD 646 ATA controller";

    case 0xc6931080:
	if (pci_get_subclass(dev) == PCIS_STORAGE_IDE)
	    return "Cypress 82C693 ATA controller";
	break;

    case 0x01021078:
	return "Cyrix 5530 ATA33 controller";

    case 0x74091022:
	return "AMD 756 ATA66 controller";

    case 0x02111166:
	return "ServerWorks ROSB4 ATA33 controller";

    case 0x4d33105a:
	return "Promise ATA33 controller";

    case 0x4d38105a:
	return "Promise ATA66 controller";

    case 0x0d30105a:
    case 0x4d30105a:
	return "Promise ATA100 controller";

    case 0x00041103:
	switch (pci_get_revid(dev)) {
	case 0x00:
	case 0x01:
	    return "HighPoint HPT366 ATA66 controller";
	case 0x02:
	    return "HighPoint HPT368 ATA66 controller";
	case 0x03:
	case 0x04:
	    return "HighPoint HPT370 ATA100 controller";
	default:
	    return "Unknown revision HighPoint ATA controller";
	}

   /* unsupported but known chipsets, generic DMA only */
    case 0x10001042:
    case 0x10011042:
	return "RZ 100? ATA controller !WARNING! buggy chip data loss possible";

    case 0x06401095:
	return "CMD 640 ATA controller !WARNING! buggy chip data loss possible";

    /* unknown chipsets, try generic DMA if it seems possible */
    default:
	if (pci_get_class(dev) == PCIC_STORAGE &&
	    (pci_get_subclass(dev) == PCIS_STORAGE_IDE))
	    return "Generic PCI ATA controller";
    }
    return NULL;
}

static int
ata_pci_probe(device_t dev)
{
    const char *desc = ata_pci_match(dev);
    
    if (desc) {
	device_set_desc(dev, desc);
	return 0;
    } 
    else
	return ENXIO;
}

static int
ata_pci_add_child(device_t dev, int unit)
{
    device_t child;

    /* check if this is located at one of the std addresses */
    if (ATA_MASTERDEV(dev)) {
	if (!(child = device_add_child(dev, "ata", unit)))
	    return ENOMEM;
    }
    else {
	if (!(child = device_add_child(dev, "ata", 2)))
	    return ENOMEM;
    }
    return 0;
}

static int
ata_pci_attach(device_t dev)
{
    struct ata_pci_softc *sc = device_get_softc(dev);
    u_int8_t class, subclass;
    u_int32_t type, cmd;
    int rid;

    /* set up vendor-specific stuff */
    type = pci_get_devid(dev);
    class = pci_get_class(dev);
    subclass = pci_get_subclass(dev);
    cmd = pci_read_config(dev, PCIR_COMMAND, 4);

    /* is busmastering supported ? */
    if ((cmd & (PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN)) == 
	(PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN)) {

	/* is there a valid port range to connect to ? */
	rid = 0x20;
	sc->bmio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				      0, ~0, 1, RF_ACTIVE);
	if (!sc->bmio)
	    device_printf(dev, "Busmastering DMA not configured\n");
    }
    else
	device_printf(dev, "Busmastering DMA not supported\n");

    /* do extra chipset specific setups */
    switch (type) {
    case 0x522910b9: /* Aladdin need to activate the ATAPI FIFO */
	pci_write_config(dev, 0x53, 
			 (pci_read_config(dev, 0x53, 1) & ~0x01) | 0x02, 1);
	break;

    case 0x4d38105a: /* Promise 66 & 100 need their clock changed */
    case 0x4d30105a:
    case 0x0d30105a:
	ATA_OUTB(sc->bmio, 0x11, ATA_INB(sc->bmio, 0x11) | 0x0a);
	/* FALLTHROUGH */

    case 0x4d33105a: /* Promise (all) need burst mode to be turned on */
	ATA_OUTB(sc->bmio, 0x1f, ATA_INB(sc->bmio, 0x1f) | 0x01);
	break;

    case 0x00041103: /* HighPoint */
	switch (pci_get_revid(dev)) {
	case 0x00:
	case 0x01:
	    /* turn off interrupt prediction */
	    pci_write_config(dev, 0x51, 
	    		     (pci_read_config(dev, 0x51, 1) & ~0x80), 1);
	    break;

	case 0x02:
	case 0x03:
	case 0x04:
	    /* turn off interrupt prediction */
	    pci_write_config(dev, 0x51, 
	    		     (pci_read_config(dev, 0x51, 1) & ~0x02), 1);
	    pci_write_config(dev, 0x55, 
	    		     (pci_read_config(dev, 0x55, 1) & ~0x02), 1);
	    /* turn on interrupts */
	    pci_write_config(dev, 0x5a, 
	    		     (pci_read_config(dev, 0x5a, 1) & ~0x10), 1);

	}
	break;

    case 0x05711106:
    case 0x74091022: /* VIA 82C586, 82C596, 82C686 & AMD 756 default setup */

	/* set prefetch, postwrite */
	pci_write_config(dev, 0x41, pci_read_config(dev, 0x41, 1) | 0xf0, 1);

	/* set fifo configuration half'n'half */
	pci_write_config(dev, 0x43, 
			 (pci_read_config(dev, 0x43, 1) & 0x90) | 0x2a, 1);

	/* set status register read retry */
	pci_write_config(dev, 0x44, pci_read_config(dev, 0x44, 1) | 0x08, 1);

	/* set DMA read & end-of-sector fifo flush */
	pci_write_config(dev, 0x46, 
			 (pci_read_config(dev, 0x46, 1) & 0x0c) | 0xf0, 1);

	/* set sector size */
	pci_write_config(dev, 0x60, DEV_BSIZE, 2);
	pci_write_config(dev, 0x68, DEV_BSIZE, 2);
	
	/* prepare for ATA-66 on the 82C686 and rev 0x12 and newer 82C596's */
	if (ata_find_dev(dev, 0x06861106, 0) || 
	    ata_find_dev(dev, 0x05961106, 0x12)) {
	    pci_write_config(dev, 0x50, 
			     pci_read_config(dev, 0x50, 4) | 0x070f070f, 4);   
	}
	break;

    case 0x10001042:   /* RZ 100? known bad, no DMA */
    case 0x10011042:
    case 0x06401095:   /* CMD 640 known bad, no DMA */
	sc->bmio = NULL;
	device_printf(dev, "Busmastering DMA disabled\n");
    }

    if (sc->bmio) {
	sc->bmaddr = rman_get_start(sc->bmio);
	BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
			     SYS_RES_IOPORT, rid, sc->bmio);
	sc->bmio = NULL;
    }

    /*
     * the Cypress chip is a mess, it contains two ATA functions, but 
     * both channels are visible on the first one.
     * simply ignore the second function for now, as the right
     * solution (ignoring the second channel on the first function)
     * doesn't work with the crappy ATA interrupt setup on the alpha.
     */
    if (pci_get_devid(dev) == 0xc6931080 && pci_get_function(dev) > 1)
	return 0;

    ata_pci_add_child(dev, 0);

    if (ATA_MASTERDEV(dev) || pci_read_config(dev, 0x18, 4) & IOMASK)
	ata_pci_add_child(dev, 1);

    return bus_generic_attach(dev);
}

static int
ata_pci_print_child(device_t dev, device_t child)
{
    struct ata_softc *scp = device_get_softc(child);
    int retval = 0;

    retval += bus_print_child_header(dev, child);
    retval += printf(": at 0x%lx", rman_get_start(scp->r_io));

    if (ATA_MASTERDEV(dev))
	retval += printf(" irq %d", 14 + scp->channel);
    
    retval += bus_print_child_footer(dev, child);

    return retval;
}

static struct resource *
ata_pci_alloc_resource(device_t dev, device_t child, int type, int *rid,
		       u_long start, u_long end, u_long count, u_int flags)
{
    struct ata_pci_softc *sc = device_get_softc(dev);
    struct resource *res = NULL;
    int channel = ((struct ata_softc *)device_get_softc(child))->channel;
    int myrid;

    if (type == SYS_RES_IOPORT) {
	switch (*rid) {
	case ATA_IOADDR_RID:
	    if (ATA_MASTERDEV(dev)) {
		myrid = 0;
		start = (channel == 0 ? IO_WD1 : IO_WD2);
		end = start + ATA_IOSIZE - 1;
		count = ATA_IOSIZE;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
					 SYS_RES_IOPORT, &myrid,
					 start, end, count, flags);
	    }
	    else {
		myrid = 0x10 + 8 * channel;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
					 SYS_RES_IOPORT, &myrid,
					 start, end, count, flags);
	    }
	    break;

	case ATA_ALTADDR_RID:
	    if (ATA_MASTERDEV(dev)) {
		myrid = 0;
		start = (channel == 0 ? IO_WD1 : IO_WD2) + ATA_ALTOFFSET;
		end = start + ATA_ALTIOSIZE - 1;
		count = ATA_ALTIOSIZE;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
					 SYS_RES_IOPORT, &myrid,
					 start, end, count, flags);
	    }
	    else {
		myrid = 0x14 + 8 * channel;
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

	case ATA_BMADDR_RID:
	    if (sc->bmaddr) {
		myrid = 0x20;
		start = (channel == 0 ? sc->bmaddr : sc->bmaddr + ATA_BMIOSIZE);
		end = start + ATA_BMIOSIZE - 1;
		count = ATA_BMIOSIZE;
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
					  SYS_RES_IOPORT, &myrid,
					  start, end, count, flags);
	    }
	}
	return res;
    }

    if (type == SYS_RES_IRQ && *rid == ATA_IRQ_RID) {
	if (ATA_MASTERDEV(dev)) {
#ifdef __alpha__
	    return alpha_platform_alloc_ide_intr(channel);
#else
	    int irq = (channel == 0 ? 14 : 15);

	    return BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
				      SYS_RES_IRQ, rid,
				      irq, irq, 1, flags & ~RF_SHAREABLE);
#endif
	}
	else {
	    /* primary and secondary channels share the same interrupt */
	    sc->irqcnt++;
	    if (!sc->irq)
		sc->irq = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
					     SYS_RES_IRQ, rid, 0, ~0, 1, flags);
	    return sc->irq;
	}
    }
    return 0;
}

static int
ata_pci_release_resource(device_t dev, device_t child, int type, int rid,
			 struct resource *r)
{
    struct ata_pci_softc *sc = device_get_softc(dev);
    int channel = ((struct ata_softc *)device_get_softc(child))->channel;
    int myrid = 0;

    if (type == SYS_RES_IOPORT) {
	switch (rid) {
	case ATA_IOADDR_RID:
	    if (ATA_MASTERDEV(dev))
		myrid = 0;
	    else
		myrid = 0x10 + 8 * channel;
	    break;

	case ATA_ALTADDR_RID:
	    if (ATA_MASTERDEV(dev))
		myrid = 0;
	    else
		myrid = 0x14 + 8 * channel;
	    break;

	case ATA_BMADDR_RID:
	    myrid = 0x20;
	    break;

	default:
	    return ENOENT;
	}

	if (ATA_MASTERDEV(dev))
	    /* make the parent just pass through the allocation. */
	    return BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
					SYS_RES_IOPORT, myrid, r);
	else
	    /* we are using the parent resource directly. */
	    return BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
					SYS_RES_IOPORT, myrid, r);
    }
    if (type == SYS_RES_IRQ) {
	if (rid != ATA_IRQ_RID)
	    return ENOENT;

	if (ATA_MASTERDEV(dev)) {
#ifdef __alpha__
	    return alpha_platform_release_ide_intr(channel, r);
#else
	    return BUS_RELEASE_RESOURCE(device_get_parent(dev),
					child, SYS_RES_IRQ, rid, r);
#endif
	}
	else {
	    if (--sc->irqcnt)
		return 0;

	    return BUS_RELEASE_RESOURCE(device_get_parent(dev),
					dev, SYS_RES_IRQ, rid, r);
	}
    }
    return EINVAL;
}

static int
ata_pci_setup_intr(device_t dev, device_t child, struct resource *irq, 
		   int flags, driver_intr_t *intr, void *arg,
		   void **cookiep)
{
    if (ATA_MASTERDEV(dev)) {
#ifdef __alpha__
	return alpha_platform_setup_ide_intr(child, irq, intr, arg, cookiep);
#else
	return BUS_SETUP_INTR(device_get_parent(dev), child, irq,
			      flags, intr, arg, cookiep);
#endif
    }
    else
	return BUS_SETUP_INTR(device_get_parent(dev), dev, irq,
			      flags, intr, arg, cookiep);
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
    else
	return BUS_TEARDOWN_INTR(device_get_parent(dev), dev, irq, cookie);
}

static device_method_t ata_pci_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,		ata_pci_probe),
    DEVMETHOD(device_attach,		ata_pci_attach),
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
    sizeof(struct ata_pci_softc),
};

DRIVER_MODULE(atapci, pci, ata_pci_driver, ata_pci_devclass, 0, 0);

static int
ata_pcisub_probe(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);
    device_t *children;
    int count, i;

    /* find channel number on this controller */
    device_get_children(device_get_parent(dev), &children, &count);
    for (i = 0; i < count; i++) {
	if (children[i] == dev)
	    scp->channel = i;
    }
    free(children, M_TEMP);
    scp->chiptype = pci_get_devid(device_get_parent(dev));
    return ata_probe(dev);
}

static device_method_t ata_pcisub_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,	ata_pcisub_probe),
    DEVMETHOD(device_attach,	ata_attach),
    DEVMETHOD(device_detach,	ata_detach),
    DEVMETHOD(device_resume,	ata_resume),
    { 0, 0 }
};

static driver_t ata_pcisub_driver = {
    "ata",
    ata_pcisub_methods,
    sizeof(struct ata_softc),
};

DRIVER_MODULE(ata, atapci, ata_pcisub_driver, ata_devclass, 0, 0);
#endif

static int
ata_probe(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);
    int rid;
    int mask = 0;
    u_int8_t status0, status1;

    if (!scp || scp->flags & ATA_ATTACHED)
	return ENXIO;

    /* initialize the softc basics */
    scp->active = ATA_IDLE;
    scp->dev = dev;
    scp->devices = 0;

    rid = ATA_IOADDR_RID;
    scp->r_io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 
				   ATA_IOSIZE, RF_ACTIVE);
    if (!scp->r_io)
	goto failure;

    rid = ATA_ALTADDR_RID;
    scp->r_altio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				      ATA_ALTIOSIZE, RF_ACTIVE);
    if (!scp->r_altio)
	goto failure;

    rid = ATA_BMADDR_RID;
    scp->r_bmio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				     ATA_BMIOSIZE, RF_ACTIVE);

    if (bootverbose)
	ata_printf(scp, -1, "iobase=0x%04x altiobase=0x%04x bmaddr=0x%04x\n", 
		   (int)rman_get_start(scp->r_io),
		   (int)rman_get_start(scp->r_altio),
		   (scp->r_bmio) ? (int)rman_get_start(scp->r_bmio) : 0);

    /* do we have any signs of ATA/ATAPI HW being present ? */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(1);
    status0 = ATA_INB(scp->r_io, ATA_STATUS);
    if ((status0 & 0xf8) != 0xf8 && status0 != 0xa5)
	mask |= 0x01;
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
    DELAY(1);	
    status1 = ATA_INB(scp->r_io, ATA_STATUS);
    if ((status1 & 0xf8) != 0xf8 && status1 != 0xa5)
	mask |= 0x02;

    if (bootverbose)
	ata_printf(scp, -1, "mask=%02x status0=%02x status1=%02x\n", 
		   mask, status0, status1);
    if (!mask)
	goto failure;

    ata_reset(scp, &mask);

    if (!mask)
	goto failure;

    TAILQ_INIT(&scp->ata_queue);
    TAILQ_INIT(&scp->atapi_queue);
    return 0;
    
failure:
    if (scp->r_io)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, scp->r_io);
    if (scp->r_altio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID,scp->r_altio);
    if (scp->r_bmio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_BMADDR_RID, scp->r_bmio);
    if (bootverbose)
	ata_printf(scp, -1, "probe allocation failed\n");
    return ENXIO;
}

static int
ata_attach(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);
    int error, rid;

    if (!scp || scp->flags & ATA_ATTACHED)
	return ENXIO;

    rid = ATA_IRQ_RID;
    scp->r_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				    RF_SHAREABLE | RF_ACTIVE);
    if (!scp->r_irq) {
	ata_printf(scp, -1, "unable to allocate interrupt\n");
	return ENXIO;
    }
    if ((error = bus_setup_intr(dev, scp->r_irq, INTR_TYPE_BIO, ata_intr,
				scp, &scp->ih)))
	return error;

    /*
     * do not attach devices if we are in early boot, this is done later 
     * when interrupts are enabled by a hook into the boot process.
     * otherwise attach what the probe has found in scp->devices.
     */
    if (!ata_delayed_attach) {
	if (scp->devices & ATA_ATA_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_SLAVE;
	if (scp->devices & ATA_ATAPI_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_SLAVE;
	if (scp->devices & ATA_ATA_MASTER)
	    if (ata_getparam(scp, ATA_MASTER, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_MASTER;
	if (scp->devices & ATA_ATAPI_MASTER)
	    if (ata_getparam(scp, ATA_MASTER,ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_MASTER;
#ifdef DEV_ATADISK
	if (scp->devices & ATA_ATA_MASTER)
	    ad_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATA_SLAVE)
	    ad_attach(scp, ATA_SLAVE);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
	if (scp->devices & ATA_ATAPI_MASTER)
	    atapi_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATAPI_SLAVE)
	    atapi_attach(scp, ATA_SLAVE);
#endif
    }
    scp->flags |= ATA_ATTACHED;
    return 0;
}

static int
ata_detach(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);
 
    if (!scp || !(scp->flags & ATA_ATTACHED))
	return ENXIO;

    /* disable interrupts on devices */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_4BIT);
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_4BIT);

#ifdef DEV_ATADISK
    if (scp->devices & ATA_ATA_MASTER)
	ad_detach(scp->dev_softc[0]);
    if (scp->devices & ATA_ATA_SLAVE)
	ad_detach(scp->dev_softc[1]);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    if (scp->devices & ATA_ATAPI_MASTER)
	atapi_detach(scp->dev_softc[0]);
    if (scp->devices & ATA_ATAPI_SLAVE)
	atapi_detach(scp->dev_softc[1]);
#endif

    if (scp->dev_param[ATA_DEV(ATA_MASTER)]) {
	free(scp->dev_param[ATA_DEV(ATA_MASTER)], M_ATA);
	scp->dev_param[ATA_DEV(ATA_MASTER)] = NULL;
    }
    if (scp->dev_param[ATA_DEV(ATA_SLAVE)]) {
	free(scp->dev_param[ATA_DEV(ATA_SLAVE)], M_ATA);
	scp->dev_param[ATA_DEV(ATA_SLAVE)] = NULL;
    }
    scp->dev_softc[ATA_DEV(ATA_MASTER)] = NULL;
    scp->dev_softc[ATA_DEV(ATA_SLAVE)] = NULL;
    scp->mode[ATA_DEV(ATA_MASTER)] = ATA_PIO;
    scp->mode[ATA_DEV(ATA_SLAVE)] = ATA_PIO;

    bus_teardown_intr(dev, scp->r_irq, scp->ih);
    bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, scp->r_irq);
    if (scp->r_bmio)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_BMADDR_RID, scp->r_bmio);
    bus_release_resource(dev, SYS_RES_IOPORT, ATA_ALTADDR_RID, scp->r_altio);
    bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, scp->r_io);
    scp->flags &= ~ATA_ATTACHED;
    return 0;
}

static int
ata_resume(device_t dev)
{
    struct ata_softc *scp = device_get_softc(dev);

    ata_reinit(scp);
    return 0;
}

static int
ata_getparam(struct ata_softc *scp, int device, u_int8_t command)
{
    struct ata_params *ata_parm;
    int8_t buffer[DEV_BSIZE];
    int retry = 0;

    /* select drive */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device);
    DELAY(1);

    /* enable interrupt */
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);
    DELAY(1);

    /* apparently some devices needs this repeated */
    do {
	if (ata_command(scp, device, command, 0, 0, 0, 0, 0, ATA_WAIT_INTR)) {
	    ata_printf(scp, device, "identify failed\n");
	    return -1;
	}
	if (retry++ > 4) {
	    ata_printf(scp, device, "identify retries exceeded\n");
	    return -1;
	}
    } while (ata_wait(scp, device, 
		      ((command == ATA_C_ATAPI_IDENTIFY) ?
			ATA_S_DRQ : (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))));

    ATA_INSW(scp->r_io, ATA_DATA, (int16_t *)buffer,
	     sizeof(buffer)/sizeof(int16_t));
    ata_parm = malloc(sizeof(struct ata_params), M_ATA, M_NOWAIT);
    if (!ata_parm) {
	ata_printf(scp, device, "malloc for identify data failed\n");
        return -1;
    }
    bcopy(buffer, ata_parm, sizeof(struct ata_params));   
    if (command == ATA_C_ATA_IDENTIFY ||
	!((ata_parm->model[0] == 'N' && ata_parm->model[1] == 'E') ||
          (ata_parm->model[0] == 'F' && ata_parm->model[1] == 'X')))
        bswap(ata_parm->model, sizeof(ata_parm->model));
    btrim(ata_parm->model, sizeof(ata_parm->model));
    bpack(ata_parm->model, ata_parm->model, sizeof(ata_parm->model));
    bswap(ata_parm->revision, sizeof(ata_parm->revision));
    btrim(ata_parm->revision, sizeof(ata_parm->revision));
    bpack(ata_parm->revision, ata_parm->revision, sizeof(ata_parm->revision));
    scp->dev_param[ATA_DEV(device)] = ata_parm;
    return 0;
}

static void 
ata_boot_attach(void)
{
    struct ata_softc *scp;
    int ctlr;

    /*
     * run through all ata devices and look for real ATA & ATAPI devices
     * using the hints we found in the early probe, this avoids some of
     * the delays probing of non-exsistent devices can cause.
     */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(scp = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (scp->devices & ATA_ATA_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_SLAVE;
	if (scp->devices & ATA_ATAPI_SLAVE)
	    if (ata_getparam(scp, ATA_SLAVE, ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_SLAVE;
	if (scp->devices & ATA_ATA_MASTER)
	    if (ata_getparam(scp, ATA_MASTER, ATA_C_ATA_IDENTIFY))
		scp->devices &= ~ATA_ATA_MASTER;
	if (scp->devices & ATA_ATAPI_MASTER)
	    if (ata_getparam(scp, ATA_MASTER,ATA_C_ATAPI_IDENTIFY))
		scp->devices &= ~ATA_ATAPI_MASTER;
    }

#ifdef DEV_ATADISK
    /* now we know whats there, do the real attach, first the ATA disks */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(scp = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (scp->devices & ATA_ATA_MASTER)
	    ad_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATA_SLAVE)
	    ad_attach(scp, ATA_SLAVE);
    }
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    /* then the atapi devices */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(scp = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (scp->devices & ATA_ATAPI_MASTER)
	    atapi_attach(scp, ATA_MASTER);
	if (scp->devices & ATA_ATAPI_SLAVE)
	    atapi_attach(scp, ATA_SLAVE);
    }
#endif
    if (ata_delayed_attach) {
	config_intrhook_disestablish(ata_delayed_attach);
	free(ata_delayed_attach, M_ATA);
	ata_delayed_attach = NULL;
    }
}

static void
ata_intr(void *data)
{
    struct ata_softc *scp = (struct ata_softc *)data;
    u_int8_t dmastat = 0;

    /* 
     * since we might share the IRQ with another device, and in some
     * cases with our twin channel, we only want to process interrupts
     * that we know this channel generated.
     */
    switch (scp->chiptype) {
#if NPCI > 0
    case 0x00041103:    /* HighPoint HPT366/368/370 */
	if (((dmastat = ata_dmastatus(scp)) &
	    (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) != ATA_BMSTAT_INTERRUPT)
	    return;
	ATA_OUTB(scp->r_bmio, ATA_BMSTAT_PORT, dmastat | ATA_BMSTAT_INTERRUPT);
	break;

    case 0x06481095:	/* CMD 648 */
    case 0x06491095:	/* CMD 649 */
        if (!(pci_read_config(device_get_parent(scp->dev), 0x71, 1) &
	      (scp->channel ? 0x08 : 0x04)))
	    return;
	goto out;

    case 0x4d33105a:	/* Promise Ultra/Fasttrak 33 */
    case 0x4d38105a:	/* Promise Ultra/Fasttrak 66 */
    case 0x4d30105a:	/* Promise Ultra/Fasttrak 100 */
    case 0x0d30105a:	/* Promise OEM ATA100 */
	if (!(ATA_INL(scp->r_bmio, (scp->channel ? 0x14 : 0x1c)) &
	      (scp->channel ? 0x00004000 : 0x00000400)))
	    return;
    	/* FALLTHROUGH */
out:
#endif
    default:
	if (scp->flags & ATA_DMA_ACTIVE) {
	    if (!((dmastat = ata_dmastatus(scp)) & ATA_BMSTAT_INTERRUPT))
		return;
	    ATA_OUTB(scp->r_bmio, ATA_BMSTAT_PORT,
		     dmastat | ATA_BMSTAT_INTERRUPT);
	}
    }
    DELAY(1);

    /* if drive is busy it didn't interrupt */
    if (ATA_INB(scp->r_altio, ATA_ALTSTAT) & ATA_S_BUSY)
	return;

    /* clear interrupt and get status */
    scp->status = ATA_INB(scp->r_io, ATA_STATUS);

    if (scp->status & ATA_S_ERROR)
	scp->error = ATA_INB(scp->r_io, ATA_ERROR);

    /* find & call the responsible driver to process this interrupt */
    switch (scp->active) {
#ifdef DEV_ATADISK
    case ATA_ACTIVE_ATA:
	if (!scp->running || ad_interrupt(scp->running) == ATA_OP_CONTINUES)
	    return;
	break;
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    case ATA_ACTIVE_ATAPI:
	if (!scp->running || atapi_interrupt(scp->running) == ATA_OP_CONTINUES)
	    return;
	break;
#endif
    case ATA_WAIT_INTR:
	wakeup((caddr_t)scp);
	break;

    case ATA_WAIT_READY:
	break;

    case ATA_REINITING:
	return;

    case ATA_IDLE:
	if (scp->flags & ATA_QUEUED) {
	    scp->active = ATA_ACTIVE; /* XXX */
	    if (ata_service(scp) == ATA_OP_CONTINUES)
		return;
	}
	/* FALLTHROUGH */

    default:
#ifdef ATA_DEBUG
    {
	static int intr_count = 0;

	if (intr_count++ < 10)
	    ata_printf(scp, -1, "unwanted interrupt %d status = %02x\n", 
		       intr_count, scp->status);
    }
#endif
    }
    scp->active = ATA_IDLE;
    scp->running = NULL;
    ata_start(scp);
    return;
}

void
ata_start(struct ata_softc *scp)
{
#ifdef DEV_ATADISK
    struct ad_request *ad_request; 
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    struct atapi_request *atapi_request;
#endif

    if (!atomic_cmpset_int(&scp->active, ATA_IDLE, ATA_ACTIVE))
	return;

#ifdef DEV_ATADISK
    /* find & call the responsible driver if anything on the ATA queue */
    if (TAILQ_EMPTY(&scp->ata_queue)) {
	if (scp->devices & (ATA_ATA_MASTER) && scp->dev_softc[0])
	    ad_start((struct ad_softc *)scp->dev_softc[0]);
	if (scp->devices & (ATA_ATA_SLAVE) && scp->dev_softc[1])
	    ad_start((struct ad_softc *)scp->dev_softc[1]);
    }
    if ((ad_request = TAILQ_FIRST(&scp->ata_queue))) {
	TAILQ_REMOVE(&scp->ata_queue, ad_request, chain);
	scp->active = ATA_ACTIVE_ATA;
	scp->running = ad_request;
	if (ad_transfer(ad_request) == ATA_OP_CONTINUES)
	    return;
    }

#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
    /* find & call the responsible driver if anything on the ATAPI queue */
    if (TAILQ_EMPTY(&scp->atapi_queue)) {
	if (scp->devices & (ATA_ATAPI_MASTER) && scp->dev_softc[0])
	    atapi_start((struct atapi_softc *)scp->dev_softc[0]);
	if (scp->devices & (ATA_ATAPI_SLAVE) && scp->dev_softc[1])
	    atapi_start((struct atapi_softc *)scp->dev_softc[1]);
    }
    if ((atapi_request = TAILQ_FIRST(&scp->atapi_queue))) {
	TAILQ_REMOVE(&scp->atapi_queue, atapi_request, chain);
	scp->active = ATA_ACTIVE_ATAPI;
	scp->running = atapi_request;
	atapi_transfer(atapi_request);
	return;
    }
#endif
    scp->active = ATA_IDLE;
}

void
ata_reset(struct ata_softc *scp, int *mask)
{
    int timeout;  
    u_int8_t a, b, ostat0, ostat1;
    u_int8_t status0 = ATA_S_BUSY, status1 = ATA_S_BUSY;

    /* get the current status of the devices */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
    DELAY(10);
    ostat1 = ATA_INB(scp->r_io, ATA_STATUS);
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(10);
    ostat0 = ATA_INB(scp->r_io, ATA_STATUS);

    /* in some setups we dont want to test for a slave */
    if (scp->flags & ATA_NO_SLAVE)
	*mask &= ~0x02;

    if (bootverbose)
	ata_printf(scp, -1, "mask=%02x ostat0=%02x ostat2=%02x\n",
		   *mask, ostat0, ostat1);

    /* reset channel */
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_RESET);
    DELAY(10000); 
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS);
    DELAY(100000);
    ATA_INB(scp->r_io, ATA_ERROR);
    scp->devices = 0;

    /* wait for BUSY to go inactive */
    for (timeout = 0; timeout < 310000; timeout++) {
	if (status0 & ATA_S_BUSY) {
            ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
            DELAY(10);
            status0 = ATA_INB(scp->r_io, ATA_STATUS);
            if (!(status0 & ATA_S_BUSY)) {
                /* check for ATAPI signature while its still there */
		a = ATA_INB(scp->r_io, ATA_CYL_LSB);
		b = ATA_INB(scp->r_io, ATA_CYL_MSB);
		if (bootverbose)
		    ata_printf(scp, ATA_MASTER,
			       "ATAPI probe a=%02x b=%02x\n", a, b);
		if (a == ATAPI_MAGIC_LSB && b == ATAPI_MAGIC_MSB)
                    scp->devices |= ATA_ATAPI_MASTER;
            }
        }
        if (status1 & ATA_S_BUSY) {
            ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
            DELAY(10);
            status1 = ATA_INB(scp->r_io, ATA_STATUS);
            if (!(status1 & ATA_S_BUSY)) {
                /* check for ATAPI signature while its still there */
		a = ATA_INB(scp->r_io, ATA_CYL_LSB);
		b = ATA_INB(scp->r_io, ATA_CYL_MSB);
		if (bootverbose)
		    ata_printf(scp, ATA_SLAVE,
			       "ATAPI probe a=%02x b=%02x\n", a, b);
		if (a == ATAPI_MAGIC_LSB && b == ATAPI_MAGIC_MSB)
                    scp->devices |= ATA_ATAPI_SLAVE;
            }
        }
	if (*mask == 0x01)      /* wait for master only */
	    if (!(status0 & ATA_S_BUSY))
		break;
	if (*mask == 0x02)      /* wait for slave only */
	    if (!(status1 & ATA_S_BUSY))
		break;
	if (*mask == 0x03)      /* wait for both master & slave */
	    if (!(status0 & ATA_S_BUSY) && !(status1 & ATA_S_BUSY))
		break;
	DELAY(100);
    }	
    DELAY(10);
    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);

    if (status0 & ATA_S_BUSY)
	*mask &= ~0x01;
    if (status1 & ATA_S_BUSY)
	*mask &= ~0x02;
    if (bootverbose)
	ata_printf(scp, -1, "mask=%02x status0=%02x status1=%02x\n", 
		   *mask, status0, status1);
    if (!*mask)
	return;

    if (*mask & 0x01 && ostat0 != 0x00 && !(scp->devices & ATA_ATAPI_MASTER)) {
        ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
        DELAY(10);
	ATA_OUTB(scp->r_io, ATA_ERROR, 0x58);
	ATA_OUTB(scp->r_io, ATA_CYL_LSB, 0xa5);
	a = ATA_INB(scp->r_io, ATA_ERROR);
	b = ATA_INB(scp->r_io, ATA_CYL_LSB);
	if (bootverbose)
	    ata_printf(scp, ATA_MASTER, "ATA probe a=%02x b=%02x\n", a, b);
        if (a != 0x58 && b == 0xa5)
            scp->devices |= ATA_ATA_MASTER;
    }
    if (*mask & 0x02 && ostat1 != 0x00 && !(scp->devices & ATA_ATAPI_SLAVE)) {
        ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
        DELAY(10);
	ATA_OUTB(scp->r_io, ATA_ERROR, 0x58);
	ATA_OUTB(scp->r_io, ATA_CYL_LSB, 0xa5);
	a = ATA_INB(scp->r_io, ATA_ERROR);
	b = ATA_INB(scp->r_io, ATA_CYL_LSB);
	if (bootverbose)
	    ata_printf(scp, ATA_SLAVE, "ATA probe a=%02x b=%02x\n", a, b);
        if (a != 0x58 && b == 0xa5)
            scp->devices |= ATA_ATA_SLAVE;
    }
    if (bootverbose)
	ata_printf(scp, -1, "devices=%02x\n", scp->devices);
}

int
ata_reinit(struct ata_softc *scp)
{
    int mask = 0, omask;

    scp->active = ATA_REINITING;
    scp->running = NULL;
    if (scp->devices & (ATA_ATA_MASTER | ATA_ATAPI_MASTER))
	mask |= 0x01;
    if (scp->devices & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE))
	mask |= 0x02;
    if (mask) {
	omask = mask;
        ata_printf(scp, -1, "resetting devices .. ");
	ata_reset(scp, &mask);
	if (omask != mask)
	    printf(" device dissapeared! %d ", omask & ~mask);

#ifdef DEV_ATADISK
	if (scp->devices & (ATA_ATA_MASTER) && scp->dev_softc[0])
	    ad_reinit((struct ad_softc *)scp->dev_softc[0]);
	if (scp->devices & (ATA_ATA_SLAVE) && scp->dev_softc[1])
	    ad_reinit((struct ad_softc *)scp->dev_softc[1]);
#endif
#if defined(DEV_ATAPICD) || defined(DEV_ATAPIFD) || defined(DEV_ATAPIST)
	if (scp->devices & (ATA_ATAPI_MASTER) && scp->dev_softc[0])
	    atapi_reinit((struct atapi_softc *)scp->dev_softc[0]);
	if (scp->devices & (ATA_ATAPI_SLAVE) && scp->dev_softc[1])
	    atapi_reinit((struct atapi_softc *)scp->dev_softc[1]);
#endif
	printf("done\n");
    }
    scp->active = ATA_IDLE;
    ata_start(scp);
    return 0;
}

static int
ata_service(struct ata_softc *scp)
{
    /* do we have a SERVICE request from the drive ? */
    if ((scp->status & (ATA_S_SERVICE|ATA_S_ERROR|ATA_S_DRQ)) == ATA_S_SERVICE){
	ATA_OUTB(scp->r_bmio, ATA_BMSTAT_PORT,
		 ata_dmastatus(scp) | ATA_BMSTAT_INTERRUPT);
#ifdef DEV_ATADISK
	if ((ATA_INB(scp->r_io, ATA_DRIVE) & ATA_SLAVE) == ATA_MASTER) {
	    if ((scp->devices & ATA_ATA_MASTER) && scp->dev_softc[0])
		return ad_service((struct ad_softc *)scp->dev_softc[0], 0);
	}
	else {
	    if ((scp->devices & ATA_ATA_SLAVE) && scp->dev_softc[1])
		return ad_service((struct ad_softc *)scp->dev_softc[1], 0);
	}
#endif
    }
    return ATA_OP_FINISHED;
}

int
ata_wait(struct ata_softc *scp, int device, u_int8_t mask)
{
    int timeout = 0;
    
    DELAY(1);
    while (timeout < 5000000) {	/* timeout 5 secs */
	scp->status = ATA_INB(scp->r_io, ATA_STATUS);

	/* if drive fails status, reselect the drive just to be sure */
	if (scp->status == 0xff) {
	    ata_printf(scp, device, "no status, reselecting device\n");
	    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device);
	    DELAY(1);
	    scp->status = ATA_INB(scp->r_io, ATA_STATUS);
	}

	/* are we done ? */
	if (!(scp->status & ATA_S_BUSY))
	    break;	      

	if (timeout > 1000) {
	    timeout += 1000;
	    DELAY(1000);
	}
	else {
	    timeout += 10;
	    DELAY(10);
	}
    }	 
    if (scp->status & ATA_S_ERROR)
	scp->error = ATA_INB(scp->r_io, ATA_ERROR);
    if (timeout >= 5000000)	 
	return -1;	    
    if (!mask)	   
	return (scp->status & ATA_S_ERROR);	 
    
    /* Wait 50 msec for bits wanted. */	   
    timeout = 5000;
    while (timeout--) {	  
	scp->status = ATA_INB(scp->r_io, ATA_STATUS);
	if ((scp->status & mask) == mask) {
	    if (scp->status & ATA_S_ERROR)
		scp->error = ATA_INB(scp->r_io, ATA_ERROR);
	    return (scp->status & ATA_S_ERROR);	      
	}
	DELAY (10);	   
    }	  
    return -1;	    
}   

int
ata_command(struct ata_softc *scp, int device, u_int8_t command,
	   u_int16_t cylinder, u_int8_t head, u_int8_t sector, 
	   u_int8_t count, u_int8_t feature, int flags)
{
    int error = 0;
#ifdef ATA_DEBUG
    ata_printf(scp, device, "ata_command: addr=%04x, cmd=%02x, "
	       "c=%d, h=%d, s=%d, count=%d, feature=%d, flags=%02x\n",
	       rman_get_start(scp->r_io), command, cylinder, head, sector,
	       count, feature, flags);
#endif

    /* disable interrupt from device */
    if (scp->flags & ATA_QUEUED)
	ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_IDS | ATA_A_4BIT);

    /* select device */
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device);

    /* ready to issue command ? */
    if (ata_wait(scp, device, 0) < 0) { 
	ata_printf(scp, device, 
		   "timeout waiting to give command=%02x s=%02x e=%02x\n",
		   command, scp->status, scp->error);
	return -1;
    }

    ATA_OUTB(scp->r_io, ATA_FEATURE, feature);
    ATA_OUTB(scp->r_io, ATA_COUNT, count);
    ATA_OUTB(scp->r_io, ATA_SECTOR, sector);
    ATA_OUTB(scp->r_io, ATA_CYL_MSB, cylinder >> 8);
    ATA_OUTB(scp->r_io, ATA_CYL_LSB, cylinder);
    ATA_OUTB(scp->r_io, ATA_DRIVE, ATA_D_IBM | device | head);

    switch (flags) {
    case ATA_WAIT_INTR:
	scp->active = ATA_WAIT_INTR;
	asleep((caddr_t)scp, PRIBIO, "atacmd", 10 * hz);
	ATA_OUTB(scp->r_io, ATA_CMD, command);

	/* enable interrupt */
	if (scp->flags & ATA_QUEUED)
	    ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);

	if (await(PRIBIO, 10 * hz)) {
	    ata_printf(scp, device, "ata_command: timeout waiting for intr\n");
	    scp->active = ATA_IDLE;
	    error = -1;
	}
	break;
    
    case ATA_WAIT_READY:
	if (scp->active != ATA_REINITING)
	    scp->active = ATA_WAIT_READY;
	ATA_OUTB(scp->r_io, ATA_CMD, command);
	if (ata_wait(scp, device, ATA_S_READY) < 0) { 
	    ata_printf(scp, device, 
		       "timeout waiting for command=%02x s=%02x e=%02x\n",
		       command, scp->status, scp->error);
	    error = -1;
	}
	if (scp->active != ATA_REINITING)
	    scp->active = ATA_IDLE;
	break;

    case ATA_IMMEDIATE:
	ATA_OUTB(scp->r_io, ATA_CMD, command);
	break;

    default:
	ata_printf(scp, device, "DANGER: illegal interrupt flag=%s\n",
		   active2str(flags));
    }
    /* enable interrupt */
    if (scp->flags & ATA_QUEUED)
	ATA_OUTB(scp->r_altio, ATA_ALTSTAT, ATA_A_4BIT);
    return error;
}

int
ata_get_lun(u_int32_t *map)
{
    int lun = ffs(~*map) - 1;

    *map |= (1 << lun);
    return lun;
}

int
ata_test_lun(u_int32_t *map, int lun)
{
    return (*map & (1 << lun));
}

void
ata_free_lun(u_int32_t *map, int lun)
{
    *map &= ~(1 << lun);
}
 
int
ata_printf(struct ata_softc *scp, int device, const char * fmt, ...)
{
    va_list ap;
    int ret;

    if (device == -1)
	ret = printf("ata%d: ", device_get_unit(scp->dev));
    else
	ret = printf("ata%d-%s: ", device_get_unit(scp->dev),
		     (device == ATA_MASTER) ? "master" : "slave");
    va_start(ap, fmt);
    ret += vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

char *
ata_mode2str(int mode)
{
    switch (mode) {
    case ATA_PIO: return "BIOSPIO";
    case ATA_PIO0: return "PIO0";
    case ATA_PIO1: return "PIO1";
    case ATA_PIO2: return "PIO2";
    case ATA_PIO3: return "PIO3";
    case ATA_PIO4: return "PIO4";
    case ATA_WDMA2: return "WDMA2";
    case ATA_UDMA2: return "UDMA33";
    case ATA_UDMA4: return "UDMA66";
    case ATA_UDMA5: return "UDMA100";
    case ATA_DMA: return "BIOSDMA";
    default: return "???";
    }
}

int
ata_pio2mode(int pio)
{
    switch (pio) {
    default:
    case 0: return ATA_PIO0;
    case 1: return ATA_PIO1;
    case 2: return ATA_PIO2;
    case 3: return ATA_PIO3;
    case 4: return ATA_PIO4;
    }
}

int
ata_pmode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_64_70) {
	if (ap->apiomodes & 2)
	    return 4;
	if (ap->apiomodes & 1) 
	    return 3;
    }	
    if (ap->opiomode == 2)
	return 2;
    if (ap->opiomode == 1)
	return 1;
    if (ap->opiomode == 0)
	return 0;
    return -1; 
} 

int
ata_wmode(struct ata_params *ap)
{
    if (ap->wdmamodes & 4)
	return 2;
    if (ap->wdmamodes & 2)
	return 1;
    if (ap->wdmamodes & 1)
	return 0;
    return -1;
}

int
ata_umode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_88) {
	if (ap->udmamodes & 0x20)
	    return 5;
	if (ap->udmamodes & 0x10)
	    return 4;
	if (ap->udmamodes & 0x08)
	    return 3;
	if (ap->udmamodes & 0x04)
	    return 2;
	if (ap->udmamodes & 0x02)
	    return 1;
	if (ap->udmamodes & 0x01)
	    return 0;
    }
    return -1;
}

static char *
active2str(int active)
{
    static char buf[8];

    switch (active) {
    case ATA_IDLE:
	return("ATA_IDLE");
    case ATA_IMMEDIATE:
	return("ATA_IMMEDIATE");
    case ATA_WAIT_INTR:
	return("ATA_WAIT_INTR");
    case ATA_WAIT_READY:
	return("ATA_WAIT_READY");
    case ATA_ACTIVE:
	return("ATA_ACTIVE");
    case ATA_ACTIVE_ATA:
	return("ATA_ACTIVE_ATA");
    case ATA_ACTIVE_ATAPI:
	return("ATA_ACTIVE_ATAPI");
    case ATA_REINITING:
	return("ATA_REINITING");
    default:
	sprintf(buf, "0x%02x", active);
	return buf;
    }
}

static void
bswap(int8_t *buf, int len) 
{
    u_int16_t *ptr = (u_int16_t*)(buf + len);

    while (--ptr >= (u_int16_t*)buf)
	*ptr = ntohs(*ptr);
} 

static void
btrim(int8_t *buf, int len)
{ 
    int8_t *ptr;

    for (ptr = buf; ptr < buf+len; ++ptr) 
	if (!*ptr)
	    *ptr = ' ';
    for (ptr = buf + len - 1; ptr >= buf && *ptr == ' '; --ptr)
	*ptr = 0;
}

static void
bpack(int8_t *src, int8_t *dst, int len)
{
    int i, j, blank;

    for (i = j = blank = 0 ; i < len; i++) {
	if (blank && src[i] == ' ') continue;
	if (blank && src[i] != ' ') {
	    dst[j++] = src[i];
	    blank = 0;
	    continue;
	}
	if (src[i] == ' ') {
	    blank = 1;
	    if (i == 0)
		continue;
	}
	dst[j++] = src[i];
    }
    if (j < len) 
	dst[j] = 0x00;
}

static void
ata_change_mode(struct ata_softc *scp, int device, int mode)
{
    int s = splbio();

    while (!atomic_cmpset_int(&scp->active, ATA_IDLE, ATA_ACTIVE))
	tsleep((caddr_t)&s, PRIBIO, "atachm", hz/4);

    ata_dmainit(scp, device, ata_pmode(ATA_PARAM(scp, device)),
		mode < ATA_DMA ?  -1 : ata_wmode(ATA_PARAM(scp, device)),
		mode < ATA_DMA ?  -1 : ata_umode(ATA_PARAM(scp, device)));
    scp->active = ATA_IDLE;
    ata_start(scp);
    splx(s);
}

static int
sysctl_hw_ata(SYSCTL_HANDLER_ARGS)
{
    struct ata_softc *scp;
    int ctlr, error, i;

    /* readout internal state */
    bzero(ata_conf, sizeof(ata_conf));
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(scp = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	for (i = 0; i < 2; i++) {
	    if (!scp->dev_softc[i])
		strcat(ata_conf, "---,");
	    else if (scp->mode[i] >= ATA_DMA)
		strcat(ata_conf, "dma,");
	    else
		strcat(ata_conf, "pio,");
	}
    }
    error = sysctl_handle_string(oidp, ata_conf, sizeof(ata_conf), req);   
    if (error == 0 && req->newptr != NULL) {
	char *ptr = ata_conf;

        /* update internal state */
	i = 0;
        while (*ptr) {
	    if (!strncmp(ptr, "pio", 3) || !strncmp(ptr, "PIO", 3)) {
		if ((scp = devclass_get_softc(ata_devclass, i >> 1)) &&
		    scp->dev_softc[i & 1] && scp->mode[i & 1] >= ATA_DMA)
		    ata_change_mode(scp, (i & 1)?ATA_SLAVE:ATA_MASTER, ATA_PIO);
	    }
	    else if (!strncmp(ptr, "dma", 3) || !strncmp(ptr, "DMA", 3)) {
		if ((scp = devclass_get_softc(ata_devclass, i >> 1)) &&
		    scp->dev_softc[i & 1] && scp->mode[i & 1] < ATA_DMA)
		    ata_change_mode(scp, (i & 1)?ATA_SLAVE:ATA_MASTER, ATA_DMA);
	    }
	    else if (strncmp(ptr, "---", 3))
		break;
	    ptr+=3;
	    if (*ptr++ != ',' ||
		++i > (devclass_get_maxunit(ata_devclass) << 1))
		break; 
        }
    }
    return error;
}
SYSCTL_PROC(_hw, OID_AUTO, atamodes, CTLTYPE_STRING | CTLFLAG_RW,
            0, sizeof(ata_conf), sysctl_hw_ata, "A", "");

static void
ata_init(void)
{
    /* register boot attach to be run when interrupts are enabled */
    if (!(ata_delayed_attach = (struct intr_config_hook *)
			       malloc(sizeof(struct intr_config_hook),
				      M_TEMP, M_NOWAIT | M_ZERO))) {
	printf("ata: malloc of delayed attach hook failed\n");
	return;
    }

    ata_delayed_attach->ich_func = (void*)ata_boot_attach;
    if (config_intrhook_establish(ata_delayed_attach) != 0) {
	printf("ata: config_intrhook_establish failed\n");
	free(ata_delayed_attach, M_TEMP);
    }
}
SYSINIT(atadev, SI_SUB_DRIVERS, SI_ORDER_SECOND, ata_init, NULL)
