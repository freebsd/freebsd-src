/*-
 * Copyright (c) 1999 Jonathan Lemon
 * Copyright (c) 1999 Michael Smith
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
 * $FreeBSD: src/sys/dev/amr/amr_disk.c,v 1.5.2.1 2000/04/11 00:12:46 msmith Exp $
 */

/*
 * Disk driver for AMI MegaRaid controllers
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

#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/amr/amrio.h>
#include <dev/amr/amrreg.h>
#include <dev/amr/amrvar.h>

#if 0
#define debug(fmt, args...)	printf("%s: " fmt "\n", __FUNCTION__ , ##args)
#else
#define debug(fmt, args...)
#endif

/* prototypes */
static int amrd_probe(device_t dev);
static int amrd_attach(device_t dev);
static int amrd_detach(device_t dev);

static	d_open_t	amrd_open;
static	d_close_t	amrd_close;
static	d_strategy_t	amrd_strategy;
static	d_ioctl_t	amrd_ioctl;

#define AMRD_BDEV_MAJOR	35
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
		/* bmaj */	AMRD_BDEV_MAJOR
};

static devclass_t	amrd_devclass;
static struct cdevsw	amrddisk_cdevsw;
static int		disks_registered = 0;

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

    debug("called");
	
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

    debug("called");
	
    if (sc == NULL)
	return (ENXIO);
    sc->amrd_flags &= ~AMRD_OPEN;
    return (0);
}

static int
amrd_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    struct amrd_softc	*sc = (struct amrd_softc *)dev->si_drv1;
    int error;

    debug("called");
	
    if (sc == NULL)
	return (ENXIO);

    if ((error = amr_submit_ioctl(sc->amrd_controller, sc->amrd_drive, cmd, addr, flag, p)) != ENOIOCTL) {
	debug("amr_submit_ioctl returned %d\n", error);
	return(error);
    }
    return (ENOTTY);
}

/*
 * Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
static void
amrd_strategy(struct buf *bp)
{
    struct amrd_softc	*sc = (struct amrd_softc *)bp->b_dev->si_drv1;

    debug("called to %s %d bytes at b_blkno 0x%x  b_pblkno 0x%x", 
	  (bp->b_flags & B_READ) ? "read" : "write", bp->b_bcount, bp->b_blkno, bp->b_pblkno);

    /* bogus disk? */
    if (sc == NULL) {
	bp->b_error = EINVAL;
	goto bad;
    }

#if 0
    /* XXX may only be temporarily offline - sleep? */
    if (sc->amrd_drive->ld_state == AMR_SYSD_OFFLINE) {
	bp->b_error = ENXIO;
	goto bad;
    }
#endif

    /* do-nothing operation */
    if (bp->b_bcount == 0)
	goto done;

    devstat_start_transaction(&sc->amrd_stats);
    amr_submit_buf(sc->amrd_controller, bp);
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
amrd_intr(void *data)
{
    struct buf *bp = (struct buf *)data;
    struct amrd_softc *sc = (struct amrd_softc *)bp->b_dev->si_drv1;

    debug("called");

    if (bp->b_flags & B_ERROR) {
	bp->b_error = EIO;
	debug("i/o error\n");
    } else {
#if 0
	int i;
	for (i = 0; i < 512; i += 16)
	    debug(" %04x  %16D", i, bp->b_data + i, " ");
#endif
	bp->b_resid = 0;
    }

    devstat_end_transaction_buf(&sc->amrd_stats, bp);
    biodone(bp);
}

static int
amrd_probe(device_t dev)
{

    debug("called");
	
    device_set_desc(dev, "MegaRAID logical drive");
    return (0);
}

static int
amrd_attach(device_t dev)
{
    struct amrd_softc	*sc = (struct amrd_softc *)device_get_softc(dev);
    device_t		parent;
    char		*state;
    
    debug("called");

    parent = device_get_parent(dev);
    sc->amrd_controller = (struct amr_softc *)device_get_softc(parent);
    sc->amrd_unit = device_get_unit(dev);
    sc->amrd_drive = device_get_ivars(dev);
    sc->amrd_dev = dev;

    switch(sc->amrd_drive->al_state) {
    case AMRD_OFFLINE:
	state = "offline";
	break;
    case AMRD_DEGRADED:
	state = "degraded";
	break;
    case AMRD_OPTIMAL:
	state = "optimal";
	break;
    case AMRD_DELETED:
	state = "deleted";
	break;
    default:
	state = "unknown state";
    }

    device_printf(dev, "%uMB (%u sectors) RAID %d (%s)\n",
		  sc->amrd_drive->al_size / ((1024 * 1024) / AMR_BLKSIZE),
		  sc->amrd_drive->al_size, sc->amrd_drive->al_properties & 0xf, state);

    devstat_add_entry(&sc->amrd_stats, "amrd", sc->amrd_unit, AMR_BLKSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_STORARRAY | DEVSTAT_TYPE_IF_OTHER, 
		      DEVSTAT_PRIORITY_ARRAY);

    sc->amrd_dev_t = disk_create(sc->amrd_unit, &sc->amrd_disk, 0, &amrd_cdevsw, &amrddisk_cdevsw);
    sc->amrd_dev_t->si_drv1 = sc;
    disks_registered++;

    /* set maximum I/O size */
    /* dsk->si_iosize_max = ??? */;

    return (0);
}

static int
amrd_detach(device_t dev)
{
    struct amrd_softc *sc = (struct amrd_softc *)device_get_softc(dev);

    debug("called");

    devstat_remove_entry(&sc->amrd_stats);
    disk_destroy(sc->amrd_dev_t);
    return(0);
}

