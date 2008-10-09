/*-
 * Copyright (c) 1998 - 2008 Søren Schmidt <sos@FreeBSD.org>
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
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ata.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>
#include <ata_if.h>

/* local prototypes */
static int ata_ahci_ctlr_reset(device_t dev);
static int ata_ahci_suspend(device_t dev);
static int ata_ahci_status(device_t dev);
static int ata_ahci_begin_transaction(struct ata_request *request);
static int ata_ahci_end_transaction(struct ata_request *request);
static int ata_ahci_pm_read(device_t dev, int port, int reg, u_int32_t *result);
static int ata_ahci_pm_write(device_t dev, int port, int reg, u_int32_t result);
static u_int32_t ata_ahci_softreset(device_t dev, int port);
static void ata_ahci_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static int ata_ahci_setup_fis(struct ata_ahci_cmd_tab *ctp, struct ata_request *equest);

/*
 * AHCI v1.x compliant SATA chipset support functions
 */
static int
ata_ahci_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    char buffer[64];

    /* is this a possible AHCI candidate ? */
    if (pci_get_subclass(dev) != PCIS_STORAGE_SATA)
	return ENXIO;

    /* is this PCI device flagged as an AHCI compliant chip ? */
    if (pci_read_config(dev, PCIR_PROGIF, 1) != PCIP_STORAGE_SATA_AHCI_1_0)
	return ENXIO;

    if (bootverbose)
	sprintf(buffer, "%s (ID=%08x) AHCI controller", 
		ata_pcivendor2str(dev), pci_get_devid(dev));
    else
	sprintf(buffer, "%s AHCI controller", ata_pcivendor2str(dev));
    device_set_desc_copy(dev, buffer);
    ctlr->chipinit = ata_ahci_chipinit;
    return 0;
}

int
ata_ahci_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    u_int32_t version;

    /* if we have a memory BAR(5) we are likely on an AHCI part */
    ctlr->r_type2 = SYS_RES_MEMORY;
    ctlr->r_rid2 = PCIR_BAR(5);
    if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
					       &ctlr->r_rid2, RF_ACTIVE)))
	return ENXIO;

    /* setup interrupt delivery if not done allready by a vendor driver */
    if (!ctlr->r_irq) {
	if (ata_setup_interrupt(dev, ata_generic_intr))
	    return ENXIO;
    }
    else
	device_printf(dev, "AHCI called from vendor specific driver\n");

    /* reset controller */
    ata_ahci_ctlr_reset(dev);

    /* get the number of HW channels */
    ctlr->channels =
	MAX(flsl(ATA_INL(ctlr->r_res2, ATA_AHCI_PI)), 
	    (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_NPMASK) + 1);

    ctlr->reset = ata_ahci_reset;
    ctlr->dmainit = ata_ahci_dmainit;
    ctlr->allocate = ata_ahci_allocate;
    ctlr->setmode = ata_sata_setmode;
    ctlr->suspend = ata_ahci_suspend;
    ctlr->resume = ata_ahci_ctlr_reset;

    /* enable PCI interrupt */
    pci_write_config(dev, PCIR_COMMAND,
		     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);

    /* announce we support the HW */
    version = ATA_INL(ctlr->r_res2, ATA_AHCI_VS);
    device_printf(dev,
		  "AHCI Version %x%x.%x%x controller with %d ports PM %s\n",
		  (version >> 24) & 0xff, (version >> 16) & 0xff,
		  (version >> 8) & 0xff, version & 0xff,
		  (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_NPMASK) + 1,
		  (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_CAP_SPM) ?
		  "supported" : "not supported");
    return 0;
}

