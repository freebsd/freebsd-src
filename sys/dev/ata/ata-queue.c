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
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

#ifndef ATA_CAM
/* prototypes */
static void ata_completed(void *, int);
static void ata_sort_queue(struct ata_channel *ch, struct ata_request *request);
static const char *ata_skey2str(u_int8_t);
#endif

#ifndef ATA_CAM
void
ata_queue_request(struct ata_request *request)
{
    struct ata_channel *ch;
    struct ata_device *atadev = device_get_softc(request->dev);

    /* treat request as virgin (this might be an ATA_R_REQUEUE) */
    request->result = request->status = request->error = 0;

    /* Prepare paramers required by low-level code. */
    request->unit = atadev->unit;
    if (!(request->parent = device_get_parent(request->dev))) {
	request->result = ENXIO;
	if (request->callback)
	    (request->callback)(request);
	return;
    }
    if ((atadev->param.config & ATA_PROTO_MASK) == ATA_PROTO_ATAPI_16)
	request->flags |= ATA_R_ATAPI16;
    if ((atadev->param.config & ATA_DRQ_MASK) == ATA_DRQ_INTR)
	request->flags |= ATA_R_ATAPI_INTR;
    if ((request->flags & ATA_R_ATAPI) == 0)
	ata_modify_if_48bit(request);
    ch = device_get_softc(request->parent);
    callout_init_mtx(&request->callout, &ch->state_mtx, CALLOUT_RETURNUNLOCKED);
    if (!request->callback && !(request->flags & ATA_R_REQUEUE))
	sema_init(&request->done, 0, "ATA request done");

    /* in ATA_STALL_QUEUE state we call HW directly */
    if ((ch->state & ATA_STALL_QUEUE) && (request->flags & ATA_R_CONTROL)) {
	mtx_lock(&ch->state_mtx);
	ch->running = request;
	if (ch->hw.begin_transaction(request) == ATA_OP_FINISHED) {
	    ch->running = NULL;
	    if (!request->callback) 
		sema_destroy(&request->done);
	    mtx_unlock(&ch->state_mtx);
	    return;
	}
	mtx_unlock(&ch->state_mtx);
    }
    /* otherwise put request on the locked queue at the specified location */
    else  {
	mtx_lock(&ch->queue_mtx);
	if (request->flags & ATA_R_AT_HEAD)
	    TAILQ_INSERT_HEAD(&ch->ata_queue, request, chain);
	else if (request->flags & ATA_R_ORDERED)
	    ata_sort_queue(ch, request);
	else
	    TAILQ_INSERT_TAIL(&ch->ata_queue, request, chain);
	mtx_unlock(&ch->queue_mtx);
	ATA_DEBUG_RQ(request, "queued");
	ata_start(ch->dev);
    }

    /* if this is a requeued request callback/sleep we're done */
    if (request->flags & ATA_R_REQUEUE)
	return;

    /* if this is not a callback wait until request is completed */
    if (!request->callback) {
	ATA_DEBUG_RQ(request, "wait for completion");
	if (!dumping &&
	    sema_timedwait(&request->done, request->timeout * hz * 4)) {
	    callout_drain(&request->callout);
	    device_printf(request->dev,
			  "WARNING - %s taskqueue timeout "
			  "- completing request directly\n",
			  ata_cmd2str(request));
	    request->flags |= ATA_R_DANGER1;
	    ata_completed(request, 0);
	}
	sema_destroy(&request->done);
    }
}
#endif

#ifndef ATA_CAM
int
ata_controlcmd(device_t dev, u_int8_t command, u_int16_t feature,
	       u_int64_t lba, u_int16_t count)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct ata_request *request = ata_alloc_request();
    int error = ENOMEM;

    if (request) {
	request->dev = dev;
	request->u.ata.command = command;
	request->u.ata.lba = lba;
	request->u.ata.count = count;
	request->u.ata.feature = feature;
	request->flags = ATA_R_CONTROL;
	if (atadev->spindown_state) {
	    device_printf(dev, "request while spun down, starting.\n");
	    atadev->spindown_state = 0;
	    request->timeout = MAX(ATA_REQUEST_TIMEOUT, 31);
	} else {
	    request->timeout = ATA_REQUEST_TIMEOUT;
	}
	request->retries = 0;
	ata_queue_request(request);
	error = request->result;
	ata_free_request(request);
    }
    return error;
}
#endif

