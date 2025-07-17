/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 * Copyright (c) 2019 Mitchell Horne <mhorne@FreeBSD.org>
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory (Department of Computer Science
 * and Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of
 * the DARPA SSITH research programme.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/interrupt-controller/irq.h>

#include "pic_if.h"

#define	PLIC_MAX_IRQS		1024

#define	PLIC_PRIORITY_BASE	0x000000U

#define	PLIC_ENABLE_BASE	0x002000U
#define	PLIC_ENABLE_STRIDE	0x80U

#define	PLIC_CONTEXT_BASE	0x200000U
#define	PLIC_CONTEXT_STRIDE	0x1000U
#define	PLIC_CONTEXT_THRESHOLD	0x0U
#define	PLIC_CONTEXT_CLAIM	0x4U

#define	PLIC_PRIORITY(_irq)	(PLIC_PRIORITY_BASE + (_irq) * sizeof(uint32_t))
#define	PLIC_ENABLE(_sc, _irq, _cpu)					\
    (_sc->contexts[_cpu].enable_offset + ((_irq) / 32) * sizeof(uint32_t))
#define	PLIC_THRESHOLD(_sc, _cpu)					\
    (_sc->contexts[_cpu].context_offset + PLIC_CONTEXT_THRESHOLD)
#define	PLIC_CLAIM(_sc, _cpu)						\
    (_sc->contexts[_cpu].context_offset + PLIC_CONTEXT_CLAIM)

static pic_disable_intr_t	plic_disable_intr;
static pic_enable_intr_t	plic_enable_intr;
static pic_map_intr_t		plic_map_intr;
static pic_setup_intr_t		plic_setup_intr;
static pic_post_ithread_t	plic_post_ithread;
static pic_pre_ithread_t	plic_pre_ithread;
static pic_bind_intr_t		plic_bind_intr;

struct plic_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
	u_int			trigtype;
};

struct plic_context {
	bus_size_t enable_offset;
	bus_size_t context_offset;
};

struct plic_softc {
	device_t		dev;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*ih;
	struct plic_irqsrc	isrcs[PLIC_MAX_IRQS];
	struct plic_context	contexts[MAXCPU];
	int			ndev;
};

static struct ofw_compat_data compat_data[] = {
	{ "riscv,plic0",	1 },
	{ "sifive,plic-1.0.0",	1 },
	{ "thead,c900-plic",	1 },
	{ NULL,			0 }
};

#define	RD4(sc, reg)				\
    bus_read_4(sc->mem_res, (reg))
#define	WR4(sc, reg, val)			\
    bus_write_4(sc->mem_res, (reg), (val))

static u_int plic_irq_cpu;

static int
riscv_hartid_to_cpu(int hartid)
{
	int i;

	CPU_FOREACH(i) {
		if (pcpu_find(i)->pc_hart == hartid)
			return (i);
	}

	return (-1);
}

static int
plic_get_hartid(device_t dev, phandle_t intc)
{
	int hart;

	/* Check the interrupt controller layout. */
	if (OF_searchencprop(intc, "#interrupt-cells", &hart,
	    sizeof(hart)) == -1) {
		device_printf(dev,
		    "Could not find #interrupt-cells for phandle %u\n", intc);
		return (-1);
	}

	/*
	 * The parent of the interrupt-controller is the CPU we are
	 * interested in, so search for its hart ID.
	 */
	if (OF_searchencprop(OF_parent(intc), "reg", (pcell_t *)&hart,
	    sizeof(hart)) == -1) {
		device_printf(dev, "Could not find hartid\n");
		return (-1);
	}

	return (hart);
}

static inline void
plic_irq_dispatch(struct plic_softc *sc, u_int irq,
    struct trapframe *tf)
{
	struct plic_irqsrc *src;

	src = &sc->isrcs[irq];

	if (intr_isrc_dispatch(&src->isrc, tf) != 0)
		device_printf(sc->dev, "Stray irq %u detected\n", irq);
}

static int
plic_intr(void *arg)
{
	struct plic_softc *sc;
	struct trapframe *tf;
	uint32_t pending;
	uint32_t cpu;

	sc = arg;
	cpu = PCPU_GET(cpuid);

	/* Claim all pending interrupts. */
	while ((pending = RD4(sc, PLIC_CLAIM(sc, cpu))) != 0) {
		tf = curthread->td_intr_frame;
		plic_irq_dispatch(sc, pending, tf);
	}

	return (FILTER_HANDLED);
}

static void
plic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct plic_softc *sc;
	struct plic_irqsrc *src;

	sc = device_get_softc(dev);
	src = (struct plic_irqsrc *)isrc;

	WR4(sc, PLIC_PRIORITY(src->irq), 0);
}

static void
plic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct plic_softc *sc;
	struct plic_irqsrc *src;

	sc = device_get_softc(dev);
	src = (struct plic_irqsrc *)isrc;

	WR4(sc, PLIC_PRIORITY(src->irq), 1);
}

