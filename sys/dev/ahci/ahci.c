/*-
 * Copyright (c) 2009 Alexander Motin <mav@FreeBSD.org>
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
#include "ahci.h"

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>

/* local prototypes */
static int ahci_setup_interrupt(device_t dev);
static void ahci_intr(void *data);
static void ahci_intr_one(void *data);
static int ahci_suspend(device_t dev);
static int ahci_resume(device_t dev);
static int ahci_ch_suspend(device_t dev);
static int ahci_ch_resume(device_t dev);
static void ahci_ch_pm(void *arg);
static void ahci_ch_intr_locked(void *data);
static void ahci_ch_intr(void *data);
static int ahci_ctlr_reset(device_t dev);
static void ahci_begin_transaction(device_t dev, union ccb *ccb);
static void ahci_dmasetprd(void *arg, bus_dma_segment_t *segs, int nsegs, int error);
static void ahci_execute_transaction(struct ahci_slot *slot);
static void ahci_timeout(struct ahci_slot *slot);
static void ahci_end_transaction(struct ahci_slot *slot, enum ahci_err_type et);
static int ahci_setup_fis(struct ahci_cmd_tab *ctp, union ccb *ccb, int tag);
static void ahci_dmainit(device_t dev);
static void ahci_dmasetupc_cb(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
static void ahci_dmafini(device_t dev);
static void ahci_slotsalloc(device_t dev);
static void ahci_slotsfree(device_t dev);
static void ahci_reset(device_t dev);
static void ahci_start(device_t dev);
static void ahci_stop(device_t dev);
static void ahci_clo(device_t dev);
static void ahci_start_fr(device_t dev);
static void ahci_stop_fr(device_t dev);

static int ahci_sata_connect(struct ahci_channel *ch);
static int ahci_sata_phy_reset(device_t dev, int quick);

static void ahci_issue_read_log(device_t dev);
static void ahci_process_read_log(device_t dev, union ccb *ccb);

static void ahciaction(struct cam_sim *sim, union ccb *ccb);
static void ahcipoll(struct cam_sim *sim);

MALLOC_DEFINE(M_AHCI, "AHCI driver", "AHCI driver data buffers");

/*
 * AHCI v1.x compliant SATA chipset support functions
 */
static int
ahci_probe(device_t dev)
{

	/* is this a possible AHCI candidate ? */
	if (pci_get_class(dev) != PCIC_STORAGE ||
	    pci_get_subclass(dev) != PCIS_STORAGE_SATA)
		return (ENXIO);

	/* is this PCI device flagged as an AHCI compliant chip ? */
	if (pci_get_progif(dev) != PCIP_STORAGE_SATA_AHCI_1_0)
		return (ENXIO);

	device_set_desc_copy(dev, "AHCI controller");
	return (BUS_PROBE_VENDOR);
}

static int
ahci_attach(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	device_t child;
	int	error, unit, speed;
	u_int32_t version;

	ctlr->dev = dev;
	resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "ccc", &ctlr->ccc);
	/* if we have a memory BAR(5) we are likely on an AHCI part */
	ctlr->r_rid = PCIR_BAR(5);
	if (!(ctlr->r_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &ctlr->r_rid, RF_ACTIVE)))
		return ENXIO;
	/* Setup our own memory management for channels. */
	ctlr->sc_iomem.rm_type = RMAN_ARRAY;
	ctlr->sc_iomem.rm_descr = "I/O memory addresses";
	if ((error = rman_init(&ctlr->sc_iomem)) != 0) {
		bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid, ctlr->r_mem);
		return (error);
	}
	if ((error = rman_manage_region(&ctlr->sc_iomem,
	    rman_get_start(ctlr->r_mem), rman_get_end(ctlr->r_mem))) != 0) {
		bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid, ctlr->r_mem);
		rman_fini(&ctlr->sc_iomem);
		return (error);
	}
	/* Reset controller */
	if ((error = ahci_ctlr_reset(dev)) != 0) {
		bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid, ctlr->r_mem);
		rman_fini(&ctlr->sc_iomem);
		return (error);
	};
	/* Get the number of HW channels */
	ctlr->ichannels = ATA_INL(ctlr->r_mem, AHCI_PI);
	ctlr->channels = MAX(flsl(ctlr->ichannels),
	    (ATA_INL(ctlr->r_mem, AHCI_CAP) & AHCI_CAP_NPMASK) + 1);
	/* Setup interrupts. */
	if (ahci_setup_interrupt(dev)) {
		bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid, ctlr->r_mem);
		rman_fini(&ctlr->sc_iomem);
		return ENXIO;
	}
	/* Announce HW capabilities. */
	version = ATA_INL(ctlr->r_mem, AHCI_VS);
	ctlr->caps = ATA_INL(ctlr->r_mem, AHCI_CAP);
	if (version >= 0x00010020)
		ctlr->caps2 = ATA_INL(ctlr->r_mem, AHCI_CAP2);
	speed = (ctlr->caps & AHCI_CAP_ISS) >> AHCI_CAP_ISS_SHIFT;
	device_printf(dev,
		    "AHCI v%x.%02x with %d %sGbps ports, Port Multiplier %s\n",
		    ((version >> 20) & 0xf0) + ((version >> 16) & 0x0f),
		    ((version >> 4) & 0xf0) + (version & 0x0f),
		    (ctlr->caps & AHCI_CAP_NPMASK) + 1,
		    ((speed == 1) ? "1.5":((speed == 2) ? "3":
		    ((speed == 3) ? "6":"?"))),
		    (ctlr->caps & AHCI_CAP_SPM) ?
		    "supported" : "not supported");
	if (bootverbose) {
		device_printf(dev, "Caps:%s%s%s%s%s%s%s%s %sGbps",
		    (ctlr->caps & AHCI_CAP_64BIT) ? " 64bit":"",
		    (ctlr->caps & AHCI_CAP_SNCQ) ? " NCQ":"",
		    (ctlr->caps & AHCI_CAP_SSNTF) ? " SNTF":"",
		    (ctlr->caps & AHCI_CAP_SMPS) ? " MPS":"",
		    (ctlr->caps & AHCI_CAP_SSS) ? " SS":"",
		    (ctlr->caps & AHCI_CAP_SALP) ? " ALP":"",
		    (ctlr->caps & AHCI_CAP_SAL) ? " AL":"",
		    (ctlr->caps & AHCI_CAP_SCLO) ? " CLO":"",
		    ((speed == 1) ? "1.5":((speed == 2) ? "3":
		    ((speed == 3) ? "6":"?"))));
		printf("%s%s%s%s%s%s %dcmd%s%s%s %dports\n",
		    (ctlr->caps & AHCI_CAP_SAM) ? " AM":"",
		    (ctlr->caps & AHCI_CAP_SPM) ? " PM":"",
		    (ctlr->caps & AHCI_CAP_FBSS) ? " FBS":"",
		    (ctlr->caps & AHCI_CAP_PMD) ? " PMD":"",
		    (ctlr->caps & AHCI_CAP_SSC) ? " SSC":"",
		    (ctlr->caps & AHCI_CAP_PSC) ? " PSC":"",
		    ((ctlr->caps & AHCI_CAP_NCS) >> AHCI_CAP_NCS_SHIFT) + 1,
		    (ctlr->caps & AHCI_CAP_CCCS) ? " CCC":"",
		    (ctlr->caps & AHCI_CAP_EMS) ? " EM":"",
		    (ctlr->caps & AHCI_CAP_SXS) ? " eSATA":"",
		    (ctlr->caps & AHCI_CAP_NPMASK) + 1);
	}
	if (bootverbose && version >= 0x00010020) {
		device_printf(dev, "Caps2:%s%s%s\n",
		    (ctlr->caps2 & AHCI_CAP2_APST) ? " APST":"",
		    (ctlr->caps2 & AHCI_CAP2_NVMP) ? " NVMP":"",
		    (ctlr->caps2 & AHCI_CAP2_BOH) ? " BOH":"");
	}
	/* Attach all channels on this controller */
	for (unit = 0; unit < ctlr->channels; unit++) {
		if ((ctlr->ichannels & (1 << unit)) == 0)
			continue;
		child = device_add_child(dev, "ahcich", -1);
		if (child == NULL)
			device_printf(dev, "failed to add channel device\n");
		else
			device_set_ivars(child, (void *)(intptr_t)unit);
	}
	bus_generic_attach(dev);
	return 0;
}

static int
ahci_detach(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	device_t *children;
	int nchildren, i;

	/* Detach & delete all children */
	if (!device_get_children(dev, &children, &nchildren)) {
		for (i = 0; i < nchildren; i++)
			device_delete_child(dev, children[i]);
		free(children, M_TEMP);
	}
	/* Free interrupts. */
	for (i = 0; i < ctlr->numirqs; i++) {
		if (ctlr->irqs[i].r_irq) {
			bus_teardown_intr(dev, ctlr->irqs[i].r_irq,
			    ctlr->irqs[i].handle);
			bus_release_resource(dev, SYS_RES_IRQ,
			    ctlr->irqs[i].r_irq_rid, ctlr->irqs[i].r_irq);
		}
	}
	pci_release_msi(dev);
	/* Free memory. */
	rman_fini(&ctlr->sc_iomem);
	if (ctlr->r_mem)
		bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid, ctlr->r_mem);
	return (0);
}

