/*
 * Copyright (c) 2001 M. Warner Losh.  All rights reserved.
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
#include <machine/resource.h>
#include <sys/timepps.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccarddevs.h>

#include <dev/sio/siovar.h>

static	int	sio_pccard_attach(device_t dev);
static	int	sio_pccard_detach(device_t dev);
static	int	sio_pccard_match(device_t self);
static	int	sio_pccard_probe(device_t dev);

static device_method_t sio_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	sio_pccard_detach),

	/* Card interface */
	DEVMETHOD(card_compat_match,	sio_pccard_match),
	DEVMETHOD(card_compat_probe,	sio_pccard_probe),
	DEVMETHOD(card_compat_attach,	sio_pccard_attach),

	{ 0, 0 }
};

static driver_t sio_pccard_driver = {
	sio_driver_name,
	sio_pccard_methods,
	0,
};

static int
sio_pccard_match(device_t dev)
{
	int		error = 0;
	u_int32_t	fcn = PCCARD_FUNCTION_UNSPEC;

	error = pccard_get_function(dev, &fcn);
	if (error != 0)
		return (error);
	/*
	 * If a serial card, we are likely the right driver.
	 */
	if (fcn == PCCARD_FUNCTION_SERIAL)
		return (0);

	return(ENXIO);
}

static int
sio_pccard_probe(dev)
	device_t	dev;
{

#ifdef PC98
	SET_FLAG(dev, SET_IFTYPE(COM_IF_MODEM_CARD));
#endif
	/* Do not probe IRQ - pccard doesn't turn on the interrupt line */
	/* until bus_setup_intr */
	return (sioprobe(dev, 0, 0UL, 1));
}

static int
sio_pccard_attach(dev)
	device_t	dev;
{
	return (sioattach(dev, 0, 0UL));
}

static int
sio_pccard_detach(dev)
	device_t	dev;
{
	return (siodetach(dev));
}

DRIVER_MODULE(sio, pccard, sio_pccard_driver, sio_devclass, 0, 0);
