/*-
 * Copyright (c) 1998 - 2007 Søren Schmidt <sos@FreeBSD.org>
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
/* ata-chipset.c */
static int ata_generic_chipinit(device_t dev);
static void ata_generic_intr(void *data);
static void ata_generic_setmode(device_t dev, int mode);
static void ata_sata_phy_check_events(device_t dev);
static void ata_sata_phy_event(void *context, int dummy);
static int ata_sata_phy_reset(device_t dev);
static int ata_sata_connect(struct ata_channel *ch);
static void ata_sata_setmode(device_t dev, int mode);
static int ata_request2fis_h2d(struct ata_request *request, u_int8_t *fis);
static int ata_ahci_chipinit(device_t dev);
static int ata_ahci_allocate(device_t dev);
static int ata_ahci_status(device_t dev);
static int ata_ahci_begin_transaction(struct ata_request *request);
static int ata_ahci_end_transaction(struct ata_request *request);
static void ata_ahci_reset(device_t dev);
static void ata_ahci_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static void ata_ahci_dmainit(device_t dev);
static int ata_ahci_setup_fis(struct ata_ahci_cmd_tab *ctp, struct ata_request *request);
static int ata_acard_chipinit(device_t dev);
static int ata_acard_allocate(device_t dev);
static int ata_acard_status(device_t dev);
static void ata_acard_850_setmode(device_t dev, int mode);
static void ata_acard_86X_setmode(device_t dev, int mode);
static int ata_ali_chipinit(device_t dev);
static int ata_ali_allocate(device_t dev);
static int ata_ali_sata_allocate(device_t dev);
static void ata_ali_reset(device_t dev);
static void ata_ali_setmode(device_t dev, int mode);
static int ata_amd_chipinit(device_t dev);
static int ata_ati_chipinit(device_t dev);
static void ata_ati_setmode(device_t dev, int mode);
static int ata_cyrix_chipinit(device_t dev);
static void ata_cyrix_setmode(device_t dev, int mode);
static int ata_cypress_chipinit(device_t dev);
static void ata_cypress_setmode(device_t dev, int mode);
static int ata_highpoint_chipinit(device_t dev);
static int ata_highpoint_allocate(device_t dev);
static void ata_highpoint_setmode(device_t dev, int mode);
static int ata_highpoint_check_80pin(device_t dev, int mode);
static int ata_intel_chipinit(device_t dev);
static int ata_intel_allocate(device_t dev);
static void ata_intel_reset(device_t dev);
static void ata_intel_old_setmode(device_t dev, int mode);
static void ata_intel_new_setmode(device_t dev, int mode);
static void ata_intel_sata_setmode(device_t dev, int mode);
static int ata_intel_31244_allocate(device_t dev);
static int ata_intel_31244_status(device_t dev);
static void ata_intel_31244_tf_write(struct ata_request *request);
static void ata_intel_31244_reset(device_t dev);
static int ata_ite_chipinit(device_t dev);
static void ata_ite_setmode(device_t dev, int mode);
static int ata_jmicron_chipinit(device_t dev);
static int ata_jmicron_allocate(device_t dev);
static void ata_jmicron_reset(device_t dev);
static void ata_jmicron_dmainit(device_t dev);
static void ata_jmicron_setmode(device_t dev, int mode);
static int ata_marvell_pata_chipinit(device_t dev);
static int ata_marvell_pata_allocate(device_t dev);
static void ata_marvell_pata_setmode(device_t dev, int mode);
static int ata_marvell_edma_chipinit(device_t dev);
static int ata_marvell_edma_allocate(device_t dev);
static int ata_marvell_edma_status(device_t dev);
static int ata_marvell_edma_begin_transaction(struct ata_request *request);
static int ata_marvell_edma_end_transaction(struct ata_request *request);
static void ata_marvell_edma_reset(device_t dev);
static void ata_marvell_edma_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static void ata_marvell_edma_dmainit(device_t dev);
static int ata_national_chipinit(device_t dev);
static void ata_national_setmode(device_t dev, int mode);
static int ata_netcell_chipinit(device_t dev);
static int ata_netcell_allocate(device_t dev);
static int ata_nvidia_chipinit(device_t dev);
static int ata_nvidia_allocate(device_t dev);
static int ata_nvidia_status(device_t dev);
static void ata_nvidia_reset(device_t dev);
static int ata_promise_chipinit(device_t dev);
static int ata_promise_allocate(device_t dev);
static int ata_promise_status(device_t dev);
static int ata_promise_dmastart(device_t dev);
static int ata_promise_dmastop(device_t dev);
static void ata_promise_dmareset(device_t dev);
static void ata_promise_dmainit(device_t dev);
static void ata_promise_setmode(device_t dev, int mode);
static int ata_promise_tx2_allocate(device_t dev);
static int ata_promise_tx2_status(device_t dev);
static int ata_promise_mio_allocate(device_t dev);
static void ata_promise_mio_intr(void *data);
static int ata_promise_mio_status(device_t dev);
static int ata_promise_mio_command(struct ata_request *request);
static void ata_promise_mio_reset(device_t dev);
static void ata_promise_mio_dmainit(device_t dev);
static void ata_promise_mio_setprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static void ata_promise_mio_setmode(device_t dev, int mode);
static void ata_promise_sx4_intr(void *data);
static int ata_promise_sx4_command(struct ata_request *request);
static int ata_promise_apkt(u_int8_t *bytep, struct ata_request *request);
static void ata_promise_queue_hpkt(struct ata_pci_controller *ctlr, u_int32_t hpkt);
static void ata_promise_next_hpkt(struct ata_pci_controller *ctlr);
static int ata_serverworks_chipinit(device_t dev);
static int ata_serverworks_allocate(device_t dev);
static void ata_serverworks_tf_read(struct ata_request *request);
static void ata_serverworks_tf_write(struct ata_request *request);
static void ata_serverworks_setmode(device_t dev, int mode);
static int ata_sii_chipinit(device_t dev);
static int ata_cmd_allocate(device_t dev);
static int ata_cmd_status(device_t dev);
static void ata_cmd_setmode(device_t dev, int mode);
static int ata_sii_allocate(device_t dev);
static int ata_sii_status(device_t dev);
static void ata_sii_reset(device_t dev);
static void ata_sii_setmode(device_t dev, int mode);
static int ata_siiprb_allocate(device_t dev);
static int ata_siiprb_status(device_t dev);
static int ata_siiprb_begin_transaction(struct ata_request *request);
static int ata_siiprb_end_transaction(struct ata_request *request);
static void ata_siiprb_reset(device_t dev);
static void ata_siiprb_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static void ata_siiprb_dmainit(device_t dev);
static int ata_sis_chipinit(device_t dev);
static int ata_sis_allocate(device_t dev);
static void ata_sis_reset(device_t dev);
static void ata_sis_setmode(device_t dev, int mode);
static int ata_via_chipinit(device_t dev);
static int ata_via_allocate(device_t dev);
static void ata_via_reset(device_t dev);
static void ata_via_setmode(device_t dev, int mode);
static void ata_via_southbridge_fixup(device_t dev);
static void ata_via_family_setmode(device_t dev, int mode);
static void ata_set_desc(device_t dev);
static struct ata_chip_id *ata_match_chip(device_t dev, struct ata_chip_id *index);
static struct ata_chip_id *ata_find_chip(device_t dev, struct ata_chip_id *index, int slot);
static int ata_setup_interrupt(device_t dev);
static int ata_serialize(device_t dev, int flags);
static void ata_print_cable(device_t dev, u_int8_t *who);
static int ata_atapi(device_t dev);
static int ata_check_80pin(device_t dev, int mode);
static int ata_mode2idx(int mode);


/*
 * generic ATA support functions
 */
int
ata_generic_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    char buffer[64];

    sprintf(buffer, "%s ATA controller", ata_pcivendor2str(dev));
    device_set_desc_copy(dev, buffer);
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
	if ((ch = ctlr->interrupt[unit].argument))
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


/*
 * SATA support functions
 */
static void
ata_sata_phy_check_events(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t error = ATA_IDX_INL(ch, ATA_SERROR);

    /* clear error bits/interrupt */
    ATA_IDX_OUTL(ch, ATA_SERROR, error);

    /* do we have any events flagged ? */
    if (error) {
	struct ata_connect_task *tp;
	u_int32_t status = ATA_IDX_INL(ch, ATA_SSTATUS);

	/* if we have a connection event deal with it */
	if ((error & ATA_SE_PHY_CHANGED) &&
	    (tp = (struct ata_connect_task *)
		  malloc(sizeof(struct ata_connect_task),
			 M_ATA, M_NOWAIT | M_ZERO))) {

	    if (((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN1) ||
		((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN2)) {
		if (bootverbose)
		    device_printf(ch->dev, "CONNECT requested\n");
		tp->action = ATA_C_ATTACH;
	    }
	    else {
		if (bootverbose)
		    device_printf(ch->dev, "DISCONNECT requested\n");
		tp->action = ATA_C_DETACH;
	    }
	    tp->dev = ch->dev;
	    TASK_INIT(&tp->task, 0, ata_sata_phy_event, tp);
	    taskqueue_enqueue(taskqueue_thread, &tp->task);
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
	if (bootverbose)
	    device_printf(tp->dev, "CONNECTED\n");
	ATA_RESET(tp->dev);
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
	if (bootverbose)
	    device_printf(tp->dev, "DISCONNECTED\n");
    }
    mtx_unlock(&Giant); /* suckage code dealt with, release Giant */
    free(tp, M_ATA);
}

static int
ata_sata_phy_reset(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    int loop, retry;

    if ((ATA_IDX_INL(ch, ATA_SCONTROL) & ATA_SC_DET_MASK) == ATA_SC_DET_IDLE)
	return ata_sata_connect(ch);

    for (retry = 0; retry < 10; retry++) {
	for (loop = 0; loop < 10; loop++) {
	    ATA_IDX_OUTL(ch, ATA_SCONTROL, ATA_SC_DET_RESET);
	    ata_udelay(100);
	    if ((ATA_IDX_INL(ch, ATA_SCONTROL) & ATA_SC_DET_MASK) == 
		ATA_SC_DET_RESET)
		break;
	}
	ata_udelay(5000);
	for (loop = 0; loop < 10; loop++) {
	    ATA_IDX_OUTL(ch, ATA_SCONTROL, ATA_SC_DET_IDLE |
					   ATA_SC_IPM_DIS_PARTIAL |
					   ATA_SC_IPM_DIS_SLUMBER);
	    ata_udelay(100);
	    if ((ATA_IDX_INL(ch, ATA_SCONTROL) & ATA_SC_DET_MASK) == 0)
		return ata_sata_connect(ch);
	}
    }
    return 0;
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
    if (bootverbose)
	device_printf(ch->dev, "SATA connect time=%dms\n", timeout * 10);

    /* clear SATA error register */
    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

    return 1;
}

static void
ata_sata_setmode(device_t dev, int mode)
{
    struct ata_device *atadev = device_get_softc(dev);

    /*
     * if we detect that the device isn't a real SATA device we limit 
     * the transfer mode to UDMA5/ATA100.
     * this works around the problems some devices has with the 
     * Marvell 88SX8030 SATA->PATA converters and UDMA6/ATA133.
     */
    if (atadev->param.satacapabilities != 0x0000 &&
	atadev->param.satacapabilities != 0xffff) {
	struct ata_channel *ch = device_get_softc(device_get_parent(dev));

	/* on some drives we need to set the transfer mode */
	ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0,
		       ata_limit_mode(dev, mode, ATA_UDMA6));

	/* query SATA STATUS for the speed */
        if (ch->r_io[ATA_SSTATUS].res && 
	   ((ATA_IDX_INL(ch, ATA_SSTATUS) & ATA_SS_CONWELL_MASK) ==
	    ATA_SS_CONWELL_GEN2))
	    atadev->mode = ATA_SA300;
	else 
	    atadev->mode = ATA_SA150;
    }
    else {
	mode = ata_limit_mode(dev, mode, ATA_UDMA5);
	if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
	    atadev->mode = mode;
    }
}

static int
ata_request2fis_h2d(struct ata_request *request, u_int8_t *fis)
{
    struct ata_device *atadev = device_get_softc(request->dev);

    if (request->flags & ATA_R_ATAPI) {
	fis[0] = 0x27;  /* host to device */
	fis[1] = 0x80;  /* command FIS (note PM goes here) */
	fis[2] = ATA_PACKET_CMD;
	if (request->flags & (ATA_R_READ | ATA_R_WRITE))
	    fis[3] = ATA_F_DMA;
	else {
	    fis[5] = request->transfersize;
	    fis[6] = request->transfersize >> 8;
	}
	fis[7] = ATA_D_LBA | atadev->unit;
	fis[15] = ATA_A_4BIT;
	return 20;
    }
    else {
	ata_modify_if_48bit(request);
	fis[0] = 0x27;  /* host to device */
	fis[1] = 0x80;  /* command FIS (note PM goes here) */
	fis[2] = request->u.ata.command;
	fis[3] = request->u.ata.feature;
	fis[4] = request->u.ata.lba;
	fis[5] = request->u.ata.lba >> 8;
	fis[6] = request->u.ata.lba >> 16;
	fis[7] = ATA_D_LBA | atadev->unit;
	if (!(atadev->flags & ATA_D_48BIT_ACTIVE))
	    fis[7] |= (request->u.ata.lba >> 24 & 0x0f);
	fis[8] = request->u.ata.lba >> 24;
	fis[9] = request->u.ata.lba >> 32; 
	fis[10] = request->u.ata.lba >> 40; 
	fis[11] = request->u.ata.feature >> 8;
	fis[12] = request->u.ata.count;
	fis[13] = request->u.ata.count >> 8;
	fis[15] = ATA_A_4BIT;
	return 20;
    }
    return 0;
}


/*
 * AHCI v1.x compliant SATA chipset support functions
 */
int
ata_ahci_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    char buffer[64];

    /* is this PCI device flagged as an AHCI compliant chip ? */
    if (pci_read_config(dev, PCIR_PROGIF, 1) != 0x01)
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

static int
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
	if (ata_setup_interrupt(dev))
	    return ENXIO;
    }
    else
	device_printf(dev, "AHCI called from vendor specific driver\n");

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

    /* get the number of HW channels */
    ctlr->channels =
	MAX(flsl(ATA_INL(ctlr->r_res2, ATA_AHCI_PI)), 
	    (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_NPMASK) + 1);

    /* clear interrupts */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_IS, ATA_INL(ctlr->r_res2, ATA_AHCI_IS));

    /* enable AHCI interrupts */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_GHC,
	     ATA_INL(ctlr->r_res2, ATA_AHCI_GHC) | ATA_AHCI_GHC_IE);

    ctlr->reset = ata_ahci_reset;
    ctlr->dmainit = ata_ahci_dmainit;
    ctlr->allocate = ata_ahci_allocate;
    ctlr->setmode = ata_sata_setmode;

    /* enable PCI interrupt */
    pci_write_config(dev, PCIR_COMMAND,
		     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);

    /* announce we support the HW */
    version = ATA_INL(ctlr->r_res2, ATA_AHCI_VS);
    device_printf(dev,
		  "AHCI Version %x%x.%x%x controller with %d ports detected\n",
		  (version >> 24) & 0xff, (version >> 16) & 0xff,
		  (version >> 8) & 0xff, version & 0xff,
		  (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_NPMASK) + 1);
    return 0;
}