static int
ahci_ctlr_reset(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	int timeout;

	if (pci_read_config(dev, 0x00, 4) == 0x28298086 &&
	    (pci_read_config(dev, 0x92, 1) & 0xfe) == 0x04)
		pci_write_config(dev, 0x92, 0x01, 1);
	/* Enable AHCI mode */
	ATA_OUTL(ctlr->r_mem, AHCI_GHC, AHCI_GHC_AE);
	/* Reset AHCI controller */
	ATA_OUTL(ctlr->r_mem, AHCI_GHC, AHCI_GHC_AE|AHCI_GHC_HR);
	for (timeout = 1000; timeout > 0; timeout--) {
		DELAY(1000);
		if ((ATA_INL(ctlr->r_mem, AHCI_GHC) & AHCI_GHC_HR) == 0)
			break;
	}
	if (timeout == 0) {
		device_printf(dev, "AHCI controller reset failure\n");
		return ENXIO;
	}
	/* Reenable AHCI mode */
	ATA_OUTL(ctlr->r_mem, AHCI_GHC, AHCI_GHC_AE);
	/* Clear interrupts */
	ATA_OUTL(ctlr->r_mem, AHCI_IS, ATA_INL(ctlr->r_mem, AHCI_IS));
	/* Configure CCC */
	if (ctlr->ccc) {
		ATA_OUTL(ctlr->r_mem, AHCI_CCCP, ATA_INL(ctlr->r_mem, AHCI_PI));
		ATA_OUTL(ctlr->r_mem, AHCI_CCCC,
		    (ctlr->ccc << AHCI_CCCC_TV_SHIFT) |
		    (4 << AHCI_CCCC_CC_SHIFT) |
		    AHCI_CCCC_EN);
		ctlr->cccv = (ATA_INL(ctlr->r_mem, AHCI_CCCC) &
		    AHCI_CCCC_INT_MASK) >> AHCI_CCCC_INT_SHIFT;
		if (bootverbose) {
			device_printf(dev,
			    "CCC with %dms/4cmd enabled on vector %d\n",
			    ctlr->ccc, ctlr->cccv);
		}
	}
	/* Enable AHCI interrupts */
	ATA_OUTL(ctlr->r_mem, AHCI_GHC,
	    ATA_INL(ctlr->r_mem, AHCI_GHC) | AHCI_GHC_IE);
	return (0);
}

static int
ahci_suspend(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);

	bus_generic_suspend(dev);
	/* Disable interupts, so the state change(s) doesn't trigger */
	ATA_OUTL(ctlr->r_mem, AHCI_GHC,
	     ATA_INL(ctlr->r_mem, AHCI_GHC) & (~AHCI_GHC_IE));
	return 0;
}

static int
ahci_resume(device_t dev)
{
	int res;

	if ((res = ahci_ctlr_reset(dev)) != 0)
		return (res);
	return (bus_generic_resume(dev));
}

static int
ahci_setup_interrupt(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	int i, msi = 1;

	/* Process hints. */
	resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "msi", &msi);
	if (msi < 0)
		msi = 0;
	else if (msi == 1)
		msi = min(1, pci_msi_count(dev));
	else if (msi > 1)
		msi = pci_msi_count(dev);
	/* Allocate MSI if needed/present. */
	if (msi && pci_alloc_msi(dev, &msi) == 0) {
		ctlr->numirqs = msi;
	} else {
		msi = 0;
		ctlr->numirqs = 1;
	}
	/* Check for single MSI vector fallback. */
	if (ctlr->numirqs > 1 &&
	    (ATA_INL(ctlr->r_mem, AHCI_GHC) & AHCI_GHC_MRSM) != 0) {
		device_printf(dev, "Falling back to one MSI\n");
		ctlr->numirqs = 1;
	}
	/* Allocate all IRQs. */
	for (i = 0; i < ctlr->numirqs; i++) {
		ctlr->irqs[i].ctlr = ctlr;
		ctlr->irqs[i].r_irq_rid = i + (msi ? 1 : 0);
		if (ctlr->numirqs == 1 || i >= ctlr->channels ||
		    (ctlr->ccc && i == ctlr->cccv))
			ctlr->irqs[i].mode = AHCI_IRQ_MODE_ALL;
		else if (i == ctlr->numirqs - 1)
			ctlr->irqs[i].mode = AHCI_IRQ_MODE_AFTER;
		else
			ctlr->irqs[i].mode = AHCI_IRQ_MODE_ONE;
		if (!(ctlr->irqs[i].r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &ctlr->irqs[i].r_irq_rid, RF_SHAREABLE | RF_ACTIVE))) {
			device_printf(dev, "unable to map interrupt\n");
			return ENXIO;
		}
		if ((bus_setup_intr(dev, ctlr->irqs[i].r_irq, ATA_INTR_FLAGS, NULL,
		    (ctlr->irqs[i].mode == AHCI_IRQ_MODE_ONE) ? ahci_intr_one : ahci_intr,
		    &ctlr->irqs[i], &ctlr->irqs[i].handle))) {
			/* SOS XXX release r_irq */
			device_printf(dev, "unable to setup interrupt\n");
			return ENXIO;
		}
	}
	return (0);
}

/*
 * Common case interrupt handler.
 */
static void
ahci_intr(void *data)
{
	struct ahci_controller_irq *irq = data;
	struct ahci_controller *ctlr = irq->ctlr;
	u_int32_t is;
	void *arg;
	int unit;

	if (irq->mode == AHCI_IRQ_MODE_ALL) {
		unit = 0;
		if (ctlr->ccc)
			is = ctlr->ichannels;
		else
			is = ATA_INL(ctlr->r_mem, AHCI_IS);
	} else {	/* AHCI_IRQ_MODE_AFTER */
		unit = irq->r_irq_rid - 1;
		is = ATA_INL(ctlr->r_mem, AHCI_IS);
	}
	for (; unit < ctlr->channels; unit++) {
		if ((is & (1 << unit)) != 0 &&
		    (arg = ctlr->interrupt[unit].argument)) {
			ctlr->interrupt[unit].function(arg);
			ATA_OUTL(ctlr->r_mem, AHCI_IS, 1 << unit);
		}
	}
}

/*
 * Simplified interrupt handler for multivector MSI mode.
 */
static void
ahci_intr_one(void *data)
{
	struct ahci_controller_irq *irq = data;
	struct ahci_controller *ctlr = irq->ctlr;
	void *arg;
	int unit;

	unit = irq->r_irq_rid - 1;
	if ((arg = ctlr->interrupt[unit].argument))
	    ctlr->interrupt[unit].function(arg);
}

static struct resource *
ahci_alloc_resource(device_t dev, device_t child, int type, int *rid,
		       u_long start, u_long end, u_long count, u_int flags)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	int unit = ((struct ahci_channel *)device_get_softc(child))->unit;
	struct resource *res = NULL;
	int offset = AHCI_OFFSET + (unit << 7);
	long st;

	switch (type) {
	case SYS_RES_MEMORY:
		st = rman_get_start(ctlr->r_mem);
		res = rman_reserve_resource(&ctlr->sc_iomem, st + offset,
		    st + offset + 127, 128, RF_ACTIVE, child);
		if (res) {
			bus_space_handle_t bsh;
			bus_space_tag_t bst;
			bsh = rman_get_bushandle(ctlr->r_mem);
			bst = rman_get_bustag(ctlr->r_mem);
			bus_space_subregion(bst, bsh, offset, 128, &bsh);
			rman_set_bushandle(res, bsh);
			rman_set_bustag(res, bst);
		}
		break;
	case SYS_RES_IRQ:
		if (*rid == ATA_IRQ_RID)
			res = ctlr->irqs[0].r_irq;
		break;
	}
	return (res);
}

static int
ahci_release_resource(device_t dev, device_t child, int type, int rid,
			 struct resource *r)
{

	switch (type) {
	case SYS_RES_MEMORY:
		rman_release_resource(r);
		return (0);
	case SYS_RES_IRQ:
		if (rid != ATA_IRQ_RID)
			return ENOENT;
		return (0);
	}
	return (EINVAL);
}

static int
ahci_setup_intr(device_t dev, device_t child, struct resource *irq, 
		   int flags, driver_filter_t *filter, driver_intr_t *function, 
		   void *argument, void **cookiep)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	int unit = (intptr_t)device_get_ivars(child);

	if (filter != NULL) {
		printf("ahci.c: we cannot use a filter here\n");
		return (EINVAL);
	}
	ctlr->interrupt[unit].function = function;
	ctlr->interrupt[unit].argument = argument;
	return (0);
}

static int
ahci_teardown_intr(device_t dev, device_t child, struct resource *irq,
		      void *cookie)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	int unit = (intptr_t)device_get_ivars(child);

	ctlr->interrupt[unit].function = NULL;
	ctlr->interrupt[unit].argument = NULL;
	return (0);
}

