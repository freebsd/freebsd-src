/*-
 * Copyright (c) 1998,1999,2000 Søren Schmidt
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

#include "atapicd.h"
#include "atapist.h"
#include "atapifd.h" 
#include "opt_global.h"
#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <machine/clock.h>
#include <machine/bus.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>

/* prototypes */
static void atapi_read(struct atapi_request *, int32_t);
static void atapi_write(struct atapi_request *, int32_t);
static void atapi_timeout(struct atapi_request *request);
static int8_t *atapi_type(int32_t);
static int8_t *atapi_cmd2str(u_int8_t);
static int8_t *atapi_skey2str(u_int8_t);

/* internal vars */
MALLOC_DEFINE(M_ATAPI, "ATAPI generic", "ATAPI driver generic layer");

/* defines */
#define ATAPI_MAX_RETRIES  	3
#define ATP_PARAM		ATA_PARAM(atp->controller, atp->unit)

void
atapi_attach(struct ata_softc *scp, int32_t device)
{
    struct atapi_softc *atp;

    if (!(atp = malloc(sizeof(struct atapi_softc), M_ATAPI, M_NOWAIT))) {
	ata_printf(scp, device, "failed to allocate driver storage\n");
	return;
    }
    bzero(atp, sizeof(struct atapi_softc));
    atp->controller = scp;
    atp->unit = device;
    if (bootverbose) 
	ata_printf(scp, device, 
		   "piomode=%d dmamode=%d udmamode=%d dmaflag=%d\n",
		   ata_pmode(ATP_PARAM), ata_wmode(ATP_PARAM),
		   ata_umode(ATP_PARAM), ATP_PARAM->dmaflag);

#ifdef ATA_ENABLE_ATAPI_DMA
    if (!(ATP_PARAM->drqtype == ATAPI_DRQT_INTR)) {
	ata_dmainit(atp->controller, atp->unit,
		    (ata_pmode(ATP_PARAM) < 0) ? 
		    (ATP_PARAM->dmaflag ? 4 : 0) : ata_pmode(ATP_PARAM),
		    (ata_wmode(ATP_PARAM) < 0) ? 
		    (ATP_PARAM->dmaflag ? 2 : 0) : ata_wmode(ATP_PARAM),
		    ata_umode(ATP_PARAM));
    }
    else
#endif
	/* set PIO mode */
	ata_dmainit(atp->controller, atp->unit, 
		    ata_pmode(ATP_PARAM)<0 ? 0 : ata_pmode(ATP_PARAM), -1, -1);

    switch (ATP_PARAM->device_type) {
#if NATAPICD > 0
    case ATAPI_TYPE_CDROM:
	if (acdattach(atp))
	    goto notfound;
	break; 
#endif
#if NATAPIFD > 0
    case ATAPI_TYPE_DIRECT:
	if (afdattach(atp))
	    goto notfound;
	break; 
#endif
#if NATAPIST > 0
    case ATAPI_TYPE_TAPE:
	if (astattach(atp))
	    goto notfound;
	break; 
#endif
notfound:
    default:
	ata_printf(scp, device, "<%.40s/%.8s> %s device - NO DRIVER!\n",
		   ATP_PARAM->model, ATP_PARAM->revision, 
		   atapi_type(ATP_PARAM->device_type));
	free(atp, M_ATAPI);
	atp = NULL;
    }
    /* store our softc */
    scp->dev_softc[ATA_DEV(device)] = atp;
}

void
atapi_detach(struct atapi_softc *atp)
{
    switch (ATP_PARAM->device_type) {
#if NATAPICD > 0
    case ATAPI_TYPE_CDROM:
	acddetach(atp);
	break; 
#endif
#if NATAPIFD > 0
    case ATAPI_TYPE_DIRECT:
	afddetach(atp);
	break; 
#endif
#if NATAPIST > 0
    case ATAPI_TYPE_TAPE:
	astdetach(atp);
	break; 
#endif
    default:
	return;
    }
    free(atp, M_ATAPI);
}

