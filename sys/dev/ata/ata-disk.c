/*-
 * Copyright (c) 1998,1999,2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
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
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/cons.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>

/* device structures */
static d_open_t		adopen;
static d_close_t	adclose;
static d_strategy_t	adstrategy;
static d_dump_t		addump;
static struct cdevsw ad_cdevsw = {
	/* open */	adopen,
	/* close */	adclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	adstrategy,
	/* name */	"ad",
	/* maj */	116,
	/* dump */	addump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
	/* bmaj */	30
};
static struct cdevsw addisk_cdevsw;

/* prototypes */
static void ad_invalidatequeue(struct ad_softc *, struct ad_request *);
static int ad_tagsupported(struct ad_softc *);
static void ad_timeout(struct ad_request *);
static void ad_free(struct ad_request *);
static int ad_version(u_int16_t);

/* misc defines */
#define AD_MAX_RETRIES	3

/* internal vars */
static u_int32_t adp_lun_map = 0;
static int ata_dma = 1;
static int ata_wc = 1;
static int ata_tags = 0; 
TUNABLE_INT("hw.ata.ata_dma", &ata_dma);
TUNABLE_INT("hw.ata.wc", &ata_wc);
TUNABLE_INT("hw.ata.tags", &ata_tags);
static MALLOC_DEFINE(M_AD, "AD driver", "ATA disk driver");

/* sysctl vars */
SYSCTL_DECL(_hw_ata);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma, CTLFLAG_RD, &ata_dma, 0,
	   "ATA disk DMA mode control");
SYSCTL_INT(_hw_ata, OID_AUTO, wc, CTLFLAG_RD, &ata_wc, 0,
	   "ATA disk write caching");
SYSCTL_INT(_hw_ata, OID_AUTO, tags, CTLFLAG_RD, &ata_tags, 0,
	   "ATA disk tagged queuing support");

