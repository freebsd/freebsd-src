/*-
 * Copyright (c) 2005 Poul-Henning Kamp
 * Copyright (c) 2010 Alexander Motin <mav@FreeBSD.org>
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

#include "opt_acpi.h"
#if defined(__amd64__) || defined(__ia64__)
#define	DEV_APIC
#else
#include "opt_apic.h"
#endif
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/timeet.h>
#include <sys/timetc.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_hpet.h>

#ifdef DEV_APIC
#include "pcib_if.h"
#endif

#define HPET_VENDID_AMD		0x4353
#define HPET_VENDID_INTEL	0x8086

ACPI_SERIAL_DECL(hpet, "ACPI HPET support");

static devclass_t hpet_devclass;

/* ACPI CA debugging */
#define _COMPONENT	ACPI_TIMER
ACPI_MODULE_NAME("HPET")

struct hpet_softc {
	device_t		dev;
	int			mem_rid;
	int			intr_rid;
	int			irq;
	int			useirq;
	int			legacy_route;
	struct resource		*mem_res;
	struct resource		*intr_res;
	void			*intr_handle;
	ACPI_HANDLE		handle;
	uint64_t		freq;
	uint32_t		caps;
	struct timecounter	tc;
	struct hpet_timer {
		struct eventtimer	et;
		struct hpet_softc	*sc;
		int			num;
		int			mode;
		int			intr_rid;
		int			irq;
		int			pcpu_master;
		int			pcpu_slaves[MAXCPU];
		struct resource		*intr_res;
		void			*intr_handle;
		uint32_t		caps;
		uint32_t		vectors;
		uint32_t		div;
		uint32_t		last;
		char			name[8];
	} 			t[32];
	int			num_timers;
};

static u_int hpet_get_timecount(struct timecounter *tc);
static void hpet_test(struct hpet_softc *sc);

static char *hpet_ids[] = { "PNP0103", NULL };

static u_int
hpet_get_timecount(struct timecounter *tc)
{
	struct hpet_softc *sc;

	sc = tc->tc_priv;
	return (bus_read_4(sc->mem_res, HPET_MAIN_COUNTER));
}

static void
hpet_enable(struct hpet_softc *sc)
{
	uint32_t val;

	val = bus_read_4(sc->mem_res, HPET_CONFIG);
	if (sc->legacy_route)
		val |= HPET_CNF_LEG_RT;
	else
		val &= ~HPET_CNF_LEG_RT;
	val |= HPET_CNF_ENABLE;
	bus_write_4(sc->mem_res, HPET_CONFIG, val);
}

static void
hpet_disable(struct hpet_softc *sc)
{
	uint32_t val;

	val = bus_read_4(sc->mem_res, HPET_CONFIG);
	val &= ~HPET_CNF_ENABLE;
	bus_write_4(sc->mem_res, HPET_CONFIG, val);
}

static int
hpet_start(struct eventtimer *et,
    struct bintime *first, struct bintime *period)
{
	struct hpet_timer *mt = (struct hpet_timer *)et->et_priv;
	struct hpet_timer *t;
	struct hpet_softc *sc = mt->sc;
	uint32_t fdiv;

	t = (mt->pcpu_master < 0) ? mt : &sc->t[mt->pcpu_slaves[curcpu]];
	if (period != NULL) {
		t->mode = 1;
		t->div = (sc->freq * (period->frac >> 32)) >> 32;
		if (period->sec != 0)
			t->div += sc->freq * period->sec;
		if (first == NULL)
			first = period;
	} else {
		t->mode = 2;
		t->div = 0;
	}
	fdiv = (sc->freq * (first->frac >> 32)) >> 32;
	if (first->sec != 0)
		fdiv += sc->freq * first->sec;
	t->last = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	if (t->mode == 1 && (t->caps & HPET_TCAP_PER_INT)) {
		t->caps |= HPET_TCNF_TYPE;
		bus_write_4(sc->mem_res, HPET_TIMER_CAP_CNF(t->num),
		    t->caps | HPET_TCNF_VAL_SET);
		bus_write_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num),
		    t->last + fdiv);
		bus_read_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num));
		bus_write_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num),
		    t->div);
	} else {
		bus_write_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num),
		    t->last + fdiv);
	}
	t->caps |= HPET_TCNF_INT_ENB;
	bus_write_4(sc->mem_res, HPET_ISR, 1 << t->num);
	bus_write_4(sc->mem_res, HPET_TIMER_CAP_CNF(t->num), t->caps);
	return (0);
}

