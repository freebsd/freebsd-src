/*-
 * Copyright (c) 2003-2012 Broadcom Corporation
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY BROADCOM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/uart.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>

static int uart_soc_probe(device_t dev);

static device_method_t uart_soc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_soc_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),

	DEVMETHOD_END
};

static driver_t uart_soc_driver = {
	uart_driver_name,
	uart_soc_methods,
	sizeof(struct uart_softc),
};

static int
uart_soc_probe(device_t dev)
{
	struct uart_softc *sc;

	if (pci_get_vendor(dev) != PCI_VENDOR_NETLOGIC ||
	    pci_get_device(dev) != PCI_DEVICE_ID_NLM_UART)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;
	device_set_desc(dev, "Netlogic SoC UART");
	return (uart_bus_probe(dev, 2, XLP_IO_CLK, 0, 0));
}

DRIVER_MODULE(uart_soc, pci, uart_soc_driver, uart_devclass, 0, 0);