void
ad_attach(struct ata_device *atadev)
{
    struct ad_softc *adp;
    dev_t dev;

    if (!(adp = malloc(sizeof(struct ad_softc), M_AD, M_NOWAIT | M_ZERO))) {
	ata_prtdev(atadev, "failed to allocate driver storage\n");
	return;
    }
    adp->device = atadev;
#ifdef ATA_STATIC_ID
    adp->lun = (device_get_unit(atadev->channel->dev)<<1)+ATA_DEV(atadev->unit);
#else
    adp->lun = ata_get_lun(&adp_lun_map);
#endif
    ata_set_name(atadev, "ad", adp->lun);
    adp->heads = atadev->param->heads;
    adp->sectors = atadev->param->sectors;
    adp->total_secs = atadev->param->cylinders * adp->heads * adp->sectors;	
    bufq_init(&adp->queue);

    /* does this device need oldstyle CHS addressing */
    if (!ad_version(atadev->param->version_major) || 
	!(atadev->param->atavalid & ATA_FLAG_54_58) || !atadev->param->lba_size)
	adp->flags |= AD_F_CHS_USED;

    /* use the 28bit LBA size if valid */
    if (atadev->param->cylinders == 16383 &&
	adp->total_secs < atadev->param->lba_size)
	adp->total_secs = atadev->param->lba_size;

    /* use the 48bit LBA size if valid */
    if (atadev->param->support.address48 &&
	atadev->param->lba_size48 > 268435455)
	adp->total_secs = atadev->param->lba_size48;
    
    ATA_SLEEPLOCK_CH(atadev->channel, ATA_CONTROL);
    /* use multiple sectors/interrupt if device supports it */
    adp->transfersize = DEV_BSIZE;
    if (ad_version(atadev->param->version_major)) {
	int secsperint = max(1, min(atadev->param->sectors_intr, 16));

	if (!ata_command(atadev, ATA_C_SET_MULTI, 0, secsperint,
			 0, ATA_WAIT_INTR) && !ata_wait(atadev, 0))
	adp->transfersize *= secsperint;
    }

    /* enable read caching if not default on device */
    if (ata_command(atadev, ATA_C_SETFEATURES,
		    0, 0, ATA_C_F_ENAB_RCACHE, ATA_WAIT_INTR))
	ata_prtdev(atadev, "enabling readahead cache failed\n");

    /* enable write caching if allowed and not default on device */
    if (ata_wc || (ata_tags && ad_tagsupported(adp))) {
	if (ata_command(atadev, ATA_C_SETFEATURES,
			0, 0, ATA_C_F_ENAB_WCACHE, ATA_WAIT_INTR))
	    ata_prtdev(atadev, "enabling write cache failed\n");
    }
    else {
	if (ata_command(atadev, ATA_C_SETFEATURES,
			0, 0, ATA_C_F_DIS_WCACHE, ATA_WAIT_INTR))
	    ata_prtdev(atadev, "disabling write cache failed\n");
    }

    /* use DMA if allowed and if drive/controller supports it */
    if (ata_dma)
	ata_dmainit(atadev, ata_pmode(atadev->param),
		    ata_wmode(atadev->param), ata_umode(atadev->param));
    else
	ata_dmainit(atadev, ata_pmode(atadev->param), -1, -1);

    /* use tagged queueing if allowed and supported */
    if (ata_tags && ad_tagsupported(adp)) {
	adp->num_tags = atadev->param->queuelen;
	adp->flags |= AD_F_TAG_ENABLED;
	adp->device->channel->flags |= ATA_QUEUED;
	if (ata_command(atadev, ATA_C_SETFEATURES,
			0, 0, ATA_C_F_DIS_RELIRQ, ATA_WAIT_INTR))
	    ata_prtdev(atadev, "disabling release interrupt failed\n");
	if (ata_command(atadev, ATA_C_SETFEATURES,
			0, 0, ATA_C_F_DIS_SRVIRQ, ATA_WAIT_INTR))
	    ata_prtdev(atadev, "disabling service interrupt failed\n");
    }

    ATA_UNLOCK_CH(atadev->channel);

    devstat_add_entry(&adp->stats, "ad", adp->lun, DEV_BSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
		      DEVSTAT_PRIORITY_DISK);

    dev = disk_create(adp->lun, &adp->disk, 0, &ad_cdevsw, &addisk_cdevsw);
    dev->si_drv1 = adp;
    dev->si_iosize_max = 256 * DEV_BSIZE;
    adp->dev = dev;

    /* construct the disklabel */
    bzero(&adp->disk.d_label, sizeof(struct disklabel));
    adp->disk.d_label.d_secsize = DEV_BSIZE;
    adp->disk.d_label.d_nsectors = adp->sectors;
    adp->disk.d_label.d_ntracks = adp->heads;
    adp->disk.d_label.d_ncylinders = adp->total_secs/(adp->heads*adp->sectors);
    adp->disk.d_label.d_secpercyl = adp->sectors * adp->heads;
    adp->disk.d_label.d_secperunit = adp->total_secs;

    atadev->driver = adp;
    atadev->flags = 0;

    /* if this disk belongs to an ATA RAID dont print the probe */
    if (ata_raiddisk_attach(adp))
	adp->flags |= AD_F_RAID_SUBDISK;
    else {
	if (atadev->driver) {
	    ad_print(adp);
	    ata_enclosure_print(atadev);
	}
    }
}

void
ad_detach(struct ata_device *atadev, int flush) /* get rid of flush XXX SOS */
{
    struct ad_softc *adp = atadev->driver;
    struct ad_request *request;
    struct buf *bp;

    atadev->flags |= ATA_D_DETACHING;
    ata_prtdev(atadev, "removed from configuration\n");
    ad_invalidatequeue(adp, NULL);
    TAILQ_FOREACH(request, &atadev->channel->ata_queue, chain) {
	if (request->softc != adp)
	    continue;
	TAILQ_REMOVE(&atadev->channel->ata_queue, request, chain);
	request->bp->b_error = ENXIO;
	request->bp->b_flags |= B_ERROR;
	biodone(request->bp);
	ad_free(request);
    }
    ata_dmafree(atadev);
    while ((bp = bufq_first(&adp->queue))) {
	bufq_remove(&adp->queue, bp); 
	bp->b_error = ENXIO;
	bp->b_flags |= B_ERROR;
	biodone(bp);
    }
    disk_invalidate(&adp->disk);
    disk_destroy(adp->dev);
    devstat_remove_entry(&adp->stats);
    if (flush) {
	if (ata_command(atadev, ATA_C_FLUSHCACHE, 0, 0, 0, ATA_WAIT_READY))
	    ata_prtdev(atadev, "flushing cache on detach failed\n");
    }
    if (adp->flags & AD_F_RAID_SUBDISK)
	ata_raiddisk_detach(adp);
    ata_free_name(atadev);
    ata_free_lun(&adp_lun_map, adp->lun);
    atadev->driver = NULL;
    atadev->flags = 0;
    free(adp, M_AD);
}