static int
ata_ahci_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int64_t work;
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

    /* setup work areas */
    work = ch->dma->work_bus + ATA_AHCI_CL_OFFSET;
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CLB + offset, work & 0xffffffff);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CLBU + offset, work >> 32);

    work = ch->dma->work_bus + ATA_AHCI_FB_OFFSET;
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FB + offset, work & 0xffffffff); 
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_FBU + offset, work >> 32);

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
ata_ahci_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t action = ATA_INL(ctlr->r_res2, ATA_AHCI_IS);
    int offset = ch->unit << 7;
    int tag = 0;

    if (action & (1 << ch->unit)) {
	u_int32_t istatus = ATA_INL(ctlr->r_res2, ATA_AHCI_P_IS + offset);
	u_int32_t cstatus = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CI + offset);

	/* clear interrupt(s) */
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_IS, action & (1 << ch->unit));
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IS + offset, istatus);

	/* do we have any PHY events ? */
	/* XXX SOS check istatus phy bits */
	ata_sata_phy_check_events(dev);

	/* do we have a potentially hanging engine to take care of? */
	if ((istatus & 0x78400050) && (cstatus & (1 << tag))) {

	    u_int32_t cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
	    int timeout = 0;

	    /* kill off all activity on this channel */
	    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		     cmd & ~(ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST));

	    /* XXX SOS this is not entirely wrong */
	    do {
		DELAY(1000);
		if (timeout++ > 500) {
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
	    return (!(cstatus & (1 << tag)));
    }
    return 0;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_ahci_begin_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_ahci_cmd_tab *ctp;
    struct ata_ahci_cmd_list *clp;
    int offset = ch->unit << 7;
    int tag = 0, entries = 0;
    int fis_size;
	
    /* get a piece of the workspace for this request */
    ctp = (struct ata_ahci_cmd_tab *)
	  (ch->dma->work + ATA_AHCI_CT_OFFSET + (ATA_AHCI_CT_SIZE * tag));

    /* setup the FIS for this request */
    if (!(fis_size = ata_ahci_setup_fis(ctp, request))) {
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
		     (request->flags & ATA_R_ATAPI ? ((1<<5) | (1<<7)) : 0) |
		     (fis_size / sizeof(u_int32_t));
    clp->bytecount = 0;
    clp->cmd_table_phys = htole64(ch->dma->work_bus + ATA_AHCI_CT_OFFSET +
				  (ATA_AHCI_CT_SIZE * tag));

    /* clear eventual ACTIVE bit */
    ATA_IDX_OUTL(ch, ATA_SACTIVE, ATA_IDX_INL(ch, ATA_SACTIVE) & (1 << tag));

    /* set command type bit */
    if (request->flags & ATA_R_ATAPI)
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		 ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset) |
		 ATA_AHCI_P_CMD_ATAPI);
    else
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		 ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset) &
		 ~ATA_AHCI_P_CMD_ATAPI);

    /* issue command to controller */
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CI + offset, (1 << tag));
    
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
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_ahci_cmd_list *clp;
    u_int32_t tf_data;
    int offset = ch->unit << 7;
    int tag = 0;

    /* kill the timeout */
    callout_stop(&request->callout);

    /* get status */
    tf_data = ATA_INL(ctlr->r_res2, ATA_AHCI_P_TFD + offset);
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
ata_ahci_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t cmd, signature;
    int offset = ch->unit << 7;
    int timeout;

    if (!(ATA_INL(ctlr->r_res2, ATA_AHCI_PI) & (1 << ch->unit))) {
	device_printf(dev, "port not implemented\n");
	return;
    }
    ch->devices = 0;

    /* kill off all activity on this channel */
    cmd = ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD + offset);
    ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
	     cmd & ~(ATA_AHCI_P_CMD_FRE | ATA_AHCI_P_CMD_ST));

    /* XXX SOS this is not entirely wrong */
    timeout = 0;
    do {
	DELAY(1000);
	if (timeout++ > 500) {
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
	    if (timeout++ > 500) {
		device_printf(dev, "executing CLO failed\n");
		break;
	    }
        }
	while (ATA_INL(ctlr->r_res2, ATA_AHCI_P_CMD+offset)&ATA_AHCI_P_CMD_CLO);
    }

    /* reset PHY and decide what is present */
    if (ata_sata_phy_reset(dev)) {

	/* clear any interrupts pending on this channel */
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_IS + offset,
		 ATA_INL(ctlr->r_res2, ATA_AHCI_P_IS + offset));

	/* clear SATA error register */
	ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

	/* start operations on this channel */
	ATA_OUTL(ctlr->r_res2, ATA_AHCI_P_CMD + offset,
		 (ATA_AHCI_P_CMD_ACTIVE | ATA_AHCI_P_CMD_FRE |
		  ATA_AHCI_P_CMD_POD | ATA_AHCI_P_CMD_SUD | ATA_AHCI_P_CMD_ST));

	signature = ATA_INL(ctlr->r_res2, ATA_AHCI_P_SIG + offset);
	if (bootverbose)
	    device_printf(dev, "SIGNATURE: %08x\n", signature);
	switch (signature) {
	case 0x00000101:
	    ch->devices = ATA_ATA_MASTER;
	    break;
	case 0x96690101:
	    ch->devices = ATA_PORTMULTIPLIER;
	    device_printf(ch->dev, "Portmultipliers not supported yet\n");
	    ch->devices = 0;
	    break;
	case 0xeb140101:
	    ch->devices = ATA_ATAPI_MASTER;
	    break;
	default: /* SOS XXX */
	    if (bootverbose)
		device_printf(ch->dev, "No signature, asuming disk device\n");
	    ch->devices = ATA_ATA_MASTER;
	}
    }
    if (bootverbose)
        device_printf(dev, "ahci_reset devices=0x%b\n", ch->devices,
                      "\20\4ATAPI_SLAVE\3ATAPI_MASTER\2ATA_SLAVE\1ATA_MASTER");
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
    KASSERT(nsegs <= ATA_DMA_ENTRIES, ("too many DMA segment entries\n"));
    args->nsegs = nsegs;
}

static void
ata_ahci_dmainit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    if (ch->dma) {
	/* note start and stop are not used here */
	ch->dma->setprd = ata_ahci_dmasetprd;
	ch->dma->max_iosize = 8192 * DEV_BSIZE;
	if (ATA_INL(ctlr->r_res2, ATA_AHCI_CAP) & ATA_AHCI_CAP_64BIT)
	    ch->dma->max_address = BUS_SPACE_MAXADDR;
    }
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


/*
 * Acard chipset support functions
 */
int
ata_acard_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_ATP850R, 0, ATPOLD, 0x00, ATA_UDMA2, "ATP850" },
     { ATA_ATP860A, 0, 0,      0x00, ATA_UDMA4, "ATP860A" },
     { ATA_ATP860R, 0, 0,      0x00, ATA_UDMA4, "ATP860R" },
     { ATA_ATP865A, 0, 0,      0x00, ATA_UDMA6, "ATP865A" },
     { ATA_ATP865R, 0, 0,      0x00, ATA_UDMA6, "ATP865R" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_acard_chipinit;
    return 0;
}

static int
ata_acard_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    ctlr->allocate = ata_acard_allocate;
    if (ctlr->chip->cfg1 == ATPOLD) {
	ctlr->setmode = ata_acard_850_setmode;
	ctlr->locking = ata_serialize;
    }
    else
	ctlr->setmode = ata_acard_86X_setmode;
    return 0;
}

static int
ata_acard_allocate(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->hw.status = ata_acard_status;
    return 0;
}

