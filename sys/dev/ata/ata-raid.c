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
static int ar_highpoint_read_conf(struct ad_softc *, struct ar_softc **);
static int ar_promise_read_conf(struct ad_softc *, struct ar_softc **);
static int ar_read(struct ad_softc *, u_int32_t, int, u_int8_t *);

/* misc defines */
#define AD_STRATEGY(x)	si_disk->d_devsw->d_strategy(x)
#define AD_SOFTC(x)	((struct ad_softc *)(x.device->driver))
  
/* internal vars */
static int ar_init = 0;
static struct ar_softc *ar_table[MAX_ARRAYS];
static MALLOC_DEFINE(M_AR, "AR driver", "ATA RAID driver");

int
ar_probe(struct ad_softc *adp) {
    if (!ar_init) {
	bzero(&ar_table, sizeof(ar_table));
	ar_init = 1;
    }

    switch(adp->device->channel->chiptype) {
    case 0x4d33105a:
    case 0x4d38105a:
    case 0x4d30105a:
    case 0x0d30105a:
    case 0x4d68105a:
    case 0x6268105a:
	/* test RAID bit in PCI reg */
	return (ar_promise_read_conf(adp, ar_table));

    case 0x00041103:
	/* test RAID bit in PCI reg */
	return (ar_highpoint_read_conf(adp, ar_table));
    }
    return 1;
}

