/*-
 * Copyright (c) 1998,1999,2000 Søren Schmidt
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

#include "ata.h"
#include "apm.h"
#include "isa.h"
#include "pci.h"
#include "atadisk.h"
#include "atapicd.h"
#include "atapifd.h"
#include "atapist.h"
#include "opt_global.h"
#include "opt_ata.h"
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
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif
#include <isa/isavar.h>
#include <isa/isareg.h>
#include <machine/clock.h>
#ifdef __i386__
#include <machine/smp.h>
#include <i386/isa/intr_machdep.h>
#endif
#if NAPM > 0
#include <machine/apm_bios.h>
#endif
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/atapi-all.h>

/* misc defines */
#if SMP == 0
#define isa_apic_irq(x) x
#endif
#define IOMASK	0xfffffffc

/* prototypes */
static int32_t ata_probe(int32_t, int32_t, int32_t, device_t, int32_t *);
static void ata_attach(void *);
static int32_t ata_getparam(struct ata_softc *, int32_t, u_int8_t);
static void ataintr(void *);
static int8_t *active2str(int32_t);
static void bswap(int8_t *, int32_t);
static void btrim(int8_t *, int32_t);
static void bpack(int8_t *, int8_t *, int32_t);

/* local vars */
static int32_t atanlun = 2;
static struct intr_config_hook *ata_attach_hook = NULL;
static devclass_t ata_devclass;
struct ata_softc *atadevices[MAXATA];
MALLOC_DEFINE(M_ATA, "ATA generic", "ATA driver generic layer");

#if NISA > 0
static struct isa_pnp_id ata_ids[] = {
    {0x0006d041,	"Generic ESDI/IDE/ATA controller"},	/* PNP0600 */
    {0x0106d041,	"Plus Hardcard II"},			/* PNP0601 */
    {0x0206d041,	"Plus Hardcard IIXL/EZ"},		/* PNP0602 */
    {0x0306d041,	"Generic ATA"},				/* PNP0603 */
    {0}
};

static int
ata_isaprobe(device_t dev)
{
    struct resource *port;
    int rid;
    int32_t ctlr, res;
    int32_t lun;

    /* Check isapnp ids */
    if (ISA_PNP_PROBE(device_get_parent(dev), dev, ata_ids) == ENXIO)
	return ENXIO;
    
    /* Allocate the port range */
    rid = 0;
    port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
    if (!port)
	return ENOMEM;

    /* check if allready in use by a PCI device */
    for (ctlr = 0; ctlr < atanlun; ctlr++) {
	if (atadevices[ctlr] && atadevices[ctlr]->ioaddr==rman_get_start(port)){
	    printf("ata-isa%d: already registered as ata%d\n", 
		   device_get_unit(dev), ctlr);
	    bus_release_resource(dev, SYS_RES_IOPORT, 0, port);
	    return ENXIO;
	}
    }

    lun = 0;
    res = ata_probe(rman_get_start(port), rman_get_start(port) + ATA_ALTPORT,
		    0, dev, &lun);

    bus_release_resource(dev, SYS_RES_IOPORT, 0, port);

    if (res) {
	isa_set_portsize(dev, res);
	*(int *)device_get_softc(dev) = lun;
        atadevices[lun]->flags |= ATA_USE_16BIT;
	return 0;
    }
    return ENXIO;
}

static int
ata_isaattach(device_t dev)
{
    struct resource *port;
    struct resource *irq;
    void *ih;
    int rid;

    /* Allocate the port range and interrupt */
    rid = 0;
    port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
    if (!port)
	return (ENOMEM);

    rid = 0;
    irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_ACTIVE);
    if (!irq) {
	bus_release_resource(dev, SYS_RES_IOPORT, 0, port);
	return (ENOMEM);
    }
    return bus_setup_intr(dev, irq, INTR_TYPE_BIO, ataintr, 
			  atadevices[*(int *)device_get_softc(dev)], &ih);
}

static device_method_t ata_isa_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	ata_isaprobe),
    DEVMETHOD(device_attach,	ata_isaattach),
    { 0, 0 }
};

static driver_t ata_isa_driver = {
    "ata",
    ata_isa_methods,
    sizeof(int),
};

DRIVER_MODULE(ata, isa, ata_isa_driver, ata_devclass, 0, 0);
#endif

