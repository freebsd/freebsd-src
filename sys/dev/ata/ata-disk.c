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
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/cons.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>

/* device structures */
static d_open_t		adopen;
static d_strategy_t	adstrategy;
static d_dump_t		addump;
static struct cdevsw ad_cdevsw = {
	/* open */	adopen,
	/* close */	nullclose,
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
};
static struct cdevsw addisk_cdevsw;

/* prototypes */
static void ad_invalidatequeue(struct ad_softc *, struct ad_request *);
static int ad_tagsupported(struct ad_softc *);
static void ad_timeout(struct ad_request *);
static void ad_free(struct ad_request *);
static int ad_version(u_int16_t);

/* internal vars */
static u_int32_t adp_lun_map = 0;
static MALLOC_DEFINE(M_AD, "AD driver", "ATA disk driver");
static int ata_dma, ata_wc, ata_tags; 
TUNABLE_INT_DECL("hw.ata.ata_dma", 1, ata_dma);
TUNABLE_INT_DECL("hw.ata.wc", 0, ata_wc);
TUNABLE_INT_DECL("hw.ata.tags", 0, ata_tags);

/* sysctl vars */
SYSCTL_DECL(_hw_ata);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma, CTLFLAG_RD, &ata_dma, 0,
	   "ATA disk DMA mode control");
SYSCTL_INT(_hw_ata, OID_AUTO, wc, CTLFLAG_RD, &ata_wc, 0,
	   "ATA disk write caching");
SYSCTL_INT(_hw_ata, OID_AUTO, tags, CTLFLAG_RD, &ata_tags, 0,
	   "ATA disk tagged queuing support");

/* defines */
#define	AD_MAX_RETRIES	3
#define AD_PARAM	ATA_PARAM(adp->controller, adp->unit)

/* experimental cache flush on BIO_ORDERED */
#define ATA_FLUSHCACHE_ON 

