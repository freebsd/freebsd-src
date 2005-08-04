/*-
 * Copyright (c) 2000 - 2005 Søren Schmidt <sos@FreeBSD.org>
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
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/cons.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <geom/geom_disk.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>
#include <dev/ata/ata-raid.h>
#include <dev/ata/ata-pci.h>
#include <ata_if.h>

/* prototypes */
static void ata_raid_done(struct ata_request *request);
static void ata_raid_config_changed(struct ar_softc *rdp, int writeback);
static int ata_raid_status(struct ata_ioc_raid_config *config);
static int ata_raid_create(struct ata_ioc_raid_config *config);
static int ata_raid_delete(int array);
static int ata_raid_addspare(struct ata_ioc_raid_config *config);
static int ata_raid_rebuild(int array);
static int ata_raid_read_metadata(device_t subdisk);
static int ata_raid_write_metadata(struct ar_softc *rdp);
static int ata_raid_adaptec_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_hptv2_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_hptv2_write_meta(struct ar_softc *rdp);
static int ata_raid_hptv3_read_meta(device_t dev, struct ar_softc **raidp);
//static int ata_raid_hptv3_write_meta(struct ar_softc *rdp);
static int ata_raid_intel_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_ite_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_lsiv2_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_lsiv3_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_nvidia_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_promise_read_meta(device_t dev, struct ar_softc **raidp, int native);
static int ata_raid_promise_write_meta(struct ar_softc *rdp);
static int ata_raid_sii_read_meta(device_t dev, struct ar_softc **raidp);
static int ata_raid_via_read_meta(device_t dev, struct ar_softc **raidp);
static struct ata_request *ata_raid_init_request(struct ar_softc *rdp, struct bio *bio);
static int ata_raid_send_request(struct ata_request *request);
static int ata_raid_rw(device_t dev, u_int64_t lba, void *data, u_int bcount, int flags);
static char * ata_raid_format(struct ar_softc *rdp);
static char * ata_raid_type(struct ar_softc *rdp);
static char * ata_raid_flags(struct ar_softc *rdp);

/* debugging only */
static void ata_raid_print_meta(struct ar_softc *meta);
static void ata_raid_adaptec_print_meta(struct adaptec_raid_conf *meta);
static void ata_raid_hptv2_print_meta(struct hptv2_raid_conf *meta);
static void ata_raid_hptv3_print_meta(struct hptv3_raid_conf *meta);
static void ata_raid_intel_print_meta(struct intel_raid_conf *meta);
static void ata_raid_ite_print_meta(struct ite_raid_conf *meta);
static void ata_raid_lsiv2_print_meta(struct lsiv2_raid_conf *meta);
static void ata_raid_lsiv3_print_meta(struct lsiv3_raid_conf *meta);
static void ata_raid_nvidia_print_meta(struct nvidia_raid_conf *meta);
static void ata_raid_promise_print_meta(struct promise_raid_conf *meta);
static void ata_raid_sii_print_meta(struct sii_raid_conf *meta);
static void ata_raid_via_print_meta(struct via_raid_conf *meta);

/* internal vars */   
static struct ar_softc *ata_raid_arrays[MAX_ARRAYS];
static MALLOC_DEFINE(M_AR, "AR driver", "ATA PseudoRAID driver");
static devclass_t ata_raid_sub_devclass;
static int testing = 0;

/* device structures */
static disk_strategy_t ata_raid_strategy;
//static dumper_t ata_raid_dump;

static void
ata_raid_attach(struct ar_softc *rdp, int writeback)
{
    char buffer[32];
    int disk;

    buffer[0] = '\0';
    mtx_init(&rdp->lock, "ATA PseudoRAID metadata lock", NULL, MTX_DEF);
    ata_raid_config_changed(rdp, writeback);

    /* sanitize arrays total_size % (width * interleave) == 0 */
    if (rdp->type == AR_T_RAID0 || rdp->type == AR_T_RAID01 ||
	rdp->type == AR_T_RAID5) {
	rdp->total_sectors = (rdp->total_sectors/(rdp->interleave*rdp->width))*
			     (rdp->interleave * rdp->width);
	sprintf(buffer, " (stripe %d KB)",
		(rdp->interleave * DEV_BSIZE) / 1024);
    }
    rdp->disk = disk_alloc();
    rdp->disk->d_strategy = ata_raid_strategy;
    //rdp->disk->d_dump = ata_raid_dump;
    rdp->disk->d_name = "ar";
    rdp->disk->d_sectorsize = DEV_BSIZE;
    rdp->disk->d_mediasize = (off_t)rdp->total_sectors * DEV_BSIZE;
    rdp->disk->d_fwsectors = rdp->sectors;
    rdp->disk->d_fwheads = rdp->heads;
    rdp->disk->d_maxsize = 128 * DEV_BSIZE;
    rdp->disk->d_drv1 = rdp;
    rdp->disk->d_unit = rdp->lun;
    disk_create(rdp->disk, DISK_VERSION);

    printf("ar%d: %lluMB <%s %s%s> status: %s\n", rdp->lun,
	   (unsigned long long)(rdp->total_sectors / ((1024L*1024L)/DEV_BSIZE)),
	   ata_raid_format(rdp), ata_raid_type(rdp),
	   buffer, ata_raid_flags(rdp));

    if (testing || bootverbose)
	printf("ar%d: %llu sectors [%dC/%dH/%dS] <%s> subdisks defined as:\n",
	       rdp->lun, (unsigned long long)rdp->total_sectors,
	       rdp->cylinders, rdp->heads, rdp->sectors, rdp->name);

    for (disk = 0; disk < rdp->total_disks; disk++) {
	printf("ar%d: disk%d ", rdp->lun, disk);
	if (rdp->disks[disk].dev) {
	    if (rdp->disks[disk].flags & AR_DF_PRESENT) {
		/* status of this disk in the array */
		if (rdp->disks[disk].flags & AR_DF_ONLINE)
		    printf("READY ");
		else if (rdp->disks[disk].flags & AR_DF_SPARE)
		    printf("SPARE ");
		else
		    printf("FREE  ");

		/* what type of disk is this in the array */
		switch (rdp->type) {
		case AR_T_RAID1:
		case AR_T_RAID01:
		    if (disk < rdp->width)
			printf("(master) ");
		    else
			printf("(mirror) ");
		}
		
		/* which physical disk is used */
		printf("using %s at ata%d-%s\n",
		       device_get_nameunit(rdp->disks[disk].dev),
		       device_get_unit(device_get_parent(rdp->disks[disk].dev)),
		       (((struct ata_device *)
			 device_get_softc(rdp->disks[disk].dev))->unit == 
			 ATA_MASTER) ? "master" : "slave");
	    }
	    else if (rdp->disks[disk].flags & AR_DF_ASSIGNED)
		printf("DOWN\n");
	    else
		printf("INVALID no RAID config on this subdisk\n");
	}
	else
	    printf("DOWN no device found for this subdisk\n");
    }
}

static int
ata_raid_ioctl(u_long cmd, caddr_t data)
{
    struct ata_ioc_raid_config *config = (struct ata_ioc_raid_config *)data;
    int *lun = (int *)data;
    int error = EOPNOTSUPP;

    switch (cmd) {
    case IOCATARAIDSTATUS:
	error = ata_raid_status(config);
	break;
			
    case IOCATARAIDCREATE:
	error = ata_raid_create(config);
	break;
	 
    case IOCATARAIDDELETE:
	error = ata_raid_delete(*lun);
	break;
     
    case IOCATARAIDADDSPARE:
	error = ata_raid_addspare(config);
	break;
			    
    case IOCATARAIDREBUILD:
	error = ata_raid_rebuild(*lun);
	break;
    }
    return error;
}

static void
ata_raid_strategy(struct bio *bp)
{
    struct ar_softc *rdp = bp->bio_disk->d_drv1;
    struct ata_request *request;
    caddr_t data;
    u_int64_t blkno, lba, blk = 0;
    int count, chunk, drv, par = 0, change = 0;

    if (!(rdp->status & AR_S_READY) ||
	(bp->bio_cmd != BIO_READ && bp->bio_cmd != BIO_WRITE)) {
	biofinish(bp, NULL, EIO);
	return;
    }

    bp->bio_resid = bp->bio_bcount;
    for (count = howmany(bp->bio_bcount, DEV_BSIZE),
	 blkno = bp->bio_pblkno, data = bp->bio_data;
	 count > 0; 
	 count -= chunk, blkno += chunk, data += (chunk * DEV_BSIZE)) {

	switch (rdp->type) {
	case AR_T_JBOD:
	case AR_T_RAID1:
	    drv = 0;
	    lba = blkno;
	    chunk = count;
	    break;
	
	case AR_T_SPAN:
	    drv = 0;
	    lba = blkno;
	    while (lba >= rdp->disks[drv].sectors)
		lba -= rdp->disks[drv++].sectors;
	    chunk = min(rdp->disks[drv].sectors - lba, count);
	    break;
	
	case AR_T_RAID0:
	case AR_T_RAID01:
	    chunk = blkno % rdp->interleave;
	    drv = (blkno / rdp->interleave) % rdp->width;
	    lba = (((blkno/rdp->interleave)/rdp->width)*rdp->interleave)+chunk;
	    chunk = min(count, rdp->interleave - chunk);
	    break;

	case AR_T_RAID5:
	    drv = (blkno / rdp->interleave) % (rdp->width - 1);
	    par = rdp->width - 1 - 
		  (blkno / (rdp->interleave * (rdp->width - 1))) % rdp->width;
	    if (drv >= par)
		drv++;
	    lba = ((blkno/rdp->interleave)/(rdp->width-1))*(rdp->interleave) +
		  ((blkno%(rdp->interleave*(rdp->width-1)))%rdp->interleave);
	    chunk = min(count, rdp->interleave - (lba % rdp->interleave));
	    break;

	default:
	    printf("ar%d: unknown array type in ata_raid_strategy\n", rdp->lun);
	    biofinish(bp, NULL, EIO);
	    return;
	}
	 
	/* offset on all but "first on HPTv2" */
	if (!(drv == 0 && rdp->format == AR_F_HPTV2_RAID))
	    lba += rdp->offset_sectors;

	if (!(request = ata_raid_init_request(rdp, bp))) {
	    biofinish(bp, NULL, EIO);
	    return;
	}
	request->data = data;
	request->bytecount = chunk * DEV_BSIZE;
	request->u.ata.lba = lba;
	request->u.ata.count = request->bytecount / DEV_BSIZE;
	    
	switch (rdp->type) {
	case AR_T_JBOD:
	case AR_T_SPAN:
	case AR_T_RAID0:
	    if (((rdp->disks[drv].flags & (AR_DF_PRESENT|AR_DF_ONLINE)) ==
		 (AR_DF_PRESENT|AR_DF_ONLINE) && !rdp->disks[drv].dev)) {
		rdp->disks[drv].flags &= ~AR_DF_ONLINE;
		ata_raid_config_changed(rdp, 1);
		ata_free_request(request);
		biofinish(bp, NULL, EIO);
		return;
	    }
	    request->this = drv;
	    request->dev = rdp->disks[request->this].dev;
	    ata_raid_send_request(request);
	    break;

	case AR_T_RAID1:
	case AR_T_RAID01:
	    if ((rdp->disks[drv].flags &
		 (AR_DF_PRESENT|AR_DF_ONLINE))==(AR_DF_PRESENT|AR_DF_ONLINE) &&
		!rdp->disks[drv].dev) {
		rdp->disks[drv].flags &= ~AR_DF_ONLINE;
		change = 1;
	    }
	    if ((rdp->disks[drv + rdp->width].flags &
		 (AR_DF_PRESENT|AR_DF_ONLINE))==(AR_DF_PRESENT|AR_DF_ONLINE) &&
		!rdp->disks[drv + rdp->width].dev) {
		rdp->disks[drv + rdp->width].flags &= ~AR_DF_ONLINE;
		change = 1;
	    }
	    if (change)
		ata_raid_config_changed(rdp, 1);
	    if (!(rdp->status & AR_S_READY)) {
		ata_free_request(request);
		biofinish(bp, NULL, EIO);
		return;
	    }

	    if (rdp->status & AR_S_REBUILDING)
		blk = ((lba / rdp->interleave) * rdp->width) * rdp->interleave +
		      (rdp->interleave * (drv % rdp->width)) +
		      lba % rdp->interleave;;

	    if (bp->bio_cmd == BIO_READ) {
		int src_online =
		    (rdp->disks[drv].flags & AR_DF_ONLINE);
		int mir_online =
		    (rdp->disks[drv+rdp->width].flags & AR_DF_ONLINE);

		/* if mirror gone or close to last access on source */
		if (!mir_online || 
		    ((src_online) &&
		     bp->bio_pblkno >=
			(rdp->disks[drv].last_lba - AR_PROXIMITY) &&
		     bp->bio_pblkno <=
			(rdp->disks[drv].last_lba + AR_PROXIMITY))) {
		    rdp->toggle = 0;
		} 
		/* if source gone or close to last access on mirror */
		else if (!src_online ||
			 ((mir_online) &&
			  bp->bio_pblkno >=
			  (rdp->disks[drv+rdp->width].last_lba-AR_PROXIMITY) &&
			  bp->bio_pblkno <=
			  (rdp->disks[drv+rdp->width].last_lba+AR_PROXIMITY))) {
		    drv += rdp->width;
		    rdp->toggle = 1;
		}
		/* not close to any previous access, toggle */
		else {
		    if (rdp->toggle)
			rdp->toggle = 0;
		    else {
			drv += rdp->width;
			rdp->toggle = 1;
		    }
		}

		if ((rdp->status & AR_S_REBUILDING) &&
		    (blk <= rdp->rebuild_lba) &&
		    ((blk + chunk) > rdp->rebuild_lba)) {
		    struct ata_composite *composite;
		    struct ata_request *rebuild;
		    int this;

		    /* figure out what part to rebuild */
		    if (drv < rdp->width)
			this = drv + rdp->width;
		    else
			this = drv - rdp->width;

		    /* do we have a spare to rebuild on ? */
		    if (rdp->disks[this].flags & AR_DF_SPARE) {
			if ((composite = ata_alloc_composite())) {
			    if ((rebuild = ata_alloc_request())) {
				rdp->rebuild_lba = blk + chunk;
				bcopy(request, rebuild,
				      sizeof(struct ata_request));
				rebuild->this = this;
				rebuild->dev = rdp->disks[this].dev;
				rebuild->flags &= ~ATA_R_READ;
				rebuild->flags |= ATA_R_WRITE;
				mtx_init(&composite->lock,
					 "ATA PseudoRAID rebuild lock",
					 NULL, MTX_DEF);
				composite->rd_needed |= (1 << drv);
				composite->wr_depend |= (1 << drv);
				composite->wr_needed |= (1 << this);
				composite->request[drv] = request;
				composite->request[this] = rebuild;
				request->composite = composite;
				rebuild->composite = composite;
				ata_raid_send_request(rebuild);
			    }
			    else {
				ata_free_composite(composite);
				printf("DOH! ata_alloc_request failed!\n");
			    }
			}
			else {
			    printf("DOH! ata_alloc_composite failed!\n");
			}
		    }
		    else if (rdp->disks[this].flags & AR_DF_ONLINE) {
			/*
			 * if we got here we are a chunk of a RAID01 that 
			 * does not need a rebuild, but we need to increment
			 * the rebuild_lba address to get the rebuild to
			 * move to the next chunk correctly
			 */
			rdp->rebuild_lba = blk + chunk;
		    }
		    else
			printf("DOH! we didn't find the rebuild part\n");
		}
	    }
	    if (bp->bio_cmd == BIO_WRITE) {
		if ((rdp->disks[drv+rdp->width].flags & AR_DF_ONLINE) ||
		    ((rdp->status & AR_S_REBUILDING) &&
		     (rdp->disks[drv+rdp->width].flags & AR_DF_SPARE) &&
		     ((blk < rdp->rebuild_lba) ||
		      ((blk <= rdp->rebuild_lba) &&
		       ((blk + chunk) > rdp->rebuild_lba))))) {
		    if ((rdp->disks[drv].flags & AR_DF_ONLINE) ||
			((rdp->status & AR_S_REBUILDING) &&
			 (rdp->disks[drv].flags & AR_DF_SPARE) &&
			 ((blk < rdp->rebuild_lba) ||
			  ((blk <= rdp->rebuild_lba) &&
			   ((blk + chunk) > rdp->rebuild_lba))))) {
			struct ata_request *mirror;
			struct ata_composite *composite;
			int this = drv + rdp->width;

			if ((composite = ata_alloc_composite())) {
			    if ((mirror = ata_alloc_request())) {
				rdp->rebuild_lba = blk + chunk;
				bcopy(request, mirror,
				      sizeof(struct ata_request));
				mirror->this = this;
				mirror->dev = rdp->disks[this].dev;
				mtx_init(&composite->lock,
					 "ATA PseudoRAID mirror lock",
					 NULL, MTX_DEF);
				composite->wr_needed |= (1 << drv);
				composite->wr_needed |= (1 << this);
				composite->request[drv] = request;
				composite->request[this] = mirror;
				request->composite = composite;
				mirror->composite = composite;
				ata_raid_send_request(mirror);
				rdp->disks[this].last_lba =
				    bp->bio_pblkno + chunk;
			    }
			    else {
				ata_free_composite(composite);
				printf("DOH! ata_alloc_request failed!\n");
			    }
			}
			else {
			    printf("DOH! ata_alloc_composite failed!\n");
			}
		    }
		    else
			drv += rdp->width;
		}
	    }
	    request->this = drv;
	    request->dev = rdp->disks[request->this].dev;
	    ata_raid_send_request(request);
	    rdp->disks[request->this].last_lba = bp->bio_pblkno + chunk;
	    break;

	case AR_T_RAID5:
	    if (((rdp->disks[drv].flags & (AR_DF_PRESENT|AR_DF_ONLINE)) ==
		 (AR_DF_PRESENT|AR_DF_ONLINE) && !rdp->disks[drv].dev)) {
		rdp->disks[drv].flags &= ~AR_DF_ONLINE;
		change = 1;
	    }
	    if (((rdp->disks[par].flags & (AR_DF_PRESENT|AR_DF_ONLINE)) ==
		 (AR_DF_PRESENT|AR_DF_ONLINE) && !rdp->disks[par].dev)) {
		rdp->disks[par].flags &= ~AR_DF_ONLINE;
		change = 1;
	    }
	    if (change)
		ata_raid_config_changed(rdp, 1);
	    if (!(rdp->status & AR_S_READY)) {
		ata_free_request(request);
		biofinish(bp, NULL, EIO);
		return;
	    }
	    if (rdp->status & AR_S_DEGRADED) {
		/* do the XOR game if possible */
	    }
	    else {
		request->this = drv;
		request->dev = rdp->disks[request->this].dev;
		if (bp->bio_cmd == BIO_READ) {
		    ata_raid_send_request(request);
		}
		if (bp->bio_cmd == BIO_WRITE) { 
		    ata_raid_send_request(request);
		    // sikre at læs-modify-skriv til hver disk er atomarisk.
		    // par kopi af request
		    // læse orgdata fra drv
		    // skriv nydata til drv
		    // læse parorgdata fra par
		    // skriv orgdata xor parorgdata xor nydata til par
		}
	    }
	    break;

	default:
	    printf("ar%d: unknown array type in ata_raid_strategy\n", rdp->lun);
	}
    }
}

