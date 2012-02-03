/*-
 * Copyright (c) 2006 Benno Rice.  All rights reserved.
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

#include <arm/xscale/pxa/pxavar.h>
#include <arm/xscale/pxa/pxareg.h>

#include "uart_if.h"

#define	PXA_UART_UUE	0x40	/* UART Unit Enable */

static int uart_pxa_probe(device_t dev);

static device_method_t uart_pxa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_pxa_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_pxa_driver = {
	uart_driver_name,
	uart_pxa_methods,
	sizeof(struct uart_softc),
};

static int
uart_pxa_probe(device_t dev)
{
	bus_space_handle_t	base;
	struct			uart_softc *sc;

	/* Check to see if the enable bit's on. */
	base = (bus_space_handle_t)pxa_get_base(dev);
	if ((bus_space_read_4(obio_tag, base,
	    (REG_IER << 2)) & PXA_UART_UUE) == 0)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;

	return(uart_bus_probe(dev, 2, PXA2X0_COM_FREQ, 0, 0));
}

DRIVER_MODULE(uart, pxa, uart_pxa_driver, uart_devclass, 0, 0);