static int
ahci_print_child(device_t dev, device_t child)
{
	int retval;

	retval = bus_print_child_header(dev, child);
	retval += printf(" at channel %d",
	    (int)(intptr_t)device_get_ivars(child));
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

devclass_t ahci_devclass;
static device_method_t ahci_methods[] = {
	DEVMETHOD(device_probe,     ahci_probe),
	DEVMETHOD(device_attach,    ahci_attach),
	DEVMETHOD(device_detach,    ahci_detach),
	DEVMETHOD(device_suspend,   ahci_suspend),
	DEVMETHOD(device_resume,    ahci_resume),
	DEVMETHOD(bus_print_child,  ahci_print_child),
	DEVMETHOD(bus_alloc_resource,       ahci_alloc_resource),
	DEVMETHOD(bus_release_resource,     ahci_release_resource),
	DEVMETHOD(bus_setup_intr,   ahci_setup_intr),
	DEVMETHOD(bus_teardown_intr,ahci_teardown_intr),
	{ 0, 0 }
};
static driver_t ahci_driver = {
        "ahci",
        ahci_methods,
        sizeof(struct ahci_controller)
};
DRIVER_MODULE(ahci, pci, ahci_driver, ahci_devclass, 0, 0);
MODULE_VERSION(ahci, 1);
MODULE_DEPEND(ahci, cam, 1, 1, 1);

static int
ahci_ch_probe(device_t dev)
{

	device_set_desc_copy(dev, "AHCI channel");
	return (0);
}

static int
ahci_ch_attach(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(device_get_parent(dev));
	struct ahci_channel *ch = device_get_softc(dev);
	struct cam_devq *devq;
	int rid, error;

	ch->dev = dev;
	ch->unit = (intptr_t)device_get_ivars(dev);
	ch->caps = ctlr->caps;
	ch->caps2 = ctlr->caps2;
	ch->numslots = ((ch->caps & AHCI_CAP_NCS) >> AHCI_CAP_NCS_SHIFT) + 1,
	mtx_init(&ch->mtx, "AHCI channel lock", NULL, MTX_DEF);
	resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "pm_level", &ch->pm_level);
	if (ch->pm_level > 3)
		callout_init_mtx(&ch->pm_timer, &ch->mtx, 0);
	/* Limit speed for my onboard JMicron external port.
	 * It is not eSATA really. */
	if (pci_get_devid(ctlr->dev) == 0x2363197b &&
	    pci_get_subvendor(ctlr->dev) == 0x1043 &&
	    pci_get_subdevice(ctlr->dev) == 0x81e4 &&
	    ch->unit == 0)
		ch->sata_rev = 1;
	resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "sata_rev", &ch->sata_rev);
	rid = ch->unit;
	if (!(ch->r_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE)))
		return (ENXIO);
	ahci_dmainit(dev);
	ahci_slotsalloc(dev);
	ahci_ch_resume(dev);
	mtx_lock(&ch->mtx);
	rid = ATA_IRQ_RID;
	if (!(ch->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &rid, RF_SHAREABLE | RF_ACTIVE))) {
		bus_release_resource(dev, SYS_RES_MEMORY, ch->unit, ch->r_mem);
		device_printf(dev, "Unable to map interrupt\n");
		return (ENXIO);
	}
	if ((bus_setup_intr(dev, ch->r_irq, ATA_INTR_FLAGS, NULL,
	    ahci_ch_intr_locked, dev, &ch->ih))) {
		device_printf(dev, "Unable to setup interrupt\n");
		error = ENXIO;
		goto err1;
	}
	/* Create the device queue for our SIM. */
	devq = cam_simq_alloc(ch->numslots);
	if (devq == NULL) {
		device_printf(dev, "Unable to allocate simq\n");
		error = ENOMEM;
		goto err1;
	}
	/* Construct SIM entry */
	ch->sim = cam_sim_alloc(ahciaction, ahcipoll, "ahcich", ch,
	    device_get_unit(dev), &ch->mtx, ch->numslots, 0, devq);
	if (ch->sim == NULL) {
		device_printf(dev, "unable to allocate sim\n");
		error = ENOMEM;
		goto err2;
	}
	if (xpt_bus_register(ch->sim, dev, 0) != CAM_SUCCESS) {
		device_printf(dev, "unable to register xpt bus\n");
		error = ENXIO;
		goto err2;
	}
	if (xpt_create_path(&ch->path, /*periph*/NULL, cam_sim_path(ch->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		device_printf(dev, "unable to create path\n");
		error = ENXIO;
		goto err3;
	}
	if (ch->pm_level > 3) {
		callout_reset(&ch->pm_timer,
		    (ch->pm_level == 4) ? hz / 1000 : hz / 8,
		    ahci_ch_pm, dev);
	}
	mtx_unlock(&ch->mtx);
	return (0);

err3:
	xpt_bus_deregister(cam_sim_path(ch->sim));
err2:
	cam_sim_free(ch->sim, /*free_devq*/TRUE);
err1:
	bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, ch->r_irq);
	bus_release_resource(dev, SYS_RES_MEMORY, ch->unit, ch->r_mem);
	mtx_unlock(&ch->mtx);
	return (error);
}

static int
ahci_ch_detach(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);

	mtx_lock(&ch->mtx);
	xpt_async(AC_LOST_DEVICE, ch->path, NULL);
	xpt_free_path(ch->path);
	xpt_bus_deregister(cam_sim_path(ch->sim));
	cam_sim_free(ch->sim, /*free_devq*/TRUE);
	mtx_unlock(&ch->mtx);

	if (ch->pm_level > 3)
		callout_drain(&ch->pm_timer);
	bus_teardown_intr(dev, ch->r_irq, ch->ih);
	bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, ch->r_irq);

	ahci_ch_suspend(dev);
	ahci_slotsfree(dev);
	ahci_dmafini(dev);

	bus_release_resource(dev, SYS_RES_MEMORY, ch->unit, ch->r_mem);
	mtx_destroy(&ch->mtx);
	return (0);
}

static int
ahci_ch_suspend(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);

	/* Disable port interrupts. */
	ATA_OUTL(ch->r_mem, AHCI_P_IE, 0);
	/* Reset command register. */
	ahci_stop(dev);
	ahci_stop_fr(dev);
	ATA_OUTL(ch->r_mem, AHCI_P_CMD, 0);
	/* Allow everything, including partial and slumber modes. */
	ATA_OUTL(ch->r_mem, AHCI_P_SCTL, 0);
	/* Request slumber mode transition and give some time to get there. */
	ATA_OUTL(ch->r_mem, AHCI_P_CMD, AHCI_P_CMD_SLUMBER);
	DELAY(100);
	/* Disable PHY. */
	ATA_OUTL(ch->r_mem, AHCI_P_SCTL, ATA_SC_DET_DISABLE);
	return (0);
}

static int
ahci_ch_resume(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	uint64_t work;

	/* Disable port interrupts */
	ATA_OUTL(ch->r_mem, AHCI_P_IE, 0);
	/* Setup work areas */
	work = ch->dma.work_bus + AHCI_CL_OFFSET;
	ATA_OUTL(ch->r_mem, AHCI_P_CLB, work & 0xffffffff);
	ATA_OUTL(ch->r_mem, AHCI_P_CLBU, work >> 32);
	work = ch->dma.rfis_bus;
	ATA_OUTL(ch->r_mem, AHCI_P_FB, work & 0xffffffff); 
	ATA_OUTL(ch->r_mem, AHCI_P_FBU, work >> 32);
	/* Activate the channel and power/spin up device */
	ATA_OUTL(ch->r_mem, AHCI_P_CMD,
	     (AHCI_P_CMD_ACTIVE | AHCI_P_CMD_POD | AHCI_P_CMD_SUD |
	     ((ch->pm_level == 2 || ch->pm_level == 3) ? AHCI_P_CMD_ALPE : 0) |
	     ((ch->pm_level > 2) ? AHCI_P_CMD_ASP : 0 )));
	ahci_start_fr(dev);
	ahci_start(dev);
	return (0);
}

devclass_t ahcich_devclass;
static device_method_t ahcich_methods[] = {
	DEVMETHOD(device_probe,     ahci_ch_probe),
	DEVMETHOD(device_attach,    ahci_ch_attach),
	DEVMETHOD(device_detach,    ahci_ch_detach),
	DEVMETHOD(device_suspend,   ahci_ch_suspend),
	DEVMETHOD(device_resume,    ahci_ch_resume),
	{ 0, 0 }
};
static driver_t ahcich_driver = {
        "ahcich",
        ahcich_methods,
        sizeof(struct ahci_channel)
};
DRIVER_MODULE(ahcich, ahci, ahcich_driver, ahci_devclass, 0, 0);

struct ahci_dc_cb_args {
	bus_addr_t maddr;
	int error;
};

