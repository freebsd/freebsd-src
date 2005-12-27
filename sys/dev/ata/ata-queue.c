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

/* prototypes */
static void ata_completed(void *, int);
static void ata_sort_queue(struct ata_channel *ch, struct ata_request *request);
static char *ata_skey2str(u_int8_t);

void
ata_queue_request(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));

    /* mark request as virgin (this might be a ATA_R_REQUEUE) */
    request->result = request->status = request->error = 0;
    callout_init_mtx(&request->callout, &ch->state_mtx, CALLOUT_RETURNUNLOCKED);

    if (!request->callback && !(request->flags & ATA_R_REQUEUE))
	sema_init(&request->done, 0, "ATA request done");

    /* in ATA_STALL_QUEUE state we call HW directly (used only during reinit) */
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
	ATA_DEBUG_RQ(request, "wait for completition");
	while (sema_timedwait(&request->done, request->timeout * hz * 4)) {
	    device_printf(request->dev,
		"req=%p %s semaphore timeout !! DANGER Will Robinson !!\n",
		      request, ata_cmd2str(request));
	    ata_start(ch->dev);
	}
	sema_destroy(&request->done);
    }
}

int
ata_controlcmd(device_t dev, u_int8_t command, u_int16_t feature,
	       u_int64_t lba, u_int16_t count)
{
    struct ata_request *request = ata_alloc_request();
    int error = ENOMEM;

    if (request) {
	request->dev = dev;
	request->u.ata.command = command;
	request->u.ata.lba = lba;
	request->u.ata.count = count;
	request->u.ata.feature = feature;
	request->flags = ATA_R_CONTROL;
	request->timeout = 1;
	request->retries = 0;
	ata_queue_request(request);
	error = request->result;
	ata_free_request(request);
    }
    return error;
}

