/*-
 * Copyright (c) 1998 - 2005 Søren Schmidt <sos@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cdio.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/bus.h>
#include <geom/geom_disk.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-fd.h>
#include <ata_if.h>


/* prototypes */
static disk_open_t afd_open;
static disk_close_t afd_close;
static disk_strategy_t afd_strategy;
static int afd_sense(device_t);
static void afd_describe(device_t);
static void afd_done(struct ata_request *);
static int afd_prevent_allow(device_t, int);
static int afd_test_ready(device_t);

/* internal vars */
static MALLOC_DEFINE(M_AFD, "AFD driver", "ATAPI floppy driver buffers");

static int 
afd_probe(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);

    if ((atadev->param.config & ATA_PROTO_ATAPI) &&
	(atadev->param.config & ATA_ATAPI_TYPE_MASK) == ATA_ATAPI_TYPE_DIRECT)
	return 0;  
    else
	return ENXIO;
}

static int 
afd_attach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    struct afd_softc *fdp;

    if (!(fdp = malloc(sizeof(struct afd_softc), M_AFD, M_NOWAIT | M_ZERO))) {
	device_printf(dev, "out of memory\n");
	return ENOMEM;
    }
    device_set_ivars(dev, fdp);
    ATA_SETMODE(device_get_parent(dev), dev);

    if (afd_sense(dev)) {
	device_set_ivars(dev, NULL);
	free(fdp, M_AFD);
	return ENXIO;
    }
    atadev->flags |= ATA_D_MEDIA_CHANGED;

    /* announce we are here */
    afd_describe(dev);

    /* create the disk device */
    fdp->disk = disk_alloc();
    fdp->disk->d_open = afd_open;
    fdp->disk->d_close = afd_close;
    fdp->disk->d_strategy = afd_strategy;
    fdp->disk->d_name = "afd";
    fdp->disk->d_drv1 = dev;
    if (ch->dma)
	fdp->disk->d_maxsize = ch->dma->max_iosize;
    else
	fdp->disk->d_maxsize = DFLTPHYS;
    fdp->disk->d_unit = device_get_unit(dev);
    disk_create(fdp->disk, DISK_VERSION);
    return 0;
}

static int
afd_detach(device_t dev)
{   
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct afd_softc *fdp = device_get_ivars(dev);
    
    /* detroy disk from the system so we dont get any further requests */
    disk_destroy(fdp->disk);

    /* fail requests on the queue and any thats "in flight" for this device */
    ata_fail_requests(ch, dev);

    /* dont leave anything behind */
    device_set_ivars(dev, NULL);
    free(fdp, M_AFD);
    return 0;
}

static void
afd_shutdown(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (atadev->param.support.command2 & ATA_SUPPORT_FLUSHCACHE)
	ata_controlcmd(atadev, ATA_FLUSHCACHE, 0, 0, 0);
}

static int
afd_reinit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    struct afd_softc *fdp = device_get_ivars(dev);
    
    if (((atadev->unit == ATA_MASTER) && !(ch->devices & ATA_ATAPI_MASTER)) ||
	((atadev->unit == ATA_SLAVE) && !(ch->devices & ATA_ATAPI_SLAVE))) {
	device_set_ivars(dev, NULL);
	free(fdp, M_AFD);
	return 1;
    }
    ATA_SETMODE(device_get_parent(dev), dev);
    return 0;
}

static int
afd_open(struct disk *dp)
{
    device_t dev = dp->d_drv1;
    struct ata_device *atadev = device_get_softc(dev);
    struct afd_softc *fdp = device_get_ivars(dev);

    if (!fdp) 
	return ENXIO;
    if (!device_is_attached(dev))
	return EBUSY;

    afd_test_ready(dev);
    afd_prevent_allow(dev, 1);

    if (afd_sense(dev))
	device_printf(dev, "sense media type failed\n");
    atadev->flags &= ~ATA_D_MEDIA_CHANGED;

    fdp->disk->d_sectorsize = fdp->cap.sector_size;
    fdp->disk->d_mediasize = (off_t)fdp->cap.sector_size * fdp->cap.sectors *
	fdp->cap.heads * fdp->cap.cylinders;
    fdp->disk->d_fwsectors = fdp->cap.sectors;
    fdp->disk->d_fwheads = fdp->cap.heads;
  
    return 0;
}

static int 
afd_close(struct disk *dp)
{
    device_t dev = dp->d_drv1;

    afd_prevent_allow(dev, 0); 
    return 0;
}