static void
ahci_dmainit(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	struct ahci_dc_cb_args dcba;

	if (ch->caps & AHCI_CAP_64BIT)
		ch->dma.max_address = BUS_SPACE_MAXADDR;
	else
		ch->dma.max_address = BUS_SPACE_MAXADDR_32BIT;
	/* Command area. */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 1024, 0,
	    ch->dma.max_address, BUS_SPACE_MAXADDR,
	    NULL, NULL, AHCI_WORK_SIZE, 1, AHCI_WORK_SIZE,
	    0, NULL, NULL, &ch->dma.work_tag))
		goto error;
	if (bus_dmamem_alloc(ch->dma.work_tag, (void **)&ch->dma.work, 0,
	    &ch->dma.work_map))
		goto error;
	if (bus_dmamap_load(ch->dma.work_tag, ch->dma.work_map, ch->dma.work,
	    AHCI_WORK_SIZE, ahci_dmasetupc_cb, &dcba, 0) || dcba.error) {
		bus_dmamem_free(ch->dma.work_tag, ch->dma.work, ch->dma.work_map);
		goto error;
	}
	ch->dma.work_bus = dcba.maddr;
	/* FIS receive area. */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 4096, 0,
	    ch->dma.max_address, BUS_SPACE_MAXADDR,
	    NULL, NULL, 4096, 1, 4096,
	    0, NULL, NULL, &ch->dma.rfis_tag))
		goto error;
	if (bus_dmamem_alloc(ch->dma.rfis_tag, (void **)&ch->dma.rfis, 0,
	    &ch->dma.rfis_map))
		goto error;
	if (bus_dmamap_load(ch->dma.rfis_tag, ch->dma.rfis_map, ch->dma.rfis,
	    4096, ahci_dmasetupc_cb, &dcba, 0) || dcba.error) {
		bus_dmamem_free(ch->dma.rfis_tag, ch->dma.rfis, ch->dma.rfis_map);
		goto error;
	}
	ch->dma.rfis_bus = dcba.maddr;
	/* Data area. */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 2, 0,
	    ch->dma.max_address, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    AHCI_SG_ENTRIES * PAGE_SIZE * ch->numslots,
	    AHCI_SG_ENTRIES, AHCI_PRD_MAX,
	    0, busdma_lock_mutex, &ch->mtx, &ch->dma.data_tag)) {
		goto error;
	}
	return;

error:
	device_printf(dev, "WARNING - DMA initialization failed\n");
	ahci_dmafini(dev);
}

static void
ahci_dmasetupc_cb(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct ahci_dc_cb_args *dcba = (struct ahci_dc_cb_args *)xsc;

	if (!(dcba->error = error))
		dcba->maddr = segs[0].ds_addr;
}

static void
ahci_dmafini(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);

	if (ch->dma.data_tag) {
		bus_dma_tag_destroy(ch->dma.data_tag);
		ch->dma.data_tag = NULL;
	}
	if (ch->dma.rfis_bus) {
		bus_dmamap_unload(ch->dma.rfis_tag, ch->dma.rfis_map);
		bus_dmamem_free(ch->dma.rfis_tag, ch->dma.rfis, ch->dma.rfis_map);
		ch->dma.rfis_bus = 0;
		ch->dma.rfis_map = NULL;
		ch->dma.rfis = NULL;
	}
	if (ch->dma.work_bus) {
		bus_dmamap_unload(ch->dma.work_tag, ch->dma.work_map);
		bus_dmamem_free(ch->dma.work_tag, ch->dma.work, ch->dma.work_map);
		ch->dma.work_bus = 0;
		ch->dma.work_map = NULL;
		ch->dma.work = NULL;
	}
	if (ch->dma.work_tag) {
		bus_dma_tag_destroy(ch->dma.work_tag);
		ch->dma.work_tag = NULL;
	}
}

static void
ahci_slotsalloc(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	int i;

	/* Alloc and setup command/dma slots */
	bzero(ch->slot, sizeof(ch->slot));
	for (i = 0; i < ch->numslots; i++) {
		struct ahci_slot *slot = &ch->slot[i];

		slot->dev = dev;
		slot->slot = i;
		slot->state = AHCI_SLOT_EMPTY;
		slot->ccb = NULL;
		callout_init_mtx(&slot->timeout, &ch->mtx, 0);

		if (bus_dmamap_create(ch->dma.data_tag, 0, &slot->dma.data_map))
			device_printf(ch->dev, "FAILURE - create data_map\n");
	}
}

static void
ahci_slotsfree(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	int i;

	/* Free all dma slots */
	for (i = 0; i < ch->numslots; i++) {
		struct ahci_slot *slot = &ch->slot[i];

		callout_drain(&slot->timeout);
		if (slot->dma.data_map) {
			bus_dmamap_destroy(ch->dma.data_tag, slot->dma.data_map);
			slot->dma.data_map = NULL;
		}
	}
}

static void
ahci_phy_check_events(device_t dev, u_int32_t serr)
{
	struct ahci_channel *ch = device_get_softc(dev);

	if ((serr & ATA_SE_PHY_CHANGED) && (ch->pm_level == 0)) {
		u_int32_t status = ATA_INL(ch->r_mem, AHCI_P_SSTS);
		if (((status & ATA_SS_DET_MASK) == ATA_SS_DET_PHY_ONLINE) &&
		    ((status & ATA_SS_SPD_MASK) != ATA_SS_SPD_NO_SPEED) &&
		    ((status & ATA_SS_IPM_MASK) == ATA_SS_IPM_ACTIVE)) {
			if (bootverbose)
				device_printf(dev, "CONNECT requested\n");
			ahci_reset(dev);
		} else {
			if (bootverbose)
				device_printf(dev, "DISCONNECT requested\n");
			ch->devices = 0;
		}
	}
}

static void
ahci_notify_events(device_t dev, u_int32_t status)
{
	struct ahci_channel *ch = device_get_softc(dev);
	struct cam_path *dpath;
	int i;

	ATA_OUTL(ch->r_mem, AHCI_P_SNTF, status);
	if (bootverbose)
		device_printf(dev, "SNTF 0x%04x\n", status);
	for (i = 0; i < 16; i++) {
		if ((status & (1 << i)) == 0)
			continue;
		if (xpt_create_path(&dpath, NULL,
		    xpt_path_path_id(ch->path), i, 0) == CAM_REQ_CMP) {
			xpt_async(AC_SCSI_AEN, dpath, NULL);
			xpt_free_path(dpath);
		}
	}
}

static void
ahci_ch_intr_locked(void *data)
{
	device_t dev = (device_t)data;
	struct ahci_channel *ch = device_get_softc(dev);

	mtx_lock(&ch->mtx);
	ahci_ch_intr(data);
	mtx_unlock(&ch->mtx);
}

static void
ahci_ch_pm(void *arg)
{
	device_t dev = (device_t)arg;
	struct ahci_channel *ch = device_get_softc(dev);
	uint32_t work;

	if (ch->numrslots != 0)
		return;
	work = ATA_INL(ch->r_mem, AHCI_P_CMD);
	if (ch->pm_level == 4)
		work |= AHCI_P_CMD_PARTIAL;
	else
		work |= AHCI_P_CMD_SLUMBER;
	ATA_OUTL(ch->r_mem, AHCI_P_CMD, work);
}

static void
ahci_ch_intr(void *data)
{
	device_t dev = (device_t)data;
	struct ahci_channel *ch = device_get_softc(dev);
	uint32_t istatus, sstatus, cstatus, serr = 0, sntf = 0, ok, err;
	enum ahci_err_type et;
	int i, ccs, ncq_err = 0;

	/* Read and clear interrupt statuses. */
	istatus = ATA_INL(ch->r_mem, AHCI_P_IS);
	if (istatus == 0)
		return;
	ATA_OUTL(ch->r_mem, AHCI_P_IS, istatus);
	/* Read command statuses. */
	sstatus = ATA_INL(ch->r_mem, AHCI_P_SACT);
	cstatus = ATA_INL(ch->r_mem, AHCI_P_CI);
	if ((istatus & AHCI_P_IX_SDB) && (ch->caps & AHCI_CAP_SSNTF))
		sntf = ATA_INL(ch->r_mem, AHCI_P_SNTF);
	/* Process PHY events */
	if (istatus & (AHCI_P_IX_PC | AHCI_P_IX_PRC | AHCI_P_IX_OF |
	    AHCI_P_IX_IF | AHCI_P_IX_HBD | AHCI_P_IX_HBF | AHCI_P_IX_TFE)) {
		serr = ATA_INL(ch->r_mem, AHCI_P_SERR);
		if (serr) {
			ATA_OUTL(ch->r_mem, AHCI_P_SERR, serr);
			ahci_phy_check_events(dev, serr);
		}
	}
	/* Process command errors */
	if (istatus & (AHCI_P_IX_OF | AHCI_P_IX_IF |
	    AHCI_P_IX_HBD | AHCI_P_IX_HBF | AHCI_P_IX_TFE)) {
//device_printf(dev, "%s ERROR is %08x cs %08x ss %08x rs %08x tfd %02x serr %08x\n",
//    __func__, istatus, cstatus, sstatus, ch->rslots, ATA_INL(ch->r_mem, AHCI_P_TFD),
//    serr);
		ccs = (ATA_INL(ch->r_mem, AHCI_P_CMD) & AHCI_P_CMD_CCS_MASK)
		    >> AHCI_P_CMD_CCS_SHIFT;
		err = ch->rslots & (cstatus | sstatus);
		/* Kick controller into sane state */
		ahci_stop(dev);
		ahci_start(dev);
	} else {
		ccs = 0;
		err = 0;
	}
	/* Complete all successfull commands. */
	ok = ch->rslots & ~(cstatus | sstatus);
	for (i = 0; i < ch->numslots; i++) {
		if ((ok >> i) & 1)
			ahci_end_transaction(&ch->slot[i], AHCI_ERR_NONE);
	}
	/* On error, complete the rest of commands with error statuses. */
	if (err) {
		if (ch->frozen) {
			union ccb *fccb = ch->frozen;
			ch->frozen = NULL;
			fccb->ccb_h.status = CAM_REQUEUE_REQ | CAM_RELEASE_SIMQ;
			if (!(fccb->ccb_h.status & CAM_DEV_QFRZN)) {
				xpt_freeze_devq(fccb->ccb_h.path, 1);
				fccb->ccb_h.status |= CAM_DEV_QFRZN;
			}
			xpt_done(fccb);
		}
		for (i = 0; i < ch->numslots; i++) {
			/* XXX: reqests in loading state. */
			if (((err >> i) & 1) == 0)
				continue;
			if (istatus & AHCI_P_IX_TFE) {
				/* Task File Error */
				if (ch->numtslots == 0) {
					/* Untagged operation. */
					if (i == ccs)
						et = AHCI_ERR_TFE;
					else
						et = AHCI_ERR_INNOCENT;
				} else {
					/* Tagged operation. */
					et = AHCI_ERR_NCQ;
					ncq_err = 1;
				}
			} else if (istatus & AHCI_P_IX_IF) {
				if (ch->numtslots == 0 && i != ccs)
					et = AHCI_ERR_INNOCENT;
				else
					et = AHCI_ERR_SATA;
			} else
				et = AHCI_ERR_INVALID;
			ahci_end_transaction(&ch->slot[i], et);
		}
		if (ncq_err)
			ahci_issue_read_log(dev);
	}
	/* Process NOTIFY events */
	if (sntf)
		ahci_notify_events(dev, sntf);
}

