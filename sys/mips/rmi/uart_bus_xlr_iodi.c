/*-
 * Copyright (c) 2006 Raza Microelectronics
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
__FBSDID("$FreeBSD: src/sys/dev/uart/uart_bus_iodi.c,v 1.6.2.5 2006/02/15 09:16:01 marius Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <mips/rmi/iomap.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

static int uart_iodi_probe(device_t dev);

static device_method_t uart_iodi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uart_iodi_probe),
	DEVMETHOD(device_attach, uart_bus_attach),
	DEVMETHOD(device_detach, uart_bus_detach),
	{0, 0}
};

static driver_t uart_iodi_driver = {
	uart_driver_name,
	uart_iodi_methods,
	sizeof(struct uart_softc),
};


extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;
static int
uart_iodi_probe(device_t dev)
{
	struct uart_softc *sc;
	sc = device_get_softc(dev);
	sc->sc_sysdev = SLIST_FIRST(&uart_sysdevs);
	sc->sc_class = &uart_ns8250_class;
	bcopy(&sc->sc_sysdev->bas, &sc->sc_bas, sizeof(sc->sc_bas));
	sc->sc_sysdev->bas.bst = rmi_bus_space;
	sc->sc_sysdev->bas.bsh = MIPS_PHYS_TO_KSEG1(XLR_UART0ADDR);
	sc->sc_bas.bst = rmi_bus_space;
	sc->sc_bas.bsh = MIPS_PHYS_TO_KSEG1(XLR_UART0ADDR);
	/* regshft = 2, rclk = 66000000, rid = 0, chan = 0 */
	return (uart_bus_probe(dev, 2, 66000000, 0, 0));
}

DRIVER_MODULE(uart, iodi, uart_iodi_driver, uart_devclass, 0, 0);
