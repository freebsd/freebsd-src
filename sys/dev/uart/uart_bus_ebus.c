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
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

static int uart_ebus_probe(device_t dev);

static device_method_t uart_ebus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_ebus_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_ebus_driver = {
	uart_driver_name,
	uart_ebus_methods,
	sizeof(struct uart_softc),
};

static int
uart_ebus_probe(device_t dev)
{
	const char *nm, *cmpt;
	struct uart_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->sc_class = NULL;

	nm = ofw_bus_get_name(dev);
	cmpt = ofw_bus_get_compat(dev);
	if (!strcmp(nm, "su") || !strcmp(nm, "su_pnp") || (cmpt != NULL &&
	    (!strcmp(cmpt, "su") || !strcmp(cmpt, "su16550")))) {
		sc->sc_class = &uart_ns8250_class;
		return (uart_bus_probe(dev, 0, 0, 0, 0));
	}
	if (!strcmp(nm, "se")) {
		sc->sc_class = &uart_sab82532_class;
		error = uart_bus_probe(dev, 0, 0, 0, 1);
		return ((error) ? error : -1);
	}

	return (ENXIO);
}

DRIVER_MODULE(uart, ebus, uart_ebus_driver, uart_devclass, 0, 0);
