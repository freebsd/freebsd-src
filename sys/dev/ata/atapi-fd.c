/*-
 * Copyright (c) 1998 - 2003 Søren Schmidt <sos@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cdio.h>
#include <sys/taskqueue.h>
#include <machine/bus.h>
#include <geom/geom_disk.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-fd.h>

/* prototypes */
static	disk_open_t	afd_open;
static	disk_close_t	afd_close;
#ifdef notyet
static	disk_ioctl_t	afd_ioctl;
#endif
static disk_strategy_t	afdstrategy;
static void afd_detach(struct ata_device *atadev);
static void afd_start(struct ata_device *atadev);
static int afd_sense(struct afd_softc *);
static void afd_describe(struct afd_softc *);
static void afd_done(struct ata_request *);
static int afd_eject(struct afd_softc *, int);
static int afd_start_stop(struct afd_softc *, int);
static int afd_prevent_allow(struct afd_softc *, int);
static int afd_test_ready(struct ata_device *atadev);

/* internal vars */
static u_int32_t afd_lun_map = 0;
static MALLOC_DEFINE(M_AFD, "AFD driver", "ATAPI floppy driver buffers");

void 
afd_attach(struct ata_device *atadev)
{
    struct afd_softc *fdp;

    fdp = malloc(sizeof(struct afd_softc), M_AFD, M_NOWAIT | M_ZERO);
    if (!fdp) {
	ata_prtdev(atadev, "out of memory\n");
	return;
    }

    fdp->device = atadev;
    fdp->lun = ata_get_lun(&afd_lun_map);
    ata_set_name(atadev, "afd", fdp->lun);
    bioq_init(&fdp->queue);
    mtx_init(&fdp->queue_mtx, "ATAPI FD bioqueue lock", MTX_DEF, 0);  

    if (afd_sense(fdp)) {
	free(fdp, M_AFD);
	return;
    }

    /* use DMA if allowed and if drive/controller supports it */
    if (atapi_dma && atadev->channel->dma &&
	(atadev->param->config & ATA_DRQ_MASK) != ATA_DRQ_INTR)
	atadev->setmode(atadev, ATA_DMA_MAX);
    else
	atadev->setmode(atadev, ATA_PIO_MAX);

    /* setup the function ptrs */
    atadev->detach = afd_detach;
    atadev->start = afd_start;
    atadev->softc = fdp;
    atadev->flags |= ATA_D_MEDIA_CHANGED;

    /* lets create the disk device */
    fdp->disk.d_open = afd_open;
    fdp->disk.d_close = afd_close;
#ifdef notyet
    fdp->disk.d_ioctl = afd_ioctl;
#endif
    fdp->disk.d_strategy = afdstrategy;
    fdp->disk.d_name = "afd";
    fdp->disk.d_drv1 = fdp;
    if (atadev->channel->dma)
	fdp->disk.d_maxsize = atadev->channel->dma->max_iosize;
    else
	fdp->disk.d_maxsize = DFLTPHYS;
    disk_create(fdp->lun, &fdp->disk, DISKFLAG_NOGIANT, NULL, NULL);

    /* announce we are here */
    afd_describe(fdp);
}

static void
afd_detach(struct ata_device *atadev)
{   
    struct afd_softc *fdp = atadev->softc;
    
    mtx_lock(&fdp->queue_mtx);
    bioq_flush(&fdp->queue, NULL, ENXIO);
    mtx_unlock(&fdp->queue_mtx);
    disk_destroy(&fdp->disk);
    ata_prtdev(atadev, "WARNING - removed from configuration\n");
    ata_free_name(atadev);
    ata_free_lun(&afd_lun_map, fdp->lun);
    atadev->attach = NULL;
    atadev->detach = NULL;
    atadev->start = NULL;
    atadev->softc = NULL;
    atadev->flags = 0;
    free(fdp, M_AFD);
}   