static int
ata_ahci_ctlr_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    /* enable AHCI mode */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC, ATA_AHCI_GHC_AE);

    /* reset AHCI controller */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC, ATA_AHCI_GHC_HR);
    DELAY(1000000);
    if (ATA_INL(ctlr->r_res2, ATA_AHCI_GHC) & ATA_AHCI_GHC_HR) {
	bus_release_resource(dev, ctlr->r_type2, ctlr->r_rid2, ctlr->r_res2);
	device_printf(dev, "AHCI controller reset failure\n");
	return ENXIO;
    }

    /* reenable AHCI mode */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC, ATA_AHCI_GHC_AE);

    /* clear interrupts */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_IS, ATA_INL(ctlr->r_res2, ATA_AHCI_IS));

    /* enable AHCI interrupts */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC,
	     ATA_INL(ctlr->r_res2, ATA_AHCI_GHC) | ATA_AHCI_GHC_IE);

    return 0;
}

static int
ata_ahci_suspend(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    /* disable interupts so the state change(s) doesn't trigger */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC,
             ATA_INL(ctlr->r_res2, ATA_AHCI_GHC) & (~ATA_AHCI_GHC_IE));
    return 0;
}


int
ata_ahci_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ch->unit << 7;

    /* set the SATA resources */
    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = ATA_AHCI_P_SSTS + offset;
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = ATA_AHCI_P_SERR + offset;
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = ATA_AHCI_P_SCTL + offset;
    ch->r_io[ATA_SACTIVE].res = ctlr->r_res2;
    ch->r_io[ATA_SACTIVE].offset = ATA_AHCI_P_SACT + offset;

    ch->hw.status = ata_ahci_status;
    ch->hw.begin_transaction = ata_ahci_begin_transaction;
    ch->hw.end_transaction = ata_ahci_end_transaction;
    ch->hw.command = NULL;      /* not used here */
    ch->hw.softreset = ata_ahci_softreset;
    ch->hw.pm_read = ata_ahci_pm_read;
    ch->hw.pm_write = ata_ahci_pm_write;

    return 0;
}

static int
ata_ahci_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t action = ATA_INL(ctlr->r_res2, ATA_AHCI_IS);
    int offset = ch->unit << 7;

