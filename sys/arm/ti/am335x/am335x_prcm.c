/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#define CM_PER				0
#define CM_PER_L4LS_CLKSTCTRL		(CM_PER + 0x00)
#define CM_PER_TIMER7_CLKCTRL		(CM_PER + 0x7C)
#define CM_PER_TIMER2_CLKCTRL		(CM_PER + 0x80)
#define CM_PER_TIMER3_CLKCTRL		(CM_PER + 0x84)
#define CM_PER_TIMER4_CLKCTRL		(CM_PER + 0x88)
#define CM_PER_TIMER5_CLKCTRL		(CM_PER + 0xEC)
#define CM_PER_TIMER6_CLKCTRL		(CM_PER + 0xF0)

#define CM_DPLL				0x500
#define CLKSEL_TIMER7_CLK		(CM_DPLL + 0x04)
#define CLKSEL_TIMER2_CLK		(CM_DPLL + 0x08)
#define CLKSEL_TIMER3_CLK		(CM_DPLL + 0x0C)
#define CLKSEL_TIMER4_CLK		(CM_DPLL + 0x10)
#define CLKSEL_TIMER5_CLK		(CM_DPLL + 0x18)
#define CLKSEL_TIMER6_CLK		(CM_DPLL + 0x1C)

struct am335x_prcm_softc {
	struct resource *	res[2];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
};

static struct resource_spec am335x_prcm_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static struct am335x_prcm_softc *am335x_prcm_sc = NULL;

/* Read/Write macros */
#define prcm_read_4(reg)		\
	bus_space_read_4(am335x_prcm_sc->bst, am335x_prcm_sc->bsh, reg)
#define prcm_write_4(reg, val)		\
	bus_space_write_4(am335x_prcm_sc->bst, am335x_prcm_sc->bsh, reg, val)

static int
am335x_prcm_probe(device_t dev)
{
	struct	am335x_prcm_softc *sc;
	sc = (struct am335x_prcm_softc *)device_get_softc(dev);

	if (ofw_bus_is_compatible(dev, "am335x,prcm")) {
		device_set_desc(dev, "AM335x PRCM");
		return(BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
am335x_prcm_attach(device_t dev)
{
	struct am335x_prcm_softc *sc = device_get_softc(dev);

	if (am335x_prcm_sc)
		return (ENXIO);

	if (bus_alloc_resources(dev, am335x_prcm_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	am335x_prcm_sc = sc;

	/* Select CLK_M_OSC clock for Timer 2 */
	prcm_write_4(CLKSEL_TIMER2_CLK,1);
        while ((prcm_read_4(CLKSEL_TIMER2_CLK) & 0x1) != 1);

	/* Enable Timer 2 Module */
	prcm_write_4(CM_PER_TIMER2_CLKCTRL, 2);
        while ((prcm_read_4(CM_PER_TIMER2_CLKCTRL) & 0x3) != 2);

	/* Select CLK_M_OSC clock for Timer 3 */
	prcm_write_4(CLKSEL_TIMER3_CLK,1);
        while ((prcm_read_4(CLKSEL_TIMER3_CLK) & 0x1) != 1);

	/* Enable Timer 3 Module */
	prcm_write_4(CM_PER_TIMER3_CLKCTRL, 2);
        while ((prcm_read_4(CM_PER_TIMER3_CLKCTRL) & 0x3) != 2);

	return (0);
}

static device_method_t am335x_prcm_methods[] = {
	DEVMETHOD(device_probe,		am335x_prcm_probe),
	DEVMETHOD(device_attach,	am335x_prcm_attach),
	{ 0, 0 }
};

static driver_t am335x_prcm_driver = {
	"am335x-prcm",
	am335x_prcm_methods,
	sizeof(struct am335x_prcm_softc),
};

static devclass_t am335x_prcm_devclass;

DRIVER_MODULE(am335x_prcm, simplebus, am335x_prcm_driver,
	am335x_prcm_devclass, 0, 0);

