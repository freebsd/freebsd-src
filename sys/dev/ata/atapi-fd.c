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
 *	$Id: atapi-fd.c,v 1.4 1999/03/28 18:57:19 sos Exp $
 */

#include "ata.h"
#include "atapifd.h"
#include "opt_devfs.h"

#if NATA > 0 && NATAPIFD > 0

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
#include <sys/cdio.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/stat.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <pci/pcivar.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>
#include <dev/ata/atapi-fd.h>

static  d_open_t	afdopen;
static  d_close_t	afdclose;
static	d_read_t	afdread;
static	d_write_t	afdwrite;
static  d_ioctl_t	afdioctl;
static  d_strategy_t	afdstrategy;

#define BDEV_MAJOR 32
#define CDEV_MAJOR 118

static struct cdevsw afd_cdevsw = {
	  afdopen,	afdclose,	afdread,	afdwrite,
	  afdioctl,	nostop,		nullreset,	nodevtotty,
	  seltrue,	nommap,		afdstrategy,	"afd",
	  NULL,		-1,		nodump,		nopsize,
	  D_DISK,	0,		-1
};

#define NUNIT 			8
#define UNIT(d)         	((minor(d) >> 3) & 3)

#define F_OPEN            	0x0001	/* The device is opened */
#define F_MEDIA_CHANGED   	0x0002	/* The media have changed */

static struct afd_softc *afdtab[NUNIT];	/* Drive info by unit number */
static int32_t afdnlun = 0;                 /* Number of config'd drives */

int32_t afdattach(struct atapi_softc *);
static int32_t afd_sense(struct afd_softc *);
static void afd_describe(struct afd_softc *);
static void afd_strategy(struct buf *);
static void afd_start(struct afd_softc *);
static void afd_done(struct atapi_request *);
static int32_t afd_start_device(struct afd_softc *, int32_t);
static int32_t afd_lock_device(struct afd_softc *, int32_t);
static int32_t afd_eject(struct afd_softc *, int32_t);
static void afd_drvinit(void *);

int32_t 
afdattach(struct atapi_softc *atp)
{
    struct afd_softc *fdp;

    if (afdnlun >= NUNIT) {
        printf("afd: too many units\n");
        return -1;
    }
    fdp = malloc(sizeof(struct afd_softc), M_TEMP, M_NOWAIT);
    if (!fdp) {
        printf("afd: out of memory\n");
        return -1;
    }
    bzero(fdp, sizeof(struct afd_softc));
    bufq_init(&fdp->buf_queue);
    fdp->atp = atp;
    fdp->lun = afdnlun;
    fdp->flags = F_MEDIA_CHANGED;

    if (afd_sense(fdp)) {
	free(fdp, M_TEMP);
	return -1;
    }

    afd_describe(fdp);
    afdtab[afdnlun++] = fdp;
    devstat_add_entry(&fdp->stats, "afd", fdp->lun, DEV_BSIZE,
                      DEVSTAT_NO_ORDERED_TAGS,
                      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
                      0x178);
#ifdef DEVFS 
    fdp->cdevs_token = devfs_add_devswf(&afd_cdevsw, dkmakeminor(fdp->lun, 0,0),
					DV_CHR, UID_ROOT, GID_OPERATOR, 
					0640, "rafd%d", fdp->lun);
    fdp->bdevs_token = devfs_add_devswf(&afd_cdevsw, dkmakeminor(fdp->lun, 0,0),
					DV_BLK, UID_ROOT, GID_OPERATOR, 
					0640, "afd%d", fdp->lun);
#endif
    return 0;
}