static int
ata_acard_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    if (ctlr->chip->cfg1 == ATPOLD &&
	ATA_LOCKING(ch->dev, ATA_LF_WHICH) != ch->unit)
	    return 0;
    if (ch->dma && (ch->dma->flags & ATA_DMA_ACTIVE)) {
	int bmstat = ATA_IDX_INB(ch, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;

	if ((bmstat & (ATA_BMSTAT_ACTIVE | ATA_BMSTAT_INTERRUPT)) !=
	    ATA_BMSTAT_INTERRUPT)
	    return 0;
	ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, bmstat & ~ATA_BMSTAT_ERROR);
	DELAY(1);
	ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
		     ATA_IDX_INB(ch, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
	DELAY(1);
    }
    if (ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_BUSY) {
	DELAY(100);
	if (ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_BUSY)
	    return 0;
    }
    return 1;
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
    static struct ata_chip_id ids[] =
    {{ ATA_ALI_5289, 0x00, 2, ALISATA, ATA_SA150, "M5289" },
     { ATA_ALI_5288, 0x00, 4, ALISATA, ATA_SA300, "M5288" },
     { ATA_ALI_5287, 0x00, 4, ALISATA, ATA_SA150, "M5287" },
     { ATA_ALI_5281, 0x00, 2, ALISATA, ATA_SA150, "M5281" },
     { ATA_ALI_5229, 0xc5, 0, ALINEW,  ATA_UDMA6, "M5229" },
     { ATA_ALI_5229, 0xc4, 0, ALINEW,  ATA_UDMA5, "M5229" },
     { ATA_ALI_5229, 0xc2, 0, ALINEW,  ATA_UDMA4, "M5229" },
     { ATA_ALI_5229, 0x20, 0, ALIOLD,  ATA_UDMA2, "M5229" },
     { ATA_ALI_5229, 0x00, 0, ALIOLD,  ATA_WDMA2, "M5229" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
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
	ctlr->channels = ctlr->chip->cfg1;
	ctlr->allocate = ata_ali_sata_allocate;
	ctlr->setmode = ata_sata_setmode;

	/* AHCI mode is correctly supported only on the ALi 5288. */
	if ((ctlr->chip->chipid == ATA_ALI_5288) &&
	    (ata_ahci_chipinit(dev) != ENXIO))
            return 0;

	/* enable PCI interrupt */
	pci_write_config(dev, PCIR_COMMAND,
			 pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);
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
	ctlr->allocate = ata_ali_allocate;
	ctlr->reset = ata_ali_reset;
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
    if (ata_pci_allocate(dev))
	return ENXIO;

    /* older chips can't do 48bit DMA transfers */
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
    ata_pci_hw(dev);
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
    static struct ata_chip_id ids[] =
    {{ ATA_AMD756,  0x00, AMDNVIDIA, 0x00,            ATA_UDMA4, "756" },
     { ATA_AMD766,  0x00, AMDNVIDIA, AMDCABLE|AMDBUG, ATA_UDMA5, "766" },
     { ATA_AMD768,  0x00, AMDNVIDIA, AMDCABLE,        ATA_UDMA5, "768" },
     { ATA_AMD8111, 0x00, AMDNVIDIA, AMDCABLE,        ATA_UDMA6, "8111" },
     { ATA_AMD5536, 0x00, AMDNVIDIA, 0x00,            ATA_UDMA5, "CS5536" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
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
 * ATI chipset support functions
 */
int
ata_ati_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_ATI_IXP200,    0x00, 0, ATIPATA, ATA_UDMA5, "IXP200" },
     { ATA_ATI_IXP300,    0x00, 0, ATIPATA, ATA_UDMA6, "IXP300" },
     { ATA_ATI_IXP300_S1, 0x00, 0, ATISATA, ATA_SA150, "IXP300" },
     { ATA_ATI_IXP400,    0x00, 0, ATIPATA, ATA_UDMA6, "IXP400" },
     { ATA_ATI_IXP400_S1, 0x00, 0, ATISATA, ATA_SA150, "IXP400" },
     { ATA_ATI_IXP400_S2, 0x00, 0, ATISATA, ATA_SA150, "IXP400" },
     { ATA_ATI_IXP600,    0x00, 0, ATIPATA, ATA_UDMA6, "IXP600" },
     { ATA_ATI_IXP600_S1, 0x00, 0, ATIAHCI, ATA_SA300, "IXP600" },
     { ATA_ATI_IXP700,    0x00, 0, ATIPATA, ATA_UDMA6, "IXP700" },
     { ATA_ATI_IXP700_S1, 0x00, 0, ATIAHCI, ATA_SA300, "IXP700" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);

    switch (ctlr->chip->cfg2) {
    case ATIPATA:
	ctlr->chipinit = ata_ati_chipinit;
	break;
    case ATISATA:
	/* the ATI SATA controller is actually a SiI 3112 controller */
	ctlr->chip->cfg1 = SIIMEMIO;
	ctlr->chipinit = ata_sii_chipinit;
	break;
    case ATIAHCI:
	ctlr->chipinit = ata_ahci_chipinit;
	break;
    }
    return 0;
}

static int
ata_ati_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    /* IXP600 & IXP700 only have 1 PATA channel */
    if ((ctlr->chip->chipid == ATA_ATI_IXP600) ||
	(ctlr->chip->chipid == ATA_ATI_IXP700))
	ctlr->channels = 1;

    ctlr->setmode = ata_ati_setmode;
    return 0;
}

static void
ata_ati_setmode(device_t dev, int mode)
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
			     (dmatimings[mode & ATA_MODE_MASK] << offset), 4);
	}
	else
	    pci_write_config(gparent, 0x54,
			     pci_read_config(gparent, 0x54, 1) &
			     ~(0x01 << devno), 1);

	pci_write_config(gparent, 0x4a,
			 (pci_read_config(gparent, 0x4a, 2) &
			  ~(0xf << (devno << 2))) |
			 (((mode - ATA_PIO0) & ATA_MODE_MASK) << (devno<<2)),2);
	pci_write_config(gparent, 0x40, 
			 (pci_read_config(gparent, 0x40, 4) &
			  ~(0xff << offset)) |
			 (piotimings[ata_mode2idx(mode)] << offset), 4);
	atadev->mode = mode;
    }
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
    ch->dma->max_iosize = 64 * DEV_BSIZE;

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
    {{ ATA_HPT374, 0x07, HPT374, 0x00,   ATA_UDMA6, "HPT374" },
     { ATA_HPT372, 0x02, HPT372, 0x00,   ATA_UDMA6, "HPT372N" },
     { ATA_HPT372, 0x01, HPT372, 0x00,   ATA_UDMA6, "HPT372" },
     { ATA_HPT371, 0x01, HPT372, 0x00,   ATA_UDMA6, "HPT371" },
     { ATA_HPT366, 0x05, HPT372, 0x00,   ATA_UDMA6, "HPT372" },
     { ATA_HPT366, 0x03, HPT370, 0x00,   ATA_UDMA5, "HPT370" },
     { ATA_HPT366, 0x02, HPT366, 0x00,   ATA_UDMA4, "HPT368" },
     { ATA_HPT366, 0x00, HPT366, HPTOLD, ATA_UDMA4, "HPT366" },
     { ATA_HPT302, 0x01, HPT372, 0x00,   ATA_UDMA6, "HPT302" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64];

    if (!(idx = ata_match_chip(dev, ids)))
	return ENXIO;

    strcpy(buffer, "HighPoint ");
    strcat(buffer, idx->text);
    if (idx->cfg1 == HPT374) {
	if (pci_get_function(dev) == 0)
	    strcat(buffer, " (channel 0+1)");
	if (pci_get_function(dev) == 1)
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

    if (ata_setup_interrupt(dev))
	return ENXIO;

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
    ctlr->allocate = ata_highpoint_allocate;
    ctlr->setmode = ata_highpoint_setmode;
    return 0;
}

static int
ata_highpoint_allocate(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->flags |= ATA_ALWAYS_DMASTAT;
    return 0;
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
    static struct ata_chip_id ids[] =
    {{ ATA_I82371FB,     0,    0, 0x00, ATA_WDMA2, "PIIX" },
     { ATA_I82371SB,     0,    0, 0x00, ATA_WDMA2, "PIIX3" },
     { ATA_I82371AB,     0,    0, 0x00, ATA_UDMA2, "PIIX4" },
     { ATA_I82443MX,     0,    0, 0x00, ATA_UDMA2, "PIIX4" },
     { ATA_I82451NX,     0,    0, 0x00, ATA_UDMA2, "PIIX4" },
     { ATA_I82801AB,     0,    0, 0x00, ATA_UDMA2, "ICH0" },
     { ATA_I82801AA,     0,    0, 0x00, ATA_UDMA4, "ICH" },
     { ATA_I82372FB,     0,    0, 0x00, ATA_UDMA4, "ICH" },
     { ATA_I82801BA,     0,    0, 0x00, ATA_UDMA5, "ICH2" },
     { ATA_I82801BA_1,   0,    0, 0x00, ATA_UDMA5, "ICH2" },
     { ATA_I82801CA,     0,    0, 0x00, ATA_UDMA5, "ICH3" },
     { ATA_I82801CA_1,   0,    0, 0x00, ATA_UDMA5, "ICH3" },
     { ATA_I82801DB,     0,    0, 0x00, ATA_UDMA5, "ICH4" },
     { ATA_I82801DB_1,   0,    0, 0x00, ATA_UDMA5, "ICH4" },
     { ATA_I82801EB,     0,    0, 0x00, ATA_UDMA5, "ICH5" },
     { ATA_I82801EB_S1,  0,    0, 0x00, ATA_SA150, "ICH5" },
     { ATA_I82801EB_R1,  0,    0, 0x00, ATA_SA150, "ICH5" },
     { ATA_I6300ESB,     0,    0, 0x00, ATA_UDMA5, "6300ESB" },
     { ATA_I6300ESB_S1,  0,    0, 0x00, ATA_SA150, "6300ESB" },
     { ATA_I6300ESB_R1,  0,    0, 0x00, ATA_SA150, "6300ESB" },
     { ATA_I82801FB,     0,    0, 0x00, ATA_UDMA5, "ICH6" },
     { ATA_I82801FB_S1,  0, AHCI, 0x00, ATA_SA150, "ICH6" },
     { ATA_I82801FB_R1,  0, AHCI, 0x00, ATA_SA150, "ICH6" },
     { ATA_I82801FBM,    0, AHCI, 0x00, ATA_SA150, "ICH6M" },
     { ATA_I82801GB,     0,    0, 0x00, ATA_UDMA5, "ICH7" },
     { ATA_I82801GB_S1,  0, AHCI, 0x00, ATA_SA300, "ICH7" },
     { ATA_I82801GB_R1,  0, AHCI, 0x00, ATA_SA300, "ICH7" },
     { ATA_I82801GB_AH,  0, AHCI, 0x00, ATA_SA300, "ICH7" },
     { ATA_I82801GBM_S1, 0, AHCI, 0x00, ATA_SA300, "ICH7M" },
     { ATA_I82801GBM_R1, 0, AHCI, 0x00, ATA_SA300, "ICH7M" },
     { ATA_I82801GBM_AH, 0, AHCI, 0x00, ATA_SA300, "ICH7M" },
     { ATA_I63XXESB2,    0,    0, 0x00, ATA_UDMA5, "63XXESB2" },
     { ATA_I63XXESB2_S1, 0, AHCI, 0x00, ATA_SA300, "63XXESB2" },
     { ATA_I63XXESB2_S2, 0, AHCI, 0x00, ATA_SA300, "63XXESB2" },
     { ATA_I63XXESB2_R1, 0, AHCI, 0x00, ATA_SA300, "63XXESB2" },
     { ATA_I63XXESB2_R2, 0, AHCI, 0x00, ATA_SA300, "63XXESB2" },
     { ATA_I82801HB_S1,  0, AHCI, 0x00, ATA_SA300, "ICH8" },
     { ATA_I82801HB_S2,  0, AHCI, 0x00, ATA_SA300, "ICH8" },
     { ATA_I82801HB_R1,  0, AHCI, 0x00, ATA_SA300, "ICH8" },
     { ATA_I82801HB_AH4, 0, AHCI, 0x00, ATA_SA300, "ICH8" },
     { ATA_I82801HB_AH6, 0, AHCI, 0x00, ATA_SA300, "ICH8" },
     { ATA_I82801HBM,    0,    0, 0x00, ATA_UDMA5, "ICH8M" },
     { ATA_I82801HBM_S1, 0,    0, 0x00, ATA_SA150, "ICH8M" },
     { ATA_I82801HBM_S2, 0, AHCI, 0x00, ATA_SA300, "ICH8M" },
     { ATA_I82801HBM_S3, 0, AHCI, 0x00, ATA_SA300, "ICH8M" },
     { ATA_I82801IB_S1,  0, AHCI, 0x00, ATA_SA300, "ICH9" },
     { ATA_I82801IB_S2,  0, AHCI, 0x00, ATA_SA300, "ICH9" },
     { ATA_I82801IB_AH2, 0, AHCI, 0x00, ATA_SA300, "ICH9" },
     { ATA_I82801IB_AH4, 0, AHCI, 0x00, ATA_SA300, "ICH9" },
     { ATA_I82801IB_AH6, 0, AHCI, 0x00, ATA_SA300, "ICH9" },
     { ATA_I82801IB_R1,  0, AHCI, 0x00, ATA_SA300, "ICH9" },
     { ATA_I31244,       0,    0, 0x00, ATA_SA150, "31244" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_intel_chipinit;
    return 0;
}

static int
ata_intel_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

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
	    ctlr->channels = 4;
	    ctlr->allocate = ata_intel_31244_allocate;
	    ctlr->reset = ata_intel_31244_reset;
	}
	ctlr->setmode = ata_sata_setmode;
    }

    /* non SATA intel chips goes here */
    else if (ctlr->chip->max_dma < ATA_SA150) {
	ctlr->allocate = ata_intel_allocate;
	ctlr->setmode = ata_intel_new_setmode;
    }

    /* SATA parts can be either compat or AHCI */
    else {
	/* force all ports active "the legacy way" */
	pci_write_config(dev, 0x92, pci_read_config(dev, 0x92, 2) | 0x0f, 2);

	ctlr->allocate = ata_intel_allocate;
	ctlr->reset = ata_intel_reset;

	/* 
	 * if we have AHCI capability and AHCI or RAID mode enabled
	 * in BIOS we try for AHCI mode
	 */ 
	if ((ctlr->chip->cfg1 == AHCI) &&
	    (pci_read_config(dev, 0x90, 1) & 0xc0) &&
	    (ata_ahci_chipinit(dev) != ENXIO))
	    return 0;
	
	/* if BAR(5) is IO it should point to SATA interface registers */
	ctlr->r_type2 = SYS_RES_IOPORT;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE)))
	    ctlr->setmode = ata_intel_sata_setmode;
	else
	    ctlr->setmode = ata_sata_setmode;

	/* enable PCI interrupt */
	pci_write_config(dev, PCIR_COMMAND,
			 pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);
    }
    return 0;
}

static int
ata_intel_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    /* if r_res2 is valid it points to SATA interface registers */
    if (ctlr->r_res2) {
	ch->r_io[ATA_IDX_ADDR].res = ctlr->r_res2;
	ch->r_io[ATA_IDX_ADDR].offset = 0x00;
	ch->r_io[ATA_IDX_DATA].res = ctlr->r_res2;
	ch->r_io[ATA_IDX_DATA].offset = 0x04;
    }

    ch->flags |= ATA_ALWAYS_DMASTAT;
    return 0;
}

static void
ata_intel_reset(device_t dev)
{
    device_t parent = device_get_parent(dev);
    struct ata_pci_controller *ctlr = device_get_softc(parent);
    struct ata_channel *ch = device_get_softc(dev);
    int mask, timeout;

    /* ICH6 & ICH7 in compat mode has 4 SATA ports as master/slave on 2 ch's */
    if (ctlr->chip->cfg1) {
	mask = (0x0005 << ch->unit);
    }
    else {
	/* ICH5 in compat mode has SATA ports as master/slave on 1 channel */
	if (pci_read_config(parent, 0x90, 1) & 0x04)
	    mask = 0x0003;
	else {
	    mask = (0x0001 << ch->unit);
	    /* XXX SOS should be in intel_allocate if we grow it */
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

static void
ata_intel_sata_setmode(device_t dev, int mode)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (atadev->param.satacapabilities != 0x0000 &&
	atadev->param.satacapabilities != 0xffff) {

	struct ata_channel *ch = device_get_softc(device_get_parent(dev));
	int devno = (ch->unit << 1) + ATA_DEV(atadev->unit);

	/* on some drives we need to set the transfer mode */
	ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0,
		       ata_limit_mode(dev, mode, ATA_UDMA6));

	/* set ATA_SSTATUS register offset */
	ATA_IDX_OUTL(ch, ATA_IDX_ADDR, devno * 0x100);

	/* query SATA STATUS for the speed */
	if ((ATA_IDX_INL(ch, ATA_IDX_DATA) & ATA_SS_CONWELL_MASK) ==
	    ATA_SS_CONWELL_GEN2)
	    atadev->mode = ATA_SA300;
	else
	    atadev->mode = ATA_SA150;
    }
    else {
	mode = ata_limit_mode(dev, mode, ATA_UDMA5);
	if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
	    atadev->mode = mode;
    }
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

    /* setup ATA registers */
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

    /* setup DMA registers */
    ch->r_io[ATA_SSTATUS].offset = ch_offset + 0x100;
    ch->r_io[ATA_SERROR].offset = ch_offset + 0x104;
    ch->r_io[ATA_SCONTROL].offset = ch_offset + 0x108;

    /* setup SATA registers */
    ch->r_io[ATA_BMCMD_PORT].offset = ch_offset + 0x70;
    ch->r_io[ATA_BMSTAT_PORT].offset = ch_offset + 0x72;
    ch->r_io[ATA_BMDTP_PORT].offset = ch_offset + 0x74;

    ch->flags |= ATA_NO_SLAVE;
    ata_pci_hw(dev);
    ch->hw.status = ata_intel_31244_status;
    ch->hw.tf_write = ata_intel_31244_tf_write;

    /* enable PHY state change interrupt */
    ATA_OUTL(ctlr->r_res2, 0x4,
	     ATA_INL(ctlr->r_res2, 0x04) | (0x01 << (ch->unit << 3)));
    return 0;
}

static int
ata_intel_31244_status(device_t dev)
{
    /* do we have any PHY events ? */
    ata_sata_phy_check_events(dev);

    /* any drive action to take care of ? */
    return ata_pci_status(dev);
}

static void
ata_intel_31244_tf_write(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	ATA_IDX_OUTW(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTW(ch, ATA_COUNT, request->u.ata.count);
	ATA_IDX_OUTW(ch, ATA_SECTOR, ((request->u.ata.lba >> 16) & 0xff00) |
				      (request->u.ata.lba & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_LSB, ((request->u.ata.lba >> 24) & 0xff00) |
				       ((request->u.ata.lba >> 8) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_MSB, ((request->u.ata.lba >> 32) & 0xff00) | 
				       ((request->u.ata.lba >> 16) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_DRIVE, ATA_D_LBA | atadev->unit);
    }
    else {
	ATA_IDX_OUTB(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTB(ch, ATA_COUNT, request->u.ata.count);
	if (atadev->flags & ATA_D_USE_CHS) {
	    int heads, sectors;
    
	    if (atadev->param.atavalid & ATA_FLAG_54_58) {
		heads = atadev->param.current_heads;
		sectors = atadev->param.current_sectors;
	    }
	    else {
		heads = atadev->param.heads;
		sectors = atadev->param.sectors;
	    }
	    ATA_IDX_OUTB(ch, ATA_SECTOR, (request->u.ata.lba % sectors)+1);
	    ATA_IDX_OUTB(ch, ATA_CYL_LSB,
			 (request->u.ata.lba / (sectors * heads)));
	    ATA_IDX_OUTB(ch, ATA_CYL_MSB,
			 (request->u.ata.lba / (sectors * heads)) >> 8);
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | atadev->unit | 
			 (((request->u.ata.lba% (sectors * heads)) /
			   sectors) & 0xf));
	}
	else {
	    ATA_IDX_OUTB(ch, ATA_SECTOR, request->u.ata.lba);
	    ATA_IDX_OUTB(ch, ATA_CYL_LSB, request->u.ata.lba >> 8);
	    ATA_IDX_OUTB(ch, ATA_CYL_MSB, request->u.ata.lba >> 16);
	    ATA_IDX_OUTB(ch, ATA_DRIVE,
			 ATA_D_IBM | ATA_D_LBA | atadev->unit |
			 ((request->u.ata.lba >> 24) & 0x0f));
	}
    }
}

static void
ata_intel_31244_reset(device_t dev)
{
    if (ata_sata_phy_reset(dev))
	ata_generic_reset(dev);
}


/*
 * Integrated Technology Express Inc. (ITE) chipset support functions
 */
int
ata_ite_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_IT8212F, 0x00, 0x00, 0x00, ATA_UDMA6, "IT8212F" },
     { ATA_IT8211F, 0x00, 0x00, 0x00, ATA_UDMA6, "IT8211F" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
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
 * JMicron chipset support functions
 */
int
ata_jmicron_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    struct ata_chip_id *idx;
    static struct ata_chip_id ids[] =
    {{ ATA_JMB360, 0, 1, 0, ATA_SA300, "JMB360" },
     { ATA_JMB361, 0, 1, 1, ATA_SA300, "JMB361" },
     { ATA_JMB363, 0, 2, 1, ATA_SA300, "JMB363" },
     { ATA_JMB365, 0, 1, 2, ATA_SA300, "JMB365" },
     { ATA_JMB366, 0, 2, 2, ATA_SA300, "JMB366" },
     { ATA_JMB368, 0, 0, 1, ATA_UDMA6, "JMB368" },
     { 0, 0, 0, 0, 0, 0}};
    char buffer[64];

    if (!(idx = ata_match_chip(dev, ids)))
        return ENXIO;

    if ((pci_read_config(dev, 0xdf, 1) & 0x40) &&
	(pci_get_function(dev) == (pci_read_config(dev, 0x40, 1) & 0x02 >> 1)))
	sprintf(buffer, "JMicron %s %s controller",
		idx->text, ata_mode2str(ATA_UDMA6));
    else
	sprintf(buffer, "JMicron %s %s controller",
		idx->text, ata_mode2str(idx->max_dma));
    device_set_desc_copy(dev, buffer);
    ctlr->chip = idx;
    ctlr->chipinit = ata_jmicron_chipinit;
    return 0;
}

static int
ata_jmicron_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    int error;

    if (ata_setup_interrupt(dev))
	return ENXIO;

    /* do we have multiple PCI functions ? */
    if (pci_read_config(dev, 0xdf, 1) & 0x40) {
	/* are we on the AHCI part ? */
	if (ata_ahci_chipinit(dev) != ENXIO)
	    return 0;

	/* otherwise we are on the PATA part */
	ctlr->allocate = ata_pci_allocate;
	ctlr->reset = ata_generic_reset;
	ctlr->dmainit = ata_pci_dmainit;
	ctlr->setmode = ata_jmicron_setmode;
	ctlr->channels = ctlr->chip->cfg2;
    }
    else {
	/* set controller configuration to a combined setup we support */
	pci_write_config(dev, 0x40, 0x80c0a131, 4);
	pci_write_config(dev, 0x80, 0x01200000, 4);

	if (ctlr->chip->cfg1 && (error = ata_ahci_chipinit(dev)))
	    return error;

	ctlr->allocate = ata_jmicron_allocate;
	ctlr->reset = ata_jmicron_reset;
	ctlr->dmainit = ata_jmicron_dmainit;
	ctlr->setmode = ata_jmicron_setmode;

	/* set the number of HW channels */ 
	ctlr->channels = ctlr->chip->cfg1 + ctlr->chip->cfg2;
    }
    return 0;
}

static int
ata_jmicron_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int error;

    if (ch->unit >= ctlr->chip->cfg1) {
	ch->unit -= ctlr->chip->cfg1;
	error = ata_pci_allocate(dev);
	ch->unit += ctlr->chip->cfg1;
    }
    else
	error = ata_ahci_allocate(dev);
    return error;
}

static void
ata_jmicron_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    if (ch->unit >= ctlr->chip->cfg1)
	ata_generic_reset(dev);
    else
	ata_ahci_reset(dev);
}