#define ATA_AHCI_STATBITS \
	(ATA_AHCI_P_IX_IF|ATA_AHCI_P_IX_HBD|ATA_AHCI_P_IX_HBF|ATA_AHCI_P_IX_TFE)

    if (action & (1 << ch->unit)) {
	u_int32_t istatus = ATA_INL(ctlr->r_res2, ATA_AHCI_P_IS + offset);
	u_int32_t cstatus = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CI + offset);

	/* clear interrupt(s) */
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IS + offset, istatus);
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_IS, action & (1 << ch->unit));

	/* do we have any PHY events ? */
	if (istatus & (ATA_AHCI_P_IX_PRC | ATA_AHCI_P_IX_PC))
	    ata_sata_phy_check_events(dev);

	/* do we have a potentially hanging engine to take care of? */
	/* XXX SOS what todo on NCQ */
	if ((istatus & ATA_AHCI_STATBITS) && (cstatus & 1)) {

	    u_int32_t cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
	    int timeout = 0;

	    /* kill off all activity on this channel */
	    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		     cmd & ~(ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST));

	    /* XXX SOS this is not entirely wrong */
	    do {
		DELAY(1000);
		if (timeout++ > 1000) {
		    device_printf(dev, "stopping AHCI engine failed\n");
		    break;
		}
    	    } while (ATA_INL(ctlr->r_res2,
			     ATA_AHCI_P_CMD + offset) & ATA_AHCI_P_CMD_CR);

	    /* start operations on this channel */
	    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		     cmd | (ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST));

	    return 1;
	}
	else
	    /* XXX SOS what todo on NCQ */
	    return (!(cstatus & 1));
    }
    return 0;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_ahci_begin_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_device *atadev = device_get_softc(request->dev);
    struct ata_ahci_cmd_tab *ctp;
    struct ata_ahci_cmd_list *clp;
    int offset = ch->unit << 7;
    int port = atadev->unit & 0x0f;
    int entries = 0;
    int fis_size;
	
    /* get a piece of the workspace for this request */
    ctp = (struct ata_ahci_cmd_tab *)
	  (ch->dma.work + ATA_AHCI_CT_OFFSET + (ATA_AHCI_CT_SIZE*request->tag));

    /* setup the FIS for this request */
    if (!(fis_size = ata_ahci_setup_fis(ctp, request))) {
	device_printf(request->dev, "setting up SATA FIS failed\n");
	request->result = EIO;
	return ATA_OP_FINISHED;
    }

    /* if request moves data setup and load SG list */
    if (request->flags & (ATA_R_READ | ATA_R_WRITE)) {
	if (ch->dma.load(request, ctp->prd_tab, &entries)) {
	    device_printf(request->dev, "setting up DMA failed\n");
	    request->result = EIO;
	    return ATA_OP_FINISHED;
	}
    }

    /* setup the command list entry */
    clp = (struct ata_ahci_cmd_list *)
	  (ch->dma.work + ATA_AHCI_CL_OFFSET + (ATA_AHCI_CL_SIZE*request->tag));

    clp->prd_length = entries;
    clp->cmd_flags = (request->flags & ATA_R_WRITE ? ATA_AHCI_CMD_WRITE : 0) |
		     (request->flags & ATA_R_ATAPI ?
		      (ATA_AHCI_CMD_ATAPI | ATA_AHCI_CMD_PREFETCH) : 0) |
		     (fis_size / sizeof(u_int32_t)) |
    		     (port << 12);
    clp->bytecount = 0;
    clp->cmd_table_phys = htole64(ch->dma.work_bus + ATA_AHCI_CT_OFFSET +
				  (ATA_AHCI_CT_SIZE * request->tag));

    /* clear eventual ACTIVE bit */
    ATA_IDX_OUTL(ch, ATA_SACTIVE,
		 ATA_IDX_INL(ch, ATA_SACTIVE) & (1 << request->tag));

    /* set command type bit */
    if (request->flags & ATA_R_ATAPI)
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		 ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset) |
		 ATA_AHCI_P_CMD_ATAPI);
    else
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		 ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset) &
		 ~ATA_AHCI_P_CMD_ATAPI);

    /* set PM port to address */
    //ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FBS + offset, (port << 8) | 0x00000001);

    /* issue command to controller */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CI + offset, (1 << request->tag));
    
    if (!(request->flags & ATA_R_ATAPI)) {
	/* device reset doesn't interrupt */
	if (request->u.ata.command == ATA_DEVICE_RESET) {
	    u_int32_t tf_data;
	    int timeout = 1000000;

	    do {
		DELAY(10);
		tf_data = ATA_INL(ctlr->r_res2, ATA_AHCI_P_TFD + (ch->unit<<7));
	    } while ((tf_data & ATA_S_BUSY) && timeout--);
	    if (bootverbose)
		device_printf(ch->dev, "device_reset timeout=%dus\n",
			      (1000000-timeout)*10);
	    request->status = tf_data;
	    if (request->status & ATA_S_ERROR)
		request->error = tf_data >> 8;
	    return ATA_OP_FINISHED;
	}
    }

    /* start the timeout */
    callout_reset(&request->callout, request->timeout * hz,
		  (timeout_t*)ata_timeout, request);
    return ATA_OP_CONTINUES;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_ahci_end_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_ahci_cmd_list *clp;
    u_int32_t tf_data;
    int offset = ch->unit << 7;

    /* kill the timeout */
    callout_stop(&request->callout);

    /* get status */
    tf_data = ATA_INL(ctlr->r_res2, ATA_AHCI_P_TFD + offset);
    request->status = tf_data;

    /* if error status get details */
    if (request->status & ATA_S_ERROR)  
	request->error = tf_data >> 8;

    /* on control commands read back registers to the request struct */
    if (request->flags & ATA_R_CONTROL) {
	struct ata_device *atadev = device_get_softc(request->dev);
	u_int8_t *fis = ch->dma.work + ATA_AHCI_FB_OFFSET + 0x40;

	request->u.ata.count = fis[12] | ((u_int16_t)fis[13] << 8);
	request->u.ata.lba = fis[4] | ((u_int64_t)fis[5] << 8) |
			     ((u_int64_t)fis[6] << 16);
	if (atadev->flags & ATA_D_48BIT_ACTIVE)
	    request->u.ata.lba |= ((u_int64_t)fis[8] << 24) |
				  ((u_int64_t)fis[9] << 32) |
				  ((u_int64_t)fis[10] << 40);
	else
	    request->u.ata.lba |= ((u_int64_t)(fis[7] & 0x0f) << 24);
    }

    /* record how much data we actually moved */
    clp = (struct ata_ahci_cmd_list *)
	  (ch->dma.work + ATA_AHCI_CL_OFFSET + (ATA_AHCI_CL_SIZE*request->tag));
    request->donecount = clp->bytecount;

    /* release SG list etc */
    ch->dma.unload(request);

    return ATA_OP_FINISHED;
}

