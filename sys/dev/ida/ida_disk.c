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
 * $FreeBSD: src/sys/dev/ida/ida_disk.c,v 1.12.2.3 2000/07/27 22:27:39 jlemon Exp $
 */

/*
 * Disk driver for Compaq SMART RAID adapters.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#if NPCI > 0
#include <machine/bus_memio.h>
#endif
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/ida/idareg.h>
#include <dev/ida/idavar.h>

/* prototypes */
static int idad_probe(device_t dev);
static int idad_attach(device_t dev);
static int idad_detach(device_t dev);

static	d_open_t	idad_open;
static	d_close_t	idad_close;
static	d_strategy_t	idad_strategy;

#define IDAD_BDEV_MAJOR	29
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
	/* dump */	nodump,
	/* psize */ 	nopsize,
	/* flags */	D_DISK,
	/* bmaj */	IDAD_BDEV_MAJOR
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
idad_open(dev_t dev, int flags, int fmt, struct proc *p)
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
idad_close(dev_t dev, int flags, int fmt, struct proc *p)
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
idad_strategy(struct buf *bp)
{
	struct idad_softc *drv;
	int s;

	drv = idad_getsoftc(bp->b_dev);
	if (drv == NULL) {
    		bp->b_error = EINVAL;
		goto bad;
	}

	/*
	 * software write protect check
	 */
	if (drv->flags & DRV_WRITEPROT && (bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		goto bad;
	}

	/*
	 * If it's a null transfer, return immediately
	 */
	if (bp->b_bcount == 0)
		goto done;

	bp->b_driver1 = drv;
	s = splbio();
	devstat_start_transaction(&drv->stats);
	ida_submit_buf(drv->controller, bp);
	splx(s);
	return;

bad:
	bp->b_flags |= B_ERROR;

done:
	/*
	 * Correctly set the buf to indicate a completed transfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

void
idad_intr(struct buf *bp)
{
	struct idad_softc *drv = (struct idad_softc *)bp->b_driver1;

	if (bp->b_flags & B_ERROR)
		bp->b_error = EIO;
	else
		bp->b_resid = 0;

	devstat_end_transaction_buf(&drv->stats, bp);
	biodone(bp);
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
	    &dinfo, sizeof(dinfo), drv->drive, DMA_DATA_IN);
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