static int 
afd_sense(struct afd_softc *fdp)
{
    int8_t ccb[16] = { ATAPI_MODE_SENSE_BIG, 0, ATAPI_REWRITEABLE_CAP_PAGE,
		       0, 0, 0, 0, sizeof(struct afd_cappage) >> 8,
		       sizeof(struct afd_cappage) & 0xff, 0, 0, 0, 0, 0, 0, 0 };
    int count;

    /* The IOMEGA Clik! doesn't support reading the cap page, fake it */
    if (!strncmp(fdp->device->param->model, "IOMEGA Clik!", 12)) {
	fdp->cap.transfer_rate = 500;
	fdp->cap.heads = 1;
	fdp->cap.sectors = 2;
	fdp->cap.cylinders = 39441;
	fdp->cap.sector_size = 512;
	afd_test_ready(fdp->device);
	return 0;
    }

    /* get drive capabilities, some bugridden drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
	if (!ata_atapicmd(fdp->device, ccb, (caddr_t)&fdp->cap,
			  sizeof(struct afd_cappage), ATA_R_READ, 30) &&
	    fdp->cap.page_code == ATAPI_REWRITEABLE_CAP_PAGE) {
	    fdp->cap.cylinders = ntohs(fdp->cap.cylinders);
	    fdp->cap.sector_size = ntohs(fdp->cap.sector_size);
	    fdp->cap.transfer_rate = ntohs(fdp->cap.transfer_rate);
	    return 0;
	}
    }
    return 1;
}

static void 
afd_describe(struct afd_softc *fdp)
{
    if (bootverbose) {
	ata_prtdev(fdp->device,
		   "<%.40s/%.8s> removable drive at ata%d as %s\n",
		   fdp->device->param->model, fdp->device->param->revision,
		   device_get_unit(fdp->device->channel->dev),
		   (fdp->device->unit == ATA_MASTER) ? "master" : "slave");
	ata_prtdev(fdp->device,
		   "%luMB (%u sectors), %u cyls, %u heads, %u S/T, %u B/S\n",
		   (fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors) / 
		   ((1024L * 1024L) / fdp->cap.sector_size),
		   fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors,
		   fdp->cap.cylinders, fdp->cap.heads, fdp->cap.sectors,
		   fdp->cap.sector_size);
	ata_prtdev(fdp->device, "%dKB/s,", fdp->cap.transfer_rate / 8);
	printf(" %s\n", ata_mode2str(fdp->device->mode));
	if (fdp->cap.medium_type) {
	    ata_prtdev(fdp->device, "Medium: ");
	    switch (fdp->cap.medium_type) {
	    case MFD_2DD:
		printf("720KB DD disk"); break;

	    case MFD_HD_12:
		printf("1.2MB HD disk"); break;

	    case MFD_HD_144:
		printf("1.44MB HD disk"); break;

	    case MFD_UHD: 
		printf("120MB UHD disk"); break;

	    default:
		printf("Unknown (0x%x)", fdp->cap.medium_type);
	    }
	    if (fdp->cap.wp) printf(", writeprotected");
	    printf("\n");
	}
    }
    else {
	ata_prtdev(fdp->device, "REMOVABLE <%.40s> at ata%d-%s %s\n",
		   fdp->device->param->model,
		   device_get_unit(fdp->device->channel->dev),
		   (fdp->device->unit == ATA_MASTER) ? "master" : "slave",
		   ata_mode2str(fdp->device->mode));
    }
}

static int
afd_open(struct disk *dp)
{
    struct afd_softc *fdp = dp->d_drv1;

    if (fdp->device->flags & ATA_D_DETACHING)
	return ENXIO;

    afd_test_ready(fdp->device);

    afd_prevent_allow(fdp, 1);

    if (afd_sense(fdp))
	ata_prtdev(fdp->device, "sense media type failed\n");

    fdp->device->flags &= ~ATA_D_MEDIA_CHANGED;

    fdp->disk.d_sectorsize = fdp->cap.sector_size;
    fdp->disk.d_mediasize = (off_t)fdp->cap.sector_size * fdp->cap.sectors *
	fdp->cap.heads * fdp->cap.cylinders;
    fdp->disk.d_fwsectors = fdp->cap.sectors;
    fdp->disk.d_fwheads = fdp->cap.heads;
  
    return 0;
}

static int 
afd_close(struct disk *dp)
{
    struct afd_softc *fdp = dp->d_drv1;

    afd_prevent_allow(fdp, 0); 
    if (0)
	afd_eject(fdp, 0);	/* to keep gcc quiet */

    return 0;
}

#ifdef notyet
static int 
afd_ioctl(struct disk *dp, u_long cmd, void *addr, int flag, struct thread *td)
{
    struct afd_softc *fdp = dp->d_drv1;

    switch (cmd) {
    case CDIOCEJECT:
	if (count_dev(dev) > 1)
	    return EBUSY;
	return afd_eject(fdp, 0);

    case CDIOCCLOSE:
	if (count_dev(dev) > 1)
	    return 0;
	return afd_eject(fdp, 1);

    default:
	return ENOIOCTL;
    }
}
#endif