static int
adopen(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct ad_softc *adp = dev->si_drv1;

    if (adp->flags & AD_F_RAID_SUBDISK)
	return EBUSY;
    return 0;
}

static int
adclose(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct ad_softc *adp = dev->si_drv1;

    ATA_SLEEPLOCK_CH(adp->device->channel, ATA_CONTROL);
    if (ata_command(adp->device, ATA_C_FLUSHCACHE, 0, 0, 0, ATA_WAIT_READY))
	ata_prtdev(adp->device, "flushing cache on close failed\n");
    ATA_UNLOCK_CH(adp->device->channel);
    return 0;
}

static void 
adstrategy(struct buf *bp)
{
    struct ad_softc *adp = bp->b_dev->si_drv1;
    int s;

    if (adp->device->flags & ATA_D_DETACHING) {
	bp->b_error = ENXIO;
	bp->b_flags |= B_ERROR;
	biodone(bp);
	return;
    }
    s = splbio();
    bufqdisksort(&adp->queue, bp);
    splx(s);
    ata_start(adp->device->channel);
}

int
addump(dev_t dev)
{
    struct ad_softc *adp = dev->si_drv1;
    struct ad_request request;
    u_int count, blkno, secsize;
    vm_paddr_t addr = 0;
    long blkcnt;
    int dumppages = MAXDUMPPGS;
    int error;
    int i;

    if ((error = disk_dumpcheck(dev, &count, &blkno, &secsize)))
	return error;
	
    if (!adp)
	return ENXIO;

    /* force PIO mode for dumps */
    adp->device->mode = ATA_PIO;
    ata_reinit(adp->device->channel);

    blkcnt = howmany(PAGE_SIZE, secsize);

    while (count > 0) {
	caddr_t va = NULL;
	DELAY(1000);

	if ((count / blkcnt) < dumppages)
	    dumppages = count / blkcnt;

	for (i = 0; i < dumppages; ++i) {
	    vm_paddr_t a = addr + (i * PAGE_SIZE);
	    if (is_physical_memory(a))
		va = pmap_kenter_temporary(trunc_page(a), i);
	    else
		va = pmap_kenter_temporary(trunc_page(0), i);
	}

	bzero(&request, sizeof(struct ad_request));
	request.softc = adp;
	request.blockaddr = blkno;
	request.bytecount = PAGE_SIZE * dumppages;
	request.data = va;

	while (request.bytecount > 0) {
	    ad_transfer(&request);
	    if (request.flags & ADR_F_ERROR)
		return EIO;
	    request.donecount += request.currentsize;
	    request.bytecount -= request.currentsize;
	    DELAY(20);
	}

	if (dumpstatus(addr, (off_t)count * DEV_BSIZE) < 0)
	    return EINTR;

	blkno += blkcnt * dumppages;
	count -= blkcnt * dumppages;
	addr += PAGE_SIZE * dumppages;
    }

    if (ata_wait(adp->device, ATA_S_READY | ATA_S_DSC) < 0)
	ata_prtdev(adp->device, "timeout waiting for final ready\n");
    return 0;
}

void
ad_start(struct ata_device *atadev)
{
    struct ad_softc *adp = atadev->driver;
    struct buf *bp = bufq_first(&adp->queue);
    struct ad_request *request;
    int tag = 0;

    if (!bp)
	return;

    /* if tagged queueing enabled get next free tag */
    if (adp->flags & AD_F_TAG_ENABLED) {
	while (tag <= adp->num_tags && adp->tags[tag])
	    tag++;
	if (tag > adp->num_tags )
	    return;
    }

    if (!(request = malloc(sizeof(struct ad_request), M_AD, M_NOWAIT|M_ZERO))) {
	ata_prtdev(atadev, "out of memory in start\n");
	return;
    }

    /* setup request */
    request->softc = adp;
    request->bp = bp;
    request->blockaddr = bp->b_pblkno;
    request->bytecount = bp->b_bcount;
    request->data = bp->b_data;
    request->tag = tag;
    if (bp->b_flags & B_READ) 
	request->flags |= ADR_F_READ;
    if (adp->device->mode >= ATA_DMA) {
	if (ata_dmaalloc(atadev))
	    adp->device->mode = ATA_PIO;
    }

    /* insert in tag array */
    adp->tags[tag] = request;

    /* remove from drive queue */
    bufq_remove(&adp->queue, bp); 

    /* link onto controller queue */
    TAILQ_INSERT_TAIL(&atadev->channel->ata_queue, request, chain);
}

