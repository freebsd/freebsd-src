/*-
 * Copyright (c) 1998,1999 Søren Schmidt
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

#include "ata.h"
#include "atapicd.h"
#include "atapist.h"
#include "atapifd.h" 
#include "apm.h"

#if NATA > 0 && (NATAPICD > 0 || NATAPIFD > 0 || NATAPIST > 0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>
#include <pci/pcivar.h>
#include <machine/clock.h>
#if NAPM > 0
#include <machine/apm_bios.h>
#endif
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>

/* prototypes */
static void atapi_attach(void *);
static int32_t atapi_getparam(struct atapi_softc *);
static void atapi_timeout(struct atapi_request *request);
static int8_t *atapi_type(int32_t);
static int8_t *atapi_cmd2str(u_int8_t);
static int8_t *atapi_skey2str(u_int8_t);
static int32_t atapi_wait(struct atapi_softc *, u_int8_t);
static void atapi_init(void);

/* extern references */
int32_t acdattach(struct atapi_softc *);
int32_t afdattach(struct atapi_softc *);
int32_t astattach(struct atapi_softc *);

static struct intr_config_hook *atapi_attach_hook;

static __inline int
apiomode(struct atapi_params *ap)
{
    if ((ap->atavalid & 2) == 2) {
	if ((ap->apiomodes & 2) == 2) return 4;
	if ((ap->apiomodes & 1) == 1) return 3;
    }
    return -1;
}

static __inline int
wdmamode(struct atapi_params *ap)
{
    if ((ap->atavalid & 2) == 2) {
	if ((ap->wdmamodes & 4) == 4) return 2;
	if ((ap->wdmamodes & 2) == 2) return 1;
	if ((ap->wdmamodes & 1) == 1) return 0;
    }
    return -1;
}

static __inline int
udmamode(struct atapi_params *ap)
{
    if ((ap->atavalid & 4) == 4) {
	if ((ap->udmamodes & 4) == 4) return 2;
	if ((ap->udmamodes & 2) == 2) return 1;
	if ((ap->udmamodes & 1) == 1) return 0;
    }
    return -1;
}

static void
atapi_attach(void *notused)
{
    struct atapi_softc *atp;
    int32_t ctlr, dev;
    int8_t model_buf[40+1];
    int8_t revision_buf[8+1];

    /* now, run through atadevices and look for ATAPI devices */
    for (ctlr=0; ctlr<MAXATA; ctlr++) {
	if (!atadevices[ctlr]) continue;
	for (dev=0; dev<2; dev++) {
	    if (atadevices[ctlr]->devices & 
		(dev ? ATA_ATAPI_SLAVE : ATA_ATAPI_MASTER)) {
		if (!(atp = malloc(sizeof(struct atapi_softc),
				   M_DEVBUF, M_NOWAIT))) {
		    printf("atapi: failed to allocate driver storage\n");
		    continue;
		}
		bzero(atp, sizeof(struct atapi_softc));
		atp->controller = atadevices[ctlr];
		atp->unit = (dev == 0) ? ATA_MASTER : ATA_SLAVE;
		if (atapi_getparam(atp)) {
		    free(atp, M_DEVBUF);
		    continue;
		}
		if (bootverbose) 
			printf("ata%d-%s: piomode=%d dmamode=%d "
			       "udmamode=%d dmaflag=%d\n",
			       ctlr, (dev == ATA_MASTER) ? "master" : "slave",
			       apiomode(atp->atapi_parm),
			       wdmamode(atp->atapi_parm),
			       udmamode(atp->atapi_parm),
			       atp->atapi_parm->dmaflag);

		if (!(atp->atapi_parm->drqtype == ATAPI_DRQT_INTR) &&
		    !ata_dmainit(atp->controller, atp->unit,
				 (apiomode(atp->atapi_parm) < 0) ? 
				 (atp->atapi_parm->dmaflag ? 4 : 0) : 
				 apiomode(atp->atapi_parm),
				 (wdmamode(atp->atapi_parm) < 0) ? 
				 (atp->atapi_parm->dmaflag ? 2 : 0) : 
				 wdmamode(atp->atapi_parm),
				 udmamode(atp->atapi_parm)))
		    atp->flags |= ATAPI_F_DMA_ENABLED;

		switch (atp->atapi_parm->device_type) {
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
		    bpack(atp->atapi_parm->model, model_buf, sizeof(model_buf));
		    bpack(atp->atapi_parm->revision, revision_buf, 
			  sizeof(revision_buf));
		    printf("ata%d-%s: <%s/%s> %s device - NO DRIVER!\n", 
			   ctlr, (dev == ATA_MASTER) ? "master" : "slave",
			   model_buf, revision_buf,
			   atapi_type(atp->atapi_parm->device_type));
		    free(atp, M_DEVBUF);
		    atp = NULL;
		}
		/* store our softc */
		atp->controller->dev_softc[(atp->unit==ATA_MASTER)?0:1] = atp;
	    }
	}
    }
    config_intrhook_disestablish(atapi_attach_hook); 
}