static void
ata_raid_done(struct ata_request *request)
{
    struct ar_softc *rdp = request->driver;
    struct ata_composite *composite = NULL;
    struct bio *bp = request->bio;
    int i, mirror, finished = 0;

    switch (rdp->type) {
    case AR_T_JBOD:
    case AR_T_SPAN:
    case AR_T_RAID0:
	if (request->result) {
	    rdp->disks[request->this].flags &= ~AR_DF_ONLINE;
	    ata_raid_config_changed(rdp, 1);
	    bp->bio_error = request->result;
	    finished = 1;
	}
	else {
	    bp->bio_resid -= request->donecount;
	    if (bp->bio_resid == 0)
		finished = 1;
	}
	break;

    case AR_T_RAID1:
    case AR_T_RAID01:
	if (request->this < rdp->width)
	    mirror = request->this + rdp->width;
	else
	    mirror = request->this - rdp->width;
	if (request->result) {
	    rdp->disks[request->this].flags &= ~AR_DF_ONLINE;
	    ata_raid_config_changed(rdp, 1);
	}
	if (rdp->status & AR_S_READY) {
	    u_int64_t blk = 0;

	    if (rdp->status & AR_S_REBUILDING) 
		blk = ((request->u.ata.lba / rdp->interleave) * rdp->width) *
		      rdp->interleave + (rdp->interleave * 
		      (request->this % rdp->width)) +
		      request->u.ata.lba % rdp->interleave;

	    if (bp->bio_cmd == BIO_READ) {

		/* is this a rebuild composite */
		if ((composite = request->composite)) {
		    mtx_lock(&composite->lock);
		
		    /* handle the read part of a rebuild composite */
		    if (request->flags & ATA_R_READ) {

			/* if read failed array is now broken */
			if (request->result) {
			    rdp->disks[request->this].flags &= ~AR_DF_ONLINE;
			    ata_raid_config_changed(rdp, 1);
			    bp->bio_error = request->result;
			    rdp->rebuild_lba = blk;
			    finished = 1;
			}

			/* good data, update how far we've gotten */
			else {
			    bp->bio_resid -= request->donecount;
			    if (bp->bio_resid == 0) {
				if (composite->wr_done & (1 << mirror))
				    finished = 1;
			    }
			}
		    }

		    /* handle the write part of a rebuild composite */
		    else if (request->flags & ATA_R_WRITE) {
			if (composite->rd_done & (1 << mirror)) {
			    if (request->result) {
				printf("DOH! rebuild failed\n"); /* XXX SOS */
				rdp->rebuild_lba = blk;
			    }
			    if (bp->bio_resid == 0)
				finished = 1;
			}
		    }
		    mtx_unlock(&composite->lock);
		}

		/* if read failed retry on the mirror */
		else if (request->result) {
		    request->dev = rdp->disks[mirror].dev;
		    ata_raid_send_request(request);
		    return;
		}

		/* we have good data */
		else {
		    bp->bio_resid -= request->donecount;
		    if (bp->bio_resid == 0)
			finished = 1;
		}
	    }
	    else if (bp->bio_cmd == BIO_WRITE) {
		/* do we have a mirror or rebuild to deal with ? */
		if ((composite = request->composite)) {
		    mtx_lock(&composite->lock);
		    if (composite->wr_done & (1 << mirror)) {
			if (request->result) {
			    if (composite->request[mirror]->result) {
				printf("DOH! all disks failed and got here\n");
				bp->bio_error = EIO;
			    }
			    if (rdp->status & AR_S_REBUILDING) {
				rdp->rebuild_lba = blk;
				printf("DOH! rebuild failed\n"); /* XXX SOS */
			    }
			    bp->bio_resid -=
				composite->request[mirror]->donecount;
			}
			else
			    bp->bio_resid -= request->donecount;
			if (bp->bio_resid == 0)
			    finished = 1;
		    }
		    mtx_unlock(&composite->lock);
		}
		/* no mirror we are done */
		else {
		    bp->bio_resid -= request->donecount;
		    if (bp->bio_resid == 0)
			finished = 1;
		}
	    }
	}
	else 
	    biofinish(bp, NULL, request->result);
	break;

    case AR_T_RAID5:
	if (request->result) {
	    rdp->disks[request->this].flags &= ~AR_DF_ONLINE;
	    ata_raid_config_changed(rdp, 1);
	    if (rdp->status & AR_S_READY) {
		if (bp->bio_cmd == BIO_READ) {
		    /* do the XOR game to recover data */
		}
		if (bp->bio_cmd == BIO_WRITE) {
		    /* if the parity failed we're OK sortof */
		    /* otherwise wee need to do the XOR long dance */
		}
		finished = 1;
	    }
	    else
		biofinish(bp, NULL, request->result);
	}
	else {
	    // did we have an XOR game going ??
	    bp->bio_resid -= request->donecount;
	    if (bp->bio_resid == 0)
		finished = 1;
	}
	break;

    default:
	printf("ar%d: unknown array type in ata_raid_done\n", rdp->lun);
    }

    if (finished) {
	if ((rdp->status & AR_S_REBUILDING) && 
	    rdp->rebuild_lba >= rdp->total_sectors) {
	    int disk;

	    for (disk = 0; disk < rdp->total_disks; disk++) {
		if ((rdp->disks[disk].flags &
		     (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_SPARE)) ==
		    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_SPARE)) {
		    rdp->disks[disk].flags &= ~AR_DF_SPARE;
		    rdp->disks[disk].flags |= AR_DF_ONLINE;
		}
	    }
	    rdp->status &= ~AR_S_REBUILDING;
	    ata_raid_config_changed(rdp, 1);
	}
	biodone(bp);
    }
		 
    if (composite) {
	if (finished) {
	    /* we are done with this composite, free all resources */
	    for (i = 0; i < 32; i++) {
		if (composite->rd_needed & (1 << i) ||
		    composite->wr_needed & (1 << i)) {
		    ata_free_request(composite->request[i]);
		}
	    }
	    mtx_destroy(&composite->lock);
	    ata_free_composite(composite);
	}
    }
    else
	ata_free_request(request);
}

static void
ata_raid_config_changed(struct ar_softc *rdp, int writeback)
{
    int disk, count, status;

    mtx_lock(&rdp->lock);
    /* set default all working mode */
    status = rdp->status;
    rdp->status &= ~AR_S_DEGRADED;
    rdp->status |= AR_S_READY;

    /* make sure all lost drives are accounted for */
    for (disk = 0; disk < rdp->total_disks; disk++) {
	if (!(rdp->disks[disk].flags & AR_DF_PRESENT))
	    rdp->disks[disk].flags &= ~AR_DF_ONLINE;
    }

    /* depending on RAID type figure out our health status */
    switch (rdp->type) {
    case AR_T_JBOD:
    case AR_T_SPAN:
    case AR_T_RAID0:
	for (disk = 0; disk < rdp->total_disks; disk++) 
	    if (!(rdp->disks[disk].flags & AR_DF_ONLINE))
		rdp->status &= ~AR_S_READY; 
	break;

    case AR_T_RAID1:
    case AR_T_RAID01:
	for (disk = 0; disk < rdp->width; disk++) {
	    if (!(rdp->disks[disk].flags & AR_DF_ONLINE) &&
		!(rdp->disks[disk + rdp->width].flags & AR_DF_ONLINE)) {
		rdp->status &= ~AR_S_READY;
	    }
	    else if (((rdp->disks[disk].flags & AR_DF_ONLINE) &&
		      !(rdp->disks[disk + rdp->width].flags & AR_DF_ONLINE)) ||
		     (!(rdp->disks[disk].flags & AR_DF_ONLINE) &&
		      (rdp->disks [disk + rdp->width].flags & AR_DF_ONLINE))) {
		rdp->status |= AR_S_DEGRADED;
	    }
	}
	break;

    case AR_T_RAID5:
	for (count = 0, disk = 0; disk < rdp->total_disks; disk++) {
	    if (!(rdp->disks[disk].flags & AR_DF_ONLINE))
		count++;
	}
	if (count) {
	    if (count > 1)
		rdp->status &= ~AR_S_READY;
	    else
		rdp->status |= AR_S_DEGRADED;
	}
	break;
    default:
	rdp->status &= ~AR_S_READY;
    }

    if (rdp->status != status) {
	if (!(rdp->status & AR_S_READY)) {
	    printf("ar%d: FAILURE - %s array broken\n",
		   rdp->lun, ata_raid_type(rdp));
	}
	else if (rdp->status & AR_S_DEGRADED) {
	    if (rdp->type & (AR_T_RAID1 | AR_T_RAID01))
		printf("ar%d: WARNING - mirror", rdp->lun);
	    else
		printf("ar%d: WARNING - parity", rdp->lun);
	    printf(" protection lost. %s array in DEGRADED mode\n",
		   ata_raid_type(rdp));
	}
    }
    mtx_unlock(&rdp->lock);
    if (writeback)
	ata_raid_write_metadata(rdp);

}

static int
ata_raid_status(struct ata_ioc_raid_config *config)
{
    struct ar_softc *rdp;
    int i;
	
    if (!(rdp = ata_raid_arrays[config->lun]))
	return ENXIO;
	
    config->type = rdp->type;
    config->total_disks = rdp->total_disks;
    for (i = 0; i < rdp->total_disks; i++ ) {
	if ((rdp->disks[i].flags & AR_DF_PRESENT) && rdp->disks[i].dev)  
	    config->disks[i] = device_get_unit(rdp->disks[i].dev);
	else
	    config->disks[i] = -1;
    }
    config->interleave = rdp->interleave;
    config->status = rdp->status;
    config->progress = 100 * rdp->rebuild_lba / rdp->total_sectors;
    return 0;
}

static int
ata_raid_create(struct ata_ioc_raid_config *config)
{
    struct ar_softc *rdp;
    device_t subdisk;
    int array, disk;
    int ctlr = 0, disk_size = 0, total_disks = 0;

    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!ata_raid_arrays[array])
	    break;
    }
    if (array >= MAX_ARRAYS)
	return ENOSPC;

    if (!(rdp = (struct ar_softc*)malloc(sizeof(struct ar_softc), M_AR,
					 M_NOWAIT | M_ZERO))) {
	printf("ar%d: no memory for metadata storage\n", array);
	return ENOMEM;
    }

    for (disk = 0; disk < config->total_disks; disk++) {
	if ((subdisk = devclass_get_device(ata_raid_sub_devclass,
					   config->disks[disk]))) {
	    struct ata_raid_subdisk *ars = device_get_softc(subdisk);

	    /* is device already assigned to another array ? */
	    if (ars->raid) {
		config->disks[disk] = -1;
		free(rdp, M_AR);
		return EBUSY;
	    }
	    rdp->disks[disk].dev = device_get_parent(subdisk);

	    switch (pci_get_vendor(GRANDPARENT(rdp->disks[disk].dev))) {
	    case ATA_HIGHPOINT_ID:
		/* 
		 * we need some way to decide if it should be v2 or v3
		 * for now just use v2 since the v3 BIOS knows how to 
		 * handle that as well.
		 */
		ctlr = AR_F_HPTV2_RAID;
		rdp->disks[disk].sectors = HPTV3_LBA(rdp->disks[disk].dev);
		break;

	    case ATA_ITE_ID:
		ctlr = AR_F_ITE_RAID;
		rdp->disks[disk].sectors = ITE_LBA(rdp->disks[disk].dev);
		break;

	    case 0:     /* XXX SOS cover up for bug in our PCI code */
	    case ATA_PROMISE_ID:        
		ctlr = AR_F_PROMISE_RAID;
		rdp->disks[disk].sectors = PR_LBA(rdp->disks[disk].dev);
		break;

	    case ATA_VIA_ID:        
		ctlr = AR_F_VIA_RAID;
		rdp->disks[disk].sectors = VIA_LBA(rdp->disks[disk].dev);
		break;

	    default:
		/* XXX SOS
		 * right, so here we are, we have an ATA chip and we want
		 * to create a RAID and store the metadata.
		 * we need to find a way to tell what kind of metadata this
		 * hardware's BIOS might be using (good ideas are welcomed)
		 * for now we just use our own native FreeBSD format.
		 * the only way to get support for the BIOS format is to
		 * setup the RAID from there, in that case we pickup the
		 * metadata format from the disks (if we support it).
		 */
		printf("WARNING!! - not able to determine metadata format\n"
		       "WARNING!! - Using FreeBSD PsuedoRAID metadata\n"
		       "If that is not what you want, use the BIOS to "
		       "create the array\n");
		ctlr = AR_F_FREEBSD_RAID;
		rdp->disks[disk].sectors = PR_LBA(rdp->disks[disk].dev);
		break;
	    }

	    /* we need all disks to be of the same format */
	    if ((rdp->format & AR_F_FORMAT_MASK) &&
		(rdp->format & AR_F_FORMAT_MASK) != (ctlr & AR_F_FORMAT_MASK)) {
		free(rdp, M_AR);
		return EXDEV;
	    }
	    else
		rdp->format = ctlr;
	    
	    /* use the smallest disk of the lots size */
	    /* gigabyte boundry ??? XXX SOS */
	    if (disk_size)
		disk_size = min(rdp->disks[disk].sectors, disk_size);
	    else
		disk_size = rdp->disks[disk].sectors;
	    rdp->disks[disk].flags = 
		(AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE);

	    total_disks++;
	}
	else {
	    config->disks[disk] = -1;
	    free(rdp, M_AR);
	    return ENXIO;
	}
    }

    if (total_disks != config->total_disks) {
	free(rdp, M_AR);
	return ENODEV;
    }

    switch (config->type) {
    case AR_T_JBOD:
    case AR_T_SPAN:
    case AR_T_RAID0:
	break;

    case AR_T_RAID1:
	if (total_disks != 2) {
	    free(rdp, M_AR);
	    return EPERM;
	}
	break;

    case AR_T_RAID01:
	if (total_disks % 2 != 0) {
	    free(rdp, M_AR);
	    return EPERM;
	}
	break;

    case AR_T_RAID5:
	if (total_disks < 3) {
	    free(rdp, M_AR);
	    return EPERM;
	}
	break;

    default:
	free(rdp, M_AR);
	return EOPNOTSUPP;
    }
    rdp->type = config->type;
    rdp->lun = array;
    if (rdp->type == AR_T_RAID0 || rdp->type == AR_T_RAID01 ||
	rdp->type == AR_T_RAID5) {
	int bit = 0;

	while (config->interleave >>= 1)
	    bit++;
	rdp->interleave = 1 << bit;
    }
    rdp->offset_sectors = 0;

    /* values that depend on metadata format */
    switch (rdp->format) {
    case AR_F_ADAPTEC_RAID:
	rdp->interleave = min(max(32, rdp->interleave), 128); /*+*/
	break;

    case AR_F_HPTV2_RAID:
	rdp->interleave = min(max(8, rdp->interleave), 128); /*+*/
	rdp->offset_sectors = HPTV2_LBA(x) + 1;
	break;

    case AR_F_HPTV3_RAID:
	rdp->interleave = min(max(32, rdp->interleave), 4096); /*+*/
	break;

    case AR_F_INTEL_RAID:
	rdp->interleave = min(max(8, rdp->interleave), 256); /*+*/
	break;

    case AR_F_ITE_RAID:
	rdp->interleave = min(max(2, rdp->interleave), 128); /*+*/
	break;

    case AR_F_LSIV2_RAID:
	rdp->interleave = min(max(2, rdp->interleave), 4096);
	break;

    case AR_F_LSIV3_RAID:
	rdp->interleave = min(max(2, rdp->interleave), 256);
	break;

    case AR_F_PROMISE_RAID:
	rdp->interleave = min(max(2, rdp->interleave), 2048); /*+*/
	break;

    case AR_F_SII_RAID:
	rdp->interleave = min(max(8, rdp->interleave), 256); /*+*/
	break;

    case AR_F_VIA_RAID:
	rdp->interleave = min(max(8, rdp->interleave), 128); /*+*/
	break;
    }

    rdp->total_disks = total_disks;
    rdp->width = total_disks / (rdp->type & (AR_RAID1 | AR_T_RAID01) ? 2 : 1);
    rdp->total_sectors = disk_size * (rdp->width - (rdp->type == AR_RAID5));
    rdp->heads = 255;
    rdp->sectors = 63;
    rdp->cylinders = rdp->total_sectors / (255 * 63);
    rdp->rebuild_lba = 0;
    rdp->status |= AR_S_READY;

    /* we are committed to this array, grap the subdisks */
    for (disk = 0; disk < config->total_disks; disk++) {
	if ((subdisk = devclass_get_device(ata_raid_sub_devclass,
					   config->disks[disk]))) {
	    struct ata_raid_subdisk *ars = device_get_softc(subdisk);

	    ars->raid = rdp;
	    ars->disk_number = disk;
	}
    }
    ata_raid_attach(rdp, 1);
    ata_raid_arrays[array] = rdp;
    config->lun = array;
    return 0;
}