#if NPCI > 0
static const char *
ata_pcimatch(device_t dev)
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
	return "Intel PIIX4 ATA-33 controller";

    case 0x24118086:
	return "Intel ICH ATA-66 controller";

    case 0x24218086:
	return "Intel ICH0 ATA-33 controller";

    case 0x522910b9:
	return "AcerLabs Aladdin ATA-33 controller";

    case 0x05711106: /* 82c586 & 82c686 */
	if (ata_find_dev(dev, 0x05861106))
	    return "VIA 82C586 ATA-33 controller";
	if (ata_find_dev(dev, 0x05961106))
	    return "VIA 82C596 ATA-33 controller";
	if (ata_find_dev(dev, 0x06861106))
	    return "VIA 82C686 ATA-66 controller";
	return "VIA Apollo ATA controller";

    case 0x55131039:
	return "SiS 5591 ATA-33 controller";

    case 0x74091022:
	return "AMD 756 ATA-66 controller";

    case 0x4d33105a:
	return "Promise ATA-33 controller";

    case 0x4d38105a:
	return "Promise ATA-66 controller";

    case 0x00041103:
	return "HighPoint HPT366 ATA-66 controller";

   /* unsupported but known chipsets, generic DMA only */
    case 0x06401095:
	return "CMD 640 ATA controller (generic mode)";

    case 0x06461095:
	return "CMD 646 ATA controller (generic mode)";

    case 0xc6931080:
	return "Cypress 82C693 ATA controller (generic mode)";

    case 0x01021078:
	return "Cyrix 5530 ATA controller (generic mode)";

    default:
	if (pci_get_class(dev) == PCIC_STORAGE &&
	    (pci_get_subclass(dev) == PCIS_STORAGE_IDE))
	    return "Unknown PCI ATA controller (generic mode)";
    }
    return NULL;
}

static int
ata_pciprobe(device_t dev)
{
    const char *desc = ata_pcimatch(dev);
    
    if (desc) {
	device_set_desc(dev, desc);
	return 0;
    } 
    else
	return ENXIO;
}

static int
ata_pciattach(device_t dev)
{
    int unit = device_get_unit(dev);
    struct ata_softc *scp;
    u_int32_t type;
    u_int8_t class, subclass;
    u_int32_t cmd;
    int32_t iobase_1, iobase_2, altiobase_1, altiobase_2; 
    int32_t bmaddr_1 = 0, bmaddr_2 = 0, irq1, irq2;
    struct resource *irq = NULL;
    int32_t lun;

    /* set up vendor-specific stuff */
    type = pci_get_devid(dev);
    class = pci_get_class(dev);
    subclass = pci_get_subclass(dev);
    cmd = pci_read_config(dev, PCIR_COMMAND, 4);

#ifdef ATA_DEBUG
    printf("ata-pci%d: type=%08x class=%02x subclass=%02x cmd=%08x if=%02x\n",
	   unit, type, class, subclass, cmd, pci_get_progif(dev));
#endif

    if (pci_get_progif(dev) & PCIP_STORAGE_IDE_MASTERDEV) {
	iobase_1 = IO_WD1;
	altiobase_1 = iobase_1 + ATA_ALTPORT;
	irq1 = 14;
    } 
    else {
	iobase_1 = pci_read_config(dev, 0x10, 4) & IOMASK;
	altiobase_1 = pci_read_config(dev, 0x14, 4) & IOMASK;
	irq1 = pci_read_config(dev, PCI_INTERRUPT_REG, 4) & 0xff;
	/* this is needed for old non-std systems */
	if (iobase_1 == IO_WD1 && irq1 == 0x00)
	    irq1 = 14;
    }

    if (pci_get_progif(dev) & PCIP_STORAGE_IDE_MASTERDEV) {
	iobase_2 = IO_WD2;
	altiobase_2 = iobase_2 + ATA_ALTPORT;
	irq2 = 15;
    }
    else {
	iobase_2 = pci_read_config(dev, 0x18, 4) & IOMASK;
	altiobase_2 = pci_read_config(dev, 0x1c, 4) & IOMASK;
	irq2 = pci_read_config(dev, PCI_INTERRUPT_REG, 4) & 0xff;
	/* this is needed for old non-std systems */
	if (iobase_2 == IO_WD2 && irq2 == 0x00)
	    irq2 = 15;
    }

    /* is this controller busmaster DMA capable ? */
    if (pci_get_progif(dev) & PCIP_STORAGE_IDE_MASTERDEV) {
	/* is busmastering support turned on ? */
	if ((pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4) & 5) == 5) {
	    /* is there a valid port range to connect to ? */
	    if ((bmaddr_1 = pci_read_config(dev, 0x20, 4) & IOMASK))
		bmaddr_2 = bmaddr_1 + ATA_BM_OFFSET1;
	    else
		printf("ata-pci%d: Busmastering DMA not configured\n", unit);
	}
	else
	    printf("ata-pci%d: Busmastering DMA not enabled\n", unit);
    }
    else {
    	if (type == 0x4d33105a || type == 0x4d38105a || type == 0x00041103) {
	    /* Promise and HPT366 controllers support busmastering DMA */
	    bmaddr_1 = pci_read_config(dev, 0x20, 4) & IOMASK;
	    bmaddr_2 = bmaddr_1 + ATA_BM_OFFSET1;
	}
	else
	    /* we dont know this controller, no busmastering DMA */
	    printf("ata-pci%d: Busmastering DMA not supported\n", unit);
    }

    /* do extra chipset specific setups */
    switch (type) {
    case 0x522910b9: /* Aladdin need to activate the ATAPI FIFO */
	pci_write_config(dev, 0x53, 
			 (pci_read_config(dev, 0x53, 1) & ~0x01) | 0x02, 1);
	break;

    case 0x4d33105a:
    case 0x4d38105a: /* Promise's need burst mode to be turned on */
	outb(bmaddr_1 + 0x1f, inb(bmaddr_1 + 0x1f) | 0x01);
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
	
	/* prepare for ATA-66 on the 82C686 */
	if (ata_find_dev(dev, 0x06861106))
	    pci_write_config(dev, 0x50, 
			     pci_read_config(dev, 0x50, 4) | 0x070f070f, 4);   
	break;
    }
	
    /* now probe the addresse found for "real" ATA/ATAPI hardware */
    lun = 0;
    if (iobase_1 && ata_probe(iobase_1, altiobase_1, bmaddr_1, dev, &lun)) {
	int rid;
	void *ih;

	scp = atadevices[lun];
	scp->chiptype = type;
	rid = 0;
	if (iobase_1 == IO_WD1) {
#ifdef __alpha__
	    alpha_platform_setup_ide_intr(0, ataintr, scp);
#else
	    bus_set_resource(dev, SYS_RES_IRQ, rid, irq1, 1);
	    if (!(irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					   RF_SHAREABLE | RF_ACTIVE)))
		printf("ata_pciattach: Unable to alloc interrupt\n");
#endif
	} else {
	    if (!(irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					   RF_SHAREABLE | RF_ACTIVE)))
		printf("ata_pciattach: Unable to alloc interrupt\n");
	}
	if (irq)
	    bus_setup_intr(dev, irq, INTR_TYPE_BIO, ataintr, scp, &ih);
	printf("ata%d at 0x%04x irq %d on ata-pci%d\n",
	       lun, iobase_1, isa_apic_irq(irq1), unit);
    }
    lun = 1;
    if (iobase_2 && ata_probe(iobase_2, altiobase_2, bmaddr_2, dev, &lun)) {
	int rid;
	void *ih;

	scp = atadevices[lun];
	scp->chiptype = type;
	if (iobase_2 == IO_WD2) {
#ifdef __alpha__
	    alpha_platform_setup_ide_intr(1, ataintr, scp);
#else
	    rid = 1;
	    bus_set_resource(dev, SYS_RES_IRQ, rid, irq2, 1);
	    if (!(irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					   RF_SHAREABLE | RF_ACTIVE)))
		printf("ata_pciattach: Unable to alloc interrupt\n");
#endif
	} else {
	    rid = 0;
	    if (irq1 != irq2 || irq == NULL) {
	  	if (!(irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					       RF_SHAREABLE | RF_ACTIVE)))
		    printf("ata_pciattach: Unable to alloc interrupt\n");
	    }
	}
	    if (irq)
		bus_setup_intr(dev, irq, INTR_TYPE_BIO, ataintr, scp, &ih);
	printf("ata%d at 0x%04x irq %d on ata-pci%d\n",
	       lun, iobase_2, isa_apic_irq(irq2), unit);
    }
    return 0;
}

