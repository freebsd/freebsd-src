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
#include <sys/cdio.h>
#include <machine/bus.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>
#include <dev/ata/atapi-fd.h>

/* device structures */
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
	/* flags */	D_DISK | D_TRACKCLOSE,
};
static struct cdevsw afddisk_cdevsw;

/* prototypes */
static int afd_sense(struct afd_softc *);
static void afd_describe(struct afd_softc *);
static int afd_done(struct atapi_request *);
static int afd_eject(struct afd_softc *, int);
static int afd_start_stop(struct afd_softc *, int);
static int afd_prevent_allow(struct afd_softc *, int);

/* internal vars */
static u_int32_t afd_lun_map = 0;
static MALLOC_DEFINE(M_AFD, "AFD driver", "ATAPI floppy driver buffers");

int 
afdattach(struct ata_device *atadev)
{
    struct afd_softc *fdp;
    dev_t dev;

    fdp = malloc(sizeof(struct afd_softc), M_AFD, M_NOWAIT | M_ZERO);
    if (!fdp) {
	ata_prtdev(atadev, "out of memory\n");
	return 0;
    }

    fdp->device = atadev;
    fdp->lun = ata_get_lun(&afd_lun_map);
    ata_set_name(atadev, "afd", fdp->lun);
    bioq_init(&fdp->queue);

    if (afd_sense(fdp)) {
	free(fdp, M_AFD);
	return 0;
    }

    devstat_add_entry(&fdp->stats, "afd", fdp->lun, DEV_BSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
		      DEVSTAT_PRIORITY_WFD);
    dev = disk_create(fdp->lun, &fdp->disk, 0, &afd_cdevsw, &afddisk_cdevsw);
    dev->si_drv1 = fdp;
    fdp->dev = dev;
    fdp->dev->si_iosize_max = 256 * DEV_BSIZE;

    afd_describe(fdp);
    atadev->flags |= ATA_D_MEDIA_CHANGED;
    atadev->driver = fdp;
    return 1;
}

void
afddetach(struct ata_device *atadev)
{   
    struct afd_softc *fdp = atadev->driver;
    struct bio *bp;
    
    while ((bp = bioq_first(&fdp->queue))) {
	bioq_remove(&fdp->queue, bp);
	biofinish(bp, NULL, ENXIO);
    }
    disk_invalidate(&fdp->disk);
    disk_destroy(fdp->dev);
    devstat_remove_entry(&fdp->stats);
    ata_free_name(atadev);
    ata_free_lun(&afd_lun_map, fdp->lun);
    free(fdp, M_AFD);
    atadev->driver = NULL;
}   

static int 
afd_sense(struct afd_softc *fdp)
{
    int8_t ccb[16] = { ATAPI_MODE_SENSE_BIG, 0, ATAPI_REWRITEABLE_CAP_PAGE,
		       0, 0, 0, 0, sizeof(struct afd_cappage) >> 8,
		       sizeof(struct afd_cappage) & 0xff, 0, 0, 0, 0, 0, 0, 0 };
    int count, error = 0;

    /* The IOMEGA Clik! doesn't support reading the cap page, fake it */
    if (!strncmp(fdp->device->param->model, "IOMEGA Clik!", 12)) {
	fdp->cap.transfer_rate = 500;
	fdp->cap.heads = 1;
	fdp->cap.sectors = 2;
	fdp->cap.cylinders = 39441;
	fdp->cap.sector_size = 512;
	atapi_test_ready(fdp->device);
	return 0;
    }

    /* get drive capabilities, some drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
	if (!(error = atapi_queue_cmd(fdp->device, ccb, (caddr_t)&fdp->cap,
				      sizeof(struct afd_cappage),
				      ATPR_F_READ, 30, NULL, NULL)))
	    break;
    }
    if (error || fdp->cap.page_code != ATAPI_REWRITEABLE_CAP_PAGE)
	return 1;   
    fdp->cap.cylinders = ntohs(fdp->cap.cylinders);
    fdp->cap.sector_size = ntohs(fdp->cap.sector_size);
    fdp->cap.transfer_rate = ntohs(fdp->cap.transfer_rate);
    return 0;
}

static void 
afd_describe(struct afd_softc *fdp)
{
    if (bootverbose) {
	ata_prtdev(fdp->device,
		   "<%.40s/%.8s> rewriteable drive at ata%d as %s\n",
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
	}
	printf("\n");
    }
    else {
	ata_prtdev(fdp->device, "%luMB <%.40s> [%d/%d/%d] at ata%d-%s %s\n",
		   (fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors) /
		   ((1024L * 1024L) / fdp->cap.sector_size),	
		   fdp->device->param->model,
		   fdp->cap.cylinders, fdp->cap.heads, fdp->cap.sectors,
		   device_get_unit(fdp->device->channel->dev),
		   (fdp->device->unit == ATA_MASTER) ? "master" : "slave",
		   ata_mode2str(fdp->device->mode));
    }
}

static int
afdopen(dev_t dev, int flags, int fmt, struct thread *td)
{
    struct afd_softc *fdp = dev->si_drv1;

    /* hold off access to we are fully attached */
    while (ata_delayed_attach)
	tsleep(&ata_delayed_attach, PRIBIO, "afdopn", 1);

    atapi_test_ready(fdp->device);

    if (count_dev(dev) == 1)
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
afdclose(dev_t dev, int flags, int fmt, struct thread *td)
{
    struct afd_softc *fdp = dev->si_drv1;

    if (count_dev(dev) == 1)
	afd_prevent_allow(fdp, 0); 
    return 0;
}

static int 
afdioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
    struct afd_softc *fdp = dev->si_drv1;

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

static void 
afdstrategy(struct bio *bp)
{
    struct afd_softc *fdp = bp->bio_dev->si_drv1;
    int s;

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

    s = splbio();
    bioqdisksort(&fdp->queue, bp);
    splx(s);
    ata_start(fdp->device->channel);
}

void 
afd_start(struct ata_device *atadev)
{
    struct afd_softc *fdp = atadev->driver;
    struct bio *bp = bioq_first(&fdp->queue);
    u_int32_t lba;
    u_int16_t count;
    int8_t ccb[16];
    caddr_t data_ptr;

    if (!bp)
	return;

    bioq_remove(&fdp->queue, bp);

    /* should reject all queued entries if media have changed. */
    if (fdp->device->flags & ATA_D_MEDIA_CHANGED) {
	biofinish(bp, NULL, EIO);
	return;
    }

    lba = bp->bio_pblkno;
    count = bp->bio_bcount / fdp->cap.sector_size;
    data_ptr = bp->bio_data;
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

    devstat_start_transaction(&fdp->stats);

    atapi_queue_cmd(fdp->device, ccb, data_ptr, count * fdp->cap.sector_size,
		    (bp->bio_cmd == BIO_READ) ? ATPR_F_READ : 0, 30,
		    afd_done, bp);
}

static int 
afd_done(struct atapi_request *request)
{
    struct bio *bp = request->driver;
    struct afd_softc *fdp = request->device->driver;

    if (request->error || (bp->bio_flags & BIO_ERROR)) {
	bp->bio_error = request->error;
	bp->bio_flags |= BIO_ERROR;
    }
    else
	bp->bio_resid = bp->bio_bcount - request->donecount;
    biofinish(bp, &fdp->stats, 0);
    return 0;
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

    return atapi_queue_cmd(fdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
}

static int
afd_prevent_allow(struct afd_softc *fdp, int lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
    if (!strncmp(fdp->device->param->model, "IOMEGA Clik!", 12))
	return 0;
    return atapi_queue_cmd(fdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
}