int32_t	  
atapi_queue_cmd(struct atapi_softc *atp, int8_t *ccb, void *data, 
		int32_t count, int32_t flags, int32_t timeout,
		atapi_callback_t callback, void *driver)
{
    struct atapi_request *request;
    int32_t error, s;
 
    if (!(request = malloc(sizeof(struct atapi_request), M_ATAPI, M_NOWAIT)))
	return ENOMEM;

    bzero(request, sizeof(struct atapi_request));
    request->device = atp;
    request->data = data;
    request->bytecount = count;
    request->flags = flags;
    request->timeout = timeout * hz;
    request->ccbsize = (ATP_PARAM->cmdsize) ? 16 : 12;
    bcopy(ccb, request->ccb, request->ccbsize);
    if (callback) {
	request->callback = callback;
	request->driver = driver;
    }

    s = splbio();

    /* if not using callbacks, prepare to sleep for this request */
    if (!callback)
	asleep((caddr_t)request, PRIBIO, "atprq", 0);

    /* append onto controller queue and try to start controller */
#ifdef ATAPI_DEBUG
    printf("%s: queueing %s ", 
	   request->device->devname, atapi_cmd2str(request->ccb[0]));
    atapi_dump("ccb = ", &request->ccb[0], sizeof(request->ccb));
#endif
    if (flags & ATPR_F_AT_HEAD)
	TAILQ_INSERT_HEAD(&atp->controller->atapi_queue, request, chain);
    else
	TAILQ_INSERT_TAIL(&atp->controller->atapi_queue, request, chain);
    ata_start(atp->controller);

    /* if callback used, then just return, gets called from interrupt context */
    if (callback) {
	splx(s);
	return 0;
    }

    /* wait for request to complete */
    await(PRIBIO, 0);
    splx(s);
    error = request->error;
#ifdef ATAPI_DEBUG
    printf("%s: finished %s\n", 
	   request->device->devname, atapi_cmd2str(request->ccb[0]));
#endif
    free(request, M_ATAPI);
    return error;
}
    
void
atapi_start(struct atapi_softc *atp)
{
    switch (ATP_PARAM->device_type) {
#if NATAPICD > 0
    case ATAPI_TYPE_CDROM:
	acd_start(atp);
	break; 
#endif
#if NATAPIFD > 0
    case ATAPI_TYPE_DIRECT:
	afd_start(atp);
	break; 
#endif
#if NATAPIST > 0
    case ATAPI_TYPE_TAPE:
	ast_start(atp);
	break; 
#endif
    default:
	return;
    }
}

void
atapi_transfer(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;
    int32_t timout;
    int8_t reason;

#ifdef ATAPI_DEBUG
    printf("%s: starting %s ", 
	   request->device->devname, atapi_cmd2str(request->ccb[0]));
    atapi_dump("ccb = ", &request->ccb[0], sizeof(request->ccb));
#endif
    /* start timeout for this command */
    request->timeout_handle = timeout((timeout_t *)atapi_timeout, 
				      request, request->timeout);
    if (request->ccb[0] != ATAPI_REQUEST_SENSE)
	atp->cmd = request->ccb[0];

    /* if DMA enabled setup DMA hardware */
    request->flags &= ~ATPR_F_DMA_USED; 
    if ((atp->controller->mode[ATA_DEV(atp->unit)] >= ATA_DMA) &&
	(request->ccb[0] == ATAPI_READ ||
	 request->ccb[0] == ATAPI_READ_BIG ||
	 ((request->ccb[0] == ATAPI_WRITE ||
	   request->ccb[0] == ATAPI_WRITE_BIG) &&
	  !(atp->controller->flags & ATA_ATAPI_DMA_RO))) &&
	!ata_dmasetup(atp->controller, atp->unit,
		      (void *)request->data, request->bytecount,
		      request->flags & ATPR_F_READ)) {
	request->flags |= ATPR_F_DMA_USED;
    }

    /* start ATAPI operation */
    if (ata_command(atp->controller, atp->unit, ATA_C_PACKET_CMD, 
		    request->bytecount, 0, 0, 0, 
		    (request->flags & ATPR_F_DMA_USED) ? ATA_F_DMA : 0,
		    ATA_IMMEDIATE))
	printf("%s: failure to send ATAPI packet command\n", atp->devname);

    if (request->flags & ATPR_F_DMA_USED)
	ata_dmastart(atp->controller);

    /* command interrupt device ? just return */
    if (ATP_PARAM->drqtype == ATAPI_DRQT_INTR)
	return;

    /* ready to write ATAPI command */
    timout = 5000; /* might be less for fast devices */
    while (timout--) {
	reason = inb(atp->controller->ioaddr + ATA_IREASON);
	atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS);
	if (((reason & (ATA_I_CMD | ATA_I_IN)) |
	     (atp->controller->status&(ATA_S_DRQ|ATA_S_BUSY)))==ATAPI_P_CMDOUT)
	    break;
	DELAY(20);
    }
    if (timout <= 0) {
	request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	printf("%s: failure to execute ATAPI packet command\n", atp->devname);
	return;
    }

    /* this seems to be needed for some (slow) devices */
    DELAY(10);

    /* send actual command */
    outsw(atp->controller->ioaddr + ATA_DATA, request->ccb, 
	  request->ccbsize / sizeof(int16_t));
}

