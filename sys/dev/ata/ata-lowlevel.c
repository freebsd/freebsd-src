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
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/ata.h>
#include <sys/conf.h>
#include <sys/ctype.h>
#include <sys/bus.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-commands.h>
#include <dev/ata/ata-pci.h>
#include <ata_if.h>

/* prototypes */
static int ata_begin_transaction(struct ata_request *);
static int ata_end_transaction(struct ata_request *);
static void ata_generic_reset(struct ata_channel *);
static int ata_wait(struct ata_channel *ch, struct ata_device *, u_int8_t);
static void ata_pio_read(struct ata_request *, int);
static void ata_pio_write(struct ata_request *, int);
static void bswap(int8_t *, int);
static void btrim(int8_t *, int);
static void bpack(int8_t *, int8_t *, int);

/* get device parameter page from device */
int
ata_getparam(device_t parent, struct ata_device *atadev, u_int8_t command)
{
    struct ata_channel *ch = device_get_softc(parent);
    int error = 0, retry = 0;

    do {
	if (retry++ > 4) {
	    if (bootverbose)
		printf("ata%d-%s: %s-identify retries exceeded\n", ch->unit,
		       atadev->unit == ATA_MASTER ? "master" : "slave",
		       command == ATA_ATAPI_IDENTIFY ? "ATAPI" : "ATA");
	    error = ENXIO;      
	    break;
	}

	/* select device */
	ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | atadev->unit);

	/* wait a bit to let slow devices settle */
	DELAY(100);

	/* disable interrupt */
	ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_4BIT | ATA_A_IDS);

	/* ready to issue command ? */
	if ((error = ata_wait(ch, atadev, 0)) < 0) { 
	    printf("ata%d-%s: timeout sending %s-identify error=%d\n",
		   device_get_unit(parent), 
		   atadev->unit == ATA_MASTER ? "master" : "slave",
		   command == ATA_ATAPI_IDENTIFY ? "ATAPI" : "ATA", error);
	    error = ENXIO;      
	    break;
	}

	/* select device */
	ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | atadev->unit);

	/* issue command */
	ATA_IDX_OUTB(ch, ATA_COMMAND, command);

    } while (ata_wait(ch, atadev, ATA_S_DRQ));

    if (!error) {
	ATA_IDX_INSW_STRM(ch, ATA_DATA, (void *)&atadev->param,
			  sizeof(struct ata_params)/sizeof(int16_t));
	ATA_IDX_INB(ch, ATA_STATUS);
    }

    if (!error && (isprint(atadev->param.model[0]) ||
		   isprint(atadev->param.model[1]))) {
	struct ata_params *atacap = &atadev->param;
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
	    printf("ata%d-%s: pio=%s wdma=%s udma=%s cable=%s wire\n",
		   ch->unit, atadev->unit == ATA_MASTER ? "master":"slave",
		   ata_mode2str(ata_pmode(atacap)),
		   ata_mode2str(ata_wmode(atacap)),
		   ata_mode2str(ata_umode(atacap)),
		   (atacap->hwres & ATA_CABLE_ID) ? "80":"40");
    }
    else {
	if (!error)
	    error = ENXIO;
    }

    return error;
}

/*
 * low level ATA functions 
 */
