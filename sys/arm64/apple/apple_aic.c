/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Andrew Turner
 * Copyright (c) 2022 Michael J. Karels <karels@freebsd.org>
 * Copyright (c) 2022 Kyle Evans <kevans@FreeBSD.org>
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/machdep.h>
#ifdef SMP
#include <machine/intr.h>
#include <machine/smp.h>
#endif

#include <dev/fdt/fdt_intr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/interrupt-controller/apple-aic.h>

#include "pic_if.h"

#define	AIC_INFO		0x0004
#define  AIC_INFO_NDIE(val)	(((val) >> 24) & 0xf)
#define	 AIC_INFO_NIRQS(val)	((val) & 0x0000ffff)

#define	AIC_WHOAMI		0x2000
#define	AIC_EVENT		0x2004
#define  AIC_EVENT_DIE(val)	(((val) >> 24) & 0xff)
#define  AIC_EVENT_TYPE(val)	(((val) >> 16) & 0xff)
#define  AIC_EVENT_TYPE_NONE	0
#define  AIC_EVENT_TYPE_IRQ	1
#define  AIC_EVENT_TYPE_IPI	4
#define  AIC_EVENT_IRQ(val)	((val) & 0xffff)
#define  AIC_EVENT_IPI_OTHER	1
#define  AIC_EVENT_IPI_SELF	2
#define	AIC_IPI_SEND		0x2008
#define	AIC_IPI_ACK		0x200c
#define AIC_IPI_MASK_SET	0x2024
#define	AIC_IPI_MASK_CLR	0x2028
#define	 AIC_IPI_OTHER		0x00000001
#define	 AIC_IPI_SELF		0x80000000
#define	AIC_TARGET_CPU(irq)	(0x3000 + (irq) * 4)
#define	AIC_SW_SET(irq)		(0x4000 + (((irq) >> 5) * 4))
#define	AIC_SW_CLEAR(irq)	(0x4080 + (((irq) >> 5) * 4))
#define	AIC_MASK_SET(irq)	(0x4100 + (((irq) >> 5) * 4))
#define	AIC_MASK_CLEAR(irq)	(0x4180 + (((irq) >> 5) * 4))
#define	 AIC_IRQ_MASK(irq)	(1u << ((irq) & 0x1f))

#define AIC_IPI_LOCAL_RR_EL1	s3_5_c15_c0_0
#define AIC_IPI_GLOBAL_RR_EL1	s3_5_c15_c0_1

#define AIC_IPI_SR_EL1		s3_5_c15_c1_1
#define  AIC_IPI_SR_EL1_PENDING	(1 << 0)

#define AIC_FIQ_VM_TIMER	s3_5_c15_c1_3
#define	AIC_FIQ_VM_TIMER_VEN	(1 << 0)
#define	AIC_FIQ_VM_TIMER_PEN	(1 << 1)
#define	AIC_FIQ_VM_TIMER_BITS	(AIC_FIQ_VM_TIMER_VEN | AIC_FIQ_VM_TIMER_PEN)

#define CNTV_CTL_ENABLE		(1 << 0)
#define CNTV_CTL_IMASK		(1 << 1)
#define CNTV_CTL_ISTATUS	(1 << 2)
#define	CNTV_CTL_BITS		\
    (CNTV_CTL_ENABLE | CNTV_CTL_IMASK | CNTV_CTL_ISTATUS)

#define	AIC_MAXCPUS		32
#define	AIC_MAXDIES		4

static struct ofw_compat_data compat_data[] = {
	{ "apple,aic",				1 },
	{ NULL,					0 }
};

enum apple_aic_irq_type {
	AIC_TYPE_INVAL,
	AIC_TYPE_IRQ,
	AIC_TYPE_FIQ,
	AIC_TYPE_IPI,
};

struct apple_aic_irqsrc {
	struct intr_irqsrc	ai_isrc;
	enum apple_aic_irq_type	ai_type;
	struct {
		/* AIC_TYPE_IRQ */
		enum intr_polarity	ai_pol;
		enum intr_trigger	ai_trig;
		u_int			ai_irq;
	};
};

#ifdef SMP
#define AIC_NIPIS		INTR_IPI_COUNT
#endif

