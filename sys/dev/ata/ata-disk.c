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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/cons.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <geom/geom_disk.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>

/* prototypes */
static void ad_detach(struct ata_device *);
static void ad_start(struct ata_device *);
static void ad_done(struct ata_request *);
static disk_open_t adopen;
static disk_strategy_t adstrategy;
static dumper_t addump;
void ad_print(struct ad_softc *);
static int ad_version(u_int16_t);

/* internal vars */
static MALLOC_DEFINE(M_AD, "AD driver", "ATA disk driver");
static u_int32_t adp_lun_map = 0;

/* sysctl vars */
SYSCTL_DECL(_hw_ata);
SYSCTL_INT(_hw_ata, OID_AUTO, ata_dma, CTLFLAG_RD, &ata_dma, 0,
	   "ATA disk DMA mode control");
SYSCTL_INT(_hw_ata, OID_AUTO, wc, CTLFLAG_RD, &ata_wc, 0,
	   "ATA disk write caching");

void
ad_attach(struct ata_device *atadev)
{
    struct ad_softc *adp;
    u_int32_t lbasize;
    u_int64_t lbasize48;

    if (!(adp = malloc(sizeof(struct ad_softc), M_AD, M_NOWAIT | M_ZERO))) {
	ata_prtdev(atadev, "FAILURE - could not allocate driver storage\n");
	atadev->attach = NULL;
	return;
    }
    adp->device = atadev;
#ifdef ATA_STATIC_ID
    adp->lun = (device_get_unit(atadev->channel->dev)<<1)+ATA_DEV(atadev->unit);
#else
    adp->lun = ata_get_lun(&adp_lun_map);
#endif
    ata_set_name(atadev, "ad", adp->lun);
    adp->heads = atadev->param->heads;
    adp->sectors = atadev->param->sectors;
    adp->total_secs = atadev->param->cylinders * adp->heads * adp->sectors;	
    if (adp->device->channel->flags & ATA_USE_PC98GEOM &&
	adp->total_secs < 17 * 8 * 65536) {
	adp->sectors = 17;
	adp->heads = 8;
    }
    mtx_init(&adp->queue_mtx, "ATA disk bioqueue lock", MTX_DEF, 0);
    bioq_init(&adp->queue);

    lbasize = (u_int32_t)atadev->param->lba_size_1 |
	       ((u_int32_t)atadev->param->lba_size_2 << 16);

    /* does this device need oldstyle CHS addressing */
    if (!ad_version(atadev->param->version_major) || 
	!(atadev->param->atavalid & ATA_FLAG_54_58) || !lbasize)
	atadev->flags |= ATA_D_USE_CHS;

    /* use the 28bit LBA size if valid or bigger than the CHS mapping */
    if (atadev->param->cylinders == 16383 || adp->total_secs < lbasize)
	adp->total_secs = lbasize;

    lbasize48 = ((u_int64_t)atadev->param->lba_size48_1) |
		((u_int64_t)atadev->param->lba_size48_2 << 16) |
		((u_int64_t)atadev->param->lba_size48_3 << 32) |
		((u_int64_t)atadev->param->lba_size48_4 << 48);

    /* use the 48bit LBA size if valid */
    if ((atadev->param->support.command2 & ATA_SUPPORT_ADDRESS48) &&
	lbasize48 > 268435455)
	adp->total_secs = lbasize48;

    /* enable read caching */
    ata_controlcmd(atadev, ATA_SETFEATURES, ATA_SF_ENAB_RCACHE, 0, 0);

    /* enable write caching if enabled */
    if (ata_wc)
	ata_controlcmd(atadev, ATA_SETFEATURES, ATA_SF_ENAB_WCACHE, 0, 0);
    else
	ata_controlcmd(atadev, ATA_SETFEATURES, ATA_SF_DIS_WCACHE, 0, 0);

    /* use multiple sectors/interrupt if device supports it */
    adp->max_iosize = DEV_BSIZE;
    if (ad_version(atadev->param->version_major)) {
	int secsperint = max(1, min(atadev->param->sectors_intr, 16));

	if (!ata_controlcmd(atadev, ATA_SET_MULTI, 0, 0, secsperint))
	    adp->max_iosize = secsperint * DEV_BSIZE;
    }

    /* use DMA if allowed and if drive/controller supports it */
    if (ata_dma && atadev->channel->dma) 
	atadev->setmode(atadev, ATA_DMA_MAX);
    else 
	atadev->setmode(atadev, ATA_PIO_MAX);

    /* setup the function ptrs */
    atadev->detach = ad_detach;
    atadev->start = ad_start;
    atadev->softc = adp;

    /* lets create the disk device */
    adp->disk.d_open = adopen;
    adp->disk.d_strategy = adstrategy;
    adp->disk.d_dump = addump;
    adp->disk.d_name = "ad";
    adp->disk.d_drv1 = adp;
    if (atadev->channel->dma)
	adp->disk.d_maxsize = atadev->channel->dma->max_iosize;
    else
	adp->disk.d_maxsize = DFLTPHYS;
    adp->disk.d_sectorsize = DEV_BSIZE;
    adp->disk.d_mediasize = DEV_BSIZE * (off_t)adp->total_secs;
    adp->disk.d_fwsectors = adp->sectors;
    adp->disk.d_fwheads = adp->heads;
    disk_create(adp->lun, &adp->disk, DISKFLAG_NOGIANT, NULL, NULL);

    /* announce we are here */
    ad_print(adp);

#ifdef DEV_ATARAID
    ata_raiddisk_attach(adp);
#endif
}

