/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * Copyright (c) 2013 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Based on mpcore_timer.c developed by Ben Gray <ben.r.gray@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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

/**
 *      Cortex-A7, Cortex-A15, ARMv8 and later Generic Timer
 */

#include "opt_acpi.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/smp.h>
#include <sys/vdso.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/md_var.h>

#if defined(__aarch64__)
#include <machine/undefined.h>
#endif

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif

#define	GT_PHYS_SECURE		0
#define	GT_PHYS_NONSECURE	1
#define	GT_VIRT			2
#define	GT_HYP_PHYS		3
#define	GT_HYP_VIRT		4
#define	GT_IRQ_COUNT		5

#define	GT_CTRL_ENABLE		(1 << 0)
#define	GT_CTRL_INT_MASK	(1 << 1)
#define	GT_CTRL_INT_STAT	(1 << 2)
#define	GT_REG_CTRL		0
#define	GT_REG_TVAL		1

#define	GT_CNTKCTL_PL0PTEN	(1 << 9) /* PL0 Physical timer reg access */
#define	GT_CNTKCTL_PL0VTEN	(1 << 8) /* PL0 Virtual timer reg access */
#define	GT_CNTKCTL_EVNTI	(0xf << 4) /* Virtual counter event bits */
#define	GT_CNTKCTL_EVNTDIR	(1 << 3) /* Virtual counter event transition */
#define	GT_CNTKCTL_EVNTEN	(1 << 2) /* Enables virtual counter events */
#define	GT_CNTKCTL_PL0VCTEN	(1 << 1) /* PL0 CNTVCT and CNTFRQ access */
#define	GT_CNTKCTL_PL0PCTEN	(1 << 0) /* PL0 CNTPCT and CNTFRQ access */

struct arm_tmr_softc;

struct arm_tmr_irq {
	struct resource	*res;
	void		*ihl;
	int		 rid;
	int		 idx;
};

struct arm_tmr_softc {
	struct arm_tmr_irq	irqs[GT_IRQ_COUNT];
	uint64_t		(*get_cntxct)(bool);
	uint32_t		clkfreq;
	int			irq_count;
	struct eventtimer	et;
	bool			physical;
};

static struct arm_tmr_softc *arm_tmr_sc = NULL;

static const struct arm_tmr_irq_defs {
	int idx;
	const char *name;
	int flags;
} arm_tmr_irq_defs[] = {
	{
		.idx = GT_PHYS_SECURE,
		.name = "sec-phys",
		.flags = RF_ACTIVE | RF_OPTIONAL,
	},
	{
		.idx = GT_PHYS_NONSECURE,
		.name = "phys",
		.flags = RF_ACTIVE,
	},
	{
		.idx = GT_VIRT,
		.name = "virt",
		.flags = RF_ACTIVE,
	},
	{
		.idx = GT_HYP_PHYS,
		.name = "hyp-phys",
		.flags = RF_ACTIVE | RF_OPTIONAL,
	},
	{
		.idx = GT_HYP_VIRT,
		.name = "hyp-virt",
		.flags = RF_ACTIVE | RF_OPTIONAL,
	},
};

static int arm_tmr_attach(device_t);

static uint32_t arm_tmr_fill_vdso_timehands(struct vdso_timehands *vdso_th,
    struct timecounter *tc);
static void arm_tmr_do_delay(int usec, void *);

static timecounter_get_t arm_tmr_get_timecount;

static struct timecounter arm_tmr_timecount = {
	.tc_name           = "ARM MPCore Timecounter",
	.tc_get_timecount  = arm_tmr_get_timecount,
	.tc_poll_pps       = NULL,
	.tc_counter_mask   = ~0u,
	.tc_frequency      = 0,
	.tc_quality        = 1000,
	.tc_fill_vdso_timehands = arm_tmr_fill_vdso_timehands,
};

#ifdef __arm__
#define	get_el0(x)	cp15_## x ##_get()
#define	get_el1(x)	cp15_## x ##_get()
#define	set_el0(x, val)	cp15_## x ##_set(val)
#define	set_el1(x, val)	cp15_## x ##_set(val)
#define	HAS_PHYS	true
#else /* __aarch64__ */
#define	get_el0(x)	READ_SPECIALREG(x ##_el0)
#define	get_el1(x)	READ_SPECIALREG(x ##_el1)
#define	set_el0(x, val)	WRITE_SPECIALREG(x ##_el0, val)
#define	set_el1(x, val)	WRITE_SPECIALREG(x ##_el1, val)
#define	HAS_PHYS	has_hyp()
#endif