struct apple_aic_softc {
	device_t		 sc_dev;
	struct resource		*sc_mem;
	struct apple_aic_irqsrc	*sc_isrcs[AIC_MAXDIES];
	u_int			sc_nirqs;
	u_int			sc_ndie;
#ifdef SMP
	struct apple_aic_irqsrc	sc_ipi_srcs[AIC_NIPIS];
	uint32_t		*sc_ipimasks;
#endif
	u_int			*sc_cpuids;	/* cpu index to AIC CPU ID */
};

static u_int aic_next_cpu;

static device_probe_t apple_aic_probe;
static device_attach_t apple_aic_attach;

static pic_disable_intr_t apple_aic_disable_intr;
static pic_enable_intr_t apple_aic_enable_intr;
static pic_map_intr_t apple_aic_map_intr;
static pic_setup_intr_t apple_aic_setup_intr;
static pic_teardown_intr_t apple_aic_teardown_intr;
static pic_post_filter_t apple_aic_post_filter;
static pic_post_ithread_t apple_aic_post_ithread;
static pic_pre_ithread_t apple_aic_pre_ithread;
#ifdef SMP
static pic_bind_intr_t apple_aic_bind_intr;
static pic_init_secondary_t apple_aic_init_secondary;
static pic_ipi_send_t apple_aic_ipi_send;
static pic_ipi_setup_t apple_aic_ipi_setup;
#endif

static int apple_aic_irq(void *);
static int apple_aic_fiq(void *);

static int
apple_aic_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Apple Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
apple_aic_attach(device_t dev)
{
	struct apple_aic_softc *sc;
	struct intr_irqsrc *isrc;
	const char *name;
	intptr_t xref;
	int error, rid;
	u_int i, cpu, j, info;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "Unable to allocate memory\n");
		return (ENXIO);
	}

	info = bus_read_4(sc->sc_mem, AIC_INFO);
	sc->sc_nirqs = AIC_INFO_NIRQS(info);
	sc->sc_ndie = AIC_INFO_NDIE(info) + 1;
	if (bootverbose)
		device_printf(dev, "Found %d interrupts, %d die\n",
		    sc->sc_nirqs, sc->sc_ndie);

	for (i = 0; i < sc->sc_ndie; i++) {
		sc->sc_isrcs[i] = mallocarray(sc->sc_nirqs,
		    sizeof(**sc->sc_isrcs), M_DEVBUF, M_WAITOK | M_ZERO);
	}

#ifdef SMP
	sc->sc_ipimasks = malloc(sizeof(*sc->sc_ipimasks) * mp_maxid + 1,
	    M_DEVBUF, M_WAITOK | M_ZERO);
#endif
	sc->sc_cpuids = malloc(sizeof(*sc->sc_cpuids) * mp_maxid + 1,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	cpu = PCPU_GET(cpuid);
	sc->sc_cpuids[cpu] = bus_read_4(sc->sc_mem, AIC_WHOAMI);
	if (bootverbose)
		device_printf(dev, "BSP CPU %d: whoami %x\n", cpu,
		    sc->sc_cpuids[cpu]);

	name = device_get_nameunit(dev);
	for (i = 0; i < sc->sc_ndie; i++) {
		struct apple_aic_irqsrc *die_isrcs;

		die_isrcs = sc->sc_isrcs[i];
		for (j = 0; j < sc->sc_nirqs; j++) {
			isrc = &die_isrcs[j].ai_isrc;
			die_isrcs[j].ai_pol = INTR_POLARITY_CONFORM;
			die_isrcs[j].ai_trig = INTR_TRIGGER_CONFORM;
			die_isrcs[j].ai_type = AIC_TYPE_INVAL;
			die_isrcs[j].ai_irq = j;

			error = intr_isrc_register(isrc, dev, 0, "%s,d%us%u", name,
			    i, j);
			if (error != 0) {
				device_printf(dev, "Unable to register irq %u:%u\n",
				    i, j);
				return (error);
			}
		}
	}

	xref = OF_xref_from_node(ofw_bus_get_node(dev));
	if (intr_pic_register(dev, xref) == NULL) {
		device_printf(dev, "Unable to register interrupt handler\n");
		return (ENXIO);
	}

	if (intr_pic_claim_root(dev, xref, apple_aic_irq, sc,
	    INTR_ROOT_IRQ) != 0) {
		device_printf(dev,
		    "Unable to set root interrupt controller\n");
		intr_pic_deregister(dev, xref);
		return (ENXIO);
	}

	if (intr_pic_claim_root(dev, xref, apple_aic_fiq, sc,
	    INTR_ROOT_FIQ) != 0) {
		device_printf(dev,
		    "Unable to set root fiq controller\n");
		intr_pic_deregister(dev, xref);
		return (ENXIO);
	}

#ifdef SMP
	if (intr_ipi_pic_register(dev, 0) != 0) {
		device_printf(dev, "could not register for IPIs\n");
		return (ENXIO);
	}
#endif

	OF_device_register_xref(xref, dev);

	return (0);
}