void
ata_generic_hw(struct ata_channel *ch)
{
    ch->hw.begin_transaction = ata_begin_transaction;
    ch->hw.end_transaction = ata_end_transaction;
    ch->hw.reset = ata_generic_reset;
    ch->hw.command = ata_generic_command;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_begin_transaction(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);

    ATA_DEBUG_RQ(request, "begin transaction");

    /* disable ATAPI DMA writes if HW doesn't support it */
    if ((ch->flags & ATA_ATAPI_DMA_RO) &&
	((request->flags & (ATA_R_ATAPI | ATA_R_DMA | ATA_R_WRITE)) ==
	 (ATA_R_ATAPI | ATA_R_DMA | ATA_R_WRITE)))
	request->flags &= ~ATA_R_DMA;

    switch (request->flags & (ATA_R_ATAPI | ATA_R_DMA)) {

    /* ATA PIO data transfer and control commands */
    default:
	{
	/* record command direction here as our request might be gone later */
	int write = (request->flags & ATA_R_WRITE);

	    /* issue command */
	    if (ch->hw.command(atadev, request->u.ata.command,
			       request->u.ata.lba, request->u.ata.count,
			       request->u.ata.feature)) {
		device_printf(request->dev, "error issueing %s command\n",
			   ata_cmd2str(request));
		request->result = EIO;
		goto begin_finished;
	    }

	    /* device reset doesn't interrupt */
	    if (request->u.ata.command == ATA_DEVICE_RESET) {
		int timeout = 1000000;
		do {
		    DELAY(10);
		    request->status = ATA_IDX_INB(ch, ATA_STATUS);
		} while (request->status & ATA_S_BUSY && timeout--);
		if (request->status & ATA_S_ERROR)
		    request->error = ATA_IDX_INB(ch, ATA_ERROR);
		goto begin_finished;
	    }

	    /* if write command output the data */
	    if (write) {
		if (ata_wait(ch, atadev,
			     (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) < 0) {
		    device_printf(request->dev,"timeout waiting for write DRQ");
		    request->result = EIO;
		    goto begin_finished;
		}
		ata_pio_write(request, request->transfersize);
	    }
	}
	goto begin_continue;

    /* ATA DMA data transfer commands */
    case ATA_R_DMA:
	/* check sanity, setup SG list and DMA engine */
	if (ch->dma->load(atadev, request->data, request->bytecount,
			  request->flags & ATA_R_READ)) {
	    device_printf(request->dev, "setting up DMA failed\n");
	    request->result = EIO;
	    goto begin_finished;
	}

	/* issue command */
	if (ch->hw.command(atadev, request->u.ata.command,
			   request->u.ata.lba, request->u.ata.count,
			   request->u.ata.feature)) {
	    device_printf(request->dev, "error issueing %s command\n",
		       ata_cmd2str(request));
	    request->result = EIO;
	    goto begin_finished;
	}

	/* start DMA engine */
	if (ch->dma->start(ch)) {
	    device_printf(request->dev, "error starting DMA\n");
	    request->result = EIO;
	    goto begin_finished;
	}
	goto begin_continue;

    /* ATAPI PIO commands */
    case ATA_R_ATAPI:
	/* is this just a POLL DSC command ? */
	if (request->u.atapi.ccb[0] == ATAPI_POLL_DSC) {
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | atadev->unit);
	    DELAY(10);
	    if (!(ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_DSC))
		request->result = EBUSY;
	    goto begin_finished;
	}

	/* start ATAPI operation */
	if (ch->hw.command(atadev, ATA_PACKET_CMD,
			   request->transfersize << 8, 0, 0)) {
	    device_printf(request->dev, "error issuing ATA PACKET command\n");
	    request->result = EIO;
	    goto begin_finished;
	}

	/* command interrupt device ? just return and wait for interrupt */
	if ((atadev->param.config & ATA_DRQ_MASK) == ATA_DRQ_INTR)
	    goto begin_continue;

	/* wait for ready to write ATAPI command block */
	{
	    int timeout = 5000; /* might be less for fast devices */
	    while (timeout--) {
		int reason = ATA_IDX_INB(ch, ATA_IREASON);
		int status = ATA_IDX_INB(ch, ATA_STATUS);

		if (((reason & (ATA_I_CMD | ATA_I_IN)) |
		     (status & (ATA_S_DRQ | ATA_S_BUSY))) == ATAPI_P_CMDOUT)
		    break;
		DELAY(20);
	    }
	    if (timeout <= 0) {
		device_printf(request->dev,"timeout waiting for ATAPI ready\n");
		request->result = EIO;
		goto begin_finished;
	    }
	}

	/* this seems to be needed for some (slow) devices */
	DELAY(10);

	/* output actual command block */
	ATA_IDX_OUTSW_STRM(ch, ATA_DATA, 
			   (int16_t *)request->u.atapi.ccb,
			   (atadev->param.config & ATA_PROTO_MASK) ==
			   ATA_PROTO_ATAPI_12 ? 6 : 8);
	goto begin_continue;

    case ATA_R_ATAPI|ATA_R_DMA:
	/* is this just a POLL DSC command ? */
	if (request->u.atapi.ccb[0] == ATAPI_POLL_DSC) {
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | atadev->unit);
	    DELAY(10);
	    if (!(ATA_IDX_INB(ch, ATA_ALTSTAT) & ATA_S_DSC))
		request->result = EBUSY;
	    goto begin_finished;
	}

	/* check sanity, setup SG list and DMA engine */
	if (ch->dma->load(atadev, request->data, request->bytecount,
			  request->flags & ATA_R_READ)) {
	    device_printf(request->dev, "setting up DMA failed\n");
	    request->result = EIO;
	    goto begin_finished;
	}

	/* start ATAPI operation */
	if (ch->hw.command(atadev, ATA_PACKET_CMD, 0, 0, ATA_F_DMA)) {
	    device_printf(request->dev, "error issuing ATAPI packet command\n");
	    request->result = EIO;
	    goto begin_finished;
	}

	/* wait for ready to write ATAPI command block */
	{
	    int timeout = 5000; /* might be less for fast devices */
	    while (timeout--) {
		int reason = ATA_IDX_INB(ch, ATA_IREASON);
		int status = ATA_IDX_INB(ch, ATA_STATUS);

		if (((reason & (ATA_I_CMD | ATA_I_IN)) |
		     (status & (ATA_S_DRQ | ATA_S_BUSY))) == ATAPI_P_CMDOUT)
		    break;
		DELAY(20);
	    }
	    if (timeout <= 0) {
		device_printf(request->dev,"timeout waiting for ATAPI ready\n");
		request->result = EIO;
		goto begin_finished;
	    }
	}

	/* this seems to be needed for some (slow) devices */
	DELAY(10);

	/* output actual command block */
	ATA_IDX_OUTSW_STRM(ch, ATA_DATA, 
			   (int16_t *)request->u.atapi.ccb,
			   (atadev->param.config & ATA_PROTO_MASK) ==
			   ATA_PROTO_ATAPI_12 ? 6 : 8);

	/* start DMA engine */
	if (ch->dma->start(ch)) {
	    request->result = EIO;
	    goto begin_finished;
	}
	goto begin_continue;
    }
    /* NOT REACHED */
    printf("ata_begin_transaction OOPS!!!\n");

