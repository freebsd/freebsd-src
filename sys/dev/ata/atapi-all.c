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
 *	$Id: atapi-all.c,v 1.10 1999/06/25 09:03:01 sos Exp $
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
#include <pci/pcivar.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>

/* prototypes */
static void atapi_attach(void *);
static int32_t atapi_getparam(struct atapi_softc *);
static int8_t *atapi_type(int32_t);
static int8_t *atapi_cmd2str(u_int8_t);
static int8_t * atapi_skey2str(u_int8_t);
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
                printf("atapi: piomode=%d, dmamode=%d, udmamode=%d\n",
                       apiomode(atp->atapi_parm),
                       wdmamode(atp->atapi_parm),
                       udmamode(atp->atapi_parm));
        	if (!(atp->atapi_parm->drqtype == ATAPI_DRQT_INTR) &&
                    !ata_dmainit(atp->controller, atp->unit,
                                 apiomode(atp->atapi_parm),
                                 wdmamode(atp->atapi_parm),
                                 udmamode(atp->atapi_parm)))
                    atp->flags |= ATAPI_F_DMA_ENABLED;

		printf("atapi: %s transfer mode set\n", 
		       (atp->flags & ATAPI_F_DMA_ENABLED) ? "DMA" :"PIO");

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
		    printf("atapi: <%s/%s> %s device at ata%d as %s "
			   "- NO DRIVER!\n", 
           	           model_buf, revision_buf,
		           atapi_type(atp->atapi_parm->device_type),
		           ctlr,
		           (dev) ? "slave" : "master ");
		    free(atp, M_DEVBUF);
		}
            }
	}
    }
    config_intrhook_disestablish(atapi_attach_hook); 
}

static int32_t
atapi_getparam(struct atapi_softc *atp)
{
    struct atapi_params	*atapi_parm;
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
atapi_queue_cmd(struct atapi_softc *atp, int8_t *ccb, void *data, 
		int32_t count, int32_t flags, int32_t timeout,
		atapi_callback_t callback, void *driver, struct buf *bp)
{
    struct atapi_request *request;
    int32_t error = 0; 
    int32_t s;
 
    atp->last_cmd = ccb[0];
    if (!(request = malloc(sizeof(struct atapi_request), M_DEVBUF, M_NOWAIT)))
        return -1;
    bzero(request, sizeof(struct atapi_request));
    request->device = atp;
    request->data = data;
    request->bytecount = count;
    request->donecount = 0;
    request->flags = flags;
    if (callback) {
    	request->callback = callback;
	request->bp = bp;
	request->driver = driver;
    }
    request->ccbsize = (atp->atapi_parm->cmdsize) ? 16 : 12;
    bcopy(ccb, request->ccb, request->ccbsize);

    s = splbio();

    /* link onto controller queue */
    if (ccb[0] == ATAPI_REQUEST_SENSE)
	TAILQ_INSERT_HEAD(&atp->controller->atapi_queue, request, chain);
    else
	TAILQ_INSERT_TAIL(&atp->controller->atapi_queue, request, chain);

    /* try to start controller */
    if (atp->controller->active == ATA_IDLE)
        ata_start(atp->controller);

    splx(s);

    /* wait for command to complete */
    if (tsleep((caddr_t)request, PRIBIO, "atprq", timeout*100)) {
        if (atp->controller->active != ATA_IDLE)
            atp->controller->active = ATA_IDLE;

	/* should we reset device here ? */
	request->result |= 0xf0;
    }

#ifdef ATAPI_DEBUG
    printf("atapi: phew, got back from tsleep with %s\n"
	   (request->result & 0xf0) == 0xf0 ? "timeout" : "wakeup"));
#endif

    if (callback) {
	(request->callback)(request);
    	free(request, M_DEVBUF);
	return 0;
    }
    error = request->result;
    free(request, M_DEVBUF);
    if (ccb[0] == ATAPI_REQUEST_SENSE)
	return 0;
    else
        return atapi_error(atp, error);
}
    
