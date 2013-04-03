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
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/ctype.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <dev/pci/pcivar.h>
#include <ata_if.h>

#ifdef ATA_CAM
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#endif

#ifndef ATA_CAM
/* device structure */
static  d_ioctl_t       ata_ioctl;
static struct cdevsw ata_cdevsw = {
	.d_version =    D_VERSION,
	.d_flags =      D_NEEDGIANT, /* we need this as newbus isn't mpsafe */
	.d_ioctl =      ata_ioctl,
	.d_name =       "ata",
};
#endif

/* prototypes */
#ifndef ATA_CAM
static void ata_boot_attach(void);
static device_t ata_add_child(device_t, struct ata_device *, int);
#else
static void ataaction(struct cam_sim *sim, union ccb *ccb);
static void atapoll(struct cam_sim *sim);
#endif
static void ata_conn_event(void *, int);
#ifndef ATA_CAM
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);
#endif
static void ata_interrupt_locked(void *data);
#ifdef ATA_CAM
static void ata_periodic_poll(void *data);
#endif

/* global vars */
MALLOC_DEFINE(M_ATA, "ata_generic", "ATA driver generic layer");
int (*ata_raid_ioctl_func)(u_long cmd, caddr_t data) = NULL;
#ifndef ATA_CAM
struct intr_config_hook *ata_delayed_attach = NULL;
#endif
devclass_t ata_devclass;
uma_zone_t ata_request_zone;
uma_zone_t ata_composite_zone;
#ifndef ATA_CAM
int ata_wc = 1;
int ata_setmax = 0;
#endif
int ata_dma_check_80pin = 1;

/* local vars */
#ifndef ATA_CAM
static int ata_dma = 1;
static int atapi_dma = 1;
#endif

/* sysctl vars */
static SYSCTL_NODE(_hw, OID_AUTO, ata, CTLFLAG_RD, 0, "ATA driver parameters");
#ifndef ATA_CAM
TUNABLE_INT("hw.ata.ata_dma", &ata_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma, CTLFLAG_RDTUN, &ata_dma, 0,
	   "ATA disk DMA mode control");
#endif
TUNABLE_INT("hw.ata.ata_dma_check_80pin", &ata_dma_check_80pin);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma_check_80pin,
	   CTLFLAG_RW, &ata_dma_check_80pin, 1,
	   "Check for 80pin cable before setting ATA DMA mode");
#ifndef ATA_CAM
TUNABLE_INT("hw.ata.atapi_dma", &atapi_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, atapi_dma, CTLFLAG_RDTUN, &atapi_dma, 0,
	   "ATAPI device DMA mode control");
TUNABLE_INT("hw.ata.wc", &ata_wc);
SYSCTL_INT(_hw_ata, OID_AUTO, wc, CTLFLAG_RDTUN, &ata_wc, 0,
	   "ATA disk write caching");
TUNABLE_INT("hw.ata.setmax", &ata_setmax);
SYSCTL_INT(_hw_ata, OID_AUTO, setmax, CTLFLAG_RDTUN, &ata_setmax, 0,
	   "ATA disk set max native address");
#endif
#ifdef ATA_CAM
FEATURE(ata_cam, "ATA devices are accessed through the cam(4) driver");
#endif

/*
 * newbus device interface related functions
 */
int
ata_probe(device_t dev)
{
    return 0;
}

int
ata_attach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    int error, rid;
#ifdef ATA_CAM
    struct cam_devq *devq;
    const char *res;
    char buf[64];
    int i, mode;
#endif

    /* check that we have a virgin channel to attach */
    if (ch->r_irq)
	return EEXIST;

    /* initialize the softc basics */
    ch->dev = dev;
    ch->state = ATA_IDLE;
    bzero(&ch->state_mtx, sizeof(struct mtx));
    mtx_init(&ch->state_mtx, "ATA state lock", NULL, MTX_DEF);
#ifndef ATA_CAM
    bzero(&ch->queue_mtx, sizeof(struct mtx));
    mtx_init(&ch->queue_mtx, "ATA queue lock", NULL, MTX_DEF);
    TAILQ_INIT(&ch->ata_queue);
#endif
    TASK_INIT(&ch->conntask, 0, ata_conn_event, dev);
#ifdef ATA_CAM
	for (i = 0; i < 16; i++) {
		ch->user[i].revision = 0;
		snprintf(buf, sizeof(buf), "dev%d.sata_rev", i);
		if (resource_int_value(device_get_name(dev),
		    device_get_unit(dev), buf, &mode) != 0 &&
		    resource_int_value(device_get_name(dev),
		    device_get_unit(dev), "sata_rev", &mode) != 0)
			mode = -1;
		if (mode >= 0)
			ch->user[i].revision = mode;
		ch->user[i].mode = 0;
		snprintf(buf, sizeof(buf), "dev%d.mode", i);
		if (resource_string_value(device_get_name(dev),
		    device_get_unit(dev), buf, &res) == 0)
			mode = ata_str2mode(res);
		else if (resource_string_value(device_get_name(dev),
		    device_get_unit(dev), "mode", &res) == 0)
			mode = ata_str2mode(res);
		else
			mode = -1;
		if (mode >= 0)
			ch->user[i].mode = mode;
		if (ch->flags & ATA_SATA)
			ch->user[i].bytecount = 8192;
		else
			ch->user[i].bytecount = MAXPHYS;
		ch->user[i].caps = 0;
		ch->curr[i] = ch->user[i];
		if (ch->pm_level > 0)
			ch->user[i].caps |= CTS_SATA_CAPS_H_PMREQ;
		if (ch->pm_level > 1)
			ch->user[i].caps |= CTS_SATA_CAPS_D_PMREQ;
	}
	callout_init(&ch->poll_callout, 1);
#endif

#ifndef ATA_CAM
    /* reset the controller HW, the channel and device(s) */
    while (ATA_LOCKING(dev, ATA_LF_LOCK) != ch->unit)
	pause("ataatch", 1);
    ATA_RESET(dev);
    ATA_LOCKING(dev, ATA_LF_UNLOCK);
#endif

    /* allocate DMA resources if DMA HW present*/
    if (ch->dma.alloc)
	ch->dma.alloc(dev);

    /* setup interrupt delivery */
    rid = ATA_IRQ_RID;
    ch->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
				       RF_SHAREABLE | RF_ACTIVE);
    if (!ch->r_irq) {
	device_printf(dev, "unable to allocate interrupt\n");
	return ENXIO;
    }
    if ((error = bus_setup_intr(dev, ch->r_irq, ATA_INTR_FLAGS, NULL,
				ata_interrupt, ch, &ch->ih))) {
	bus_release_resource(dev, SYS_RES_IRQ, rid, ch->r_irq);
	device_printf(dev, "unable to setup interrupt\n");
	return error;
    }

#ifndef ATA_CAM
    /* probe and attach devices on this channel unless we are in early boot */
    if (!ata_delayed_attach)
	ata_identify(dev);
    return (0);
#else
	if (ch->flags & ATA_PERIODIC_POLL)
		callout_reset(&ch->poll_callout, hz, ata_periodic_poll, ch);
	mtx_lock(&ch->state_mtx);
	/* Create the device queue for our SIM. */
	devq = cam_simq_alloc(1);
	if (devq == NULL) {
		device_printf(dev, "Unable to allocate simq\n");
		error = ENOMEM;
		goto err1;
	}
	/* Construct SIM entry */
	ch->sim = cam_sim_alloc(ataaction, atapoll, "ata", ch,
	    device_get_unit(dev), &ch->state_mtx, 1, 0, devq);
	if (ch->sim == NULL) {
		device_printf(dev, "unable to allocate sim\n");
		cam_simq_free(devq);
		error = ENOMEM;
		goto err1;
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
	mtx_unlock(&ch->state_mtx);
	return (0);

err3:
	xpt_bus_deregister(cam_sim_path(ch->sim));
err2:
	cam_sim_free(ch->sim, /*free_devq*/TRUE);
	ch->sim = NULL;
err1:
	bus_release_resource(dev, SYS_RES_IRQ, rid, ch->r_irq);
	mtx_unlock(&ch->state_mtx);
	if (ch->flags & ATA_PERIODIC_POLL)
		callout_drain(&ch->poll_callout);
	return (error);
#endif
}

