/*-
 * Copyright (c) 1998,1999,2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/disk.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>
#include <sys/sysctl.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <machine/clock.h>
#ifdef __alpha__
#include <machine/md_var.h>
#endif
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <dev/ata/ata-all.h>

/* device structures */
struct ata_pci_controller {
    struct resource *bmio;
    int bmaddr;
    struct resource *irq;
    int irqcnt;
};

/* misc defines */
#define IOMASK			0xfffffffc
#define GRANDPARENT(dev)	device_get_parent(device_get_parent(dev))
#define ATA_MASTERDEV(dev)	((pci_get_progif(dev) & 0x80) && \
				 (pci_get_progif(dev) & 0x05) != 0x05)

int
ata_find_dev(device_t dev, u_int32_t devid, u_int32_t revid)
{
    device_t *children;
    int nchildren, i;

    if (device_get_children(device_get_parent(dev), &children, &nchildren))
	return 0;

    for (i = 0; i < nchildren; i++) {
	if (pci_get_devid(children[i]) == devid &&
	    pci_get_revid(children[i]) >= revid) {
	    free(children, M_TEMP);
	    return 1;
	}
    }
    free(children, M_TEMP);
    return 0;
}

static void
ata_via_southbridge_fixup(device_t dev)
{
    device_t *children;
    int nchildren, i;

    if (device_get_children(device_get_parent(dev), &children, &nchildren))
	return;

    for (i = 0; i < nchildren; i++) {
	if (pci_get_devid(children[i]) == 0x03051106 ||		/* VIA VT8363 */
	    pci_get_devid(children[i]) == 0x03911106 ||		/* VIA VT8371 */
	    pci_get_devid(children[i]) == 0x31021106 ||		/* VIA VT8662 */
	    pci_get_devid(children[i]) == 0x31121106) {		/* VIA VT8361 */
	    u_int8_t reg76 = pci_read_config(children[i], 0x76, 1);

	    if ((reg76 & 0xf0) != 0xd0) {
		device_printf(dev,
		"Correcting VIA config for southbridge data corruption bug\n");
		pci_write_config(children[i], 0x75, 0x80, 1);
		pci_write_config(children[i], 0x76, (reg76 & 0x0f) | 0xd0, 1);
	    }
	    break;
	}
    }
    free(children, M_TEMP);
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
    case 0x84ca8086:
	return "Intel PIIX4 ATA33 controller";

    case 0x24218086:
	return "Intel ICH0 ATA33 controller";

    case 0x24118086:
    case 0x76018086:
	return "Intel ICH ATA66 controller";

    case 0x244a8086:
    case 0x244b8086:
	return "Intel ICH2 ATA100 controller";

    case 0x248a8086:
    case 0x248b8086:
	return "Intel ICH3 ATA100 controller";

    case 0x24cb8086:
	return "Intel ICH4 ATA100 controller";

    case 0x24db8086:
	return "Intel ICH5 ATA100 controller";

    case 0x522910b9:
	if (pci_get_revid(dev) >= 0xc4)
	    return "AcerLabs Aladdin ATA100 controller";
	else if (pci_get_revid(dev) >= 0xc2)
	    return "AcerLabs Aladdin ATA66 controller";
	else if (pci_get_revid(dev) >= 0x20)
	    return "AcerLabs Aladdin ATA33 controller";
	else
	    return "AcerLabs Aladdin ATA controller";

    case 0x05711106: 
	if (ata_find_dev(dev, 0x05861106, 0x02))
	    return "VIA 82C586 ATA33 controller";
	if (ata_find_dev(dev, 0x05861106, 0))
	    return "VIA 82C586 ATA controller";
	if (ata_find_dev(dev, 0x05961106, 0x12))
	    return "VIA 82C596 ATA66 controller";
	if (ata_find_dev(dev, 0x05961106, 0))
	    return "VIA 82C596 ATA33 controller";
	if (ata_find_dev(dev, 0x06861106, 0x40))
	    return "VIA 82C686 ATA100 controller";
	if (ata_find_dev(dev, 0x06861106, 0x10))
	    return "VIA 82C686 ATA66 controller";
	if (ata_find_dev(dev, 0x06861106, 0))
	    return "VIA 82C686 ATA33 controller";
	if (ata_find_dev(dev, 0x82311106, 0))
	    return "VIA 8231 ATA100 controller";
	if (ata_find_dev(dev, 0x30741106, 0) ||
	    ata_find_dev(dev, 0x31091106, 0))
	    return "VIA 8233 ATA100 controller";
	if (ata_find_dev(dev, 0x31471106, 0))
	    return "VIA 8233 ATA133 controller";
	if (ata_find_dev(dev, 0x31771106, 0))
	    return "VIA 8235 ATA133 controller";
	return "VIA Apollo ATA controller";

    case 0x55131039:
	if (ata_find_dev(dev, 0x06301039, 0x30) ||
	    ata_find_dev(dev, 0x06331039, 0) ||
	    ata_find_dev(dev, 0x06351039, 0) ||
	    ata_find_dev(dev, 0x06401039, 0) ||
	    ata_find_dev(dev, 0x06451039, 0) ||
	    ata_find_dev(dev, 0x06501039, 0) ||
	    ata_find_dev(dev, 0x07301039, 0) ||
	    ata_find_dev(dev, 0x07331039, 0) ||
	    ata_find_dev(dev, 0x07351039, 0) ||
	    ata_find_dev(dev, 0x07401039, 0) ||
	    ata_find_dev(dev, 0x07451039, 0) ||
	    ata_find_dev(dev, 0x07501039, 0))
	    return "SiS 5591 ATA100 controller";
	else if (ata_find_dev(dev, 0x05301039, 0) ||
	    ata_find_dev(dev, 0x05401039, 0) ||
	    ata_find_dev(dev, 0x06201039, 0) ||
	    ata_find_dev(dev, 0x06301039, 0))
	    return "SiS 5591 ATA66 controller";
	else
	    return "SiS 5591 ATA33 controller";

    case 0x06801095:
	return "SiI 0680 ATA133 controller";

    case 0x06491095:
	return "CMD 649 ATA100 controller";

    case 0x06481095:
	return "CMD 648 ATA66 controller";

    case 0x06461095:
	return "CMD 646 ATA controller";

    case 0xc6931080:
	if (pci_get_subclass(dev) == PCIS_STORAGE_IDE)
	    return "Cypress 82C693 ATA controller";
	return NULL;

    case 0x01021078:
	return "Cyrix 5530 ATA33 controller";

    case 0x74091022:
	return "AMD 756 ATA66 controller";

    case 0x74111022:
	return "AMD 766 ATA100 controller";

    case 0x74411022:
	return "AMD 768 ATA100 controller";

    case 0x01bc10de:
	return "nVIDIA nForce ATA100 controller";

    case 0x02111166:
	return "ServerWorks ROSB4 ATA33 controller";

    case 0x02121166:
	if (pci_get_revid(dev) >= 0x92)
	    return "ServerWorks CSB5 ATA100 controller";
	else
	    return "ServerWorks CSB5 ATA66 controller";

    case 0x4d33105a:
	return "Promise ATA33 controller";

    case 0x0d38105a:
    case 0x4d38105a:
	return "Promise ATA66 controller";

    case 0x0d30105a:
    case 0x4d30105a:
	return "Promise ATA100 controller";

    case 0x4d68105a:
    case 0x6268105a: 
	if (pci_get_devid(GRANDPARENT(dev)) == 0x00221011 &&
	    pci_get_class(GRANDPARENT(dev)) == PCIC_BRIDGE) {
	    static long start = 0, end = 0;

	    /* we belive we are on a TX4, now do our (simple) magic */
	    if (pci_get_slot(dev) == 1) {
		bus_get_resource(dev, SYS_RES_IRQ, 0, &start, &end);
		return "Promise TX4 ATA100 controller (channel 0+1)";
	    }
	    else if (pci_get_slot(dev) == 2 && start && end) {
		bus_set_resource(dev, SYS_RES_IRQ, 0, start, end);
		start = end = 0;
		return "Promise TX4 ATA100 controller (channel 2+3)";
	    }
	    else
		start = end = 0;
	}
	return "Promise TX2 ATA100 controller";

    case 0x4d69105a:
    case 0x5275105a:
    case 0x6269105a: 
    case 0x7275105a: 
	return "Promise TX2 ATA133 controller";

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
	case 0x05:
	    return "HighPoint HPT372 ATA133 controller";
	}
	return NULL;

    case 0x00051103:
	switch (pci_get_revid(dev)) {
	case 0x01:
	case 0x02:
	    return "HighPoint HPT372 ATA133 controller";
	}
	return NULL;

    case 0x00081103:
	switch (pci_get_revid(dev)) {
	case 0x07:
	    return "HighPoint HPT374 ATA133 controller";
	}
	return NULL;

    case 0x000116ca:
	return "Cenatek Rocket Drive controller";

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
    struct ata_pci_controller *controller = device_get_softc(dev);
    u_int8_t class, subclass;
    u_int32_t type, cmd;
    int rid;

    /* set up vendor-specific stuff */
    type = pci_get_devid(dev);
    class = pci_get_class(dev);
    subclass = pci_get_subclass(dev);
    cmd = pci_read_config(dev, PCIR_COMMAND, 4);

    if (!(cmd & PCIM_CMD_PORTEN)) {
	device_printf(dev, "ATA channel disabled by BIOS\n");
	return 0;
    }

    /* is busmastering supported ? */
    if ((cmd & (PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN)) == 
	(PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN)) {

	/* is there a valid port range to connect to ? */
	rid = 0x20;
	controller->bmio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					      0, ~0, 1, RF_ACTIVE);
	if (!controller->bmio)
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

    case 0x4d38105a: /* Promise 66 & 100 (before TX2) need the clock changed */
    case 0x4d30105a:
    case 0x0d30105a:
	ATA_OUTB(controller->bmio, 0x11, ATA_INB(controller->bmio, 0x11)|0x0a);
	/* FALLTHROUGH */

    case 0x4d33105a: /* Promise (before TX2) need burst mode turned on */
	ATA_OUTB(controller->bmio, 0x1f, ATA_INB(controller->bmio, 0x1f)|0x01);
	break;

    case 0x00041103:	/* HighPoint HPT366/368/370/372 */
	if (pci_get_revid(dev) < 2) {	/* HPT 366 */
	    /* turn off interrupt prediction */
	    pci_write_config(dev, 0x51, 
			     (pci_read_config(dev, 0x51, 1) & ~0x80), 1);
	    break;
	}
	if (pci_get_revid(dev) < 5) {   /* HPT368/370 */
	    /* turn off interrupt prediction */
	    pci_write_config(dev, 0x51,
			     (pci_read_config(dev, 0x51, 1) & ~0x03), 1);
	    pci_write_config(dev, 0x55,
			     (pci_read_config(dev, 0x55, 1) & ~0x03), 1);

	    /* turn on interrupts */
	    pci_write_config(dev, 0x5a,
			     (pci_read_config(dev, 0x5a, 1) & ~0x10), 1);

	    /* set clocks etc */
	    pci_write_config(dev, 0x5b, 0x22, 1);
	    break;
	}
	/* FALLTHROUGH */

    case 0x00051103:	/* HighPoint HPT372 */
    case 0x00081103:	/* HighPoint HPT374 */
	/* turn off interrupt prediction */
	pci_write_config(dev, 0x51, (pci_read_config(dev, 0x51, 1) & ~0x03), 1);
	pci_write_config(dev, 0x55, (pci_read_config(dev, 0x55, 1) & ~0x03), 1);

	/* turn on interrupts */
	pci_write_config(dev, 0x5a, (pci_read_config(dev, 0x5a, 1) & ~0x10), 1);

	/* set clocks etc */
	pci_write_config(dev, 0x5b,
			 (pci_read_config(dev, 0x5b, 1) & 0x01) | 0x20, 1);
	break;

    case 0x05711106: /* VIA 82C586, '596, '686 default setup */
	/* prepare for ATA-66 on the 82C686a and 82C596b */
	if ((ata_find_dev(dev, 0x06861106, 0x10) && 
	     !ata_find_dev(dev, 0x06861106, 0x40)) || 
	    ata_find_dev(dev, 0x05961106, 0x12))
	    pci_write_config(dev, 0x50, 0x030b030b, 4);	  

	/* the southbridge might need the data corruption fix */
	if (ata_find_dev(dev, 0x06861106, 0x40) ||
	    ata_find_dev(dev, 0x82311106, 0x10))
	    ata_via_southbridge_fixup(dev);
	/* FALLTHROUGH */

    case 0x74091022: /* AMD 756 default setup */
    case 0x74111022: /* AMD 766 default setup */
    case 0x74411022: /* AMD 768 default setup */
    case 0x01bc10de: /* nVIDIA nForce default setup */
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
	break;

    case 0x02111166: /* ServerWorks ROSB4 enable UDMA33 */
	pci_write_config(dev, 0x64,   
			 (pci_read_config(dev, 0x64, 4) & ~0x00002000) |
			 0x00004000, 4);
	break;
	
    case 0x02121166: /* ServerWorks CSB5 enable UDMA66/100 depending on rev */
	pci_write_config(dev, 0x5a,   
			 (pci_read_config(dev, 0x5a, 1) & ~0x40) |
			 (pci_get_revid(dev) >= 0x92) ? 0x03 : 0x02, 1);
	break;

    case 0x06801095: /* SiI 0680 set ATA reference clock speed */
	if ((pci_read_config(dev, 0x8a, 1) & 0x30) != 0x10)
	    pci_write_config(dev, 0x8a, 
			     (pci_read_config(dev, 0x8a, 1) & 0x0F) | 0x10, 1);
	if ((pci_read_config(dev, 0x8a, 1) & 0x30) != 0x10)
	    device_printf(dev, "SiI 0680 could not set clock\n");
	break;

    case 0x06491095:
    case 0x06481095:
    case 0x06461095: /* CMD 646 enable interrupts, set DMA read mode */
	pci_write_config(dev, 0x71, 0x01, 1);
	break;

    case 0x10001042:   /* RZ 100? known bad, no DMA */
    case 0x10011042:
    case 0x06401095:   /* CMD 640 known bad, no DMA */
	controller->bmio = NULL;
	device_printf(dev, "Busmastering DMA disabled\n");
    }

    if (controller->bmio) {
	controller->bmaddr = rman_get_start(controller->bmio);
	BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
			     SYS_RES_IOPORT, rid, controller->bmio);
	controller->bmio = NULL;
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
ata_pci_intr(struct ata_channel *ch)
{
    u_int8_t dmastat;

    /* 
     * since we might share the IRQ with another device, and in some
     * cases with our twin channel, we only want to process interrupts
     * that we know this channel generated.
     */
    switch (ch->chiptype) {
    case 0x00041103:	/* HighPoint HPT366/368/370/372 */
    case 0x00051103:	/* HighPoint HPT372 */
    case 0x00081103:	/* HighPoint HPT374 */
	if (((dmastat = ata_dmastatus(ch)) &
	    (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) != ATA_BMSTAT_INTERRUPT)
	    return 1;
	ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT, dmastat | ATA_BMSTAT_INTERRUPT);
	DELAY(1);
	return 0;

    case 0x06481095:	/* CMD 648 */
    case 0x06491095:	/* CMD 649 */
	if (!(pci_read_config(device_get_parent(ch->dev), 0x71, 1) &
	      (ch->unit ? 0x08 : 0x04)))
	    return 1;
	break;

    case 0x4d33105a:	/* Promise Ultra/Fasttrak 33 */
    case 0x0d38105a:	/* Promise Fasttrak 66 */
    case 0x4d38105a:	/* Promise Ultra/Fasttrak 66 */
    case 0x0d30105a:	/* Promise OEM ATA100 */
    case 0x4d30105a:	/* Promise Ultra/Fasttrak 100 */
	if (!(ATA_INL(ch->r_bmio, (ch->unit ? 0x14 : 0x1c)) &
	      (ch->unit ? 0x00004000 : 0x00000400)))
	    return 1;
	break;

    case 0x4d68105a:	/* Promise TX2 ATA100 */
    case 0x6268105a:	/* Promise TX2 ATA100 */
    case 0x4d69105a:	/* Promise TX2 ATA133 */
    case 0x5275105a:	/* Promise TX2 ATA133 */
    case 0x6269105a:	/* Promise TX2 ATA133 */
    case 0x7275105a:	/* Promise TX2 ATA133 */
	ATA_OUTB(ch->r_bmio, ATA_BMDEVSPEC_0, 0x0b);
	if (!(ATA_INB(ch->r_bmio, ATA_BMDEVSPEC_1) & 0x20))
	    return 1;
	break;
    }

    if (ch->flags & ATA_DMA_ACTIVE) {
	if (!((dmastat = ata_dmastatus(ch)) & ATA_BMSTAT_INTERRUPT))
	    return 1;
	ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT, dmastat | ATA_BMSTAT_INTERRUPT);
	DELAY(1);
    }
    return 0;
}