static int32_t
atapi_getparam(struct atapi_softc *atp)
{
    struct atapi_params *atapi_parm;
    int8_t buffer[DEV_BSIZE];

    /* select drive */
    outb(atp->controller->ioaddr + ATA_DRIVE, ATA_D_IBM | atp->unit);
    DELAY(1);
    ata_command(atp->controller, atp->unit, ATA_C_ATAPI_IDENTIFY,
		0, 0, 0, 0, 0, ATA_WAIT_INTR);	   
    if (atapi_wait(atp, ATA_S_DRQ))
	return -1;
    insw(atp->controller->ioaddr + ATA_DATA, buffer,
	 sizeof(buffer)/sizeof(int16_t));
    if (atapi_wait(atp, 0))
	return -1;
    if (!(atapi_parm = malloc(sizeof(struct atapi_params), M_DEVBUF, M_NOWAIT)))
	return -1; 
    bcopy(buffer, atapi_parm, sizeof(struct atapi_params));
    if (!((atapi_parm->model[0] == 'N' && atapi_parm->model[1] == 'E') ||
	  (atapi_parm->model[0] == 'F' && atapi_parm->model[1] == 'X')))
	bswap(atapi_parm->model, sizeof(atapi_parm->model));
    btrim(atapi_parm->model, sizeof(atapi_parm->model));
    bswap(atapi_parm->revision, sizeof(atapi_parm->revision));
    btrim(atapi_parm->revision, sizeof(atapi_parm->revision));
    atp->atapi_parm = atapi_parm;
    return 0;
}

int32_t	  
atapi_immed_cmd(struct atapi_softc *atp, int8_t *ccb, void *data, 
		int32_t count, int32_t flags, int32_t timeout)
{
    struct atapi_request *request;
    int32_t error, s;
 
    if (!(request = malloc(sizeof(struct atapi_request), M_DEVBUF, M_NOWAIT)))
	return ENOMEM;
    bzero(request, sizeof(struct atapi_request));
    request->device = atp;
    request->data = data;
    request->bytecount = count;
    request->donecount = 0;
    request->flags = flags;
    request->timeout = timeout * hz;
    request->ccbsize = (atp->atapi_parm->cmdsize) ? 16 : 12;
    bcopy(ccb, request->ccb, request->ccbsize);

    s = splbio();

    TAILQ_INSERT_HEAD(&atp->controller->atapi_queue, request, chain);

    /* try to start controller */
    if (atp->controller->active == ATA_IDLE)
	ata_start(atp->controller);

    splx(s);

    /* wait for command to complete */
    tsleep((caddr_t)request, PRIBIO, "atpim", 0);

#ifdef ATAPI_DEBUG
    printf("atapi: phew, got back from tsleep\n");
#endif

    error = request->result;
    free(request, M_DEVBUF);
    return error;
}
    