static int
apple_aic_map_intr_fdt(struct apple_aic_softc *sc,
    struct intr_map_data_fdt *data, u_int *irq, enum apple_aic_irq_type *typep,
    enum intr_polarity *polp, enum intr_trigger *trigp, u_int *die)
{
	if (data->ncells != 3)
		return (EINVAL);

	/* XXX AIC2 */
	*die = 0;

	/*
	 * The first cell is the interrupt type:
	 *   0 = IRQ
	 *   1 = FIQ
	 * The second cell is the interrupt number
	 * The third cell is the flags
	 */
	switch(data->cells[0]) {
	case 0:
		if (typep != NULL)
			*typep = AIC_TYPE_IRQ;
		break;
	case 1:
		if (typep != NULL)
			*typep = AIC_TYPE_FIQ;
		break;
	default:
		return (EINVAL);
	}

	*irq = data->cells[1];
	if (*irq > sc->sc_nirqs)
		return (EINVAL);

	if (trigp != NULL) {
		if ((data->cells[2] & FDT_INTR_EDGE_MASK) != 0)
			*trigp = INTR_TRIGGER_EDGE;
		else
			*trigp = INTR_TRIGGER_LEVEL;
	}
	if (polp != NULL) {
		if ((data->cells[2] & FDT_INTR_LEVEL_HIGH) != 0)
			*polp = INTR_POLARITY_HIGH;
		else
			*polp = INTR_POLARITY_LOW;
	}

	return (0);
}

static int
apple_aic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct apple_aic_softc *sc;
	int error;
	u_int irq;
	u_int die;

	sc = device_get_softc(dev);

	error = 0;
	switch(data->type) {
	case INTR_MAP_DATA_FDT:
		error = apple_aic_map_intr_fdt(sc,
		    (struct intr_map_data_fdt *)data, &irq, NULL, NULL, NULL,
		    &die);
		if (error == 0)
			*isrcp = &sc->sc_isrcs[0 /* XXX */][irq].ai_isrc;
		break;
	default:
		return (ENOTSUP);
	}

	return (error);
}

static int
apple_aic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct apple_aic_softc *sc;
	enum apple_aic_irq_type type;
	struct apple_aic_irqsrc *ai;
	enum intr_trigger trig;
	enum intr_polarity pol;
	int error;
	u_int die, irq;

	sc = device_get_softc(dev);
	ai = (struct apple_aic_irqsrc *)isrc;

	if (data != NULL) {
		KASSERT(data->type == INTR_MAP_DATA_FDT,
		    ("%s: Only FDT data is supported (got %#x)", __func__,
		    data->type));
		error = apple_aic_map_intr_fdt(sc,
		    (struct intr_map_data_fdt *)data, &irq, &type, &pol, &trig,
		    &die);
		if (error != 0)
			return (error);
	} else {
		pol = INTR_POLARITY_CONFORM;
		trig = INTR_TRIGGER_CONFORM;
	}

	if (isrc->isrc_handlers != 0) {
		/* TODO */
		return (0);
	}

	if (pol == INTR_POLARITY_CONFORM)
		pol = INTR_POLARITY_LOW;
	if (trig == INTR_TRIGGER_CONFORM)
		trig = INTR_TRIGGER_EDGE;

	ai->ai_pol = pol;
	ai->ai_trig = trig;
	ai->ai_type = type;

	/*
	 * Only the timer uses FIQs. These could be sent to any CPU.
	 */
	switch (type) {
	case AIC_TYPE_IRQ:
		/* XXX die sensitive? */
		aic_next_cpu = intr_irq_next_cpu(aic_next_cpu, &all_cpus);
		bus_write_4(sc->sc_mem, AIC_TARGET_CPU(irq),
		    1 << sc->sc_cpuids[aic_next_cpu]);
		break;
	case AIC_TYPE_FIQ:
		isrc->isrc_flags |= INTR_ISRCF_PPI;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
apple_aic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	panic("%s\n", __func__);
}