#ifndef ATA_CAM
int
ata_atapicmd(device_t dev, u_int8_t *ccb, caddr_t data,
	     int count, int flags, int timeout)
{
    struct ata_request *request = ata_alloc_request();
    int error = ENOMEM;

    if (request) {
	request->dev = dev;
	bcopy(ccb, request->u.atapi.ccb, 16);
	request->data = data;
	request->bytecount = count;
	request->transfersize = min(request->bytecount, 65534);
	request->flags = flags | ATA_R_ATAPI;
	request->timeout = timeout;
	request->retries = 0;
	ata_queue_request(request);
	error = request->result;
	ata_free_request(request);
    }
    return error;
}
#endif

#ifndef ATA_CAM
void
ata_start(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_request *request;
    struct ata_composite *cptr;
    int dependencies = 0;

    /* if we have a request on the queue try to get it running */
    mtx_lock(&ch->queue_mtx);
    if ((request = TAILQ_FIRST(&ch->ata_queue))) {

	/* we need the locking function to get the lock for this channel */
	if (ATA_LOCKING(dev, ATA_LF_LOCK) == ch->unit) {

	    /* check for composite dependencies */
	    if ((cptr = request->composite)) {
		mtx_lock(&cptr->lock);
		if ((request->flags & ATA_R_WRITE) &&
		    (cptr->wr_depend & cptr->rd_done) != cptr->wr_depend) {
		    dependencies = 1;
		}
		mtx_unlock(&cptr->lock);
	    }

	    /* check we are in the right state and has no dependencies */
	    mtx_lock(&ch->state_mtx);
	    if (ch->state == ATA_IDLE && !dependencies) {
		ATA_DEBUG_RQ(request, "starting");
		TAILQ_REMOVE(&ch->ata_queue, request, chain);
		ch->running = request;
		ch->state = ATA_ACTIVE;

		/* if we are the freezing point release it */
		if (ch->freezepoint == request)
		    ch->freezepoint = NULL;

		if (ch->hw.begin_transaction(request) == ATA_OP_FINISHED) {
		    ch->running = NULL;
		    ch->state = ATA_IDLE;
		    mtx_unlock(&ch->state_mtx);
		    mtx_unlock(&ch->queue_mtx);
		    ATA_LOCKING(dev, ATA_LF_UNLOCK);
		    ata_finish(request);
		    return;
		}
	    }
	    mtx_unlock(&ch->state_mtx);
	}
    }
    mtx_unlock(&ch->queue_mtx);
    if (dumping) {
	while (ch->running) {
	    ata_interrupt(ch);
	    DELAY(10);
	}
    }
}
#endif

#ifndef ATA_CAM
void
ata_finish(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(request->parent);

    /*
     * if in ATA_STALL_QUEUE state or request has ATA_R_DIRECT flags set
     * we need to call ata_complete() directly here (no taskqueue involvement)
     */
    if (dumping ||
	(ch->state & ATA_STALL_QUEUE) || (request->flags & ATA_R_DIRECT)) {
	ATA_DEBUG_RQ(request, "finish directly");
	ata_completed(request, 0);
    }
    else {
	/* put request on the proper taskqueue for completion */
	if (request->bio && !(request->flags & (ATA_R_THREAD | ATA_R_TIMEOUT))){
	    ATA_DEBUG_RQ(request, "finish bio_taskqueue");
	    bio_taskqueue(request->bio, (bio_task_t *)ata_completed, request);
	}
	else {
	    TASK_INIT(&request->task, 0, ata_completed, request);
	    ATA_DEBUG_RQ(request, "finish taskqueue_swi");
	    taskqueue_enqueue(taskqueue_swi, &request->task);
	}
    }
}
#endif

