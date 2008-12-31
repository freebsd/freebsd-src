/*-
 * Copyright (c) 1998, 2001 Nicolas Souchu
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
 * $FreeBSD: src/sys/dev/iicbus/iic.c,v 1.39.6.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/fcntl.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iic.h>

#include "iicbus_if.h"

#define BUFSIZE 1024

struct iic_softc {

	u_char sc_addr;			/* 7 bit address on iicbus */
	int sc_count;			/* >0 if device opened */

	char sc_buffer[BUFSIZE];	/* output buffer */
	char sc_inbuf[BUFSIZE];		/* input buffer */

	struct cdev *sc_devnode;
};

#define IIC_SOFTC(unit) \
	((struct iic_softc *)devclass_get_softc(iic_devclass, (unit)))

#define IIC_DEVICE(unit) \
	(devclass_get_device(iic_devclass, (unit)))

static int iic_probe(device_t);
static int iic_attach(device_t);
static int iic_detach(device_t);
static void iic_identify(driver_t *driver, device_t parent);

static devclass_t iic_devclass;

static device_method_t iic_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	iic_identify),
	DEVMETHOD(device_probe,		iic_probe),
	DEVMETHOD(device_attach,	iic_attach),
	DEVMETHOD(device_detach,	iic_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_intr,		iicbus_generic_intr),

	{ 0, 0 }
};

static driver_t iic_driver = {
	"iic",
	iic_methods,
	sizeof(struct iic_softc),
};

static	d_open_t	iicopen;
static	d_close_t	iicclose;
static	d_write_t	iicwrite;
static	d_read_t	iicread;
static	d_ioctl_t	iicioctl;

static struct cdevsw iic_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	iicopen,
	.d_close =	iicclose,
	.d_read =	iicread,
	.d_write =	iicwrite,
	.d_ioctl =	iicioctl,
	.d_name =	"iic",
};

static void
iic_identify(driver_t *driver, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "iic", -1);
}

static int
iic_probe(device_t dev)
{
	device_set_desc(dev, "I2C generic I/O");
	return (0);
}
	
static int
iic_attach(device_t dev)
{
	struct iic_softc *sc = (struct iic_softc *)device_get_softc(dev);

	sc->sc_devnode = make_dev(&iic_cdevsw, device_get_unit(dev),
			UID_ROOT, GID_WHEEL,
			0600, "iic%d", device_get_unit(dev));
	return (0);
}

static int
iic_detach(device_t dev)
{
	struct iic_softc *sc = (struct iic_softc *)device_get_softc(dev);

	if (sc->sc_devnode)
		destroy_dev(sc->sc_devnode);

	return (0);
}

static int
iicopen(struct cdev *dev, int flags, int fmt, struct thread *td)
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
iicclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct iic_softc *sc = IIC_SOFTC(minor(dev));

	if (!sc)
		return (EINVAL);

	if (!sc->sc_count)
		return (EINVAL);

	sc->sc_count--;

	if (sc->sc_count < 0)
		panic("%s: iic_count < 0!", __func__);

	return (0);
}

static int
iicwrite(struct cdev *dev, struct uio * uio, int ioflag)
{
	device_t iicdev = IIC_DEVICE(minor(dev));
	struct iic_softc *sc = IIC_SOFTC(minor(dev));
	int sent, error, count;

	if (!sc || !iicdev || !sc->sc_addr)
		return (EINVAL);

	if (sc->sc_count == 0)
		return (EINVAL);

	if ((error = iicbus_request_bus(device_get_parent(iicdev), iicdev, IIC_DONTWAIT)))
		return (error);

	count = min(uio->uio_resid, BUFSIZE);
	uiomove(sc->sc_buffer, count, uio);

	error = iicbus_block_write(device_get_parent(iicdev), sc->sc_addr,
					sc->sc_buffer, count, &sent);

	iicbus_release_bus(device_get_parent(iicdev), iicdev);

	return(error);
}

