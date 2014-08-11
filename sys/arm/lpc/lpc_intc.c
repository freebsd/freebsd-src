/*-
 * Copyright (c) 2010 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
#include <sys/timetc.h>
#include <sys/cpuset.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>

#include "pic_if.h"

struct lpc_intc_softc {
	device_t		li_dev;
	struct resource *	li_mem_res;
	struct resource *	li_irq_res;
	bus_space_tag_t		li_bst;
	bus_space_handle_t	li_bsh;
	void *			li_intrhand;
};

static int lpc_intc_probe(device_t);
static int lpc_intc_attach(device_t);
static int lpc_intc_intr(void *);
static int lpc_intc_config(device_t, int, enum intr_trigger, enum intr_polarity);
static void lpc_intc_mask(device_t, int);
static void lpc_intc_unmask(device_t, int);
static void lpc_intc_eoi(device_t, int);

#define	intc_read_4(_sc, _reg)		\
    bus_space_read_4((_sc)->li_bst, (_sc)->li_bsh, _reg)
#define	intc_write_4(_sc, _reg, _val)		\
    bus_space_write_4((_sc)->li_bst, (_sc)->li_bsh, _reg, _val)

static int
lpc_intc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "lpc,pic"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_intc_attach(device_t dev)
{
	struct lpc_intc_softc *sc = device_get_softc(dev);
	int rid = 0;

	sc->li_dev = dev;
	sc->li_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 
	    RF_ACTIVE);
	if (!sc->li_mem_res) {
		device_printf(dev, "could not alloc memory resource\n");
		return (ENXIO);
	}

	rid = 0;
	sc->li_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!sc->li_irq_res) {
		device_printf(dev, "could not alloc interrupt resource\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->li_mem_res);
		return (ENXIO);
	}

	sc->li_bst = rman_get_bustag(sc->li_mem_res);
	sc->li_bsh = rman_get_bushandle(sc->li_mem_res);

	if (bus_setup_intr(dev, sc->li_irq_res, INTR_TYPE_MISC | INTR_CONTROLLER,
	    lpc_intc_intr, NULL, sc, &sc->li_intrhand)) {
		device_printf(dev, "could not setup interrupt handler\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->li_mem_res);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->li_irq_res);
		return (ENXIO);
	}

	arm_register_pic(dev, 0);

	/* Clear interrupt status registers and disable all interrupts */
	intc_write_4(sc, LPC_INTC_ER, 0);
	intc_write_4(sc, LPC_INTC_RSR, ~0);
	return (0);
}

static int
lpc_intc_intr(void *arg)
{
	struct lpc_intc_softc *sc = (struct lpc_intc_softc *)arg;
	uint32_t value;
	int i;

	value = intc_read_4(sc, LPC_INTC_SR);
	for (i = 0; i < 32; i++) {
		if (value & (1 << i))
			arm_dispatch_irq(sc->li_dev, NULL, i);
	}

	return (FILTER_HANDLED);
}

static int
lpc_intc_config(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	/* no-op */
	return (0);
}

static void
lpc_intc_mask(device_t dev, int irq)
{
	struct lpc_intc_softc *sc = device_get_softc(dev);
	uint32_t value;

	/* Make sure interrupt isn't active already */
	lpc_intc_eoi(dev, irq);

	/* Clear bit in ER register */
	value = intc_read_4(sc, LPC_INTC_ER);
	value &= ~(1 << irq);
	intc_write_4(sc, LPC_INTC_ER, value);
}

static void
lpc_intc_unmask(device_t dev, int irq)
{
	struct lpc_intc_softc *sc = device_get_softc(dev);
	uint32_t value;

	/* Set bit in ER register */
	value = intc_read_4(sc, LPC_INTC_ER);
	value |= (1 << irq);
	intc_write_4(sc, LPC_INTC_ER, value);
}

static void
lpc_intc_eoi(device_t dev, int irq)
{
	struct lpc_intc_softc *sc = device_get_softc(dev);
	uint32_t value;
	
	/* Set bit in RSR register */
	value = intc_read_4(sc, LPC_INTC_RSR);
	value |= (1 << irq);
	intc_write_4(sc, LPC_INTC_RSR, value);
}

static device_method_t lpc_intc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_intc_probe),
	DEVMETHOD(device_attach,	lpc_intc_attach),
	/* Interrupt controller interface */
	DEVMETHOD(pic_config,		lpc_intc_config),
	DEVMETHOD(pic_mask,		lpc_intc_mask),
	DEVMETHOD(pic_unmask,		lpc_intc_unmask),
	DEVMETHOD(pic_eoi,		lpc_intc_eoi),
	{ 0, 0 }
};

static driver_t lpc_intc_driver = {
	"pic",
	lpc_intc_methods,
	sizeof(struct lpc_intc_softc),
};

static devclass_t lpc_intc_devclass;

DRIVER_MODULE(pic, simplebus, lpc_intc_driver, lpc_intc_devclass, 0, 0);

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ NULL, NULL }
};

static int
fdt_pic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{
	if (!fdt_is_compatible(node, "lpc,pic"))
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]);
	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;
	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_pic_decode_ic,
	NULL
};
