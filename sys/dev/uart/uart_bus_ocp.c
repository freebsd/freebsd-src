/*-
 * Copyright 2006 by Juniper Networks.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/tty.h>
#include <machine/bus.h>

#include <machine/ocpbus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>

static int uart_ocp_probe(device_t);

static device_method_t uart_ocp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_ocp_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_ocp_driver = {
	uart_driver_name,
	uart_ocp_methods,
	sizeof(struct uart_softc),
};

static int
uart_ocp_probe(device_t dev)
{
	device_t parent;
	struct uart_softc *sc;
	uintptr_t clock, devtype;
	int error;

	parent = device_get_parent(dev);

	error = BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_DEVTYPE, &devtype);
	if (error)
		return (error);
	if (devtype != OCPBUS_DEVTYPE_UART)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;

	if (BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_CLOCK, &clock))
		clock = 0;
	return (uart_bus_probe(dev, 0, clock, 0, 0));
}

DRIVER_MODULE(uart, ocpbus, uart_ocp_driver, uart_devclass, 0, 0);