begin_finished:
    if (ch->dma && ch->dma->flags & ATA_DMA_LOADED)
	ch->dma->unload(ch);
    return ATA_OP_FINISHED;

begin_continue:
    callout_reset(&request->callout, request->timeout * hz,
		  (timeout_t*)ata_timeout, request);
    return ATA_OP_CONTINUES;
}

/* must be called with ATA channel locked and state_mtx held */
static int
ata_end_transaction(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    struct ata_device *atadev = device_get_softc(request->dev);
    int length;

    ATA_DEBUG_RQ(request, "end transaction");

    /* clear interrupt and get status */
    request->status = ATA_IDX_INB(ch, ATA_STATUS);

    switch (request->flags & (ATA_R_ATAPI | ATA_R_DMA | ATA_R_CONTROL)) {

    /* ATA PIO data transfer and control commands */
    default:

	/* on timeouts we have no data or anything so just return */
	if (request->flags & ATA_R_TIMEOUT)
	    goto end_finished;

	/* on control commands read back registers to the request struct */
	if (request->flags & ATA_R_CONTROL) {
	    if (ch->flags & ATA_48BIT_ACTIVE) {
		ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_4BIT | ATA_A_HOB);
		request->u.ata.count = (ATA_IDX_INB(ch, ATA_COUNT) << 8);
		request->u.ata.lba =
		    ((u_int64_t)(ATA_IDX_INB(ch, ATA_SECTOR)) << 24) |
		    ((u_int64_t)(ATA_IDX_INB(ch, ATA_CYL_LSB)) << 32) |
		    ((u_int64_t)(ATA_IDX_INB(ch, ATA_CYL_MSB)) << 40);

		ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_4BIT);
		request->u.ata.count |= ATA_IDX_INB(ch, ATA_COUNT);
		request->u.ata.lba |= 
		    (ATA_IDX_INB(ch, ATA_SECTOR) |
		     (ATA_IDX_INB(ch, ATA_CYL_LSB) << 8) |
		     (ATA_IDX_INB(ch, ATA_CYL_MSB) << 16));
	    }
	    else {
	        request->u.ata.count = ATA_IDX_INB(ch, ATA_COUNT);
	        request->u.ata.lba = ATA_IDX_INB(ch, ATA_SECTOR) |
				     (ATA_IDX_INB(ch, ATA_CYL_LSB) << 8) |
				     (ATA_IDX_INB(ch, ATA_CYL_MSB) << 16) |
				     ((ATA_IDX_INB(ch, ATA_DRIVE) & 0xf) << 24);
	    }
	}

	/* if we got an error we are done with the HW */
	if (request->status & ATA_S_ERROR) {
	    request->error = ATA_IDX_INB(ch, ATA_ERROR);
	    goto end_finished;
	}
	
	/* are we moving data ? */
	if (request->flags & (ATA_R_READ | ATA_R_WRITE)) {

	    /* if read data get it */
	    if (request->flags & ATA_R_READ) {
		if (ata_wait(ch, atadev,
			     (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) < 0) {
		    device_printf(request->dev, "timeout waiting for read DRQ");
		    request->result = EIO;
		    goto end_finished;
		}
		ata_pio_read(request, request->transfersize);
	    }

	    /* update how far we've gotten */
	    request->donecount += request->transfersize;

	    /* do we need a scoop more ? */
	    if (request->bytecount > request->donecount) {

		/* set this transfer size according to HW capabilities */
		request->transfersize = 
		    min((request->bytecount - request->donecount),
			request->transfersize);

		/* if data write command, output the data */
		if (request->flags & ATA_R_WRITE) {

		    /* if we get an error here we are done with the HW */
		    if (ata_wait(ch, atadev,
				 (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) < 0) {
			device_printf(request->dev,
				      "timeout waiting for write DRQ");
			request->status = ATA_IDX_INB(ch, ATA_STATUS);
			goto end_finished;
		    }

		    /* output data and return waiting for new interrupt */
		    ata_pio_write(request, request->transfersize);
		    goto end_continue;
		}

		/* if data read command, return & wait for interrupt */
		if (request->flags & ATA_R_READ)
		    goto end_continue;
	    }
	}
	/* done with HW */
	goto end_finished;

    /* ATA DMA data transfer commands */
    case ATA_R_DMA:

	/* stop DMA engine and get status */
	if (ch->dma->stop)
	    request->dmastat = ch->dma->stop(ch);

	/* did we get error or data */
	if (request->status & ATA_S_ERROR)
	    request->error = ATA_IDX_INB(ch, ATA_ERROR);
	else if (request->dmastat & ATA_BMSTAT_ERROR)
	    request->status |= ATA_S_ERROR;
	else if (!(request->flags & ATA_R_TIMEOUT))
	    request->donecount = request->bytecount;

	/* release SG list etc */
	ch->dma->unload(ch);

	/* done with HW */
	goto end_finished;

    /* ATAPI PIO commands */
    case ATA_R_ATAPI:
	length = ATA_IDX_INB(ch, ATA_CYL_LSB)|(ATA_IDX_INB(ch, ATA_CYL_MSB)<<8);

	/* on timeouts we have no data or anything so just return */
	if (request->flags & ATA_R_TIMEOUT)
	    goto end_finished;

	switch ((ATA_IDX_INB(ch, ATA_IREASON) & (ATA_I_CMD | ATA_I_IN)) |
		(request->status & ATA_S_DRQ)) {

	case ATAPI_P_CMDOUT:
	    /* this seems to be needed for some (slow) devices */
	    DELAY(10);

	    if (!(request->status & ATA_S_DRQ)) {
		device_printf(request->dev, "command interrupt without DRQ\n");
		request->status = ATA_S_ERROR;
		goto end_finished;
	    }
	    ATA_IDX_OUTSW_STRM(ch, ATA_DATA, (int16_t *)request->u.atapi.ccb,
			       (atadev->param.config &
				ATA_PROTO_MASK)== ATA_PROTO_ATAPI_12 ? 6 : 8);
	    /* return wait for interrupt */
	    goto end_continue;

	case ATAPI_P_WRITE:
	    if (request->flags & ATA_R_READ) {
		request->status = ATA_S_ERROR;
		device_printf(request->dev,
			      "%s trying to write on read buffer\n",
			   ata_cmd2str(request));
		goto end_finished;
		break;
	    }
	    ata_pio_write(request, length);
	    request->donecount += length;

	    /* set next transfer size according to HW capabilities */
	    request->transfersize = min((request->bytecount-request->donecount),
					request->transfersize);
	    /* return wait for interrupt */
	    goto end_continue;

	case ATAPI_P_READ:
	    if (request->flags & ATA_R_WRITE) {
		request->status = ATA_S_ERROR;
		device_printf(request->dev,
			      "%s trying to read on write buffer\n",
			   ata_cmd2str(request));
		goto end_finished;
	    }
	    ata_pio_read(request, length);
	    request->donecount += length;

	    /* set next transfer size according to HW capabilities */
	    request->transfersize = min((request->bytecount-request->donecount),
					request->transfersize);
	    /* return wait for interrupt */
	    goto end_continue;

	case ATAPI_P_DONEDRQ:
	    device_printf(request->dev,
			  "WARNING - %s DONEDRQ non conformant device\n",
			  ata_cmd2str(request));
	    if (request->flags & ATA_R_READ) {
		ata_pio_read(request, length);
		request->donecount += length;
	    }
	    else if (request->flags & ATA_R_WRITE) {
		ata_pio_write(request, length);
		request->donecount += length;
	    }
	    else
		request->status = ATA_S_ERROR;
	    /* FALLTHROUGH */

	case ATAPI_P_ABORT:
	case ATAPI_P_DONE:
	    if (request->status & (ATA_S_ERROR | ATA_S_DWF))
		request->error = ATA_IDX_INB(ch, ATA_ERROR);
	    goto end_finished;

	default:
	    device_printf(request->dev, "unknown transfer phase\n");
	    request->status = ATA_S_ERROR;
	}

	/* done with HW */
	goto end_finished;

    /* ATAPI DMA commands */
    case ATA_R_ATAPI|ATA_R_DMA:

	/* stop the engine and get engine status */
	if (ch->dma->stop)
	    request->dmastat = ch->dma->stop(ch);

	/* did we get error or data */
	if (request->status & (ATA_S_ERROR | ATA_S_DWF))
	    request->error = ATA_IDX_INB(ch, ATA_ERROR);
	else if (request->dmastat & ATA_BMSTAT_ERROR)
	    request->status |= ATA_S_ERROR;
	else if (!(request->flags & ATA_R_TIMEOUT))
	    request->donecount = request->bytecount;
 
	/* release SG list etc */
	ch->dma->unload(ch);

	/* done with HW */
	goto end_finished;
    }
    /* NOT REACHED */
    printf("ata_end_transaction OOPS!!\n");

end_finished:
    callout_stop(&request->callout);
    return ATA_OP_FINISHED;

end_continue:
    return ATA_OP_CONTINUES;
}

