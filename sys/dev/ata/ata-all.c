/*-
 * Copyright (c) 1998 - 2004 Søren Schmidt <sos@FreeBSD.org>
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
#include <sys/ata.h>
#include <sys/kernel.h>
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
#include <geom/geom_disk.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>

/* device structures */
static	d_ioctl_t	ata_ioctl;
static struct cdevsw ata_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_ioctl =	ata_ioctl,
	.d_name =	"ata",
};

/* prototypes */
static void ata_shutdown(void *, int);
static void ata_interrupt(void *);
static int ata_getparam(struct ata_device *, u_int8_t);
static void ata_identify_devices(struct ata_channel *);
static void ata_boot_attach(void);
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);
static void ata_init(void);

/* global vars */
MALLOC_DEFINE(M_ATA, "ATA generic", "ATA driver generic layer");
devclass_t ata_devclass;
uma_zone_t ata_zone;
int ata_wc = 1;

/* local vars */
static struct intr_config_hook *ata_delayed_attach = NULL;
static int ata_dma = 1;
static int atapi_dma = 1;
static int ata_resuming = 0;

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, ata, CTLFLAG_RD, 0, "ATA driver parameters");
TUNABLE_INT("hw.ata.ata_dma", &ata_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma, CTLFLAG_RDTUN, &ata_dma, 0,
	   "ATA disk DMA mode control");
TUNABLE_INT("hw.ata.wc", &ata_wc);
SYSCTL_INT(_hw_ata, OID_AUTO, wc, CTLFLAG_RDTUN, &ata_wc, 0,
	   "ATA disk write caching");
TUNABLE_INT("hw.ata.atapi_dma", &atapi_dma);
SYSCTL_INT(_hw_ata, OID_AUTO, atapi_dma, CTLFLAG_RDTUN, &atapi_dma, 0,
	   "ATAPI device DMA mode control");

/*
 * newbus device interface related functions
 */
int
ata_probe(device_t dev)
{
    struct ata_channel *ch;

    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    if (ch->r_irq)
	return EEXIST;

    return 0;
}

int
ata_attach(device_t dev)
{
    struct ata_channel *ch;
    int error, rid;

    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    /* initialize the softc basics */
    ch->device[MASTER].channel = ch;
    ch->device[MASTER].unit = ATA_MASTER;
    ch->device[MASTER].mode = ATA_PIO;
    ch->device[SLAVE].channel = ch;
    ch->device[SLAVE].unit = ATA_SLAVE;
    ch->device[SLAVE].mode = ATA_PIO;
    ch->dev = dev;
    ch->state = ATA_IDLE;
    bzero(&ch->state_mtx, sizeof(struct mtx));
    mtx_init(&ch->state_mtx, "ATA state lock", NULL, MTX_DEF);

    /* initialise device(s) on this channel */
    while (ch->locking(ch, ATA_LF_LOCK) != ch->unit)
	tsleep(&error, PRIBIO, "ataatch", 1);
    ch->hw.reset(ch);
    ch->locking(ch, ATA_LF_UNLOCK);

    rid = ATA_IRQ_RID;
    ch->r_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
				       RF_SHAREABLE | RF_ACTIVE);
    if (!ch->r_irq) {
	ata_printf(ch, -1, "unable to allocate interrupt\n");
	return ENXIO;
    }
    if ((error = bus_setup_intr(dev, ch->r_irq, ATA_INTR_FLAGS,
				ata_interrupt, ch, &ch->ih))) {
	ata_printf(ch, -1, "unable to setup interrupt\n");
	return error;
    }

    /* initialize queue and associated lock */
    bzero(&ch->queue_mtx, sizeof(struct mtx));
    mtx_init(&ch->queue_mtx, "ATA queue lock", NULL, MTX_DEF);
    TAILQ_INIT(&ch->ata_queue);

    /* do not attach devices if we are in early boot */
    if (ata_delayed_attach)
	return 0;

    ata_identify_devices(ch);

    if (ch->device[MASTER].attach)
	ch->device[MASTER].attach(&ch->device[MASTER]);
    if (ch->device[SLAVE].attach)
	ch->device[SLAVE].attach(&ch->device[SLAVE]);
#ifdef DEV_ATAPICAM
    atapi_cam_attach_bus(ch);