static int
plic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct intr_map_data_fdt *daf;
	struct plic_softc *sc;
	u_int irq, type;

	sc = device_get_softc(dev);

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 1 && daf->ncells != 2) {
		device_printf(dev, "invalid ncells value: %u\n", daf->ncells);
		return (EINVAL);
	}

	irq = daf->cells[0];
	type = daf->ncells == 2 ? daf->cells[1] : IRQ_TYPE_LEVEL_HIGH;

	if (irq > sc->ndev) {
		device_printf(dev, "irq (%u) > sc->ndev (%u)",
		    daf->cells[0], sc->ndev);
		return (EINVAL);
	}

	/*
	 * TODO: handling of edge-triggered interrupts.
	 *
	 * From sifive,plic-1.0.0.yaml:
	 *
	 * "The PLIC supports both edge-triggered and level-triggered
	 * interrupts. For edge-triggered interrupts, the RISC-V PLIC spec
	 * allows two responses to edges seen while an interrupt handler is
	 * active; the PLIC may either queue them or ignore them. In the first
	 * case, handlers are oblivious to the trigger type, so it is not
	 * included in the interrupt specifier. In the second case, software
	 * needs to know the trigger type, so it can reorder the interrupt flow
	 * to avoid missing interrupts. This special handling is needed by at
	 * least the Renesas RZ/Five SoC (AX45MP AndesCore with a NCEPLIC100)
	 * and the T-HEAD C900 PLIC."
	 *
	 * For now, prevent interrupts with type IRQ_TYPE_EDGE_RISING from
	 * allocation. Emit a message so that when the relevant driver fails to
	 * attach, it will at least be clear why.
	 */
	if (type != IRQ_TYPE_LEVEL_HIGH) {
		device_printf(dev, "edge-triggered interrupts not supported\n");
		return (EINVAL);
	}

	sc->isrcs[irq].trigtype = type;
	*isrcp = &sc->isrcs[irq].isrc;

	return (0);
}

static int
plic_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RISC-V PLIC");

	return (BUS_PROBE_DEFAULT);
}

static int
plic_attach(device_t dev)
{
	struct plic_irqsrc *isrcs;
	struct plic_softc *sc;
	struct intr_pic *pic;
	pcell_t *cells;
	uint32_t irq;
	const char *name;
	phandle_t node;
	phandle_t xref;
	uint32_t cpu;
	int error;
	int rid;
	int nintr;
	int context;
	int i;
	int hart;

	sc = device_get_softc(dev);

	sc->dev = dev;

	node = ofw_bus_get_node(dev);
	if ((OF_getencprop(node, "riscv,ndev", &sc->ndev,
	    sizeof(sc->ndev))) < 0) {
		device_printf(dev,
		    "Error: could not get number of devices\n");
		return (ENXIO);
	}

	if (sc->ndev >= PLIC_MAX_IRQS) {
		device_printf(dev,
		    "Error: invalid ndev (%d)\n", sc->ndev);
		return (ENXIO);
	}

	/* Request memory resources */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev,
		    "Error: could not allocate memory resources\n");
		return (ENXIO);
	}

	/* Register the interrupt sources */
	isrcs = sc->isrcs;
	name = device_get_nameunit(sc->dev);
	for (irq = 1; irq <= sc->ndev; irq++) {
		isrcs[irq].irq = irq;
		error = intr_isrc_register(&isrcs[irq].isrc, sc->dev,
		    0, "%s,%u", name, irq);
		if (error != 0)
			return (error);

		WR4(sc, PLIC_PRIORITY(irq), 0);
	}

	/*
	 * Calculate the per-cpu enable and context register offsets.
	 *
	 * This is tricky for a few reasons. The PLIC divides the interrupt
	 * enable, threshold, and claim bits by "context", where each context
	 * routes to a core's local interrupt controller.
	 *
	 * The tricky part is that the PLIC spec imposes no restrictions on how
	 * these contexts are laid out. So for example, there is no guarantee
	 * that each CPU will have both a machine mode and supervisor context,
	 * or that different PLIC implementations will organize the context
	 * registers in the same way. On top of this, we must handle the fact
	 * that cpuid != hartid, as they may have been renumbered during boot.
	 * We perform the following steps:
	 *
	 * 1. Examine the PLIC's "interrupts-extended" property and skip any
	 *    entries that are not for supervisor external interrupts.
	 *
	 * 2. Walk up the device tree to find the corresponding CPU, and grab
	 *    its hart ID.
	 *
	 * 3. Convert the hart to a cpuid, and calculate the register offsets
	 *    based on the context number.
	 *
	 * 4. Save the index for the boot hart's S-mode external interrupt in
	 *    order to allocate and setup the corresponding resource, since the
	 *    local interrupt controller newbus device is associated with that
	 *    specific node.
	 */
	nintr = OF_getencprop_alloc_multi(node, "interrupts-extended",
	    sizeof(uint32_t), (void **)&cells);
	if (nintr <= 0) {
		device_printf(dev, "Could not read interrupts-extended\n");
		return (ENXIO);
	}

	/* interrupts-extended is a list of phandles and interrupt types. */
	rid = -1;
	for (i = 0, context = 0; i < nintr; i += 2, context++) {
		/* Skip M-mode external interrupts */
		if (cells[i + 1] != IRQ_EXTERNAL_SUPERVISOR)
			continue;

		/*
		 * Get the hart ID from the core's interrupt controller
		 * phandle.
		 */
		hart = plic_get_hartid(dev, OF_node_from_xref(cells[i]));
		if (hart < 0) {
			OF_prop_free(cells);
			return (ENXIO);
		}

		/* Get the corresponding cpuid. */
		cpu = riscv_hartid_to_cpu(hart);
		if (cpu < 0) {
			device_printf(dev, "Invalid hart!\n");
			OF_prop_free(cells);
			return (ENXIO);
		}

		if (cpu == 0)
			rid = i / 2;

		/* Set the enable and context register offsets for the CPU. */
		sc->contexts[cpu].enable_offset = PLIC_ENABLE_BASE +
		    context * PLIC_ENABLE_STRIDE;
		sc->contexts[cpu].context_offset = PLIC_CONTEXT_BASE +
		    context * PLIC_CONTEXT_STRIDE;
	}
	OF_prop_free(cells);

	if (rid == -1) {
		device_printf(dev,
		    "Could not find local interrupt controller\n");
		return (ENXIO);
	}

	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev,
		    "Error: could not allocate IRQ resources\n");
		return (ENXIO);
	}

	/* Set the threshold for each CPU to accept all priorities. */
	CPU_FOREACH(cpu)
		WR4(sc, PLIC_THRESHOLD(sc, cpu), 0);

	xref = OF_xref_from_node(node);
	pic = intr_pic_register(sc->dev, xref);
	if (pic == NULL)
		return (ENXIO);

	return (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CLK | INTR_MPSAFE,
	    plic_intr, NULL, sc, &sc->ih));
}

