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
 * $FreeBSD: src/sys/dev/mlx/mlx_disk.c,v 1.8.2.3 2000/04/26 01:43:21 billf Exp $
 */

/*
 * Disk driver for Mylex DAC960 RAID adapters.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/mlx/mlxio.h>
#include <dev/mlx/mlxvar.h>
#include <dev/mlx/mlxreg.h>

/* prototypes */
static int mlxd_probe(device_t dev);
static int mlxd_attach(device_t dev);
static int mlxd_detach(device_t dev);

static	d_open_t	mlxd_open;
static	d_close_t	mlxd_close;
static	d_strategy_t	mlxd_strategy;
static	d_ioctl_t	mlxd_ioctl;

#define MLXD_BDEV_MAJOR	27
#define MLXD_CDEV_MAJOR	131

static struct cdevsw mlxd_cdevsw = {
		/* open */	mlxd_open,
		/* close */	mlxd_close,
		/* read */	physread,
		/* write */	physwrite,
		/* ioctl */	mlxd_ioctl,
		/* poll */	nopoll,
		/* mmap */	nommap,
		/* strategy */	mlxd_strategy,
		/* name */ 	"mlxd",
		/* maj */	MLXD_CDEV_MAJOR,
		/* dump */	nodump,
		/* psize */ 	nopsize,
		/* flags */	D_DISK,
		/* bmaj */	MLXD_BDEV_MAJOR
};

devclass_t		mlxd_devclass;
static int		disks_registered = 0;
static struct cdevsw	mlxddisk_cdevsw;

static device_method_t mlxd_methods[] = {
    DEVMETHOD(device_probe,	mlxd_probe),
    DEVMETHOD(device_attach,	mlxd_attach),
    DEVMETHOD(device_detach,	mlxd_detach),
    { 0, 0 }
};

static driver_t mlxd_driver = {
    "mlxd",
    mlxd_methods,
    sizeof(struct mlxd_softc)
};

DRIVER_MODULE(mlxd, mlx, mlxd_driver, mlxd_devclass, 0, 0);

static int
mlxd_open(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct mlxd_softc	*sc = (struct mlxd_softc *)dev->si_drv1;
    struct disklabel	*label;

    debug_called(1);
	
    if (sc == NULL)
	return (ENXIO);

    /* controller not active? */
    if (sc->mlxd_controller->mlx_state & MLX_STATE_SHUTDOWN)
	return(ENXIO);

    label = &sc->mlxd_disk.d_label;
    bzero(label, sizeof(*label));
    label->d_type = DTYPE_SCSI;
    label->d_secsize    = MLX_BLKSIZE;
    label->d_nsectors   = sc->mlxd_drive->ms_sectors;
    label->d_ntracks    = sc->mlxd_drive->ms_heads;
    label->d_ncylinders = sc->mlxd_drive->ms_cylinders;
    label->d_secpercyl  = sc->mlxd_drive->ms_sectors * sc->mlxd_drive->ms_heads;
    label->d_secperunit = sc->mlxd_drive->ms_size;

    sc->mlxd_flags |= MLXD_OPEN;
    return (0);
}

static int
mlxd_close(dev_t dev, int flags, int fmt, struct proc *p)
{
    struct mlxd_softc	*sc = (struct mlxd_softc *)dev->si_drv1;

    debug_called(1);
	
    if (sc == NULL)
	return (ENXIO);
    sc->mlxd_flags &= ~MLXD_OPEN;
    return (0);
}

static int
mlxd_ioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    struct mlxd_softc	*sc = (struct mlxd_softc *)dev->si_drv1;
    int error;

    debug_called(1);
	
    if (sc == NULL)
	return (ENXIO);

    if ((error = mlx_submit_ioctl(sc->mlxd_controller, sc->mlxd_drive, cmd, addr, flag, p)) != ENOIOCTL) {
	debug(0, "mlx_submit_ioctl returned %d\n", error);
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
mlxd_strategy(struct buf *bp)
{
    struct mlxd_softc	*sc = (struct mlxd_softc *)bp->b_dev->si_drv1;

    debug_called(1);

    /* bogus disk? */
    if (sc == NULL) {
	bp->b_error = EINVAL;
	goto bad;
    }

    /* XXX may only be temporarily offline - sleep? */
    if (sc->mlxd_drive->ms_state == MLX_SYSD_OFFLINE) {
	bp->b_error = ENXIO;
	goto bad;
    }

    /* do-nothing operation */
    if (bp->b_bcount == 0)
	goto done;

    devstat_start_transaction(&sc->mlxd_stats);
    mlx_submit_buf(sc->mlxd_controller, bp);
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
mlxd_intr(void *data)
{
    struct buf *bp = (struct buf *)data;
    struct mlxd_softc	*sc = (struct mlxd_softc *)bp->b_dev->si_drv1;

    debug_called(1);
	
    if (bp->b_flags & B_ERROR)
	bp->b_error = EIO;
    else
	bp->b_resid = 0;

    devstat_end_transaction_buf(&sc->mlxd_stats, bp);
    biodone(bp);
}

static int
mlxd_probe(device_t dev)
{

    debug_called(1);
	
    device_set_desc(dev, "Mylex System Drive");
    return (0);
}

static int
mlxd_attach(device_t dev)
{
    struct mlxd_softc	*sc = (struct mlxd_softc *)device_get_softc(dev);
    device_t		parent;
    char		*state;
    dev_t		dsk;
    
    debug_called(1);

    parent = device_get_parent(dev);
    sc->mlxd_controller = (struct mlx_softc *)device_get_softc(parent);
    sc->mlxd_unit = device_get_unit(dev);
    sc->mlxd_drive = device_get_ivars(dev);
    sc->mlxd_dev = dev;

    switch(sc->mlxd_drive->ms_state) {
    case MLX_SYSD_ONLINE:
	state = "online";
	break;
    case MLX_SYSD_CRITICAL:
	state = "critical";
	break;
    case MLX_SYSD_OFFLINE:
	state = "offline";
	break;
    default:
	state = "unknown state";
    }

    device_printf(dev, "%uMB (%u sectors) RAID %d (%s)\n",
		  sc->mlxd_drive->ms_size / ((1024 * 1024) / MLX_BLKSIZE),
		  sc->mlxd_drive->ms_size, sc->mlxd_drive->ms_raidlevel, state);

    devstat_add_entry(&sc->mlxd_stats, "mlxd", sc->mlxd_unit, MLX_BLKSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_STORARRAY | DEVSTAT_TYPE_IF_OTHER, 
		      DEVSTAT_PRIORITY_ARRAY);

    dsk = disk_create(sc->mlxd_unit, &sc->mlxd_disk, 0, &mlxd_cdevsw, &mlxddisk_cdevsw);
    dsk->si_drv1 = sc;
    disks_registered++;

    /* set maximum I/O size */
    dsk->si_iosize_max = sc->mlxd_controller->mlx_enq2->me_maxblk * MLX_BLKSIZE;

    return (0);
}

static int
mlxd_detach(device_t dev)
{
    struct mlxd_softc *sc = (struct mlxd_softc *)device_get_softc(dev);

    debug_called(1);

    devstat_remove_entry(&sc->mlxd_stats);

    /* hack to handle lack of destroy_disk() */
    if (--disks_registered == 0)
	cdevsw_remove(&mlxddisk_cdevsw);

    return(0);
}