int32_t
atapi_interrupt(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;
    int8_t **buffer = (int8_t **)&request->data;
    int32_t reason, dma_stat = 0;

    if (request->ccb[0] == ATAPI_REQUEST_SENSE)
	*buffer = (int8_t *)&request->sense;

    reason = (inb(atp->controller->ioaddr+ATA_IREASON) & (ATA_I_CMD|ATA_I_IN)) |
	     (atp->controller->status & ATA_S_DRQ);

    if (reason == ATAPI_P_CMDOUT) {
	if (!(atp->controller->status & ATA_S_DRQ)) {
	    request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	    printf("%s: command interrupt without DRQ\n", atp->devname);
	    goto op_finished;
	}
	outsw(atp->controller->ioaddr + ATA_DATA, request->ccb,
	      request->ccbsize / sizeof(int16_t));
	return ATA_OP_CONTINUES;
    }

    if (request->flags & ATPR_F_DMA_USED) {
	dma_stat = ata_dmadone(atp->controller);
	if ((atp->controller->status & (ATA_S_ERROR | ATA_S_DWF)) ||
	    dma_stat & ATA_BMSTAT_ERROR) {
	    request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	}
	else {
	    request->result = 0;
	    request->donecount = request->bytecount;
	    request->bytecount = 0;
	}
    }
    else {
	int32_t length = inb(atp->controller->ioaddr + ATA_CYL_LSB) |
			 inb(atp->controller->ioaddr + ATA_CYL_MSB) << 8;

	switch (reason) {
	case ATAPI_P_WRITE:
	    if (request->flags & ATPR_F_READ) {
		request->result = inb(atp->controller->ioaddr + ATA_ERROR);
		printf("%s: %s trying to write on read buffer\n",
		       atp->devname, atapi_cmd2str(atp->cmd));
		break;
	    }
	    atapi_write(request, length);
	    return ATA_OP_CONTINUES;
	
	case ATAPI_P_READ:
	    if (!(request->flags & ATPR_F_READ)) {
		request->result = inb(atp->controller->ioaddr + ATA_ERROR);
		printf("%s: %s trying to read on write buffer\n",
		       atp->devname, atapi_cmd2str(atp->cmd));
		break;
	    }
	    atapi_read(request, length);
	    return ATA_OP_CONTINUES;

	case ATAPI_P_DONEDRQ:
	    printf("%s: %s DONEDRQ\n", atp->devname, atapi_cmd2str(atp->cmd));
	    if (request->flags & ATPR_F_READ)
		atapi_read(request, length);
	    else
		atapi_write(request, length);
	    /* FALLTHROUGH */

	    case ATAPI_P_ABORT:
	    case ATAPI_P_DONE:
	    if (atp->controller->status & (ATA_S_ERROR | ATA_S_DWF))
		request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	    else 
		if (request->ccb[0] != ATAPI_REQUEST_SENSE)
		    request->result = 0;
	    break;

	default:
	    printf("%s: unknown transfer phase %d\n", atp->devname, reason);
	}
    }

op_finished:
    untimeout((timeout_t *)atapi_timeout, request, request->timeout_handle);

    /* check for error, if valid sense key, queue a request sense cmd */
    if ((request->result & ATAPI_SK_MASK) && 
	request->ccb[0] != ATAPI_REQUEST_SENSE) {
	bzero(request->ccb, request->ccbsize);
	request->ccb[0] = ATAPI_REQUEST_SENSE;
	request->ccb[4] = sizeof(struct atapi_reqsense);
	request->bytecount = sizeof(struct atapi_reqsense);
	request->flags = ATPR_F_READ;
	TAILQ_INSERT_HEAD(&atp->controller->atapi_queue, request, chain);
    }
    else {
	request->error = 0;
    	if (request->result) {
	    switch ((request->result & ATAPI_SK_MASK)) {
	    case ATAPI_SK_RESERVED:
		printf("%s: %s - timeout error = %02x\n", 
		       atp->devname,
		       atapi_cmd2str(atp->cmd), request->result & ATAPI_E_MASK);
		request->error = EIO;
		break;

	    case ATAPI_SK_NO_SENSE:
		request->error = 0;
		break;

	    case ATAPI_SK_RECOVERED_ERROR:
		printf("%s: %s - recovered error\n", 
		       atp->devname, atapi_cmd2str(atp->cmd));
		request->error = 0;
		break;

	    case ATAPI_SK_NOT_READY:
		request->error = EBUSY;
		break;

	    case ATAPI_SK_UNIT_ATTENTION:
		atp->flags |= ATAPI_F_MEDIA_CHANGED;
		request->error = EIO;
		break;

	    default: 
		printf("%s: %s - %s asc=%02x ascq=%02x error=%02x\n",
		       atp->devname, atapi_cmd2str(atp->cmd), 
		       atapi_skey2str(request->sense.sense_key), 
		       request->sense.asc, request->sense.ascq,
		       request->result & ATAPI_E_MASK);
		request->error = EIO;
	    }
	}
	if (request->callback) {
#ifdef ATAPI_DEBUG
	    printf("%s: finished %s (callback)\n", 
		   request->device->devname, atapi_cmd2str(request->ccb[0]));
#endif
	    if (!((request->callback)(request)))
		free(request, M_ATAPI);
	}
	else 
            wakeup((caddr_t)request);	
    }
    return ATA_OP_FINISHED;
}