void
ad_attach(struct ata_softc *scp, int device)
{
    struct ad_softc *adp;
    dev_t dev;
    int secsperint;
    char name[16];

    if (!(adp = malloc(sizeof(struct ad_softc), M_AD, M_NOWAIT | M_ZERO))) {
	ata_printf(scp, device, "failed to allocate driver storage\n");
	return;
    }
    adp->controller = scp;
    adp->unit = device;
#ifdef ATA_STATIC_ID
    adp->lun = (device_get_unit(scp->dev) << 1) + ATA_DEV(device);
#else
    adp->lun = ata_get_lun(&adp_lun_map);
#endif
    sprintf(name, "ad%d", adp->lun);
    ata_set_name(scp, device, name);
    adp->heads = AD_PARAM->heads;
    adp->sectors = AD_PARAM->sectors;
    adp->total_secs = AD_PARAM->cylinders * adp->heads * adp->sectors;	
    if (AD_PARAM->cylinders == 16383 && adp->total_secs < AD_PARAM->lbasize)
	adp->total_secs = AD_PARAM->lbasize;
    
    if (ad_version(AD_PARAM->versmajor) && 
	AD_PARAM->atavalid & ATA_FLAG_54_58 && AD_PARAM->lbasize)
	adp->flags |= AD_F_LBA_ENABLED;

    /* use multiple sectors/interrupt if device supports it */
    adp->transfersize = DEV_BSIZE;
    if (ad_version(AD_PARAM->versmajor)) {
	secsperint = max(1, min(AD_PARAM->nsecperint, 16));
	if (!ata_command(adp->controller, adp->unit, ATA_C_SET_MULTI,
			 0, 0, 0, secsperint, 0, ATA_WAIT_INTR) &&
            !ata_wait(adp->controller, adp->unit, 0))
        adp->transfersize *= secsperint;
    }

    /* enable read cacheing if not default on device */
    if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
		    0, 0, 0, 0, ATA_C_F_ENAB_RCACHE, ATA_WAIT_INTR))
	ata_printf(scp, device, "enabling readahead cache failed\n");

    /* enable write cacheing if allowed and not default on device */
    if (ata_wc || ata_tags) {
	if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			0, 0, 0, 0, ATA_C_F_ENAB_WCACHE, ATA_WAIT_INTR))
	    ata_printf(scp, device, "enabling write cache failed\n");
    }
    else {
	if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			0, 0, 0, 0, ATA_C_F_DIS_WCACHE, ATA_WAIT_INTR))
	    ata_printf(scp, device, "disabling write cache failed\n");
    }

    /* use DMA if allowed and if drive/controller supports it */
    if (ata_dma)
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM), 
		    ata_wmode(AD_PARAM), ata_umode(AD_PARAM));
    else
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM), -1, -1);

    /* use tagged queueing if allowed and supported */
    if (ata_tags && ad_tagsupported(adp)) {
	adp->num_tags = AD_PARAM->queuelen;
	adp->flags |= AD_F_TAG_ENABLED;
	adp->controller->flags |= ATA_QUEUED;
	if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			0, 0, 0, 0, ATA_C_F_DIS_RELIRQ, ATA_WAIT_INTR))
	    ata_printf(scp, device, "disabling release interrupt failed\n");
	if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			0, 0, 0, 0, ATA_C_F_DIS_SRVIRQ, ATA_WAIT_INTR))
	    ata_printf(scp, device, "disabling service interrupt failed\n");
    }

    devstat_add_entry(&adp->stats, "ad", adp->lun, DEV_BSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
		      DEVSTAT_PRIORITY_DISK);

    dev = disk_create(adp->lun, &adp->disk, 0, &ad_cdevsw, &addisk_cdevsw);
    dev->si_drv1 = adp;
    dev->si_iosize_max = 256 * DEV_BSIZE;
    adp->dev = dev;
    bioq_init(&adp->queue);

    /* if this disk belongs to an ATA RAID dont print the probe */
    if (!ar_probe(adp))
	adp->flags |= AD_F_RAID_SUBDISK;
    else
	ad_print(adp, "");

    /* construct the disklabel */
    bzero(&adp->disk.d_label, sizeof(struct disklabel));
    adp->disk.d_label.d_secsize = DEV_BSIZE;
    adp->disk.d_label.d_nsectors = adp->sectors;
    adp->disk.d_label.d_ntracks = adp->heads;
    adp->disk.d_label.d_ncylinders = adp->total_secs/(adp->heads*adp->sectors);
    adp->disk.d_label.d_secpercyl = adp->sectors * adp->heads;
    adp->disk.d_label.d_secperunit = adp->total_secs;

    /* store our softc signalling we are ready to go */
    scp->dev_softc[ATA_DEV(device)] = adp;
}