static void
ad_detach(struct ata_device *atadev)
{
    struct ad_softc *adp = atadev->softc;

    atadev->flags |= ATA_D_DETACHING;
#ifdef DEV_ATARAID
    if (adp->flags & AD_F_RAID_SUBDISK)
	ata_raiddisk_detach(adp);
#endif
    mtx_lock(&adp->queue_mtx);
    bioq_flush(&adp->queue, NULL, ENXIO);
    mtx_unlock(&adp->queue_mtx);
    disk_destroy(&adp->disk);
    ata_prtdev(atadev, "WARNING - removed from configuration\n");
    ata_free_name(atadev);
    ata_free_lun(&adp_lun_map, adp->lun);
    atadev->attach = NULL;
    atadev->detach = NULL;
    atadev->start = NULL;
    atadev->softc = NULL;
    atadev->flags = 0;
    free(adp, M_AD);
}

static int
adopen(struct disk *dp)
{
    struct ad_softc *adp = dp->d_drv1;

    if (adp->device->flags & ATA_D_DETACHING)
	return ENXIO;
    return 0;
}

static void 
adstrategy(struct bio *bp)
{
    struct ad_softc *adp = bp->bio_disk->d_drv1;

    if (adp->device->flags & ATA_D_DETACHING) {
	biofinish(bp, NULL, ENXIO);
	return;
    }
    mtx_lock(&adp->queue_mtx);
    bioq_disksort(&adp->queue, bp);
    mtx_unlock(&adp->queue_mtx);
    ata_start(adp->device->channel);
}

static void
ad_start(struct ata_device *atadev)
{
    struct ad_softc *adp = atadev->softc;
    struct bio *bp;
    struct ata_request *request;

    /* remove request from drive queue */
    mtx_lock(&adp->queue_mtx);
    bp = bioq_first(&adp->queue);
    if (!bp) {
	mtx_unlock(&adp->queue_mtx);
	return;
    }
    bioq_remove(&adp->queue, bp); 
    mtx_unlock(&adp->queue_mtx);

    if (!(request = ata_alloc_request())) {
	ata_prtdev(atadev, "FAILURE - out of memory in start\n");
	biofinish(bp, NULL, ENOMEM);
	return;
    }

    /* setup request */
    request->device = atadev;
    request->driver = bp;
    request->callback = ad_done;
    request->timeout = 10;
    request->retries = 2;
    request->data = bp->bio_data;
    request->bytecount = bp->bio_bcount;

    /* convert LBA contents if this is an old non-LBA device */
    if (atadev->flags & ATA_D_USE_CHS) {
	struct ata_params *param = atadev->param;
	int sector = (bp->bio_pblkno % param->sectors) + 1;
	int cylinder = bp->bio_pblkno / (param->sectors * param->heads);
	int head = (bp->bio_pblkno %
		    (param->sectors * param->heads)) / param->sectors;

	request->u.ata.lba =
	    (sector & 0xff) | (cylinder & 0xffff) << 8 | (head & 0xf) << 24;
    }
    else
	request->u.ata.lba = bp->bio_pblkno;

    request->u.ata.count = request->bytecount / DEV_BSIZE;
    request->transfersize = min(bp->bio_bcount, adp->max_iosize);

    switch (bp->bio_cmd) {
    case BIO_READ:
	request->flags |= ATA_R_READ;
	if (atadev->mode >= ATA_DMA) {
	    request->u.ata.command = ATA_READ_DMA;
	    request->flags |= ATA_R_DMA;
	}
	else if (adp->max_iosize > DEV_BSIZE)
	    request->u.ata.command = ATA_READ_MUL;
	else
	    request->u.ata.command = ATA_READ;

	break;
    case BIO_WRITE:
	request->flags |= ATA_R_WRITE;
	if (atadev->mode >= ATA_DMA) {
	    request->u.ata.command = ATA_WRITE_DMA;
	    request->flags |= ATA_R_DMA;
	}
	else if (adp->max_iosize > DEV_BSIZE)
	    request->u.ata.command = ATA_WRITE_MUL;
	else
	    request->u.ata.command = ATA_WRITE;
	break;
    default:
	ata_prtdev(atadev, "FAILURE - unknown BIO operation\n");
	ata_free_request(request);
	biofinish(bp, NULL, EIO);
	return;
    }
    request->flags |= ATA_R_SKIPSTART;
    ata_queue_request(request);
}