static int
get_freq(void)
{
	return (get_el0(cntfrq));
}

static uint64_t
get_cntxct_a64_unstable(bool physical)
{
	uint64_t val
;
	isb();
	if (physical) {
		do {
			val = get_el0(cntpct);
		}
		while (((val + 1) & 0x7FF) <= 1);
	}
	else {
		do {
			val = get_el0(cntvct);
		}
		while (((val + 1) & 0x7FF) <= 1);
	}

	return (val);
}

static uint64_t
get_cntxct(bool physical)
{
	uint64_t val;

	isb();
	if (physical)
		val = get_el0(cntpct);
	else
		val = get_el0(cntvct);

	return (val);
}

static int
set_ctrl(uint32_t val, bool physical)
{

	if (physical)
		set_el0(cntp_ctl, val);
	else
		set_el0(cntv_ctl, val);
	isb();

	return (0);
}

static int
set_tval(uint32_t val, bool physical)
{

	if (physical)
		set_el0(cntp_tval, val);
	else
		set_el0(cntv_tval, val);
	isb();

	return (0);
}

static int
get_ctrl(bool physical)
{
	uint32_t val;

	if (physical)
		val = get_el0(cntp_ctl);
	else
		val = get_el0(cntv_ctl);

	return (val);
}

static void
setup_user_access(void *arg __unused)
{
	uint32_t cntkctl;

	cntkctl = get_el1(cntkctl);
	cntkctl &= ~(GT_CNTKCTL_PL0PTEN | GT_CNTKCTL_PL0VTEN |
	    GT_CNTKCTL_EVNTEN | GT_CNTKCTL_PL0PCTEN);
	/* Always enable the virtual timer */
	cntkctl |= GT_CNTKCTL_PL0VCTEN;
	/* Enable the physical timer if supported */
	if (arm_tmr_sc->physical) {
		cntkctl |= GT_CNTKCTL_PL0PCTEN;
	}
	set_el1(cntkctl, cntkctl);
	isb();
}

#ifdef __aarch64__
static int
cntpct_handler(vm_offset_t va, uint32_t insn, struct trapframe *frame,
    uint32_t esr)
{
	uint64_t val;
	int reg;

	if ((insn & MRS_MASK) != MRS_VALUE)
		return (0);

	if (MRS_SPECIAL(insn) != MRS_SPECIAL(CNTPCT_EL0))
		return (0);

	reg = MRS_REGISTER(insn);
	val = READ_SPECIALREG(cntvct_el0);
	if (reg < nitems(frame->tf_x)) {
		frame->tf_x[reg] = val;
	} else if (reg == 30) {
		frame->tf_lr = val;
	}

	/*
	 * We will handle this instruction, move to the next so we
	 * don't trap here again.
	 */
	frame->tf_elr += INSN_SIZE;

	return (1);
}
#endif

static void
tmr_setup_user_access(void *arg __unused)
{
#ifdef __aarch64__
	int emulate;
#endif

	if (arm_tmr_sc != NULL) {
		smp_rendezvous(NULL, setup_user_access, NULL, NULL);
#ifdef __aarch64__
		if (TUNABLE_INT_FETCH("hw.emulate_phys_counter", &emulate) &&
		    emulate != 0) {
			install_undef_handler(true, cntpct_handler);
		}
#endif
	}
}
SYSINIT(tmr_ua, SI_SUB_SMP, SI_ORDER_ANY, tmr_setup_user_access, NULL);

static unsigned
arm_tmr_get_timecount(struct timecounter *tc)
{

	return (arm_tmr_sc->get_cntxct(arm_tmr_sc->physical));
}

static int
arm_tmr_start(struct eventtimer *et, sbintime_t first,
    sbintime_t period __unused)
{
	struct arm_tmr_softc *sc;
	int counts, ctrl;

	sc = (struct arm_tmr_softc *)et->et_priv;

	if (first != 0) {
		counts = ((uint32_t)et->et_frequency * first) >> 32;
		ctrl = get_ctrl(sc->physical);
		ctrl &= ~GT_CTRL_INT_MASK;
		ctrl |= GT_CTRL_ENABLE;
		set_tval(counts, sc->physical);
		set_ctrl(ctrl, sc->physical);
		return (0);
	}

	return (EINVAL);

}

static void
arm_tmr_disable(bool physical)
{
	int ctrl;

	ctrl = get_ctrl(physical);
	ctrl &= ~GT_CTRL_ENABLE;
	set_ctrl(ctrl, physical);
}

static int
arm_tmr_stop(struct eventtimer *et)
{
	struct arm_tmr_softc *sc;

	sc = (struct arm_tmr_softc *)et->et_priv;
	arm_tmr_disable(sc->physical);

	return (0);
}