static int
ata_raid_delete(int array)
{
    struct ar_softc *rdp;    
    device_t subdisk;
    int disk;

    if (!(rdp = ata_raid_arrays[array]))
	return ENXIO;
 
    rdp->status &= ~AR_S_READY;
    disk_destroy(rdp->disk);

    for (disk = 0; disk < rdp->total_disks; disk++) {
	if ((rdp->disks[disk].flags & AR_DF_PRESENT) && rdp->disks[disk].dev) {
	    if ((subdisk = devclass_get_device(ata_raid_sub_devclass,
		     device_get_unit(rdp->disks[disk].dev)))) {
		struct ata_raid_subdisk *ars = device_get_softc(subdisk);

		if (ars->raid != rdp)           /* XXX SOS */
		    device_printf(subdisk, "DOH! this disk doesn't belong\n");
		if (ars->disk_number != disk)   /* XXX SOS */
		    device_printf(subdisk, "DOH! this disk number is wrong\n");
		ars->raid = NULL;
		ars->disk_number = -1;
	    }
	    rdp->disks[disk].flags = 0;
	}
    }
    ata_raid_write_metadata(rdp);       /* XXX SOS wipe metadata instead? */
    ata_raid_arrays[array] = NULL;
    free(rdp, M_AR);
    return 0;
}

static int
ata_raid_addspare(struct ata_ioc_raid_config *config)
{
    struct ar_softc *rdp;    
    device_t subdisk;
    int disk;

    if (!(rdp = ata_raid_arrays[config->lun]))
	return ENXIO;
    if (!(rdp->status & AR_S_DEGRADED) || !(rdp->status & AR_S_READY))
	return ENXIO;
    if (rdp->status & AR_S_REBUILDING)
	return EBUSY; 
    switch (rdp->type) {
    case AR_T_RAID1:
    case AR_T_RAID01:
    case AR_T_RAID5:
	for (disk = 0; disk < rdp->total_disks; disk++ ) {

	    if (((rdp->disks[disk].flags & (AR_DF_PRESENT | AR_DF_ONLINE)) ==
		 (AR_DF_PRESENT | AR_DF_ONLINE)) && rdp->disks[disk].dev)
		continue;

	    if ((subdisk = devclass_get_device(ata_raid_sub_devclass,
					       config->disks[0] ))) {
		struct ata_raid_subdisk *ars = device_get_softc(subdisk);

		if (ars->raid) 
		    return EBUSY;
    
		/* XXX SOS validate size etc etc */
		ars->raid = rdp;
		ars->disk_number = disk;
		rdp->disks[disk].dev = device_get_parent(subdisk);
		rdp->disks[disk].flags =
		    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_SPARE);

		device_printf(rdp->disks[disk].dev,
			      "inserted into ar%d disk%d as spare\n",
			      rdp->lun, disk);
		ata_raid_config_changed(rdp, 1);
		return 0;
	    }
	}
	return ENXIO;

    default:
	return EPERM;
    }
}
 
static int
ata_raid_rebuild(int array)
{
    struct ar_softc *rdp;    
    int disk, count;

    if (!(rdp = ata_raid_arrays[array]))
	return ENXIO;
    /* XXX SOS we should lock the rdp softc here */
    if (!(rdp->status & AR_S_DEGRADED) || !(rdp->status & AR_S_READY))
	return ENXIO;
    if (rdp->status & AR_S_REBUILDING)
	return EBUSY; 

    switch (rdp->type) {
    case AR_T_RAID1:
    case AR_T_RAID01:
    case AR_T_RAID5:
	for (count = 0, disk = 0; disk < rdp->total_disks; disk++ ) {
	    if (((rdp->disks[disk].flags &
		  (AR_DF_PRESENT|AR_DF_ASSIGNED|AR_DF_ONLINE|AR_DF_SPARE)) ==
		 (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_SPARE)) &&
		rdp->disks[disk].dev) {
		count++;
	    }
	}

	if (count) {
	    rdp->rebuild_lba = 0;
	    rdp->status |= AR_S_REBUILDING;
	    return 0;
	}
	return EIO;

    default:
	return EPERM;
    }
}

static int
ata_raid_read_metadata(device_t subdisk)
{
    devclass_t pci_devclass = devclass_find("pci");
    devclass_t devclass=device_get_devclass(GRANDPARENT(GRANDPARENT(subdisk)));

    /* prioritize vendor native metadata layout if possible */
    if (devclass == pci_devclass) {
	switch (pci_get_vendor(GRANDPARENT(device_get_parent(subdisk)))) {
	case ATA_HIGHPOINT_ID: 
	    if (ata_raid_hptv3_read_meta(subdisk, ata_raid_arrays))
		return 0;
	    if (ata_raid_hptv2_read_meta(subdisk, ata_raid_arrays))
		return 0;
	    break;

	case ATA_INTEL_ID:
	    if (ata_raid_intel_read_meta(subdisk, ata_raid_arrays))
		return 0;
	    break;

	case ATA_ITE_ID:
	    if (ata_raid_ite_read_meta(subdisk, ata_raid_arrays))
		return 0;
	    break;

	case ATA_NVIDIA_ID:
	    if (ata_raid_nvidia_read_meta(subdisk, ata_raid_arrays))
		return 0;
	    break;

	case 0:         /* XXX SOS cover up for bug in our PCI code */
	case ATA_PROMISE_ID: 
	    if (ata_raid_promise_read_meta(subdisk, ata_raid_arrays, 0))
		return 0;
	    break;

	case ATA_SILICON_IMAGE_ID:
	    if (ata_raid_sii_read_meta(subdisk, ata_raid_arrays))
		return 0;
	    break;

	case ATA_VIA_ID:
	    if (ata_raid_via_read_meta(subdisk, ata_raid_arrays))
		return 0;
	    break;
	}
    }
    
    /* handle controllers that have multiple layout possibilities */
    /* NOTE: the order of these are not insignificant */

    /* Adaptec HostRAID */
    if (ata_raid_adaptec_read_meta(subdisk, ata_raid_arrays))
	return 0;

    /* LSILogic v3 and v2 */
    if (ata_raid_lsiv3_read_meta(subdisk, ata_raid_arrays))
	return 0;
    if (ata_raid_lsiv2_read_meta(subdisk, ata_raid_arrays))
	return 0;

    /* if none of the above matched, try FreeBSD native format */
    return ata_raid_promise_read_meta(subdisk, ata_raid_arrays, 1);
}

static int
ata_raid_write_metadata(struct ar_softc *rdp)
{
    switch (rdp->format) {
    case AR_F_FREEBSD_RAID:
    case AR_F_PROMISE_RAID: 
	return ata_raid_promise_write_meta(rdp);

    case AR_F_HPTV3_RAID:
    case AR_F_HPTV2_RAID:
	/*
	 * always write HPT v2 metadata, the v3 BIOS knows it as well.
	 * this is handy since we cannot know what version BIOS is on there
	 */
	return ata_raid_hptv2_write_meta(rdp);
#if 0
    case AR_F_HPTV3_RAID:
	return ata_raid_hptv3_write_meta(rdp);

    case AR_F_ADAPTEC_RAID:
	return ata_raid_adaptec_write_meta(rdp);

    case AR_F_INTEL_RAID:
	return ata_raid_intel_write_meta(rdp);

    case AR_F_ITE_RAID:
	return ata_raid_ite_write_meta(rdp);

    case AR_F_LSIV2_RAID:
	return ata_raid_lsiv2_write_meta(rdp);

    case AR_F_LSIV3_RAID:
	return ata_raid_lsiv3_write_meta(rdp);

    case AR_F_NVIDIA_RAID:
	return ata_raid_nvidia_write_meta(rdp);

    case AR_F_SII_RAID:
	return ata_raid_sii_write_meta(rdp);

    case AR_F_VIA_RAID:
	return ata_raid_via_write_meta(rdp);
#endif
    default:
	printf("ar%d: writing of %s metadata is NOT supported yet\n",
	       rdp->lun, ata_raid_format(rdp));
    }
    return -1;
}

/* Adaptec HostRAID Metadata */
static int
ata_raid_adaptec_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct adaptec_raid_conf *meta;
    struct ar_softc *raid;
    int array, disk, retval = 0; 

    if (!(meta = (struct adaptec_raid_conf *)
	  malloc(sizeof(struct adaptec_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, ADP_LBA(parent),
		    meta, sizeof(struct adaptec_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "Adaptec read metadata failed\n");
	goto adaptec_out;
    }

    /* check if this is a Adaptec RAID struct */
    if (meta->magic_0 != ADP_MAGIC_0 || meta->magic_3 != ADP_MAGIC_3) {
	if (testing || bootverbose)
	    device_printf(parent, "Adaptec check1 failed\n");
	goto adaptec_out;
    }

    if (testing || bootverbose)
	ata_raid_adaptec_print_meta(meta);

    /* now convert Adaptec metadata into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto adaptec_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_ADAPTEC_RAID))
	    continue;

	if (raid->magic_0 && raid->magic_0 != meta->configs[0].magic_0)
	    continue;

	if (!meta->generation || be32toh(meta->generation) > raid->generation) {
	    switch (meta->configs[0].type) {
	    case ADP_T_RAID0:
		raid->magic_0 = meta->configs[0].magic_0;
		raid->type = AR_T_RAID0;
		raid->interleave = 1 << (meta->configs[0].stripe_shift >> 1);
		raid->width = be16toh(meta->configs[0].total_disks);
		break;
	    
	    case ADP_T_RAID1:
		raid->magic_0 = meta->configs[0].magic_0;
		raid->type = AR_T_RAID1;
		raid->width = be16toh(meta->configs[0].total_disks) / 2;
		break;

	    default:
		device_printf(parent, "Adaptec unknown RAID type 0x%02x\n",
			      meta->configs[0].type);
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto adaptec_out;
	    }

	    raid->format = AR_F_ADAPTEC_RAID;
	    raid->generation = be32toh(meta->generation);
	    raid->total_disks = be16toh(meta->configs[0].total_disks);
	    raid->total_sectors = be32toh(meta->configs[0].sectors);
	    raid->heads = 255;
	    raid->sectors = 63;
	    raid->cylinders = raid->total_sectors / (63 * 255);
	    raid->offset_sectors = 0;
	    raid->rebuild_lba = 0;
	    raid->lun = array;
	    strncpy(raid->name, meta->configs[0].name,
		    min(sizeof(raid->name), sizeof(meta->configs[0].name)));

	    /* clear out any old info */
	    if (raid->generation) {
		for (disk = 0; disk < raid->total_disks; disk++) {
		    raid->disks[disk].dev = NULL;
		    raid->disks[disk].flags = 0;
		}
	    }
	}
	if (be32toh(meta->generation) >= raid->generation) {
	    struct ata_device *atadev = device_get_softc(parent);
	    struct ata_channel *ch = device_get_softc(GRANDPARENT(dev));
	    int disk_number = (ch->unit << !(ch->flags & ATA_NO_SLAVE)) +
			      (atadev->unit == ATA_MASTER ? 0 : 1);

	    raid->disks[disk_number].dev = parent;
	    raid->disks[disk_number].sectors = 
		be32toh(meta->configs[disk_number + 1].sectors);
	    raid->disks[disk_number].flags =
		(AR_DF_ONLINE | AR_DF_PRESENT | AR_DF_ASSIGNED);
	    ars->raid = raid;
	    ars->disk_number = disk_number;
	    retval = 1;
	}
	break;
    }

adaptec_out:
    free(meta, M_AR);
    return retval;
}

/* Highpoint V2 RocketRAID Metadata */
static int
ata_raid_hptv2_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct hptv2_raid_conf *meta;
    struct ar_softc *raid = NULL;
    int array, disk_number = 0, retval = 0;

    if (!(meta = (struct hptv2_raid_conf *)
	  malloc(sizeof(struct hptv2_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, HPTV2_LBA(parent),
		    meta, sizeof(struct hptv2_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "HighPoint (v2) read metadata failed\n");
	goto hptv2_out;
    }

    /* check if this is a HighPoint v2 RAID struct */
    if (meta->magic != HPTV2_MAGIC_OK && meta->magic != HPTV2_MAGIC_BAD) {
	if (testing || bootverbose)
	    device_printf(parent, "HighPoint (v2) check1 failed\n");
	goto hptv2_out;
    }

    /* is this disk defined, or an old leftover/spare ? */
    if (!meta->magic_0) {
	if (testing || bootverbose)
	    device_printf(parent, "HighPoint (v2) check2 failed\n");
	goto hptv2_out;
    }

    if (testing || bootverbose)
	ata_raid_hptv2_print_meta(meta);

    /* now convert HighPoint (v2) metadata into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto hptv2_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_HPTV2_RAID))
	    continue;

	switch (meta->type) {
	case HPTV2_T_RAID0:
	    if ((meta->order & (HPTV2_O_RAID0|HPTV2_O_OK)) ==
		(HPTV2_O_RAID0|HPTV2_O_OK))
		goto highpoint_raid1;
	    if (meta->order & (HPTV2_O_RAID0 | HPTV2_O_RAID1))
		goto highpoint_raid01;
	    if (raid->magic_0 && raid->magic_0 != meta->magic_0)
		continue;
	    raid->magic_0 = meta->magic_0;
	    raid->type = AR_T_RAID0;
	    raid->interleave = 1 << meta->stripe_shift;
	    disk_number = meta->disk_number;
	    if (!(meta->order & HPTV2_O_OK))
		meta->magic = 0;        /* mark bad */
	    break;

	case HPTV2_T_RAID1:
highpoint_raid1:
	    if (raid->magic_0 && raid->magic_0 != meta->magic_0)
		continue;
	    raid->magic_0 = meta->magic_0;
	    raid->type = AR_T_RAID1;
	    disk_number = (meta->disk_number > 0);
	    break;

	case HPTV2_T_RAID01_RAID0:
highpoint_raid01:
	    if (meta->order & HPTV2_O_RAID0) {
		if ((raid->magic_0 && raid->magic_0 != meta->magic_0) ||
		    (raid->magic_1 && raid->magic_1 != meta->magic_1))
		    continue;
		raid->magic_0 = meta->magic_0;
		raid->magic_1 = meta->magic_1;
		raid->type = AR_T_RAID01;
		raid->interleave = 1 << meta->stripe_shift;
		disk_number = meta->disk_number;
	    }
	    else {
		if (raid->magic_1 && raid->magic_1 != meta->magic_1)
		    continue;
		raid->magic_1 = meta->magic_1;
		raid->type = AR_T_RAID01;
		raid->interleave = 1 << meta->stripe_shift;
		disk_number = meta->disk_number + meta->array_width;
		if (!(meta->order & HPTV2_O_RAID1))
		    meta->magic = 0;    /* mark bad */
	    }
	    break;

	case HPTV2_T_SPAN:
	    if (raid->magic_0 && raid->magic_0 != meta->magic_0)
		continue;
	    raid->magic_0 = meta->magic_0;
	    raid->type = AR_T_SPAN;
	    disk_number = meta->disk_number;
	    break;

	default:
	    device_printf(parent, "Highpoint (v2) unknown RAID type 0x%02x\n",
			  meta->type);
	    free(raidp[array], M_AR);
	    raidp[array] = NULL;
	    goto hptv2_out;
	}

	raid->format |= AR_F_HPTV2_RAID;
	raid->disks[disk_number].dev = parent;
	raid->disks[disk_number].flags = (AR_DF_PRESENT | AR_DF_ASSIGNED);
	raid->lun = array;
	strncpy(raid->name, meta->name_1,
		min(sizeof(raid->name), sizeof(meta->name_1)));
	if (meta->magic == HPTV2_MAGIC_OK) {
	    raid->disks[disk_number].flags |= AR_DF_ONLINE;
	    raid->width = meta->array_width;
	    raid->total_sectors = meta->total_sectors;
	    raid->heads = 255;
	    raid->sectors = 63;
	    raid->cylinders = raid->total_sectors / (63 * 255);
	    raid->offset_sectors = HPTV2_LBA(parent) + 1;
	    raid->rebuild_lba = meta->rebuild_lba;
	    raid->disks[disk_number].sectors =
		raid->total_sectors / raid->width;
	}
	else
	    raid->disks[disk_number].flags &= ~AR_DF_ONLINE;

	if ((raid->type & AR_T_RAID0) && (raid->total_disks < raid->width))
	    raid->total_disks = raid->width;
	if (disk_number >= raid->total_disks)
	    raid->total_disks = disk_number + 1;
	ars->raid = raid;
	ars->disk_number = disk_number;
	retval = 1;
	break;
    }

hptv2_out:
    free(meta, M_AR);
    return retval;
}

