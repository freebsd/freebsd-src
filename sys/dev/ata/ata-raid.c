/*-
 * Copyright (c) 2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
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

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/ata.h> 
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/cons.h>
#include <sys/unistd.h>
#include <sys/kthread.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>

/* device structures */
static d_open_t		aropen;
static d_strategy_t	arstrategy;
static struct cdevsw ar_cdevsw = {
	/* open */	aropen,
	/* close */	nullclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	noioctl, 
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	arstrategy,
	/* name */	"ar",
	/* maj */	157,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
};  
static struct cdevsw ardisk_cdevsw;

/* prototypes */
static void ar_attach_raid(struct ar_softc *, int);
static void ar_done(struct bio *);
static void ar_config_changed(struct ar_softc *, int);
static void ar_rebuild(void *);
static int ar_highpoint_read_conf(struct ad_softc *, struct ar_softc **);
static int ar_highpoint_write_conf(struct ar_softc *);
static int ar_promise_read_conf(struct ad_softc *, struct ar_softc **, int);
static int ar_promise_write_conf(struct ar_softc *);
static int ar_rw(struct ad_softc *, u_int32_t, int, caddr_t, int);
static struct ata_device *ar_locate_disk(int);

/* internal vars */
static struct ar_softc **ar_table = NULL;
static MALLOC_DEFINE(M_AR, "AR driver", "ATA RAID driver");

int
ata_raiddisk_attach(struct ad_softc *adp)
{
    struct ar_softc *rdp;
    int array, disk;

    if (ar_table) {
	for (array = 0; array < MAX_ARRAYS; array++) {
	    if (!(rdp = ar_table[array]) || !rdp->flags)
		continue;
   
	    for (disk = 0; disk < rdp->total_disks; disk++) {
		if ((rdp->disks[disk].flags & AR_DF_ASSIGNED) &&
		    rdp->disks[disk].device == adp->device) {
		    ata_prtdev(rdp->disks[disk].device,
			       "inserted into ar%d disk%d as spare\n",
			       array, disk);
		    rdp->disks[disk].flags |= (AR_DF_PRESENT | AR_DF_SPARE);
		    AD_SOFTC(rdp->disks[disk])->flags = AD_F_RAID_SUBDISK;
		    ar_config_changed(rdp, 1);
		    return 1;
		}
	    }
	}
    }

    if (!ar_table)
	ar_table = malloc(sizeof(struct ar_soft *) * MAX_ARRAYS,
			  M_AR, M_NOWAIT | M_ZERO);
    if (!ar_table) {
	ata_prtdev(adp->device, "no memory for ATA raid array\n");
	return 0;
    }

    switch(adp->device->channel->chiptype) {
    case 0x4d33105a: case 0x4d38105a: case 0x4d30105a:
    case 0x0d30105a: case 0x4d68105a: case 0x6268105a:
    case 0x4d69105a: case 0x5275105a: case 0x6269105a:
    case 0x7275105a:

	/* test RAID bit in PCI reg XXX */
	return (ar_promise_read_conf(adp, ar_table, 0));

    case 0x00041103: case 0x00051103: case 0x00081103:
	return (ar_highpoint_read_conf(adp, ar_table));

    default:
	return (ar_promise_read_conf(adp, ar_table, 1));
    }
    return 0;
}

int
ata_raiddisk_detach(struct ad_softc *adp)
{
    struct ar_softc *rdp;
    int array, disk;

    if (ar_table) {
	for (array = 0; array < MAX_ARRAYS; array++) {
	    if (!(rdp = ar_table[array]) || !rdp->flags)
		continue; 
	    for (disk = 0; disk < rdp->total_disks; disk++) {
		if (rdp->disks[disk].device == adp->device) {
		    ata_prtdev(rdp->disks[disk].device,
			       "deleted from ar%d disk%d\n", array, disk);
		    rdp->disks[disk].flags &= ~(AR_DF_PRESENT | AR_DF_ONLINE);
		    AD_SOFTC(rdp->disks[disk])->flags &= ~AD_F_RAID_SUBDISK;
		    ar_config_changed(rdp, 1);
		    return 1;
		}
	    }
	}
    }
    return 0;
}

void
ata_raid_attach()
{
    struct ar_softc *rdp;
    int array;

    if (!ar_table)
	return;

    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!(rdp = ar_table[array]) || !rdp->flags)
	    continue;
	ar_attach_raid(rdp, 0);
    }
}

static void
ar_attach_raid(struct ar_softc *rdp, int update)
{
    dev_t dev;
    int disk;

    ar_config_changed(rdp, update);
    dev = disk_create(rdp->lun, &rdp->disk, 0, &ar_cdevsw, &ardisk_cdevsw);
    dev->si_drv1 = rdp;
    dev->si_iosize_max = 256 * DEV_BSIZE;
    rdp->dev = dev;

    printf("ar%d: %lluMB <ATA ", rdp->lun, (unsigned long long)
	   (rdp->total_sectors / ((1024L * 1024L) / DEV_BSIZE)));
    switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
    case AR_F_RAID0:
	printf("RAID0 "); break;
    case AR_F_RAID1:
	printf("RAID1 "); break;
    case AR_F_SPAN:
	printf("SPAN "); break;
    case (AR_F_RAID0 | AR_F_RAID1):
	printf("RAID0+1 "); break;
    default:
	printf("unknown 0x%x> ", rdp->flags);
	return;
    }
    printf("array> [%d/%d/%d] status: ",
	   rdp->cylinders, rdp->heads, rdp->sectors);
    switch (rdp->flags & (AR_F_DEGRADED | AR_F_READY)) {
    case AR_F_READY:
	printf("READY");
	break;
    case (AR_F_DEGRADED | AR_F_READY):
	printf("DEGRADED");
	break;
    default:
	printf("BROKEN");
	break;
    }
    printf(" subdisks:\n");
    for (disk = 0; disk < rdp->total_disks; disk++) {
	if (rdp->disks[disk].flags & AR_DF_PRESENT) {
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		printf(" %d READY ", disk);
	    else if (rdp->disks[disk].flags & AR_DF_SPARE)
		printf(" %d SPARE ", disk);
	    else
		printf(" %d FREE  ", disk);
	    ad_print(AD_SOFTC(rdp->disks[disk]));
	    printf("         ");
	    ata_enclosure_print(AD_SOFTC(rdp->disks[disk])->device);
	}
	else if (rdp->disks[disk].flags & AR_DF_ASSIGNED)
	    printf(" %d DOWN\n", disk);
	else
	    printf(" %d INVALID no RAID config info on this disk\n", disk);
    }
}

