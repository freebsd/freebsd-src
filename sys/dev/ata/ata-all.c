/*-
 * Copyright (c) 1998,1999 Søren Schmidt
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
 *  $Id: ata-all.c,v 1.4 1999/03/01 21:03:15 sos Exp sos $
 */

#include "ata.h"
#if NATA > 0
#include "pci.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/atapi-all.h>

/* misc defines */
#define UNIT(dev) (dev>>3 & 0x1f)   		/* assume 8 minor # per unit */

/* prototypes */
void ataintr(int32_t);
static int32_t ata_isaprobe(struct isa_device *);
static int32_t ata_isaattach(struct isa_device *);
static const char *ata_pciprobe(pcici_t, pcidi_t);
static void ata_pciattach(pcici_t, int32_t);
static int32_t ata_probe(int32_t, int32_t, int32_t *);
static int32_t ata_attach(int32_t);
static void promise_intr(int32_t);
static int32_t ata_reset(struct ata_softc *);
static int32_t ata_device_attach(struct ata_softc *, int32_t);
static int32_t atapi_device_attach(struct ata_softc *, int32_t);
static void bswap(int8_t *, int32_t);
static void btrim(int8_t *, int32_t);

static int32_t atanlun, sysctrl = 0;
struct ata_softc *atadevices[MAXATA];
struct isa_driver atadriver = { ata_isaprobe, ata_isaattach, "ata" };

static int32_t
ata_isaprobe(struct isa_device *devp)
{
    int32_t ctlr, res;
    
    for (ctlr = 0; ctlr < atanlun; ctlr++) {
	if (atadevices[ctlr]->ioaddr == devp->id_iobase) {
	    printf("ata-isa%d: already registered as ata%d\n", 
		   devp->id_unit, ctlr);
	    return 0;
	}
    }
    res=ata_probe(devp->id_iobase, devp->id_iobase+ATA_ALTPORT, &devp->id_unit);
    if (res)
	devp->id_intr = ataintr;
    return res;
}

static int32_t
ata_isaattach(struct isa_device *devp)
{
    return ata_attach(devp->id_unit);
}

static u_long ata_pcicount;
static struct pci_device ata_pcidevice = {
    "ata-pci", ata_pciprobe, ata_pciattach, &ata_pcicount, 0
};

DATA_SET(pcidevice_set, ata_pcidevice);

static const char *
ata_pciprobe(pcici_t tag, pcidi_t type)
{
    u_int32_t data;

    data = pci_conf_read(tag, PCI_CLASS_REG);
    if ((data & PCI_CLASS_MASK) == PCI_CLASS_MASS_STORAGE &&
	((data & PCI_SUBCLASS_MASK) == 0x00010000 ||
	((data & PCI_SUBCLASS_MASK) == 0x00040000))) {
	switch (type) {
	case 0x71118086:
	    return "Intel PIIX4 IDE controller";
	case 0x70108086:
	    return "Intel PIIX3 IDE controller";
	case 0x12308086:
	    return "Intel PIIX IDE controller";
	case 0x4d33105a:
	    return "Promise Ultra/33 IDE controller";
	case 0x05711106:
	    return "VIA Apollo IDE controller";
	case 0x01021078:
	    return "Cyrix 5530 IDE controller";
	case 0x522910b9:
	    return "Acer Aladdin IV/V IDE controller";
	default:
	    return ("Unknown PCI IDE controller");
	}
    }
    return NULL;
}

static void
ata_pciattach(pcici_t tag, int32_t unit)
{
    pcidi_t type, class, cmd;
    int32_t iobase_1, iobase_2, altiobase_1, altiobase_2, irq1, irq2;
    int32_t lun;

    /* set up vendor-specific stuff */
    type = pci_conf_read(tag, PCI_ID_REG);
    class = pci_conf_read(tag, PCI_CLASS_REG);
    cmd = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);

#ifdef ATA_DEBUG
    printf("ata: type=%08x class=%08x cmd=%08x\n", type, class, cmd);