static int
arm_tmr_intr(void *arg)
{
	struct arm_tmr_softc *sc;
	int ctrl;

	sc = (struct arm_tmr_softc *)arg;
	ctrl = get_ctrl(sc->physical);
	if (ctrl & GT_CTRL_INT_STAT) {
		ctrl |= GT_CTRL_INT_MASK;
		set_ctrl(ctrl, sc->physical);
	}

	if (sc->et.et_active)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

	return (FILTER_HANDLED);
}

static int
arm_tmr_attach_irq(device_t dev, struct arm_tmr_softc *sc,
    const struct arm_tmr_irq_defs *irq_def, int rid, int flags)
{
	struct arm_tmr_irq *irq;

	irq = &sc->irqs[sc->irq_count];
	irq->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &rid, flags);
	if (irq->res == NULL) {
		if (bootverbose || (flags & RF_OPTIONAL) == 0) {
			device_printf(dev,
			    "could not allocate irq for %s interrupt '%s'\n",
			    (flags & RF_OPTIONAL) != 0 ? "optional" :
			    "required", irq_def->name);
		}

		if ((flags & RF_OPTIONAL) == 0)
			return (ENXIO);
	} else {
		if (bootverbose)
			device_printf(dev, "allocated irq for '%s'\n",
			    irq_def->name);
		irq->rid = rid;
		irq->idx = irq_def->idx;
		sc->irq_count++;
	}

	return (0);
}

#ifdef FDT
static int
arm_tmr_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "arm,armv8-timer")) {
		device_set_desc(dev, "ARMv8 Generic Timer");
		return (BUS_PROBE_DEFAULT);
	} else if (ofw_bus_is_compatible(dev, "arm,armv7-timer")) {
		device_set_desc(dev, "ARMv7 Generic Timer");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
arm_tmr_fdt_attach(device_t dev)
{
	struct arm_tmr_softc *sc;
	const struct arm_tmr_irq_defs *irq_def;
	size_t i;
	phandle_t node;
	int error, rid;
	bool has_names;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	has_names = OF_hasprop(node, "interrupt-names");
	for (i = 0; i < nitems(arm_tmr_irq_defs); i++) {
		int flags;

		/*
		 * If we don't have names to go off of, we assume that they're
		 * in the "usual" order with sec-phys first and allocate by idx.
		 */
		irq_def = &arm_tmr_irq_defs[i];
		rid = irq_def->idx;
		flags = irq_def->flags;
		if (has_names) {
			error = ofw_bus_find_string_index(node,
			    "interrupt-names", irq_def->name, &rid);

			/*
			 * If we have names, missing a name means we don't
			 * have it.
			 */
			if (error != 0) {
				/*
				 * Could be noisy on a lot of platforms for no
				 * good cause.
				 */
				if (bootverbose || (flags & RF_OPTIONAL) == 0) {
					device_printf(dev,
					    "could not find irq for %s interrupt '%s'\n",
					    (flags & RF_OPTIONAL) != 0 ?
					    "optional" : "required",
					    irq_def->name);
				}

				if ((flags & RF_OPTIONAL) == 0)
					goto out;

				continue;
			}

			/*
			 * Warn about failing to activate if we did actually
			 * have the name present.
			 */
			flags &= ~RF_OPTIONAL;
		}

		error = arm_tmr_attach_irq(dev, sc, irq_def, rid, flags);
		if (error != 0)
			goto out;
	}

	error = arm_tmr_attach(dev);
out:
	if (error != 0) {
		for (i = 0; i < sc->irq_count; i++) {
			bus_release_resource(dev, SYS_RES_IRQ, sc->irqs[i].rid,
			    sc->irqs[i].res);
		}
	}

	return (error);

}
#endif

#ifdef DEV_ACPI
static void
arm_tmr_acpi_add_irq(device_t parent, device_t dev, int rid, u_int irq)
{

	BUS_SET_RESOURCE(parent, dev, SYS_RES_IRQ, rid, irq, 1);
}

static void
arm_tmr_acpi_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_GTDT *gtdt;
	vm_paddr_t physaddr;
	device_t dev;

	physaddr = acpi_find_table(ACPI_SIG_GTDT);
	if (physaddr == 0)
		return;

	gtdt = acpi_map_table(physaddr, ACPI_SIG_GTDT);
	if (gtdt == NULL) {
		device_printf(parent, "gic: Unable to map the GTDT\n");
		return;
	}

	dev = BUS_ADD_CHILD(parent, BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE,
	    "generic_timer", -1);
	if (dev == NULL) {
		device_printf(parent, "add gic child failed\n");
		goto out;
	}

	arm_tmr_acpi_add_irq(parent, dev, GT_PHYS_SECURE,
	    gtdt->SecureEl1Interrupt);
	arm_tmr_acpi_add_irq(parent, dev, GT_PHYS_NONSECURE,
	    gtdt->NonSecureEl1Interrupt);
	arm_tmr_acpi_add_irq(parent, dev, GT_VIRT,
	    gtdt->VirtualTimerInterrupt);

out:
	acpi_unmap_table(gtdt);
}

