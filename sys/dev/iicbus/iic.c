/*-
 * Copyright (c) 1998 Nicolas Souchu
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
 *	$Id: iic.c,v 1.1.2.9 1998/08/13 17:10:42 son Exp $
 *
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <machine/clock.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <machine/iic.h>

#include "iicbus_if.h"

#define BUFSIZE 1024

struct iic_softc {

	u_char sc_addr;			/* address on iicbus */
	int sc_count;			/* >0 if device opened */

	char sc_buffer[BUFSIZE];	/* output buffer */
	char sc_inbuf[BUFSIZE];		/* input buffer */
};

#define IIC_SOFTC(unit) \
	((struct iic_softc *)devclass_get_softc(iic_devclass, (unit)))

#define IIC_DEVICE(unit) \
	(devclass_get_device(iic_devclass, (unit)))

static int iic_probe(device_t);
static int iic_attach(device_t);

static devclass_t iic_devclass;

static device_method_t iic_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		iic_probe),
	DEVMETHOD(device_attach,	iic_attach),

	/* iicbus interface */
	DEVMETHOD(iicbus_intr,		iicbus_generic_intr),

	{ 0, 0 }
};

static driver_t iic_driver = {
	"iic",
	iic_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct iic_softc),
};

static	d_open_t	iicopen;
static	d_close_t	iicclose;
static	d_write_t	iicwrite;
static	d_read_t	iicread;
static	d_ioctl_t	iicioctl;

#define CDEV_MAJOR 15
static struct cdevsw iic_cdevsw = 
	{ iicopen,	iicclose,	iicread,	iicwrite,	/*15*/
	  iicioctl,	nullstop,	nullreset,	nodevtotty,	/*iic*/
	  seltrue,	nommap,		nostrat,	"iic",	NULL,	-1 };

/*
 * iicprobe()
 */
static int
iic_probe(device_t dev)
{
	struct iic_softc *sc = (struct iic_softc *)device_get_softc(dev);

	sc->sc_addr = iicbus_get_addr(dev);

	/* XXX detect chip with start/stop conditions */

	return (0);
}
	
/*
 * iicattach()
 */
static int
iic_attach(device_t dev)
{
	struct iic_softc *sc = (struct iic_softc *)device_get_softc(dev);

	return (0);
}

static int
iicopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	struct iic_softc *sc = IIC_SOFTC(minor(dev));

	if (!sc)
		return (EINVAL);

	if (sc->sc_count > 0)
		return (EBUSY);

	sc->sc_count++;

	return (0);
}

static int
iicclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct iic_softc *sc = IIC_SOFTC(minor(dev));

	if (!sc)
		return (EINVAL);

	if (!sc->sc_count)
		return (EINVAL);

	sc->sc_count--;

	if (sc->sc_count < 0)
		panic("%s: iic_count < 0!", __FUNCTION__);

	return (0);
}

static int
iicwrite(dev_t dev, struct uio * uio, int ioflag)
{
	device_t iicdev = IIC_DEVICE(minor(dev));
	struct iic_softc *sc = IIC_SOFTC(minor(dev));
	int sent, error, count;

	if (!sc || !iicdev)
		return (EINVAL);

	if (sc->sc_count == 0)
		return (EINVAL);

	count = min(uio->uio_resid, BUFSIZE);
	uiomove(sc->sc_buffer, count, uio);

	error = iicbus_block_write(device_get_parent(iicdev), sc->sc_addr,
					sc->sc_buffer, count, &sent);

	return(error);
}

static int
iicread(dev_t dev, struct uio * uio, int ioflag)
{
	device_t iicdev = IIC_DEVICE(minor(dev));
	struct iic_softc *sc = IIC_SOFTC(minor(dev));
	int len, error = 0;
	int bufsize;

	if (!sc || !iicdev)
		return (EINVAL);

	if (sc->sc_count == 0)
		return (EINVAL);

	/* max amount of data to read */
	len = min(uio->uio_resid, BUFSIZE);

	if ((error = iicbus_block_read(device_get_parent(iicdev), sc->sc_addr,
					sc->sc_inbuf, len, &bufsize)))
		return (error);

	if (bufsize > uio->uio_resid)
		panic("%s: too much data read!", __FUNCTION__);

	return (uiomove(sc->sc_inbuf, bufsize, uio));
}

static int
iicioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	device_t iicdev = IIC_DEVICE(minor(dev));
	struct iic_softc *sc = IIC_SOFTC(minor(dev));
	int error;
	device_t parent = device_get_parent(iicdev);

	if (!sc)
		return (EINVAL);

	switch (cmd) {
	case I2CSTART:
		error = iicbus_start(parent, sc->sc_addr);
		break;

	case I2CSTOP:
		error = iicbus_stop(parent);
		break;

	case I2CRSTCARD:
		error = iicbus_reset(parent, 0);
		break;

	default:
		error = ENODEV;
	}

	return (error);
}

static int iic_devsw_installed = 0;

static void
iic_drvinit(void *unused)
{
        dev_t dev;

        if( ! iic_devsw_installed ) {
                dev = makedev(CDEV_MAJOR,0);
                cdevsw_add(&dev,&iic_cdevsw,NULL);
                iic_devsw_installed = 1;
        }
}

CDEV_DRIVER_MODULE(iic, iicbus, iic_driver, iic_devclass, CDEV_MAJOR,
			iic_cdevsw, 0, 0);

SYSINIT(iicdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,iic_drvinit,NULL)
