/*-
 * Copyright (c) 1998,1999,2000,2001 Søren Schmidt
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

#include "opt_global.h"
#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>

/* prototypes */
static void atapi_read(struct atapi_request *, int);
static void atapi_write(struct atapi_request *, int);
static void atapi_finish(struct atapi_request *);
static void atapi_timeout(struct atapi_request *);
static char *atapi_type(int);
static char *atapi_cmd2str(u_int8_t);
static char *atapi_skey2str(u_int8_t);

/* internal vars */
static MALLOC_DEFINE(M_ATAPI, "ATAPI generic", "ATAPI driver generic layer");
static int atapi_dma = 0;
TUNABLE_INT("hw.ata.atapi_dma", atapi_dma);

/* systcl vars */
SYSCTL_DECL(_hw_ata);
SYSCTL_INT(_hw_ata, OID_AUTO, atapi_dma, CTLFLAG_RD, &atapi_dma, 0,
	   "ATAPI device DMA mode control");

/* defines */
#define ATAPI_MAX_RETRIES  	3
#define ATP_PARAM		ATA_PARAM(atp->controller, atp->unit)

void
atapi_attach(struct ata_softc *scp, int device)
{
    struct atapi_softc *atp;

    if (!(atp = malloc(sizeof(struct atapi_softc), M_ATAPI, M_NOWAIT|M_ZERO))) {
	ata_printf(scp, device, "failed to allocate driver storage\n");
	return;
    }
    atp->controller = scp;
    atp->unit = device;
    if (bootverbose) 
	ata_printf(scp, device, 
		   "piomode=%d dmamode=%d udmamode=%d dmaflag=%d\n",
		   ata_pmode(ATP_PARAM), ata_wmode(ATP_PARAM),
		   ata_umode(ATP_PARAM), ATP_PARAM->dmaflag);

    if (atapi_dma && !(ATP_PARAM->drqtype == ATAPI_DRQT_INTR)){
	ata_dmainit(atp->controller, atp->unit,
		    (ata_pmode(ATP_PARAM) < 0) ? 
		    (ATP_PARAM->dmaflag ? 4 : 0) : ata_pmode(ATP_PARAM),
		    (ata_wmode(ATP_PARAM) < 0) ? 
		    (ATP_PARAM->dmaflag ? 2 : 0) : ata_wmode(ATP_PARAM),
		    ata_umode(ATP_PARAM));
    }
    else
	ata_dmainit(atp->controller, atp->unit,
		    ata_pmode(ATP_PARAM) < 0 ? 0 : ata_pmode(ATP_PARAM), -1,-1);

    switch (ATP_PARAM->device_type) {
#ifdef DEV_ATAPICD
    case ATAPI_TYPE_CDROM:
	if (acdattach(atp))
	    goto notfound;
	break; 
#endif
#ifdef DEV_ATAPIFD
    case ATAPI_TYPE_DIRECT:
	if (afdattach(atp))
	    goto notfound;
	break; 
#endif
#ifdef DEV_ATAPIST
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

    /* store our softc signalling we are ready to go */
    scp->dev_softc[ATA_DEV(device)] = atp;
}

void
atapi_detach(struct atapi_softc *atp)
{
    struct atapi_request *request;

    atp->flags |= ATAPI_F_DETACHING;
    ata_printf(atp->controller, atp->unit, "removed from configuration\n");
    switch (ATP_PARAM->device_type) {
#ifdef DEV_ATAPICD
    case ATAPI_TYPE_CDROM:
	acddetach(atp);
	break; 
#endif
#ifdef DEV_ATAPIFD
    case ATAPI_TYPE_DIRECT:
	afddetach(atp);
	break; 
#endif
#ifdef DEV_ATAPIST
    case ATAPI_TYPE_TAPE:
	astdetach(atp);
	break; 
#endif
    default:
	return;
    }
    TAILQ_FOREACH(request, &atp->controller->atapi_queue, chain) {
	if (request->device != atp)
	    continue;
	TAILQ_REMOVE(&atp->controller->atapi_queue, request, chain);
	if (request->driver) {
	    struct bio *bp = (struct bio *) request->driver;
	    biofinish(bp, NULL, ENXIO);
	}
	if (request->dmatab)
	    free(request->dmatab, M_DEVBUF);
        free(request, M_ATAPI);
    }
    atp->controller->dev_softc[ATA_DEV(atp->unit)] = NULL;
    free(atp, M_ATAPI);
}

int	  
atapi_queue_cmd(struct atapi_softc *atp, int8_t *ccb, caddr_t data, 
		int count, int flags, int timeout,
		atapi_callback_t callback, void *driver)
{
    struct atapi_request *request;
    int error, s;
 
    if (!(request = malloc(sizeof(struct atapi_request), M_ATAPI,
			   M_NOWAIT | M_ZERO)))
	return ENOMEM;

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
    if (atp->controller->mode[ATA_DEV(atp->unit)] >= ATA_DMA) {
	if (!(request->dmatab = ata_dmaalloc(atp->controller, atp->unit)))
	    atp->controller->mode[ATA_DEV(atp->unit)] = ATA_PIO;
    }

    s = splbio();

    /* if not using callbacks, prepare to sleep for this request */
    if (!callback)
	asleep((caddr_t)request, PRIBIO, "atprq", 0);

    /* append onto controller queue and try to start controller */
#ifdef ATAPI_DEBUG
    ata_printf(atp->controller, atp->unit, "queueing %s ", 
	       atapi_cmd2str(request->ccb[0]));
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
    if (error)
	 atp->sense = request->sense;
    if (request->dmatab)
	free(request->dmatab, M_DEVBUF);
    free(request, M_ATAPI);
    return error;
}
    
void
atapi_start(struct atapi_softc *atp)
{
    switch (ATP_PARAM->device_type) {
#ifdef DEV_ATAPICD
    case ATAPI_TYPE_CDROM:
	acd_start(atp);
	break; 
#endif
#ifdef DEV_ATAPIFD
    case ATAPI_TYPE_DIRECT:
	afd_start(atp);
	break; 
#endif
#ifdef DEV_ATAPIST
    case ATAPI_TYPE_TAPE:
	ast_start(atp);
	break; 
#endif
    default:
	return;
    }
}

int
atapi_transfer(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;
    int timout;
    u_int8_t reason;

#ifdef ATAPI_DEBUG
    ata_printf(atp->controller, atp->unit, "starting %s ", 
	       atapi_cmd2str(request->ccb[0]));
    atapi_dump("ccb = ", &request->ccb[0], sizeof(request->ccb));
#endif
    /* is this just a POLL DSC command ? */
    if (request->ccb[0] == ATAPI_POLL_DSC) {
	ATA_OUTB(atp->controller->r_io, ATA_DRIVE, ATA_D_IBM | atp->unit);
	DELAY(10);
	if (ATA_INB(atp->controller->r_altio, ATA_ALTSTAT) & ATA_S_DSC)
	    request->error = 0;
	else
	    request->error = EBUSY;
	atapi_finish(request);
	return ATA_OP_FINISHED;
    }

    /* start timeout for this command */
    request->timeout_handle = timeout((timeout_t *)atapi_timeout, 
				      request, request->timeout);

    if (!(request->flags & ATPR_F_INTERNAL))
	atp->cmd = request->ccb[0];

    /* if DMA enabled setup DMA hardware */
    request->flags &= ~ATPR_F_DMA_USED; 
    if ((atp->controller->mode[ATA_DEV(atp->unit)] >= ATA_DMA) &&
	(request->ccb[0] == ATAPI_READ ||
	 request->ccb[0] == ATAPI_READ_BIG ||
	 ((request->ccb[0] == ATAPI_WRITE ||
	   request->ccb[0] == ATAPI_WRITE_BIG) &&
	  !(atp->controller->flags & ATA_ATAPI_DMA_RO))) &&
	!ata_dmasetup(atp->controller, atp->unit, request->dmatab,
		      (void *)request->data, request->bytecount)) {
	request->flags |= ATPR_F_DMA_USED;
    }

    /* start ATAPI operation */
    if (ata_command(atp->controller, atp->unit, ATA_C_PACKET_CMD, 
		    request->bytecount, 0, 0, 0, 
		    (request->flags & ATPR_F_DMA_USED) ? ATA_F_DMA : 0,
		    ATA_IMMEDIATE))
	ata_printf(atp->controller, atp->unit,
		   "failure to send ATAPI packet command\n");

    if (request->flags & ATPR_F_DMA_USED)
	ata_dmastart(atp->controller, atp->unit, 
		     request->dmatab, request->flags & ATPR_F_READ);

    /* command interrupt device ? just return */
    if (ATP_PARAM->drqtype == ATAPI_DRQT_INTR)
	return ATA_OP_CONTINUES;

    /* ready to write ATAPI command */
    timout = 5000; /* might be less for fast devices */
    while (timout--) {
	reason = ATA_INB(atp->controller->r_io, ATA_IREASON);
	atp->controller->status = ATA_INB(atp->controller->r_io, ATA_STATUS);
	if (((reason & (ATA_I_CMD | ATA_I_IN)) |
	     (atp->controller->status&(ATA_S_DRQ|ATA_S_BUSY)))==ATAPI_P_CMDOUT)
	    break;
	DELAY(20);
    }
    if (timout <= 0) {
	ata_printf(atp->controller, atp->unit,
		   "failure to execute ATAPI packet command\n");
	untimeout((timeout_t *)atapi_timeout, request, request->timeout_handle);
	request->error = EIO;
        atapi_finish(request);	
	return ATA_OP_FINISHED;
    }

    /* this seems to be needed for some (slow) devices */
    DELAY(10);

    /* send actual command */
    ATA_OUTSW(atp->controller->r_io, ATA_DATA, (int16_t *)request->ccb,
	      request->ccbsize / sizeof(int16_t));
    return ATA_OP_CONTINUES;
}

int
atapi_interrupt(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;
    int reason, dma_stat = 0;

    reason = (ATA_INB(atp->controller->r_io, ATA_IREASON)&(ATA_I_CMD|ATA_I_IN))|
	     (atp->controller->status & ATA_S_DRQ);

    if (reason == ATAPI_P_CMDOUT) {
	if (!(atp->controller->status & ATA_S_DRQ)) {
	    ata_printf(atp->controller, atp->unit,
		       "command interrupt without DRQ\n");
	    untimeout((timeout_t *)atapi_timeout,
		      request, request->timeout_handle);
	    request->error = EIO;
            atapi_finish(request);	
	    return ATA_OP_FINISHED;
	}
	ATA_OUTSW(atp->controller->r_io, ATA_DATA, (int16_t *)request->ccb,
		  request->ccbsize / sizeof(int16_t));
	return ATA_OP_CONTINUES;
    }

    if (request->flags & ATPR_F_DMA_USED) {
	dma_stat = ata_dmadone(atp->controller);
	if ((atp->controller->status & (ATA_S_ERROR | ATA_S_DWF)) ||
	    dma_stat & ATA_BMSTAT_ERROR) {
	    request->result = ATA_INB(atp->controller->r_io, ATA_ERROR);
	}
	else {
	    request->result = 0;
	    request->donecount = request->bytecount;
	    request->bytecount = 0;
	}
    }
    else {
	int length = ATA_INB(atp->controller->r_io, ATA_CYL_LSB) |
		     ATA_INB(atp->controller->r_io, ATA_CYL_MSB) << 8;

	switch (reason) {
	case ATAPI_P_WRITE:
	    if (request->flags & ATPR_F_READ) {
		request->result = ATA_INB(atp->controller->r_io, ATA_ERROR);
		ata_printf(atp->controller, atp->unit,
			   "%s trying to write on read buffer\n",
			   atapi_cmd2str(atp->cmd));
		break;
	    }
	    atapi_write(request, length);
	    return ATA_OP_CONTINUES;
	
	case ATAPI_P_READ:
	    if (!(request->flags & ATPR_F_READ)) {
		request->result = ATA_INB(atp->controller->r_io, ATA_ERROR);
		ata_printf(atp->controller, atp->unit,
			   "%s trying to read on write buffer\n",
			   atapi_cmd2str(atp->cmd));
		break;
	    }
	    atapi_read(request, length);
	    return ATA_OP_CONTINUES;

	case ATAPI_P_DONEDRQ:
	    ata_printf(atp->controller, atp->unit, "%s DONEDRQ\n",
		       atapi_cmd2str(atp->cmd));
	    if (request->flags & ATPR_F_READ)
		atapi_read(request, length);
	    else
		atapi_write(request, length);
	    /* FALLTHROUGH */

	case ATAPI_P_ABORT:
	case ATAPI_P_DONE:
	    if (atp->controller->status & (ATA_S_ERROR | ATA_S_DWF))
		request->result = ATA_INB(atp->controller->r_io, ATA_ERROR);
	    else 
		if (!(request->flags & ATPR_F_INTERNAL))
		    request->result = 0;
	    break;

	default:
	    ata_printf(atp->controller, atp->unit,
		       "unknown transfer phase %d\n", reason);
	}
    }
    untimeout((timeout_t *)atapi_timeout, request, request->timeout_handle);

    /* check for error, if valid sense key, queue a request sense cmd */
    if ((request->result & ATAPI_SK_MASK) && 
	request->ccb[0] != ATAPI_REQUEST_SENSE) {
	bzero(request->ccb, request->ccbsize);
	request->ccb[0] = ATAPI_REQUEST_SENSE;
	request->ccb[4] = sizeof(struct atapi_reqsense);
	request->bytecount = sizeof(struct atapi_reqsense);
	request->flags &= ATPR_F_QUIET;
	request->flags |= ATPR_F_READ | ATPR_F_INTERNAL;
	TAILQ_INSERT_HEAD(&atp->controller->atapi_queue, request, chain);
    }
    else {
    	if (request->result) {
	    switch ((request->result & ATAPI_SK_MASK)) {
	    case ATAPI_SK_NO_SENSE:
		request->error = 0;
		break;

	    case ATAPI_SK_RECOVERED_ERROR:
		ata_printf(atp->controller, atp->unit,
			   "%s - recovered error\n",
			   atapi_cmd2str(atp->cmd));
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
		request->error = EIO;

		if (request->flags & ATPR_F_QUIET)
		    break;

		ata_printf(atp->controller, atp->unit,
			   "%s - %s asc=0x%02x ascq=0x%02x ",
			   atapi_cmd2str(atp->cmd), 
			   atapi_skey2str(request->sense.sense_key), 
			   request->sense.asc, request->sense.ascq);
		if (request->sense.sksv)
		    printf("sks=0x%02x 0x%02x 0x%02x ",
			   request->sense.sk_specific,
			   request->sense.sk_specific1,
			   request->sense.sk_specific2);
		printf("error=0x%02x\n", request->result & ATAPI_E_MASK);
	    }
	}
	else
	    request->error = 0;
        atapi_finish(request);	
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

int
atapi_test_ready(struct atapi_softc *atp)
{
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	
    return atapi_queue_cmd(atp, ccb, NULL, 0, 0, 30, NULL, NULL);
}
	
int
atapi_wait_dsc(struct atapi_softc *atp, int timeout)
{
    int error = 0;
    int8_t ccb[16] = { ATAPI_POLL_DSC, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    timeout *= hz;
    while (timeout > 0) {
	error = atapi_queue_cmd(atp, ccb, NULL, 0, 0, 0, NULL, NULL);
	if (error != EBUSY)
	    break;
	tsleep((caddr_t)&error, PRIBIO, "atpwt", hz / 2);
	timeout -= (hz / 2);
    }
    return error;
}

void
atapi_dump(char *label, void *data, int len)
{
    u_int8_t *p = data;

    printf("%s %02x", label, *p++);
    while (--len > 0) 
	printf ("-%02x", *p++);
    printf("\n");
}

static void
atapi_read(struct atapi_request *request, int length)
{
    int8_t **buffer = (int8_t **)&request->data;
    int size = min(request->bytecount, length);
    struct ata_softc *scp = request->device->controller;
    int resid;

    if (request->flags & ATPR_F_INTERNAL)
	*buffer = (int8_t *)&request->sense;

    if (scp->flags & ATA_USE_16BIT || (size % sizeof(int32_t)))
	ATA_INSW(scp->r_io, ATA_DATA, (void *)((uintptr_t)*buffer), 
		 size / sizeof(int16_t));
    else
	ATA_INSL(scp->r_io, ATA_DATA, (void *)((uintptr_t)*buffer),
		 size / sizeof(int32_t));

    if (request->bytecount < length) {
	ata_printf(scp, request->device->unit, "read data overrun %d/%d\n",
		   length, request->bytecount);
	for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
	     ATA_INW(scp->r_io, ATA_DATA);
    }
    *buffer += size;
    request->bytecount -= size;
    request->donecount += size;
}

static void
atapi_write(struct atapi_request *request, int length)
{
    int8_t **buffer = (int8_t **)&request->data;
    int size = min(request->bytecount, length);
    struct ata_softc *scp = request->device->controller;
    int resid;

    if (request->flags & ATPR_F_INTERNAL)
	*buffer = (int8_t *)&request->sense;

    if (scp->flags & ATA_USE_16BIT || (size % sizeof(int32_t)))
	ATA_OUTSW(scp->r_io, ATA_DATA, (void *)((uintptr_t)*buffer),
		  size / sizeof(int16_t));
    else
	ATA_OUTSL(scp->r_io, ATA_DATA, (void *)((uintptr_t)*buffer),
		  size / sizeof(int32_t));

    if (request->bytecount < length) {
	ata_printf(scp, request->device->unit, "write data underrun %d/%d\n",
		   length, request->bytecount);
	for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
	    ATA_OUTW(scp->r_io, ATA_DATA, 0);
    }
    *buffer += size;
    request->bytecount -= size;
    request->donecount += size;
}

static void
atapi_finish(struct atapi_request *request)
{
#ifdef ATAPI_DEBUG
    ata_printf(request->device->controller, request->device->unit,
	       "finished %s%s\n", request->callback ? "callback " : "",
	       atapi_cmd2str(request->ccb[0]));
#endif
    if (request->callback) {
	if (!((request->callback)(request))) {
	    if (request->dmatab)
		free(request->dmatab, M_DEVBUF);
	    free(request, M_ATAPI);
	}
    }
    else 
	wakeup((caddr_t)request);	
}

static void 
atapi_timeout(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;
    int s = splbio();

    atp->controller->running = NULL;
    ata_printf(atp->controller, atp->unit, "%s command timeout - resetting\n", 
	       atapi_cmd2str(request->ccb[0]));

    if (request->flags & ATPR_F_DMA_USED) {
	ata_dmadone(atp->controller);
	if (request->retries == ATAPI_MAX_RETRIES) {
	    ata_dmainit(atp->controller, atp->unit,
		        (ata_pmode(ATP_PARAM)<0)?0:ata_pmode(ATP_PARAM),-1,-1);
	    ata_printf(atp->controller, atp->unit, 
		       "trying fallback to PIO mode\n");
	    request->retries = 0;
	}
    }

    /* if retries still permit, reinject this request */
    if (request->retries++ < ATAPI_MAX_RETRIES)
        TAILQ_INSERT_HEAD(&atp->controller->atapi_queue, request, chain);
    else {
        /* retries all used up, return error */
	request->error = EIO;
	wakeup((caddr_t)request);
    } 
    ata_reinit(atp->controller);
    splx(s);
}

static char *
atapi_type(int type)
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

static char *
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
    default: {
	static char buffer[16];
	sprintf(buffer, "unknown CMD (0x%02x)", cmd);
	return buffer;
	}
    }
}

static char *
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