int
ata_atapicmd(device_t dev, u_int8_t *ccb, caddr_t data,
	     int count, int flags, int timeout)
{
    struct ata_request *request = ata_alloc_request();
    struct ata_device *atadev = device_get_softc(dev);
    int error = ENOMEM;

    if (request) {
	request->dev = dev;
	if ((atadev->param.config & ATA_PROTO_MASK) == ATA_PROTO_ATAPI_12)
	    bcopy(ccb, request->u.atapi.ccb, 12);
	else
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
}

void
ata_finish(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));

    /*
     * if in ATA_STALL_QUEUE state or request has ATA_R_DIRECT flags set
     * we need to call ata_complete() directly here (no taskqueue involvement)
     */
    if ((ch->state & ATA_STALL_QUEUE) || (request->flags & ATA_R_DIRECT)) {
	ATA_DEBUG_RQ(request, "finish directly");
	ata_completed(request, 0);
    }
    else {
	/* put request on the proper taskqueue for completition */
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

static void
ata_completed(void *context, int dummy)
{
    struct ata_request *request = (struct ata_request *)context;
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);
    struct ata_composite *composite;

    ATA_DEBUG_RQ(request, "completed entered");

    /* if we had a timeout, reinit channel and deal with the falldown */
    if (request->flags & ATA_R_TIMEOUT) {
	/*
	 * if reinit succeeds and the device doesn't get detached and
	 * there are retries left we reinject this request
	 */
	if (!ata_reinit(ch->dev) && !request->result &&
	    (request->retries-- > 0)) {
	    if (!(request->flags & ATA_R_QUIET)) {
		device_printf(request->dev,
			      "TIMEOUT - %s retrying (%d retr%s left)",
			      ata_cmd2str(request), request->retries,
			      request->retries == 1 ? "y" : "ies");
		if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		    printf(" LBA=%llu", (unsigned long long)request->u.ata.lba);
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
			printf(" LBA=%llu",
			       (unsigned long long)request->u.ata.lba);
		    printf("\n");
		}
	    }
	    request->result = EIO;
	}
    }
    else {
	/* if this is a soft ECC error warn about it */
	/* XXX SOS we could do WARF here */
	if ((request->status & (ATA_S_CORR | ATA_S_ERROR)) == ATA_S_CORR) {
	    device_printf(request->dev,
			  "WARNING - %s soft error (ECC corrected)",
			  ata_cmd2str(request));
	    if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		printf(" LBA=%llu", (unsigned long long)request->u.ata.lba);
	    printf("\n");
	}

	/* if this is a UDMA CRC error we reinject if there are retries left */
	if (request->flags & ATA_R_DMA && request->error & ATA_E_ICRC) {
	    if (request->retries-- > 0) {
		device_printf(request->dev,
			      "WARNING - %s UDMA ICRC error (retrying request)",
			      ata_cmd2str(request));
		if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		    printf(" LBA=%llu", (unsigned long long)request->u.ata.lba);
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
		if ((request->flags & ATA_R_DMA) &&
		    (request->dmastat & ATA_BMSTAT_ERROR))
		    printf(" dma=0x%02x", request->dmastat);
		if (!(request->flags & (ATA_R_ATAPI | ATA_R_CONTROL)))
		    printf(" LBA=%llu", (unsigned long long)request->u.ata.lba);
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
	if (request->error & ATA_SK_MASK &&
	    request->u.atapi.ccb[0] != ATAPI_REQUEST_SENSE) {
	    static u_int8_t ccb[16] = { ATAPI_REQUEST_SENSE, 0, 0, 0,
					sizeof(struct atapi_sense),
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	    request->u.atapi.sense_key = request->error;
	    request->u.atapi.sense_cmd = request->u.atapi.ccb[0];
	    bcopy(ccb, request->u.atapi.ccb, 16);
	    request->data = (caddr_t)&request->u.atapi.sense_data;
	    request->bytecount = sizeof(struct atapi_sense);
	    request->donecount = 0;
	    request->transfersize = sizeof(struct atapi_sense);
	    request->timeout = 5;
	    request->flags &= (ATA_R_ATAPI | ATA_R_QUIET);
	    request->flags |= (ATA_R_READ | ATA_R_AT_HEAD | ATA_R_REQUEUE);
	    ATA_DEBUG_RQ(request, "autoissue request sense");
	    ata_queue_request(request);
	    return;
	}

	switch (request->u.atapi.sense_key & ATA_SK_MASK) {
	case ATA_SK_RECOVERED_ERROR:
	    device_printf(request->dev, "WARNING - %s recovered error\n",
			  ata_cmd2str(request));
	    /* FALLTHROUGH */

	case ATA_SK_NO_SENSE:
	    request->result = 0;
	    break;

	case ATA_SK_NOT_READY: 
	    request->result = EBUSY;
	    break;

	case ATA_SK_UNIT_ATTENTION:
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
			  (request->u.atapi.sense_key & ATA_SK_MASK) >> 4),
			  request->u.atapi.sense_data.asc,
			  request->u.atapi.sense_data.ascq);
	    if (request->u.atapi.sense_data.sksv)
		printf("sks=0x%02x 0x%02x 0x%02x ",
		       request->u.atapi.sense_data.sk_specific,
		       request->u.atapi.sense_data.sk_specific1,
		       request->u.atapi.sense_data.sk_specific2);
	    printf("error=%b\n",
		   (request->u.atapi.sense_key & ATA_E_MASK),
		   "\20\4MEDIA_CHANGE_REQUEST\3ABORTED"
		   "\2NO_MEDIA\1ILLEGAL_LENGTH");
	}

	if ((request->u.atapi.sense_key ?
	     request->u.atapi.sense_key : request->error) & ATA_E_MASK)
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

    ata_start(ch->dev);
}

void
ata_timeout(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));

    //request->flags |= ATA_R_DEBUG;
    ATA_DEBUG_RQ(request, "timeout");

    /*
     * if we have an ATA_ACTIVE request running, we flag the request 
     * ATA_R_TIMEOUT so ata_finish will handle it correctly
     * also NULL out the running request so we wont loose 
     * the race with an eventual interrupt arriving late
     */
    if (ch->state == ATA_ACTIVE) {
	request->flags |= ATA_R_TIMEOUT;
	ch->running = NULL;
	mtx_unlock(&ch->state_mtx);
	ATA_LOCKING(ch->dev, ATA_LF_UNLOCK);
	ata_finish(request);
    }
    else {
	mtx_unlock(&ch->state_mtx);
    }
}

void
ata_fail_requests(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_request *request;

    /* do we have any outstanding request to care about ?*/
    mtx_lock(&ch->state_mtx);
    if ((request = ch->running) && (!dev || request->dev == dev)) {
	callout_stop(&request->callout);
	ch->running = NULL;
    }
    else
	request = NULL;
    mtx_unlock(&ch->state_mtx);
    if (request)  {
	request->result = ENXIO;
	ata_finish(request);
    }

    /* fail all requests queued on this channel for device dev if !NULL */
    mtx_lock(&ch->queue_mtx);
    while ((request = TAILQ_FIRST(&ch->ata_queue))) {
	if (!dev || request->dev == dev) {
	    TAILQ_REMOVE(&ch->ata_queue, request, chain);
	    mtx_unlock(&ch->queue_mtx);
	    request->result = ENXIO;
	    ata_finish(request);
	    mtx_lock(&ch->queue_mtx);
	}
    }
    mtx_unlock(&ch->queue_mtx);
}

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

