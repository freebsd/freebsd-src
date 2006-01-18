/*-
 * Copyright (c) 1998 - 2006 Søren Schmidt <sos@FreeBSD.org>
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
#ifdef __alpha__
#include <machine/md_var.h>
#endif
#include <dev/ata/ata-all.h>
#include <ata_if.h>

/* device structure */
static  d_ioctl_t       ata_ioctl;
static struct cdevsw ata_cdevsw = {
	.d_version =    D_VERSION,
	.d_flags =      D_NEEDGIANT, /* we need this as newbus isn't mpsafe */
	.d_ioctl =      ata_ioctl,
	.d_name =       "ata",
};

/* prototypes */
static void ata_boot_attach(void);
static device_t ata_add_child(device_t, struct ata_device *, int);
static int ata_getparam(struct ata_device *, int);
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);

/* global vars */
MALLOC_DEFINE(M_ATA, "ata_generic", "ATA driver generic layer");
int (*ata_raid_ioctl_func)(u_long cmd, caddr_t data) = NULL;
devclass_t ata_devclass;
uma_zone_t ata_request_zone;
uma_zone_t ata_composite_zone;
int ata_wc = 1;

/* local vars */
static struct intr_config_hook *ata_delayed_attach = NULL;
static int ata_dma = 1;
static int atapi_dma = 1;

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, ata, CTLFLAG_RD, 0, "ATA driver parameters");
TUNABLE_INT("hw.ata.ata_dma", &ata_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma, CTLFLAG_RDTUN, &ata_dma, 0,
	   "ATA disk DMA mode control");
TUNABLE_INT("hw.ata.atapi_dma", &atapi_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, atapi_dma, CTLFLAG_RDTUN, &atapi_dma, 0,
	   "ATAPI device DMA mode control");
TUNABLE_INT("hw.ata.wc", &ata_wc);
SYSCTL_INT(_hw_ata, OID_AUTO, wc, CTLFLAG_RDTUN, &ata_wc, 0,
	   "ATA disk write caching");

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

    /* check that we have a virgin channel to attach */
    if (ch->r_irq)
	return EEXIST;

    /* initialize the softc basics */
    ch->dev = dev;
    ch->state = ATA_IDLE;
    bzero(&ch->state_mtx, sizeof(struct mtx));
    mtx_init(&ch->state_mtx, "ATA state lock", NULL, MTX_DEF);
    bzero(&ch->queue_mtx, sizeof(struct mtx));
    mtx_init(&ch->queue_mtx, "ATA queue lock", NULL, MTX_DEF);
    TAILQ_INIT(&ch->ata_queue);

    /* reset the controller HW, the channel and device(s) */
    while (ATA_LOCKING(dev, ATA_LF_LOCK) != ch->unit)
	tsleep(&error, PRIBIO, "ataatch", 1);
    ATA_RESET(dev);
    ATA_LOCKING(dev, ATA_LF_UNLOCK);

    /* setup interrupt delivery */
    rid = ATA_IRQ_RID;
    ch->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
				       RF_SHAREABLE | RF_ACTIVE);
    if (!ch->r_irq) {
	device_printf(dev, "unable to allocate interrupt\n");
	return ENXIO;
    }
    if ((error = bus_setup_intr(dev, ch->r_irq, ATA_INTR_FLAGS,
				(driver_intr_t *)ata_interrupt, ch, &ch->ih))) {
	device_printf(dev, "unable to setup interrupt\n");
	return error;
    }

    /* probe and attach devices on this channel unless we are in early boot */
    if (!ata_delayed_attach)
	ata_identify(dev);
    return 0;
}

int
ata_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int nchildren, i;

    /* check that we have a valid channel to detach */
    if (!ch->r_irq)
	return ENXIO;

    /* detach & delete all children */
    if (!device_get_children(dev, &children, &nchildren)) {
	for (i = 0; i < nchildren; i++)
	    if (children[i])
		device_delete_child(dev, children[i]);
	free(children, M_TEMP);
    } 

    /* release resources */
    bus_teardown_intr(dev, ch->r_irq, ch->ih);
    bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, ch->r_irq);
    ch->r_irq = NULL;
    mtx_destroy(&ch->state_mtx);
    mtx_destroy(&ch->queue_mtx);
    return 0;
}

