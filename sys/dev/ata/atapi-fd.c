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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/cdio.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <pci/pcivar.h>
#if NAPM > 0
#include <machine/apm_bios.h>
#endif
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>
#include <dev/ata/atapi-fd.h>

static	d_open_t	afdopen;
static	d_close_t	afdclose;
static	d_ioctl_t	afdioctl;
static	d_strategy_t	afdstrategy;

static struct cdevsw afd_cdevsw = {
	/* open */	afdopen,
	/* close */	afdclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	afdioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	afdstrategy,
	/* name */	"afd",
	/* maj */	118,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
	/* bmaj */	32,
};
static struct cdevsw afddisk_cdevsw;

/* prototypes */
int32_t afdattach(struct atapi_softc *);
static int32_t afd_sense(struct afd_softc *);
static void afd_describe(struct afd_softc *);
static void afd_start(struct afd_softc *);
static void afd_partial_done(struct atapi_request *);
static void afd_done(struct atapi_request *);
static int32_t afd_eject(struct afd_softc *, int32_t);
static int32_t afd_start_stop(struct afd_softc *, int32_t);
static int32_t afd_prevent_allow(struct afd_softc *, int32_t);

/* internal vars */
static int32_t afdnlun = 0;		/* number of config'd drives */

int32_t 
afdattach(struct atapi_softc *atp)
{
    struct afd_softc *fdp;
    dev_t dev;

    fdp = malloc(sizeof(struct afd_softc), M_TEMP, M_NOWAIT);
    if (!fdp) {
	printf("afd: out of memory\n");
	return -1;
    }
    bzero(fdp, sizeof(struct afd_softc));
    bufq_init(&fdp->buf_queue);
    fdp->atp = atp;
    fdp->lun = afdnlun++;
    fdp->atp->flags |= ATAPI_F_MEDIA_CHANGED;

    if (afd_sense(fdp)) {
	free(fdp, M_TEMP);
	return -1;
    }

    if (!strncmp(atp->atapi_parm->model, "IOMEGA  ZIP", 11))
	fdp->transfersize = 64;

    afd_describe(fdp);
    devstat_add_entry(&fdp->stats, "afd", fdp->lun, DEV_BSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
		      0x174);
    dev = disk_create(fdp->lun, &fdp->disk, 0, &afd_cdevsw, &afddisk_cdevsw);
    dev->si_drv1 = fdp;
    return 0;
}

static int32_t 
afd_sense(struct afd_softc *fdp)
{
    int8_t buffer[256];
    int8_t ccb[16] = { ATAPI_MODE_SENSE_BIG, 0, ATAPI_REWRITEABLE_CAP_PAGE,
		       0, 0, 0, 0, sizeof(buffer)>>8, sizeof(buffer) & 0xff,
		       0, 0, 0, 0, 0, 0, 0 };
    int32_t error, count;

    bzero(buffer, sizeof(buffer));
    /* get drive capabilities, some drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
	if (!(error = atapi_immed_cmd(fdp->atp, ccb, buffer, sizeof(buffer),
				      A_READ, 30))) {
	    error = atapi_error(fdp->atp, error);
	    break;
	}
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
    fdp->cap.transfer_rate = ntohs(fdp->cap.transfer_rate);
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
	   fdp->lun, 
	   (fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors) / 
	   ((1024L * 1024L) / fdp->cap.sector_size),
	   fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors,
	   fdp->cap.cylinders, fdp->cap.heads, fdp->cap.sectors,
	   fdp->cap.sector_size);
    printf("afd%d: %dKB/s,", fdp->lun, fdp->cap.transfer_rate/8);
    if (fdp->transfersize)
	printf(" transfer limit %d blks,", fdp->transfersize);
    printf(" %s\n", ata_mode2str(fdp->atp->controller->mode[
				 (fdp->atp->unit == ATA_MASTER) ? 0 : 1]));
    printf("afd%d: Medium: ", fdp->lun);
    switch (fdp->header.medium_type) {
	case MFD_2DD:
	    printf("720KB DD disk"); break;

	case MFD_HD_12:
	    printf("1.2MB HD disk"); break;

	case MFD_HD_144:
	    printf("1.44MB HD disk"); break;

	case MFD_UHD: 
	    printf("120MB UHD disk"); break;

	default: printf("Unknown media (0x%x)", fdp->header.medium_type);
    }
    if (fdp->header.wp) printf(", writeprotected");
    printf("\n");
}

static int
afdopen(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    struct afd_softc *fdp = dev->si_drv1;
    struct disklabel *label;

    dev->si_iosize_max = 254 * DEV_BSIZE;

    fdp->atp->flags &= ~ATAPI_F_MEDIA_CHANGED;
    afd_prevent_allow(fdp, 1);
    if (afd_sense(fdp))
	printf("afd%d: sense media type failed\n", fdp->lun);

    /* build disklabel and initilize slice tables */
    label = &fdp->disk.d_label;
    bzero(label, sizeof *label);
    label->d_secsize = fdp->cap.sector_size;
    label->d_nsectors = fdp->cap.sectors;  
    label->d_ntracks = fdp->cap.heads;
    label->d_ncylinders = fdp->cap.cylinders;
    label->d_secpercyl = fdp->cap.heads * fdp->cap.sectors;
    label->d_secperunit = label->d_secpercyl * fdp->cap.cylinders;

    return 0;
}

