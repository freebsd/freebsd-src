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
 *	$Id: atapi-all.c,v 1.1 1999/03/01 21:19:18 sos Exp $
 */

#include "ata.h"
#include "atapicd.h"
#include "atapist.h"
#include "atapifd.h" 
#include "opt_devfs.h"

#if NATA > 0 

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/devicestat.h>
#include <machine/clock.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>

/* prototypes */
static void atapi_attach(void *);
static int8_t *atapi_type(int32_t);
static int8_t *atapi_cmd2str(u_int8_t);
static void atapi_init(void);

/* extern references */
int32_t acdattach(struct atapi_softc *);
int32_t afdattach(struct atapi_softc *);
int32_t astattach(struct atapi_softc *);

static struct intr_config_hook *atapi_attach_hook;

static void
atapi_attach(void *notused)
{
    int32_t ctlr, dev;
    int8_t model_buf[40+1];
    int8_t revision_buf[8+1];

    /* now, run through atadevices and look for ATAPI devices */
    for (ctlr=0; ctlr<MAXATA && atadevices[ctlr]; ctlr++) {
	for (dev=0; dev<2; dev++) {
	    struct ata_softc *scp = atadevices[ctlr];

	    if (scp->atapi_parm[dev]) {
                struct atapi_softc *atp;

                atp = malloc(sizeof(struct atapi_softc), M_DEVBUF, M_NOWAIT);
                bzero(atp, sizeof(struct atapi_softc));
                atp->controller = scp;
                atp->atapi_parm = scp->atapi_parm[dev];
                atp->unit = (dev) ? ATA_SLAVE : ATA_MASTER;

		switch (scp->atapi_parm[dev]->device_type) {
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
		    free(atp, M_DEVBUF);
	            bpack(scp->atapi_parm[dev]->model, model_buf,
			  sizeof(model_buf));
		    bpack(scp->atapi_parm[dev]->revision, revision_buf, 
		          sizeof(revision_buf));
		    printf("atapi: <%s/%s> %s device at ata%d as %s "
			   "- NO DRIVER!\n", 
           	           model_buf, revision_buf,
		           atapi_type(scp->atapi_parm[dev]->device_type),
		           ctlr,
		           (dev) ? "slave" : "master ");
		}
            }
	}
    }
    config_intrhook_disestablish(atapi_attach_hook); 
}

int32_t   
atapi_queue_cmd(struct atapi_softc *atp, int8_t *ccb, void *data, 
		int32_t count, int32_t flags,
		atapi_callback_t callback, void *driver, struct buf *bp)
{
    struct atapi_request *request;
    int32_t error = 0; 
    int32_t s = splbio();
 
    if (!(request = malloc(sizeof(struct atapi_request), M_DEVBUF, M_NOWAIT))) {
        splx(s);
        return -1;
    }
    bzero(request, sizeof(struct atapi_request));
    request->device = atp;
    request->data = data;
    request->bytecount = count;
    request->flags = flags;
    if (callback) {
    	request->callback = callback;
	request->bp = bp;
	request->driver = driver;
    }
    request->ccbsize = (atp->atapi_parm->cmdsize) ? 16 : 12;
    bcopy(ccb, request->ccb, request->ccbsize);

    /* link onto controller queue */
    TAILQ_INSERT_TAIL(&atp->controller->atapi_queue, request, chain);
#ifdef ATAPI_DEBUG
    printf("atapi: trying to start %s command\n", atapi_cmd2str(ccb[0]));
#endif
    /* try to start controller */
    if (!atp->controller->active)
        ata_start(atp->controller);

    if (!callback) {
    	/* wait for command to complete */
    	tsleep((caddr_t)request, PRIBIO, "atprq", 0);
#ifdef ATAPI_DEBUG
    printf("atapi: phew, got back from tsleep\n");
#endif
    	error = request->result;
    	free(request, M_DEVBUF);
    }
    splx(s);
    return error;
}
    
