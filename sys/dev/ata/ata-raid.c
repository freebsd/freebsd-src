/*-
 * Copyright (c) 2000 Søren Schmidt
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/cons.h>
#include <machine/bus.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>

/* device structures */
static d_open_t         aropen;
static d_strategy_t     arstrategy;
static struct cdevsw ar_cdevsw = {
	/* open */      aropen,
	/* close */     nullclose,
	/* read */      physread,
	/* write */     physwrite,
	/* ioctl */     noioctl, 
	/* poll */      nopoll,
	/* mmap */      nommap,
	/* strategy */  arstrategy,
	/* name */      "ar",
	/* maj */       157,
	/* dump */      nodump,
	/* psize */     nopsize,
	/* flags */     D_DISK,
	/* bmaj */      -1
};  
static struct cdevsw ardisk_cdevsw;

/* prototypes */
static void ar_attach(struct ar_softc *);
static void ar_done(struct buf *);
static int ar_highpoint_conf(struct ad_softc *, struct ar_config *);
static int32_t ar_promise_magic(struct promise_raid_conf *);
static int ar_promise_conf(struct ad_softc *, struct ar_config *);
static int ar_read(struct ad_softc *, u_int32_t, int, char *);

/* internal vars */
static int ar_init = 0;
static struct ar_config ar_table;
MALLOC_DEFINE(M_AR, "AR driver", "ATA RAID driver");
  
/* defines */
#define PRINT_AD(adp) \
        printf("  ad%d: %luMB <%.40s> [%d/%d/%d] at ata%d-%s %s%s\n", \
               adp->lun, adp->total_secs / ((1024L * 1024L) / DEV_BSIZE), \
	       adp->controller->dev_param[ATA_DEV(adp->unit)]->model, \
	       adp->total_secs / (adp->heads * adp->sectors), \
               adp->heads, adp->sectors, device_get_unit(adp->controller->dev),\
               (adp->unit == ATA_MASTER) ? "master" : "slave", \
               (adp->flags & AD_F_TAG_ENABLED) ? "tagged " : "", \
               ata_mode2str(adp->controller->mode[ATA_DEV(adp->unit)]))

int
ar_probe(struct ad_softc *adp)
{
    if (!ar_init) {
	bzero(&ar_table, sizeof(ar_table));
	ar_init = 1;
    }

    switch(adp->controller->chiptype) {
    case 0x4d33105a:
    case 0x4d38105a:
    case 0x4d30105a:
    case 0x0d30105a:
	if (ar_promise_conf(adp, &ar_table))
	    break;
	return 0;

    case 0x00041103:
	if (ar_highpoint_conf(adp, &ar_table))
	    break;
	return 0;
    }
    return 1;
}

static void
ar_attach(struct ar_softc *raid)
{
    dev_t dev;
    int i;

    printf("ar%d: %luMB <ATA ",
	   raid->lun, raid->total_secs / ((1024L*1024L)/DEV_BSIZE));
    switch (raid->flags & (AR_F_RAID_0 | AR_F_RAID_1 | AR_F_SPAN)) {
    case AR_F_RAID_0:
        printf("RAID0 "); break;
    case AR_F_RAID_1:
        printf("RAID1 "); break;
    case AR_F_SPAN:
        printf("SPAN "); break;
    case (AR_F_RAID_0 | AR_F_RAID_1):
        printf("RAID0+1 "); break;
    default:
    	printf("unknown array 0x%x ", raid->flags);
	return;
    }
    printf("array> [%d/%d/%d] subdisks: \n",
    	   raid->cylinders, raid->heads, raid->sectors);
    for (i = 0; i < raid->num_subdisks; i++)
	PRINT_AD(raid->subdisk[i]);
    for (i = 0; i < raid->num_mirrordisks; i++)
	PRINT_AD(raid->mirrordisk[i]);

    dev = disk_create(raid->lun, &raid->disk, 0, &ar_cdevsw, &ardisk_cdevsw);
    dev->si_drv1 = raid;
    dev->si_iosize_max = 256 * DEV_BSIZE;
    raid->dev = dev;
}