static void
apple_aic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct apple_aic_irqsrc *ai;
	struct apple_aic_softc *sc;
	u_int irq;

	ai = (struct apple_aic_irqsrc *)isrc;
	irq = ai->ai_irq;
	switch(ai->ai_type) {
	case AIC_TYPE_IRQ:
		sc = device_get_softc(dev);
		bus_write_4(sc->sc_mem, AIC_MASK_CLEAR(irq), AIC_IRQ_MASK(irq));
		break;
	case AIC_TYPE_IPI:
		/* Nothing needed here. */
		break;
	case AIC_TYPE_FIQ:
		/* TODO */
		break;
	default:
		panic("%s: %x\n", __func__, ai->ai_type);
	}
}

static void
apple_aic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct apple_aic_irqsrc *ai;
	struct apple_aic_softc *sc;
	u_int irq;

	ai = (struct apple_aic_irqsrc *)isrc;
	irq = ai->ai_irq;
	switch(ai->ai_type) {
	case AIC_TYPE_IRQ:
		sc = device_get_softc(dev);
		bus_write_4(sc->sc_mem, AIC_MASK_SET(irq), AIC_IRQ_MASK(irq));
		break;
	case AIC_TYPE_IPI:
		/* Nothing needed here. */
		break;
	case AIC_TYPE_FIQ:
		/* TODO */
		break;
	default:
		panic("%s: %x\n", __func__, ai->ai_type);
	}
}

static void
apple_aic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct apple_aic_softc *sc;
	struct apple_aic_irqsrc *ai;
	int irq;

	ai = (struct apple_aic_irqsrc *)isrc;
	irq = ai->ai_irq;
	switch(ai->ai_type) {
	case AIC_TYPE_IRQ:
		sc = device_get_softc(dev);
		bus_write_4(sc->sc_mem, AIC_SW_CLEAR(irq), AIC_IRQ_MASK(irq));
		bus_write_4(sc->sc_mem, AIC_MASK_CLEAR(irq), AIC_IRQ_MASK(irq));
		break;
	case AIC_TYPE_FIQ:
		/* TODO */
		break;
	default:
		panic("%s: %x\n", __func__, ai->ai_type);
	}
}

static void
apple_aic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct apple_aic_softc *sc;
	struct apple_aic_irqsrc *ai;
	int irq;

	ai = (struct apple_aic_irqsrc *)isrc;
	sc = device_get_softc(dev);
	irq = ai->ai_irq;
	bus_write_4(sc->sc_mem, AIC_SW_CLEAR(irq), AIC_IRQ_MASK(irq));
	apple_aic_disable_intr(dev, isrc);
	/* ACK IT */
}

static void
apple_aic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct apple_aic_softc *sc;
	struct apple_aic_irqsrc *ai;
	int irq;

	ai = (struct apple_aic_irqsrc *)isrc;
	sc = device_get_softc(dev);
	irq = ai->ai_irq;

	bus_write_4(sc->sc_mem, AIC_MASK_CLEAR(irq), AIC_IRQ_MASK(irq));
	apple_aic_enable_intr(dev, isrc);
}

#ifdef SMP
static void
apple_aic_ipi_received(struct apple_aic_softc *sc, struct trapframe *tf)
{
	uint32_t mask;
	uint32_t ipi;
	int cpu;

	cpu = PCPU_GET(cpuid);

	mask = atomic_readandclear_32(&sc->sc_ipimasks[cpu]);

	while (mask != 0) {
		ipi = ffs(mask) - 1;
		mask &= ~(1 << ipi);

		intr_ipi_dispatch(ipi);
	}
}
#endif