static void
ad_done(struct ata_request *request)
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
addump(void *arg, void *virtual, vm_offset_t physical,
       off_t offset, size_t length)
{
    struct ata_request request;
    struct disk *dp = arg;
    struct ad_softc *adp = dp->d_drv1;

    if (!adp)
	return ENXIO;

    bzero(&request, sizeof(struct ata_request));
    request.device = adp->device;
    request.data = virtual;
    request.bytecount = length;
    request.transfersize = min(length, adp->max_iosize);

    request.flags = ATA_R_WRITE;
    if (adp->max_iosize > DEV_BSIZE)
	request.u.ata.command = ATA_WRITE_MUL;
    else
	request.u.ata.command = ATA_WRITE;
    request.u.ata.lba = offset / DEV_BSIZE;
    request.u.ata.count = request.bytecount / DEV_BSIZE;

    if (adp->device->channel->hw.transaction(&request) == ATA_OP_FINISHED)
	return EIO;
    while (request.bytecount > request.donecount) {
	DELAY(20);
	adp->device->channel->running = &request;
	adp->device->channel->hw.interrupt(adp->device->channel);
	adp->device->channel->running = NULL;
	if (request.status & ATA_S_ERROR)
	    return EIO;
    }
    return 0;
}

void
ad_print(struct ad_softc *adp) 
{
    if (bootverbose) {
	ata_prtdev(adp->device, "<%.40s/%.8s> ATA-%d disk at ata%d-%s\n", 
		   adp->device->param->model, adp->device->param->revision,
		   ad_version(adp->device->param->version_major), 
		   device_get_unit(adp->device->channel->dev),
		   (adp->device->unit == ATA_MASTER) ? "master" : "slave");

	ata_prtdev(adp->device,
		   "%lluMB (%llu sectors), %llu C, %u H, %u S, %u B\n",
		   (unsigned long long)(adp->total_secs /
					((1024L*1024L)/DEV_BSIZE)),
		   (unsigned long long)adp->total_secs,
		   (unsigned long long)(adp->total_secs /
					(adp->heads * adp->sectors)),
		   adp->heads, adp->sectors, DEV_BSIZE);

	ata_prtdev(adp->device, "%d secs/int, %d depth queue, %s%s\n", 
		   adp->max_iosize / DEV_BSIZE, adp->num_tags + 1,
		   (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
		   ata_mode2str(adp->device->mode));
    }
    else
	ata_prtdev(adp->device,"%lluMB <%.40s> [%lld/%d/%d] at ata%d-%s %s%s\n",
		   (unsigned long long)(adp->total_secs /
					((1024L * 1024L) / DEV_BSIZE)),
		   adp->device->param->model,
		   (unsigned long long)(adp->total_secs /
					(adp->heads * adp->sectors)),
		   adp->heads, adp->sectors,
		   device_get_unit(adp->device->channel->dev),
		   (adp->device->unit == ATA_MASTER) ? "master" : "slave",
		   (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "",
		   ata_mode2str(adp->device->mode));
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
