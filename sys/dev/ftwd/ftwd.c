/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Poul-Henning Kamp
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/watchdog.h>

#include <dev/superio/superio.h>

#include <machine/bus.h>
#include <machine/resource.h>

struct ftwd_softc {
	eventhandler_tag	wd_ev;
};

static void
ftwd_func(void *priv, u_int cmd, int *error)
{
	device_t dev = priv;
	uint64_t timeout;
	uint8_t val = 0;
	uint8_t minutes = 0;

	if (cmd != 0) {
		cmd &= WD_INTERVAL;

		/* Convert the requested timeout to seconds. */
		if (cmd >= WD_TO_1SEC)
			timeout = (uint64_t)1 << (cmd - WD_TO_1SEC);
		else
			timeout = 1;

		if (timeout <= UINT8_MAX) {
			val = timeout;
			*error = 0;
		} else if ((timeout / 60) <= UINT8_MAX) {
			val = timeout / 60;
			minutes = 1;
			*error = 0;
		}
	}
	if (bootverbose) {
                if (val == 0) {
			device_printf(dev, "disabling watchdog\n");
		} else {
			device_printf(dev,
			    "arm watchdog to %d %s%s (Was: 0x%02x)\n",
			    val, minutes ? "minute" : "second",
                            val == 1 ? "" : "s",
			    superio_read(dev, 0xf6)
			);
		}
	}
	superio_write(dev, 0xf0, 0x00);		// Disable WDTRST#
	superio_write(dev, 0xf6, val);		// Set Counter

	if (minutes)
		superio_write(dev, 0xf5, 0x7d);	// minutes, act high, 125ms
	else
		superio_write(dev, 0xf5, 0x75);	// seconds, act high, 125ms

	if (val)
		superio_write(dev, 0xf7, 0x01);	// Disable PME
	if (val)
		superio_write(dev, 0xf0, 0x81);	// Enable WDTRST#
	else
		superio_write(dev, 0xf0, 0x00);	// Disable WDTRST
}

static int
ftwd_probe(device_t dev)
{

	if (superio_vendor(dev) != SUPERIO_VENDOR_FINTEK ||
	    superio_get_type(dev) != SUPERIO_DEV_WDT)
		return (ENXIO);
	device_set_desc(dev, "Watchdog Timer on Fintek SuperIO");
	return (BUS_PROBE_DEFAULT);
}

static int
ftwd_attach(device_t dev)
{
	struct ftwd_softc *sc = device_get_softc(dev);

	/*
	 * We do not touch the watchdog at this time, it might be armed
	 * by firmware to protect the full boot sequence.
	 */

	sc->wd_ev = EVENTHANDLER_REGISTER(watchdog_list, ftwd_func, dev, 0);
	return (0);
}

static int
ftwd_detach(device_t dev)
{
	struct ftwd_softc *sc = device_get_softc(dev);
	int dummy;

	if (sc->wd_ev != NULL)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->wd_ev);
	ftwd_func(dev, 0, &dummy);
	return (0);
}

static device_method_t ftwd_methods[] = {
	DEVMETHOD(device_probe,		ftwd_probe),
	DEVMETHOD(device_attach,	ftwd_attach),
	DEVMETHOD(device_detach,	ftwd_detach),
	{ 0, 0 }
};

static driver_t ftwd_driver = {
	"ftwd",
	ftwd_methods,
	sizeof (struct ftwd_softc)
};

static devclass_t ftwd_devclass;

DRIVER_MODULE(ftwd, superio, ftwd_driver, ftwd_devclass, NULL, NULL);
MODULE_DEPEND(ftwd, superio, 1, 1, 1);
MODULE_VERSION(ftwd, 1);