int32_t	  
atapi_queue_cmd(struct atapi_softc *atp, int8_t *ccb, void *data, 
		int32_t count, int32_t flags, int32_t timeout,
		atapi_callback_t callback, void *driver, struct buf *bp)
{
    struct atapi_request *request;
    int32_t error, s;
 
    if (!(request = malloc(sizeof(struct atapi_request), M_DEVBUF, M_NOWAIT)))
	return ENOMEM;

    bzero(request, sizeof(struct atapi_request));
    request->device = atp;
    request->data = data;
    request->bytecount = count;
    request->donecount = 0;
    request->flags = flags;
    request->timeout = timeout * hz;
    request->ccbsize = (atp->atapi_parm->cmdsize) ? 16 : 12;
    bcopy(ccb, request->ccb, request->ccbsize);
    if (callback) {
	request->callback = callback;
	request->bp = bp;
	request->driver = driver;
    }

    s = splbio();

    /* link onto controller queue */
    TAILQ_INSERT_TAIL(&atp->controller->atapi_queue, request, chain);

    /* try to start controller */
    if (atp->controller->active == ATA_IDLE)
	ata_start(atp->controller);

    splx(s);

    /* wait for command to complete */
    tsleep((caddr_t)request, PRIBIO, "atprq", 0);

#ifdef ATAPI_DEBUG
    printf("atapi: phew, got back from tsleep\n");
#endif

    error = request->result;
    if (request->callback) {
	(request->callback)(request);
	error = 0;
    }
    free(request, M_DEVBUF);
    return atapi_error(atp, error);
}
    
