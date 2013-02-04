/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

struct lpc_pwr_softc {
	device_t		dp_dev;
	struct resource *	dp_mem_res;
	bus_space_tag_t		dp_bst;
	bus_space_handle_t	dp_bsh;
};

static struct lpc_pwr_softc *lpc_pwr_sc = NULL;

static int lpc_pwr_probe(device_t);
static int lpc_pwr_attach(device_t);

#define	lpc_pwr_read_4(_sc, _reg)			\
    bus_space_read_4((_sc)->dp_bst, (_sc)->dp_bsh, _reg)
#define	lpc_pwr_write_4(_sc, _reg, _val)		\
    bus_space_write_4((_sc)->dp_bst, (_sc)->dp_bsh, _reg, _val)

static int
lpc_pwr_probe(device_t dev)
{
	
	if (!ofw_bus_is_compatible(dev, "lpc,pwr"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 Power Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_pwr_attach(device_t dev)
{
	struct lpc_pwr_softc *sc = device_get_softc(dev);
	int rid;

	sc->dp_dev = dev;

	rid = 0;
	sc->dp_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 
	    RF_ACTIVE);
	if (!sc->dp_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->dp_bst = rman_get_bustag(sc->dp_mem_res);
	sc->dp_bsh = rman_get_bushandle(sc->dp_mem_res);

	lpc_pwr_sc = sc;

	return (0);
}

uint32_t
lpc_pwr_read(device_t dev, int reg)
{
	return (lpc_pwr_read_4(lpc_pwr_sc, reg));
}

void
lpc_pwr_write(device_t dev, int reg, uint32_t value)
{
	lpc_pwr_write_4(lpc_pwr_sc, reg, value);
}

static device_method_t lpc_pwr_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_pwr_probe),
	DEVMETHOD(device_attach,	lpc_pwr_attach),
	{ 0, 0 }
};

static devclass_t lpc_pwr_devclass;

static driver_t lpc_pwr_driver = {
	"pwr",
	lpc_pwr_methods,
	sizeof(struct lpc_pwr_softc),
};

DRIVER_MODULE(pwr, simplebus, lpc_pwr_driver, lpc_pwr_devclass, 0, 0);