static int32_t 
afd_sense(struct afd_softc *fdp)
{
    int32_t error, count;
    int8_t buffer[256];
    int8_t ccb[16] = { ATAPI_MODE_SENSE, 0, ATAPI_REWRITEABLE_CAP_PAGE,
		       0, 0, 0, 0, sizeof(buffer)>>8, sizeof(buffer) & 0xff,
            	       0, 0, 0, 0, 0, 0, 0 };

    bzero(buffer, sizeof(buffer));
    /* Get drive capabilities, some drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
        if (!(error = atapi_queue_cmd(fdp->atp, ccb, buffer, sizeof(buffer),
				      A_READ, NULL, NULL, NULL)))
            break;
    }
#ifdef AFD_DEBUG
    atapi_dump("afd: sense", buffer, sizeof(buffer));
#endif
    if (error)
        return error;
    bcopy(buffer, &fdp->header, sizeof(struct afd_header));
    bcopy(buffer+sizeof(struct afd_header), &fdp->cap, 
	  sizeof(struct afd_cappage));
    if (fdp->cap.page_code != ATAPI_REWRITEABLE_CAP_PAGE)
        return 1;   
    fdp->cap.cylinders = ntohs(fdp->cap.cylinders);
    fdp->cap.sector_size = ntohs(fdp->cap.sector_size);
    return 0;
}

static void 
afd_describe(struct afd_softc *fdp)
{
    int8_t model_buf[40+1];
    int8_t revision_buf[8+1];

    bpack(fdp->atp->atapi_parm->model, model_buf, sizeof(model_buf));
    bpack(fdp->atp->atapi_parm->revision, revision_buf, sizeof(revision_buf));
    printf("afd%d: <%s/%s> rewriteable drive at ata%d as %s\n",
	   fdp->lun, model_buf, revision_buf,
           fdp->atp->controller->lun,
	   (fdp->atp->unit == ATA_MASTER) ? "master" : "slave ");
    printf("afd%d: %luMB (%u sectors), %u cyls, %u heads, %u S/T, %u B/S\n",
           afdnlun, 
	   (fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors) / 
		((1024L * 1024L) / fdp->cap.sector_size),
	   fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors,
	   fdp->cap.cylinders, fdp->cap.heads, fdp->cap.sectors,
	   fdp->cap.sector_size);
    printf("afd%d: ", fdp->lun);
    switch (fdp->header.medium_type) {
	default: printf("Unknown media (0x%x)", fdp->header.medium_type);
    }
    if (fdp->header.wp) printf(", writeprotected");
    printf("\n");
}

static int
afdopen(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    struct afd_softc *fdp;
    struct disklabel label;
    int32_t lun = UNIT(dev);

    if (lun >= afdnlun || !(fdp = afdtab[lun])) 
        return ENXIO;

    fdp->flags &= ~F_MEDIA_CHANGED;
    afd_lock_device(fdp, 1);
    if (afd_sense(fdp))
        printf("afd%d: sense media type failed\n", fdp->lun);

    /* build disklabel and initilize slice tables */
    bzero(&label, sizeof label);
    label.d_secsize = fdp->cap.sector_size;
    label.d_nsectors = fdp->cap.sectors;  
    label.d_ntracks = fdp->cap.heads;
    label.d_ncylinders = fdp->cap.cylinders;
    label.d_secpercyl = fdp->cap.heads * fdp->cap.sectors;
    label.d_secperunit = fdp->cap.heads * fdp->cap.sectors * fdp->cap.cylinders;

    /* Initialize slice tables. */
    return dsopen("afd", dev, fmt, 0, &fdp->slices, &label, afd_strategy,
		  (ds_setgeom_t *)NULL, &afd_cdevsw);
}

static int 
afdclose(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    int32_t lun = UNIT(dev);
    struct afd_softc *fdp;

    if (lun >= afdnlun || !(fdp = afdtab[lun]))      
        return ENXIO;

    dsclose(dev, fmt, fdp->slices);
    if(!dsisopen(fdp->slices))
        afd_lock_device(fdp, 0); 
    return 0;
}

static int
afdread(dev_t dev, struct uio *uio, int32_t ioflag)
{
	return physio(afdstrategy, NULL, dev, 1, minphys, uio);
}

static int
afdwrite(dev_t dev, struct uio *uio, int32_t ioflag)
{
	return physio(afdstrategy, NULL, dev, 0, minphys, uio);
}

static int 
afdioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    int32_t lun = UNIT(dev);
    int32_t error = 0;
    struct afd_softc *fdp;

    if (lun >= afdnlun || !(fdp = afdtab[lun]))
        return ENXIO;

    error = dsioctl("sd", dev, cmd, addr, flag, &fdp->slices,
                    afd_strategy, (ds_setgeom_t *)NULL);

    if (error != ENOIOCTL)
        return error;

    switch (cmd) {
    case CDIOCEJECT:
        if ((fdp->flags & F_OPEN) && fdp->refcnt)
            return EBUSY;
        return afd_eject(fdp, 0);

    case CDIOCCLOSE:
        if ((fdp->flags & F_OPEN) && fdp->refcnt)
            return 0;
        return afd_eject(fdp, 1);

    default:
        return ENOTTY;
    }
    return error;
}

