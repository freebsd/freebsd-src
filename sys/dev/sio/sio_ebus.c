/*-
 * Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/timepps.h>
#include <sys/tty.h>

#include <machine/bus.h>

#include <dev/sio/siovar.h>

#include <dev/ofw/openfirm.h>
#include <sparc64/ebus/ebusvar.h>

int	sio_ofw_inlist(char *name, char *list[]);
static	int	sio_ebus_attach(device_t dev);
static	int	sio_ebus_probe(device_t dev);

static device_method_t sio_ebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sio_ebus_probe),
	DEVMETHOD(device_attach,	sio_ebus_attach),

	{ 0, 0 }
};

static driver_t sio_ebus_driver = {
	sio_driver_name,
	sio_ebus_methods,
	0,
};

DRIVER_MODULE(sio, ebus, sio_ebus_driver, sio_devclass, 0, 0);

/* Needed for EBus attach and sparc64 console support */
char *sio_ofw_names[] = {
	"serial",
	"su",
	"su_pnp",
	NULL
};

char *sio_ofw_compat[] = {
	"su",
	"su16550",
	NULL
};

int
sio_ofw_inlist(name, list)
	char	*name;
	char	*list[];
{
	int	i;

	if (name == NULL)
		return (0);
	for (i = 0; list[i] != NULL; i++) {
		if (strcmp(name, list[i]) == 0)
			return (1);
	}
	return (0);
}

static int
sio_ebus_probe(dev)
	device_t	dev;
{
	char	*n;

	n = ebus_get_name(dev);
	if (!sio_ofw_inlist(n, sio_ofw_names) && (strcmp(n, "serial") != 0 ||
	    !sio_ofw_inlist(ebus_get_compat(dev), sio_ofw_compat)))
		return (ENXIO);
	/* Do not probe IRQ - isa_irq_pending() does not work for ebus. */
	return (sioprobe(dev, 0, 0UL, 1));
}

static int
sio_ebus_attach(dev)
	device_t	dev;
{

	return (sioattach(dev, 0, 0UL));
}