static void
ata_jmicron_dmainit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    if (ch->unit >= ctlr->chip->cfg1)
	ata_pci_dmainit(dev);
    else
	ata_ahci_dmainit(dev);
}

static void
ata_jmicron_setmode(device_t dev, int mode)
{
    struct ata_pci_controller *ctlr = device_get_softc(GRANDPARENT(dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));

    if (pci_read_config(dev, 0xdf, 1) & 0x40 || ch->unit >= ctlr->chip->cfg1) {
	struct ata_device *atadev = device_get_softc(dev);

	/* check for 80pin cable present */
	if (pci_read_config(dev, 0x40, 1) & 0x08)
	    mode = ata_limit_mode(dev, mode, ATA_UDMA2);
	else
	    mode = ata_limit_mode(dev, mode, ATA_UDMA6);

	if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
	    atadev->mode = mode;
    }
    else
	ata_sata_setmode(dev, mode);
}


/*
 * Marvell chipset support functions
 */
#define ATA_MV_HOST_BASE(ch) \
	((ch->unit & 3) * 0x0100) + (ch->unit > 3 ? 0x30000 : 0x20000)
#define ATA_MV_EDMA_BASE(ch) \
	((ch->unit & 3) * 0x2000) + (ch->unit > 3 ? 0x30000 : 0x20000)

struct ata_marvell_response {
    u_int16_t   tag;
    u_int8_t    edma_status;
    u_int8_t    dev_status;
    u_int32_t   timestamp;
};

struct ata_marvell_dma_prdentry {
    u_int32_t addrlo;
    u_int32_t count;
    u_int32_t addrhi;
    u_int32_t reserved;
};  

int
ata_marvell_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_M88SX5040, 0, 4, MV50XX, ATA_SA150, "88SX5040" },
     { ATA_M88SX5041, 0, 4, MV50XX, ATA_SA150, "88SX5041" },
     { ATA_M88SX5080, 0, 8, MV50XX, ATA_SA150, "88SX5080" },
     { ATA_M88SX5081, 0, 8, MV50XX, ATA_SA150, "88SX5081" },
     { ATA_M88SX6041, 0, 4, MV60XX, ATA_SA300, "88SX6041" },
     { ATA_M88SX6081, 0, 8, MV60XX, ATA_SA300, "88SX6081" },
     { ATA_M88SX6101, 0, 1, MV61XX, ATA_UDMA6, "88SX6101" },
     { ATA_M88SX6145, 0, 2, MV61XX, ATA_UDMA6, "88SX6145" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);

    switch (ctlr->chip->cfg2) {
    case MV50XX:
    case MV60XX:
	ctlr->chipinit = ata_marvell_edma_chipinit;
	break;
    case MV61XX:
	ctlr->chipinit = ata_marvell_pata_chipinit;
	break;
    }
    return 0;
}

static int
ata_marvell_pata_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    ctlr->allocate = ata_marvell_pata_allocate;
    ctlr->setmode = ata_marvell_pata_setmode;
    ctlr->channels = ctlr->chip->cfg1;
    return 0;
}

static int
ata_marvell_pata_allocate(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
 
    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;
 
    /* dont use 32 bit PIO transfers */
	ch->flags |= ATA_USE_16BIT;

    return 0;
}

static void
ata_marvell_pata_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_device *atadev = device_get_softc(dev);

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);
    mode = ata_check_80pin(dev, mode);
    if (!ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode))
	atadev->mode = mode;
}

static int
ata_marvell_edma_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    ctlr->r_type1 = SYS_RES_MEMORY;
    ctlr->r_rid1 = PCIR_BAR(0);
    if (!(ctlr->r_res1 = bus_alloc_resource_any(dev, ctlr->r_type1,
						&ctlr->r_rid1, RF_ACTIVE)))
	return ENXIO;

    /* mask all host controller interrupts */
    ATA_OUTL(ctlr->r_res1, 0x01d64, 0x00000000);

    /* mask all PCI interrupts */
    ATA_OUTL(ctlr->r_res1, 0x01d5c, 0x00000000);

    ctlr->allocate = ata_marvell_edma_allocate;
    ctlr->reset = ata_marvell_edma_reset;
    ctlr->dmainit = ata_marvell_edma_dmainit;
    ctlr->setmode = ata_sata_setmode;
    ctlr->channels = ctlr->chip->cfg1;

    /* clear host controller interrupts */
    ATA_OUTL(ctlr->r_res1, 0x20014, 0x00000000);
    if (ctlr->chip->cfg1 > 4)
	ATA_OUTL(ctlr->r_res1, 0x30014, 0x00000000);

    /* clear PCI interrupts */
    ATA_OUTL(ctlr->r_res1, 0x01d58, 0x00000000);

    /* unmask PCI interrupts we want */
    ATA_OUTL(ctlr->r_res1, 0x01d5c, 0x007fffff);

    /* unmask host controller interrupts we want */
    ATA_OUTL(ctlr->r_res1, 0x01d64, 0x000000ff/*HC0*/ | 0x0001fe00/*HC1*/ |
	     /*(1<<19) | (1<<20) | (1<<21) |*/(1<<22) | (1<<24) | (0x7f << 25));

    /* enable PCI interrupt */
    pci_write_config(dev, PCIR_COMMAND,
		     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);
    return 0;
}

static int
ata_marvell_edma_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int64_t work = ch->dma->work_bus;
    int i;

    /* clear work area */
    bzero(ch->dma->work, 1024+256);
    bus_dmamap_sync(ch->dma->work_tag, ch->dma->work_map,
	BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

    /* set legacy ATA resources */
    for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
	ch->r_io[i].res = ctlr->r_res1;
	ch->r_io[i].offset = 0x02100 + (i << 2) + ATA_MV_EDMA_BASE(ch);
    }
    ch->r_io[ATA_CONTROL].res = ctlr->r_res1;
    ch->r_io[ATA_CONTROL].offset = 0x02120 + ATA_MV_EDMA_BASE(ch);
    ch->r_io[ATA_IDX_ADDR].res = ctlr->r_res1;
    ata_default_registers(dev);

    /* set SATA resources */
    switch (ctlr->chip->cfg2) {
    case MV50XX:
	ch->r_io[ATA_SSTATUS].res = ctlr->r_res1;
	ch->r_io[ATA_SSTATUS].offset =  0x00100 + ATA_MV_HOST_BASE(ch);
	ch->r_io[ATA_SERROR].res = ctlr->r_res1;
	ch->r_io[ATA_SERROR].offset = 0x00104 + ATA_MV_HOST_BASE(ch);
	ch->r_io[ATA_SCONTROL].res = ctlr->r_res1;
	ch->r_io[ATA_SCONTROL].offset = 0x00108 + ATA_MV_HOST_BASE(ch);
	break;
    case MV60XX:
	ch->r_io[ATA_SSTATUS].res = ctlr->r_res1;
	ch->r_io[ATA_SSTATUS].offset =  0x02300 + ATA_MV_EDMA_BASE(ch);
	ch->r_io[ATA_SERROR].res = ctlr->r_res1;
	ch->r_io[ATA_SERROR].offset = 0x02304 + ATA_MV_EDMA_BASE(ch);
	ch->r_io[ATA_SCONTROL].res = ctlr->r_res1;
	ch->r_io[ATA_SCONTROL].offset = 0x02308 + ATA_MV_EDMA_BASE(ch);
	ch->r_io[ATA_SACTIVE].res = ctlr->r_res1;
	ch->r_io[ATA_SACTIVE].offset = 0x02350 + ATA_MV_EDMA_BASE(ch);
	break;
    }

    ch->flags |= ATA_NO_SLAVE;
    ch->flags |= ATA_USE_16BIT; /* XXX SOS needed ? */
    ata_generic_hw(dev);
    ch->hw.begin_transaction = ata_marvell_edma_begin_transaction;
    ch->hw.end_transaction = ata_marvell_edma_end_transaction;
    ch->hw.status = ata_marvell_edma_status;

    /* disable the EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000002);
    DELAY(100000);       /* SOS should poll for disabled */

    /* set configuration to non-queued 128b read transfers stop on error */
    ATA_OUTL(ctlr->r_res1, 0x02000 + ATA_MV_EDMA_BASE(ch), (1<<11) | (1<<13));

    /* request queue base high */
    ATA_OUTL(ctlr->r_res1, 0x02010 + ATA_MV_EDMA_BASE(ch), work >> 32);

    /* request queue in ptr */
    ATA_OUTL(ctlr->r_res1, 0x02014 + ATA_MV_EDMA_BASE(ch), work & 0xffffffff);

    /* request queue out ptr */
    ATA_OUTL(ctlr->r_res1, 0x02018 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* response queue base high */
    work += 1024;
    ATA_OUTL(ctlr->r_res1, 0x0201c + ATA_MV_EDMA_BASE(ch), work >> 32);

    /* response queue in ptr */
    ATA_OUTL(ctlr->r_res1, 0x02020 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* response queue out ptr */
    ATA_OUTL(ctlr->r_res1, 0x02024 + ATA_MV_EDMA_BASE(ch), work & 0xffffffff);

    /* clear SATA error register */
    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

    /* clear any outstanding error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x02008 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* unmask all error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x0200c + ATA_MV_EDMA_BASE(ch), ~0x0);
    
    /* enable EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000001);
    return 0;
}

static int
ata_marvell_edma_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t cause = ATA_INL(ctlr->r_res1, 0x01d60);
    int shift = (ch->unit << 1) + (ch->unit > 3);

    if (cause & (1 << shift)) {

	/* clear interrupt(s) */
	ATA_OUTL(ctlr->r_res1, 0x02008 + ATA_MV_EDMA_BASE(ch), 0x0);

	/* do we have any PHY events ? */
	ata_sata_phy_check_events(dev);
    }

    /* do we have any device action ? */
    return (cause & (2 << shift));
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_marvell_edma_begin_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    u_int32_t req_in;
    u_int8_t *bytep;
    int i, tag = 0x07;
    int dummy, error, slot;

    /* only DMA R/W goes through the EMDA machine */
    if (request->u.ata.command != ATA_READ_DMA &&
	request->u.ata.command != ATA_WRITE_DMA) {

	/* disable the EDMA machinery */
	if (ATA_INL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001)
	    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000002);
	return ata_begin_transaction(request);
    }

    /* check for 48 bit access and convert if needed */
    ata_modify_if_48bit(request);

    /* check sanity, setup SG list and DMA engine */
    if ((error = ch->dma->load(ch->dev, request->data, request->bytecount,
			       request->flags & ATA_R_READ, ch->dma->sg,
			       &dummy))) {
	device_printf(request->dev, "setting up DMA failed\n");
	request->result = error;
	return ATA_OP_FINISHED;
    }

    /* get next free request queue slot */
    req_in = ATA_INL(ctlr->r_res1, 0x02014 + ATA_MV_EDMA_BASE(ch));
    slot = (((req_in & ~0xfffffc00) >> 5) + 0) & 0x1f;
    bytep = (u_int8_t *)(ch->dma->work);
    bytep += (slot << 5);

    /* fill in this request */
    le32enc(bytep + 0 * sizeof(u_int32_t),
	ch->dma->sg_bus & 0xffffffff);
    le32enc(bytep + 1 * sizeof(u_int32_t),
	(u_int64_t)ch->dma->sg_bus >> 32);
    le16enc(bytep + 4 * sizeof(u_int16_t),
	(request->flags & ATA_R_READ ? 0x01 : 0x00) | (tag << 1));

    i = 10;
    bytep[i++] = (request->u.ata.count >> 8) & 0xff;
    bytep[i++] = 0x10 | ATA_COUNT;
    bytep[i++] = request->u.ata.count & 0xff;
    bytep[i++] = 0x10 | ATA_COUNT;

    bytep[i++] = (request->u.ata.lba >> 24) & 0xff;
    bytep[i++] = 0x10 | ATA_SECTOR;
    bytep[i++] = request->u.ata.lba & 0xff;
    bytep[i++] = 0x10 | ATA_SECTOR;

    bytep[i++] = (request->u.ata.lba >> 32) & 0xff;
    bytep[i++] = 0x10 | ATA_CYL_LSB;
    bytep[i++] = (request->u.ata.lba >> 8) & 0xff;
    bytep[i++] = 0x10 | ATA_CYL_LSB;

    bytep[i++] = (request->u.ata.lba >> 40) & 0xff;
    bytep[i++] = 0x10 | ATA_CYL_MSB;
    bytep[i++] = (request->u.ata.lba >> 16) & 0xff;
    bytep[i++] = 0x10 | ATA_CYL_MSB;

    bytep[i++] = ATA_D_LBA | ATA_D_IBM | ((request->u.ata.lba >> 24) & 0xf);
    bytep[i++] = 0x10 | ATA_DRIVE;

    bytep[i++] = request->u.ata.command;
    bytep[i++] = 0x90 | ATA_COMMAND;

    bus_dmamap_sync(ch->dma->work_tag, ch->dma->work_map,
	BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

    /* enable EDMA machinery if needed */
    if (!(ATA_INL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001)) {
	ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000001);
	while (!(ATA_INL(ctlr->r_res1,
			 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001))
	    DELAY(10);
    }

    /* tell EDMA it has a new request */
    slot = (((req_in & ~0xfffffc00) >> 5) + 1) & 0x1f;
    req_in &= 0xfffffc00;
    req_in += (slot << 5);
    ATA_OUTL(ctlr->r_res1, 0x02014 + ATA_MV_EDMA_BASE(ch), req_in);
   
    return ATA_OP_CONTINUES;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_marvell_edma_end_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    int offset = (ch->unit > 3 ? 0x30014 : 0x20014);
    u_int32_t icr = ATA_INL(ctlr->r_res1, offset);
    int res;

    /* EDMA interrupt */
    if ((icr & (0x0001 << (ch->unit & 3)))) {
	struct ata_marvell_response *response;
	u_int32_t rsp_in, rsp_out;
	int slot;

	/* stop timeout */
	callout_stop(&request->callout);

	/* get response ptr's */
	rsp_in = ATA_INL(ctlr->r_res1, 0x02020 + ATA_MV_EDMA_BASE(ch));
	rsp_out = ATA_INL(ctlr->r_res1, 0x02024 + ATA_MV_EDMA_BASE(ch));
	slot = (((rsp_in & ~0xffffff00) >> 3)) & 0x1f;
	rsp_out &= 0xffffff00;
	rsp_out += (slot << 3);
	bus_dmamap_sync(ch->dma->work_tag, ch->dma->work_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	response = (struct ata_marvell_response *)
		   (ch->dma->work + 1024 + (slot << 3));

	/* record status for this request */
	request->status = response->dev_status;
	request->error = 0; 

	/* ack response */
	ATA_OUTL(ctlr->r_res1, 0x02024 + ATA_MV_EDMA_BASE(ch), rsp_out);

	/* update progress */
	if (!(request->status & ATA_S_ERROR) &&
	    !(request->flags & ATA_R_TIMEOUT))
	    request->donecount = request->bytecount;

	/* unload SG list */
	ch->dma->unload(ch->dev);

	res = ATA_OP_FINISHED;
    }

    /* legacy ATA interrupt */
    else {
	res = ata_end_transaction(request);
    }

    /* ack interrupt */
    ATA_OUTL(ctlr->r_res1, offset, ~(icr & (0x0101 << (ch->unit & 3))));
    return res;
}

static void
ata_marvell_edma_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* disable the EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000002);
    while ((ATA_INL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch)) & 0x00000001))
	DELAY(10);

    /* clear SATA error register */
    ATA_IDX_OUTL(ch, ATA_SERROR, ATA_IDX_INL(ch, ATA_SERROR));

    /* clear any outstanding error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x02008 + ATA_MV_EDMA_BASE(ch), 0x0);

    /* unmask all error interrupts */
    ATA_OUTL(ctlr->r_res1, 0x0200c + ATA_MV_EDMA_BASE(ch), ~0x0);

    /* enable channel and test for devices */
    if (ata_sata_phy_reset(dev))
	ata_generic_reset(dev);

    /* enable EDMA machinery */
    ATA_OUTL(ctlr->r_res1, 0x02028 + ATA_MV_EDMA_BASE(ch), 0x00000001);
}

