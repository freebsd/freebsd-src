/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * $FreeBSD: src/sys/dev/twe/twe_disk.c,v 1.1.2.2 2000/07/20 02:32:35 msmith Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#if __FreeBSD_version < 500000
# include <dev/twe/twe_compat.h>
#else
# include <sys/bio.h>
#endif
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/twe/twereg.h>
#include <dev/twe/twevar.h>

/*
 * Interface to controller.
 */
static int twed_probe(device_t dev);
static int twed_attach(device_t dev);
static int twed_detach(device_t dev);

/*
 * Interface to the device switch
 */
static	d_open_t	twed_open;
static	d_close_t	twed_close;
static	d_strategy_t	twed_strategy;

#define TWED_CDEV_MAJOR	147

static struct cdevsw twed_cdevsw = {
    /* open */		twed_open,
    /* close */		twed_close,
    /* read */		physread,
    /* write */		physwrite,
    /* ioctl */		noioctl,
    /* poll */		nopoll,
    /* mmap */		nommap,
    /* strategy */	twed_strategy,
    /* name */ 		"twed",
    /* maj */		TWED_CDEV_MAJOR,
    /* dump */		nodump,
    /* psize */ 	nopsize,
    /* flags */		D_DISK,
    /* bmaj */		-1
};

devclass_t		twed_devclass;
static struct cdevsw	tweddisk_cdevsw;
#ifdef FREEBSD_4
static int		disks_registered = 0;
#endif

static device_method_t twed_methods[] = {
    DEVMETHOD(device_probe,	twed_probe),
    DEVMETHOD(device_attach,	twed_attach),
    DEVMETHOD(device_detach,	twed_detach),
    { 0, 0 }
};

static driver_t twed_driver = {
    "twed",
    twed_methods,
    sizeof(struct twed_softc)
};

DRIVER_MODULE(twed, twe, twed_driver, twed_devclass, 0, 0);

/********************************************************************************
 * Handle open from generic layer.
 *
 * Note that this is typically only called by the diskslice code, and not
 * for opens on subdevices (eg. slices, partitions).
 */
static int
twed_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct twed_softc	*sc = (struct twed_softc *)dev->si_drv1;
    struct disklabel	*label;

    debug_called(4);
	
    if (sc == NULL)
	return (ENXIO);

    /* check that the controller is up and running */
    if (sc->twed_controller->twe_state & TWE_STATE_SHUTDOWN)
	return(ENXIO);

    /* build synthetic label */
    label = &sc->twed_disk.d_label;
    bzero(label, sizeof(*label));
    label->d_type = DTYPE_ESDI;
    label->d_secsize    = TWE_BLOCK_SIZE;
    label->d_nsectors   = sc->twed_drive->td_sectors;
    label->d_ntracks    = sc->twed_drive->td_heads;
    label->d_ncylinders = sc->twed_drive->td_cylinders;
    label->d_secpercyl  = sc->twed_drive->td_sectors * sc->twed_drive->td_heads;
    label->d_secperunit = sc->twed_drive->td_size;

    sc->twed_flags |= TWED_OPEN;
    return (0);
}

/********************************************************************************
 * Handle last close of the disk device.
 */
static int
twed_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct twed_softc	*sc = (struct twed_softc *)dev->si_drv1;

    debug_called(4);
	
    if (sc == NULL)
	return (ENXIO);

    sc->twed_flags &= ~TWED_OPEN;
    return (0);
}

/********************************************************************************
 * Handle an I/O request.
 */
static void
twed_strategy(struct bio *bp)
{
    struct twed_softc	*sc = (struct twed_softc *)bp->bio_dev->si_drv1;

    debug_called(4);

    /* bogus disk? */
    if (sc == NULL) {
	bp->bio_flags |= BIO_ERROR;
	bp->bio_error = EINVAL;
	return;
    }

    /* do-nothing operation? */
    if (bp->bio_bcount == 0) {
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
	return;
    }

    /* perform accounting */
    devstat_start_transaction(&sc->twed_stats);

    /* pass the bio to the controller - it can work out who we are */
    twe_submit_buf(sc->twed_controller, bp);
    return;
}

/********************************************************************************
 * Handle completion of an I/O request.
 */
void
twed_intr(void *data)
{
    struct bio *bp = (struct bio *)data;
    struct twed_softc	*sc = (struct twed_softc *)bp->bio_dev->si_drv1;

    debug_called(4);

    /* check for error from controller code */
    if (bp->bio_flags & BIO_ERROR) {
	bp->bio_error = EIO;
    } else {
	bp->bio_resid = 0;
    }

    devstat_end_transaction_bio(&sc->twed_stats, bp);
    biodone(bp);
}

/********************************************************************************
 * Stub to provide device identification when probed.
 */
static int
twed_probe(device_t dev)
{

    debug_called(4);
	
    device_set_desc(dev, "3ware RAID unit");
    return (0);
}

/********************************************************************************
 * Attach a unit to the controller.
 */
static int
twed_attach(device_t dev)
{
    struct twed_softc	*sc = (struct twed_softc *)device_get_softc(dev);
    device_t		parent;
    dev_t		dsk;
    
    debug_called(4);

    /* initialise our softc */
    parent = device_get_parent(dev);
    sc->twed_controller = (struct twe_softc *)device_get_softc(parent);
    sc->twed_drive = device_get_ivars(dev);
    sc->twed_dev = dev;

    /* report the drive */
    device_printf(dev, "%uMB (%u sectors)\n",
		  sc->twed_drive->td_size / ((1024 * 1024) / TWE_BLOCK_SIZE),
		  sc->twed_drive->td_size);

    devstat_add_entry(&sc->twed_stats, "twed", device_get_unit(dev), TWE_BLOCK_SIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_STORARRAY | DEVSTAT_TYPE_IF_OTHER, 
		      DEVSTAT_PRIORITY_ARRAY);

    /* attach a generic disk device to ourselves */
    dsk = disk_create(device_get_unit(dev), &sc->twed_disk, 0, &twed_cdevsw, &tweddisk_cdevsw);
    dsk->si_drv1 = sc;
    sc->twed_dev_t = dsk;
#ifdef FREEBSD_4
    disks_registered++;
#endif

    /* set the maximum I/O size to the theoretical maximum allowed by the S/G list size */
    dsk->si_iosize_max = (TWE_MAX_SGL_LENGTH - 1) * PAGE_SIZE;

    return (0);
}

/********************************************************************************
 * Disconnect ourselves from the system.
 */
static int
twed_detach(device_t dev)
{
    struct twed_softc *sc = (struct twed_softc *)device_get_softc(dev);

    debug_called(4);

    if (sc->twed_flags & TWED_OPEN)
	return(EBUSY);

    devstat_remove_entry(&sc->twed_stats);
#ifdef FREEBSD_4
    if (--disks_registered == 0)
	cdevsw_remove(&tweddisk_cdevsw);
#else
    disk_destroy(sc->twed_dev_t);
#endif

    return(0);
}

