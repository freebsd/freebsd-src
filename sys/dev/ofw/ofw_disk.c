/*-
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <geom/geom_disk.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/nexusvar.h>

#define	OFWD_BLOCKSIZE	512

struct ofwd_softc
{
	device_t	ofwd_dev;
	struct		disk *ofwd_disk;
	phandle_t	ofwd_package;
	ihandle_t	ofwd_instance;
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
static disk_strategy_t	ofwd_strategy;

/*
 * Handle an I/O request.
 */
static void
ofwd_strategy(struct bio *bp)
{
	struct	ofwd_softc *sc;
	long	r;

	sc = (struct ofwd_softc *)bp->bio_disk->d_drv1;

	if (sc == NULL) {
		printf("ofwd: bio for invalid disk!\n");
		biofinish(bp, NULL, EINVAL);
		return;
	}

	bp->bio_resid = bp->bio_bcount;

	r = OF_seek(sc->ofwd_instance, bp->bio_offset);
	if (r == -1) {
		device_printf(sc->ofwd_dev, "seek failed\n");
		biofinish(bp, NULL, EIO);
		return;
	}

	if (bp->bio_cmd == BIO_READ) {
		r = OF_read(sc->ofwd_instance, (void *)bp->bio_data,
		    bp->bio_bcount);
	} else {
		r = OF_write(sc->ofwd_instance, (void *)bp->bio_data,
			    bp->bio_bcount);
	}

	if (r > bp->bio_bcount)
		panic("ofwd: more bytes read/written than requested");
	if (r == -1) {
		device_printf(sc->ofwd_dev, "r (%ld) < bp->bio_bcount (%ld)\n",
		    r, bp->bio_bcount);
		biofinish(bp, NULL, EIO);
		return;
	}

	bp->bio_resid -= r;

	if (r < bp->bio_bcount) {
		device_printf(sc->ofwd_dev, "r (%ld) < bp->bio_bcount (%ld)\n",
		    r, bp->bio_bcount);
		biofinish(bp, NULL, EIO);	/* XXX: probably not an error */
		return;
	}
	biodone(bp);
	return;
}

/*
 * Attach the Open Firmware disk to nexus if present.
 */
static void
ofwd_identify(driver_t *driver, device_t parent)
{
	device_t child;
	phandle_t ofd;
	static char type[8];

	ofd = OF_finddevice("ofwdisk");
	if (ofd == -1)
		return;

	OF_getprop(ofd, "device_type", type, sizeof(type));

	child = BUS_ADD_CHILD(parent, INT_MAX, "ofwd", 0);
	if (child != NULL) {
		nexus_set_device_type(child, type);
		nexus_set_node(child, ofd);
	}
}

/*
 * Probe for an Open Firmware disk.
 */
static int
ofwd_probe(device_t dev)
{
	char		*type;
	char		fname[32];
	phandle_t	node;

	type = nexus_get_device_type(dev);
	node = nexus_get_node(dev);

	if (type == NULL ||
	    (strcmp(type, "disk") != 0 && strcmp(type, "block") != 0))
		return (ENXIO);

	if (OF_getprop(node, "file", fname, sizeof(fname)) == -1)
		return (ENXIO);

	device_set_desc(dev, "Open Firmware disk");
	return (0);
}

static int
ofwd_attach(device_t dev)
{
	struct	ofwd_softc *sc;
	char	path[128];
	char	fname[32];

	sc = device_get_softc(dev);
	sc->ofwd_dev = dev;

	bzero(path, 128);
	OF_package_to_path(nexus_get_node(dev), path, 128);
	OF_getprop(nexus_get_node(dev), "file", fname, sizeof(fname));
	device_printf(dev, "located at %s, file %s\n", path, fname);
	sc->ofwd_instance = OF_open(path);
	if (sc->ofwd_instance == -1) {
		device_printf(dev, "could not create instance\n");
		return (ENXIO);
	}

	sc->ofwd_disk = disk_alloc();
	sc->ofwd_disk->d_strategy = ofwd_strategy;
	sc->ofwd_disk->d_name = "ofwd";
	sc->ofwd_disk->d_sectorsize = OFWD_BLOCKSIZE;
	sc->ofwd_disk->d_mediasize = (off_t)33554432 * OFWD_BLOCKSIZE;
	sc->ofwd_disk->d_fwsectors = 0;
	sc->ofwd_disk->d_fwheads = 0;
	sc->ofwd_disk->d_drv1 = sc;
	sc->ofwd_disk->d_maxsize = PAGE_SIZE;
	sc->ofwd_disk->d_unit = device_get_unit(dev);
	sc->ofwd_disk->d_flags = DISKFLAG_NEEDSGIANT;
	disk_create(sc->ofwd_disk, DISK_VERSION);

	return (0);
}
