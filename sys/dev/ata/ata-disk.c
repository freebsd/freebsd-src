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

#include "apm.h"
#include "opt_global.h"
#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#if NAPM > 0
#include <machine/apm_bios.h>
#endif
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>

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
	/* bmaj */	30,
};
static struct cdevsw addisk_cdevsw;
static struct cdevsw fakewd_cdevsw;
static struct cdevsw fakewddisk_cdevsw;

/* prototypes */
static void ad_attach(void *);
static int32_t ad_getparam(struct ad_softc *);
static void ad_start(struct ad_softc *);
static void ad_timeout(struct ad_request *);
static void ad_sleep(struct ad_softc *, int8_t *);
static int8_t ad_version(u_int16_t);
static void ad_drvinit(void);

/* internal vars */
static struct intr_config_hook *ad_attach_hook;
MALLOC_DEFINE(M_AD, "AD driver", "ATA disk driver");

/* defines */
#define	AD_MAX_RETRIES	5

static __inline int
apiomode(struct ata_params *ap)
{
    if (ap->atavalid & 2) {
	if (ap->apiomodes & 2) return 4;
	if (ap->apiomodes & 1) return 3;
    }	
    return -1; 
} 

static __inline int
wdmamode(struct ata_params *ap)
{
    if (ap->atavalid & 2) {
	if (ap->wdmamodes & 4) return 2;
	if (ap->wdmamodes & 2) return 1;
	if (ap->wdmamodes & 1) return 0;
    }
    return -1;
}

static __inline int
udmamode(struct ata_params *ap)
{
    if (ap->atavalid & 4) {
	if (ap->udmamodes & 0x10 && ap->cblid) return 4;
	if (ap->udmamodes & 0x08 && ap->cblid) return 3;
	if (ap->udmamodes & 0x04) return 2;
	if (ap->udmamodes & 0x02) return 1;
	if (ap->udmamodes & 0x01) return 0;
    }
    return -1;
}