int
ata_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
#ifndef ATA_CAM
    device_t *children;
    int nchildren, i;
#endif

    /* check that we have a valid channel to detach */
    if (!ch->r_irq)
	return ENXIO;

    /* grap the channel lock so no new requests gets launched */
    mtx_lock(&ch->state_mtx);
    ch->state |= ATA_STALL_QUEUE;
    mtx_unlock(&ch->state_mtx);
#ifdef ATA_CAM
    if (ch->flags & ATA_PERIODIC_POLL)
	callout_drain(&ch->poll_callout);
#endif

#ifndef ATA_CAM
    /* detach & delete all children */
    if (!device_get_children(dev, &children, &nchildren)) {
	for (i = 0; i < nchildren; i++)
	    if (children[i])
		device_delete_child(dev, children[i]);
	free(children, M_TEMP);
    } 
#endif
    taskqueue_drain(taskqueue_thread, &ch->conntask);

#ifdef ATA_CAM
	mtx_lock(&ch->state_mtx);
	xpt_async(AC_LOST_DEVICE, ch->path, NULL);
	xpt_free_path(ch->path);
	xpt_bus_deregister(cam_sim_path(ch->sim));
	cam_sim_free(ch->sim, /*free_devq*/TRUE);
	ch->sim = NULL;
	mtx_unlock(&ch->state_mtx);
#endif

    /* release resources */
    bus_teardown_intr(dev, ch->r_irq, ch->ih);
    bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, ch->r_irq);
    ch->r_irq = NULL;

    /* free DMA resources if DMA HW present*/
    if (ch->dma.free)
	ch->dma.free(dev);

    mtx_destroy(&ch->state_mtx);
#ifndef ATA_CAM
    mtx_destroy(&ch->queue_mtx);
#endif
    return 0;
}

static void
ata_conn_event(void *context, int dummy)
{
	device_t dev = (device_t)context;
#ifdef ATA_CAM
	struct ata_channel *ch = device_get_softc(dev);
	union ccb *ccb;

	mtx_lock(&ch->state_mtx);
	if (ch->sim == NULL) {
		mtx_unlock(&ch->state_mtx);
		return;
	}
	ata_reinit(dev);
	if ((ccb = xpt_alloc_ccb_nowait()) == NULL)
		return;
	if (xpt_create_path(&ccb->ccb_h.path, NULL,
	    cam_sim_path(ch->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_free_ccb(ccb);
		return;
	}
	xpt_rescan(ccb);
	mtx_unlock(&ch->state_mtx);
#else
	ata_reinit(dev);
#endif
}

int
ata_reinit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_request *request;
#ifndef ATA_CAM
    device_t *children;
    int nchildren, i;

    /* check that we have a valid channel to reinit */
    if (!ch || !ch->r_irq)
	return ENXIO;

    if (bootverbose)
	device_printf(dev, "reiniting channel ..\n");

    /* poll for locking the channel */
    while (ATA_LOCKING(dev, ATA_LF_LOCK) != ch->unit)
	pause("atarini", 1);

    /* catch eventual request in ch->running */
    mtx_lock(&ch->state_mtx);
    if (ch->state & ATA_STALL_QUEUE) {
	/* Recursive reinits and reinits during detach prohobited. */
	mtx_unlock(&ch->state_mtx);
	return (ENXIO);
    }
    if ((request = ch->running))
	callout_stop(&request->callout);
    ch->running = NULL;

    /* unconditionally grap the channel lock */
    ch->state |= ATA_STALL_QUEUE;
    mtx_unlock(&ch->state_mtx);

    /* reset the controller HW, the channel and device(s) */
    ATA_RESET(dev);

    /* reinit the children and delete any that fails */
    if (!device_get_children(dev, &children, &nchildren)) {
	mtx_lock(&Giant);       /* newbus suckage it needs Giant */
	for (i = 0; i < nchildren; i++) {
	    /* did any children go missing ? */
	    if (children[i] && device_is_attached(children[i]) &&
		ATA_REINIT(children[i])) {
		/*
		 * if we had a running request and its device matches
		 * this child we need to inform the request that the 
		 * device is gone.
		 */
		if (request && request->dev == children[i]) {
		    request->result = ENXIO;
		    device_printf(request->dev, "FAILURE - device detached\n");

		    /* if not timeout finish request here */
		    if (!(request->flags & ATA_R_TIMEOUT))
			    ata_finish(request);
		    request = NULL;
		}
		device_delete_child(dev, children[i]);
	    }
	}
	free(children, M_TEMP);
	mtx_unlock(&Giant);     /* newbus suckage dealt with, release Giant */
    }

    /* if we still have a good request put it on the queue again */
    if (request && !(request->flags & ATA_R_TIMEOUT)) {
	device_printf(request->dev,
		      "WARNING - %s requeued due to channel reset",
		      ata_cmd2str(request));
	if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
	    printf(" LBA=%ju", request->u.ata.lba);
	printf("\n");
	request->flags |= ATA_R_REQUEUE;
	ata_queue_request(request);
    }

    /* we're done release the channel for new work */
    mtx_lock(&ch->state_mtx);
    ch->state = ATA_IDLE;
    mtx_unlock(&ch->state_mtx);
    ATA_LOCKING(dev, ATA_LF_UNLOCK);

    /* Add new children. */
/*    ata_identify(dev); */

    if (bootverbose)
	device_printf(dev, "reinit done ..\n");

    /* kick off requests on the queue */
    ata_start(dev);
#else
	xpt_freeze_simq(ch->sim, 1);
	if ((request = ch->running)) {
		ch->running = NULL;
		if (ch->state == ATA_ACTIVE)
		    ch->state = ATA_IDLE;
		callout_stop(&request->callout);
		if (ch->dma.unload)
		    ch->dma.unload(request);
		request->result = ERESTART;
		ata_cam_end_transaction(dev, request);
	}
	/* reset the controller HW, the channel and device(s) */
	ATA_RESET(dev);
	/* Tell the XPT about the event */
	xpt_async(AC_BUS_RESET, ch->path, NULL);
	xpt_release_simq(ch->sim, TRUE);
#endif
	return(0);
}

int
ata_suspend(device_t dev)
{
    struct ata_channel *ch;

    /* check for valid device */
    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

#ifdef ATA_CAM
	if (ch->flags & ATA_PERIODIC_POLL)
		callout_drain(&ch->poll_callout);
	mtx_lock(&ch->state_mtx);
	xpt_freeze_simq(ch->sim, 1);
	while (ch->state != ATA_IDLE)
		msleep(ch, &ch->state_mtx, PRIBIO, "atasusp", hz/100);
	mtx_unlock(&ch->state_mtx);
#else
    /* wait for the channel to be IDLE or detached before suspending */
    while (ch->r_irq) {
	mtx_lock(&ch->state_mtx);
	if (ch->state == ATA_IDLE) {
	    ch->state = ATA_ACTIVE;
	    mtx_unlock(&ch->state_mtx);
	    break;
	}
	mtx_unlock(&ch->state_mtx);
	tsleep(ch, PRIBIO, "atasusp", hz/10);
    }
    ATA_LOCKING(dev, ATA_LF_UNLOCK);
#endif
    return(0);
}

int
ata_resume(device_t dev)
{
    struct ata_channel *ch;
    int error;

    /* check for valid device */
    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

#ifdef ATA_CAM
	mtx_lock(&ch->state_mtx);
	error = ata_reinit(dev);
	xpt_release_simq(ch->sim, TRUE);
	mtx_unlock(&ch->state_mtx);
	if (ch->flags & ATA_PERIODIC_POLL)
		callout_reset(&ch->poll_callout, hz, ata_periodic_poll, ch);
#else
    /* reinit the devices, we dont know what mode/state they are in */
    error = ata_reinit(dev);
    /* kick off requests on the queue */
    ata_start(dev);
#endif
    return error;
}

void
ata_interrupt(void *data)
{
#ifdef ATA_CAM
    struct ata_channel *ch = (struct ata_channel *)data;

    mtx_lock(&ch->state_mtx);
    xpt_batch_start(ch->sim);
#endif
    ata_interrupt_locked(data);
#ifdef ATA_CAM
    xpt_batch_done(ch->sim);
    mtx_unlock(&ch->state_mtx);
#endif
}

static void
ata_interrupt_locked(void *data)
{
    struct ata_channel *ch = (struct ata_channel *)data;
    struct ata_request *request;

#ifndef ATA_CAM
    mtx_lock(&ch->state_mtx);
#endif
    do {
	/* ignore interrupt if its not for us */
	if (ch->hw.status && !ch->hw.status(ch->dev))
	    break;

	/* do we have a running request */
	if (!(request = ch->running))
	    break;

	ATA_DEBUG_RQ(request, "interrupt");

	/* safetycheck for the right state */
	if (ch->state == ATA_IDLE) {
	    device_printf(request->dev, "interrupt on idle channel ignored\n");
	    break;
	}

	/*
	 * we have the HW locks, so end the transaction for this request
	 * if it finishes immediately otherwise wait for next interrupt
	 */
	if (ch->hw.end_transaction(request) == ATA_OP_FINISHED) {
	    ch->running = NULL;
	    if (ch->state == ATA_ACTIVE)
		ch->state = ATA_IDLE;
#ifdef ATA_CAM
	    ata_cam_end_transaction(ch->dev, request);
#else
	    mtx_unlock(&ch->state_mtx);
	    ATA_LOCKING(ch->dev, ATA_LF_UNLOCK);
	    ata_finish(request);
#endif
	    return;
	}
    } while (0);
#ifndef ATA_CAM
    mtx_unlock(&ch->state_mtx);
#endif
}

#ifdef ATA_CAM
static void
ata_periodic_poll(void *data)
{
    struct ata_channel *ch = (struct ata_channel *)data;

    callout_reset(&ch->poll_callout, hz, ata_periodic_poll, ch);
    ata_interrupt(ch);
}
#endif

void
ata_print_cable(device_t dev, u_int8_t *who)
{
    device_printf(dev,
                  "DMA limited to UDMA33, %s found non-ATA66 cable\n", who);
}

#ifndef ATA_CAM
int
ata_check_80pin(device_t dev, int mode)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (!ata_dma_check_80pin) {
        if (bootverbose)
            device_printf(dev, "Skipping 80pin cable check\n");
        return mode;
    }

    if (mode > ATA_UDMA2 && !(atadev->param.hwres & ATA_CABLE_ID)) {
        ata_print_cable(dev, "device");
        mode = ATA_UDMA2;
    }
    return mode;
}
#endif

