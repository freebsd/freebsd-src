/*-
 * Copyright (c) 1998 - 2007 Søren Schmidt <sos@FreeBSD.org>
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

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/cons.h>
#include <sys/sysctl.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/md_var.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <geom/geom_disk.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>
#include <ata_if.h>

/* prototypes */
static void ad_init(device_t);
static void ad_done(struct ata_request *);
static void ad_describe(device_t dev);
static int ad_version(u_int16_t);
static disk_strategy_t ad_strategy;
static disk_ioctl_t ad_ioctl;
static dumper_t ad_dump;

/*
 * Most platforms map firmware geom to actual, but some don't.  If
 * not overridden, default to nothing.
 */
#ifndef ad_firmware_geom_adjust
#define ad_firmware_geom_adjust(dev, disk)
#endif

/* local vars */
static MALLOC_DEFINE(M_AD, "ad_driver", "ATA disk driver");

static int
ad_probe(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (!(atadev->param.config & ATA_PROTO_ATAPI) ||
	(atadev->param.config == ATA_CFA_MAGIC1) ||
	(atadev->param.config == ATA_CFA_MAGIC2) ||
	(atadev->param.config == ATA_CFA_MAGIC3))
	return 0;
    else
	return ENXIO;
}

static int
ad_attach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    struct ad_softc *adp;
    u_int32_t lbasize;
    u_int64_t lbasize48;

    /* check that we have a virgin disk to attach */
    if (device_get_ivars(dev))
	return EEXIST;

    if (!(adp = malloc(sizeof(struct ad_softc), M_AD, M_NOWAIT | M_ZERO))) {
	device_printf(dev, "out of memory\n");
	return ENOMEM;
    }
    device_set_ivars(dev, adp);

    if ((atadev->param.atavalid & ATA_FLAG_54_58) &&
	atadev->param.current_heads && atadev->param.current_sectors) {
	adp->heads = atadev->param.current_heads;
	adp->sectors = atadev->param.current_sectors;
	adp->total_secs = (u_int32_t)atadev->param.current_size_1 |
			  ((u_int32_t)atadev->param.current_size_2 << 16);
    }
    else {
	adp->heads = atadev->param.heads;
	adp->sectors = atadev->param.sectors;
	adp->total_secs = atadev->param.cylinders * adp->heads * adp->sectors;  
    }
    lbasize = (u_int32_t)atadev->param.lba_size_1 |
	      ((u_int32_t)atadev->param.lba_size_2 << 16);

    /* does this device need oldstyle CHS addressing */
    if (!ad_version(atadev->param.version_major) || !lbasize)
	atadev->flags |= ATA_D_USE_CHS;

    /* use the 28bit LBA size if valid or bigger than the CHS mapping */
    if (atadev->param.cylinders == 16383 || adp->total_secs < lbasize)
	adp->total_secs = lbasize;

    /* use the 48bit LBA size if valid */
    lbasize48 = ((u_int64_t)atadev->param.lba_size48_1) |
		((u_int64_t)atadev->param.lba_size48_2 << 16) |
		((u_int64_t)atadev->param.lba_size48_3 << 32) |
		((u_int64_t)atadev->param.lba_size48_4 << 48);
    if ((atadev->param.support.command2 & ATA_SUPPORT_ADDRESS48) &&
	lbasize48 > ATA_MAX_28BIT_LBA)
	adp->total_secs = lbasize48;

    /* init device parameters */
    ad_init(dev);

    /* announce we are here */
    ad_describe(dev);

    /* create the disk device */
    adp->disk = disk_alloc();
    adp->disk->d_strategy = ad_strategy;
    adp->disk->d_ioctl = ad_ioctl;
    adp->disk->d_dump = ad_dump;
    adp->disk->d_name = "ad";
    adp->disk->d_drv1 = dev;
    if (ch->dma)
	adp->disk->d_maxsize = ch->dma->max_iosize;
    else
	adp->disk->d_maxsize = DFLTPHYS;
    adp->disk->d_sectorsize = DEV_BSIZE;
    adp->disk->d_mediasize = DEV_BSIZE * (off_t)adp->total_secs;
    adp->disk->d_fwsectors = adp->sectors;
    adp->disk->d_fwheads = adp->heads;
    adp->disk->d_unit = device_get_unit(dev);
    if (atadev->param.support.command2 & ATA_SUPPORT_FLUSHCACHE)
	adp->disk->d_flags = DISKFLAG_CANFLUSHCACHE;
    snprintf(adp->disk->d_ident, sizeof(adp->disk->d_ident), "ad:%s",
	atadev->param.serial);
    disk_create(adp->disk, DISK_VERSION);
    device_add_child(dev, "subdisk", device_get_unit(dev));
    ad_firmware_geom_adjust(dev, adp->disk);
    bus_generic_attach(dev);
    return 0;
}