int
ad_transfer(struct ad_request *request)
{
    struct ad_softc *adp;
    u_int64_t lba;
    u_int32_t count, max_count;
    u_int8_t cmd;
    int flags = ATA_IMMEDIATE;

    /* get request params */
    adp = request->softc;

    /* calculate transfer details */
    lba = request->blockaddr + (request->donecount / DEV_BSIZE);
   
    if (request->donecount == 0) {

	/* start timeout for this transfer */
	if (dumping)
	    request->timeout_handle.callout = NULL;
	else
	    request->timeout_handle = 
		timeout((timeout_t*)ad_timeout, request, 10 * hz);

	/* setup transfer parameters */
	count = howmany(request->bytecount, DEV_BSIZE);
	max_count = adp->device->param->support.address48 ? 65536 : 256;
	if (count > max_count) {
	    ata_prtdev(adp->device,
		       "count %d size transfers not supported\n", count);
	    count = max_count;
	}

	if (adp->flags & AD_F_CHS_USED) {
	    int sector = (lba % adp->sectors) + 1;
	    int cylinder = lba / (adp->sectors * adp->heads);
	    int head = (lba % (adp->sectors * adp->heads)) / adp->sectors;

	    lba = (sector&0xff) | ((cylinder&0xffff)<<8) | ((head&0xf)<<24);
	    adp->device->flags |= ATA_D_USE_CHS;
	}

	/* setup first transfer length */
	request->currentsize = min(request->bytecount, adp->transfersize);

	devstat_start_transaction(&adp->stats);

	/* does this drive & transfer work with DMA ? */
	request->flags &= ~ADR_F_DMA_USED;
	if (adp->device->mode >= ATA_DMA &&
	    !ata_dmasetup(adp->device, request->data, request->bytecount)) {
	    request->flags |= ADR_F_DMA_USED;
	    request->currentsize = request->bytecount;

	    /* do we have tags enabled ? */
	    if (adp->flags & AD_F_TAG_ENABLED) {
		cmd = (request->flags & ADR_F_READ) ?
		    ATA_C_READ_DMA_QUEUED : ATA_C_WRITE_DMA_QUEUED;

		if (ata_command(adp->device, cmd, lba,
				request->tag << 3, count, flags)) {
		    ata_prtdev(adp->device, "error executing command");
		    goto transfer_failed;
		}
		if (ata_wait(adp->device, ATA_S_READY)) {
		    ata_prtdev(adp->device, "timeout waiting for READY\n");
		    goto transfer_failed;
		}
		adp->outstanding++;

		/* if ATA bus RELEASE check for SERVICE */
		if (adp->flags & AD_F_TAG_ENABLED &&
		    ATA_INB(adp->device->channel->r_io, ATA_IREASON) &
		    ATA_I_RELEASE)
		    return ad_service(adp, 1);
	    }
	    else {
		cmd = (request->flags & ADR_F_READ) ?
		    ATA_C_READ_DMA : ATA_C_WRITE_DMA;

		if (ata_command(adp->device, cmd, lba, count, 0, flags)) {
		    ata_prtdev(adp->device, "error executing command");
		    goto transfer_failed;
		}
#if 0
		/*
		 * wait for data transfer phase
		 *
		 * well this should be here acording to specs, but older
		 * promise controllers doesn't like it, they lockup!
		 */
		if (ata_wait(adp->device, ATA_S_READY | ATA_S_DRQ)) {
		    ata_prtdev(adp->device, "timeout waiting for data phase\n");
		    goto transfer_failed;
		}
#endif
	    }

	    /* start transfer, return and wait for interrupt */
	    ata_dmastart(adp->device, request->data, request->bytecount,
			 request->flags & ADR_F_READ);
	    return ATA_OP_CONTINUES;
	}

	/* does this drive support multi sector transfers ? */
	if (request->currentsize > DEV_BSIZE)
	    cmd = request->flags&ADR_F_READ ? ATA_C_READ_MUL : ATA_C_WRITE_MUL;

	/* just plain old single sector transfer */
	else
	    cmd = request->flags&ADR_F_READ ? ATA_C_READ : ATA_C_WRITE;

	if (ata_command(adp->device, cmd, lba, count, 0, flags)){
	    ata_prtdev(adp->device, "error executing command");
	    goto transfer_failed;
	}
    }
   
    /* calculate this transfer length */
    request->currentsize = min(request->bytecount, adp->transfersize);

    /* if this is a PIO read operation, return and wait for interrupt */
    if (request->flags & ADR_F_READ)
	return ATA_OP_CONTINUES;

    /* ready to write PIO data ? */
    if (ata_wait(adp->device, (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) < 0) {
	ata_prtdev(adp->device, "timeout waiting for DRQ");
	goto transfer_failed;
    }

    /* output the data */
    if (adp->device->channel->flags & ATA_USE_16BIT)
	ATA_OUTSW(adp->device->channel->r_io, ATA_DATA,
		  (void *)((uintptr_t)request->data + request->donecount),
		  request->currentsize / sizeof(int16_t));
    else
	ATA_OUTSL(adp->device->channel->r_io, ATA_DATA,
		  (void *)((uintptr_t)request->data + request->donecount),
		  request->currentsize / sizeof(int32_t));
    return ATA_OP_CONTINUES;

transfer_failed:
    untimeout((timeout_t *)ad_timeout, request, request->timeout_handle);
    ad_invalidatequeue(adp, request);
    printf(" - resetting\n");

    /* if retries still permit, reinject this request */
    if (request->retries++ < AD_MAX_RETRIES)
	TAILQ_INSERT_HEAD(&adp->device->channel->ata_queue, request, chain);
    else {
	/* retries all used up, return error */
	request->bp->b_error = EIO;
	request->bp->b_flags |= B_ERROR;
	request->bp->b_resid = request->bytecount;
	devstat_end_transaction_buf(&adp->stats, request->bp);
	biodone(request->bp);
	ad_free(request);
    }
    ata_reinit(adp->device->channel);
    return ATA_OP_CONTINUES;
}

int
ad_interrupt(struct ad_request *request)
{
    struct ad_softc *adp = request->softc;
    int dma_stat = 0;

    /* finish DMA transfer */
    if (request->flags & ADR_F_DMA_USED)
	dma_stat = ata_dmadone(adp->device);

    /* do we have a corrected soft error ? */
    if (adp->device->channel->status & ATA_S_CORR)
	diskerr(request->bp, "soft error (ECC corrected)", LOG_PRINTF,
		request->blockaddr + (request->donecount / DEV_BSIZE),
		&adp->disk.d_label);

    /* did any real errors happen ? */
    if ((adp->device->channel->status & ATA_S_ERROR) ||
	(request->flags & ADR_F_DMA_USED && dma_stat & ATA_BMSTAT_ERROR)) {
	adp->device->channel->error =
	    ATA_INB(adp->device->channel->r_io, ATA_ERROR);
	diskerr(request->bp, (adp->device->channel->error & ATA_E_ICRC) ?
		"UDMA ICRC error" : "hard error", LOG_PRINTF,
		request->blockaddr + (request->donecount / DEV_BSIZE),
		&adp->disk.d_label);

	/* if this is a UDMA CRC error, reinject request */
	if (request->flags & ADR_F_DMA_USED &&
	    adp->device->channel->error & ATA_E_ICRC) {
	    untimeout((timeout_t *)ad_timeout, request,request->timeout_handle);
	    ad_invalidatequeue(adp, request);

	    if (request->retries++ < AD_MAX_RETRIES)
		printf(" retrying\n");
	    else {
		ata_dmainit(adp->device, ata_pmode(adp->device->param), -1, -1);
		printf(" falling back to PIO mode\n");
	    }
	    TAILQ_INSERT_HEAD(&adp->device->channel->ata_queue, request, chain);
	    return ATA_OP_FINISHED;
	}

	/* if using DMA, try once again in PIO mode */
	if (request->flags & ADR_F_DMA_USED) {
	    untimeout((timeout_t *)ad_timeout, request,request->timeout_handle);
	    ad_invalidatequeue(adp, request);
	    ata_dmainit(adp->device, ata_pmode(adp->device->param), -1, -1);
	    request->flags |= ADR_F_FORCE_PIO;
	    printf(" trying PIO mode\n");
	    TAILQ_INSERT_HEAD(&adp->device->channel->ata_queue, request, chain);
	    return ATA_OP_FINISHED;
	}

	request->flags |= ADR_F_ERROR;
	printf(" status=%02x error=%02x\n", 
	       adp->device->channel->status, adp->device->channel->error);
    }

    /* if we arrived here with forced PIO mode, DMA doesn't work right */
    if (request->flags & ADR_F_FORCE_PIO && !(request->flags & ADR_F_ERROR))
	ata_prtdev(adp->device, "DMA problem fallback to PIO mode\n");

    /* if this was a PIO read operation, get the data */
    if (!(request->flags & ADR_F_DMA_USED) &&
	(request->flags & (ADR_F_READ | ADR_F_ERROR)) == ADR_F_READ) {

	/* ready to receive data? */
	if ((adp->device->channel->status & (ATA_S_READY|ATA_S_DSC|ATA_S_DRQ))
	    != (ATA_S_READY|ATA_S_DSC|ATA_S_DRQ))
	    ata_prtdev(adp->device, "read interrupt arrived early");

	if (ata_wait(adp->device, (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) != 0) {
	    ata_prtdev(adp->device, "read error detected (too) late");
	    request->flags |= ADR_F_ERROR;
	}
	else {
	    /* data ready, read in */
	    if (adp->device->channel->flags & ATA_USE_16BIT)
		ATA_INSW(adp->device->channel->r_io, ATA_DATA,
			 (void*)((uintptr_t)request->data + request->donecount),
			 request->currentsize / sizeof(int16_t));
	    else
		ATA_INSL(adp->device->channel->r_io, ATA_DATA,
			 (void*)((uintptr_t)request->data + request->donecount),
			 request->currentsize / sizeof(int32_t));
	}
    }

    /* finish up transfer */
    if (request->flags & ADR_F_ERROR) {
	request->bp->b_error = EIO;
	request->bp->b_flags |= B_ERROR;
    } 
    else {
	request->bytecount -= request->currentsize;
	request->donecount += request->currentsize;
	if (request->bytecount > 0) {
	    ad_transfer(request);
	    return ATA_OP_CONTINUES;
	}
    }

    /* disarm timeout for this transfer */
    untimeout((timeout_t *)ad_timeout, request, request->timeout_handle);

    request->bp->b_resid = request->bytecount;

    devstat_end_transaction_buf(&adp->stats, request->bp);
    biodone(request->bp);
    ad_free(request);
    adp->outstanding--;

    /* check for SERVICE (tagged operations only) */
    return ad_service(adp, 1);
}

int
ad_service(struct ad_softc *adp, int change)
{
    /* do we have to check the other device on this channel ? */
    if (adp->device->channel->flags & ATA_QUEUED && change) {
	int device = adp->device->unit;

	if (adp->device->unit == ATA_MASTER) {
	    if ((adp->device->channel->devices & ATA_ATA_SLAVE) &&
		(adp->device->channel->device[SLAVE].driver) &&
		((struct ad_softc *) (adp->device->channel->
		 device[SLAVE].driver))->flags & AD_F_TAG_ENABLED)
		device = ATA_SLAVE;
	}
	else {
	    if ((adp->device->channel->devices & ATA_ATA_MASTER) &&
		(adp->device->channel->device[MASTER].driver) &&
		((struct ad_softc *) (adp->device->channel->
		 device[MASTER].driver))->flags & AD_F_TAG_ENABLED)
		device = ATA_MASTER;
	}
	if (device != adp->device->unit &&
	    ((struct ad_softc *)
	     (adp->device->channel->
	      device[ATA_DEV(device)].driver))->outstanding > 0) {
	    ATA_OUTB(adp->device->channel->r_io, ATA_DRIVE, ATA_D_IBM | device);
	    adp = adp->device->channel->device[ATA_DEV(device)].driver;
	    DELAY(1);
	}
    }
    adp->device->channel->status =
	ATA_INB(adp->device->channel->r_altio, ATA_ALTSTAT);
 
    /* do we have a SERVICE request from the drive ? */
    if (adp->flags & AD_F_TAG_ENABLED &&
	adp->outstanding > 0 &&
	adp->device->channel->status & ATA_S_SERVICE) {
	struct ad_request *request;
	int tag;

	/* check for error */
	if (adp->device->channel->status & ATA_S_ERROR) {
	    ata_prtdev(adp->device, "Oops! controller says s=0x%02x e=0x%02x\n",
		       adp->device->channel->status,
		       adp->device->channel->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}

	/* issue SERVICE cmd */
	if (ata_command(adp->device, ATA_C_SERVICE, 0, 0, 0, ATA_IMMEDIATE)) {
	    ata_prtdev(adp->device, "problem executing SERVICE cmd\n");
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}

	/* setup the transfer environment when ready */
	if (ata_wait(adp->device, ATA_S_READY)) {
	    ata_prtdev(adp->device, "SERVICE timeout tag=%d s=%02x e=%02x\n",
		       ATA_INB(adp->device->channel->r_io, ATA_COUNT) >> 3,
		       adp->device->channel->status,
		       adp->device->channel->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	tag = ATA_INB(adp->device->channel->r_io, ATA_COUNT) >> 3;
	if (!(request = adp->tags[tag])) {
	    ata_prtdev(adp->device, "no request for tag=%d\n", tag);	
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	ATA_FORCELOCK_CH(adp->device->channel, ATA_ACTIVE_ATA);
	adp->device->channel->running = request;
	request->serv++;

	/* start DMA transfer when ready */
	if (ata_wait(adp->device, ATA_S_READY | ATA_S_DRQ)) {
	    ata_prtdev(adp->device, "timeout starting DMA s=%02x e=%02x\n",
		       adp->device->channel->status,
		       adp->device->channel->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	ata_dmastart(adp->device, request->data, request->bytecount,
		     request->flags & ADR_F_READ);
	return ATA_OP_CONTINUES;
    }
    return ATA_OP_FINISHED;
}

static void
ad_free(struct ad_request *request)
{
    request->softc->tags[request->tag] = NULL;
    free(request, M_AD);
}

static void
ad_invalidatequeue(struct ad_softc *adp, struct ad_request *request)
{
    /* if tags used invalidate all other tagged transfers */
    if (adp->flags & AD_F_TAG_ENABLED) {
	struct ad_request *tmpreq;
	int tag;

	ata_prtdev(adp->device, "invalidating queued requests\n");
	for (tag = 0; tag <= adp->num_tags; tag++) {
	    tmpreq = adp->tags[tag];
	    adp->tags[tag] = NULL;
	    if (tmpreq == request || tmpreq == NULL)
		continue;
	    untimeout((timeout_t *)ad_timeout, tmpreq, tmpreq->timeout_handle);
	    TAILQ_INSERT_HEAD(&adp->device->channel->ata_queue, tmpreq, chain);
	}
	if (ata_command(adp->device, ATA_C_NOP,
			0, 0, ATA_C_F_FLUSHQUEUE, ATA_WAIT_READY))
	    ata_prtdev(adp->device, "flush queue failed\n");
	adp->outstanding = 0;
    }
}

static int
ad_tagsupported(struct ad_softc *adp)
{
    const char *good[] = {"IBM-DPTA", "IBM-DTLA", NULL};
    int i = 0;

    switch (adp->device->channel->chiptype) {
    case 0x4d33105a: /* Promises before TX2 doesn't work with tagged queuing */
    case 0x4d38105a:
    case 0x0d30105a:
    case 0x4d30105a:  
	return 0;
    }

    /* check that drive does DMA, has tags enabled, and is one we know works */
    if (adp->device->mode >= ATA_DMA && adp->device->param->support.queued && 
	adp->device->param->enabled.queued) {
	while (good[i] != NULL) {
	    if (!strncmp(adp->device->param->model, good[i], strlen(good[i])))
		return 1;
	    i++;
	}
	/* 
	 * check IBM's new obscure way of naming drives 
	 * we want "IC" (IBM CORP) and "AT" or "AV" (ATA interface)
	 * but doesn't care about the other info (size, capacity etc)
	 */
	if (!strncmp(adp->device->param->model, "IC", 2) &&
	    (!strncmp(adp->device->param->model + 8, "AT", 2) ||
	     !strncmp(adp->device->param->model + 8, "AV", 2)))
		return 1;
    }
    return 0;
}

static void
ad_timeout(struct ad_request *request)
{
    struct ad_softc *adp = request->softc;

    adp->device->channel->running = NULL;
    ata_prtdev(adp->device, "%s command timeout tag=%d serv=%d - resetting\n",
	       (request->flags & ADR_F_READ) ? "READ" : "WRITE",
	       request->tag, request->serv);

    if (request->flags & ADR_F_DMA_USED) {
	ata_dmadone(adp->device);
	ad_invalidatequeue(adp, request);
	if (request->retries == AD_MAX_RETRIES) {
	    ata_dmainit(adp->device, ata_pmode(adp->device->param), -1, -1);
	    ata_prtdev(adp->device, "trying fallback to PIO mode\n");
	    request->retries = 0;
	}
    }

    /* if retries still permit, reinject this request */
    if (request->retries++ < AD_MAX_RETRIES) {
	TAILQ_INSERT_HEAD(&adp->device->channel->ata_queue, request, chain);
    }
    else {
	/* retries all used up, return error */
	request->bp->b_error = EIO;
	request->bp->b_flags |= B_ERROR;
	devstat_end_transaction_buf(&adp->stats, request->bp);
	biodone(request->bp);
	ad_free(request);
    }
    ata_reinit(adp->device->channel);
}

void
ad_reinit(struct ata_device *atadev)
{
    struct ad_softc *adp = atadev->driver;

    /* reinit disk parameters */
    ad_invalidatequeue(atadev->driver, NULL);
    ata_command(atadev, ATA_C_SET_MULTI, 0,
		adp->transfersize / DEV_BSIZE, 0, ATA_WAIT_READY);
    if (adp->device->mode >= ATA_DMA)
	ata_dmainit(atadev, ata_pmode(adp->device->param),
		    ata_wmode(adp->device->param),
		    ata_umode(adp->device->param));
    else
	ata_dmainit(atadev, ata_pmode(adp->device->param), -1, -1);
}

void
ad_print(struct ad_softc *adp) 
{
    if (bootverbose) {
	ata_prtdev(adp->device, "<%.40s/%.8s> ATA-%d disk at ata%d-%s\n", 
		   adp->device->param->model, adp->device->param->revision,
		   ad_version(adp->device->param->version_major), 
		   device_get_unit(adp->device->channel->dev),
		   (adp->device->unit == ATA_MASTER) ? "master" : "slave");

	ata_prtdev(adp->device,
		   "%lluMB (%llu sectors), %llu C, %u H, %u S, %u B\n",
		   (unsigned long long)(adp->total_secs /
		   ((1024L*1024L)/DEV_BSIZE)),
		   (unsigned long long) adp->total_secs,
		   (unsigned long long) (adp->total_secs /
		    (adp->heads * adp->sectors)),
		   adp->heads, adp->sectors, DEV_BSIZE);

	ata_prtdev(adp->device, "%d secs/int, %d depth queue, %s%s\n", 
		   adp->transfersize / DEV_BSIZE, adp->num_tags + 1,
		   (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
		   ata_mode2str(adp->device->mode));

	ata_prtdev(adp->device, "piomode=%d dmamode=%d udmamode=%d cblid=%d\n",
		   ata_pmode(adp->device->param), ata_wmode(adp->device->param),
		   ata_umode(adp->device->param), 
		   adp->device->param->hwres_cblid);

    }
    else
	ata_prtdev(adp->device,"%lluMB <%.40s> [%lld/%d/%d] at ata%d-%s %s%s\n",
		   (unsigned long long)(adp->total_secs /
		   ((1024L * 1024L) / DEV_BSIZE)),
		   adp->device->param->model,
		   (unsigned long long)(adp->total_secs /
		    (adp->heads*adp->sectors)),
		   adp->heads, adp->sectors,
		   device_get_unit(adp->device->channel->dev),
		   (adp->device->unit == ATA_MASTER) ? "master" : "slave",
		   (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
		   ata_mode2str(adp->device->mode));
}

static int
ad_version(u_int16_t version)
{
    int bit;

    if (version == 0xffff)
	return 0;
    for (bit = 15; bit >= 0; bit--)
	if (version & (1<<bit))
	    return bit;
    return 0;
}
