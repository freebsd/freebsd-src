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
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/cons.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>

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
static void ad_timeout(struct ad_request *);
static int32_t ad_version(u_int16_t);

/* internal vars */
static u_int32_t adp_lun_map = 0;
MALLOC_DEFINE(M_AD, "AD driver", "ATA disk driver");

/* defines */
#define	AD_MAX_RETRIES	3
#define AD_PARAM	ATA_PARAM(adp->controller, adp->unit)

void
ad_attach(struct ata_softc *scp, int32_t device)
{
    struct ad_softc *adp;
    dev_t dev;
    int32_t secsperint;


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
    
    if (AD_PARAM->atavalid & ATA_FLAG_54_58 && AD_PARAM->lbasize)
	adp->flags |= AD_F_LBA_ENABLED;

    /* use multiple sectors/interrupt if device supports it */
    adp->transfersize = DEV_BSIZE;
    if (ad_version(AD_PARAM->versmajor)) {
	secsperint = max(1, min(AD_PARAM->nsecperint, 16));
	if (!ata_command(adp->controller, adp->unit, ATA_C_SET_MULTI,
			 0, 0, 0, secsperint, 0, ATA_WAIT_INTR) &&
            ata_wait(adp->controller, adp->unit, ATA_S_READY) >= 0)
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
    ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM),
		ata_wmode(AD_PARAM), ata_umode(AD_PARAM));

    /* use tagged queueing if supported (not yet) */
    if ((adp->num_tags = (AD_PARAM->queuelen & 0x1f) + 1))
	adp->flags |= AD_F_TAG_ENABLED;

    if (bootverbose) {
	printf("ad%d: <%.40s/%.8s> ATA-%d disk at ata%d as %s\n", 
	       adp->lun, AD_PARAM->model, AD_PARAM->revision,
	       ad_version(AD_PARAM->versmajor), device_get_unit(scp->dev),
	       (adp->unit == ATA_MASTER) ? "master" : "slave");

	 printf("ad%d: %luMB (%u sectors), %u cyls, %u heads, %u S/T, %u B/S\n",
	       adp->lun, adp->total_secs / ((1024L * 1024L)/DEV_BSIZE),
	       adp->total_secs, 
	       adp->total_secs / (adp->heads * adp->sectors),
	       adp->heads, adp->sectors, DEV_BSIZE);

	printf("ad%d: %d secs/int, %d depth queue, %s\n", 
	       adp->lun, adp->transfersize / DEV_BSIZE, adp->num_tags,
	       ata_mode2str(adp->controller->mode[ATA_DEV(adp->unit)]));

	printf("ad%d: piomode=%d dmamode=%d udmamode=%d cblid=%d\n",
	       adp->lun, ata_pmode(AD_PARAM), ata_wmode(AD_PARAM), 
	       ata_umode(AD_PARAM), AD_PARAM->cblid);

    }
    else
	printf("ad%d: %luMB <%.40s> [%d/%d/%d] at ata%d-%s using %s\n",
	       adp->lun, adp->total_secs / ((1024L * 1024L) / DEV_BSIZE),
	       AD_PARAM->model, adp->total_secs / (adp->heads * adp->sectors),
	       adp->heads, adp->sectors, device_get_unit(scp->dev),
	       (adp->unit == ATA_MASTER) ? "master" : "slave",
	       ata_mode2str(adp->controller->mode[ATA_DEV(adp->unit)]));

    devstat_add_entry(&adp->stats, "ad", adp->lun, DEV_BSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
		      DEVSTAT_PRIORITY_DISK);

    dev = disk_create(adp->lun, &adp->disk, 0, &ad_cdevsw, &addisk_cdevsw);
    dev->si_drv1 = adp;
    dev->si_iosize_max = 256 * DEV_BSIZE;
    adp->dev1 = dev;

    bioq_init(&adp->queue);
}

void
ad_detach(struct ad_softc *adp)
{
    disk_invalidate(&adp->disk);
    disk_destroy(adp->dev1);
    devstat_remove_entry(&adp->stats);
    ata_free_lun(&adp_lun_map, adp->lun);
    free(adp, M_AD);
}