int
ata_reinit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_request *request;
    device_t *children;
    int nchildren, i;

    /* check that we have a valid channel to reinit */
    if (!ch || !ch->r_irq)
	return ENXIO;

    if (bootverbose)
	device_printf(dev, "reiniting channel ..\n");

    /* poll for locking the channel */
    while (ATA_LOCKING(dev, ATA_LF_LOCK) != ch->unit)
	tsleep(&dev, PRIBIO, "atarini", 1);

    /* unconditionally grap the channel lock */
    mtx_lock(&ch->state_mtx);
    ch->state = ATA_STALL_QUEUE;
    mtx_unlock(&ch->state_mtx);

    /* reset the controller HW, the channel and device(s) */
    ATA_RESET(dev);

    /* reinit the children and delete any that fails */
    if (!device_get_children(dev, &children, &nchildren)) {
	mtx_lock(&Giant);       /* newbus suckage it needs Giant */
	for (i = 0; i < nchildren; i++) {
	    if (children[i] && device_is_attached(children[i]))
		if (ATA_REINIT(children[i])) {
		    /*
		     * if we have a running request and its device matches
		     * this child we need to inform the request that the 
		     * device is gone and remove it from ch->running
		     */
		    mtx_lock(&ch->state_mtx);
		    if (ch->running && ch->running->dev == children[i]) {
			callout_stop(&ch->running->callout);
			request = ch->running;
    			ch->running = NULL;
		    }
		    else
			request = NULL;
    		    mtx_unlock(&ch->state_mtx);

		    if (request) {
			request->result = ENXIO;
			device_printf(request->dev,
				      "FAILURE - device detached\n");

			/* if not timeout finish request here */
			if (!(request->flags & ATA_R_TIMEOUT))
			    ata_finish(request);
                    }
		    device_delete_child(dev, children[i]);
		}
	}
	free(children, M_TEMP);
	mtx_unlock(&Giant);     /* newbus suckage dealt with, release Giant */
    }

    /* catch request in ch->running if we havn't already */
    mtx_lock(&ch->state_mtx);
    if ((request = ch->running))
	callout_stop(&request->callout);
    ch->running = NULL;
    mtx_unlock(&ch->state_mtx);

    /* if we got one put it on the queue again */
    if (request) {
	device_printf(request->dev,
		      "WARNING - %s requeued due to channel reset",
		      ata_cmd2str(request));
	if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
	    printf(" LBA=%llu", (unsigned long long)request->u.ata.lba);
	printf("\n");
	request->flags |= ATA_R_REQUEUE;
	ata_queue_request(request);
    }

    /* we're done release the channel for new work */
    mtx_lock(&ch->state_mtx);
    ch->state = ATA_IDLE;
    mtx_unlock(&ch->state_mtx);
    ATA_LOCKING(dev, ATA_LF_UNLOCK);

    if (bootverbose)
	device_printf(dev, "reinit done ..\n");

    /* kick off requests on the queue */
    ata_start(dev);
    return 0;
}