static void
ata_marvell_edma_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs,
			   int error)
{
    struct ata_dmasetprd_args *args = xsc;
    struct ata_marvell_dma_prdentry *prd = args->dmatab;
    int i;

    if ((args->error = error))
	return;

    for (i = 0; i < nsegs; i++) {
	prd[i].addrlo = htole32(segs[i].ds_addr);
	prd[i].count = htole32(segs[i].ds_len);
	prd[i].addrhi = htole32((u_int64_t)segs[i].ds_addr >> 32);
	prd[i].reserved = 0;
    }
    prd[i - 1].count |= htole32(ATA_DMA_EOT);
    KASSERT(nsegs <= ATA_DMA_ENTRIES, ("too many DMA segment entries\n"));
    args->nsegs = nsegs;
}

static void
ata_marvell_edma_dmainit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    if (ch->dma) {
	/* note start and stop are not used here */
	ch->dma->setprd = ata_marvell_edma_dmasetprd;
	
	/* if 64bit support present adjust max address used */
	if (ATA_INL(ctlr->r_res1, 0x00d00) & 0x00000004)
	    ch->dma->max_address = BUS_SPACE_MAXADDR;

	/* chip does not reliably do 64K DMA transfers */
	ch->dma->max_iosize = 64 * DEV_BSIZE; 
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
    
static int
ata_national_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    
    if (ata_setup_interrupt(dev))
	return ENXIO;
		    
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
    ch->dma->max_iosize = 64 * DEV_BSIZE;

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
 * NetCell chipset support functions
 */
int
ata_netcell_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (pci_get_devid(dev) == ATA_NETCELL_SR) {
	device_set_desc(dev, "Netcell SyncRAID SR3000/5000 RAID Controller");
	ctlr->chipinit = ata_netcell_chipinit;
	return 0;
    }
    return ENXIO;
}

static int
ata_netcell_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_generic_chipinit(dev))
	return ENXIO;

    ctlr->allocate = ata_netcell_allocate;
    return 0;
}

static int
ata_netcell_allocate(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
 
    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;
 
    /* the NetCell only supports 16 bit PIO transfers */
    ch->flags |= ATA_USE_16BIT;

    return 0;
}


/*
 * nVidia chipset support functions
 */