/* Must be called with channel locked. */
static int
ahci_check_collision(device_t dev, union ccb *ccb)
{
	struct ahci_channel *ch = device_get_softc(dev);

	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & CAM_ATAIO_FPDMA)) {
		/* Tagged command while untagged are active. */
		if (ch->numrslots != 0 && ch->numtslots == 0)
			return (1);
		/* Tagged command while tagged to other target is active. */
		if (ch->numtslots != 0 &&
		    ch->taggedtarget != ccb->ccb_h.target_id)
			return (1);
	} else {
		/* Untagged command while tagged are active. */
		if (ch->numrslots != 0 && ch->numtslots != 0)
			return (1);
	}
	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & (CAM_ATAIO_CONTROL | CAM_ATAIO_NEEDRESULT))) {
		/* Atomic command while anything active. */
		if (ch->numrslots != 0)
			return (1);
	}
       /* We have some atomic command running. */
       if (ch->aslots != 0)
               return (1);
	return (0);
}

/* Must be called with channel locked. */
static void
ahci_begin_transaction(device_t dev, union ccb *ccb)
{
	struct ahci_channel *ch = device_get_softc(dev);
	struct ahci_slot *slot;
	int tag;

	/* Choose empty slot. */
	tag = ch->lastslot;
	while (ch->slot[tag].state != AHCI_SLOT_EMPTY) {
		if (++tag >= ch->numslots)
			tag = 0;
		KASSERT(tag != ch->lastslot, ("ahci: ALL SLOTS BUSY!"));
	}
	ch->lastslot = tag;
	/* Occupy chosen slot. */
	slot = &ch->slot[tag];
	slot->ccb = ccb;
	/* Stop PM timer. */
	if (ch->numrslots == 0 && ch->pm_level > 3)
		callout_stop(&ch->pm_timer);
	/* Update channel stats. */
	ch->numrslots++;
	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & CAM_ATAIO_FPDMA)) {
		ch->numtslots++;
		ch->taggedtarget = ccb->ccb_h.target_id;
	}
	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & (CAM_ATAIO_CONTROL | CAM_ATAIO_NEEDRESULT)))
		ch->aslots |= (1 << slot->slot);
	slot->dma.nsegs = 0;
	/* If request moves data, setup and load SG list */
	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		void *buf;
		bus_size_t size;

		slot->state = AHCI_SLOT_LOADING;
		if (ccb->ccb_h.func_code == XPT_ATA_IO) {
			buf = ccb->ataio.data_ptr;
			size = ccb->ataio.dxfer_len;
		} else {
			buf = ccb->csio.data_ptr;
			size = ccb->csio.dxfer_len;
		}
		bus_dmamap_load(ch->dma.data_tag, slot->dma.data_map,
		    buf, size, ahci_dmasetprd, slot, 0);
	} else
		ahci_execute_transaction(slot);
}

/* Locked by busdma engine. */
static void
ahci_dmasetprd(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{    
	struct ahci_slot *slot = arg;
	struct ahci_channel *ch = device_get_softc(slot->dev);
	struct ahci_cmd_tab *ctp;
	struct ahci_dma_prd *prd;
	int i;

	if (error) {
		device_printf(slot->dev, "DMA load error\n");
		ahci_end_transaction(slot, AHCI_ERR_INVALID);
		return;
	}
	KASSERT(nsegs <= AHCI_SG_ENTRIES, ("too many DMA segment entries\n"));
	/* Get a piece of the workspace for this request */
	ctp = (struct ahci_cmd_tab *)
		(ch->dma.work + AHCI_CT_OFFSET + (AHCI_CT_SIZE * slot->slot));
	/* Fill S/G table */
	prd = &ctp->prd_tab[0];
	for (i = 0; i < nsegs; i++) {
		prd[i].dba = htole64(segs[i].ds_addr);
		prd[i].dbc = htole32((segs[i].ds_len - 1) & AHCI_PRD_MASK);
	}
	slot->dma.nsegs = nsegs;
	bus_dmamap_sync(ch->dma.data_tag, slot->dma.data_map,
	    ((slot->ccb->ccb_h.flags & CAM_DIR_IN) ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE));
	ahci_execute_transaction(slot);
}

/* Must be called with channel locked. */
static void
ahci_execute_transaction(struct ahci_slot *slot)
{
	device_t dev = slot->dev;
	struct ahci_channel *ch = device_get_softc(dev);
	struct ahci_cmd_tab *ctp;
	struct ahci_cmd_list *clp;
	union ccb *ccb = slot->ccb;
	int port = ccb->ccb_h.target_id & 0x0f;
	int fis_size;

	/* Get a piece of the workspace for this request */
	ctp = (struct ahci_cmd_tab *)
		(ch->dma.work + AHCI_CT_OFFSET + (AHCI_CT_SIZE * slot->slot));
	/* Setup the FIS for this request */
	if (!(fis_size = ahci_setup_fis(ctp, ccb, slot->slot))) {
		device_printf(ch->dev, "Setting up SATA FIS failed\n");
		ahci_end_transaction(slot, AHCI_ERR_INVALID);
		return;
	}
	/* Setup the command list entry */
	clp = (struct ahci_cmd_list *)
	    (ch->dma.work + AHCI_CL_OFFSET + (AHCI_CL_SIZE * slot->slot));
	clp->prd_length = slot->dma.nsegs;
	clp->cmd_flags = (ccb->ccb_h.flags & CAM_DIR_OUT ? AHCI_CMD_WRITE : 0) |
		     (ccb->ccb_h.func_code == XPT_SCSI_IO ?
		      (AHCI_CMD_ATAPI | AHCI_CMD_PREFETCH) : 0) |
		     (fis_size / sizeof(u_int32_t)) |
		     (port << 12);
	/* Special handling for Soft Reset command. */
	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & CAM_ATAIO_CONTROL) &&
	    (ccb->ataio.cmd.control & ATA_A_RESET)) {
		/* Kick controller into sane state */
		ahci_stop(dev);
		ahci_clo(dev);
		ahci_start(dev);
		clp->cmd_flags |= AHCI_CMD_RESET | AHCI_CMD_CLR_BUSY;
	}
	clp->bytecount = 0;
	clp->cmd_table_phys = htole64(ch->dma.work_bus + AHCI_CT_OFFSET +
				  (AHCI_CT_SIZE * slot->slot));
	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ch->dma.rfis_tag, ch->dma.rfis_map,
	    BUS_DMASYNC_PREREAD);
	/* Set ACTIVE bit for NCQ commands. */
	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & CAM_ATAIO_FPDMA)) {
		ATA_OUTL(ch->r_mem, AHCI_P_SACT, 1 << slot->slot);
	}
	/* Issue command to the controller. */
	slot->state = AHCI_SLOT_RUNNING;
	ch->rslots |= (1 << slot->slot);
	ATA_OUTL(ch->r_mem, AHCI_P_CI, (1 << slot->slot));
	/* Device reset commands doesn't interrupt. Poll them. */
	if (ccb->ccb_h.func_code == XPT_ATA_IO &&
	    (ccb->ataio.cmd.command == ATA_DEVICE_RESET ||
	    (ccb->ataio.cmd.flags & CAM_ATAIO_CONTROL))) {
		int count, timeout = ccb->ccb_h.timeout;
		enum ahci_err_type et = AHCI_ERR_NONE;

		for (count = 0; count < timeout; count++) {
			DELAY(1000);
			if (!(ATA_INL(ch->r_mem, AHCI_P_CI) & (1 << slot->slot)))
				break;
			if (ATA_INL(ch->r_mem, AHCI_P_TFD) & ATA_S_ERROR) {
				device_printf(ch->dev,
				    "Poll error on slot %d, TFD: %04x\n",
				    slot->slot, ATA_INL(ch->r_mem, AHCI_P_TFD));
				et = AHCI_ERR_TFE;
				break;
			}
		}
		if (timeout && (count >= timeout)) {
			device_printf(ch->dev,
			    "Poll timeout on slot %d\n", slot->slot);
			et = AHCI_ERR_TIMEOUT;
		}
		if (et != AHCI_ERR_NONE) {
			/* Kick controller into sane state */
			ahci_stop(ch->dev);
			ahci_start(ch->dev);
		}
		ahci_end_transaction(slot, et);
		return;
	}
	/* Start command execution timeout */
	callout_reset(&slot->timeout, (int)ccb->ccb_h.timeout * hz / 2000,
	    (timeout_t*)ahci_timeout, slot);
	return;
}