void
atapi_reinit(struct atapi_softc *atp)
{
    /* reinit device parameters */
     if (atp->controller->mode[ATA_DEV(atp->unit)] >= ATA_DMA)
	ata_dmainit(atp->controller, atp->unit,
		    (ata_pmode(ATP_PARAM) < 0) ?
		    (ATP_PARAM->dmaflag ? 4 : 0) : ata_pmode(ATP_PARAM),
		    (ata_wmode(ATP_PARAM) < 0) ? 
		    (ATP_PARAM->dmaflag ? 2 : 0) : ata_wmode(ATP_PARAM),
		    ata_umode(ATP_PARAM));
    else
	ata_dmainit(atp->controller, atp->unit, 
		    ata_pmode(ATP_PARAM)<0 ? 0 : ata_pmode(ATP_PARAM), -1, -1);
}

int32_t
atapi_test_ready(struct atapi_softc *atp)
{
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	
    return atapi_queue_cmd(atp, ccb, NULL, 0, 0, 30, NULL, NULL);
}
	
int32_t
atapi_wait_ready(struct atapi_softc *atp, int32_t timeout)
{
    int32_t error = 0, timout = timeout * hz;

    while (timout > 0) {
	if ((error = atapi_test_ready(atp)) != EBUSY)
	    break;
	tsleep((caddr_t)&error, PRIBIO, "atpwt", 50);
	timout -= 50;
    }
    return error;
}

void
atapi_dump(int8_t *label, void *data, int32_t len)
{
    u_int8_t *p = data;

    printf ("%s %02x", label, *p++);
    while (--len > 0) 
	printf ("-%02x", *p++);
    printf ("\n");
}

static void
atapi_read(struct atapi_request *request, int32_t length)
{
    int8_t **buffer = (int8_t **)&request->data;
    int32_t size = min(request->bytecount, length);
    int32_t resid;

    if (request->ccb[0] == ATAPI_REQUEST_SENSE)
	*buffer = (int8_t *)&request->sense;

    if (request->device->controller->flags & ATA_USE_16BIT ||
	(size % sizeof(int32_t)))
	insw(request->device->controller->ioaddr + ATA_DATA, 
	     (void *)((uintptr_t)*buffer), size / sizeof(int16_t));
    else
	insl(request->device->controller->ioaddr + ATA_DATA, 
	     (void *)((uintptr_t)*buffer), size / sizeof(int32_t));

    if (request->bytecount < length) {
	printf("%s: read data overrun %d/%d\n",
	       request->device->devname, length, request->bytecount);
	for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
	     inw(request->device->controller->ioaddr + ATA_DATA);
    }
    *buffer += size;
    request->bytecount -= size;
    request->donecount += size;
}