static int
apple_aic_irq(void *arg)
{
	struct apple_aic_softc *sc;
	uint32_t die, event, irq, type;
	struct apple_aic_irqsrc	*aisrc;
	struct trapframe *tf;

	sc = arg;
	tf = curthread->td_intr_frame;

	event = bus_read_4(sc->sc_mem, AIC_EVENT);
	type = AIC_EVENT_TYPE(event);

	/* If we get an IPI here, we really goofed. */
	MPASS(type != AIC_EVENT_TYPE_IPI);

	if (type != AIC_EVENT_TYPE_IRQ) {
		if (type != AIC_EVENT_TYPE_NONE)
			device_printf(sc->sc_dev, "unexpected event type %d\n",
			    type);
		return (FILTER_STRAY);
	}

	die = AIC_EVENT_DIE(event);
	irq = AIC_EVENT_IRQ(event);

	if (die >= sc->sc_ndie)
		panic("%s: unexpected die %d", __func__, die);
	if (irq >= sc->sc_nirqs)
		panic("%s: unexpected irq %d", __func__, irq);

	aisrc = &sc->sc_isrcs[die][irq];
	if (intr_isrc_dispatch(&aisrc->ai_isrc, tf) != 0) {
		device_printf(sc->sc_dev, "Stray irq %u:%u disabled\n",
		    die, irq);
		return (FILTER_STRAY);
	}

	return (FILTER_HANDLED);
}

static int
apple_aic_fiq(void *arg)
{
	struct apple_aic_softc *sc;
	struct apple_aic_irqsrc *isrcs;
	struct trapframe *tf;

	sc = arg;
	tf = curthread->td_intr_frame;

#ifdef SMP
	/* Handle IPIs. */
	if ((READ_SPECIALREG(AIC_IPI_SR_EL1) & AIC_IPI_SR_EL1_PENDING) != 0) {
		WRITE_SPECIALREG(AIC_IPI_SR_EL1, AIC_IPI_SR_EL1_PENDING);
		apple_aic_ipi_received(sc, tf);
	}
#endif

	/*
	 * FIQs don't store any state in the interrupt controller at all outside
	 * of IPI handling, so we have to probe around outside of AIC to
	 * determine if we might have been fired off due to a timer.
	 */
	isrcs = sc->sc_isrcs[0];
	if ((READ_SPECIALREG(cntv_ctl_el0) & CNTV_CTL_BITS) ==
	    (CNTV_CTL_ENABLE | CNTV_CTL_ISTATUS)) {
		intr_isrc_dispatch(&isrcs[AIC_TMR_GUEST_VIRT].ai_isrc, tf);
	}

	if (has_hyp()) {
		uint64_t reg;

		if ((READ_SPECIALREG(cntp_ctl_el0) & CNTV_CTL_ISTATUS) != 0) {
			intr_isrc_dispatch(&isrcs[AIC_TMR_GUEST_PHYS].ai_isrc,
			    tf);
		}

		reg = READ_SPECIALREG(AIC_FIQ_VM_TIMER);
		if ((reg & AIC_FIQ_VM_TIMER_PEN) != 0) {
			intr_isrc_dispatch(&isrcs[AIC_TMR_HV_PHYS].ai_isrc, tf);
		}

		if ((reg & AIC_FIQ_VM_TIMER_VEN) != 0) {
			intr_isrc_dispatch(&isrcs[AIC_TMR_HV_VIRT].ai_isrc, tf);
		}
	}

	return (FILTER_HANDLED);
}

#ifdef SMP
static int
apple_aic_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct apple_aic_softc *sc = device_get_softc(dev);
	static int aic_next_cpu;
	uint32_t targets = 0;
	u_int irq, cpu;

	MPASS(((struct apple_aic_irqsrc *)isrc)->ai_type == AIC_TYPE_IRQ);
	irq = ((struct apple_aic_irqsrc *)isrc)->ai_irq;
	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		aic_next_cpu = intr_irq_next_cpu(aic_next_cpu, &all_cpus);
		CPU_SETOF(aic_next_cpu, &isrc->isrc_cpu);
		bus_write_4(sc->sc_mem, AIC_TARGET_CPU(irq),
		    sc->sc_cpuids[aic_next_cpu] << 1);
	} else {
		CPU_FOREACH_ISSET(cpu, &isrc->isrc_cpu) {
			targets |= sc->sc_cpuids[cpu] << 1;
		}
		bus_write_4(sc->sc_mem, AIC_TARGET_CPU(irq), targets);
	}
	return (0);
}

