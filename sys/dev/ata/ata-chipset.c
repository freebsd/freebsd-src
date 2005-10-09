/*-
 * Copyright (c) 1998 - 2005 Søren Schmidt <sos@FreeBSD.org>
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
static int ata_generic_chipinit(device_t dev);
static void ata_generic_intr(void *data);
static void ata_generic_setmode(device_t dev, int mode);
static void ata_sata_setmode(device_t dev, int mode);
static int ata_sata_connect(struct ata_channel *ch);
static void ata_sata_phy_enable(struct ata_channel *ch);
static void ata_sata_phy_event(void *context, int dummy);
static int ata_ahci_allocate(device_t dev);
static int ata_ahci_setup_fis(u_int8_t *fis, struct ata_request *request);
static int ata_ahci_begin_transaction(struct ata_request *request);
static int ata_ahci_end_transaction(struct ata_request *request);
static void ata_ahci_intr(void *data);
static void ata_ahci_reset(device_t dev);
static void ata_ahci_dmainit(device_t dev);
static int ata_acard_chipinit(device_t dev);
static void ata_acard_intr(void *data);
static void ata_acard_850_setmode(device_t dev, int mode);
static void ata_acard_86X_setmode(device_t dev, int mode);
static int ata_ali_chipinit(device_t dev);
static int ata_ali_allocate(device_t dev);
static int ata_ali_sata_allocate(device_t dev);
static void ata_ali_reset(device_t dev);
static void ata_ali_setmode(device_t dev, int mode);
static int ata_amd_chipinit(device_t dev);
static int ata_cyrix_chipinit(device_t dev);
static void ata_cyrix_setmode(device_t dev, int mode);
static int ata_cypress_chipinit(device_t dev);
static void ata_cypress_setmode(device_t dev, int mode);
static int ata_highpoint_chipinit(device_t dev);
static void ata_highpoint_intr(void *data);
static void ata_highpoint_setmode(device_t dev, int mode);
static int ata_highpoint_check_80pin(device_t dev, int mode);
static int ata_intel_chipinit(device_t dev);
static int ata_intel_31244_allocate(device_t dev);
static void ata_intel_31244_intr(void *data);
static void ata_intel_31244_reset(device_t dev);
static int ata_intel_31244_command(struct ata_request *request);
static void ata_intel_intr(void *data);
static void ata_intel_reset(device_t dev);
static void ata_intel_old_setmode(device_t dev, int mode);
static void ata_intel_new_setmode(device_t dev, int mode);
static int ata_ite_chipinit(device_t dev);
static void ata_ite_setmode(device_t dev, int mode);
static int ata_national_chipinit(device_t dev);
static void ata_national_setmode(device_t dev, int mode);
static int ata_nvidia_chipinit(device_t dev);
static int ata_nvidia_allocate(device_t dev);
static void ata_nvidia_intr(void *data);
static void ata_nvidia_reset(device_t dev);
static int ata_promise_chipinit(device_t dev);
static int ata_promise_mio_allocate(device_t dev);
static void ata_promise_mio_intr(void *data);
static void ata_promise_sx4_intr(void *data);
static void ata_promise_mio_dmainit(device_t dev);
static void ata_promise_mio_reset(device_t dev);
static int ata_promise_mio_command(struct ata_request *request);
static int ata_promise_sx4_command(struct ata_request *request);
static int ata_promise_apkt(u_int8_t *bytep, struct ata_request *request);
static void ata_promise_queue_hpkt(struct ata_pci_controller *ctlr, u_int32_t hpkt);
static void ata_promise_next_hpkt(struct ata_pci_controller *ctlr);
static void ata_promise_tx2_intr(void *data);
static void ata_promise_old_intr(void *data);
static int ata_promise_new_dmastart(device_t dev);
static int ata_promise_new_dmastop(device_t dev);
static void ata_promise_new_dmareset(device_t dev);
static void ata_promise_new_dmainit(device_t dev);
static void ata_promise_setmode(device_t dev, int mode);
static int ata_serverworks_chipinit(device_t dev);
static void ata_serverworks_setmode(device_t dev, int mode);
static int ata_sii_chipinit(device_t dev);
static int ata_sii_allocate(device_t dev);
static void ata_sii_intr(void *data);
static void ata_cmd_intr(void *data);
static void ata_cmd_old_intr(void *data);
static void ata_sii_reset(device_t dev);
static void ata_sii_setmode(device_t dev, int mode);
static void ata_cmd_setmode(device_t dev, int mode);
static int ata_sis_chipinit(device_t dev);
static int ata_sis_allocate(device_t dev);
static void ata_sis_reset(device_t dev);
static void ata_sis_setmode(device_t dev, int mode);
static int ata_via_chipinit(device_t dev);
static int ata_via_allocate(device_t dev);
static void ata_via_reset(device_t dev);
static void ata_via_southbridge_fixup(device_t dev);
static void ata_via_family_setmode(device_t dev, int mode);
static struct ata_chip_id *ata_find_chip(device_t dev, struct ata_chip_id *index, int slot);
static int ata_setup_interrupt(device_t dev);
static int ata_serialize(device_t dev, int flags);
static void ata_print_cable(device_t dev, u_int8_t *who);
static int ata_atapi(device_t dev);
static int ata_check_80pin(device_t dev, int mode);
static int ata_mode2idx(int mode);


/* generic or unknown ATA chipset support functions */
int
ata_generic_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    device_set_desc(dev, "GENERIC ATA controller");
    ctlr->chipinit = ata_generic_chipinit;
    return 0;
}

static int
ata_generic_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;
    ctlr->setmode = ata_generic_setmode;
    return 0;
}

static void
ata_generic_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
	    int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	    if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		ATA_BMSTAT_INTERRUPT)
		continue;
	    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	    DELAY(1);
	}
	ctlr->interrupt[unit].function(ch);
    }
}

static void
ata_generic_setmode(device_t dev, int mode)
{
    struct ata_device *atadev = device_get_softc(dev);

    mode = ata_limit_mode(dev, mode, ATA_UDMA2);
    mode = ata_check_80pin(dev, mode);
    if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
	atadev->mode = mode;
}


/* SATA support functions */
static void
ata_sata_setmode(device_t dev, int mode)
{
    struct ata_pci_controller *ctlr = device_get_softc(GRANDPARENT(dev));
    struct ata_device *atadev = device_get_softc(dev);

    /*
     * if we detect that the device isn't a real SATA device we limit 
     * the transfer mode to UDMA5/ATA100.
     * this works around the problems some devices has with the 
     * Marvell 88SX8030 SATA->PATA converters and UDMA6/ATA133.
     */
    if (atadev->param.satacapabilities != 0x0000 &&
	atadev->param.satacapabilities != 0xffff) {
	if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0,
			    ata_limit_mode(dev, mode, ATA_UDMA6)))
	    atadev->mode = ctlr->chip->max_dma;
    }
    else {
	mode = ata_limit_mode(dev, mode, ATA_UDMA5);
	if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
	    atadev->mode = mode;
    }
}

static int
ata_sata_connect(struct ata_channel *ch)
{
    u_int32_t status;
    int timeout;

    /* wait up to 1 second for "connect well" */
    for (timeout = 0; timeout < 100 ; timeout++) {
	status = ATA_IDX_INL(ch, ATA_SSTATUS);
	if ((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN1 ||
	    (status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN2)
	    break;
	ata_udelay(10000);
    }
    if (timeout >= 100) {
	if (bootverbose)
	    device_printf(ch->dev, "SATA connect status=%08x\n", status);
	return 0;
    }

    /* clear SATA error register */
    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

    /* find out what type device we got poll for spec'd 31 seconds */
    /* XXX SOS 10 secs for now as I have little patience */
    ch->devices = 0;
    for (timeout = 0; timeout < 1000; timeout++) {
	if (ATA_IDX_INB(ch, ATA_STATUS) & ATA_S_BUSY) 
	    DELAY(10000);
	else
	    break;
    }
    if (bootverbose)
	device_printf(ch->dev, "SATA connect ready time=%dms\n", timeout * 10);
    if (timeout < 1000) {
	if ((ATA_IDX_INB(ch, ATA_CYL_LSB) == ATAPI_MAGIC_LSB) &&
	    (ATA_IDX_INB(ch, ATA_CYL_MSB) == ATAPI_MAGIC_MSB))
	    ch->devices = ATA_ATAPI_MASTER;
	else 
	    ch->devices = ATA_ATA_MASTER;
    }
    if (bootverbose)
	device_printf(ch->dev, "sata_connect devices=0x%b\n",
		      ch->devices, "\20\3ATAPI_MASTER\1ATA_MASTER");
    return 1;
}

static void
ata_sata_phy_enable(struct ata_channel *ch)
{
    int loop, retry;

    if ((ATA_IDX_INL(ch, ATA_SCONTROL) & ATA_SC_DET_MASK) == ATA_SC_DET_IDLE) {
	ata_sata_connect(ch);
	return;
    }

    for (retry = 0; retry < 10; retry++) {
	for (loop = 0; loop < 10; loop++) {
	    ATA_IDX_OUTL(ch, ATA_SCONTROL, ATA_SC_DET_RESET);
	    ata_udelay(100);
	    if ((ATA_IDX_INL(ch, ATA_SCONTROL) &
		 ATA_SC_DET_MASK) == ATA_SC_DET_RESET)
		break;
	}
	ata_udelay(5000);
	for (loop = 0; loop < 10; loop++) {
	    ATA_IDX_OUTL(ch, ATA_SCONTROL, ATA_SC_DET_IDLE);
	    ata_udelay(100);
	    if ((ATA_IDX_INL(ch, ATA_SCONTROL) & ATA_SC_DET_MASK) == 0) {
		ata_sata_connect(ch);
		return;
	    }
	}
    }
}

static void
ata_sata_phy_event(void *context, int dummy)
{
    struct ata_connect_task *tp = (struct ata_connect_task *)context;
    struct ata_channel *ch = device_get_softc(tp->dev);
    device_t *children;
    int nchildren, i;

    mtx_lock(&Giant);   /* newbus suckage it needs Giant */
    if (tp->action == ATA_C_ATTACH) {
	device_printf(tp->dev, "CONNECTED\n");
	ata_sata_connect(ch);
	ata_identify(tp->dev);
    }
    if (tp->action == ATA_C_DETACH) {
	if (!device_get_children(tp->dev, &children, &nchildren)) {
	    for (i = 0; i < nchildren; i++)
		if (children[i])
		    device_delete_child(tp->dev, children[i]);
	    free(children, M_TEMP);
	}    
	mtx_lock(&ch->state_mtx);
	ch->state = ATA_IDLE;
	mtx_unlock(&ch->state_mtx);
	device_printf(tp->dev, "DISCONNECTED\n");
    }
    mtx_unlock(&Giant); /* suckage code dealt with, release Giant */
    free(tp, M_ATA);
}


/*
 * AHCI v1.0 compliant SATA chipset support functions
 */
struct ata_ahci_dma_prd {
    u_int64_t                   dba;
    u_int32_t                   reserved;
    u_int32_t                   dbc;            /* 0 based */
#define ATA_AHCI_PRD_MASK       0x003fffff      /* max 4MB */
#define ATA_AHCI_PRD_IPC        (1<<31)
} __packed;

struct ata_ahci_cmd_tab {
    u_int8_t                    cfis[64];
    u_int8_t                    acmd[32];
    u_int8_t                    reserved[32];
    struct ata_ahci_dma_prd     prd_tab[16];
} __packed;

struct ata_ahci_cmd_list {
    u_int16_t                   cmd_flags;
    u_int16_t                   prd_length;     /* PRD entries */
    u_int32_t                   bytecount;
    u_int64_t                   cmd_table_phys; /* 128byte aligned */
} __packed;


static int
ata_ahci_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = (ch->unit << 7);

    /* XXX SOS this is a hack to satisfy various legacy cruft */
    ch->r_io[ATA_CYL_LSB].res = ctlr->r_res2;
    ch->r_io[ATA_CYL_LSB].offset = ATA_AHCI_P_SIG + 1 + offset;
    ch->r_io[ATA_CYL_MSB].res = ctlr->r_res2;
    ch->r_io[ATA_CYL_MSB].offset = ATA_AHCI_P_SIG + 3 + offset;
    ch->r_io[ATA_STATUS].res = ctlr->r_res2;
    ch->r_io[ATA_STATUS].offset = ATA_AHCI_P_TFD + offset;
    ch->r_io[ATA_ALTSTAT].res = ctlr->r_res2;
    ch->r_io[ATA_ALTSTAT].offset = ATA_AHCI_P_TFD + offset;

    /* set the SATA resources */
    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = ATA_AHCI_P_SSTS + offset;
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = ATA_AHCI_P_SERR + offset;
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = ATA_AHCI_P_SCTL + offset;
    ch->r_io[ATA_SACTIVE].res = ctlr->r_res2;
    ch->r_io[ATA_SACTIVE].offset = ATA_AHCI_P_SACT + offset;

    ch->hw.begin_transaction = ata_ahci_begin_transaction;
    ch->hw.end_transaction = ata_ahci_end_transaction;
    ch->hw.command = NULL;      /* not used here */

    /* setup the work areas */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CLB + offset,
	     ch->dma->work_bus + ATA_AHCI_CL_OFFSET);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CLBU + offset, 0x00000000);

    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FB + offset,
	     ch->dma->work_bus + ATA_AHCI_FB_OFFSET);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FBU + offset, 0x00000000);

    /* enable wanted port interrupts */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IE + offset,
	     (ATA_AHCI_P_IX_CPD | ATA_AHCI_P_IX_TFE | ATA_AHCI_P_IX_HBF |
	      ATA_AHCI_P_IX_HBD | ATA_AHCI_P_IX_IF | ATA_AHCI_P_IX_OF |
	      ATA_AHCI_P_IX_PRC | ATA_AHCI_P_IX_PC | ATA_AHCI_P_IX_DP |
	      ATA_AHCI_P_IX_UF | ATA_AHCI_P_IX_SDB | ATA_AHCI_P_IX_DS |
	      ATA_AHCI_P_IX_PS | ATA_AHCI_P_IX_DHR));

    /* start operations on this channel */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
	     (ATA_AHCI_P_CMD_ACTIVE | ATA_AHCI_P_CMD_FRE |
	      ATA_AHCI_P_CMD_POD | ATA_AHCI_P_CMD_SUD | ATA_AHCI_P_CMD_ST));
    return 0;
}

static int
ata_ahci_setup_fis(u_int8_t *fis, struct ata_request *request)
{
    struct ata_device *atadev = device_get_softc(request->dev);
    int idx = 0;

    /* XXX SOS add ATAPI commands support later */
    ata_modify_if_48bit(request);

    fis[idx++] = 0x27;  /* host to device */
    fis[idx++] = 0x80;  /* command FIS (note PM goes here) */
    fis[idx++] = request->u.ata.command;
    fis[idx++] = request->u.ata.feature;

    fis[idx++] = request->u.ata.lba;
    fis[idx++] = request->u.ata.lba >> 8;
    fis[idx++] = request->u.ata.lba >> 16;
    fis[idx] = ATA_D_LBA | atadev->unit;
    if (atadev->flags & ATA_D_48BIT_ACTIVE)
	idx++;
    else
	fis[idx++] |= (request->u.ata.lba >> 24 & 0x0f);

    fis[idx++] = request->u.ata.lba >> 24;
    fis[idx++] = request->u.ata.lba >> 32; 
    fis[idx++] = request->u.ata.lba >> 40; 
    fis[idx++] = request->u.ata.feature >> 8;

    fis[idx++] = request->u.ata.count;
    fis[idx++] = request->u.ata.count >> 8;
    fis[idx++] = 0x00;
    fis[idx++] = ATA_A_4BIT;

    fis[idx++] = 0x00;
    fis[idx++] = 0x00;
    fis[idx++] = 0x00;
    fis[idx++] = 0x00;
    return idx;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_ahci_begin_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_ahci_cmd_tab *ctp;
    struct ata_ahci_cmd_list *clp;
    int fis_size, entries;
    int tag = 0;

    /* get a piece of the workspace for this request */
    ctp = (struct ata_ahci_cmd_tab *)
	  (ch->dma->work + ATA_AHCI_CT_OFFSET + (ATA_AHCI_CT_SIZE * tag));

    /* setup the FIS for this request */ /* XXX SOS ATAPI missing still */
    if (!(fis_size = ata_ahci_setup_fis(&ctp->cfis[0], request))) {
	device_printf(request->dev, "setting up SATA FIS failed\n");
	request->result = EIO;
	return ATA_OP_FINISHED;
    }

    /* if request moves data setup and load SG list */
    if (request->flags & (ATA_R_READ | ATA_R_WRITE)) {
	if (ch->dma->load(ch->dev, request->data, request->bytecount,
			  request->flags & ATA_R_READ,
			  ctp->prd_tab, &entries)) {
	    device_printf(request->dev, "setting up DMA failed\n");
	    request->result = EIO;
	    return ATA_OP_FINISHED;
	}
    }

    /* setup the command list entry */
    clp = (struct ata_ahci_cmd_list *)
	  (ch->dma->work + ATA_AHCI_CL_OFFSET + (ATA_AHCI_CL_SIZE * tag));

    clp->prd_length = entries;
    clp->cmd_flags = (request->flags & ATA_R_WRITE ? (1<<6) : 0) |
		     (request->flags & ATA_R_ATAPI ? (1<<5) : 0) |
		     (fis_size / sizeof(u_int32_t));
    clp->bytecount = 0;
    clp->cmd_table_phys = htole64(ch->dma->work_bus + ATA_AHCI_CT_OFFSET +
				  (ATA_AHCI_CT_SIZE * tag));

    /* clear eventual ACTIVE bit */
    ATA_IDX_OUTL(ch, ATA_SACTIVE, ATA_IDX_INL(ch, ATA_SACTIVE) & (1 << tag));

    /* issue the command */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CI + (ch->unit << 7), (1 << tag));

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
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_ahci_cmd_list *clp;
    u_int32_t tf_data;
    int tag = 0;

    /* kill the timeout */
    callout_stop(&request->callout);

    /* get status */
    tf_data = ATA_INL(ctlr->r_res2, ATA_AHCI_P_TFD + (ch->unit << 7));
    request->status = tf_data;

    /* if error status get details */
    if (request->status & ATA_S_ERROR)  
	request->error = tf_data >> 8;

    /* record how much data we actually moved */
    clp = (struct ata_ahci_cmd_list *)
	  (ch->dma->work + ATA_AHCI_CL_OFFSET + (ATA_AHCI_CL_SIZE * tag));
    request->donecount = clp->bytecount;

    /* release SG list etc */
    ch->dma->unload(ch->dev);

    return ATA_OP_FINISHED;
}