static int
ad_detach(device_t dev)
{
    struct ad_softc *adp = device_get_ivars(dev);
    device_t *children;
    int nchildren, i;

    /* check that we have a valid disk to detach */
    if (!device_get_ivars(dev))
	return ENXIO;
    
    /* detach & delete all children */
    if (!device_get_children(dev, &children, &nchildren)) {
	for (i = 0; i < nchildren; i++)
	    if (children[i])
		device_delete_child(dev, children[i]);
	free(children, M_TEMP);
    }

    /* detroy disk from the system so we dont get any further requests */
    disk_destroy(adp->disk);

    /* fail requests on the queue and any thats "in flight" for this device */
    ata_fail_requests(dev);

    /* dont leave anything behind */
    device_set_ivars(dev, NULL);
    free(adp, M_AD);
    return 0;
}

static void
ad_shutdown(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (atadev->param.support.command2 & ATA_SUPPORT_FLUSHCACHE)
	ata_controlcmd(dev, ATA_FLUSHCACHE, 0, 0, 0);
}

static int
ad_reinit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);

    /* if detach pending, return error */
    if (((atadev->unit == ATA_MASTER) && !(ch->devices & ATA_ATA_MASTER)) ||
	((atadev->unit == ATA_SLAVE) && !(ch->devices & ATA_ATA_SLAVE))) {
	return 1;
    }
    ad_init(dev);
    return 0;
}

static void 
ad_strategy(struct bio *bp)
{
    device_t dev =  bp->bio_disk->d_drv1;
    struct ata_device *atadev = device_get_softc(dev);
    struct ata_request *request;

    if (!(request = ata_alloc_request())) {
	device_printf(dev, "FAILURE - out of memory in start\n");
	biofinish(bp, NULL, ENOMEM);
	return;
    }

    /* setup request */
    request->dev = dev;
    request->bio = bp;
    request->callback = ad_done;
    request->timeout = 5;
    request->retries = 2;
    request->data = bp->bio_data;
    request->bytecount = bp->bio_bcount;
    request->u.ata.lba = bp->bio_pblkno;
    request->u.ata.count = request->bytecount / DEV_BSIZE;
    request->transfersize = min(bp->bio_bcount, atadev->max_iosize);

    switch (bp->bio_cmd) {
    case BIO_READ:
	request->flags = ATA_R_READ;
	if (atadev->mode >= ATA_DMA) {
	    request->u.ata.command = ATA_READ_DMA;
	    request->flags |= ATA_R_DMA;
	}
	else if (request->transfersize > DEV_BSIZE)
	    request->u.ata.command = ATA_READ_MUL;
	else
	    request->u.ata.command = ATA_READ;
	break;
    case BIO_WRITE:
	request->flags = ATA_R_WRITE;
	if (atadev->mode >= ATA_DMA) {
	    request->u.ata.command = ATA_WRITE_DMA;
	    request->flags |= ATA_R_DMA;
	}
	else if (request->transfersize > DEV_BSIZE)
	    request->u.ata.command = ATA_WRITE_MUL;
	else
	    request->u.ata.command = ATA_WRITE;
	break;
    case BIO_FLUSH:
	request->u.ata.lba = 0;
	request->u.ata.count = 0;
	request->u.ata.feature = 0;
	request->bytecount = 0;
	request->transfersize = 0;
	request->flags = ATA_R_CONTROL;
	request->u.ata.command = ATA_FLUSHCACHE;
	break;
    default:
	device_printf(dev, "FAILURE - unknown BIO operation\n");
	ata_free_request(request);
	biofinish(bp, NULL, EIO);
	return;
    }
    request->flags |= ATA_R_ORDERED;
    ata_queue_request(request);
}