static int
arm_tmr_acpi_probe(device_t dev)
{

	device_set_desc(dev, "ARM Generic Timer");
	return (BUS_PROBE_NOWILDCARD);
}

static int
arm_tmr_acpi_attach(device_t dev)
{
	const struct arm_tmr_irq_defs *irq_def;
	struct arm_tmr_softc *sc;
	int error;

	sc = device_get_softc(dev);
	for (int i = 0; i < nitems(arm_tmr_irq_defs); i++) {
		irq_def = &arm_tmr_irq_defs[i];
		error = arm_tmr_attach_irq(dev, sc, irq_def, irq_def->idx,
		    irq_def->flags);
		if (error != 0)
			goto out;
	}

	error = arm_tmr_attach(dev);
out:
	if (error != 0) {
		for (int i = 0; i < sc->irq_count; i++) {
			bus_release_resource(dev, SYS_RES_IRQ,
			    sc->irqs[i].rid, sc->irqs[i].res);
		}
	}
	return (error);
}
#endif

static int
arm_tmr_attach(device_t dev)
{
	struct arm_tmr_softc *sc;
#ifdef INVARIANTS
	const struct arm_tmr_irq_defs *irq_def;
#endif
#ifdef FDT
	phandle_t node;
	pcell_t clock;
#endif
	int error;
	int i, first_timer, last_timer;

	sc = device_get_softc(dev);
	if (arm_tmr_sc)
		return (ENXIO);

	sc->get_cntxct = &get_cntxct;
#ifdef FDT
	/* Get the base clock frequency */
	node = ofw_bus_get_node(dev);
	if (node > 0) {
		error = OF_getencprop(node, "clock-frequency", &clock,
		    sizeof(clock));
		if (error > 0)
			sc->clkfreq = clock;

		if (OF_hasprop(node, "allwinner,sun50i-a64-unstable-timer")) {
			sc->get_cntxct = &get_cntxct_a64_unstable;
			if (bootverbose)
				device_printf(dev,
				    "Enabling allwinner unstable timer workaround\n");
		}
	}
#endif

	if (sc->clkfreq == 0) {
		/* Try to get clock frequency from timer */
		sc->clkfreq = get_freq();
	}

	if (sc->clkfreq == 0) {
		device_printf(dev, "No clock frequency specified\n");
		return (ENXIO);
	}

#ifdef INVARIANTS
	/* Confirm that non-optional irqs were allocated before coming in. */
	for (i = 0; i < nitems(arm_tmr_irq_defs); i++) {
		int j;

		irq_def = &arm_tmr_irq_defs[i];

		/* Skip optional interrupts */
		if ((irq_def->flags & RF_OPTIONAL) != 0)
			continue;

		for (j = 0; j < sc->irq_count; j++) {
			if (sc->irqs[j].idx == irq_def->idx)
				break;
		}
		KASSERT(j < sc->irq_count, ("%s: Missing required interrupt %s",
		    __func__, irq_def->name));
	}
#endif

#ifdef __aarch64__
	/*
	 * Use the virtual timer when we can't use the hypervisor.
	 * A hypervisor guest may change the virtual timer registers while
	 * executing so any use of the virtual timer interrupt needs to be
	 * coordinated with the virtual machine manager.
	 */
	if (!HAS_PHYS) {
		sc->physical = false;
		first_timer = GT_VIRT;
		last_timer = GT_VIRT;
	} else
#endif
	/* Otherwise set up the secure and non-secure physical timers. */
	{
		sc->physical = true;
		first_timer = GT_PHYS_SECURE;
		last_timer = GT_PHYS_NONSECURE;
	}

	arm_tmr_sc = sc;

	/* Setup secure, non-secure and virtual IRQs handler */
	for (i = 0; i < sc->irq_count; i++) {
		/* Only enable IRQs on timers we expect to use */
		if (sc->irqs[i].idx < first_timer ||
		    sc->irqs[i].idx > last_timer)
			continue;
		error = bus_setup_intr(dev, sc->irqs[i].res, INTR_TYPE_CLK,
		    arm_tmr_intr, NULL, sc, &sc->irqs[i].ihl);
		if (error) {
			device_printf(dev, "Unable to alloc int resource.\n");
			for (int j = 0; j < i; j++)
				bus_teardown_intr(dev, sc->irqs[j].res,
				    &sc->irqs[j].ihl);
			return (ENXIO);
		}
	}

	/* Disable the timers until we are ready */
	arm_tmr_disable(false);
	if (HAS_PHYS)
		arm_tmr_disable(true);

	arm_tmr_timecount.tc_frequency = sc->clkfreq;
	tc_init(&arm_tmr_timecount);

	sc->et.et_name = "ARM MPCore Eventtimer";
	sc->et.et_flags = ET_FLAGS_ONESHOT | ET_FLAGS_PERCPU;
	sc->et.et_quality = 1000;

	sc->et.et_frequency = sc->clkfreq;
	sc->et.et_min_period = (0x00000010LLU << 32) / sc->et.et_frequency;
	sc->et.et_max_period = (0xfffffffeLLU << 32) / sc->et.et_frequency;
	sc->et.et_start = arm_tmr_start;
	sc->et.et_stop = arm_tmr_stop;
	sc->et.et_priv = sc;
	et_register(&sc->et);

#if defined(__arm__)
	arm_set_delay(arm_tmr_do_delay, sc);
#endif

	return (0);
}