static int
hpet_stop(struct eventtimer *et)
{
	struct hpet_timer *mt = (struct hpet_timer *)et->et_priv;
	struct hpet_timer *t;
	struct hpet_softc *sc = mt->sc;

	t = (mt->pcpu_master < 0) ? mt : &sc->t[mt->pcpu_slaves[curcpu]];
	t->mode = 0;
	t->caps &= ~(HPET_TCNF_INT_ENB | HPET_TCNF_TYPE);
	bus_write_4(sc->mem_res, HPET_TIMER_CAP_CNF(t->num), t->caps);
	return (0);
}

static int
hpet_intr_single(void *arg)
{
	struct hpet_timer *t = (struct hpet_timer *)arg;
	struct hpet_timer *mt;
	struct hpet_softc *sc = t->sc;
	uint32_t now;

	if (t->mode == 1 &&
	    (t->caps & HPET_TCAP_PER_INT) == 0) {
		t->last += t->div;
		now = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
		if ((int32_t)(now - (t->last + t->div / 2)) > 0)
			t->last = now - t->div / 2;
		bus_write_4(sc->mem_res,
		    HPET_TIMER_COMPARATOR(t->num), t->last + t->div);
	} else if (t->mode == 2)
		t->mode = 0;
	mt = (t->pcpu_master < 0) ? t : &sc->t[t->pcpu_master];
	if (mt->et.et_active) {
		mt->et.et_event_cb(&mt->et,
		    mt->et.et_arg ? mt->et.et_arg : curthread->td_intr_frame);
	}
	return (FILTER_HANDLED);
}

static int
hpet_intr(void *arg)
{
	struct hpet_softc *sc = (struct hpet_softc *)arg;
	int i;
	uint32_t val;

	val = bus_read_4(sc->mem_res, HPET_ISR);
	if (val) {
		bus_write_4(sc->mem_res, HPET_ISR, val);
		val &= sc->useirq;
		for (i = 0; i < sc->num_timers; i++) {
			if ((val & (1 << i)) == 0)
				continue;
			hpet_intr_single(&sc->t[i]);
		}
		return (FILTER_HANDLED);
	}
	return (FILTER_STRAY);
}

static ACPI_STATUS
hpet_find(ACPI_HANDLE handle, UINT32 level, void *context,
    void **status)
{
	char 		**ids;
	uint32_t	id = (uint32_t)(uintptr_t)context;
	uint32_t	uid = 0;

	for (ids = hpet_ids; *ids != NULL; ids++) {
		if (acpi_MatchHid(handle, *ids))
		        break;
	}
	if (*ids == NULL)
		return (AE_OK);
	if (ACPI_FAILURE(acpi_GetInteger(handle, "_UID", &uid)) ||
	    id == uid)
		*((int *)status) = 1;
	return (AE_OK);
}

/* Discover the HPET via the ACPI table of the same name. */
static void 
hpet_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_HPET *hpet;
	ACPI_STATUS	status;
	device_t	child;
	int 		i, found;

	/* Only one HPET device can be added. */
	if (devclass_get_device(hpet_devclass, 0))
		return;
	for (i = 1; ; i++) {
		/* Search for HPET table. */
		status = AcpiGetTable(ACPI_SIG_HPET, i, (ACPI_TABLE_HEADER **)&hpet);
		if (ACPI_FAILURE(status))
			return;
		/* Search for HPET device with same ID. */
		found = 0;
		AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
		    100, hpet_find, NULL, (void *)(uintptr_t)hpet->Sequence, (void *)&found);
		/* If found - let it be probed in normal way. */
		if (found)
			continue;
		/* If not - create it from table info. */
		child = BUS_ADD_CHILD(parent, ACPI_DEV_BASE_ORDER, "hpet", 0);
		if (child == NULL) {
			printf("%s: can't add child\n", __func__);
			continue;
		}
		bus_set_resource(child, SYS_RES_MEMORY, 0, hpet->Address.Address,
		    HPET_MEM_WIDTH);
	}
}