void
atapi_transfer(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;
    int32_t timout;
    int8_t reason;

#ifdef ATAPI_DEBUG
    printf("atapi: starting %s ", atapi_cmd2str(request->ccb[0]));
    atapi_dump("ccb = ", &request->ccb[0], sizeof(request->ccb));
#endif
    atp->cmd = request->ccb[0];

    /* start timeout for this command */
    request->timeout_handle = timeout((timeout_t *)atapi_timeout, 
				      request, request->timeout);

    /* if DMA enabled setup DMA hardware */
    if ((atp->flags & ATAPI_F_DMA_ENABLED) &&
	(request->ccb[0] == ATAPI_READ ||
	 request->ccb[0] == ATAPI_READ_BIG ||
	 ((request->ccb[0] == ATAPI_WRITE ||
	   request->ccb[0] == ATAPI_WRITE_BIG) &&
	  !(atp->controller->flags & ATA_ATAPI_DMA_RO))) &&
	!ata_dmasetup(atp->controller, atp->unit,
		      (void *)request->data, request->bytecount,
		      request->flags & A_READ)) {
	atp->flags |= ATAPI_F_DMA_USED;
    }

    /* start ATAPI operation */
    ata_command(atp->controller, atp->unit, ATA_C_PACKET_CMD, 
		request->bytecount, 0, 0, 0, 
		(atp->flags & ATAPI_F_DMA_USED) ? ATA_F_DMA : 0,
		ATA_IMMEDIATE);

    if (atp->flags & ATAPI_F_DMA_USED)
	ata_dmastart(atp->controller, atp->unit);

    /* command interrupt device ? just return */
    if (atp->atapi_parm->drqtype == ATAPI_DRQT_INTR)
	return;

    /* ready to write ATAPI command */
    timout = 5000; /* might be less for fast devices */
    while (timout--) {
	reason = inb(atp->controller->ioaddr + ATA_IREASON);
	atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS);
	if (((reason & (ATA_I_CMD | ATA_I_IN)) |
	     (atp->controller->status&(ATA_S_DRQ|ATA_S_BSY))) == ATAPI_P_CMDOUT)
	    break;
	DELAY(20);
    }
    if (timout <= 0) {
	request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	printf("atapi_transfer: device hanging on packet cmd\n");
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
    int32_t length, reason, resid, dma_stat = 0;

#ifdef ATAPI_DEBUG
    printf("atapi_interrupt: enter\n");
#endif
    reason = (inb(atp->controller->ioaddr+ATA_IREASON) & (ATA_I_CMD|ATA_I_IN)) |
	     (atp->controller->status & ATA_S_DRQ);

    if (reason == ATAPI_P_CMDOUT) {
	if (!(atp->controller->status & ATA_S_DRQ)) {
	    request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	    printf("atapi_interrupt: command interrupt, but no DRQ\n");
	    goto op_finished;
	}
	outsw(atp->controller->ioaddr + ATA_DATA, request->ccb,
	      request->ccbsize / sizeof(int16_t));
	return ATA_OP_CONTINUES;
    }

    if (atp->flags & ATAPI_F_DMA_USED)
	dma_stat = ata_dmadone(atp->controller, atp->unit);

    if (atapi_wait(atp, 0) < 0) {
	printf("atapi_interrupt: timeout waiting for status");
	atp->flags &= ~ATAPI_F_DMA_USED; 
	request->result = inb(atp->controller->ioaddr + ATA_ERROR) | 0xf0;
	goto op_finished;
    }

    if (atp->flags & ATAPI_F_DMA_USED) {
	atp->flags &= ~ATAPI_F_DMA_USED; 
	if ((atp->controller->status & (ATA_S_ERROR | ATA_S_DWF)) ||
	    dma_stat != ATA_BMSTAT_INTERRUPT) {
	    request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	}
	else {
	    request->result = 0;
	    request->donecount = request->bytecount;
	    request->bytecount = 0;
	}
	goto op_finished;
    }

    length = inb(atp->controller->ioaddr + ATA_CYL_LSB);
    length |= inb(atp->controller->ioaddr + ATA_CYL_MSB) << 8;
#ifdef ATAPI_DEBUG
    printf("atapi_interrupt: length=%d reason=0x%02x\n", length, reason);
#endif

    switch (reason) {
    
    case ATAPI_P_WRITE:
	if (request->flags & A_READ) {
	    request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	    printf("ata%d-%s: %s trying to write on read buffer\n",
		   atp->controller->lun,
		   (atp->unit == ATA_MASTER) ? "master" : "slave",
		   atapi_cmd2str(atp->cmd));
	    goto op_finished;
	}
	if (request->bytecount < length) {
	    printf("atapi_interrupt: write data underrun %d/%d\n",
		   length, request->bytecount);
#if 0
	    outsw(atp->controller->ioaddr + ATA_DATA, 
		  (void *)((uintptr_t)request->data), length / sizeof(int16_t));
#else
	    outsl(atp->controller->ioaddr + ATA_DATA, 
		  (void *)((uintptr_t)request->data), length / sizeof(int32_t));
#endif
	    for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
		outw(atp->controller->ioaddr + ATA_DATA, 0);
	}
	else {
	    outsw(atp->controller->ioaddr + ATA_DATA, 
		  (void *)((uintptr_t)request->data), length / sizeof(int16_t));
	}
	request->bytecount -= length;
	request->donecount += length;
	request->data += length;
	return ATA_OP_CONTINUES;
	
    case ATAPI_P_READ:
	if (!(request->flags & A_READ)) {
	    request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	    printf("ata%d-%s: %s trying to read on write buffer\n",
		   atp->controller->lun,
		   (atp->unit == ATA_MASTER) ? "master" : "slave",
		   atapi_cmd2str(atp->cmd));
	    goto op_finished;
	}
	if (request->bytecount < length) {
	    printf("atapi_interrupt: read data overrun %d/%d\n",
		   length, request->bytecount);
#if 0
	    insw(atp->controller->ioaddr + ATA_DATA, 
		  (void *)((uintptr_t)request->data), length / sizeof(int16_t));
#else
	    insl(atp->controller->ioaddr + ATA_DATA, 
		  (void *)((uintptr_t)request->data), length / sizeof(int32_t));
#endif
	    for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
		inw(atp->controller->ioaddr + ATA_DATA);
	}			
	else {
	    insw(atp->controller->ioaddr + ATA_DATA,
		  (void *)((uintptr_t)request->data), length / sizeof(int16_t));
	}
	request->bytecount -= length;
	request->donecount += length;
	request->data += length;
	return ATA_OP_CONTINUES;

    case ATAPI_P_ABORT:
    case ATAPI_P_DONE:
	if (atp->controller->status & (ATA_S_ERROR | ATA_S_DWF))
	    request->result = inb(atp->controller->ioaddr + ATA_ERROR);
	else
	    request->result = 0;
#ifdef ATAPI_DEBUG
	if (request->bytecount > 0) {
	    printf("atapi_interrupt: %s size problem, %d bytes residue\n",
		   (request->flags & A_READ) ? "read" : "write", 
		   request->bytecount);
	}
#endif
	goto op_finished;
    default:
	printf("atapi_interrupt: unknown transfer phase %d\n", reason);
    }

op_finished:
    untimeout((timeout_t *)atapi_timeout, request, request->timeout_handle);
    wakeup((caddr_t)request);	
#ifdef ATAPI_DEBUG
    printf("atapi_interrupt: error=0x%02x\n", request->result);
#endif
    return ATA_OP_FINISHED;
}