#endif

    switch (type) {
    case 0x71118086:
    case 0x70108086:
    case 0x12308086: /* Intel PIIX, PIIX3, PIIX4 */
    	break;

    case 0x05711106: /* VIA Apollo chipset family */
    	break;

    case 0x4d33105a: /* Promise controllers */
    	break;

    case 0x01021078: /* Cyrix 5530 */
    	break;

    case 0x522910B9: /* Acer Aladdin IV/V (M5229) */
    	break;
    default:
    	/* everybody else */
    	break;
    }

    if (type == 0x4d33105a) { /* the Promise is special */
	iobase_1 = pci_conf_read(tag, 0x10) & 0xfffc;
	altiobase_1 = pci_conf_read(tag, 0x14) & 0xfffc;
	iobase_2 = pci_conf_read(tag, 0x18) & 0xfffc;
	altiobase_2 = pci_conf_read(tag, 0x1c) & 0xfffc;
	irq1 = irq2 = pci_conf_read(tag, PCI_INTERRUPT_REG) & 0xff;
	sysctrl = (pci_conf_read(tag, 0x20) & 0xfffc) + 0x1c;
    }
    else {
	if ((class & 0x100) == 0) {
		iobase_1 = IO_WD1;
		altiobase_1 = iobase_1 + ATA_ALTPORT;
		irq1 = 14;
	} 
	else {
		iobase_1 = pci_conf_read(tag, 0x10) & 0xfffc;
		altiobase_1 = pci_conf_read(tag, 0x14) & 0xfffc;
		irq1 = pci_conf_read(tag, PCI_INTERRUPT_REG) & 0xff;
	}
	if ((class & 0x400) == 0) {
		iobase_2 = IO_WD2;
		altiobase_2 = iobase_2 + ATA_ALTPORT;
		irq2 = 15;
	}
	else {
		iobase_2 = pci_conf_read(tag, 0x18) & 0xfffc;
		altiobase_2 = pci_conf_read(tag, 0x1c) & 0xfffc;
		irq2 = pci_conf_read(tag, PCI_INTERRUPT_REG) & 0xff;
	}
    }
	
    /* now probe the addresse found for "real" ATA/ATAPI hardware */
    if (ata_probe(iobase_1, altiobase_1, &lun)) {
	if (iobase_1 == IO_WD1)
	    register_intr(irq1, 0, 0, (inthand2_t *)ataintr, &bio_imask, lun);
	else {
	    if (sysctrl)
	        pci_map_int(tag, promise_intr, (void *)lun, &bio_imask);
	    else
	        pci_map_int(tag, ataintr, (void *)lun, &bio_imask);
	}
	printf("ata%d at 0x%04x irq %d on ata-pci%d\n",
	       lun, iobase_1, irq1, unit);
	ata_attach(lun);
    }
    if (ata_probe(iobase_2, altiobase_2, &lun)) {
	if (iobase_2 == IO_WD2)
	    register_intr(irq2, 0, 0, (inthand2_t *)ataintr, &bio_imask, lun);
	else {
	    if (!sysctrl)
	        pci_map_int(tag, ataintr, (void *) lun, &bio_imask);
	}
	printf("ata%d at 0x%04x irq %d on ata-pci%d\n",
	       lun, iobase_2, irq2, unit);
	ata_attach(lun);
    }
}

static void
promise_intr(int32_t unit)
{
    if (inl(sysctrl) & 0x00000400)
	ataintr(unit);
    if (inl(sysctrl) & 0x00004000)
	ataintr(unit+1);
}

