/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_uart.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <dev/bhnd/cores/chipc/chipcvar.h>

#include "uart_if.h"
#include "bhnd_chipc_if.h"


static int	uart_chipc_probe(device_t dev);

extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;

static void
uart_chipc_identify(driver_t *driver, device_t parent)
{
	struct chipc_caps	*caps;

	if (device_find_child(parent, "uart", -1) != NULL)
		return;

	caps = BHND_CHIPC_GET_CAPS(parent);

	if (caps == NULL) {
		device_printf(parent, "error: can't retrieve ChipCommon "
		    "capabilities\n");
		return;
	}

	if (caps->num_uarts == 0)
		return;

	/*
	 * TODO: add more than one UART
	 */
	BUS_ADD_CHILD(parent, 0, "uart", -1);
}

static int
uart_chipc_probe(device_t dev)
{
	struct uart_softc 	*sc;
	struct resource		*res;
	int			 rid;
	int			 err;

	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "can't allocate main resource\n");
		return (ENXIO);
	}

	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;
	sc->sc_sysdev = SLIST_FIRST(&uart_sysdevs);
	if (sc->sc_sysdev == NULL) {
		device_printf(dev, "missing sysdev\n");
		return (EINVAL);
	}

	bcopy(&sc->sc_sysdev->bas, &sc->sc_bas, sizeof(sc->sc_bas));

	sc->sc_sysdev->bas.bst = rman_get_bustag(res);
	sc->sc_sysdev->bas.bsh = rman_get_bushandle(res);
	sc->sc_bas.bst = sc->sc_sysdev->bas.bst;
	sc->sc_bas.bsh = sc->sc_sysdev->bas.bsh;

	err = bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
	if (err) {
		device_printf(dev, "can't release resource [%d]\n", rid);
		return (ENXIO);
	}

	/* We use internal SoC clock generator with non-standart freq MHz */
	return (uart_bus_probe(dev, 0, sc->sc_sysdev->bas.rclk, 0, 0));
}

static device_method_t uart_chipc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	uart_chipc_identify),
	DEVMETHOD(device_probe,		uart_chipc_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_chipc_driver = {
	uart_driver_name,
	uart_chipc_methods,
	sizeof(struct uart_softc),
};

DRIVER_MODULE(uart, bhnd_chipc, uart_chipc_driver, uart_devclass, 0, 0);