int32_t
ata_find_dev(device_t dev, int32_t type)
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
	    pci_get_device(child) == ((type & 0xffff0000)>>16)) {
	    free(children, M_TEMP);
	    return 1;
	}
    }
    free(children, M_TEMP);
    return 0;
}

static device_method_t ata_pci_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	ata_pciprobe),
    DEVMETHOD(device_attach,	ata_pciattach),
    { 0, 0 }
};

static driver_t ata_pci_driver = {
    "ata-pci",
    ata_pci_methods,
    sizeof(int),
};

DRIVER_MODULE(ata, pci, ata_pci_driver, ata_devclass, 0, 0);
#endif

static int32_t
ata_probe(int32_t ioaddr, int32_t altioaddr, int32_t bmaddr,
	  device_t dev, int32_t *unit)
{
    struct ata_softc *scp;
    int32_t lun, mask = 0;
    u_int8_t status0, status1;

    if (atanlun > MAXATA) {
	printf("ata: unit out of range(%d)\n", atanlun);
	return 0;
    }

    /* check if this is located at one of the std addresses */
    if (ioaddr == IO_WD1)
	lun = 0;
    else if (ioaddr == IO_WD2)
	lun = 1;
    else
	lun = atanlun++;

    if ((scp = atadevices[lun])) {
	ata_printf(scp, -1, "unit already attached\n");
	return 0;
    }
    scp = malloc(sizeof(struct ata_softc), M_ATA, M_NOWAIT);
    if (scp == NULL) {
	ata_printf(scp, -1, "failed to allocate driver storage\n");
	return 0;
    }
    bzero(scp, sizeof(struct ata_softc));

    scp->ioaddr = ioaddr; 
    scp->altioaddr = altioaddr;
    scp->bmaddr = bmaddr;
    scp->lun = lun;
    scp->unit = *unit;
    scp->active = ATA_IDLE;

    if (bootverbose)
	ata_printf(scp, -1, "iobase=0x%04x altiobase=0x%04x bmaddr=0x%04x\n", 
		   scp->ioaddr, scp->altioaddr, scp->bmaddr);

    /* do we have any signs of ATA/ATAPI HW being present ? */
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(1);
    status0 = inb(scp->ioaddr + ATA_STATUS);
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
    DELAY(1);	
    status1 = inb(scp->ioaddr + ATA_STATUS);
    if ((status0 & 0xf8) != 0xf8)
	mask |= 0x01;
    if ((status1 & 0xf8) != 0xf8)
	mask |= 0x02;
    if (bootverbose)
	ata_printf(scp, -1, "mask=%02x status0=%02x status1=%02x\n", 
		   mask, status0, status1);
    if (!mask) {
	free(scp, M_DEVBUF);
	return 0;
    } 
    ata_reset(scp, &mask);
    if (!mask) {
	free(scp, M_DEVBUF);
	return 0;
    }
    /* 
     * OK, we have at least one device on the chain,
     * check for ATAPI signatures, if none check if its
     * a good old ATA device.
     */ 
    outb(scp->ioaddr + ATA_DRIVE, (ATA_D_IBM | ATA_MASTER));
    DELAY(1);
    if (inb(scp->ioaddr + ATA_CYL_LSB) == ATAPI_MAGIC_LSB &&
	inb(scp->ioaddr + ATA_CYL_MSB) == ATAPI_MAGIC_MSB) {
	scp->devices |= ATA_ATAPI_MASTER;
    }
    outb(scp->ioaddr + ATA_DRIVE, (ATA_D_IBM | ATA_SLAVE));
    DELAY(1);
    if (inb(scp->ioaddr + ATA_CYL_LSB) == ATAPI_MAGIC_LSB &&
	inb(scp->ioaddr + ATA_CYL_MSB) == ATAPI_MAGIC_MSB) {
	scp->devices |= ATA_ATAPI_SLAVE;
    }
    if (status0 != 0x00 && !(scp->devices & ATA_ATAPI_MASTER)) {
	outb(scp->ioaddr + ATA_DRIVE, (ATA_D_IBM | ATA_MASTER));
	DELAY(1);
	outb(scp->ioaddr + ATA_ERROR, 0x58);
	outb(scp->ioaddr + ATA_CYL_LSB, 0xa5);
	if (inb(scp->ioaddr + ATA_ERROR) != 0x58 &&
	    inb(scp->ioaddr + ATA_CYL_LSB) == 0xa5) {
	    scp->devices |= ATA_ATA_MASTER;
	}
    }
    if (status1 != 0x00 && !(scp->devices & ATA_ATAPI_SLAVE)) {
	outb(scp->ioaddr + ATA_DRIVE, (ATA_D_IBM | ATA_SLAVE));
	DELAY(1);
	outb(scp->ioaddr + ATA_ERROR, 0x58);
	outb(scp->ioaddr + ATA_CYL_LSB, 0xa5);
	if (inb(scp->ioaddr + ATA_ERROR) != 0x58 &&
	    inb(scp->ioaddr + ATA_CYL_LSB) == 0xa5) {
	    scp->devices |= ATA_ATA_SLAVE;
	}
    }
    if (bootverbose)
	ata_printf(scp, -1, "devices = 0x%x\n", scp->devices);
    if (!scp->devices) {
	free(scp, M_DEVBUF);
	return 0;
    }
    TAILQ_INIT(&scp->ata_queue);
    TAILQ_INIT(&scp->atapi_queue);
    *unit = scp->lun;
    scp->dev = dev;
    atadevices[scp->lun] = scp;

    /* register callback for when interrupts are enabled */
    if (!ata_attach_hook) {
	if (!(ata_attach_hook = (struct intr_config_hook *)
				malloc(sizeof(struct intr_config_hook),
				M_TEMP, M_NOWAIT))) {
            ata_printf(scp, -1, "ERROR malloc attach_hook failed\n");
            return 0;
	}
	bzero(ata_attach_hook, sizeof(struct intr_config_hook));

	ata_attach_hook->ich_func = ata_attach;
	if (config_intrhook_establish(ata_attach_hook) != 0) {
            ata_printf(scp, -1, "config_intrhook_establish failed\n");
            free(ata_attach_hook, M_TEMP);
	}
    }
#if NAPM > 0
    scp->resume_hook.ah_fun = (void *)ata_reinit;
    scp->resume_hook.ah_arg = scp;
    scp->resume_hook.ah_name = "ATA driver";
    scp->resume_hook.ah_order = APM_MID_ORDER;
    apm_hook_establish(APM_HOOK_RESUME, &scp->resume_hook);
#endif
    return ATA_IOSIZE;
}