int
ata_nvidia_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_NFORCE1,         0, AMDNVIDIA, NVIDIA,  ATA_UDMA5, "nForce" },
     { ATA_NFORCE2,         0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce2" },
     { ATA_NFORCE2_PRO,     0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce2 Pro" },
     { ATA_NFORCE2_PRO_S1,  0, 0,         0,       ATA_SA150, "nForce2 Pro" },
     { ATA_NFORCE3,         0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce3" },
     { ATA_NFORCE3_PRO,     0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce3 Pro" },
     { ATA_NFORCE3_PRO_S1,  0, 0,         0,       ATA_SA150, "nForce3 Pro" },
     { ATA_NFORCE3_PRO_S2,  0, 0,         0,       ATA_SA150, "nForce3 Pro" },
     { ATA_NFORCE_MCP04,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP" },
     { ATA_NFORCE_MCP04_S1, 0, 0,         NV4,     ATA_SA150, "nForce MCP" },
     { ATA_NFORCE_MCP04_S2, 0, 0,         NV4,     ATA_SA150, "nForce MCP" },
     { ATA_NFORCE_CK804,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce CK804" },
     { ATA_NFORCE_CK804_S1, 0, 0,         NV4,     ATA_SA300, "nForce CK804" },
     { ATA_NFORCE_CK804_S2, 0, 0,         NV4,     ATA_SA300, "nForce CK804" },
     { ATA_NFORCE_MCP51,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP51" },
     { ATA_NFORCE_MCP51_S1, 0, 0,         NV4|NVQ, ATA_SA300, "nForce MCP51" },
     { ATA_NFORCE_MCP51_S2, 0, 0,         NV4|NVQ, ATA_SA300, "nForce MCP51" },
     { ATA_NFORCE_MCP55,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP55" },
     { ATA_NFORCE_MCP55_S1, 0, 0,         NV4|NVQ, ATA_SA300, "nForce MCP55" },
     { ATA_NFORCE_MCP55_S2, 0, 0,         NV4|NVQ, ATA_SA300, "nForce MCP55" },
     { ATA_NFORCE_MCP61,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP61" },
     { ATA_NFORCE_MCP61_S1, 0, 0,         NV4|NVQ, ATA_SA300, "nForce MCP61" },
     { ATA_NFORCE_MCP61_S2, 0, 0,         NV4|NVQ, ATA_SA300, "nForce MCP61" },
     { ATA_NFORCE_MCP61_S3, 0, 0,         NV4|NVQ, ATA_SA300, "nForce MCP61" },
     { ATA_NFORCE_MCP65,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP65" },
     { ATA_NFORCE_MCP67,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP67" },
     { ATA_NFORCE_MCP73,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP73" },
     { ATA_NFORCE_MCP77,    0, AMDNVIDIA, NVIDIA,  ATA_UDMA6, "nForce MCP77" },
     { 0, 0, 0, 0, 0, 0}} ;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
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
	    int offset = ctlr->chip->cfg2 & NV4 ? 0x0440 : 0x0010;

	    ctlr->allocate = ata_nvidia_allocate;
	    ctlr->reset = ata_nvidia_reset;

	    /* enable control access */
	    pci_write_config(dev, 0x50, pci_read_config(dev, 0x50, 1) | 0x04,1);

	    if (ctlr->chip->cfg2 & NVQ) {
		/* clear interrupt status */
		ATA_OUTL(ctlr->r_res2, offset, 0x00ff00ff);

		/* enable device and PHY state change interrupts */
		ATA_OUTL(ctlr->r_res2, offset + 4, 0x000d000d);

		/* disable NCQ support */
		ATA_OUTL(ctlr->r_res2, 0x0400,
			 ATA_INL(ctlr->r_res2, 0x0400) & 0xfffffff9);
	    } 
	    else {
		/* clear interrupt status */
		ATA_OUTB(ctlr->r_res2, offset, 0xff);

		/* enable device and PHY state change interrupts */
		ATA_OUTB(ctlr->r_res2, offset + 1, 0xdd);
	    }

	    /* enable PCI interrupt */
	    pci_write_config(dev, PCIR_COMMAND,
			     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400,2);

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
    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = (ch->unit << 6);
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x04 + (ch->unit << 6);
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x08 + (ch->unit << 6);

    ch->hw.status = ata_nvidia_status;
    ch->flags |= ATA_NO_SLAVE;

    return 0;
}

static int 
ata_nvidia_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ctlr->chip->cfg2 & NV4 ? 0x0440 : 0x0010;
    int shift = ch->unit << (ctlr->chip->cfg2 & NVQ ? 4 : 2);
    u_int32_t istatus = ATA_INL(ctlr->r_res2, offset);

    /* do we have any PHY events ? */
    if (istatus & (0x0c << shift))
	ata_sata_phy_check_events(dev);

    /* clear interrupt(s) */
    ATA_OUTB(ctlr->r_res2, offset,
	     (0x0f << shift) | (ctlr->chip->cfg2 & NVQ ? 0x00f000f0 : 0));

    /* do we have any device action ? */
    return (istatus & (0x01 << shift));
}

static void
ata_nvidia_reset(device_t dev)
{
    if (ata_sata_phy_reset(dev))
	ata_generic_reset(dev);
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
    {{ ATA_PDC20246,  0, PROLD, 0x00,    ATA_UDMA2, "PDC20246" },
     { ATA_PDC20262,  0, PRNEW, 0x00,    ATA_UDMA4, "PDC20262" },
     { ATA_PDC20263,  0, PRNEW, 0x00,    ATA_UDMA4, "PDC20263" },
     { ATA_PDC20265,  0, PRNEW, 0x00,    ATA_UDMA5, "PDC20265" },
     { ATA_PDC20267,  0, PRNEW, 0x00,    ATA_UDMA5, "PDC20267" },
     { ATA_PDC20268,  0, PRTX,  PRTX4,   ATA_UDMA5, "PDC20268" },
     { ATA_PDC20269,  0, PRTX,  0x00,    ATA_UDMA6, "PDC20269" },
     { ATA_PDC20270,  0, PRTX,  PRTX4,   ATA_UDMA5, "PDC20270" },
     { ATA_PDC20271,  0, PRTX,  0x00,    ATA_UDMA6, "PDC20271" },
     { ATA_PDC20275,  0, PRTX,  0x00,    ATA_UDMA6, "PDC20275" },
     { ATA_PDC20276,  0, PRTX,  PRSX6K,  ATA_UDMA6, "PDC20276" },
     { ATA_PDC20277,  0, PRTX,  0x00,    ATA_UDMA6, "PDC20277" },
     { ATA_PDC20318,  0, PRMIO, PRSATA,  ATA_SA150, "PDC20318" },
     { ATA_PDC20319,  0, PRMIO, PRSATA,  ATA_SA150, "PDC20319" },
     { ATA_PDC20371,  0, PRMIO, PRCMBO,  ATA_SA150, "PDC20371" },
     { ATA_PDC20375,  0, PRMIO, PRCMBO,  ATA_SA150, "PDC20375" },
     { ATA_PDC20376,  0, PRMIO, PRCMBO,  ATA_SA150, "PDC20376" },
     { ATA_PDC20377,  0, PRMIO, PRCMBO,  ATA_SA150, "PDC20377" },
     { ATA_PDC20378,  0, PRMIO, PRCMBO,  ATA_SA150, "PDC20378" },
     { ATA_PDC20379,  0, PRMIO, PRCMBO,  ATA_SA150, "PDC20379" },
     { ATA_PDC20571,  0, PRMIO, PRCMBO2, ATA_SA150, "PDC20571" },
     { ATA_PDC20575,  0, PRMIO, PRCMBO2, ATA_SA150, "PDC20575" },
     { ATA_PDC20579,  0, PRMIO, PRCMBO2, ATA_SA150, "PDC20579" },
     { ATA_PDC20771,  0, PRMIO, PRCMBO2, ATA_SA300, "PDC20771" },
     { ATA_PDC40775,  0, PRMIO, PRCMBO2, ATA_SA300, "PDC40775" },
     { ATA_PDC20617,  0, PRMIO, PRPATA,  ATA_UDMA6, "PDC20617" },
     { ATA_PDC20618,  0, PRMIO, PRPATA,  ATA_UDMA6, "PDC20618" },
     { ATA_PDC20619,  0, PRMIO, PRPATA,  ATA_UDMA6, "PDC20619" },
     { ATA_PDC20620,  0, PRMIO, PRPATA,  ATA_UDMA6, "PDC20620" },
     { ATA_PDC20621,  0, PRMIO, PRSX4X,  ATA_UDMA5, "PDC20621" },
     { ATA_PDC20622,  0, PRMIO, PRSX4X,  ATA_SA150, "PDC20622" },
     { ATA_PDC40518,  0, PRMIO, PRSATA2, ATA_SA150, "PDC40518" },
     { ATA_PDC40519,  0, PRMIO, PRSATA2, ATA_SA150, "PDC40519" },
     { ATA_PDC40718,  0, PRMIO, PRSATA2, ATA_SA300, "PDC40718" },
     { ATA_PDC40719,  0, PRMIO, PRSATA2, ATA_SA300, "PDC40719" },
     { ATA_PDC40779,  0, PRMIO, PRSATA2, ATA_SA300, "PDC40779" },
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

    strcpy(buffer, "Promise ");
    strcat(buffer, idx->text);

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
    int fake_reg, stat_reg;

    if (ata_setup_interrupt(dev))
	return ENXIO;

    switch  (ctlr->chip->cfg1) {
    case PRNEW:
	/* setup clocks */
	ATA_OUTB(ctlr->r_res1, 0x11, ATA_INB(ctlr->r_res1, 0x11) | 0x0a);

	ctlr->dmainit = ata_promise_dmainit;
	/* FALLTHROUGH */

    case PROLD:
	/* enable burst mode */
	ATA_OUTB(ctlr->r_res1, 0x1f, ATA_INB(ctlr->r_res1, 0x1f) | 0x01);
	ctlr->allocate = ata_promise_allocate;
	ctlr->setmode = ata_promise_setmode;
	return 0;

    case PRTX:
	ctlr->allocate = ata_promise_tx2_allocate;
	ctlr->setmode = ata_promise_setmode;
	return 0;

    case PRMIO:
	ctlr->r_type1 = SYS_RES_MEMORY;
	ctlr->r_rid1 = PCIR_BAR(4);
	if (!(ctlr->r_res1 = bus_alloc_resource_any(dev, ctlr->r_type1,
						    &ctlr->r_rid1, RF_ACTIVE)))
	    goto failnfree;

	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(3);
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE)))
	    goto failnfree;

	if (ctlr->chip->cfg2 == PRSX4X) {
	    struct ata_promise_sx4 *hpkt;
	    u_int32_t dimm = ATA_INL(ctlr->r_res2, 0x000c0080);

	    if (bus_teardown_intr(dev, ctlr->r_irq, ctlr->handle) ||
		bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			       ata_promise_sx4_intr, ctlr, &ctlr->handle)) {
		device_printf(dev, "unable to setup interrupt\n");
		goto failnfree;
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
	    hpkt->busy = 0;
	    device_set_ivars(dev, hpkt);
	    ctlr->allocate = ata_promise_mio_allocate;
	    ctlr->reset = ata_promise_mio_reset;
	    ctlr->dmainit = ata_promise_mio_dmainit;
	    ctlr->setmode = ata_promise_setmode;
	    ctlr->channels = 4;
	    return 0;
	}

	/* mio type controllers need an interrupt intercept */
	if (bus_teardown_intr(dev, ctlr->r_irq, ctlr->handle) ||
		bus_setup_intr(dev, ctlr->r_irq, ATA_INTR_FLAGS,
			       ata_promise_mio_intr, ctlr, &ctlr->handle)) {
		device_printf(dev, "unable to setup interrupt\n");
		goto failnfree;
	}

	switch (ctlr->chip->cfg2) {
	case PRPATA:
	    ctlr->channels = ((ATA_INL(ctlr->r_res2, 0x48) & 0x01) > 0) +
			     ((ATA_INL(ctlr->r_res2, 0x48) & 0x02) > 0) + 2;
	    goto sata150;
	case PRCMBO:
	    ctlr->channels = 3;
	    goto sata150;
	case PRSATA:
	    ctlr->channels = 4;
sata150:
	    fake_reg = 0x60;
	    stat_reg = 0x6c;
	    break;

	case PRCMBO2: 
	    ctlr->channels = 3;
	    goto sataii;
	case PRSATA2:
	default:
	    ctlr->channels = 4;
sataii:
	    fake_reg = 0x54;
	    stat_reg = 0x60;
	    break;
	}

	/* prime fake interrupt register */
	ATA_OUTL(ctlr->r_res2, fake_reg, 0xffffffff);

	/* clear SATA status and unmask interrupts */
	ATA_OUTL(ctlr->r_res2, stat_reg, 0x000000ff);

	/* enable "long burst lenght" on gen2 chips */
	if ((ctlr->chip->cfg2 == PRSATA2) || (ctlr->chip->cfg2 == PRCMBO2))
	    ATA_OUTL(ctlr->r_res2, 0x44, ATA_INL(ctlr->r_res2, 0x44) | 0x2000);

	ctlr->allocate = ata_promise_mio_allocate;
	ctlr->reset = ata_promise_mio_reset;
	ctlr->dmainit = ata_promise_mio_dmainit;
	ctlr->setmode = ata_promise_mio_setmode;

	return 0;
    }

failnfree:
    if (ctlr->r_res2)
	bus_release_resource(dev, ctlr->r_type2, ctlr->r_rid2, ctlr->r_res2);
    if (ctlr->r_res1)
	bus_release_resource(dev, ctlr->r_type1, ctlr->r_rid1, ctlr->r_res1);
    return ENXIO;
}

static int
ata_promise_allocate(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->hw.status = ata_promise_status;
    return 0;
}

static int
ata_promise_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    if (ATA_INL(ctlr->r_res1, 0x1c) & (ch->unit ? 0x00004000 : 0x00000400)) {
	return ata_pci_status(dev);
    }
    return 0;
}

static int
ata_promise_dmastart(device_t dev)
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
ata_promise_dmastop(device_t dev)
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
ata_promise_dmareset(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ATA_IDX_OUTB(ch, ATA_BMCMD_PORT,
		 ATA_IDX_INB(ch, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    ATA_IDX_OUTB(ch, ATA_BMSTAT_PORT, ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR); 
    ch->flags &= ~ATA_DMA_ACTIVE;
}

static void
ata_promise_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    if (ch->dma) {
	ch->dma->start = ata_promise_dmastart;
	ch->dma->stop = ata_promise_dmastop;
	ch->dma->reset = ata_promise_dmareset;
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
    u_int32_t timings[][2] = {
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
			     timings[ata_mode2idx(mode)][ctlr->chip->cfg1], 4);
	atadev->mode = mode;
    }
    return;
}

static int
ata_promise_tx2_allocate(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->hw.status = ata_promise_tx2_status;
    return 0;
}

static int
ata_promise_tx2_status(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ATA_IDX_OUTB(ch, ATA_BMDEVSPEC_0, 0x0b);
    if (ATA_IDX_INB(ch, ATA_BMDEVSPEC_1) & 0x20) {
	return ata_pci_status(dev);
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
    if (ctlr->chip->cfg2 & PRSX4X) {
	ch->hw.command = ata_promise_sx4_command;
    }
    else {
	ch->hw.command = ata_promise_mio_command;
	ch->hw.status = ata_promise_mio_status;
     }
    return 0;
}

static void
ata_promise_mio_intr(void *data)
{
    struct ata_pci_controller *ctlr = data;
    struct ata_channel *ch;
    u_int32_t vector;
    int unit, fake_reg;

    switch (ctlr->chip->cfg2) {
    case PRPATA:
    case PRCMBO:
    case PRSATA:
	fake_reg = 0x60;
	break;
    case PRCMBO2: 
    case PRSATA2:
    default:
	fake_reg = 0x54;
	break;
    }

    /*
     * since reading interrupt status register on early "mio" chips
     * clears the status bits we cannot read it for each channel later on
     * in the generic interrupt routine.
     * store the bits in an unused register in the chip so we can read
     * it from there safely to get around this "feature".
     */
    vector = ATA_INL(ctlr->r_res2, 0x040);
    ATA_OUTL(ctlr->r_res2, 0x040, vector);
    ATA_OUTL(ctlr->r_res2, fake_reg, vector);

    for (unit = 0; unit < ctlr->channels; unit++) {
	if ((ch = ctlr->interrupt[unit].argument))
	    ctlr->interrupt[unit].function(ch);
    }

    ATA_OUTL(ctlr->r_res2, fake_reg, 0xffffffff);
}

static int
ata_promise_mio_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_connect_task *tp;
    u_int32_t fake_reg, stat_reg, vector, status;

    switch (ctlr->chip->cfg2) {
    case PRPATA:
    case PRCMBO:
    case PRSATA:
	fake_reg = 0x60;
	stat_reg = 0x6c;
	break;
    case PRCMBO2: 
    case PRSATA2:
    default:
	fake_reg = 0x54;
	stat_reg = 0x60;
	break;
    }

    /* read and acknowledge interrupt */
    vector = ATA_INL(ctlr->r_res2, fake_reg);

    /* read and clear interface status */
    status = ATA_INL(ctlr->r_res2, stat_reg);
    ATA_OUTL(ctlr->r_res2, stat_reg, status & (0x00000011 << ch->unit));

    /* check for and handle disconnect events */
    if ((status & (0x00000001 << ch->unit)) &&
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
    if ((status & (0x00000010 << ch->unit)) &&
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

    /* do we have any device action ? */
    return (vector & (1 << (ch->unit + 1)));
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

    case PRPATA:
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

	    if (ata_sata_phy_reset(dev))
		ata_generic_reset(dev);

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

	    if (ata_sata_phy_reset(dev))
		ata_generic_reset(dev);

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

static void
ata_promise_mio_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* note start and stop are not used here */
    ata_dmainit(dev);
    if (ch->dma) 
	ch->dma->setprd = ata_promise_mio_setprd;
}


#define MAXLASTSGSIZE (32 * sizeof(u_int32_t))
static void 
ata_promise_mio_setprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dmasetprd_args *args = xsc;
    struct ata_dma_prdentry *prd = args->dmatab;
    int i;

    if ((args->error = error))
	return;

    for (i = 0; i < nsegs; i++) {
	prd[i].addr = htole32(segs[i].ds_addr);
	prd[i].count = htole32(segs[i].ds_len);
    }
    if (segs[i - 1].ds_len > MAXLASTSGSIZE) {
	//printf("split last SG element of %u\n", segs[i - 1].ds_len);
	prd[i - 1].count = htole32(segs[i - 1].ds_len - MAXLASTSGSIZE);
	prd[i].count = htole32(MAXLASTSGSIZE);
	prd[i].addr = htole32(segs[i - 1].ds_addr +
			      (segs[i - 1].ds_len - MAXLASTSGSIZE));
	nsegs++;
	i++;
    }
    prd[i - 1].count |= htole32(ATA_DMA_EOT);
    KASSERT(nsegs <= ATA_DMA_ENTRIES, ("too many DMA segment entries\n"));
    args->nsegs = nsegs;
}

static void
ata_promise_mio_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));

    if ( (ctlr->chip->cfg2 == PRSATA) ||
	((ctlr->chip->cfg2 == PRCMBO) && (ch->unit < 2)) ||
	(ctlr->chip->cfg2 == PRSATA2) ||
	((ctlr->chip->cfg2 == PRCMBO2) && (ch->unit < 2)))
	ata_sata_setmode(dev, mode);
    else
	ata_promise_setmode(dev, mode);
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


/*
 * ServerWorks chipset support functions
 */
int
ata_serverworks_ident(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_ROSB4,     0x00, SWKS33,  0, ATA_UDMA2, "ROSB4" },
     { ATA_CSB5,      0x92, SWKS100, 0, ATA_UDMA5, "CSB5" },
     { ATA_CSB5,      0x00, SWKS66,  0, ATA_UDMA4, "CSB5" },
     { ATA_CSB6,      0x00, SWKS100, 0, ATA_UDMA5, "CSB6" },
     { ATA_CSB6_1,    0x00, SWKS66,  0, ATA_UDMA4, "CSB6" },
     { ATA_HT1000,    0x00, SWKS100, 0, ATA_UDMA5, "HT1000" },
     { ATA_HT1000_S1, 0x00, SWKSMIO, 4, ATA_SA150, "HT1000" },
     { ATA_HT1000_S2, 0x00, SWKSMIO, 4, ATA_SA150, "HT1000" },
     { ATA_K2,        0x00, SWKSMIO, 4, ATA_SA150, "K2" },
     { ATA_FRODO4,    0x00, SWKSMIO, 4, ATA_SA150, "Frodo4" },
     { ATA_FRODO8,    0x00, SWKSMIO, 8, ATA_SA150, "Frodo8" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_serverworks_chipinit;
    return 0;
}

static int
ata_serverworks_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    if (ctlr->chip->cfg1 == SWKSMIO) {
	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE)))
	    return ENXIO;

	ctlr->channels = ctlr->chip->cfg2;
	ctlr->allocate = ata_serverworks_allocate;
	ctlr->setmode = ata_sata_setmode;
	return 0;
    }
    else if (ctlr->chip->cfg1 == SWKS33) {
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

static int
ata_serverworks_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int ch_offset;
    int i;

    ch_offset = ch->unit * 0x100;

    for (i = ATA_DATA; i < ATA_MAX_RES; i++)
	ch->r_io[i].res = ctlr->r_res2;

    /* setup ATA registers */
    ch->r_io[ATA_DATA].offset = ch_offset + 0x00;
    ch->r_io[ATA_FEATURE].offset = ch_offset + 0x04;
    ch->r_io[ATA_COUNT].offset = ch_offset + 0x08;
    ch->r_io[ATA_SECTOR].offset = ch_offset + 0x0c;
    ch->r_io[ATA_CYL_LSB].offset = ch_offset + 0x10;
    ch->r_io[ATA_CYL_MSB].offset = ch_offset + 0x14;
    ch->r_io[ATA_DRIVE].offset = ch_offset + 0x18;
    ch->r_io[ATA_COMMAND].offset = ch_offset + 0x1c;
    ch->r_io[ATA_CONTROL].offset = ch_offset + 0x20;
    ata_default_registers(dev);

    /* setup DMA registers */
    ch->r_io[ATA_BMCMD_PORT].offset = ch_offset + 0x30;
    ch->r_io[ATA_BMSTAT_PORT].offset = ch_offset + 0x32;
    ch->r_io[ATA_BMDTP_PORT].offset = ch_offset + 0x34;

    /* setup SATA registers */
    ch->r_io[ATA_SSTATUS].offset = ch_offset + 0x40;
    ch->r_io[ATA_SERROR].offset = ch_offset + 0x44;
    ch->r_io[ATA_SCONTROL].offset = ch_offset + 0x48;

    ch->flags |= ATA_NO_SLAVE;
    ata_pci_hw(dev);
    ch->hw.tf_read = ata_serverworks_tf_read;
    ch->hw.tf_write = ata_serverworks_tf_write;

    /* chip does not reliably do 64K DMA transfers */
    if (ch->dma)
	ch->dma->max_iosize = 64 * DEV_BSIZE;

    return 0;
}

static void
ata_serverworks_tf_read(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	u_int16_t temp;

	request->u.ata.count = ATA_IDX_INW(ch, ATA_COUNT);
	temp = ATA_IDX_INW(ch, ATA_SECTOR);
	request->u.ata.lba = (u_int64_t)(temp & 0x00ff) |
			     ((u_int64_t)(temp & 0xff00) << 24);
	temp = ATA_IDX_INW(ch, ATA_CYL_LSB);
	request->u.ata.lba |= ((u_int64_t)(temp & 0x00ff) << 8) |
			      ((u_int64_t)(temp & 0xff00) << 32);
	temp = ATA_IDX_INW(ch, ATA_CYL_MSB);
	request->u.ata.lba |= ((u_int64_t)(temp & 0x00ff) << 16) |
			      ((u_int64_t)(temp & 0xff00) << 40);
    }
    else {
	request->u.ata.count = ATA_IDX_INW(ch, ATA_COUNT) & 0x00ff;
	request->u.ata.lba = (ATA_IDX_INW(ch, ATA_SECTOR) & 0x00ff) |
			     ((ATA_IDX_INW(ch, ATA_CYL_LSB) & 0x00ff) << 8) |
			     ((ATA_IDX_INW(ch, ATA_CYL_MSB) & 0x00ff) << 16) |
			     ((ATA_IDX_INW(ch, ATA_DRIVE) & 0xf) << 24);
    }
}

static void
ata_serverworks_tf_write(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	ATA_IDX_OUTW(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTW(ch, ATA_COUNT, request->u.ata.count);
	ATA_IDX_OUTW(ch, ATA_SECTOR, ((request->u.ata.lba >> 16) & 0xff00) |
				      (request->u.ata.lba & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_LSB, ((request->u.ata.lba >> 24) & 0xff00) |
				       ((request->u.ata.lba >> 8) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_MSB, ((request->u.ata.lba >> 32) & 0xff00) | 
				       ((request->u.ata.lba >> 16) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_DRIVE, ATA_D_LBA | atadev->unit);
    }
    else {
	ATA_IDX_OUTW(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTW(ch, ATA_COUNT, request->u.ata.count);
	if (atadev->flags & ATA_D_USE_CHS) {
	    int heads, sectors;
    
	    if (atadev->param.atavalid & ATA_FLAG_54_58) {
		heads = atadev->param.current_heads;
		sectors = atadev->param.current_sectors;
	    }
	    else {
		heads = atadev->param.heads;
		sectors = atadev->param.sectors;
	    }
	    ATA_IDX_OUTW(ch, ATA_SECTOR, (request->u.ata.lba % sectors)+1);
	    ATA_IDX_OUTW(ch, ATA_CYL_LSB,
			 (request->u.ata.lba / (sectors * heads)));
	    ATA_IDX_OUTW(ch, ATA_CYL_MSB,
			 (request->u.ata.lba / (sectors * heads)) >> 8);
	    ATA_IDX_OUTW(ch, ATA_DRIVE, ATA_D_IBM | atadev->unit | 
			 (((request->u.ata.lba% (sectors * heads)) /
			   sectors) & 0xf));
	}
	else {
	    ATA_IDX_OUTW(ch, ATA_SECTOR, request->u.ata.lba);
	    ATA_IDX_OUTW(ch, ATA_CYL_LSB, request->u.ata.lba >> 8);
	    ATA_IDX_OUTW(ch, ATA_CYL_MSB, request->u.ata.lba >> 16);
	    ATA_IDX_OUTW(ch, ATA_DRIVE,
			 ATA_D_IBM | ATA_D_LBA | atadev->unit |
			 ((request->u.ata.lba >> 24) & 0x0f));
	}
    }
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
			     (dmatimings[mode & ATA_MODE_MASK] << offset), 4);
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
    static struct ata_chip_id ids[] =
    {{ ATA_SII3114,   0x00, SIIMEMIO, SII4CH,    ATA_SA150, "SiI 3114" },
     { ATA_SII3512,   0x02, SIIMEMIO, 0,         ATA_SA150, "SiI 3512" },
     { ATA_SII3112,   0x02, SIIMEMIO, 0,         ATA_SA150, "SiI 3112" },
     { ATA_SII3112_1, 0x02, SIIMEMIO, 0,         ATA_SA150, "SiI 3112" },
     { ATA_SII3512,   0x00, SIIMEMIO, SIIBUG,    ATA_SA150, "SiI 3512" },
     { ATA_SII3112,   0x00, SIIMEMIO, SIIBUG,    ATA_SA150, "SiI 3112" },
     { ATA_SII3112_1, 0x00, SIIMEMIO, SIIBUG,    ATA_SA150, "SiI 3112" },
     { ATA_SII3124,   0x00, SIIPRBIO, SII4CH,    ATA_SA300, "SiI 3124" },
     { ATA_SII3132,   0x00, SIIPRBIO, 0,         ATA_SA300, "SiI 3132" },
     { ATA_SII0680,   0x00, SIIMEMIO, SIISETCLK, ATA_UDMA6, "SiI 0680" },
     { ATA_CMD649,    0x00, 0,        SIIINTR,   ATA_UDMA5, "CMD 649" },
     { ATA_CMD648,    0x00, 0,        SIIINTR,   ATA_UDMA4, "CMD 648" },
     { ATA_CMD646,    0x07, 0,        0,         ATA_UDMA2, "CMD 646U2" },
     { ATA_CMD646,    0x00, 0,        0,         ATA_WDMA2, "CMD 646" },
     { 0, 0, 0, 0, 0, 0}};

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_sii_chipinit;
    return 0;
}

static int
ata_sii_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev))
	return ENXIO;

    switch (ctlr->chip->cfg1) {
    case SIIPRBIO:
	ctlr->r_type1 = SYS_RES_MEMORY;
	ctlr->r_rid1 = PCIR_BAR(0);
	if (!(ctlr->r_res1 = bus_alloc_resource_any(dev, ctlr->r_type1,
						    &ctlr->r_rid1, RF_ACTIVE)))
	    return ENXIO;

	ctlr->r_rid2 = PCIR_BAR(2);
	ctlr->r_type2 = SYS_RES_MEMORY;
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE))){
	    bus_release_resource(dev, ctlr->r_type1, ctlr->r_rid1,ctlr->r_res1);
	    return ENXIO;
	}
	ctlr->allocate = ata_siiprb_allocate;
	ctlr->reset = ata_siiprb_reset;
	ctlr->dmainit = ata_siiprb_dmainit;
	ctlr->setmode = ata_sata_setmode;
	ctlr->channels = (ctlr->chip->cfg2 == SII4CH) ? 4 : 2;

	/* reset controller */
	ATA_OUTL(ctlr->r_res1, 0x0040, 0x80000000);
	DELAY(10000);
	ATA_OUTL(ctlr->r_res1, 0x0040, 0x0000000f);

	/* enable PCI interrupt */
	pci_write_config(dev, PCIR_COMMAND,
			 pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);
	break;

    case SIIMEMIO:
	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE))) {
	    if (ctlr->chip->chipid != ATA_SII0680 ||
			    (pci_read_config(dev, 0x8a, 1) & 1))
		return ENXIO;
	}

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

	/* dont block interrupts from any channel */
	pci_write_config(dev, 0x48,
			 (pci_read_config(dev, 0x48, 4) & ~0x03c00000), 4);

	/* enable PCI interrupt as BIOS might not */
	pci_write_config(dev, 0x8a, (pci_read_config(dev, 0x8a, 1) & 0x3f), 1);

	if (ctlr->r_res2)
	    ctlr->allocate = ata_sii_allocate;

	if (ctlr->chip->max_dma >= ATA_SA150) {
	    ctlr->reset = ata_sii_reset;
	    ctlr->setmode = ata_sata_setmode;
	}
	else
	    ctlr->setmode = ata_sii_setmode;
	break;
    
    default:
	if ((pci_read_config(dev, 0x51, 1) & 0x08) != 0x08) {
	    device_printf(dev, "HW has secondary channel disabled\n");
	    ctlr->channels = 1;
	}    

	/* enable interrupt as BIOS might not */
	pci_write_config(dev, 0x71, 0x01, 1);

	ctlr->allocate = ata_cmd_allocate;
	ctlr->setmode = ata_cmd_setmode;
	break;
    }
    return 0;
}