static int
hpet_probe(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	if (acpi_disabled("hpet"))
		return (ENXIO);
	if (acpi_get_handle(dev) != NULL &&
	    ACPI_ID_PROBE(device_get_parent(dev), dev, hpet_ids) == NULL)
		return (ENXIO);

	device_set_desc(dev, "High Precision Event Timer");
	return (0);
}

static int
hpet_attach(device_t dev)
{
	struct hpet_softc *sc;
	struct hpet_timer *t;
	int i, j, num_msi, num_timers, num_percpu_et, num_percpu_t, cur_cpu;
	int pcpu_master;
	static int maxhpetet = 0;
	uint32_t val, val2, cvectors;
	uint16_t vendor, rev;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		return (ENOMEM);

	/* Validate that we can access the whole region. */
	if (rman_get_size(sc->mem_res) < HPET_MEM_WIDTH) {
		device_printf(dev, "memory region width %ld too small\n",
		    rman_get_size(sc->mem_res));
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
		return (ENXIO);
	}

	/* Be sure timer is enabled. */
	hpet_enable(sc);

	/* Read basic statistics about the timer. */
	val = bus_read_4(sc->mem_res, HPET_PERIOD);
	if (val == 0) {
		device_printf(dev, "invalid period\n");
		hpet_disable(sc);
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
		return (ENXIO);
	}

	sc->freq = (1000000000000000LL + val / 2) / val;
	sc->caps = bus_read_4(sc->mem_res, HPET_CAPABILITIES);
	vendor = (sc->caps & HPET_CAP_VENDOR_ID) >> 16;
	rev = sc->caps & HPET_CAP_REV_ID;
	num_timers = 1 + ((sc->caps & HPET_CAP_NUM_TIM) >> 8);
	/*
	 * ATI/AMD violates IA-PC HPET (High Precision Event Timers)
	 * Specification and provides an off by one number
	 * of timers/comparators.
	 * Additionally, they use unregistered value in VENDOR_ID field.
	 */
	if (vendor == HPET_VENDID_AMD && rev < 0x10 && num_timers > 0)
		num_timers--;
	sc->num_timers = num_timers;
	if (bootverbose) {
		device_printf(dev,
		    "vendor 0x%x, rev 0x%x, %jdHz%s, %d timers,%s\n",
		    vendor, rev, sc->freq,
		    (sc->caps & HPET_CAP_COUNT_SIZE) ? " 64bit" : "",
		    num_timers,
		    (sc->caps & HPET_CAP_LEG_RT) ? " legacy route" : "");
	}
	for (i = 0; i < num_timers; i++) {
		t = &sc->t[i];
		t->sc = sc;
		t->num = i;
		t->mode = 0;
		t->intr_rid = -1;
		t->irq = -1;
		t->pcpu_master = -1;
		t->caps = bus_read_4(sc->mem_res, HPET_TIMER_CAP_CNF(i));
		t->vectors = bus_read_4(sc->mem_res, HPET_TIMER_CAP_CNF(i) + 4);
		if (bootverbose) {
			device_printf(dev,
			    " t%d: irqs 0x%08x (%d)%s%s%s\n", i,
			    t->vectors, (t->caps & HPET_TCNF_INT_ROUTE) >> 9,
			    (t->caps & HPET_TCAP_FSB_INT_DEL) ? ", MSI" : "",
			    (t->caps & HPET_TCAP_SIZE) ? ", 64bit" : "",
			    (t->caps & HPET_TCAP_PER_INT) ? ", periodic" : "");
		}
	}
	if (testenv("debug.acpi.hpet_test"))
		hpet_test(sc);
	/*
	 * Don't attach if the timer never increments.  Since the spec
	 * requires it to be at least 10 MHz, it has to change in 1 us.
	 */
	val = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	DELAY(1);
	val2 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	if (val == val2) {
		device_printf(dev, "HPET never increments, disabling\n");
		hpet_disable(sc);
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
		return (ENXIO);
	}
	/* Announce first HPET as timecounter. */
	if (device_get_unit(dev) == 0) {
		sc->tc.tc_get_timecount = hpet_get_timecount,
		sc->tc.tc_counter_mask = ~0u,
		sc->tc.tc_name = "HPET",
		sc->tc.tc_quality = 900,
		sc->tc.tc_frequency = sc->freq;
		sc->tc.tc_priv = sc;
		tc_init(&sc->tc);
	}
	/* If not disabled - setup and announce event timers. */
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	     "clock", &i) == 0 && i == 0)
	        return (0);

	/* Check whether we can and want legacy routing. */
	sc->legacy_route = 0;
	resource_int_value(device_get_name(dev), device_get_unit(dev),
	     "legacy_route", &sc->legacy_route);
	if ((sc->caps & HPET_CAP_LEG_RT) == 0)
		sc->legacy_route = 0;
	if (sc->legacy_route) {
		sc->t[0].vectors = 0;
		sc->t[1].vectors = 0;
	}

	num_msi = 0;
	sc->useirq = 0;
	/* Find common legacy IRQ vectors for all timers. */
	cvectors = 0xffff0000;
	/*
	 * HPETs in AMD chipsets before SB800 have problems with IRQs >= 16
	 * Lower are also not always working for different reasons.
	 * SB800 fixed it, but seems do not implements level triggering
	 * properly, that makes it very unreliable - it freezes after any
	 * interrupt loss. Avoid legacy IRQs for AMD.
	 */
	if (vendor == HPET_VENDID_AMD)
		cvectors = 0x00000000;
	for (i = 0; i < num_timers; i++) {
		t = &sc->t[i];
		if (sc->legacy_route && i < 2)
			t->irq = (i == 0) ? 0 : 8;
#ifdef DEV_APIC
		else if (t->caps & HPET_TCAP_FSB_INT_DEL) {
			if ((j = PCIB_ALLOC_MSIX(
			    device_get_parent(device_get_parent(dev)), dev,
			    &t->irq))) {
				device_printf(dev,
				    "Can't allocate interrupt for t%d.\n", j);
			}
		}
#endif
		if (t->irq >= 0) {
			if (!(t->intr_res =
			    bus_alloc_resource(dev, SYS_RES_IRQ, &t->intr_rid,
			    t->irq, t->irq, 1, RF_ACTIVE))) {
				t->irq = -1;
				device_printf(dev,
				    "Can't map interrupt for t%d.\n", i);
			} else if ((bus_setup_intr(dev, t->intr_res,
			    INTR_MPSAFE | INTR_TYPE_CLK,
			    (driver_filter_t *)hpet_intr_single, NULL,
			    t, &t->intr_handle))) {
				t->irq = -1;
				device_printf(dev,
				    "Can't setup interrupt for t%d.\n", i);
			} else {
				bus_describe_intr(dev, t->intr_res,
				    t->intr_handle, "t%d", i);
				num_msi++;
			}
		}
		if (t->irq < 0 && (cvectors & t->vectors) != 0) {
			cvectors &= t->vectors;
			sc->useirq |= (1 << i);
		}
	}
	if (sc->legacy_route && sc->t[0].irq < 0 && sc->t[1].irq < 0)
		sc->legacy_route = 0;
	if (sc->legacy_route)
		hpet_enable(sc);
	/* Group timers for per-CPU operation. */
	num_percpu_et = min(num_msi / mp_ncpus, 2);
	num_percpu_t = num_percpu_et * mp_ncpus;
	pcpu_master = 0;
	cur_cpu = CPU_FIRST();
	for (i = 0; i < num_timers; i++) {
		t = &sc->t[i];
		if (t->irq >= 0 && num_percpu_t > 0) {
			if (cur_cpu == CPU_FIRST())
				pcpu_master = i;
			t->pcpu_master = pcpu_master;
			sc->t[pcpu_master].
			    pcpu_slaves[cur_cpu] = i;
			bus_bind_intr(dev, t->intr_res, cur_cpu);
			cur_cpu = CPU_NEXT(cur_cpu);
			num_percpu_t--;
		}
	}
	bus_write_4(sc->mem_res, HPET_ISR, 0xffffffff);
	sc->irq = -1;
	sc->intr_rid = -1;
	/* If at least one timer needs legacy IRQ - setup it. */
	if (sc->useirq) {
		j = i = fls(cvectors) - 1;
		while (j > 0 && (cvectors & (1 << (j - 1))) != 0)
			j--;
		if (!(sc->intr_res = bus_alloc_resource(dev, SYS_RES_IRQ,
		    &sc->intr_rid, j, i, 1, RF_SHAREABLE | RF_ACTIVE)))
			device_printf(dev,"Can't map interrupt.\n");
		else if ((bus_setup_intr(dev, sc->intr_res,
		    INTR_MPSAFE | INTR_TYPE_CLK,
		    (driver_filter_t *)hpet_intr, NULL,
		    sc, &sc->intr_handle))) {
			device_printf(dev, "Can't setup interrupt.\n");
		} else {
			sc->irq = rman_get_start(sc->intr_res);
			/* Bind IRQ to BSP to avoid live migration. */
			bus_bind_intr(dev, sc->intr_res, CPU_FIRST());
		}
	}
	/* Program and announce event timers. */
	for (i = 0; i < num_timers; i++) {
		t = &sc->t[i];
		t->caps &= ~(HPET_TCNF_FSB_EN | HPET_TCNF_INT_ROUTE);
		t->caps &= ~(HPET_TCNF_VAL_SET | HPET_TCNF_INT_ENB);
		t->caps &= ~(HPET_TCNF_INT_TYPE);
		t->caps |= HPET_TCNF_32MODE;
		if (t->irq >= 0 && sc->legacy_route && i < 2) {
			/* Legacy route doesn't need more configuration. */
		} else
#ifdef DEV_APIC
		if (t->irq >= 0) {
			uint64_t addr;
			uint32_t data;	
			
			if (PCIB_MAP_MSI(
			    device_get_parent(device_get_parent(dev)), dev,
			    t->irq, &addr, &data) == 0) {
				bus_write_4(sc->mem_res,
				    HPET_TIMER_FSB_ADDR(i), addr);
				bus_write_4(sc->mem_res,
				    HPET_TIMER_FSB_VAL(i), data);
				t->caps |= HPET_TCNF_FSB_EN;
			} else
				t->irq = -2;
		} else
#endif
		if (sc->irq >= 0 && (t->vectors & (1 << sc->irq)))
			t->caps |= (sc->irq << 9) | HPET_TCNF_INT_TYPE;
		bus_write_4(sc->mem_res, HPET_TIMER_CAP_CNF(i), t->caps);
		/* Skip event timers without set up IRQ. */
		if (t->irq < 0 &&
		    (sc->irq < 0 || (t->vectors & (1 << sc->irq)) == 0))
			continue;
		/* Announce the reset. */
		if (maxhpetet == 0)
			t->et.et_name = "HPET";
		else {
			sprintf(t->name, "HPET%d", maxhpetet);
			t->et.et_name = t->name;
		}
		t->et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
		t->et.et_quality = 450;
		if (t->pcpu_master >= 0) {
			t->et.et_flags |= ET_FLAGS_PERCPU;
			t->et.et_quality += 100;
		}
		if ((t->caps & HPET_TCAP_PER_INT) == 0)
			t->et.et_quality -= 10;
		t->et.et_frequency = sc->freq;
		t->et.et_start = hpet_start;
		t->et.et_stop = hpet_stop;
		t->et.et_priv = &sc->t[i];
		if (t->pcpu_master < 0 || t->pcpu_master == i) {
			et_register(&t->et);
			maxhpetet++;
		}
	}
	return (0);
}

