/*-
 * Copyright (c) 1998 - 2003 Søren Schmidt <sos@FreeBSD.org>
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
 * $FreeBSD$
 */

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>

/* prototypes */
static void ata_completed(void *context, int pending);
static void ata_timeout(struct ata_request *request);
static char *ata_sensekey2str(u_int8_t skey);

/* local vars */
static MALLOC_DEFINE(M_ATA_REQ, "ATA request", "ATA request");
static int atadebug = 0;

/*
 * ATA request related functions
 */
struct ata_request *
ata_alloc_request(void)
{
    struct ata_request *request;

    request = malloc(sizeof(struct ata_request), M_ATA_REQ, M_NOWAIT | M_ZERO);
    if (!request)
	printf("FAILURE - malloc ATA request failed\n");
    return request;
}

void
ata_free_request(struct ata_request *request)
{
    free(request, M_ATA_REQ);
}

void
ata_queue_request(struct ata_request *request)
{
    /* mark request as virgin (it might be a reused one) */
    request->result = request->status = request->error = 0;
    request->flags &= ~ATA_R_DONE;

    /* put request on the locked queue at the specified location */
    mtx_lock(&request->device->channel->queue_mtx);
    if (request->flags & ATA_R_AT_HEAD)
	TAILQ_INSERT_HEAD(&request->device->channel->ata_queue, request, chain);
    else
	TAILQ_INSERT_TAIL(&request->device->channel->ata_queue, request, chain);
    mtx_unlock(&request->device->channel->queue_mtx);

    /* should we skip start ? */
    if (!(request->flags & ATA_R_SKIPSTART))
	ata_start(request->device->channel);

    /* if this was a requeue op callback/sleep already setup */
    if (request->flags & ATA_R_REQUEUE)
	return;

    /* if this is not a callback and we havn't seen DONE yet -> sleep */
    if (!request->callback && !(request->flags & ATA_R_DONE)) {
	while (tsleep(request, PRIBIO, "atareq", 60*10*hz)) ;
    }
}

int
ata_controlcmd(struct ata_device *atadev, u_int8_t command, u_int16_t feature,
	       u_int64_t lba, u_int16_t count)
{
    struct ata_request *request = ata_alloc_request();
    int error = ENOMEM;

    if (request) {
	request->device = atadev;
	request->u.ata.command = command;
	request->u.ata.lba = lba;
	request->u.ata.count = count;
	request->u.ata.feature = feature;
	request->flags = ATA_R_CONTROL;
	request->timeout = 5;
	ata_queue_request(request);
	error = request->result;
	ata_free_request(request);
    }
    return error;
}

int
ata_atapicmd(struct ata_device *atadev, u_int8_t *ccb, caddr_t data,
	     int count, int flags, int timeout)
{
    struct ata_request *request = ata_alloc_request();
    int packet_size, error = ENOMEM;

    if ((atadev->param->config & ATA_PROTO_MASK) == ATA_PROTO_ATAPI_12)
	packet_size = 12;
    else
	packet_size = 16;
    if (request) {
	request->device = atadev;
	bcopy(ccb, request->u.atapi.ccb, packet_size);
	request->data = data;
	request->bytecount = count;
	request->transfersize = min(request->bytecount, 65534);
	request->flags = flags | ATA_R_ATAPI;
	request->timeout = timeout;
	ata_queue_request(request);
	error = request->result;
	ata_free_request(request);
    }
    return error;
}