static void 
afd_strategy(struct bio *bp)
{
    device_t dev = bp->bio_disk->d_drv1;
    struct ata_device *atadev = device_get_softc(dev);
    struct afd_softc *fdp = device_get_ivars(dev);
    struct ata_request *request;
    u_int16_t count;
    int8_t ccb[16];

    /* if it's a null transfer, return immediatly. */
    if (bp->bio_bcount == 0) {
	bp->bio_resid = 0;
	biodone(bp);
	return;
    }

    /* should reject all queued entries if media have changed. */
    if (atadev->flags & ATA_D_MEDIA_CHANGED) {
	biofinish(bp, NULL, EIO);
	return;
    }

    count = bp->bio_bcount / fdp->cap.sector_size;
    bp->bio_resid = bp->bio_bcount; 

    bzero(ccb, sizeof(ccb));

    if (bp->bio_cmd == BIO_READ)
	ccb[0] = ATAPI_READ_BIG;
    else
	ccb[0] = ATAPI_WRITE_BIG;

    ccb[2] = bp->bio_pblkno >> 24;
    ccb[3] = bp->bio_pblkno >> 16;
    ccb[4] = bp->bio_pblkno >> 8;
    ccb[5] = bp->bio_pblkno;
    ccb[7] = count>>8;
    ccb[8] = count;

    if (!(request = ata_alloc_request())) {
	biofinish(bp, NULL, ENOMEM);
	return;
    }
    request->dev = dev;
    request->bio = bp;
    bcopy(ccb, request->u.atapi.ccb,
	  (atadev->param.config & ATA_PROTO_MASK) == 
	  ATA_PROTO_ATAPI_12 ? 16 : 12);
    request->data = bp->bio_data;
    request->bytecount = count * fdp->cap.sector_size;
    request->transfersize = min(request->bytecount, 65534);
    request->timeout = (ccb[0] == ATAPI_WRITE_BIG) ? 60 : 30;
    request->retries = 2;
    request->callback = afd_done;
    switch (bp->bio_cmd) {
    case BIO_READ:
	request->flags = (ATA_R_ATAPI | ATA_R_READ);
	break;
    case BIO_WRITE:
	request->flags = (ATA_R_ATAPI | ATA_R_WRITE);
	break;
    default:
	device_printf(dev, "unknown BIO operation\n");
	ata_free_request(request);
	biofinish(bp, NULL, EIO);
	return;
    }
    if (atadev->mode >= ATA_DMA)
	request->flags |= ATA_R_DMA;
    request->flags |= ATA_R_ORDERED;
    ata_queue_request(request);
}

static void 
afd_done(struct ata_request *request)
{
    struct bio *bp = request->bio;

    /* finish up transfer */
    if ((bp->bio_error = request->result))
	bp->bio_flags |= BIO_ERROR;
    bp->bio_resid = bp->bio_bcount - request->donecount;
    biodone(bp);
    ata_free_request(request);
}

static int 
afd_sense(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct afd_softc *fdp = device_get_ivars(dev);
    int8_t ccb[16] = { ATAPI_MODE_SENSE_BIG, 0, ATAPI_REWRITEABLE_CAP_PAGE,
		       0, 0, 0, 0, sizeof(struct afd_cappage) >> 8,
		       sizeof(struct afd_cappage) & 0xff, 0, 0, 0, 0, 0, 0, 0 };
    int count;

    /* The IOMEGA Clik! doesn't support reading the cap page, fake it */
    if (!strncmp(atadev->param.model, "IOMEGA Clik!", 12)) {
	fdp->cap.transfer_rate = 500;
	fdp->cap.heads = 1;
	fdp->cap.sectors = 2;
	fdp->cap.cylinders = 39441;
	fdp->cap.sector_size = 512;
	afd_test_ready(dev);
	return 0;
    }

    /* get drive capabilities, some bugridden drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
	if (!ata_atapicmd(atadev, ccb, (caddr_t)&fdp->cap,
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

static int
afd_prevent_allow(device_t dev, int lock)
{
    struct ata_device *atadev = device_get_softc(dev);
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    
    if (!strncmp(atadev->param.model, "IOMEGA Clik!", 12))
	return 0;
    return ata_atapicmd(atadev, ccb, NULL, 0, 0, 30);
}

static int
afd_test_ready(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(atadev, ccb, NULL, 0, 0, 30);
}

static void 
afd_describe(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    struct afd_softc *fdp = device_get_ivars(dev);

    if (bootverbose) {
	device_printf(dev, "<%.40s/%.8s> removable drive at ata%d as %s\n",
		      atadev->param.model, atadev->param.revision,
		      device_get_unit(ch->dev),
		      (atadev->unit == ATA_MASTER) ? "master" : "slave");
	device_printf(dev,
		      "%luMB (%u sectors), %u cyls, %u heads, %u S/T, %u B/S\n",
		      (fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors) /
		      ((1024L * 1024L) / fdp->cap.sector_size),
		      fdp->cap.cylinders * fdp->cap.heads * fdp->cap.sectors,
		      fdp->cap.cylinders, fdp->cap.heads, fdp->cap.sectors,
		      fdp->cap.sector_size);
	device_printf(dev, "%dKB/s,", fdp->cap.transfer_rate / 8);
	printf(" %s\n", ata_mode2str(atadev->mode));
	if (fdp->cap.medium_type) {
	    device_printf(dev, "Medium: ");
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
	device_printf(dev, "REMOVABLE <%.40s/%.8s> at ata%d-%s %s\n",
		      atadev->param.model, atadev->param.revision,
		      device_get_unit(ch->dev),
		      (atadev->unit == ATA_MASTER) ? "master" : "slave",
		      ata_mode2str(atadev->mode));
    }
}

static device_method_t afd_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,     afd_probe),
    DEVMETHOD(device_attach,    afd_attach),
    DEVMETHOD(device_detach,    afd_detach),
    DEVMETHOD(device_shutdown,  afd_shutdown),
    
    /* ATA methods */
    DEVMETHOD(ata_reinit,       afd_reinit),
    
    { 0, 0 }
};
    
static driver_t afd_driver = {
    "afd",
    afd_methods,
    0,
};

static devclass_t afd_devclass;

DRIVER_MODULE(afd, ata, afd_driver, afd_devclass, NULL, NULL);
MODULE_VERSION(afd, 1);
MODULE_DEPEND(afd, ata, 1, 1, 1);