static void 
afdstrategy(struct buf *bp)
{
    int32_t lun = UNIT(bp->b_dev);
    struct afd_softc *fdp = afdtab[lun];
    int32_t x;

    if (bp->b_bcount == 0) {
        bp->b_resid = 0;
        biodone(bp);
        return;
    }
    if (dscheck(bp, fdp->slices) <= 0) {
        biodone(bp);
        return;
    }
    x = splbio();
    bufq_insert_tail(&fdp->buf_queue, bp);
    afd_start(fdp);
    splx(x);
}

static void 
afd_strategy(struct buf *bp)
{
    afdstrategy(bp);
}

static void 
afd_start(struct afd_softc *fdp)
{
    struct buf *bp = bufq_first(&fdp->buf_queue);
    u_int32_t lba, count;
    int8_t ccb[16];
    
    if (!bp)
        return;
    bzero(ccb, sizeof(ccb));
    bufq_remove(&fdp->buf_queue, bp);

    /* Should reject all queued entries if media have changed. */
    if (fdp->flags & F_MEDIA_CHANGED) {
        bp->b_error = EIO;
        bp->b_flags |= B_ERROR;
        biodone(bp);
        return;
    }

    lba = bp->b_blkno / (fdp->cap.sector_size / DEV_BSIZE);
    count = (bp->b_bcount + (fdp->cap.sector_size - 1)) / fdp->cap.sector_size;

    if (count > 64) /* only needed for ZIP drives SOS */
	count = 64;

    if (bp->b_flags & B_READ)
	ccb[0] = ATAPI_READ_BIG;
    else
	ccb[0] = ATAPI_WRITE_BIG;

    ccb[1] = 0; 
    ccb[2] = lba>>24;  
    ccb[3] = lba>>16;
    ccb[4] = lba>>8;
    ccb[5] = lba;   
    ccb[7] = count>>8;
    ccb[8] = count;

    devstat_start_transaction(&fdp->stats);

    atapi_queue_cmd(fdp->atp, ccb, bp->b_data, count*fdp->cap.sector_size, 
		    (bp->b_flags & B_READ) ? A_READ : 0, afd_done, fdp, bp);
}

static void 
afd_done(struct atapi_request *request)
{
    struct buf *bp = request->bp;
    struct afd_softc *fdp = request->driver;

    devstat_end_transaction(&fdp->stats, bp->b_bcount-request->bytecount,
                            DEVSTAT_TAG_NONE,
                            (bp->b_flags&B_READ) ? DEVSTAT_READ:DEVSTAT_WRITE);
 
    if (request->result) {
	printf("afd_done: ");
        atapi_error(request->device, request->result);
        bp->b_error = EIO;
        bp->b_flags |= B_ERROR;
    }
    else
	bp->b_resid = request->bytecount;
    biodone(bp);
    afd_start(fdp);
}

static int32_t
afd_start_device(struct afd_softc *fdp, int32_t start)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0, 0, 0, start,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(fdp->atp, ccb, NULL, 0, 0, NULL, NULL, NULL);
}

static int32_t
afd_lock_device(struct afd_softc *fdp, int32_t lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
    return atapi_queue_cmd(fdp->atp, ccb, NULL, 0, 0, NULL, NULL, NULL);
}

static int32_t 
afd_eject(struct afd_softc *fdp, int32_t close)
{
    int32_t error;

    error = afd_start_device(fdp, 0);
    if ((error & ATAPI_SK_MASK) &&
        ((error & ATAPI_SK_MASK) == ATAPI_SK_NOT_READY ||
         (error & ATAPI_SK_MASK) == ATAPI_SK_UNIT_ATTENTION)) {

        if (!close)
            return 0;
	if ((error = afd_start_device(fdp, 3)))
	    return error;
	afd_lock_device(fdp, 1);
        return 0;
    }
    if (error) {
        atapi_error(fdp->atp, error);
        return EIO;
    }
    if (close)
        return 0;

    tsleep((caddr_t) &lbolt, PRIBIO, "afdej1", 0);
    tsleep((caddr_t) &lbolt, PRIBIO, "afdej2", 0);
    afd_lock_device(fdp, 0);
    fdp->flags |= F_MEDIA_CHANGED;
    return afd_start_device(fdp, 2);
}


static void
afd_drvinit(void *unused)
{
    static int32_t afd_devsw_installed = 0;

    if (!afd_devsw_installed) {
	cdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &afd_cdevsw);
	afd_devsw_installed = 1;
    }
}

SYSINIT(afddev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, afd_drvinit, NULL)
#endif /* NATA & NATAPIFD */
