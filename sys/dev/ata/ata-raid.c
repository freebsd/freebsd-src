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

#include "opt_global.h"
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
#include <sys/devicestat.h>
#include <sys/cons.h>
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
static void ar_done(struct bio *);
static void ar_config_changed(struct ar_softc *, int);
static int ar_rebuild(struct ar_softc *);
static int ar_highpoint_read_conf(struct ad_softc *, struct ar_softc **);
static int ar_highpoint_write_conf(struct ar_softc *);
static int ar_promise_read_conf(struct ad_softc *, struct ar_softc **);
static int ar_promise_write_conf(struct ar_softc *);
static int ar_rw(struct ad_softc *, u_int32_t, int, caddr_t, int);

/* misc defines */
#define AD_STRATEGY(x)	si_disk->d_devsw->d_strategy(x)
#define AD_SOFTC(x)	((struct ad_softc *)(x.device->driver))
#define AR_READ		0x01
#define AR_WRITE	0x02
#define AR_WAIT		0x04
  
/* internal vars */
static struct ar_softc **ar_table = NULL;
static MALLOC_DEFINE(M_AR, "AR driver", "ATA RAID driver");

int
ata_raiddisk_attach(struct ad_softc *adp)
{
    struct ar_softc *rdp;
    int array, disk;

    switch(adp->device->channel->chiptype) {
    default:
	return 0;

    case 0x4d33105a: case 0x4d38105a: case 0x4d30105a:
    case 0x0d30105a: case 0x4d68105a: case 0x6268105a:
    case 0x00041103: case 0x00051103: case 0x00081103:
    }
    if (ar_table) {
	for (array = 0; array < MAX_ARRAYS; array++) {
	    if (!(rdp = ar_table[array]) || !rdp->flags)
		continue;
   
	    for (disk = 0; disk < rdp->total_disks; disk++) {
		if (rdp->disks[disk].device == adp->device) {
		    ata_prtdev(rdp->disks[disk].device,
			       "inserted into ar%d disk%d as spare\n",
			       array, disk);
		    rdp->disks[disk].flags = (AR_DF_PRESENT | AR_DF_SPARE);
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
	/* test RAID bit in PCI reg XXX */
	return (ar_promise_read_conf(adp, ar_table));

    case 0x00041103: case 0x00051103: case 0x00081103:
	return (ar_highpoint_read_conf(adp, ar_table));
    }
    return 0;
}

int
ata_raiddisk_detach(struct ad_softc *adp)
{
    struct ar_softc *rdp;
    int array, disk;

    switch(adp->device->channel->chiptype) {
    default:
	return 0;

    case 0x4d33105a: case 0x4d38105a: case 0x4d30105a:
    case 0x0d30105a: case 0x4d68105a: case 0x6268105a:
    case 0x00041103: case 0x00051103: case 0x00081103:
    }
    if (ar_table) {
	for (array = 0; array < MAX_ARRAYS; array++) {
	    if (!(rdp = ar_table[array]) || !rdp->flags)
		continue; 
	    for (disk = 0; disk < rdp->total_disks; disk++) {
		if (rdp->disks[disk].device == adp->device) {
		    ata_prtdev(rdp->disks[disk].device,
			       "deleted from ar%d disk%d\n", array, disk);
		    rdp->disks[disk].flags &= ~(AR_DF_PRESENT | AR_DF_ONLINE);
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
    dev_t dev;
    int array, disk;

    if (!ar_table)
	return;

    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!(rdp = ar_table[array]) || !rdp->flags)
	    continue;
   
	ar_config_changed(rdp, 0);
	dev = disk_create(rdp->lun, &rdp->disk, 0, &ar_cdevsw,&ardisk_cdevsw);
	dev->si_drv1 = rdp;
	dev->si_iosize_max = 256 * DEV_BSIZE;
	rdp->dev = dev;

	printf("ar%d: %lluMB <ATA ",
	       rdp->lun, (unsigned long long)
	       rdp->total_sectors / ((1024L * 1024L) / DEV_BSIZE));
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
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		printf(" %d READY ", disk);
	    else if (rdp->disks[disk].flags & AR_DF_ASSIGNED)
		printf(" %d DOWN  ", disk);
	    else if (rdp->disks[disk].flags & AR_DF_SPARE)
		printf(" %d SPARE ", disk);
	    else if (rdp->disks[disk].flags & AR_DF_PRESENT)
		printf(" %d FREE  ", disk);
	    else
		printf(" %d INVALID no RAID config info on this disk\n", disk);
	    if (rdp->disks[disk].flags & AR_DF_PRESENT)
		ad_print(AD_SOFTC(rdp->disks[disk]));
	}
    }
}

int
ata_raid_rebuild(int array)
{
    struct ar_softc *rdp;

    if (!ar_table || !(rdp = ar_table[array]))
	return ENXIO;
    return ar_rebuild(rdp);
}

static int
aropen(dev_t dev, int flags, int fmt, struct thread *td)
{
    struct ar_softc *rdp = dev->si_drv1;
    struct disklabel *dl;
	
    dl = &rdp->disk.d_label;
    bzero(dl, sizeof *dl);
    dl->d_secsize = DEV_BSIZE;
    dl->d_nsectors = rdp->sectors;
    dl->d_ntracks = rdp->heads;
    dl->d_ncylinders = rdp->cylinders;
    dl->d_secpercyl = rdp->sectors * rdp->heads;
    dl->d_secperunit = rdp->total_sectors;
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
	    if (rdp->disks[buf1->drive].flags & AR_DF_ONLINE &&
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
	    buf1->bp.bio_dev->AD_STRATEGY((struct bio *)buf1);
	    break;

	case AR_F_RAID1:
	case AR_F_RAID0 | AR_F_RAID1:
	    if (rdp->flags & AR_F_REBUILDING) {
		int start = rdp->lock_start / rdp->width;
		int end = rdp->lock_end / rdp->width;

		if ((bp->bio_pblkno >= end && bp->bio_pblkno < end) ||
		     ((bp->bio_pblkno + chunk) >= start &&
		      (bp->bio_pblkno + chunk) < end)) {
		    tsleep(rdp, PRIBIO, "arwait", 0);
		}
	    }
	    if (rdp->disks[buf1->drive].flags & AR_DF_ONLINE &&
		!AD_SOFTC(rdp->disks[buf1->drive])->dev->si_disk) {
		rdp->disks[buf1->drive].flags &= ~AR_DF_ONLINE;
		change = 1;
	    }
	    if (rdp->disks[buf1->drive + rdp->width].flags & AR_DF_ONLINE &&
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
	    if (bp->bio_cmd == BIO_WRITE) {
		if (rdp->disks[buf1->drive + rdp->width].flags & AR_DF_ONLINE) {
		    if (rdp->disks[buf1->drive].flags & AR_DF_ONLINE) {
			buf2 = malloc(sizeof(struct ar_buf), M_AR, M_NOWAIT);
			bcopy(buf1, buf2, sizeof(struct ar_buf));
			buf1->mirror = buf2;
			buf2->mirror = buf1;
			buf2->drive = buf1->drive + rdp->width;
			buf2->bp.bio_dev =
			    AD_SOFTC(rdp->disks[buf2->drive])->dev;
			buf2->bp.bio_dev->AD_STRATEGY((struct bio *)buf2);
			rdp->disks[buf2->drive].last_lba =
			    buf2->bp.bio_pblkno + chunk;
		    }
		    else
			buf1->drive = buf1->drive + rdp->width;
		}
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
	    buf1->bp.bio_dev = AD_SOFTC(rdp->disks[buf1->drive])->dev;
	    buf1->bp.bio_dev->AD_STRATEGY((struct bio *)buf1);
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
		    buf->bp.bio_dev->AD_STRATEGY((struct bio *)buf);
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
	if ((rdp->disks[disk].flags & AR_DF_PRESENT) &&
	    rdp->disks[disk].device) {
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		ata_drawerleds(rdp->disks[disk].device, ATA_LED_GREEN);
	    else
		ata_drawerleds(rdp->disks[disk].device, ATA_LED_RED);
	}
    }
    if (writeback) {
	if (rdp->flags & AR_F_PROMISE_RAID)
	    ar_promise_write_conf(rdp);
	if (rdp->flags & AR_F_HIGHPOINT_RAID)
	    ar_highpoint_write_conf(rdp);
    }
}

static int
ar_rebuild(struct ar_softc *rdp)
{
    caddr_t buffer;
    int count = 0;
    int disk;

    if ((rdp->flags & (AR_F_READY|AR_F_DEGRADED)) != (AR_F_READY|AR_F_DEGRADED))
	return EEXIST;

    for (disk = 0; disk < rdp->total_disks; disk++) {
	if (((rdp->disks[disk].flags&(AR_DF_PRESENT|AR_DF_ONLINE|AR_DF_SPARE))==
	     (AR_DF_PRESENT | AR_DF_SPARE)) && rdp->disks[disk].device) {
	    if (AD_SOFTC(rdp->disks[disk])->total_secs <
		rdp->disks[disk].disk_sectors) {
		ata_prtdev(rdp->disks[disk].device,
			   "disk capacity too small for this RAID config\n");
		rdp->disks[disk].flags &= ~AR_DF_SPARE;
		continue;
	    }
	    ata_drawerleds(rdp->disks[disk].device, ATA_LED_ORANGE);
	    count++;
	}
    }
    if (!count)
	return ENODEV;

    /* setup start conditions */
    rdp->lock_start = 0;
    rdp->lock_end = rdp->lock_start + 256;
    rdp->flags |= AR_F_REBUILDING;
    buffer = malloc(256 * DEV_BSIZE, M_AR, M_NOWAIT | M_ZERO);

    /* now go copy entire disk(s) */
    while (rdp->lock_start < rdp->total_sectors) {
	for (disk = 0; disk < rdp->width; disk++) {
	    if (((rdp->disks[disk].flags & AR_DF_ONLINE) &&
		 (rdp->disks[disk + rdp->width].flags & AR_DF_ONLINE)) ||
		((rdp->disks[disk].flags & AR_DF_ONLINE) && 
		 !(rdp->disks[disk + rdp->width].flags & AR_DF_SPARE)) ||
		((rdp->disks[disk + rdp->width].flags & AR_DF_ONLINE) &&
		 !(rdp->disks[disk].flags & AR_DF_SPARE)))
		continue;
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		ar_rw(AD_SOFTC(rdp->disks[disk]), rdp->lock_start,
		      256 * DEV_BSIZE, buffer, AR_READ | AR_WAIT);
	    else
		ar_rw(AD_SOFTC(rdp->disks[disk + rdp->width]), rdp->lock_start,
		      256 * DEV_BSIZE, buffer, AR_READ | AR_WAIT);

	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		ar_rw(AD_SOFTC(rdp->disks[disk + rdp->width]), rdp->lock_start,
		      256 * DEV_BSIZE, buffer, AR_WRITE | AR_WAIT);
	    else
		ar_rw(AD_SOFTC(rdp->disks[disk]), rdp->lock_start,
		      256 * DEV_BSIZE, buffer, AR_WRITE | AR_WAIT);
	}
	rdp->lock_start = rdp->lock_end;
	rdp->lock_end =
	    rdp->lock_start + min(256, rdp->total_sectors - rdp->lock_end);
	wakeup(rdp);
    }
    free(buffer, M_AR);
    for (disk = 0; disk < rdp->total_disks; disk++) {
	if ((rdp->disks[disk].flags&(AR_DF_PRESENT|AR_DF_ONLINE|AR_DF_SPARE))==
	    (AR_DF_PRESENT | AR_DF_SPARE)) {
	    rdp->disks[disk].flags &= ~AR_DF_SPARE;
	    rdp->disks[disk].flags |= (AR_DF_ASSIGNED | AR_DF_ONLINE);
	}
    }
    rdp->flags &= ~AR_F_REBUILDING;
    ar_config_changed(rdp, 1);
    return 0;
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
	case HPT_T_RAID01_RAID0:
	    /* check the order byte to determine what this really is */
	    switch (info->order) {
	    case HPT_O_DOWN:
		if (raid->magic_0 && raid->magic_0 != info->magic_0)
		    continue;
		raid->magic_0 = info->magic_0;
		raid->magic_1 = info->magic_1;
		raid->flags |= AR_F_RAID0;
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number;
		info->magic = 0;	/* mark bad */
		break;

	    case HPT_O_RAID01DEGRADED:
		if (raid->magic_0 && raid->magic_0 != info->magic_0)
		    continue;
		raid->magic_0 = info->magic_0;
		raid->magic_1 = info->magic_1;
		raid->flags |= AR_F_RAID0;
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number;
		break;

	    case HPT_O_RAID01SRC:
		if ((raid->magic_0 && raid->magic_0 != info->magic_0) ||
		    (raid->magic_1 && raid->magic_1 != info->magic_1))
		    continue;
		raid->magic_0 = info->magic_0;
		raid->magic_1 = info->magic_1;
		raid->flags |= (AR_F_RAID0 | AR_F_RAID1);
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number;
		break;

	    case HPT_O_RAID01DST:
		if (raid->magic_1 && raid->magic_1 != info->magic_1)
		    continue;
		raid->magic_1 = info->magic_1;
		raid->flags |= (AR_F_RAID0 | AR_F_RAID1);
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number + info->array_width;
		break;

	    case HPT_O_READY:
		if (raid->magic_0 && raid->magic_0 != info->magic_0)
		    continue;
		raid->magic_0 = info->magic_0;
		raid->flags |= AR_F_RAID0;
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number;
		break;
	    }
	    break;

	case HPT_T_RAID1:
	    if (raid->magic_0 && raid->magic_0 != info->magic_0)
		continue;
	    raid->magic_0 = info->magic_0;
	    raid->flags |= AR_F_RAID1;
	    disk_number = (info->disk_number > 0);
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
	if (info->magic == HPT_MAGIC_OK) {
	    raid->disks[disk_number].flags |= AR_DF_ONLINE;
	    raid->flags |= AR_F_READY;
	    raid->lun = array;
	    raid->width = info->array_width;
	    raid->heads = 255;
	    raid->sectors = 63;
	    raid->cylinders = (info->total_sectors - HPT_LBA) / (63 * 255);
	    raid->total_sectors = info->total_sectors - (HPT_LBA * raid->width);
	    raid->offset = 10;
	    raid->reserved = 10;
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
    rdp->magic_0 = timestamp.tv_sec + 1;
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
	if (rdp->disks[disk].flags & AR_DF_ASSIGNED)
	    config->magic_0 = rdp->magic_0;
	config->disk_number = disk;

	switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_RAID0:
	    config->type = HPT_T_RAID0;
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		config->order = HPT_O_READY;
	    else
		config->order = HPT_O_DOWN;
	    break;

	case AR_F_RAID1:
	    config->type = HPT_T_RAID1;
	    config->disk_number = (disk < rdp->width) ? disk : disk + 10;
	    break;

	case AR_F_RAID0 | AR_F_RAID1:
	    config->magic_1 = rdp->magic_1;
	    config->type = HPT_T_RAID01_RAID0;
#if 0
	    if ((rdp->flags & (AR_F_READY | AR_F_DEGRADED)) == AR_F_READY)
		if (disk < rdp->width)
		    config->order = HPT_O_RAID01SRC;
		else
		    config->order = HPT_O_RAID01DST;
	    else 
		if (rdp->disks[disk].flags & AR_DF_ONLINE)
		    config->order = HPT_O_RAID01DEGRADED;
		else
		    config->order = HPT_O_DOWN;
#else
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		if (disk < rdp->width)
		    config->order = HPT_O_RAID01SRC;
		else
		    config->order = HPT_O_RAID01DST;
	    else 
		config->order = HPT_O_DOWN;
#endif
	    if (disk >= rdp->width) {
		config->magic_0 = rdp->magic_0 + 1;
		config->disk_number -= rdp->width;
	    }
	    break;

	case AR_F_SPAN:
	    config->type = HPT_T_SPAN;
	    break;
	}

	config->array_width = rdp->width;
	config->stripe_shift = (rdp->width > 1) ? (ffs(rdp->interleave)-1) : 0;
	config->total_sectors = rdp->total_sectors;

	if ((rdp->disks[disk].flags & AR_DF_PRESENT) &&
	    rdp->disks[disk].device && rdp->disks[disk].device->driver &&
	    !(rdp->disks[disk].device->flags & ATA_D_DETACHING)) {
	    if (ar_rw(AD_SOFTC(rdp->disks[disk]), HPT_LBA,
		      sizeof(struct highpoint_raid_conf),
		      (caddr_t)config, AR_WRITE)) {
		if (bootverbose)
		    printf("ar%d: Highpoint write conf failed\n", rdp->lun);
		return -1;
	    }
	}
    }
    return 0;
}

static int
ar_promise_read_conf(struct ad_softc *adp, struct ar_softc **raidp)
{
    struct promise_raid_conf *info;
    struct ar_softc *raid;
    u_int32_t magic, cksum, *ckptr;
    int array, count, disk, retval = 0; 

    if (!(info = (struct promise_raid_conf *)
	  malloc(sizeof(struct promise_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return retval;

    if (ar_rw(adp, PR_LBA(adp), sizeof(struct promise_raid_conf),
	      (caddr_t)info, AR_READ | AR_WAIT)) {
	if (bootverbose)
	    printf("ar: Promise read conf failed\n");
	goto promise_out;
    }

    /* check if this is a Promise RAID struct */
    if (strncmp(info->promise_id, PR_MAGIC, sizeof(PR_MAGIC))) {
	if (bootverbose)
	    printf("ar: Promise check1 failed\n");
	goto promise_out;
    }	

    /* check if the checksum is OK */
    for (cksum = 0, ckptr = (int32_t *)info, count = 0; count < 511; count++)
	cksum += *ckptr++;
    if (cksum != *ckptr) {  
	if (bootverbose)
	    printf("ar: Promise check2 failed\n");	     
	goto promise_out;
    }

    /* now convert Promise config info into our generic form */
    if (info->raid.integrity != PR_I_VALID) {
	if (bootverbose)
	    printf("ar: Promise check3 failed\n");	     
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
		printf("ar%d: Promise unknown RAID type 0x%02x\n",
		       array, info->raid.type);
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

	    /* convert disk flags to our internal types */
	    for (disk = 0; disk < info->raid.total_disks; disk++) {
		raid->disks[disk].flags = 0;
		if (info->raid.disk[disk].flags & PR_F_ONLINE)
		    raid->disks[disk].flags |= AR_DF_ONLINE;
		if (info->raid.disk[disk].flags & PR_F_ASSIGNED)
		    raid->disks[disk].flags |= AR_DF_ASSIGNED;
		if (info->raid.disk[disk].flags & PR_F_SPARE)
		    raid->disks[disk].flags |= AR_DF_SPARE;
		if (info->raid.disk[disk].flags & (PR_F_REDIR | PR_F_DOWN)) {
		    raid->disks[disk].flags &= ~AR_DF_ONLINE;
		    raid->disks[disk].flags |= AR_DF_PRESENT;
		}
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

    rdp->generation++;
    microtime(&timestamp);

    for (disk = 0; disk < rdp->total_disks; disk++) {
	if (!(config = (struct promise_raid_conf *)
	      malloc(sizeof(struct promise_raid_conf), M_AR, M_NOWAIT))) {
	    printf("ar%d: Promise write conf failed\n", rdp->lun);
	    return -1;
	}
	for (count = 0; count < sizeof(struct promise_raid_conf); count++)
	    *(((u_int8_t *)config) + count) = 255 - (count % 256);

	bcopy(PR_MAGIC, config->promise_id, sizeof(PR_MAGIC));
	config->dummy_0 = 0x00020000;
	config->magic_0 = PR_MAGIC0(rdp->disks[disk]) | timestamp.tv_sec;
	config->magic_1 = timestamp.tv_sec >> 16;
	config->magic_2 = timestamp.tv_sec;

	config->raid.integrity = PR_I_VALID;

	config->raid.flags = 0;
	if (rdp->disks[disk].flags & AR_DF_PRESENT)
	    config->raid.flags |= PR_F_VALID;
	if (rdp->disks[disk].flags & AR_DF_ASSIGNED)
	    config->raid.flags |= PR_F_ASSIGNED;
	if (rdp->disks[disk].flags & AR_DF_ONLINE)
	    config->raid.flags |= PR_F_ONLINE;
	else
	    config->raid.flags |= PR_F_DOWN;
	config->raid.disk_number = disk;
	if (rdp->disks[disk].flags & AR_DF_PRESENT && rdp->disks[disk].device) {
	    config->raid.channel = rdp->disks[disk].device->channel->unit;
	    config->raid.device = (rdp->disks[disk].device->unit != 0);
	    if (AD_SOFTC(rdp->disks[disk])->dev->si_disk)
		config->raid.disk_sectors = PR_LBA(AD_SOFTC(rdp->disks[disk]));
	    /*config->raid.disk_offset*/
	}
	config->raid.magic_0 = config->magic_0;
	config->raid.rebuild_lba = 0xffffffff;
	config->raid.generation = rdp->generation;

	if (rdp->flags & AR_F_READY) {
	    config->raid.status = 
		(PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY);
	    if (rdp->flags & AR_F_DEGRADED)
		config->raid.status |= PR_S_DEGRADED;
	    else
		config->raid.status |= PR_S_FUNCTIONAL;
	}
	else
	    config->raid.status = 0;

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
	config->raid.array_number = rdp->magic_0 >> 16;
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
	if ((rdp->disks[disk].flags & AR_DF_PRESENT) &&
	    rdp->disks[disk].device && rdp->disks[disk].device->driver &&
	    !(rdp->disks[disk].device->flags & ATA_D_DETACHING)) {
	    if (ar_rw(AD_SOFTC(rdp->disks[disk]),
		      PR_LBA(AD_SOFTC(rdp->disks[disk])),
		      sizeof(struct promise_raid_conf),
		      (caddr_t)config, AR_WRITE)) {
		if (bootverbose)
		    printf("ar%d: Promise write conf failed\n", rdp->lun);
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
    int s;

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
    s = splbio();
    bp->bio_dev->AD_STRATEGY(bp);
    splx(s);
    if (flags & AR_WAIT) {
	tsleep(bp, PRIBIO, "arrw", 0);
	free(bp, M_AR);
    }
    return 0;
}