static void
ata_ahci_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    u_int32_t port, status, error, issued;
    int unit;
    int tag = 0;

    port = ATA_INL(ctlr->r_res2, ATA_AHCI_IS);

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (port & (1 << unit)) {
	    if ((ch = ctlr->interrupt[unit].argument)) {
		struct ata_connect_task *tp;
		int offset = (ch->unit << 7);

		error = ATA_INL(ctlr->r_res2, ATA_AHCI_P_SERR + offset);
		ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_SERR + offset, error);
		status = ATA_INL(ctlr->r_res2, ATA_AHCI_P_IS + offset);
		ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IS + offset, status);
		issued = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CI + offset);

		/* do we have cold connect surprise */
		if (status & ATA_AHCI_P_IX_CPD) {
		    printf("ata_ahci_intr status=%08x error=%08x issued=%08x\n",
			   status, error, issued);
		}

		/* check for and handle connect events */
		if ((status & ATA_AHCI_P_IX_PC) &&
		    (tp = (struct ata_connect_task *)
			  malloc(sizeof(struct ata_connect_task),
				 M_ATA, M_NOWAIT | M_ZERO))) {
    
		    device_printf(ch->dev, "CONNECT requested\n");
		    tp->action = ATA_C_ATTACH;
		    tp->dev = ch->dev;
		    TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		    taskqueue_enqueue(taskqueue_thread, &tp->task);
		}

		/* check for and handle disconnect events */
		if (((status & (ATA_AHCI_P_IX_PRC | ATA_AHCI_P_IX_PC)) ==
		     ATA_AHCI_P_IX_PRC) &&
		    (tp = (struct ata_connect_task *)
			  malloc(sizeof(struct ata_connect_task),
			       M_ATA, M_NOWAIT | M_ZERO))) {
    
		    device_printf(ch->dev, "DISCONNECT requested\n");
		    tp->action = ATA_C_DETACH;
		    tp->dev = ch->dev;
		    TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		    taskqueue_enqueue(taskqueue_thread, &tp->task);
		}

		/* any drive action to take care of ? */
		if (!(issued & (1<<tag)))
		    ctlr->interrupt[unit].function(ch);
	    }
	}
    }
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_IS, port);
}

static void
ata_ahci_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t cmd;
    int offset = (ch->unit << 7);

    /* kill off all activity on this channel */
    cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
	     cmd & ~(ATA_AHCI_P_CMD_CR | ATA_AHCI_P_CMD_FR |
		     ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST));

    DELAY(500000);      /* XXX SOS this is not entirely wrong */

    /* spin up device */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset, ATA_AHCI_P_CMD_SUD);

    ata_sata_phy_enable(ch);

    /* clear any interrupts pending on this channel */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IS + offset,
	     ATA_INL(ctlr->r_res2, ATA_AHCI_P_IS + offset));

    /* start operations on this channel */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
	     (ATA_AHCI_P_CMD_ACTIVE | ATA_AHCI_P_CMD_FRE |
	      ATA_AHCI_P_CMD_POD | ATA_AHCI_P_CMD_SUD | ATA_AHCI_P_CMD_ST));
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
    args->nsegs = nsegs;
}

static void
ata_ahci_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    if (ch->dma) {
	/* note start and stop are not used here */
	ch->dma->setprd = ata_ahci_dmasetprd;
	ch->dma->max_iosize = 8192 * DEV_BSIZE;
    }
}


/*
 * Acard chipset support functions
 */
int
ata_acard_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_ATP850R, 0, ATPOLD, 0x00, ATA_UDMA2, "Acard ATP850" },
     { ATA_ATP860A, 0, 0,      0x00, ATA_UDMA4, "Acard ATP860A" },
     { ATA_ATP860R, 0, 0,      0x00, ATA_UDMA4, "Acard ATP860R" },
     { ATA_ATP865A, 0, 0,      0x00, ATA_UDMA6, "Acard ATP865A" },
     { ATA_ATP865R, 0, 0,      0x00, ATA_UDMA6, "Acard ATP865R" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64]; 

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_acard_chipinit;
    return 0;
}

static int
ata_acard_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    int rid = ATA_IRQ_RID;

    if (!(ctlr->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					       RF_SHAREABLE | RF_ACTIVE))) {
	device_printf(dev, "unable to map interrupt\n");
	return ENXIO;
    }
    if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			ata_acard_intr, ctlr, &ctlr->handle))) {
	device_printf(dev, "unable to setup interrupt\n");
	return ENXIO;
    }
    if (ctlr->chip->cfg1 == ATPOLD) {
	ctlr->setmode = ata_acard_850_setmode;
	ctlr->locking = ata_serialize;
    }
    else
	ctlr->setmode = ata_acard_86X_setmode;
    return 0;
}

static void
ata_acard_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (ctlr->chip->cfg1 == ATPOLD &&
	    ATA_LOCKING(ch->dev, ATA_LF_WHICH) != unit)
	    continue;
	if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
	    int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	    if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		ATA_BMSTAT_INTERRUPT)
		continue;
	    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	    DELAY(1);
	    ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
			 ATA_IDX_INB(ch, ATA_BMCMD_PORT)&~ATA_BMCMD_START_STOP);
	    DELAY(1);
	}
	ctlr->interrupt[unit].function(ch);
    }
}

static void
ata_acard_850_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;

    mode = ata_limit_mode(dev, mode,
			  ata_atapi(dev) ? ATA_PIO_MAX : ctlr->chip->max_dma);

    /* XXX SOS missing WDMA0+1 + PIO modes */
    if (mode >= ATA_WDMA2) {
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	    device_printf(dev, "%ssetting %s on %s chip\n",
			  (error) ? "FAILURE " : "",
			  ata_mode2str(mode), ctlr->chip->text);
	if (!error) {
	    u_int8_t reg54 = pci_read_config(gparent, 0x54, 1);
	    
	    reg54 &= ~(0x03 << (devno << 1));
	    if (mode >= ATA_UDMA0)
		reg54 |= (((mode & ATA_MODE_MASK) + 1) << (devno << 1));
	    pci_write_config(gparent, 0x54, reg54, 1);
	    pci_write_config(gparent, 0x4a, 0xa6, 1);
	    pci_write_config(gparent, 0x40 + (devno << 1), 0x0301, 2);
	    atadev->mode = mode;
	    return;
	}
    }
    /* we could set PIO mode timings, but we assume the BIOS did that */
}

static void
ata_acard_86X_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;


    mode = ata_limit_mode(dev, mode,
			  ata_atapi(dev) ? ATA_PIO_MAX : ctlr->chip->max_dma);

    mode = ata_check_80pin(dev, mode);

    /* XXX SOS missing WDMA0+1 + PIO modes */
    if (mode >= ATA_WDMA2) {
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	    device_printf(dev, "%ssetting %s on %s chip\n",
			  (error) ? "FAILURE " : "",
			  ata_mode2str(mode), ctlr->chip->text);
	if (!error) {
	    u_int16_t reg44 = pci_read_config(gparent, 0x44, 2);
	    
	    reg44 &= ~(0x000f << (devno << 2));
	    if (mode >= ATA_UDMA0)
		reg44 |= (((mode & ATA_MODE_MASK) + 1) << (devno << 2));
	    pci_write_config(gparent, 0x44, reg44, 2);
	    pci_write_config(gparent, 0x4a, 0xa6, 1);
	    pci_write_config(gparent, 0x40 + devno, 0x31, 1);
	    atadev->mode = mode;
	    return;
	}
    }
    /* we could set PIO mode timings, but we assume the BIOS did that */
}


/*
 * Acer Labs Inc (ALI) chipset support functions
 */
int
ata_ali_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_ALI_5289, 0x00, 2, ALISATA, ATA_SA150, "AcerLabs M5289" },
     { ATA_ALI_5287, 0x00, 4, ALISATA, ATA_SA150, "AcerLabs M5287" },
     { ATA_ALI_5281, 0x00, 2, ALISATA, ATA_SA150, "AcerLabs M5281" },
     { ATA_ALI_5229, 0xc5, 0, ALINEW,  ATA_UDMA6, "AcerLabs M5229" },
     { ATA_ALI_5229, 0xc4, 0, ALINEW,  ATA_UDMA5, "AcerLabs M5229" },
     { ATA_ALI_5229, 0xc2, 0, ALINEW,  ATA_UDMA4, "AcerLabs M5229" },
     { ATA_ALI_5229, 0x20, 0, ALIOLD,  ATA_UDMA2, "AcerLabs M5229" },
     { ATA_ALI_5229, 0x00, 0, ALIOLD,  ATA_WDMA2, "AcerLabs M5229" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64]; 

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_ali_chipinit;
    return 0;
}

static int
ata_ali_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    switch (ctlr->chip->cfg2) {
    case ALISATA:
	pci_write_config(dev, PCIR_COMMAND,
			 pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);
	ctlr->channels = ctlr->chip->cfg1;
	ctlr->allocate = ata_ali_sata_allocate;
	ctlr->setmode = ata_sata_setmode;
	break;

    case ALINEW:
	/* use device interrupt as byte count end */
	pci_write_config(dev, 0x4a, pci_read_config(dev, 0x4a, 1) | 0x20, 1);

	/* enable cable detection and UDMA support on newer chips */
	pci_write_config(dev, 0x4b, pci_read_config(dev, 0x4b, 1) | 0x09, 1);

	/* enable ATAPI UDMA mode */
	pci_write_config(dev, 0x53, pci_read_config(dev, 0x53, 1) | 0x01, 1);

	/* only chips with revision > 0xc4 can do 48bit DMA */
	if (ctlr->chip->chiprev <= 0xc4)
	    device_printf(dev,
			  "using PIO transfers above 137GB as workaround for "
			  "48bit DMA access bug, expect reduced performance\n");
	ctlr->reset = ata_ali_reset;
	ctlr->allocate = ata_ali_allocate;
	ctlr->setmode = ata_ali_setmode;
	break;

    case ALIOLD:
	/* deactivate the ATAPI FIFO and enable ATAPI UDMA */
	pci_write_config(dev, 0x53, pci_read_config(dev, 0x53, 1) | 0x03, 1);
	ctlr->setmode = ata_ali_setmode;
	break;
    }
    return 0;
}

static int
ata_ali_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    ata_pci_allocate(dev);

    if (ctlr->chip->chiprev <= 0xc4)
	ch->flags |= ATA_NO_48BIT_DMA;

    return 0;
}

static int
ata_ali_sata_allocate(device_t dev)
{
    device_t parent = device_get_parent(dev);
    struct ata_pci_controller *ctlr = device_get_softc(parent);
    struct ata_channel *ch = device_get_softc(dev);
    struct resource *io = NULL, *ctlio = NULL;
    int unit01 = (ch->unit & 1), unit10 = (ch->unit & 2);
    int i, rid;
		
    rid = PCIR_BAR(0) + (unit01 ? 8 : 0);
    io = bus_alloc_resource_any(parent, SYS_RES_IOPORT, &rid, RF_ACTIVE);
    if (!io)
	return ENXIO;

    rid = PCIR_BAR(1) + (unit01 ? 8 : 0);
    ctlio = bus_alloc_resource_any(parent, SYS_RES_IOPORT, &rid, RF_ACTIVE);
    if (!ctlio) {
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, io);
	return ENXIO;
    }
		
    for (i = ATA_DATA; i <= ATA_COMMAND; i ++) {
	ch->r_io[i].res = io;
	ch->r_io[i].offset = i + (unit10 ? 8 : 0);
    }
    ch->r_io[ATA_CONTROL].res = ctlio;
    ch->r_io[ATA_CONTROL].offset = 2 + (unit10 ? 4 : 0);
    ch->r_io[ATA_IDX_ADDR].res = io;
    ata_default_registers(dev);
    if (ctlr->r_res1) {
	for (i = ATA_BMCMD_PORT; i <= ATA_BMDTP_PORT; i++) {
	    ch->r_io[i].res = ctlr->r_res1;
	    ch->r_io[i].offset = (i - ATA_BMCMD_PORT)+(ch->unit * ATA_BMIOSIZE);
	}
    }
    ch->flags |= ATA_NO_SLAVE;

    /* XXX SOS PHY handling awkward in ALI chip not supported yet */
    ata_generic_hw(dev);
    return 0;
}

static void
ata_ali_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int nchildren, i;

    ata_generic_reset(dev);

    /*
     * workaround for datacorruption bug found on at least SUN Blade-100
     * find the ISA function on the southbridge and disable then enable
     * the ATA channel tristate buffer
     */
    if (ctlr->chip->chiprev == 0xc3 || ctlr->chip->chiprev == 0xc2) {
	if (!device_get_children(GRANDPARENT(dev), &children, &nchildren)) {
	    for (i = 0; i < nchildren; i++) {
		if (pci_get_devid(children[i]) == ATA_ALI_1533) {
		    pci_write_config(children[i], 0x58, 
				     pci_read_config(children[i], 0x58, 1) &
				     ~(0x04 << ch->unit), 1);
		    pci_write_config(children[i], 0x58, 
				     pci_read_config(children[i], 0x58, 1) |
				     (0x04 << ch->unit), 1);
		    break;
		}
	    }
	    free(children, M_TEMP);
	}
    }
}

static void
ata_ali_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if (ctlr->chip->cfg2 & ALINEW) {
	if (mode > ATA_UDMA2 &&
	    pci_read_config(gparent, 0x4a, 1) & (1 << ch->unit)) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
    }
    else
	mode = ata_check_80pin(dev, mode);

    if (ctlr->chip->cfg2 & ALIOLD) {
	/* doesn't support ATAPI DMA on write */
	ch->flags |= ATA_ATAPI_DMA_RO;
	if (ch->devices & ATA_ATAPI_MASTER && ch->devices & ATA_ATAPI_SLAVE) {
	    /* doesn't support ATAPI DMA on two ATAPI devices */
	    device_printf(dev, "two atapi devices on this channel, no DMA\n");
	    mode = ata_limit_mode(dev, mode, ATA_PIO_MAX);
	}
    }

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		   (error) ? "FAILURE " : "", 
		   ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    u_int8_t udma[] = {0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x0f, 0x0d};
	    u_int32_t word54 = pci_read_config(gparent, 0x54, 4);

	    word54 &= ~(0x000f000f << (devno << 2));
	    word54 |= (((udma[mode&ATA_MODE_MASK]<<16)|0x05)<<(devno<<2));
	    pci_write_config(gparent, 0x54, word54, 4);
	    pci_write_config(gparent, 0x58 + (ch->unit << 2),
			     0x00310001, 4);
	}
	else {
	    u_int32_t piotimings[] =
		{ 0x006d0003, 0x00580002, 0x00440001, 0x00330001,
		  0x00310001, 0x00440001, 0x00330001, 0x00310001};

	    pci_write_config(gparent, 0x54, pci_read_config(gparent, 0x54, 4) &
					    ~(0x0008000f << (devno << 2)), 4);
	    pci_write_config(gparent, 0x58 + (ch->unit << 2),
			     piotimings[ata_mode2idx(mode)], 4);
	}
	atadev->mode = mode;
    }
}


/*
 * American Micro Devices (AMD) chipset support functions
 */
int
ata_amd_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_AMD756,  0x00, AMDNVIDIA, 0x00,            ATA_UDMA4, "AMD 756" },
     { ATA_AMD766,  0x00, AMDNVIDIA, AMDCABLE|AMDBUG, ATA_UDMA5, "AMD 766" },
     { ATA_AMD768,  0x00, AMDNVIDIA, AMDCABLE,        ATA_UDMA5, "AMD 768" },
     { ATA_AMD8111, 0x00, AMDNVIDIA, AMDCABLE,        ATA_UDMA6, "AMD 8111" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64]; 

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_amd_chipinit;
    return 0;
}

static int
ata_amd_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    /* disable/set prefetch, postwrite */
    if (ctlr->chip->cfg2 & AMDBUG)
	pci_write_config(dev, 0x41, pci_read_config(dev, 0x41, 1) & 0x0f, 1);
    else
	pci_write_config(dev, 0x41, pci_read_config(dev, 0x41, 1) | 0xf0, 1);

    ctlr->setmode = ata_via_family_setmode;
    return 0;
}


/*
 * Cyrix chipset support functions
 */
int
ata_cyrix_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (pci_get_devid(dev) == ATA_CYRIX_5530) {
	device_set_desc(dev, "Cyrix 5530 ATA33 controller");
	ctlr->chipinit = ata_cyrix_chipinit;
	return 0;
    }
    return ENXIO;
}

static int
ata_cyrix_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    if (ctlr->r_res1)
	ctlr->setmode = ata_cyrix_setmode;
    else
	ctlr->setmode = ata_generic_setmode;
    return 0;
}

static void
ata_cyrix_setmode(device_t dev, int mode)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    u_int32_t piotiming[] = 
	{ 0x00009172, 0x00012171, 0x00020080, 0x00032010, 0x00040010 };
    u_int32_t dmatiming[] = { 0x00077771, 0x00012121, 0x00002020 };
    u_int32_t udmatiming[] = { 0x00921250, 0x00911140, 0x00911030 };
    int error;

    ch->dma->alignment = 16;
    ch->dma->max_iosize = 126 * DEV_BSIZE;

    mode = ata_limit_mode(dev, mode, ATA_UDMA2);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on Cyrix chip\n",
		      (error) ? "FAILURE " : "", ata_mode2str(mode));
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    ATA_OUTL(ch->r_io[ATA_BMCMD_PORT].res,
		     0x24 + (devno << 3), udmatiming[mode & ATA_MODE_MASK]);
	}
	else if (mode >= ATA_WDMA0) {
	    ATA_OUTL(ch->r_io[ATA_BMCMD_PORT].res,
		     0x24 + (devno << 3), dmatiming[mode & ATA_MODE_MASK]);
	}
	else {
	    ATA_OUTL(ch->r_io[ATA_BMCMD_PORT].res,
		     0x20 + (devno << 3), piotiming[mode & ATA_MODE_MASK]);
	}
	atadev->mode = mode;
    }
}


