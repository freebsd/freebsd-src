/*-
 * Copyright (c) 2025 Arm Ltd
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_acpi.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rman.h>

#include <machine/bus.h>

#ifdef FDT
#include <dev/fdt/fdt_intr.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "pci_if.h"
#include "pic_if.h"

#define	IWB_IDR0		0x0000
#define	 IDR0_IW_RANGE_SHIFT	0
#define	 IDR0_IW_RANGE_MASK	(0x7ffu << IDR0_IW_RANGE_SHIFT)
#define	 IDR0_IW_RANGE_IRQs(x)	\
    ((((x) & IDR0_IW_RANGE_MASK) + 1) * 32)
#define	IWB_IIDR		0x0040
#define	IWB_AIDR		0x0044
#define	IWB_CR0			0x0080
#define	 CR0_IWBEN		(0x1u << 0)
#define	IWB_WENABLE_STATUSR	0x00C0
#define	 WENABLE_STATUSR_IDLE	(0x1u << 0)
#define	IWB_WRESAMPLER		0x00C8
#define	IWB_WENABLER(irq)	(0x2000 + (4 * ((irq) / 32)))
#define	 WENABLER_MASK(irq)	(0x1u << ((irq) % 32))
#define	 WENABLER_ENABLED(irq)	(0x1u << ((irq) % 32))
#define	IWB_WTMR(irq)		(0x4000 + (4 * ((irq) / 32)))
#define	 WTMR_MASK(irq)		(0x1u << ((irq) % 32))
#define	 WTMR_LEVEL(irq)	(0x1u << ((irq) % 32))

struct gicv5_iwb_softc;

struct gicv5_iwb_irqsrc {
	struct intr_irqsrc	gi_isrc;
	uint32_t		gi_irq;
	enum intr_polarity	gi_pol;
	enum intr_trigger	gi_trig;
	struct resource		*gi_res; /* Parent MSI interrupt resource */
	void			*gi_cookie;
	struct gicv5_iwb_softc	*gi_sc;
	int			gi_rid;
};

struct gicv5_iwb_softc {
	struct mtx		sc_mtx;
	device_t		sc_dev;
	struct resource		*sc_mem;
	struct intr_pic		*sc_pic;
	struct gicv5_iwb_irqsrc	*sc_irqs;
	int			sc_mem_rid;
	u_int			sc_nirq;
};

static device_attach_t gicv5_iwb_attach;

static pic_disable_intr_t gicv5_iwb_disable_intr;
static pic_enable_intr_t gicv5_iwb_enable_intr;
static pic_map_intr_t gicv5_iwb_map_intr;
static pic_setup_intr_t gicv5_iwb_setup_intr;
static pic_teardown_intr_t gicv5_iwb_teardown_intr;
static pic_post_filter_t gicv5_iwb_post_filter;
static pic_post_ithread_t gicv5_iwb_post_ithread;
static pic_pre_ithread_t gicv5_iwb_pre_ithread;

static device_method_t gicv5_iwb_methods[] = {
	/* Bus interface */
	DEVMETHOD(device_attach,	gicv5_iwb_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	gicv5_iwb_disable_intr),
	DEVMETHOD(pic_enable_intr,	gicv5_iwb_enable_intr),
	DEVMETHOD(pic_map_intr,		gicv5_iwb_map_intr),
	DEVMETHOD(pic_setup_intr,	gicv5_iwb_setup_intr),
	DEVMETHOD(pic_teardown_intr,	gicv5_iwb_teardown_intr),
	DEVMETHOD(pic_post_filter,	gicv5_iwb_post_filter),
	DEVMETHOD(pic_post_ithread,	gicv5_iwb_post_ithread),
	DEVMETHOD(pic_pre_ithread,	gicv5_iwb_pre_ithread),

	/* End */
	DEVMETHOD_END
};

static DEFINE_CLASS_0(iwb, gicv5_iwb_driver, gicv5_iwb_methods,
    sizeof(struct gicv5_iwb_softc));

static void
gicv5_iwb_wait_for_wenabler(struct gicv5_iwb_softc *sc)
{
	uint32_t reg;
	int timeout;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	/* Timeout of ~10ms */
	timeout = 10000;
	do {
		reg = bus_read_4(sc->sc_mem, IWB_WENABLE_STATUSR);
		if ((reg & WENABLE_STATUSR_IDLE) != 0)
			return;
		DELAY(1);
	} while (--timeout > 0);

	device_printf(sc->sc_dev, "IWB_WENABLE_STATUSR timeout\n");
}