static void 
atapi_timeout(struct atapi_request *request)
{
    struct atapi_softc *atp = request->device;

    printf("ata%d-%s: atapi_timeout: cmd=%s - resetting\n", 
	   atp->controller->lun, (atp->unit == ATA_MASTER) ? "master" : "slave",
	   atapi_cmd2str(request->ccb[0]));

    if (request->flags & ATAPI_F_DMA_USED)
	ata_dmadone(atp->controller, atp->unit);

    ata_reinit(atp->controller);
}

void
atapi_reinit(struct atapi_softc *atp)
{
    /* reinit device parameters */
    ata_dmainit(atp->controller, atp->unit,
		(apiomode(atp->atapi_parm) < 0) ?
		(atp->atapi_parm->dmaflag ? 4 : 0) : apiomode(atp->atapi_parm),
		(wdmamode(atp->atapi_parm) < 0) ? 
		(atp->atapi_parm->dmaflag ? 2 : 0) : wdmamode(atp->atapi_parm),
		udmamode(atp->atapi_parm));
}

int32_t
atapi_error(struct atapi_softc *atp, int32_t error)
{
    struct atapi_reqsense sense;
    int8_t cmd = atp->cmd;

    if (cmd == ATAPI_REQUEST_SENSE)
	return 0;

    switch ((error & ATAPI_SK_MASK)) {
    case ATAPI_SK_RESERVED:
	printf("atapi_error: %s - timeout error = %02x\n", 
	       atapi_cmd2str(cmd), error & ATAPI_E_MASK);
	return EIO;

    case ATAPI_SK_NO_SENSE:
	return 0;

    case ATAPI_SK_RECOVERED_ERROR:
	printf("atapi_error: %s - recovered error\n", atapi_cmd2str(cmd));
	return 0;

    case ATAPI_SK_NOT_READY:
	atp->flags |= ATAPI_F_MEDIA_CHANGED;
	return EBUSY;

    case ATAPI_SK_UNIT_ATTENTION:
	atp->flags |= ATAPI_F_MEDIA_CHANGED;
	return EIO;
    }

    atapi_request_sense(atp, &sense);
    printf("atapi_error: %s - %s skey=%01x asc=%02x ascq=%02x error=%02x\n", 
	   atapi_cmd2str(cmd), atapi_skey2str(sense.sense_key),
	   sense.sense_key, sense.asc, sense.ascq, error & ATAPI_E_MASK);
    return EIO;
}

int32_t
atapi_test_ready(struct atapi_softc *atp)
{
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	
    return atapi_immed_cmd(atp, ccb, NULL, 0, 0, 30);
}
	