/* must be called with ATA channel locked */
static void
ata_generic_reset(struct ata_channel *ch)
{
    u_int8_t ostat0 = 0, stat0 = 0, ostat1 = 0, stat1 = 0;
    u_int8_t err = 0, lsb = 0, msb = 0;
    int mask = 0, timeout;

    /* reset controller (host) */
    ATA_RESET(ch->dev);

    /* do we have any signs of ATA/ATAPI HW being present ? */
    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | ATA_MASTER);
    DELAY(10);
    ostat0 = ATA_IDX_INB(ch, ATA_STATUS);
    if ((ostat0 & 0xf8) != 0xf8 && ostat0 != 0xa5) {
	stat0 = ATA_S_BUSY;
	mask |= 0x01;
    }

    /* in some setups we dont want to test for a slave */
    if (!(ch->flags & ATA_NO_SLAVE)) {
	ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | ATA_SLAVE);
	DELAY(10);      
	ostat1 = ATA_IDX_INB(ch, ATA_STATUS);
	if ((ostat1 & 0xf8) != 0xf8 && ostat1 != 0xa5) {
	    stat1 = ATA_S_BUSY;
	    mask |= 0x02;
	}
    }

    if (bootverbose)
	device_printf(ch->dev, "reset tp1 mask=%02x ostat0=%02x ostat1=%02x\n",
		      mask, ostat0, ostat1);

    /* if nothing showed up there is no need to get any further */
    /* XXX SOS is that too strong?, we just might loose devices here */
    ch->devices = 0;
    if (!mask)
	return;

    /* reset (both) devices on this channel */
    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | ATA_MASTER);
    DELAY(10);
    ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_IDS | ATA_A_RESET);
    ata_udelay(10000); 
    ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_IDS);
    ata_udelay(100000);
    ATA_IDX_INB(ch, ATA_ERROR);

    /* wait for BUSY to go inactive */
    for (timeout = 0; timeout < 310; timeout++) {
	if ((mask & 0x01) && (stat0 & ATA_S_BUSY)) {
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
	    DELAY(10);
	    err = ATA_IDX_INB(ch, ATA_ERROR);
	    lsb = ATA_IDX_INB(ch, ATA_CYL_LSB);
	    msb = ATA_IDX_INB(ch, ATA_CYL_MSB);
	    stat0 = ATA_IDX_INB(ch, ATA_STATUS);
	    if (bootverbose)
		device_printf(ch->dev,
			      "stat0=0x%02x err=0x%02x lsb=0x%02x msb=0x%02x\n",
			      stat0, err, lsb, msb);
	    if (stat0 == err && lsb == err && msb == err &&
		timeout > (stat0 & ATA_S_BUSY ? 100 : 10))
		mask &= ~0x01;
	    if (!(stat0 & ATA_S_BUSY)) {
		if ((err & 0x7f) == ATA_E_ILI) {
		    if (lsb == ATAPI_MAGIC_LSB && msb == ATAPI_MAGIC_MSB) {
			ch->devices |= ATA_ATAPI_MASTER;
		    }
		    else if (stat0 & ATA_S_READY) {
			ch->devices |= ATA_ATA_MASTER;
		    }
		}
		else if ((stat0 & 0x0f) && err == lsb && err == msb) {
		    stat0 |= ATA_S_BUSY;
		}
	    }
	}

	if ((mask & 0x02) && (stat1 & ATA_S_BUSY) &&
	    !((mask & 0x01) && (stat0 & ATA_S_BUSY))) {
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_SLAVE);
	    DELAY(10);
	    err = ATA_IDX_INB(ch, ATA_ERROR);
	    lsb = ATA_IDX_INB(ch, ATA_CYL_LSB);
	    msb = ATA_IDX_INB(ch, ATA_CYL_MSB);
	    stat1 = ATA_IDX_INB(ch, ATA_STATUS);
	    if (bootverbose)
		device_printf(ch->dev,
			      "stat1=0x%02x err=0x%02x lsb=0x%02x msb=0x%02x\n",
			      stat1, err, lsb, msb);
	    if (stat1 == err && lsb == err && msb == err &&
		timeout > (stat1 & ATA_S_BUSY ? 100 : 10))
		mask &= ~0x02;
	    if (!(stat1 & ATA_S_BUSY)) {
		if ((err & 0x7f) == ATA_E_ILI) {
		    if (lsb == ATAPI_MAGIC_LSB && msb == ATAPI_MAGIC_MSB) {
			ch->devices |= ATA_ATAPI_SLAVE;
		    }
		    else if (stat1 & ATA_S_READY) {
			ch->devices |= ATA_ATA_SLAVE;
		    }
		}
		else if ((stat1 & 0x0f) && err == lsb && err == msb) {
		    stat1 |= ATA_S_BUSY;
		}
	    }
	}

	if (mask == 0x00)       /* nothing to wait for */
	    break;
	if (mask == 0x01)       /* wait for master only */
	    if (!(stat0 & ATA_S_BUSY) || (stat0 == 0xff && timeout > 10))
		break;
	if (mask == 0x02)       /* wait for slave only */
	    if (!(stat1 & ATA_S_BUSY) || (stat1 == 0xff && timeout > 10))
		break;
	if (mask == 0x03) {     /* wait for both master & slave */
	    if (!(stat0 & ATA_S_BUSY) && !(stat1 & ATA_S_BUSY))
		break;
	    if ((stat0 == 0xff) && (timeout > 20))
		mask &= ~0x01;
	    if ((stat1 == 0xff) && (timeout > 20))
		mask &= ~0x02;
	}
	ata_udelay(100000);
    }

    if (bootverbose)
	device_printf(ch->dev, "reset tp2 stat0=%02x stat1=%02x devices=0x%b\n",
		      stat0, stat1, ch->devices,
		      "\20\4ATAPI_SLAVE\3ATAPI_MASTER\2ATA_SLAVE\1ATA_MASTER");
}