#ifndef ATA_CAM
void
ata_setmode(device_t dev)
{
	struct ata_channel *ch = device_get_softc(device_get_parent(dev));
	struct ata_device *atadev = device_get_softc(dev);
	int error, mode, pmode;

	mode = atadev->mode;
	do {
		pmode = mode = ata_limit_mode(dev, mode, ATA_DMA_MAX);
		mode = ATA_SETMODE(device_get_parent(dev), atadev->unit, mode);
		if ((ch->flags & (ATA_CHECKS_CABLE | ATA_SATA)) == 0)
			mode = ata_check_80pin(dev, mode);
	} while (pmode != mode); /* Interate till successfull negotiation. */
	error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
	if (bootverbose)
	        device_printf(dev, "%ssetting %s\n",
		    (error) ? "FAILURE " : "", ata_mode2str(mode));
	atadev->mode = mode;
}
#endif

/*
 * device related interfaces
 */
#ifndef ATA_CAM
static int
ata_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	  int32_t flag, struct thread *td)
{
    device_t device, *children;
    struct ata_ioc_devices *devices = (struct ata_ioc_devices *)data;
    int *value = (int *)data;
    int i, nchildren, error = ENOTTY;

    switch (cmd) {
    case IOCATAGMAXCHANNEL:
	/* In case we have channel 0..n this will return n+1. */
	*value = devclass_get_maxunit(ata_devclass);
	error = 0;
	break;

    case IOCATAREINIT:
	if (*value >= devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)) ||
	    !device_is_attached(device))
	    return ENXIO;
	error = ata_reinit(device);
	break;

    case IOCATAATTACH:
	if (*value >= devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)) ||
	    !device_is_attached(device))
	    return ENXIO;
	error = DEVICE_ATTACH(device);
	break;

    case IOCATADETACH:
	if (*value >= devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)) ||
	    !device_is_attached(device))
	    return ENXIO;
	error = DEVICE_DETACH(device);
	break;

    case IOCATADEVICES:
	if (devices->channel >= devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, devices->channel)) ||
	    !device_is_attached(device))
	    return ENXIO;
	bzero(devices->name[0], 32);
	bzero(&devices->params[0], sizeof(struct ata_params));
	bzero(devices->name[1], 32);
	bzero(&devices->params[1], sizeof(struct ata_params));
	if (!device_get_children(device, &children, &nchildren)) {
	    for (i = 0; i < nchildren; i++) {
		if (children[i] && device_is_attached(children[i])) {
		    struct ata_device *atadev = device_get_softc(children[i]);

		    if (atadev->unit == ATA_MASTER) { /* XXX SOS PM */
			strncpy(devices->name[0],
				device_get_nameunit(children[i]), 32);
			bcopy(&atadev->param, &devices->params[0],
			      sizeof(struct ata_params));
		    }
		    if (atadev->unit == ATA_SLAVE) { /* XXX SOS PM */
			strncpy(devices->name[1],
				device_get_nameunit(children[i]), 32);
			bcopy(&atadev->param, &devices->params[1],
			      sizeof(struct ata_params));
		    }
		}
	    }
	    free(children, M_TEMP);
	    error = 0;
	}
	else
	    error = ENODEV;
	break;

    default:
	if (ata_raid_ioctl_func)
	    error = ata_raid_ioctl_func(cmd, data);
    }
    return error;
}
#endif

#ifndef ATA_CAM
int
ata_device_ioctl(device_t dev, u_long cmd, caddr_t data)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_ioc_request *ioc_request = (struct ata_ioc_request *)data;
    struct ata_params *params = (struct ata_params *)data;
    int *mode = (int *)data;
    struct ata_request *request;
    caddr_t buf;
    int error;

    switch (cmd) {
    case IOCATAREQUEST:
	if (ioc_request->count >
	    (ch->dma.max_iosize ? ch->dma.max_iosize : DFLTPHYS)) {
		return (EFBIG);
	}
	if (!(buf = malloc(ioc_request->count, M_ATA, M_NOWAIT))) {
	    return ENOMEM;
	}
	if (!(request = ata_alloc_request())) {
	    free(buf, M_ATA);
	    return  ENOMEM;
	}
	request->dev = atadev->dev;
	if (ioc_request->flags & ATA_CMD_WRITE) {
	    error = copyin(ioc_request->data, buf, ioc_request->count);
	    if (error) {
		free(buf, M_ATA);
		ata_free_request(request);
		return error;
	    }
	}
	if (ioc_request->flags & ATA_CMD_ATAPI) {
	    request->flags = ATA_R_ATAPI;
	    bcopy(ioc_request->u.atapi.ccb, request->u.atapi.ccb, 16);
	}
	else {
	    request->u.ata.command = ioc_request->u.ata.command;
	    request->u.ata.feature = ioc_request->u.ata.feature;
	    request->u.ata.lba = ioc_request->u.ata.lba;
	    request->u.ata.count = ioc_request->u.ata.count;
	}
	request->timeout = ioc_request->timeout;
	request->data = buf;
	request->bytecount = ioc_request->count;
	request->transfersize = request->bytecount;
	if (ioc_request->flags & ATA_CMD_CONTROL)
	    request->flags |= ATA_R_CONTROL;
	if (ioc_request->flags & ATA_CMD_READ)
	    request->flags |= ATA_R_READ;
	if (ioc_request->flags & ATA_CMD_WRITE)
	    request->flags |= ATA_R_WRITE;
	ata_queue_request(request);
	if (request->flags & ATA_R_ATAPI) {
	    bcopy(&request->u.atapi.sense, &ioc_request->u.atapi.sense,
		  sizeof(struct atapi_sense));
	}
	else {
	    ioc_request->u.ata.command = request->u.ata.command;
	    ioc_request->u.ata.feature = request->u.ata.feature;
	    ioc_request->u.ata.lba = request->u.ata.lba;
	    ioc_request->u.ata.count = request->u.ata.count;
	}
	ioc_request->error = request->result;
	if (ioc_request->flags & ATA_CMD_READ)
	    error = copyout(buf, ioc_request->data, ioc_request->count);
	else
	    error = 0;
	free(buf, M_ATA);
	ata_free_request(request);
	return error;
   
    case IOCATAGPARM:
	ata_getparam(atadev, 0);
	bcopy(&atadev->param, params, sizeof(struct ata_params));
	return 0;
	
    case IOCATASMODE:
	atadev->mode = *mode;
	ata_setmode(dev);
	return 0;

    case IOCATAGMODE:
	*mode = atadev->mode |
	    (ATA_GETREV(device_get_parent(dev), atadev->unit) << 8);
	return 0;
    case IOCATASSPINDOWN:
	atadev->spindown = *mode;
	return 0;
    case IOCATAGSPINDOWN:
	*mode = atadev->spindown;
	return 0;
    default:
	return ENOTTY;
    }
}
#endif