int
ata_raid_create(struct raid_setup *setup)
{
    struct ata_device *atadev;
    struct ar_softc *rdp;
    int array, disk;
    int ctlr = 0, disk_size = 0, total_disks = 0;

    if (!ar_table)
	ar_table = malloc(sizeof(struct ar_soft *) * MAX_ARRAYS,
			  M_AR, M_NOWAIT | M_ZERO);
    if (!ar_table) {
	printf("ar: no memory for ATA raid array\n");
	return 0;
    }
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!ar_table[array])
	    break;
    }
    if (array >= MAX_ARRAYS)
	return ENOSPC;

    if (!(rdp = (struct ar_softc*)malloc(sizeof(struct ar_softc), M_AR,
					 M_NOWAIT | M_ZERO))) {
	printf("ar%d: failed to allocate raid config storage\n", array);
	return ENOMEM;
    }

    for (disk = 0; disk < setup->total_disks; disk++) {
	if ((atadev = ar_locate_disk(setup->disks[disk]))) {
	    rdp->disks[disk].device = atadev;
	    if (AD_SOFTC(rdp->disks[disk])->flags & AD_F_RAID_SUBDISK) {
		setup->disks[disk] = -1;
		free(rdp, M_AR);
		return EBUSY;
	    }

	    switch (rdp->disks[disk].device->channel->chiptype & 0xffff) {
	    case 0x1103:
		ctlr |= AR_F_HIGHPOINT_RAID;
		rdp->disks[disk].disk_sectors =
		    AD_SOFTC(rdp->disks[disk])->total_secs;
		break;

	    default:
		ctlr |= AR_F_FREEBSD_RAID;
		/* FALLTHROUGH */

	    case 0x105a:	
		ctlr |= AR_F_PROMISE_RAID;
		rdp->disks[disk].disk_sectors =
		    PR_LBA(AD_SOFTC(rdp->disks[disk]));
		break;
	    }
	    if (rdp->flags & (AR_F_PROMISE_RAID|AR_F_HIGHPOINT_RAID) &&
		(rdp->flags & (AR_F_PROMISE_RAID|AR_F_HIGHPOINT_RAID)) !=
		 (ctlr & (AR_F_PROMISE_RAID|AR_F_HIGHPOINT_RAID))) {
		free(rdp, M_AR);
		return EXDEV;
	    }
	    else
		rdp->flags |= ctlr;
	    
	    if (disk_size)
	    	disk_size = min(rdp->disks[disk].disk_sectors, disk_size);
	    else
		disk_size = rdp->disks[disk].disk_sectors;
	    rdp->disks[disk].flags = 
		(AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE);

	    total_disks++;
	}
	else {
	    setup->disks[disk] = -1;
	    free(rdp, M_AR);
	    return ENXIO;
	}
    }
    if (!total_disks) {
	free(rdp, M_AR);
	return ENODEV;
    }

    switch (setup->type) {
    case 1:
	rdp->flags |= AR_F_RAID0;
	break;
    case 2:
	rdp->flags |= AR_F_RAID1;
	if (total_disks != 2) {
	    free(rdp, M_AR);
	    return EPERM;
	}
	break;
    case 3:
	rdp->flags |= (AR_F_RAID0 | AR_F_RAID1);
	if (total_disks % 2 != 0) {
	    free(rdp, M_AR);
	    return EPERM;
	}
	break;
    case 4:
	rdp->flags |= AR_F_SPAN;
	break;
    }

    for (disk = 0; disk < total_disks; disk++)
	AD_SOFTC(rdp->disks[disk])->flags = AD_F_RAID_SUBDISK;

    rdp->lun = array;
    if (rdp->flags & AR_F_RAID0) {
	int bit = 0;

	while (setup->interleave >>= 1)
	    bit++;
	if (rdp->flags & AR_F_PROMISE_RAID)
	    rdp->interleave = min(max(2, 1 << bit), 2048);
	if (rdp->flags & AR_F_HIGHPOINT_RAID)
	    rdp->interleave = min(max(32, 1 << bit), 128);
    }
    rdp->total_disks = total_disks;
    rdp->width = total_disks / ((rdp->flags & AR_F_RAID1) ? 2 : 1);	
    rdp->total_sectors = disk_size * rdp->width;
    rdp->heads = 255;
    rdp->sectors = 63;
    rdp->cylinders = rdp->total_sectors / (255 * 63);
    if (rdp->flags & AR_F_PROMISE_RAID) {
	rdp->offset = 0;
	rdp->reserved = 63;
    }
    if (rdp->flags & AR_F_HIGHPOINT_RAID) {
	rdp->offset = HPT_LBA + 1;
	rdp->reserved = HPT_LBA + 1;
    }
    rdp->lock_start = rdp->lock_end = 0xffffffff;
    rdp->flags |= AR_F_READY;

    ar_table[array] = rdp;
    ar_attach_raid(rdp, 1);
    setup->unit = array;
    return 0;
}

int
ata_raid_delete(int array)
{
    struct ar_softc *rdp;
    int disk;

    if (!ar_table) {
	printf("ar: no memory for ATA raid array\n");
	return 0;
    }
    if (!(rdp = ar_table[array]))
	return ENXIO;
    
    rdp->flags &= ~AR_F_READY;
    for (disk = 0; disk < rdp->total_disks; disk++) {
	if ((rdp->disks[disk].flags&AR_DF_PRESENT) && rdp->disks[disk].device) {
	    AD_SOFTC(rdp->disks[disk])->flags &= ~AD_F_RAID_SUBDISK;
	    ata_enclosure_leds(rdp->disks[disk].device, ATA_LED_GREEN);
	    rdp->disks[disk].flags = 0;
	}
    }
    if (rdp->flags & AR_F_PROMISE_RAID)
	ar_promise_write_conf(rdp);
    else
	ar_highpoint_write_conf(rdp);
    disk_destroy(rdp->dev);
    free(rdp, M_AR);
    ar_table[array] = NULL;
    return 0;
}

int
ata_raid_status(int array, struct raid_status *status)
{
    struct ar_softc *rdp;
    int i;

    if (!ar_table || !(rdp = ar_table[array]))
	return ENXIO;

    switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
    case AR_F_RAID0:
	status->type = AR_RAID0;
	break;
    case AR_F_RAID1:
	status->type = AR_RAID1;
	break;
    case AR_F_RAID0 | AR_F_RAID1:
	status->type = AR_RAID0 | AR_RAID1;
	break;
    case AR_F_SPAN:
	status->type = AR_SPAN;
	break;
    }
    status->total_disks = rdp->total_disks;
    for (i = 0; i < rdp->total_disks; i++ ) {
	if ((rdp->disks[i].flags & AR_DF_PRESENT) && rdp->disks[i].device)
	    status->disks[i] = AD_SOFTC(rdp->disks[i])->lun;
	else
	    status->disks[i] = -1;
    }
    status->interleave = rdp->interleave;
    status->status = 0;
    if (rdp->flags & AR_F_READY)
	status->status |= AR_READY;
    if (rdp->flags & AR_F_DEGRADED)
	status->status |= AR_DEGRADED;
    if (rdp->flags & AR_F_REBUILDING) {
	status->status |= AR_REBUILDING;
	status->progress = 100*rdp->lock_start/(rdp->total_sectors/rdp->width);
    }
    return 0;
}