static int
ata_ahci_issue_cmd(device_t dev, u_int16_t flags, int timeout)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_ahci_cmd_list *clp =
	(struct ata_ahci_cmd_list *)(ch->dma.work + ATA_AHCI_CL_OFFSET);
    struct ata_ahci_cmd_tab *ctp =
	(struct ata_ahci_cmd_tab *)(ch->dma.work + ATA_AHCI_CT_OFFSET);
    u_int32_t status = 0;
    int offset = ch->unit << 7;
    int port = (ctp->cfis[1] & 0x0f);
    int count;

    clp->prd_length = 0;
    clp->cmd_flags = (20 / sizeof(u_int32_t)) | flags | (port << 12);
    clp->bytecount = 0;
    clp->cmd_table_phys = htole64(ch->dma.work_bus + ATA_AHCI_CT_OFFSET);

    /* set PM port */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FBS + offset, (port << 8) | 0x00000001);

    /* issue command to controller */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CI + offset, 1);

    /* poll for command finished */
    for (count = 0; count < timeout; count++) {
        DELAY(1000);
        if (!((status = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CI + offset)) & 1))
            break;
    }

    /* clear interrupts */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IS + offset,
	     ATA_INL(ctlr->r_res2, ATA_AHCI_P_IS + offset));

    if (bootverbose)
	device_printf(dev, "ahci_issue_cmd time=%dms cnt=%dms status=%08x\n",
		      timeout, count, status);
    if (timeout && (count >= timeout))
	return EIO;

    return 0;
}

static int
ata_ahci_pm_read(device_t dev, int port, int reg, u_int32_t *result)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_ahci_cmd_tab *ctp =
	(struct ata_ahci_cmd_tab *)(ch->dma.work + ATA_AHCI_CT_OFFSET);
    u_int8_t *fis = ch->dma.work + ATA_AHCI_FB_OFFSET + 0x40;

    bzero(ctp->cfis, 64);
    ctp->cfis[0] = 0x27;	/* host to device */
    ctp->cfis[1] = 0x8f;	/* command FIS to PM port */
    ctp->cfis[2] = ATA_READ_PM;
    ctp->cfis[3] = reg;
    ctp->cfis[7] = port | ATA_D_LBA;
    ctp->cfis[15] = ATA_A_4BIT;

    if (ata_ahci_issue_cmd(dev, 0, 10)) {
	device_printf(dev, "error reading PM port\n");
	return EIO;
    }

    *result = fis[12] | (fis[4] << 8) | (fis[5] << 16) | (fis[6] << 24);
    return 0;
}

static int
ata_ahci_pm_write(device_t dev, int port, int reg, u_int32_t value)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_ahci_cmd_tab *ctp =
	(struct ata_ahci_cmd_tab *)(ch->dma.work + ATA_AHCI_CT_OFFSET);
    int offset = ch->unit << 7;

    bzero(ctp->cfis, 64);
    ctp->cfis[0] = 0x27;	/* host to device */
    ctp->cfis[1] = 0x8f;	/* command FIS to PM port */
    ctp->cfis[2] = ATA_WRITE_PM;
    ctp->cfis[3] = reg;
    ctp->cfis[7] = port | ATA_D_LBA;
    ctp->cfis[12] = value & 0xff;
    ctp->cfis[4] = (value >> 8) & 0xff;;
    ctp->cfis[5] = (value >> 16) & 0xff;;
    ctp->cfis[6] = (value >> 24) & 0xff;;
    ctp->cfis[15] = ATA_A_4BIT;

    if (ata_ahci_issue_cmd(dev, 0, 100)) {
	device_printf(dev, "error writing PM port\n");
	return ATA_E_ABORT;
    }

    return (ATA_INL(ctlr->r_res2, ATA_AHCI_P_TFD + offset) >> 8) & 0xff;
}