/*
 * Cypress chipset support functions
 */
int
ata_cypress_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    /*
     * the Cypress chip is a mess, it contains two ATA functions, but
     * both channels are visible on the first one.
     * simply ignore the second function for now, as the right
     * solution (ignoring the second channel on the first function)
     * doesn't work with the crappy ATA interrupt setup on the alpha.
     */
    if (pci_get_devid(dev) == ATA_CYPRESS_82C693 &&
	pci_get_function(dev) == 1 &&
	pci_get_subclass(dev) == PCIS_STORAGE_IDE) {
	device_set_desc(dev, "Cypress 82C693 ATA controller");
	ctlr->chipinit = ata_cypress_chipinit;
	return 0;
    }
    return ENXIO;
}

static int
ata_cypress_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    ctlr->setmode = ata_cypress_setmode;
    return 0;
}

static void
ata_cypress_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int error;

    mode = ata_limit_mode(dev, mode, ATA_WDMA2);

    /* XXX SOS missing WDMA0+1 + PIO modes */
    if (mode == ATA_WDMA2) { 
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	    device_printf(dev, "%ssetting WDMA2 on Cypress chip\n",
			  error ? "FAILURE " : "");
	if (!error) {
	    pci_write_config(gparent, ch->unit ? 0x4e : 0x4c, 0x2020, 2);
	    atadev->mode = mode;
	    return;
	}
    }
    /* we could set PIO mode timings, but we assume the BIOS did that */
}


/*
 * HighPoint chipset support functions
 */
int
ata_highpoint_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_HPT374, 0x07, HPT374, 0x00,   ATA_UDMA6, "HighPoint HPT374" },
     { ATA_HPT372, 0x02, HPT372, 0x00,   ATA_UDMA6, "HighPoint HPT372N" },
     { ATA_HPT372, 0x01, HPT372, 0x00,   ATA_UDMA6, "HighPoint HPT372" },
     { ATA_HPT371, 0x01, HPT372, 0x00,   ATA_UDMA6, "HighPoint HPT371" },
     { ATA_HPT366, 0x05, HPT372, 0x00,   ATA_UDMA6, "HighPoint HPT372" },
     { ATA_HPT366, 0x03, HPT370, 0x00,   ATA_UDMA5, "HighPoint HPT370" },
     { ATA_HPT366, 0x02, HPT366, 0x00,   ATA_UDMA4, "HighPoint HPT368" },
     { ATA_HPT366, 0x00, HPT366, HPTOLD, ATA_UDMA4, "HighPoint HPT366" },
     { ATA_HPT302, 0x01, HPT372, 0x00,   ATA_UDMA6, "HighPoint HPT302" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64];

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    strcpy(buffer, idx->text);
    if (idx->cfg1 == HPT374) {
	if (pci_get_function(dev) == 0)
	    strcat(buffer, " (channel 0+1)");
	else if (pci_get_function(dev) == 1)
	    strcat(buffer, " (channel 2+3)");
    }
    sprintf(buffer, "%s %s controller", buffer, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_highpoint_chipinit;
    return 0;
}

static int
ata_highpoint_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    int rid = ATA_IRQ_RID;

    if (!(ctlr->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					       RF_SHAREABLE | RF_ACTIVE))) {
	device_printf(dev, "unable to map interrupt\n");
	return ENXIO;
    }
    if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			ata_highpoint_intr, ctlr, &ctlr->handle))) {
	device_printf(dev, "unable to setup interrupt\n");
	return ENXIO;
    }

    if (ctlr->chip->cfg2 == HPTOLD) {
	/* disable interrupt prediction */
	pci_write_config(dev, 0x51, (pci_read_config(dev, 0x51, 1) & ~0x80), 1);
    }
    else {
	/* disable interrupt prediction */
	pci_write_config(dev, 0x51, (pci_read_config(dev, 0x51, 1) & ~0x03), 1);
	pci_write_config(dev, 0x55, (pci_read_config(dev, 0x55, 1) & ~0x03), 1);

	/* enable interrupts */
	pci_write_config(dev, 0x5a, (pci_read_config(dev, 0x5a, 1) & ~0x10), 1);

	/* set clocks etc */
	if (ctlr->chip->cfg1 < HPT372)
	    pci_write_config(dev, 0x5b, 0x22, 1);
	else
	    pci_write_config(dev, 0x5b,
			     (pci_read_config(dev, 0x5b, 1) & 0x01) | 0x20, 1);
    }
    ctlr->setmode = ata_highpoint_setmode;
    return 0;
}

static void
ata_highpoint_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (ch->dma) {
	    int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	    if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		ATA_BMSTAT_INTERRUPT)
		continue;
	    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	    DELAY(1);
	}
	ctlr->interrupt[unit].function(ch);
    }
}

static void
ata_highpoint_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;
    u_int32_t timings33[][4] = {
    /*    HPT366      HPT370      HPT372      HPT374               mode */
	{ 0x40d0a7aa, 0x06914e57, 0x0d029d5e, 0x0ac1f48a },     /* PIO 0 */
	{ 0x40d0a7a3, 0x06914e43, 0x0d029d26, 0x0ac1f465 },     /* PIO 1 */
	{ 0x40d0a753, 0x06514e33, 0x0c829ca6, 0x0a81f454 },     /* PIO 2 */
	{ 0x40c8a742, 0x06514e22, 0x0c829c84, 0x0a81f443 },     /* PIO 3 */
	{ 0x40c8a731, 0x06514e21, 0x0c829c62, 0x0a81f442 },     /* PIO 4 */
	{ 0x20c8a797, 0x26514e97, 0x2c82922e, 0x228082ea },     /* MWDMA 0 */
	{ 0x20c8a732, 0x26514e33, 0x2c829266, 0x22808254 },     /* MWDMA 1 */
	{ 0x20c8a731, 0x26514e21, 0x2c829262, 0x22808242 },     /* MWDMA 2 */
	{ 0x10c8a731, 0x16514e31, 0x1c829c62, 0x121882ea },     /* UDMA 0 */
	{ 0x10cba731, 0x164d4e31, 0x1c9a9c62, 0x12148254 },     /* UDMA 1 */
	{ 0x10caa731, 0x16494e31, 0x1c929c62, 0x120c8242 },     /* UDMA 2 */
	{ 0x10cfa731, 0x166d4e31, 0x1c8e9c62, 0x128c8242 },     /* UDMA 3 */
	{ 0x10c9a731, 0x16454e31, 0x1c8a9c62, 0x12ac8242 },     /* UDMA 4 */
	{ 0,          0x16454e31, 0x1c8a9c62, 0x12848242 },     /* UDMA 5 */
	{ 0,          0,          0x1c869c62, 0x12808242 }      /* UDMA 6 */
    };

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if (ctlr->chip->cfg1 == HPT366 && ata_atapi(dev))
	mode = ata_limit_mode(dev, mode, ATA_PIO_MAX);

    mode = ata_highpoint_check_80pin(dev, mode);

    /*
     * most if not all HPT chips cant really handle that the device is
     * running at ATA_UDMA6/ATA133 speed, so we cheat at set the device to
     * a max of ATA_UDMA5/ATA100 to guard against suboptimal performance
     */
    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0,
			   ata_limit_mode(dev, mode, ATA_UDMA5));
    if (bootverbose)
	device_printf(dev, "%ssetting %s on HighPoint chip\n",
		      (error) ? "FAILURE " : "", ata_mode2str(mode));
    if (!error)
	pci_write_config(gparent, 0x40 + (devno << 2),
			 timings33[ata_mode2idx(mode)][ctlr->chip->cfg1], 4);
    atadev->mode = mode;
}

static int
ata_highpoint_check_80pin(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    u_int8_t reg, val, res;

    if (ctlr->chip->cfg1 == HPT374 && pci_get_function(gparent) == 1) {
	reg = ch->unit ? 0x57 : 0x53;
	val = pci_read_config(gparent, reg, 1);
	pci_write_config(gparent, reg, val | 0x80, 1);
    }
    else {
	reg = 0x5b;
	val = pci_read_config(gparent, reg, 1);
	pci_write_config(gparent, reg, val & 0xfe, 1);
    }
    res = pci_read_config(gparent, 0x5a, 1) & (ch->unit ? 0x1:0x2);
    pci_write_config(gparent, reg, val, 1);

    if (mode > ATA_UDMA2 && res) {
	ata_print_cable(dev, "controller");
	mode = ATA_UDMA2;
    }
    return mode;
}


/*
 * Intel chipset support functions
 */
int
ata_intel_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_I82371FB,    0, 0, 0x00, ATA_WDMA2, "Intel PIIX" },
     { ATA_I82371SB,    0, 0, 0x00, ATA_WDMA2, "Intel PIIX3" },
     { ATA_I82371AB,    0, 0, 0x00, ATA_UDMA2, "Intel PIIX4" },
     { ATA_I82443MX,    0, 0, 0x00, ATA_UDMA2, "Intel PIIX4" },
     { ATA_I82451NX,    0, 0, 0x00, ATA_UDMA2, "Intel PIIX4" },
     { ATA_I82801AB,    0, 0, 0x00, ATA_UDMA2, "Intel ICH0" },
     { ATA_I82801AA,    0, 0, 0x00, ATA_UDMA4, "Intel ICH" },
     { ATA_I82372FB,    0, 0, 0x00, ATA_UDMA4, "Intel ICH" },
     { ATA_I82801BA,    0, 0, 0x00, ATA_UDMA5, "Intel ICH2" },
     { ATA_I82801BA_1,  0, 0, 0x00, ATA_UDMA5, "Intel ICH2" },
     { ATA_I82801CA,    0, 0, 0x00, ATA_UDMA5, "Intel ICH3" },
     { ATA_I82801CA_1,  0, 0, 0x00, ATA_UDMA5, "Intel ICH3" },
     { ATA_I82801DB,    0, 0, 0x00, ATA_UDMA5, "Intel ICH4" },
     { ATA_I82801DB_1,  0, 0, 0x00, ATA_UDMA5, "Intel ICH4" },
     { ATA_I82801EB,    0, 0, 0x00, ATA_UDMA5, "Intel ICH5" },
     { ATA_I82801EB_S1, 0, 0, 0x00, ATA_SA150, "Intel ICH5" },
     { ATA_I82801EB_R1, 0, 0, 0x00, ATA_SA150, "Intel ICH5" },
     { ATA_I6300ESB,    0, 0, 0x00, ATA_UDMA5, "Intel 6300ESB" },
     { ATA_I6300ESB_S1, 0, 0, 0x00, ATA_SA150, "Intel 6300ESB" },
     { ATA_I6300ESB_R1, 0, 0, 0x00, ATA_SA150, "Intel 6300ESB" },
     { ATA_I82801FB,    0, 0, 0x00, ATA_UDMA5, "Intel ICH6" },
     { ATA_I82801FB_S1, 0, 0, 0x00, ATA_SA150, "Intel ICH6" },
     { ATA_I82801FB_R1, 0, 0, 0x00, ATA_SA150, "Intel ICH6" },
     { ATA_I82801FB_M,  0, 0, 0x00, ATA_SA150, "Intel ICH6" },
     { ATA_I82801GB,    0, 0, 0x00, ATA_UDMA5, "Intel ICH7" },
     { ATA_I82801GB_S1, 0, 0, 0x00, ATA_SA150, "Intel ICH7" },
     { ATA_I82801GB_R1, 0, 0, 0x00, ATA_SA150, "Intel ICH7" },
     { ATA_I82801GB_M,  0, 0, 0x00, ATA_SA150, "Intel ICH7" },
     { ATA_I82801GB_AH, 0, 0, 0x00, ATA_SA150, "Intel ICH7" },
     { ATA_I31244,	0, 0, 0x00, ATA_SA150, "Intel 31244" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64]; 

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_intel_chipinit;
    return 0;
}

static int
ata_intel_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    int rid = ATA_IRQ_RID;

    if (!ata_legacy(dev)) {
	if (!(ctlr->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
						   RF_SHAREABLE | RF_ACTIVE))) {
	    device_printf(dev, "unable to map interrupt\n");
	    return ENXIO;
	}
	if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			    ata_intel_intr, ctlr, &ctlr->handle))) {
	    device_printf(dev, "unable to setup interrupt\n");
	    return ENXIO;
	}
    }

    /* good old PIIX needs special treatment (not implemented) */
    if (ctlr->chip->chipid == ATA_I82371FB) {
	ctlr->setmode = ata_intel_old_setmode;
    }

    /* the intel 31244 needs special care if in DPA mode */
    else if (ctlr->chip->chipid == ATA_I31244) {
	if (pci_get_subclass(dev) != PCIS_STORAGE_IDE) {
	    ctlr->r_type2 = SYS_RES_MEMORY;
	    ctlr->r_rid2 = PCIR_BAR(0);
	    if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						        &ctlr->r_rid2,
							RF_ACTIVE)))
	        return ENXIO;
    	
	    if (bus_teardown_intr(dev, ctlr->r_irq, ctlr->handle) ||
	        bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			       ata_intel_31244_intr, ctlr, &ctlr->handle)) {
	        device_printf(dev, "unable to setup interrupt\n");
	        return ENXIO;
	    }
	    ctlr->channels = 4;
	    ctlr->reset = ata_intel_31244_reset;
	    ctlr->allocate = ata_intel_31244_allocate;
	}
	ctlr->setmode = ata_sata_setmode;
    }

    /* non SATA intel chips goes here */
    else if (ctlr->chip->max_dma < ATA_SA150) {
	ctlr->setmode = ata_intel_new_setmode;
    }

    /* SATA parts can be either compat or AHCI */
    else {
	/* if we have BAR(5) as a memory resource we should use AHCI mode */
	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE))) {
	    if (bus_teardown_intr(dev, ctlr->r_irq, ctlr->handle) ||
		bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
				ata_ahci_intr, ctlr, &ctlr->handle)) {
		device_printf(dev, "unable to setup interrupt\n");
		return ENXIO;
	    }

	    /* force all ports active "the legacy way" */
	    pci_write_config(dev, 0x92, pci_read_config(dev, 0x92, 2) | 0x0f,2);

	    /* enable AHCI mode */
	    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC, ATA_AHCI_GHC_AE);

	    /* get the number of HW channels */
	    ctlr->channels = (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) &
			      ATA_AHCI_NPMASK) + 1;

	    /* enable AHCI interrupts */
	    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC,
		     ATA_INL(ctlr->r_res2, ATA_AHCI_GHC) | ATA_AHCI_GHC_IE);

	    ctlr->reset = ata_ahci_reset;
	    ctlr->dmainit = ata_ahci_dmainit;
	    ctlr->allocate = ata_ahci_allocate;
	}
	else {
	    ctlr->reset = ata_intel_reset;
	}
	ctlr->setmode = ata_sata_setmode;
	pci_write_config(dev, PCIR_COMMAND,
			 pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);
    }
    return 0;
}

static int
ata_intel_31244_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int i;
    int ch_offset;

    ch_offset = 0x200 + ch->unit * 0x200;

    for (i = ATA_DATA; i < ATA_MAX_RES; i++)
	ch->r_io[i].res = ctlr->r_res2;
    ch->r_io[ATA_DATA].offset = ch_offset + 0x00;
    ch->r_io[ATA_FEATURE].offset = ch_offset + 0x06;
    ch->r_io[ATA_COUNT].offset = ch_offset + 0x08;
    ch->r_io[ATA_SECTOR].offset = ch_offset + 0x0c;
    ch->r_io[ATA_CYL_LSB].offset = ch_offset + 0x10;
    ch->r_io[ATA_CYL_MSB].offset = ch_offset + 0x14;
    ch->r_io[ATA_DRIVE].offset = ch_offset + 0x18;
    ch->r_io[ATA_COMMAND].offset = ch_offset + 0x1d;
    ch->r_io[ATA_ERROR].offset = ch_offset + 0x04;
    ch->r_io[ATA_STATUS].offset = ch_offset + 0x1c;
    ch->r_io[ATA_ALTSTAT].offset = ch_offset + 0x28;
    ch->r_io[ATA_CONTROL].offset = ch_offset + 0x29;
    ch->r_io[ATA_SSTATUS].offset = ch_offset + 0x100;
    ch->r_io[ATA_SERROR].offset = ch_offset + 0x104;
    ch->r_io[ATA_SCONTROL].offset = ch_offset + 0x108;
    ch->r_io[ATA_BMCMD_PORT].offset = ch_offset + 0x70;
    ch->r_io[ATA_BMSTAT_PORT].offset = ch_offset + 0x72;
    ch->r_io[ATA_BMDTP_PORT].offset = ch_offset + 0x74;

    ch->flags |= ATA_NO_SLAVE;
    ata_generic_hw(dev);
    ch->hw.command = ata_intel_31244_command;

    /* enable PHY state change interrupt */
    ATA_OUTL(ctlr->r_res2, 0x4,
	     ATA_INL(ctlr->r_res2, 0x04) | (0x01 << (ch->unit << 3)));
    return 0;
}

static void
ata_intel_31244_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;

	/* check for PHY related interrupts on SATA capable HW */
	if (ctlr->chip->max_dma >= ATA_SA150) {
	    u_int32_t status = ATA_IDX_INL(ch, ATA_SSTATUS);
	    u_int32_t error = ATA_IDX_INL(ch, ATA_SERROR);
	    struct ata_connect_task *tp;

	    if (error) {
		/* clear error bits/interrupt */
		ATA_IDX_OUTL(ch, ATA_SERROR, error);

		/* if we have a connection event deal with it */
		if ((error & ATA_SE_PHY_CHANGED) &&
		    (tp = (struct ata_connect_task *)
			  malloc(sizeof(struct ata_connect_task),
				 M_ATA, M_NOWAIT | M_ZERO))) {

		    if ((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN1) {
			device_printf(ch->dev, "CONNECT requested\n");
			tp->action = ATA_C_ATTACH;
		    }
		    else {
			device_printf(ch->dev, "DISCONNECT requested\n");
			tp->action = ATA_C_DETACH;
		    }
		    tp->dev = ch->dev;
		    TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		    taskqueue_enqueue(taskqueue_thread, &tp->task);
		}
	    }
	}

	/* any drive action to take care of ? */
	if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
	    int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	    if (!(bmstat & ATA_BMSTAT_INTERRUPT))
		continue;
	    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	    DELAY(1);
	}
	ctlr->interrupt[unit].function(ch);
    }
}

