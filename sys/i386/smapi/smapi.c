/*-
 * Copyright (c) 2003 Matthew N. Dodd <winter@jurai.net>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/smapi.h>
#include <i386/smapi/smapi_var.h>

u_long smapi32_offset;
u_short smapi32_segment;
#define	SMAPI32_SEGMENT	0x18

devclass_t smapi_devclass;

static d_open_t smapi_open;
static d_close_t smapi_close;
static d_ioctl_t smapi_ioctl;

#define CDEV_MAJOR	183

static struct cdevsw smapi_cdevsw = {
	/* open */	smapi_open,
	/* close */	smapi_close,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	smapi_ioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"smapi",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_MEM,
	/* kqfilter */	NULL,
};

static int
smapi_open (dev, oflags, devtype, td)
	dev_t		dev;
	int		oflags;
	int		devtype;
	d_thread_t *	td;
{

	return (0);
}

static int
smapi_close (dev, fflag, devtype, td)
	dev_t		dev;
	int		fflag;
	int		devtype;
	d_thread_t *	td;
{

	return (0);
}

static int
smapi_ioctl (dev, cmd, data, fflag, td)
	dev_t		dev;
	u_long		cmd;
	caddr_t		data;
	int		fflag;
	d_thread_t *	td;
{
	struct smapi_softc *sc;
	int error;

	error = 0;
	sc = devclass_get_softc(smapi_devclass, minor(dev)); 
        if (sc == NULL) {
                error = ENXIO;
                goto fail;
        }

	switch (cmd) {
	case SMAPIOGHEADER:
		bcopy((caddr_t)sc->header, data,
				sizeof(struct smapi_bios_header)); 
		error = 0;
		break;
	case SMAPIOCGFUNCTION:
#if 1
		smapi32_segment = SMAPI32_SEGMENT;
		smapi32_offset = sc->smapi32_entry;
		error = smapi32(
#else
		error = smapi32_new(sc->smapi32_entry, SMAPI32_SEGMENT,
#endif
				(struct smapi_bios_parameter *)data,
				(struct smapi_bios_parameter *)data);
		break;
	default:
		error = ENOTTY;
	}

fail:
	return (error);
}

int
smapi_attach (struct smapi_softc *sc)
{

	sc->cdev = make_dev(&smapi_cdevsw,
			device_get_unit(sc->dev),
			UID_ROOT, GID_WHEEL, 0600,
			"%s%d",
			smapi_cdevsw.d_name,
			device_get_unit(sc->dev));

	return (0);
}

int
smapi_detach (struct smapi_softc *sc)
{

	destroy_dev(sc->cdev);
	return (0);
}
