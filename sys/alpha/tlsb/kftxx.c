/* $FreeBSD: src/sys/alpha/tlsb/kftxx.c,v 1.8.2.1 2000/03/27 18:32:49 mjacob Exp $ */
/* $NetBSD: kftxx.c,v 1.9 1998/05/14 00:01:32 thorpej Exp $ */

/*
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * KFTIA and KFTHA Bus Adapter Node for I/O hoses
 * found on AlphaServer 8200 and 8400 systems.
 *
 * i.e., handler for all TLSB I/O nodes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/rpb.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/tlsbvar.h>
#include <alpha/tlsb/kftxxreg.h>
#include <alpha/tlsb/kftxxvar.h>

struct kft_softc {
	int		sc_node;	/* TLSB node */
	u_int16_t	sc_dtype;	/* device type */
};

/*
 * Instance variables for kft devices.
 */
struct kft_device {
	char *		kd_name;	/*  name */
	int		kd_node;	/* node number */
	u_int16_t	kd_dtype;	/* device type */
	u_int16_t	kd_hosenum;	/* hose number */
};

#define KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))

static devclass_t kft_devclass;

/*
 * Device methods
 */
static int kft_probe(device_t);
static int kft_print_child(device_t, device_t);
static int kft_read_ivar(device_t, device_t, int, u_long *);

static device_method_t kft_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		kft_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	kft_print_child),
	DEVMETHOD(bus_read_ivar,	kft_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t kft_driver = {
	"kft",
	kft_methods,
	1,			/* no softc */
};

static int
kft_probe(device_t dev)
{
	static int dun;
	device_t cd;
	struct kft_softc *sc = (struct kft_softc *) device_get_softc(dev);
	struct kft_device *kd;
	int hose;

	if (!TLDEV_ISIOPORT(tlsb_get_dtype(dev)))
		return ENXIO;

	sc->sc_node = tlsb_get_node(dev);
	sc->sc_dtype = tlsb_get_dtype(dev);

	for (hose = 0; hose < MAXHOSE; hose++) {
		u_int32_t value =
			TLSB_GET_NODEREG(sc->sc_node, KFT_IDPNSEX(hose));
		if (value & 0x0E000000) {
			printf("%s: Hose %d IDPNSE has %x\n",
			       device_get_nameunit(dev),
			       hose, value);
			continue;
		}
		if ((value & 0x1) != 0x0) {
			printf("%s: Hose %d has a Bad Cable (0x%x)\n",
			       device_get_nameunit(dev),
			       hose, value);
			continue;
		}
		if ((value & 0x6) != 0x6) {
			if (value)
				printf("%s: Hose %d is missing PWROK (0x%x)\n",
				       device_get_nameunit(dev),
				       hose, value);
			continue;
		}

		kd = (struct kft_device *)
		    malloc(sizeof(struct kft_device), M_DEVBUF, M_NOWAIT);

		if (kd == NULL)
			continue;

		kd->kd_name = "dwlpx";
		kd->kd_node = sc->sc_node;
		kd->kd_dtype = sc->sc_dtype;
		kd->kd_hosenum = hose;
		cd = device_add_child_ordered(dev, hose, kd->kd_name, dun++);
		device_set_ivars(cd, kd);
	}

	return 0;
}

static int
kft_print_child(device_t bus, device_t dev)
{
	struct kft_device *kd = (struct kft_device*) device_get_ivars(dev);
	int retval = 0;

	retval += bus_print_child_header(bus, dev);
	retval += printf(" on %s hose %d\n", device_get_nameunit(bus),
			 kd->kd_hosenum);

	return (retval);
}

static int
kft_read_ivar(device_t bus, device_t dev,
	      int index, u_long* result)
{
	struct kft_device *kd = (struct kft_device*) device_get_ivars(dev);

	switch (index) {
	case KFT_IVAR_NAME:
		*result = (u_long) kd->kd_name;
		return 0;

	case KFT_IVAR_NODE:
		*result = (u_long) kd->kd_node;
		return 0;

	case KFT_IVAR_DTYPE:
		*result = (u_long) kd->kd_dtype;
		return 0;

	case KFT_IVAR_HOSENUM:
		*result = (u_long) kd->kd_hosenum;
		return 0;

	default:
		return ENOENT;
	}
}

DRIVER_MODULE(kft, tlsb, kft_driver, kft_devclass, 0, 0);
