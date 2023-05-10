/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Justin Hibbits
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/uio.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include "opal.h"

#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#define	NVRAM_BUFSIZE	(65536)	/* 64k blocks */

struct opal_nvram_softc {
	device_t	 sc_dev;
	struct mtx	 sc_mtx;
	uint32_t	 sc_size;
	uint8_t		*sc_buf;
	vm_paddr_t	 sc_buf_phys;

	struct cdev 	*sc_cdev;
	int		 sc_isopen;
};

#define	NVRAM_LOCK(sc)		mtx_lock(&sc->sc_mtx)
#define	NVRAM_UNLOCK(sc)	mtx_unlock(&sc->sc_mtx)

/*
 * Device interface.
 */
static int		opal_nvram_probe(device_t);
static int		opal_nvram_attach(device_t);
static int		opal_nvram_detach(device_t);

/*
 * Driver methods.
 */
static device_method_t	opal_nvram_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		opal_nvram_probe),
	DEVMETHOD(device_attach,	opal_nvram_attach),
	DEVMETHOD(device_detach,	opal_nvram_detach),
	{ 0, 0 }
};

static driver_t	opal_nvram_driver = {
	"opal_nvram",
	opal_nvram_methods,
	sizeof(struct opal_nvram_softc)
};

DRIVER_MODULE(opal_nvram, opal, opal_nvram_driver, 0, 0);

/*
 * Cdev methods.
 */

static	d_open_t	opal_nvram_open;
static	d_close_t	opal_nvram_close;
static	d_read_t	opal_nvram_read;
static	d_write_t	opal_nvram_write;
static	d_ioctl_t	opal_nvram_ioctl;

static struct cdevsw opal_nvram_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	opal_nvram_open,
	.d_close =	opal_nvram_close,
	.d_read =	opal_nvram_read,
	.d_write =	opal_nvram_write,
	.d_ioctl =	opal_nvram_ioctl,
	.d_name =	"nvram",
};

static int
opal_nvram_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "ibm,opal-nvram"))
		return (ENXIO);

	device_set_desc(dev, "OPAL NVRAM");
	return (BUS_PROBE_DEFAULT);
}

static int
opal_nvram_attach(device_t dev)
{
	struct opal_nvram_softc *sc;
	phandle_t node;
	int err;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);

	sc->sc_dev = dev;

	err = OF_getencprop(node, "#bytes", &sc->sc_size,
	    sizeof(sc->sc_size));

	if (err < 0)
		return (ENXIO);

	sc->sc_buf = contigmalloc(NVRAM_BUFSIZE, M_DEVBUF, M_WAITOK,
	    0, BUS_SPACE_MAXADDR, PAGE_SIZE, 0);
	if (sc->sc_buf == NULL) {
		device_printf(dev, "No memory for buffer.\n");
		return (ENXIO);
	}
	sc->sc_buf_phys = pmap_kextract((vm_offset_t)sc->sc_buf);
	sc->sc_cdev = make_dev(&opal_nvram_cdevsw, 0, 0, 0, 0600,
	    "nvram");
	sc->sc_cdev->si_drv1 = sc;

	mtx_init(&sc->sc_mtx, "opal_nvram", 0, MTX_DEF);

	return (0);
}

static int
opal_nvram_detach(device_t dev)
{
	struct opal_nvram_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_cdev != NULL)
		destroy_dev(sc->sc_cdev);
	if (sc->sc_buf != NULL)
		contigfree(sc->sc_buf, NVRAM_BUFSIZE, M_DEVBUF);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static int
opal_nvram_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct opal_nvram_softc *sc = dev->si_drv1;
	int err;

	err = 0;

	NVRAM_LOCK(sc);
	if (sc->sc_isopen)
		err = EBUSY;
	else
		sc->sc_isopen = 1;
	NVRAM_UNLOCK(sc);

	return (err);
}

static int
opal_nvram_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct opal_nvram_softc *sc = dev->si_drv1;

	NVRAM_LOCK(sc);
	sc->sc_isopen = 0;
	NVRAM_UNLOCK(sc);

	return (0);
}

static int
opal_nvram_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct opal_nvram_softc *sc = dev->si_drv1;
	int rv, amnt;

	rv = 0;

	NVRAM_LOCK(sc);
	while (uio->uio_resid > 0) {
		amnt = MIN(uio->uio_resid, sc->sc_size - uio->uio_offset);
		amnt = MIN(amnt, NVRAM_BUFSIZE);
		if (amnt == 0)
			break;

		rv = opal_call(OPAL_READ_NVRAM, sc->sc_buf_phys,
		    amnt, uio->uio_offset);
		if (rv != OPAL_SUCCESS) {
			switch (rv) {
			case OPAL_HARDWARE:
				rv = EIO;
				break;
			case OPAL_PARAMETER:
				rv = EINVAL;
				break;
			}
			break;
		}
		rv = uiomove(sc->sc_buf, amnt, uio);
		if (rv != 0)
			break;
	}
	NVRAM_UNLOCK(sc);

	return (rv);
}

static int
opal_nvram_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	off_t offset;
	int rv, amnt;
	struct opal_nvram_softc *sc = dev->si_drv1;

	rv = 0;

	NVRAM_LOCK(sc);
	while (uio->uio_resid > 0) {
		amnt = MIN(uio->uio_resid, sc->sc_size - uio->uio_offset);
		amnt = MIN(amnt, NVRAM_BUFSIZE);
		if (amnt == 0) {
			rv = ENOSPC;
			break;
		}
		offset = uio->uio_offset;
		rv = uiomove(sc->sc_buf, amnt, uio);
		if (rv != 0)
			break;
		rv = opal_call(OPAL_WRITE_NVRAM, sc->sc_buf_phys, amnt,
		    offset);
		if (rv != OPAL_SUCCESS) {
			switch (rv) {
			case OPAL_HARDWARE:
				rv = EIO;
				break;
			case OPAL_PARAMETER:
				rv = EINVAL;
				break;
			}
			break;
		}
	}

	NVRAM_UNLOCK(sc);

	return (rv);
}

static int
opal_nvram_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct opal_nvram_softc *sc = dev->si_drv1;

	switch (cmd) {
	case DIOCGMEDIASIZE:
		*(off_t *)data = sc->sc_size;
		return (0);
	}
	return (EINVAL);
}
