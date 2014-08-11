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

#include "pic_if.h"

#define	INTC_PENDING_BASIC	0x00
#define	INTC_PENDING_BANK1	0x04
#define	INTC_PENDING_BANK2	0x08
#define	INTC_FIQ_CONTROL	0x0C
#define	INTC_ENABLE_BANK1	0x10
#define	INTC_ENABLE_BANK2	0x14
#define	INTC_ENABLE_BASIC	0x18
#define	INTC_DISABLE_BANK1	0x1C
#define	INTC_DISABLE_BANK2	0x20
#define	INTC_DISABLE_BASIC	0x24

#define	BANK1_START	8
#define	BANK1_END	(BANK1_START + 32 - 1)
#define	BANK2_START	(BANK1_START + 32)
#define	BANK2_END	(BANK2_START + 32 - 1)

#define	IS_IRQ_BASIC(n)	(((n) >= 0) && ((n) < BANK1_START))
#define	IS_IRQ_BANK1(n)	(((n) >= BANK1_START) && ((n) <= BANK1_END))
#define	IS_IRQ_BANK2(n)	(((n) >= BANK2_START) && ((n) <= BANK2_END))
#define	IRQ_BANK1(n)	((n) - BANK1_START)
#define	IRQ_BANK2(n)	((n) - BANK2_START)

#ifdef  DEBUG
#define dprintf(fmt, args...) printf(fmt, ##args)
#else
#define dprintf(fmt, args...)
#endif

struct bcm_intc_softc {
	device_t		sc_dev;
	struct resource *	intc_mem_res;
	struct resource *	intc_irq_res;
	void *			intc_intrhand;
	bus_space_tag_t		intc_bst;
	bus_space_handle_t	intc_bsh;
};

#define	intc_read_4(_sc, reg)		\
    bus_space_read_4(_sc->intc_bst, _sc->intc_bsh, reg)
#define	intc_write_4(_sc, reg, val)		\
    bus_space_write_4(_sc->intc_bst, _sc->intc_bsh, reg, val)

static int bcm_intc_probe(device_t);
static int bcm_intc_attach(device_t);
static int bcm_intc_intr(void *);
static int bcm_intc_config(device_t, int, enum intr_trigger, enum intr_polarity);
static void bcm_intc_mask(device_t, int);
static void bcm_intc_unmask(device_t, int);

static int
bcm_intc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "broadcom,bcm2835-armctrl-ic"))
		return (ENXIO);
	device_set_desc(dev, "BCM2835 Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
bcm_intc_attach(device_t dev)
{
	struct		bcm_intc_softc *sc = device_get_softc(dev);
	int		rid;

	sc->sc_dev = dev;

	rid = 0;
	sc->intc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->intc_mem_res == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		return (ENXIO);
	}

	rid = 0;
	sc->intc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->intc_irq_res == NULL) {
		device_printf(dev, "could not alloc interrupt resource\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->intc_mem_res);
		return (ENXIO);
	}

	sc->intc_bst = rman_get_bustag(sc->intc_mem_res);
	sc->intc_bsh = rman_get_bushandle(sc->intc_mem_res);


	if (bus_setup_intr(dev, sc->intc_irq_res,
	    INTR_TYPE_MISC | INTR_CONTROLLER, bcm_intc_intr, NULL, sc, 
	    &sc->intc_intrhand)) {
		device_printf(dev, "could not setup interrupt handler\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->intc_mem_res);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->intc_irq_res);
		return (ENXIO);
	}

	arm_register_pic(dev, 0);

	return (0);
}

static int
bcm_intc_intr(void *arg)
{
	struct bcm_intc_softc *sc = (struct bcm_intc_softc *)arg;
	uint32_t pending;
	int32_t irq = 0;

	/* TODO: should we mask last_irq? */
	pending = intc_read_4(sc, INTC_PENDING_BASIC);
	while (irq < BANK1_START) {
		if (pending & (1 << irq))
			arm_dispatch_irq(sc->sc_dev, NULL, irq);
		irq++;
	}

	pending = intc_read_4(sc, INTC_PENDING_BANK1);
	while (irq < BANK2_START) {
		if (pending & (1 << IRQ_BANK1(irq)))
			arm_dispatch_irq(sc->sc_dev, NULL, irq);
		irq++;
	}

	pending = intc_read_4(sc, INTC_PENDING_BANK2);
	while (irq <= BANK2_END) {
		if (pending & (1 << IRQ_BANK2(irq)))
			arm_dispatch_irq(sc->sc_dev, NULL, irq);
		irq++;
	}

	return (FILTER_HANDLED);
}

static int
bcm_intc_config(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{

	/* no-op */
	return (0);
}


static void
bcm_intc_mask(device_t dev, int nb)
{
	struct bcm_intc_softc *sc = device_get_softc(dev);
	dprintf("%s: %d\n", __func__, nb);

	if (IS_IRQ_BASIC(nb))
		intc_write_4(sc, INTC_DISABLE_BASIC, (1 << nb));
	else if (IS_IRQ_BANK1(nb))
		intc_write_4(sc, INTC_DISABLE_BANK1, (1 << IRQ_BANK1(nb)));
	else if (IS_IRQ_BANK2(nb))
		intc_write_4(sc, INTC_DISABLE_BANK2, (1 << IRQ_BANK2(nb)));
	else
		printf("arm_mask_irq: Invalid IRQ number: %d\n", nb);
}

static void
bcm_intc_unmask(device_t dev, int nb)
{
	struct bcm_intc_softc *sc = device_get_softc(dev);
	dprintf("%s: %d\n", __func__, nb);

	if (IS_IRQ_BASIC(nb))
		intc_write_4(sc, INTC_ENABLE_BASIC, (1 << nb));
	else if (IS_IRQ_BANK1(nb))
		intc_write_4(sc, INTC_ENABLE_BANK1, (1 << IRQ_BANK1(nb)));
	else if (IS_IRQ_BANK2(nb))
		intc_write_4(sc, INTC_ENABLE_BANK2, (1 << IRQ_BANK2(nb)));
	else
		printf("arm_mask_irq: Invalid IRQ number: %d\n", nb);
}

static device_method_t bcm_intc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_intc_probe),
	DEVMETHOD(device_attach,	bcm_intc_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_config,		bcm_intc_config),
	DEVMETHOD(pic_mask,		bcm_intc_mask),
	DEVMETHOD(pic_unmask,		bcm_intc_unmask),
	{ 0, 0 }
};

static driver_t bcm_intc_driver = {
	"intc",
	bcm_intc_methods,
	sizeof(struct bcm_intc_softc),
};

static devclass_t bcm_intc_devclass;

DRIVER_MODULE(intc, simplebus, bcm_intc_driver, bcm_intc_devclass, 0, 0);