static int
gicv5_iwb_intr(void *arg)
{
	struct gicv5_iwb_irqsrc *gi = arg;
	struct trapframe *tf;

	tf = curthread->td_intr_frame;
	if (intr_isrc_dispatch(&gi->gi_isrc, tf) != 0) {
		device_t dev;

		dev = gi->gi_sc->sc_dev;
		gicv5_iwb_disable_intr(dev, &gi->gi_isrc);
		device_printf(dev, "Stray irq %u disabled\n",
		    gi->gi_irq);
	}

	return (FILTER_HANDLED);
}

static int
gicv5_iwb_attach(device_t dev)
{
	struct gicv5_iwb_softc *sc;
	const char *name;
	uint32_t cr0;
	int count, error;
	u_int i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, "GICv5 IWB lock", NULL, MTX_SPIN);

	sc->sc_mem_rid = 0;
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem == NULL)
		return (ENXIO);

	cr0 = bus_read_4(sc->sc_mem, IWB_CR0);
	if ((cr0 & CR0_IWBEN) == 0) {
		device_printf(dev, "IWB not enabled in firmware: %x\n", cr0);
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
		    sc->sc_mem);
		return (ENXIO);
	}

	sc->sc_nirq = IDR0_IW_RANGE_IRQs(bus_read_4(sc->sc_mem, IWB_IDR0));
	if (bootverbose)
		device_printf(dev, "Found %u irqs\n", sc->sc_nirq);

	/* Disable all interrupts */
	for (int i = 0; i < sc->sc_nirq; i += 32)
		bus_write_4(sc->sc_mem, IWB_WENABLER(i), 0);
	mtx_lock_spin(&sc->sc_mtx);
	gicv5_iwb_wait_for_wenabler(sc);
	mtx_unlock_spin(&sc->sc_mtx);

	/* Allocate an MSI for each interrupt */
	count = sc->sc_nirq;
	error = PCI_ALLOC_MSI(device_get_parent(dev), dev, &count);
	if (error != 0) {
		device_printf(dev, "Unable to allocate MSI interrupts\n");
		return (error);
	}

	name = device_get_nameunit(dev);
	sc->sc_irqs = mallocarray(sc->sc_nirq, sizeof(struct gicv5_iwb_irqsrc),
	    M_DEVBUF, M_ZERO | M_WAITOK);
	for (i = 0; i < sc->sc_nirq; i++) {
		sc->sc_irqs[i].gi_irq = i;
		sc->sc_irqs[i].gi_sc = sc;

		sc->sc_irqs[i].gi_rid = i + 1;
		sc->sc_irqs[i].gi_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->sc_irqs[i].gi_rid, RF_ACTIVE);
		if (sc->sc_irqs[i].gi_res == NULL) {
			device_printf(dev,
			    "Unable to allocate MSI for wire %d\n", i);
			error = ENXIO;
			goto exit;
		}

		error = bus_setup_intr(dev, sc->sc_irqs[i].gi_res,
		    INTR_TYPE_MISC | INTR_MPSAFE, gicv5_iwb_intr, NULL,
		    &sc->sc_irqs[i], &sc->sc_irqs[i].gi_cookie);
		if (error != 0) {
			device_printf(dev, "Unable to setup MSI for wire %d\n",
			    i);
			goto exit_setup_intr;
		}

		error = intr_isrc_register(&sc->sc_irqs[i].gi_isrc, dev,
		    0, "%s,%u", name, i);
		if (error != 0) {
			device_printf(dev, "Unable to register wire %d\n", i);
			goto exit_isrc;
		}
	}

	return (0);

exit_isrc:
	bus_teardown_intr(dev, sc->sc_irqs[i].gi_res, sc->sc_irqs[i].gi_cookie);
exit_setup_intr:
	bus_release_resource(dev, SYS_RES_IRQ,
	    sc->sc_irqs[i].gi_rid, sc->sc_irqs[i].gi_res);