static int
hpet_detach(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	/* XXX Without a tc_remove() function, we can't detach. */
	return (EBUSY);
}

static int
hpet_suspend(device_t dev)
{
	struct hpet_softc *sc;

	/*
	 * Disable the timer during suspend.  The timer will not lose
	 * its state in S1 or S2, but we are required to disable
	 * it.
	 */
	sc = device_get_softc(dev);
	hpet_disable(sc);

	return (0);
}

static int
hpet_resume(device_t dev)
{
	struct hpet_softc *sc;
	struct hpet_timer *t;
	int i;

	/* Re-enable the timer after a resume to keep the clock advancing. */
	sc = device_get_softc(dev);
	hpet_enable(sc);
	/* Restart event timers that were running on suspend. */
	for (i = 0; i < sc->num_timers; i++) {
		t = &sc->t[i];
#ifdef DEV_APIC
		if (t->irq >= 0 && (sc->legacy_route == 0 || i >= 2)) {
			uint64_t addr;
			uint32_t data;	
			
			if (PCIB_MAP_MSI(
			    device_get_parent(device_get_parent(dev)), dev,
			    t->irq, &addr, &data) == 0) {
				bus_write_4(sc->mem_res,
				    HPET_TIMER_FSB_ADDR(i), addr);
				bus_write_4(sc->mem_res,
				    HPET_TIMER_FSB_VAL(i), data);
			}
		}
#endif
		if (t->mode == 0)
			continue;
		t->last = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
		if (t->mode == 1 && (t->caps & HPET_TCAP_PER_INT)) {
			t->caps |= HPET_TCNF_TYPE;
			bus_write_4(sc->mem_res, HPET_TIMER_CAP_CNF(t->num),
			    t->caps | HPET_TCNF_VAL_SET);
			bus_write_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num),
			    t->last + t->div);
			bus_read_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num));
			bus_write_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num),
			    t->div);
		} else {
			bus_write_4(sc->mem_res, HPET_TIMER_COMPARATOR(t->num),
			    t->last + sc->freq / 1024);
		}
		bus_write_4(sc->mem_res, HPET_ISR, 1 << t->num);
		bus_write_4(sc->mem_res, HPET_TIMER_CAP_CNF(t->num), t->caps);
	}
	return (0);
}

