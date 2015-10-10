/*-
 * Copyright (c) 2006 Warner Losh.  All rights reserved.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Generic IIC eeprom support, modeled after the AT24C family of products.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/sx.h>
#include <sys/uio.h>
#include <machine/bus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

#define	IIC_M_WR	0	/* write operation */
#define	MAX_RD_SZ	256	/* Largest read size we support */
#define MAX_WR_SZ	256	/* Largest write size we support */

struct icee_softc {
	device_t	sc_dev;		/* Myself */
	device_t	sc_busdev;	/* Parent bus */
	struct cdev	*cdev;		/* user interface */
	int		addr;
	int		size;		/* How big am I? */
	int		type;		/* What type 8 or 16 bit? */
	int		rd_sz;		/* What's the read page size */
	int		wr_sz;		/* What's the write page size */
};

#define CDEV2SOFTC(dev)		((dev)->si_drv1)

/* cdev routines */
static d_open_t icee_open;
static d_close_t icee_close;
static d_read_t icee_read;
static d_write_t icee_write;

static struct cdevsw icee_cdevsw =
{
	.d_version = D_VERSION,
	.d_flags = D_TRACKCLOSE,
	.d_open = icee_open,
	.d_close = icee_close,
	.d_read = icee_read,
	.d_write = icee_write
};

static int
icee_probe(device_t dev)
{

	device_set_desc(dev, "I2C EEPROM");
	return (BUS_PROBE_NOWILDCARD);
}

static int
icee_attach(device_t dev)
{
	struct icee_softc *sc = device_get_softc(dev);
	const char *dname;
	int dunit, err;

	sc->sc_dev = dev;
	sc->sc_busdev = device_get_parent(sc->sc_dev);
	sc->addr = iicbus_get_addr(dev);
	err = 0;
	dname = device_get_name(dev);
	dunit = device_get_unit(dev);
	resource_int_value(dname, dunit, "size", &sc->size);
	resource_int_value(dname, dunit, "type", &sc->type);
	resource_int_value(dname, dunit, "rd_sz", &sc->rd_sz);
	if (sc->rd_sz > MAX_RD_SZ)
		sc->rd_sz = MAX_RD_SZ;
	resource_int_value(dname, dunit, "wr_sz", &sc->wr_sz);
	if (bootverbose)
		device_printf(dev, "size: %d bytes bus_width: %d-bits\n",
		    sc->size, sc->type);
	sc->cdev = make_dev(&icee_cdevsw, device_get_unit(dev), UID_ROOT,
	    GID_WHEEL, 0600, "icee%d", device_get_unit(dev));
	if (sc->cdev == NULL) {
		err = ENOMEM;
		goto out;
	}
	sc->cdev->si_drv1 = sc;
out:
	return (err);
}

static int 
icee_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{

	return (0);
}

static int
icee_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{

	return (0);
}

static int
icee_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct icee_softc *sc;
	uint8_t addr[2];
	uint8_t data[MAX_RD_SZ];
	int error, i, len, slave;
	struct iic_msg msgs[2] = {
	     { 0, IIC_M_WR, 1, addr },
	     { 0, IIC_M_RD, 0, data },
	};

	sc = CDEV2SOFTC(dev);
	if (uio->uio_offset == sc->size)
		return (0);
	if (uio->uio_offset > sc->size)
		return (EIO);
	if (sc->type != 8 && sc->type != 16)
		return (EINVAL);
	error = iicbus_request_bus(sc->sc_busdev, sc->sc_dev, IIC_INTRWAIT);
	if (error!= 0)
		return (iic2errno(error));
	slave = error = 0;
	while (uio->uio_resid > 0) {
		if (uio->uio_offset >= sc->size)
			break;
		len = MIN(sc->rd_sz - (uio->uio_offset & (sc->rd_sz - 1)),
		    uio->uio_resid);
		switch (sc->type) {
		case 8:
			slave = (uio->uio_offset >> 7) | sc->addr;
			msgs[0].len = 1;
			msgs[1].len = len;
			addr[0] = uio->uio_offset & 0xff;
			break;
		case 16:
			slave = sc->addr | (uio->uio_offset >> 15);
			msgs[0].len = 2;
			msgs[1].len = len;
			addr[0] = (uio->uio_offset >> 8) & 0xff;
			addr[1] = uio->uio_offset & 0xff;
			break;
		}
		for (i = 0; i < 2; i++)
			msgs[i].slave = slave;
		error = iicbus_transfer(sc->sc_dev, msgs, 2);
		if (error) {
			error = iic2errno(error);
			break;
		}
		error = uiomove(data, len, uio);
		if (error)
			break;
	}
	iicbus_release_bus(sc->sc_busdev, sc->sc_dev);
	return (error);
}

/*
 * Write to the part.  We use three transfers here since we're actually
 * doing a write followed by a read to make sure that the write finished.
 * It is easier to encode the dummy read here than to break things up
 * into smaller chunks...
 */
static int
icee_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct icee_softc *sc;
	int error, len, slave, waitlimit;
	uint8_t data[MAX_WR_SZ + 2];
	struct iic_msg wr[1] = {
	     { 0, IIC_M_WR, 0, data },
	};
	struct iic_msg rd[1] = {
	     { 0, IIC_M_RD, 1, data },
	};

	sc = CDEV2SOFTC(dev);
	if (uio->uio_offset >= sc->size)
		return (EIO);
	if (sc->type != 8 && sc->type != 16)
		return (EINVAL);

	error = iicbus_request_bus(sc->sc_busdev, sc->sc_dev, IIC_INTRWAIT);
	if (error!= 0)
		return (iic2errno(error));
	slave = error = 0;
	while (uio->uio_resid > 0) {
		if (uio->uio_offset >= sc->size)
			break;
		len = MIN(sc->wr_sz - (uio->uio_offset & (sc->wr_sz - 1)),
		    uio->uio_resid);
		switch (sc->type) {
		case 8:
			slave = (uio->uio_offset >> 7) | sc->addr;
			wr[0].len = 1 + len;
			data[0] = uio->uio_offset & 0xff;
			break;
		case 16:
			slave = sc->addr | (uio->uio_offset >> 15);
			wr[0].len = 2 + len;
			data[0] = (uio->uio_offset >> 8) & 0xff;
			data[1] = uio->uio_offset & 0xff;
			break;
		}
		wr[0].slave = slave;
		error = uiomove(data + sc->type / 8, len, uio);
		if (error)
			break;
		error = iicbus_transfer(sc->sc_dev, wr, 1);
		if (error) {
			error = iic2errno(error);
			break;
		}
		/* Read after write to wait for write-done. */
		waitlimit = 10000;
		rd[0].slave = slave;
		do {
			error = iicbus_transfer(sc->sc_dev, rd, 1);
		} while (waitlimit-- > 0 && error != 0);
		if (error) {
			error = iic2errno(error);
			break;
		}
	}
	iicbus_release_bus(sc->sc_busdev, sc->sc_dev);
	return error;
}

static device_method_t icee_methods[] = {
	DEVMETHOD(device_probe,		icee_probe),
	DEVMETHOD(device_attach,	icee_attach),

	DEVMETHOD_END
};

static driver_t icee_driver = {
	"icee",
	icee_methods,
	sizeof(struct icee_softc),
};
static devclass_t icee_devclass;

DRIVER_MODULE(icee, iicbus, icee_driver, icee_devclass, 0, 0);
MODULE_VERSION(icee, 1);
MODULE_DEPEND(icee, iicbus, 1, 1, 1);
