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
 * $FreeBSD: src/sys/dev/smbus/smb.c,v 1.20 1999/11/18 05:44:56 peter Exp $
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
#include <sys/fcntl.h>

#include <machine/clock.h>

#include <dev/smbus/smbconf.h>
#include <dev/smbus/smbus.h>
#include <machine/smb.h>

#include "smbus_if.h"

#define BUFSIZE 1024

struct smb_softc {

	int sc_addr;			/* address on smbus */
	int sc_count;			/* >0 if device opened */

	char *sc_cp;			/* output buffer pointer */

	char sc_buffer[BUFSIZE];	/* output buffer */
	char sc_inbuf[BUFSIZE];		/* input buffer */
};

#define IIC_SOFTC(unit) \
	((struct smb_softc *)devclass_get_softc(smb_devclass, (unit)))

#define IIC_DEVICE(unit) \
	(devclass_get_device(smb_devclass, (unit)))

static int smb_probe(device_t);
static int smb_attach(device_t);

static devclass_t smb_devclass;

static device_method_t smb_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		smb_probe),
	DEVMETHOD(device_attach,	smb_attach),

	/* smbus interface */
	DEVMETHOD(smbus_intr,		smbus_generic_intr),

	{ 0, 0 }
};

static driver_t smb_driver = {
	"smb",
	smb_methods,
	sizeof(struct smb_softc),
};

static	d_open_t	smbopen;
static	d_close_t	smbclose;
static	d_write_t	smbwrite;
static	d_read_t	smbread;
static	d_ioctl_t	smbioctl;

#define CDEV_MAJOR 106
static struct cdevsw smb_cdevsw = {
	/* open */	smbopen,
	/* close */	smbclose,
	/* read */	smbread,
	/* write */	smbwrite,
	/* ioctl */	smbioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"smb",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

/*
 * smbprobe()
 */
static int
smb_probe(device_t dev)
{
	struct smb_softc *sc = (struct smb_softc *)device_get_softc(dev);

	sc->sc_addr = smbus_get_addr(dev);

	/* XXX detect chip with start/stop conditions */

	return (0);
}
	
/*
 * smbattach()
 */
static int
smb_attach(device_t dev)
{
	make_dev(&smb_cdevsw, device_get_unit(dev),	/* XXX cleanup */
			UID_ROOT, GID_WHEEL,
			0600, "smb%d", device_get_unit(dev));
	return (0);
}

static int
smbopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	struct smb_softc *sc = IIC_SOFTC(minor(dev));

	if (!sc)
		return (EINVAL);

	if (sc->sc_count)
		return (EBUSY);

	sc->sc_count++;

	return (0);
}

static int
smbclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct smb_softc *sc = IIC_SOFTC(minor(dev));

	if (!sc)
		return (EINVAL);

	if (!sc->sc_count)
		return (EINVAL);

	sc->sc_count--;

	return (0);
}

static int
smbwrite(dev_t dev, struct uio * uio, int ioflag)
{
	/* not supported */

	return (EINVAL);
}

static int
smbread(dev_t dev, struct uio * uio, int ioflag)
{
	/* not supported */

	return (EINVAL);
}

static int
smbioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	device_t smbdev = IIC_DEVICE(minor(dev));
	struct smb_softc *sc = IIC_SOFTC(minor(dev));
	device_t parent = device_get_parent(smbdev);

	int error = 0;
	struct smbcmd *s = (struct smbcmd *)data;

	if (!sc || !s)
		return (EINVAL);

	/* allocate the bus */
	if ((error = smbus_request_bus(parent, smbdev,
			(flags & O_NONBLOCK) ? SMB_DONTWAIT : (SMB_WAIT | SMB_INTR))))
		return (error);

	switch (cmd) {
	case SMB_QUICK_WRITE:
		error = smbus_error(smbus_quick(parent, s->slave, SMB_QWRITE));
		break;

	case SMB_QUICK_READ:
		error = smbus_error(smbus_quick(parent, s->slave, SMB_QREAD));
		break;

	case SMB_SENDB:
		error = smbus_error(smbus_sendb(parent, s->slave, s->cmd));
		break;

	case SMB_RECVB:
		error = smbus_error(smbus_recvb(parent, s->slave, &s->cmd));
		break;

	case SMB_WRITEB:
		error = smbus_error(smbus_writeb(parent, s->slave, s->cmd,
						s->data.byte));
		break;

	case SMB_WRITEW:
		error = smbus_error(smbus_writew(parent, s->slave,
						s->cmd, s->data.word));
		break;

	case SMB_READB:
		if (s->data.byte_ptr)
			error = smbus_error(smbus_readb(parent, s->slave,
						s->cmd, s->data.byte_ptr));
		break;

	case SMB_READW:
		if (s->data.word_ptr)
			error = smbus_error(smbus_readw(parent, s->slave,
						s->cmd, s->data.word_ptr));
		break;

	case SMB_PCALL:
		if (s->data.process.rdata)
			error = smbus_error(smbus_pcall(parent, s->slave, s->cmd,
				s->data.process.sdata, s->data.process.rdata));
		break;

	case SMB_BWRITE:
		if (s->count && s->data.byte_ptr)
			error = smbus_error(smbus_bwrite(parent, s->slave,
						s->cmd, s->count, s->data.byte_ptr));
		break;

	case SMB_BREAD:
		if (s->count && s->data.byte_ptr)
			error = smbus_error(smbus_bread(parent, s->slave,
						s->cmd, s->count, s->data.byte_ptr));
		break;
		
	default:
		error = ENODEV;
	}

	/* release the bus */
	smbus_release_bus(parent, smbdev);

	return (error);
}

DRIVER_MODULE(smb, smbus, smb_driver, smb_devclass, 0, 0);