void
atapi_transfer(struct atapi_request *request)
{
    struct atapi_softc *atp;
    int32_t timeout;
    int8_t int_reason; /* not needed really */

    /* get device params */
    atp = request->device;

    /* start ATAPI operation */
    outb(atp->controller->ioaddr + ATA_DRIVE, ATA_D_IBM | atp->unit);
    if (atapi_wait(atp->controller, 0) < 0) {
        printf ("atapi_transfer: timeout waiting to send PACKET command\n");
	/* now what ? SOS */
    }
    outb(atp->controller->ioaddr + ATA_PRECOMP, 0);
    outb(atp->controller->ioaddr + ATA_COUNT, 0);
    outb(atp->controller->ioaddr + ATA_SECTOR, 0);
    outb(atp->controller->ioaddr + ATA_CYL_LSB, request->bytecount);
    outb(atp->controller->ioaddr + ATA_CYL_MSB, request->bytecount >> 8);
    outb(atp->controller->ioaddr + ATA_CMD, ATA_C_PACKET_CMD);
 
    /* wait for interrupt ?? not supported yet. */
    /* just return then and let atapi_interrupt handle it */

    /* ready to write ATAPI command */
    timeout = 5000; /* might be less for fast devices */
    while (timeout--) {
	int_reason = inb(atp->controller->ioaddr + ATA_COUNT);
	atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS);
	if (((int_reason & (ATA_I_CMD | ATA_I_IN)) |
	     (atp->controller->status&(ATA_S_DRQ|ATA_S_BSY))) == ATAPI_P_CMDOUT)
	    break;
	DELAY(20);
    }
    if (timeout <= 0) {
	atp->controller->error = inb(atp->controller->ioaddr + ATA_ERROR);
	printf("atapi_transfer: bad command phase\n");
	/* now what ?? SOS atapi-done & again */
    }

    /* send actual command */
    outsw(atp->controller->ioaddr + ATA_DATA, request->ccb, 
	  request->ccbsize / sizeof(int16_t));
}

void
atapi_interrupt(struct atapi_request *request)
{
    struct atapi_softc *atp;
    int32_t length, reason, resid;

#ifdef ATAPI_DEBUG
printf("atapi_interrupt: enter\n");
#endif
    /* get device params */
    atp = request->device;

    /* get drive status */
    if (atapi_wait(atp->controller, 0) < 0) {
        printf("atapi_interrupt: timeout waiting for status");
	/* maybe check sense code ??  SOS */
	return;
    }
    atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS); 
    atp->controller->error = inb(atp->controller->ioaddr + ATA_ERROR);

    length = inb(atp->controller->ioaddr + ATA_CYL_LSB);
    length |= inb(atp->controller->ioaddr + ATA_CYL_MSB) << 8;
    reason = (inb(atp->controller->ioaddr + ATA_COUNT)&(ATA_I_CMD|ATA_I_IN)) |
	     (atp->controller->status & ATA_S_DRQ);