static int 
afdclose(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    struct afd_softc *fdp = dev->si_drv1;

    afd_prevent_allow(fdp, 0); 
    return 0;
}

static int 
afdioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    struct afd_softc *fdp = dev->si_drv1;

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
	return ENOIOCTL;
    }
}

static void 
afdstrategy(struct buf *bp)
{
    struct afd_softc *fdp = bp->b_dev->si_drv1;
    int32_t s;

    s = splbio();
    bufq_insert_tail(&fdp->buf_queue, bp);
    afd_start(fdp);
    splx(s);
}

static void 
afd_start(struct afd_softc *fdp)
{
    struct buf *bp = bufq_first(&fdp->buf_queue);
    u_int32_t lba, count;
    int8_t ccb[16];
    int8_t *data_ptr;
    
    if (!bp)
	return;

    bufq_remove(&fdp->buf_queue, bp);

    /* should reject all queued entries if media have changed. */
    if (fdp->atp->flags & ATAPI_F_MEDIA_CHANGED) {
	bp->b_error = EIO;
	bp->b_flags |= B_ERROR;
	biodone(bp);
	return;
    }

    lba = bp->b_blkno / (fdp->cap.sector_size / DEV_BSIZE);
    count = (bp->b_bcount + (fdp->cap.sector_size - 1)) / fdp->cap.sector_size;
    data_ptr = bp->b_data;
    bp->b_resid = 0; 

    bzero(ccb, sizeof(ccb));

    if (bp->b_flags & B_READ)
	ccb[0] = ATAPI_READ_BIG;
    else
	ccb[0] = ATAPI_WRITE_BIG;

    devstat_start_transaction(&fdp->stats);

    while (fdp->transfersize && (count > fdp->transfersize)) {
	ccb[2] = lba>>24;
	ccb[3] = lba>>16;
	ccb[4] = lba>>8;
	ccb[5] = lba;
	ccb[7] = fdp->transfersize>>8;
	ccb[8] = fdp->transfersize;

	atapi_queue_cmd(fdp->atp, ccb, data_ptr, 
			fdp->transfersize * fdp->cap.sector_size,
			(bp->b_flags & B_READ) ? A_READ : 0, 30,
			afd_partial_done, fdp, bp);

	count -= fdp->transfersize;
	lba += fdp->transfersize;
	data_ptr += fdp->transfersize * fdp->cap.sector_size;
    }

    ccb[2] = lba>>24;
    ccb[3] = lba>>16;
    ccb[4] = lba>>8;
    ccb[5] = lba;
    ccb[7] = count>>8;
    ccb[8] = count;

    atapi_queue_cmd(fdp->atp, ccb, data_ptr, count * fdp->cap.sector_size,
		    (bp->b_flags & B_READ) ? A_READ : 0, 30, afd_done, fdp, bp);
}

static void 
afd_partial_done(struct atapi_request *request)
{
    struct buf *bp = request->bp;

    if (request->result) {
	bp->b_error = atapi_error(request->device, request->result);
	bp->b_flags |= B_ERROR;
    }
    bp->b_resid += request->bytecount;
}

static void 
afd_done(struct atapi_request *request)
{
    struct buf *bp = request->bp;
    struct afd_softc *fdp = request->driver;

    if (request->result || (bp->b_flags & B_ERROR)) {
	bp->b_error = atapi_error(request->device, request->result);
	bp->b_flags |= B_ERROR;
    }
    else
	bp->b_resid += request->bytecount;
    devstat_end_transaction_buf(&fdp->stats, bp);
    biodone(bp);
    afd_start(fdp);
}

static int32_t 
afd_eject(struct afd_softc *fdp, int32_t close)
{
    int32_t error;
     
    if ((error = afd_start_stop(fdp, 0)) == EBUSY) {
	if (!close)
	    return 0;
	if ((error = afd_start_stop(fdp, 3)))
	    return error;
	return afd_prevent_allow(fdp, 1);
    }
    if (error)
	return error;
    if (close)
	return 0;
    if ((error = afd_prevent_allow(fdp, 0)))
	return error;
    fdp->atp->flags |= ATAPI_F_MEDIA_CHANGED;
    return afd_start_stop(fdp, 2);
}

static int32_t
afd_start_stop(struct afd_softc *fdp, int32_t start)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0x01, 0, 0, start,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int32_t error;

    error = atapi_immed_cmd(fdp->atp, ccb, NULL, 0, 0, 10);
    if (error)
	return atapi_error(fdp->atp, error);
    return atapi_error(fdp->atp, atapi_wait_ready(fdp->atp, 30));
}

static int32_t
afd_prevent_allow(struct afd_softc *fdp, int32_t lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
    return atapi_error(fdp->atp, atapi_immed_cmd(fdp->atp, ccb, NULL, 0, 0,30));
}