void
ata_start(struct ata_channel *ch)
{
    struct ata_request *request;

    /* lock the ATA HW for this request */
    ch->locking(ch, ATA_LF_LOCK);
    if (!ATA_LOCK_CH(ch, ATA_ACTIVE)) {
	return;
    }

if (atadebug && mtx_owned(&Giant)) printf("ata_start holds GIANT!!!\n");

    /* if we dont have any work, ask the subdriver(s) */
    mtx_lock(&ch->queue_mtx);
    if (TAILQ_EMPTY(&ch->ata_queue)) {
	mtx_unlock(&ch->queue_mtx);
	if (ch->device[MASTER].start)
	    ch->device[MASTER].start(&ch->device[MASTER]);
	if (ch->device[SLAVE].start)
	    ch->device[SLAVE].start(&ch->device[SLAVE]);
	mtx_lock(&ch->queue_mtx);
    }
    if ((request = TAILQ_FIRST(&ch->ata_queue))) {
	TAILQ_REMOVE(&ch->ata_queue, request, chain);
	mtx_unlock(&ch->queue_mtx);

	/* arm timeout */
	if (!request->timeout_handle.callout && !dumping) {
	    request->timeout_handle =
		timeout((timeout_t*)ata_timeout, request, request->timeout*hz);
	}

	/* kick HW into action */
	if (ch->hw.transaction(request) == ATA_OP_CONTINUES)
	    return;

	/* untimeout request */
	untimeout((timeout_t *)ata_timeout, request, request->timeout_handle);
	ata_finish(request);
    }
    else
	mtx_unlock(&ch->queue_mtx);

    ATA_UNLOCK_CH(ch);
    ch->locking(ch, ATA_LF_UNLOCK);
}

void
ata_finish(struct ata_request *request)
{
    /* request is done schedule it for completition */
    TASK_INIT(&request->task, 0, ata_completed, request);
    taskqueue_enqueue(taskqueue_swi, &request->task);
}

/* current command finished, clean up and return result */
static void
ata_completed(void *context, int pending)
{
    struct ata_request *request = (struct ata_request *)context;
    struct ata_channel *channel = request->device->channel;

    /* untimeout request now we have control back */
    untimeout((timeout_t *)ata_timeout, request, request->timeout_handle);

    /* do the all the magic for completition evt retry etc etc */
    if (request->status & ATA_S_CORR)
	ata_prtdev(request->device, "WARNING - %s soft error (ECC corrected)",
		   ata_cmd2str(request));

    /* if this is a UDMA CRC error, retry request */
    if (request->flags & ATA_R_DMA && request->error & ATA_E_ICRC) {
	if (request->retries--) {
	    ata_prtdev(request->device,
		       "WARNING - %s UDMA ICRC error (retrying request)\n",
		       ata_cmd2str(request));
	    ata_queue_request(request);
	    return;
	}
    }

    switch (request->flags & ATA_R_ATAPI) {
    /* ATA errors */
    default:
	if (request->status & ATA_S_ERROR) {
	    if (!(request->flags & ATA_R_QUIET)) {
		ata_prtdev(request->device,
			   "FAILURE - %s status=%b error=%b", 
			   ata_cmd2str(request),
			   request->status, "\20\10BUSY\7READY\6DMA_READY"
			   "\5DSC\4DRQ\3CORRECTABLE\2INDEX\1ERROR",
			   request->error, "\20\10ICRC\7UNCORRECTABLE"
			   "\6MEDIA_CHANGED\5NID_NOT_FOUND\4MEDIA_CHANGE_REQEST"
			   "\3ABORTED\2NO_MEDIA\1ILLEGAL_LENGTH");
		if (request->flags & ATA_R_DMA &&
		    request->dmastat & ATA_BMSTAT_ERROR) 
		    printf(" dma=0x%02x\n", request->dmastat);
		else
		    printf("\n");
	    }

	    /* SOS this could be more precise ? XXX*/
	    request->result = EIO;
	}
	break;

    /* ATAPI errors */
    case ATA_R_ATAPI:
	/* is result already set return */
	if (request->result)
	    break;

	if (request->error & ATA_E_MASK) {
	    switch ((request->result & ATA_SK_MASK)) {
	    case ATA_SK_RECOVERED_ERROR:
		ata_prtdev(request->device, "WARNING - %s recovered error\n",
			   ata_cmd2str(request));
		/* FALLTHROUGH */

	    case ATA_SK_NO_SENSE:
		request->result = 0;
		break;

	    case ATA_SK_NOT_READY: 
		request->result = EBUSY;
		break;

	    case ATA_SK_UNIT_ATTENTION:
		request->device->flags |= ATA_D_MEDIA_CHANGED;
		request->result = EIO;
		break;

	    default:
		request->result = EIO;
	    }
	    if (request->result && !(request->flags & ATA_R_QUIET))
		ata_prtdev(request->device,
			   "FAILURE - %s status=%b sensekey=%s error=%b\n",
			   ata_cmd2str(request),
			   request->status, "\20\10BUSY\7READY\6DMA"
			   "\5DSC\4DRQ\3CORRECTABLE\2INDEX\1ERROR",
			   ata_sensekey2str((request->error & ATA_SK_MASK)>>4),
			   (request->error & ATA_E_MASK),
			   "\20\4MEDIA_CHANGE_REQUEST\3ABORTED"
			   "\2NO_MEDIA\1ILLEGAL_LENGTH");
	}
	break;
    }

    request->flags |= ATA_R_DONE;
    if (request->callback)
	(request->callback)(request);
    else
	wakeup(request);
    ata_start(channel);
}

