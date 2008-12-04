/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_uart.h"

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

#include <arm/at91/at91rm92reg.h>

#include "uart_if.h"

static int usart_at91rm92_probe(device_t dev);

extern struct uart_class at91_usart_class;

static device_method_t usart_at91rm92_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		usart_at91rm92_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t usart_at91rm92_driver = {
	uart_driver_name,
	usart_at91rm92_methods,
	sizeof(struct uart_softc),
};

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;

static int
usart_at91rm92_probe(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	switch (device_get_unit(dev))
	{
	case 0:
#ifdef SKYEYE_WORKAROUNDS
		device_set_desc(dev, "USART0");
#else
		device_set_desc(dev, "DBGU");
#endif
		/*
		 * Setting sc_sysdev makes this device a 'system device' and
		 * indirectly makes it the system console.
		 */
		sc->sc_sysdev = SLIST_FIRST(&uart_sysdevs);
		bcopy(&sc->sc_sysdev->bas, &sc->sc_bas, sizeof(sc->sc_bas));
		break;
	case 1:
		device_set_desc(dev, "USART0");
		break;
	case 2:
		device_set_desc(dev, "USART1");
		break;
	case 3:
		device_set_desc(dev, "USART2");
		break;
	case 4:
		device_set_desc(dev, "USART3");
		break;
	}
	sc->sc_class = &at91_usart_class;
	return (uart_bus_probe(dev, 0, 0, 0, device_get_unit(dev)));
}


DRIVER_MODULE(uart, atmelarm, usart_at91rm92_driver, uart_devclass, 0, 0);