static int
ata_raid_hptv2_write_meta(struct ar_softc *rdp)
{
    struct hptv2_raid_conf *meta;
    struct timeval timestamp;
    int disk, error = 0;

    if (!(meta = (struct hptv2_raid_conf *)
	  malloc(sizeof(struct hptv2_raid_conf), M_AR, M_NOWAIT | M_ZERO))) {
	printf("ar%d: failed to allocate metadata storage\n", rdp->lun);
	return ENOMEM;
    }

    microtime(&timestamp);
    rdp->magic_0 = timestamp.tv_sec + 2;
    rdp->magic_1 = timestamp.tv_sec;
   
    for (disk = 0; disk < rdp->total_disks; disk++) {
	if ((rdp->disks[disk].flags & (AR_DF_PRESENT | AR_DF_ONLINE)) ==
	    (AR_DF_PRESENT | AR_DF_ONLINE))
	    meta->magic = HPTV2_MAGIC_OK;
	if (rdp->disks[disk].flags & AR_DF_ASSIGNED) {
	    meta->magic_0 = rdp->magic_0;
	    if (strlen(rdp->name))
		strncpy(meta->name_1, rdp->name, sizeof(meta->name_1));
	    else
		strcpy(meta->name_1, "FreeBSD");
	}
	meta->disk_number = disk;

	switch (rdp->type) {
	case AR_T_RAID0:
	    meta->type = HPTV2_T_RAID0;
	    strcpy(meta->name_2, "RAID 0");
	    if (rdp->disks[disk].flags & AR_DF_ONLINE)
		meta->order = HPTV2_O_OK;
	    break;

	case AR_T_RAID1:
	    meta->type = HPTV2_T_RAID0;
	    strcpy(meta->name_2, "RAID 1");
	    meta->disk_number = (disk < rdp->width) ? disk : disk + 5;
	    meta->order = HPTV2_O_RAID0 | HPTV2_O_OK;
	    break;

	case AR_T_RAID01:
	    meta->type = HPTV2_T_RAID01_RAID0;
	    strcpy(meta->name_2, "RAID 0+1");
	    if (rdp->disks[disk].flags & AR_DF_ONLINE) {
		if (disk < rdp->width) {
		    meta->order = (HPTV2_O_RAID0 | HPTV2_O_RAID1);
		    meta->magic_0 = rdp->magic_0 - 1;
		}
		else {
		    meta->order = HPTV2_O_RAID1;
		    meta->disk_number -= rdp->width;
		}
	    }
	    else
		meta->magic_0 = rdp->magic_0 - 1;
	    meta->magic_1 = rdp->magic_1;
	    break;

	case AR_T_SPAN:
	    meta->type = HPTV2_T_SPAN;
	    strcpy(meta->name_2, "SPAN");
	    break;
	}

	meta->array_width = rdp->width;
	meta->stripe_shift = (rdp->width > 1) ? (ffs(rdp->interleave)-1) : 0;
	meta->total_sectors = rdp->total_sectors;
	meta->rebuild_lba = rdp->rebuild_lba;

	if (rdp->disks[disk].dev &&
	    (rdp->disks[disk].flags & (AR_DF_PRESENT | AR_DF_ONLINE)) ==
	    (AR_DF_PRESENT | AR_DF_ONLINE)) {
	    if (ata_raid_rw(rdp->disks[disk].dev,
			    HPTV2_LBA(rdp->disks[disk].dev), meta,
			    sizeof(struct promise_raid_conf),
			    ATA_R_WRITE | ATA_R_DIRECT)) {
		device_printf(rdp->disks[disk].dev, "write metadata failed\n");
		error = EIO;
	    }
	}
    }
    free(meta, M_AR);
    return error;
}

