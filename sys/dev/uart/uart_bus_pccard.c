/*
 * Copyright (c) 2001 M. Warner Losh.  All rights reserved.
 * Copyright (c) 2003 Norikatsu Shigemura, Takenori Watanabe All rights reserved.
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
#include <machine/resource.h>

#include <dev/pccard/pccard_cis.h>
#include <dev/pccard/pccarddevs.h>
#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>

static	int	uart_pccard_match(device_t self);
static	int	uart_pccard_probe(device_t dev);

static device_method_t uart_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),

	/* Card interface */
	DEVMETHOD(card_compat_match,	uart_pccard_match),
	DEVMETHOD(card_compat_probe,	uart_pccard_probe),
	DEVMETHOD(card_compat_attach,	uart_bus_attach),

	{ 0, 0 }
};

static driver_t uart_pccard_driver = {
	uart_driver_name,
	uart_pccard_methods,
	sizeof(struct uart_softc),
};

static int
uart_pccard_match(device_t dev)
{
	int		error = 0;
	u_int32_t	fcn = PCCARD_FUNCTION_UNSPEC;

	error = pccard_get_function(dev, &fcn);
	if (error != 0)
		return (error);
	/*
	 * If a serial card, we are likely the right driver.  However,
	 * some serial cards are better servered by other drivers, so
	 * allow other drivers to claim it, if they want.
	 */
	if (fcn == PCCARD_FUNCTION_SERIAL)
		return (-100);

	return(ENXIO);
}

static int
uart_pccard_probe(dev)
	device_t	dev;
{
	struct uart_softc *sc;
	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;

	/* Do not probe IRQ - pccard doesn't turn on the interrupt line */
	/* until bus_setup_intr but how can I do so?*/
	return (uart_bus_probe(dev, 0, 0, 0, 0));
}

DRIVER_MODULE(uart, pccard, uart_pccard_driver, uart_devclass, 0, 0);