int
ata_raid_rebuild(int array)
{
    struct ar_softc *rdp;

    if (!ar_table || !(rdp = ar_table[array]))
	return ENXIO;
    if (rdp->flags & AR_F_REBUILDING)
	return EBUSY;
    return kthread_create(ar_rebuild, rdp, &rdp->pid, RFNOWAIT, 0,
			  "rebuilding ar%d", array);
}

static int
aropen(dev_t dev, int flags, int fmt, struct thread *td)
{
    struct ar_softc *rdp = dev->si_drv1;
	
    rdp->disk.d_sectorsize = DEV_BSIZE;
    rdp->disk.d_mediasize = (off_t)rdp->total_sectors * DEV_BSIZE;
    rdp->disk.d_fwsectors = rdp->sectors;
    rdp->disk.d_fwheads = rdp->heads;
    return 0;
}

static void
arstrategy(struct bio *bp)
{
    struct ar_softc *rdp = bp->bio_dev->si_drv1;
    int blkno, count, chunk, lba, lbs, tmplba;
    int drv = 0, change = 0;
    caddr_t data;

    if (!(rdp->flags & AR_F_READY)) {
	bp->bio_flags |= BIO_ERROR;
	bp->bio_error = EIO;
	biodone(bp);
	return;
    }

    bp->bio_resid = bp->bio_bcount;
    blkno = bp->bio_pblkno;
    data = bp->bio_data;
    for (count = howmany(bp->bio_bcount, DEV_BSIZE); count > 0; 
	 count -= chunk, blkno += chunk, data += (chunk * DEV_BSIZE)) {
	struct ar_buf *buf1, *buf2;

	switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_SPAN:
	    lba = blkno;
	    while (lba >= AD_SOFTC(rdp->disks[drv])->total_secs-rdp->reserved)
		lba -= AD_SOFTC(rdp->disks[drv++])->total_secs-rdp->reserved;
	    chunk = min(AD_SOFTC(rdp->disks[drv])->total_secs-rdp->reserved-lba,
			count);
	    break;
	
	case AR_F_RAID0:
	case AR_F_RAID0 | AR_F_RAID1:
	    tmplba = blkno / rdp->interleave;
	    chunk = blkno % rdp->interleave;
	    if (tmplba == rdp->total_sectors / rdp->interleave) {
		lbs = (rdp->total_sectors-(tmplba*rdp->interleave))/rdp->width;
		drv = chunk / lbs;
		lba = ((tmplba/rdp->width)*rdp->interleave) + chunk%lbs;
		chunk = min(count, lbs);
	    }
	    else {
		drv = tmplba % rdp->width;
		lba = ((tmplba / rdp->width) * rdp->interleave) + chunk;
		chunk = min(count, rdp->interleave - chunk);
	    }
	    break;

	case AR_F_RAID1:
	    drv = 0;
	    lba = blkno;
	    chunk = count;
	    break;

	default:
	    printf("ar%d: unknown array type in arstrategy\n", rdp->lun);
	    bp->bio_flags |= BIO_ERROR;
	    bp->bio_error = EIO;
	    biodone(bp);
	    return;
	}

	buf1 = malloc(sizeof(struct ar_buf), M_AR, M_NOWAIT | M_ZERO);
	buf1->bp.bio_pblkno = lba;
	if ((buf1->drive = drv) > 0)
	    buf1->bp.bio_pblkno += rdp->offset;
	buf1->bp.bio_caller1 = (void *)rdp;
	buf1->bp.bio_bcount = chunk * DEV_BSIZE;
	buf1->bp.bio_data = data;
	buf1->bp.bio_cmd = bp->bio_cmd;
	buf1->bp.bio_flags = bp->bio_flags;
	buf1->bp.bio_done = ar_done;
	buf1->org = bp;

	switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_SPAN:
	case AR_F_RAID0:
	    if ((rdp->disks[buf1->drive].flags &
		 (AR_DF_PRESENT|AR_DF_ONLINE))==(AR_DF_PRESENT|AR_DF_ONLINE) &&
		!AD_SOFTC(rdp->disks[buf1->drive])->dev->si_disk) {
		rdp->disks[buf1->drive].flags &= ~AR_DF_ONLINE;
		ar_config_changed(rdp, 1);
		free(buf1, M_AR);
		bp->bio_flags |= BIO_ERROR;
		bp->bio_error = EIO;
		biodone(bp);
		return;
	    }
	    buf1->bp.bio_dev = AD_SOFTC(rdp->disks[buf1->drive])->dev;
	    AR_STRATEGY((struct bio *)buf1);
	    break;

	case AR_F_RAID1:
	case AR_F_RAID0 | AR_F_RAID1:
	    if (rdp->flags & AR_F_REBUILDING && bp->bio_cmd == BIO_WRITE) {
		if ((bp->bio_pblkno >= rdp->lock_start &&
		     bp->bio_pblkno < rdp->lock_end) ||
		    ((bp->bio_pblkno + chunk) > rdp->lock_start &&
		     (bp->bio_pblkno + chunk) <= rdp->lock_end)) {
		    tsleep(rdp, PRIBIO, "arwait", 0);
		}
	    }
	    if ((rdp->disks[buf1->drive].flags &
		 (AR_DF_PRESENT|AR_DF_ONLINE))==(AR_DF_PRESENT|AR_DF_ONLINE) &&
		!AD_SOFTC(rdp->disks[buf1->drive])->dev->si_disk) {
		rdp->disks[buf1->drive].flags &= ~AR_DF_ONLINE;
		change = 1;
	    }
	    if ((rdp->disks[buf1->drive + rdp->width].flags &
		 (AR_DF_PRESENT|AR_DF_ONLINE))==(AR_DF_PRESENT|AR_DF_ONLINE) &&
		!AD_SOFTC(rdp->disks[buf1->drive + rdp->width])->dev->si_disk) {
		rdp->disks[buf1->drive + rdp->width].flags &= ~AR_DF_ONLINE;
		change = 1;
	    }
	    if (change)
		ar_config_changed(rdp, 1);
		
	    if (!(rdp->flags & AR_F_READY)) {
		free(buf1, M_AR);
		bp->bio_flags |= BIO_ERROR;
		bp->bio_error = EIO;
		biodone(bp);
		return;
	    }
	    if (bp->bio_cmd == BIO_READ) {
		if ((buf1->bp.bio_pblkno <
		     (rdp->disks[buf1->drive].last_lba - AR_PROXIMITY) ||
		     buf1->bp.bio_pblkno >
		     (rdp->disks[buf1->drive].last_lba + AR_PROXIMITY) ||
		     !(rdp->disks[buf1->drive].flags & AR_DF_ONLINE)) &&
		     (rdp->disks[buf1->drive+rdp->width].flags & AR_DF_ONLINE))
			buf1->drive = buf1->drive + rdp->width;
	    }
	    if (bp->bio_cmd == BIO_WRITE) {
		if ((rdp->disks[buf1->drive+rdp->width].flags & AR_DF_ONLINE) ||
		    ((rdp->flags & AR_F_REBUILDING) &&
		     (rdp->disks[buf1->drive+rdp->width].flags & AR_DF_SPARE) &&
		     buf1->bp.bio_pblkno < rdp->lock_start)) {
		    if ((rdp->disks[buf1->drive].flags & AR_DF_ONLINE) ||
			((rdp->flags & AR_F_REBUILDING) &&
			 (rdp->disks[buf1->drive].flags & AR_DF_SPARE) &&
			 buf1->bp.bio_pblkno < rdp->lock_start)) {
			buf2 = malloc(sizeof(struct ar_buf), M_AR, M_NOWAIT);
			bcopy(buf1, buf2, sizeof(struct ar_buf));
			buf1->mirror = buf2;
			buf2->mirror = buf1;
			buf2->drive = buf1->drive + rdp->width;
			buf2->bp.bio_dev =
			    AD_SOFTC(rdp->disks[buf2->drive])->dev;
			AR_STRATEGY((struct bio *)buf2);
			rdp->disks[buf2->drive].last_lba =
			    buf2->bp.bio_pblkno + chunk;
		    }
		    else
			buf1->drive = buf1->drive + rdp->width;
		}
	    }
	    buf1->bp.bio_dev = AD_SOFTC(rdp->disks[buf1->drive])->dev;
	    AR_STRATEGY((struct bio *)buf1);
	    rdp->disks[buf1->drive].last_lba = buf1->bp.bio_pblkno + chunk;
	    break;

	default:
	    printf("ar%d: unknown array type in arstrategy\n", rdp->lun);
	}
    }
}

