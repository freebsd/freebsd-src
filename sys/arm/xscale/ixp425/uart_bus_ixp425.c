/*-
 * Copyright (c) 2006 Kevin Lo.  All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/arm/xscale/ixp425/uart_bus_ixp425.c,v 1.3 2007/05/29 18:10:42 jhay Exp $");

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
#include <dev/ic/ns16550.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#include "uart_if.h"

static int uart_ixp425_probe(device_t dev);

static device_method_t uart_ixp425_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_ixp425_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_ixp425_driver = {
	uart_driver_name,
	uart_ixp425_methods,
	sizeof(struct uart_softc),
};
DRIVER_MODULE(uart, ixp, uart_ixp425_driver, uart_devclass, 0, 0);

static int
uart_ixp425_probe(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;
	sc->sc_rrid = 0;
	sc->sc_rtype = SYS_RES_MEMORY;
	sc->sc_rres = bus_alloc_resource(dev, sc->sc_rtype, &sc->sc_rrid,
	    0, ~0, uart_getrange(sc->sc_class), RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		return (ENXIO);
	}
	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);
	/*
	 * XXX set UART Unit Enable (0x40) AND
	 *     receiver timeout int enable (0x10).
	 * The first turns on the UART.  The second is necessary to get
	 * interrupts when the FIFO has data but is not full.  Note that
	 * uart_ns8250 carefully avoids touching these bits so we can
	 * just set them here and proceed.  But this is fragile...
	 */
	bus_space_write_4(sc->sc_bas.bst, sc->sc_bas.bsh, IXP425_UART_IER,
	    IXP425_UART_IER_UUE | IXP425_UART_IER_RTOIE);
	bus_release_resource(dev, sc->sc_rtype, sc->sc_rrid, sc->sc_rres);

	return uart_bus_probe(dev, 0, IXP425_UART_FREQ, 0, 0);
}