#ifndef ATA_CAM
static void
ata_boot_attach(void)
{
    struct ata_channel *ch;
    int ctlr;

    mtx_lock(&Giant);       /* newbus suckage it needs Giant */

    /* kick off probe and attach on all channels */
    for (ctlr = 0; ctlr < devclass_get_maxunit(ata_devclass); ctlr++) {
	if ((ch = devclass_get_softc(ata_devclass, ctlr))) {
	    ata_identify(ch->dev);
	}
    }

    /* release the hook that got us here, we are only needed once during boot */
    if (ata_delayed_attach) {
	config_intrhook_disestablish(ata_delayed_attach);
	free(ata_delayed_attach, M_TEMP);
	ata_delayed_attach = NULL;
    }

    mtx_unlock(&Giant);     /* newbus suckage dealt with, release Giant */
}
#endif

/*
 * misc support functions
 */
#ifndef ATA_CAM
static device_t
ata_add_child(device_t parent, struct ata_device *atadev, int unit)
{
    device_t child;

    if ((child = device_add_child(parent, (unit < 0) ? NULL : "ad", unit))) {
	device_set_softc(child, atadev);
	device_quiet(child);
	atadev->dev = child;
	atadev->max_iosize = DEV_BSIZE;
	atadev->mode = ATA_PIO_MAX;
    }
    return child;
}
#endif

#ifndef ATA_CAM
int
ata_getparam(struct ata_device *atadev, int init)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(atadev->dev));
    struct ata_request *request;
    const char *res;
    char buf[64];
    u_int8_t command = 0;
    int error = ENOMEM, retries = 2, mode = -1;

    if (ch->devices & (ATA_ATA_MASTER << atadev->unit))
	command = ATA_ATA_IDENTIFY;
    if (ch->devices & (ATA_ATAPI_MASTER << atadev->unit))
	command = ATA_ATAPI_IDENTIFY;
    if (!command)
	return ENXIO;

    while (retries-- > 0 && error) {
	if (!(request = ata_alloc_request()))
	    break;
	request->dev = atadev->dev;
	request->timeout = 1;
	request->retries = 0;
	request->u.ata.command = command;
	request->flags = (ATA_R_READ|ATA_R_AT_HEAD|ATA_R_DIRECT);
	if (!bootverbose)
	    request->flags |= ATA_R_QUIET;
	request->data = (void *)&atadev->param;
	request->bytecount = sizeof(struct ata_params);
	request->donecount = 0;
	request->transfersize = DEV_BSIZE;
	ata_queue_request(request);
	error = request->result;
	ata_free_request(request);
    }

    if (!error && (isprint(atadev->param.model[0]) ||
		   isprint(atadev->param.model[1]))) {
	struct ata_params *atacap = &atadev->param;
	int16_t *ptr;

	for (ptr = (int16_t *)atacap;
	     ptr < (int16_t *)atacap + sizeof(struct ata_params)/2; ptr++) {
	    *ptr = le16toh(*ptr);
	}
	if (!(!strncmp(atacap->model, "FX", 2) ||
	      !strncmp(atacap->model, "NEC", 3) ||
	      !strncmp(atacap->model, "Pioneer", 7) ||
	      !strncmp(atacap->model, "SHARP", 5))) {
	    bswap(atacap->model, sizeof(atacap->model));
	    bswap(atacap->revision, sizeof(atacap->revision));
	    bswap(atacap->serial, sizeof(atacap->serial));
	}
	btrim(atacap->model, sizeof(atacap->model));
	bpack(atacap->model, atacap->model, sizeof(atacap->model));
	btrim(atacap->revision, sizeof(atacap->revision));
	bpack(atacap->revision, atacap->revision, sizeof(atacap->revision));
	btrim(atacap->serial, sizeof(atacap->serial));
	bpack(atacap->serial, atacap->serial, sizeof(atacap->serial));

	if (bootverbose)
	    printf("ata%d-%s: pio=%s wdma=%s udma=%s cable=%s wire\n",
		   device_get_unit(ch->dev),
		   ata_unit2str(atadev),
		   ata_mode2str(ata_pmode(atacap)),
		   ata_mode2str(ata_wmode(atacap)),
		   ata_mode2str(ata_umode(atacap)),
		   (atacap->hwres & ATA_CABLE_ID) ? "80":"40");

	if (init) {
	    char buffer[64];

	    sprintf(buffer, "%.40s/%.8s", atacap->model, atacap->revision);
	    device_set_desc_copy(atadev->dev, buffer);
	    if ((atadev->param.config & ATA_PROTO_ATAPI) &&
		(atadev->param.config != ATA_CFA_MAGIC1) &&
		(atadev->param.config != ATA_CFA_MAGIC2)) {
		if (atapi_dma &&
		    (atadev->param.config & ATA_DRQ_MASK) != ATA_DRQ_INTR &&
		    ata_umode(&atadev->param) >= ATA_UDMA2)
		    atadev->mode = ATA_DMA_MAX;
	    }
	    else {
		if (ata_dma &&
		    (ata_umode(&atadev->param) > 0 ||
		     ata_wmode(&atadev->param) > 0))
		    atadev->mode = ATA_DMA_MAX;
	    }
	    snprintf(buf, sizeof(buf), "dev%d.mode", atadev->unit);
	    if (resource_string_value(device_get_name(ch->dev),
	        device_get_unit(ch->dev), buf, &res) == 0)
		    mode = ata_str2mode(res);
	    else if (resource_string_value(device_get_name(ch->dev),
		device_get_unit(ch->dev), "mode", &res) == 0)
		    mode = ata_str2mode(res);
	    if (mode >= 0)
		    atadev->mode = mode;
	}
    }
    else {
	if (!error)
	    error = ENXIO;
    }
    return error;
}
#endif