int
ata_suspend(device_t dev)
{
    struct ata_channel *ch;

    /* check for valid device */
    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    /* wait for the channel to be IDLE before entering suspend mode */
    while (1) {
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
    return 0;
}

int
ata_resume(device_t dev)
{
    struct ata_channel *ch;
    int error;

    /* check for valid device */
    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    /* reinit the devices, we dont know what mode/state they are in */
    error = ata_reinit(dev);

    /* kick off requests on the queue */
    ata_start(dev);
    return error;
}

int
ata_interrupt(void *data)
{
    struct ata_channel *ch = (struct ata_channel *)data;
    struct ata_request *request;

    mtx_lock(&ch->state_mtx);
    do {
	/* ignore interrupt if its not for us */
	if (ch->hw.status && !ch->hw.status(ch->dev))
	    break;

	/* do we have a running request */
	if (!(request = ch->running))
	    break;

	ATA_DEBUG_RQ(request, "interrupt");

	/* safetycheck for the right state */
	if (ch->state != ATA_ACTIVE && ch->state != ATA_STALL_QUEUE) {
	    device_printf(request->dev, "interrupt on idle channel ignored\n");
	    break;
	}

	/*
	 * we have the HW locks, so end the tranaction for this request
	 * if it finishes immediately otherwise wait for next interrupt
	 */
	if (ch->hw.end_transaction(request) == ATA_OP_FINISHED) {
	    ch->running = NULL;
	    if (ch->state == ATA_ACTIVE)
		ch->state = ATA_IDLE;
	    mtx_unlock(&ch->state_mtx);
	    ATA_LOCKING(ch->dev, ATA_LF_UNLOCK);
	    ata_finish(request);
	    return 1;
	}
    } while (0);
    mtx_unlock(&ch->state_mtx);
    return 0;
}

/*
 * device related interfaces
 */
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
	*value = devclass_get_maxunit(ata_devclass);
	error = 0;
	break;

    case IOCATAREINIT:
	if (*value > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)))
	    return ENXIO;
	error = ata_reinit(device);
	ata_start(device);
	break;

    case IOCATAATTACH:
	if (*value > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)))
	    return ENXIO;
	/* XXX SOS should enable channel HW on controller */
	error = ata_attach(device);
	break;

    case IOCATADETACH:
	if (*value > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, *value)))
	    return ENXIO;
	error = ata_detach(device);
	/* XXX SOS should disable channel HW on controller */
	break;

    case IOCATADEVICES:
	if (devices->channel > devclass_get_maxunit(ata_devclass) ||
	    !(device = devclass_get_device(ata_devclass, devices->channel)))
	    return ENXIO;
	bzero(devices->name[0], 32);
	bzero(&devices->params[0], sizeof(struct ata_params));
	bzero(devices->name[1], 32);
	bzero(&devices->params[1], sizeof(struct ata_params));
	if (!device_get_children(device, &children, &nchildren)) {
	    for (i = 0; i < nchildren; i++) {
		if (children[i] && device_is_attached(children[i])) {
		    struct ata_device *atadev = device_get_softc(children[i]);

		    if (atadev->unit == ATA_MASTER) {
			strncpy(devices->name[0],
				device_get_nameunit(children[i]), 32);
			bcopy(&atadev->param, &devices->params[0],
			      sizeof(struct ata_params));
		    }
		    if (atadev->unit == ATA_SLAVE) {
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

int
ata_device_ioctl(device_t dev, u_long cmd, caddr_t data)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct ata_ioc_request *ioc_request = (struct ata_ioc_request *)data;
    struct ata_params *params = (struct ata_params *)data;
    int *mode = (int *)data;
    struct ata_request *request;
    caddr_t buf;
    int error;

    switch (cmd) {
    case IOCATAREQUEST:
	if (!(buf = malloc(ioc_request->count, M_ATA, M_NOWAIT))) {
	    return ENOMEM;
	}
	if (!(request = ata_alloc_request())) {
	    free(buf, M_ATA);
	    return  ENOMEM;
	}
	if (ioc_request->flags & ATA_CMD_WRITE) {
	    error = copyin(ioc_request->data, buf, ioc_request->count);
	    if (error) {
		free(buf, M_ATA);
		ata_free_request(request);
		return error;
	    }
	}
	request->dev = dev;
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
	if (!(request->flags & ATA_R_ATAPI)) {
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
	ATA_SETMODE(device_get_parent(dev), dev);
	return 0;

    case IOCATAGMODE:
	*mode = atadev->mode;
	return 0;
    default:
	return ENOTTY;
    }
}

static void
ata_boot_attach(void)
{
    struct ata_channel *ch;
    int ctlr;

    mtx_lock(&Giant);       /* newbus suckage it needs Giant */

    /* kick of probe and attach on all channels */
    for (ctlr = 0; ctlr < devclass_get_maxunit(ata_devclass); ctlr++) {
	if ((ch = devclass_get_softc(ata_devclass, ctlr))) {
	    ata_identify(ch->dev);
	}
    }

    /* release the hook that got us here, we are only needed once during boot */
    if (ata_delayed_attach) {
	config_intrhook_disestablish(ata_delayed_attach);
	ata_delayed_attach = NULL;
	free(ata_delayed_attach, M_TEMP);
    }

    mtx_unlock(&Giant);     /* newbus suckage dealt with, release Giant */
}


/*
 * misc support functions
 */
static device_t
ata_add_child(device_t parent, struct ata_device *atadev, int unit)
{
    device_t child;

    if ((child = device_add_child(parent, NULL, unit))) {
	device_set_softc(child, atadev);
	device_quiet(child);
	atadev->dev = child;
	atadev->max_iosize = DEV_BSIZE;
	atadev->mode = ATA_PIO_MAX;
    }
    return child;
}

static int
ata_getparam(struct ata_device *atadev, int init)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(atadev->dev));
    struct ata_request *request;
    u_int8_t command = 0;
    int error = ENOMEM, retries = 2;

    if (ch->devices &
	(atadev->unit == ATA_MASTER ? ATA_ATA_MASTER : ATA_ATA_SLAVE))
	command = ATA_ATA_IDENTIFY;
    if (ch->devices &
	(atadev->unit == ATA_MASTER ? ATA_ATAPI_MASTER : ATA_ATAPI_SLAVE))
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
	request->flags = (ATA_R_READ|ATA_R_AT_HEAD|ATA_R_DIRECT|ATA_R_QUIET);
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
	char buffer[64];
#if BYTE_ORDER == BIG_ENDIAN
	int16_t *ptr;

	for (ptr = (int16_t *)atacap;
	     ptr < (int16_t *)atacap + sizeof(struct ata_params)/2; ptr++) {
	    *ptr = bswap16(*ptr);
	}
#endif
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
		   ch->unit, atadev->unit == ATA_MASTER ? "master":"slave",
		   ata_mode2str(ata_pmode(atacap)),
		   ata_mode2str(ata_wmode(atacap)),
		   ata_mode2str(ata_umode(atacap)),
		   (atacap->hwres & ATA_CABLE_ID) ? "80":"40");

	if (init) {
	    sprintf(buffer, "%.40s/%.8s", atacap->model, atacap->revision);
	    device_set_desc_copy(atadev->dev, buffer);
	    if (atadev->param.config & ATA_PROTO_ATAPI) {
		if (atapi_dma && ch->dma &&
		    (atadev->param.config & ATA_DRQ_MASK) != ATA_DRQ_INTR &&
		    ata_umode(&atadev->param) >= ATA_UDMA2)
		    atadev->mode = ATA_DMA_MAX;
	    }
	    else {
		if (ata_dma && ch->dma &&
		    (ata_umode(&atadev->param) > 0 ||
		     ata_wmode(&atadev->param) > 0))
		    atadev->mode = ATA_DMA_MAX;
	    }
	}
    }
    else {
	if (!error)
	    error = ENXIO;
    }
    return error;
}