exit:
	sc->sc_irqs[i].gi_res = NULL;

	for (u_int j = 0; j < i; j++) {
		MPASS(sc->sc_irqs[j].gi_res != NULL);

		intr_isrc_deregister(&sc->sc_irqs[j].gi_isrc);
		if (sc->sc_irqs[j].gi_cookie != NULL)
			bus_teardown_intr(dev, sc->sc_irqs[j].gi_res,
			    sc->sc_irqs[j].gi_cookie);
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->sc_irqs[j].gi_rid, sc->sc_irqs[j].gi_res);
	}

	/* TODO: Implement in buses this could attach to */
	/*PCI_RELEASE_MSI(device_get_parent(dev), dev, count);*/

	return (error);
}

static void
gicv5_iwb_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_iwb_irqsrc *gi = (struct gicv5_iwb_irqsrc *)isrc;
	struct gicv5_iwb_softc *sc;
	uint32_t reg;
	u_int irq;

	sc = device_get_softc(dev);
	irq = gi->gi_irq;

	mtx_lock_spin(&sc->sc_mtx);
	reg = bus_read_4(sc->sc_mem, IWB_WENABLER(irq));
	reg &= ~WENABLER_ENABLED(irq);
	bus_write_4(sc->sc_mem, IWB_WENABLER(irq), reg);

	gicv5_iwb_wait_for_wenabler(sc);
	mtx_unlock_spin(&sc->sc_mtx);
}

static void
gicv5_iwb_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_iwb_irqsrc *gi = (struct gicv5_iwb_irqsrc *)isrc;
	struct gicv5_iwb_softc *sc;
	uint32_t reg;
	u_int irq;

	sc = device_get_softc(dev);
	irq = gi->gi_irq;

	mtx_lock_spin(&sc->sc_mtx);
	reg = bus_read_4(sc->sc_mem, IWB_WENABLER(irq));
	reg |= WENABLER_ENABLED(irq);
	bus_write_4(sc->sc_mem, IWB_WENABLER(irq), reg);

	gicv5_iwb_wait_for_wenabler(sc);
	mtx_unlock_spin(&sc->sc_mtx);
}

#ifdef FDT
static int
gicv5_iwb_map_fdt(device_t dev, u_int ncells, pcell_t *cells, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	if (ncells < 2)
		return (EINVAL);

	/*
	 * The 1st cell contains the interrupt number
	 * The 2nd cell is the flags, encoded as follows:
	 *   bits[3:0] trigger type and level flags
	 */
	*irqp = cells[0];

	switch (cells[1] & FDT_INTR_MASK) {
	case FDT_INTR_EDGE_RISING:
		*trigp = INTR_TRIGGER_EDGE;
		*polp = INTR_POLARITY_HIGH;
		break;
	case FDT_INTR_EDGE_FALLING:
		*trigp = INTR_TRIGGER_EDGE;
		*polp = INTR_POLARITY_LOW;
		break;
	case FDT_INTR_LEVEL_HIGH:
		*trigp = INTR_TRIGGER_LEVEL;
		*polp = INTR_POLARITY_HIGH;
		break;
	case FDT_INTR_LEVEL_LOW:
		*trigp = INTR_TRIGGER_LEVEL;
		*polp = INTR_POLARITY_LOW;
		break;
	default:
		device_printf(dev, "unsupported trigger/polarity "
		    "configuration 0x%02x\n", cells[2]);
		return (EINVAL);
	}

	return (0);
}
#endif

static int
do_gicv5_iwb_map_intr(device_t dev, struct intr_map_data *data, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	struct gicv5_iwb_softc *sc;
	enum intr_polarity pol;
	enum intr_trigger trig;
#ifdef FDT
	struct intr_map_data_fdt *daf;
#endif
	u_int irq;

	sc = device_get_softc(dev);

	switch (data->type) {
#ifdef FDT
	case INTR_MAP_DATA_FDT:
		daf = (struct intr_map_data_fdt *)data;
		if (gicv5_iwb_map_fdt(dev, daf->ncells, daf->cells, &irq, &pol,
		    &trig) != 0)
			return (EINVAL);
		break;
#endif
	default:
		return (EINVAL);
	}

	if (irq > sc->sc_nirq)
		return (EINVAL);

	switch (pol) {
	case INTR_POLARITY_CONFORM:
	case INTR_POLARITY_LOW:
	case INTR_POLARITY_HIGH:
		break;
	default:
		return (EINVAL);
	}
	switch (trig) {
	case INTR_TRIGGER_CONFORM:
	case INTR_TRIGGER_EDGE:
	case INTR_TRIGGER_LEVEL:
		break;
	default:
		return (EINVAL);
	}

	*irqp = irq;
	if (polp != NULL)
		*polp = pol;
	if (trigp != NULL)
		*trigp = trig;
	return (0);
}