/* Highpoint V3 RocketRAID Metadata */
static int
ata_raid_hptv3_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct hptv3_raid_conf *meta;
    struct ar_softc *raid = NULL;
    int array, disk_number, retval = 0;

    if (!(meta = (struct hptv3_raid_conf *)
	  malloc(sizeof(struct hptv3_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, HPTV3_LBA(parent),
		    meta, sizeof(struct hptv3_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "HighPoint (v3) read metadata failed\n");
	goto hptv3_out;
    }

    /* check if this is a HighPoint v3 RAID struct */
    if (meta->magic != HPTV3_MAGIC) {
	if (testing || bootverbose)
	    device_printf(parent, "HighPoint (v3) check1 failed\n");
	goto hptv3_out;
    }

    /* check if there are any config_entries */
    if (meta->config_entries < 1) {
	if (testing || bootverbose)
	    device_printf(parent, "HighPoint (v3) check2 failed\n");
	goto hptv3_out;
    }

    if (testing || bootverbose)
	ata_raid_hptv3_print_meta(meta);

    /* now convert HighPoint (v3) metadata into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto hptv3_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_HPTV3_RAID))
	    continue;

	if ((raid->format & AR_F_HPTV3_RAID) && raid->magic_0 != meta->magic_0)
	    continue;
	
	switch (meta->configs[0].type) {
	case HPTV3_T_RAID0:
	    raid->type = AR_T_RAID0;
	    raid->width = meta->configs[0].total_disks;
	    disk_number = meta->configs[0].disk_number;
	    break;

	case HPTV3_T_RAID1:
	    raid->type = AR_T_RAID1;
	    raid->width = meta->configs[0].total_disks / 2;
	    disk_number = meta->configs[0].disk_number;
	    break;

	case HPTV3_T_RAID5:
	    raid->type = AR_T_RAID5;
	    raid->width = meta->configs[0].total_disks;
	    disk_number = meta->configs[0].disk_number;
	    break;

	case HPTV3_T_SPAN:
	    raid->type = AR_T_SPAN;
	    raid->width = meta->configs[0].total_disks;
	    disk_number = meta->configs[0].disk_number;
	    break;

	default:
	    device_printf(parent, "Highpoint (v3) unknown RAID type 0x%02x\n",
			  meta->configs[0].type);
	    free(raidp[array], M_AR);
	    raidp[array] = NULL;
	    goto hptv3_out;
	}
	if (meta->config_entries == 2) {
	    switch (meta->configs[1].type) {
	    case HPTV3_T_RAID1:
		if (raid->type == AR_T_RAID0) {
		    raid->type = AR_T_RAID01;
		    disk_number = meta->configs[1].disk_number +
				  (meta->configs[0].disk_number << 1);
		    break;
		}
	    default:
		device_printf(parent, "Highpoint (v3) unknown level 2 0x%02x\n",
			      meta->configs[1].type);
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto hptv3_out;
	    }
	}

	raid->magic_0 = meta->magic_0;
	raid->format = AR_F_HPTV3_RAID;
	raid->generation = meta->timestamp;
	raid->interleave = 1 << meta->configs[0].stripe_shift;
	raid->total_disks = meta->configs[0].total_disks +
	    meta->configs[1].total_disks;
	raid->total_sectors = meta->configs[0].total_sectors +
	    ((u_int64_t)meta->configs_high[0].total_sectors << 32);
	raid->heads = 255;
	raid->sectors = 63;
	raid->cylinders = raid->total_sectors / (63 * 255);
	raid->offset_sectors = 0;
	raid->rebuild_lba = meta->configs[0].rebuild_lba +
	    ((u_int64_t)meta->configs_high[0].rebuild_lba << 32);
	raid->lun = array;
	strncpy(raid->name, meta->name,
		min(sizeof(raid->name), sizeof(meta->name)));
	raid->disks[disk_number].sectors = raid->total_sectors /
	    (raid->type == AR_T_RAID5 ? raid->width - 1 : raid->width);
	raid->disks[disk_number].dev = parent;
	raid->disks[disk_number].flags = 
	    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE);
	ars->raid = raid;
	ars->disk_number = disk_number;
	retval = 1;
	break;
    }

hptv3_out:
    free(meta, M_AR);
    return retval;
}
#if 0
static int
ata_raid_hptv3_write_meta(struct ar_softc *rdp)
{
    struct hptv3_raid_conf *meta;
    int error = 0;

    if (!(meta = (struct hptv3_raid_conf *)
	  malloc(sizeof(struct hptv3_raid_conf), M_AR, M_NOWAIT | M_ZERO))) {
	printf("ar%d: failed to allocate metadata storage\n", rdp->lun);
	return ENOMEM;
    }
    return error;
}
#endif

/* Intel MatrixRAID Metadata */
static int
ata_raid_intel_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct intel_raid_conf *meta;
    struct ar_softc *raid = NULL;
    u_int32_t checksum, *ptr;
    int array, count, disk, retval = 0;

    if (!(meta = (struct intel_raid_conf *)
	  malloc(1024, M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, INTEL_LBA(parent),
		    meta, 1024, ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "Intel read metadata failed\n");
	goto intel_out;
    }

    /* check if this is a Intel RAID struct */
    if (strncmp(meta->intel_id, INTEL_MAGIC, strlen(INTEL_MAGIC))) {
	if (testing || bootverbose)
	    device_printf(parent, "Intel check1 failed\n");
	goto intel_out;
    }
    for (checksum = 0, ptr = (u_int32_t *)meta, count = 0;
	 count < (meta->config_size / sizeof(u_int32_t)); count++) {
	checksum += *ptr++;
    }

/* XXX SOS needs to be fixed */
device_printf(parent, "Intel calc=%08x meta=%08x\n", checksum, meta->checksum);

    if (checksum != meta->checksum) {  
	if (testing || bootverbose)
	    device_printf(parent, "Intel check2 failed\n");          
	//goto intel_out;
    }

    if (testing || bootverbose)
	ata_raid_intel_print_meta(meta);

    /* now convert Intel metadata into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto intel_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_INTEL_RAID))
	    continue;

	if ((raid->format & AR_F_INTEL_RAID) &&
	    (raid->magic_0 != meta->config_id))
	    continue;

	/*
	 * update our knowledge about the array config based on generation
	 * we only grap the first volume description (yet) since the
	 * BIOS'n I have access to puts crap into the following ones
	 */
	if (!meta->generation || meta->generation > raid->generation) {
	    struct intel_raid_mapping *map = 
		(struct intel_raid_mapping *)&meta->disk[meta->total_disks];

	    switch (map->type) {
	    case INTEL_T_RAID0:
		raid->type = AR_T_RAID0;
		raid->width = map->total_disks;
		break;

	    case INTEL_T_RAID1:
		raid->type = AR_T_RAID1;
		raid->width = map->total_disks / 2;
		break;

	    default:
		device_printf(parent, "Intel unknown RAID type 0x%02x\n",
			      map->type);
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto intel_out;
	    }

	    switch (map->status) {
	    case INTEL_S_READY:
		raid->status = AR_S_READY;
		break;
	    case INTEL_S_DEGRADED:
		raid->status |= AR_S_DEGRADED;
		break;
	    case INTEL_S_DISABLED:
	    case INTEL_S_FAILURE:
		raid->status = 0;
	    }

	    raid->magic_0 = meta->config_id;
	    raid->format = AR_F_INTEL_RAID;
	    raid->generation = meta->generation;
	    raid->interleave = map->stripe_sectors;
	    raid->total_disks = map->total_disks;
	    raid->total_sectors = map->total_sectors;
	    raid->heads = 255;
	    raid->sectors = 63;
	    raid->cylinders = raid->total_sectors / (63 * 255);
	    raid->offset_sectors = map->offset;         
	    raid->rebuild_lba = 0;
	    raid->lun = array;
	    strncpy(raid->name, map->name,
		    min(sizeof(raid->name), sizeof(map->name)));

	    /* clear out any old info */
	    for (disk = 0; disk < raid->total_disks; disk++) {
		raid->disks[disk].dev = NULL;
		bcopy(meta->disk[map->disk_idx[disk]].serial,
		      raid->disks[disk].serial,
		      sizeof(raid->disks[disk].serial));
		raid->disks[disk].sectors = map->disk_sectors;
		raid->disks[disk].flags = 0;
		if (meta->disk[map->disk_idx[disk]].flags & INTEL_F_ONLINE)
		    raid->disks[disk].flags |= AR_DF_ONLINE;
		if (meta->disk[map->disk_idx[disk]].flags & INTEL_F_ASSIGNED)
		    raid->disks[disk].flags |= AR_DF_ASSIGNED;
		if (meta->disk[map->disk_idx[disk]].flags & INTEL_F_SPARE) {
		    raid->disks[disk].flags &= ~(AR_DF_ONLINE | AR_DF_ASSIGNED);
		    raid->disks[disk].flags |= AR_DF_SPARE;
		}
		if (meta->disk[map->disk_idx[disk]].flags & INTEL_F_DOWN)
		    raid->disks[disk].flags &= ~AR_DF_ONLINE;
	    }
	}
	if (meta->generation >= raid->generation) {
	    for (disk = 0; disk < raid->total_disks; disk++) {
		struct ata_device *atadev = device_get_softc(parent);

		if (!strncmp(raid->disks[disk].serial, atadev->param.serial,
		    sizeof(raid->disks[disk].serial))) {
		    raid->disks[disk].dev = parent;
		    raid->disks[disk].flags |= (AR_DF_PRESENT | AR_DF_ONLINE);
		    ars->raid = raid;
		    ars->disk_number = disk;
		    retval = 1;
		}
	    }
	}
	if (retval)
	    break;
    }

intel_out:
    free(meta, M_AR);
    return retval;
}

/* Integrated Technology Express Metadata */
static int
ata_raid_ite_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct ite_raid_conf *meta;
    struct ar_softc *raid = NULL;
    int array, disk_number, count, retval = 0;
    u_int16_t *ptr;

    if (!(meta = (struct ite_raid_conf *)
	  malloc(sizeof(struct ite_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, ITE_LBA(parent),
		    meta, sizeof(struct ite_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "ITE read metadata failed\n");
	goto ite_out;
    }

    /* check if this is a ITE RAID struct */
    for (ptr = (u_int16_t *)meta->ite_id, count = 0;
	 count < sizeof(meta->ite_id)/sizeof(uint16_t); count++)
	ptr[count] = be16toh(ptr[count]);

    if (strncmp(meta->ite_id, ITE_MAGIC, strlen(ITE_MAGIC))) {
	if (testing || bootverbose)
	    device_printf(parent, "ITE check1 failed\n");
	goto ite_out;
    }

    if (testing || bootverbose)
	ata_raid_ite_print_meta(meta);

    /* now convert ITE metadata into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if ((raid = raidp[array])) {
	    if (raid->format != AR_F_ITE_RAID)
		continue;
	    if (raid->magic_0 != *((u_int64_t *)meta->timestamp_0))
		continue;
	}

	/* if we dont have a disks timestamp the RAID is invalidated */
	if (*((u_int64_t *)meta->timestamp_1) == 0)
	    goto ite_out;

	if (!raid) {
	    raidp[array] = (struct ar_softc *)malloc(sizeof(struct ar_softc),
						     M_AR, M_NOWAIT | M_ZERO);
	    if (!(raid = raidp[array])) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto ite_out;
	    }
	}

	switch (meta->type) {
	case ITE_T_RAID0:
	    raid->type = AR_T_RAID0;
	    raid->width = meta->array_width;
	    raid->total_disks = meta->array_width;
	    disk_number = meta->disk_number;
	    break;

	case ITE_T_RAID1:
	    raid->type = AR_T_RAID1;
	    raid->width = 1;
	    raid->total_disks = 2;
	    disk_number = meta->disk_number;
	    break;

	case ITE_T_RAID01:
	    raid->type = AR_T_RAID01;
	    raid->width = meta->array_width;
	    raid->total_disks = 4;
	    disk_number = ((meta->disk_number & 0x02) >> 1) |
			  ((meta->disk_number & 0x01) << 1);
	    break;

	case ITE_T_SPAN:
	    raid->type = AR_T_SPAN;
	    raid->width = 1;
	    raid->total_disks = meta->array_width;
	    disk_number = meta->disk_number;
	    break;

	default:
	    device_printf(parent, "ITE unknown RAID type 0x%02x\n", meta->type);
	    free(raidp[array], M_AR);
	    raidp[array] = NULL;
	    goto ite_out;
	}

	raid->magic_0 = *((u_int64_t *)meta->timestamp_0);
	raid->format = AR_F_ITE_RAID;
	raid->generation = 0;
	raid->interleave = meta->stripe_sectors;
	raid->total_sectors = meta->total_sectors;
	raid->heads = 255;
	raid->sectors = 63;
	raid->cylinders = raid->total_sectors / (63 * 255);
	raid->offset_sectors = 0;
	raid->rebuild_lba = 0;
	raid->lun = array;

	raid->disks[disk_number].dev = parent;
	raid->disks[disk_number].sectors = raid->total_sectors / raid->width;
	raid->disks[disk_number].flags = 
	    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE);
	ars->raid = raid;
	ars->disk_number = disk_number;
	retval = 1;
	break;
    }
ite_out:
    free(meta, M_AR);
    return retval;
}

/* LSILogic V2 MegaRAID Metadata */
static int
ata_raid_lsiv2_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct lsiv2_raid_conf *meta;
    struct ar_softc *raid = NULL;
    int array, retval = 0;

    if (!(meta = (struct lsiv2_raid_conf *)
	  malloc(sizeof(struct lsiv2_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, LSIV2_LBA(parent),
		    meta, sizeof(struct lsiv2_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "LSI (v2) read metadata failed\n");
	goto lsiv2_out;
    }

    /* check if this is a LSI RAID struct */
    if (strncmp(meta->lsi_id, LSIV2_MAGIC, strlen(LSIV2_MAGIC))) {
	if (testing || bootverbose)
	    device_printf(parent, "LSI (v2) check1 failed\n");
	goto lsiv2_out;
    }

    if (testing || bootverbose)
	ata_raid_lsiv2_print_meta(meta);

    /* now convert LSI (v2) config meta into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	int raid_entry, conf_entry;

	if (!raidp[array + meta->raid_number]) {
	    raidp[array + meta->raid_number] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array + meta->raid_number]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto lsiv2_out;
	    }
	}
	raid = raidp[array + meta->raid_number];
	if (raid->format && (raid->format != AR_F_LSIV2_RAID))
	    continue;

	if (raid->magic_0 && 
	    ((raid->magic_0 != meta->timestamp) ||
	     (raid->magic_1 != meta->raid_number)))
	    continue;

	array += meta->raid_number;

	raid_entry = meta->raid_number;
	conf_entry = (meta->configs[raid_entry].raid.config_offset >> 4) +
		     meta->disk_number - 1;

	switch (meta->configs[raid_entry].raid.type) {
	case LSIV2_T_RAID0:
	    raid->magic_0 = meta->timestamp;
	    raid->magic_1 = meta->raid_number;
	    raid->type = AR_T_RAID0;
	    raid->interleave = meta->configs[raid_entry].raid.stripe_sectors;
	    raid->width = meta->configs[raid_entry].raid.array_width; 
	    break;

	case LSIV2_T_RAID1:
	    raid->magic_0 = meta->timestamp;
	    raid->magic_1 = meta->raid_number;
	    raid->type = AR_T_RAID1;
	    raid->width = meta->configs[raid_entry].raid.array_width; 
	    break;
	    
	case LSIV2_T_RAID0 | LSIV2_T_RAID1:
	    raid->magic_0 = meta->timestamp;
	    raid->magic_1 = meta->raid_number;
	    raid->type = AR_T_RAID01;
	    raid->interleave = meta->configs[raid_entry].raid.stripe_sectors;
	    raid->width = meta->configs[raid_entry].raid.array_width; 
	    break;

	default:
	    device_printf(parent, "LSI v2 unknown RAID type 0x%02x\n",
			  meta->configs[raid_entry].raid.type);
	    free(raidp[array], M_AR);
	    raidp[array] = NULL;
	    goto lsiv2_out;
	}

	raid->format = AR_F_LSIV2_RAID;
	raid->generation = 0;
	raid->total_disks = meta->configs[raid_entry].raid.disk_count;
	raid->total_sectors = meta->configs[raid_entry].raid.total_sectors;
	raid->heads = 255;
	raid->sectors = 63;
	raid->cylinders = raid->total_sectors / (63 * 255);
	raid->offset_sectors = 0;
	raid->rebuild_lba = 0;
	raid->lun = array;

	if (meta->configs[conf_entry].disk.device != LSIV2_D_NONE) {
	    raid->disks[meta->disk_number].dev = parent;
	    raid->disks[meta->disk_number].sectors = 
		meta->configs[conf_entry].disk.disk_sectors;
	    raid->disks[meta->disk_number].flags = 
		(AR_DF_ONLINE | AR_DF_PRESENT | AR_DF_ASSIGNED);
	    ars->raid = raid;
	    ars->disk_number = meta->disk_number;
	    retval = 1;
	}
	else
	    raid->disks[meta->disk_number].flags &= ~AR_DF_ONLINE;

	break;
    }

lsiv2_out:
    free(meta, M_AR);
    return retval;
}

/* LSILogic V3 MegaRAID Metadata */
static int
ata_raid_lsiv3_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct lsiv3_raid_conf *meta;
    struct ar_softc *raid = NULL;
    u_int8_t checksum, *ptr;
    int array, entry, count, disk_number, retval = 0;

    if (!(meta = (struct lsiv3_raid_conf *)
	  malloc(sizeof(struct lsiv3_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, LSIV3_LBA(parent),
		    meta, sizeof(struct lsiv3_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "LSI (v3) read metadata failed\n");
	goto lsiv3_out;
    }

    /* check if this is a LSI RAID struct */
    if (strncmp(meta->lsi_id, LSIV3_MAGIC, strlen(LSIV3_MAGIC))) {
	if (testing || bootverbose)
	    device_printf(parent, "LSI (v3) check1 failed\n");
	goto lsiv3_out;
    }

    /* check if the checksum is OK */
    for (checksum = 0, ptr = meta->lsi_id, count = 0; count < 512; count++)
	checksum += *ptr++;
    if (checksum) {  
	if (testing || bootverbose)
	    device_printf(parent, "LSI (v3) check2 failed\n");
	goto lsiv3_out;
    }

    if (testing || bootverbose)
	ata_raid_lsiv3_print_meta(meta);

    /* now convert LSI (v3) config meta into our generic form */
    for (array = 0, entry = 0; array < MAX_ARRAYS && entry < 8;) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto lsiv3_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_LSIV3_RAID)) {
	    array++;
	    continue;
	}

	if ((raid->format == AR_F_LSIV3_RAID) &&
	    (raid->magic_0 != meta->timestamp)) {
	    array++;
	    continue;
	}

	switch (meta->raid[entry].total_disks) {
	case 0:
	    entry++;
	    continue;
	case 1:
	    if (meta->raid[entry].device == meta->device) {
		disk_number = 0;
		break;
	    }
	    if (raid->format)
		array++;
	    entry++;
	    continue;
	case 2:
	    disk_number = (meta->device & (LSIV3_D_DEVICE|LSIV3_D_CHANNEL))?1:0;
	    break;
	default:
	    device_printf(parent, "lsiv3 > 2 disk support untested!!\n");
	    disk_number = (meta->device & LSIV3_D_DEVICE ? 1 : 0) +
			  (meta->device & LSIV3_D_CHANNEL ? 2 : 0);
	    break;
	}

	switch (meta->raid[entry].type) {
	case LSIV3_T_RAID0:
	    raid->type = AR_T_RAID0;
	    raid->width = meta->raid[entry].total_disks;
	    break;

	case LSIV3_T_RAID1:
	    raid->type = AR_T_RAID1;
	    raid->width = meta->raid[entry].array_width;
	    break;

	default:
	    device_printf(parent, "LSI v3 unknown RAID type 0x%02x\n",
			  meta->raid[entry].type);
	    free(raidp[array], M_AR);
	    raidp[array] = NULL;
	    entry++;
	    continue;
	}

	raid->magic_0 = meta->timestamp;
	raid->format = AR_F_LSIV3_RAID;
	raid->generation = 0;
	raid->interleave = meta->raid[entry].stripe_pages * 8;
	raid->total_disks = meta->raid[entry].total_disks;
	raid->total_sectors = raid->width * meta->raid[entry].sectors;
	raid->heads = 255;
	raid->sectors = 63;
	raid->cylinders = raid->total_sectors / (63 * 255);
	raid->offset_sectors = meta->raid[entry].offset;
	raid->rebuild_lba = 0;
	raid->lun = array;

	raid->disks[disk_number].dev = parent;
	raid->disks[disk_number].sectors = raid->total_sectors / raid->width;
	raid->disks[disk_number].flags = 
	    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE);
	ars->raid = raid;
	ars->disk_number = disk_number;
	retval = 1;
	entry++;
	array++;
    }

lsiv3_out:
    free(meta, M_AR);
    return retval;
}

/* nVidia MediaShield Metadata */
static int
ata_raid_nvidia_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct nvidia_raid_conf *meta;
    struct ar_softc *raid = NULL;
    u_int32_t checksum, *ptr;
    int array, count, retval = 0;

    if (!(meta = (struct nvidia_raid_conf *)
	  malloc(sizeof(struct nvidia_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, NVIDIA_LBA(parent),
		    meta, sizeof(struct nvidia_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "nVidia read metadata failed\n");
	goto nvidia_out;
    }

    /* check if this is a nVidia RAID struct */
    if (strncmp(meta->nvidia_id, NV_MAGIC, strlen(NV_MAGIC))) {
	if (testing || bootverbose)
	    device_printf(parent, "nVidia check1 failed\n");
	goto nvidia_out;
    }

    /* check if the checksum is OK */
    for (checksum = 0, ptr = (u_int32_t*)meta, count = 0; 
	 count < meta->config_size; count++)
	checksum += *ptr++;
    if (checksum) {  
	if (testing || bootverbose)
	    device_printf(parent, "nVidia check2 failed\n");
	goto nvidia_out;
    }

    if (testing || bootverbose)
	ata_raid_nvidia_print_meta(meta);

    /* now convert nVidia meta into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] =
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto nvidia_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_NVIDIA_RAID))
	    continue;

	if (raid->format == AR_F_NVIDIA_RAID &&
	    ((raid->magic_0 != meta->magic_1) ||
	     (raid->magic_1 != meta->magic_2))) {
	    continue;
	}

	switch (meta->type) {
	case NV_T_SPAN:
	    raid->type = AR_T_SPAN;
	    break;

	case NV_T_RAID0: 
	    raid->type = AR_T_RAID0;
	    break;

	case NV_T_RAID1:
	    raid->type = AR_T_RAID1;
	    break;

	case NV_T_RAID5:
	    raid->type = AR_T_RAID5;
	    break;

	case NV_T_RAID01:
	    raid->type = AR_T_RAID01;
	    break;

	default:
	    device_printf(parent, "nVidia unknown RAID type 0x%02x\n",
			  meta->type);
	    free(raidp[array], M_AR);
	    raidp[array] = NULL;
	    goto nvidia_out;
	}
	raid->magic_0 = meta->magic_1;
	raid->magic_1 = meta->magic_2;
	raid->format = AR_F_NVIDIA_RAID;
	raid->generation = 0;
	raid->interleave = meta->stripe_sectors;
	raid->width = meta->array_width;
	raid->total_disks = meta->total_disks;
	raid->total_sectors = meta->total_sectors;
	raid->heads = 255;
	raid->sectors = 63;
	raid->cylinders = raid->total_sectors / (63 * 255);
	raid->offset_sectors = 0;
	raid->rebuild_lba = meta->rebuild_lba;
	raid->lun = array;
	raid->status = AR_S_READY;
	if (meta->status & NV_S_DEGRADED)
	    raid->status |= AR_S_DEGRADED;

	raid->disks[meta->disk_number].dev = parent;
	raid->disks[meta->disk_number].sectors =
	    raid->total_sectors / raid->width;
	raid->disks[meta->disk_number].flags =
	    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE);
	ars->raid = raid;
	ars->disk_number = meta->disk_number;
	retval = 1;
	break;
    }

nvidia_out:
    free(meta, M_AR);
    return retval;
}

/* Promise FastTrak Metadata */
static int
ata_raid_promise_read_meta(device_t dev, struct ar_softc **raidp, int native)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct promise_raid_conf *meta;
    struct ar_softc *raid;
    u_int32_t checksum, *ptr;
    int array, count, disk, disksum = 0, retval = 0; 

    if (!(meta = (struct promise_raid_conf *)
	  malloc(sizeof(struct promise_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, PR_LBA(parent),
		    meta, sizeof(struct promise_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "%s read metadata failed\n",
			  native ? "FreeBSD" : "Promise");
	goto promise_out;
    }

    /* check the signature */
    if (native) {
	if (strncmp(meta->promise_id, ATA_MAGIC, strlen(ATA_MAGIC))) {
	    if (testing || bootverbose)
		device_printf(parent, "FreeBSD check1 failed\n");
	    goto promise_out;
	}
    }
    else {
	if (strncmp(meta->promise_id, PR_MAGIC, strlen(PR_MAGIC))) {
	    if (testing || bootverbose)
		device_printf(parent, "Promise check1 failed\n");
	    goto promise_out;
	}
    }

    /* check if the checksum is OK */
    for (checksum = 0, ptr = (u_int32_t *)meta, count = 0; count < 511; count++)
	checksum += *ptr++;
    if (checksum != *ptr) {  
	if (testing || bootverbose)
	    device_printf(parent, "%s check2 failed\n",
			  native ? "FreeBSD" : "Promise");           
	goto promise_out;
    }

    /* check on disk integrity status */
    if (meta->raid.integrity != PR_I_VALID) {
	if (testing || bootverbose)
	    device_printf(parent, "%s check3 failed\n",
			  native ? "FreeBSD" : "Promise");           
	goto promise_out;
    }

    if (testing || bootverbose)
	ata_raid_promise_print_meta(meta);

    /* now convert Promise metadata into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto promise_out;
	    }
	}
	raid = raidp[array];
	if (raid->format &&
	    (raid->format != (native ? AR_F_FREEBSD_RAID : AR_F_PROMISE_RAID)))
	    continue;

	if ((raid->format == (native ? AR_F_FREEBSD_RAID : AR_F_PROMISE_RAID))&&
	    !(meta->raid.magic_1 == (raid->magic_1)))
	    continue;

	/* update our knowledge about the array config based on generation */
	if (!meta->raid.generation || meta->raid.generation > raid->generation){
	    switch (meta->raid.type) {
	    case PR_T_SPAN:
		raid->type = AR_T_SPAN;
		break;

	    case PR_T_JBOD:
		raid->type = AR_T_JBOD;
		break;

	    case PR_T_RAID0:
		raid->type = AR_T_RAID0;
		break;

	    case PR_T_RAID1:
		raid->type = AR_T_RAID1;
		if (meta->raid.array_width > 1)
		    raid->type = AR_T_RAID01;
		break;

	    case PR_T_RAID5:
		raid->type = AR_T_RAID5;
		break;

	    default:
		device_printf(parent, "%s unknown RAID type 0x%02x\n",
			      native ? "FreeBSD" : "Promise", meta->raid.type);
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto promise_out;
	    }
	    raid->magic_1 = meta->raid.magic_1;
	    raid->format = (native ? AR_F_FREEBSD_RAID : AR_F_PROMISE_RAID);
	    raid->generation = meta->raid.generation;
	    raid->interleave = 1 << meta->raid.stripe_shift;
	    raid->width = meta->raid.array_width;
	    raid->total_disks = meta->raid.total_disks;
	    raid->heads = meta->raid.heads + 1;
	    raid->sectors = meta->raid.sectors;
	    raid->cylinders = meta->raid.cylinders + 1;
	    raid->total_sectors = meta->raid.total_sectors;
	    raid->offset_sectors = 0;
	    raid->rebuild_lba = meta->raid.rebuild_lba;
	    raid->lun = array;
	    if ((meta->raid.status &
		 (PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY)) ==
		(PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY)) {
		raid->status |= AR_S_READY;
		if (meta->raid.status & PR_S_DEGRADED)
		    raid->status |= AR_S_DEGRADED;
	    }
	    else
		raid->status &= ~AR_S_READY;

	    /* convert disk flags to our internal types */
	    for (disk = 0; disk < meta->raid.total_disks; disk++) {
		raid->disks[disk].dev = NULL;
		raid->disks[disk].flags = 0;
		*((u_int64_t *)(raid->disks[disk].serial)) = 
		    meta->raid.disk[disk].magic_0;
		disksum += meta->raid.disk[disk].flags;
		if (meta->raid.disk[disk].flags & PR_F_ONLINE)
		    raid->disks[disk].flags |= AR_DF_ONLINE;
		if (meta->raid.disk[disk].flags & PR_F_ASSIGNED)
		    raid->disks[disk].flags |= AR_DF_ASSIGNED;
		if (meta->raid.disk[disk].flags & PR_F_SPARE) {
		    raid->disks[disk].flags &= ~(AR_DF_ONLINE | AR_DF_ASSIGNED);
		    raid->disks[disk].flags |= AR_DF_SPARE;
		}
		if (meta->raid.disk[disk].flags & (PR_F_REDIR | PR_F_DOWN))
		    raid->disks[disk].flags &= ~AR_DF_ONLINE;
	    }
	    if (!disksum) {
		device_printf(parent, "%s subdisks has no flags\n",
			      native ? "FreeBSD" : "Promise");
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto promise_out;
	    }
	}
	if (meta->raid.generation >= raid->generation) {
	    int disk_number = meta->raid.disk_number;

	    if (raid->disks[disk_number].flags && (meta->magic_0 ==
		*((u_int64_t *)(raid->disks[disk_number].serial)))) {
		raid->disks[disk_number].dev = parent;
		raid->disks[disk_number].flags |= AR_DF_PRESENT;
		raid->disks[disk_number].sectors = meta->raid.disk_sectors;
		if ((raid->disks[disk_number].flags &
		    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE)) ==
		    (AR_DF_PRESENT | AR_DF_ASSIGNED | AR_DF_ONLINE)) {
		    ars->raid = raid;
		    ars->disk_number = disk_number;
		    retval = 1;
		}
	    }
	}
	break;
    }

promise_out:
    free(meta, M_AR);
    return retval;
}

static int
ata_raid_promise_write_meta(struct ar_softc *rdp)
{
    struct promise_raid_conf *meta;
    struct timeval timestamp;
    u_int32_t *ckptr;
    int count, disk, drive, error = 0;

    if (!(meta = (struct promise_raid_conf *)
	  malloc(sizeof(struct promise_raid_conf), M_AR, M_NOWAIT))) {
	printf("ar%d: failed to allocate metadata storage\n", rdp->lun);
	return ENOMEM;
    }

    rdp->generation++;
    microtime(&timestamp);

    for (disk = 0; disk < rdp->total_disks; disk++) {
	for (count = 0; count < sizeof(struct promise_raid_conf); count++)
	    *(((u_int8_t *)meta) + count) = 255 - (count % 256);
	meta->dummy_0 = 0x00020000;
	meta->raid.disk_number = disk;

	if ((rdp->disks[disk].flags & AR_DF_PRESENT) && rdp->disks[disk].dev) {
	    struct ata_device *atadev = device_get_softc(rdp->disks[disk].dev);
	    struct ata_channel *ch = 
		device_get_softc(device_get_parent(rdp->disks[disk].dev));

	    meta->raid.channel = ch->unit;
	    meta->raid.device = (atadev->unit != 0);
	    meta->raid.disk_sectors = rdp->disks[disk].sectors;
	    meta->raid.disk_offset = rdp->offset_sectors;
	}
	else {
	    meta->raid.channel = 0;
	    meta->raid.device = 0;
	    meta->raid.disk_sectors = 0;
	    meta->raid.disk_offset = 0;
	}
	meta->magic_0 = PR_MAGIC0(meta->raid) | timestamp.tv_sec;
	meta->magic_1 = timestamp.tv_sec >> 16;
	meta->magic_2 = timestamp.tv_sec;
	meta->raid.integrity = PR_I_VALID;
	meta->raid.magic_0 = meta->magic_0;
	meta->raid.rebuild_lba = rdp->rebuild_lba;
	meta->raid.generation = rdp->generation;

	if (rdp->status & AR_S_READY) {
	    meta->raid.flags = (PR_F_VALID | PR_F_ASSIGNED | PR_F_ONLINE);
	    meta->raid.status = 
		(PR_S_VALID | PR_S_ONLINE | PR_S_INITED | PR_S_READY);
	    if (rdp->status & AR_S_DEGRADED)
		meta->raid.status |= PR_S_DEGRADED;
	    else
		meta->raid.status |= PR_S_FUNCTIONAL;
	}
	else {
	    meta->raid.flags = PR_F_DOWN;
	    meta->raid.status = 0;
	}

	switch (rdp->type) {
	case AR_T_RAID0:
	    meta->raid.type = PR_T_RAID0;
	    break;
	case AR_T_RAID1:
	    meta->raid.type = PR_T_RAID1;
	    break;
	case AR_T_RAID01:
	    meta->raid.type = PR_T_RAID1;
	    break;
	case AR_T_RAID5:
	    meta->raid.type = PR_T_RAID5;
	    break;
	case AR_T_SPAN:
	    meta->raid.type = PR_T_SPAN;
	    break;
	case AR_T_JBOD:
	    meta->raid.type = PR_T_JBOD;
	    break;
	}

	meta->raid.total_disks = rdp->total_disks;
	meta->raid.stripe_shift = ffs(rdp->interleave) - 1;
	meta->raid.array_width = rdp->width;
	meta->raid.array_number = rdp->lun;
	meta->raid.total_sectors = rdp->total_sectors;
	meta->raid.cylinders = rdp->cylinders - 1;
	meta->raid.heads = rdp->heads - 1;
	meta->raid.sectors = rdp->sectors;
	meta->raid.magic_1 = (u_int64_t)meta->magic_2<<16 | meta->magic_1;

	bzero(&meta->raid.disk, 8 * 12);
	for (drive = 0; drive < rdp->total_disks; drive++) {
	    meta->raid.disk[drive].flags = 0;
	    if (rdp->disks[drive].flags & AR_DF_PRESENT)
		meta->raid.disk[drive].flags |= PR_F_VALID;
	    if (rdp->disks[drive].flags & AR_DF_ASSIGNED)
		meta->raid.disk[drive].flags |= PR_F_ASSIGNED;
	    if (rdp->disks[drive].flags & AR_DF_ONLINE)
		meta->raid.disk[drive].flags |= PR_F_ONLINE;
	    else
		if (rdp->disks[drive].flags & AR_DF_PRESENT)
		    meta->raid.disk[drive].flags = (PR_F_REDIR | PR_F_DOWN);
	    if (rdp->disks[drive].flags & AR_DF_SPARE)
		meta->raid.disk[drive].flags |= PR_F_SPARE;
	    meta->raid.disk[drive].dummy_0 = 0x0;
	    if (rdp->disks[drive].dev) {
		struct ata_channel *ch = 
		    device_get_softc(device_get_parent(rdp->disks[drive].dev));
		struct ata_device *atadev =
		    device_get_softc(rdp->disks[drive].dev);

		meta->raid.disk[drive].channel = ch->unit;
		meta->raid.disk[drive].device = (atadev->unit != 0);
	    }
	    meta->raid.disk[drive].magic_0 =
		PR_MAGIC0(meta->raid.disk[drive]) | timestamp.tv_sec;
	}

	if (rdp->disks[disk].dev) {
	    if ((rdp->disks[disk].flags & (AR_DF_PRESENT | AR_DF_ONLINE)) ==
		(AR_DF_PRESENT | AR_DF_ONLINE)) {
		if (rdp->format == AR_F_FREEBSD_RAID)
		    bcopy(ATA_MAGIC, meta->promise_id, sizeof(ATA_MAGIC));
		else
		    bcopy(PR_MAGIC, meta->promise_id, sizeof(PR_MAGIC));
	    }
	    else
		bzero(meta->promise_id, sizeof(meta->promise_id));
	    meta->checksum = 0;
	    for (ckptr = (int32_t *)meta, count = 0; count < 511; count++)
		meta->checksum += *ckptr++;
	    if (ata_raid_rw(rdp->disks[disk].dev, PR_LBA(rdp->disks[disk].dev),
			    meta, sizeof(struct promise_raid_conf),
			    ATA_R_WRITE | ATA_R_DIRECT)) {
		device_printf(rdp->disks[disk].dev, "write metadata failed\n");
		error = EIO;
	    }
	}
    }
    free(meta, M_AR);
    return error;
}

/* Silicon Image Medley Metadata */
static int
ata_raid_sii_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct sii_raid_conf *meta;
    struct ar_softc *raid = NULL;
    u_int16_t checksum, *ptr;
    int array, count, disk, retval = 0;

    if (!(meta = (struct sii_raid_conf *)
	  malloc(sizeof(struct sii_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, SII_LBA(parent),
		    meta, sizeof(struct sii_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "Silicon Image read metadata failed\n");
	goto sii_out;
    }

    /* check if this is a Silicon Image (Medley) RAID struct */
    for (checksum = 0, ptr = (u_int16_t *)meta, count = 0; count < 160; count++)
	checksum += *ptr++;
    if (checksum) {  
	if (testing || bootverbose)
	    device_printf(parent, "Silicon Image check1 failed\n");
	goto sii_out;
    }

    for (checksum = 0, ptr = (u_int16_t *)meta, count = 0; count < 256; count++)
	checksum += *ptr++;
    if (checksum != meta->checksum_1) {  
	if (testing || bootverbose)
	    device_printf(parent, "Silicon Image check2 failed\n");          
	goto sii_out;
    }

    /* check verison */
    if (meta->version_major != 0x0002 ||
	(meta->version_minor != 0x0000 && meta->version_minor != 0x0001)) {
	if (testing || bootverbose)
	    device_printf(parent, "Silicon Image check3 failed\n");          
	goto sii_out;
    }

    if (testing || bootverbose)
	ata_raid_sii_print_meta(meta);

    /* now convert Silicon Image meta into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto sii_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_SII_RAID))
	    continue;

	if (raid->format == AR_F_SII_RAID &&
	    (raid->magic_0 != *((u_int64_t *)meta->timestamp))) {
	    continue;
	}

	/* update our knowledge about the array config based on generation */
	if (!meta->generation || meta->generation > raid->generation) {
	    switch (meta->type) {
	    case SII_T_RAID0:
		raid->type = AR_T_RAID0;
		break;

	    case SII_T_RAID1:
		raid->type = AR_T_RAID1;
		break;

	    case SII_T_RAID01:
		raid->type = AR_T_RAID01;
		break;

	    case SII_T_SPARE:
		device_printf(parent, "Silicon Image SPARE disk\n");
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto sii_out;

	    default:
		device_printf(parent,"Silicon Image unknown RAID type 0x%02x\n",
			      meta->type);
		free(raidp[array], M_AR);
		raidp[array] = NULL;
		goto sii_out;
	    }
	    raid->magic_0 = *((u_int64_t *)meta->timestamp);
	    raid->format = AR_F_SII_RAID;
	    raid->generation = meta->generation;
	    raid->interleave = meta->stripe_sectors;
	    raid->width = (meta->raid0_disks != 0xff) ? meta->raid0_disks : 1;
	    raid->total_disks = 
		((meta->raid0_disks != 0xff) ? meta->raid0_disks : 0) +
		((meta->raid1_disks != 0xff) ? meta->raid1_disks : 0);
	    raid->total_sectors = meta->total_sectors;
	    raid->heads = 255;
	    raid->sectors = 63;
	    raid->cylinders = raid->total_sectors / (63 * 255);
	    raid->offset_sectors = 0;
	    raid->rebuild_lba = meta->rebuild_lba;
	    raid->lun = array;
	    strncpy(raid->name, meta->name,
		    min(sizeof(raid->name), sizeof(meta->name)));

	    /* clear out any old info */
	    if (raid->generation) {
		for (disk = 0; disk < raid->total_disks; disk++) {
		    raid->disks[disk].dev = NULL;
		    raid->disks[disk].flags = 0;
		}
	    }
	}
	if (meta->generation >= raid->generation) {
	    /* XXX SOS add check for the right physical disk by serial# */
	    if (meta->status & SII_S_READY) {
		int disk_number = (raid->type == AR_T_RAID01) ?
		    meta->raid1_ident + (meta->raid0_ident << 1) :
		    meta->disk_number;

		raid->disks[disk_number].dev = parent;
		raid->disks[disk_number].sectors = 
		    raid->total_sectors / raid->width;
		raid->disks[disk_number].flags =
		    (AR_DF_ONLINE | AR_DF_PRESENT | AR_DF_ASSIGNED);
		ars->raid = raid;
		ars->disk_number = disk_number;
		retval = 1;
	    }
	}
	break;
    }

sii_out:
    free(meta, M_AR);
    return retval;
}

/* VIA Tech V-RAID Metadata */
static int
ata_raid_via_read_meta(device_t dev, struct ar_softc **raidp)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);
    device_t parent = device_get_parent(dev);
    struct via_raid_conf *meta;
    struct ar_softc *raid = NULL;
    u_int8_t checksum, *ptr;
    int array, count, disk, retval = 0;

    if (!(meta = (struct via_raid_conf *)
	  malloc(sizeof(struct via_raid_conf), M_AR, M_NOWAIT | M_ZERO)))
	return ENOMEM;

    if (ata_raid_rw(parent, VIA_LBA(parent),
		    meta, sizeof(struct via_raid_conf), ATA_R_READ)) {
	if (testing || bootverbose)
	    device_printf(parent, "VIA read metadata failed\n");
	goto via_out;
    }

    /* check if this is a VIA RAID struct */
    if (meta->magic != VIA_MAGIC) {
	if (testing || bootverbose)
	    device_printf(parent, "VIA check1 failed\n");
	goto via_out;
    }

    /* calc the checksum and compare for valid */
    for (checksum = 0, ptr = (u_int8_t *)meta, count = 0; count < 50; count++)
	checksum += *ptr++;
    if (checksum != meta->checksum) {  
	if (testing || bootverbose)
	    device_printf(parent, "VIA check2 failed\n");
	goto via_out;
    }

    if (testing || bootverbose)
	ata_raid_via_print_meta(meta);

    /* now convert VIA meta into our generic form */
    for (array = 0; array < MAX_ARRAYS; array++) {
	if (!raidp[array]) {
	    raidp[array] = 
		(struct ar_softc *)malloc(sizeof(struct ar_softc), M_AR,
					  M_NOWAIT | M_ZERO);
	    if (!raidp[array]) {
		device_printf(parent, "failed to allocate metadata storage\n");
		goto via_out;
	    }
	}
	raid = raidp[array];
	if (raid->format && (raid->format != AR_F_VIA_RAID))
	    continue;

	if (raid->format == AR_F_VIA_RAID && (raid->magic_0 != meta->disks[0]))
	    continue;

	switch (meta->type & VIA_T_MASK) {
	case VIA_T_RAID0:
	    raid->type = AR_T_RAID0;
	    raid->width = meta->stripe_layout & VIA_L_MASK;
	    raid->total_sectors = meta->total_sectors;
	    break;

	case VIA_T_RAID1:
	    raid->type = AR_T_RAID1;
	    raid->width = 1;
	    raid->total_sectors = meta->total_sectors;
	    break;

	case VIA_T_SPAN:
	    raid->type = AR_T_SPAN;
	    raid->width = 1;
	    raid->total_sectors += meta->total_sectors;
	    break;

	default:
	    device_printf(parent,"VIA unknown RAID type 0x%02x\n", meta->type);
	    free(raidp[array], M_AR);
	    raidp[array] = NULL;
	    goto via_out;
	}
	raid->magic_0 = meta->disks[0];
	raid->format = AR_F_VIA_RAID;
	raid->generation = 0;
	raid->interleave = 0x08 << (meta->stripe_layout >> VIA_L_SHIFT);
	for (count = 0, disk = 0; disk < 8; disk++)
	    if (meta->disks[disk])
		count++;
	raid->total_disks = count;
	raid->heads = 255;
	raid->sectors = 63;
	raid->cylinders = raid->total_sectors / (63 * 255);

	for (disk = 0; disk < 8; disk++) {
	    if ((meta->disks[disk] == meta->disk_id) &&
		((disk << 2) == meta->disk_index)) {
		raid->disks[disk].dev = parent;
		raid->disks[disk].sectors = meta->total_sectors / raid->width;
		raid->disks[disk].flags =
		    (AR_DF_ONLINE | AR_DF_PRESENT | AR_DF_ASSIGNED);
		ars->raid = raid;
		ars->disk_number = disk;
		retval = 1;
		break;
	    }
	}
	break;
    }

via_out:
    free(meta, M_AR);
    return retval;
}

static struct ata_request *
ata_raid_init_request(struct ar_softc *rdp, struct bio *bio)
{
    struct ata_request *request;

    if (!(request = ata_alloc_request())) {
	printf("FAILURE - out of memory in ata_raid_init_request\n");
	return NULL;
    }
    request->timeout = 5;
    request->retries = 2;
    request->callback = ata_raid_done;
    request->driver = rdp;
    request->bio = bio;
    switch (request->bio->bio_cmd) {
    case BIO_READ:
	request->flags = ATA_R_READ;
	break;
    case BIO_WRITE:
	request->flags = ATA_R_WRITE;
	break;
    }
    return request;
}

static int
ata_raid_send_request(struct ata_request *request)
{
    struct ata_device *atadev = device_get_softc(request->dev);
  
    request->transfersize = min(request->bytecount, atadev->max_iosize);
    if (request->flags & ATA_R_READ) {
	if (atadev->mode >= ATA_DMA) {
	    request->flags |= ATA_R_DMA;
	    request->u.ata.command = ATA_READ_DMA;
	}
	else if (atadev->max_iosize > DEV_BSIZE)
	    request->u.ata.command = ATA_READ_MUL;
	else
	    request->u.ata.command = ATA_READ;
    }
    else if (request->flags & ATA_R_WRITE) {
	if (atadev->mode >= ATA_DMA) {
	    request->flags |= ATA_R_DMA;
	    request->u.ata.command = ATA_WRITE_DMA;
	}
	else if (atadev->max_iosize > DEV_BSIZE)
	    request->u.ata.command = ATA_WRITE_MUL;
	else
	    request->u.ata.command = ATA_WRITE;
    }
    else {
	device_printf(request->dev, "FAILURE - unknown IO operation\n");
	ata_free_request(request);
	return EIO;
    }
    request->flags |= (ATA_R_ORDERED | ATA_R_THREAD);
    ata_queue_request(request);
    return 0;
}

static int
ata_raid_rw(device_t dev, u_int64_t lba, void *data, u_int bcount, int flags)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct ata_request *request;
    int error;

    if (bcount % DEV_BSIZE) {
	device_printf(dev, "FAILURE - transfers must be modulo sectorsize\n");
	return ENOMEM;
    }
	
    if (!(request = ata_alloc_request())) {
	device_printf(dev, "FAILURE - out of memory in ata_raid_rw\n");
	return ENOMEM;
    }

    /* setup request */
    request->dev = dev;
    request->timeout = 10;
    request->retries = 0;
    request->data = data;
    request->bytecount = bcount;
    request->transfersize = DEV_BSIZE;
    request->u.ata.lba = lba;
    request->u.ata.count = request->bytecount / DEV_BSIZE;
    request->flags = flags;

    if (flags & ATA_R_READ) {
	if (atadev->mode >= ATA_DMA) {
	    request->u.ata.command = ATA_READ_DMA;
	    request->flags |= ATA_R_DMA;
	}
	else
	    request->u.ata.command = ATA_READ;
	ata_queue_request(request);
    }
    else if (flags & ATA_R_WRITE) {
	if (atadev->mode >= ATA_DMA) {
	    request->u.ata.command = ATA_WRITE_DMA;
	    request->flags |= ATA_R_DMA;
	}
	else
	    request->u.ata.command = ATA_WRITE;
	ata_queue_request(request);
    }
    else {
	device_printf(dev, "FAILURE - unknown IO operation\n");
	request->result = EIO;
    }
    error = request->result;
    ata_free_request(request);
    return error;
}

/*
 * module handeling
 */
static int
ata_raid_subdisk_probe(device_t dev)
{
    device_quiet(dev);
    return 0;
}

static int
ata_raid_subdisk_attach(device_t dev)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);

    ars->raid = NULL;
    ars->disk_number = -1;
    ata_raid_read_metadata(dev);
    return 0;
}

static int
ata_raid_subdisk_detach(device_t dev)
{
    struct ata_raid_subdisk *ars = device_get_softc(dev);

    if (ars->raid) {
	ars->raid->disks[ars->disk_number].flags &= 
	    ~(AR_DF_PRESENT | AR_DF_ONLINE);
	ars->raid->disks[ars->disk_number].dev = NULL;
	ata_raid_config_changed(ars->raid, 1);
	ars->raid = NULL;
	ars->disk_number = -1;
    }
    return 0;
}

static device_method_t ata_raid_sub_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,     ata_raid_subdisk_probe),
    DEVMETHOD(device_attach,    ata_raid_subdisk_attach),
    DEVMETHOD(device_detach,    ata_raid_subdisk_detach),
    { 0, 0 }
};