static int
adopen(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
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
    int32_t s;

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
    int error;

    if ((error = disk_dumpcheck(dev, &count, &blkno, &secsize)))
	return error;
	
    if (!adp)
	return ENXIO;

    /* force PIO mode for dumps */
    adp->controller->mode[ATA_DEV(adp->unit)] = ATA_PIO;
    ata_reinit(adp->controller);

    while (count > 0) {
	void *va;
	DELAY(1000);
	if (is_physical_memory(addr))
	    va = pmap_kenter_temporary(trunc_page(addr));
	else
	    va = pmap_kenter_temporary(trunc_page(0));

	bzero(&request, sizeof(struct ad_request));
	request.device = adp;
	request.blockaddr = blkno;
	request.bytecount = PAGE_SIZE;
	request.data = (int8_t *) va;

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

	blkno += howmany(PAGE_SIZE, secsize);
	count -= howmany(PAGE_SIZE, secsize);
	addr += PAGE_SIZE;
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

    if (!bp)
	return;

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
    request->flags = (bp->bio_cmd == BIO_READ) ? ADR_F_READ : 0;

    /* remove from drive queue */
    bioq_remove(&adp->queue, bp); 

    /* link onto controller queue */
    TAILQ_INSERT_TAIL(&adp->controller->ata_queue, request, chain);
}

void
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
	if (panicstr)
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
	if ((adp->controller->mode[ATA_DEV(adp->unit)] >= ATA_DMA) &&
	    !ata_dmasetup(adp->controller, adp->unit, 
			  (void *)request->data, request->bytecount,
			  (request->flags & ADR_F_READ))) {
	    request->flags |= ADR_F_DMA_USED;
	    cmd = request->flags&ADR_F_READ ? ATA_C_READ_DMA : ATA_C_WRITE_DMA;
	    request->currentsize = request->bytecount;
	}

	/* does this drive support multi sector transfers ? */
	else if (request->currentsize > DEV_BSIZE)
	    cmd = request->flags&ADR_F_READ ? ATA_C_READ_MUL : ATA_C_WRITE_MUL;

	/* just plain old single sector transfer */
	else
	    cmd = request->flags&ADR_F_READ ? ATA_C_READ : ATA_C_WRITE;

	if (ata_command(adp->controller, adp->unit, cmd, 
			cylinder, head, sector, count, 0, ATA_IMMEDIATE)) {
	    printf("ad%d: error executing command", adp->lun);
	    goto transfer_failed;
	}

	/* if this is a DMA transfer, start it, return and wait for interrupt */
	if (request->flags & ADR_F_DMA_USED) {
	    ata_dmastart(adp->controller);
	    return;
	}
    }
   
    /* calculate this transfer length */
    request->currentsize = min(request->bytecount, adp->transfersize);

    /* if this is a PIO read operation, return and wait for interrupt */
    if (request->flags & ADR_F_READ)
	return;

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
    return;

transfer_failed:
    untimeout((timeout_t *)ad_timeout, request, request->timeout_handle);
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
	free(request, M_AD);
    }
    ata_reinit(adp->controller);
}

int32_t
ad_interrupt(struct ad_request *request)
{
    struct ad_softc *adp = request->device;
    int32_t dma_stat = 0;

    /* finish DMA transfer */
    if (request->flags & ADR_F_DMA_USED)
	dma_stat = ata_dmadone(adp->controller);

    /* get drive status */
    if (ata_wait(adp->controller, adp->unit, 0) < 0) {
	printf("ad%d: timeout waiting for status", adp->lun);
	request->flags |= ADR_F_ERROR;
    }

    /* do we have a corrected soft error ? */
    if (adp->controller->status & ATA_S_CORR)
	    printf("ad%d: soft error ECC corrected\n", adp->lun); 

    /* did any real errors happen ? */
    if ((adp->controller->status & ATA_S_ERROR) ||
	((request->flags & ADR_F_DMA_USED) && (dma_stat & ATA_BMSTAT_ERROR))) {
	printf("ad%d: %s %s ERROR blk# %d", adp->lun,
	       (adp->controller->error & ATA_E_ICRC) ? "UDMA ICRC" : "HARD",
	       (request->flags & ADR_F_READ) ? "READ" : "WRITE",
	       request->blockaddr + (request->donecount / DEV_BSIZE)); 

	/* if this is a UDMA CRC error, reinject request */
	if (request->flags & ADR_F_DMA_USED &&
	    adp->controller->error & ATA_E_ICRC) {
	    untimeout((timeout_t *)ad_timeout, request,request->timeout_handle);

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
	    ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM), -1,-1);
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
	((request->flags & (ADR_F_READ | ADR_F_ERROR)) == ADR_F_READ)) {

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
    devstat_end_transaction_bio(&adp->stats, request->bp);
    biodone(request->bp);
    free(request, M_AD);
    return ATA_OP_FINISHED;
}

void
ad_reinit(struct ad_softc *adp)
{
    /* reinit disk parameters */
    ata_command(adp->controller, adp->unit, ATA_C_SET_MULTI, 0, 0, 0,
		adp->transfersize / DEV_BSIZE, 0, ATA_WAIT_READY);
    if (adp->controller->mode[ATA_DEV(adp->unit)] >= ATA_DMA)
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM),
		    ata_wmode(AD_PARAM), ata_umode(AD_PARAM));
    else
	ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM), -1, -1);
}

static void
ad_timeout(struct ad_request *request)
{
    struct ad_softc *adp = request->device;
    int32_t s = splbio();

    adp->controller->running = NULL;
    printf("ad%d: %s command timeout - resetting\n",
	   adp->lun, (request->flags & ADR_F_READ) ? "READ" : "WRITE");

    if (request->flags & ADR_F_DMA_USED) {
	ata_dmadone(adp->controller);
        if (request->retries == AD_MAX_RETRIES) {
	    ata_dmainit(adp->controller, adp->unit, ata_pmode(AD_PARAM), -1,-1);
	    printf("ad%d: trying fallback to PIO mode\n", adp->lun);
	    request->retries = 0;
	}
    }

    /* if retries still permit, reinject this request */
    if (request->retries++ < AD_MAX_RETRIES)
	TAILQ_INSERT_HEAD(&adp->controller->ata_queue, request, chain);
    else {
	/* retries all used up, return error */
	request->bp->bio_error = EIO;
	request->bp->bio_flags |= BIO_ERROR;
	devstat_end_transaction_bio(&adp->stats, request->bp);
	biodone(request->bp);
	free(request, M_AD);
    }
    ata_reinit(adp->controller);
    splx(s);
}

static int32_t
ad_version(u_int16_t version)
{
    int32_t bit;

    if (version == 0xffff)
	return 0;
    for (bit = 15; bit >= 0; bit--)
	if (version & (1<<bit))
	    return bit;
    return 0;
}
