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

#include "opt_platform.h"

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

/*
 * Compatible devices.  Keep this sorted in most- to least-specific order first,
 * alphabetical second.  That is, "zwie,ns16550" should appear before "ns16550"
 * on the theory that the zwie driver knows how to make better use of the
 * hardware than the generic driver.  Likewise with chips within a family, the
 * highest-numbers / most recent models should probably appear earlier.
 */
static struct ofw_compat_data compat_data[] = {
	{"arm,pl011",		(uintptr_t)&uart_pl011_class},
	{"cadence,uart",	(uintptr_t)&uart_cdnc_class},
	{"exynos",		(uintptr_t)&uart_s3c2410_class},
	{"fsl,imx6q-uart",	(uintptr_t)&uart_imx_class},
	{"fsl,imx53-uart",	(uintptr_t)&uart_imx_class},
	{"fsl,imx51-uart",	(uintptr_t)&uart_imx_class},
	{"fsl,imx31-uart",	(uintptr_t)&uart_imx_class},
	{"fsl,imx27-uart",	(uintptr_t)&uart_imx_class},
	{"fsl,imx25-uart",	(uintptr_t)&uart_imx_class},
	{"fsl,imx21-uart",	(uintptr_t)&uart_imx_class},
	{"fsl,mvf600-uart",	(uintptr_t)&uart_vybrid_class},
	{"lpc,uart",		(uintptr_t)&uart_lpc_class},
	{"ti,ns16550",		(uintptr_t)&uart_ti8250_class},
	{"ns16550",		(uintptr_t)&uart_ns8250_class},
	{NULL,			(uintptr_t)NULL},
};

/* Export the compat_data table for use by the uart_cpu_fdt.c probe routine. */
const struct ofw_compat_data *uart_fdt_compat_data = compat_data;

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
	const struct ofw_compat_data * cd;

	sc = device_get_softc(dev);

	cd = ofw_bus_search_compatible(dev, compat_data);
	if (cd->ocd_data == (uintptr_t)NULL)
		return (ENXIO);

	sc->sc_class = (struct uart_class *)cd->ocd_data;

	node = ofw_bus_get_node(dev);

	if ((err = uart_fdt_get_clock(node, &clock)) != 0)
		return (err);
	uart_fdt_get_shift(node, &shift);

	return (uart_bus_probe(dev, (int)shift, (int)clock, 0, 0));
}

DRIVER_MODULE(uart, simplebus, uart_fdt_driver, uart_devclass, 0, 0);