#endif
    return 0;
}

int
ata_detach(device_t dev)
{
    struct ata_channel *ch;

    if (!dev || !(ch = device_get_softc(dev)) || !ch->r_irq)
	return ENXIO;

    /* mark devices on this channel as detaching */
    ch->device[MASTER].flags |= ATA_D_DETACHING;
    ch->device[SLAVE].flags |= ATA_D_DETACHING;

    /* fail outstanding requests on this channel */
    ata_fail_requests(ch, NULL);

    /* unlock the channel */
    mtx_lock(&ch->state_mtx);
    ch->state = ATA_IDLE;
    mtx_unlock(&ch->state_mtx);
    ch->locking(ch, ATA_LF_UNLOCK);

    /* detach devices on this channel */
    if (ch->device[MASTER].detach)
	ch->device[MASTER].detach(&ch->device[MASTER]);
    if (ch->device[SLAVE].detach)
	ch->device[SLAVE].detach(&ch->device[SLAVE]);
#ifdef DEV_ATAPICAM
    atapi_cam_detach_bus(ch);
#endif

    /* flush cache and powerdown device */
    if (ch->device[MASTER].param) {
	if (ch->device[MASTER].param->support.command2 & ATA_SUPPORT_FLUSHCACHE)
	    ata_controlcmd(&ch->device[MASTER], ATA_FLUSHCACHE, 0, 0, 0);
	ata_controlcmd(&ch->device[MASTER], ATA_SLEEP, 0, 0, 0);
	free(ch->device[MASTER].param, M_ATA);
	ch->device[MASTER].param = NULL;
    }
    if (ch->device[SLAVE].param) {
	if (ch->device[SLAVE].param->support.command2 & ATA_SUPPORT_FLUSHCACHE)
	    ata_controlcmd(&ch->device[SLAVE], ATA_FLUSHCACHE, 0, 0, 0);
	ata_controlcmd(&ch->device[SLAVE], ATA_SLEEP, 0, 0, 0);
	free(ch->device[SLAVE].param, M_ATA);
	ch->device[SLAVE].param = NULL;
    }
    ch->device[MASTER].mode = ATA_PIO;
    ch->device[SLAVE].mode = ATA_PIO;
    ch->devices = 0;

    bus_teardown_intr(dev, ch->r_irq, ch->ih);
    bus_release_resource(dev, SYS_RES_IRQ, ATA_IRQ_RID, ch->r_irq);
    ch->r_irq = NULL;
    mtx_destroy(&ch->queue_mtx);
    return 0;
}