static void
plic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	plic_disable_intr(dev, isrc);
}

static void
plic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct plic_softc *sc;
	struct plic_irqsrc *src;
	uint32_t cpu;

	sc = device_get_softc(dev);
	src = (struct plic_irqsrc *)isrc;

	cpu = CPU_FFS(&isrc->isrc_cpu) - 1;

	/* Complete the interrupt. */
	WR4(sc, PLIC_CLAIM(sc, cpu), src->irq);
	plic_enable_intr(dev, isrc);
}

static int
plic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	CPU_ZERO(&isrc->isrc_cpu);
	plic_bind_intr(dev, isrc);

	return (0);
}

static int
plic_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct plic_softc *sc;
	struct plic_irqsrc *src;
	uint32_t reg;
	u_int cpu;

	sc = device_get_softc(dev);
	src = (struct plic_irqsrc *)isrc;

	/* Disable the interrupt source on all CPUs. */
	CPU_FOREACH(cpu) {
		reg = RD4(sc, PLIC_ENABLE(sc, src->irq, cpu));
		reg &= ~(1 << (src->irq % 32));
		WR4(sc, PLIC_ENABLE(sc, src->irq, cpu), reg);
	}

	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		cpu = plic_irq_cpu = intr_irq_next_cpu(plic_irq_cpu, &all_cpus);
		CPU_SETOF(cpu, &isrc->isrc_cpu);
	} else {
		/*
		 * We will only bind to a single CPU so select the first
		 * CPU found.
		 */
		cpu = CPU_FFS(&isrc->isrc_cpu) - 1;
	}

	/* Enable the interrupt on the selected CPU only. */
	reg = RD4(sc, PLIC_ENABLE(sc, src->irq, cpu));
	reg |= (1 << (src->irq % 32));
	WR4(sc, PLIC_ENABLE(sc, src->irq, cpu), reg);

	return (0);
}

static device_method_t plic_methods[] = {
	DEVMETHOD(device_probe,		plic_probe),
	DEVMETHOD(device_attach,	plic_attach),

	DEVMETHOD(pic_disable_intr,	plic_disable_intr),
	DEVMETHOD(pic_enable_intr,	plic_enable_intr),
	DEVMETHOD(pic_map_intr,		plic_map_intr),
	DEVMETHOD(pic_pre_ithread,	plic_pre_ithread),
	DEVMETHOD(pic_post_ithread,	plic_post_ithread),
	DEVMETHOD(pic_post_filter,	plic_post_ithread),
	DEVMETHOD(pic_setup_intr,	plic_setup_intr),
	DEVMETHOD(pic_bind_intr,	plic_bind_intr),

	DEVMETHOD_END
};

static driver_t plic_driver = {
	"plic",
	plic_methods,
	sizeof(struct plic_softc),
};

EARLY_DRIVER_MODULE(plic, simplebus, plic_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
