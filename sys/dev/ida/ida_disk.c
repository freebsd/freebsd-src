/*-
 * Copyright (c) 1999,2000 Jonathan Lemon
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
 * $FreeBSD$
 */

/*
 * Disk driver for Compaq SMART RAID adapters.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>

#include <dev/ida/idareg.h>
#include <dev/ida/idavar.h>

/* prototypes */
static int idad_probe(device_t dev);
static int idad_attach(device_t dev);
static int idad_detach(device_t dev);

static	d_open_t	idad_open;
static	d_close_t	idad_close;
static	d_strategy_t	idad_strategy;
static	d_dump_t	idad_dump;

#define IDAD_CDEV_MAJOR	109

static struct cdevsw id_cdevsw = {
	/* open */	idad_open,
	/* close */	idad_close,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	idad_strategy,
	/* name */ 	"idad",
	/* maj */	IDAD_CDEV_MAJOR,
	/* dump */	idad_dump,
	/* psize */ 	nopsize,
	/* flags */	D_DISK,
};

static devclass_t	idad_devclass;
static struct cdevsw 	idaddisk_cdevsw;
static int		disks_registered = 0;

static device_method_t idad_methods[] = {
	DEVMETHOD(device_probe,		idad_probe),
	DEVMETHOD(device_attach,	idad_attach),
	DEVMETHOD(device_detach,	idad_detach),
	{ 0, 0 }
};

static driver_t idad_driver = {
	"idad",
	idad_methods,
	sizeof(struct idad_softc)
};

DRIVER_MODULE(idad, ida, idad_driver, idad_devclass, 0, 0);

static __inline struct idad_softc *
idad_getsoftc(dev_t dev)
{

	return ((struct idad_softc *)dev->si_drv1);
}

static int
idad_open(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct idad_softc *drv;
	struct disklabel *label;

	drv = idad_getsoftc(dev);
	if (drv == NULL)
		return (ENXIO);

	label = &drv->disk.d_label;
	bzero(label, sizeof(*label));
	label->d_type = DTYPE_SCSI;
	label->d_secsize = drv->secsize;
	label->d_nsectors = drv->sectors;
	label->d_ntracks = drv->heads;
	label->d_ncylinders = drv->cylinders;
	label->d_secpercyl = drv->sectors * drv->heads;
	label->d_secperunit = drv->secperunit;

	return (0);
}

static int
idad_close(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct idad_softc *drv;

	drv = idad_getsoftc(dev);
	if (drv == NULL)
		return (ENXIO);
	return (0);
}

/*
 * Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
static void
idad_strategy(struct bio *bp)
{
	struct idad_softc *drv;
	int s;

	drv = idad_getsoftc(bp->bio_dev);
	if (drv == NULL) {
    		bp->bio_error = EINVAL;
		goto bad;
	}

	/*
	 * software write protect check
	 */
	if (drv->flags & DRV_WRITEPROT && (bp->bio_cmd == BIO_WRITE)) {
		bp->bio_error = EROFS;
		goto bad;
	}

	bp->bio_driver1 = drv;
	s = splbio();
	devstat_start_transaction(&drv->stats);
	ida_submit_buf(drv->controller, bp);
	splx(s);
	return;

bad:
	bp->bio_flags |= BIO_ERROR;

	/*
	 * Correctly set the buf to indicate a completed transfer
	 */
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
	return;
}

static int
idad_dump(dev_t dev)
{
	struct idad_softc *drv;
	u_int count, blkno, secsize;
	long blkcnt;
	int i, error, dumppages;
        caddr_t va;
	vm_offset_t addr, a;

	if ((error = disk_dumpcheck(dev, &count, &blkno, &secsize)))
		return (error);

	drv = idad_getsoftc(dev);
	if (drv == NULL)
		return (ENXIO);

	addr = 0;
	blkcnt = howmany(PAGE_SIZE, secsize);

	while (count > 0) {
		va = NULL;

		dumppages = imin(count / blkcnt, MAXDUMPPGS); 

		for (i = 0; i < dumppages; i++) {
			a = addr + (i * PAGE_SIZE);
			if (is_physical_memory(a))
				va = pmap_kenter_temporary(trunc_page(a), i);
			else
				va = pmap_kenter_temporary(trunc_page(0), i);
		}

		error = ida_command(drv->controller, CMD_WRITE, va,
		    PAGE_SIZE * dumppages, drv->drive, blkno, DMA_DATA_OUT);
		if (error)
			return (error);

		if (dumpstatus(addr, (off_t)count * DEV_BSIZE) < 0)
			return (EINTR);

		blkno += blkcnt * dumppages;
		count -= blkcnt * dumppages;
		addr += PAGE_SIZE * dumppages;
	}
	return (0);
}

void
idad_intr(struct bio *bp)
{
	struct idad_softc *drv = (struct idad_softc *)bp->bio_driver1;

	if (bp->bio_flags & BIO_ERROR)
		bp->bio_error = EIO;
	else
		bp->bio_resid = 0;

	biofinish(bp, &drv->stats, 0);
}

static int
idad_probe(device_t dev)
{

	device_set_desc(dev, "Compaq Logical Drive");
	return (0);
}

static int
idad_attach(device_t dev)
{
	struct ida_drive_info dinfo;
	struct idad_softc *drv;
	device_t parent;
	dev_t dsk;
	int error;

	drv = (struct idad_softc *)device_get_softc(dev);
	parent = device_get_parent(dev);
	drv->controller = (struct ida_softc *)device_get_softc(parent);
	drv->unit = device_get_unit(dev);
	drv->drive = drv->controller->num_drives;
	drv->controller->num_drives++;

	error = ida_command(drv->controller, CMD_GET_LOG_DRV_INFO,
	    &dinfo, sizeof(dinfo), drv->drive, 0, DMA_DATA_IN);
	if (error) {
		device_printf(dev, "CMD_GET_LOG_DRV_INFO failed\n");
		return (ENXIO);
	}

	drv->cylinders = dinfo.ncylinders;
	drv->heads = dinfo.nheads;
	drv->sectors = dinfo.nsectors;
	drv->secsize = dinfo.secsize == 0 ? 512 : dinfo.secsize;
	drv->secperunit = dinfo.secperunit;

	/* XXX
	 * other initialization
	 */
	device_printf(dev, "%uMB (%u sectors), blocksize=%d\n",
	    drv->secperunit / ((1024 * 1024) / drv->secsize),
	    drv->secperunit, drv->secsize);

	devstat_add_entry(&drv->stats, "idad", drv->unit, drv->secsize,
	    DEVSTAT_NO_ORDERED_TAGS,
	    DEVSTAT_TYPE_STORARRAY| DEVSTAT_TYPE_IF_OTHER,
	    DEVSTAT_PRIORITY_ARRAY);

	dsk = disk_create(drv->unit, &drv->disk, 0,
	    &id_cdevsw, &idaddisk_cdevsw);

	dsk->si_drv1 = drv;
	dsk->si_iosize_max = DFLTPHYS;		/* XXX guess? */
	disks_registered++;

	return (0);
}

static int
idad_detach(device_t dev)
{
	struct idad_softc *drv;

	drv = (struct idad_softc *)device_get_softc(dev);
	devstat_remove_entry(&drv->stats);

	if (--disks_registered == 0)
		cdevsw_remove(&idaddisk_cdevsw);
	return (0);
}