int
ata_reinit(struct ata_channel *ch)
{
    int devices, misdev, newdev;

    if (!ch->r_irq)
	return ENXIO;

    if (bootverbose)
	ata_printf(ch, -1, "reiniting channel ..\n");

    /* poll for locking of this channel */
    while (ch->locking(ch, ATA_LF_LOCK) != ch->unit)
	tsleep(&devices, PRIBIO, "atarint", 1);

    ata_catch_inflight(ch);

    /* grap the channel lock no matter what */
    mtx_lock(&ch->state_mtx);
    ch->state = ATA_ACTIVE;
    mtx_unlock(&ch->state_mtx);

    if (ch->flags & ATA_IMMEDIATE_MODE)
	return EIO;
    else
	ch->flags |= ATA_IMMEDIATE_MODE;

    devices = ch->devices;

    ch->hw.reset(ch);

    if (bootverbose)
	ata_printf(ch, -1, "resetting done ..\n");

    /* detach what left the channel during reset */
    if ((misdev = devices & ~ch->devices)) {
	if ((misdev & (ATA_ATA_MASTER | ATA_ATAPI_MASTER)) &&
	    ch->device[MASTER].detach) {
	    ata_fail_requests(ch, &ch->device[MASTER]);
	    ch->device[MASTER].detach(&ch->device[MASTER]);
	    free(ch->device[MASTER].param, M_ATA);
	    ch->device[MASTER].param = NULL;
	}
	if ((misdev & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE)) &&
	    ch->device[SLAVE].detach) {
	    ata_fail_requests(ch, &ch->device[SLAVE]);
	    ch->device[SLAVE].detach(&ch->device[SLAVE]);
	    free(ch->device[SLAVE].param, M_ATA);
	    ch->device[SLAVE].param = NULL;
	}
    }

    /* identify what is present on the channel now */
    ata_identify_devices(ch);

    /* detach what left the channel during identify */
    if ((misdev = devices & ~ch->devices)) {
	if ((misdev & (ATA_ATA_MASTER | ATA_ATAPI_MASTER)) &&
	    ch->device[MASTER].detach) {
	    ata_fail_requests(ch, &ch->device[MASTER]);
	    ch->device[MASTER].detach(&ch->device[MASTER]);
	    free(ch->device[MASTER].param, M_ATA);
	    ch->device[MASTER].param = NULL;
	}
	if ((misdev & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE)) &&
	    ch->device[SLAVE].detach) {
	    ata_fail_requests(ch, &ch->device[SLAVE]);
	    ch->device[SLAVE].detach(&ch->device[SLAVE]);
	    free(ch->device[SLAVE].param, M_ATA);
	    ch->device[SLAVE].param = NULL;
	}
    }

    ch->flags &= ~ATA_IMMEDIATE_MODE;
    mtx_lock(&ch->state_mtx);
    ch->state = ATA_IDLE;
    mtx_unlock(&ch->state_mtx);
    ch->locking(ch, ATA_LF_UNLOCK);

    /* attach new devices */
    if ((newdev = ~devices & ch->devices)) {
	if ((newdev & (ATA_ATA_MASTER | ATA_ATAPI_MASTER)) &&
	    ch->device[MASTER].attach)
	    ch->device[MASTER].attach(&ch->device[MASTER]);
	if ((newdev & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE)) &&
	    ch->device[SLAVE].attach)
	    ch->device[SLAVE].attach(&ch->device[SLAVE]);
    }

#ifdef DEV_ATAPICAM
    atapi_cam_reinit_bus(ch);
#endif

    if (bootverbose)
	ata_printf(ch, -1, "device config done ..\n");

    ata_start(ch);
    return 0;
}

int
ata_suspend(device_t dev)
{
    struct ata_channel *ch;

    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

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
    ch->locking(ch, ATA_LF_UNLOCK);
    return 0;
}

int
ata_resume(device_t dev)
{
    struct ata_channel *ch;
    int error;

    if (!dev || !(ch = device_get_softc(dev)))
	return ENXIO;

    ata_resuming = 1;
    error = ata_reinit(ch);
    ata_start(ch);
    ata_resuming = 0;
    return error;
}

