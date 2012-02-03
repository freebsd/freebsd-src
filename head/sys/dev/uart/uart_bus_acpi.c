/*-
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

#include <isa/isavar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>

static int uart_acpi_probe(device_t dev);

static device_method_t uart_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_acpi_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_acpi_driver = {
	uart_driver_name,
	uart_acpi_methods,
	sizeof(struct uart_softc),
};

static struct isa_pnp_id acpi_ns8250_ids[] = {
	{0x0005d041, "Standard PC COM port"},		/* PNP0500 */
	{0x0105d041, "16550A-compatible COM port"},	/* PNP0501 */
	{0x04f0235c, "Wacom Tablet PC Screen"},		/* WACF004 */
	{0}
};

static int
uart_acpi_probe(device_t dev)
{
	struct uart_softc *sc;
	device_t parent;

	parent = device_get_parent(dev);
	sc = device_get_softc(dev);

	if (!ISA_PNP_PROBE(parent, dev, acpi_ns8250_ids)) {
		sc->sc_class = &uart_ns8250_class;
		return (uart_bus_probe(dev, 0, 0, 0, 0));
	}

	/* Add checks for non-ns8250 IDs here. */
	return (ENXIO);
}

DRIVER_MODULE(uart, acpi, uart_acpi_driver, uart_devclass, 0, 0);