static void
ar_done(struct bio *bp)
{
    struct ar_softc *rdp = (struct ar_softc *)bp->bio_caller1;
    struct ar_buf *buf = (struct ar_buf *)bp;

    switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
    case AR_F_SPAN:
    case AR_F_RAID0:
	if (buf->bp.bio_flags & BIO_ERROR) {
	    rdp->disks[buf->drive].flags &= ~AR_DF_ONLINE;
	    ar_config_changed(rdp, 1);
	    buf->org->bio_flags |= BIO_ERROR;
	    buf->org->bio_error = EIO;
	    biodone(buf->org);
	}
	else {
	    buf->org->bio_resid -= buf->bp.bio_bcount;
	    if (buf->org->bio_resid == 0)
		biodone(buf->org);
	}
	break;

    case AR_F_RAID1:
    case AR_F_RAID0 | AR_F_RAID1:
	if (buf->bp.bio_flags & BIO_ERROR) {
	    rdp->disks[buf->drive].flags &= ~AR_DF_ONLINE;
	    ar_config_changed(rdp, 1);
	    if (rdp->flags & AR_F_READY) {
		if (buf->bp.bio_cmd == BIO_READ) {
		    if (buf->drive < rdp->width)
			buf->drive = buf->drive + rdp->width;
		    else
			buf->drive = buf->drive - rdp->width;
		    buf->bp.bio_dev = AD_SOFTC(rdp->disks[buf->drive])->dev;
		    buf->bp.bio_flags = buf->org->bio_flags;
		    buf->bp.bio_error = 0;
		    AR_STRATEGY((struct bio *)buf);
		    return;
		}
		if (buf->bp.bio_cmd == BIO_WRITE) {
		    if (buf->flags & AB_F_DONE) {
			buf->org->bio_resid -= buf->bp.bio_bcount;
			if (buf->org->bio_resid == 0)
			    biodone(buf->org);
		    }
		    else
			buf->mirror->flags |= AB_F_DONE;
		}
	    }
	    else {
		buf->org->bio_flags |= BIO_ERROR;
		buf->org->bio_error = EIO;
		biodone(buf->org);
	    }
	} 
	else {
	    if (buf->bp.bio_cmd == BIO_WRITE) {
		if (buf->mirror && !(buf->flags & AB_F_DONE)){
		    buf->mirror->flags |= AB_F_DONE;
		    break;
		}
	    }
	    buf->org->bio_resid -= buf->bp.bio_bcount;
	    if (buf->org->bio_resid == 0)
		biodone(buf->org);
	}
	break;
	
    default:
	printf("ar%d: unknown array type in ar_done\n", rdp->lun);
    }
    free(buf, M_AR);
}

static void
ar_config_changed(struct ar_softc *rdp, int writeback)
{
    int disk, flags;

    flags = rdp->flags;
    rdp->flags |= AR_F_READY;
    rdp->flags &= ~AR_F_DEGRADED;

    for (disk = 0; disk < rdp->total_disks; disk++)
	if (!(rdp->disks[disk].flags & AR_DF_PRESENT))
	    rdp->disks[disk].flags &= ~AR_DF_ONLINE;

    for (disk = 0; disk < rdp->total_disks; disk++) {
	switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_SPAN:
	case AR_F_RAID0:
	    if (!(rdp->disks[disk].flags & AR_DF_ONLINE)) {
		rdp->flags &= ~AR_F_READY;
		printf("ar%d: ERROR - array broken\n", rdp->lun);
	    }
	    break;

	case AR_F_RAID1:
	case AR_F_RAID0 | AR_F_RAID1:
	    if (disk < rdp->width) {
		if (!(rdp->disks[disk].flags & AR_DF_ONLINE) &&
		    !(rdp->disks[disk + rdp->width].flags & AR_DF_ONLINE)) {
		    rdp->flags &= ~AR_F_READY;
		    printf("ar%d: ERROR - array broken\n", rdp->lun);
		}
		else if (((rdp->disks[disk].flags & AR_DF_ONLINE) &&
			  !(rdp->disks
			    [disk + rdp->width].flags & AR_DF_ONLINE))||
			 (!(rdp->disks[disk].flags & AR_DF_ONLINE) &&
			  (rdp->disks
			   [disk + rdp->width].flags & AR_DF_ONLINE))) {
		    rdp->flags |= AR_F_DEGRADED;
		    if (!(flags & AR_F_DEGRADED))
			printf("ar%d: WARNING - mirror lost\n", rdp->lun);
		}
	    }
	    break;
	}
	if ((rdp->disks[disk].flags&AR_DF_PRESENT) && rdp->disks[disk].device) {
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		ata_enclosure_leds(rdp->disks[disk].device, ATA_LED_GREEN);
	    else
		ata_enclosure_leds(rdp->disks[disk].device, ATA_LED_RED);
	}
    }
    if (writeback) {
	if (rdp->flags & AR_F_PROMISE_RAID)
	    ar_promise_write_conf(rdp);
	if (rdp->flags & AR_F_HIGHPOINT_RAID)
	    ar_highpoint_write_conf(rdp);
    }
}