#ifdef ATAPI_DEBUG
printf("atapi_interrupt: length=%d reason=0x%02x\n", length, reason);
#endif

    switch (reason) {
    
    case ATAPI_P_CMDOUT:
        printf("atapi_interrupt: command interrupt, not supported yet\n");
#if notyet
    	/* send actual command */
	if (!(atp->status & ATA_S_DRQ))
            printf("atapi_interrupt: command interrupt, but no DRQ\n");
	else
    	    outsw(atp->controller->ioaddr + ATA_DATA, request->ccb,
                  request->ccdsize / sizeof(int16_t));
#endif
	return;
	
    case ATAPI_P_WRITE:
	if (request->flags & A_READ) {
	    printf("atapi_interrupt: trying to write on read buffer\n");
	    break;
        }
	if (request->bytecount < length) {
	    printf("atapi_interrupt: write data underrun %d/%d\n",
		   length, request->bytecount);
	    outsw(atp->controller->ioaddr + ATA_DATA, 
		  (void *)((int32_t)request->data), /* + donecount ?? */
		  length / sizeof(int16_t));
	    for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
		outw(atp->controller->ioaddr + ATA_DATA, 0);
	}
	else {
	    outsw(atp->controller->ioaddr + ATA_DATA, 
                  (void *)((int32_t)request->data), /* + donecount ?? */
                  length / sizeof(int16_t));
	}
	request->bytecount -= length;
	request->data += length;
	return;
	
    case ATAPI_P_READ:
        if (!(request->flags & A_READ)) {
            printf("atapi_interrupt: trying to read on write buffer\n");
            break;
        }
        if (request->bytecount < length) {
            printf("atapi_interrupt: read data overrun %d/%d\n",
                   length, request->bytecount);
            insw(atp->controller->ioaddr + ATA_DATA, 
                  (void *)((int32_t)request->data), /* + donecount ?? */
                  length / sizeof(int16_t));
            for (resid=request->bytecount; resid<length; resid+=sizeof(int16_t))
                inw(atp->controller->ioaddr + ATA_DATA);
        }                       
        else {
            insw(atp->controller->ioaddr + ATA_DATA,
                  (void *)((int32_t)request->data), /* + donecount ?? */
                  length / sizeof(int16_t));
        }
	request->bytecount -= length;
	request->data += length;
        return;

    case ATAPI_P_ABORT:
    case ATAPI_P_DONE:
	request->result = 0;
	if (atp->controller->status & (ATA_S_ERROR | ATA_S_DWF)) {
	    /* check sense !! SOS */
	    request->result = atp->controller->error;
	}
#ifdef ATAPI_DEBUG
        if (request->bytecount > 0) {
	    printf("atapi_interrupt: %s size problem, %d bytes residue\n",
		   (request->flags & A_READ) ? "read" : "write", 
		   request->bytecount);
	}
#endif
	break;
    default:
	printf("atapi_interrupt: unknown transfer phase\n");
    }

    TAILQ_REMOVE(&atp->controller->atapi_queue, request, chain);
#ifdef ATAPI_DEBUG
printf("atapi_interrupt: error=0x%02x\n", request->result);
#endif
    if (request->callback) {
	(request->callback)(request);
	free(request, M_DEVBUF);
    }
    else
	wakeup((caddr_t)request);	
    atp->controller->active = ATA_IDLE;
    ata_start(atp->controller);
}

void 
atapi_error(struct atapi_softc *atp, int32_t error)
{
    printf("atapi: error = 0x%02x\n", error);
}

void
atapi_dump(int8_t *label, void *data, int32_t len)
{
        u_int8_t *p = data;

        printf ("atapi: %s %x", label, *p++);
        while (--len > 0) printf ("-%02x", *p++);
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
    case 0x01: return ("REZERO_UNIT");
    case 0x03: return ("REQUEST_SENSE");
    case 0x04: return ("FORMAT_UNIT");
    case 0x1a: return ("TAPE_MODE_SENSE");
    case 0x1b: return ("START_STOP");
    case 0x1e: return ("PREVENT_ALLOW");
    case 0x25: return ("READ_CAPACITY");
    case 0x28: return ("READ_BIG");
    case 0x2a: return ("WRITE_BIG");
    case 0x35: return ("SYNCHRONIZE_CACHE");
    case 0x42: return ("READ_SUBCHANNEL");
    case 0x43: return ("READ_TOC");
    case 0x51: return ("READ_DISC_INFO");
    case 0x52: return ("READ_TRACK_INFO");
    case 0x53: return ("RESERVE_TRACK");
    case 0x54: return ("SEND_OPC_INFO");
    case 0x55: return ("MODE_SELECT");
    case 0x58: return ("REPAIR_TRACK");
    case 0x59: return ("READ_MASTER_CUE");
    case 0x5a: return ("MODE_SENSE");
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
	sprintf(buffer, "Unknown 0x%02x", cmd);
	return buffer;
	}
    }
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
#endif /* NATA */