void
ar_attach()
{
    struct ar_softc *raid;
    dev_t dev;
    int array, disk;

    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!(raid = ar_table[array]) || !raid->flags) {
	    continue;
	}
   
	for (disk = 0; disk < raid->total_disks; disk++) {
	    if (raid->disks[disk].device) {
		switch (raid->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
		case AR_F_SPAN:
		case AR_F_RAID0:
		    if (raid->disks[disk].flags & AR_DF_ONLINE)
			ata_drawerleds(raid->disks[disk].device, ATA_LED_GREEN);
		    else {
			raid->flags &= ~AR_F_READY;
			ata_drawerleds(raid->disks[disk].device, ATA_LED_RED);
		    }
		    break;

		case AR_F_RAID1:
		case AR_F_RAID0 | AR_F_RAID1:
		    if (disk < raid->width) {
			if (!(raid->disks[disk].flags & AR_DF_ONLINE) &&
			    !(raid->disks[disk+raid->width].flags&AR_DF_ONLINE))
			    raid->flags &= ~AR_F_READY;
			else if (((raid->disks[disk].flags & AR_DF_ONLINE) &&
				 !(raid->disks
				   [disk + raid->width].flags & AR_DF_ONLINE))||
				 (!(raid->disks[disk].flags & AR_DF_ONLINE) &&
				 (raid->disks
				   [disk + raid->width].flags & AR_DF_ONLINE)))
			    raid->flags |= AR_F_DEGRADED;
		    }
		    if (raid->disks[disk].flags & AR_DF_ONLINE)
			ata_drawerleds(raid->disks[disk].device, ATA_LED_GREEN);
		    else
			ata_drawerleds(raid->disks[disk].device, ATA_LED_RED);
		    break;
		}
	    }
	    else {
		raid->disks[disk].flags &=
		    ~(AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE);
	    }
	}

	dev = disk_create(raid->lun, &raid->disk, 0, &ar_cdevsw,&ardisk_cdevsw);
	dev->si_drv1 = raid;
	dev->si_iosize_max = 256 * DEV_BSIZE;
	raid->dev = dev;

	printf("ar%d: %lluMB <ATA ",
	       raid->lun, raid->total_sectors / ((1024L * 1024L) / DEV_BSIZE));
	switch (raid->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_RAID0:
	    printf("RAID0 "); break;
	case AR_F_RAID1:
	    printf("RAID1 "); break;
	case AR_F_SPAN:
	    printf("SPAN "); break;
	case (AR_F_RAID0 | AR_F_RAID1):
	    printf("RAID0+1 "); break;
	default:
	    printf("unknown 0x%x ", raid->flags);
	    return;
	}
	printf("array> [%d/%d/%d] status: ",
	       raid->cylinders, raid->heads, raid->sectors);
	switch (raid->flags & (AR_F_DEGRADED | AR_F_READY)) {
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
	for (disk = 0; disk < raid->total_disks; disk++) {
	    if (raid->disks[disk].flags & AR_DF_ONLINE)
		printf(" %d READY ", disk);
	    else if (raid->disks[disk].flags & AR_DF_ASSIGNED)
		printf(" %d DOWN  ", disk);
	    else if (raid->disks[disk].flags & AR_DF_SPARE)
		printf(" %d SPARE ", disk);
	    else if (raid->disks[disk].flags & AR_DF_PRESENT)
		printf(" %d FREE  ", disk);
	    else
		printf(" %d FAILURE no device present\n", disk);
	    if (raid->disks[disk].flags & AR_DF_PRESENT)
		ad_print(AD_SOFTC(raid->disks[disk]), "");
	}
    }
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
    int lba, count, chunk;
    caddr_t data;

    if (!(rdp->flags & AR_F_READY)) {
	bp->bio_flags |= BIO_ERROR;
	bp->bio_error = EIO;
	biodone(bp);
	return;
    }
    bp->bio_resid = bp->bio_bcount;
    lba = bp->bio_pblkno;
    data = bp->bio_data;
    for (count = howmany(bp->bio_bcount, DEV_BSIZE); count > 0; 
	 count -= chunk, lba += chunk, data += (chunk * DEV_BSIZE)) {
	struct ar_buf *buf1, *buf2;
	int plba;

	buf1 = malloc(sizeof(struct ar_buf), M_AR, M_NOWAIT | M_ZERO);
	switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {
	case AR_F_SPAN:
	    plba = lba;
	    while (plba >=
		AD_SOFTC(rdp->disks[buf1->drive])->total_secs-rdp->reserved)
		plba -= (AD_SOFTC(rdp->disks[buf1->drive++])->total_secs -
			rdp->reserved);
	    buf1->bp.bio_pblkno = plba;
	    chunk = min(AD_SOFTC(rdp->disks[buf1->drive])->total_secs - 
			rdp->reserved - plba, count);
	    break;
	
	case AR_F_RAID0:
	case AR_F_RAID0 | AR_F_RAID1:
	    plba = lba / rdp->interleave;
	    chunk = lba % rdp->interleave;
	    if (plba == rdp->total_sectors / rdp->interleave) {
		int lastblksize = 
		    (rdp->total_sectors-(plba*rdp->interleave))/rdp->width;

		buf1->drive = chunk / lastblksize;
		buf1->bp.bio_pblkno =
		    ((plba / rdp->width) * rdp->interleave) + chunk%lastblksize;
		chunk = min(count, lastblksize);
	    }
	    else {
		buf1->drive = plba % rdp->width;
		buf1->bp.bio_pblkno = 
		    ((plba / rdp->width) * rdp->interleave) + chunk;
		chunk = min(count, rdp->interleave - chunk);
	    }
	    break;

	case AR_F_RAID1:
	    buf1->bp.bio_pblkno = lba;
	    buf1->drive = 0;
	    chunk = count;
	    break;

	default:
	    printf("Oops! unknown array type in arstrategy\n");
	    bp->bio_flags |= BIO_ERROR;
	    bp->bio_error = EIO;
	    biodone(bp);
	    return;
	}

	if (buf1->drive > 0)
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
	    if (!AD_SOFTC(rdp->disks[buf1->drive])->dev->si_disk) {
		rdp->disks[buf1->drive].flags = ~(AR_DF_PRESENT | AR_DF_ONLINE);
		ata_drawerleds(rdp->disks[buf1->drive].device, ATA_LED_RED);
		rdp->flags &= ~AR_F_READY;
		printf("ar%d: ERROR broken array in strategy\n", rdp->lun);
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
		if ((bp->bio_pblkno >= rdp->lock_start &&
		     bp->bio_pblkno < rdp->lock_end) ||
		     ((bp->bio_pblkno + chunk) >= rdp->lock_start &&
		      (bp->bio_pblkno + chunk) < rdp->lock_end)) {
		    tsleep(rdp, PRIBIO, "arwait", 0);
		}
	    }
	    if (rdp->disks[buf1->drive].flags & AR_DF_ONLINE &&
		!AD_SOFTC(rdp->disks[buf1->drive])->dev->si_disk) {
		rdp->disks[buf1->drive].flags = ~(AR_DF_PRESENT | AR_DF_ONLINE);
		ata_drawerleds(rdp->disks[buf1->drive].device, ATA_LED_RED);
		if (rdp->disks[buf1->drive + rdp->width].flags & AR_DF_ONLINE) {
		    rdp->flags |= AR_F_DEGRADED;
		    printf("ar%d: WARNING mirror lost in strategy\n", rdp->lun);
		}
		else 
		    rdp->flags &= ~AR_F_READY;
	    }
	    if (rdp->disks[buf1->drive + rdp->width].flags & AR_DF_ONLINE &&
		!AD_SOFTC(rdp->disks[buf1->drive + rdp->width])->dev->si_disk) {
		rdp->disks[buf1->drive + rdp->width].flags = 
		    ~(AR_DF_PRESENT | AR_DF_ONLINE);
		ata_drawerleds(rdp->disks[buf1->drive + rdp->width].device,
			       ATA_LED_RED);
		if (rdp->disks[buf1->drive].flags & AR_DF_ONLINE) {
		    rdp->flags |= AR_F_DEGRADED;
		    printf("ar%d: WARNING mirror lost in strategy\n", rdp->lun);
		}
		else
		    rdp->flags &= ~AR_F_READY;
	    }
	    if (!(rdp->flags & AR_F_READY)) {
		printf("ar%d: ERROR broken array in strategy\n", rdp->lun);
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
			    buf1->bp.bio_pblkno + chunk;
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
	    printf("DOH!! unknown array type in arstrategy\n");
	}
    }
}