static void
ata_ahci_restart(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t cmd;
    int offset = ch->unit << 7;
    int timeout;

    /* kill off all activity on this channel */
    cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
	     cmd & ~(ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST));

    /* XXX SOS this is not entirely wrong */
    timeout = 0;
    do {
	DELAY(1000);
	if (timeout++ > 1000) {
	    device_printf(dev, "stopping AHCI engine failed\n");
	    break;
	}
    }
    while (ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset) & ATA_AHCI_P_CMD_CR);

    /* issue Command List Override if supported */ 
    if (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_CAP_CLO) {
	cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
	cmd |= ATA_AHCI_P_CMD_CLO;
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset, cmd);
	timeout = 0;
	do {
	    DELAY(1000);
	    if (timeout++ > 1000) {
		device_printf(dev, "executing CLO failed\n");
		break;
	    }
        }
	while (ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD+offset)&ATA_AHCI_P_CMD_CLO);
    }

    /* clear SATA error register */
    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

    /* clear any interrupts pending on this channel */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IS + offset,
	     ATA_INL(ctlr->r_res2, ATA_AHCI_P_IS + offset));

    /* start operations on this channel */
    cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
	     cmd | (ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST) |
	     (ch->devices & ATA_PORTMULTIPLIER ? ATA_AHCI_P_CMD_PMA : 0));
}

static u_int32_t
ata_ahci_softreset(device_t dev, int port)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ch->unit << 7;
    int timeout = 0;
#ifdef AHCI_PM
    struct ata_ahci_cmd_tab *ctp =
	(struct ata_ahci_cmd_tab *)(ch->dma.work + ATA_AHCI_CT_OFFSET);

    /* kick controller into sane state if needed */
    ata_ahci_restart(dev);

    /* pull reset active */
    bzero(ctp->cfis, 64);
    ctp->cfis[0] = 0x27;
    ctp->cfis[1] = port & 0x0f;
    //ctp->cfis[7] = ATA_D_LBA | ATA_D_IBM;
    ctp->cfis[15] = (ATA_A_4BIT | ATA_A_RESET);

    if (ata_ahci_issue_cmd(dev, ATA_AHCI_CMD_RESET | ATA_AHCI_CMD_CLR_BUSY,100))
	device_printf(dev, "setting SRST failed ??\n");
	//return -1;

    ata_udelay(5000);

    /* pull reset inactive -> device softreset */
    bzero(ctp->cfis, 64);
    ctp->cfis[0] = 0x27;
    ctp->cfis[1] = port & 0x0f;
    //ctp->cfis[7] = ATA_D_LBA | ATA_D_IBM;
    ctp->cfis[15] = ATA_A_4BIT;
    if (ata_ahci_issue_cmd(dev, 0, 0))
	return -1;

    ata_udelay(150000);

#endif
    do {
	    DELAY(1000);
	    if (timeout++ > 1000) {
		device_printf(dev, "still BUSY after softreset\n");
		break;
	    }
    } while (ATA_INL(ctlr->r_res2, ATA_AHCI_P_TFD + offset) & ATA_S_BUSY);
    if (bootverbose)
	device_printf(dev, "BUSY wait time=%dms\n", timeout);

    return ATA_INL(ctlr->r_res2, ATA_AHCI_P_SIG + offset);
}