#ifndef ATA_CAM
int
ata_identify(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_device *atadev;
    device_t *children;
    device_t child, master = NULL;
    int nchildren, i, n = ch->devices;

    if (bootverbose)
	device_printf(dev, "Identifying devices: %08x\n", ch->devices);

    mtx_lock(&Giant);
    /* Skip existing devices. */
    if (!device_get_children(dev, &children, &nchildren)) {
	for (i = 0; i < nchildren; i++) {
	    if (children[i] && (atadev = device_get_softc(children[i])))
		n &= ~((ATA_ATA_MASTER | ATA_ATAPI_MASTER) << atadev->unit);
	}
	free(children, M_TEMP);
    }
    /* Create new devices. */
    if (bootverbose)
	device_printf(dev, "New devices: %08x\n", n);
    if (n == 0) {
	mtx_unlock(&Giant);
	return (0);
    }
    for (i = 0; i < ATA_PM; ++i) {
	if (n & (((ATA_ATA_MASTER | ATA_ATAPI_MASTER) << i))) {
	    int unit = -1;

	    if (!(atadev = malloc(sizeof(struct ata_device),
				  M_ATA, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "out of memory\n");
		return ENOMEM;
	    }
	    atadev->unit = i;
#ifdef ATA_STATIC_ID
	    if (n & (ATA_ATA_MASTER << i))
		unit = (device_get_unit(dev) << 1) + i;
#endif
	    if ((child = ata_add_child(dev, atadev, unit))) {
		/*
		 * PATA slave should be identified first, to allow
		 * device cable detection on master to work properly.
		 */
		if (i == 0 && (n & ATA_PORTMULTIPLIER) == 0 &&
			(n & ((ATA_ATA_MASTER | ATA_ATAPI_MASTER) << 1)) != 0) {
		    master = child;
		    continue;
		}
		if (ata_getparam(atadev, 1)) {
		    device_delete_child(dev, child);
		    free(atadev, M_ATA);
		}
	    }
	    else
		free(atadev, M_ATA);
	}
    }
    if (master) {
	atadev = device_get_softc(master);
	if (ata_getparam(atadev, 1)) {
	    device_delete_child(dev, master);
	    free(atadev, M_ATA);
	}
    }
    bus_generic_probe(dev);
    bus_generic_attach(dev);
    mtx_unlock(&Giant);
    return 0;
}
#endif

void
ata_default_registers(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* fill in the defaults from whats setup already */
    ch->r_io[ATA_ERROR].res = ch->r_io[ATA_FEATURE].res;
    ch->r_io[ATA_ERROR].offset = ch->r_io[ATA_FEATURE].offset;
    ch->r_io[ATA_IREASON].res = ch->r_io[ATA_COUNT].res;
    ch->r_io[ATA_IREASON].offset = ch->r_io[ATA_COUNT].offset;
    ch->r_io[ATA_STATUS].res = ch->r_io[ATA_COMMAND].res;
    ch->r_io[ATA_STATUS].offset = ch->r_io[ATA_COMMAND].offset;
    ch->r_io[ATA_ALTSTAT].res = ch->r_io[ATA_CONTROL].res;
    ch->r_io[ATA_ALTSTAT].offset = ch->r_io[ATA_CONTROL].offset;
}

#ifndef ATA_CAM
void
ata_modify_if_48bit(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_device *atadev = device_get_softc(request->dev);

    request->flags &= ~ATA_R_48BIT;

    if (((request->u.ata.lba + request->u.ata.count) >= ATA_MAX_28BIT_LBA ||
	 request->u.ata.count > 256) &&
	atadev->param.support.command2 & ATA_SUPPORT_ADDRESS48) {

	/* translate command into 48bit version */
	switch (request->u.ata.command) {
	case ATA_READ:
	    request->u.ata.command = ATA_READ48;
	    break;
	case ATA_READ_MUL:
	    request->u.ata.command = ATA_READ_MUL48;
	    break;
	case ATA_READ_DMA:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_READ_MUL48;
		else
		    request->u.ata.command = ATA_READ48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_READ_DMA48;
	    break;
	case ATA_READ_DMA_QUEUED:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_READ_MUL48;
		else
		    request->u.ata.command = ATA_READ48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_READ_DMA_QUEUED48;
	    break;
	case ATA_WRITE:
	    request->u.ata.command = ATA_WRITE48;
	    break;
	case ATA_WRITE_MUL:
	    request->u.ata.command = ATA_WRITE_MUL48;
	    break;
	case ATA_WRITE_DMA:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_WRITE_MUL48;
		else
		    request->u.ata.command = ATA_WRITE48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_WRITE_DMA48;
	    break;
	case ATA_WRITE_DMA_QUEUED:
	    if (ch->flags & ATA_NO_48BIT_DMA) {
		if (request->transfersize > DEV_BSIZE)
		    request->u.ata.command = ATA_WRITE_MUL48;
		else
		    request->u.ata.command = ATA_WRITE48;
		request->u.ata.command = ATA_WRITE48;
		request->flags &= ~ATA_R_DMA;
	    }
	    else
		request->u.ata.command = ATA_WRITE_DMA_QUEUED48;
	    break;
	case ATA_FLUSHCACHE:
	    request->u.ata.command = ATA_FLUSHCACHE48;
	    break;
	case ATA_SET_MAX_ADDRESS:
	    request->u.ata.command = ATA_SET_MAX_ADDRESS48;
	    break;
	default:
	    return;
	}
	request->flags |= ATA_R_48BIT;
    }
    else if (atadev->param.support.command2 & ATA_SUPPORT_ADDRESS48) {

	/* translate command into 48bit version */
	switch (request->u.ata.command) {
	case ATA_FLUSHCACHE:
	    request->u.ata.command = ATA_FLUSHCACHE48;
	    break;
	case ATA_READ_NATIVE_MAX_ADDRESS:
	    request->u.ata.command = ATA_READ_NATIVE_MAX_ADDRESS48;
	    break;
	case ATA_SET_MAX_ADDRESS:
	    request->u.ata.command = ATA_SET_MAX_ADDRESS48;
	    break;
	default:
	    return;
	}
	request->flags |= ATA_R_48BIT;
    }
}
#endif

void
ata_udelay(int interval)
{
    /* for now just use DELAY, the timer/sleep subsytems are not there yet */
    if (1 || interval < (1000000/hz) || ata_delayed_attach)
	DELAY(interval);
    else
	pause("ataslp", interval/(1000000/hz));
}

#ifndef ATA_CAM
const char *
ata_unit2str(struct ata_device *atadev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(atadev->dev));
    static char str[8];

    if (ch->devices & ATA_PORTMULTIPLIER)
	sprintf(str, "port%d", atadev->unit);
    else
	sprintf(str, "%s", atadev->unit == ATA_MASTER ? "master" : "slave");
    return str;
}
#endif

const char *
ata_mode2str(int mode)
{
    switch (mode) {
    case -1: return "UNSUPPORTED";
    case ATA_PIO0: return "PIO0";
    case ATA_PIO1: return "PIO1";
    case ATA_PIO2: return "PIO2";
    case ATA_PIO3: return "PIO3";
    case ATA_PIO4: return "PIO4";
    case ATA_WDMA0: return "WDMA0";
    case ATA_WDMA1: return "WDMA1";
    case ATA_WDMA2: return "WDMA2";
    case ATA_UDMA0: return "UDMA16";
    case ATA_UDMA1: return "UDMA25";
    case ATA_UDMA2: return "UDMA33";
    case ATA_UDMA3: return "UDMA40";
    case ATA_UDMA4: return "UDMA66";
    case ATA_UDMA5: return "UDMA100";
    case ATA_UDMA6: return "UDMA133";
    case ATA_SA150: return "SATA150";
    case ATA_SA300: return "SATA300";
    default:
	if (mode & ATA_DMA_MASK)
	    return "BIOSDMA";
	else
	    return "BIOSPIO";
    }
}

int
ata_str2mode(const char *str)
{

	if (!strcasecmp(str, "PIO0")) return (ATA_PIO0);
	if (!strcasecmp(str, "PIO1")) return (ATA_PIO1);
	if (!strcasecmp(str, "PIO2")) return (ATA_PIO2);
	if (!strcasecmp(str, "PIO3")) return (ATA_PIO3);
	if (!strcasecmp(str, "PIO4")) return (ATA_PIO4);
	if (!strcasecmp(str, "WDMA0")) return (ATA_WDMA0);
	if (!strcasecmp(str, "WDMA1")) return (ATA_WDMA1);
	if (!strcasecmp(str, "WDMA2")) return (ATA_WDMA2);
	if (!strcasecmp(str, "UDMA0")) return (ATA_UDMA0);
	if (!strcasecmp(str, "UDMA16")) return (ATA_UDMA0);
	if (!strcasecmp(str, "UDMA1")) return (ATA_UDMA1);
	if (!strcasecmp(str, "UDMA25")) return (ATA_UDMA1);
	if (!strcasecmp(str, "UDMA2")) return (ATA_UDMA2);
	if (!strcasecmp(str, "UDMA33")) return (ATA_UDMA2);
	if (!strcasecmp(str, "UDMA3")) return (ATA_UDMA3);
	if (!strcasecmp(str, "UDMA44")) return (ATA_UDMA3);
	if (!strcasecmp(str, "UDMA4")) return (ATA_UDMA4);
	if (!strcasecmp(str, "UDMA66")) return (ATA_UDMA4);
	if (!strcasecmp(str, "UDMA5")) return (ATA_UDMA5);
	if (!strcasecmp(str, "UDMA100")) return (ATA_UDMA5);
	if (!strcasecmp(str, "UDMA6")) return (ATA_UDMA6);
	if (!strcasecmp(str, "UDMA133")) return (ATA_UDMA6);
	return (-1);
}