static void
ar_rebuild(void *arg)
{
    struct ar_softc *rdp = arg;
    int disk, s, count = 0, error = 0;
    caddr_t buffer;

    if ((rdp->flags & (AR_F_READY|AR_F_DEGRADED)) != (AR_F_READY|AR_F_DEGRADED))
	kthread_exit(EEXIST);

    for (disk = 0; disk < rdp->total_disks; disk++) {
	if (((rdp->disks[disk].flags&(AR_DF_PRESENT|AR_DF_ONLINE|AR_DF_SPARE))==
	     (AR_DF_PRESENT | AR_DF_SPARE)) && rdp->disks[disk].device) {
	    if (AD_SOFTC(rdp->disks[disk])->total_secs <
		rdp->disks[disk].disk_sectors) {
		ata_prtdev(rdp->disks[disk].device,
			   "disk capacity too small for this RAID config\n");
#if 0
		rdp->disks[disk].flags &= ~AR_DF_SPARE;
		AD_SOFTC(rdp->disks[disk])->flags &= ~AD_F_RAID_SUBDISK;
#endif
		continue;
	    }
	    ata_enclosure_leds(rdp->disks[disk].device, ATA_LED_ORANGE);
	    count++;
	}
    }
    if (!count)
	kthread_exit(ENODEV);

    /* setup start conditions */
    s = splbio();
    rdp->lock_start = 0;
    rdp->lock_end = rdp->lock_start + 256;
    rdp->flags |= AR_F_REBUILDING;
    splx(s);
    buffer = malloc(256 * DEV_BSIZE, M_AR, M_NOWAIT | M_ZERO);

    /* now go copy entire disk(s) */
    while (rdp->lock_end < (rdp->total_sectors / rdp->width)) {
	int size = min(256, (rdp->total_sectors / rdp->width) - rdp->lock_end);

	for (disk = 0; disk < rdp->width; disk++) {
	    struct ad_softc *adp;

	    if (((rdp->disks[disk].flags & AR_DF_ONLINE) &&
		 (rdp->disks[disk + rdp->width].flags & AR_DF_ONLINE)) ||
		((rdp->disks[disk].flags & AR_DF_ONLINE) && 
		 !(rdp->disks[disk + rdp->width].flags & AR_DF_SPARE)) ||
		((rdp->disks[disk + rdp->width].flags & AR_DF_ONLINE) &&
		 !(rdp->disks[disk].flags & AR_DF_SPARE)))
		continue;

	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		adp = AD_SOFTC(rdp->disks[disk]);
	    else
		adp = AD_SOFTC(rdp->disks[disk + rdp->width]);
	    if ((error = ar_rw(adp, rdp->lock_start,
			       size * DEV_BSIZE, buffer, AR_READ | AR_WAIT)))
		break;

	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		adp = AD_SOFTC(rdp->disks[disk + rdp->width]);
	    else
		adp = AD_SOFTC(rdp->disks[disk]);
	    if ((error = ar_rw(adp, rdp->lock_start,
			       size * DEV_BSIZE, buffer, AR_WRITE | AR_WAIT)))
		break;
	}
	if (error) {
	    wakeup(rdp);
	    free(buffer, M_AR);
	    kthread_exit(error);
	}
	s = splbio();
	rdp->lock_start = rdp->lock_end;
	rdp->lock_end = rdp->lock_start + size;
	splx(s);
	wakeup(rdp);
	sprintf(rdp->pid->p_comm, "rebuilding ar%d %lld%%", rdp->lun,
		(unsigned long long)(100 * rdp->lock_start /
				     (rdp->total_sectors / rdp->width)));
    }
    free(buffer, M_AR);
    for (disk = 0; disk < rdp->total_disks; disk++) {
	if ((rdp->disks[disk].flags&(AR_DF_PRESENT|AR_DF_ONLINE|AR_DF_SPARE))==
	    (AR_DF_PRESENT | AR_DF_SPARE)) {
	    rdp->disks[disk].flags &= ~AR_DF_SPARE;
	    rdp->disks[disk].flags |= (AR_DF_ASSIGNED | AR_DF_ONLINE);
	}
    }
    s = splbio();
    rdp->lock_start = 0xffffffff;
    rdp->lock_end = 0xffffffff;
    rdp->flags &= ~AR_F_REBUILDING;
    splx(s);
    ar_config_changed(rdp, 1);
    kthread_exit(0);
}