static void
ar_done(struct bio *bp)
{
    struct ar_softc *rdp = (struct ar_softc *)bp->bio_caller1;
    struct ar_buf *buf = (struct ar_buf *)bp;
    int s;

    s = splbio();

    switch (rdp->flags & (AR_F_RAID0 | AR_F_RAID1 | AR_F_SPAN)) {

    case AR_F_SPAN:
    case AR_F_RAID0:
	if (bp->bio_flags & BIO_ERROR) {
	    printf("ar%d: ERROR broken array in done\n", rdp->lun);
	    rdp->disks[buf->drive].flags = ~(AR_DF_PRESENT | AR_DF_ONLINE);
	    rdp->flags &= ~AR_F_READY;
	    buf->org->bio_flags |= BIO_ERROR;
	    buf->org->bio_error = EIO;
	    biodone(buf->org);
	    ata_drawerleds(rdp->disks[buf->drive].device, ATA_LED_RED);
	}
	else {
	    buf->org->bio_resid -= buf->bp.bio_bcount;
	    if (buf->org->bio_resid == 0)
		biodone(buf->org);
	}
	break;

    case AR_F_RAID1:
    case AR_F_RAID0 | AR_F_RAID1:
	if (bp->bio_flags & BIO_ERROR) {
	    rdp->disks[buf->drive].flags = ~(AR_DF_PRESENT | AR_DF_ONLINE);
	    ata_drawerleds(rdp->disks[buf->drive].device, ATA_LED_RED);
	    if (rdp->flags & AR_F_DEGRADED &&
		!(rdp->disks[buf->mirror->drive].flags & AR_DF_ONLINE)) {
		rdp->flags &= ~AR_F_READY;
		printf("ar%d: ERROR broken array in done\n", rdp->lun);
		buf->org->bio_flags |= BIO_ERROR;
		buf->org->bio_error = EIO;
		biodone(buf->org);
	    }
	    else {
		rdp->flags |= AR_F_DEGRADED;
		printf("ar%d: WARNING mirror lost in done\n", rdp->lun);
		if (bp->bio_cmd == BIO_READ) {
		    if (buf->drive < rdp->width)
			buf->drive = buf->drive + rdp->width;
		    else
			buf->drive = buf->drive - rdp->width;
		    buf->bp.bio_dev = AD_SOFTC(rdp->disks[buf->drive])->dev;
		    buf->bp.bio_flags = buf->org->bio_flags;
		    buf->bp.bio_error = 0;
		    buf->bp.bio_dev->AD_STRATEGY((struct bio *)buf);
		    splx(s);
		    return;
		}
		if (bp->bio_cmd == BIO_WRITE) {
		    if (!(buf->flags & AB_F_DONE))
			buf->mirror->flags |= AB_F_DONE;
		    else {
			buf->org->bio_resid -= bp->bio_bcount;
			if (buf->org->bio_resid == 0)
			    biodone(buf->org);
		    }
		}
	    }
	} 
	else {
	    if (bp->bio_cmd == BIO_WRITE) {
		if (!(buf->flags & AB_F_DONE) && !(rdp->flags & AR_F_DEGRADED)){
		    buf->mirror->flags |= AB_F_DONE;
		    break;
		}
	    }
	    buf->org->bio_resid -= bp->bio_bcount;
	    if (buf->org->bio_resid == 0)
		biodone(buf->org);
	}
	break;
	
    default:
	printf("Oops! unknown array type in ar_done\n");
    }
    free(buf, M_AR);
    splx(s);
}

