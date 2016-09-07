/*-
 * Copyright (c) 2015 Alexander Kabaev
 * Copyright (c) 2006 Oleksandr Tymoshenko
 * Copyright (c) 2002-2004 Juli Mallett <jmallett@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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

#include "opt_platform.h"
#include "opt_hwpmc_hooks.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/sched.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>

#include <machine/bus.h>
#include <machine/hwfunc.h>
#include <machine/intr.h>
#include <machine/smp.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "pic_if.h"

#define NHARD_IRQS	6
#define NSOFT_IRQS	2
#define NREAL_IRQS	(NHARD_IRQS + NSOFT_IRQS)

static int mips_pic_intr(void *);

struct intr_map_data_mips_pic {
	struct intr_map_data	hdr;
	u_int			irq;
};

struct mips_pic_irqsrc {
	struct intr_irqsrc	isrc;
	struct resource		*res;
	u_int			irq;
};

struct mips_pic_softc {
	device_t			pic_dev;
	struct mips_pic_irqsrc		pic_irqs[NREAL_IRQS];
	struct rman			pic_irq_rman;
	struct mtx			mutex;
	uint32_t			nirqs;
};

static struct mips_pic_softc *pic_sc;

#define PIC_INTR_ISRC(sc, irq)		(&(sc)->pic_irqs[(irq)].isrc)

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{"mti,cpu-interrupt-controller",	true},
	{NULL,					false}
};
#endif

#ifndef FDT
static void
mips_pic_identify(driver_t *drv, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "cpupic", 0);
}
#endif

static int
mips_pic_probe(device_t dev)
{

#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);
#endif
	device_set_desc(dev, "MIPS32 Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static inline void
pic_irq_unmask(struct mips_pic_softc *sc, u_int irq)
{

	mips_wr_status(mips_rd_status() | ((1 << irq) << 8));
}

static inline void
pic_irq_mask(struct mips_pic_softc *sc, u_int irq)
{

	mips_wr_status(mips_rd_status() & ~((1 << irq) << 8));
}

static inline intptr_t
pic_xref(device_t dev)
{
#ifdef FDT
	return (OF_xref_from_node(ofw_bus_get_node(dev)));
#else
	return (0);
#endif
}

static int
mips_pic_register_isrcs(struct mips_pic_softc *sc)
{
	int error;
	uint32_t irq, i, tmpirq;
	struct intr_irqsrc *isrc;
	char *name;

	for (irq = 0; irq < sc->nirqs; irq++) {
		sc->pic_irqs[irq].irq = irq;
		sc->pic_irqs[irq].res = rman_reserve_resource(&sc->pic_irq_rman,
		    irq, irq, 1, RF_ACTIVE, sc->pic_dev);
		if (sc->pic_irqs[irq].res == NULL) {
			device_printf(sc->pic_dev,
			    "%s failed to alloc resource for irq %u",
			    __func__, irq);
			return (ENOMEM);
		}
		isrc = PIC_INTR_ISRC(sc, irq);
		if (irq < NSOFT_IRQS) {
			name = "sint";
			tmpirq = irq;
		} else {
			name = "int";
			tmpirq = irq - NSOFT_IRQS;
		}
		error = intr_isrc_register(isrc, sc->pic_dev, 0, "%s%u",
		    name, tmpirq);
		if (error != 0) {
			for (i = 0; i < irq; i++) {
				intr_isrc_deregister(PIC_INTR_ISRC(sc, i));
			}
			device_printf(sc->pic_dev, "%s failed", __func__);
			return (error);
		}
	}

	return (0);
}

static int
mips_pic_attach(device_t dev)
{
	struct		mips_pic_softc *sc;
	intptr_t	xref = pic_xref(dev);

	if (pic_sc)
		return (ENXIO);

	sc = device_get_softc(dev);

	sc->pic_dev = dev;
	pic_sc = sc;

	/* Initialize mutex */
	mtx_init(&sc->mutex, "PIC lock", "", MTX_SPIN);

	/* Set the number of interrupts */
	sc->nirqs = nitems(sc->pic_irqs);

	/* Init the IRQ rman */
	sc->pic_irq_rman.rm_type = RMAN_ARRAY;
	sc->pic_irq_rman.rm_descr = "MIPS PIC IRQs";
	if (rman_init(&sc->pic_irq_rman) != 0 ||
	    rman_manage_region(&sc->pic_irq_rman, 0, sc->nirqs - 1) != 0) {
		device_printf(dev, "failed to setup IRQ rman\n");
		goto cleanup;
	}

	/* Register the interrupts */
	if (mips_pic_register_isrcs(sc) != 0) {
		device_printf(dev, "could not register PIC ISRCs\n");
		goto cleanup;
	}

	/*
	 * Now, when everything is initialized, it's right time to
	 * register interrupt controller to interrupt framefork.
	 */
	if (intr_pic_register(dev, xref) == NULL) {
		device_printf(dev, "could not register PIC\n");
		goto cleanup;
	}

	/* Claim our root controller role */
	if (intr_pic_claim_root(dev, xref, mips_pic_intr, sc, 0) != 0) {
		device_printf(dev, "could not set PIC as a root\n");
		intr_pic_deregister(dev, xref);
		goto cleanup;
	}

	return (0);

