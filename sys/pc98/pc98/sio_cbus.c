/*
 * Copyright (c) 2001 Yoshihiro TAKAHASHI.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/tty.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <sys/timepps.h>

#include <dev/sio/siovar.h>

#include <isa/isavar.h>

static	int	sio_isa_attach	__P((device_t dev));
static	int	sio_isa_probe	__P((device_t dev));

static device_method_t sio_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sio_isa_probe),
	DEVMETHOD(device_attach,	sio_isa_attach),

	{ 0, 0 }
};

static driver_t sio_isa_driver = {
	sio_driver_name,
	sio_isa_methods,
	0,
};

static struct isa_pnp_id sio_ids[] = {
	{0x0100e4a5, "RSA-98III"},
	{0}
};

static int
sio_isa_probe(dev)
	device_t	dev;
{
#ifdef PC98
	int	logical_id;
#endif
	/* Check isapnp ids */
	if (ISA_PNP_PROBE(device_get_parent(dev), dev, sio_ids) == ENXIO)
		return (ENXIO);
#ifdef PC98
	logical_id = isa_get_logicalid(dev);
	switch (logical_id) {
	case 0x0100e4a5:	/* RSA-98III */
		SET_FLAG(dev, SET_IFTYPE(COM_IF_RSA98III));
		break;
	}
#endif
	return (sioprobe(dev, 0, 0UL, 0));
}

static int
sio_isa_attach(dev)
	device_t	dev;
{
	return (sioattach(dev, 0, 0UL));
}

DRIVER_MODULE(sio, isa, sio_isa_driver, sio_devclass, 0, 0);