/* read the RAID info from a disk on a HighPoint controller */
static int
ar_highpoint_read_conf(struct ad_softc *adp, struct ar_softc **raidp)
{
    struct highpoint_raid_conf *info;
    struct ar_softc *raid = NULL;
    int array, disk_number = 0, error = 1;

    if (!(info = (struct highpoint_raid_conf *)
	  malloc(sizeof(struct highpoint_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return error;

    if (ar_read(adp, HPT_LBA, sizeof(struct highpoint_raid_conf),
		(u_int8_t *)info)) {
	if (bootverbose)
	    printf("HighPoint read conf failed\n");
	goto highpoint_out;
    }

    /* check if this is a HighPoint RAID struct */
    if (!(info->magic == HPT_MAGIC_OK || info->magic == HPT_MAGIC_BAD)) {
	if (bootverbose)
	    printf("HighPoint check1 failed\n");
	goto highpoint_out;
    }

    /* now convert HighPoint config info into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc*)malloc(sizeof(struct ar_softc), M_AR,
					 M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		printf("ar: failed to allocate raid config storage\n");
		goto highpoint_out;
	    }
	}
	raid = raidp[array];
	if (raid->flags & AR_F_PROMISE_RAID)
	    continue;
	raid->flags = AR_F_HIGHPOINT_RAID;

	switch (info->type) {
	case HPT_T_RAID0:
	    /* check the order byte to determine what this really is */
	    switch (info->order & HPT_O_RAIDMASK) {
	    case HPT_O_RAID0:
		if (raid->magic_0 && raid->magic_0 != info->magic_0)
		    continue;
		raid->magic_0 = info->magic_0;
		raid->flags |= AR_F_RAID0;
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number;
		break;

	    case HPT_O_RAID1:
		if (raid->magic_0 && raid->magic_0 != info->magic_0)
		    continue;
		raid->magic_0 = info->magic_0;
		raid->flags |= AR_F_RAID1;
		disk_number = (info->disk_number > 0);
		break;
	    
	    case HPT_O_RAID01SRC:
		if (raid->magic_0 && raid->magic_0 != info->magic_0)
		    continue;
		raid->magic_0 = info->magic_0;
		raid->flags |= (AR_F_RAID0 | AR_F_RAID1);
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number;
		break;

	    case HPT_O_RAID01DST:
		if (raid->magic_1 && raid->magic_1 != info->magic_0)
		    continue;
		raid->magic_1 = info->magic_0;
		raid->flags |= (AR_F_RAID0 | AR_F_RAID1);
		raid->interleave = 1 << info->stripe_shift;
		disk_number = info->disk_number + info->array_width;
		break;
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
	    printf("HighPoint unknown RAID type 0x%02x\n", info->type);
	    goto highpoint_out;
	}

	raid->disks[disk_number].device = adp->device;
	if (info->magic == HPT_MAGIC_OK) {
	    raid->disks[disk_number].flags |=
		(AR_DF_PRESENT | AR_DF_ONLINE | AR_DF_ASSIGNED);
	    raid->flags |= AR_F_READY;
	    raid->lun = array;
	    raid->width = info->array_width;
	    raid->heads = 255;
	    raid->sectors = 63;
	    raid->cylinders = (info->total_sectors - 9) / (63 * 255);
	    raid->total_sectors = info->total_sectors - (9 * raid->width);
	    raid->offset = 10;
	    raid->reserved = 10;
	}
	else {
	    raid->disks[disk_number].flags &= ~ AR_DF_ONLINE;
	    raid->disks[disk_number].flags |= (AR_DF_PRESENT | AR_DF_ASSIGNED); 
	}
	if (disk_number >= raid->total_disks)
	    raid->total_disks = disk_number + 1;
	error = 0;
	break;
    }
highpoint_out:
    free(info, M_AR);
    return error;
}

/* read the RAID info from a disk on a Promise Fasttrak controller */
static int
ar_promise_read_conf(struct ad_softc *adp, struct ar_softc **raidp)
{
    struct promise_raid_conf *info;
    struct ar_softc *raid;
    u_int32_t magic, cksum, *ckptr;
    int array, count, disk, error = 1; 

    if (!(info = (struct promise_raid_conf *)
	  malloc(sizeof(struct promise_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return error;

    if (ar_read(adp, PR_LBA(adp), sizeof(struct promise_raid_conf), 
		(u_int8_t *)info)) {
	if (bootverbose)
	    printf("Promise read conf failed\n");
	goto promise_out;
    }

    /* check if this is a Promise RAID struct */
    if (strncmp(info->promise_id, PR_MAGIC, sizeof(PR_MAGIC))) {
	if (bootverbose)
	    printf("Promise check1 failed\n");
	goto promise_out;
    }	

    /* check if the checksum is OK */
    for (cksum = 0, ckptr = (int32_t *)info, count = 0; count < 511; count++)
	cksum += *ckptr++;
    if (cksum != *ckptr) {  
	if (bootverbose)
	    printf("Promise check2 failed\n");	     
	goto promise_out;
    }

    /* now convert Promise config info into our generic form */
    if ((info->raid.integrity != PR_I_VALID) || 
	(((info->raid.status & (PR_S_VALID | PR_S_ONLINE)) != 
	  (PR_S_VALID | PR_S_ONLINE)))) {
	if (bootverbose)
	    printf("Promise check3 failed\n");	     
	goto promise_out;
    }

    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc*)malloc(sizeof(struct ar_softc), M_AR,
					 M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		printf("ar: failed to allocate raid config storage\n");
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
	if (info->raid.generation >= raid->generation) {
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
		printf("Promise unknown RAID type 0x%02x\n", info->raid.type);
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
		if (adp->device) {
		    if (info->raid.disk[disk].flags & PR_F_VALID)
			raid->disks[disk].flags |= AR_DF_PRESENT;
		    if (info->raid.disk[disk].flags & PR_F_ONLINE)
			raid->disks[disk].flags |= AR_DF_ONLINE;
		    if (info->raid.disk[disk].flags & PR_F_ASSIGNED)
			raid->disks[disk].flags |= AR_DF_ASSIGNED;
		    if (info->raid.disk[disk].flags & PR_F_SPARE)
			raid->disks[disk].flags |= AR_DF_SPARE;
		    if (info->raid.disk[disk].flags & (PR_F_REDIR | PR_F_DOWN))
			raid->disks[disk].flags &= ~AR_DF_ONLINE;
		    if (info->raid.disk[disk].flags & PR_F_READY)
			raid->disks[disk].flags |= AR_DF_PRESENT;
		}
	    }
	}

	raid->disks[info->raid.disk_number].device = adp->device;
	error = 0;
	break;
    }

promise_out:
    free(info, M_AR);
    return error;
}

static int
ar_read(struct ad_softc *adp, u_int32_t lba, int count, u_int8_t *data)
{
    if (ata_command(adp->device, count > DEV_BSIZE ? ATA_C_READ_MUL:ATA_C_READ,
		    lba, count / DEV_BSIZE, 0, ATA_WAIT_INTR)) {
	ata_printf(adp->device->channel, adp->device->unit,
		   "RAID read config failed\n");
	return 1;
    }
    if (ata_wait(adp->device, ATA_S_READY|ATA_S_DSC|ATA_S_DRQ)){
	ata_printf(adp->device->channel, adp->device->unit,
		   "RAID read config timeout\n");
	return 1;
    }
    ATA_INSW(adp->device->channel->r_io, ATA_DATA, (int16_t *)data,
	     count/sizeof(int16_t));
    ATA_INB(adp->device->channel->r_io, ATA_STATUS);
    return 0;
}