void 
ata_attach(void *dummy)
{
    int32_t ctlr;

    /*
     * run through atadevices[] and look for real ATA & ATAPI devices
     * using the hints we found in the early probe to avoid probing
     * of non-exsistent devices and thereby long delays
     */
    for (ctlr=0; ctlr<MAXATA; ctlr++) {
	if (!atadevices[ctlr]) continue;
	if (atadevices[ctlr]->devices & ATA_ATA_SLAVE)
	    if (ata_getparam(atadevices[ctlr], ATA_SLAVE, ATA_C_ATA_IDENTIFY))
		atadevices[ctlr]->devices &= ~ATA_ATA_SLAVE;
	if (atadevices[ctlr]->devices & ATA_ATAPI_SLAVE)
	    if (ata_getparam(atadevices[ctlr], ATA_SLAVE, ATA_C_ATAPI_IDENTIFY))
		atadevices[ctlr]->devices &= ~ATA_ATAPI_SLAVE;
	if (atadevices[ctlr]->devices & ATA_ATA_MASTER)
	    if (ata_getparam(atadevices[ctlr], ATA_MASTER, ATA_C_ATA_IDENTIFY))
		atadevices[ctlr]->devices &= ~ATA_ATA_MASTER;
	if (atadevices[ctlr]->devices & ATA_ATAPI_MASTER)
	    if (ata_getparam(atadevices[ctlr], ATA_MASTER,ATA_C_ATAPI_IDENTIFY))
		atadevices[ctlr]->devices &= ~ATA_ATAPI_MASTER;
    }

#if NATADISK > 0
    /* now we know whats there, do the real attach, first the ATA disks */
    for (ctlr=0; ctlr<MAXATA; ctlr++) {
	if (!atadevices[ctlr]) continue;
	if (atadevices[ctlr]->devices & ATA_ATA_MASTER)
	    ad_attach(atadevices[ctlr], ATA_MASTER);
	if (atadevices[ctlr]->devices & ATA_ATA_SLAVE)
	    ad_attach(atadevices[ctlr], ATA_SLAVE);
    }
#endif
#if NATAPICD > 0 || NATAPIFD > 0 || NATAPIST > 0
    /* then the atapi devices */
    for (ctlr=0; ctlr<MAXATA; ctlr++) {
	if (!atadevices[ctlr]) continue;
	if (atadevices[ctlr]->devices & ATA_ATAPI_MASTER)
	    atapi_attach(atadevices[ctlr], ATA_MASTER);
	if (atadevices[ctlr]->devices & ATA_ATAPI_SLAVE)
	    atapi_attach(atadevices[ctlr], ATA_SLAVE);
    }
#endif
    if (ata_attach_hook) {
	config_intrhook_disestablish(ata_attach_hook);
	free(ata_attach_hook, M_ATA);
	ata_attach_hook = NULL;
    }
}

