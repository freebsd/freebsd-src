/*-
 * Copyright (c) 2005, M. Warner Losh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/cardbus/cardbus_device.c,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbus_cis.h>
#include <dev/pccard/pccard_cis.h>

static	d_open_t	cardbus_open;
static	d_close_t	cardbus_close;
static	d_read_t	cardbus_read;
static	d_ioctl_t	cardbus_ioctl;

static struct cdevsw cardbus_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	cardbus_open,
	.d_close =	cardbus_close,
	.d_read =	cardbus_read,
	.d_ioctl =	cardbus_ioctl,
	.d_name =	"cardbus"
};

int
cardbus_device_create(struct cardbus_softc *sc)
{
	uint32_t minor;

	minor = device_get_unit(sc->sc_dev) << 16;
	sc->sc_cisdev = make_dev(&cardbus_cdevsw, minor, 0, 0, 0666,
	    "cardbus%u.cis", device_get_unit(sc->sc_dev));
	sc->sc_cisdev->si_drv1 = sc;
	return (0);
}

int
cardbus_device_destroy(struct cardbus_softc *sc)
{
	if (sc->sc_cisdev)
		destroy_dev(sc->sc_cisdev);
	return (0);
}

static int
cardbus_build_cis(device_t cbdev, device_t child, int id,
    int len, uint8_t *tupledata, uint32_t start, uint32_t *off,
    struct tuple_callbacks *info, void *argp)
{
	struct cis_buffer *cis;
	int i;

	cis = (struct cis_buffer *)argp;
	/*
	 * CISTPL_END is a special case, it has no length field.
	 */
	if (id == CISTPL_END) {
		if (cis->len + 1 > sizeof(cis->buffer))
			return (ENOSPC);
		cis->buffer[cis->len++] = id;
		return (0);
	}
	if (cis->len + 2 + len > sizeof(cis->buffer))
		return (ENOSPC);
	cis->buffer[cis->len++] = id;
	cis->buffer[cis->len++] = len;
	for (i = 0; i < len; i++)
		cis->buffer[cis->len++] = tupledata[i];
	return (0);
}

static	int
cardbus_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	device_t parent, child;
	device_t *kids;
	int cnt, err;
	struct cardbus_softc *sc;
	struct tuple_callbacks cb[] = {
		{CISTPL_GENERIC, "GENERIC", cardbus_build_cis}
	};

	sc = dev->si_drv1;
	if (sc->sc_cis_open)
		return (EBUSY);
	parent = sc->sc_dev;
	err = device_get_children(parent, &kids, &cnt);
	if (err)
		return err;
	if (cnt == 0) {
		free(kids, M_TEMP);
		sc->sc_cis_open++;
		sc->sc_cis = NULL;
		return (0);
	}
	child = kids[0];
	free(kids, M_TEMP);
	sc->sc_cis = malloc(sizeof(*sc->sc_cis), M_TEMP, M_ZERO | M_WAITOK);
	err = cardbus_parse_cis(parent, child, cb, sc->sc_cis);
	if (err) {
		free(sc->sc_cis, M_TEMP);
		sc->sc_cis = NULL;
		return (err);
	}
	sc->sc_cis_open++;
	return (0);
}

static	int
cardbus_close(struct cdev *dev, int fflags, int devtype, struct thread *td)
{
	struct cardbus_softc *sc;

	sc = dev->si_drv1;
	free(sc->sc_cis, M_TEMP);
	sc->sc_cis = NULL;
	sc->sc_cis_open = 0;
	return (0);
}

static	int
cardbus_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	return (ENOTTY);
}

static	int
cardbus_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct cardbus_softc *sc;

	sc = dev->si_drv1;
	/* EOF */
	if (sc->sc_cis == NULL || uio->uio_offset > sc->sc_cis->len)
		return (0);
	return (uiomove(sc->sc_cis->buffer + uio->uio_offset,
	  MIN(uio->uio_resid, sc->sc_cis->len - uio->uio_offset), uio));
}
