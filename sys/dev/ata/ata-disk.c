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

#include "apm.h"
#include "opt_global.h"
#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/cons.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/bus.h>
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
	/* bmaj */	30
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
MALLOC_DEFINE(M_AD, "AD driver", "ATA disk driver");

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


    if (!(adp = malloc(sizeof(struct ad_softc), M_AD, M_NOWAIT))) {
	ata_printf(scp, device, "failed to allocate driver storage\n");
	return;
    }
    bzero(adp, sizeof(struct ad_softc));
    scp->dev_softc[ATA_DEV(device)] = adp;
    adp->controller = scp;
    adp->unit = device;
#ifdef ATA_STATIC_ID
    adp->lun = (device_get_unit(scp->dev) << 1) + ATA_DEV(device);
#else
    adp->lun = ata_get_lun(&adp_lun_map);
#endif
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

    /* enable read/write cacheing if not default on device */
    if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
		    0, 0, 0, 0, ATA_C_F_ENAB_RCACHE, ATA_WAIT_INTR))
	printf("ad%d: enabling readahead cache failed\n", adp->lun);

    if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
		    0, 0, 0, 0, ATA_C_F_ENAB_WCACHE, ATA_WAIT_INTR))
	printf("ad%d: enabling write cache failed\n", adp->lun);

    /* use DMA if drive & controller supports it */
    ata_dmainit(adp->controller, adp->unit,
    		ata_pmode(AD_PARAM), ata_wmode(AD_PARAM), ata_umode(AD_PARAM));

    /* use tagged queueing if supported */
    if (ad_tagsupported(adp)) {
	adp->num_tags = AD_PARAM->queuelen;
	adp->flags |= AD_F_TAG_ENABLED;
	adp->controller->flags |= ATA_QUEUED;
	if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			0, 0, 0, 0, ATA_C_F_DIS_RELIRQ, ATA_WAIT_INTR))
	    printf("ad%d: disabling release interrupt failed\n", adp->lun);
	if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			0, 0, 0, 0, ATA_C_F_DIS_SRVIRQ, ATA_WAIT_INTR))
	    printf("ad%d: disabling service interrupt failed\n", adp->lun);
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

    if (bootverbose) {
	printf("ad%d: <%.40s/%.8s> ATA-%d disk at ata%d-%s\n", 
	       adp->lun, AD_PARAM->model, AD_PARAM->revision,
	       ad_version(AD_PARAM->versmajor), device_get_unit(scp->dev),
	       (adp->unit == ATA_MASTER) ? "master" : "slave");

	 printf("ad%d: %luMB (%u sectors), %u cyls, %u heads, %u S/T, %u B/S\n",
	       adp->lun, adp->total_secs / ((1024L * 1024L)/DEV_BSIZE),
	       adp->total_secs, 
	       adp->total_secs / (adp->heads * adp->sectors),
	       adp->heads, adp->sectors, DEV_BSIZE);

	printf("ad%d: %d secs/int, %d depth queue, %s%s\n", 
	       adp->lun, adp->transfersize / DEV_BSIZE, adp->num_tags + 1,
	       (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
	       ata_mode2str(adp->controller->mode[ATA_DEV(adp->unit)]));

	printf("ad%d: piomode=%d dmamode=%d udmamode=%d cblid=%d\n",
	       adp->lun, ata_pmode(AD_PARAM), ata_wmode(AD_PARAM), 
	       ata_umode(AD_PARAM), AD_PARAM->cblid);

    }

    /* if this disk belongs to an ATA RAID dont print the probe */
    if (ar_probe(adp))
	printf("ad%d: %luMB <%.40s> [%d/%d/%d] at ata%d-%s %s%s\n",
	       adp->lun, adp->total_secs / ((1024L * 1024L) / DEV_BSIZE),
	       AD_PARAM->model, adp->total_secs / (adp->heads * adp->sectors),
	       adp->heads, adp->sectors, device_get_unit(scp->dev),
	       (adp->unit == ATA_MASTER) ? "master" : "slave",
	       (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
	       ata_mode2str(adp->controller->mode[ATA_DEV(adp->unit)]));
}

void
ad_detach(struct ad_softc *adp)
{
    disk_invalidate(&adp->disk);
    disk_destroy(adp->dev);
    devstat_remove_entry(&adp->stats);
    ata_free_lun(&adp_lun_map, adp->lun);
    free(adp, M_AD);
}

static int
adopen(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct ad_softc *adp = dev->si_drv1;
    struct disklabel *dl;

    dl = &adp->disk.d_label;
    bzero(dl, sizeof *dl);
    dl->d_secsize = DEV_BSIZE;
    dl->d_nsectors = adp->sectors;
    dl->d_ntracks = adp->heads;
    dl->d_ncylinders = adp->total_secs / (adp->heads * adp->sectors);
    dl->d_secpercyl = adp->sectors * adp->heads;
    dl->d_secperunit = adp->total_secs;
    return 0;
}

static void 
adstrategy(struct bio *bp)
{
    struct ad_softc *adp = bp->bio_dev->si_drv1;
    int s;

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

	if (addr % (1024 * 1024) == 0) {
#ifdef HW_WDOG
	    if (wdog_tickler)
		(*wdog_tickler)();
#endif
	    printf("%ld ", (long)(count * DEV_BSIZE) / (1024 * 1024));
	}

	blkno += blkcnt * dumppages;
	count -= blkcnt * dumppages;
	addr += PAGE_SIZE * dumppages;
	if (cncheckc() != -1)
	    return EINTR;
    }

    if (ata_wait(adp->controller, adp->unit, ATA_S_READY | ATA_S_DSC) < 0)
	printf("ad%d: timeout waiting for final ready\n", adp->lun);

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

    if (!(request = malloc(sizeof(struct ad_request), M_AD, M_NOWAIT))) {
	printf("ad%d: out of memory in start\n", adp->lun);
	return;
    }

    /* setup request */
    bzero(request, sizeof(struct ad_request));
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
	    printf("ad%d: count %d size transfers not supported\n",
		   adp->lun, count);
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
		    printf("ad%d: error executing command", adp->lun);
		    goto transfer_failed;
		}
		if (ata_wait(adp->controller, adp->unit, ATA_S_READY)) {
		    printf("ad%d: timeout waiting for READY\n", adp->lun);
		    goto transfer_failed;
		}
		adp->outstanding++;

		/* if ATA bus RELEASE check for SERVICE */
		if (adp->flags & AD_F_TAG_ENABLED &&
		    inb(adp->controller->ioaddr + ATA_IREASON) & ATA_I_RELEASE){
		    return ad_service(adp, 1);
		}
	    }
	    else {
	    	cmd = (request->flags & ADR_F_READ) ?
		    ATA_C_READ_DMA : ATA_C_WRITE_DMA;

		if (ata_command(adp->controller, adp->unit, cmd, cylinder, 
		    		head, sector, count, 0, ATA_IMMEDIATE)) {
		    printf("ad%d: error executing command", adp->lun);
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
		    printf("ad%d: timeout waiting for data phase\n", adp->lun);
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
	    printf("ad%d: error executing command", adp->lun);
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
	printf("ad%d: timeout waiting for DRQ", adp->lun);
	goto transfer_failed;
    }

    /* output the data */
    if (adp->controller->flags & ATA_USE_16BIT)
	outsw(adp->controller->ioaddr + ATA_DATA,
	      (void *)((uintptr_t)request->data + request->donecount),
	      request->currentsize / sizeof(int16_t));
    else
	outsl(adp->controller->ioaddr + ATA_DATA,
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
	    printf("ad%d: soft error ECC corrected\n", adp->lun); 

    /* did any real errors happen ? */
    if ((adp->controller->status & ATA_S_ERROR) ||
	(request->flags & ADR_F_DMA_USED && dma_stat & ATA_BMSTAT_ERROR)) {
	adp->controller->error = inb(adp->controller->ioaddr + ATA_ERROR);
	printf("ad%d: %s %s ERROR blk# %d", adp->lun,
	       (adp->controller->error & ATA_E_ICRC) ? "UDMA ICRC" : "HARD",
	       (request->flags & ADR_F_READ) ? "READ" : "WRITE",
	       request->blockaddr + (request->donecount / DEV_BSIZE)); 

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
	printf("ad%d: DMA problem fallback to PIO mode\n", adp->lun);

    /* if this was a PIO read operation, get the data */
    if (!(request->flags & ADR_F_DMA_USED) &&
	(request->flags & (ADR_F_READ | ADR_F_ERROR)) == ADR_F_READ) {

	/* ready to receive data? */
	if ((adp->controller->status & (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))
	    != (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))
	    printf("ad%d: read interrupt arrived early", adp->lun);

	if (ata_wait(adp->controller, adp->unit, 
		     (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ)) != 0) {
	    printf("ad%d: read error detected (too) late", adp->lun);
	    request->flags |= ADR_F_ERROR;
	}
	else {
	    /* data ready, read in */
	    if (adp->controller->flags & ATA_USE_16BIT)
		insw(adp->controller->ioaddr + ATA_DATA,
		     (void *)((uintptr_t)request->data + request->donecount), 
		     request->currentsize / sizeof(int16_t));
	    else
		insl(adp->controller->ioaddr + ATA_DATA,
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
	    printf("ad%d: flushing cache failed\n", adp->lun);
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
	    outb(adp->controller->ioaddr + ATA_DRIVE, ATA_D_IBM | device);
	    adp = adp->controller->dev_softc[ATA_DEV(device)];
	    DELAY(1);
	}
    }
    adp->controller->status = inb(adp->controller->altioaddr);
 
    /* do we have a SERVICE request from the drive ? */
    if (adp->flags & AD_F_TAG_ENABLED &&
	adp->outstanding > 0 &&
	adp->controller->status & ATA_S_SERVICE) {
	struct ad_request *request;
	int tag;

	/* check for error */
	if (adp->controller->status & ATA_S_ERROR) {
	    printf("ad%d: Oops! controller says s=0x%02x e=0x%02x\n",
	           adp->lun, adp->controller->status,  adp->controller->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}

	/* issue SERVICE cmd */
	if (ata_command(adp->controller, adp->unit, ATA_C_SERVICE, 
	    		0, 0, 0, 0, 0, ATA_IMMEDIATE)) {
	    printf("ad%d: problem executing SERVICE cmd\n", adp->lun);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}

	/* setup the transfer environment when ready */
	if (ata_wait(adp->controller, adp->unit, ATA_S_READY)) {
	    printf("ad%d: problem issueing SERVICE tag=%d s=0x%02x e=0x%02x\n",
	           adp->lun, inb(adp->controller->ioaddr + ATA_COUNT) >> 3,
		   adp->controller->status, adp->controller->error);
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	tag = inb(adp->controller->ioaddr + ATA_COUNT) >> 3;
	if (!(request = adp->tags[tag])) {
	    printf("ad%d: no request for this tag=%d??\n", adp->lun, tag);	
	    ad_invalidatequeue(adp, NULL);
	    return ATA_OP_FINISHED;
	}
	adp->controller->active = ATA_ACTIVE_ATA;
	adp->controller->running = request;
	request->serv++;

	/* start DMA transfer when ready */
	if (ata_wait(adp->controller, adp->unit, ATA_S_READY | ATA_S_DRQ)) {
	    printf("ad%d: timeout waiting for data phase s=%02x e=%02x\n",
		   adp->lun, adp->controller->status, adp->controller->error);
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

        printf("ad%d: invalidating queued requests\n", adp->lun);
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
	    printf("ad%d: flushing queue failed\n", adp->lun);
	adp->outstanding = 0;
    }
}

static int
ad_tagsupported(struct ad_softc *adp)
{
#ifdef ATA_ENABLE_TAGS
    const char *drives[] = {"IBM-DPTA", "IBM-DTLA", NULL};
    int i = 0;

    /* Promise controllers doesn't work with tagged queuing */
    if ((adp->controller->chiptype & 0x0000ffff) == 0x0000105a)
	return 0;

    /* check that drive has tags enabled, and is one we know works */
    if (AD_PARAM->supqueued && AD_PARAM->enabqueued) {
	while (drives[i] != NULL) {
	    if (!strncmp(AD_PARAM->model, drives[i], strlen(drives[i])))
		return 1;
	    i++;
	}
    }
#endif
    return 0;
}

static void
ad_timeout(struct ad_request *request)
{
    struct ad_softc *adp = request->device;
    int s = splbio();

    adp->controller->running = NULL;
    printf("ad%d: %s command timeout tag=%d serv=%d - resetting\n",
	   adp->lun, (request->flags & ADR_F_READ) ? "READ" : "WRITE",
	   request->tag, request->serv);

    if (request->flags & ADR_F_DMA_USED) {
	ata_dmadone(adp->controller);
	ad_invalidatequeue(adp, request);
        if (request->retries == AD_MAX_RETRIES) {
	    ata_dmainit(adp->controller, adp->unit,
	    		ata_pmode(AD_PARAM), -1, -1);
	    printf("ad%d: trying fallback to PIO mode\n", adp->lun);
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
		adp->transfersize / DEV_BSIZE, 0, ATA_WAIT_READY);
    if (adp->controller->mode[ATA_DEV(adp->unit)] >= ATA_DMA)
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM),
		    ata_wmode(AD_PARAM), ata_umode(AD_PARAM));
    else
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM), -1, -1);
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