static driver_t ata_raid_sub_driver = {
    "subdisk",
    ata_raid_sub_methods,
    sizeof(struct ata_raid_subdisk)
};

DRIVER_MODULE(subdisk, ad, ata_raid_sub_driver, ata_raid_sub_devclass, NULL, NULL);

static int
ata_raid_module_event_handler(module_t mod, int what, void *arg)
{
    int i;

    switch (what) {
    case MOD_LOAD:
	if (bootverbose)
	    printf("ATA PseudoRAID loaded\n");
#if 0
	/* setup table to hold metadata for all ATA PseudoRAID arrays */
	ata_raid_arrays = malloc(sizeof(struct ar_soft *) * MAX_ARRAYS,
				M_AR, M_NOWAIT | M_ZERO);
	if (!ata_raid_arrays) {
	    printf("ataraid: no memory for metadata storage\n");
	    return ENOMEM;
	}
#endif
	/* attach found PseudoRAID arrays */
	for (i = 0; i < MAX_ARRAYS; i++) {
	    struct ar_softc *rdp = ata_raid_arrays[i];
	    
	    if (!rdp || !rdp->format)
		continue;
	    if (testing || bootverbose)
		ata_raid_print_meta(rdp);
	    ata_raid_attach(rdp, 0);
	}   
	ata_raid_ioctl_func = ata_raid_ioctl;
	return 0;

    case MOD_UNLOAD:
	/* detach found PseudoRAID arrays */
	for (i = 0; i < MAX_ARRAYS; i++) {
	    struct ar_softc *rdp = ata_raid_arrays[i];

	    if (!rdp || !rdp->status)
		continue;
	    disk_destroy(rdp->disk);
	}
	if (bootverbose)
	    printf("ATA PseudoRAID unloaded\n");
#if 0
	free(ata_raid_arrays, M_AR);
#endif
	ata_raid_ioctl_func = NULL;
	return 0;
	
    default:
	return EOPNOTSUPP;
    }
}

