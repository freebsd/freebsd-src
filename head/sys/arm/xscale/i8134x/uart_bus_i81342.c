/*-
 * Copyright (c) 2004 Olivier Houchard.  All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <dev/ic/ns16550.h>

#include "uart_if.h"

static int uart_i81342_probe(device_t dev);

static device_method_t uart_i81342_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_i81342_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_i81342_driver = {
	uart_driver_name,
	uart_i81342_methods,
	sizeof(struct uart_softc),
};

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;
static int
uart_i81342_probe(device_t dev)
{
	struct uart_softc *sc;
	int err;

	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;
	if (device_get_unit(dev) == 0) {
		sc->sc_sysdev = SLIST_FIRST(&uart_sysdevs);
		bcopy(&sc->sc_sysdev->bas, &sc->sc_bas, sizeof(sc->sc_bas));
	}
	sc->sc_rres = bus_alloc_resource_anywhere(dev, SYS_RES_IOPORT,
	    &sc->sc_rrid, uart_getrange(sc->sc_class), RF_ACTIVE);
	
	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);
	bus_space_write_4(sc->sc_bas.bst, sc->sc_bas.bsh, REG_IER << 2,
	    0x40 | 0x10);
        bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);

	err = uart_bus_probe(dev, 2, 33334000, 0, device_get_unit(dev));
	sc->sc_rxfifosz = sc->sc_txfifosz = 1;
	return (err);
}


DRIVER_MODULE(uart, obio, uart_i81342_driver, uart_devclass, 0, 0);