/* Print some basic latency/rate information to assist in debugging. */
static void
hpet_test(struct hpet_softc *sc)
{
	int i;
	uint32_t u1, u2;
	struct bintime b0, b1, b2;
	struct timespec ts;

	binuptime(&b0);
	binuptime(&b0);
	binuptime(&b1);
	u1 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	for (i = 1; i < 1000; i++)
		u2 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	binuptime(&b2);
	u2 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);

	bintime_sub(&b2, &b1);
	bintime_sub(&b1, &b0);
	bintime_sub(&b2, &b1);
	bintime2timespec(&b2, &ts);

	device_printf(sc->dev, "%ld.%09ld: %u ... %u = %u\n",
	    (long)ts.tv_sec, ts.tv_nsec, u1, u2, u2 - u1);

	device_printf(sc->dev, "time per call: %ld ns\n", ts.tv_nsec / 1000);
}

#ifdef DEV_APIC
static int
hpet_remap_intr(device_t dev, device_t child, u_int irq)
{
	struct hpet_softc *sc = device_get_softc(dev);
	struct hpet_timer *t;
	uint64_t addr;
	uint32_t data;	
	int error, i;

	for (i = 0; i < sc->num_timers; i++) {
		t = &sc->t[i];
		if (t->irq != irq)
			continue;
		error = PCIB_MAP_MSI(
		    device_get_parent(device_get_parent(dev)), dev,
		    irq, &addr, &data);
		if (error)
			return (error);
		hpet_disable(sc); /* Stop timer to avoid interrupt loss. */
		bus_write_4(sc->mem_res, HPET_TIMER_FSB_ADDR(i), addr);
		bus_write_4(sc->mem_res, HPET_TIMER_FSB_VAL(i), data);
		hpet_enable(sc);
		return (0);
	}
	return (ENOENT);
}
#endif

static device_method_t hpet_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, hpet_identify),
	DEVMETHOD(device_probe, hpet_probe),
	DEVMETHOD(device_attach, hpet_attach),
	DEVMETHOD(device_detach, hpet_detach),
	DEVMETHOD(device_suspend, hpet_suspend),
	DEVMETHOD(device_resume, hpet_resume),

#ifdef DEV_APIC
	DEVMETHOD(bus_remap_intr, hpet_remap_intr),
#endif

	{0, 0}
};

static driver_t	hpet_driver = {
	"hpet",
	hpet_methods,
	sizeof(struct hpet_softc),
};

DRIVER_MODULE(hpet, acpi, hpet_driver, hpet_devclass, 0, 0);
MODULE_DEPEND(hpet, acpi, 1, 1, 1);