static void
ata_intel_31244_reset(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_sata_phy_enable(ch);
}

static int
ata_intel_31244_command(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);
    u_int64_t lba;

    if (!(atadev->flags & ATA_D_48BIT_ACTIVE))
	    return (ata_generic_command(request));

    lba = request->u.ata.lba;
    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | atadev->unit);
    /* enable interrupt */
    ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_4BIT);
    ATA_IDX_OUTW(ch, ATA_FEATURE, request->u.ata.feature);
    ATA_IDX_OUTW(ch, ATA_COUNT, request->u.ata.count);
    ATA_IDX_OUTW(ch, ATA_SECTOR, ((lba >> 16) & 0xff00) | (lba & 0x00ff));
    ATA_IDX_OUTW(ch, ATA_CYL_LSB, ((lba >> 24) & 0xff00) |
				  ((lba >> 8) & 0x00ff));
    ATA_IDX_OUTW(ch, ATA_CYL_MSB, ((lba >> 32) & 0xff00) | 
				  ((lba >> 16) & 0x00ff));

    /* issue command to controller */
    ATA_IDX_OUTB(ch, ATA_COMMAND, request->u.ata.command);

    return 0;
}

static void
ata_intel_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (ch->dma) {
	    int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	    if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		ATA_BMSTAT_INTERRUPT)
		continue;
	    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	    DELAY(1);
	}
	ctlr->interrupt[unit].function(ch);
    }
}

static void
ata_intel_reset(device_t dev)
{
    device_t parent = device_get_parent(dev);
    struct ata_pci_controller *ctlr = device_get_softc(parent);
    struct ata_channel *ch = device_get_softc(dev);
    int mask, timeout;

    /* ICH6 has 4 SATA ports as master/slave on 2 channels so deal with pairs */
    if (ctlr->chip->chipid == ATA_I82801FB_S1 ||
	ctlr->chip->chipid == ATA_I82801FB_R1 ||
	ctlr->chip->chipid == ATA_I82801FB_M) {
	mask = (0x0005 << ch->unit);
    }
    else {
	/* ICH5 in compat mode has SATA ports as master/slave on 1 channel */
	if (pci_read_config(parent, 0x90, 1) & 0x04)
	    mask = 0x0003;
	else {
	    mask = (0x0001 << ch->unit);
	    /* XXX SOS should be in intel_allocate when we grow it */
	    ch->flags |= ATA_NO_SLAVE;
	}
    }
    pci_write_config(parent, 0x92, pci_read_config(parent, 0x92, 2) & ~mask, 2);
    DELAY(10);
    pci_write_config(parent, 0x92, pci_read_config(parent, 0x92, 2) | mask, 2);

    /* wait up to 1 sec for "connect well" */
    for (timeout = 0; timeout < 100 ; timeout++) {
	if (((pci_read_config(parent, 0x92, 2) & (mask << 4)) == (mask << 4)) &&
	    (ATA_IDX_INB(ch, ATA_STATUS) != 0xff))
	    break;
	ata_udelay(10000);
    }
    ata_generic_reset(dev);
}

static void
ata_intel_old_setmode(device_t dev, int mode)
{
    /* NOT YET */
}

static void
ata_intel_new_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    u_int32_t reg40 = pci_read_config(gparent, 0x40, 4);
    u_int8_t reg44 = pci_read_config(gparent, 0x44, 1);
    u_int8_t reg48 = pci_read_config(gparent, 0x48, 1);
    u_int16_t reg4a = pci_read_config(gparent, 0x4a, 2);
    u_int16_t reg54 = pci_read_config(gparent, 0x54, 2);
    u_int32_t mask40 = 0, new40 = 0;
    u_int8_t mask44 = 0, new44 = 0;
    int error;
    u_int8_t timings[] = { 0x00, 0x00, 0x10, 0x21, 0x23, 0x10, 0x21, 0x23,
			   0x23, 0x23, 0x23, 0x23, 0x23, 0x23 };

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if ( mode > ATA_UDMA2 && !(reg54 & (0x10 << devno))) {
	ata_print_cable(dev, "controller");
	mode = ATA_UDMA2;
    }

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "",
		      ata_mode2str(mode), ctlr->chip->text);
    if (error)
	return;

    if (mode >= ATA_UDMA0) {
	pci_write_config(gparent, 0x48, reg48 | (0x0001 << devno), 2);
	pci_write_config(gparent, 0x4a,
			 (reg4a & ~(0x3 << (devno << 2))) |
			 ((0x01 + !(mode & 0x01)) << (devno << 2)), 2);
    }
    else {
	pci_write_config(gparent, 0x48, reg48 & ~(0x0001 << devno), 2);
	pci_write_config(gparent, 0x4a, (reg4a & ~(0x3 << (devno << 2))), 2);
    }
    reg54 |= 0x0400;
    if (mode >= ATA_UDMA2)
	pci_write_config(gparent, 0x54, reg54 | (0x1 << devno), 2);
    else
	pci_write_config(gparent, 0x54, reg54 & ~(0x1 << devno), 2);

    if (mode >= ATA_UDMA5)
	pci_write_config(gparent, 0x54, reg54 | (0x1000 << devno), 2);
    else 
	pci_write_config(gparent, 0x54, reg54 & ~(0x1000 << devno), 2);

    reg40 &= ~0x00ff00ff;
    reg40 |= 0x40774077;

    if (atadev->unit == ATA_MASTER) {
	mask40 = 0x3300;
	new40 = timings[ata_mode2idx(mode)] << 8;
    }
    else {
	mask44 = 0x0f;
	new44 = ((timings[ata_mode2idx(mode)] & 0x30) >> 2) |
		(timings[ata_mode2idx(mode)] & 0x03);
    }
    if (ch->unit) {
	mask40 <<= 16;
	new40 <<= 16;
	mask44 <<= 4;
	new44 <<= 4;
    }
    pci_write_config(gparent, 0x40, (reg40 & ~mask40) | new40, 4);
    pci_write_config(gparent, 0x44, (reg44 & ~mask44) | new44, 1);

    atadev->mode = mode;
}


/*
 * Integrated Technology Express Inc. (ITE) chipset support functions
 */
int
ata_ite_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_IT8212F, 0x00, 0x00, 0x00, ATA_UDMA6, "ITE IT8212F" },
     { ATA_IT8211F, 0x00, 0x00, 0x00, ATA_UDMA6, "ITE IT8211F" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64]; 

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_ite_chipinit;
    return 0;
}

static int
ata_ite_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    ctlr->setmode = ata_ite_setmode;

    /* set PCI mode and 66Mhz reference clock */
    pci_write_config(dev, 0x50, pci_read_config(dev, 0x50, 1) & ~0x83, 1);

    /* set default active & recover timings */
    pci_write_config(dev, 0x54, 0x31, 1);
    pci_write_config(dev, 0x56, 0x31, 1);
    return 0;
}
 
static void
ata_ite_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;

    /* correct the mode for what the HW supports */
    mode = ata_limit_mode(dev, mode, ATA_UDMA6);

    /* check the CBLID bits for 80 conductor cable detection */
    if (mode > ATA_UDMA2 && (pci_read_config(gparent, 0x40, 2) &
			     (ch->unit ? (1<<3) : (1<<2)))) {
	ata_print_cable(dev, "controller");
	mode = ATA_UDMA2;
    }

    /* set the wanted mode on the device */
    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%s setting %s on ITE8212F chip\n",
		      (error) ? "failed" : "success", ata_mode2str(mode));

    /* if the device accepted the mode change, setup the HW accordingly */
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    u_int8_t udmatiming[] =
		{ 0x44, 0x42, 0x31, 0x21, 0x11, 0xa2, 0x91 };

	    /* enable UDMA mode */
	    pci_write_config(gparent, 0x50,
			     pci_read_config(gparent, 0x50, 1) &
			     ~(1 << (devno + 3)), 1);

	    /* set UDMA timing */
	    pci_write_config(gparent,
			     0x56 + (ch->unit << 2) + ATA_DEV(atadev->unit),
			     udmatiming[mode & ATA_MODE_MASK], 1);
	}
	else {
	    u_int8_t chtiming[] =
		{ 0xaa, 0xa3, 0xa1, 0x33, 0x31, 0x88, 0x32, 0x31 };

	    /* disable UDMA mode */
	    pci_write_config(gparent, 0x50,
			     pci_read_config(gparent, 0x50, 1) |
			     (1 << (devno + 3)), 1);

	    /* set active and recover timing (shared between master & slave) */
	    if (pci_read_config(gparent, 0x54 + (ch->unit << 2), 1) <
		chtiming[ata_mode2idx(mode)])
		pci_write_config(gparent, 0x54 + (ch->unit << 2),
				 chtiming[ata_mode2idx(mode)], 1);
	}
	atadev->mode = mode;
    }
}


/*
 * National chipset support functions
 */
int
ata_national_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    /* this chip is a clone of the Cyrix chip, bugs and all */
    if (pci_get_devid(dev) == ATA_SC1100) {
	device_set_desc(dev, "National Geode SC1100 ATA33 controller");
	ctlr->chipinit = ata_national_chipinit;
	return 0;
    }
    return ENXIO;
}
    
static device_t nat_host = NULL;

static int
ata_national_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    device_t *children;
    int nchildren, i;
    
    if (ata_setup_interrupt(dev))
	return ENXIO;
		    
    /* locate the ISA part in the southbridge and enable UDMA33 */
    if (!device_get_children(device_get_parent(dev), &children,&nchildren)){
	for (i = 0; i < nchildren; i++) {
	    if (pci_get_devid(children[i]) == 0x0510100b) {
		nat_host = children[i];
		break;
	    }
	}
	free(children, M_TEMP);
    }
    ctlr->setmode = ata_national_setmode;
    return 0;
}

static void
ata_national_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    u_int32_t piotiming[] =
	{ 0x9172d132, 0x21717121, 0x00803020, 0x20102010, 0x00100010,
	  0x00803020, 0x20102010, 0x00100010,
	  0x00100010, 0x00100010, 0x00100010 };
    u_int32_t dmatiming[] = { 0x80077771, 0x80012121, 0x80002020 };
    u_int32_t udmatiming[] = { 0x80921250, 0x80911140, 0x80911030 };
    int error;

    ch->dma->alignment = 16;
    ch->dma->max_iosize = 126 * DEV_BSIZE;

    mode = ata_limit_mode(dev, mode, ATA_UDMA2);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%s setting %s on National chip\n",
		      (error) ? "failed" : "success", ata_mode2str(mode));
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    pci_write_config(gparent, 0x44 + (devno << 3),
			     udmatiming[mode & ATA_MODE_MASK], 4);
	}
	else if (mode >= ATA_WDMA0) {
	    pci_write_config(gparent, 0x44 + (devno << 3),
			     dmatiming[mode & ATA_MODE_MASK], 4);
	}
	else {
	    pci_write_config(gparent, 0x44 + (devno << 3),
			     pci_read_config(gparent, 0x44 + (devno << 3), 4) |
			     0x80000000, 4);
	}
	pci_write_config(gparent, 0x40 + (devno << 3),
			 piotiming[ata_mode2idx(mode)], 4);
	atadev->mode = mode;
    }
}


/*
 * nVidia chipset support functions
 */
int
ata_nvidia_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_NFORCE1,     0, AMDNVIDIA, NVIDIA, ATA_UDMA5, "nVidia nForce" },
     { ATA_NFORCE2,     0, AMDNVIDIA, NVIDIA, ATA_UDMA6, "nVidia nForce2" },
     { ATA_NFORCE2_MCP, 0, AMDNVIDIA, NVIDIA, ATA_UDMA6, "nVidia nForce2 MCP" },
     { ATA_NFORCE2_MCP_S1, 0, 0,      0,      ATA_SA150, "nVidia nForce2 MCP" },
     { ATA_NFORCE3,     0, AMDNVIDIA, NVIDIA, ATA_UDMA6, "nVidia nForce3" },
     { ATA_NFORCE3_PRO, 0, AMDNVIDIA, NVIDIA, ATA_UDMA6, "nVidia nForce3 Pro" },
     { ATA_NFORCE3_PRO_S1, 0, 0,      0,      ATA_SA150, "nVidia nForce3 Pro" },
     { ATA_NFORCE3_PRO_S2, 0, 0,      0,      ATA_SA150, "nVidia nForce3 Pro" },
     { ATA_NFORCE3_MCP, 0, AMDNVIDIA, NVIDIA, ATA_UDMA6, "nVidia nForce3 MCP" },
     { ATA_NFORCE3_MCP_S1, 0, 0,      NV4OFF, ATA_SA150, "nVidia nForce3 MCP" },
     { ATA_NFORCE3_MCP_S2, 0, 0,      NV4OFF, ATA_SA150, "nVidia nForce3 MCP" },
     { ATA_NFORCE4,     0, AMDNVIDIA, NVIDIA, ATA_UDMA6, "nVidia nForce4" },
     { ATA_NFORCE4_S1,  0, 0,         NV4OFF, ATA_SA150, "nVidia nForce4" },
     { ATA_NFORCE4_S2,  0, 0,         NV4OFF, ATA_SA150, "nVidia nForce4" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64];

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_nvidia_chipinit;
    return 0;
}

static int
ata_nvidia_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    if (ctlr->chip->max_dma >= ATA_SA150) {
	if (pci_read_config(dev, PCIR_BAR(5), 1) & 1)
	    ctlr->r_type2 = SYS_RES_IOPORT;
	else
	    ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE))) {
	    int offset = ctlr->chip->cfg2 & NV4OFF ? 0x0440 : 0x0010;

	    if (bus_teardown_intr(dev, ctlr->r_irq, ctlr->handle) ||
		bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
				ata_nvidia_intr, ctlr, &ctlr->handle)) {
		device_printf(dev, "unable to setup interrupt\n");
		return ENXIO;
	    }

	    /* enable control access */
	    pci_write_config(dev, 0x50, pci_read_config(dev, 0x50, 1) | 0x04,1);

	    /* clear interrupt status */
	    ATA_OUTB(ctlr->r_res2, offset, 0xff);

	    /* enable device and PHY state change interrupts */
	    ATA_OUTB(ctlr->r_res2, offset + 1, 0xdd);

	    /* enable PCI interrupt */
	    pci_write_config(dev, PCIR_COMMAND,
			     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400,2);

	    ctlr->allocate = ata_nvidia_allocate;
	    ctlr->reset = ata_nvidia_reset;
	}
	ctlr->setmode = ata_sata_setmode;
    }
    else {
	/* disable prefetch, postwrite */
	pci_write_config(dev, 0x51, pci_read_config(dev, 0x51, 1) & 0x0f, 1);
	ctlr->setmode = ata_via_family_setmode;
    }
    return 0;
}

static int
ata_nvidia_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    ata_pci_allocate(dev);

    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = (ch->unit << 6);
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x04 + (ch->unit << 6);
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x08 + (ch->unit << 6);
    ch->flags |= ATA_NO_SLAVE;

    return 0;
}

static void 
ata_nvidia_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int offset = ctlr->chip->cfg2 & NV4OFF ? 0x0440 : 0x0010;
    u_int8_t status;
    int unit;

    /* get interrupt status */
    status = ATA_INB(ctlr->r_res2, offset);

    /* clear interrupt status */
    ATA_OUTB(ctlr->r_res2, offset, 0xff);

    for (unit = 0; unit < ctlr->channels; unit++) {
	if ((ch = ctlr->interrupt[unit].argument)) {
	    struct ata_connect_task *tp;
	    int maskshift = ch->unit << 2;

	    /* clear error status */
	    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

	    /* check for and handle connect events */
	    if (((status & (0x0c << maskshift)) == (0x04 << maskshift)) &&
		(tp = (struct ata_connect_task *)
		      malloc(sizeof(struct ata_connect_task),
			     M_ATA, M_NOWAIT | M_ZERO))) {

		device_printf(ch->dev, "CONNECT requested\n");
		tp->action = ATA_C_ATTACH;
		tp->dev = ch->dev;
		TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		taskqueue_enqueue(taskqueue_thread, &tp->task);
	    }

	    /* check for and handle disconnect events */
	    if ((status & (0x08 << maskshift)) &&
		(tp = (struct ata_connect_task *)
		      malloc(sizeof(struct ata_connect_task),
			   M_ATA, M_NOWAIT | M_ZERO))) {

		device_printf(ch->dev, "DISCONNECT requested\n");
		tp->action = ATA_C_DETACH;
		tp->dev = ch->dev;
		TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		taskqueue_enqueue(taskqueue_thread, &tp->task);
	    }

	    /* any drive action to take care of ? */
	    if (status & (0x01 << maskshift)) {
		if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
		    int bmstat = 
			ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

		    if (!(bmstat & ATA_BMSTAT_INTERRUPT))
			continue;
		    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat&~ATA_BMSTAT_ERROR);
		    DELAY(1);
		}
		ctlr->interrupt[unit].function(ch);
	    }
	}
    }
}

static void
ata_nvidia_reset(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_sata_phy_enable(ch);
}


/*
 * Promise chipset support functions
 */
#define ATA_PDC_APKT_OFFSET     0x00000010 
#define ATA_PDC_HPKT_OFFSET     0x00000040
#define ATA_PDC_ASG_OFFSET      0x00000080
#define ATA_PDC_LSG_OFFSET      0x000000c0
#define ATA_PDC_HSG_OFFSET      0x00000100
#define ATA_PDC_CHN_OFFSET      0x00000400
#define ATA_PDC_BUF_BASE        0x00400000
#define ATA_PDC_BUF_OFFSET      0x00100000
#define ATA_PDC_MAX_HPKT        8
#define ATA_PDC_WRITE_REG       0x00
#define ATA_PDC_WRITE_CTL       0x0e
#define ATA_PDC_WRITE_END       0x08
#define ATA_PDC_WAIT_NBUSY      0x10
#define ATA_PDC_WAIT_READY      0x18
#define ATA_PDC_1B              0x20
#define ATA_PDC_2B              0x40

struct host_packet {
u_int32_t                       addr;
    TAILQ_ENTRY(host_packet)    chain;
};

struct ata_promise_sx4 {
    struct mtx                  mtx;
    TAILQ_HEAD(, host_packet)   queue;
    int                         busy;
};

