/*
 * Copyright (C) 2002 Benno Rice <benno@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/nexusvar.h>

#define	OFWD_BLOCKSIZE	512

struct ofwd_softc
{
	device_t	ofwd_dev;
	dev_t		ofwd_dev_t;
	struct		disk ofwd_disk;
	phandle_t	ofwd_package;
	ihandle_t	ofwd_instance;
	u_int		ofwd_flags;
#define	OFWD_OPEN	(1<<0)
};

/*
 * Disk device bus interface.
 */
static void	ofwd_identify(driver_t *, device_t);
static int	ofwd_probe(device_t);
static int	ofwd_attach(device_t);

static device_method_t	ofwd_methods[] = {
	DEVMETHOD(device_identify,	ofwd_identify),
	DEVMETHOD(device_probe, 	ofwd_probe),
	DEVMETHOD(device_attach,	ofwd_attach),
	{ 0, 0 }
};

static driver_t ofwd_driver = {
	"ofwd",
	ofwd_methods,
	sizeof(struct ofwd_softc)
};

static devclass_t	ofwd_devclass;

DRIVER_MODULE(ofwd, nexus, ofwd_driver, ofwd_devclass, 0, 0);

/*
 * Disk device control interface.
 */
static d_open_t		ofwd_open;
static d_close_t	ofwd_close;
static d_strategy_t	ofwd_strategy;

#define	OFWD_CDEV_MAJOR	169

static struct cdevsw ofwd_cdevsw = {
	ofwd_open,
	ofwd_close,
	physread,
	physwrite,
	noioctl,
	nopoll,
	nommap,
	ofwd_strategy,
	"ofwd",
	OFWD_CDEV_MAJOR,
	nodump,
	nopsize,
	D_DISK
};

static struct cdevsw	ofwddisk_cdevsw;

/*
 * Handle open from generic layer.
 *
 * This is typically only called by the diskslice code and not for opens on
 * subdevices.
 */
static int
ofwd_open(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct	ofwd_softc *sc;

	sc = (struct ofwd_softc *)dev->si_drv1;
	if (sc == NULL)
		return (ENXIO);

	sc->ofwd_disk.d_sectorsize = OFWD_BLOCKSIZE;
	sc->ofwd_disk.d_mediasize = (off_t)33554432 * OFWD_BLOCKSIZE;
	/* XXX: probably don't need the next two */
	sc->ofwd_disk.d_fwsectors = 33554432;
	sc->ofwd_disk.d_fwheads = 1

	sc->ofwd_flags |= OFWD_OPEN;
	return (0);
}

/*
 * Handle last close of the disk device.
 */
static int
ofwd_close(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct	ofwd_softc *sc;

	sc = (struct ofwd_softc *)dev->si_drv1;

	if (sc == NULL)
		return (ENXIO);

	sc->ofwd_flags &= ~OFWD_OPEN;

	return (0);
}

/*
 * Handle an I/O request.
 */
static void
ofwd_strategy(struct bio *bp)
{
	struct	ofwd_softc *sc;
	long	r;

	sc = (struct ofwd_softc *)bp->bio_dev->si_drv1;

	if (sc == NULL) {
		bp->bio_error = EINVAL;
		bp->bio_flags |= BIO_ERROR;
		printf("ofwd: bio for invalid disk!\n");
		biodone(bp);
		return;
	}

	r = OF_seek(sc->ofwd_instance,
	    (u_quad_t)(bp->bio_blkno * OFWD_BLOCKSIZE));
	if (r == -1) {
		bp->bio_resid = bp->bio_bcount;
		bp->bio_error = EIO;
		bp->bio_flags |= BIO_ERROR;
		device_printf(sc->ofwd_dev, "seek failed\n");
		biodone(bp);
		return;
	}

	if (bp->bio_cmd == BIO_READ) {
		r = OF_read(sc->ofwd_instance, (void *)bp->bio_data,
		    bp->bio_bcount);
	} else {
		r = OF_write(sc->ofwd_instance, (void *)bp->bio_data,
			    bp->bio_bcount);
	}

	if (r < bp->bio_bcount) {
		device_printf(sc->ofwd_dev, "r (%ld) < bp->bio_bcount (%ld)\n",
		    r, bp->bio_bcount);
		if (r != -1)
			bp->bio_resid = bp->bio_bcount - r;
		bp->bio_error = EIO;
		bp->bio_flags |= BIO_ERROR;
	} else if (r > bp->bio_bcount)
		panic("ofwd: more bytes read/written than requested");

	bp->bio_resid -= r;
	biodone(bp);

	return;
}

/*
 * Probe for an OpenFirmware disk.
 */
static int
ofwd_probe(device_t dev)
{
	char		*type;

	type = nexus_get_device_type(dev);

	if (type == NULL || strcmp(type, "disk") != 0)
		return (ENXIO);

	device_set_desc(dev, "OpenFirmware disk");
	return (0);
}

static int
ofwd_attach(device_t dev)
{
	struct	ofwd_softc *sc;
	char	path[128];
	dev_t	dsk;

	sc = device_get_softc(dev);
	sc->ofwd_dev = dev;

	bzero(path, 128);
	OF_package_to_path(nexus_get_node(dev), path, 128);
	device_printf(dev, "located at %s\n", path);
	sc->ofwd_instance = OF_open(path);
	if (sc->ofwd_instance == -1) {
		device_printf(dev, "could not create instance\n");
		return (ENXIO);
	}

	dsk = disk_create(device_get_unit(dev), &sc->ofwd_disk, 0,
	    &ofwd_cdevsw, &ofwddisk_cdevsw);
	dsk->si_drv1 = sc;
	sc->ofwd_dev_t = dsk;

	dsk->si_iosize_max = PAGE_SIZE;

	return (0);
}

static void
ofwd_identify(driver_t *driver, device_t parent)
{
}