static void
ad_done(struct ata_request *request)
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
ad_ioctl(struct disk *disk, u_long cmd, void *data, int flag, struct thread *td)
{
    return ata_device_ioctl(disk->d_drv1, cmd, data);
}

static int
ad_dump(void *arg, void *virtual, vm_offset_t physical,
	off_t offset, size_t length)
{
    struct disk *dp = arg;
    struct bio bp;

    /* length zero is special and really means flush buffers to media */
    if (!length) {
        struct ata_device *atadev = device_get_softc(dp->d_drv1);
	int error = 0;

	if (atadev->param.support.command2 & ATA_SUPPORT_FLUSHCACHE)
	    error = ata_controlcmd(dp->d_drv1, ATA_FLUSHCACHE, 0, 0, 0);
	return error;
    }

    bzero(&bp, sizeof(struct bio));
    bp.bio_disk = dp;
    bp.bio_pblkno = offset / DEV_BSIZE;
    bp.bio_bcount = length;
    bp.bio_data = virtual;
    bp.bio_cmd = BIO_WRITE;
    ad_strategy(&bp);
    return bp.bio_error;
}

static void
ad_init(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);

    ATA_SETMODE(device_get_parent(dev), dev);

    /* enable readahead caching */
    if (atadev->param.support.command1 & ATA_SUPPORT_LOOKAHEAD)
	ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_ENAB_RCACHE, 0, 0);

    /* enable write caching if supported and configured */
    if (atadev->param.support.command1 & ATA_SUPPORT_WRITECACHE) {
	if (ata_wc)
	    ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_ENAB_WCACHE, 0, 0);
	else
	    ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_DIS_WCACHE, 0, 0);
    }

    /* use multiple sectors/interrupt if device supports it */
    if (ad_version(atadev->param.version_major)) {
	int secsperint = max(1, min(atadev->param.sectors_intr, 16));

	if (!ata_controlcmd(dev, ATA_SET_MULTI, 0, 0, secsperint))
	    atadev->max_iosize = secsperint * DEV_BSIZE;
    }
    else
	atadev->max_iosize = DEV_BSIZE;
}

void
ad_describe(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    struct ad_softc *adp = device_get_ivars(dev);
    u_int8_t *marker, vendor[64], product[64];

    /* try to seperate the ATA model string into vendor and model parts */
    if ((marker = index(atadev->param.model, ' ')) ||
	(marker = index(atadev->param.model, '-'))) {
	int len = (marker - atadev->param.model);

	strncpy(vendor, atadev->param.model, len);
	vendor[len++] = 0;
	strcat(vendor, " ");
	strncpy(product, atadev->param.model + len, 40 - len);
	vendor[40 - len] = 0;
    }
    else {
	if (!strncmp(atadev->param.model, "ST", 2))
	    strcpy(vendor, "Seagate ");
	else
	    strcpy(vendor, "");
	strncpy(product, atadev->param.model, 40);
    }

    device_printf(dev, "%juMB <%s%s %.8s> at ata%d-%s %s%s\n",
		  adp->total_secs / (1048576 / DEV_BSIZE),
		  vendor, product, atadev->param.revision,
		  device_get_unit(ch->dev),
		  (atadev->unit == ATA_MASTER) ? "master" : "slave",
		  (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
		  ata_mode2str(atadev->mode));
    if (bootverbose) {
	device_printf(dev, "%ju sectors [%juC/%dH/%dS] "
		      "%d sectors/interrupt %d depth queue\n", adp->total_secs,
		      adp->total_secs / (adp->heads * adp->sectors),
		      adp->heads, adp->sectors, atadev->max_iosize / DEV_BSIZE,
		      adp->num_tags + 1);
    }
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

static device_method_t ad_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,     ad_probe),
    DEVMETHOD(device_attach,    ad_attach),
    DEVMETHOD(device_detach,    ad_detach),
    DEVMETHOD(device_shutdown,  ad_shutdown),

    /* ATA methods */
    DEVMETHOD(ata_reinit,       ad_reinit),

    { 0, 0 }
};

static driver_t ad_driver = {
    "ad",
    ad_methods,
    0,
};

devclass_t ad_devclass;

DRIVER_MODULE(ad, ata, ad_driver, ad_devclass, NULL, NULL);
MODULE_VERSION(ad, 1);
MODULE_DEPEND(ad, ata, 1, 1, 1);