static void 
afdstrategy(struct bio *bp)
{
    struct afd_softc *fdp = bp->bio_disk->d_drv1;

    if (fdp->device->flags & ATA_D_DETACHING) {
	biofinish(bp, NULL, ENXIO);
	return;
    }

    /* if it's a null transfer, return immediatly. */
    if (bp->bio_bcount == 0) {
	bp->bio_resid = 0;
	biodone(bp);
	return;
    }

    mtx_lock(&fdp->queue_mtx);
    bioq_disksort(&fdp->queue, bp);
    mtx_unlock(&fdp->queue_mtx);
    ata_start(fdp->device->channel);
}

static void 
afd_start(struct ata_device *atadev)
{
    struct afd_softc *fdp = atadev->softc;
    struct bio *bp;
    struct ata_request *request;
    u_int32_t lba;
    u_int16_t count;
    int8_t ccb[16];


    mtx_lock(&fdp->queue_mtx);
    bp = bioq_first(&fdp->queue);
    if (!bp) {
	mtx_unlock(&fdp->queue_mtx);
	return;
    }
    bioq_remove(&fdp->queue, bp);
    mtx_unlock(&fdp->queue_mtx);

    /* should reject all queued entries if media have changed. */
    if (fdp->device->flags & ATA_D_MEDIA_CHANGED) {
	biofinish(bp, NULL, EIO);
	return;
    }

    lba = bp->bio_pblkno;
    count = bp->bio_bcount / fdp->cap.sector_size;
    bp->bio_resid = bp->bio_bcount; 

    bzero(ccb, sizeof(ccb));

    if (bp->bio_cmd == BIO_READ)
	ccb[0] = ATAPI_READ_BIG;
    else
	ccb[0] = ATAPI_WRITE_BIG;

    ccb[2] = lba>>24;
    ccb[3] = lba>>16;
    ccb[4] = lba>>8;
    ccb[5] = lba;
    ccb[7] = count>>8;
    ccb[8] = count;

    if (!(request = ata_alloc_request())) {
	biofinish(bp, NULL, EIO);
	return;
    }
    request->device = atadev;
    request->driver = bp;
    bcopy(ccb, request->u.atapi.ccb,
	  (request->device->param->config & ATA_PROTO_MASK) == 
	  ATA_PROTO_ATAPI_12 ? 16 : 12);
    request->data = bp->bio_data;
    request->bytecount = count * fdp->cap.sector_size;
    request->transfersize = min(request->bytecount, 65534);
    request->timeout = (ccb[0] == ATAPI_WRITE_BIG) ? 60 : 30;
    request->retries = 2;
    request->callback = afd_done;
    switch (bp->bio_cmd) {
    case BIO_READ:
	request->flags |= (ATA_R_SKIPSTART | ATA_R_ATAPI | ATA_R_READ);
	break;
    case BIO_WRITE:
	request->flags |= (ATA_R_SKIPSTART | ATA_R_ATAPI | ATA_R_WRITE);
	break;
    default:
	ata_prtdev(atadev, "unknown BIO operation\n");
	ata_free_request(request);
	biofinish(bp, NULL, EIO);
	return;
    }
    ata_queue_request(request);

}

static void 
afd_done(struct ata_request *request)
{
    struct bio *bp = request->driver;

    /* finish up transfer */
    if ((bp->bio_error = request->result))
	bp->bio_flags |= BIO_ERROR;
    bp->bio_resid = bp->bio_bcount - request->donecount;
    biodone(bp);
    ata_free_request(request);
}

static int 
afd_eject(struct afd_softc *fdp, int close)
{
    int error;
     
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
    fdp->device->flags |= ATA_D_MEDIA_CHANGED;
    return afd_start_stop(fdp, 2);
}

static int
afd_start_stop(struct afd_softc *fdp, int start)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0, 0, 0, start,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(fdp->device, ccb, NULL, 0, 0, 30);
}

static int
afd_prevent_allow(struct afd_softc *fdp, int lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
    if (!strncmp(fdp->device->param->model, "IOMEGA Clik!", 12))
	return 0;
    return ata_atapicmd(fdp->device, ccb, NULL, 0, 0, 30);
}

static int
afd_test_ready(struct ata_device *atadev)
{
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(atadev, ccb, NULL, 0, 0, 30);
}