static int32_t
ata_probe(int32_t ioaddr, int32_t altioaddr, int32_t *unit)
{
    struct ata_softc *scp = atadevices[atanlun];
    u_int8_t status0, status1;
    int32_t mask = 0;
    int32_t timeout;  

    if (atanlun > MAXATA) {
	printf("ata: unit of of range(%d)\n", atanlun);
	return(0);
    }
    if (scp) {
	printf("ata%d: unit already attached\n", atanlun);
	return(0);
    }
    scp = malloc(sizeof(struct ata_softc), M_DEVBUF, M_NOWAIT);
    if (scp == NULL) {
	printf("ata%d: failed to allocate driver storage\n", atanlun);
	return(0);
    }
    bzero(scp, sizeof(struct ata_softc));

    scp->unit = atanlun;
    scp->ioaddr = ioaddr; 
    scp->altioaddr = altioaddr;

#ifdef ATA_DEBUG
    printf("ata%d: iobase=0x%04x altiobase=0x%04x\n", 
	   atanlun, scp->ioaddr, scp->altioaddr);
#endif

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
#ifdef ATA_DEBUG
    printf("ata%d: mask=%02x status0=%02x status1=%02x\n", 
	   atanlun, mask, status0, status1);
#endif
    if (!mask) {
	free(scp, M_DEVBUF);
        return 0;
    } 
    /* assert reset for devices and wait for completition */
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
    DELAY(1);
    outb(scp->altioaddr, ATA_A_IDS | ATA_A_RESET);
    DELAY(1000); 
    outb(scp->altioaddr, ATA_A_IDS);
    DELAY(1000);
    inb(scp->ioaddr + ATA_ERROR);
    outb(scp->altioaddr, ATA_A_4BIT);
    DELAY(1);   

    /* wait for BUSY to go inactive */
    for (timeout = 0; timeout < 30000*10; timeout++) {
        outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
        DELAY(1);
        status0 = inb(scp->ioaddr + ATA_STATUS);
        outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
        DELAY(1);
        status1 = inb(scp->ioaddr + ATA_STATUS);
        if (mask == 0x01)      /* wait for master only */
            if (!(status0 & ATA_S_BSY)) 
            	break;
        if (mask == 0x02)      /* wait for slave only */
            if (!(status1 & ATA_S_BSY))
            	break;
        if (mask == 0x03)      /* wait for both master & slave */
            if (!(status0 & ATA_S_BSY) && !(status1 & ATA_S_BSY))
            	break;
        DELAY(100);
    }   
    if (status0 & ATA_S_BSY)
        mask &= ~0x01;
    if (status1 & ATA_S_BSY)
        mask &= ~0x02;
#ifdef ATA_DEBUG
    printf("ata%d: mask=%02x status0=%02x status1=%02x\n", 
	   atanlun, mask, status0, status1);
#endif
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
#ifdef ATA_DEBUG
    printf("ata%d: devices = 0x%x\n", atanlun, scp->devices);
#endif
    if (!(scp->devices & (ATA_ATA_MASTER|ATA_ATAPI_MASTER)))
	scp->flags |= ATA_F_SLAVE_ONLY;
    if (!scp->devices) {
	free(scp, M_DEVBUF);
	return 0;
    }
    bufq_init(&scp->ata_queue);
    TAILQ_INIT(&scp->atapi_queue);
    *unit = atanlun;
    atadevices[atanlun++] = scp;
    return ATA_IOSIZE;
}

static int32_t
ata_attach(int32_t unit)
{
    struct ata_softc *scp;
    
    if (unit > atanlun)
	return 0;

    scp = atadevices[unit];

    if (scp->devices & ATA_ATA_MASTER)
        if (ata_device_attach(scp, ATA_MASTER))
	    scp->devices &= ~ATA_ATA_MASTER;
    if (scp->devices & ATA_ATA_SLAVE)
	if (ata_device_attach(scp, ATA_SLAVE))
	    scp->devices &= ~ATA_ATA_SLAVE;
    if (scp->devices & ATA_ATAPI_MASTER)
	if (atapi_device_attach(scp, ATA_MASTER))
	    scp->devices &= ~ATA_ATAPI_MASTER;
    if (scp->devices & ATA_ATAPI_SLAVE)
	if (atapi_device_attach(scp, ATA_SLAVE))
	    scp->devices &= ~ATA_ATAPI_SLAVE;
    return scp->devices;
}