char *
ata_cmd2str(struct ata_request *request)
{
    static char buffer[20];

    if (request->flags & ATA_R_ATAPI) {
	switch (request->u.atapi.sense_key ?
		request->u.atapi.sense_cmd : request->u.atapi.ccb[0]) {
	case 0x00: return ("TEST_UNIT_READY");
	case 0x01: return ("REZERO");
	case 0x03: return ("REQUEST_SENSE");
	case 0x04: return ("FORMAT");
	case 0x08: return ("READ");
	case 0x0a: return ("WRITE");
	case 0x10: return ("WEOF");
	case 0x11: return ("SPACE");
	case 0x12: return ("INQUIRY");
	case 0x15: return ("MODE_SELECT");
	case 0x19: return ("ERASE");
	case 0x1a: return ("MODE_SENSE");
	case 0x1b: return ("START_STOP");
	case 0x1e: return ("PREVENT_ALLOW");
	case 0x23: return ("ATAPI_READ_FORMAT_CAPACITIES");
	case 0x25: return ("READ_CAPACITY");
	case 0x28: return ("READ_BIG");
	case 0x2a: return ("WRITE_BIG");
	case 0x2b: return ("LOCATE");
	case 0x34: return ("READ_POSITION");
	case 0x35: return ("SYNCHRONIZE_CACHE");
	case 0x3b: return ("WRITE_BUFFER");
	case 0x3c: return ("READ_BUFFER");
	case 0x42: return ("READ_SUBCHANNEL");
	case 0x43: return ("READ_TOC");
	case 0x45: return ("PLAY_10");
	case 0x47: return ("PLAY_MSF");
	case 0x48: return ("PLAY_TRACK");
	case 0x4b: return ("PAUSE");
	case 0x51: return ("READ_DISK_INFO");
	case 0x52: return ("READ_TRACK_INFO");
	case 0x53: return ("RESERVE_TRACK");
	case 0x54: return ("SEND_OPC_INFO");
	case 0x55: return ("MODE_SELECT_BIG");
	case 0x58: return ("REPAIR_TRACK");
	case 0x59: return ("READ_MASTER_CUE");
	case 0x5a: return ("MODE_SENSE_BIG");
	case 0x5b: return ("CLOSE_TRACK/SESSION");
	case 0x5c: return ("READ_BUFFER_CAPACITY");
	case 0x5d: return ("SEND_CUE_SHEET");
	case 0xa1: return ("BLANK_CMD");
	case 0xa3: return ("SEND_KEY");
	case 0xa4: return ("REPORT_KEY");
	case 0xa5: return ("PLAY_12");
	case 0xa6: return ("LOAD_UNLOAD");
	case 0xad: return ("READ_DVD_STRUCTURE");
	case 0xb4: return ("PLAY_CD");
	case 0xbb: return ("SET_SPEED");
	case 0xbd: return ("MECH_STATUS");
	case 0xbe: return ("READ_CD");
	case 0xff: return ("POLL_DSC");
	}
    }
    else {
	switch (request->u.ata.command) {
	case 0x00: return ("NOP");
	case 0x08: return ("DEVICE_RESET");
	case 0x20: return ("READ");
	case 0x24: return ("READ48");
	case 0x25: return ("READ_DMA48");
	case 0x26: return ("READ_DMA_QUEUED48");
	case 0x29: return ("READ_MUL48");
	case 0x30: return ("WRITE");
	case 0x34: return ("WRITE48");
	case 0x35: return ("WRITE_DMA48");
	case 0x36: return ("WRITE_DMA_QUEUED48");
	case 0x39: return ("WRITE_MUL48");
	case 0x70: return ("SEEK");
	case 0xa0: return ("PACKET_CMD");
	case 0xa1: return ("ATAPI_IDENTIFY");
	case 0xa2: return ("SERVICE");
	case 0xc0: return ("CFA ERASE");
	case 0xc4: return ("READ_MUL");
	case 0xc5: return ("WRITE_MUL");
	case 0xc6: return ("SET_MULTI");
	case 0xc7: return ("READ_DMA_QUEUED");
	case 0xc8: return ("READ_DMA");
	case 0xca: return ("WRITE_DMA");
	case 0xcc: return ("WRITE_DMA_QUEUED");
	case 0xe6: return ("SLEEP");
	case 0xe7: return ("FLUSHCACHE");
	case 0xea: return ("FLUSHCACHE48");
	case 0xec: return ("ATA_IDENTIFY");
	case 0xef:
	    switch (request->u.ata.feature) {
	    case 0x03: return ("SETFEATURES SET TRANSFER MODE");
	    case 0x02: return ("SETFEATURES ENABLE WCACHE");
	    case 0x82: return ("SETFEATURES DISABLE WCACHE");
	    case 0xaa: return ("SETFEATURES ENABLE RCACHE");
	    case 0x55: return ("SETFEATURES DISABLE RCACHE");
	    }
	    sprintf(buffer, "SETFEATURES 0x%02x", request->u.ata.feature);
	    return buffer;
	}
    }
    sprintf(buffer, "unknown CMD (0x%02x)", request->u.ata.command);
    return buffer;
}

static char *
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