#ifndef ATA_CAM
const char *
ata_satarev2str(int rev)
{
	switch (rev) {
	case 0: return "";
	case 1: return "SATA 1.5Gb/s";
	case 2: return "SATA 3Gb/s";
	case 3: return "SATA 6Gb/s";
	case 0xff: return "SATA";
	default: return "???";
	}
}
#endif

int
ata_atapi(device_t dev, int target)
{
    struct ata_channel *ch = device_get_softc(dev);

    return (ch->devices & (ATA_ATAPI_MASTER << target));
}

#ifndef ATA_CAM
int
ata_pmode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_64_70) {
	if (ap->apiomodes & 0x02)
	    return ATA_PIO4;
	if (ap->apiomodes & 0x01)
	    return ATA_PIO3;
    }
    if (ap->mwdmamodes & 0x04)
	return ATA_PIO4;
    if (ap->mwdmamodes & 0x02)
	return ATA_PIO3;
    if (ap->mwdmamodes & 0x01)
	return ATA_PIO2;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x200)
	return ATA_PIO2;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x100)
	return ATA_PIO1;
    if ((ap->retired_piomode & ATA_RETIRED_PIO_MASK) == 0x000)
	return ATA_PIO0;
    return ATA_PIO0;
}
#endif

#ifndef ATA_CAM
int
ata_wmode(struct ata_params *ap)
{
    if (ap->mwdmamodes & 0x04)
	return ATA_WDMA2;
    if (ap->mwdmamodes & 0x02)
	return ATA_WDMA1;
    if (ap->mwdmamodes & 0x01)
	return ATA_WDMA0;
    return -1;
}
#endif

#ifndef ATA_CAM
int
ata_umode(struct ata_params *ap)
{
    if (ap->atavalid & ATA_FLAG_88) {
	if (ap->udmamodes & 0x40)
	    return ATA_UDMA6;
	if (ap->udmamodes & 0x20)
	    return ATA_UDMA5;
	if (ap->udmamodes & 0x10)
	    return ATA_UDMA4;
	if (ap->udmamodes & 0x08)
	    return ATA_UDMA3;
	if (ap->udmamodes & 0x04)
	    return ATA_UDMA2;
	if (ap->udmamodes & 0x02)
	    return ATA_UDMA1;
	if (ap->udmamodes & 0x01)
	    return ATA_UDMA0;
    }
    return -1;
}
#endif

#ifndef ATA_CAM
int
ata_limit_mode(device_t dev, int mode, int maxmode)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (maxmode && mode > maxmode)
	mode = maxmode;

    if (mode >= ATA_UDMA0 && ata_umode(&atadev->param) > 0)
	return min(mode, ata_umode(&atadev->param));

    if (mode >= ATA_WDMA0 && ata_wmode(&atadev->param) > 0)
	return min(mode, ata_wmode(&atadev->param));

    if (mode > ata_pmode(&atadev->param))
	return min(mode, ata_pmode(&atadev->param));

    return mode;
}
#endif

#ifndef ATA_CAM
static void
bswap(int8_t *buf, int len)
{
    u_int16_t *ptr = (u_int16_t*)(buf + len);

    while (--ptr >= (u_int16_t*)buf)
	*ptr = ntohs(*ptr);
}
#endif

#ifndef ATA_CAM
static void
btrim(int8_t *buf, int len)
{
    int8_t *ptr;

    for (ptr = buf; ptr < buf+len; ++ptr)
	if (!*ptr || *ptr == '_')
	    *ptr = ' ';
    for (ptr = buf + len - 1; ptr >= buf && *ptr == ' '; --ptr)
	*ptr = 0;
}
#endif

#ifndef ATA_CAM
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
#endif

#ifdef ATA_CAM
static void
ata_cam_begin_transaction(device_t dev, union ccb *ccb)
{
	struct ata_channel *ch = device_get_softc(dev);
	struct ata_request *request;

	if (!(request = ata_alloc_request())) {
		device_printf(dev, "FAILURE - out of memory in start\n");
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		return;
	}
	bzero(request, sizeof(*request));

	/* setup request */
	request->dev = NULL;
	request->parent = dev;
	request->unit = ccb->ccb_h.target_id;
	if (ccb->ccb_h.func_code == XPT_ATA_IO) {
		request->data = ccb->ataio.data_ptr;
		request->bytecount = ccb->ataio.dxfer_len;
		request->u.ata.command = ccb->ataio.cmd.command;
		request->u.ata.feature = ((uint16_t)ccb->ataio.cmd.features_exp << 8) |
					  (uint16_t)ccb->ataio.cmd.features;
		request->u.ata.count = ((uint16_t)ccb->ataio.cmd.sector_count_exp << 8) |
					(uint16_t)ccb->ataio.cmd.sector_count;
		if (ccb->ataio.cmd.flags & CAM_ATAIO_48BIT) {
			request->flags |= ATA_R_48BIT;
			request->u.ata.lba =
				     ((uint64_t)ccb->ataio.cmd.lba_high_exp << 40) |
				     ((uint64_t)ccb->ataio.cmd.lba_mid_exp << 32) |
				     ((uint64_t)ccb->ataio.cmd.lba_low_exp << 24);
		} else {
			request->u.ata.lba =
				     ((uint64_t)(ccb->ataio.cmd.device & 0x0f) << 24);
		}
		request->u.ata.lba |= ((uint64_t)ccb->ataio.cmd.lba_high << 16) |
				      ((uint64_t)ccb->ataio.cmd.lba_mid << 8) |
				       (uint64_t)ccb->ataio.cmd.lba_low;
		if (ccb->ataio.cmd.flags & CAM_ATAIO_NEEDRESULT)
			request->flags |= ATA_R_NEEDRESULT;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE &&
		    ccb->ataio.cmd.flags & CAM_ATAIO_DMA)
			request->flags |= ATA_R_DMA;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			request->flags |= ATA_R_READ;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			request->flags |= ATA_R_WRITE;
		if (ccb->ataio.cmd.command == ATA_READ_MUL ||
		    ccb->ataio.cmd.command == ATA_READ_MUL48 ||
		    ccb->ataio.cmd.command == ATA_WRITE_MUL ||
		    ccb->ataio.cmd.command == ATA_WRITE_MUL48) {
			request->transfersize = min(request->bytecount,
			    ch->curr[ccb->ccb_h.target_id].bytecount);
		} else
			request->transfersize = min(request->bytecount, 512);
	} else {
		request->data = ccb->csio.data_ptr;
		request->bytecount = ccb->csio.dxfer_len;
		bcopy((ccb->ccb_h.flags & CAM_CDB_POINTER) ?
		    ccb->csio.cdb_io.cdb_ptr : ccb->csio.cdb_io.cdb_bytes,
		    request->u.atapi.ccb, ccb->csio.cdb_len);
		request->flags |= ATA_R_ATAPI;
		if (ch->curr[ccb->ccb_h.target_id].atapi == 16)
			request->flags |= ATA_R_ATAPI16;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE &&
		    ch->curr[ccb->ccb_h.target_id].mode >= ATA_DMA)
			request->flags |= ATA_R_DMA;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			request->flags |= ATA_R_READ;
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			request->flags |= ATA_R_WRITE;
		request->transfersize = min(request->bytecount,
		    ch->curr[ccb->ccb_h.target_id].bytecount);
	}
	request->retries = 0;
	request->timeout = (ccb->ccb_h.timeout + 999) / 1000;
	callout_init_mtx(&request->callout, &ch->state_mtx, CALLOUT_RETURNUNLOCKED);
	request->ccb = ccb;
	request->flags |= ATA_R_DATA_IN_CCB;

	ch->running = request;
	ch->state = ATA_ACTIVE;
	if (ch->hw.begin_transaction(request) == ATA_OP_FINISHED) {
	    ch->running = NULL;
	    ch->state = ATA_IDLE;
	    ata_cam_end_transaction(dev, request);
	    return;
	}
}

