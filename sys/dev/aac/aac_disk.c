/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2001 Scott Long
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001 Adaptec, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include "opt_aac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <dev/aac/aac_compat.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <dev/aac/aacreg.h>
#include <dev/aac/aac_ioctl.h>
#include <dev/aac/aacvar.h>

/*
 * Interface to parent.
 */
static int aac_disk_probe(device_t dev);
static int aac_disk_attach(device_t dev);
static int aac_disk_detach(device_t dev);

/*
 * Interface to the device switch.
 */
static	d_open_t	aac_disk_open;
static	d_close_t	aac_disk_close;
static	d_strategy_t	aac_disk_strategy;
static	d_dump_t	aac_disk_dump;

#define AAC_DISK_CDEV_MAJOR	151

static struct cdevsw aac_disk_cdevsw = {
	/* open */		aac_disk_open,
	/* close */		aac_disk_close,
	/* read */		physread,
	/* write */		physwrite,
	/* ioctl */		noioctl,
	/* poll */		nopoll,
	/* mmap */		nommap,
	/* strategy */		aac_disk_strategy,
	/* name */ 		"aacd",
	/* maj */		AAC_DISK_CDEV_MAJOR,
	/* dump */		aac_disk_dump,
	/* psize */ 		nopsize,
	/* flags */		D_DISK,
#if __FreeBSD_version < 500005
	/* bmaj */		-1
#endif
};

static devclass_t	aac_disk_devclass;
static struct cdevsw	aac_disk_disk_cdevsw;
#ifdef FREEBSD_4
static int		disks_registered = 0;
#endif

static device_method_t aac_disk_methods[] = {
	DEVMETHOD(device_probe,	aac_disk_probe),
	DEVMETHOD(device_attach,	aac_disk_attach),
	DEVMETHOD(device_detach,	aac_disk_detach),
	{ 0, 0 }
};

static driver_t aac_disk_driver = {
	"aacd",
	aac_disk_methods,
	sizeof(struct aac_disk)
};

#define AAC_MAXIO	65536

DRIVER_MODULE(aacd, aac, aac_disk_driver, aac_disk_devclass, 0, 0);

/* sysctl tunables */
static unsigned int aac_iosize_max = AAC_MAXIO;	/* due to limits of the card */
TUNABLE_INT("hw.aac.iosize_max", &aac_iosize_max);

SYSCTL_DECL(_hw_aac);
SYSCTL_UINT(_hw_aac, OID_AUTO, iosize_max, CTLFLAG_RD, &aac_iosize_max, 0,
	    "Max I/O size per transfer to an array");

/*
 * Handle open from generic layer.
 *
 * This is called by the diskslice code on first open in order to get the 
 * basic device geometry paramters.
 */
static int
aac_disk_open(dev_t dev, int flags, int fmt, d_thread_t *td)
{
	struct aac_disk	*sc;
	struct disklabel *label;

	debug_called(4);

	sc = (struct aac_disk *)dev->si_drv1;
	
	if (sc == NULL)
		return (ENXIO);

	/* check that the controller is up and running */
	if (sc->ad_controller->aac_state & AAC_STATE_SUSPEND)
		return(ENXIO);

	/* build synthetic label */
	label = &sc->ad_disk.d_label;
	bzero(label, sizeof(*label));
	label->d_type = DTYPE_ESDI;
	label->d_secsize	= AAC_BLOCK_SIZE;
	label->d_nsectors   = sc->ad_sectors;
	label->d_ntracks	= sc->ad_heads;
	label->d_ncylinders = sc->ad_cylinders;
	label->d_secpercyl  = sc->ad_sectors * sc->ad_heads;
	label->d_secperunit = sc->ad_size;

	sc->ad_flags |= AAC_DISK_OPEN;
	return (0);
}

/*
 * Handle last close of the disk device.
 */
static int
aac_disk_close(dev_t dev, int flags, int fmt, d_thread_t *td)
{
	struct aac_disk	*sc;

	debug_called(4);

	sc = (struct aac_disk *)dev->si_drv1;
	
	if (sc == NULL)
		return (ENXIO);

	sc->ad_flags &= ~AAC_DISK_OPEN;
	return (0);
}

/*
 * Handle an I/O request.
 */
static void
aac_disk_strategy(struct bio *bp)
{
	struct aac_disk	*sc;

	debug_called(4);

	sc = (struct aac_disk *)bp->bio_dev->si_drv1;

	/* bogus disk? */
	if (sc == NULL) {
		bp->bio_flags |= BIO_ERROR;
		bp->bio_error = EINVAL;
		biodone(bp);
		return;
	}

	/* do-nothing operation? */
	if (bp->bio_bcount == 0) {
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
		return;
	}

	/* perform accounting */
	devstat_start_transaction(&sc->ad_stats);

	/* pass the bio to the controller - it can work out who we are */
	aac_submit_bio(bp);
	return;
}

/*
 * Map the S/G elements for doing a dump.
 */
static void
aac_dump_map_sg(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct aac_fib *fib;
	struct aac_blockwrite *bw;
	struct aac_sg_table *sg;
	int i;

	fib = (struct aac_fib *)arg;
	bw = (struct aac_blockwrite *)&fib->data[0];
	sg = &bw->SgMap;

	if (sg != NULL) {
		sg->SgCount = nsegs;
		for (i = 0; i < nsegs; i++) {
			sg->SgEntry[i].SgAddress = segs[i].ds_addr;
			sg->SgEntry[i].SgByteCount = segs[i].ds_len;
		}
		fib->Header.Size = nsegs * sizeof(struct aac_sg_entry);
	}
}

/*
 * Dump memory out to an array
 *
 * Send out one command at a time with up to AAC_MAXIO of data.
 */
