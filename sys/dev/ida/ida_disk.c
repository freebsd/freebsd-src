/*-
 * Copyright (c) 1999 Jonathan Lemon
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
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>

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
static void id_drvinit(void);
static int idprobe(device_t dev);
static int idattach(device_t dev);

static	d_open_t	idopen;
static	d_close_t	idclose;
static	d_strategy_t	idstrategy;
static	d_ioctl_t	idioctl;
static	d_psize_t	idsize;

#define ID_BDEV_MAJOR	29
#define ID_CDEV_MAJOR	109

#define WD_BDEV_MAJOR	0
#define WD_CDEV_MAJOR	3

static struct cdevsw id_cdevsw = {
	/* open */	idopen,
	/* close */	idclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	idioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	idstrategy,
	/* name */ 	"id",
	/* maj */	ID_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */ 	idsize,
	/* flags */	D_DISK,
	/* bmaj */	ID_BDEV_MAJOR
};
static struct cdevsw stolen_cdevsw;

static devclass_t	id_devclass;

static device_method_t id_methods[] = {
	DEVMETHOD(device_probe,		idprobe),
	DEVMETHOD(device_attach,	idattach),
	{ 0, 0 }
};

static driver_t id_driver = {
	"id",
	id_methods,
	sizeof(struct id_softc)
};

static __inline struct id_softc *
idgetsoftc(dev_t dev)
{
	int unit;

	unit = dkunit(dev);
	return ((struct id_softc *)devclass_get_softc(id_devclass, unit));
}

static int
idopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct id_softc *drv;
	struct disklabel label;
	int error;

	drv = idgetsoftc(dev);
	if (drv == NULL)
		return (ENXIO);

	/* XXX block against race where > 1 person is reading label? */

	bzero(&label, sizeof(label));
	label.d_type = DTYPE_SCSI;	/* XXX should this be DTYPE_RAID? */
/*
	strncpy(label.d_typename, ...
	strncpy(label.d_packname, ...
*/
	label.d_secsize = drv->secsize;
	label.d_nsectors = drv->sectors;
	label.d_ntracks = drv->heads;
	label.d_ncylinders = drv->cylinders;
	label.d_secpercyl = drv->sectors * drv->heads;
	label.d_secperunit = drv->secperunit;

	/* Initialize slice tables. */
	error = dsopen(dev, fmt, 0, &drv->slices, &label);

	return (error);
}

static int
idclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct id_softc *drv;

	drv = idgetsoftc(dev);
	if (drv == NULL)
		return (ENXIO);
	dsclose(dev, fmt, drv->slices);
	return (0);
}

static int
idioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
	struct id_softc *drv;
	int error;

	drv = idgetsoftc(dev);
	if (drv == NULL)
		return (ENXIO);

	error = dsioctl(dev, cmd, addr, flag, &drv->slices);

	if (error == ENOIOCTL)
		return (ENOTTY);

	return (error);
}

static int
idsize(dev_t dev)
{
	struct id_softc *drv;

	drv = idgetsoftc(dev);
	if (drv == NULL)
		return (ENXIO);
	return (dssize(dev, &drv->slices));
}

/*
 * Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
static void
idstrategy(struct buf *bp)
{
	struct id_softc *drv;
	int s;

	drv = idgetsoftc(bp->b_dev);
	if (drv == NULL) {
    		bp->b_error = EINVAL;
		goto bad;
	}

	if (dscheck(bp, drv->slices) <= 0)
		goto done;

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
id_intr(struct buf *bp)
{
	struct id_softc *drv = (struct id_softc *)bp->b_driver1;

	if (bp->b_flags & B_ERROR)
		bp->b_error = EIO;
	else
		bp->b_resid = 0;

	devstat_end_transaction_buf(&drv->stats, bp);
	biodone(bp);
}

static void
id_drvinit(void)
{
	static int devsw_installed = 0;

	if (devsw_installed)
		return;				/* XXX is this needed? */

	cdevsw_add(&id_cdevsw);
	stolen_cdevsw = id_cdevsw;
	stolen_cdevsw.d_maj = WD_CDEV_MAJOR;
	stolen_cdevsw.d_bmaj = WD_BDEV_MAJOR;
	cdevsw_add(&stolen_cdevsw);
	devsw_installed = 1;
}

static int
idprobe(device_t dev)
{

	id_drvinit();
	device_set_desc(dev, "Compaq Logical Drive");
	return (0);
}

static int
idattach(device_t dev)
{
	struct ida_drive_info dinfo;
	struct id_softc *drv;
	device_t parent;
	int error;

	drv = (struct id_softc *)device_get_softc(dev);
	parent = device_get_parent(dev);
	drv->controller = (struct ida_softc *)device_get_softc(parent);
	drv->unit = device_get_unit(dev);

	error = ida_command(drv->controller, CMD_GET_LOG_DRV_INFO,
	    &dinfo, sizeof(dinfo), drv->unit, DMA_DATA_IN);
	if (error) {
		device_printf(dev, "CMD_GET_LOG_DRV_INFO failed\n");
		return (ENXIO);
	}

	drv->cylinders = dinfo.ncylinders;
	drv->heads = dinfo.nheads;
	drv->sectors = dinfo.nsectors;
	drv->secsize = dinfo.secsize;
	drv->secperunit = dinfo.secperunit;

	/* XXX
	 * other initialization
	 */
	device_printf(dev, "%uMB (%u sectors), blocksize=%d\n",
	    drv->secperunit / ((1024 * 1024) / drv->secsize),
	    drv->secperunit, drv->secsize);

	devstat_add_entry(&drv->stats, "id", drv->unit, drv->secsize,
	    DEVSTAT_NO_ORDERED_TAGS,
	    DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_OTHER, DEVSTAT_PRIORITY_DA);

	return (0);
}

DEV_DRIVER_MODULE(id, ida, id_driver, id_devclass, id_cdevsw, 0, 0);