static void
ata_cam_request_sense(device_t dev, struct ata_request *request)
{
	struct ata_channel *ch = device_get_softc(dev);
	union ccb *ccb = request->ccb;

	ch->requestsense = 1;

	bzero(request, sizeof(*request));
	request->dev = NULL;
	request->parent = dev;
	request->unit = ccb->ccb_h.target_id;
	request->data = (void *)&ccb->csio.sense_data;
	request->bytecount = ccb->csio.sense_len;
	request->u.atapi.ccb[0] = ATAPI_REQUEST_SENSE;
	request->u.atapi.ccb[4] = ccb->csio.sense_len;
	request->flags |= ATA_R_ATAPI;
	if (ch->curr[ccb->ccb_h.target_id].atapi == 16)
		request->flags |= ATA_R_ATAPI16;
	if (ch->curr[ccb->ccb_h.target_id].mode >= ATA_DMA)
		request->flags |= ATA_R_DMA;
	request->flags |= ATA_R_READ;
	request->transfersize = min(request->bytecount,
	    ch->curr[ccb->ccb_h.target_id].bytecount);
	request->retries = 0;
	request->timeout = (ccb->ccb_h.timeout + 999) / 1000;
	callout_init_mtx(&request->callout, &ch->state_mtx, CALLOUT_RETURNUNLOCKED);
	request->ccb = ccb;

	ch->running = request;
	ch->state = ATA_ACTIVE;
	if (ch->hw.begin_transaction(request) == ATA_OP_FINISHED) {
		ch->running = NULL;
		ch->state = ATA_IDLE;
		ata_cam_end_transaction(dev, request);
		return;
	}
}

static void
ata_cam_process_sense(device_t dev, struct ata_request *request)
{
	struct ata_channel *ch = device_get_softc(dev);
	union ccb *ccb = request->ccb;
	int fatalerr = 0;

	ch->requestsense = 0;

	if (request->flags & ATA_R_TIMEOUT)
		fatalerr = 1;
	if ((request->flags & ATA_R_TIMEOUT) == 0 &&
	    (request->status & ATA_S_ERROR) == 0 &&
	    request->result == 0) {
		ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
	} else {
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		ccb->ccb_h.status |= CAM_AUTOSENSE_FAIL;
	}

	ata_free_request(request);
	xpt_done(ccb);
	/* Do error recovery if needed. */
	if (fatalerr)
		ata_reinit(dev);
}

void
ata_cam_end_transaction(device_t dev, struct ata_request *request)
{
	struct ata_channel *ch = device_get_softc(dev);
	union ccb *ccb = request->ccb;
	int fatalerr = 0;

	if (ch->requestsense) {
		ata_cam_process_sense(dev, request);
		return;
	}

	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	if (request->flags & ATA_R_TIMEOUT) {
		xpt_freeze_simq(ch->sim, 1);
		ccb->ccb_h.status &= ~CAM_STATUS_MASK;
		ccb->ccb_h.status |= CAM_CMD_TIMEOUT | CAM_RELEASE_SIMQ;
		fatalerr = 1;
	} else if (request->status & ATA_S_ERROR) {
		if (ccb->ccb_h.func_code == XPT_ATA_IO) {
			ccb->ccb_h.status |= CAM_ATA_STATUS_ERROR;
		} else {
			ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
			ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
		}
	} else if (request->result == ERESTART)
		ccb->ccb_h.status |= CAM_REQUEUE_REQ;
	else if (request->result != 0)
		ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
	else
		ccb->ccb_h.status |= CAM_REQ_CMP;
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP &&
	    !(ccb->ccb_h.status & CAM_DEV_QFRZN)) {
		xpt_freeze_devq(ccb->ccb_h.path, 1);
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
	}
	if (ccb->ccb_h.func_code == XPT_ATA_IO &&
	    ((request->status & ATA_S_ERROR) ||
	    (ccb->ataio.cmd.flags & CAM_ATAIO_NEEDRESULT))) {
		struct ata_res *res = &ccb->ataio.res;
		res->status = request->status;
		res->error = request->error;
		res->lba_low = request->u.ata.lba;
		res->lba_mid = request->u.ata.lba >> 8;
		res->lba_high = request->u.ata.lba >> 16;
		res->device = request->u.ata.lba >> 24;
		res->lba_low_exp = request->u.ata.lba >> 24;
		res->lba_mid_exp = request->u.ata.lba >> 32;
		res->lba_high_exp = request->u.ata.lba >> 40;
		res->sector_count = request->u.ata.count;
		res->sector_count_exp = request->u.ata.count >> 8;
	}
	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		if (ccb->ccb_h.func_code == XPT_ATA_IO) {
			ccb->ataio.resid =
			    ccb->ataio.dxfer_len - request->donecount;
		} else {
			ccb->csio.resid =
			    ccb->csio.dxfer_len - request->donecount;
		}
	}
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_SCSI_STATUS_ERROR &&
	    (ccb->ccb_h.flags & CAM_DIS_AUTOSENSE) == 0)
		ata_cam_request_sense(dev, request);
	else {
		ata_free_request(request);
		xpt_done(ccb);
	}
	/* Do error recovery if needed. */
	if (fatalerr)
		ata_reinit(dev);
}

static int
ata_check_ids(device_t dev, union ccb *ccb)
{
	struct ata_channel *ch = device_get_softc(dev);

	if (ccb->ccb_h.target_id > ((ch->flags & ATA_NO_SLAVE) ? 0 : 1)) {
		ccb->ccb_h.status = CAM_TID_INVALID;
		xpt_done(ccb);
		return (-1);
	}
	if (ccb->ccb_h.target_lun != 0) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return (-1);
	}
	return (0);
}