void
ataintr(int32_t unit)
{
    struct ata_softc *scp;
    struct atapi_request *atapi_request;
    struct buf *ata_request; 

    static int32_t intcount = 0;

#ifdef ATA_DEBUG
    printf("ataintr: entered unit=%d\n", unit);
#endif
    if (unit < 0 || unit > atanlun) {
	printf("ataintr: unit %d unusable\n", unit);
	return;
    }

    scp = atadevices[unit];

    /* find & call the responsible driver to process this interrupt */
    switch (scp->active) {
    case ATA_IDLE:
	if (intcount++ < 5)
	    printf("ata%d: unwanted interrupt\n", unit);
	break;

    case ATA_ACTIVE_ATA:
    	if ((ata_request = bufq_first(&scp->ata_queue)))
            ad_interrupt(ata_request);
	break;

    case ATA_ACTIVE_ATAPI:
        if ((atapi_request = TAILQ_FIRST(&scp->atapi_queue)))
	    atapi_interrupt(atapi_request);
	break;

    case ATA_IGNORE_INTR:
	scp->active = ATA_IDLE;
	break;
    }
}

void
ata_start(struct ata_softc *scp)
{
    struct buf *ata_request; 
    struct atapi_request *atapi_request;

#ifdef ATA_DEBUG
    printf("ata_start: entered\n");
#endif
    if (scp->active) {
	printf("ata: unwanted ata_start\n");
	return;
    }

    /* find & call the responsible driver if anything on ATA queue */
    if ((ata_request = bufq_first(&scp->ata_queue))) {
	scp->active = ATA_ACTIVE_ATA;
        ad_transfer(ata_request);
    }

    /* find & call the responsible driver if anything on ATAPI queue */
    if ((atapi_request = TAILQ_FIRST(&scp->atapi_queue))) {
    	scp->active = ATA_ACTIVE_ATAPI;
	atapi_transfer(atapi_request);
    }
}

int32_t
ata_wait(struct ata_softc *scp, u_int8_t mask)
{
    u_int8_t status;
    u_int32_t timeout = 0;

    while (timeout++ <= 50000) {	/* timeout 5 secs */
	status = inb(scp->ioaddr + ATA_STATUS);
	if ((status == 0xff) && (scp->flags & ATA_F_SLAVE_ONLY)) {
    	    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
	    status = inb(scp->ioaddr + ATA_STATUS);
	}
	if (status == 0xff)
	    return -1;
	scp->status = status;
	if (!(status & ATA_S_BSY)) {
	    if (status & ATA_S_ERROR)
		scp->error = inb(scp->ioaddr + ATA_ERROR);
	    if ((status & mask) == mask) 
		return (status & ATA_S_ERROR);
    	}
	if (timeout > 1000)
	    DELAY(1000);
	else
	    DELAY(10);
    }
    return -1;
}

static int32_t
ata_reset(struct ata_softc *scp)
{
    outb(scp->altioaddr, ATA_A_RESET | ATA_A_IDS);
    DELAY(10000);
    outb(scp->altioaddr, ATA_A_IDS);
    DELAY(10000);
    inb(scp->ioaddr + ATA_ERROR);
    outb(scp->altioaddr, ATA_A_4BIT);
    if (ata_wait(scp, 0) < 0) {
	printf("ata%d: RESET failed\n", scp->unit);
	return 1;
    }
    return 0;
}