cleanup:
	return(ENXIO);
}

int
mips_pic_intr(void *arg)
{
	struct mips_pic_softc *sc = arg;
	register_t cause, status;
	int i, intr;

	cause = mips_rd_cause();
	status = mips_rd_status();
	intr = (cause & MIPS_INT_MASK) >> 8;
	/*
	 * Do not handle masked interrupts. They were masked by
	 * pre_ithread function (mips_mask_XXX_intr) and will be
	 * unmasked once ithread is through with handler
	 */
	intr &= (status & MIPS_INT_MASK) >> 8;
	while ((i = fls(intr)) != 0) {
		i--; /* Get a 0-offset interrupt. */
		intr &= ~(1 << i);

		if (intr_isrc_dispatch(PIC_INTR_ISRC(sc, i),
		    curthread->td_intr_frame) != 0) {
			device_printf(sc->pic_dev,
			    "Stray interrupt %u detected\n", i);
			pic_irq_mask(sc, i);
			continue;
		}
	}

	KASSERT(i == 0, ("all interrupts handled"));

#ifdef HWPMC_HOOKS
	if (pmc_hook && (PCPU_GET(curthread)->td_pflags & TDP_CALLCHAIN)) {
		struct trapframe *tf = PCPU_GET(curthread)->td_intr_frame;

		pmc_hook(PCPU_GET(curthread), PMC_FN_USER_CALLCHAIN, tf);
	}
#endif
	return (FILTER_HANDLED);
}

static void
mips_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	u_int irq;

	irq = ((struct mips_pic_irqsrc *)isrc)->irq;
	pic_irq_mask(device_get_softc(dev), irq);
}

static void
mips_pic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	u_int irq;

	irq = ((struct mips_pic_irqsrc *)isrc)->irq;
	pic_irq_unmask(device_get_softc(dev), irq);
}

static int
mips_pic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct mips_pic_softc *sc;
	int res;

	sc = device_get_softc(dev);
	res = 0;
#ifdef FDT
	if (data->type == INTR_MAP_DATA_FDT) {
		struct intr_map_data_fdt *daf;

		daf = (struct intr_map_data_fdt *)data;

		if (daf->ncells != 1 || daf->cells[0] >= sc->nirqs)
			return (EINVAL);

		*isrcp = PIC_INTR_ISRC(sc, daf->cells[0]);
	} else
#endif
	if (data->type == INTR_MAP_DATA_PLAT_1) {
		struct intr_map_data_mips_pic *mpd;

		mpd = (struct intr_map_data_mips_pic *)data;

		if (mpd->irq < 0 || mpd->irq >= sc->nirqs)
			return (EINVAL);

		*isrcp = PIC_INTR_ISRC(sc, mpd->irq);
	} else {
		res = ENOTSUP;
	}

	return (res);
}

static void
mips_pic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	mips_pic_disable_intr(dev, isrc);
}

static void
mips_pic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	mips_pic_enable_intr(dev, isrc);
}

static void
mips_pic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
}

