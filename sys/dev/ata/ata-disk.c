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
 *	$Id: ata-disk.c,v 1.6 1999/04/10 18:53:35 sos Exp $
 */

#include "ata.h"
#include "atadisk.h"
#include "opt_devfs.h"

#if NATA > 0 && NATADISK > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/devicestat.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/stat.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>

static d_open_t		adopen;
static d_close_t	adclose;
static d_read_t		adread;
static d_write_t	adwrite;
static d_ioctl_t	adioctl;
static d_strategy_t	adstrategy;
static d_psize_t	adpsize;

#define BDEV_MAJOR 30
#define CDEV_MAJOR 116
static struct cdevsw ad_cdevsw = {
    adopen,	adclose,	adread,		adwrite,	
    adioctl,	nostop,		nullreset,	nodevtotty,
#ifdef NOTYET	/* the boot code needs to be fixed to boot arbitrary devices */
    seltrue,	nommap,		adstrategy,	"ad",
#else
    seltrue,	nommap,		adstrategy,	"wd",
#endif
    NULL,	-1,		nodump,		adpsize,
    D_DISK,	0,		-1
};

/* misc defines */
#define UNIT(dev) (dev>>3 & 0x1f)		/* assume 8 minor # per unit */
#define NUNIT	16				/* max # of devices */

/* prototypes */
static void ad_attach(void *);
static int32_t ad_getparam(struct ad_softc *);
static void ad_strategy(struct buf *);
static void ad_start(struct ad_softc *);
static void ad_sleep(struct ad_softc *, int8_t *);
static int8_t ad_version(u_int16_t);
int32_t ad_timeout(char *data);
static void ad_drvinit(void);

static struct ad_softc *adtab[NUNIT];
static int32_t adnlun = 0;     			/* number of config'd drives */
static struct intr_config_hook *ad_attach_hook;

static __inline int
apiomode(struct ata_params *ap)
{
        if ((ap->atavalid & 2) == 2) {
                if ((ap->apiomodes & 2) == 2) return 4;
                if ((ap->apiomodes & 1) == 1) return 3;
        }       
        return -1; 
} 

static __inline int
wdmamode(struct ata_params *ap)
{
        if ((ap->atavalid & 2) == 2) {
                if ((ap->wdmamodes & 4) == 4) return 2;
                if ((ap->wdmamodes & 2) == 2) return 1;
                if ((ap->wdmamodes & 1) == 1) return 0;
        }
        return -1;
}