static moduledata_t ata_raid_moduledata =
    { "ataraid", ata_raid_module_event_handler, NULL };
DECLARE_MODULE(ata, ata_raid_moduledata, SI_SUB_RAID, SI_ORDER_FIRST);
MODULE_VERSION(ataraid, 1);
MODULE_DEPEND(ataraid, ata, 1, 1, 1);
MODULE_DEPEND(ataraid, ad, 1, 1, 1);

static char *
ata_raid_format(struct ar_softc *rdp)
{
    switch (rdp->format) {
    case AR_F_FREEBSD_RAID:     return "FreeBSD PseudoRAID";
    case AR_F_ADAPTEC_RAID:     return "Adaptec HostRAID";
    case AR_F_HPTV2_RAID:       return "HighPoint v2 RocketRAID";
    case AR_F_HPTV3_RAID:       return "HighPoint v3 RocketRAID";
    case AR_F_INTEL_RAID:       return "Intel MatrixRAID";
    case AR_F_ITE_RAID:         return "Integrated Technology Express";
    case AR_F_LSIV2_RAID:       return "LSILogic v2 MegaRAID";
    case AR_F_LSIV3_RAID:       return "LSILogic v3 MegaRAID";
    case AR_F_NVIDIA_RAID:      return "nVidia MediaShield";
    case AR_F_PROMISE_RAID:     return "Promise Fasttrak";
    case AR_F_SII_RAID:         return "Silicon Image Medley";
    case AR_F_VIA_RAID:         return "VIA Tech V-RAID";
    default:                    return "UNKNOWN";
    }
}

static char *
ata_raid_type(struct ar_softc *rdp)
{
    switch (rdp->type) {
    case AR_T_JBOD:     return "JBOD";
    case AR_T_SPAN:     return "SPAN";
    case AR_T_RAID0:    return "RAID0";
    case AR_T_RAID1:    return "RAID1";
    case AR_T_RAID3:    return "RAID3";
    case AR_T_RAID4:    return "RAID4";
    case AR_T_RAID5:    return "RAID5";
    case AR_T_RAID01:   return "RAID0+1";
    default:            return "UNKNOWN";
    }
}

static char *
ata_raid_flags(struct ar_softc *rdp)
{
    switch (rdp->status & (AR_S_READY | AR_S_DEGRADED | AR_S_REBUILDING)) {
    case AR_S_READY:                                    return "READY";
    case AR_S_READY | AR_S_DEGRADED:                    return "DEGRADED";
    case AR_S_READY | AR_S_REBUILDING:
    case AR_S_READY | AR_S_DEGRADED | AR_S_REBUILDING:  return "REBUILDING";
    default:                                            return "BROKEN";
    }
}

/* debugging gunk */
static void
ata_raid_print_meta(struct ar_softc *raid)
{
    int i;

    printf("********** ATA PseudoRAID ar%d Metadata **********\n", raid->lun);
    printf("=================================================\n");
    printf("format              %s\n", ata_raid_format(raid));
    printf("type                %s\n", ata_raid_type(raid));
    printf("flags               0x%02x %b\n", raid->status, raid->status,
	   "\20\3REBUILDING\2DEGRADED\1READY\n");
    printf("magic_0             0x%016llx\n",(unsigned long long)raid->magic_0);
    printf("magic_1             0x%016llx\n",(unsigned long long)raid->magic_1);
    printf("generation          %u\n", raid->generation);
    printf("total_sectors       %llu\n",
	   (unsigned long long)raid->total_sectors);
    printf("offset_sectors      %llu\n",
	   (unsigned long long)raid->offset_sectors);
    printf("heads               %u\n", raid->heads);
    printf("sectors             %u\n", raid->sectors);
    printf("cylinders           %u\n", raid->cylinders);
    printf("width               %u\n", raid->width);
    printf("interleave          %u\n", raid->interleave);
    printf("total_disks         %u\n", raid->total_disks);
    for (i = 0; i < raid->total_disks; i++) {
	printf("    disk %d:      flags = 0x%02x %b\n", i, raid->disks[i].flags,
	       raid->disks[i].flags, "\20\4ONLINE\3SPARE\2ASSIGNED\1PRESENT\n");
	if (raid->disks[i].dev) {
	    printf("        ");
	    device_printf(raid->disks[i].dev, " sectors %lld\n",
			  (long long)raid->disks[i].sectors);
	}
    }
    printf("=================================================\n");
}