static void
apple_aic_ipi_send(device_t dev, struct intr_irqsrc *isrc, cpuset_t cpus,
    u_int ipi)
{
	struct apple_aic_softc *sc;
	uint64_t aff, localgrp, sendmask;
	u_int cpu;

	sc = device_get_softc(dev);
	sendmask = 0;
	localgrp = CPU_AFF1(CPU_AFFINITY(PCPU_GET(cpuid)));

	KASSERT(isrc == &sc->sc_ipi_srcs[ipi].ai_isrc,
	    ("%s: bad ISRC %p argument", __func__, isrc));
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ISSET(cpu, &cpus)) {
			aff = CPU_AFFINITY(cpu);
			sendmask = CPU_AFF0(aff);
			atomic_set_32(&sc->sc_ipimasks[cpu], 1 << ipi);

			/*
			 * The above write to sc_ipimasks needs to be visible
			 * before we write to the ipi register to avoid the
			 * targetted CPU missing the dispatch in
			 * apple_aic_ipi_received().  Note that WRITE_SPECIALREG
			 * isn't a memory operation, so we can't relax this to a
			 * a dmb.
			 */
			dsb(ishst);

			if (CPU_AFF1(aff) == localgrp) {
				WRITE_SPECIALREG(AIC_IPI_LOCAL_RR_EL1,
				    sendmask);
			} else {
				sendmask |= CPU_AFF1(aff) << 16;
				WRITE_SPECIALREG(AIC_IPI_GLOBAL_RR_EL1,
				    sendmask);
			}

			isb();
		}
	}
}

static int
apple_aic_ipi_setup(device_t dev, u_int ipi, struct intr_irqsrc **isrcp)
{
	struct apple_aic_softc *sc = device_get_softc(dev);
	struct apple_aic_irqsrc *ai;

	KASSERT(ipi < AIC_NIPIS, ("%s: ipi %u too high", __func__, ipi));

	ai = &sc->sc_ipi_srcs[ipi];
	ai->ai_type = AIC_TYPE_IPI;

	*isrcp = &ai->ai_isrc;
	return (0);
}

static void
apple_aic_init_secondary(device_t dev, uint32_t rootnum)
{
	struct apple_aic_softc *sc = device_get_softc(dev);
	u_int cpu = PCPU_GET(cpuid);

	/* We don't need to re-initialize for the FIQ root. */
	if (rootnum != INTR_ROOT_IRQ)
		return;

	sc->sc_cpuids[cpu] = bus_read_4(sc->sc_mem, AIC_WHOAMI);
	if (bootverbose)
		device_printf(dev, "CPU %d: whoami %x\n", cpu,
		    sc->sc_cpuids[cpu]);

	bus_write_4(sc->sc_mem, AIC_IPI_MASK_SET, AIC_IPI_SELF | AIC_IPI_OTHER);
}
#endif

static device_method_t apple_aic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		apple_aic_probe),
	DEVMETHOD(device_attach,	apple_aic_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	apple_aic_disable_intr),
	DEVMETHOD(pic_enable_intr,	apple_aic_enable_intr),
	DEVMETHOD(pic_map_intr,		apple_aic_map_intr),
	DEVMETHOD(pic_setup_intr,	apple_aic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	apple_aic_teardown_intr),
	DEVMETHOD(pic_post_filter,	apple_aic_post_filter),
	DEVMETHOD(pic_post_ithread,	apple_aic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	apple_aic_pre_ithread),
#ifdef SMP
	DEVMETHOD(pic_bind_intr,	apple_aic_bind_intr),
	DEVMETHOD(pic_init_secondary,	apple_aic_init_secondary),
	DEVMETHOD(pic_ipi_send,		apple_aic_ipi_send),
	DEVMETHOD(pic_ipi_setup,	apple_aic_ipi_setup),
#endif

	/* End */
	DEVMETHOD_END
};

static DEFINE_CLASS_0(aic, apple_aic_driver, apple_aic_methods,
    sizeof(struct apple_aic_softc));

EARLY_DRIVER_MODULE(aic, simplebus, apple_aic_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