#ifndef ATA_CAM
static void
ata_completed(void *context, int dummy)
{
    struct ata_request *request = (struct ata_request *)context;
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_device *atadev = device_get_softc(request->dev);
    struct ata_composite *composite;

    if (request->flags & ATA_R_DANGER2) {
	device_printf(request->dev,
		      "WARNING - %s freeing taskqueue zombie request\n",
		      ata_cmd2str(request));
	request->flags &= ~(ATA_R_DANGER1 | ATA_R_DANGER2);
	ata_free_request(request);
	return;
    }
    if (request->flags & ATA_R_DANGER1)
	request->flags |= ATA_R_DANGER2;

    ATA_DEBUG_RQ(request, "completed entered");

    /* if we had a timeout, reinit channel and deal with the falldown */
    if (request->flags & ATA_R_TIMEOUT) {
	/*
	 * if the channel is still present and
	 * reinit succeeds and
	 * the device doesn't get detached and
	 * there are retries left we reinject this request
	 */
	if (ch && !ata_reinit(ch->dev) && !request->result &&
	    (request->retries-- > 0)) {
	    if (!(request->flags & ATA_R_QUIET)) {
		device_printf(request->dev,
			      "TIMEOUT - %s retrying (%d retr%s left)",
			      ata_cmd2str(request), request->retries,
			      request->retries == 1 ? "y" : "ies");
		if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		    printf(" LBA=%ju", request->u.ata.lba);
		printf("\n");
	    }
	    request->flags &= ~(ATA_R_TIMEOUT | ATA_R_DEBUG);
	    request->flags |= (ATA_R_AT_HEAD | ATA_R_REQUEUE);
	    ATA_DEBUG_RQ(request, "completed reinject");
	    ata_queue_request(request);
	    return;
	}

	/* ran out of good intentions so finish with error */
	if (!request->result) {
	    if (!(request->flags & ATA_R_QUIET)) {
		if (request->dev) {
		    device_printf(request->dev, "FAILURE - %s timed out",
				  ata_cmd2str(request));
		    if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
			printf(" LBA=%ju", request->u.ata.lba);
		    printf("\n");
		}
	    }
	    request->result = EIO;
	}
    }
    else if (!(request->flags & ATA_R_ATAPI) ){
	/* if this is a soft ECC error warn about it */
	/* XXX SOS we could do WARF here */
	if ((request->status & (ATA_S_CORR | ATA_S_ERROR)) == ATA_S_CORR) {
	    device_printf(request->dev,
			  "WARNING - %s soft error (ECC corrected)",
			  ata_cmd2str(request));
	    if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		printf(" LBA=%ju", request->u.ata.lba);
	    printf("\n");
	}

	/* if this is a UDMA CRC error we reinject if there are retries left */
	if (request->flags & ATA_R_DMA && request->error & ATA_E_ICRC) {
	    if (request->retries-- > 0) {
		device_printf(request->dev,
			      "WARNING - %s UDMA ICRC error (retrying request)",
			      ata_cmd2str(request));
		if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		    printf(" LBA=%ju", request->u.ata.lba);
		printf("\n");
		request->flags |= (ATA_R_AT_HEAD | ATA_R_REQUEUE);
		ata_queue_request(request);
		return;
	    }
	}
    }

    switch (request->flags & ATA_R_ATAPI) {

    /* ATA errors */
    default:
	if (!request->result && request->status & ATA_S_ERROR) {
	    if (!(request->flags & ATA_R_QUIET)) {
		device_printf(request->dev,
			      "FAILURE - %s status=%b error=%b", 
			      ata_cmd2str(request),
			      request->status, "\20\10BUSY\7READY\6DMA_READY"
			      "\5DSC\4DRQ\3CORRECTABLE\2INDEX\1ERROR",
			      request->error, "\20\10ICRC\7UNCORRECTABLE"
			      "\6MEDIA_CHANGED\5NID_NOT_FOUND"
			      "\4MEDIA_CHANGE_REQEST"
			      "\3ABORTED\2NO_MEDIA\1ILLEGAL_LENGTH");
		if ((request->flags & ATA_R_DMA) && request->dma &&
		    (request->dma->status & ATA_BMSTAT_ERROR))
		    printf(" dma=0x%02x", request->dma->status);
		if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		    printf(" LBA=%ju", request->u.ata.lba);
		printf("\n");
	    }
	    request->result = EIO;
	}
	break;

    /* ATAPI errors */
    case ATA_R_ATAPI:
	/* skip if result already set */
	if (request->result)
	    break;

	/* if we have a sensekey -> request sense from device */
	if ((request->error & ATA_E_ATAPI_SENSE_MASK) &&
	    (request->u.atapi.ccb[0] != ATAPI_REQUEST_SENSE)) {
	    static u_int8_t ccb[16] = { ATAPI_REQUEST_SENSE, 0, 0, 0,
					sizeof(struct atapi_sense),
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	    request->u.atapi.saved_cmd = request->u.atapi.ccb[0];
	    bcopy(ccb, request->u.atapi.ccb, 16);
	    request->data = (caddr_t)&request->u.atapi.sense;
	    request->bytecount = sizeof(struct atapi_sense);
	    request->donecount = 0;
	    request->transfersize = sizeof(struct atapi_sense);
	    request->timeout = ATA_REQUEST_TIMEOUT;
	    request->flags &= (ATA_R_ATAPI | ATA_R_QUIET | ATA_R_DEBUG);
	    request->flags |= (ATA_R_READ | ATA_R_AT_HEAD | ATA_R_REQUEUE);
	    ATA_DEBUG_RQ(request, "autoissue request sense");
	    ata_queue_request(request);
	    return;
	}

	switch (request->u.atapi.sense.key & ATA_SENSE_KEY_MASK) {
	case ATA_SENSE_RECOVERED_ERROR:
	    device_printf(request->dev, "WARNING - %s recovered error\n",
			  ata_cmd2str(request));
	    /* FALLTHROUGH */

	case ATA_SENSE_NO_SENSE:
	    request->result = 0;
	    break;

	case ATA_SENSE_NOT_READY: 
	    request->result = EBUSY;
	    break;

	case ATA_SENSE_UNIT_ATTENTION:
	    atadev->flags |= ATA_D_MEDIA_CHANGED;
	    request->result = EIO;
	    break;

	default:
	    request->result = EIO;
	    if (request->flags & ATA_R_QUIET)
		break;

	    device_printf(request->dev,
			  "FAILURE - %s %s asc=0x%02x ascq=0x%02x ",
			  ata_cmd2str(request), ata_skey2str(
			  (request->u.atapi.sense.key & ATA_SENSE_KEY_MASK)),
			  request->u.atapi.sense.asc,
			  request->u.atapi.sense.ascq);
	    if (request->u.atapi.sense.specific & ATA_SENSE_SPEC_VALID)
		printf("sks=0x%02x 0x%02x 0x%02x\n",
		       request->u.atapi.sense.specific & ATA_SENSE_SPEC_MASK,
		       request->u.atapi.sense.specific1,
		       request->u.atapi.sense.specific2);
	    else
		printf("\n");
	}

	if (!request->result &&
	     (request->u.atapi.sense.key & ATA_SENSE_KEY_MASK ||
	     request->error))
	    request->result = EIO;
    }

    ATA_DEBUG_RQ(request, "completed callback/wakeup");

    /* if we are part of a composite operation we need to maintain progress */
    if ((composite = request->composite)) {
	int index = 0;

	mtx_lock(&composite->lock);

	/* update whats done */
	if (request->flags & ATA_R_READ)
	    composite->rd_done |= (1 << request->this);
	if (request->flags & ATA_R_WRITE)
	    composite->wr_done |= (1 << request->this);

	/* find ready to go dependencies */
	if (composite->wr_depend &&
	    (composite->rd_done & composite->wr_depend)==composite->wr_depend &&
	    (composite->wr_needed & (~composite->wr_done))) {
	    index = composite->wr_needed & ~composite->wr_done;
	}

	mtx_unlock(&composite->lock);

	/* if we have any ready candidates kick them off */
	if (index) {
	    int bit;
	    
	    for (bit = 0; bit < MAX_COMPOSITES; bit++) {
		if (index & (1 << bit))
		    ata_start(device_get_parent(composite->request[bit]->dev));
	    }
	}
    }

    /* get results back to the initiator for this request */
    if (request->callback)
	(request->callback)(request);
    else
	sema_post(&request->done);

    /* only call ata_start if channel is present */
    if (ch)
	ata_start(ch->dev);
}
#endif

#ifndef ATA_CAM
void
ata_fail_requests(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_request *request, *tmp;
    TAILQ_HEAD(, ata_request) fail_requests;
    TAILQ_INIT(&fail_requests);

    /* grap all channel locks to avoid races */
    mtx_lock(&ch->queue_mtx);
    mtx_lock(&ch->state_mtx);

    /* do we have any running request to care about ? */
    if ((request = ch->running) && (!dev || request->dev == dev)) {
	callout_stop(&request->callout);
	ch->running = NULL;
	request->result = ENXIO;
	TAILQ_INSERT_TAIL(&fail_requests, request, chain);
    }

    /* fail all requests queued on this channel for device dev if !NULL */
    TAILQ_FOREACH_SAFE(request, &ch->ata_queue, chain, tmp) {
	if (!dev || request->dev == dev) {
	    TAILQ_REMOVE(&ch->ata_queue, request, chain);
	    request->result = ENXIO;
	    TAILQ_INSERT_TAIL(&fail_requests, request, chain);
	}
    }

    mtx_unlock(&ch->state_mtx);
    mtx_unlock(&ch->queue_mtx);
   
    /* finish up all requests collected above */
    TAILQ_FOREACH_SAFE(request, &fail_requests, chain, tmp) {
        TAILQ_REMOVE(&fail_requests, request, chain);
        ata_finish(request);
    }
}
#endif

#ifndef ATA_CAM
/*
 * Rudely drop all requests queued to the channel of specified device.
 * XXX: The requests are leaked, use only in fatal case.
 */
void
ata_drop_requests(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_request *request, *tmp;

    mtx_lock(&ch->queue_mtx);
    TAILQ_FOREACH_SAFE(request, &ch->ata_queue, chain, tmp) {
	TAILQ_REMOVE(&ch->ata_queue, request, chain);
	request->result = ENXIO;
    }
    mtx_unlock(&ch->queue_mtx);
}
#endif

#ifndef ATA_CAM
static u_int64_t
ata_get_lba(struct ata_request *request)
{
    if (request->flags & ATA_R_ATAPI) {
	switch (request->u.atapi.ccb[0]) {
	case ATAPI_READ_BIG:
	case ATAPI_WRITE_BIG:
	case ATAPI_READ_CD:
	    return (request->u.atapi.ccb[5]) | (request->u.atapi.ccb[4]<<8) |
		   (request->u.atapi.ccb[3]<<16)|(request->u.atapi.ccb[2]<<24);
	case ATAPI_READ:
	case ATAPI_WRITE:
	    return (request->u.atapi.ccb[4]) | (request->u.atapi.ccb[3]<<8) |
		   (request->u.atapi.ccb[2]<<16);
	default:
	    return 0;
	}
    }
    else
	return request->u.ata.lba;
}
#endif

#ifndef ATA_CAM
static void
ata_sort_queue(struct ata_channel *ch, struct ata_request *request)
{
    struct ata_request *this, *next;

    this = TAILQ_FIRST(&ch->ata_queue);

    /* if the queue is empty just insert */
    if (!this) {
	if (request->composite)
	    ch->freezepoint = request;
	TAILQ_INSERT_TAIL(&ch->ata_queue, request, chain);
	return;
    }

    /* dont sort frozen parts of the queue */
    if (ch->freezepoint)
	this = ch->freezepoint;
	
    /* if position is less than head we add after tipping point */
    if (ata_get_lba(request) < ata_get_lba(this)) {
	while ((next = TAILQ_NEXT(this, chain))) {

	    /* have we reached the tipping point */
	    if (ata_get_lba(next) < ata_get_lba(this)) {

		/* sort the insert */
		do {
		    if (ata_get_lba(request) < ata_get_lba(next))
			break;
		    this = next;
		} while ((next = TAILQ_NEXT(this, chain)));
		break;
	    }
	    this = next;
	}
    }

    /* we are after head so sort the insert before tipping point */
    else {
	while ((next = TAILQ_NEXT(this, chain))) {
	    if (ata_get_lba(next) < ata_get_lba(this) ||
		ata_get_lba(request) < ata_get_lba(next))
		break;
	    this = next;
	}
    }

    if (request->composite)
	ch->freezepoint = request;
    TAILQ_INSERT_AFTER(&ch->ata_queue, this, request, chain);
}
#endif

#ifndef ATA_CAM
static const char *
ata_skey2str(u_int8_t skey)
{
    switch (skey) {
    case 0x00: return ("NO SENSE");
    case 0x01: return ("RECOVERED ERROR");
    case 0x02: return ("NOT READY");
    case 0x03: return ("MEDIUM ERROR");
    case 0x04: return ("HARDWARE ERROR");
    case 0x05: return ("ILLEGAL REQUEST");
    case 0x06: return ("UNIT ATTENTION");
    case 0x07: return ("DATA PROTECT");
    case 0x08: return ("BLANK CHECK");
    case 0x09: return ("VENDOR SPECIFIC");
    case 0x0a: return ("COPY ABORTED");
    case 0x0b: return ("ABORTED COMMAND");
    case 0x0c: return ("EQUAL");
    case 0x0d: return ("VOLUME OVERFLOW");
    case 0x0e: return ("MISCOMPARE");
    case 0x0f: return ("RESERVED");
    default: return("UNKNOWN");
    }
}
#endif