static void
ad_attach(void *notused)
{
    struct ad_softc *adp;
    int32_t ctlr, dev, secsperint;
    int8_t model_buf[40+1];
    int8_t revision_buf[8+1];
    dev_t dev1;
    static int32_t adnlun = 0;

    /* now, run through atadevices and look for ATA disks */
    for (ctlr=0; ctlr<MAXATA; ctlr++) {
	if (!atadevices[ctlr]) continue;
	for (dev=0; dev<2; dev++) {
	    if (atadevices[ctlr]->devices & 
		(dev ? ATA_ATA_SLAVE : ATA_ATA_MASTER)) {
#ifdef ATA_STATIC_ID
		adnlun = dev + ctlr * 2;   
#endif
		if (!(adp = malloc(sizeof(struct ad_softc), 
				   M_AD, M_NOWAIT))) {
		    printf("ad%d: failed to allocate driver storage\n", adnlun);
		    continue;
		}
		bzero(adp, sizeof(struct ad_softc));
		adp->controller = atadevices[ctlr];
		adp->unit = (dev == 0) ? ATA_MASTER : ATA_SLAVE;
		adp->lun = adnlun++;
		if (ad_getparam(adp)) {
		    free(adp, M_AD);
		    continue;
		}
		adp->cylinders = adp->ata_parm->cylinders;
		adp->heads = adp->ata_parm->heads;
		adp->sectors = adp->ata_parm->sectors;
		adp->total_secs = adp->cylinders * adp->heads * adp->sectors;	
		if (adp->cylinders == 16383 && 
		    adp->total_secs < adp->ata_parm->lbasize) {
		    adp->total_secs = adp->ata_parm->lbasize;
		    adp->cylinders = adp->total_secs/(adp->heads*adp->sectors);
		}
		if (adp->ata_parm->atavalid & ATA_FLAG_54_58 &&
		    adp->ata_parm->lbasize)
		    adp->flags |= AD_F_LBA_ENABLED;

		/* use multiple sectors/interrupt if device supports it */
		adp->transfersize = DEV_BSIZE;
		secsperint = min(adp->ata_parm->nsecperint, 16);
		if (!ata_command(adp->controller, adp->unit, ATA_C_SET_MULTI,
				 0, 0, 0, secsperint, 0, ATA_WAIT_INTR) &&
		    ata_wait(adp->controller, adp->unit, ATA_S_READY) >= 0)
		    adp->transfersize *= secsperint;

		/* enable read/write cacheing if not default on device */
		if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			    0, 0, 0, 0, ATA_C_F_ENAB_RCACHE, ATA_WAIT_INTR))
		    printf("ad%d: enabling readahead cache failed\n", adp->lun);

		if (ata_command(adp->controller, adp->unit, ATA_C_SETFEATURES,
			    0, 0, 0, 0, ATA_C_F_ENAB_WCACHE, ATA_WAIT_INTR))
		    printf("ad%d: enabling write cache failed\n", adp->lun);

		/* use DMA if drive & controller supports it */
		if (!ata_dmainit(adp->controller, adp->unit,
				 apiomode(adp->ata_parm),
				 wdmamode(adp->ata_parm),
				 udmamode(adp->ata_parm)))
		    adp->flags |= AD_F_DMA_ENABLED;

		/* use tagged queueing if supported (not yet) */
		if ((adp->num_tags = adp->ata_parm->queuelen & 0x1f))
		    adp->flags |= AD_F_TAG_ENABLED;

		/* store our softc */
		adp->controller->dev_softc[(adp->unit==ATA_MASTER)?0:1] = adp;

		bpack(adp->ata_parm->model, model_buf, sizeof(model_buf));
		bpack(adp->ata_parm->revision, revision_buf, 
		      sizeof(revision_buf));

		if (bootverbose)
		    printf("ad%d: piomode=%d dmamode=%d udmamode=%d\n",
		           adp->lun, apiomode(adp->ata_parm),
		           wdmamode(adp->ata_parm), udmamode(adp->ata_parm));

		printf("ad%d: <%s/%s> ATA-%c disk at ata%d as %s\n", 
		       adp->lun, model_buf, revision_buf,
		       ad_version(adp->ata_parm->versmajor), ctlr,
		       (adp->unit == ATA_MASTER) ? "master" : "slave ");

		printf("ad%d: %luMB (%u sectors), "
		       "%u cyls, %u heads, %u S/T, %u B/S\n",
		       adp->lun, adp->total_secs / ((1024L * 1024L)/DEV_BSIZE),
		       adp->total_secs, adp->cylinders, adp->heads,
		       adp->sectors, DEV_BSIZE);

		printf("ad%d: %d secs/int, %d depth queue, %s\n", 
		       adp->lun, adp->transfersize / DEV_BSIZE, adp->num_tags,
		       ata_mode2str(adp->controller->mode[
				    (adp->unit == ATA_MASTER) ? 0 : 1]));

		devstat_add_entry(&adp->stats, "ad", adp->lun, DEV_BSIZE,
				  DEVSTAT_NO_ORDERED_TAGS,
				  DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
				  0x180);

		dev1 = disk_create(adp->lun, &adp->disk, 0, &ad_cdevsw, 
				   &addisk_cdevsw);
		dev1->si_drv1 = adp;
		dev1->si_iosize_max = 256 * DEV_BSIZE;

		dev1 = disk_create(adp->lun, &adp->disk, 0, &fakewd_cdevsw,
				   &fakewddisk_cdevsw);
		dev1->si_drv1 = adp;
		dev1->si_iosize_max = 256 * DEV_BSIZE;

		bufq_init(&adp->queue);
	    }
	}
    }
    config_intrhook_disestablish(ad_attach_hook);
}

static int32_t
ad_getparam(struct ad_softc *adp)
{
    struct ata_params *ata_parm;
    int8_t buffer[DEV_BSIZE];

    /* select drive */
    outb(adp->controller->ioaddr + ATA_DRIVE, ATA_D_IBM | adp->unit);
    DELAY(1);
    ata_command(adp->controller, adp->unit, ATA_C_ATA_IDENTIFY,
		0, 0, 0, 0, 0, ATA_WAIT_INTR);
    if (ata_wait(adp->controller, adp->unit,
		 ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))
	return -1;
    insw(adp->controller->ioaddr + ATA_DATA, buffer, 
	 sizeof(buffer)/sizeof(int16_t));
    ata_parm = malloc(sizeof(struct ata_params), M_AD, M_NOWAIT);
    if (!ata_parm) 
	return -1; 
    bcopy(buffer, ata_parm, sizeof(struct ata_params));
    bswap(ata_parm->model, sizeof(ata_parm->model));
    btrim(ata_parm->model, sizeof(ata_parm->model));
    bswap(ata_parm->revision, sizeof(ata_parm->revision));
    btrim(ata_parm->revision, sizeof(ata_parm->revision));
    adp->ata_parm = ata_parm;
    return 0;
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
    dl->d_ncylinders = adp->cylinders;
    dl->d_secpercyl = adp->sectors * adp->heads;
    dl->d_secperunit = adp->total_secs;
    ad_sleep(adp, "adop2");
    return 0;
}