static int
gicv5_iwb_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct gicv5_iwb_softc *sc;
	u_int irq;
	int error;

	error = do_gicv5_iwb_map_intr(dev, data, &irq, NULL, NULL);
	if (error == 0) {
		sc = device_get_softc(dev);
		*isrcp = &sc->sc_irqs[irq].gi_isrc;
	}
	return (error);
}

static int
gicv5_iwb_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct gicv5_iwb_irqsrc *gi = (struct gicv5_iwb_irqsrc *)isrc;
	struct gicv5_iwb_softc *sc;
	enum intr_trigger trig;
	enum intr_polarity pol;
	uint32_t reg;
	u_int irq;
	int error;

	if (data == NULL)
		return (ENOTSUP);

	error = do_gicv5_iwb_map_intr(dev, data, &irq, &pol, &trig);
	if (error != 0)
		return (error);

	if (gi->gi_irq != irq || pol == INTR_POLARITY_CONFORM ||
	    trig == INTR_TRIGGER_CONFORM)
		return (EINVAL);

	/* Compare config if this is not first setup. */
	if (isrc->isrc_handlers != 0) {
		if (pol != gi->gi_pol || trig != gi->gi_trig)
			return (EINVAL);
		else
			return (0);
	}

	sc = device_get_softc(dev);

	gi->gi_pol = pol;
	gi->gi_trig = trig;

	mtx_lock_spin(&sc->sc_mtx);
	reg = bus_read_4(sc->sc_mem, IWB_WTMR(irq));
	reg &= ~WTMR_MASK(irq);
	if (trig == INTR_TRIGGER_LEVEL)
		reg |= WTMR_LEVEL(irq);
	bus_write_4(sc->sc_mem, IWB_WTMR(irq), reg);
	mtx_unlock_spin(&sc->sc_mtx);

	return (0);
}

static int
gicv5_iwb_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	return (0);
}

static void
gicv5_iwb_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	/* Handled by the parent as we use a filter to dispatch */
}

static void
gicv5_iwb_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	gicv5_iwb_enable_intr(dev, isrc);
}

static void
gicv5_iwb_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	gicv5_iwb_disable_intr(dev, isrc);
}

#ifdef FDT
struct gicv5_iwb_fdt_softc {
	struct gicv5_iwb_softc sc_base;
};

static device_probe_t gicv5_iwb_fdt_probe;
static device_attach_t gicv5_iwb_fdt_attach;

static device_method_t gicv5_iwb_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gicv5_iwb_fdt_probe),
	DEVMETHOD(device_attach,	gicv5_iwb_fdt_attach),

	/* End */
	DEVMETHOD_END
};

#define iwb_baseclasses iwbv5_fdt_baseclasses
DEFINE_CLASS_1(iwb, gicv5_iwb_fdt_driver, gicv5_iwb_fdt_methods,
    sizeof(struct gicv5_iwb_fdt_softc), gicv5_iwb_driver);
#undef iwb_baseclasses

/* This needs to be after the ITS as it sends MSI messages there */
EARLY_DRIVER_MODULE(gicv5_iwb, simplebus, gicv5_iwb_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);

static int
gicv5_iwb_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "arm,gic-v5-iwb"))
		return (ENXIO);

	device_set_desc(dev, "ARM GICv5 Interrupt Wire Bridge");
	return (BUS_PROBE_DEFAULT);
}

static int
gicv5_iwb_fdt_attach(device_t dev)
{
	struct gicv5_iwb_fdt_softc *sc;
	intptr_t xref;
	phandle_t node;
	int error;

	error = gicv5_iwb_attach(dev);
	if (error != 0)
		return (error);

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	xref = OF_xref_from_node(node);
	sc->sc_base.sc_pic = intr_pic_register(dev, xref);
	if (sc->sc_base.sc_pic == NULL)
		return (ENXIO);

	OF_device_register_xref(xref, dev);

	return (0);
}
#endif /* FDT */
