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

void
ata_timeout(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(request->parent);

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
	if (ch->dma.unload)
	    ch->dma.unload(request);
	ch->running = NULL;
	ch->state = ATA_IDLE;
	ata_cam_end_transaction(ch->dev, request);
	mtx_unlock(&ch->state_mtx);
    }
    else {
	mtx_unlock(&ch->state_mtx);
    }
}

const char *
ata_cmd2str(struct ata_request *request)
{
    static char buffer[20];

    if (request->flags & ATA_R_ATAPI) {
	switch (request->u.atapi.sense.key ?
		request->u.atapi.saved_cmd : request->u.atapi.ccb[0]) {
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
        case 0x96: return ("SERVICE_ACTION_IN");
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
	case 0x27: return ("READ_NATIVE_MAX_ADDRESS48");
	case 0x29: return ("READ_MUL48");
	case 0x30: return ("WRITE");
	case 0x34: return ("WRITE48");
	case 0x35: return ("WRITE_DMA48");
	case 0x36: return ("WRITE_DMA_QUEUED48");
	case 0x37: return ("SET_MAX_ADDRESS48");
	case 0x39: return ("WRITE_MUL48");
	case 0x70: return ("SEEK");
	case 0xa0: return ("PACKET_CMD");
	case 0xa1: return ("ATAPI_IDENTIFY");
	case 0xa2: return ("SERVICE");
	case 0xb0: return ("SMART");
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
	case 0xf5: return ("SECURITY_FREE_LOCK");
	case 0xf8: return ("READ_NATIVE_MAX_ADDRESS");
	case 0xf9: return ("SET_MAX_ADDRESS");
	}
    }
    sprintf(buffer, "unknown CMD (0x%02x)", request->u.ata.command);
    return buffer;
}