int32_t
atapi_wait_ready(struct atapi_softc *atp, int32_t timeout)
{
    int32_t error = 0, timout = timeout * hz;

    while (timout > 0) {
	error = atapi_test_ready(atp);
	if ((error & ATAPI_SK_MASK) != ATAPI_SK_NOT_READY) 
	    break;
#ifdef ATAPI_DEBUG
	printf("atapi: waiting on error=%02x\n", error);
#endif
	tsleep((caddr_t)&error, PRIBIO, "atpwt", 50);
	timout -= 50;
    }
    return error;
}

void
atapi_request_sense(struct atapi_softc *atp, struct atapi_reqsense *sense)
{
    int8_t ccb[16] = { ATAPI_REQUEST_SENSE, 0, 0, 0, sizeof(sense),
		       0, 0, 0 ,0 ,0, 0, 0, 0, 0, 0, 0 };
 
    bzero(sense, sizeof(struct atapi_reqsense));
    atapi_immed_cmd(atp, ccb, sense, sizeof(struct atapi_reqsense), 
			A_READ, 10);
}

void
atapi_dump(int8_t *label, void *data, int32_t len)
{
    u_int8_t *p = data;

    printf ("atapi: %s %x", label, *p++);
    while (--len > 0) 
	printf ("-%02x", *p++);
    printf ("\n");
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
    case 0x34: return ("TAPE_READ_POSITION");
    case 0x35: return ("SYNCHRONIZE_CACHE");
    case 0x42: return ("READ_SUBCHANNEL");
    case 0x43: return ("READ_TOC");
    case 0x51: return ("READ_DISC_INFO");
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
    case 0x47: return ("PLAY_MSF");
    case 0x4b: return ("PAUSE");
    case 0x48: return ("PLAY_TRACK");
    case 0xa1: return ("BLANK_CMD");
    case 0xa5: return ("PLAY_BIG");
    case 0xb4: return ("PLAY_CD");
    case 0xbd: return ("MECH_STATUS");
    case 0xbe: return ("READ_CD");
    default: {
	static int8_t buffer[16];
	sprintf(buffer, "Unknown CMD (0x%02x)", cmd);
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

static int32_t
atapi_wait(struct atapi_softc *atp, u_int8_t mask)
{
    u_int32_t timeout = 0;
    
    while (timeout++ <= 500000) {	 /* timeout 5 secs */
	atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS);

	/* if drive fails status, reselect the drive just to be sure */
	if (atp->controller->status == 0xff) {
	    outb(atp->controller->ioaddr + ATA_DRIVE, ATA_D_IBM | atp->unit);
	    DELAY(1);
	    atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS);
	}
	if (!(atp->controller->status & ATA_S_BSY) && (atp->controller->status & ATA_S_DRDY))  
	    break;	      
	DELAY (10);	   
    }	 
    if (timeout <= 0)	 
	return -1;	    
    if (!mask)	   
	return (atp->controller->status & ATA_S_ERROR);	 
    
    /* Wait 50 msec for bits wanted. */	   
    for (timeout=5000; timeout>0; --timeout) {	  
	atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS);
	if ((atp->controller->status & mask) == mask)	    
	    return (atp->controller->status & ATA_S_ERROR);	      
	DELAY (10);	   
    }	  
    return -1;	    
}   

static void
atapi_init(void)
{
    /* register callback for when interrupts are enabled */
    if (!(atapi_attach_hook =
	(struct intr_config_hook *)malloc(sizeof(struct intr_config_hook),
					  M_TEMP, M_NOWAIT))) {
	printf("atapi: malloc attach_hook failed\n");
	return;
    }
    bzero(atapi_attach_hook, sizeof(struct intr_config_hook));

    atapi_attach_hook->ich_func = atapi_attach;
    if (config_intrhook_establish(atapi_attach_hook) != 0) {
	printf("atapi: config_intrhook_establish failed\n");
	free(atapi_attach_hook, M_TEMP);
    }

}

SYSINIT(atconf, SI_SUB_CONFIGURE, SI_ORDER_SECOND, atapi_init, NULL)
#endif /* NATA > 0 && (NATAPICD > 0 || NATAPIFD > 0 || NATAPIST > 0) */