int
ata_promise_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_PDC20246,  0, PROLD, 0x00,    ATA_UDMA2, "Promise PDC20246" },
     { ATA_PDC20262,  0, PRNEW, 0x00,    ATA_UDMA4, "Promise PDC20262" },
     { ATA_PDC20263,  0, PRNEW, 0x00,    ATA_UDMA4, "Promise PDC20263" },
     { ATA_PDC20265,  0, PRNEW, 0x00,    ATA_UDMA5, "Promise PDC20265" },
     { ATA_PDC20267,  0, PRNEW, 0x00,    ATA_UDMA5, "Promise PDC20267" },
     { ATA_PDC20268,  0, PRTX,  PRTX4,   ATA_UDMA5, "Promise PDC20268" },
     { ATA_PDC20269,  0, PRTX,  0x00,    ATA_UDMA6, "Promise PDC20269" },
     { ATA_PDC20270,  0, PRTX,  PRTX4,   ATA_UDMA5, "Promise PDC20270" },
     { ATA_PDC20271,  0, PRTX,  0x00,    ATA_UDMA6, "Promise PDC20271" },
     { ATA_PDC20275,  0, PRTX,  0x00,    ATA_UDMA6, "Promise PDC20275" },
     { ATA_PDC20276,  0, PRTX,  PRSX6K,  ATA_UDMA6, "Promise PDC20276" },
     { ATA_PDC20277,  0, PRTX,  0x00,    ATA_UDMA6, "Promise PDC20277" },
     { ATA_PDC20318,  0, PRMIO, PRSATA,  ATA_SA150, "Promise PDC20318" },
     { ATA_PDC20319,  0, PRMIO, PRSATA,  ATA_SA150, "Promise PDC20319" },
     { ATA_PDC20371,  0, PRMIO, PRCMBO,  ATA_SA150, "Promise PDC20371" },
     { ATA_PDC20375,  0, PRMIO, PRCMBO,  ATA_SA150, "Promise PDC20375" },
     { ATA_PDC20376,  0, PRMIO, PRCMBO,  ATA_SA150, "Promise PDC20376" },
     { ATA_PDC20377,  0, PRMIO, PRCMBO,  ATA_SA150, "Promise PDC20377" },
     { ATA_PDC20378,  0, PRMIO, PRCMBO,  ATA_SA150, "Promise PDC20378" },
     { ATA_PDC20379,  0, PRMIO, PRCMBO,  ATA_SA150, "Promise PDC20379" },
     { ATA_PDC20571,  0, PRMIO, PRCMBO2, ATA_SA150, "Promise PDC20571" },
     { ATA_PDC20575,  0, PRMIO, PRCMBO2, ATA_SA150, "Promise PDC20575" },
     { ATA_PDC20579,  0, PRMIO, PRCMBO2, ATA_SA150, "Promise PDC20579" },
     { ATA_PDC20580,  0, PRMIO, PRCMBO2, ATA_SA150, "Promise PDC20580" },
     { ATA_PDC20617,  0, PRMIO, PRPATA,  ATA_UDMA6, "Promise PDC20617" },
     { ATA_PDC20618,  0, PRMIO, PRPATA,  ATA_UDMA6, "Promise PDC20618" },
     { ATA_PDC20619,  0, PRMIO, PRPATA,  ATA_UDMA6, "Promise PDC20619" },
     { ATA_PDC20620,  0, PRMIO, PRPATA,  ATA_UDMA6, "Promise PDC20620" },
     { ATA_PDC20621,  0, PRMIO, PRSX4X,  ATA_UDMA5, "Promise PDC20621" },
     { ATA_PDC20622,  0, PRMIO, PRSX4X,  ATA_SA150, "Promise PDC20622" },
     { ATA_PDC40518,  0, PRMIO, PRSATA2, ATA_SA150, "Promise PDC40518" },
     { ATA_PDC40519,  0, PRMIO, PRSATA2, ATA_SA150, "Promise PDC40519" },
     { ATA_PDC40718,  0, PRMIO, PRSATA2, ATA_SA300, "Promise PDC40718" },
     { ATA_PDC40719,  0, PRMIO, PRSATA2, ATA_SA300, "Promise PDC40719" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64];
    uintptr_t devid = 0;

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    /* if we are on a SuperTrak SX6000 dont attach */
    if ((idx->cfg2 & PRSX6K) && pci_get_class(GRANDPARENT(dev))==PCIC_BRIDGE &&
	!BUS_READ_IVAR(device_get_parent(GRANDPARENT(dev)),
		       GRANDPARENT(dev), PCI_IVAR_DEVID, &devid) &&
	devid == ATA_I960RM) 
	return ENXIO;

    strcpy(buffer, idx->text);

    /* if we are on a FastTrak TX4, adjust the interrupt resource */
    if ((idx->cfg2 & PRTX4) && pci_get_class(GRANDPARENT(dev))==PCIC_BRIDGE &&
	!BUS_READ_IVAR(device_get_parent(GRANDPARENT(dev)),
		       GRANDPARENT(dev), PCI_IVAR_DEVID, &devid) &&
	((devid == ATA_DEC_21150) || (devid == ATA_DEC_21150_1))) {
	static long start = 0, end = 0;

	if (pci_get_slot(dev) == 1) {
	    bus_get_resource(dev, SYS_RES_IRQ, 0, &start, &end);
	    strcat(buffer, " (channel 0+1)");
	}
	else if (pci_get_slot(dev) == 2 && start && end) {
	    bus_set_resource(dev, SYS_RES_IRQ, 0, start, end);
	    start = end = 0;
	    strcat(buffer, " (channel 2+3)");
	}
	else {
	    start = end = 0;
	}
    }
    sprintf(buffer, "%s %s controller", buffer, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_promise_chipinit;
    return 0;
}

static int
ata_promise_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    int rid = ATA_IRQ_RID;

    if (!(ctlr->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					       RF_SHAREABLE | RF_ACTIVE))) {
	device_printf(dev, "unable to map interrupt\n");
	return ENXIO;
    }

    if (ctlr->chip->max_dma >= ATA_SA150)
	ctlr->setmode = ata_sata_setmode;
    else
	ctlr->setmode = ata_promise_setmode;

    switch  (ctlr->chip->cfg1) {
    case PRNEW:
	/* setup clocks */
	ATA_OUTB(ctlr->r_res1, 0x11, ATA_INB(ctlr->r_res1, 0x11) | 0x0a);

	ctlr->dmainit = ata_promise_new_dmainit;
	/* FALLTHROUGH */

    case PROLD:
	/* enable burst mode */
	ATA_OUTB(ctlr->r_res1, 0x1f, ATA_INB(ctlr->r_res1, 0x1f) | 0x01);

	if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			    ata_promise_old_intr, ctlr, &ctlr->handle))) {
	    device_printf(dev, "unable to setup interrupt\n");
	    return ENXIO;
	}
	break;

    case PRTX:
	if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			    ata_promise_tx2_intr, ctlr, &ctlr->handle))) {
	    device_printf(dev, "unable to setup interrupt\n");
	    return ENXIO;
	}
	break;

    case PRMIO:
//      if (ctlr->r_res1)
//          bus_release_resource(dev, ctlr->r_type1, ctlr->r_rid1,ctlr->r_res1);
	ctlr->r_type1 = SYS_RES_MEMORY;
	ctlr->r_rid1 = PCIR_BAR(4);
	if (!(ctlr->r_res1 = bus_alloc_resource_any(dev, ctlr->r_type1,
						    &ctlr->r_rid1, RF_ACTIVE)))
	    return ENXIO;

	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(3);
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE))){
	    bus_release_resource(dev, ctlr->r_type1, ctlr->r_rid1,ctlr->r_res1);
	    return ENXIO;
	}
	ctlr->reset = ata_promise_mio_reset;
	ctlr->dmainit = ata_promise_mio_dmainit;
	ctlr->allocate = ata_promise_mio_allocate;

	if (ctlr->chip->cfg2 == PRSX4X) {
	    struct ata_promise_sx4 *hpkt;
	    u_int32_t dimm = ATA_INL(ctlr->r_res2, 0x000c0080);

	    if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
				ata_promise_sx4_intr, ctlr, &ctlr->handle))) {
		device_printf(dev, "unable to setup interrupt\n");
		/* XXX SOS release resources */
		return ENXIO;
	    }

	    /* print info about cache memory */
	    device_printf(dev, "DIMM size %dMB @ 0x%08x%s\n",
			  (((dimm >> 16) & 0xff)-((dimm >> 24) & 0xff)+1) << 4,
			  ((dimm >> 24) & 0xff),
			  ATA_INL(ctlr->r_res2, 0x000c0088) & (1<<16) ?
			  " ECC enabled" : "" );

	    /* adjust cache memory parameters */
	    ATA_OUTL(ctlr->r_res2, 0x000c000c, 
		     (ATA_INL(ctlr->r_res2, 0x000c000c) & 0xffff0000));

	    /* setup host packet controls */
	    hpkt = malloc(sizeof(struct ata_promise_sx4),
			  M_TEMP, M_NOWAIT | M_ZERO);
	    mtx_init(&hpkt->mtx, "ATA promise HPKT lock", NULL, MTX_DEF);
	    TAILQ_INIT(&hpkt->queue);
	    hpkt->busy = 0; //hpkt->head = hpkt->tail = 0;
	    device_set_ivars(dev, hpkt);
	    ctlr->channels = 4;
	    return 0;
	}

	if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			    ata_promise_mio_intr, ctlr, &ctlr->handle))) {
	    device_printf(dev, "unable to setup interrupt\n");
	    /* XXX SOS release resources */
	    return ENXIO;
	}

	switch (ctlr->chip->cfg2) {
	case PRPATA:
	    ctlr->channels = ((ATA_INL(ctlr->r_res2, 0x48) & 0x01) > 0) +
			     ((ATA_INL(ctlr->r_res2, 0x48) & 0x02) > 0) + 2;
	    break;

	case PRCMBO:
	    ATA_OUTL(ctlr->r_res2, 0x06c, 0x000000ff);
	    ctlr->channels = ((ATA_INL(ctlr->r_res2, 0x48) & 0x02) > 0) + 3;
	    break;

	case PRSATA:
	    ATA_OUTL(ctlr->r_res2, 0x06c, 0x000000ff);
	    ctlr->channels = 4;
	    break;

	case PRCMBO2:
	    ATA_OUTL(ctlr->r_res2, 0x060, 0x000000ff);
	    ctlr->channels = 3;
	    break;

	case PRSATA2:
	    ATA_OUTL(ctlr->r_res2, 0x060, 0x000000ff);
	    ctlr->channels = 4;
	    break;

	default:
	    /* XXX SOS release resources */
	    return ENXIO;
	}
    default:
	/* XXX SOS release resources */
	return ENXIO;
    }
    return 0;
}

static int
ata_promise_mio_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = (ctlr->chip->cfg2 & PRSX4X) ? 0x000c0000 : 0;
    int i;
 
    for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
	ch->r_io[i].res = ctlr->r_res2;
	ch->r_io[i].offset = offset + 0x0200 + (i << 2) + (ch->unit << 7); 
    }
    ch->r_io[ATA_CONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_CONTROL].offset = offset + 0x0238 + (ch->unit << 7);
    ch->r_io[ATA_IDX_ADDR].res = ctlr->r_res2;
    ata_default_registers(dev);
    if ((ctlr->chip->cfg2 & (PRSATA | PRSATA2)) ||
	((ctlr->chip->cfg2 & (PRCMBO | PRCMBO2)) && ch->unit < 2)) {
	ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
	ch->r_io[ATA_SSTATUS].offset = 0x400 + (ch->unit << 8);
	ch->r_io[ATA_SERROR].res = ctlr->r_res2;
	ch->r_io[ATA_SERROR].offset = 0x404 + (ch->unit << 8);
	ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
	ch->r_io[ATA_SCONTROL].offset = 0x408 + (ch->unit << 8);
	ch->flags |= ATA_NO_SLAVE;
    }
    ch->flags |= ATA_USE_16BIT;

    ata_generic_hw(dev);
    if (offset)
	ch->hw.command = ata_promise_sx4_command;
    else
	ch->hw.command = ata_promise_mio_command;
    return 0;
}

static void
ata_promise_mio_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    u_int32_t vector = 0, status = 0;
    int unit;

    switch (ctlr->chip->cfg2) {
    case PRSATA:
    case PRCMBO:
	/* read and acknowledge interrupt(s) */
	vector = ATA_INL(ctlr->r_res2, 0x040);

	/* read and clear interface status */
	status = ATA_INL(ctlr->r_res2, 0x06c);
	ATA_OUTL(ctlr->r_res2, 0x06c, status & 0x000000ff);
	break;

    case PRSATA2:
    case PRCMBO2:
	/* read and acknowledge interrupt(s) */
	vector = ATA_INL(ctlr->r_res2, 0x040);
	ATA_OUTL(ctlr->r_res2, 0x040, vector & 0x0000ffff);

	/* read and clear interface status */
	status = ATA_INL(ctlr->r_res2, 0x060);
	ATA_OUTL(ctlr->r_res2, 0x060, status & 0x000000ff);
	break;
    }

    for (unit = 0; unit < ctlr->channels; unit++) {

	if ((ch = ctlr->interrupt[unit].argument)) {
	    struct ata_connect_task *tp;

	    /* check for and handle disconnect events */
	    if ((status & (0x00000001 << unit)) &&
		(tp = (struct ata_connect_task *)
		      malloc(sizeof(struct ata_connect_task),
			     M_ATA, M_NOWAIT | M_ZERO))) {

		if (bootverbose)
		    device_printf(ch->dev, "DISCONNECT requested\n");
		tp->action = ATA_C_DETACH;
		tp->dev = ch->dev;
		TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		taskqueue_enqueue(taskqueue_thread, &tp->task);
	    }

	    /* check for and handle connect events */
	    if ((status & (0x00000010 << unit)) &&
		(tp = (struct ata_connect_task *)
		      malloc(sizeof(struct ata_connect_task),
			     M_ATA, M_NOWAIT | M_ZERO))) {

		if (bootverbose)
		    device_printf(ch->dev, "CONNECT requested\n");
		tp->action = ATA_C_ATTACH;
		tp->dev = ch->dev;
		TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		taskqueue_enqueue(taskqueue_thread, &tp->task);
	    }

	    /* active interrupt(s) need to call the interrupt handler */
	    if (vector & (1 << (unit + 1)))
		if ((ch = ctlr->interrupt[unit].argument))
		    ctlr->interrupt[unit].function(ch);
	}
    }
}

static void
ata_promise_sx4_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    u_int32_t vector = ATA_INL(ctlr->r_res2, 0x000c0480);
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (vector & (1 << (unit + 1)))
	    if ((ch = ctlr->interrupt[unit].argument))
		ctlr->interrupt[unit].function(ch);
	if (vector & (1 << (unit + 5)))
	    if ((ch = ctlr->interrupt[unit].argument))
		ata_promise_queue_hpkt(ctlr,
				       htole32((ch->unit * ATA_PDC_CHN_OFFSET) +
					       ATA_PDC_HPKT_OFFSET));
	if (vector & (1 << (unit + 9))) {
	    ata_promise_next_hpkt(ctlr);
	    if ((ch = ctlr->interrupt[unit].argument))
		ctlr->interrupt[unit].function(ch);
	}
	if (vector & (1 << (unit + 13))) {
	    ata_promise_next_hpkt(ctlr);
	    if ((ch = ctlr->interrupt[unit].argument))
		ATA_OUTL(ctlr->r_res2, 0x000c0240 + (ch->unit << 7),
			 htole32((ch->unit * ATA_PDC_CHN_OFFSET) +
			 ATA_PDC_APKT_OFFSET));
	}
    }
}

static void
ata_promise_mio_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    if (ch->dma) {
	/* note start and stop are not used here */
    }
}

static void
ata_promise_mio_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_promise_sx4 *hpktp;

    switch (ctlr->chip->cfg2) {
    case PRSX4X:

	/* softreset channel ATA module */
	hpktp = device_get_ivars(ctlr->dev);
	ATA_OUTL(ctlr->r_res2, 0xc0260 + (ch->unit << 7), ch->unit + 1);
	ata_udelay(1000);
	ATA_OUTL(ctlr->r_res2, 0xc0260 + (ch->unit << 7),
		 (ATA_INL(ctlr->r_res2, 0xc0260 + (ch->unit << 7)) &
		  ~0x00003f9f) | (ch->unit + 1));

	/* softreset HOST module */ /* XXX SOS what about other outstandings */
	mtx_lock(&hpktp->mtx);
	ATA_OUTL(ctlr->r_res2, 0xc012c,
		 (ATA_INL(ctlr->r_res2, 0xc012c) & ~0x00000f9f) | (1 << 11));
	DELAY(10);
	ATA_OUTL(ctlr->r_res2, 0xc012c,
		 (ATA_INL(ctlr->r_res2, 0xc012c) & ~0x00000f9f));
	hpktp->busy = 0;
	mtx_unlock(&hpktp->mtx);
	ata_generic_reset(dev);
	break;

    case PRCMBO:
    case PRSATA:
	if ((ctlr->chip->cfg2 == PRSATA) ||
	    ((ctlr->chip->cfg2 == PRCMBO) && (ch->unit < 2))) {

	    /* mask plug/unplug intr */
	    ATA_OUTL(ctlr->r_res2, 0x06c, (0x00110000 << ch->unit));
	}

	/* softreset channels ATA module */
	ATA_OUTL(ctlr->r_res2, 0x0260 + (ch->unit << 7), (1 << 11));
	ata_udelay(10000);
	ATA_OUTL(ctlr->r_res2, 0x0260 + (ch->unit << 7),
		 (ATA_INL(ctlr->r_res2, 0x0260 + (ch->unit << 7)) &
		  ~0x00003f9f) | (ch->unit + 1));

	if ((ctlr->chip->cfg2 == PRSATA) ||
	    ((ctlr->chip->cfg2 == PRCMBO) && (ch->unit < 2))) {

	    ata_sata_phy_enable(ch);

	    /* reset and enable plug/unplug intr */
	    ATA_OUTL(ctlr->r_res2, 0x06c, (0x00000011 << ch->unit));
	}
	else
	    ata_generic_reset(dev);
	break;

    case PRCMBO2:
    case PRSATA2:
	if ((ctlr->chip->cfg2 == PRSATA2) ||
	    ((ctlr->chip->cfg2 == PRCMBO2) && (ch->unit < 2))) {
	    /* set portmultiplier port */
	    ATA_OUTL(ctlr->r_res2, 0x4e8 + (ch->unit << 8), 0x0f);

	    /* mask plug/unplug intr */
	    ATA_OUTL(ctlr->r_res2, 0x060, (0x00110000 << ch->unit));
	}

	/* softreset channels ATA module */
	ATA_OUTL(ctlr->r_res2, 0x0260 + (ch->unit << 7), (1 << 11));
	ata_udelay(10000);
	ATA_OUTL(ctlr->r_res2, 0x0260 + (ch->unit << 7),
		 (ATA_INL(ctlr->r_res2, 0x0260 + (ch->unit << 7)) &
		  ~0x00003f9f) | (ch->unit + 1));

	if ((ctlr->chip->cfg2 == PRSATA2) ||
	    ((ctlr->chip->cfg2 == PRCMBO2) && (ch->unit < 2))) {

	    /* set PHY mode to "improved" */
	    ATA_OUTL(ctlr->r_res2, 0x414 + (ch->unit << 8),
		     (ATA_INL(ctlr->r_res2, 0x414 + (ch->unit << 8)) &
		     ~0x00000003) | 0x00000001);

	    ata_sata_phy_enable(ch);

	    /* reset and enable plug/unplug intr */
	    ATA_OUTL(ctlr->r_res2, 0x060, (0x00000011 << ch->unit));

	    /* set portmultiplier port */
	    ATA_OUTL(ctlr->r_res2, 0x4e8 + (ch->unit << 8), 0x00);
	}
	else
	    ata_generic_reset(dev);
	break;

    }
}