static int
aropen(dev_t dev, int flags, int fmt, struct proc *p)
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
    dl->d_secperunit = rdp->total_secs;
    return 0;
}

static void
arstrategy(struct buf *bp)
{
    struct ar_softc *rdp = bp->b_dev->si_drv1;
    int lba, count, chunk;
    caddr_t data;

    /* if it's a null transfer, return immediatly. */
    if (bp->b_bcount == 0) {
	bp->b_resid = 0;
	biodone(bp);
	return;
    }

    bp->b_resid = bp->b_bcount;
    lba = bp->b_pblkno;
    data = bp->b_data;
    for (count = howmany(bp->b_bcount, DEV_BSIZE); count > 0; 
	 count -= chunk, lba += chunk, data += (chunk * DEV_BSIZE)) {
	struct ar_buf *buf1, *buf2;
	int plba;

	buf1 = malloc(sizeof(struct ar_buf), M_AR, M_NOWAIT);
	bzero(buf1, sizeof(struct ar_buf));
	if (rdp->flags & AR_F_SPAN) {
	    plba = lba;
	    while (plba >= (rdp->subdisk[buf1->drive]->total_secs-rdp->reserved)
		   && buf1->drive < rdp->num_subdisks)
		plba-=(rdp->subdisk[buf1->drive++]->total_secs-rdp->reserved);
	    buf1->bp.b_pblkno = plba;
	    chunk = min(rdp->subdisk[buf1->drive]->total_secs - 
		        rdp->reserved - plba, count);
	}
	else if (rdp->flags & AR_F_RAID_0) {
	    plba = lba / rdp->interleave;
	    chunk = lba % rdp->interleave;
	    buf1->drive = plba % rdp->num_subdisks;
	    buf1->bp.b_pblkno = 
		((plba / rdp->num_subdisks) * rdp->interleave) + chunk;
	    chunk = min(rdp->interleave - chunk, count);
	}
	else {
	    buf1->bp.b_pblkno = lba;
	    buf1->drive = 0;
	    chunk = count;
	}

	buf1->bp.b_pblkno += rdp->offset;
	buf1->bp.b_caller1 = (void *)rdp;
	buf1->bp.b_bcount = chunk * DEV_BSIZE;
	buf1->bp.b_data = data;
	buf1->bp.b_flags = bp->b_flags | B_CALL;
	buf1->bp.b_iodone = ar_done;
	buf1->org = bp;

	/* simpleminded load balancing on RAID1 arrays */
	if (rdp->flags & AR_F_RAID_1 && bp->b_flags & B_READ) {
	    if (buf1->bp.b_pblkno < 
		(rdp->last_lba[buf1->drive][rdp->last_disk] - 100) ||
		buf1->bp.b_pblkno > 
		(rdp->last_lba[buf1->drive][rdp->last_disk] + 100)) {
		rdp->last_disk = 1 - rdp->last_disk;
		rdp->last_lba[buf1->drive][rdp->last_disk] = 
		    buf1->bp.b_pblkno;
	    }
	    if (rdp->last_disk)
	    	buf1->bp.b_dev = rdp->mirrordisk[buf1->drive]->dev1;
	    else
	    	buf1->bp.b_dev = rdp->subdisk[buf1->drive]->dev1;
	}
	else
	    buf1->bp.b_dev = rdp->subdisk[buf1->drive]->dev1;

	if (rdp->flags & AR_F_RAID_1 && !(bp->b_flags & B_READ)) {
	    buf2 = malloc(sizeof(struct ar_buf), M_AR, M_NOWAIT);
	    bcopy(buf1, buf2, sizeof(struct ar_buf));
	    buf2->bp.b_dev = rdp->mirrordisk[buf1->drive]->dev1;
	    buf2->mirror = buf1;
	    buf1->mirror = buf2;
	    buf2->bp.b_dev->si_disk->d_devsw->d_strategy((struct buf *)buf2);
	}
	buf1->bp.b_dev->si_disk->d_devsw->d_strategy((struct buf *)buf1);
    }
}