/* Locked by callout mechanism. */
static void
ahci_timeout(struct ahci_slot *slot)
{
	device_t dev = slot->dev;
	struct ahci_channel *ch = device_get_softc(dev);
	uint32_t sstatus;
	int ccs;
	int i;

	/* Check for stale timeout. */
	if (slot->state < AHCI_SLOT_RUNNING)
		return;

	/* Check if slot was not being executed last time we checked. */
	if (slot->state < AHCI_SLOT_EXECUTING) {
		/* Check if slot started executing. */
		sstatus = ATA_INL(ch->r_mem, AHCI_P_SACT);
		ccs = (ATA_INL(ch->r_mem, AHCI_P_CMD) & AHCI_P_CMD_CCS_MASK)
		    >> AHCI_P_CMD_CCS_SHIFT;
		if ((sstatus & (1 << slot->slot)) != 0 || ccs == slot->slot)
			slot->state = AHCI_SLOT_EXECUTING;

		callout_reset(&slot->timeout,
		    (int)slot->ccb->ccb_h.timeout * hz / 2000,
		    (timeout_t*)ahci_timeout, slot);
		return;
	}

	device_printf(dev, "Timeout on slot %d\n", slot->slot);
	device_printf(dev, "is %08x cs %08x ss %08x rs %08x tfd %02x serr %08x\n",
	    ATA_INL(ch->r_mem, AHCI_P_IS), ATA_INL(ch->r_mem, AHCI_P_CI),
	    ATA_INL(ch->r_mem, AHCI_P_SACT), ch->rslots,
	    ATA_INL(ch->r_mem, AHCI_P_TFD), ATA_INL(ch->r_mem, AHCI_P_SERR));
	/* Kick controller into sane state. */
	ahci_stop(ch->dev);
	ahci_start(ch->dev);

	/* Handle frozen command. */
	if (ch->frozen) {
		union ccb *fccb = ch->frozen;
		ch->frozen = NULL;
		fccb->ccb_h.status = CAM_REQUEUE_REQ | CAM_RELEASE_SIMQ;
		if (!(fccb->ccb_h.status & CAM_DEV_QFRZN)) {
			xpt_freeze_devq(fccb->ccb_h.path, 1);
			fccb->ccb_h.status |= CAM_DEV_QFRZN;
		}
		xpt_done(fccb);
	}
	/* Handle command with timeout. */
	ahci_end_transaction(&ch->slot[slot->slot], AHCI_ERR_TIMEOUT);
	/* Handle the rest of commands. */
	for (i = 0; i < ch->numslots; i++) {
		/* Do we have a running request on slot? */
		if (ch->slot[i].state < AHCI_SLOT_RUNNING)
			continue;
		ahci_end_transaction(&ch->slot[i], AHCI_ERR_INNOCENT);
	}
}

/* Must be called with channel locked. */
static void
ahci_end_transaction(struct ahci_slot *slot, enum ahci_err_type et)
{
	device_t dev = slot->dev;
	struct ahci_channel *ch = device_get_softc(dev);
	union ccb *ccb = slot->ccb;

	bus_dmamap_sync(ch->dma.work_tag, ch->dma.work_map,
	    BUS_DMASYNC_POSTWRITE);
	/* Read result registers to the result struct
	 * May be incorrect if several commands finished same time,
	 * so read only when sure or have to.
	 */
	if (ccb->ccb_h.func_code == XPT_ATA_IO) {
		struct ata_res *res = &ccb->ataio.res;

		if ((et == AHCI_ERR_TFE) ||
		    (ccb->ataio.cmd.flags & CAM_ATAIO_NEEDRESULT)) {
			u_int8_t *fis = ch->dma.rfis + 0x40;
			uint16_t tfd = ATA_INL(ch->r_mem, AHCI_P_TFD);

			bus_dmamap_sync(ch->dma.rfis_tag, ch->dma.rfis_map,
			    BUS_DMASYNC_POSTREAD);
			res->status = tfd;
			res->error = tfd >> 8;
			res->lba_low = fis[4];
			res->lba_mid = fis[5];
			res->lba_high = fis[6];
			res->device = fis[7];
			res->lba_low_exp = fis[8];
			res->lba_mid_exp = fis[9];
			res->lba_high_exp = fis[10];
			res->sector_count = fis[12];
			res->sector_count_exp = fis[13];
		} else
			bzero(res, sizeof(*res));
	}
	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmamap_sync(ch->dma.data_tag, slot->dma.data_map,
		    (ccb->ccb_h.flags & CAM_DIR_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ch->dma.data_tag, slot->dma.data_map);
	}
	/* In case of error, freeze device for proper recovery. */
	if ((et != AHCI_ERR_NONE) && (!ch->readlog) &&
	    !(ccb->ccb_h.status & CAM_DEV_QFRZN)) {
		xpt_freeze_devq(ccb->ccb_h.path, 1);
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
	}
	/* Set proper result status. */
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	switch (et) {
	case AHCI_ERR_NONE:
		ccb->ccb_h.status |= CAM_REQ_CMP;
		if (ccb->ccb_h.func_code == XPT_SCSI_IO)
			ccb->csio.scsi_status = SCSI_STATUS_OK;
		break;
	case AHCI_ERR_INVALID:
		ccb->ccb_h.status |= CAM_REQ_INVALID;
		break;
	case AHCI_ERR_INNOCENT:
		ccb->ccb_h.status |= CAM_REQUEUE_REQ;
		break;
	case AHCI_ERR_TFE:
	case AHCI_ERR_NCQ:
		if (ccb->ccb_h.func_code == XPT_SCSI_IO) {
			ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
			ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
		} else {
			ccb->ccb_h.status |= CAM_ATA_STATUS_ERROR;
		}
		break;
	case AHCI_ERR_SATA:
		if (!ch->readlog) {
			xpt_freeze_simq(ch->sim, 1);
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
			ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		}
		ccb->ccb_h.status |= CAM_UNCOR_PARITY;
		break;
	case AHCI_ERR_TIMEOUT:
		if (!ch->readlog) {
			xpt_freeze_simq(ch->sim, 1);
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
			ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		}
		ccb->ccb_h.status |= CAM_CMD_TIMEOUT;
		break;
	default:
		ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
	}
	/* Free slot. */
	ch->rslots &= ~(1 << slot->slot);
	ch->aslots &= ~(1 << slot->slot);
	slot->state = AHCI_SLOT_EMPTY;
	slot->ccb = NULL;
	/* Update channel stats. */
	ch->numrslots--;
	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & CAM_ATAIO_FPDMA)) {
		ch->numtslots--;
	}
	/* If it was first request of reset sequence and there is no error,
	 * proceed to second request. */
	if ((ccb->ccb_h.func_code == XPT_ATA_IO) &&
	    (ccb->ataio.cmd.flags & CAM_ATAIO_CONTROL) &&
	    (ccb->ataio.cmd.control & ATA_A_RESET) &&
	    et == AHCI_ERR_NONE) {
		ccb->ataio.cmd.control &= ~ATA_A_RESET;
		ahci_begin_transaction(dev, ccb);
		return;
	}
	/* If it was NCQ command error, put result on hold. */
	if (et == AHCI_ERR_NCQ) {
		ch->hold[slot->slot] = ccb;
	} else if (ch->readlog)	/* If it was our READ LOG command - process it. */
		ahci_process_read_log(dev, ccb);
	else
		xpt_done(ccb);
	/* Unfreeze frozen command. */
	if (ch->frozen && ch->numrslots == 0) {
		union ccb *fccb = ch->frozen;
		ch->frozen = NULL;
		ahci_begin_transaction(dev, fccb);
		xpt_release_simq(ch->sim, TRUE);
	}
	/* Start PM timer. */
	if (ch->numrslots == 0 && ch->pm_level > 3) {
		callout_schedule(&ch->pm_timer,
		    (ch->pm_level == 4) ? hz / 1000 : hz / 8);
	}
}

static void
ahci_issue_read_log(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	union ccb *ccb;
	struct ccb_ataio *ataio;
	int i;

	ch->readlog = 1;
	/* Find some holden command. */
	for (i = 0; i < ch->numslots; i++) {
		if (ch->hold[i])
			break;
	}
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		device_printf(dev, "Unable allocate READ LOG command");
		return; /* XXX */
	}
	ccb->ccb_h = ch->hold[i]->ccb_h;	/* Reuse old header. */
	ccb->ccb_h.func_code = XPT_ATA_IO;
	ccb->ccb_h.flags = CAM_DIR_IN;
	ccb->ccb_h.timeout = 1000;	/* 1s should be enough. */
	ataio = &ccb->ataio;
	ataio->data_ptr = malloc(512, M_AHCI, M_NOWAIT);
	if (ataio->data_ptr == NULL) {
		device_printf(dev, "Unable allocate memory for READ LOG command");
		return; /* XXX */
	}
	ataio->dxfer_len = 512;
	bzero(&ataio->cmd, sizeof(ataio->cmd));
	ataio->cmd.flags = CAM_ATAIO_48BIT;
	ataio->cmd.command = 0x2F;	/* READ LOG EXT */
	ataio->cmd.sector_count = 1;
	ataio->cmd.sector_count_exp = 0;
	ataio->cmd.lba_low = 0x10;
	ataio->cmd.lba_mid = 0;
	ataio->cmd.lba_mid_exp = 0;
	/* Freeze SIM while doing READ LOG EXT. */
	xpt_freeze_simq(ch->sim, 1);
	ahci_begin_transaction(dev, ccb);
}