static __inline int
udmamode(struct ata_params *ap)
{
        if ((ap->atavalid & 4) == 4) {
                if ((ap->udmamodes & 4) == 4) return 2;
                if ((ap->udmamodes & 2) == 2) return 1;
                if ((ap->udmamodes & 1) == 1) return 0;
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

    /* now, run through atadevices and look for ATA disks */
    for (ctlr=0; ctlr<MAXATA; ctlr++) {
    	if (!atadevices[ctlr]) continue;
	for (dev=0; dev<2; dev++) {
	    if (atadevices[ctlr]->devices & 
		(dev ? ATA_ATA_SLAVE : ATA_ATA_MASTER)) {
#ifdef ATA_STATIC_ID
		adnlun = dev + ctlr * 2;   
#endif
    		adp = adtab[adnlun];
    		if (adp)
        	    printf("ad%d: unit already attached\n", adnlun);
    		if (!(adp = malloc(sizeof(struct ad_softc), 
				   M_DEVBUF, M_NOWAIT))) {
        	    printf("ad%d: failed to allocate driver storage\n", adnlun);
		    continue;
		}
    		bzero(adp, sizeof(struct ad_softc));
	        adp->controller = atadevices[ctlr];
		adp->unit = (dev == 0) ? ATA_MASTER : ATA_SLAVE;
		adp->lun = adnlun;
		if (ad_getparam(adp)) {
		    free(adp, M_DEVBUF);
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
		    adp->flags |= AD_F_USE_LBA;

		/* use multiple sectors/interrupt if device supports it */
		adp->transfersize = DEV_BSIZE;
		secsperint = min(adp->ata_parm->nsecperint, 16);

		if (!ata_command(adp->controller, adp->unit, ATA_C_SET_MULTI,
				 0, 0, 0, secsperint, 0, ATA_WAIT_INTR) &&
		    ata_wait(adp->controller, adp->unit, ATA_S_DRDY) >= 0)
		    adp->transfersize *= secsperint;

		/* use DMA if drive & controller supports it */
                if (!ata_dmainit(adp->controller, adp->unit,
                                 apiomode(adp->ata_parm),
                                 wdmamode(adp->ata_parm),
                                 udmamode(adp->ata_parm)))
                    adp->flags |= AD_F_DMA_ENABLED;

	        bpack(adp->ata_parm->model, model_buf, sizeof(model_buf));
		bpack(adp->ata_parm->revision, revision_buf, 
		      sizeof(revision_buf));
		printf("ad%d: <%s/%s> ATA-%c disk at ata%d as %s\n", 
		       adnlun,
           	       model_buf, revision_buf,
		       ad_version(adp->ata_parm->versmajor),
		       ctlr,
		       (adp->unit == ATA_MASTER) ? "master" : "slave ");
		printf("ad%d: %luMB (%u sectors), "
		       "%u cyls, %u heads, %u S/T, %u B/S\n",
		       adnlun,
		       adp->total_secs / ((1024L * 1024L) / DEV_BSIZE),
		       adp->total_secs,
		       adp->cylinders,
		       adp->heads,
		       adp->sectors,
		       DEV_BSIZE);
		printf("ad%d: piomode=%d, dmamode=%d, udmamode=%d\n",
		       adnlun, 
		       apiomode(adp->ata_parm),
		       wdmamode(adp->ata_parm),
		       udmamode(adp->ata_parm));
		printf("ad%d: %d secs/int, %d depth queue, %s mode\n", 
		       adnlun, adp->transfersize / DEV_BSIZE,
		       adp->ata_parm->queuelen & 0x1f,
		       (adp->flags & AD_F_DMA_ENABLED) ? "DMA" :"PIO");
                devstat_add_entry(&adp->stats, "ad", adnlun, DEV_BSIZE,
				  DEVSTAT_NO_ORDERED_TAGS,
                                  DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
				  0x180);
#ifdef DEVFS
    		adp->cdevs_token = devfs_add_devswf(&ad_cdevsw, 
						    dkmakeminor(adp->lun, 0, 0),
						    DV_CHR, 
						    UID_ROOT, GID_OPERATOR,
						    0640, "rad%d", adp->lun);
    		adp->bdevs_token = devfs_add_devswf(&ad_cdevsw,
					 	    dkmakeminor(adp->lun, 0, 0),
						    DV_BLK, 
						    UID_ROOT, GID_OPERATOR,
						    0640, "ad%d", adp->lun);
#endif
		bufq_init(&adp->queue);
	        adtab[adnlun++] = adp;
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
		 ATA_S_DRDY | ATA_S_DSC | ATA_S_DRQ))
	return -1;
    insw(adp->controller->ioaddr + ATA_DATA, buffer, 
	 sizeof(buffer)/sizeof(int16_t));
    ata_parm = malloc(sizeof(struct ata_params), M_DEVBUF, M_NOWAIT);
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
    int32_t lun = UNIT(dev);
    struct ad_softc *adp;
    struct disklabel label;
    int32_t error;

#ifdef AD_DEBUG
printf("adopen: lun=%d adnlun=%d\n", lun, adnlun);
#endif
    if (lun >= adnlun || !(adp = adtab[lun]))
        return ENXIO;

    /* spinwait if anybody else is reading the disk label */
    /* is this needed anymore ?? SOS XXX */
    while (adp->flags & AD_F_LABELLING)
        tsleep((caddr_t)&adp->flags, PZERO - 1, "adop1", 1);

    /* protect agains label race */
    adp->flags |= AD_F_LABELLING;

    /* build disklabel and initilize slice tables */
    bzero(&label, sizeof label);
    label.d_secsize = DEV_BSIZE;
    label.d_nsectors = adp->sectors;
    label.d_ntracks = adp->heads;
    label.d_ncylinders = adp->cylinders;
    label.d_secpercyl = adp->sectors * adp->heads;
    label.d_secperunit = adp->total_secs;

    error = dsopen("ad", dev, fmt, 0, &adp->slices, &label, ad_strategy,
                   (ds_setgeom_t *)NULL, &ad_cdevsw);

    adp->flags &= ~AD_F_LABELLING;
    ad_sleep(adp, "adop2");
    return error;
}

static int 
adclose(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    int32_t lun = UNIT(dev);
    struct ad_softc *adp;

#ifdef AD_DEBUG
printf("adclose: lun=%d adnlun=%d\n", lun, adnlun);
#endif
    if (lun >= adnlun || !(adp = adtab[lun]))
        return ENXIO;

    dsclose(dev, fmt, adp->slices);
    return 0;
}

static int
adread(dev_t dev, struct uio *uio, int32_t ioflag)
{
    return physio(adstrategy, NULL, dev, 1, minphys, uio);
}

static int
adwrite(dev_t dev, struct uio *uio, int32_t ioflag)
{
    return physio(adstrategy, NULL, dev, 0, minphys, uio);
}

static int 
adioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flags, struct proc *p)
{
    struct ad_softc *adp;
    int32_t lun = UNIT(dev);
    int32_t error = 0;

    if (lun >= adnlun || !(adp = adtab[lun]))
        return ENXIO;

    ad_sleep(adp, "adioct");
    error = dsioctl("sd", dev, cmd, addr, flags, &adp->slices, 
		    ad_strategy, (ds_setgeom_t *)NULL);

    if (error != ENOIOCTL)
        return error;
    return ENOTTY;
}

static void 
adstrategy(struct buf *bp)
{
    struct ad_softc *adp;
    int32_t lun = UNIT(bp->b_dev);
    int32_t s;

#ifdef AD_DEBUG
printf("adstrategy: entered\n");
#endif
    if (lun >= adnlun ||  bp->b_blkno < 0 || !(adp = adtab[lun]) 
	|| bp->b_bcount % DEV_BSIZE != 0) {
        bp->b_error = EINVAL; 
        bp->b_flags |= B_ERROR;
        biodone(bp);
	return;
    }

    if (dscheck(bp, adp->slices) <= 0) {
	biodone(bp);
	return;
    }

    /* hang around if somebody else is labelling */
    if (adp->flags & AD_F_LABELLING)
        ad_sleep(adp, "adlab");

    s = splbio();
    bufqdisksort(&adp->queue, bp);

    if (!adp->active)
	ad_start(adp);

    if (adp->controller->active == ATA_IDLE)
	ata_start(adp->controller);

    splx(s);
    return;
}

static int
adpsize(dev_t dev)
{
    struct ad_softc *adp;
    int32_t lun = UNIT(dev);

    if (lun >= adnlun || !(adp = adtab[lun]))
        return -1;
    return dssize(dev, &adp->slices, adopen, adclose);
}

static void 
ad_strategy(struct buf *bp)
{
    adstrategy(bp);
}

static void
ad_start(struct ad_softc *adp)
{
    struct buf *bp;

#ifdef AD_DEBUG
printf("ad_start:\n");
#endif
    if (adp->active) {
	printf("ad_start: should newer be called when active\n"); /* SOS */
	return;
    }
    if (!(bp = bufq_first(&adp->queue)))
        return;

    /* remove from drive queue */
    bufq_remove(&adp->queue, bp); 
    bp->b_driver1 = adp;

    /* link onto controller queue */
    bufq_insert_tail(&adp->controller->ata_queue, bp);

    /* mark the drive as busy */
    adp->active = 1;
}

void
ad_transfer(struct buf *bp)
{
    struct ad_softc *adp;
    u_int32_t blknum, secsprcyl;
    u_int32_t cylinder, head, sector, count, command;

    /* get request params */
    adp = bp->b_driver1;

    /* calculate transfer details */
    blknum = bp->b_pblkno + (adp->donecount / DEV_BSIZE);
   
#ifdef AD_DEBUG
        printf("ad_transfer: blknum=%d\n", blknum);
#endif
    if (adp->donecount == 0) {

	/* setup transfer parameters */
        adp->bytecount = bp->b_bcount;
	count = howmany(adp->bytecount, DEV_BSIZE);

	if (adp->flags & AD_F_USE_LBA) {
	    sector = (blknum >> 0) & 0xff; 
	    cylinder = (blknum >> 8) & 0xffff;
	    head = ((blknum >> 24) & 0xf) | ATA_D_LBA; 
	}
	else {
            secsprcyl = adp->sectors * adp->heads;
            cylinder = blknum / secsprcyl;
            head = (blknum % secsprcyl) / adp->sectors;
            sector = (blknum % adp->sectors) + 1;
	}

	/* setup first transfer length */
     	adp->currentsize = min(adp->bytecount, adp->transfersize);

        devstat_start_transaction(&adp->stats);

	/* does this drive & transfer work with DMA ? */
	adp->flags &= ~AD_F_DMA_USED;
	if ((adp->flags & AD_F_DMA_ENABLED) &&
	    !ata_dmasetup(adp->controller, adp->unit,
			  (void *)bp->b_data, adp->bytecount,
			  (bp->b_flags & B_READ))) {
	    adp->flags |= AD_F_DMA_USED;
	    command = (bp->b_flags&B_READ) ? ATA_C_READ_DMA : ATA_C_WRITE_DMA;
	    adp->currentsize = adp->bytecount;
        }
	/* does this drive support multi sector transfers ? */
	else if (adp->currentsize > DEV_BSIZE)
	    command = (bp->b_flags&B_READ) ? ATA_C_READ_MULTI:ATA_C_WRITE_MULTI;
	else
	    command = (bp->b_flags&B_READ) ? ATA_C_READ : ATA_C_WRITE;

        ata_command(adp->controller, adp->unit, command, cylinder, head, 
		    sector, count, 0, ATA_IMMEDIATE);
    }
   
    /* if this is a DMA transaction start it, return and wait for interrupt */
    if (adp->flags & AD_F_DMA_USED) {
	ata_dmastart(adp->controller, adp->unit);
#ifdef AD_DEBUG
        printf("ad_transfer: return waiting for DMA interrupt\n");
#endif
	return;
    }

    /* calculate this transfer length */
    adp->currentsize = min(adp->bytecount, adp->transfersize);

    /* if this is a PIO read operation, return and wait for interrupt */
    if (bp->b_flags & B_READ) {
#ifdef AD_DEBUG
    	printf("ad_transfer: return waiting for PIO read interrupt\n");
#endif
        return;
    }

    /* ready to write PIO data ? */
    if (ata_wait(adp->controller, adp->unit, 
		 ATA_S_DRDY | ATA_S_DSC | ATA_S_DRQ) < 0) {
        printf("ad_transfer: timeout waiting for DRQ");
    }                               
    
    /* output the data */
#if 0
    outsw(adp->controller->ioaddr + ATA_DATA,
          (void *)((uintptr_t)bp->b_data + adp->donecount),
          adp->currentsize / sizeof(int16_t));
#else
    outsl(adp->controller->ioaddr + ATA_DATA,
          (void *)((uintptr_t)bp->b_data + adp->donecount),
          adp->currentsize / sizeof(int32_t));
#endif
    adp->bytecount -= adp->currentsize;
#ifdef AD_DEBUG
    printf("ad_transfer: return wrote data\n");
#endif
}

int32_t
ad_interrupt(struct buf *bp)
{
    struct ad_softc *adp = bp->b_driver1;
    int32_t dma_stat = 0;

    /* finish DMA transfer */
    if (adp->flags & AD_F_DMA_USED) {
        if (!(ata_dmastatus(adp->controller, adp->unit) & ATA_BMSTAT_INTERRUPT)){
printf("extra SMP interrupt\n");
            return ATA_OP_CONTINUES;
}
        dma_stat = ata_dmadone(adp->controller, adp->unit);
    }

    /* get drive status */
    if (ata_wait(adp->controller, adp->unit, 0) < 0)
         printf("ad_interrupt: timeout waiting for status");
    if (adp->controller->status & (ATA_S_ERROR | ATA_S_CORR) ||
	(adp->flags & AD_F_DMA_USED && dma_stat != ATA_BMSTAT_INTERRUPT)) {
oops:
	printf("ad%d: status=%02x error=%02x\n", 
	       adp->lun, adp->controller->status, adp->controller->error);
	if (adp->controller->status & ATA_S_ERROR) {
       	    printf("ad_interrupt: hard error\n"); 
            bp->b_error = EIO;
            bp->b_flags |= B_ERROR;
	}
	if (adp->controller->status & ATA_S_CORR)
       	    printf("ad_interrupt: soft ECC\n"); 
    }

    /* if this was a PIO read operation, get the data */
    if (adp->active && !(adp->flags & AD_F_DMA_USED) &&
        ((bp->b_flags & (B_READ | B_ERROR)) == B_READ)) {

        /* ready to receive data? */
        if ((adp->controller->status & (ATA_S_DRDY | ATA_S_DSC | ATA_S_DRQ))
            != (ATA_S_DRDY | ATA_S_DSC | ATA_S_DRQ))
            printf("ad_interrupt: read interrupt arrived early");

        if (ata_wait(adp->controller, adp->unit,
		     ATA_S_DRDY | ATA_S_DSC | ATA_S_DRQ) != 0){
            printf("ad_interrupt: read error detected late");
            goto oops;   
        }

        /* data ready, read in */
#if 0
        insw(adp->controller->ioaddr + ATA_DATA,
             (void *)((uintptr_t)bp->b_data + adp->donecount), 
	     adp->currentsize / sizeof(int16_t));
#else
        insl(adp->controller->ioaddr + ATA_DATA,
             (void *)((uintptr_t)bp->b_data + adp->donecount), 
	     adp->currentsize / sizeof(int32_t));
#endif
        adp->bytecount -= adp->currentsize;
#ifdef AD_DEBUG
    printf("ad_interrupt: read in data\n");
#endif
    }

    /* if this was a DMA operation finish up */
    if (adp->active && (adp->flags & AD_F_DMA_USED) && !(bp->b_flags & B_ERROR))
        adp->bytecount -= adp->currentsize;

    /* finish up this tranfer, check for more work on this buffer */
    if (adp->controller->active == ATA_ACTIVE_ATA) {
	if ((bp->b_flags & B_ERROR) == 0) {
	    adp->donecount += adp->currentsize;
#ifdef AD_DEBUG
    	    printf("ad_interrupt: %s op OK\n", (bp->b_flags & B_READ)?"R":"W");
#endif
	    if (adp->bytecount > 0) {
	        ad_transfer(bp);
		return ATA_OP_CONTINUES;
	    }
	}
	bufq_remove(&adp->controller->ata_queue, bp);
	bp->b_resid = bp->b_bcount - adp->donecount;
	biodone(bp);
        devstat_end_transaction(&adp->stats, bp->b_bcount - bp->b_resid,
                                DEVSTAT_TAG_NONE,
                                (bp->b_flags & B_READ) ? 
				DEVSTAT_READ : DEVSTAT_WRITE);
	adp->donecount = 0;
	adp->active = 0;
    }
    ad_start(adp);
#ifdef AD_DEBUG
    printf("ad_interrupt: completed\n");
#endif
    return ATA_OP_FINISHED;
}

static void
ad_sleep(struct ad_softc *adp, int8_t *mesg)
{
    int32_t s = splbio();  

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
    static int32_t ad_devsw_installed = 0;

    if (!ad_devsw_installed) {
        cdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &ad_cdevsw);
        cdevsw_add_generic(0, 3, &ad_cdevsw);	/* grap wd entries too */
        ad_devsw_installed = 1;
    }
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
#endif /* NATA && NATADISK */