void
atapi_transfer(struct atapi_request *request)
{
    struct atapi_softc *atp;
    int32_t timeout;
    int8_t reason;

    /* get device params */
    atp = request->device;

#ifdef ATAPI_DEBUG
    printf("atapi: trying to start %s cmd\n", atapi_cmd2str(request->ccb[0]));
#endif
    /* if DMA enabled setup DMA hardware */
    atp->flags &= ~ATAPI_F_DMA_USED;
    if ((request->ccb[0]==ATAPI_READ_BIG || request->ccb[0]==ATAPI_WRITE_BIG) &&
        (atp->flags & ATAPI_F_DMA_ENABLED) &&
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

    /* command interrupt device ? just return */
    if (atp->atapi_parm->drqtype == ATAPI_DRQT_INTR)
	return;

    /* ready to write ATAPI command */
    timeout = 5000; /* might be less for fast devices */
    while (timeout--) {
	reason = inb(atp->controller->ioaddr + ATA_IREASON);
	atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS);
	if (((reason & (ATA_I_CMD | ATA_I_IN)) |
	     (atp->controller->status&(ATA_S_DRQ|ATA_S_BSY))) == ATAPI_P_CMDOUT)
	    break;
	DELAY(20);
    }
    if (timeout <= 0) {
	atp->controller->error = inb(atp->controller->ioaddr + ATA_ERROR);
	printf("atapi_transfer: bad command phase\n");
	/* now what ?? done & again ?? SOS */
    }

    /* this seems to be needed for some (slow) devices */
    DELAY(10);

    if (atp->flags & ATAPI_F_DMA_USED)
        ata_dmastart(atp->controller, atp->unit);

    /* send actual command */
    outsw(atp->controller->ioaddr + ATA_DATA, request->ccb, 
	  request->ccbsize / sizeof(int16_t));
}