static int32_t
ata_device_attach(struct ata_softc *scp, int32_t device)
{
    struct ata_params *ata_parm;
    int8_t buffer[DEV_BSIZE];

    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | device);
    if (ata_wait(scp, 0) < 0)
	return -1;
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | device); /* XXX SOS */
    scp->active = ATA_IGNORE_INTR;
    outb(scp->ioaddr + ATA_CMD, ATA_C_ATA_IDENTIFY);
    if (ata_wait(scp, ATA_S_DRDY | ATA_S_DSC | ATA_S_DRQ))
	return -1;

    insw(scp->ioaddr + ATA_DATA, buffer, sizeof(buffer)/sizeof(int16_t));
    ata_parm = malloc(sizeof(struct ata_params), M_DEVBUF, M_NOWAIT);
    if (!ata_parm) 
   	return -1; 
    bcopy(buffer, ata_parm, sizeof(struct ata_params));
    bswap(ata_parm->model, sizeof(ata_parm->model));
    btrim(ata_parm->model, sizeof(ata_parm->model));
    bswap(ata_parm->revision, sizeof(ata_parm->revision));
    btrim(ata_parm->revision, sizeof(ata_parm->revision));
    scp->ata_parm[device == ATA_SLAVE] = ata_parm;
    return 0;
}

int32_t
atapi_wait(struct ata_softc *scp, u_int8_t mask)
{
    u_int8_t status;
    u_int32_t timeout = 0;
    
    while (timeout++ <= 500000) {        /* timeout 5 secs */
        status = inb(scp->ioaddr + ATA_STATUS);
        if ((status == 0xff) && (scp->flags & ATA_F_SLAVE_ONLY)) {
            outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
            status = inb(scp->ioaddr + ATA_STATUS);
        }

        if (!(status & ATA_S_BSY))  
            break;            
        DELAY (10);        
    }    
    if (timeout <= 0)    
        return -1;          
    if (!mask)     
        return (status & ATA_S_ERROR);   
    
    /* Wait 50 msec for bits wanted. */    
    for (timeout=5000; timeout>0; --timeout) {    
        status = inb(scp->ioaddr + ATA_STATUS);        
        if ((status & mask) == mask)        
            return (status & ATA_S_ERROR);            
        DELAY (10);        
    }     
    return -1;      
}   

static int32_t
atapi_device_attach(struct ata_softc *scp, int32_t device)
{
    struct atapi_params	*atapi_parm;
    int8_t buffer[DEV_BSIZE];

    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | device);
    if (atapi_wait(scp, 0) < 0)
	return -1;
    outb(scp->ioaddr + ATA_DRIVE, ATA_D_IBM | device); /* XXX SOS */
    outb(scp->ioaddr + ATA_CMD, ATA_C_ATAPI_IDENTIFY);
    if (atapi_wait(scp, ATA_S_DRQ))
	return -1;

    insw(scp->ioaddr + ATA_DATA, buffer, sizeof(buffer)/sizeof(int16_t));
    atapi_parm = malloc(sizeof(struct atapi_params), M_DEVBUF, M_NOWAIT);
    if (!atapi_parm) 
   	return -1; 
    
    bcopy(buffer, atapi_parm, sizeof(struct atapi_params));
    if (!((atapi_parm->model[0] == 'N' && atapi_parm->model[1] == 'E') ||
          (atapi_parm->model[0] == 'F' && atapi_parm->model[1] == 'X')))
        bswap(atapi_parm->model, sizeof(atapi_parm->model));
    btrim(atapi_parm->model, sizeof(atapi_parm->model));
    bswap(atapi_parm->revision, sizeof(atapi_parm->revision));
    btrim(atapi_parm->revision, sizeof(atapi_parm->revision));
    bswap(atapi_parm->serial, sizeof(atapi_parm->serial)); /* unused SOS */
    btrim(atapi_parm->serial, sizeof(atapi_parm->serial)); /* unused SOS */
    scp->atapi_parm[device == ATA_SLAVE] = atapi_parm;
    return 0;
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

void
bpack(int8_t *src, int8_t *dst, int32_t len)
{
    int32_t i, j, blank;

    for (i = j = blank = 0 ; i < len-1; i++) {
	if (blank && src[i] == ' ') continue;
	if (blank && src[i] != ' ') {
	    dst[j++] = src[i];
	    blank = 0;
	    continue;
	}
	if (src[i] == ' ')
	    blank = 1;
	dst[j++] = src[i];
    }
    dst[j] = 0x00;
}
#endif