static void
ar_done(struct buf *bp)
{
    struct ar_softc *rdp = (struct ar_softc *)bp->b_caller1;
    struct ar_buf *buf = (struct ar_buf *)bp;
    int s;

    s = splbio();

    if (bp->b_flags & B_ERROR) {
	if (!(bp->b_flags & B_READ) || buf->done || !(rdp->flags&AR_F_RAID_1)) {
	    buf->org->b_flags |= B_ERROR;
	    buf->org->b_error = bp->b_error;
	}
	printf("ar%d: subdisk error\n", rdp->lun);
    }

    if (rdp->flags & AR_F_RAID_1) {
	if (!(bp->b_flags & B_READ)) {
	    if (!buf->done) {
		buf->mirror->done = 1;
		goto done;
	    }
	}
	else {
	    if (!buf->done && bp->b_flags & B_ERROR) {
		/* read error on this disk, try mirror */
		buf->done = 1;
	    	buf->bp.b_dev = rdp->mirrordisk[buf->drive]->dev1;
	    	buf->bp.b_dev->si_disk->d_devsw->d_strategy((struct buf *)buf);
		return;
	    }
	}
    }
    buf->org->b_resid -= bp->b_bcount;
    if (buf->org->b_resid == 0)
	biodone(buf->org);
done:
    free(buf, M_AR);
    splx(s);
}

/* read the RAID info from a disk on a HighPoint controller */
static int
ar_highpoint_conf(struct ad_softc *adp, struct ar_config *raidp)
{
    struct highpoint_raid_conf info;
    struct ar_softc *raid;
    int array_done = 0, r;

    if (ar_read(adp, 0x09, DEV_BSIZE, (char *)&info)) {
	if (bootverbose)
	    printf("HighPoint read conf failed\n");
	return 1;
    }

    /* check if this is a HighPoint RAID struct */
    if (info.magic != HPT_MAGIC_OK) {
	if (bootverbose)
	    printf("HighPoint check1 failed\n");
	return 1;
    }   

    /* now convert HighPoint config info into our generic form */
    for (r = 0; r < 8; r++) {
	if (!raidp->raid[r]) {
	    raidp->raid[r] = 
	        (struct ar_softc*)malloc(sizeof(struct ar_softc),M_AR,M_NOWAIT);
	    if (!raidp->raid[r]) {
		printf("ar: failed to allocate raid config storage\n");
		return 1;
	    }
	    bzero(raidp->raid[r], sizeof(struct ar_softc));
	}
	raid = raidp->raid[r];

	switch (info.type) {
	case HPT_T_RAID_0:
	    /* check the order byte to determine what this really is */
	    switch (info.order & (HPT_O_MIRROR | HPT_O_STRIPE)) {
	    case HPT_O_MIRROR:
	    	goto hpt_mirror;
	    
	    case HPT_O_STRIPE:
		if (raid->magic_0 && raid->magic_0 != info.magic_0)
		    continue;
		raid->magic_0 = info.magic_0;
		raid->flags |= (AR_F_RAID_0 | AR_F_RAID_1);
		raid->interleave = 1 << info.raid0_shift;
		raid->subdisk[info.disk_number] = adp;
		raid->num_subdisks++;
    		if ((raid->num_subdisks + raid->num_mirrordisks) == 
		    (info.raid_disks * 2))
		    array_done = 1;
		break;

	    case (HPT_O_MIRROR | HPT_O_STRIPE):
		if (raid->magic_1 && raid->magic_1 != info.magic_1)
		    continue;
		raid->magic_1 = info.magic_1;
		raid->flags |= (AR_F_RAID_0 | AR_F_RAID_1);
		raid->mirrordisk[info.disk_number] = adp;
		raid->num_mirrordisks++;
    		if ((raid->num_subdisks + raid->num_mirrordisks) ==
		    (info.raid_disks * 2))
		    array_done = 1;
		break;

	    default:
		if (raid->magic_0 && raid->magic_0 != info.magic_0)
		    continue;
		raid->magic_0 = info.magic_0;
		raid->magic_1 = 0xffffffff;
		raid->flags |= AR_F_RAID_0;
		raid->interleave = 1 << info.raid0_shift;
		raid->subdisk[info.disk_number] = adp;
		raid->num_subdisks++;
    		if (raid->num_subdisks == info.raid_disks)
		    array_done = 1;
	    }
	    break;

	case HPT_T_RAID_1:
hpt_mirror:
	    if (raid->magic_1 && raid->magic_1 != info.magic_1)
		continue;
	    raid->magic_1 = info.magic_1;
	    raid->magic_0 = 0xffffffff;
	    raid->flags |= AR_F_RAID_1;
	    if (info.disk_number == 0 && raid->num_subdisks == 0) {
		raid->subdisk[raid->num_subdisks] = adp;
		raid->num_subdisks = 1;
	    }
	    if (info.disk_number == 1 && raid->num_mirrordisks == 0) {
		raid->mirrordisk[raid->num_mirrordisks] = adp;
		raid->num_mirrordisks = 1;
	    }
    	    if ((raid->num_subdisks + 
		 raid->num_mirrordisks) == (info.raid_disks * 2))
		array_done = 1;
	    break;

	case HPT_T_SPAN:
	    if (raid->magic_0 && raid->magic_0 != info.magic_0)
		continue;
	    raid->magic_0 = info.magic_0;
	    raid->flags |= AR_F_SPAN;
	    raid->subdisk[raid->num_subdisks] = adp;
	    raid->num_subdisks++;
    	    if (raid->num_subdisks == info.raid_disks)
		array_done = 1;
	    break;

	default:
	    printf("HighPoint unknown RAID type 0x%02x\n", info.type);
	}
	if (array_done) {
		raid->lun = r;
		raid->heads = 255;
		raid->sectors = 63;
		raid->cylinders = (info.total_secs - 9) / (63 * 255);
		raid->total_secs = info.total_secs - (9 * raid->num_subdisks);
		raid->offset = 10;
		raid->reserved = 10;
		ar_attach(raid);
	    return 0;
	}
	break;
    }
    return 0;
}