void
ad_detach(struct ad_softc *adp, int flush)
{
    struct ad_request *request;
    struct bio *bp;

    adp->flags |= AD_F_DETACHING;

    if (adp->flags & AD_F_RAID_SUBDISK)
	printf("WARNING! detaching RAID subdisk, danger ahead\n");

    ata_printf(adp->controller, adp->unit, "removed from configuration\n");
    ad_invalidatequeue(adp, NULL);
    TAILQ_FOREACH(request, &adp->controller->ata_queue, chain) {
	if (request->device != adp)
	    continue;
	TAILQ_REMOVE(&adp->controller->ata_queue, request, chain);
	request->bp->bio_error = ENXIO;
	request->bp->bio_flags |= BIO_ERROR;
	biodone(request->bp);
	ad_free(request);
    }
    while ((bp = bioq_first(&adp->queue))) {
	bp->bio_error = ENXIO;
	bp->bio_flags |= BIO_ERROR;
	biodone(bp);
    }
    disk_invalidate(&adp->disk);
    disk_destroy(adp->dev);
    devstat_remove_entry(&adp->stats);
    if (flush) {
	if (ata_command(adp->controller, adp->unit, ATA_C_FLUSHCACHE,
			0, 0, 0, 0, 0, ATA_WAIT_READY))
    	    ata_printf(adp->controller, adp->unit,
		       "flushing cache on detach failed\n");
    }
    ata_free_lun(&adp_lun_map, adp->lun);
    adp->controller->dev_softc[ATA_DEV(adp->unit)] = NULL;
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

static void 
adstrategy(struct bio *bp)
{
    struct ad_softc *adp = bp->bio_dev->si_drv1;
    int s;

    if (adp->flags & AD_F_DETACHING) {
	bp->bio_error = ENXIO;
	bp->bio_flags |= BIO_ERROR;
	biodone(bp);
	return;
    }

    /* if it's a null transfer, return immediatly. */
    if (bp->bio_bcount == 0) {
	bp->bio_resid = 0;
	biodone(bp);
	return;
    }

    s = splbio();
    bioqdisksort(&adp->queue, bp);
    ata_start(adp->controller);
    splx(s);
}

int
addump(dev_t dev)
{
    struct ad_softc *adp = dev->si_drv1;
    struct ad_request request;
    u_int count, blkno, secsize;
    vm_offset_t addr = 0;
    long blkcnt;
    int dumppages = MAXDUMPPGS;
    int error;
    int i;

    if ((error = disk_dumpcheck(dev, &count, &blkno, &secsize)))
	return error;
	
    if (!adp)
	return ENXIO;

    /* force PIO mode for dumps */
    adp->controller->mode[ATA_DEV(adp->unit)] = ATA_PIO;
    ata_reinit(adp->controller);

    blkcnt = howmany(PAGE_SIZE, secsize);

    while (count > 0) {
	caddr_t va = NULL;
	DELAY(1000);

	if ((count / blkcnt) < dumppages)
	    dumppages = count / blkcnt;

	for (i = 0; i < dumppages; ++i) {
	    vm_offset_t a = addr + (i * PAGE_SIZE);
	    if (is_physical_memory(a))
		va = pmap_kenter_temporary(trunc_page(a), i);
	    else
		va = pmap_kenter_temporary(trunc_page(0), i);
	}

	bzero(&request, sizeof(struct ad_request));
	request.device = adp;
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

	if (dumpstatus(addr, (long)(count * DEV_BSIZE)) < 0)
	    return EINTR;

	blkno += blkcnt * dumppages;
	count -= blkcnt * dumppages;
	addr += PAGE_SIZE * dumppages;
    }

    if (ata_wait(adp->controller, adp->unit, ATA_S_READY | ATA_S_DSC) < 0)
	ata_printf(adp->controller, adp->unit,
		   "timeout waiting for final ready\n");
    return 0;
}

void
ad_start(struct ad_softc *adp)
{
    struct bio *bp = bioq_first(&adp->queue);
    struct ad_request *request;
    int tag = 0;

    if (!bp)
	return;

#ifdef ATA_FLUSHCACHE_ON 
    /*
     * if BIO_ORDERED is set cache should be flushed, if there are
     * any outstanding requests, hold off and wait for them to finish
     */
    if (adp->flags & AD_F_TAG_ENABLED &&
	bp->bio_flags & BIO_ORDERED && adp->outstanding > 0)
	return;
#endif

    /* if tagged queueing enabled get next free tag */
    if (adp->flags & AD_F_TAG_ENABLED) {
	while (tag <= adp->num_tags && adp->tags[tag])
	    tag++;
	if (tag > adp->num_tags )
	    return;
    }

    if (!(request = malloc(sizeof(struct ad_request), M_AD, M_NOWAIT|M_ZERO))) {
	ata_printf(adp->controller, adp->unit, "out of memory in start\n");
	return;
    }

    /* setup request */
    request->device = adp;
    request->bp = bp;
    request->blockaddr = bp->bio_pblkno;
    request->bytecount = bp->bio_bcount;
    request->data = bp->bio_data;
    request->tag = tag;
    if (bp->bio_cmd == BIO_READ) 
	request->flags |= ADR_F_READ;
    if (adp->controller->mode[ATA_DEV(adp->unit)] >= ATA_DMA) {
	if (!(request->dmatab = ata_dmaalloc(adp->controller, adp->unit)))
	    adp->controller->mode[ATA_DEV(adp->unit)] = ATA_PIO;
    }

    /* insert in tag array */
    adp->tags[tag] = request;

    /* remove from drive queue */
    bioq_remove(&adp->queue, bp); 

    /* link onto controller queue */
    TAILQ_INSERT_TAIL(&adp->controller->ata_queue, request, chain);
}

int
ad_transfer(struct ad_request *request)
{
    struct ad_softc *adp;
    u_int32_t blkno, secsprcyl;
    u_int32_t cylinder, head, sector, count, cmd;

    /* get request params */
    adp = request->device;

    /* calculate transfer details */
    blkno = request->blockaddr + (request->donecount / DEV_BSIZE);
   
    if (request->donecount == 0) {

	/* start timeout for this transfer */
	if (dumping)
	    request->timeout_handle.callout = NULL;
	else
	    request->timeout_handle = 
		timeout((timeout_t*)ad_timeout, request, 10 * hz);

	/* setup transfer parameters */
	count = howmany(request->bytecount, DEV_BSIZE);
	if (count > 256) {
	    count = 256;
	    ata_printf(adp->controller, adp->unit,
		       "count %d size transfers not supported\n", count);
	}

	if (adp->flags & AD_F_LBA_ENABLED) {
	    sector = (blkno >> 0) & 0xff; 
	    cylinder = (blkno >> 8) & 0xffff;
	    head = ((blkno >> 24) & 0xf) | ATA_D_LBA; 
	}
	else {
	    secsprcyl = adp->sectors * adp->heads;
	    cylinder = blkno / secsprcyl;
	    head = (blkno % secsprcyl) / adp->sectors;
	    sector = (blkno % adp->sectors) + 1;
	}

	/* setup first transfer length */
	request->currentsize = min(request->bytecount, adp->transfersize);

	devstat_start_transaction(&adp->stats);

	/* does this drive & transfer work with DMA ? */
	request->flags &= ~ADR_F_DMA_USED;
	if (adp->controller->mode[ATA_DEV(adp->unit)] >= ATA_DMA &&
	    !ata_dmasetup(adp->controller, adp->unit, request->dmatab,
			  request->data, request->bytecount)) {
	    request->flags |= ADR_F_DMA_USED;
	    request->currentsize = request->bytecount;

	    /* do we have tags enabled ? */
	    if (adp->flags & AD_F_TAG_ENABLED) {
	    	cmd = (request->flags & ADR_F_READ) ?
		    ATA_C_READ_DMA_QUEUED : ATA_C_WRITE_DMA_QUEUED;

		if (ata_command(adp->controller, adp->unit, cmd, 
		    		cylinder, head, sector, request->tag << 3,
				count, ATA_IMMEDIATE)) {
		    ata_printf(adp->controller, adp->unit,
			       "error executing command");
		    goto transfer_failed;
		}
		if (ata_wait(adp->controller, adp->unit, ATA_S_READY)) {
		    ata_printf(adp->controller, adp->unit,
			       "timeout waiting for READY\n");
		    goto transfer_failed;
		}
		adp->outstanding++;

		/* if ATA bus RELEASE check for SERVICE */
		if (adp->flags & AD_F_TAG_ENABLED &&
		    ATA_INB(adp->controller->r_io, ATA_IREASON) & ATA_I_RELEASE) {
		    return ad_service(adp, 1);
		}
	    }
	    else {
	    	cmd = (request->flags & ADR_F_READ) ?
		    ATA_C_READ_DMA : ATA_C_WRITE_DMA;

		if (ata_command(adp->controller, adp->unit, cmd, cylinder, 
		    		head, sector, count, 0, ATA_IMMEDIATE)) {
		    ata_printf(adp->controller, adp->unit,
			       "error executing command");
		    goto transfer_failed;
		}
#if 0
		/*
		 * wait for data transfer phase
		 *
		 * well this should be here acording to specs, but
		 * promise controllers doesn't like it, they lockup!
		 * thats probably why tags doesn't work on the promise
		 * as this is needed there...
		 */
		if (ata_wait(adp->controller, adp->unit, 
		    	     ATA_S_READY | ATA_S_DRQ)) {
		    ata_printf(adp->controller, adp->unit,
			       "timeout waiting for data phase\n");
		    goto transfer_failed;
		}
#endif
	    }

	    /* start transfer, return and wait for interrupt */
	    ata_dmastart(adp->controller, adp->unit,
	    		 request->dmatab, request->flags & ADR_F_READ);
	    return ATA_OP_CONTINUES;
	}

	/* does this drive support multi sector transfers ? */
	if (request->currentsize > DEV_BSIZE)
	    cmd = request->flags&ADR_F_READ ? ATA_C_READ_MUL : ATA_C_WRITE_MUL;

	/* just plain old single sector transfer */
	else
	    cmd = request->flags&ADR_F_READ ? ATA_C_READ : ATA_C_WRITE;

	if (ata_command(adp->controller, adp->unit, cmd, 
			cylinder, head, sector, count, 0, ATA_IMMEDIATE)) {
	    ata_printf(adp->controller, adp->unit, "error executing command");
	    goto transfer_failed;
	}
    }
   
    /* calculate this transfer length */
    request->currentsize = min(request->bytecount, adp->transfersize);

    /* if this is a PIO read operation, return and wait for interrupt */
    if (request->flags & ADR_F_READ)
	return ATA_OP_CONTINUES;

    /* ready to write PIO data ? */
    if (ata_wait(adp->controller, adp->unit, 
		 (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) < 0) {
	ata_printf(adp->controller, adp->unit, "timeout waiting for DRQ");
	goto transfer_failed;
    }

    /* output the data */
    if (adp->controller->flags & ATA_USE_16BIT)
	ATA_OUTSW(adp->controller->r_io, ATA_DATA,
		     (void *)((uintptr_t)request->data + request->donecount),
		     request->currentsize / sizeof(int16_t));
    else
	ATA_OUTSL(adp->controller->r_io, ATA_DATA,
		     (void *)((uintptr_t)request->data + request->donecount),
		     request->currentsize / sizeof(int32_t));
    return ATA_OP_CONTINUES;

transfer_failed:
    untimeout((timeout_t *)ad_timeout, request, request->timeout_handle);
    ad_invalidatequeue(adp, request);
    printf(" - resetting\n");

    /* if retries still permit, reinject this request */
    if (request->retries++ < AD_MAX_RETRIES)
	TAILQ_INSERT_HEAD(&adp->controller->ata_queue, request, chain);
    else {
	/* retries all used up, return error */
	request->bp->bio_error = EIO;
	request->bp->bio_flags |= BIO_ERROR;
	request->bp->bio_resid = request->bytecount;
	devstat_end_transaction_bio(&adp->stats, request->bp);
	biodone(request->bp);
	ad_free(request);
    }
    ata_reinit(adp->controller);
    return ATA_OP_CONTINUES;
}

int
ad_interrupt(struct ad_request *request)
{
    struct ad_softc *adp = request->device;
    int dma_stat = 0;

    if (request->flags & ADR_F_FLUSHCACHE)
        goto finish;

    /* finish DMA transfer */
    if (request->flags & ADR_F_DMA_USED)
	dma_stat = ata_dmadone(adp->controller);

    /* do we have a corrected soft error ? */
    if (adp->controller->status & ATA_S_CORR)
	diskerr(request->bp, "soft error (ECC corrected)",
		request->blockaddr + (request->donecount / DEV_BSIZE),
		&adp->disk.d_label);

    /* did any real errors happen ? */
    if ((adp->controller->status & ATA_S_ERROR) ||
	(request->flags & ADR_F_DMA_USED && dma_stat & ATA_BMSTAT_ERROR)) {
	adp->controller->error = ATA_INB(adp->controller->r_io, ATA_ERROR);
	diskerr(request->bp,
		(adp->controller->error & ATA_E_ICRC) ?
			"UDMA ICRC error" : "hard error",
		request->blockaddr + (request->donecount / DEV_BSIZE),
		&adp->disk.d_label);

	/* if this is a UDMA CRC error, reinject request */
	if (request->flags & ADR_F_DMA_USED &&
	    adp->controller->error & ATA_E_ICRC) {
	    untimeout((timeout_t *)ad_timeout, request,request->timeout_handle);
	    ad_invalidatequeue(adp, request);

	    if (request->retries++ < AD_MAX_RETRIES)
		printf(" retrying\n");
	    else {
		ata_dmainit(adp->controller, adp->unit, 
			    ata_pmode(AD_PARAM), -1, -1);
		printf(" falling back to PIO mode\n");
	    }
	    TAILQ_INSERT_HEAD(&adp->controller->ata_queue, request, chain);
	    return ATA_OP_FINISHED;
	}

	/* if using DMA, try once again in PIO mode */
	if (request->flags & ADR_F_DMA_USED) {
	    untimeout((timeout_t *)ad_timeout, request,request->timeout_handle);
	    ad_invalidatequeue(adp, request);
	    ata_dmainit(adp->controller, adp->unit,
	    		ata_pmode(AD_PARAM), -1, -1);
	    request->flags |= ADR_F_FORCE_PIO;
	    TAILQ_INSERT_HEAD(&adp->controller->ata_queue, request, chain);
	    return ATA_OP_FINISHED;
	}

	request->flags |= ADR_F_ERROR;
	printf(" status=%02x error=%02x\n", 
	       adp->controller->status, adp->controller->error);
    }

    /* if we arrived here with forced PIO mode, DMA doesn't work right */
    if (request->flags & ADR_F_FORCE_PIO)
	ata_printf(adp->controller, adp->unit,
		   "DMA problem fallback to PIO mode\n");

    /* if this was a PIO read operation, get the data */
    if (!(request->flags & ADR_F_DMA_USED) &&
	(request->flags & (ADR_F_READ | ADR_F_ERROR)) == ADR_F_READ) {

	/* ready to receive data? */
	if ((adp->controller->status & (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))
	    != (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))
	    ata_printf(adp->controller, adp->unit,
		       "read interrupt arrived early");

	if (ata_wait(adp->controller, adp->unit, 
		     (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) != 0) {
	    ata_printf(adp->controller, adp->unit,
		       "read error detected (too) late");
	    request->flags |= ADR_F_ERROR;
	}
	else {
	    /* data ready, read in */
	    if (adp->controller->flags & ATA_USE_16BIT)
		ATA_INSW(adp->controller->r_io, ATA_DATA,
		    (void *)((uintptr_t)request->data + request->donecount), 
		    request->currentsize / sizeof(int16_t));
	    else
		ATA_INSL(adp->controller->r_io, ATA_DATA,
		    (void *)((uintptr_t)request->data + request->donecount), 
		    request->currentsize / sizeof(int32_t));
	}
    }

    /* finish up transfer */
    if (request->flags & ADR_F_ERROR) {
	request->bp->bio_error = EIO;
	request->bp->bio_flags |= BIO_ERROR;
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

    request->bp->bio_resid = request->bytecount;

#ifdef ATA_FLUSHCACHE_ON 
    if (request->bp->bio_flags & BIO_ORDERED) {
	request->flags |= ADR_F_FLUSHCACHE;
	if (ata_command(adp->controller, adp->unit, ATA_C_FLUSHCACHE,
			0, 0, 0, 0, 0, ATA_IMMEDIATE))
	    ata_printf(adp->controller, adp->unit, "flushing cache failed\n");
	else
	    return ATA_OP_CONTINUES;
    }
#endif
finish:
    devstat_end_transaction_bio(&adp->stats, request->bp);
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
    if (adp->controller->flags & ATA_QUEUED && change) {
	int device = adp->unit;

	if (adp->unit == ATA_MASTER) {
	    if (adp->controller->devices & ATA_ATA_SLAVE &&
	        ((struct ad_softc *)
		 (adp->controller->dev_softc[ATA_DEV(ATA_SLAVE)]))->flags & 
		AD_F_TAG_ENABLED)  
		device = ATA_SLAVE;
	}
	else {
	    if (adp->controller->devices & ATA_ATA_MASTER &&
	        ((struct ad_softc *)
		 (adp->controller->dev_softc[ATA_DEV(ATA_MASTER)]))->flags & 
		AD_F_TAG_ENABLED)  
		device = ATA_MASTER;
	}
	if (device != adp->unit &&
	    ((struct ad_softc *)
	     (adp->controller->dev_softc[ATA_DEV(device)]))->outstanding > 0) {
	    ATA_OUTB(adp->controller->r_io, ATA_DRIVE, ATA_D_IBM | device);
	    adp = adp->controller->dev_softc[ATA_DEV(device)];
	    DELAY(1);
	}
    }
    adp->controller->status = ATA_INB(adp->controller->r_altio, ATA_ALTSTAT);
 
    /* do we have a SERVICE request from the drive ? */
    if (adp->flags & AD_F_TAG_ENABLED &&
	adp->outstanding > 0 &&
	adp->controller->status & ATA_S_SERVICE) {
	struct ad_request *request;
	int tag;

	/* check for error */
	if (adp->controller->status & ATA_S_ERROR) {
	    ata_printf(adp->controller, adp->unit,
		       "Oops! controller says s=0x%02x e=0x%02x\n",
		       adp->controller->status,  adp->controller->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}

	/* issue SERVICE cmd */
	if (ata_command(adp->controller, adp->unit, ATA_C_SERVICE, 
	    		0, 0, 0, 0, 0, ATA_IMMEDIATE)) {
	    ata_printf(adp->controller, adp->unit,
		       "problem executing SERVICE cmd\n");
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}

	/* setup the transfer environment when ready */
	if (ata_wait(adp->controller, adp->unit, ATA_S_READY)) {
	    ata_printf(adp->controller, adp->unit,
		       "problem issueing SERVICE tag=%d s=0x%02x e=0x%02x\n",
		       ATA_INB(adp->controller->r_io, ATA_COUNT) >> 3,
		       adp->controller->status, adp->controller->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	tag = ATA_INB(adp->controller->r_io, ATA_COUNT) >> 3;
	if (!(request = adp->tags[tag])) {
	    ata_printf(adp->controller, adp->unit,
		       "no request for tag=%d\n", tag);	
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	adp->controller->active = ATA_ACTIVE_ATA;
	adp->controller->running = request;
	request->serv++;

	/* start DMA transfer when ready */
	if (ata_wait(adp->controller, adp->unit, ATA_S_READY | ATA_S_DRQ)) {
	    ata_printf(adp->controller, adp->unit,
		       "timeout waiting for data phase s=%02x e=%02x\n",
		       adp->controller->status, adp->controller->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	ata_dmastart(adp->controller, adp->unit,
		     request->dmatab, request->flags & ADR_F_READ);
	return ATA_OP_CONTINUES;
    }
    return ATA_OP_FINISHED;
}

static void
ad_free(struct ad_request *request)
{
    int s = splbio();

    if (request->dmatab)
	free(request->dmatab, M_DEVBUF);
    request->device->tags[request->tag] = NULL;
    free(request, M_AD);

    splx(s);
}

static void
ad_invalidatequeue(struct ad_softc *adp, struct ad_request *request)
{
    /* if tags used invalidate all other tagged transfers */
    if (adp->flags & AD_F_TAG_ENABLED) {
	struct ad_request *tmpreq;
	int tag;

        ata_printf(adp->controller, adp->unit,"invalidating queued requests\n");
	for (tag = 0; tag <= adp->num_tags; tag++) {
	    tmpreq = adp->tags[tag];
	    adp->tags[tag] = NULL;
	    if (tmpreq == request || tmpreq == NULL)
		continue;
    	    untimeout((timeout_t *)ad_timeout, tmpreq, tmpreq->timeout_handle);
	    TAILQ_INSERT_HEAD(&adp->controller->ata_queue, tmpreq, chain);
	}
	if (ata_command(adp->controller, adp->unit, ATA_C_NOP,
			0, 0, 0, 0, ATA_C_F_FLUSHQUEUE, ATA_WAIT_READY))
	    ata_printf(adp->controller, adp->unit, "flush queue failed\n");
	adp->outstanding = 0;
    }
}

static int
ad_tagsupported(struct ad_softc *adp)
{
    const char *drives[] = {"IBM-DPTA", "IBM-DTLA", NULL};
    int i = 0;

    /* Promise controllers doesn't work with tagged queuing */
    if ((adp->controller->chiptype & 0x0000ffff) == 0x0000105a)
	return 0;

    /* check that drive does DMA, has tags enabled, and is one we know works */
    if (adp->controller->mode[ATA_DEV(adp->unit)] >= ATA_DMA &&
	AD_PARAM->supqueued && AD_PARAM->enabqueued) {
	while (drives[i] != NULL) {
	    if (!strncmp(AD_PARAM->model, drives[i], strlen(drives[i])))
		return 1;
	    i++;
	}
    }
    return 0;
}

static void
ad_timeout(struct ad_request *request)
{
    struct ad_softc *adp = request->device;
    int s = splbio();

    adp->controller->running = NULL;
    ata_printf(adp->controller, adp->unit,
	       "%s command timeout tag=%d serv=%d - resetting\n",
	       (request->flags & ADR_F_READ) ? "READ" : "WRITE",
	       request->tag, request->serv);

    if (request->flags & ADR_F_DMA_USED) {
	ata_dmadone(adp->controller);
	ad_invalidatequeue(adp, request);
        if (request->retries == AD_MAX_RETRIES) {
	    ata_dmainit(adp->controller, adp->unit,
	    		ata_pmode(AD_PARAM), -1, -1);
	    ata_printf(adp->controller, adp->unit,
		       "trying fallback to PIO mode\n");
	    request->retries = 0;
	}
    }

    /* if retries still permit, reinject this request */
    if (request->retries++ < AD_MAX_RETRIES) {
	TAILQ_INSERT_HEAD(&adp->controller->ata_queue, request, chain);
    }
    else {
	/* retries all used up, return error */
	request->bp->bio_error = EIO;
	request->bp->bio_flags |= BIO_ERROR;
	devstat_end_transaction_bio(&adp->stats, request->bp);
	biodone(request->bp);
	ad_free(request);
    }
    ata_reinit(adp->controller);
    splx(s);
}

void
ad_reinit(struct ad_softc *adp)
{
    /* reinit disk parameters */
    ad_invalidatequeue(adp, NULL);
    ata_command(adp->controller, adp->unit, ATA_C_SET_MULTI, 0, 0, 0,
		adp->transfersize / DEV_BSIZE, 0, ATA_WAIT_INTR);
    if (adp->controller->mode[ATA_DEV(adp->unit)] >= ATA_DMA)
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM),
		    ata_wmode(AD_PARAM), ata_umode(AD_PARAM));
    else
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM), -1, -1);
}

void
ad_print(struct ad_softc *adp, char *prepend) 
{
    if (prepend)
	printf("%s", prepend);
    if (bootverbose) {
	ata_printf(adp->controller, adp->unit,
		   "<%.40s/%.8s> ATA-%d disk at ata%d-%s\n", 
		   AD_PARAM->model, AD_PARAM->revision,
		   ad_version(AD_PARAM->versmajor), 
		   device_get_unit(adp->controller->dev),
		   (adp->unit == ATA_MASTER) ? "master" : "slave");

	ata_printf(adp->controller, adp->unit,
		   "%luMB (%u sectors), %u C, %u H, %u S, %u B\n",
		   adp->total_secs / ((1024L*1024L)/DEV_BSIZE), adp->total_secs,
		   adp->total_secs / (adp->heads * adp->sectors),
		   adp->heads, adp->sectors, DEV_BSIZE);

	ata_printf(adp->controller, adp->unit,
		   "%d secs/int, %d depth queue, %s%s\n", 
		   adp->transfersize / DEV_BSIZE, adp->num_tags + 1,
		   (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
		   ata_mode2str(adp->controller->mode[ATA_DEV(adp->unit)]));

	ata_printf(adp->controller, adp->unit,
		   "piomode=%d dmamode=%d udmamode=%d cblid=%d\n",
		   ata_pmode(AD_PARAM), ata_wmode(AD_PARAM), 
		   ata_umode(AD_PARAM), AD_PARAM->cblid);

    }
    else
	ata_printf(adp->controller, adp->unit,
		   "%luMB <%.40s> [%d/%d/%d] at ata%d-%s %s%s\n",
		   adp->total_secs / ((1024L * 1024L) / DEV_BSIZE),
		   AD_PARAM->model, adp->total_secs / (adp->heads*adp->sectors),
		   adp->heads, adp->sectors,
		   device_get_unit(adp->controller->dev),
		   (adp->unit == ATA_MASTER) ? "master" : "slave",
		   (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
		   ata_mode2str(adp->controller->mode[ATA_DEV(adp->unit)]));
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