static int
ata_cmd_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    if (ctlr->chip->cfg2 & SIIINTR)
	ch->hw.status = ata_cmd_status;

    return 0;
}

static int
ata_cmd_status(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    u_int8_t reg71;

    if (((reg71 = pci_read_config(device_get_parent(ch->dev), 0x71, 1)) &
	 (ch->unit ? 0x08 : 0x04))) {
	pci_write_config(device_get_parent(ch->dev), 0x71,
			 reg71 & ~(ch->unit ? 0x04 : 0x08), 1);
	return ata_pci_status(dev);
    }
    return 0;
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
	ch->dma->boundary = 8192;
	ch->dma->segsize = 15 * DEV_BSIZE;
    }

    ata_pci_hw(dev);
    ch->hw.status = ata_sii_status;
    return 0;
}

static int
ata_sii_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset0 = ((ch->unit & 1) << 3) + ((ch->unit & 2) << 8);
    int offset1 = ((ch->unit & 1) << 6) + ((ch->unit & 2) << 8);

    /* do we have any PHY events ? */
    if (ctlr->chip->max_dma >= ATA_SA150 &&
	(ATA_INL(ctlr->r_res2, 0x10 + offset0) & 0x00000010))
	ata_sata_phy_check_events(dev);

    if (ATA_INL(ctlr->r_res2, 0xa0 + offset1) & 0x00000800)
	return ata_pci_status(dev);
    else
	return 0;
}

static void
ata_sii_reset(device_t dev)
{
    if (ata_sata_phy_reset(dev))
	ata_generic_reset(dev);
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


struct ata_siiprb_dma_prdentry {
    u_int64_t addr;
    u_int32_t count;
    u_int32_t control;
} __packed;

struct ata_siiprb_ata_command {
    struct ata_siiprb_dma_prdentry prd[126];
} __packed;

struct ata_siiprb_atapi_command {
    u_int8_t ccb[16];
    struct ata_siiprb_dma_prdentry prd[125];
} __packed;

struct ata_siiprb_command {
    u_int16_t control;
    u_int16_t protocol_override;
    u_int32_t transfer_count;
    u_int8_t fis[24];
    union {
	struct ata_siiprb_ata_command ata;
	struct ata_siiprb_atapi_command atapi;
    } u;
} __packed;

static int
ata_siiprb_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ch->unit * 0x2000;

    /* set the SATA resources */
    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = 0x1f04 + offset;
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x1f08 + offset;
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x1f00 + offset;
    ch->r_io[ATA_SACTIVE].res = ctlr->r_res2;
    ch->r_io[ATA_SACTIVE].offset = 0x1f0c + offset;
   
    ch->hw.begin_transaction = ata_siiprb_begin_transaction;
    ch->hw.end_transaction = ata_siiprb_end_transaction;
    ch->hw.status = ata_siiprb_status;
    ch->hw.command = NULL;	/* not used here */
    return 0;
}

static int
ata_siiprb_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t action = ATA_INL(ctlr->r_res1, 0x0044);
    int offset = ch->unit * 0x2000;

    if (action & (1 << ch->unit)) {
	u_int32_t istatus = ATA_INL(ctlr->r_res2, 0x1008 + offset);

	/* do we have any PHY events ? */
	ata_sata_phy_check_events(dev);

	/* clear interrupt(s) */
	ATA_OUTL(ctlr->r_res2, 0x1008 + offset, istatus);

	/* do we have any device action ? */
	return (istatus & 0x00000003);
    }
    return 0;
}

static int
ata_siiprb_begin_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_siiprb_command *prb;
    struct ata_siiprb_dma_prdentry *prd;
    int offset = ch->unit * 0x2000;
    u_int64_t prb_bus;
    int tag = 0, dummy;

    /* SOS XXX */
    if (request->u.ata.command == ATA_DEVICE_RESET) {
        request->result = 0;
        return ATA_OP_FINISHED;
    }

    /* check for 48 bit access and convert if needed */
    ata_modify_if_48bit(request);

    /* get a piece of the workspace for this request */
    prb = (struct ata_siiprb_command *)
	(ch->dma->work + (sizeof(struct ata_siiprb_command) * tag));

    /* set basic prd options ata/atapi etc etc */
    bzero(prb, sizeof(struct ata_siiprb_command));

    /* setup the FIS for this request */
    if (!ata_request2fis_h2d(request, &prb->fis[0])) {
        device_printf(request->dev, "setting up SATA FIS failed\n");
        request->result = EIO;
        return ATA_OP_FINISHED;
    }

    /* setup transfer type */
    if (request->flags & ATA_R_ATAPI) {
        struct ata_device *atadev = device_get_softc(request->dev);

	bcopy(request->u.atapi.ccb, prb->u.atapi.ccb, 16);
	if ((atadev->param.config & ATA_PROTO_MASK) == ATA_PROTO_ATAPI_12)
	    ATA_OUTL(ctlr->r_res2, 0x1004 + offset, 0x00000020);
	else
	    ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000020);
	if (request->flags & ATA_R_READ)
	    prb->control = htole16(0x0010);
	if (request->flags & ATA_R_WRITE)
	    prb->control = htole16(0x0020);
	prd = &prb->u.atapi.prd[0];
    }
    else
	prd = &prb->u.ata.prd[0];

    /* if request moves data setup and load SG list */
    if (request->flags & (ATA_R_READ | ATA_R_WRITE)) {
	if (ch->dma->load(ch->dev, request->data, request->bytecount,
			  request->flags & ATA_R_READ, prd, &dummy)) {
	    device_printf(request->dev, "setting up DMA failed\n");
	    request->result = EIO;
	    return ATA_OP_FINISHED;
	}
    }

    /* activate the prb */
    prb_bus = ch->dma->work_bus + (sizeof(struct ata_siiprb_command) * tag);
    ATA_OUTL(ctlr->r_res2,
	     0x1c00 + offset + (tag * sizeof(u_int64_t)), prb_bus);
    ATA_OUTL(ctlr->r_res2,
	     0x1c04 + offset + (tag * sizeof(u_int64_t)), prb_bus>>32);

    /* start the timeout */
    callout_reset(&request->callout, request->timeout * hz,
                  (timeout_t*)ata_timeout, request);
    return ATA_OP_CONTINUES;
}