static void
ahci_process_read_log(device_t dev, union ccb *ccb)
{
	struct ahci_channel *ch = device_get_softc(dev);
	uint8_t *data;
	struct ata_res *res;
	int i;

	ch->readlog = 0;

	data = ccb->ataio.data_ptr;
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP &&
	    (data[0] & 0x80) == 0) {
		for (i = 0; i < ch->numslots; i++) {
			if (!ch->hold[i])
				continue;
			if ((data[0] & 0x1F) == i) {
				res = &ch->hold[i]->ataio.res;
				res->status = data[2];
				res->error = data[3];
				res->lba_low = data[4];
				res->lba_mid = data[5];
				res->lba_high = data[6];
				res->device = data[7];
				res->lba_low_exp = data[8];
				res->lba_mid_exp = data[9];
				res->lba_high_exp = data[10];
				res->sector_count = data[12];
				res->sector_count_exp = data[13];
			} else {
				ch->hold[i]->ccb_h.status &= ~CAM_STATUS_MASK;
				ch->hold[i]->ccb_h.status |= CAM_REQUEUE_REQ;
			}
			xpt_done(ch->hold[i]);
			ch->hold[i] = NULL;
		}
	} else {
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)
			device_printf(dev, "Error while READ LOG EXT\n");
		else if ((data[0] & 0x80) == 0) {
			device_printf(dev, "Non-queued command error in READ LOG EXT\n");
		}
		for (i = 0; i < ch->numslots; i++) {
			if (!ch->hold[i])
				continue;
			xpt_done(ch->hold[i]);
			ch->hold[i] = NULL;
		}
	}
	free(ccb->ataio.data_ptr, M_AHCI);
	xpt_free_ccb(ccb);
	xpt_release_simq(ch->sim, TRUE);
}

static void
ahci_start(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	u_int32_t cmd;

	/* Clear SATA error register */
	ATA_OUTL(ch->r_mem, AHCI_P_SERR, 0xFFFFFFFF);
	/* Clear any interrupts pending on this channel */
	ATA_OUTL(ch->r_mem, AHCI_P_IS, 0xFFFFFFFF);
	/* Start operations on this channel */
	cmd = ATA_INL(ch->r_mem, AHCI_P_CMD);
	ATA_OUTL(ch->r_mem, AHCI_P_CMD, cmd | AHCI_P_CMD_ST |
	    (ch->pm_present ? AHCI_P_CMD_PMA : 0));
}

static void
ahci_stop(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	u_int32_t cmd;
	int timeout;

	/* Kill all activity on this channel */
	cmd = ATA_INL(ch->r_mem, AHCI_P_CMD);
	ATA_OUTL(ch->r_mem, AHCI_P_CMD, cmd & ~AHCI_P_CMD_ST);
	/* Wait for activity stop. */
	timeout = 0;
	do {
		DELAY(1000);
		if (timeout++ > 1000) {
			device_printf(dev, "stopping AHCI engine failed\n");
			break;
		}
	} while (ATA_INL(ch->r_mem, AHCI_P_CMD) & AHCI_P_CMD_CR);
}

static void
ahci_clo(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	u_int32_t cmd;
	int timeout;

	/* Issue Command List Override if supported */ 
	if (ch->caps & AHCI_CAP_SCLO) {
		cmd = ATA_INL(ch->r_mem, AHCI_P_CMD);
		cmd |= AHCI_P_CMD_CLO;
		ATA_OUTL(ch->r_mem, AHCI_P_CMD, cmd);
		timeout = 0;
		do {
			DELAY(1000);
			if (timeout++ > 1000) {
			    device_printf(dev, "executing CLO failed\n");
			    break;
			}
		} while (ATA_INL(ch->r_mem, AHCI_P_CMD) & AHCI_P_CMD_CLO);
	}
}

static void
ahci_stop_fr(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	u_int32_t cmd;
	int timeout;

	/* Kill all FIS reception on this channel */
	cmd = ATA_INL(ch->r_mem, AHCI_P_CMD);
	ATA_OUTL(ch->r_mem, AHCI_P_CMD, cmd & ~AHCI_P_CMD_FRE);
	/* Wait for FIS reception stop. */
	timeout = 0;
	do {
		DELAY(1000);
		if (timeout++ > 1000) {
			device_printf(dev, "stopping AHCI FR engine failed\n");
			break;
		}
	} while (ATA_INL(ch->r_mem, AHCI_P_CMD) & AHCI_P_CMD_FR);
}

static void
ahci_start_fr(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	u_int32_t cmd;

	/* Start FIS reception on this channel */
	cmd = ATA_INL(ch->r_mem, AHCI_P_CMD);
	ATA_OUTL(ch->r_mem, AHCI_P_CMD, cmd | AHCI_P_CMD_FRE);
}

static int
ahci_wait_ready(device_t dev, int t)
{
	struct ahci_channel *ch = device_get_softc(dev);
	int timeout = 0;
	uint32_t val;

	while ((val = ATA_INL(ch->r_mem, AHCI_P_TFD)) &
	    (ATA_S_BUSY | ATA_S_DRQ)) {
		DELAY(1000);
		if (timeout++ > t) {
			device_printf(dev, "port is not ready (timeout %dms) "
			    "tfd = %08x\n", t, val);
			return (EBUSY);
		}
	} 
	if (bootverbose)
		device_printf(dev, "ready wait time=%dms\n", timeout);
	return (0);
}

static void
ahci_reset(device_t dev)
{
	struct ahci_channel *ch = device_get_softc(dev);
	struct ahci_controller *ctlr = device_get_softc(device_get_parent(dev));
	int i;

	if (bootverbose)
		device_printf(dev, "AHCI reset...\n");
	/* Requeue freezed command. */
	if (ch->frozen) {
		union ccb *fccb = ch->frozen;
		ch->frozen = NULL;
		fccb->ccb_h.status = CAM_REQUEUE_REQ | CAM_RELEASE_SIMQ;
		if (!(fccb->ccb_h.status & CAM_DEV_QFRZN)) {
			xpt_freeze_devq(fccb->ccb_h.path, 1);
			fccb->ccb_h.status |= CAM_DEV_QFRZN;
		}
		xpt_done(fccb);
	}
	/* Kill the engine and requeue all running commands. */
	ahci_stop(dev);
	for (i = 0; i < ch->numslots; i++) {
		/* Do we have a running request on slot? */
		if (ch->slot[i].state < AHCI_SLOT_RUNNING)
			continue;
		/* XXX; Commands in loading state. */
		ahci_end_transaction(&ch->slot[i], AHCI_ERR_INNOCENT);
	}
	/* Tell the XPT about the event */
	xpt_async(AC_BUS_RESET, ch->path, NULL);
	/* Disable port interrupts */
	ATA_OUTL(ch->r_mem, AHCI_P_IE, 0);
	/* Reset and reconnect PHY, */
	if (!ahci_sata_phy_reset(dev, 0)) {
		if (bootverbose)
			device_printf(dev,
			    "AHCI reset done: phy reset found no device\n");
		ch->devices = 0;
		/* Enable wanted port interrupts */
		ATA_OUTL(ch->r_mem, AHCI_P_IE,
		    (AHCI_P_IX_CPD | AHCI_P_IX_PRC | AHCI_P_IX_PC));
		return;
	}
	/* Wait for clearing busy status. */
	if (ahci_wait_ready(dev, 10000)) {
		device_printf(dev, "device ready timeout\n");
		ahci_clo(dev);
	}
	ahci_start(dev);
	ch->devices = 1;
	/* Enable wanted port interrupts */
	ATA_OUTL(ch->r_mem, AHCI_P_IE,
	     (AHCI_P_IX_CPD | AHCI_P_IX_TFE | AHCI_P_IX_HBF |
	      AHCI_P_IX_HBD | AHCI_P_IX_IF | AHCI_P_IX_OF |
	      ((ch->pm_level == 0) ? AHCI_P_IX_PRC | AHCI_P_IX_PC : 0) |
	      AHCI_P_IX_DP | AHCI_P_IX_UF | (ctlr->ccc ? 0 : AHCI_P_IX_SDB) |
	      AHCI_P_IX_DS | AHCI_P_IX_PS | (ctlr->ccc ? 0 : AHCI_P_IX_DHR)));
	if (bootverbose)
		device_printf(dev, "AHCI reset done: device found\n");
}