static int
ata_pci_print_child(device_t dev, device_t child)
{
    struct ata_channel *ch = device_get_softc(child);
    int retval = 0;

    retval += bus_print_child_header(dev, child);
    retval += printf(": at 0x%lx", rman_get_start(ch->r_io));

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
    struct resource *res = NULL;
    int unit = ((struct ata_channel *)device_get_softc(child))->unit;
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
			end = rman_get_start(res) + ATA_ALTIOSIZE - 1;
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
	    if (controller->bmaddr) {
		myrid = 0x20;
		start = (unit == 0 ? 
			 controller->bmaddr : controller->bmaddr+ATA_BMIOSIZE);
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
	    return alpha_platform_alloc_ide_intr(unit);
#else
	    int irq = (unit == 0 ? 14 : 15);

	    return BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
				      SYS_RES_IRQ, rid, irq, irq, 1, flags);
#endif
	}
	else {
	    /* primary and secondary channels share interrupt, keep track */
	    if (!controller->irq)
		controller->irq = BUS_ALLOC_RESOURCE(device_get_parent(dev), 
						     dev, SYS_RES_IRQ,
						     rid, 0, ~0, 1, flags);
	    controller->irqcnt++;
	    return controller->irq;
	}
    }
    return 0;
}

static int
ata_pci_release_resource(device_t dev, device_t child, int type, int rid,
			 struct resource *r)
{
    struct ata_pci_controller *controller = device_get_softc(dev);
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

	case ATA_BMADDR_RID:
	    return BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
					SYS_RES_IOPORT, 0x20, r);
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
	else {
	    /* primary and secondary channels share interrupt, keep track */
	    if (--controller->irqcnt)
		return 0;
	    controller->irq = NULL;
	    return BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
					SYS_RES_IRQ, rid, r);
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
	return alpha_platform_setup_ide_intr(irq, intr, arg, cookiep);
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
	return alpha_platform_teardown_ide_intr(irq, cookie);
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
    sizeof(struct ata_pci_controller),
};

static devclass_t ata_pci_devclass;

DRIVER_MODULE(atapci, pci, ata_pci_driver, ata_pci_devclass, 0, 0);

static int
ata_pcisub_probe(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int count, i;

    /* find channel number on this controller */
    device_get_children(device_get_parent(dev), &children, &count);
    for (i = 0; i < count; i++) {
	if (children[i] == dev)
	    ch->unit = i;
    }
    free(children, M_TEMP);
    ch->chiptype = pci_get_devid(device_get_parent(dev));
    ch->intr_func = ata_pci_intr;
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
    sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, atapci, ata_pcisub_driver, ata_devclass, 0, 0);
