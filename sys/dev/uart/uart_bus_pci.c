/*
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

#include <dev/pci/pcivar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>

static int uart_pci_probe(device_t dev);

static device_method_t uart_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_pci_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_pci_driver = {
	uart_driver_name,
	uart_pci_methods,
	sizeof(struct uart_softc),
};

struct pci_id {
	uint32_t	type;
	const char	*desc;
	int		rid;
};

static struct pci_id pci_ns8250_ids[] = {
	{ 0x100812b9, "3COM PCI FaxModem", 0x10 },
	{ 0x2000131f, "CyberSerial (1-port) 16550", 0x10 },
	{ 0x01101407, "Koutech IOFLEX-2S PCI Dual Port Serial", 0x10 },
	{ 0x01111407, "Koutech IOFLEX-2S PCI Dual Port Serial", 0x10 },
	{ 0x048011c1, "Lucent kermit based PCI Modem", 0x14 },
	{ 0x95211415, "Oxford Semiconductor PCI Dual Port Serial", 0x10 },
	{ 0x7101135e, "SeaLevel Ultra 530.PCI Single Port Serial", 0x18 },
	{ 0x0000151f, "SmartLink 5634PCV SurfRider", 0x10 },
	{ 0x0103115d, "Xircom Cardbus modem", 0x10 },
	{ 0x98459710, "Netmos Nm9845 PCI Bridge with Dual UART", 0x10 },
	{ 0x01c0135c, "Quatech SSCLP-200/300", 0x18 
		/* 
		 * NB: You must mount the "SPAD" jumper to correctly detect
		 * the FIFO on the UART.  Set the options on the jumpers,
		 * we do not support the extra registers on the Quatech.
		 */
	},
	{ 0x00000000, NULL, 0 }
};

static struct pci_id *
uart_pci_match(uint32_t type, struct pci_id *id)
{

	while (id->type && id->type != type)
		id++;
	return ((id->type) ? id : NULL);
}

static int
uart_pci_probe(device_t dev)
{
	struct uart_softc *sc;
	struct pci_id *id;

	sc = device_get_softc(dev);

	id = uart_pci_match(pci_get_devid(dev), pci_ns8250_ids);
	if (id != NULL) {
		sc->sc_class = &uart_ns8250_class;
		goto match;
	}
	/* Add checks for non-ns8250 IDs here. */
	return (ENXIO);

 match:
	if (id->desc)
		device_set_desc(dev, id->desc);
	return (uart_bus_probe(dev, 0, 0, id->rid, 0));
}

DRIVER_MODULE(uart, pci, uart_pci_driver, uart_devclass, 0, 0);
DRIVER_MODULE(uart, cardbus, uart_pci_driver, uart_devclass, 0, 0);