static char *
ata_raid_adaptec_type(int type)
{
    static char buffer[16];

    switch (type) {
    case ADP_T_RAID0:   return "RAID0";
    case ADP_T_RAID1:   return "RAID1";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_adaptec_print_meta(struct adaptec_raid_conf *meta)
{
    int i;

    printf("********* ATA Adaptec HostRAID Metadata *********\n");
    printf("magic_0             <0x%08x>\n", be32toh(meta->magic_0));
    printf("generation          0x%08x\n", be32toh(meta->generation));
    printf("dummy_0             0x%04x\n", be16toh(meta->dummy_0));
    printf("total_configs       %u\n", be16toh(meta->total_configs));
    printf("dummy_1             0x%04x\n", be16toh(meta->dummy_1));
    printf("checksum            0x%04x\n", be16toh(meta->checksum));
    printf("dummy_2             0x%08x\n", be32toh(meta->dummy_2));
    printf("dummy_3             0x%08x\n", be32toh(meta->dummy_3));
    printf("flags               0x%08x\n", be32toh(meta->flags));
    printf("timestamp           0x%08x\n", be32toh(meta->timestamp));
    printf("dummy_4             0x%08x 0x%08x 0x%08x 0x%08x\n",
	   be32toh(meta->dummy_4[0]), be32toh(meta->dummy_4[1]),
	   be32toh(meta->dummy_4[2]), be32toh(meta->dummy_4[3]));
    printf("dummy_5             0x%08x 0x%08x 0x%08x 0x%08x\n",
	   be32toh(meta->dummy_5[0]), be32toh(meta->dummy_5[1]),
	   be32toh(meta->dummy_5[2]), be32toh(meta->dummy_5[3]));

    for (i = 0; i < be16toh(meta->total_configs); i++) {
	printf("    %d   total_disks  %u\n", i,
	       be16toh(meta->configs[i].disk_number));
	printf("    %d   generation   %u\n", i,
	       be16toh(meta->configs[i].generation));
	printf("    %d   magic_0      0x%08x\n", i,
	       be32toh(meta->configs[i].magic_0));
	printf("    %d   dummy_0      0x%02x\n", i, meta->configs[i].dummy_0);
	printf("    %d   type         %s\n", i,
	       ata_raid_adaptec_type(meta->configs[i].type));
	printf("    %d   dummy_1      0x%02x\n", i, meta->configs[i].dummy_1);
	printf("    %d   flags        %d\n", i,
	       be32toh(meta->configs[i].flags));
	printf("    %d   dummy_2      0x%02x\n", i, meta->configs[i].dummy_2);
	printf("    %d   dummy_3      0x%02x\n", i, meta->configs[i].dummy_3);
	printf("    %d   dummy_4      0x%02x\n", i, meta->configs[i].dummy_4);
	printf("    %d   dummy_5      0x%02x\n", i, meta->configs[i].dummy_5);
	printf("    %d   disk_number  %u\n", i,
	       be32toh(meta->configs[i].disk_number));
	printf("    %d   dummy_6      0x%08x\n", i,
	       be32toh(meta->configs[i].dummy_6));
	printf("    %d   sectors      %u\n", i,
	       be32toh(meta->configs[i].sectors));
	printf("    %d   stripe_shift %u\n", i,
	       be16toh(meta->configs[i].stripe_shift));
	printf("    %d   dummy_7      0x%08x\n", i,
	       be32toh(meta->configs[i].dummy_7));
	printf("    %d   dummy_8      0x%08x 0x%08x 0x%08x 0x%08x\n", i,
	       be32toh(meta->configs[i].dummy_8[0]),
	       be32toh(meta->configs[i].dummy_8[1]),
	       be32toh(meta->configs[i].dummy_8[2]),
	       be32toh(meta->configs[i].dummy_8[3]));
	printf("    %d   name         <%s>\n", i, meta->configs[i].name);
    }
    printf("magic_1             <0x%08x>\n", be32toh(meta->magic_1));
    printf("magic_2             <0x%08x>\n", be32toh(meta->magic_2));
    printf("magic_3             <0x%08x>\n", be32toh(meta->magic_3));
    printf("magic_4             <0x%08x>\n", be32toh(meta->magic_4));
    printf("=================================================\n");
}

static char *
ata_raid_hptv2_type(int type)
{
    static char buffer[16];

    switch (type) {
    case HPTV2_T_RAID0:         return "RAID0";
    case HPTV2_T_RAID1:         return "RAID1";
    case HPTV2_T_RAID01_RAID0:  return "RAID01_RAID0";
    case HPTV2_T_SPAN:          return "SPAN";
    case HPTV2_T_RAID_3:        return "RAID3";
    case HPTV2_T_RAID_5:        return "RAID5";
    case HPTV2_T_JBOD:          return "JBOD";
    case HPTV2_T_RAID01_RAID1:  return "RAID01_RAID1";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_hptv2_print_meta(struct hptv2_raid_conf *meta)
{
    int i;

    printf("****** ATA Highpoint V2 RocketRAID Metadata *****\n");
    printf("magic               0x%08x\n", meta->magic);
    printf("magic_0             0x%08x\n", meta->magic_0);
    printf("magic_1             0x%08x\n", meta->magic_1);
    printf("order               0x%08x\n", meta->order);
    printf("array_width         %u\n", meta->array_width);
    printf("stripe_shift        %u\n", meta->stripe_shift);
    printf("type                %s\n", ata_raid_hptv2_type(meta->type));
    printf("disk_number         %u\n", meta->disk_number);
    printf("total_sectors       %u\n", meta->total_sectors);
    printf("disk_mode           0x%08x\n", meta->disk_mode);
    printf("boot_mode           0x%08x\n", meta->boot_mode);
    printf("boot_disk           0x%02x\n", meta->boot_disk);
    printf("boot_protect        0x%02x\n", meta->boot_protect);
    printf("log_entries         0x%02x\n", meta->error_log_entries);
    printf("log_index           0x%02x\n", meta->error_log_index);
    if (meta->error_log_entries) {
	printf("    timestamp  reason disk  status  sectors lba\n");
	for (i = meta->error_log_index;
	     i < meta->error_log_index + meta->error_log_entries; i++)
	    printf("    0x%08x  0x%02x  0x%02x  0x%02x    0x%02x    0x%08x\n",
		   meta->errorlog[i%32].timestamp,
		   meta->errorlog[i%32].reason,
		   meta->errorlog[i%32].disk, meta->errorlog[i%32].status,
		   meta->errorlog[i%32].sectors, meta->errorlog[i%32].lba);
    }
    printf("rebuild_lba         0x%08x\n", meta->rebuild_lba);
    printf("dummy_1             0x%02x\n", meta->dummy_1);
    printf("name_1              <%.15s>\n", meta->name_1);
    printf("dummy_2             0x%02x\n", meta->dummy_2);
    printf("name_2              <%.15s>\n", meta->name_2);
    printf("=================================================\n");
}

static char *
ata_raid_hptv3_type(int type)
{
    static char buffer[16];

    switch (type) {
    case HPTV3_T_SPARE: return "SPARE";
    case HPTV3_T_JBOD:  return "JBOD";
    case HPTV3_T_SPAN:  return "SPAN";
    case HPTV3_T_RAID0: return "RAID0";
    case HPTV3_T_RAID1: return "RAID1";
    case HPTV3_T_RAID3: return "RAID3";
    case HPTV3_T_RAID5: return "RAID5";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_hptv3_print_meta(struct hptv3_raid_conf *meta)
{
    int i;

    printf("****** ATA Highpoint V3 RocketRAID Metadata *****\n");
    printf("magic               0x%08x\n", meta->magic);
    printf("magic_0             0x%08x\n", meta->magic_0);
    printf("checksum_0          0x%02x\n", meta->checksum_0);
    printf("mode                0x%02x\n", meta->mode);
    printf("user_mode           0x%02x\n", meta->user_mode);
    printf("config_entries      0x%02x\n", meta->config_entries);
    for (i = 0; i < meta->config_entries; i++) {
	printf("config %d:\n", i);
	printf("    total_sectors       %llu\n",
	       (unsigned long long)(meta->configs[0].total_sectors +
	       ((u_int64_t)meta->configs_high[0].total_sectors << 32)));
	printf("    type                %s\n",
	       ata_raid_hptv3_type(meta->configs[i].type)); 
	printf("    total_disks         %u\n", meta->configs[i].total_disks);
	printf("    disk_number         %u\n", meta->configs[i].disk_number);
	printf("    stripe_shift        %u\n", meta->configs[i].stripe_shift);
	printf("    status              %b\n", meta->configs[i].status,
	       "\20\2RAID5\1NEED_REBUILD\n");
	printf("    critical_disks      %u\n", meta->configs[i].critical_disks);
	printf("    rebuild_lba         %llu\n",
	       (unsigned long long)(meta->configs_high[0].rebuild_lba +
	       ((u_int64_t)meta->configs_high[0].rebuild_lba << 32)));
    }
    printf("name                <%.16s>\n", meta->name);
    printf("timestamp           0x%08x\n", meta->timestamp);
    printf("description         <%.16s>\n", meta->description);
    printf("creator             <%.16s>\n", meta->creator);
    printf("checksum_1          0x%02x\n", meta->checksum_1);
    printf("dummy_0             0x%02x\n", meta->dummy_0);
    printf("dummy_1             0x%02x\n", meta->dummy_1);
    printf("flags               %b\n", meta->flags,
	   "\20\4RCACHE\3WCACHE\2NCQ\1TCQ\n");
    printf("=================================================\n");
}

static char *
ata_raid_intel_type(int type)
{
    static char buffer[16];

    switch (type) {
    case INTEL_T_RAID0: return "RAID0";
    case INTEL_T_RAID1: return "RAID1";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_intel_print_meta(struct intel_raid_conf *meta)
{
    struct intel_raid_mapping *map;
    int i, j;

    printf("********* ATA Intel MatrixRAID Metadata *********\n");
    printf("intel_id            <%.24s>\n", meta->intel_id);
    printf("version             <%.6s>\n", meta->version);
    printf("checksum            0x%08x\n", meta->checksum);
    printf("config_size         0x%08x\n", meta->config_size);
    printf("config_id           0x%08x\n", meta->config_id);
    printf("generation          0x%08x\n", meta->generation);
    printf("total_disks         %u\n", meta->total_disks);
    printf("total_volumes       %u\n", meta->total_volumes);
    printf("DISK#   serial disk_sectors disk_id flags\n");
    for (i = 0; i < meta->total_disks; i++ ) {
	printf("    %d   <%.16s> %u 0x%08x 0x%08x\n", i,
	       meta->disk[i].serial, meta->disk[i].sectors,
	       meta->disk[i].id, meta->disk[i].flags);
    }
    map = (struct intel_raid_mapping *)&meta->disk[meta->total_disks];
    for (j = 0; j < meta->total_volumes; j++) {
	printf("name                %.16s\n", map->name);
	printf("total_sectors       %llu\n",
	       (unsigned long long)map->total_sectors);
	printf("state               %u\n", map->state);
	printf("reserved            %u\n", map->reserved);
	printf("offset              %u\n", map->offset);
	printf("disk_sectors        %u\n", map->disk_sectors);
	printf("stripe_count        %u\n", map->stripe_count);
	printf("stripe_sectors      %u\n", map->stripe_sectors);
	printf("status              %u\n", map->status);
	printf("type                %s\n", ata_raid_intel_type(map->type));
	printf("total_disks         %u\n", map->total_disks);
	for (i = 0; i < map->total_disks; i++ ) {
	    printf("    disk %d at disk_idx 0x%08x\n", i, map->disk_idx[i]);
	}
	map = (struct intel_raid_mapping *)&map->disk_idx[i];
    }
    printf("=================================================\n");
}

static char *
ata_raid_ite_type(int type)
{
    static char buffer[16];

    switch (type) {
    case ITE_T_RAID0:   return "RAID0";
    case ITE_T_RAID1:   return "RAID1";
    case ITE_T_RAID01:  return "RAID0+1";
    case ITE_T_SPAN:    return "SPAN";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_ite_print_meta(struct ite_raid_conf *meta)
{
    printf("*** ATA Integrated Technology Express Metadata **\n");
    printf("ite_id              <%.40s>\n", meta->ite_id);
    printf("timestamp_0         %04x/%02x/%02x %02x:%02x:%02x.%02x\n",
	   *((u_int16_t *)meta->timestamp_0), meta->timestamp_0[2],
	   meta->timestamp_0[3], meta->timestamp_0[5], meta->timestamp_0[4],
	   meta->timestamp_0[7], meta->timestamp_0[6]);
    printf("total_sectors       %lld\n",
	   (unsigned long long)meta->total_sectors);
    printf("type                %s\n", ata_raid_ite_type(meta->type));
    printf("stripe_1kblocks     %u\n", meta->stripe_1kblocks);
    printf("timestamp_1         %04x/%02x/%02x %02x:%02x:%02x.%02x\n",
	   *((u_int16_t *)meta->timestamp_1), meta->timestamp_1[2],
	   meta->timestamp_1[3], meta->timestamp_1[5], meta->timestamp_1[4],
	   meta->timestamp_1[7], meta->timestamp_1[6]);
    printf("stripe_sectors      %u\n", meta->stripe_sectors);
    printf("array_width         %u\n", meta->array_width);
    printf("disk_number         %u\n", meta->disk_number);
    printf("disk_sectors        %u\n", meta->disk_sectors);
    printf("=================================================\n");
}

static char *
ata_raid_lsiv2_type(int type)
{
    static char buffer[16];

    switch (type) {
    case LSIV2_T_RAID0: return "RAID0";
    case LSIV2_T_RAID1: return "RAID1";
    case LSIV2_T_SPARE: return "SPARE";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_lsiv2_print_meta(struct lsiv2_raid_conf *meta)
{
    int i;

    printf("******* ATA LSILogic V2 MegaRAID Metadata *******\n");
    printf("lsi_id              <%s>\n", meta->lsi_id);
    printf("dummy_0             0x%02x\n", meta->dummy_0);
    printf("flags               0x%02x\n", meta->flags);
    printf("version             0x%04x\n", meta->version);
    printf("config_entries      0x%02x\n", meta->config_entries);
    printf("raid_count          0x%02x\n", meta->raid_count);
    printf("total_disks         0x%02x\n", meta->total_disks);
    printf("dummy_1             0x%02x\n", meta->dummy_1);
    printf("dummy_2             0x%04x\n", meta->dummy_2);
    for (i = 0; i < meta->config_entries; i++) {
	printf("    type             %s\n",
	       ata_raid_lsiv2_type(meta->configs[i].raid.type));
	printf("    dummy_0          %02x\n", meta->configs[i].raid.dummy_0);
	printf("    stripe_sectors   %u\n",
	       meta->configs[i].raid.stripe_sectors);
	printf("    array_width      %u\n",
	       meta->configs[i].raid.array_width);
	printf("    disk_count       %u\n", meta->configs[i].raid.disk_count);
	printf("    config_offset    %u\n",
	       meta->configs[i].raid.config_offset);
	printf("    dummy_1          %u\n", meta->configs[i].raid.dummy_1);
	printf("    flags            %02x\n", meta->configs[i].raid.flags);
	printf("    total_sectors    %u\n",
	       meta->configs[i].raid.total_sectors);
    }
    printf("disk_number         0x%02x\n", meta->disk_number);
    printf("raid_number         0x%02x\n", meta->raid_number);
    printf("timestamp           0x%08x\n", meta->timestamp);
    printf("=================================================\n");
}

static char *
ata_raid_lsiv3_type(int type)
{
    static char buffer[16];

    switch (type) {
    case LSIV3_T_RAID0: return "RAID0";
    case LSIV3_T_RAID1: return "RAID1";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_lsiv3_print_meta(struct lsiv3_raid_conf *meta)
{
    int i;

    printf("******* ATA LSILogic V3 MegaRAID Metadata *******\n");
    printf("lsi_id              <%.6s>\n", meta->lsi_id);
    printf("dummy_0             0x%04x\n", meta->dummy_0);
    printf("version             0x%04x\n", meta->version);
    printf("dummy_0             0x%04x\n", meta->dummy_1);
    printf("RAID configs:\n");
    for (i = 0; i < 8; i++) {
	if (meta->raid[i].total_disks) {
	    printf("%02d  stripe_pages       %u\n", i,
		   meta->raid[i].stripe_pages);
	    printf("%02d  type               %s\n", i,
		   ata_raid_lsiv3_type(meta->raid[i].type));
	    printf("%02d  total_disks        %u\n", i,
		   meta->raid[i].total_disks);
	    printf("%02d  array_width        %u\n", i,
		   meta->raid[i].array_width);
	    printf("%02d  sectors            %u\n", i, meta->raid[i].sectors);
	    printf("%02d  offset             %u\n", i, meta->raid[i].offset);
	    printf("%02d  device             0x%02x\n", i,
		   meta->raid[i].device);
	}
    }
    printf("DISK configs:\n");
    for (i = 0; i < 6; i++) {
	    if (meta->disk[i].disk_sectors) {
	    printf("%02d  disk_sectors       %u\n", i,
		   meta->disk[i].disk_sectors);
	    printf("%02d  flags              0x%02x\n", i, meta->disk[i].flags);
	}
    }
    printf("device              0x%02x\n", meta->device);
    printf("timestamp           0x%08x\n", meta->timestamp);
    printf("checksum_1          0x%02x\n", meta->checksum_1);
    printf("=================================================\n");
}

static char *
ata_raid_nvidia_type(int type)
{
    static char buffer[16];

    switch (type) {
    case NV_T_SPAN:     return "SPAN";
    case NV_T_RAID0:    return "RAID0";
    case NV_T_RAID1:    return "RAID1";
    case NV_T_RAID3:    return "RAID3";
    case NV_T_RAID5:    return "RAID5";
    case NV_T_RAID01:   return "RAID0+1";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_nvidia_print_meta(struct nvidia_raid_conf *meta)
{
    printf("******** ATA nVidia MediaShield Metadata ********\n");
    printf("nvidia_id           <%.8s>\n", meta->nvidia_id);
    printf("config_size         %d\n", meta->config_size);
    printf("checksum            0x%08x\n", meta->checksum);
    printf("version             0x%04x\n", meta->version);
    printf("disk_number         %d\n", meta->disk_number);
    printf("dummy_0             0x%02x\n", meta->dummy_0);
    printf("total_sectors       %d\n", meta->total_sectors);
    printf("sectors_size        %d\n", meta->sector_size);
    printf("serial              %.16s\n", meta->serial);
    printf("revision            %.4s\n", meta->revision);
    printf("dummy_1             0x%08x\n", meta->dummy_1);
    printf("magic_0             0x%08x\n", meta->magic_0);
    printf("magic_1             0x%016llx\n",(unsigned long long)meta->magic_1);
    printf("magic_2             0x%016llx\n",(unsigned long long)meta->magic_2);
    printf("flags               0x%02x\n", meta->flags);
    printf("array_width         %d\n", meta->array_width);
    printf("total_disks         %d\n", meta->total_disks);
    printf("dummy_2             0x%02x\n", meta->dummy_2);
    printf("type                %s\n", ata_raid_nvidia_type(meta->type));
    printf("dummy_3             0x%04x\n", meta->dummy_3);
    printf("stripe_sectors      %d\n", meta->stripe_sectors);
    printf("stripe_bytes        %d\n", meta->stripe_bytes);
    printf("stripe_shift        %d\n", meta->stripe_shift);
    printf("stripe_mask         0x%08x\n", meta->stripe_mask);
    printf("stripe_sizesectors  %d\n", meta->stripe_sizesectors);
    printf("stripe_sizebytes    %d\n", meta->stripe_sizebytes);
    printf("rebuild_lba         %d\n", meta->rebuild_lba);
    printf("dummy_4             0x%08x\n", meta->dummy_4);
    printf("dummy_5             0x%08x\n", meta->dummy_5);
    printf("status              0x%08x\n", meta->status);
    printf("=================================================\n");
}

static char *
ata_raid_promise_type(int type)
{
    static char buffer[16];

    switch (type) {
    case PR_T_RAID0:    return "RAID0";
    case PR_T_RAID1:    return "RAID1";
    case PR_T_RAID3:    return "RAID3";
    case PR_T_RAID5:    return "RAID5";
    case PR_T_SPAN:     return "SPAN";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_promise_print_meta(struct promise_raid_conf *meta)
{
    int i;

    printf("********* ATA Promise FastTrak Metadata *********\n");
    printf("promise_id          <%s>\n", meta->promise_id);
    printf("dummy_0             0x%08x\n", meta->dummy_0);
    printf("magic_0             0x%016llx\n",(unsigned long long)meta->magic_0);
    printf("magic_1             0x%04x\n", meta->magic_1);
    printf("magic_2             0x%08x\n", meta->magic_2);
    printf("integrity           0x%08x %b\n", meta->raid.integrity,
		meta->raid.integrity, "\20\10VALID\n" );
    printf("flags               0x%02x %b\n",
	   meta->raid.flags, meta->raid.flags,
	   "\20\10READY\7DOWN\6REDIR\5DUPLICATE\4SPARE"
	   "\3ASSIGNED\2ONLINE\1VALID\n");
    printf("disk_number         %d\n", meta->raid.disk_number);
    printf("channel             0x%02x\n", meta->raid.channel);
    printf("device              0x%02x\n", meta->raid.device);
    printf("magic_0             0x%016llx\n",
	   (unsigned long long)meta->raid.magic_0);
    printf("disk_offset         %u\n", meta->raid.disk_offset);
    printf("disk_sectors        %u\n", meta->raid.disk_sectors);
    printf("rebuild_lba         0x%08x\n", meta->raid.rebuild_lba);
    printf("generation          0x%04x\n", meta->raid.generation);
    printf("status              0x%02x %b\n",
	    meta->raid.status, meta->raid.status,
	   "\20\6MARKED\5DEGRADED\4READY\3INITED\2ONLINE\1VALID\n");
    printf("type                %s\n", ata_raid_promise_type(meta->raid.type));
    printf("total_disks         %u\n", meta->raid.total_disks);
    printf("stripe_shift        %u\n", meta->raid.stripe_shift);
    printf("array_width         %u\n", meta->raid.array_width);
    printf("array_number        %u\n", meta->raid.array_number);
    printf("total_sectors       %u\n", meta->raid.total_sectors);
    printf("cylinders           %u\n", meta->raid.cylinders);
    printf("heads               %u\n", meta->raid.heads);
    printf("sectors             %u\n", meta->raid.sectors);
    printf("magic_1             0x%016llx\n",
	   (unsigned long long)meta->raid.magic_1);
    printf("DISK#   flags dummy_0 channel device  magic_0\n");
    for (i = 0; i < 8; i++) {
	printf("  %d    %b    0x%02x  0x%02x  0x%02x  ",
	       i, meta->raid.disk[i].flags,
	       "\20\10READY\7DOWN\6REDIR\5DUPLICATE\4SPARE"
	       "\3ASSIGNED\2ONLINE\1VALID\n", meta->raid.disk[i].dummy_0,
	       meta->raid.disk[i].channel, meta->raid.disk[i].device);
	printf("0x%016llx\n",
	       (unsigned long long)meta->raid.disk[i].magic_0);
    }
    printf("checksum            0x%08x\n", meta->checksum);
    printf("=================================================\n");
}

static char *
ata_raid_sii_type(int type)
{
    static char buffer[16];

    switch (type) {
    case SII_T_RAID0:   return "RAID0";
    case SII_T_RAID1:   return "RAID1";
    case SII_T_RAID01:  return "RAID0+1";
    case SII_T_SPARE:   return "SPARE";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_sii_print_meta(struct sii_raid_conf *meta)
{
    printf("******* ATA Silicon Image Medley Metadata *******\n");
    printf("total_sectors       %llu\n",
	   (unsigned long long)meta->total_sectors);
    printf("dummy_0             0x%04x\n", meta->dummy_0);
    printf("dummy_1             0x%04x\n", meta->dummy_1);
    printf("controller_pci_id   0x%08x\n", meta->controller_pci_id);
    printf("version_minor       0x%04x\n", meta->version_minor);
    printf("version_major       0x%04x\n", meta->version_major);
    printf("timestamp           20%02x/%02x/%02x %02x:%02x:%02x\n",
	   meta->timestamp[5], meta->timestamp[4], meta->timestamp[3],
	   meta->timestamp[2], meta->timestamp[1], meta->timestamp[0]);
    printf("stripe_sectors      %u\n", meta->stripe_sectors);
    printf("dummy_2             0x%04x\n", meta->dummy_2);
    printf("disk_number         %u\n", meta->disk_number);
    printf("type                %s\n", ata_raid_sii_type(meta->type));
    printf("raid0_disks         %u\n", meta->raid0_disks);
    printf("raid0_ident         %u\n", meta->raid0_ident);
    printf("raid1_disks         %u\n", meta->raid1_disks);
    printf("raid1_ident         %u\n", meta->raid1_ident);
    printf("rebuild_lba         %llu\n", (unsigned long long)meta->rebuild_lba);
    printf("generation          0x%08x\n", meta->generation);
    printf("status              0x%02x %b\n",
	    meta->status, meta->status,
	   "\20\1READY\n");
    printf("base_raid1_position %02x\n", meta->base_raid1_position);
    printf("base_raid0_position %02x\n", meta->base_raid0_position);
    printf("position            %02x\n", meta->position);
    printf("dummy_3             %04x\n", meta->dummy_3);
    printf("name                <%.16s>\n", meta->name);
    printf("checksum_0          0x%04x\n", meta->checksum_0);
    printf("checksum_1          0x%04x\n", meta->checksum_1);
    printf("=================================================\n");
}

static char *
ata_raid_via_type(int type)
{
    static char buffer[16];

    switch (type) {
    case VIA_T_RAID0:   return "RAID0";
    case VIA_T_RAID1:   return "RAID1";
    case VIA_T_SPAN:    return "SPAN";
    default:            sprintf(buffer, "UNKNOWN 0x%02x", type);
			return buffer;
    }
}

static void
ata_raid_via_print_meta(struct via_raid_conf *meta)
{
    int i;
  
    printf("*************** ATA VIA Metadata ****************\n");
    printf("magic               0x%02x\n", meta->magic);
    printf("dummy_0             0x%02x\n", meta->dummy_0);
    printf("type                %s\n",
	   ata_raid_via_type(meta->type & VIA_T_MASK));
    printf("bootable            %d\n", meta->type & VIA_T_BOOTABLE);
    printf("disk_index          0x%02x\n", meta->disk_index);
    printf("stripe_disks        %d\n", meta->stripe_layout & VIA_L_MASK);
    printf("stripe_sectors      %d\n",
	   0x08 << (meta->stripe_layout >> VIA_L_SHIFT));
    printf("total_sectors       %llu\n",
	   (unsigned long long)meta->total_sectors);
    printf("disk_id             0x%08x\n", meta->disk_id);
    printf("DISK#   disk_id\n");
    for (i = 0; i < 8; i++) {
	if (meta->disks[i])
	    printf("  %d    0x%08x\n", i, meta->disks[i]);
    }    
    printf("checksum            0x%02x\n", meta->checksum);
    printf("=================================================\n");
}