static void
ataaction(struct cam_sim *sim, union ccb *ccb)
{
	device_t dev, parent;
	struct ata_channel *ch;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("ataaction func_code=%x\n",
	    ccb->ccb_h.func_code));

	ch = (struct ata_channel *)cam_sim_softc(sim);
	dev = ch->dev;
	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_ATA_IO:	/* Execute the requested I/O operation */
	case XPT_SCSI_IO:
		if (ata_check_ids(dev, ccb))
			return;
		if ((ch->devices & ((ATA_ATA_MASTER | ATA_ATAPI_MASTER)
		    << ccb->ccb_h.target_id)) == 0) {
			ccb->ccb_h.status = CAM_SEL_TIMEOUT;
			break;
		}
		if (ch->running)
			device_printf(dev, "already running!\n");
		if (ccb->ccb_h.func_code == XPT_ATA_IO &&
		    (ccb->ataio.cmd.flags & CAM_ATAIO_CONTROL) &&
		    (ccb->ataio.cmd.control & ATA_A_RESET)) {
			struct ata_res *res = &ccb->ataio.res;
			
			bzero(res, sizeof(*res));
			if (ch->devices & (ATA_ATA_MASTER << ccb->ccb_h.target_id)) {
				res->lba_high = 0;
				res->lba_mid = 0;
			} else {
				res->lba_high = 0xeb;
				res->lba_mid = 0x14;
			}
			ccb->ccb_h.status = CAM_REQ_CMP;
			break;
		}
		ata_cam_begin_transaction(dev, ccb);
		return;
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	case XPT_SET_TRAN_SETTINGS:
	{
		struct	ccb_trans_settings *cts = &ccb->cts;
		struct	ata_cam_device *d; 

		if (ata_check_ids(dev, ccb))
			return;
		if (cts->type == CTS_TYPE_CURRENT_SETTINGS)
			d = &ch->curr[ccb->ccb_h.target_id];
		else
			d = &ch->user[ccb->ccb_h.target_id];
		if (ch->flags & ATA_SATA) {
			if (cts->xport_specific.sata.valid & CTS_SATA_VALID_REVISION)
				d->revision = cts->xport_specific.sata.revision;
			if (cts->xport_specific.sata.valid & CTS_SATA_VALID_MODE) {
				if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
					d->mode = ATA_SETMODE(ch->dev,
					    ccb->ccb_h.target_id,
					    cts->xport_specific.sata.mode);
				} else
					d->mode = cts->xport_specific.sata.mode;
			}
			if (cts->xport_specific.sata.valid & CTS_SATA_VALID_BYTECOUNT)
				d->bytecount = min(8192, cts->xport_specific.sata.bytecount);
			if (cts->xport_specific.sata.valid & CTS_SATA_VALID_ATAPI)
				d->atapi = cts->xport_specific.sata.atapi;
			if (cts->xport_specific.sata.valid & CTS_SATA_VALID_CAPS)
				d->caps = cts->xport_specific.sata.caps;
		} else {
			if (cts->xport_specific.ata.valid & CTS_ATA_VALID_MODE) {
				if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
					d->mode = ATA_SETMODE(ch->dev,
					    ccb->ccb_h.target_id,
					    cts->xport_specific.ata.mode);
				} else
					d->mode = cts->xport_specific.ata.mode;
			}
			if (cts->xport_specific.ata.valid & CTS_ATA_VALID_BYTECOUNT)
				d->bytecount = cts->xport_specific.ata.bytecount;
			if (cts->xport_specific.ata.valid & CTS_ATA_VALID_ATAPI)
				d->atapi = cts->xport_specific.ata.atapi;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		struct	ccb_trans_settings *cts = &ccb->cts;
		struct  ata_cam_device *d;

		if (ata_check_ids(dev, ccb))
			return;
		if (cts->type == CTS_TYPE_CURRENT_SETTINGS)
			d = &ch->curr[ccb->ccb_h.target_id];
		else
			d = &ch->user[ccb->ccb_h.target_id];
		cts->protocol = PROTO_UNSPECIFIED;
		cts->protocol_version = PROTO_VERSION_UNSPECIFIED;
		if (ch->flags & ATA_SATA) {
			cts->transport = XPORT_SATA;
			cts->transport_version = XPORT_VERSION_UNSPECIFIED;
			cts->xport_specific.sata.valid = 0;
			cts->xport_specific.sata.mode = d->mode;
			cts->xport_specific.sata.valid |= CTS_SATA_VALID_MODE;
			cts->xport_specific.sata.bytecount = d->bytecount;
			cts->xport_specific.sata.valid |= CTS_SATA_VALID_BYTECOUNT;
			if (cts->type == CTS_TYPE_CURRENT_SETTINGS) {
				cts->xport_specific.sata.revision =
				    ATA_GETREV(dev, ccb->ccb_h.target_id);
				if (cts->xport_specific.sata.revision != 0xff) {
					cts->xport_specific.sata.valid |=
					    CTS_SATA_VALID_REVISION;
				}
				cts->xport_specific.sata.caps =
				    d->caps & CTS_SATA_CAPS_D;
				if (ch->pm_level) {
					cts->xport_specific.sata.caps |=
					    CTS_SATA_CAPS_H_PMREQ;
				}
				cts->xport_specific.sata.caps &=
				    ch->user[ccb->ccb_h.target_id].caps;
				cts->xport_specific.sata.valid |=
				    CTS_SATA_VALID_CAPS;
			} else {
				cts->xport_specific.sata.revision = d->revision;
				cts->xport_specific.sata.valid |= CTS_SATA_VALID_REVISION;
				cts->xport_specific.sata.caps = d->caps;
				cts->xport_specific.sata.valid |= CTS_SATA_VALID_CAPS;
			}
			cts->xport_specific.sata.atapi = d->atapi;
			cts->xport_specific.sata.valid |= CTS_SATA_VALID_ATAPI;
		} else {
			cts->transport = XPORT_ATA;
			cts->transport_version = XPORT_VERSION_UNSPECIFIED;
			cts->xport_specific.ata.valid = 0;
			cts->xport_specific.ata.mode = d->mode;
			cts->xport_specific.ata.valid |= CTS_ATA_VALID_MODE;
			cts->xport_specific.ata.bytecount = d->bytecount;
			cts->xport_specific.ata.valid |= CTS_ATA_VALID_BYTECOUNT;
			cts->xport_specific.ata.atapi = d->atapi;
			cts->xport_specific.ata.valid |= CTS_ATA_VALID_ATAPI;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
		ata_reinit(dev);
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		parent = device_get_parent(dev);
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_SEQSCAN;
		cpi->hba_eng_cnt = 0;
		if (ch->flags & ATA_NO_SLAVE)
			cpi->max_target = 0;
		else
			cpi->max_target = 1;
		cpi->max_lun = 0;
		cpi->initiator_id = 0;
		cpi->bus_id = cam_sim_bus(sim);
		if (ch->flags & ATA_SATA)
			cpi->base_transfer_speed = 150000;
		else
			cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "ATA", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		if (ch->flags & ATA_SATA)
			cpi->transport = XPORT_SATA;
		else
			cpi->transport = XPORT_ATA;
		cpi->transport_version = XPORT_VERSION_UNSPECIFIED;
		cpi->protocol = PROTO_ATA;
		cpi->protocol_version = PROTO_VERSION_UNSPECIFIED;
		cpi->maxio = ch->dma.max_iosize ? ch->dma.max_iosize : DFLTPHYS;
		if (device_get_devclass(device_get_parent(parent)) ==
		    devclass_find("pci")) {
			cpi->hba_vendor = pci_get_vendor(parent);
			cpi->hba_device = pci_get_device(parent);
			cpi->hba_subvendor = pci_get_subvendor(parent);
			cpi->hba_subdevice = pci_get_subdevice(parent);
		}
		cpi->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}
	xpt_done(ccb);
}

static void
atapoll(struct cam_sim *sim)
{
	struct ata_channel *ch = (struct ata_channel *)cam_sim_softc(sim);

	ata_interrupt_locked(ch);
}
#endif

/*
 * module handeling
 */
static int
ata_module_event_handler(module_t mod, int what, void *arg)
{
#ifndef ATA_CAM
    static struct cdev *atacdev;
#endif

    switch (what) {
    case MOD_LOAD:
#ifndef ATA_CAM
	/* register controlling device */
	atacdev = make_dev(&ata_cdevsw, 0, UID_ROOT, GID_OPERATOR, 0600, "ata");

	if (cold) {
	    /* register boot attach to be run when interrupts are enabled */
	    if (!(ata_delayed_attach = (struct intr_config_hook *)
				       malloc(sizeof(struct intr_config_hook),
					      M_TEMP, M_NOWAIT | M_ZERO))) {
		printf("ata: malloc of delayed attach hook failed\n");
		return EIO;
	    }
	    ata_delayed_attach->ich_func = (void*)ata_boot_attach;
	    if (config_intrhook_establish(ata_delayed_attach) != 0) {
		printf("ata: config_intrhook_establish failed\n");
		free(ata_delayed_attach, M_TEMP);
	    }
	}
#endif
	return 0;

    case MOD_UNLOAD:
#ifndef ATA_CAM
	/* deregister controlling device */
	destroy_dev(atacdev);
#endif
	return 0;

    default:
	return EOPNOTSUPP;
    }
}

static moduledata_t ata_moduledata = { "ata", ata_module_event_handler, NULL };
DECLARE_MODULE(ata, ata_moduledata, SI_SUB_CONFIGURE, SI_ORDER_SECOND);
MODULE_VERSION(ata, 1);
#ifdef ATA_CAM
MODULE_DEPEND(ata, cam, 1, 1, 1);
#endif

static void
ata_init(void)
{
    ata_request_zone = uma_zcreate("ata_request", sizeof(struct ata_request),
				   NULL, NULL, NULL, NULL, 0, 0);
    ata_composite_zone = uma_zcreate("ata_composite",
				     sizeof(struct ata_composite),
				     NULL, NULL, NULL, NULL, 0, 0);
}
SYSINIT(ata_register, SI_SUB_DRIVERS, SI_ORDER_SECOND, ata_init, NULL);

static void
ata_uninit(void)
{
    uma_zdestroy(ata_composite_zone);
    uma_zdestroy(ata_request_zone);
}
SYSUNINIT(ata_unregister, SI_SUB_DRIVERS, SI_ORDER_SECOND, ata_uninit, NULL);
