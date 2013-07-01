/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

static int uart_fdt_probe(device_t);

static device_method_t uart_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_fdt_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_fdt_driver = {
	uart_driver_name,
	uart_fdt_methods,
	sizeof(struct uart_softc),
};

static int
uart_fdt_get_clock(phandle_t node, pcell_t *cell)
{
	pcell_t clock;

	if ((OF_getprop(node, "clock-frequency", &clock,
	    sizeof(clock))) <= 0)
		return (ENXIO);

	if (clock == 0)
		/* Try to retrieve parent 'bus-frequency' */
		/* XXX this should go to simple-bus fixup or so */
		if ((OF_getprop(OF_parent(node), "bus-frequency", &clock,
		    sizeof(clock))) <= 0)
			clock = 0;

	*cell = fdt32_to_cpu(clock);
	return (0);
}

static int
uart_fdt_get_shift(phandle_t node, pcell_t *cell)
{
	pcell_t shift;

	if ((OF_getprop(node, "reg-shift", &shift, sizeof(shift))) <= 0)
		shift = 0;
	*cell = fdt32_to_cpu(shift);
	return (0);
}

static int
uart_fdt_probe(device_t dev)
{
	struct uart_softc *sc;
	phandle_t node;
	pcell_t clock, shift;
	int err;

	sc = device_get_softc(dev);
	if (ofw_bus_is_compatible(dev, "ns16550"))
		sc->sc_class = &uart_ns8250_class;
	else if (ofw_bus_is_compatible(dev, "lpc,uart"))
		sc->sc_class = &uart_lpc_class;
	else if (ofw_bus_is_compatible(dev, "fsl,imx-uart"))
		sc->sc_class = &uart_imx_class;
	else if (ofw_bus_is_compatible(dev, "arm,pl011"))
		sc->sc_class = &uart_pl011_class;
	else if (ofw_bus_is_compatible(dev, "exynos"))
		sc->sc_class = &uart_s3c2410_class;
	else if (ofw_bus_is_compatible(dev, "cadence,uart"))
		sc->sc_class = &uart_cdnc_class;
	else
		return (ENXIO);

	node = ofw_bus_get_node(dev);

	if ((err = uart_fdt_get_clock(node, &clock)) != 0)
		return (err);
	uart_fdt_get_shift(node, &shift);

	return (uart_bus_probe(dev, (int)shift, (int)clock, 0, 0));
}

DRIVER_MODULE(uart, simplebus, uart_fdt_driver, uart_devclass, 0, 0);