static void
ata_shutdown(void *arg, int howto)
{
    struct ata_channel *ch;
    int ctlr;

    /* flush cache on all devices */
    for (ctlr = 0; ctlr < devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(ch = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (ch->device[MASTER].param &&
	    ch->device[MASTER].param->support.command2 & ATA_SUPPORT_FLUSHCACHE)
	    ata_controlcmd(&ch->device[MASTER], ATA_FLUSHCACHE, 0, 0, 0);
	if (ch->device[SLAVE].param &&
	    ch->device[SLAVE].param->support.command2 & ATA_SUPPORT_FLUSHCACHE)
	    ata_controlcmd(&ch->device[SLAVE], ATA_FLUSHCACHE, 0, 0, 0);
    }
}

static void
ata_interrupt(void *data)
{
    struct ata_channel *ch = (struct ata_channel *)data;
    struct ata_request *request;

    mtx_lock(&ch->state_mtx);
    do {
	/* do we have a running request */
	if (!(request = ch->running))
	    break;

	ATA_DEBUG_RQ(request, "interrupt");

	/* ignore interrupt if device is busy */
	if (ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_BUSY) {
	    DELAY(100);
	    if (ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_BUSY)
		break;
	}

	/* check for the right state */
	if (ch->state == ATA_ACTIVE) {
	    request->flags |= ATA_R_INTR_SEEN;
	    ch->state = ATA_INTERRUPT;
	}
	else {
	    ata_prtdev(request->device,
		       "interrupt state=%d unexpected\n", ch->state);
	    break;
	}

	if (ch->hw.end_transaction(request) == ATA_OP_FINISHED) {
	    ch->running = NULL;
	    if (ch->flags & ATA_IMMEDIATE_MODE)
		ch->state = ATA_ACTIVE;
	    else
		ch->state = ATA_IDLE;
	    mtx_unlock(&ch->state_mtx);
	    ch->locking(ch, ATA_LF_UNLOCK);
	    ata_finish(request);
	    return;
	}
	else {
	    request->flags &= ~ATA_R_INTR_SEEN;
	    ch->state = ATA_ACTIVE;
	}
    } while (0);
    mtx_unlock(&ch->state_mtx);
}

/*
 * device related interfaces
 */
static int
ata_ioctl(struct cdev *dev, u_long cmd, caddr_t addr,
	  int32_t flag, struct thread *td)
{
    struct ata_cmd *iocmd = (struct ata_cmd *)addr;
    device_t device = devclass_get_device(ata_devclass, iocmd->channel);
    struct ata_channel *ch;
    struct ata_device *atadev;
    struct ata_request *request;
    caddr_t buf;
    int error = ENOTTY;

    if (cmd != IOCATA)
	return error;

    DROP_GIANT();
    switch (iocmd->cmd) {
    case ATAGMAXCHANNEL:
	iocmd->u.maxchan = devclass_get_maxunit(ata_devclass);
	error = 0;
	break;

    case ATAGPARM:
	if (!device || !(ch = device_get_softc(device))) {
	    error = ENXIO;
	    break;
	}
	iocmd->u.param.type[MASTER] =
	    ch->devices & (ATA_ATA_MASTER | ATA_ATAPI_MASTER);
	iocmd->u.param.type[SLAVE] =
	    ch->devices & (ATA_ATA_SLAVE | ATA_ATAPI_SLAVE);
	if (ch->device[MASTER].name)
	    strcpy(iocmd->u.param.name[MASTER], ch->device[MASTER].name);
	if (ch->device[SLAVE].name)
	    strcpy(iocmd->u.param.name[SLAVE], ch->device[SLAVE].name);
	if (ch->device[MASTER].param)
	    bcopy(ch->device[MASTER].param, &iocmd->u.param.params[MASTER],
		  sizeof(struct ata_params));
	if (ch->device[SLAVE].param)
	    bcopy(ch->device[SLAVE].param, &iocmd->u.param.params[SLAVE],
		  sizeof(struct ata_params));
	error = 0;
	break;

    case ATAGMODE:
	if (!device || !(ch = device_get_softc(device))) {
	    error = ENXIO;
	    break;
	}
	iocmd->u.mode.mode[MASTER] = ch->device[MASTER].mode;
	iocmd->u.mode.mode[SLAVE] = ch->device[SLAVE].mode;
	error = 0;
	break;

    case ATASMODE:
	if (!device || !(ch = device_get_softc(device))) {
	    error = ENXIO;
	    break;
	}
	if (iocmd->u.mode.mode[MASTER] >= 0 && ch->device[MASTER].param)
	    ch->device[MASTER].setmode(&ch->device[MASTER],
				       iocmd->u.mode.mode[MASTER]);
	iocmd->u.mode.mode[MASTER] = ch->device[MASTER].mode;
	if (iocmd->u.mode.mode[SLAVE] >= 0 && ch->device[SLAVE].param)
	    ch->device[SLAVE].setmode(&ch->device[SLAVE],
				      iocmd->u.mode.mode[SLAVE]);
	iocmd->u.mode.mode[SLAVE] = ch->device[SLAVE].mode;
	error = 0;
	break;

   case ATAREQUEST:
	if (!device || !(ch = device_get_softc(device))) {
	    error = ENXIO;
	    break;
	}
	if (!(atadev = &ch->device[iocmd->device])) {
	    error = ENODEV;
	    break;
	}
	if (!(buf = malloc(iocmd->u.request.count, M_ATA, M_NOWAIT))) {
	    error = ENOMEM;
	    break;
	}
	if (!(request = ata_alloc_request())) {
	    error = ENOMEM;
	    free(buf, M_ATA);
	    break;
	}
	if (iocmd->u.request.flags & ATA_CMD_WRITE) {
	    error = copyin(iocmd->u.request.data, buf, iocmd->u.request.count);
	    if (error) {
		free(buf, M_ATA);
		ata_free_request(request);
		break;
	    }
	}

	request->device = atadev;

	if (iocmd->u.request.flags & ATA_CMD_ATAPI) {
	    request->flags = ATA_R_ATAPI;
	    bcopy(iocmd->u.request.u.atapi.ccb, request->u.atapi.ccb, 16);
	}
	else {
	     request->u.ata.command = iocmd->u.request.u.ata.command;
	     request->u.ata.feature = iocmd->u.request.u.ata.feature;
	     request->u.ata.lba = iocmd->u.request.u.ata.lba;
	     request->u.ata.count = iocmd->u.request.u.ata.count;
	}

	request->timeout = iocmd->u.request.timeout;
	request->data = buf;
	request->bytecount = iocmd->u.request.count;
	request->transfersize = request->bytecount;

	if (iocmd->u.request.flags & ATA_CMD_CONTROL)
	    request->flags |= ATA_R_CONTROL;
	if (iocmd->u.request.flags & ATA_CMD_READ)
	    request->flags |= ATA_R_READ;
	if (iocmd->u.request.flags & ATA_CMD_WRITE)
	    request->flags |= ATA_R_WRITE;

	ata_queue_request(request);

	if (request->result)
	    iocmd->u.request.error = request->result;
	else {
	    if (iocmd->u.request.flags & ATA_CMD_READ)
		error = copyout(buf,
				iocmd->u.request.data, iocmd->u.request.count);
	    else
		error = 0;
	}
	free(buf, M_ATA);
	ata_free_request(request);
	break;

    case ATAREINIT:
	if (!device || !(ch = device_get_softc(device))) {
	    error = ENXIO;
	    break;
	}
	error = ata_reinit(ch);
	ata_start(ch);
	break;

    case ATAATTACH:
	if (!device) {
	    error =  ENXIO;
	    break;
	}
	/* SOS should enable channel HW on controller XXX */
	error = ata_probe(device);
	if (!error)
	    error = ata_attach(device);
	break;

    case ATADETACH:
	if (!device) {
	    error = ENXIO;
	    break;
	}
	error = ata_detach(device);
	/* SOS should disable channel HW on controller XXX */
	break;


#ifdef DEV_ATARAID
    case ATARAIDCREATE:
	error = ata_raid_create(&iocmd->u.raid_setup);
	break;

    case ATARAIDDELETE:
	error = ata_raid_delete(iocmd->channel);
	break;

    case ATARAIDSTATUS:
	error = ata_raid_status(iocmd->channel, &iocmd->u.raid_status);
	break;

    case ATARAIDADDSPARE:
	error = ata_raid_addspare(iocmd->channel, iocmd->u.raid_spare.disk);
	break;

    case ATARAIDREBUILD:
	error = ata_raid_rebuild(iocmd->channel);
	break;
#endif
    }
    PICKUP_GIANT();
    return error;
}

/*
 * device probe functions
 */
static int
ata_getparam(struct ata_device *atadev, u_int8_t command)
{
    struct ata_request *request;
    int error = ENOMEM;

    if (!atadev->param)
	atadev->param = malloc(sizeof(struct ata_params), M_ATA, M_NOWAIT);
    if (atadev->param) {
	request = ata_alloc_request();
	if (request) {
	    int retries = 2;
	    while (retries-- > 0) {
		request->device = atadev;
		request->timeout = 5;
		request->retries = 0;
		request->u.ata.command = command;
		request->flags = (ATA_R_READ | ATA_R_IMMEDIATE);
		request->data = (caddr_t)atadev->param;
		request->bytecount = sizeof(struct ata_params);
		request->donecount = 0;
		request->transfersize = DEV_BSIZE;
		ata_queue_request(request);
		if (!(error = request->result))
		    break;
	    }
	    ata_free_request(request);
	}
	if (!error && (isprint(atadev->param->model[0]) ||
		       isprint(atadev->param->model[1]))) {
	    struct ata_params *atacap = atadev->param;
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
		ata_prtdev(atadev,
			   "pio=0x%02x wdma=0x%02x udma=0x%02x cable=%spin\n",
			   ata_pmode(atacap), ata_wmode(atacap),
			   ata_umode(atacap),
			   (atacap->hwres & ATA_CABLE_ID) ? "80":"40");
	}
	else {
	    if (!error)
		error = ENXIO;
	    if (atadev->param) {
		free(atadev->param, M_ATA);
		atadev->param = NULL;
	    }
	}
    }
    return error;
}

static void
ata_identify_devices(struct ata_channel *ch)
{
    if (ch->devices & ATA_ATA_SLAVE) {
	if (ata_getparam(&ch->device[SLAVE], ATA_ATA_IDENTIFY))
	    ch->devices &= ~ATA_ATA_SLAVE;
#ifdef DEV_ATADISK
	else
	    ch->device[SLAVE].attach = ad_attach;
#endif
    }
    if (ch->devices & ATA_ATAPI_SLAVE) {
	if (ata_getparam(&ch->device[SLAVE], ATA_ATAPI_IDENTIFY))
	    ch->devices &= ~ATA_ATAPI_SLAVE;
	else {
	    ata_controlcmd(&ch->device[SLAVE], ATA_ATAPI_RESET, 0, 0, 0);
	    switch (ch->device[SLAVE].param->config & ATA_ATAPI_TYPE_MASK) {
#ifdef DEV_ATAPICD
	    case ATA_ATAPI_TYPE_CDROM:
		ch->device[SLAVE].attach = acd_attach;
		break;
#endif
#ifdef DEV_ATAPIFD
	    case ATA_ATAPI_TYPE_DIRECT:
		ch->device[SLAVE].attach = afd_attach;
		break;
#endif
#ifdef DEV_ATAPIST
	    case ATA_ATAPI_TYPE_TAPE:
		ch->device[SLAVE].attach = ast_attach;
		break;
#endif
	    }
	}
    }
    if (ch->devices & ATA_ATA_MASTER) {
	if (ata_getparam(&ch->device[MASTER], ATA_ATA_IDENTIFY))
	    ch->devices &= ~ATA_ATA_MASTER;
#ifdef DEV_ATADISK
	else
	    ch->device[MASTER].attach = ad_attach;
#endif
    }
    if (ch->devices & ATA_ATAPI_MASTER) {
	if (ata_getparam(&ch->device[MASTER], ATA_ATAPI_IDENTIFY))
	    ch->devices &= ~ATA_ATAPI_MASTER;
	else {
	    ata_controlcmd(&ch->device[MASTER], ATA_ATAPI_RESET, 0, 0, 0);
	    switch (ch->device[MASTER].param->config & ATA_ATAPI_TYPE_MASK) {
#ifdef DEV_ATAPICD
	    case ATA_ATAPI_TYPE_CDROM:
		ch->device[MASTER].attach = acd_attach;
		break;
#endif
#ifdef DEV_ATAPIFD
	    case ATA_ATAPI_TYPE_DIRECT:
		ch->device[MASTER].attach = afd_attach;
		break;
#endif
#ifdef DEV_ATAPIST
	    case ATA_ATAPI_TYPE_TAPE:
		ch->device[MASTER].attach = ast_attach;
		break;
#endif
	    }
	}
    }

    /* setup basic transfer mode by setting PIO mode and DMA if supported */
    if (ch->device[MASTER].param) {
	ch->device[MASTER].setmode(&ch->device[MASTER], ATA_PIO_MAX);
	if ((((ch->devices & ATA_ATAPI_MASTER) && atapi_dma &&
	      (ch->device[MASTER].param->config&ATA_DRQ_MASK) != ATA_DRQ_INTR &&
	      ata_umode(ch->device[MASTER].param) >= ATA_UDMA2) ||
	     ((ch->devices & ATA_ATA_MASTER) && ata_dma)) && ch->dma)
	    ch->device[MASTER].setmode(&ch->device[MASTER], ATA_DMA_MAX);

    }
    if (ch->device[SLAVE].param) {
	ch->device[SLAVE].setmode(&ch->device[SLAVE], ATA_PIO_MAX);
	if ((((ch->devices & ATA_ATAPI_SLAVE) && atapi_dma &&
	      (ch->device[SLAVE].param->config&ATA_DRQ_MASK) != ATA_DRQ_INTR &&
	      ata_umode(ch->device[SLAVE].param) >= ATA_UDMA2) ||
	     ((ch->devices & ATA_ATA_SLAVE) && ata_dma)) && ch->dma)
	    ch->device[SLAVE].setmode(&ch->device[SLAVE], ATA_DMA_MAX);
    }
}

static void
ata_boot_attach(void)
{
    struct ata_channel *ch;
    int ctlr;

    if (ata_delayed_attach) {
	config_intrhook_disestablish(ata_delayed_attach);
	free(ata_delayed_attach, M_TEMP);
	ata_delayed_attach = NULL;
    }

    /*
     * run through all ata devices and look for real ATA & ATAPI devices
     * using the hints we found in the early probe, this avoids some of
     * the delays probing of non-exsistent devices can cause.
     */
    for (ctlr=0; ctlr<devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(ch = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	ata_identify_devices(ch);
	if (ch->device[MASTER].attach)
	    ch->device[MASTER].attach(&ch->device[MASTER]);
	if (ch->device[SLAVE].attach)
	    ch->device[SLAVE].attach(&ch->device[SLAVE]);
#ifdef DEV_ATAPICAM
	atapi_cam_attach_bus(ch);
#endif
    }
#ifdef DEV_ATARAID
    ata_raid_attach();
#endif
}

/*
 * misc support functions
 */
void
ata_udelay(int interval)
{
    if (interval < (1000000/hz) || ata_delayed_attach || ata_resuming)
	DELAY(interval);
    else
	tsleep(&interval, PRIBIO, "ataslp", interval/(1000000/hz));
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

int
ata_printf(struct ata_channel *ch, int device, const char * fmt, ...)
{
    va_list ap;
    int ret;

    if (device == -1)
	ret = printf("ata%d: ", device_get_unit(ch->dev));
    else {
	if (ch->device[ATA_DEV(device)].name)
	    ret = printf("%s: ", ch->device[ATA_DEV(device)].name);
	else
	    ret = printf("ata%d-%s: ", device_get_unit(ch->dev),
			 (device == ATA_MASTER) ? "master" : "slave");
    }
    va_start(ap, fmt);
    ret += vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

int
ata_prtdev(struct ata_device *atadev, const char * fmt, ...)
{
    va_list ap;
    int ret;

    if (atadev->name)
	ret = printf("%s: ", atadev->name);
    else
	ret = printf("ata%d-%s: ", device_get_unit(atadev->channel->dev),
		     (atadev->unit == ATA_MASTER) ? "master" : "slave");
    va_start(ap, fmt);
    ret += vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void
ata_set_name(struct ata_device *atadev, char *name, int lun)
{
    atadev->name = malloc(strlen(name) + 4, M_ATA, M_NOWAIT);
    if (atadev->name)
	sprintf(atadev->name, "%s%d", name, lun);
}

void
ata_free_name(struct ata_device *atadev)
{
    if (atadev->name)
	free(atadev->name, M_ATA);
    atadev->name = NULL;
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
    case ATA_DMA: return "BIOSDMA";
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
    default: return "???";
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
ata_limit_mode(struct ata_device *atadev, int mode, int maxmode)
{
    if (maxmode && mode > maxmode)
	mode = maxmode;

    if (mode >= ATA_UDMA0 && ata_umode(atadev->param) > 0)
	return min(mode, ata_umode(atadev->param));

    if (mode >= ATA_WDMA0 && ata_wmode(atadev->param) > 0)
	return min(mode, ata_wmode(atadev->param));

    if (mode > ata_pmode(atadev->param))
	return min(mode, ata_pmode(atadev->param));

    return mode;
}

static void
ata_init(void)
{
    /* register controlling device */
    make_dev(&ata_cdevsw, 0, UID_ROOT, GID_OPERATOR, 0600, "ata");

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

    /* register handler to flush write caches on shutdown */
    if ((EVENTHANDLER_REGISTER(shutdown_post_sync, ata_shutdown,
			       NULL, SHUTDOWN_PRI_DEFAULT)) == NULL)
	printf("ata: shutdown event registration failed!\n");

    /* init our UMA zone for ATA requests */
    ata_zone = uma_zcreate("ata_request", sizeof(struct ata_request),
			   NULL, NULL, NULL, NULL, 0, 0);
}
SYSINIT(atadev, SI_SUB_DRIVERS, SI_ORDER_SECOND, ata_init, NULL)
