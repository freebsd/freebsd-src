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
 * $FreeBSD$
 */

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_compat.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/fcntl.h>

#include <dev/smbus/smbconf.h>
#include <dev/smbus/smbus.h>
#include <dev/smbus/smb.h>

#include "smbus_if.h"

#define BUFSIZE 1024

struct smb_softc {
	device_t sc_dev;
	int sc_count;			/* >0 if device opened */
	struct cdev *sc_devnode;
	struct mtx sc_lock;
};

static void smb_identify(driver_t *driver, device_t parent);
static int smb_probe(device_t);
static int smb_attach(device_t);
static int smb_detach(device_t);

static devclass_t smb_devclass;

static device_method_t smb_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	smb_identify),
	DEVMETHOD(device_probe,		smb_probe),
	DEVMETHOD(device_attach,	smb_attach),
	DEVMETHOD(device_detach,	smb_detach),

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
static	d_ioctl_t	smbioctl;

static struct cdevsw smb_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE,
	.d_open =	smbopen,
	.d_close =	smbclose,
	.d_ioctl =	smbioctl,
	.d_name =	"smb",
};

static void
smb_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "smb", -1) == NULL)
		BUS_ADD_CHILD(parent, 0, "smb", -1);
}

static int
smb_probe(device_t dev)
{
	device_set_desc(dev, "SMBus generic I/O");

	return (0);
}
	
static int
smb_attach(device_t dev)
{
	struct smb_softc *sc = device_get_softc(dev);

	sc->sc_dev = dev;
	sc->sc_devnode = make_dev(&smb_cdevsw, device_get_unit(dev),
	    UID_ROOT, GID_WHEEL, 0600, "smb%d", device_get_unit(dev));
	sc->sc_devnode->si_drv1 = sc;
	mtx_init(&sc->sc_lock, device_get_nameunit(dev), NULL, MTX_DEF);

	return (0);
}

static int
smb_detach(device_t dev)
{
	struct smb_softc *sc = (struct smb_softc *)device_get_softc(dev);

	if (sc->sc_devnode)
		destroy_dev(sc->sc_devnode);
	mtx_destroy(&sc->sc_lock);

	return (0);
}

static int
smbopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct smb_softc *sc = dev->si_drv1;

	mtx_lock(&sc->sc_lock);
	if (sc->sc_count != 0) {
		mtx_unlock(&sc->sc_lock);
		return (EBUSY);
	}

	sc->sc_count++;
	mtx_unlock(&sc->sc_lock);

	return (0);
}

static int
smbclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct smb_softc *sc = dev->si_drv1;

	mtx_lock(&sc->sc_lock);
	KASSERT(sc->sc_count == 1, ("device not busy"));
	sc->sc_count--;
	mtx_unlock(&sc->sc_lock);

	return (0);
}

static int
smbioctl(struct cdev *dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	char buf[SMB_MAXBLOCKSIZE];
	device_t parent;
	struct smbcmd *s = (struct smbcmd *)data;
	struct smb_softc *sc = dev->si_drv1;
	device_t smbdev = sc->sc_dev;
	int error;
	short w;
	u_char count;
	char c;

	parent = device_get_parent(smbdev);

	/* Make sure that LSB bit is cleared. */
	if (s->slave & 0x1)
		return (EINVAL);

	/* Allocate the bus. */
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
		if (s->data.byte_ptr) {
			error = smbus_error(smbus_readb(parent, s->slave,
						s->cmd, &c));
			if (error)
				break;
			error = copyout(&c, s->data.byte_ptr,
					sizeof(*(s->data.byte_ptr)));
		}
		break;

	case SMB_READW:
		if (s->data.word_ptr) {
			error = smbus_error(smbus_readw(parent, s->slave,
						s->cmd, &w));
			if (error == 0) {
				error = copyout(&w, s->data.word_ptr,
						sizeof(*(s->data.word_ptr)));
			}
		}
		break;

	case SMB_PCALL:
		if (s->data.process.rdata) {

			error = smbus_error(smbus_pcall(parent, s->slave, s->cmd,
				s->data.process.sdata, &w));
			if (error)
				break;
			error = copyout(&w, s->data.process.rdata,
					sizeof(*(s->data.process.rdata)));
		}
		
		break;

	case SMB_BWRITE:
		if (s->count && s->data.byte_ptr) {
			if (s->count > SMB_MAXBLOCKSIZE)
				s->count = SMB_MAXBLOCKSIZE;
			error = copyin(s->data.byte_ptr, buf, s->count);
			if (error)
				break;
			error = smbus_error(smbus_bwrite(parent, s->slave,
						s->cmd, s->count, buf));
		}
		break;

#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || defined(COMPAT_FREEBSD6)
	case SMB_OLD_BREAD:
#endif
	case SMB_BREAD:
		if (s->count && s->data.byte_ptr) {
			count = min(s->count, SMB_MAXBLOCKSIZE);
			error = smbus_error(smbus_bread(parent, s->slave,
						s->cmd, &count, buf));
			if (error)
				break;
			error = copyout(buf, s->data.byte_ptr,
			    min(count, s->count));
			s->count = count;
		}
		break;
		
	default:
		error = ENOTTY;
	}

	smbus_release_bus(parent, smbdev);

	return (error);
}

DRIVER_MODULE(smb, smbus, smb_driver, smb_devclass, 0, 0);
MODULE_DEPEND(smb, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(smb, 1);