static int
aac_disk_dump(dev_t dev, void *virtual, vm_offset_t physical, off_t offset, size_t length)
{
	struct aac_disk *ad;
	struct aac_softc *sc;
	struct aac_fib *fib;
	struct aac_blockwrite *bw;
	size_t len;
	int size;
	static bus_dmamap_t dump_datamap;
	static int first = 0;

	ad = dev->si_drv1;

	if (ad == NULL)
		return (EINVAL);

	sc= ad->ad_controller;

	if (!first) {
		first = 1;
		if (bus_dmamap_create(sc->aac_buffer_dmat, 0, &dump_datamap)) {
			printf("bus_dmamap_create failed\n");
			return (ENOMEM);
		}
	}

	aac_alloc_sync_fib(sc, &fib, AAC_SYNC_LOCK_FORCE);
	bw = (struct aac_blockwrite *)&fib->data[0];

	while (length > 0) {
		len = (length > AAC_MAXIO) ? AAC_MAXIO : length;
		bw->Command = VM_CtBlockWrite;
		bw->ContainerId = ad->ad_container->co_mntobj.ObjectId;
		bw->BlockNumber = offset / AAC_BLOCK_SIZE;
		bw->ByteCount = len;
		bw->Stable = CUNSTABLE;
		bus_dmamap_load(sc->aac_buffer_dmat, dump_datamap, virtual,
		    len, aac_dump_map_sg, fib, 0);
		bus_dmamap_sync(sc->aac_buffer_dmat, dump_datamap,
		    BUS_DMASYNC_PREWRITE);

		/* fib->Header.Size is set in aac_dump_map_sg */
		size = fib->Header.Size + sizeof(struct aac_blockwrite);

		if (aac_sync_fib(sc, ContainerCommand, 0, fib, size)) {
			printf("Error dumping block 0x%x\n", physical);
			return (EIO);
		}
		length -= len;
		offset += len;
	}

	return (0);
}

/*
 * Handle completion of an I/O request.
 */
void
aac_biodone(struct bio *bp)
{
	struct aac_disk	*sc;

	debug_called(4);

	sc = (struct aac_disk *)bp->bio_dev->si_drv1;

	devstat_end_transaction_bio(&sc->ad_stats, bp);
	if (bp->bio_flags & BIO_ERROR) {
#if __FreeBSD_version > 500039
		disk_err(bp, "hard error", -1, 1);
#elif __FreeBSD_version > 500005
		int blkno;
		blkno = (sc->ad_label.d_nsectors) ? 0 : -1;
		diskerr(bp, (char *)bp->bio_driver1, blkno, &sc->ad_label);
#else
		int blkno;
		blkno = (sc->ad_label.d_nsectors) ? 0 : -1;
		diskerr(bp, (char *)bp->bio_driver1, 0, blkno, &sc->ad_label);
#endif
	}
	biodone(bp);
}

/*
 * Stub only.
 */
static int
aac_disk_probe(device_t dev)
{

	debug_called(2);

	return (0);
}

/*
 * Attach a unit to the controller.
 */
static int
aac_disk_attach(device_t dev)
{
	struct aac_disk	*sc;
	
	debug_called(1);

	sc = (struct aac_disk *)device_get_softc(dev);

	/* initialise our softc */
	sc->ad_controller =
	    (struct aac_softc *)device_get_softc(device_get_parent(dev));
	sc->ad_container = device_get_ivars(dev);
	sc->ad_dev = dev;

	/*
	 * require that extended translation be enabled - other drivers read the
	 * disk!
	 */
	sc->ad_size = sc->ad_container->co_mntobj.Capacity;
	if (sc->ad_size >= (2 * 1024 * 1024)) {		/* 2GB */
		sc->ad_heads = 255;
		sc->ad_sectors = 63;
	} else if (sc->ad_size >= (1 * 1024 * 1024)) {	/* 1GB */
		sc->ad_heads = 128;
		sc->ad_sectors = 32;
	} else {
		sc->ad_heads = 64;
		sc->ad_sectors = 32;
	}
	sc->ad_cylinders = (sc->ad_size / (sc->ad_heads * sc->ad_sectors));

	device_printf(dev, "%uMB (%u sectors)\n",
		      sc->ad_size / ((1024 * 1024) / AAC_BLOCK_SIZE),
		      sc->ad_size);

	devstat_add_entry(&sc->ad_stats, "aacd", device_get_unit(dev),
			  AAC_BLOCK_SIZE, DEVSTAT_NO_ORDERED_TAGS,
			  DEVSTAT_TYPE_STORARRAY | DEVSTAT_TYPE_IF_OTHER, 
			  DEVSTAT_PRIORITY_ARRAY);

	/* attach a generic disk device to ourselves */
	sc->ad_dev_t = disk_create(device_get_unit(dev), &sc->ad_disk, 0,
				   &aac_disk_cdevsw, &aac_disk_disk_cdevsw);
	sc->ad_dev_t->si_drv1 = sc;
#ifdef FREEBSD_4
	disks_registered++;
#endif

	sc->ad_dev_t->si_iosize_max = aac_iosize_max;
	sc->unit = device_get_unit(dev);

	return (0);
}

/*
 * Disconnect ourselves from the system.
 */
static int
aac_disk_detach(device_t dev)
{
	struct aac_disk *sc;

	debug_called(2);

	sc = (struct aac_disk *)device_get_softc(dev);

	if (sc->ad_flags & AAC_DISK_OPEN)
		return(EBUSY);

	devstat_remove_entry(&sc->ad_stats);
	disk_destroy(sc->ad_dev_t);
#ifdef FREEBSD_4
	if (--disks_registered == 0)
		cdevsw_remove(&aac_disk_cdevsw);
#endif

	return(0);
}