static int
ata_promise_mio_command(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    u_int32_t *wordp = (u_int32_t *)ch->dma->work;

    ATA_OUTL(ctlr->r_res2, (ch->unit + 1) << 2, 0x00000001);

    /* XXX SOS add ATAPI commands support later */
    switch (request->u.ata.command) {
    default:
	return ata_generic_command(request);

    case ATA_READ_DMA:
    case ATA_READ_DMA48:
	wordp[0] = htole32(0x04 | ((ch->unit + 1) << 16) | (0x00 << 24));
	break;

    case ATA_WRITE_DMA:
    case ATA_WRITE_DMA48:
	wordp[0] = htole32(0x00 | ((ch->unit + 1) << 16) | (0x00 << 24));
	break;
    }
    wordp[1] = htole32(ch->dma->sg_bus);
    wordp[2] = 0;
    ata_promise_apkt((u_int8_t*)wordp, request);

    ATA_OUTL(ctlr->r_res2, 0x0240 + (ch->unit << 7), ch->dma->work_bus);
    return 0;
}

static int
ata_promise_sx4_command(struct ata_request *request)
{
    device_t gparent = GRANDPARENT(request->dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_dma_prdentry *prd = ch->dma->sg;
    caddr_t window = rman_get_virtual(ctlr->r_res1);
    u_int32_t *wordp;
    int i, idx, length = 0;

    /* XXX SOS add ATAPI commands support later */
    switch (request->u.ata.command) {    

    default:
	return -1;

    case ATA_ATA_IDENTIFY:
    case ATA_READ:
    case ATA_READ48:
    case ATA_READ_MUL:
    case ATA_READ_MUL48:
    case ATA_WRITE:
    case ATA_WRITE48:
    case ATA_WRITE_MUL:
    case ATA_WRITE_MUL48:
	ATA_OUTL(ctlr->r_res2, 0x000c0400 + ((ch->unit + 1) << 2), 0x00000001);
	return ata_generic_command(request);

    case ATA_SETFEATURES:
    case ATA_FLUSHCACHE:
    case ATA_FLUSHCACHE48:
    case ATA_SLEEP:
    case ATA_SET_MULTI:
	wordp = (u_int32_t *)
	    (window + (ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_APKT_OFFSET);
	wordp[0] = htole32(0x08 | ((ch->unit + 1)<<16) | (0x00 << 24));
	wordp[1] = 0;
	wordp[2] = 0;
	ata_promise_apkt((u_int8_t *)wordp, request);
	ATA_OUTL(ctlr->r_res2, 0x000c0484, 0x00000001);
	ATA_OUTL(ctlr->r_res2, 0x000c0400 + ((ch->unit + 1) << 2), 0x00000001);
	ATA_OUTL(ctlr->r_res2, 0x000c0240 + (ch->unit << 7),
		 htole32((ch->unit * ATA_PDC_CHN_OFFSET)+ATA_PDC_APKT_OFFSET));
	return 0;

    case ATA_READ_DMA:
    case ATA_READ_DMA48:
    case ATA_WRITE_DMA:
    case ATA_WRITE_DMA48:
	wordp = (u_int32_t *)
	    (window + (ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_HSG_OFFSET);
	i = idx = 0;
	do {
	    wordp[idx++] = prd[i].addr;
	    wordp[idx++] = prd[i].count;
	    length += (prd[i].count & ~ATA_DMA_EOT);
	} while (!(prd[i++].count & ATA_DMA_EOT));

	wordp = (u_int32_t *)
	    (window + (ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_LSG_OFFSET);
	wordp[0] = htole32((ch->unit * ATA_PDC_BUF_OFFSET) + ATA_PDC_BUF_BASE);
	wordp[1] = htole32(request->bytecount | ATA_DMA_EOT);

	wordp = (u_int32_t *)
	    (window + (ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_ASG_OFFSET);
	wordp[0] = htole32((ch->unit * ATA_PDC_BUF_OFFSET) + ATA_PDC_BUF_BASE);
	wordp[1] = htole32(request->bytecount | ATA_DMA_EOT);

	wordp = (u_int32_t *)
	    (window + (ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_HPKT_OFFSET);
	if (request->flags & ATA_R_READ)
	    wordp[0] = htole32(0x14 | ((ch->unit+9)<<16) | ((ch->unit+5)<<24));
	if (request->flags & ATA_R_WRITE)
	    wordp[0] = htole32(0x00 | ((ch->unit+13)<<16) | (0x00<<24));
	wordp[1] = htole32((ch->unit * ATA_PDC_CHN_OFFSET)+ATA_PDC_HSG_OFFSET);
	wordp[2] = htole32((ch->unit * ATA_PDC_CHN_OFFSET)+ATA_PDC_LSG_OFFSET);
	wordp[3] = 0;

	wordp = (u_int32_t *)
	    (window + (ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_APKT_OFFSET);
	if (request->flags & ATA_R_READ)
	    wordp[0] = htole32(0x04 | ((ch->unit+5)<<16) | (0x00<<24));
	if (request->flags & ATA_R_WRITE)
	    wordp[0] = htole32(0x10 | ((ch->unit+1)<<16) | ((ch->unit+13)<<24));
	wordp[1] = htole32((ch->unit * ATA_PDC_CHN_OFFSET)+ATA_PDC_ASG_OFFSET);
	wordp[2] = 0;
	ata_promise_apkt((u_int8_t *)wordp, request);
	ATA_OUTL(ctlr->r_res2, 0x000c0484, 0x00000001);

	if (request->flags & ATA_R_READ) {
	    ATA_OUTL(ctlr->r_res2, 0x000c0400 + ((ch->unit+5)<<2), 0x00000001);
	    ATA_OUTL(ctlr->r_res2, 0x000c0400 + ((ch->unit+9)<<2), 0x00000001);
	    ATA_OUTL(ctlr->r_res2, 0x000c0240 + (ch->unit << 7),
		htole32((ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_APKT_OFFSET));
	}
	if (request->flags & ATA_R_WRITE) {
	    ATA_OUTL(ctlr->r_res2, 0x000c0400 + ((ch->unit+1)<<2), 0x00000001);
	    ATA_OUTL(ctlr->r_res2, 0x000c0400 + ((ch->unit+13)<<2), 0x00000001);
	    ata_promise_queue_hpkt(ctlr,
		htole32((ch->unit * ATA_PDC_CHN_OFFSET) + ATA_PDC_HPKT_OFFSET));
	}
	return 0;
    }
}

static int
ata_promise_apkt(u_int8_t *bytep, struct ata_request *request)
{ 
    struct ata_device *atadev = device_get_softc(request->dev);
    int i = 12;

    bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_PDC_WAIT_NBUSY|ATA_DRIVE;
    bytep[i++] = ATA_D_IBM | ATA_D_LBA | atadev->unit;
    bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_CTL;
    bytep[i++] = ATA_A_4BIT;

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	bytep[i++] = ATA_PDC_2B | ATA_PDC_WRITE_REG | ATA_FEATURE;
	bytep[i++] = request->u.ata.feature >> 8;
	bytep[i++] = request->u.ata.feature;
	bytep[i++] = ATA_PDC_2B | ATA_PDC_WRITE_REG | ATA_COUNT;
	bytep[i++] = request->u.ata.count >> 8;
	bytep[i++] = request->u.ata.count;
	bytep[i++] = ATA_PDC_2B | ATA_PDC_WRITE_REG | ATA_SECTOR;
	bytep[i++] = request->u.ata.lba >> 24;
	bytep[i++] = request->u.ata.lba;
	bytep[i++] = ATA_PDC_2B | ATA_PDC_WRITE_REG | ATA_CYL_LSB;
	bytep[i++] = request->u.ata.lba >> 32;
	bytep[i++] = request->u.ata.lba >> 8;
	bytep[i++] = ATA_PDC_2B | ATA_PDC_WRITE_REG | ATA_CYL_MSB;
	bytep[i++] = request->u.ata.lba >> 40;
	bytep[i++] = request->u.ata.lba >> 16;
	bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_DRIVE;
	bytep[i++] = ATA_D_LBA | atadev->unit;
    }
    else {
	bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_FEATURE;
	bytep[i++] = request->u.ata.feature;
	bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_COUNT;
	bytep[i++] = request->u.ata.count;
	bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_SECTOR;
	bytep[i++] = request->u.ata.lba;
	bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_CYL_LSB;
	bytep[i++] = request->u.ata.lba >> 8;
	bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_CYL_MSB;
	bytep[i++] = request->u.ata.lba >> 16;
	bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_REG | ATA_DRIVE;
	bytep[i++] = (atadev->flags & ATA_D_USE_CHS ? 0 : ATA_D_LBA) |
		   ATA_D_IBM | atadev->unit | ((request->u.ata.lba >> 24)&0xf);
    }
    bytep[i++] = ATA_PDC_1B | ATA_PDC_WRITE_END | ATA_COMMAND;
    bytep[i++] = request->u.ata.command;
    return i;
}

static void
ata_promise_queue_hpkt(struct ata_pci_controller *ctlr, u_int32_t hpkt)
{
    struct ata_promise_sx4 *hpktp = device_get_ivars(ctlr->dev);

    mtx_lock(&hpktp->mtx);
    if (hpktp->busy) {
	struct host_packet *hp = 
	    malloc(sizeof(struct host_packet), M_TEMP, M_NOWAIT | M_ZERO);
	hp->addr = hpkt;
	TAILQ_INSERT_TAIL(&hpktp->queue, hp, chain);
    }
    else {
	hpktp->busy = 1;
	ATA_OUTL(ctlr->r_res2, 0x000c0100, hpkt);
    }
    mtx_unlock(&hpktp->mtx);
}

static void
ata_promise_next_hpkt(struct ata_pci_controller *ctlr)
{
    struct ata_promise_sx4 *hpktp = device_get_ivars(ctlr->dev);
    struct host_packet *hp;

    mtx_lock(&hpktp->mtx);
    if ((hp = TAILQ_FIRST(&hpktp->queue))) {
	TAILQ_REMOVE(&hpktp->queue, hp, chain);
	ATA_OUTL(ctlr->r_res2, 0x000c0100, hp->addr);
	free(hp, M_TEMP);
    }
    else
	hpktp->busy = 0;
    mtx_unlock(&hpktp->mtx);
}

static void
ata_promise_tx2_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	ATA_IDX_OUTB(ch, ATA_BMDEVSPEC_0, 0x0b);
	if (ATA_IDX_INB(ch, ATA_BMDEVSPEC_1) & 0x20) {
	    if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
		int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

		if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		    ATA_BMSTAT_INTERRUPT)
		    continue;
		ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
		DELAY(1);
	    }
	    ctlr->interrupt[unit].function(ch);
	}
    }
}

static void
ata_promise_old_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (ATA_INL(ctlr->r_res1, 0x1c) & (ch->unit ? 0x00004000 : 0x00000400)){
	    if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
		int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

		if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		    ATA_BMSTAT_INTERRUPT)
		    continue;
		ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
		DELAY(1);
	    }
	    ctlr->interrupt[unit].function(ch);
	}
    }
}

static int
ata_promise_new_dmastart(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(GRANDPARENT(dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev  = device_get_softc(dev);

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	ATA_OUTB(ctlr->r_res1, 0x11,
		 ATA_INB(ctlr->r_res1, 0x11) | (ch->unit ? 0x08 : 0x02));
	ATA_OUTL(ctlr->r_res1, ch->unit ? 0x24 : 0x20,
		 ((ch->dma->flags & ATA_DMA_READ) ? 0x05000000 : 0x06000000) |
		 (ch->dma->cur_iosize >> 1));
    }
    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, (ATA_IDX_INB(ch, ATA_BMSTAT_PORT) |
		 (ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR)));
    ATA_IDX_OUTL(ch, ATA_BMDTP_PORT, ch->dma->sg_bus);
    ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
		 ((ch->dma->flags & ATA_DMA_READ) ? ATA_BMCMD_WRITE_READ : 0) |
		 ATA_BMCMD_START_STOP);
    ch->flags |= ATA_DMA_ACTIVE;
    return 0;
}

static int
ata_promise_new_dmastop(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(GRANDPARENT(dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev  = device_get_softc(dev);
    int error;

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	ATA_OUTB(ctlr->r_res1, 0x11,
		 ATA_INB(ctlr->r_res1, 0x11) & ~(ch->unit ? 0x08 : 0x02));
	ATA_OUTL(ctlr->r_res1, ch->unit ? 0x24 : 0x20, 0);
    }
    error = ATA_IDX_INB(ch, ATA_BMSTAT_PORT);
    ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
		 ATA_IDX_INB(ch, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR); 
    ch->flags &= ~ATA_DMA_ACTIVE;
    return error;
}

static void
ata_promise_new_dmareset(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
		 ATA_IDX_INB(ch, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR); 
    ch->flags &= ~ATA_DMA_ACTIVE;
}

static void
ata_promise_new_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    if (ch->dma) {
	ch->dma->start = ata_promise_new_dmastart;
	ch->dma->stop = ata_promise_new_dmastop;
	ch->dma->reset = ata_promise_new_dmareset;
    }
}

static void
ata_promise_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;
    u_int32_t timings33[][2] = {
    /*    PROLD       PRNEW                mode */
	{ 0x004ff329, 0x004fff2f },     /* PIO 0 */
	{ 0x004fec25, 0x004ff82a },     /* PIO 1 */
	{ 0x004fe823, 0x004ff026 },     /* PIO 2 */
	{ 0x004fe622, 0x004fec24 },     /* PIO 3 */
	{ 0x004fe421, 0x004fe822 },     /* PIO 4 */
	{ 0x004567f3, 0x004acef6 },     /* MWDMA 0 */
	{ 0x004467f3, 0x0048cef6 },     /* MWDMA 1 */
	{ 0x004367f3, 0x0046cef6 },     /* MWDMA 2 */
	{ 0x004367f3, 0x0046cef6 },     /* UDMA 0 */
	{ 0x004247f3, 0x00448ef6 },     /* UDMA 1 */
	{ 0x004127f3, 0x00436ef6 },     /* UDMA 2 */
	{ 0,          0x00424ef6 },     /* UDMA 3 */
	{ 0,          0x004127f3 },     /* UDMA 4 */
	{ 0,          0x004127f3 }      /* UDMA 5 */
    };

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    switch (ctlr->chip->cfg1) {
    case PROLD:
    case PRNEW:
	if (mode > ATA_UDMA2 && (pci_read_config(gparent, 0x50, 2) &
				 (ch->unit ? 1 << 11 : 1 << 10))) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
	if (ata_atapi(dev) && mode > ATA_PIO_MAX)
	    mode = ata_limit_mode(dev, mode, ATA_PIO_MAX);
	break;

    case PRTX:
	ATA_IDX_OUTB(ch, ATA_BMDEVSPEC_0, 0x0b);
	if (mode > ATA_UDMA2 &&
	    ATA_IDX_INB(ch, ATA_BMDEVSPEC_1) & 0x04) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
	break;
   
    case PRMIO:
	if (mode > ATA_UDMA2 &&
	    (ATA_INL(ctlr->r_res2,
		     (ctlr->chip->cfg2 & PRSX4X ? 0x000c0260 : 0x0260) +
		     (ch->unit << 7)) & 0x01000000)) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
	break;
    }

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		     (error) ? "FAILURE " : "",
		     ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	if (ctlr->chip->cfg1 < PRTX)
	    pci_write_config(gparent, 0x60 + (devno << 2),
			     timings33[ctlr->chip->cfg1][ata_mode2idx(mode)],4);
	atadev->mode = mode;
    }
    return;
}


/*
 * ServerWorks chipset support functions
 */
int
ata_serverworks_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_ROSB4,  0x00, SWKS33,  0x00, ATA_UDMA2, "ServerWorks ROSB4" },
     { ATA_CSB5,   0x92, SWKS100, 0x00, ATA_UDMA5, "ServerWorks CSB5" },
     { ATA_CSB5,   0x00, SWKS66,  0x00, ATA_UDMA4, "ServerWorks CSB5" },
     { ATA_CSB6,   0x00, SWKS100, 0x00, ATA_UDMA5, "ServerWorks CSB6" },
     { ATA_CSB6_1, 0x00, SWKS66,  0x00, ATA_UDMA4, "ServerWorks CSB6" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64];

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_serverworks_chipinit;
    return 0;
}