static int32_t
ata_getparam(struct ata_softc *scp, int32_t device, u_int8_t command)
{
    struct ata_params *ata_parm;
    int8_t buffer[DEV_BSIZE];
    int retry = 0;

    /* select drive */
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | device);
    DELAY(1);

    /* enable interrupts */
    outb(scp->altioaddr, ATA_A_4BIT);
    DELAY(1);

    /* apparently some devices needs this repeated */
    do {
	if (ata_command(scp, device, command, 0, 0, 0, 0, 0, ATA_WAIT_INTR)) {
	    ata_printf(scp, device, "identify failed\n");
	    return -1;
	}
	if (retry++ > 4) {
	    ata_printf(scp, device, "drive wont come ready after identify\n");
	    return -1;
	}
    } while (ata_wait(scp, device, 
		      ((command == ATA_C_ATAPI_IDENTIFY) ?
			ATA_S_DRQ : (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))));

    insw(scp->ioaddr + ATA_DATA, buffer, sizeof(buffer)/sizeof(int16_t));
    ata_parm = malloc(sizeof(struct ata_params), M_ATA, M_NOWAIT);
    if (!ata_parm) {
	ata_printf(scp, device, "malloc for ata_param failed\n");
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
ataintr(void *data)
{
    struct ata_softc *scp = (struct ata_softc *)data;

    /* check if this interrupt is for us (shared PCI interrupts) */
    /* if DMA active look at the dmastatus */
    if ((scp->flags & ATA_DMA_ACTIVE) &&
	!(ata_dmastatus(scp) & ATA_BMSTAT_INTERRUPT))
	    return;

    /* if drive is busy it didn't interrupt */
    if (((scp->status = inb(scp->ioaddr + ATA_STATUS))&ATA_S_BUSY)==ATA_S_BUSY)
	return;

    /* find & call the responsible driver to process this interrupt */
    switch (scp->active) {
#if NATADISK > 0
    case ATA_ACTIVE_ATA:
	if (!scp->running)
	    return;
	if (ad_interrupt(scp->running) == ATA_OP_CONTINUES)
	    return;
	break;
#endif
#if NATAPICD > 0 || NATAPIFD > 0 || NATAPIST > 0
    case ATA_ACTIVE_ATAPI:
	if (!scp->running)
	    return;
	if (atapi_interrupt(scp->running) == ATA_OP_CONTINUES)
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

    default:
    case ATA_IDLE:
#ifdef ATA_DEBUG
	{
    	    static int32_t intr_count = 0;
	    if (intr_count++ < 10)
		ata_printf(scp, -1, "unwanted interrupt %d status = %02x\n", 
			   intr_count, scp->status);
	}
#endif
    }
    scp->active = ATA_IDLE;
    scp->running = NULL;
    ata_start(scp);
}

void
ata_start(struct ata_softc *scp)
{
    struct ad_request *ad_request; 
    struct atapi_request *atapi_request;

    if (scp->active != ATA_IDLE)
	return;
    scp->active = ATA_ACTIVE;

#if NATADISK > 0
    /* find & call the responsible driver if anything on the ATA queue */
    if ((ad_request = TAILQ_FIRST(&scp->ata_queue))) {
	TAILQ_REMOVE(&scp->ata_queue, ad_request, chain);
	scp->active = ATA_ACTIVE_ATA;
	scp->running = ad_request;
	ad_transfer(ad_request);
	return;
    }
#endif
#if NATAPICD > 0 || NATAPIFD > 0 || NATAPIST > 0
    /*
     * find & call the responsible driver if anything on the ATAPI queue.
     * check for device busy by polling the DSC bit, if busy, check
     * for requests to the other device on the channel (if any).
     * if the other device is an ATA disk it already had its chance above.
     * if no request can be served, timeout a call to ata_start.
     */
    if ((atapi_request = TAILQ_FIRST(&scp->atapi_queue))) {
	struct atapi_softc *atp = atapi_request->device;
	static int32_t interval = 1;

	if (atp->flags & ATAPI_F_DSC_USED) {
	    outb(atp->controller->ioaddr + ATA_DRIVE, ATA_D_IBM | atp->unit);
	    DELAY(1);
	    if (!(inb(atp->controller->ioaddr + ATA_STATUS) & ATA_S_DSC)) {
		while ((atapi_request = TAILQ_NEXT(atapi_request, chain))) {
		    if (atapi_request->device->unit != atp->unit) {
			struct atapi_softc *tmp = atapi_request->device;

			outb(tmp->controller->ioaddr + ATA_DRIVE, 
			     ATA_D_IBM | tmp->unit);
			DELAY(1);
			if (!inb(tmp->controller->ioaddr+ATA_STATUS)&ATA_S_DSC)
			    atapi_request = NULL;
			break;
		    }
	        }
	    }
	    if (!atapi_request) {
		timeout((timeout_t *)ata_start, atp->controller, interval++);
		return;
	    }
	    else
		interval = 1;
	}
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
ata_reset(struct ata_softc *scp, int32_t *mask)
{
    int32_t timeout;  
    int8_t status0, status1;

    /* reset channel */
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(1);
    inb(scp->ioaddr + ATA_STATUS);
    outb(scp->altioaddr, ATA_A_IDS | ATA_A_RESET);
    DELAY(10000); 
    outb(scp->altioaddr, ATA_A_IDS);
    DELAY(10000);
    inb(scp->ioaddr + ATA_ERROR);
    DELAY(3000);

    /* wait for BUSY to go inactive */
    for (timeout = 0; timeout < 310000; timeout++) {
	outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
	DELAY(1);
	status0 = inb(scp->ioaddr + ATA_STATUS);
	outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
	DELAY(1);
	status1 = inb(scp->ioaddr + ATA_STATUS);
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
    DELAY(1);
    outb(scp->altioaddr, ATA_A_4BIT);
    if (status0 & ATA_S_BUSY)
	*mask &= ~0x01;
    if (status1 & ATA_S_BUSY)
	*mask &= ~0x02;
    if (bootverbose)
	ata_printf(scp, -1, "mask=%02x status0=%02x status1=%02x\n", 
		   *mask, status0, status1);
}

int32_t
ata_reinit(struct ata_softc *scp)
{
    int32_t mask = 0, omask;

    scp->active = ATA_REINITING;
    scp->running = NULL;
    ata_printf(scp, -1, "resetting devices .. ");
    if (scp->devices & (ATA_ATA_MASTER | ATA_ATAPI_MASTER))
	mask |= 0x01;
    if (scp->devices & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE))
	mask |= 0x02;
    omask = mask;
    ata_reset(scp, &mask);
    if (omask != mask)
	printf(" device dissapeared! %d ", omask & ~mask);

#if NATADISK > 0
    if (scp->devices & (ATA_ATA_MASTER) && scp->dev_softc[0])
	ad_reinit((struct ad_softc *)scp->dev_softc[0]);
    if (scp->devices & (ATA_ATA_SLAVE) && scp->dev_softc[1])
	ad_reinit((struct ad_softc *)scp->dev_softc[1]);
#endif
#if NATAPICD > 0 || NATAPIFD > 0 || NATAPIST > 0
    if (scp->devices & (ATA_ATAPI_MASTER) && scp->dev_softc[0])
	atapi_reinit((struct atapi_softc *)scp->dev_softc[0]);
    if (scp->devices & (ATA_ATAPI_SLAVE) && scp->dev_softc[1])
	atapi_reinit((struct atapi_softc *)scp->dev_softc[1]);
#endif
    printf("done\n");
    scp->active = ATA_IDLE;
    ata_start(scp);
    return 0;
}

int32_t
ata_wait(struct ata_softc *scp, int32_t device, u_int8_t mask)
{
    u_int32_t timeout = 0;
    
    DELAY(1);
    while (timeout < 5000000) {	/* timeout 5 secs */
	scp->status = inb(scp->ioaddr + ATA_STATUS);

	/* if drive fails status, reselect the drive just to be sure */
	if (scp->status == 0xff) {
	    ata_printf(scp, device, "no status, reselecting device\n");
	    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | device);
	    DELAY(1);
	    scp->status = inb(scp->ioaddr + ATA_STATUS);
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
	scp->error = inb(scp->ioaddr + ATA_ERROR);
    if (timeout >= 5000000)	 
	return -1;	    
    if (!mask)	   
	return (scp->status & ATA_S_ERROR);	 
    
    /* Wait 50 msec for bits wanted. */	   
    timeout = 5000;
    while (timeout--) {	  
	scp->status = inb(scp->ioaddr + ATA_STATUS);
	if ((scp->status & mask) == mask) {
	    if (scp->status & ATA_S_ERROR)
		scp->error = inb(scp->ioaddr + ATA_ERROR);
	    return (scp->status & ATA_S_ERROR);	      
	}
	DELAY (10);	   
    }	  
    return -1;	    
}   

int32_t
ata_command(struct ata_softc *scp, int32_t device, u_int32_t command,
	   u_int32_t cylinder, u_int32_t head, u_int32_t sector, 
	   u_int32_t count, u_int32_t feature, int32_t flags)
{
#ifdef ATA_DEBUG
    ata_printf(scp, device, "ata_command: addr=%04x, cmd=%02x, "
	       "c=%d, h=%d, s=%d, count=%d, flags=%02x\n",
	       scp->ioaddr, command, cylinder, head, sector, count, flags);
#endif

    /* ready to issue command ? */
    if (ata_wait(scp, device, 0) < 0) { 
	ata_printf(scp, device, 
		   "timeout waiting to give command=%02x s=%02x e=%02x\n",
		   command, scp->status, scp->error);
	return -1;
    }
    outb(scp->ioaddr + ATA_FEATURE, feature);
    outb(scp->ioaddr + ATA_CYL_LSB, cylinder);
    outb(scp->ioaddr + ATA_CYL_MSB, cylinder >> 8);
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | device | head);
    outb(scp->ioaddr + ATA_SECTOR, sector);
    outb(scp->ioaddr + ATA_COUNT, count);

    switch (flags) {
    case ATA_WAIT_INTR:
	if (scp->active != ATA_IDLE)
	    ata_printf(scp, device, "WARNING: WAIT_INTR active=%s\n",
		       active2str(scp->active));
	scp->active = ATA_WAIT_INTR;
	asleep((caddr_t)scp, PRIBIO, "atacmd", 10 * hz);
	outb(scp->ioaddr + ATA_CMD, command);
	if (await(PRIBIO, 10 * hz)) {
	    ata_printf(scp, device, "ata_command: timeout waiting for intr\n");
	    scp->active = ATA_IDLE;
	    return -1;
	}
	break;
    
    case ATA_WAIT_READY:
	if (scp->active != ATA_IDLE && scp->active != ATA_REINITING)
	    ata_printf(scp, device, "WARNING: WAIT_READY active=%s\n",
		       active2str(scp->active));
	if (scp->active != ATA_REINITING)
	    scp->active = ATA_WAIT_READY;
	outb(scp->ioaddr + ATA_CMD, command);
	if (ata_wait(scp, device, ATA_S_READY) < 0) { 
	    ata_printf(scp, device, 
		       "timeout waiting for command=%02x s=%02x e=%02x\n",
		       command, scp->status, scp->error);
	    if (scp->active != ATA_REINITING)
		scp->active = ATA_IDLE;
	    return -1;
	}
	if (scp->active != ATA_REINITING)
	    scp->active = ATA_IDLE;
	break;

    case ATA_IMMEDIATE:
	outb(scp->ioaddr + ATA_CMD, command);
	break;

    default:
	ata_printf(scp, device, "DANGER: illegal interrupt flag=%s\n",
		   active2str(flags));
    }
    return 0;
}

int8_t *
ata_mode2str(int32_t mode)
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
    case ATA_DMA: return "BIOSDMA";
    default: return "???";
    }
}

int8_t
ata_pio2mode(int32_t pio)
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

static int8_t *
active2str(int32_t active)
{
    static char buf[8];

    switch (active) {
    case ATA_IDLE:
	return("ATA_IDLE");
    case ATA_WAIT_INTR:
	return("ATA_WAIT_INTR");
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

int32_t
ata_pmode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_64_70) {
	if (ap->apiomodes & 2) return 4;
	if (ap->apiomodes & 1) return 3;
    }	
    if (ap->opiomode == 2) return 2;
    if (ap->opiomode == 1) return 1;
    if (ap->opiomode == 0) return 0;
    return -1; 
} 

int32_t
ata_wmode(struct ata_params *ap)
{
    if (ap->wdmamodes & 4) return 2;
    if (ap->wdmamodes & 2) return 1;
    if (ap->wdmamodes & 1) return 0;
    return -1;
}

int32_t
ata_umode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_88) {
	if (ap->udmamodes & 0x10) return (ap->cblid ? 4 : 2);
	if (ap->udmamodes & 0x08) return (ap->cblid ? 3 : 2);
	if (ap->udmamodes & 0x04) return 2;
	if (ap->udmamodes & 0x02) return 1;
	if (ap->udmamodes & 0x01) return 0;
    }
    return -1;
}