static int
iicread(struct cdev *dev, struct uio * uio, int ioflag)
{
	device_t iicdev = IIC_DEVICE(minor(dev));
	struct iic_softc *sc = IIC_SOFTC(minor(dev));
	int len, error = 0;
	int bufsize;

	if (!sc || !iicdev || !sc->sc_addr)
		return (EINVAL);

	if (sc->sc_count == 0)
		return (EINVAL);

	if ((error = iicbus_request_bus(device_get_parent(iicdev), iicdev, IIC_DONTWAIT)))
		return (error);

	/* max amount of data to read */
	len = min(uio->uio_resid, BUFSIZE);

	if ((error = iicbus_block_read(device_get_parent(iicdev), sc->sc_addr,
					sc->sc_inbuf, len, &bufsize)))
		return (error);

	if (bufsize > uio->uio_resid)
		panic("%s: too much data read!", __func__);

	iicbus_release_bus(device_get_parent(iicdev), iicdev);

	return (uiomove(sc->sc_inbuf, bufsize, uio));
}

static int
iicioctl(struct cdev *dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	device_t iicdev = IIC_DEVICE(minor(dev));
	struct iic_softc *sc = IIC_SOFTC(minor(dev));
	device_t parent = device_get_parent(iicdev);
	struct iiccmd *s = (struct iiccmd *)data;
	struct iic_rdwr_data *d = (struct iic_rdwr_data *)data;
	struct iic_msg *m;
	int error, count, i;
	char *buf = NULL;
	void **usrbufs = NULL;

	if (!sc)
		return (EINVAL);

	if ((error = iicbus_request_bus(parent, iicdev,
	    (flags & O_NONBLOCK) ? IIC_DONTWAIT : (IIC_WAIT | IIC_INTR))))
		return (error);

	switch (cmd) {
	case I2CSTART:
		error = iicbus_start(parent, s->slave, 0);

		/*
		 * Implicitly set the chip addr to the slave addr passed as
		 * parameter. Consequently, start/stop shall be called before
		 * the read or the write of a block.
		 */
		if (!error)
			sc->sc_addr = s->slave;

		break;

	case I2CSTOP:
		error = iicbus_stop(parent);
		break;

	case I2CRSTCARD:
		error = iicbus_reset(parent, IIC_UNKNOWN, 0, NULL);
		break;

	case I2CWRITE:
		if (s->count <= 0) {
			error = EINVAL;
			break;
		}
		buf = malloc((unsigned long)s->count, M_TEMP, M_WAITOK);
		error = copyin(s->buf, buf, s->count);
		if (error)
			break;
		error = iicbus_write(parent, buf, s->count, &count, 10);
		break;

	case I2CREAD:
		if (s->count <= 0) {
			error = EINVAL;
			break;
		}
		buf = malloc((unsigned long)s->count, M_TEMP, M_WAITOK);
		error = iicbus_read(parent, buf, s->count, &count, s->last, 10);
		if (error)
			break;
		error = copyout(buf, s->buf, s->count);
		break;

	case I2CRDWR:
		buf = malloc(sizeof(*d->msgs) * d->nmsgs, M_TEMP, M_WAITOK);
		usrbufs = malloc(sizeof(void *) * d->nmsgs, M_TEMP, M_ZERO | M_WAITOK);
		error = copyin(d->msgs, buf, sizeof(*d->msgs) * d->nmsgs);
		if (error)
			break;
		/* Alloc kernel buffers for userland data, copyin write data */
		for (i = 0; i < d->nmsgs; i++) {
			m = &((struct iic_msg *)buf)[i];
			usrbufs[i] = m->buf;
			m->buf = malloc(m->len, M_TEMP, M_WAITOK);
			if (!(m->flags & IIC_M_RD))
				copyin(usrbufs[i], m->buf, m->len);
		}
		error = iicbus_transfer(iicdev, (struct iic_msg *)buf, d->nmsgs);
		/* Copyout all read segments, free up kernel buffers */
		for (i = 0; i < d->nmsgs; i++) {
			m = &((struct iic_msg *)buf)[i];
			if (m->flags & IIC_M_RD)
				copyout(m->buf, usrbufs[i], m->len);
			free(m->buf, M_TEMP);
		}
		free(usrbufs, M_TEMP);
		break;
	default:
		error = ENOTTY;
	}

	iicbus_release_bus(parent, iicdev);

	if (buf != NULL)
		free(buf, M_TEMP);
	return (error);
}

DRIVER_MODULE(iic, iicbus, iic_driver, iic_devclass, 0, 0);
MODULE_DEPEND(iic, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(iic, 1);