static void
ata_timeout(struct ata_request *request)
{
    /* clear timeout etc */
    request->timeout_handle.callout = NULL;

    /* call interrupt to try finish up the command */
    request->device->channel->hw.interrupt(request->device->channel);

    if (request->device->channel->running == NULL) {
	if (!(request->flags & ATA_R_QUIET))
	    ata_prtdev(request->device,
		       "WARNING - %s recovered from missing interrupt\n",
		       ata_cmd2str(request));
	return;
    }

    /* if this was a DMA request stop the engine to be on the safe side */
    if (request->flags & ATA_R_DMA) {
	request->dmastat =
	    request->device->channel->dma->stop(request->device->channel);
    }

    /* try to adjust HW's attitude towards work */
    ata_reinit(request->device->channel);

    /* if retries still permit, reinject this request */
    if (request->retries-- > 0) {
	if (!(request->flags & ATA_R_QUIET))
	    ata_prtdev(request->device,
		       "TIMEOUT - %s retrying (%d retr%s left)\n",
		       ata_cmd2str(request), request->retries,
		       request->retries == 1 ? "y" : "ies");
	request->flags |= (ATA_R_AT_HEAD | ATA_R_REQUEUE);
	request->flags &= ~ATA_R_SKIPSTART;
	ata_queue_request(request);
    }
    /* otherwise just schedule finish with error */
    else {
	request->status = ATA_S_ERROR;
	TASK_INIT(&request->task, 0, ata_completed, request);
	taskqueue_enqueue(taskqueue_swi, &request->task);
    }
}

char *
ata_cmd2str(struct ata_request *request)
{
    static char buffer[20];

    if (request->flags & ATA_R_ATAPI) {
	switch (request->u.atapi.ccb[0]) {
	case 0x00: return ("TEST_UNIT_READY");
	case 0x01: return ("REZERO");
	case 0x03: return ("REQUEST_SENSE");
	case 0x04: return ("FORMAT");
	case 0x08: return ("READ");
	case 0x0a: return ("WRITE");
	case 0x10: return ("WEOF");
	case 0x11: return ("SPACE");
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
	case 0x08: return ("ATAPI_RESET");
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
	case 0xa0: return ("PACKET_CMD");
	case 0xa1: return ("ATAPI_IDENTIFY");
	case 0xa2: return ("SERVICE");
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
	case 0xef: return ("SETFEATURES");
	}
    }
    sprintf(buffer, "unknown CMD (0x%02x)", request->u.ata.command);
    return buffer;
}

static char *
ata_sensekey2str(u_int8_t skey)
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