static void
atapi_write(struct atapi_request *request, int32_t length)
{
    int8_t **buffer = (int8_t **)&request->data;
    int32_t size = min(request->bytecount, length);
    int32_t resid;

    if (request->ccb[0] == ATAPI_REQUEST_SENSE)
	*buffer = (int8_t *)&request->sense;

    if (request->device->controller->flags & ATA_USE_16BIT ||
	(size % sizeof(int32_t)))
	outsw(request->device->controller->ioaddr + ATA_DATA, 
	      (void *)((uintptr_t)*buffer), size / sizeof(int16_t));
    else
	outsl(request->device->controller->ioaddr + ATA_DATA, 
	      (void *)((uintptr_t)*buffer), size / sizeof(int32_t));

    if (request->bytecount < length) {
	printf("%s: write data underrun %d/%d\n",
	       request->device->devname, length, request->bytecount);
	for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
	     outw(request->device->controller->ioaddr + ATA_DATA, 0);
    }
    *buffer += size;
    request->bytecount -= size;
    request->donecount += size;
}

static void 
atapi_timeout(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;
    int32_t s = splbio();

    atp->controller->running = NULL;
    printf("%s: %s command timeout - resetting\n", 
	   atp->devname, atapi_cmd2str(request->ccb[0]));

    if (request->flags & ATPR_F_DMA_USED) {
	ata_dmadone(atp->controller);
	if (request->retries == ATAPI_MAX_RETRIES) {
	    ata_dmainit(atp->controller, atp->unit, 
		        (ata_pmode(ATP_PARAM)<0)?0:ata_pmode(ATP_PARAM),-1,-1);
	    printf("%s: trying fallback to PIO mode\n", atp->devname);
	    request->retries = 0;
	}
    }

    /* if retries still permit, reinject this request */
    if (request->retries++ < ATAPI_MAX_RETRIES)
        TAILQ_INSERT_HEAD(&atp->controller->atapi_queue, request, chain);
    else {
        /* retries all used up, return error */
	request->result = ATAPI_SK_RESERVED | ATAPI_E_ABRT;
	wakeup((caddr_t)request);
    } 
    ata_reinit(atp->controller);
    splx(s);
}

static int8_t *
atapi_type(int32_t type)
{
    switch (type) {
    case ATAPI_TYPE_CDROM:
	return "CDROM";
    case ATAPI_TYPE_DIRECT:
	return "floppy";
    case ATAPI_TYPE_TAPE:
	return "tape";
    case ATAPI_TYPE_OPTICAL:
	return "optical";
    default:
	return "Unknown";
    }
}

static int8_t *
atapi_cmd2str(u_int8_t cmd)
{
    switch (cmd) {
    case 0x00: return ("TEST_UNIT_READY");
    case 0x01: return ("REWIND");
    case 0x03: return ("REQUEST_SENSE");
    case 0x04: return ("FORMAT_UNIT");
    case 0x08: return ("READ");
    case 0x0a: return ("WRITE");
    case 0x10: return ("WEOF");
    case 0x11: return ("SPACE");
    case 0x15: return ("MODE_SELECT");
    case 0x19: return ("ERASE");
    case 0x1a: return ("MODE_SENSE");
    case 0x1b: return ("START_STOP");
    case 0x1e: return ("PREVENT_ALLOW");
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
    case 0xa5: return ("PLAY_BIG");
    case 0xa6: return ("LOAD_UNLOAD");
    case 0xad: return ("READ_DVD_STRUCTURE");
    case 0xb4: return ("PLAY_CD");
    case 0xbb: return ("SET_SPEED");
    case 0xbd: return ("MECH_STATUS");
    case 0xbe: return ("READ_CD");
    default: {
	static int8_t buffer[16];
	sprintf(buffer, "unknown CMD (0x%02x)", cmd);
	return buffer;
	}
    }
}

static int8_t *
atapi_skey2str(u_int8_t skey)
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