static int
ata_siiprb_end_transaction(struct ata_request *request)
{
    struct ata_pci_controller *ctlr=device_get_softc(GRANDPARENT(request->dev));
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_siiprb_command *prb;
    int offset = ch->unit * 0x2000;
    int error, timeout, tag = 0;

    /* kill the timeout */
    callout_stop(&request->callout);
    
    prb = (struct ata_siiprb_command *)
	((u_int8_t *)rman_get_virtual(ctlr->r_res2) + (tag << 7) + offset);

    /* any controller errors flagged ? */
    if ((error = ATA_INL(ctlr->r_res2, 0x1024 + offset))) {
	if (bootverbose)
	    printf("ata_siiprb_end_transaction %s error=%08x\n",
		   ata_cmd2str(request), error);

	/* if device error status get details */
	if (error == 1 || error == 2) {
	    request->status = prb->fis[2];
	    if (request->status & ATA_S_ERROR)
		request->error = prb->fis[3];
	}

 	/* SOS XXX handle other controller errors here */

	/* initialize port */
	ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000004);

	/* poll for port ready */
	for (timeout = 0; timeout < 1000; timeout++) {
	    DELAY(1000);
            if (ATA_INL(ctlr->r_res2, 0x1008 + offset) & 0x00040000)
        	break;
	}
	if (bootverbose) {
	    if (timeout >= 1000)
		device_printf(ch->dev, "port initialize timeout\n");
	    else
		device_printf(ch->dev, "port initialize time=%dms\n", timeout);
	}
    }

    /* update progress */
    if (!(request->status & ATA_S_ERROR) && !(request->flags & ATA_R_TIMEOUT)) {
	if (request->flags & ATA_R_READ)
	    request->donecount = prb->transfer_count;
	else
	    request->donecount = request->bytecount;
    }

    /* release SG list etc */
    ch->dma->unload(ch->dev);

    return ATA_OP_FINISHED;
}

static void
ata_siiprb_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ch->unit * 0x2000;
    struct ata_siiprb_command *prb;
    u_int64_t prb_bus;
    u_int32_t status, signature;
    int timeout, tag = 0;

    /* reset channel HW */
    ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000001);
    DELAY(1000);
    ATA_OUTL(ctlr->r_res2, 0x1004 + offset, 0x00000001);
    DELAY(10000);

    /* poll for channel ready */
    for (timeout = 0; timeout < 1000; timeout++) {
        if ((status = ATA_INL(ctlr->r_res2, 0x1008 + offset)) & 0x00040000)
            break;
        DELAY(1000);
    }

    if (bootverbose) {
	if (timeout >= 1000)
	    device_printf(ch->dev, "channel HW reset timeout\n");
	else
	    device_printf(ch->dev, "channel HW reset time=%dms\n", timeout);
    }

    /* reset phy */
    if (!ata_sata_phy_reset(dev)) {
	if (bootverbose)
	    device_printf(ch->dev, "phy reset found no device\n");
	ch->devices = 0;
	goto finish;
    }

    /* get a piece of the workspace for a soft reset request */
    prb = (struct ata_siiprb_command *)
	(ch->dma->work + (sizeof(struct ata_siiprb_command) * tag));
    bzero(prb, sizeof(struct ata_siiprb_command));
    prb->control = htole16(0x0080);

    /* activate the soft reset prb */
    prb_bus = ch->dma->work_bus + (sizeof(struct ata_siiprb_command) * tag);
    ATA_OUTL(ctlr->r_res2,
	     0x1c00 + offset + (tag * sizeof(u_int64_t)), prb_bus);
    ATA_OUTL(ctlr->r_res2,
	     0x1c04 + offset + (tag * sizeof(u_int64_t)), prb_bus>>32);

    /* poll for command finished */
    for (timeout = 0; timeout < 10000; timeout++) {
        DELAY(1000);
        if ((status = ATA_INL(ctlr->r_res2, 0x1008 + offset)) & 0x00010000)
            break;
    }
    if (timeout >= 1000) {
	device_printf(ch->dev, "reset timeout - no device found\n");
	ch->devices = 0;
	goto finish;
    }
    if (bootverbose)
	device_printf(ch->dev, "soft reset exec time=%dms status=%08x\n",
			timeout, status);

    /* find out whats there */
    prb = (struct ata_siiprb_command *)
	((u_int8_t *)rman_get_virtual(ctlr->r_res2) + (tag << 7) + offset);
    signature =
	prb->fis[12]|(prb->fis[4]<<8)|(prb->fis[5]<<16)|(prb->fis[6]<<24);
    if (bootverbose)
	device_printf(ch->dev, "SIGNATURE=%08x\n", signature);
    switch (signature) {
    case 0x00000101:
	ch->devices = ATA_ATA_MASTER;
	break;
    case 0x96690101:
	ch->devices = ATA_PORTMULTIPLIER;
	device_printf(ch->dev, "Portmultipliers not supported yet\n");
	ch->devices = 0;
	break;
    case 0xeb140101:
	ch->devices = ATA_ATAPI_MASTER;
	break;
    default:
	ch->devices = 0;
    }
    if (bootverbose)
        device_printf(dev, "siiprb_reset devices=0x%b\n", ch->devices,
                      "\20\4ATAPI_SLAVE\3ATAPI_MASTER\2ATA_SLAVE\1ATA_MASTER");

finish:
    /* clear interrupt(s) */
    ATA_OUTL(ctlr->r_res2, 0x1008 + offset, 0x000008ff);

    /* require explicit interrupt ack */
    ATA_OUTL(ctlr->r_res2, 0x1000 + offset, 0x00000008);

    /* 64bit mode */
    ATA_OUTL(ctlr->r_res2, 0x1004 + offset, 0x00000400);

    /* enable interrupts wanted */
    ATA_OUTL(ctlr->r_res2, 0x1010 + offset, 0x000000ff);
}

static void
ata_siiprb_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dmasetprd_args *args = xsc;
    struct ata_siiprb_dma_prdentry *prd = args->dmatab;
    int i;

    if ((args->error = error))
	return;

    for (i = 0; i < nsegs; i++) {
	prd[i].addr = htole64(segs[i].ds_addr);
	prd[i].count = htole32(segs[i].ds_len);
    }
    prd[i - 1].control = htole32(ATA_DMA_EOT);
    KASSERT(nsegs <= ATA_DMA_ENTRIES, ("too many DMA segment entries\n"));
    args->nsegs = nsegs;
}

static void
ata_siiprb_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    ata_dmainit(dev);
    if (ch->dma) {
	/* note start and stop are not used here */
	ch->dma->setprd = ata_siiprb_dmasetprd;
	ch->dma->max_address = BUS_SPACE_MAXADDR;
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
    {{ ATA_SIS182,  0x00, SISSATA,   0, ATA_SA150, "182" }, /* south */
     { ATA_SIS181,  0x00, SISSATA,   0, ATA_SA150, "181" }, /* south */
     { ATA_SIS180,  0x00, SISSATA,   0, ATA_SA150, "180" }, /* south */
     { ATA_SIS965,  0x00, SIS133NEW, 0, ATA_UDMA6, "965" }, /* south */
     { ATA_SIS964,  0x00, SIS133NEW, 0, ATA_UDMA6, "964" }, /* south */
     { ATA_SIS963,  0x00, SIS133NEW, 0, ATA_UDMA6, "963" }, /* south */
     { ATA_SIS962,  0x00, SIS133NEW, 0, ATA_UDMA6, "962" }, /* south */

     { ATA_SIS745,  0x00, SIS100NEW, 0, ATA_UDMA5, "745" }, /* 1chip */
     { ATA_SIS735,  0x00, SIS100NEW, 0, ATA_UDMA5, "735" }, /* 1chip */
     { ATA_SIS733,  0x00, SIS100NEW, 0, ATA_UDMA5, "733" }, /* 1chip */
     { ATA_SIS730,  0x00, SIS100OLD, 0, ATA_UDMA5, "730" }, /* 1chip */

     { ATA_SIS635,  0x00, SIS100NEW, 0, ATA_UDMA5, "635" }, /* 1chip */
     { ATA_SIS633,  0x00, SIS100NEW, 0, ATA_UDMA5, "633" }, /* unknown */
     { ATA_SIS630,  0x30, SIS100OLD, 0, ATA_UDMA5, "630S"}, /* 1chip */
     { ATA_SIS630,  0x00, SIS66,     0, ATA_UDMA4, "630" }, /* 1chip */
     { ATA_SIS620,  0x00, SIS66,     0, ATA_UDMA4, "620" }, /* 1chip */

     { ATA_SIS550,  0x00, SIS66,     0, ATA_UDMA5, "550" },
     { ATA_SIS540,  0x00, SIS66,     0, ATA_UDMA4, "540" },
     { ATA_SIS530,  0x00, SIS66,     0, ATA_UDMA4, "530" },

     { ATA_SIS5513, 0xc2, SIS33,     1, ATA_UDMA2, "5513" },
     { ATA_SIS5513, 0x00, SIS33,     1, ATA_WDMA2, "5513" },
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
	sprintf(buffer,"SiS %s %s controller",
		idx->text, ata_mode2str(idx->max_dma));

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
	    ctlr->allocate = ata_sis_allocate;
	    ctlr->reset = ata_sis_reset;

	    /* enable PCI interrupt */
	    pci_write_config(dev, PCIR_COMMAND,
			     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400,2);
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
    int offset = ch->unit << ((ctlr->chip->chipid == ATA_SIS182) ? 5 : 6);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = 0x00 + offset;
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x04 + offset;
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x08 + offset;
    ch->flags |= ATA_NO_SLAVE;

    /* XXX SOS PHY hotplug handling missing in SiS chip ?? */
    /* XXX SOS unknown how to enable PHY state change interrupt */
    return 0;
}

static void
ata_sis_reset(device_t dev)
{
    if (ata_sata_phy_reset(dev))
	ata_generic_reset(dev);
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
    static struct ata_chip_id ids[] =
    {{ ATA_VIA82C586, 0x02, VIA33,  0x00,    ATA_UDMA2, "82C586B" },
     { ATA_VIA82C586, 0x00, VIA33,  0x00,    ATA_WDMA2, "82C586" },
     { ATA_VIA82C596, 0x12, VIA66,  VIACLK,  ATA_UDMA4, "82C596B" },
     { ATA_VIA82C596, 0x00, VIA33,  0x00,    ATA_UDMA2, "82C596" },
     { ATA_VIA82C686, 0x40, VIA100, VIABUG,  ATA_UDMA5, "82C686B"},
     { ATA_VIA82C686, 0x10, VIA66,  VIACLK,  ATA_UDMA4, "82C686A" },
     { ATA_VIA82C686, 0x00, VIA33,  0x00,    ATA_UDMA2, "82C686" },
     { ATA_VIA8231,   0x00, VIA100, VIABUG,  ATA_UDMA5, "8231" },
     { ATA_VIA8233,   0x00, VIA100, 0x00,    ATA_UDMA5, "8233" },
     { ATA_VIA8233C,  0x00, VIA100, 0x00,    ATA_UDMA5, "8233C" },
     { ATA_VIA8233A,  0x00, VIA133, 0x00,    ATA_UDMA6, "8233A" },
     { ATA_VIA8235,   0x00, VIA133, 0x00,    ATA_UDMA6, "8235" },
     { ATA_VIA8237,   0x00, VIA133, 0x00,    ATA_UDMA6, "8237" },
     { ATA_VIA8237A,  0x00, VIA133, 0x00,    ATA_UDMA6, "8237A" },
     { ATA_VIA8237S,  0x00, VIA133, 0x00,    ATA_UDMA6, "8237S" },
     { ATA_VIA8251,   0x00, VIA133, 0x00,    ATA_UDMA6, "8251" },
     { 0, 0, 0, 0, 0, 0 }};
    static struct ata_chip_id new_ids[] =
    {{ ATA_VIA6410,   0x00, 0,      0x00,    ATA_UDMA6, "6410" },
     { ATA_VIA6420,   0x00, 7,      0x00,    ATA_SA150, "6420" },
     { ATA_VIA6421,   0x00, 6,      VIABAR,  ATA_SA150, "6421" },
     { ATA_VIA8237A,  0x00, 7,      0x00,    ATA_SA150, "8237A" },
     { ATA_VIA8237S,  0x00, 7,      0x00,    ATA_SA150, "8237S" },
     { ATA_VIA8251,   0x00, 0,      VIAAHCI, ATA_SA300, "8251" },
     { 0, 0, 0, 0, 0, 0 }};

    if (pci_get_devid(dev) == ATA_VIA82C571) {
	if (!(ctlr->chip = ata_find_chip(dev, ids, -99))) 
	    return ENXIO;
    }
    else {
	if (!(ctlr->chip = ata_match_chip(dev, new_ids))) 
	    return ENXIO;
    }

    ata_set_desc(dev);
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
	/* do we have AHCI capability ? */
	if ((ctlr->chip->cfg2 == VIAAHCI) && ata_ahci_chipinit(dev) != ENXIO)
	    return 0;

	ctlr->r_type2 = SYS_RES_IOPORT;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE))) {
	    ctlr->allocate = ata_via_allocate;
	    ctlr->reset = ata_via_reset;

	    /* enable PCI interrupt */
	    pci_write_config(dev, PCIR_COMMAND,
			     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400,2);
	}

	if (ctlr->chip->cfg2 & VIABAR) {
	    ctlr->channels = 3;
	    ctlr->setmode = ata_via_setmode;
	}
	else
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
	    ch->r_io[i].offset = (i - ATA_BMCMD_PORT)+(ch->unit * ATA_BMIOSIZE);
	}
	ata_pci_hw(dev);
	if (ch->unit >= 2)
	    return 0;
    }
    else {
	/* setup the usual register normal pci style */
	if (ata_pci_allocate(dev))
	    return ENXIO;
    }

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
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    if ((ctlr->chip->cfg2 & VIABAR) && (ch->unit > 1))
        ata_generic_reset(dev);
    else
	if (ata_sata_phy_reset(dev))
	    ata_generic_reset(dev);
}

static void
ata_via_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int error;

    if ((ctlr->chip->cfg2 & VIABAR) && (ch->unit > 1)) {
        u_int8_t pio_timings[] = { 0xa8, 0x65, 0x65, 0x32, 0x20,
				   0x65, 0x32, 0x20,
				   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
        u_int8_t dma_timings[] = { 0xee, 0xe8, 0xe6, 0xe4, 0xe2, 0xe1, 0xe0 };

	mode = ata_check_80pin(dev, ata_limit_mode(dev, mode, ATA_UDMA6));
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	    device_printf(dev, "%ssetting %s on %s chip\n",
			  (error) ? "FAILURE " : "", ata_mode2str(mode),
			  ctlr->chip->text);
	if (!error) {
	    pci_write_config(gparent, 0xab, pio_timings[ata_mode2idx(mode)], 1);
	    if (mode >= ATA_UDMA0)
		pci_write_config(gparent, 0xb3,
				 dma_timings[mode & ATA_MODE_MASK], 1);
	    atadev->mode = mode;
	}
    }
    else
	ata_sata_setmode(dev, mode);
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
			   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
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
static void
ata_set_desc(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    char buffer[128];

    sprintf(buffer, "%s %s %s controller",
            ata_pcivendor2str(dev), ctlr->chip->text, 
            ata_mode2str(ctlr->chip->max_dma));
    device_set_desc_copy(dev, buffer);
}

static struct ata_chip_id *
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
	    /* SOS XXX release r_irq */
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