static int
ata_serverworks_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    if (ctlr->chip->cfg1 == SWKS33) {
	device_t *children;
	int nchildren, i;

	/* locate the ISA part in the southbridge and enable UDMA33 */
	if (!device_get_children(device_get_parent(dev), &children,&nchildren)){
	    for (i = 0; i < nchildren; i++) {
		if (pci_get_devid(children[i]) == ATA_ROSB4_ISA) {
		    pci_write_config(children[i], 0x64,
				     (pci_read_config(children[i], 0x64, 4) &
				      ~0x00002000) | 0x00004000, 4);
		    break;
		}
	    }
	    free(children, M_TEMP);
	}
    }
    else {
	pci_write_config(dev, 0x5a,
			 (pci_read_config(dev, 0x5a, 1) & ~0x40) |
			 (ctlr->chip->cfg1 == SWKS100) ? 0x03 : 0x02, 1);
    }
    ctlr->setmode = ata_serverworks_setmode;
    return 0;
}

static void
ata_serverworks_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int offset = (devno ^ 0x01) << 3;
    int error;
    u_int8_t piotimings[] = { 0x5d, 0x47, 0x34, 0x22, 0x20, 0x34, 0x22, 0x20,
			      0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
    u_int8_t dmatimings[] = { 0x77, 0x21, 0x20 };

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    mode = ata_check_80pin(dev, mode);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "",
		      ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    pci_write_config(gparent, 0x56, 
			     (pci_read_config(gparent, 0x56, 2) &
			      ~(0xf << (devno << 2))) |
			     ((mode & ATA_MODE_MASK) << (devno << 2)), 2);
	    pci_write_config(gparent, 0x54,
			     pci_read_config(gparent, 0x54, 1) |
			     (0x01 << devno), 1);
	    pci_write_config(gparent, 0x44, 
			     (pci_read_config(gparent, 0x44, 4) &
			      ~(0xff << offset)) |
			     (dmatimings[2] << offset), 4);
	}
	else if (mode >= ATA_WDMA0) {
	    pci_write_config(gparent, 0x54,
			     pci_read_config(gparent, 0x54, 1) &
			      ~(0x01 << devno), 1);
	    pci_write_config(gparent, 0x44, 
			     (pci_read_config(gparent, 0x44, 4) &
			      ~(0xff << offset)) |
			     (dmatimings[mode & ATA_MODE_MASK] << offset),4);
	}
	else
	    pci_write_config(gparent, 0x54,
			     pci_read_config(gparent, 0x54, 1) &
			     ~(0x01 << devno), 1);

	pci_write_config(gparent, 0x40, 
			 (pci_read_config(gparent, 0x40, 4) &
			  ~(0xff << offset)) |
			 (piotimings[ata_mode2idx(mode)] << offset), 4);
	atadev->mode = mode;
    }
}


/*
 * Silicon Image Inc. (SiI) (former CMD) chipset support functions
 */
int
ata_sii_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_SII3114,   0x00, SIIMEMIO, SII4CH,    ATA_SA150, "SiI 3114" },
     { ATA_SII3512,   0x02, SIIMEMIO, 0,         ATA_SA150, "SiI 3512" },
     { ATA_SII3112,   0x02, SIIMEMIO, 0,         ATA_SA150, "SiI 3112" },
     { ATA_SII3112_1, 0x02, SIIMEMIO, 0,         ATA_SA150, "SiI 3112" },
     { ATA_SII3512,   0x00, SIIMEMIO, SIIBUG,    ATA_SA150, "SiI 3512" },
     { ATA_SII3112,   0x00, SIIMEMIO, SIIBUG,    ATA_SA150, "SiI 3112" },
     { ATA_SII3112_1, 0x00, SIIMEMIO, SIIBUG,    ATA_SA150, "SiI 3112" },
     { ATA_SII0680,   0x00, SIIMEMIO, SIISETCLK, ATA_UDMA6, "SiI 0680" },
     { ATA_CMD649,    0x00, 0,        SIIINTR,   ATA_UDMA5, "CMD 649" },
     { ATA_CMD648,    0x00, 0,        SIIINTR,   ATA_UDMA4, "CMD 648" },
     { ATA_CMD646,    0x07, 0,        0,         ATA_UDMA2, "CMD 646U2" },
     { ATA_CMD646,    0x00, 0,        0,         ATA_WDMA2, "CMD 646" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64];

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_sii_chipinit;
    return 0;
}

static int
ata_sii_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    int rid = ATA_IRQ_RID;

    if (!(ctlr->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					       RF_SHAREABLE | RF_ACTIVE))) {
	device_printf(dev, "unable to map interrupt\n");
	return ENXIO;
    }

    if (ctlr->chip->cfg1 == SIIMEMIO) {
	if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			    ata_sii_intr, ctlr, &ctlr->handle))) {
	    device_printf(dev, "unable to setup interrupt\n");
	    return ENXIO;
	}

	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE)))
	    return ENXIO;

	if (ctlr->chip->cfg2 & SIISETCLK) {
	    if ((pci_read_config(dev, 0x8a, 1) & 0x30) != 0x10)
		pci_write_config(dev, 0x8a, 
				 (pci_read_config(dev, 0x8a, 1) & 0xcf)|0x10,1);
	    if ((pci_read_config(dev, 0x8a, 1) & 0x30) != 0x10)
		device_printf(dev, "%s could not set ATA133 clock\n",
			      ctlr->chip->text);
	}

	/* if we have 4 channels enable the second set */
	if (ctlr->chip->cfg2 & SII4CH) {
	    ATA_OUTL(ctlr->r_res2, 0x0200, 0x00000002);
	    ctlr->channels = 4;
	}

	/* enable PCI interrupt as BIOS might not */
	pci_write_config(dev, 0x8a, (pci_read_config(dev, 0x8a, 1) & 0x3f), 1);

	/* dont block interrupts from any channel */
	pci_write_config(dev, 0x48,
			 (pci_read_config(dev, 0x48, 4) & ~0x03c00000), 4);

	ctlr->allocate = ata_sii_allocate;
	if (ctlr->chip->max_dma >= ATA_SA150) {
	    ctlr->reset = ata_sii_reset;
	    ctlr->setmode = ata_sata_setmode;
	}
	else
	    ctlr->setmode = ata_sii_setmode;
    }
    else {
	if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			    ctlr->chip->cfg2 & SIIINTR ? 
			    ata_cmd_intr : ata_cmd_old_intr,
			    ctlr, &ctlr->handle))) {
	    device_printf(dev, "unable to setup interrupt\n");
	    return ENXIO;
	}

	if ((pci_read_config(dev, 0x51, 1) & 0x08) != 0x08) {
	    device_printf(dev, "HW has secondary channel disabled\n");
	    ctlr->channels = 1;
	}    

	/* enable interrupt as BIOS might not */
	pci_write_config(dev, 0x71, 0x01, 1);

	ctlr->setmode = ata_cmd_setmode;
    }
    return 0;
}

static int
ata_sii_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int unit01 = (ch->unit & 1), unit10 = (ch->unit & 2);
    int i;

    for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
	ch->r_io[i].res = ctlr->r_res2;
	ch->r_io[i].offset = 0x80 + i + (unit01 << 6) + (unit10 << 8);
    }
    ch->r_io[ATA_CONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_CONTROL].offset = 0x8a + (unit01 << 6) + (unit10 << 8);
    ch->r_io[ATA_IDX_ADDR].res = ctlr->r_res2;
    ata_default_registers(dev);
    ch->r_io[ATA_BMCMD_PORT].res = ctlr->r_res2;
    ch->r_io[ATA_BMCMD_PORT].offset = 0x00 + (unit01 << 3) + (unit10 << 8);
    ch->r_io[ATA_BMSTAT_PORT].res = ctlr->r_res2;
    ch->r_io[ATA_BMSTAT_PORT].offset = 0x02 + (unit01 << 3) + (unit10 << 8);
    ch->r_io[ATA_BMDTP_PORT].res = ctlr->r_res2;
    ch->r_io[ATA_BMDTP_PORT].offset = 0x04 + (unit01 << 3) + (unit10 << 8);
    ch->r_io[ATA_BMDEVSPEC_0].res = ctlr->r_res2;
    ch->r_io[ATA_BMDEVSPEC_0].offset = 0xa1 + (unit01 << 6) + (unit10 << 8);

    if (ctlr->chip->max_dma >= ATA_SA150) {
	ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
	ch->r_io[ATA_SSTATUS].offset = 0x104 + (unit01 << 7) + (unit10 << 8);
	ch->r_io[ATA_SERROR].res = ctlr->r_res2;
	ch->r_io[ATA_SERROR].offset = 0x108 + (unit01 << 7) + (unit10 << 8);
	ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
	ch->r_io[ATA_SCONTROL].offset = 0x100 + (unit01 << 7) + (unit10 << 8);
	ch->flags |= ATA_NO_SLAVE;

	/* enable PHY state change interrupt */
	ATA_OUTL(ctlr->r_res2, 0x148 + (unit01 << 7) + (unit10 << 8),(1 << 16));
    }

    if ((ctlr->chip->cfg2 & SIIBUG) && ch->dma) {
	/* work around errata in early chips */
	ch->dma->boundary = 16 * DEV_BSIZE;
	ch->dma->segsize = 15 * DEV_BSIZE;
    }

    ata_generic_hw(dev);
    return 0;
}

static void
ata_sii_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;

	/* check for PHY related interrupts on SATA capable HW */
	if (ctlr->chip->max_dma >= ATA_SA150) {
	    u_int32_t status = ATA_IDX_INL(ch, ATA_SSTATUS);
	    u_int32_t error = ATA_IDX_INL(ch, ATA_SERROR);
	    struct ata_connect_task *tp;

	    if (error) {
		/* clear error bits/interrupt */
		ATA_IDX_OUTL(ch, ATA_SERROR, error);

		/* if we have a connection event deal with it */
		if ((error & ATA_SE_PHY_CHANGED) &&
		    (tp = (struct ata_connect_task *)
			  malloc(sizeof(struct ata_connect_task),
				 M_ATA, M_NOWAIT | M_ZERO))) {

		    if ((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN1) {
			device_printf(ch->dev, "CONNECT requested\n");
			tp->action = ATA_C_ATTACH;
		    }
		    else {
			device_printf(ch->dev, "DISCONNECT requested\n");
			tp->action = ATA_C_DETACH;
		    }
		    tp->dev = ch->dev;
		    TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
		    taskqueue_enqueue(taskqueue_thread, &tp->task);
		}
	    }
	}

	/* any drive action to take care of ? */
	if (ATA_IDX_INB(ch, ATA_BMDEVSPEC_0) & 0x08) {
	    if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
		int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

		if (!(bmstat & ATA_BMSTAT_INTERRUPT))
		    continue;
		ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
		DELAY(1);
	    }
	    ctlr->interrupt[unit].function(ch);
	}

    }
}

static void
ata_cmd_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    u_int8_t reg71;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (((reg71 = pci_read_config(device_get_parent(ch->dev), 0x71, 1)) &
	     (ch->unit ? 0x08 : 0x04))) {
	    pci_write_config(device_get_parent(ch->dev), 0x71,
			     reg71 & ~(ch->unit ? 0x04 : 0x08), 1);
	    if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
		int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

		if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		    ATA_BMSTAT_INTERRUPT)
		    continue;
		ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
		DELAY(1);
	    }
	    ctlr->interrupt[unit].function(ch);
	}
    }
}

static void
ata_cmd_old_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    int unit;

    for (unit = 0; unit < ctlr->channels; unit++) {
	if (!(ch = ctlr->interrupt[unit].argument))
	    continue;
	if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
	    int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	    if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
		ATA_BMSTAT_INTERRUPT)
		continue;
	    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	    DELAY(1);
	}
	ctlr->interrupt[unit].function(ch);
    }
}

static void
ata_sii_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ((ch->unit & 1) << 7) + ((ch->unit & 2) << 8);

    /* disable PHY state change interrupt */
    ATA_OUTL(ctlr->r_res2, 0x148 + offset, ~(1 << 16));

    /* reset controller part for this channel */
    ATA_OUTL(ctlr->r_res2, 0x48,
	     ATA_INL(ctlr->r_res2, 0x48) | (0xc0 >> ch->unit));
    DELAY(1000);
    ATA_OUTL(ctlr->r_res2, 0x48,
	     ATA_INL(ctlr->r_res2, 0x48) & ~(0xc0 >> ch->unit));

    ata_sata_phy_enable(ch);

    /* enable PHY state change interrupt */
    ATA_OUTL(ctlr->r_res2, 0x148 + offset, (1 << 16));
}

static void
ata_sii_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int rego = (ch->unit << 4) + (ATA_DEV(atadev->unit) << 1);
    int mreg = ch->unit ? 0x84 : 0x80;
    int mask = 0x03 << (ATA_DEV(atadev->unit) << 2);
    int mval = pci_read_config(gparent, mreg, 1) & ~mask;
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if (ctlr->chip->cfg2 & SIISETCLK) {
	if (mode > ATA_UDMA2 && (pci_read_config(gparent, 0x79, 1) &
				 (ch->unit ? 0x02 : 0x01))) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
    }
    else
	mode = ata_check_80pin(dev, mode);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "",
		      ata_mode2str(mode), ctlr->chip->text);
    if (error)
	return;

    if (mode >= ATA_UDMA0) {
	u_int8_t udmatimings[] = { 0xf, 0xb, 0x7, 0x5, 0x3, 0x2, 0x1 };
	u_int8_t ureg = 0xac + rego;

	pci_write_config(gparent, mreg,
			 mval | (0x03 << (ATA_DEV(atadev->unit) << 2)), 1);
	pci_write_config(gparent, ureg, 
			 (pci_read_config(gparent, ureg, 1) & ~0x3f) |
			 udmatimings[mode & ATA_MODE_MASK], 1);

    }
    else if (mode >= ATA_WDMA0) {
	u_int8_t dreg = 0xa8 + rego;
	u_int16_t dmatimings[] = { 0x2208, 0x10c2, 0x10c1 };

	pci_write_config(gparent, mreg,
			 mval | (0x02 << (ATA_DEV(atadev->unit) << 2)), 1);
	pci_write_config(gparent, dreg, dmatimings[mode & ATA_MODE_MASK], 2);

    }
    else {
	u_int8_t preg = 0xa4 + rego;
	u_int16_t piotimings[] = { 0x328a, 0x2283, 0x1104, 0x10c3, 0x10c1 };

	pci_write_config(gparent, mreg,
			 mval | (0x01 << (ATA_DEV(atadev->unit) << 2)), 1);
	pci_write_config(gparent, preg, piotimings[mode & ATA_MODE_MASK], 2);
    }
    atadev->mode = mode;
}

static void
ata_cmd_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    mode = ata_check_80pin(dev, mode);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "",
		      ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	int treg = 0x54 + ((devno < 3) ? (devno << 1) : 7);
	int ureg = ch->unit ? 0x7b : 0x73;

	if (mode >= ATA_UDMA0) {        
	    int udmatimings[][2] = { { 0x31,  0xc2 }, { 0x21,  0x82 },
				     { 0x11,  0x42 }, { 0x25,  0x8a },
				     { 0x15,  0x4a }, { 0x05,  0x0a } };

	    u_int8_t umode = pci_read_config(gparent, ureg, 1);

	    umode &= ~(atadev->unit == ATA_MASTER ? 0x35 : 0xca);
	    umode |= udmatimings[mode & ATA_MODE_MASK][ATA_DEV(atadev->unit)];
	    pci_write_config(gparent, ureg, umode, 1);
	}
	else if (mode >= ATA_WDMA0) { 
	    int dmatimings[] = { 0x87, 0x32, 0x3f };

	    pci_write_config(gparent, treg, dmatimings[mode & ATA_MODE_MASK],1);
	    pci_write_config(gparent, ureg, 
			     pci_read_config(gparent, ureg, 1) &
			     ~(atadev->unit == ATA_MASTER ? 0x35 : 0xca), 1);
	}
	else {
	   int piotimings[] = { 0xa9, 0x57, 0x44, 0x32, 0x3f };
	    pci_write_config(gparent, treg,
			     piotimings[(mode & ATA_MODE_MASK) - ATA_PIO0], 1);
	    pci_write_config(gparent, ureg, 
			     pci_read_config(gparent, ureg, 1) &
			     ~(atadev->unit == ATA_MASTER ? 0x35 : 0xca), 1);
	}
	atadev->mode = mode;
    }
}


/*
 * Silicon Integrated Systems Corp. (SiS) chipset support functions
 */