#ifdef FDT
static device_method_t arm_tmr_fdt_methods[] = {
	DEVMETHOD(device_probe,		arm_tmr_fdt_probe),
	DEVMETHOD(device_attach,	arm_tmr_fdt_attach),
	{ 0, 0 }
};

static DEFINE_CLASS_0(generic_timer, arm_tmr_fdt_driver, arm_tmr_fdt_methods,
    sizeof(struct arm_tmr_softc));

EARLY_DRIVER_MODULE(timer, simplebus, arm_tmr_fdt_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(timer, ofwbus, arm_tmr_fdt_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);
#endif

#ifdef DEV_ACPI
static device_method_t arm_tmr_acpi_methods[] = {
	DEVMETHOD(device_identify,	arm_tmr_acpi_identify),
	DEVMETHOD(device_probe,		arm_tmr_acpi_probe),
	DEVMETHOD(device_attach,	arm_tmr_acpi_attach),
	{ 0, 0 }
};

static DEFINE_CLASS_0(generic_timer, arm_tmr_acpi_driver, arm_tmr_acpi_methods,
    sizeof(struct arm_tmr_softc));

EARLY_DRIVER_MODULE(timer, acpi, arm_tmr_acpi_driver, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);
#endif

static void
arm_tmr_do_delay(int usec, void *arg)
{
	struct arm_tmr_softc *sc = arg;
	int32_t counts, counts_per_usec;
	uint32_t first, last;

	/* Get the number of times to count */
	counts_per_usec = ((arm_tmr_timecount.tc_frequency / 1000000) + 1);

	/*
	 * Clamp the timeout at a maximum value (about 32 seconds with
	 * a 66MHz clock). *Nobody* should be delay()ing for anywhere
	 * near that length of time and if they are, they should be hung
	 * out to dry.
	 */
	if (usec >= (0x80000000U / counts_per_usec))
		counts = (0x80000000U / counts_per_usec) - 1;
	else
		counts = usec * counts_per_usec;

	first = sc->get_cntxct(sc->physical);

	while (counts > 0) {
		last = sc->get_cntxct(sc->physical);
		counts -= (int32_t)(last - first);
		first = last;
	}
}

#if defined(__aarch64__)
void
DELAY(int usec)
{
	int32_t counts;

	TSENTER();
	/*
	 * Check the timers are setup, if not just
	 * use a for loop for the meantime
	 */
	if (arm_tmr_sc == NULL) {
		for (; usec > 0; usec--)
			for (counts = 200; counts > 0; counts--)
				/*
				 * Prevent the compiler from optimizing
				 * out the loop
				 */
				cpufunc_nullop();
	} else
		arm_tmr_do_delay(usec, arm_tmr_sc);
	TSEXIT();
}
#endif

static uint32_t
arm_tmr_fill_vdso_timehands(struct vdso_timehands *vdso_th,
    struct timecounter *tc)
{

	vdso_th->th_algo = VDSO_TH_ALGO_ARM_GENTIM;
	vdso_th->th_physical = arm_tmr_sc->physical;
	bzero(vdso_th->th_res, sizeof(vdso_th->th_res));
	return (1);
}
