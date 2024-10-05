/*-
 * Copyright (c) 2015-2017 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 * Copyright (c) 2021 Jessica Clarke <jrtc27@FreeBSD.org>
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/cpuset.h>
#include <sys/interrupt.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"

#define	INTC_NIRQS	16

struct intc_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
};

struct intc_softc {
	device_t		dev;
	struct intc_irqsrc	isrcs[INTC_NIRQS];
};

static int	intc_intr(void *arg);

static phandle_t
intc_ofw_find(device_t dev, uint32_t hartid)
{
	phandle_t node;
	pcell_t reg;

	node = OF_finddevice("/cpus");
	if (node == -1) {
		device_printf(dev, "Can't find cpus node\n");
		return ((phandle_t)-1);
	}

	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;

		if (!ofw_bus_node_is_compatible(node, "riscv"))
			continue;

		if (OF_searchencprop(node, "reg", &reg, sizeof(reg)) == -1)
			continue;

		if (reg == hartid)
			break;
	}

	if (node == 0) {
		device_printf(dev, "Can't find boot cpu node\n");
		return ((phandle_t)-1);
	}

	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;

		if (ofw_bus_node_is_compatible(node, "riscv,cpu-intc"))
			break;
	}

	if (node == 0) {
		device_printf(dev,
		    "Can't find boot cpu local interrupt controller\n");
		return ((phandle_t)-1);
	}

	return (node);
}

static void
intc_identify(driver_t *driver, device_t parent)
{
	device_t dev;
	phandle_t node;

	if (device_find_child(parent, "intc", -1) != NULL)
		return;

	node = intc_ofw_find(parent, PCPU_GET(hart));
	if (node == -1)
		return;

	dev = simplebus_add_device(parent, node, 0, "intc", -1, NULL);
	if (dev == NULL)
		device_printf(parent, "Can't add intc child\n");
}

static int
intc_probe(device_t dev)
{
	device_set_desc(dev, "RISC-V Local Interrupt Controller");

	return (BUS_PROBE_NOWILDCARD);
}

static int
intc_attach(device_t dev)
{
	struct intc_irqsrc *isrcs;
	struct intc_softc *sc;
	struct intr_pic *pic;
	const char *name;
	phandle_t xref;
	u_int flags;
	int i, error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	name = device_get_nameunit(dev);
	xref = OF_xref_from_node(ofw_bus_get_node(dev));

	isrcs = sc->isrcs;
	for (i = 0; i < INTC_NIRQS; i++) {
		isrcs[i].irq = i;
		flags = i == IRQ_SOFTWARE_SUPERVISOR
		    ? INTR_ISRCF_IPI : INTR_ISRCF_PPI;
		error = intr_isrc_register(&isrcs[i].isrc, sc->dev, flags,
		    "%s,%u", name, i);
		if (error != 0) {
			device_printf(dev, "Can't register interrupt %d\n", i);
			return (error);
		}
	}

	pic = intr_pic_register(sc->dev, xref);
	if (pic == NULL)
		return (ENXIO);

	return (intr_pic_claim_root(sc->dev, xref, intc_intr, sc, INTR_ROOT_IRQ));
}

static void
intc_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	u_int irq;

	irq = ((struct intc_irqsrc *)isrc)->irq;
	if (irq >= INTC_NIRQS)
		panic("%s: Unsupported IRQ %u", __func__, irq);

	csr_clear(sie, 1ul << irq);
}

static void
intc_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	u_int irq;

	irq = ((struct intc_irqsrc *)isrc)->irq;
	if (irq >= INTC_NIRQS)
		panic("%s: Unsupported IRQ %u", __func__, irq);

	csr_set(sie, 1ul << irq);
}

static int
intc_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct intr_map_data_fdt *daf;
	struct intc_softc *sc;

	sc = device_get_softc(dev);

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 1 || daf->cells[0] >= INTC_NIRQS)
		return (EINVAL);

	*isrcp = &sc->isrcs[daf->cells[0]].isrc;

	return (0);
}

static int
intc_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	if (isrc->isrc_flags & INTR_ISRCF_PPI)
		CPU_SET(PCPU_GET(cpuid), &isrc->isrc_cpu);

	return (0);
}

#ifdef SMP
static void
intc_init_secondary(device_t dev, uint32_t rootnum)
{
	struct intc_softc *sc;
	struct intr_irqsrc *isrc;
	u_int cpu, irq;

	sc = device_get_softc(dev);
	cpu = PCPU_GET(cpuid);

	/* Unmask attached interrupts */
	for (irq = 0; irq < INTC_NIRQS; irq++) {
		isrc = &sc->isrcs[irq].isrc;
		if (intr_isrc_init_on_cpu(isrc, cpu))
			intc_enable_intr(dev, isrc);
	}
}
#endif

static int
intc_intr(void *arg)
{
	struct trapframe *frame;
	struct intc_softc *sc;
	uint64_t active_irq;
	struct intc_irqsrc *src;

	sc = arg;
	frame = curthread->td_intr_frame;

	KASSERT((frame->tf_scause & SCAUSE_INTR) != 0,
	    ("%s: not an interrupt frame", __func__));

	active_irq = frame->tf_scause & SCAUSE_CODE;

	if (active_irq >= INTC_NIRQS)
		return (FILTER_HANDLED);

	src = &sc->isrcs[active_irq];
	if (intr_isrc_dispatch(&src->isrc, frame) != 0) {
		intc_disable_intr(sc->dev, &src->isrc);
		device_printf(sc->dev, "Stray irq %lu disabled\n",
		    active_irq);
	}

	return (FILTER_HANDLED);
}

static device_method_t intc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	intc_identify),
	DEVMETHOD(device_probe,		intc_probe),
	DEVMETHOD(device_attach,	intc_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	intc_disable_intr),
	DEVMETHOD(pic_enable_intr,	intc_enable_intr),
	DEVMETHOD(pic_map_intr,		intc_map_intr),
	DEVMETHOD(pic_setup_intr,	intc_setup_intr),
#ifdef SMP
	DEVMETHOD(pic_init_secondary,	intc_init_secondary),
#endif

	DEVMETHOD_END
};

DEFINE_CLASS_0(intc, intc_driver, intc_methods, sizeof(struct intc_softc));
EARLY_DRIVER_MODULE(intc, ofwbus, intc_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_FIRST);