void
ata_ahci_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int64_t work;
    u_int32_t cmd, signature;
    int offset = ch->unit << 7;

    if (!(ATA_INL(ctlr->r_res2, ATA_AHCI_PI) & (1 << ch->unit))) {
	device_printf(dev, "port not implemented\n");
	return;
    }

    /* setup work areas */
    work = ch->dma.work_bus + ATA_AHCI_CL_OFFSET;
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CLB + offset, work & 0xffffffff);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CLBU + offset, work >> 32);

    work = ch->dma.work_bus + ATA_AHCI_FB_OFFSET;
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FB + offset, work & 0xffffffff); 
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FBU + offset, work >> 32);

    /* enable wanted port interrupts */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IE + offset,
	     (ATA_AHCI_P_IX_CPD | ATA_AHCI_P_IX_TFE | ATA_AHCI_P_IX_HBF |
	      ATA_AHCI_P_IX_HBD | ATA_AHCI_P_IX_IF | ATA_AHCI_P_IX_OF |
	      ATA_AHCI_P_IX_PRC | ATA_AHCI_P_IX_PC | ATA_AHCI_P_IX_DP |
	      ATA_AHCI_P_IX_UF | ATA_AHCI_P_IX_SDB | ATA_AHCI_P_IX_DS |
	      ATA_AHCI_P_IX_PS | ATA_AHCI_P_IX_DHR));

    /* activate the channel and power/spin up device */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
	     (ATA_AHCI_P_CMD_ACTIVE | ATA_AHCI_P_CMD_POD | ATA_AHCI_P_CMD_SUD));

    ata_ahci_restart(dev);

    /* enable FIS based switching */
    //ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FBS + offset, 0x00000003);

    if (!ata_sata_phy_reset(dev)) {
	if (bootverbose)
	    device_printf(dev, "phy reset found no device\n");
	ch->devices = 0;

	/* kill off all activity on this channel */
	cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		 cmd & ~(ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST));
	return;
    }

    /* only probe for PortMultiplier if HW has support */
    if (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_CAP_SPM)
	signature = ata_ahci_softreset(dev, ATA_PM);
    else {
	signature = ata_ahci_softreset(dev, 0);
    }
    if (bootverbose)
	device_printf(dev, "SIGNATURE: %08x\n", signature);

    switch (signature) {
    case 0x00000101:
	ch->devices = ATA_ATA_MASTER;
	break;
    case 0x96690101:
	ch->devices = ATA_PORTMULTIPLIER;
	ata_pm_identify(dev);
	break;
    case 0xeb140101:
	ch->devices = ATA_ATAPI_MASTER;
	break;
    default: /* SOS XXX */
	if (bootverbose)
	    device_printf(dev, "No signature, asuming disk device\n");
	ch->devices = ATA_ATA_MASTER;
    }
    if (bootverbose)
        device_printf(dev, "ahci_reset devices=%08x\n", ch->devices);
}

static void
ata_ahci_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{    
    struct ata_dmasetprd_args *args = xsc;
    struct ata_ahci_dma_prd *prd = args->dmatab;
    int i;

    if (!(args->error = error)) {
	for (i = 0; i < nsegs; i++) {
	    prd[i].dba = htole64(segs[i].ds_addr);
	    prd[i].dbc = htole32((segs[i].ds_len - 1) & ATA_AHCI_PRD_MASK);
	}
    }

    KASSERT(nsegs <= ATA_AHCI_DMA_ENTRIES, ("too many DMA segment entries\n"));
    args->nsegs = nsegs;
}

void
ata_ahci_dmainit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    /* note start and stop are not used here */
    ch->dma.setprd = ata_ahci_dmasetprd;
    ch->dma.max_iosize = 8192 * DEV_BSIZE;
    if (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_CAP_64BIT)
	ch->dma.max_address = BUS_SPACE_MAXADDR;
}

static int
ata_ahci_setup_fis(struct ata_ahci_cmd_tab *ctp, struct ata_request *request)
{
    bzero(ctp->cfis, 64);
    if (request->flags & ATA_R_ATAPI) {
	bzero(ctp->acmd, 32);
	bcopy(request->u.atapi.ccb, ctp->acmd, 16);
    }
    return ata_request2fis_h2d(request, &ctp->cfis[0]);
}

ATA_DECLARE_DRIVER(ata_ahci);