int
ata_sis_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_SIS182,  0x00, SISSATA,   0, ATA_SA150, "SiS 182" }, /* south */
     { ATA_SIS181,  0x00, SISSATA,   0, ATA_SA150, "SiS 181" }, /* south */
     { ATA_SIS180,  0x00, SISSATA,   0, ATA_SA150, "SiS 180" }, /* south */
     { ATA_SIS965,  0x00, SIS133NEW, 0, ATA_UDMA6, "SiS 965" }, /* south */
     { ATA_SIS964,  0x00, SIS133NEW, 0, ATA_UDMA6, "SiS 964" }, /* south */
     { ATA_SIS963,  0x00, SIS133NEW, 0, ATA_UDMA6, "SiS 963" }, /* south */
     { ATA_SIS962,  0x00, SIS133NEW, 0, ATA_UDMA6, "SiS 962" }, /* south */

     { ATA_SIS745,  0x00, SIS100NEW, 0, ATA_UDMA5, "SiS 745" }, /* 1chip */
     { ATA_SIS735,  0x00, SIS100NEW, 0, ATA_UDMA5, "SiS 735" }, /* 1chip */
     { ATA_SIS733,  0x00, SIS100NEW, 0, ATA_UDMA5, "SiS 733" }, /* 1chip */
     { ATA_SIS730,  0x00, SIS100OLD, 0, ATA_UDMA5, "SiS 730" }, /* 1chip */

     { ATA_SIS635,  0x00, SIS100NEW, 0, ATA_UDMA5, "SiS 635" }, /* 1chip */
     { ATA_SIS633,  0x00, SIS100NEW, 0, ATA_UDMA5, "SiS 633" }, /* unknown */
     { ATA_SIS630,  0x30, SIS100OLD, 0, ATA_UDMA5, "SiS 630S"}, /* 1chip */
     { ATA_SIS630,  0x00, SIS66,     0, ATA_UDMA4, "SiS 630" }, /* 1chip */
     { ATA_SIS620,  0x00, SIS66,     0, ATA_UDMA4, "SiS 620" }, /* 1chip */

     { ATA_SIS550,  0x00, SIS66,     0, ATA_UDMA5, "SiS 550" },
     { ATA_SIS540,  0x00, SIS66,     0, ATA_UDMA4, "SiS 540" },
     { ATA_SIS530,  0x00, SIS66,     0, ATA_UDMA4, "SiS 530" },

     { ATA_SIS5513, 0xc2, SIS33,     1, ATA_UDMA2, "SiS 5513" },
     { ATA_SIS5513, 0x00, SIS33,     1, ATA_WDMA2, "SiS 5513" },
     { 0, 0, 0, 0, 0, 0 }};
    char buffer[64];
    int found = 0;

    if (!(idx = ata_find_chip(dev, ids, -pci_get_slot(dev)))) 
	return ENXIO;

    if (idx->cfg2 && !found) {
	u_int8_t reg57 = pci_read_config(dev, 0x57, 1);

	pci_write_config(dev, 0x57, (reg57 & 0x7f), 1);
	if (pci_read_config(dev, PCIR_DEVVENDOR, 4) == ATA_SIS5518) {
	    found = 1;
	    idx->cfg1 = SIS133NEW;
	    idx->max_dma = ATA_UDMA6;
	    sprintf(buffer, "SiS 962/963 %s controller",
		    ata_mode2str(idx->max_dma));
	}
	pci_write_config(dev, 0x57, reg57, 1);
    }
    if (idx->cfg2 && !found) {
	u_int8_t reg4a = pci_read_config(dev, 0x4a, 1);

	pci_write_config(dev, 0x4a, (reg4a | 0x10), 1);
	if (pci_read_config(dev, PCIR_DEVVENDOR, 4) == ATA_SIS5517) {
	    struct ata_chip_id id[] =
		{{ ATA_SISSOUTH, 0x10, 0, 0, 0, "" }, { 0, 0, 0, 0, 0, 0 }};

	    found = 1;
	    if (ata_find_chip(dev, id, pci_get_slot(dev))) {
		idx->cfg1 = SIS133OLD;
		idx->max_dma = ATA_UDMA6;
	    }
	    else {
		idx->cfg1 = SIS100NEW;
		idx->max_dma = ATA_UDMA5;
	    }
	    sprintf(buffer, "SiS 961 %s controller",ata_mode2str(idx->max_dma));
	}
	pci_write_config(dev, 0x4a, reg4a, 1);
    }
    if (!found)
	sprintf(buffer,"%s %s controller",idx->text,ata_mode2str(idx->max_dma));

    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_sis_chipinit;
    return 0;
}

static int
ata_sis_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;
    
    switch (ctlr->chip->cfg1) {
    case SIS33:
	break;
    case SIS66:
    case SIS100OLD:
	pci_write_config(dev, 0x52, pci_read_config(dev, 0x52, 1) & ~0x04, 1);
	break;
    case SIS100NEW:
    case SIS133OLD:
	pci_write_config(dev, 0x49, pci_read_config(dev, 0x49, 1) & ~0x01, 1);
	break;
    case SIS133NEW:
	pci_write_config(dev, 0x50, pci_read_config(dev, 0x50, 2) | 0x0008, 2);
	pci_write_config(dev, 0x52, pci_read_config(dev, 0x52, 2) | 0x0008, 2);
	break;
    case SISSATA:
	ctlr->r_type2 = SYS_RES_IOPORT;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE))) {
	    pci_write_config(dev, PCIR_COMMAND,
			     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400,2);
	    ctlr->allocate = ata_sis_allocate;
	    ctlr->reset = ata_sis_reset;
	}
	ctlr->setmode = ata_sata_setmode;
	return 0;
    default:
	return ENXIO;
    }
    ctlr->setmode = ata_sis_setmode;
    return 0;
}

static int
ata_sis_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    ata_pci_allocate(dev);

    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = (ch->unit << 4);
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x04 + (ch->unit << 4);
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x08 + (ch->unit << 4);
    ch->flags |= ATA_NO_SLAVE;

    /* XXX SOS PHY hotplug handling missing in SiS chip ?? */
    /* XXX SOS unknown how to enable PHY state change interrupt */
    return 0;
}

static void
ata_sis_reset(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_sata_phy_enable(ch);
}


static void
ata_sis_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if (ctlr->chip->cfg1 == SIS133NEW) {
	if (mode > ATA_UDMA2 &&
	    pci_read_config(gparent, ch->unit ? 0x52 : 0x50,2) & 0x8000) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
    }
    else {
	if (mode > ATA_UDMA2 &&
	    pci_read_config(gparent, 0x48, 1)&(ch->unit ? 0x20 : 0x10)) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
    }

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "",
		      ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	switch (ctlr->chip->cfg1) {
	case SIS133NEW: {
	    u_int32_t timings[] = 
		{ 0x28269008, 0x0c266008, 0x04263008, 0x0c0a3008, 0x05093008,
		  0x22196008, 0x0c0a3008, 0x05093008, 0x050939fc, 0x050936ac,
		  0x0509347c, 0x0509325c, 0x0509323c, 0x0509322c, 0x0509321c};
	    u_int32_t reg;

	    reg = (pci_read_config(gparent, 0x57, 1)&0x40?0x70:0x40)+(devno<<2);
	    pci_write_config(gparent, reg, timings[ata_mode2idx(mode)], 4);
	    break;
	    }
	case SIS133OLD: {
	    u_int16_t timings[] =
	     { 0x00cb, 0x0067, 0x0044, 0x0033, 0x0031, 0x0044, 0x0033, 0x0031,
	       0x8f31, 0x8a31, 0x8731, 0x8531, 0x8331, 0x8231, 0x8131 };
		  
	    u_int16_t reg = 0x40 + (devno << 1);

	    pci_write_config(gparent, reg, timings[ata_mode2idx(mode)], 2);
	    break;
	    }
	case SIS100NEW: {
	    u_int16_t timings[] =
		{ 0x00cb, 0x0067, 0x0044, 0x0033, 0x0031, 0x0044, 0x0033,
		  0x0031, 0x8b31, 0x8731, 0x8531, 0x8431, 0x8231, 0x8131 };
	    u_int16_t reg = 0x40 + (devno << 1);

	    pci_write_config(gparent, reg, timings[ata_mode2idx(mode)], 2);
	    break;
	    }
	case SIS100OLD:
	case SIS66:
	case SIS33: {
	    u_int16_t timings[] =
		{ 0x0c0b, 0x0607, 0x0404, 0x0303, 0x0301, 0x0404, 0x0303,
		  0x0301, 0xf301, 0xd301, 0xb301, 0xa301, 0x9301, 0x8301 };
	    u_int16_t reg = 0x40 + (devno << 1);

	    pci_write_config(gparent, reg, timings[ata_mode2idx(mode)], 2);
	    break;
	    }
	}
	atadev->mode = mode;
    }
}


/* VIA Technologies Inc. chipset support functions */
int
ata_via_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_VIA82C586, 0x02, VIA33,  0x00,   ATA_UDMA2, "VIA 82C586B" },
     { ATA_VIA82C586, 0x00, VIA33,  0x00,   ATA_WDMA2, "VIA 82C586" },
     { ATA_VIA82C596, 0x12, VIA66,  VIACLK, ATA_UDMA4, "VIA 82C596B" },
     { ATA_VIA82C596, 0x00, VIA33,  0x00,   ATA_UDMA2, "VIA 82C596" },
     { ATA_VIA82C686, 0x40, VIA100, VIABUG, ATA_UDMA5, "VIA 82C686B"},
     { ATA_VIA82C686, 0x10, VIA66,  VIACLK, ATA_UDMA4, "VIA 82C686A" },
     { ATA_VIA82C686, 0x00, VIA33,  0x00,   ATA_UDMA2, "VIA 82C686" },
     { ATA_VIA8231,   0x00, VIA100, VIABUG, ATA_UDMA5, "VIA 8231" },
     { ATA_VIA8233,   0x00, VIA100, 0x00,   ATA_UDMA5, "VIA 8233" },
     { ATA_VIA8233C,  0x00, VIA100, 0x00,   ATA_UDMA5, "VIA 8233C" },
     { ATA_VIA8233A,  0x00, VIA133, 0x00,   ATA_UDMA6, "VIA 8233A" },
     { ATA_VIA8235,   0x00, VIA133, 0x00,   ATA_UDMA6, "VIA 8235" },
     { ATA_VIA8237,   0x00, VIA133, 0x00,   ATA_UDMA6, "VIA 8237" },
     { 0, 0, 0, 0, 0, 0 }};
    static struct ata_chip_id new_ids[] =
    {{ ATA_VIA6410,   0x00, 0,      0x00,   ATA_UDMA6, "VIA 6410" },
     { ATA_VIA6420,   0x00, 7,      0x00,   ATA_SA150, "VIA 6420" },
     { ATA_VIA6421,   0x00, 6,      VIABAR, ATA_SA150, "VIA 6421" },
     { 0, 0, 0, 0, 0, 0 }};
    char buffer[64];

    if (pci_get_devid(dev) == ATA_VIA82C571) {
	if (!(idx = ata_find_chip(dev, ids, -99))) 
	    return ENXIO;
    }
    else {
	if (!(idx = ata_match_chip(dev, new_ids))) 
	    return ENXIO;
    }

    sprintf(buffer, "%s %s controller", idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_via_chipinit;
    return 0;
}

static int
ata_via_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;
    
    if (ctlr->chip->max_dma >= ATA_SA150) {
	ctlr->r_type2 = SYS_RES_IOPORT;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE))) {
	    pci_write_config(dev, PCIR_COMMAND,
			     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400,2);
	    ctlr->allocate = ata_via_allocate;
	    ctlr->reset = ata_via_reset;
	}
	ctlr->setmode = ata_sata_setmode;
	return 0;
    }

    /* prepare for ATA-66 on the 82C686a and 82C596b */
    if (ctlr->chip->cfg2 & VIACLK)
	pci_write_config(dev, 0x50, 0x030b030b, 4);       

    /* the southbridge might need the data corruption fix */
    if (ctlr->chip->cfg2 & VIABUG)
	ata_via_southbridge_fixup(dev);

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

    ctlr->setmode = ata_via_family_setmode;
    return 0;
}

static int
ata_via_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);


    /* newer SATA chips has resources in one BAR for each channel */
    if (ctlr->chip->cfg2 & VIABAR) {
	struct resource *r_io;
	int i, rid;
		
	rid = PCIR_BAR(ch->unit);
	if (!(r_io = bus_alloc_resource_any(device_get_parent(dev),
	   				    SYS_RES_IOPORT,
					    &rid, RF_ACTIVE)))
	    return ENXIO;

	for (i = ATA_DATA; i <= ATA_COMMAND; i ++) {
	    ch->r_io[i].res = r_io;
	    ch->r_io[i].offset = i;
	}
	ch->r_io[ATA_CONTROL].res = r_io;
	ch->r_io[ATA_CONTROL].offset = 2 + ATA_IOSIZE;
	ch->r_io[ATA_IDX_ADDR].res = r_io;
	ata_default_registers(dev);
	for (i = ATA_BMCMD_PORT; i <= ATA_BMDTP_PORT; i++) {
	    ch->r_io[i].res = ctlr->r_res1;
	    ch->r_io[i].offset = i - ATA_BMCMD_PORT;
	}
	ata_generic_hw(dev);
    }
    else
	ata_pci_allocate(dev);

    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = (ch->unit << ctlr->chip->cfg1);
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x04 + (ch->unit << ctlr->chip->cfg1);
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x08 + (ch->unit << ctlr->chip->cfg1);
    ch->flags |= ATA_NO_SLAVE;

    /* XXX SOS PHY hotplug handling missing in VIA chip ?? */
    /* XXX SOS unknown how to enable PHY state change interrupt */
    return 0;
}

static void
ata_via_reset(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_sata_phy_enable(ch);
}

static void
ata_via_southbridge_fixup(device_t dev)
{
    device_t *children;
    int nchildren, i;

    if (device_get_children(device_get_parent(dev), &children, &nchildren))
	return;

    for (i = 0; i < nchildren; i++) {
	if (pci_get_devid(children[i]) == ATA_VIA8363 ||
	    pci_get_devid(children[i]) == ATA_VIA8371 ||
	    pci_get_devid(children[i]) == ATA_VIA8662 ||
	    pci_get_devid(children[i]) == ATA_VIA8361) {
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


/* common code for VIA, AMD & nVidia */
static void
ata_via_family_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    u_int8_t timings[] = { 0xa8, 0x65, 0x42, 0x22, 0x20, 0x42, 0x22, 0x20,
			   0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
    int modes[][7] = {
	{ 0xc2, 0xc1, 0xc0, 0x00, 0x00, 0x00, 0x00 },   /* VIA ATA33 */
	{ 0xee, 0xec, 0xea, 0xe9, 0xe8, 0x00, 0x00 },   /* VIA ATA66 */
	{ 0xf7, 0xf6, 0xf4, 0xf2, 0xf1, 0xf0, 0x00 },   /* VIA ATA100 */
	{ 0xf7, 0xf7, 0xf6, 0xf4, 0xf2, 0xf1, 0xf0 },   /* VIA ATA133 */
	{ 0xc2, 0xc1, 0xc0, 0xc4, 0xc5, 0xc6, 0xc7 }};  /* AMD/nVIDIA */
    int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);
    int reg = 0x53 - devno;
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if (ctlr->chip->cfg2 & AMDCABLE) {
	if (mode > ATA_UDMA2 &&
	    !(pci_read_config(gparent, 0x42, 1) & (1 << devno))) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
    }
    else 
	mode = ata_check_80pin(dev, mode);

    if (ctlr->chip->cfg2 & NVIDIA)
	reg += 0x10;

    if (ctlr->chip->cfg1 != VIA133)
	pci_write_config(gparent, reg - 0x08, timings[ata_mode2idx(mode)], 1);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "", ata_mode2str(mode),
		      ctlr->chip->text);
    if (!error) {
	if (mode >= ATA_UDMA0)
	    pci_write_config(gparent, reg,
			     modes[ctlr->chip->cfg1][mode & ATA_MODE_MASK], 1);
	else
	    pci_write_config(gparent, reg, 0x8b, 1);
	atadev->mode = mode;
    }
}


/* misc functions */
struct ata_chip_id *
ata_match_chip(device_t dev, struct ata_chip_id *index)
{
    while (index->chipid != 0) {
	if (pci_get_devid(dev) == index->chipid &&
	    pci_get_revid(dev) >= index->chiprev)
	    return index;
	index++;
    }
    return NULL;
}

static struct ata_chip_id *
ata_find_chip(device_t dev, struct ata_chip_id *index, int slot)
{
    device_t *children;
    int nchildren, i;

    if (device_get_children(device_get_parent(dev), &children, &nchildren))
	return 0;

    while (index->chipid != 0) {
	for (i = 0; i < nchildren; i++) {
	    if (((slot >= 0 && pci_get_slot(children[i]) == slot) || 
		 (slot < 0 && pci_get_slot(children[i]) <= -slot)) &&
		pci_get_devid(children[i]) == index->chipid &&
		pci_get_revid(children[i]) >= index->chiprev) {
		free(children, M_TEMP);
		return index;
	    }
	}
	index++;
    }
    free(children, M_TEMP);
    return NULL;
}

static int
ata_setup_interrupt(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    int rid = ATA_IRQ_RID;

    if (!ata_legacy(dev)) {
	if (!(ctlr->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
						   RF_SHAREABLE | RF_ACTIVE))) {
	    device_printf(dev, "unable to map interrupt\n");
	    return ENXIO;
	}
	if ((bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			    ata_generic_intr, ctlr, &ctlr->handle))) {
	    device_printf(dev, "unable to setup interrupt\n");
	    return ENXIO;
	}
    }
    return 0;
}

struct ata_serialize {
    struct mtx  locked_mtx;
    int         locked_ch;
    int         restart_ch;
};

static int
ata_serialize(device_t dev, int flags)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_serialize *serial;
    static int inited = 0;
    int res;

    if (!inited) {
	serial = malloc(sizeof(struct ata_serialize),
			      M_TEMP, M_NOWAIT | M_ZERO);
	mtx_init(&serial->locked_mtx, "ATA serialize lock", NULL, MTX_DEF); 
	serial->locked_ch = -1;
	serial->restart_ch = -1;
	device_set_ivars(ctlr->dev, serial);
	inited = 1;
    }
    else
	serial = device_get_ivars(ctlr->dev);

    mtx_lock(&serial->locked_mtx);
    switch (flags) {
    case ATA_LF_LOCK:
	if (serial->locked_ch == -1)
	    serial->locked_ch = ch->unit;
	if (serial->locked_ch != ch->unit)
	    serial->restart_ch = ch->unit;
	break;

    case ATA_LF_UNLOCK:
	if (serial->locked_ch == ch->unit) {
	    serial->locked_ch = -1;
	    if (serial->restart_ch != -1) {
		if ((ch = ctlr->interrupt[serial->restart_ch].argument)) {
		    serial->restart_ch = -1;
		    mtx_unlock(&serial->locked_mtx);
		    ata_start(ch->dev);
		    return -1;
		}
	    }
	}
	break;

    case ATA_LF_WHICH:
	break;
    }
    res = serial->locked_ch;
    mtx_unlock(&serial->locked_mtx);
    return res;
}

static void
ata_print_cable(device_t dev, u_int8_t *who)
{
    device_printf(dev,
		  "DMA limited to UDMA33, %s found non-ATA66 cable\n", who);
}

static int
ata_atapi(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);

    return ((atadev->unit == ATA_MASTER && ch->devices & ATA_ATAPI_MASTER) ||
	    (atadev->unit == ATA_SLAVE && ch->devices & ATA_ATAPI_SLAVE));
}

static int
ata_check_80pin(device_t dev, int mode)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (mode > ATA_UDMA2 && !(atadev->param.hwres & ATA_CABLE_ID)) {
	ata_print_cable(dev, "device");
	mode = ATA_UDMA2;
    }
    return mode;
}

static int
ata_mode2idx(int mode)
{
    if ((mode & ATA_DMA_MASK) == ATA_UDMA0)
	 return (mode & ATA_MODE_MASK) + 8;
    if ((mode & ATA_DMA_MASK) == ATA_WDMA0)
	 return (mode & ATA_MODE_MASK) + 5;
    return (mode & ATA_MODE_MASK) - ATA_PIO0;
}