static device_method_t mips_pic_methods[] = {
	/* Device interface */
#ifndef FDT
	DEVMETHOD(device_identify,	mips_pic_identify),
#endif
	DEVMETHOD(device_probe,		mips_pic_probe),
	DEVMETHOD(device_attach,	mips_pic_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	mips_pic_disable_intr),
	DEVMETHOD(pic_enable_intr,	mips_pic_enable_intr),
	DEVMETHOD(pic_map_intr,		mips_pic_map_intr),
	DEVMETHOD(pic_pre_ithread,	mips_pic_pre_ithread),
	DEVMETHOD(pic_post_ithread,	mips_pic_post_ithread),
	DEVMETHOD(pic_post_filter,	mips_pic_post_filter),

	{ 0, 0 }
};

static driver_t mips_pic_driver = {
	"cpupic",
	mips_pic_methods,
	sizeof(struct mips_pic_softc),
};

static devclass_t mips_pic_devclass;

#ifdef FDT
EARLY_DRIVER_MODULE(cpupic, ofwbus, mips_pic_driver, mips_pic_devclass, 0, 0,
    BUS_PASS_INTERRUPT);
#else
EARLY_DRIVER_MODULE(cpupic, nexus, mips_pic_driver, mips_pic_devclass, 0, 0,
    BUS_PASS_INTERRUPT);
#endif

void
cpu_init_interrupts(void)
{
}

int
cpu_create_intr_map(int irq)
{
	struct intr_map_data_mips_pic *mips_pic_data;
	intptr_t iparent;
	size_t len;
	u_int new_irq;

	len = sizeof(*mips_pic_data);
	iparent = pic_xref(pic_sc->pic_dev);

	/* Allocate mips_pic data and fill it in */
	mips_pic_data = (struct intr_map_data_mips_pic *)intr_alloc_map_data(
	    INTR_MAP_DATA_PLAT_1, len, M_WAITOK | M_ZERO);
	mips_pic_data->irq = irq;

	/* Get the new irq number */
	new_irq = intr_map_irq(pic_sc->pic_dev, iparent,
	    (struct intr_map_data *)mips_pic_data);

	/* Adjust the resource accordingly */
	rman_set_start(pic_sc->pic_irqs[irq].res, new_irq);
	rman_set_end(pic_sc->pic_irqs[irq].res, new_irq);

	/* Activate the new irq */
	return (intr_activate_irq(pic_sc->pic_dev, pic_sc->pic_irqs[irq].res));
}

struct resource *
cpu_get_irq_resource(int irq)
{

	KASSERT(pic_sc != NULL, ("%s: no pic", __func__));

	if (irq < 0 || irq >= pic_sc->nirqs)
		panic("%s called for unknown irq %d", __func__, irq);

	return pic_sc->pic_irqs[irq].res;
}

void
cpu_establish_hardintr(const char *name, driver_filter_t *filt,
    void (*handler)(void*), void *arg, int irq, int flags, void **cookiep)
{
	int res;

	/*
	 * We have 6 levels, but thats 0 - 5 (not including 6)
	 */
	if (irq < 0 || irq >= NHARD_IRQS)
		panic("%s called for unknown hard intr %d", __func__, irq);

	KASSERT(pic_sc != NULL, ("%s: no pic", __func__));

	irq += NSOFT_IRQS;

	res = cpu_create_intr_map(irq);
	if (res != 0) panic("Unable to create map for hard IRQ %d", irq);

	res = intr_setup_irq(pic_sc->pic_dev, pic_sc->pic_irqs[irq].res, filt,
	    handler, arg, flags, cookiep);
	if (res != 0) panic("Unable to add hard IRQ %d handler", irq);
}

void
cpu_establish_softintr(const char *name, driver_filter_t *filt,
    void (*handler)(void*), void *arg, int irq, int flags,
    void **cookiep)
{
	int res;

	if (irq < 0 || irq > NSOFT_IRQS)
		panic("%s called for unknown soft intr %d", __func__, irq);

	KASSERT(pic_sc != NULL, ("%s: no pic", __func__));

	res = cpu_create_intr_map(irq);
	if (res != 0) panic("Unable to create map for soft IRQ %d", irq);

	res = intr_setup_irq(pic_sc->pic_dev, pic_sc->pic_irqs[irq].res, filt,
	    handler, arg, flags, cookiep);
	if (res != 0) panic("Unable to add soft IRQ %d handler", irq);
}

