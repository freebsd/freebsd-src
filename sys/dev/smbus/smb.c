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
 *	$Id: smb.c,v 1.1.2.1 1998/08/13 15:15:19 son Exp $
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
	DRIVER_TYPE_MISC,
	sizeof(struct smb_softc),
};

static	d_open_t	smbopen;
static	d_close_t	smbclose;
static	d_write_t	smbwrite;
static	d_read_t	smbread;
static	d_ioctl_t	smbioctl;

#define CDEV_MAJOR 73
static struct cdevsw smb_cdevsw = 
	{ smbopen,	smbclose,	smbread,	smbwrite,	/*73*/
	  smbioctl,	nullstop,	nullreset,	nodevtotty,	/*smb*/
	  seltrue,	nommap,		nostrat,	"smb",	NULL,	-1 };

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
	struct smb_softc *sc = (struct smb_softc *)device_get_softc(dev);

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
	device_t smbdev = IIC_DEVICE(minor(dev));
	struct smb_softc *sc = IIC_SOFTC(minor(dev));
	int error, count;

	if (!sc || !smbdev)
		return (EINVAL);

	if (sc->sc_count == 0)
		return (EINVAL);

	count = min(uio->uio_resid, BUFSIZE);
	uiomove(sc->sc_buffer, count, uio);

	/* we consider the command char as the first character to send */
	smbus_bwrite(device_get_parent(smbdev), sc->sc_addr,
				sc->sc_buffer[0], count-1, sc->sc_buffer+1);

	return (0);
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

	if (!sc)
		return (EINVAL);

	switch (cmd) {
	case SMB_QUICK_WRITE:
		smbus_quick(parent, sc->sc_addr, SMB_QWRITE);
		goto end;

	case SMB_QUICK_READ:
		smbus_quick(parent, sc->sc_addr, SMB_QREAD);
		goto end;
	};

	if (!s)
		return (EINVAL);

	switch (cmd) {
	case SMB_SENDB:
		smbus_sendb(parent, sc->sc_addr, s->cmd);
		break;

	case SMB_RECVB:
		smbus_recvb(parent, sc->sc_addr, &s->cmd);
		break;

	case SMB_WRITEB:
		smbus_writeb(parent, sc->sc_addr, s->cmd, s->data.byte);
		break;

	case SMB_WRITEW:
		smbus_writew(parent, sc->sc_addr, s->cmd, s->data.word);
		break;

	case SMB_READB:
		if (s->data.byte_ptr)
			smbus_readb(parent, sc->sc_addr, s->cmd,
							s->data.byte_ptr);
		break;

	case SMB_READW:
		if (s->data.word_ptr)
			smbus_readw(parent, sc->sc_addr, s->cmd, s->data.word_ptr);
		break;

	case SMB_PCALL:
		if (s->data.process.rdata)
			smbus_pcall(parent, sc->sc_addr, s->cmd,
				s->data.process.sdata, s->data.process.rdata);
		break;

	case SMB_BWRITE:
		if (s->count && s->data.byte_ptr)
			smbus_bwrite(parent, sc->sc_addr, s->cmd, s->count,
							s->data.byte_ptr);
		break;

	case SMB_BREAD:
		if (s->count && s->data.byte_ptr)
			smbus_bread(parent, sc->sc_addr, s->cmd, s->count,
							s->data.byte_ptr);
		break;
		
	default:
		error = ENODEV;
	}

end:
	return (error);
}

static int smb_devsw_installed = 0;

static void
smb_drvinit(void *unused)
{
        dev_t dev;

        if( ! smb_devsw_installed ) {
                dev = makedev(CDEV_MAJOR,0);
                cdevsw_add(&dev,&smb_cdevsw,NULL);
                smb_devsw_installed = 1;
        }
}

CDEV_DRIVER_MODULE(smb, smbus, smb_driver, smb_devclass, CDEV_MAJOR,
			smb_cdevsw, 0, 0);

SYSINIT(smbdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,smb_drvinit,NULL)