int32_t
atapi_interrupt(struct atapi_request *request)
{
    struct atapi_softc *atp;
    int32_t length, reason, resid, dma_stat = 0;

#ifdef ATAPI_DEBUG
printf("atapi_interrupt: enter\n");
#endif
    /* get device params */
    atp = request->device;

    if (atp->flags & ATAPI_F_DMA_USED)
        dma_stat = ata_dmadone(atp->controller, atp->unit);

    /* get drive status */
    if (atapi_wait(atp, 0) < 0) {
        printf("atapi_interrupt: timeout waiting for status");
	request->result = inb(atp->controller->ioaddr + ATA_ERROR);
        wakeup((caddr_t)request);	
	return ATA_OP_FINISHED;
    }
    atp->controller->status = inb(atp->controller->ioaddr + ATA_STATUS); 
    atp->controller->error = inb(atp->controller->ioaddr + ATA_ERROR);

    length = inb(atp->controller->ioaddr + ATA_CYL_LSB);
    length |= inb(atp->controller->ioaddr + ATA_CYL_MSB) << 8;
    reason = (inb(atp->controller->ioaddr + ATA_IREASON)&(ATA_I_CMD|ATA_I_IN)) |
	     (atp->controller->status & ATA_S_DRQ);

#ifdef ATAPI_DEBUG
printf("atapi_interrupt: length=%d reason=0x%02x\n", length, reason);
#endif

    if (atp->flags & ATAPI_F_DMA_USED) {
        if (atp->controller->status & (ATA_S_ERROR | ATA_S_DWF) &&
	    dma_stat != ATA_BMSTAT_INTERRUPT)
            request->result = atp->controller->error;
	else {
	    request->donecount = request->bytecount;
            request->bytecount = 0;
            request->result = 0;
	}
        TAILQ_REMOVE(&atp->controller->atapi_queue, request, chain);
        atp->flags &= ~ATAPI_F_DMA_USED; 
        wakeup((caddr_t)request);
	return ATA_OP_FINISHED;
    }

    switch (reason) {
    
    case ATAPI_P_CMDOUT:
    	/* send ATAPI command */
	if (!(atp->controller->status & ATA_S_DRQ))
            printf("atapi_interrupt: command interrupt, but no DRQ\n");
	else
    	    outsw(atp->controller->ioaddr + ATA_DATA, request->ccb,
                  request->ccbsize / sizeof(int16_t));
	return ATA_OP_CONTINUES;
	
    case ATAPI_P_WRITE:
	if (request->flags & A_READ) {
	    printf("atapi_interrupt: trying to write on read buffer\n");
	    break;
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
            printf("atapi_interrupt: trying to read on write buffer\n");
            break;
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
	request->result = 0;
	if (atp->controller->status & (ATA_S_ERROR | ATA_S_DWF)) {
	    /* check sense !! SOS */
	    request->result = atp->controller->error;
	    break;
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
	printf("atapi_interrupt: unknown transfer phase %d\n", reason);
    }

    TAILQ_REMOVE(&atp->controller->atapi_queue, request, chain);

#ifdef ATAPI_DEBUG
printf("atapi_interrupt: error=0x%02x\n", request->result);
#endif

    wakeup((caddr_t)request);	
    return ATA_OP_FINISHED;
}

int32_t
atapi_error(struct atapi_softc *atp, int32_t error)
{
    struct atapi_reqsense sense;
    int8_t cmd = atp->last_cmd;
    int8_t ccb[16] = { ATAPI_REQUEST_SENSE, 0, 0, 0, sizeof(sense),
                       0, 0, 0 ,0 ,0, 0, 0, 0, 0, 0, 0 };

    switch ((error & 0xf0)) {
    case ATAPI_SK_RESERVED:
        printf("atapi_error: %s - timeout error = %02x\n", 
	       atapi_cmd2str(cmd), error & 0x0f);
	return EIO;

    case ATAPI_SK_NO_SENSE:
	if (error & 0x0f) {
            printf("atapi_error: %s - error = %02x\n",
		   atapi_cmd2str(cmd), error & 0x0f);
	    return EIO;
	}
	return 0;

    case ATAPI_SK_RECOVERED_ERROR:
        printf("atapi_error: %s - recovered error\n", atapi_cmd2str(cmd));
	return 0;

    case ATAPI_SK_NOT_READY:
	if (error & 0x0f)
	    break;
	return EBUSY;

    case ATAPI_SK_UNIT_ATTENTION:
	return EAGAIN;	/* misused */
    }

    bzero(&sense, sizeof(struct atapi_reqsense));
    atapi_queue_cmd(atp, ccb, &sense, sizeof(struct atapi_reqsense),
		    A_READ, 10, NULL, NULL, NULL);
    printf("atapi_error: %s - %s skey=%01x asc=%02x ascq=%02x error=%02x\n", 
	   atapi_cmd2str(cmd), atapi_skey2str(sense.sense_key),
	   sense.sense_key, sense.asc, sense.ascq, error & 0x0f);
    return EIO;
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
    case 0x01: return ("REZERO_UNIT/TAPE_REWIND");
    case 0x03: return ("REQUEST_SENSE");
    case 0x04: return ("FORMAT_UNIT");
    case 0x08: return ("TAPE_READ");
    case 0x0a: return ("TAPE_WRITE");
    case 0x10: return ("TAPE_WEOF");
    case 0x11: return ("TAPE_SPACE");
    case 0x19: return ("TAPE_ERASE");
    case 0x1a: return ("TAPE_MODE_SENSE");
    case 0x1b: return ("START_STOP/TAPE_LOAD");
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
    u_int8_t status;
    u_int32_t timeout = 0;
    
    while (timeout++ <= 500000) {        /* timeout 5 secs */
        status = inb(atp->controller->ioaddr + ATA_STATUS);

        /* if drive fails status, reselect the drive just to be sure */
        if (status == 0xff) {
            outb(atp->controller->ioaddr + ATA_DRIVE, ATA_D_IBM | atp->unit);
            DELAY(1);
            status = inb(atp->controller->ioaddr + ATA_STATUS);
        }
        if (!(status & ATA_S_BSY) && (status & ATA_S_DRDY))  
            break;            
        DELAY (10);        
    }    
    if (timeout <= 0)    
        return -1;          
    if (!mask)     
        return (status & ATA_S_ERROR);   
    
    /* Wait 50 msec for bits wanted. */    
    for (timeout=5000; timeout>0; --timeout) {    
        status = inb(atp->controller->ioaddr + ATA_STATUS);        
        if ((status & mask) == mask)        
            return (status & ATA_S_ERROR);            
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
#endif /* NATA */