static int
ata_wait(struct ata_channel *ch, struct ata_device *atadev, u_int8_t mask)
{
    u_int8_t status;
    int timeout = 0;
    
    DELAY(1);

    /* wait at max 1 second for device to get !BUSY */ 
    while (timeout < 1000000) {
	status = ATA_IDX_INB(ch, ATA_ALTSTAT);

	/* if drive fails status, reselect the drive and try again */
	if (status == 0xff) {
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | atadev->unit);
	    timeout += 1000;
	    DELAY(1000);
	    continue;
	}

	/* are we done ? */
	if (!(status & ATA_S_BUSY))
	    break;            

	if (timeout > 1000) {
	    timeout += 1000;
	    DELAY(1000);
	}
	else {
	    timeout += 10;
	    DELAY(10);
	}
    }    
    if (timeout >= 1000000)      
	return -2;          
    if (!mask)     
	return (status & ATA_S_ERROR);   

    DELAY(1);
    
    /* wait 50 msec for bits wanted */     
    timeout = 5000;
    while (timeout--) {   
	status = ATA_IDX_INB(ch, ATA_ALTSTAT);
	if ((status & mask) == mask) 
	    return (status & ATA_S_ERROR);            
	DELAY(10);         
    }     
    return -3;      
}   

int
ata_generic_command(struct ata_device *atadev, u_int8_t command,
		    u_int64_t lba, u_int16_t count, u_int16_t feature)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(atadev->dev));

    /* select device */
    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | atadev->unit);

    /* ready to issue command ? */
    if (ata_wait(ch, atadev, 0) < 0) { 
	device_printf(atadev->dev, "timeout sending command=%02x\n", command);
	return -1;
    }

    /* enable interrupt */
    ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_4BIT);

    /* only use 48bit addressing if needed (avoid bugs and overhead) */
    if ((lba >= ATA_MAX_28BIT_LBA || count > 256) &&
	atadev->param.support.command2 & ATA_SUPPORT_ADDRESS48) {

	/* translate command into 48bit version */
	switch (command) {
	case ATA_READ:
	    command = ATA_READ48; break;
	case ATA_READ_MUL:
	    command = ATA_READ_MUL48; break;
	case ATA_READ_DMA:
	    command = ATA_READ_DMA48; break;
	case ATA_READ_DMA_QUEUED:
	    command = ATA_READ_DMA_QUEUED48; break;
	case ATA_WRITE:
	    command = ATA_WRITE48; break;
	case ATA_WRITE_MUL:
	    command = ATA_WRITE_MUL48; break;
	case ATA_WRITE_DMA:
	    command = ATA_WRITE_DMA48; break;
	case ATA_WRITE_DMA_QUEUED:
	    command = ATA_WRITE_DMA_QUEUED48; break;
	case ATA_FLUSHCACHE:
	    command = ATA_FLUSHCACHE48; break;
	default:
	    device_printf(atadev->dev,"can't translate cmd to 48bit version\n");
	    return -1;
	}
	ATA_IDX_OUTB(ch, ATA_FEATURE, (feature>>8) & 0xff);
	ATA_IDX_OUTB(ch, ATA_FEATURE, feature & 0xff);
	ATA_IDX_OUTB(ch, ATA_COUNT, (count>>8) & 0xff);
	ATA_IDX_OUTB(ch, ATA_COUNT, count & 0xff);
	ATA_IDX_OUTB(ch, ATA_SECTOR, (lba>>24) & 0xff);
	ATA_IDX_OUTB(ch, ATA_SECTOR, lba & 0xff);
	ATA_IDX_OUTB(ch, ATA_CYL_LSB, (lba>>32) & 0xff);
	ATA_IDX_OUTB(ch, ATA_CYL_LSB, (lba>>8) & 0xff);
	ATA_IDX_OUTB(ch, ATA_CYL_MSB, (lba>>40) & 0xff);
	ATA_IDX_OUTB(ch, ATA_CYL_MSB, (lba>>16) & 0xff);
	ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_LBA | atadev->unit);
	ch->flags |= ATA_48BIT_ACTIVE;
    }
    else {
	ATA_IDX_OUTB(ch, ATA_FEATURE, feature);
	ATA_IDX_OUTB(ch, ATA_COUNT, count);
	if (atadev->flags & ATA_D_USE_CHS) {
	    int heads, sectors;

	    if (atadev->param.atavalid & ATA_FLAG_54_58) {
		heads = atadev->param.current_heads;
		sectors = atadev->param.current_sectors;
	    }
	    else {
		heads = atadev->param.heads;
		sectors = atadev->param.sectors;
	    }
	    ATA_IDX_OUTB(ch, ATA_SECTOR, (lba % sectors) + 1);
	    ATA_IDX_OUTB(ch, ATA_CYL_LSB, (lba / (sectors * heads)));
	    ATA_IDX_OUTB(ch, ATA_CYL_MSB, (lba / (sectors * heads)) >> 8);
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | atadev->unit | 
			 (((lba % (sectors * heads)) / sectors) & 0xf));
	}
	else {
	    ATA_IDX_OUTB(ch, ATA_SECTOR, lba & 0xff);
	    ATA_IDX_OUTB(ch, ATA_CYL_LSB, (lba>>8) & 0xff);
	    ATA_IDX_OUTB(ch, ATA_CYL_MSB, (lba>>16) & 0xff);
	    ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | atadev->unit |
					((lba>>24) & 0x0f));
	}
	ch->flags &= ~ATA_48BIT_ACTIVE;
    }

    /* issue command to controller */
    ATA_IDX_OUTB(ch, ATA_COMMAND, command);

    return 0;
}

