/*-
 * Copyright (c) 1999 Jonathan Lemon
 * Copyright (c) 1999, 2000 Michael Smith
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
 * $FreeBSD$
 */

/*
 * Disk driver for AMI MegaRaid controllers
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <dev/amr/amr_compat.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/bus.h>
#include <sys/rman.h>

#include <dev/amr/amrio.h>
#include <dev/amr/amrreg.h>
#include <dev/amr/amrvar.h>
#include <dev/amr/amr_tables.h>

/* prototypes */
static int amrd_probe(device_t dev);
static int amrd_attach(device_t dev);
static int amrd_detach(device_t dev);

static	d_open_t	amrd_open;
static	d_close_t	amrd_close;
static	d_strategy_t	amrd_strategy;
static	d_ioctl_t	amrd_ioctl;

#define AMRD_CDEV_MAJOR	133

static struct cdevsw amrd_cdevsw = {
		/* open */	amrd_open,
		/* close */	amrd_close,
		/* read */	physread,
		/* write */	physwrite,
		/* ioctl */	amrd_ioctl,
		/* poll */	nopoll,
		/* mmap */	nommap,
		/* strategy */	amrd_strategy,
		/* name */ 	"amrd",
		/* maj */	AMRD_CDEV_MAJOR,
		/* dump */	nodump,
		/* psize */ 	nopsize,
		/* flags */	D_DISK,
};

static devclass_t	amrd_devclass;
static struct cdevsw	amrddisk_cdevsw;
#ifdef FREEBSD_4
static int		disks_registered = 0;
#endif

static device_method_t amrd_methods[] = {
    DEVMETHOD(device_probe,	amrd_probe),
    DEVMETHOD(device_attach,	amrd_attach),
    DEVMETHOD(device_detach,	amrd_detach),
    { 0, 0 }
};

static driver_t amrd_driver = {
    "amrd",
    amrd_methods,
    sizeof(struct amrd_softc)
};

DRIVER_MODULE(amrd, amr, amrd_driver, amrd_devclass, 0, 0);

static int
amrd_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct amrd_softc	*sc = (struct amrd_softc *)dev->si_drv1;
    struct disklabel	*label;

    debug_called(1);
	
    if (sc == NULL)
	return (ENXIO);

    /* controller not active? */
    if (sc->amrd_controller->amr_state & AMR_STATE_SHUTDOWN)
	return(ENXIO);

    label = &sc->amrd_disk.d_label;
    bzero(label, sizeof(*label));
    label->d_type 	= DTYPE_SCSI;
    label->d_secsize    = AMR_BLKSIZE;
    label->d_nsectors   = sc->amrd_drive->al_sectors;
    label->d_ntracks    = sc->amrd_drive->al_heads;
    label->d_ncylinders = sc->amrd_drive->al_cylinders;
    label->d_secpercyl  = sc->amrd_drive->al_sectors * sc->amrd_drive->al_heads;
    label->d_secperunit = sc->amrd_drive->al_size;

    sc->amrd_flags |= AMRD_OPEN;
    return (0);
}

static int
amrd_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct amrd_softc	*sc = (struct amrd_softc *)dev->si_drv1;

    debug_called(1);
	
    if (sc == NULL)
	return (ENXIO);
    sc->amrd_flags &= ~AMRD_OPEN;
    return (0);
}

static int
amrd_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{

    return (ENOTTY);
}

/*
 * Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
static void
amrd_strategy(struct bio *bio)
{
    struct amrd_softc	*sc = (struct amrd_softc *)bio->bio_dev->si_drv1;

    /* bogus disk? */
    if (sc == NULL) {
	bio->bio_error = EINVAL;
	goto bad;
    }

    devstat_start_transaction(&sc->amrd_stats);
    amr_submit_bio(sc->amrd_controller, bio);
    return;

 bad:
    bio->bio_flags |= BIO_ERROR;

 done:
    /*
     * Correctly set the buf to indicate a completed transfer
     */
    bio->bio_resid = bio->bio_bcount;
    biodone(bio);
    return;
}

void
amrd_intr(void *data)
{
    struct bio *bio = (struct bio *)data;
    struct amrd_softc *sc = (struct amrd_softc *)bio->bio_dev->si_drv1;

    debug_called(2);

    if (bio->bio_flags & BIO_ERROR) {
	bio->bio_error = EIO;
	debug(1, "i/o error\n");
    } else {
	bio->bio_resid = 0;
    }

    biofinish(bio, &sc->amrd_stats, 0);
}

static int
amrd_probe(device_t dev)
{

    debug_called(1);
	
    device_set_desc(dev, "MegaRAID logical drive");
    return (0);
}

static int
amrd_attach(device_t dev)
{
    struct amrd_softc	*sc = (struct amrd_softc *)device_get_softc(dev);
    device_t		parent;
    
    debug_called(1);

    parent = device_get_parent(dev);
    sc->amrd_controller = (struct amr_softc *)device_get_softc(parent);
    sc->amrd_unit = device_get_unit(dev);
    sc->amrd_drive = device_get_ivars(dev);
    sc->amrd_dev = dev;

    device_printf(dev, "%uMB (%u sectors) RAID %d (%s)\n",
		  sc->amrd_drive->al_size / ((1024 * 1024) / AMR_BLKSIZE),
		  sc->amrd_drive->al_size, sc->amrd_drive->al_properties & AMR_DRV_RAID_MASK, 
		  amr_describe_code(amr_table_drvstate, AMR_DRV_CURSTATE(sc->amrd_drive->al_state)));

    devstat_add_entry(&sc->amrd_stats, "amrd", sc->amrd_unit, AMR_BLKSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_STORARRAY | DEVSTAT_TYPE_IF_OTHER, 
		      DEVSTAT_PRIORITY_ARRAY);

    sc->amrd_dev_t = disk_create(sc->amrd_unit, &sc->amrd_disk, 0, &amrd_cdevsw, &amrddisk_cdevsw);
    sc->amrd_dev_t->si_drv1 = sc;
#ifdef FREEBSD_4
    disks_registered++;
#endif

    /* set maximum I/O size to match the maximum s/g size */
    sc->amrd_dev_t->si_iosize_max = (AMR_NSEG - 1) * PAGE_SIZE;

    return (0);
}

static int
amrd_detach(device_t dev)
{
    struct amrd_softc *sc = (struct amrd_softc *)device_get_softc(dev);

    debug_called(1);

    if (sc->amrd_flags & AMRD_OPEN)
	return(EBUSY);

    devstat_remove_entry(&sc->amrd_stats);
#ifdef FREEBSD_4
    if (--disks_registered == 0)
	cdevsw_remove(&amrddisk_cdevsw);
#else
    disk_destroy(sc->amrd_dev_t);
#endif
    return(0);
}