static int
ar_highpoint_read_conf(struct ad_softc *adp, struct ar_softc **raidp)
{
    struct highpoint_raid_conf *info;
    struct ar_softc *raid = NULL;
    int array, disk_number = 0, retval = 0;

    if (!(info = (struct highpoint_raid_conf *)
	  malloc(sizeof(struct highpoint_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return retval;

    if (ar_rw(adp, HPT_LBA, sizeof(struct highpoint_raid_conf),
	      (caddr_t)info, AR_READ | AR_WAIT)) {
	if (bootverbose)
	    printf("ar: HighPoint read conf failed\n");
	goto highpoint_out;
    }

    /* check if this is a HighPoint RAID struct */
    if (info->magic != HPT_MAGIC_OK && info->magic != HPT_MAGIC_BAD) {
	if (bootverbose)
	    printf("ar: HighPoint check1 failed\n");
	goto highpoint_out;
    }

    /* is this disk defined, or an old leftover/spare ? */
    if (!info->magic_0) {
	if (bootverbose)
	    printf("ar: HighPoint check2 failed\n");
	goto highpoint_out;
    }

    /* now convert HighPoint config info into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc*)malloc(sizeof(struct ar_softc), M_AR,
					 M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		printf("ar%d: failed to allocate raid config storage\n", array);
		goto highpoint_out;
	    }
	}
	raid = raidp[array];
	if (raid->flags & AR_F_PROMISE_RAID)
	    continue;

	switch (info->type) {
	case HPT_T_RAID0:
	    if ((info->order & (HPT_O_RAID0|HPT_O_OK))==(HPT_O_RAID0|HPT_O_OK))
		goto highpoint_raid1;
	    if (info->order & (HPT_O_RAID0 | HPT_O_RAID1))
		goto highpoint_raid01;
	    if (raid->magic_0 && raid->magic_0 != info->magic_0)
		continue;
	    raid->magic_0 = info->magic_0;
	    raid->flags |= AR_F_RAID0;
	    raid->interleave = 1 << info->stripe_shift;
	    disk_number = info->disk_number;
	    if (!(info->order & HPT_O_OK))
		info->magic = 0;	/* mark bad */
	    break;

	case HPT_T_RAID1:
highpoint_raid1:
	    if (raid->magic_0 && raid->magic_0 != info->magic_0)
		continue;
	    raid->magic_0 = info->magic_0;
	    raid->flags |= AR_F_RAID1;
	    disk_number = (info->disk_number > 0);
	    break;

	case HPT_T_RAID01_RAID0:
highpoint_raid01:
	    if (info->order & HPT_O_RAID0) {
		if ((raid->magic_0 && raid->magic_0 != info->magic_0) ||
		    (raid->magic_1 && raid->magic_1 != info->magic_1))
		    continue;
		raid->magic_0 = info->magic_0;
		raid->magic_1 = info->magic_1;
		raid->flags |= (AR_F_RAID0 | AR_F_RAID1);
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number;
	    }
	    else {
		if (raid->magic_1 && raid->magic_1 != info->magic_1)
		    continue;
		raid->magic_1 = info->magic_1;
		raid->flags |= (AR_F_RAID0 | AR_F_RAID1);
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number + info->array_width;
		if (!(info->order & HPT_O_RAID1))
		    info->magic = 0;	/* mark bad */
	    }
	    break;

	case HPT_T_SPAN:
	    if (raid->magic_0 && raid->magic_0 != info->magic_0)
		continue;
	    raid->magic_0 = info->magic_0;
	    raid->flags |= AR_F_SPAN;
	    disk_number = info->disk_number;
	    break;

	default:
	    printf("ar%d: HighPoint unknown RAID type 0x%02x\n",
		   array, info->type);
	    goto highpoint_out;
	}

	raid->flags |= AR_F_HIGHPOINT_RAID;
	raid->disks[disk_number].device = adp->device;
	raid->disks[disk_number].flags = (AR_DF_PRESENT | AR_DF_ASSIGNED);
	raid->lun = array;
	if (info->magic == HPT_MAGIC_OK) {
	    raid->disks[disk_number].flags |= AR_DF_ONLINE;
	    raid->flags |= AR_F_READY;
	    raid->width = info->array_width;
	    raid->heads = 255;
	    raid->sectors = 63;
	    raid->cylinders = info->total_sectors / (63 * 255);
	    raid->total_sectors = info->total_sectors;
	    raid->offset = HPT_LBA + 1;
	    raid->reserved = HPT_LBA + 1;
	    raid->lock_start = raid->lock_end = info->rebuild_lba;
	    raid->disks[disk_number].disk_sectors =
		info->total_sectors / info->array_width;
	}
	else
	    raid->disks[disk_number].flags &= ~ AR_DF_ONLINE;

	if ((raid->flags & AR_F_RAID0) && (raid->total_disks < raid->width))
	    raid->total_disks = raid->width;
	if (disk_number >= raid->total_disks)
	    raid->total_disks = disk_number + 1;
	retval = 1;
	break;
    }
highpoint_out:
    free(info, M_AR);
    return retval;
}

static int
ar_highpoint_write_conf(struct ar_softc *rdp)
{
    struct highpoint_raid_conf *config;
    struct timeval timestamp;
    int disk;

    microtime(&timestamp);
    rdp->magic_0 = timestamp.tv_sec + 2;
    rdp->magic_1 = timestamp.tv_sec;
   
    for (disk = 0; disk < rdp->total_disks; disk++) {
	if (!(config = (struct highpoint_raid_conf *)
	      malloc(sizeof(struct highpoint_raid_conf),
		     M_AR, M_NOWAIT | M_ZERO))) {
	    printf("ar%d: Highpoint write conf failed\n", rdp->lun);
	    return -1;
	}
	if ((rdp->disks[disk].flags & (AR_DF_PRESENT | AR_DF_ONLINE)) ==
	    (AR_DF_PRESENT | AR_DF_ONLINE))
	    config->magic = HPT_MAGIC_OK;
	if (rdp->disks[disk].flags & AR_DF_ASSIGNED) {
	    config->magic_0 = rdp->magic_0;
	    strcpy(config->name_1, "FreeBSD");
	}
	config->disk_number = disk;

	switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_RAID0:
	    config->type = HPT_T_RAID0;
	    strcpy(config->name_2, "RAID 0");
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		config->order = HPT_O_OK;
	    break;

	case AR_F_RAID1:
	    config->type = HPT_T_RAID0;
	    strcpy(config->name_2, "RAID 1");
	    config->disk_number = (disk < rdp->width) ? disk : disk + 5;
	    config->order = HPT_O_RAID0 | HPT_O_OK;
	    break;

	case AR_F_RAID0 | AR_F_RAID1:
	    config->type = HPT_T_RAID01_RAID0;
	    strcpy(config->name_2, "RAID 0+1");
	    if (rdp->disks[disk].flags & AR_DF_ONLINE) {
		if (disk < rdp->width) {
		    config->order = (HPT_O_RAID0 | HPT_O_RAID1);
		    config->magic_0 = rdp->magic_0 - 1;
		}
		else {
		    config->order = HPT_O_RAID1;
		    config->disk_number -= rdp->width;
		}
	    }
	    else
		config->magic_0 = rdp->magic_0 - 1;
	    config->magic_1 = rdp->magic_1;
	    break;

	case AR_F_SPAN:
	    config->type = HPT_T_SPAN;
	    strcpy(config->name_2, "SPAN");
	    break;
	}

	config->array_width = rdp->width;
	config->stripe_shift = (rdp->width > 1) ? (ffs(rdp->interleave)-1) : 0;
	config->total_sectors = rdp->total_sectors;
	config->rebuild_lba = rdp->lock_start;

	if (rdp->disks[disk].device && rdp->disks[disk].device->driver &&
	    !(rdp->disks[disk].device->flags & ATA_D_DETACHING)) {
	    if (ar_rw(AD_SOFTC(rdp->disks[disk]), HPT_LBA,
		      sizeof(struct highpoint_raid_conf),
		      (caddr_t)config, AR_WRITE)) {
		printf("ar%d: Highpoint write conf failed\n", rdp->lun);
		return -1;
	    }
	}
    }
    return 0;
}

static int
ar_promise_read_conf(struct ad_softc *adp, struct ar_softc **raidp, int local)
{
    struct promise_raid_conf *info;
    struct ar_softc *raid;
    u_int32_t magic, cksum, *ckptr;
    int array, count, disk, disksum = 0, retval = 0; 

    if (!(info = (struct promise_raid_conf *)
	  malloc(sizeof(struct promise_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return retval;

    if (ar_rw(adp, PR_LBA(adp), sizeof(struct promise_raid_conf),
	      (caddr_t)info, AR_READ | AR_WAIT)) {
	if (bootverbose)
	    printf("ar: %s read conf failed\n", local ? "FreeBSD" : "Promise");
	goto promise_out;
    }

    /* check if this is a Promise RAID struct (or our local one) */
    if (local) {
	if (strncmp(info->promise_id, ATA_MAGIC, sizeof(ATA_MAGIC))) {
	    if (bootverbose)
		printf("ar: FreeBSD check1 failed\n");
	    goto promise_out;
	}
    }
    else {
	if (strncmp(info->promise_id, PR_MAGIC, sizeof(PR_MAGIC))) {
	    if (bootverbose)
		printf("ar: Promise check1 failed\n");
	    goto promise_out;
	}
    }

    /* check if the checksum is OK */
    for (cksum = 0, ckptr = (int32_t *)info, count = 0; count < 511; count++)
	cksum += *ckptr++;
    if (cksum != *ckptr) {  
	if (bootverbose)
	    printf("ar: %s check2 failed\n", local ? "FreeBSD" : "Promise");	     
	goto promise_out;
    }

    /* now convert Promise config info into our generic form */
    if (info->raid.integrity != PR_I_VALID) {
	if (bootverbose)
	    printf("ar: %s check3 failed\n", local ? "FreeBSD" : "Promise");	     
	goto promise_out;
    }

    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc*)malloc(sizeof(struct ar_softc), M_AR,
					 M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		printf("ar%d: failed to allocate raid config storage\n", array);
		goto promise_out;
	    }
	}
	raid = raidp[array];
	if (raid->flags & AR_F_HIGHPOINT_RAID)
	    continue;

	magic = (adp->device->channel->chiptype >> 16) |
		(info->raid.array_number << 16);

	if (raid->flags & AR_F_PROMISE_RAID && magic != raid->magic_0)
	    continue;

	/* update our knowledge about the array config based on generation */
	if (!info->raid.generation || info->raid.generation > raid->generation){
	    raid->generation = info->raid.generation;
	    raid->flags = AR_F_PROMISE_RAID;
    	    if (local)
		raid->flags |= AR_F_FREEBSD_RAID;
	    raid->magic_0 = magic;
	    raid->lun = array;
	    if ((info->raid.status &
		 (PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY)) ==
		(PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY)) {
		raid->flags |= AR_F_READY;
		if (info->raid.status & PR_S_DEGRADED)
		    raid->flags |= AR_F_DEGRADED;
	    }
	    else
		raid->flags &= ~AR_F_READY;

	    switch (info->raid.type) {
	    case PR_T_RAID0:
		raid->flags |= AR_F_RAID0;
		break;

	    case PR_T_RAID1:
		raid->flags |= AR_F_RAID1;
		if (info->raid.array_width > 1)
		    raid->flags |= AR_F_RAID0;
		break;

	    case PR_T_SPAN:
		raid->flags |= AR_F_SPAN;
		break;

	    default:
		printf("ar%d: %s unknown RAID type 0x%02x\n",
		       array, local ? "FreeBSD" : "Promise", info->raid.type);
		goto promise_out;
	    }
	    raid->interleave = 1 << info->raid.stripe_shift;
	    raid->width = info->raid.array_width;
	    raid->total_disks = info->raid.total_disks;
	    raid->heads = info->raid.heads + 1;
	    raid->sectors = info->raid.sectors;
	    raid->cylinders = info->raid.cylinders + 1;
	    raid->total_sectors = info->raid.total_sectors;
	    raid->offset = 0;
	    raid->reserved = 63;
	    raid->lock_start = raid->lock_end = info->raid.rebuild_lba;

	    /* convert disk flags to our internal types */
	    for (disk = 0; disk < info->raid.total_disks; disk++) {
		raid->disks[disk].flags = 0;
		disksum += info->raid.disk[disk].flags;
		if (info->raid.disk[disk].flags & PR_F_ONLINE)
		    raid->disks[disk].flags |= AR_DF_ONLINE;
		if (info->raid.disk[disk].flags & PR_F_ASSIGNED)
		    raid->disks[disk].flags |= AR_DF_ASSIGNED;
		if (info->raid.disk[disk].flags & PR_F_SPARE) {
		    raid->disks[disk].flags &= ~AR_DF_ONLINE;
		    raid->disks[disk].flags |= AR_DF_SPARE;
		}
		if (info->raid.disk[disk].flags & (PR_F_REDIR | PR_F_DOWN))
		    raid->disks[disk].flags &= ~AR_DF_ONLINE;
	    }
	    if (!disksum) {
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto promise_out;
	    }
	}
	if (raid->disks[info->raid.disk_number].flags && adp->device) {
	    raid->disks[info->raid.disk_number].device = adp->device;
	    raid->disks[info->raid.disk_number].flags |= AR_DF_PRESENT;
	    raid->disks[info->raid.disk_number].disk_sectors =
		info->raid.disk_sectors;
	    retval = 1;
	}
	break;
    }
promise_out:
    free(info, M_AR);
    return retval;
}

static int
ar_promise_write_conf(struct ar_softc *rdp)
{
    struct promise_raid_conf *config;
    struct timeval timestamp;
    u_int32_t *ckptr;
    int count, disk, drive;
    int local = rdp->flags & AR_F_FREEBSD_RAID;

    rdp->generation++;
    microtime(&timestamp);

    for (disk = 0; disk < rdp->total_disks; disk++) {
	if (!(config = (struct promise_raid_conf *)
	      malloc(sizeof(struct promise_raid_conf), M_AR, M_NOWAIT))) {
	    printf("ar%d: %s write conf failed\n",
		   rdp->lun, local ? "FreeBSD" : "Promise");
	    return -1;
	}
	for (count = 0; count < sizeof(struct promise_raid_conf); count++)
	    *(((u_int8_t *)config) + count) = 255 - (count % 256);

	if (local)
	    bcopy(ATA_MAGIC, config->promise_id, sizeof(ATA_MAGIC));
	else
	    bcopy(PR_MAGIC, config->promise_id, sizeof(PR_MAGIC));
	config->dummy_0 = 0x00020000;
	config->magic_0 = PR_MAGIC0(rdp->disks[disk]) | timestamp.tv_sec;
	config->magic_1 = timestamp.tv_sec >> 16;
	config->magic_2 = timestamp.tv_sec;
	config->raid.integrity = PR_I_VALID;

	config->raid.disk_number = disk;
	if (rdp->disks[disk].flags & AR_DF_PRESENT && rdp->disks[disk].device) {
	    config->raid.channel = rdp->disks[disk].device->channel->unit;
	    config->raid.device = (rdp->disks[disk].device->unit != 0);
	    if (AD_SOFTC(rdp->disks[disk])->dev->si_disk)
		config->raid.disk_sectors = PR_LBA(AD_SOFTC(rdp->disks[disk]));
	    /*config->raid.disk_offset*/
	}
	config->raid.magic_0 = config->magic_0;
	config->raid.rebuild_lba = rdp->lock_start;
	config->raid.generation = rdp->generation;

	if (rdp->flags & AR_F_READY) {
	    config->raid.flags = (PR_F_VALID | PR_F_ASSIGNED | PR_F_ONLINE);
	    config->raid.status = 
		(PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY);
	    if (rdp->flags & AR_F_DEGRADED)
		config->raid.status |= PR_S_DEGRADED;
	    else
		config->raid.status |= PR_S_FUNCTIONAL;
	}
	else {
	    config->raid.flags = PR_F_DOWN;
	    config->raid.status = 0;
	}

	switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_RAID0:
	    config->raid.type = PR_T_RAID0;
	    break;
	case AR_F_RAID1:
	    config->raid.type = PR_T_RAID1;
	    break;
	case AR_F_RAID0 | AR_F_RAID1:
	    config->raid.type = PR_T_RAID1;
	    break;
	case AR_F_SPAN:
	    config->raid.type = PR_T_SPAN;
	    break;
	}

	config->raid.total_disks = rdp->total_disks;
	config->raid.stripe_shift = ffs(rdp->interleave) - 1;
	config->raid.array_width = rdp->width;
	config->raid.array_number = rdp->lun;
	config->raid.total_sectors = rdp->total_sectors;
	config->raid.cylinders = rdp->cylinders - 1;
	config->raid.heads = rdp->heads - 1;
	config->raid.sectors = rdp->sectors;
	config->raid.magic_1 = (u_int64_t)config->magic_2<<16 | config->magic_1;

	bzero(&config->raid.disk, 8 * 12);
	for (drive = 0; drive < rdp->total_disks; drive++) {
	    config->raid.disk[drive].flags = 0;
	    if (rdp->disks[drive].flags & AR_DF_PRESENT)
		config->raid.disk[drive].flags |= PR_F_VALID;
	    if (rdp->disks[drive].flags & AR_DF_ASSIGNED)
		config->raid.disk[drive].flags |= PR_F_ASSIGNED;
	    if (rdp->disks[drive].flags & AR_DF_ONLINE)
		config->raid.disk[drive].flags |= PR_F_ONLINE;
	    else
		if (rdp->disks[drive].flags & AR_DF_PRESENT)
		    config->raid.disk[drive].flags = (PR_F_REDIR | PR_F_DOWN);
	    if (rdp->disks[drive].flags & AR_DF_SPARE)
		config->raid.disk[drive].flags |= PR_F_SPARE;
	    config->raid.disk[drive].dummy_0 = 0x0;
	    if (rdp->disks[drive].device) {
		config->raid.disk[drive].channel =
		    rdp->disks[drive].device->channel->unit;
		config->raid.disk[drive].device =
		    (rdp->disks[drive].device->unit != 0);
	    }
	    config->raid.disk[drive].magic_0 =
		PR_MAGIC0(rdp->disks[drive]) | timestamp.tv_sec;
	}

	config->checksum = 0;
	for (ckptr = (int32_t *)config, count = 0; count < 511; count++)
	    config->checksum += *ckptr++;
	if (rdp->disks[disk].device && rdp->disks[disk].device->driver &&
	    !(rdp->disks[disk].device->flags & ATA_D_DETACHING)) {
	    if (ar_rw(AD_SOFTC(rdp->disks[disk]),
		      PR_LBA(AD_SOFTC(rdp->disks[disk])),
		      sizeof(struct promise_raid_conf),
		      (caddr_t)config, AR_WRITE)) {
		printf("ar%d: %s write conf failed\n",
		       rdp->lun, local ? "FreeBSD" : "Promise");
		return -1;
	    }
	}
    }
    return 0;
}

static void
ar_rw_done(struct bio *bp)
{
    free(bp->bio_data, M_AR);
    free(bp, M_AR);
}

static int
ar_rw(struct ad_softc *adp, u_int32_t lba, int count, caddr_t data, int flags)
{
    struct bio *bp;
    int retry = 0, error = 0;

    if (!(bp = (struct bio *)malloc(sizeof(struct bio), M_AR, M_NOWAIT|M_ZERO)))
	return 1;
    bp->bio_dev = adp->dev;
    bp->bio_data = data;
    bp->bio_pblkno = lba;
    bp->bio_bcount = count;
    if (flags & AR_READ)
	bp->bio_cmd = BIO_READ;
    if (flags & AR_WRITE)
	bp->bio_cmd = BIO_WRITE;
    if (flags & AR_WAIT)
	bp->bio_done = (void *)wakeup;
    else
	bp->bio_done = ar_rw_done;

    AR_STRATEGY(bp);

    if (flags & AR_WAIT) {
	while ((retry++ < (15*hz/10)) && (error = !(bp->bio_flags & BIO_DONE)))
	    error = tsleep(bp, PRIBIO, "arrw", 10);
	if (!error && bp->bio_flags & BIO_ERROR)
	    error = bp->bio_error;
	free(bp, M_AR);
    }
    return error;
}

static struct ata_device *
ar_locate_disk(int diskno)
{
    struct ata_channel *ch;
    int ctlr;

    for (ctlr = 0; ctlr < devclass_get_maxunit(ata_devclass); ctlr++) {
	if (!(ch = devclass_get_softc(ata_devclass, ctlr)))
	    continue;
	if (ch->devices & ATA_ATA_MASTER)
	    if (ch->device[MASTER].driver &&
		((struct ad_softc *)(ch->device[MASTER].driver))->lun == diskno)
		return &ch->device[MASTER];
	if (ch->devices & ATA_ATA_SLAVE)
	    if (ch->device[SLAVE].driver &&
		((struct ad_softc *)(ch->device[SLAVE].driver))->lun == diskno)
		return &ch->device[SLAVE];
    }
    return NULL;
}