static void
bswap(int8_t *buf, int32_t len) 
{
    u_int16_t *p = (u_int16_t*)(buf + len);

    while (--p >= (u_int16_t*)buf)
	*p = ntohs(*p);
} 

static void
btrim(int8_t *buf, int32_t len)
{ 
    int8_t *p;

    for (p = buf; p < buf+len; ++p) 
	if (!*p)
	    *p = ' ';
    for (p = buf + len - 1; p >= buf && *p == ' '; --p)
	*p = 0;
}

static void
bpack(int8_t *src, int8_t *dst, int32_t len)
{
    int32_t i, j, blank;

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

int32_t
ata_printf(struct ata_softc *scp, int32_t device, const char * fmt, ...)
{
    va_list ap;
    int ret;

    if (device == -1)
	ret = printf("ata%d: ", scp->lun);
    else
	ret = printf("ata%d-%s: ", scp->lun,
		     (device == ATA_MASTER) ? "master" : "slave");
    va_start(ap, fmt);
    ret += vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

static char ata_conf[1024];
 
static void
ata_change_mode(struct ata_softc *scp, int32_t device, int32_t mode)
{
    int32_t s = splbio();

    while (scp->active != ATA_IDLE)
	tsleep((caddr_t)&s, PRIBIO, "atachm", hz/4);
    scp->active = ATA_REINITING;
    ata_dmainit(scp, device, ata_pmode(ATA_PARAM(scp, device)), 
		mode < ATA_DMA ?  -1 : ata_wmode(ATA_PARAM(scp, device)),
		mode < ATA_DMA ?  -1 : ata_umode(ATA_PARAM(scp, device)));
    scp->active = ATA_IDLE;
    ata_start(scp);
    splx(s);
}

static int
sysctl_hw_ata SYSCTL_HANDLER_ARGS
{
    int error, i;

    /* readout internal state */
    bzero(ata_conf, sizeof(ata_conf));
    for (i = 0; i < (atanlun << 1); i++) {
	if (!atadevices[i >> 1] || !atadevices[ i >> 1]->dev_softc[i & 1])
	    strcat(ata_conf, "---,");
	else if (atadevices[i >> 1]->mode[i & 1] >= ATA_DMA)
	    strcat(ata_conf, "dma,");
	else
	    strcat(ata_conf, "pio,");
    }
    error = sysctl_handle_string(oidp, ata_conf, sizeof(ata_conf), req);   
    if (error == 0 && req->newptr != NULL) {
	char *ptr = ata_conf;

        /* update internal state */
	i = 0;
        while (*ptr) {
	    if (!strncmp(ptr, "pio", 3) || !strncmp(ptr, "PIO", 3)) {
		if (atadevices[i >> 1]->dev_softc[i & 1] &&
		    atadevices[i >>1 ]->mode[i & 1] >= ATA_DMA)
		    ata_change_mode(atadevices[i >> 1], 
				    (i & 1) ? ATA_SLAVE : ATA_MASTER, ATA_PIO);
	    }
	    else if (!strncmp(ptr, "dma", 3) || !strncmp(ptr, "DMA", 3)) {
		if (atadevices[i >> 1]->dev_softc[i & 1] &&
		    atadevices[i >> 1]->mode[i & 1] < ATA_DMA)
		    ata_change_mode(atadevices[i >> 1], 
				    (i & 1) ? ATA_SLAVE : ATA_MASTER, ATA_DMA);
	    }
	    else if (strncmp(ptr, "---", 3))
		break;
	    ptr+=3;
	    if (*ptr++ != ',' || ++i > (atanlun << 1))
		break; 
        }
    }
    return error;
}
 
SYSCTL_PROC(_hw, OID_AUTO, atamodes, CTLTYPE_STRING | CTLFLAG_RW,
            0, sizeof(ata_conf), sysctl_hw_ata, "A", "");