static int32_t
ar_promise_magic(struct promise_raid_conf *info)
{
    int i, j;
    int32_t magic = 0;

    for (i = 0; i < 4; i++) {
	if ((info->raid[i].flags != PR_F_CONFED) || 
	    (((info->raid[i].status & (PR_S_DEFINED|PR_S_ONLINE)) != 
	      (PR_S_DEFINED|PR_S_ONLINE))))
            continue;
	for (j = 0; j < info->raid[i].total_disks; j++) {
	    magic <<= 8;
	    magic |= ((info->raid[i].disk[j].magic_0 & 0x00ff0000)>>16);
	}
    }
    return magic;
}

/* read the RAID info from a disk on a Promise Fasttrak controller */
static int
ar_promise_conf(struct ad_softc *adp, struct ar_config *raidp)
{
    struct promise_raid_conf info;
    struct ar_softc *raid;
    u_int32_t lba;
    u_int32_t cksum, *ckptr;
    int count, i, j, r; 

    lba = adp->total_secs - adp->sectors;

    if (ar_read(adp, lba, 4 * DEV_BSIZE, (char *)&info)) {
	if (bootverbose)
	    printf("Promise read conf failed\n");
	return 1;
    }

    /* check if this is a Promise RAID struct */
    if (strncmp(info.promise_id, PR_MAGIC, sizeof(PR_MAGIC))) {
	if (bootverbose)
	    printf("Promise check1 failed\n");
	return 1;
    }   

    /* check if the checksum is OK */
    for (cksum = 0, ckptr = (int32_t *)&info, count = 0; count < 511; count++)
	cksum += *ckptr++;
    if (cksum != *ckptr) {  
	if (bootverbose)
	    printf("Promise check2 failed\n");       
	return 1;
    }

    /* now convert Promise config info into our generic form */
    for (i = 0, r = 0; i < 4; i++) {
	if ((info.raid[i].flags != PR_F_CONFED) || 
	    (((info.raid[i].status & (PR_S_DEFINED|PR_S_ONLINE)) != 
	      (PR_S_DEFINED|PR_S_ONLINE)))) {
            continue;
	}

	if (raidp->raid[r]) {
	    if (ar_promise_magic(&info) != raidp->raid[r]->magic_0) {
		r++;
		i--;
		continue;
	    }
	}
	else {
	    if (!(raidp->raid[r] = (struct ar_softc *)
		  malloc(sizeof(struct ar_softc), M_AR, M_NOWAIT))) {
		printf("ar: failed to allocate raid config storage\n");
		return 1;
	    }
	    else
		bzero(raidp->raid[r], sizeof(struct ar_softc));
	}
	raid = raidp->raid[r];
	raid->magic_0 = ar_promise_magic(&info);

	switch (info.raid[i].type) {
	case PR_T_STRIPE:
	    raid->flags |= AR_F_RAID_0;
	    raid->interleave = 1 << info.raid[i].raid0_shift;
	    break;

	case PR_T_MIRROR:
	    raid->flags |= AR_F_RAID_1;
	    break;

	case PR_T_SPAN:
	    raid->flags |= AR_F_SPAN;
	    break;

	default:
	    printf("Promise unknown RAID type 0x%02x\n", info.raid[i].type);
	}

	/* find out where this disk is in the defined array */
	/* first RAID0 / SPAN disks */
	for (j = 0; j < info.raid[i].raid0_disks; j++) {
	    if (info.channel == info.raid[i].disk[j].channel &&
		info.device == info.raid[i].disk[j].device) {
		raid->subdisk[raid->num_subdisks] = adp;
		raid->num_subdisks++;
		if (raid->num_subdisks > 1 && !(raid->flags & AR_F_SPAN)) {
		    raid->flags |= AR_F_RAID_0;
		    raid->interleave = 1 << info.raid[i].raid0_shift;
		}
	    }
	}

	/* if any left they are RAID1 disks eventually in a RADI0+1 config */
	for (; j < info.raid[i].total_disks; j++) {
	    if (info.channel == info.raid[i].disk[j].channel &&
		info.device == info.raid[i].disk[j].device) {
		raid-> mirrordisk[raid->num_mirrordisks] = adp;
		raid->num_mirrordisks++;
	    }
	}

	/* do we have a complete array to attach to ? */
	if (raid->num_subdisks + raid->num_mirrordisks ==
	    info.raid[i].total_disks) {
	    raid->lun = r;
	    raid->heads = info.raid[i].heads + 1;
	    raid->sectors = info.raid[i].sectors;
	    raid->cylinders = info.raid[i].cylinders + 1;
	    raid->total_secs = info.raid[i].total_secs;
	    raid->offset = 0;
	    raid->reserved = 63;
	    ar_attach(raid);
	}
	r++;
    }
    return 0;
}

int
ar_read(struct ad_softc *adp, u_int32_t lba, int count, char *data)
{
    if (ata_command(adp->controller, adp->unit | ATA_D_LBA, 
	(count > DEV_BSIZE) ? ATA_C_READ_MUL : ATA_C_READ,
	(lba >> 8) & 0xffff, (lba >> 24) & 0xff, lba & 0xff,
	count / DEV_BSIZE, 0, ATA_WAIT_INTR)) {
	ata_printf(adp->controller, adp->unit, "RAID read config failed\n");
	return 1;
    }
    if (ata_wait(adp->controller, adp->unit, ATA_S_READY|ATA_S_DSC|ATA_S_DRQ)) {
	ata_printf(adp->controller, adp->unit, "RAID read config timeout\n");
	return 1;
    }
    insw(adp->controller->ioaddr + ATA_DATA, data, count/sizeof(int16_t));
    inb(adp->controller->ioaddr + ATA_STATUS);
    return 0;
}