static void
ata_pio_read(struct ata_request *request, int length)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    int size = min(request->transfersize, length);
    int resid;

    if (ch->flags & ATA_USE_16BIT || (size % sizeof(int32_t)))
	ATA_IDX_INSW_STRM(ch, ATA_DATA,
			  (void*)((uintptr_t)request->data+request->donecount),
			  size / sizeof(int16_t));
    else
	ATA_IDX_INSL_STRM(ch, ATA_DATA,
			  (void*)((uintptr_t)request->data+request->donecount),
			  size / sizeof(int32_t));

    if (request->transfersize < length) {
	device_printf(request->dev, "WARNING - %s read data overrun %d>%d\n",
		   ata_cmd2str(request), length, request->transfersize);
	for (resid = request->transfersize; resid < length;
	     resid += sizeof(int16_t))
	    ATA_IDX_INW(ch, ATA_DATA);
    }
}

static void
ata_pio_write(struct ata_request *request, int length)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(request->dev));
    int size = min(request->transfersize, length);
    int resid;

    if (ch->flags & ATA_USE_16BIT || (size % sizeof(int32_t)))
	ATA_IDX_OUTSW_STRM(ch, ATA_DATA,
			   (void*)((uintptr_t)request->data+request->donecount),
			   size / sizeof(int16_t));
    else
	ATA_IDX_OUTSL_STRM(ch, ATA_DATA,
			   (void*)((uintptr_t)request->data+request->donecount),
			   size / sizeof(int32_t));

    if (request->transfersize < length) {
	device_printf(request->dev, "WARNING - %s write data underrun %d>%d\n",
		   ata_cmd2str(request), length, request->transfersize);
	for (resid = request->transfersize; resid < length;
	     resid += sizeof(int16_t))
	    ATA_IDX_OUTW(ch, ATA_DATA, 0);
    }
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