static int
ahci_setup_fis(struct ahci_cmd_tab *ctp, union ccb *ccb, int tag)
{
	u_int8_t *fis = &ctp->cfis[0];

	bzero(ctp->cfis, 64);
	fis[0] = 0x27;  		/* host to device */
	fis[1] = (ccb->ccb_h.target_id & 0x0f);
	if (ccb->ccb_h.func_code == XPT_SCSI_IO) {
		fis[1] |= 0x80;
		fis[2] = ATA_PACKET_CMD;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE)
			fis[3] = ATA_F_DMA;
		else {
			fis[5] = ccb->csio.dxfer_len;
		        fis[6] = ccb->csio.dxfer_len >> 8;
		}
		fis[7] = ATA_D_LBA;
		fis[15] = ATA_A_4BIT;
		bzero(ctp->acmd, 32);
		bcopy((ccb->ccb_h.flags & CAM_CDB_POINTER) ?
		    ccb->csio.cdb_io.cdb_ptr : ccb->csio.cdb_io.cdb_bytes,
		    ctp->acmd, ccb->csio.cdb_len);
	} else if ((ccb->ataio.cmd.flags & CAM_ATAIO_CONTROL) == 0) {
		fis[1] |= 0x80;
		fis[2] = ccb->ataio.cmd.command;
		fis[3] = ccb->ataio.cmd.features;
		fis[4] = ccb->ataio.cmd.lba_low;
		fis[5] = ccb->ataio.cmd.lba_mid;
		fis[6] = ccb->ataio.cmd.lba_high;
		fis[7] = ccb->ataio.cmd.device;
		fis[8] = ccb->ataio.cmd.lba_low_exp;
		fis[9] = ccb->ataio.cmd.lba_mid_exp;
		fis[10] = ccb->ataio.cmd.lba_high_exp;
		fis[11] = ccb->ataio.cmd.features_exp;
		if (ccb->ataio.cmd.flags & CAM_ATAIO_FPDMA) {
			fis[12] = tag << 3;
			fis[13] = 0;
		} else {
			fis[12] = ccb->ataio.cmd.sector_count;
			fis[13] = ccb->ataio.cmd.sector_count_exp;
		}
		fis[15] = ATA_A_4BIT;
	} else {
		fis[15] = ccb->ataio.cmd.control;
	}
	return (20);
}

static int
ahci_sata_connect(struct ahci_channel *ch)
{
	u_int32_t status;
	int timeout;

	/* Wait up to 100ms for "connect well" */
	for (timeout = 0; timeout < 100 ; timeout++) {
		status = ATA_INL(ch->r_mem, AHCI_P_SSTS);
		if (((status & ATA_SS_DET_MASK) == ATA_SS_DET_PHY_ONLINE) &&
		    ((status & ATA_SS_SPD_MASK) != ATA_SS_SPD_NO_SPEED) &&
		    ((status & ATA_SS_IPM_MASK) == ATA_SS_IPM_ACTIVE))
			break;
		if ((status & ATA_SS_DET_MASK) == ATA_SS_DET_PHY_OFFLINE) {
			if (bootverbose) {
				device_printf(ch->dev, "SATA offline status=%08x\n",
				    status);
			}
			return (0);
		}
		DELAY(1000);
	}
	if (timeout >= 100) {
		if (bootverbose) {
			device_printf(ch->dev, "SATA connect timeout status=%08x\n",
			    status);
		}
		return (0);
	}
	if (bootverbose) {
		device_printf(ch->dev, "SATA connect time=%dms status=%08x\n",
		    timeout, status);
	}
	/* Clear SATA error register */
	ATA_OUTL(ch->r_mem, AHCI_P_SERR, 0xffffffff);
	return (1);
}

static int
ahci_sata_phy_reset(device_t dev, int quick)
{
	struct ahci_channel *ch = device_get_softc(dev);
	uint32_t val;

	if (quick) {
		val = ATA_INL(ch->r_mem, AHCI_P_SCTL);
		if ((val & ATA_SC_DET_MASK) == ATA_SC_DET_IDLE)
			return (ahci_sata_connect(ch));
	}

	if (bootverbose)
		device_printf(dev, "hardware reset ...\n");
	if (ch->sata_rev == 1)
		val = ATA_SC_SPD_SPEED_GEN1;
	else if (ch->sata_rev == 2)
		val = ATA_SC_SPD_SPEED_GEN2;
	else if (ch->sata_rev == 3)
		val = ATA_SC_SPD_SPEED_GEN3;
	else
		val = 0;
	ATA_OUTL(ch->r_mem, AHCI_P_SCTL,
	    ATA_SC_DET_RESET | val |
	    ATA_SC_IPM_DIS_PARTIAL | ATA_SC_IPM_DIS_SLUMBER);
	DELAY(5000);
	ATA_OUTL(ch->r_mem, AHCI_P_SCTL,
	    ATA_SC_DET_IDLE | val | ((ch->pm_level > 0) ? 0 :
	    (ATA_SC_IPM_DIS_PARTIAL | ATA_SC_IPM_DIS_SLUMBER)));
	DELAY(5000);
	return (ahci_sata_connect(ch));
}

static void
ahciaction(struct cam_sim *sim, union ccb *ccb)
{
	device_t dev;
	struct ahci_channel *ch;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("ahciaction func_code=%x\n",
	    ccb->ccb_h.func_code));

	ch = (struct ahci_channel *)cam_sim_softc(sim);
	dev = ch->dev;
	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_ATA_IO:	/* Execute the requested I/O operation */
	case XPT_SCSI_IO:
		if (ch->devices == 0) {
			ccb->ccb_h.status = CAM_SEL_TIMEOUT;
			xpt_done(ccb);
			break;
		}
		/* Check for command collision. */
		if (ahci_check_collision(dev, ccb)) {
			/* Freeze command. */
			ch->frozen = ccb;
			/* We have only one frozen slot, so freeze simq also. */
			xpt_freeze_simq(ch->sim, 1);
			return;
		}
		ahci_begin_transaction(dev, ccb);
		break;
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
	{
		struct	ccb_trans_settings *cts = &ccb->cts;

		if (cts->xport_specific.sata.valid & CTS_SATA_VALID_PM) {
			ch->pm_present = cts->xport_specific.sata.pm_present;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	/* Get default/user set transfer settings for the target */
	{
		struct	ccb_trans_settings *cts = &ccb->cts;
		uint32_t status;

		cts->protocol = PROTO_ATA;
		cts->protocol_version = PROTO_VERSION_UNSPECIFIED;
		cts->transport = XPORT_SATA;
		cts->transport_version = XPORT_VERSION_UNSPECIFIED;
		cts->proto_specific.valid = 0;
		cts->xport_specific.sata.valid = 0;
		if (cts->type == CTS_TYPE_CURRENT_SETTINGS)
			status = ATA_INL(ch->r_mem, AHCI_P_SSTS) & ATA_SS_SPD_MASK;
		else
			status = ATA_INL(ch->r_mem, AHCI_P_SCTL) & ATA_SC_SPD_MASK;
		if (status & ATA_SS_SPD_GEN3) {
			cts->xport_specific.sata.bitrate = 600000;
			cts->xport_specific.sata.valid |= CTS_SATA_VALID_SPEED;
		} else if (status & ATA_SS_SPD_GEN2) {
			cts->xport_specific.sata.bitrate = 300000;
			cts->xport_specific.sata.valid |= CTS_SATA_VALID_SPEED;
		} else if (status & ATA_SS_SPD_GEN1) {
			cts->xport_specific.sata.bitrate = 150000;
			cts->xport_specific.sata.valid |= CTS_SATA_VALID_SPEED;
		}
		if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
			cts->xport_specific.sata.pm_present =
			    (ATA_INL(ch->r_mem, AHCI_P_CMD) & AHCI_P_CMD_PMA) ?
			    1 : 0;
		} else {
			cts->xport_specific.sata.pm_present = ch->pm_present;
		}
		cts->xport_specific.sata.valid |= CTS_SATA_VALID_PM;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
#if 0
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		uint32_t size_mb;
		uint32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		if (size_mb >= 1024 && (aha->extended_trans != 0)) {
			if (size_mb >= 2048) {
				ccg->heads = 255;
				ccg->secs_per_track = 63;
			} else {
				ccg->heads = 128;
				ccg->secs_per_track = 32;
			}
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
#endif
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
		ahci_reset(dev);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE | PI_TAG_ABLE;
		if (ch->caps & AHCI_CAP_SPM)
			cpi->hba_inquiry |= PI_SATAPM;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_SEQSCAN;
		cpi->hba_eng_cnt = 0;
		if (ch->caps & AHCI_CAP_SPM)
			cpi->max_target = 15;
		else
			cpi->max_target = 0;
		cpi->max_lun = 0;
		cpi->initiator_id = 0;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 150000;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "AHCI", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->transport = XPORT_SATA;
		cpi->transport_version = XPORT_VERSION_UNSPECIFIED;
		cpi->protocol = PROTO_ATA;
		cpi->protocol_version = PROTO_VERSION_UNSPECIFIED;
		cpi->maxio = MAXPHYS;
		/* ATI SB600 can't handle 256 sectors with FPDMA (NCQ). */
		if (pci_get_devid(device_get_parent(dev)) == 0x43801002)
			cpi->maxio = min(cpi->maxio, 128 * 512);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

static void
ahcipoll(struct cam_sim *sim)
{
	struct ahci_channel *ch = (struct ahci_channel *)cam_sim_softc(sim);

	ahci_ch_intr(ch->dev);
}
