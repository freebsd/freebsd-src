/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
 * All rights reserved.
 *
 * Based on OMAP3 INTC code by Ben Gray
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
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define INTC_REVISION		0x00
#define INTC_SYSCONFIG		0x10
#define INTC_SYSSTATUS		0x14
#define INTC_SIR_IRQ		0x40
#define INTC_CONTROL		0x48
#define INTC_THRESHOLD		0x68
#define INTC_MIR_CLEAR(x)	(0x88 + ((x) * 0x20))
#define INTC_MIR_SET(x)		(0x8C + ((x) * 0x20))
#define INTC_ISR_SET(x)		(0x90 + ((x) * 0x20))
#define INTC_ISR_CLEAR(x)	(0x94 + ((x) * 0x20))

struct ti_aintc_softc {
	device_t		sc_dev;
	struct resource *	aintc_res[3];
	bus_space_tag_t		aintc_bst;
	bus_space_handle_t	aintc_bsh;
	uint8_t			ver;
};

static struct resource_spec ti_aintc_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static struct ti_aintc_softc *ti_aintc_sc = NULL;

#define	aintc_read_4(_sc, reg)		\
    bus_space_read_4((_sc)->aintc_bst, (_sc)->aintc_bsh, (reg))
#define	aintc_write_4(_sc, reg, val)		\
    bus_space_write_4((_sc)->aintc_bst, (_sc)->aintc_bsh, (reg), (val))

/* List of compatible strings for FDT tree */
static struct ofw_compat_data compat_data[] = {
	{"ti,am33xx-intc",	1},
	{"ti,omap2-intc",	1},
	{NULL,		 	0},
};

static void
aintc_post_filter(void *arg)
{

	arm_irq_memory_barrier(0);
	aintc_write_4(ti_aintc_sc, INTC_CONTROL, 1); /* EOI */
}

static int
ti_aintc_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI AINTC Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ti_aintc_attach(device_t dev)
{
	struct		ti_aintc_softc *sc = device_get_softc(dev);
	uint32_t x;

	sc->sc_dev = dev;

	if (ti_aintc_sc)
		return (ENXIO);

	if (bus_alloc_resources(dev, ti_aintc_spec, sc->aintc_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->aintc_bst = rman_get_bustag(sc->aintc_res[0]);
	sc->aintc_bsh = rman_get_bushandle(sc->aintc_res[0]);

	ti_aintc_sc = sc;

	x = aintc_read_4(sc, INTC_REVISION);
	device_printf(dev, "Revision %u.%u\n",(x >> 4) & 0xF, x & 0xF);

	/* SoftReset */
	aintc_write_4(sc, INTC_SYSCONFIG, 2);

	/* Wait for reset to complete */
	while(!(aintc_read_4(sc, INTC_SYSSTATUS) & 1));

	/*Set Priority Threshold */
	aintc_write_4(sc, INTC_THRESHOLD, 0xFF);

	arm_post_filter = aintc_post_filter;

	return (0);
}

static device_method_t ti_aintc_methods[] = {
	DEVMETHOD(device_probe,		ti_aintc_probe),
	DEVMETHOD(device_attach,	ti_aintc_attach),
	{ 0, 0 }
};

static driver_t ti_aintc_driver = {
	"aintc",
	ti_aintc_methods,
	sizeof(struct ti_aintc_softc),
};

static devclass_t ti_aintc_devclass;

EARLY_DRIVER_MODULE(aintc, simplebus, ti_aintc_driver, ti_aintc_devclass,
    0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

int
arm_get_next_irq(int last_irq)
{
	struct ti_aintc_softc *sc = ti_aintc_sc;
	uint32_t active_irq;

	/* Get the next active interrupt */
	active_irq = aintc_read_4(sc, INTC_SIR_IRQ);

	/* Check for spurious interrupt */
	if ((active_irq & 0xffffff80)) {
		device_printf(sc->sc_dev,
		    "Spurious interrupt detected (0x%08x)\n", active_irq);
		aintc_write_4(sc, INTC_SIR_IRQ, 0);
		return -1;
	}

	if (active_irq != last_irq)
		return active_irq;
	else
		return -1;
}

void
arm_mask_irq(uintptr_t nb)
{
	struct ti_aintc_softc *sc = ti_aintc_sc;

	aintc_write_4(sc, INTC_MIR_SET(nb >> 5), (1UL << (nb & 0x1F)));
	aintc_write_4(sc, INTC_CONTROL, 1); /* EOI */
}

void
arm_unmask_irq(uintptr_t nb)
{
	struct ti_aintc_softc *sc = ti_aintc_sc;

	arm_irq_memory_barrier(nb);
	aintc_write_4(sc, INTC_MIR_CLEAR(nb >> 5), (1UL << (nb & 0x1F)));
}