int
ata_identify(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_device *master = NULL, *slave = NULL;
    device_t master_child = NULL, slave_child = NULL;
    int master_unit = -1, slave_unit = -1;

    if (ch->devices & (ATA_ATA_MASTER | ATA_ATAPI_MASTER)) {
	if (!(master = malloc(sizeof(struct ata_device),
			      M_ATA, M_NOWAIT | M_ZERO))) {
	    device_printf(dev, "out of memory\n");
	    return ENOMEM;
	}
	master->unit = ATA_MASTER;
    }
    if (ch->devices & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE)) {
	if (!(slave = malloc(sizeof(struct ata_device),
			     M_ATA, M_NOWAIT | M_ZERO))) {
	    free(master, M_ATA);
	    device_printf(dev, "out of memory\n");
	    return ENOMEM;
	}
	slave->unit = ATA_SLAVE;
    }

#ifdef ATA_STATIC_ID
    if (ch->devices & ATA_ATA_MASTER)
	master_unit = (device_get_unit(dev) << 1);
#endif
    if (master && !(master_child = ata_add_child(dev, master, master_unit))) {
	free(master, M_ATA);
	master = NULL;
    }
#ifdef ATA_STATIC_ID
    if (ch->devices & ATA_ATA_SLAVE)
	slave_unit = (device_get_unit(dev) << 1) + 1;
#endif
    if (slave && !(slave_child = ata_add_child(dev, slave, slave_unit))) {
	free(slave, M_ATA);
	slave = NULL;
    }

    if (slave && ata_getparam(slave, 1)) {
	device_delete_child(dev, slave_child);
	free(slave, M_ATA);
    }
    if (master && ata_getparam(master, 1)) {
	device_delete_child(dev, master_child);
	free(master, M_ATA);
    }

    bus_generic_probe(dev);
    bus_generic_attach(dev);
    return 0;
}

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

void
ata_modify_if_48bit(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);

    atadev->flags &= ~ATA_D_48BIT_ACTIVE;

    if ((request->u.ata.lba >= ATA_MAX_28BIT_LBA ||
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
	case ATA_READ_NATIVE_MAX_ADDDRESS:
	    request->u.ata.command = ATA_READ_NATIVE_MAX_ADDDRESS48;
	    break;
	case ATA_SET_MAX_ADDRESS:
	    request->u.ata.command = ATA_SET_MAX_ADDRESS48;
	    break;
	default:
	    return;
	}
	atadev->flags |= ATA_D_48BIT_ACTIVE;
    }
}

void
ata_udelay(int interval)
{
    /* for now just use DELAY, the timer/sleep subsytems are not there yet */
    if (1 || interval < (1000000/hz) || ata_delayed_attach)
	DELAY(interval);
    else
	tsleep(&interval, PRIBIO, "ataslp", interval/(1000000/hz));
}

char *
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
	if (!*ptr || *ptr == '_')
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


/*
 * module handeling
 */
static int
ata_module_event_handler(module_t mod, int what, void *arg)
{
    static struct cdev *atacdev;

    switch (what) {
    case MOD_LOAD:
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
	return 0;

    case MOD_UNLOAD:
	/* deregister controlling device */
	destroy_dev(atacdev);
	return 0;

    default:
	return EOPNOTSUPP;
    }
}

static moduledata_t ata_moduledata = { "ata", ata_module_event_handler, NULL };
DECLARE_MODULE(ata, ata_moduledata, SI_SUB_CONFIGURE, SI_ORDER_SECOND);
MODULE_VERSION(ata, 1);

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