static void 
adstrategy(struct buf *bp)
{
    struct ad_softc *adp = bp->b_dev->si_drv1;
    int32_t s;

#ifdef AD_DEBUG
    printf("adstrategy: entered count=%d\n", bp->b_bcount);
#endif
    s = splbio();
    bufqdisksort(&adp->queue, bp);
    ad_start(adp);
    splx(s);
#ifdef AD_DEBUG
    printf("adstrategy: leaving\n");
#endif
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

    adp->flags &= ~AD_F_DMA_ENABLED;

    while (count > 0) {
	DELAY(1000);
	if (is_physical_memory(addr))
	    pmap_enter(kernel_pmap, (vm_offset_t)CADDR1,
		       trunc_page(addr), VM_PROT_READ, TRUE);
	else
	    pmap_enter(kernel_pmap, (vm_offset_t)CADDR1,
		       trunc_page(0), VM_PROT_READ, TRUE);

	bzero(&request, sizeof(struct ad_request));
	request.device = adp;
	request.blockaddr = blkno;
	request.bytecount = PAGE_SIZE;
	request.data = CADDR1;

	while (request.bytecount > 0) {
	    ad_transfer(&request);
	    request.donecount += request.currentsize;
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
    }

    if (ata_wait(adp->controller, adp->unit, ATA_S_READY | ATA_S_DSC) < 0)
	printf("ad_dump: timeout waiting for final ready\n");

    return 0;
}

static void
ad_start(struct ad_softc *adp)
{
    struct buf *bp = bufq_first(&adp->queue);
    struct ad_request *request;

#ifdef AD_DEBUG
    printf("ad_start:\n");
#endif
    if (!bp)
	return;

    if (!(request = malloc(sizeof(struct ad_request), M_AD, M_NOWAIT))) {
	printf("ad_start: out of memory\n");
	return;
    }

    /* setup request */
    bzero(request, sizeof(struct ad_request));
    request->device = adp;
    request->bp = bp;
    request->blockaddr = bp->b_pblkno;
    request->bytecount = bp->b_bcount;
    request->data = bp->b_data;
    request->flags = (bp->b_flags & B_READ) ? AR_F_READ : 0;

    /* remove from drive queue */
    bufq_remove(&adp->queue, bp); 

    /* link onto controller queue */
    TAILQ_INSERT_TAIL(&adp->controller->ata_queue, request, chain);

    /* try to start controller */
    if (adp->controller->active == ATA_IDLE)
	ata_start(adp->controller);
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
   
#ifdef AD_DEBUG
    printf("ad_transfer: blkno=%d\n", blkno);
#endif
    if (request->donecount == 0) {

	/* start timeout for this transfer */
	if (panicstr)
	    request->timeout_handle.callout = NULL;
	else
	    request->timeout_handle = 
		timeout((timeout_t*)ad_timeout, request, 3*hz);

	/* setup transfer parameters */
	count = howmany(request->bytecount, DEV_BSIZE);
	if (count > 256) {
	    count = 256;
	    printf("ad_transfer: count=%d not supported\n", count);
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
	request->flags &= ~AR_F_DMA_USED;
	if ((adp->flags & AD_F_DMA_ENABLED) &&
	    !ata_dmasetup(adp->controller, adp->unit,
			  (void *)request->data, request->bytecount,
			  (request->flags & AR_F_READ))) {
	    request->flags |= AR_F_DMA_USED;
	    cmd = request->flags & AR_F_READ ? ATA_C_READ_DMA : ATA_C_WRITE_DMA;
	    request->currentsize = request->bytecount;
	}
	/* does this drive support multi sector transfers ? */
	else if (request->currentsize > DEV_BSIZE)
	    cmd = request->flags & AR_F_READ?ATA_C_READ_MULTI:ATA_C_WRITE_MULTI;
	else
	    cmd = request->flags & AR_F_READ ? ATA_C_READ : ATA_C_WRITE;

	ata_command(adp->controller, adp->unit, cmd, cylinder, head, 
		    sector, count, 0, ATA_IMMEDIATE);
    }
   
    /* if this is a DMA transaction start it, return and wait for interrupt */
    if (request->flags & AR_F_DMA_USED) {
	ata_dmastart(adp->controller);
#ifdef AD_DEBUG
	printf("ad_transfer: return waiting for DMA interrupt\n");
#endif
	return;
    }

    /* calculate this transfer length */
    request->currentsize = min(request->bytecount, adp->transfersize);

    /* if this is a PIO read operation, return and wait for interrupt */
    if (request->flags & AR_F_READ) {
#ifdef AD_DEBUG
	printf("ad_transfer: return waiting for PIO read interrupt\n");
#endif
	return;
    }

    /* ready to write PIO data ? */
    if (ata_wait(adp->controller, adp->unit, 
		 ATA_S_READY | ATA_S_DSC | ATA_S_DRQ) < 0) {
	printf("ad_transfer: timeout waiting for DRQ");
    }				    
    
    /* output the data */
#ifdef ATA_16BIT_ONLY
    outsw(adp->controller->ioaddr + ATA_DATA,
	  (void *)((uintptr_t)request->data + request->donecount),
	  request->currentsize / sizeof(int16_t));
#else
    outsl(adp->controller->ioaddr + ATA_DATA,
	  (void *)((uintptr_t)request->data + request->donecount),
	  request->currentsize / sizeof(int32_t));
#endif
    request->bytecount -= request->currentsize;
#ifdef AD_DEBUG
    printf("ad_transfer: return wrote data\n");
#endif
}

int32_t
ad_interrupt(struct ad_request *request)
{
    struct ad_softc *adp = request->device;
    int32_t dma_stat = 0;

    /* finish DMA transfer */
    if (request->flags & AR_F_DMA_USED)
	dma_stat = ata_dmadone(adp->controller);

    /* get drive status */
    if (ata_wait(adp->controller, adp->unit, 0) < 0)
	 printf("ad_interrupt: timeout waiting for status");
    if (adp->controller->status & (ATA_S_ERROR | ATA_S_CORR) ||
	(request->flags & AR_F_DMA_USED && dma_stat != ATA_BMSTAT_INTERRUPT)) {
oops:
	printf("ad%d: status=%02x error=%02x\n", 
	       adp->lun, adp->controller->status, adp->controller->error);
	if (adp->controller->status & ATA_S_ERROR) {
	    printf("ad_interrupt: hard error\n"); 
	    request->flags |= AR_F_ERROR;
	}
	if (adp->controller->status & ATA_S_CORR)
	    printf("ad_interrupt: soft error ECC corrected\n"); 
    }

    /* if this was a PIO read operation, get the data */
    if (!(request->flags & AR_F_DMA_USED) &&
	((request->flags & (AR_F_READ | AR_F_ERROR)) == AR_F_READ)) {

	/* ready to receive data? */
	if ((adp->controller->status & (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))
	    != (ATA_S_READY | ATA_S_DSC | ATA_S_DRQ))
	    printf("ad_interrupt: read interrupt arrived early");

	if (ata_wait(adp->controller, adp->unit,
		     ATA_S_READY | ATA_S_DSC | ATA_S_DRQ) != 0){
	    printf("ad_interrupt: read error detected late");
	    goto oops;	 
	}

	/* data ready, read in */
#ifdef ATA_16BIT_ONLY 
	insw(adp->controller->ioaddr + ATA_DATA,
	     (void *)((uintptr_t)request->data + request->donecount), 
	     request->currentsize / sizeof(int16_t));
#else
	insl(adp->controller->ioaddr + ATA_DATA,
	     (void *)((uintptr_t)request->data + request->donecount), 
	     request->currentsize / sizeof(int32_t));
#endif
	request->bytecount -= request->currentsize;
#ifdef AD_DEBUG
    printf("ad_interrupt: read in data\n");
#endif
    }

    /* if this was a DMA operation finish up */
    if ((request->flags & AR_F_DMA_USED) && !(request->flags & AR_F_ERROR))
	request->bytecount -= request->currentsize;

    /* finish up this tranfer, check for more work on this buffer */
    if (adp->controller->active == ATA_ACTIVE_ATA) {
	if (request->flags & AR_F_ERROR) {
	    request->bp->b_error = EIO;
	    request->bp->b_flags |= B_ERROR;
	} 
	else {
	    request->donecount += request->currentsize;
#ifdef AD_DEBUG
	    printf("ad_interrupt: %s cmd OK\n", 
		   (request->flags & AR_F_READ) ? "read" : "write");
#endif
	    if (request->bytecount > 0) {
		ad_transfer(request);
		return ATA_OP_CONTINUES;
	    }
	}

	request->bp->b_resid = request->bytecount;
	devstat_end_transaction_buf(&adp->stats, request->bp);
	biodone(request->bp);
    }
    /* disarm timeout for this transfer */
    untimeout((timeout_t *)ad_timeout, request, request->timeout_handle);

    free(request, M_AD);
    ad_start(adp);
#ifdef AD_DEBUG
    printf("ad_interrupt: completed\n");
#endif
    return ATA_OP_FINISHED;
}

void
ad_reinit(struct ad_softc *adp)
{
    /* reinit disk parameters */
    ata_command(adp->controller, adp->unit, ATA_C_SET_MULTI, 0, 0, 0,
		adp->transfersize / DEV_BSIZE, 0, ATA_IMMEDIATE);
    ata_wait(adp->controller, adp->unit, ATA_S_READY);
    ata_dmainit(adp->controller, adp->unit, apiomode(adp->ata_parm),
		wdmamode(adp->ata_parm), udmamode(adp->ata_parm));
}

static void
ad_timeout(struct ad_request *request)
{
    struct ad_softc *adp = request->device;

    adp->controller->running = NULL;
    printf("ata%d-%s: ad_timeout: lost disk contact - resetting\n",
	   adp->controller->lun, 
	   (adp->unit == ATA_MASTER) ? "master" : "slave");

    if (request->flags & AR_F_DMA_USED)
	ata_dmadone(adp->controller);

    if (request->retries < AD_MAX_RETRIES) {
	/* reinject this request */
	request->retries++;
	TAILQ_INSERT_HEAD(&adp->controller->ata_queue, request, chain);
    }
    else {
	/* retries all used up, return error */
	request->bp->b_error = EIO;
	request->bp->b_flags |= B_ERROR;
	devstat_end_transaction_buf(&adp->stats, request->bp);
	biodone(request->bp);
	free(request, M_AD);
    }
    ata_reinit(adp->controller);
}

static void
ad_sleep(struct ad_softc *adp, int8_t *mesg)
{
    int32_t s;

    s = splbio();
    while (adp->controller->active != ATA_IDLE)
	tsleep((caddr_t)&adp->controller->active, PZERO - 1, mesg, 1);
    splx(s);
}

static int8_t
ad_version(u_int16_t version)
{
    int32_t bit;

    if (version == 0xffff)
	return '?';
    for (bit = 15; bit >= 0; bit--)
	if (version & (1<<bit))
	    return ('0' + bit);
    return '?';
}

static void 
ad_drvinit(void)
{
    fakewd_cdevsw = ad_cdevsw;
    fakewd_cdevsw.d_maj = 3;
    fakewd_cdevsw.d_bmaj = 0;
    fakewd_cdevsw.d_name = "wd";
    
    /* register callback for when interrupts are enabled */
    if (!(ad_attach_hook = 
	(struct intr_config_hook *)malloc(sizeof(struct intr_config_hook),
					  M_TEMP, M_NOWAIT))) {
	printf("ad: malloc attach_hook failed\n");
	return;
    }
    bzero(ad_attach_hook, sizeof(struct intr_config_hook));

    ad_attach_hook->ich_func = ad_attach;
    if (config_intrhook_establish(ad_attach_hook) != 0) {
	printf("ad: config_intrhook_establish failed\n");
	free(ad_attach_hook, M_TEMP);
    }
}

SYSINIT(addev, SI_SUB_DRIVERS, SI_ORDER_SECOND, ad_drvinit, NULL)
