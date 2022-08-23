/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Ampere Computing LLC
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_hwpmc_hooks.h"
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <dev/hwpmc/pmu_dmc620_reg.h>

static char *pmu_dmc620_ids[] = {
	"ARMHD620",
	NULL
};

static struct resource_spec pmu_dmc620_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

struct pmu_dmc620_softc {
	device_t	sc_dev;
	int		sc_unit;
	int		sc_domain;
	struct resource *sc_res[2];
	void		*sc_ih;
	uint32_t	sc_clkdiv2_conters_hi[DMC620_CLKDIV2_COUNTERS_N];
	uint32_t	sc_clk_conters_hi[DMC620_CLK_COUNTERS_N];
	uint32_t	sc_saved_control[DMC620_COUNTERS_N];
};

#define	RD4(sc, r)		bus_read_4((sc)->sc_res[0], (r))
#define	WR4(sc, r, v)		bus_write_4((sc)->sc_res[0], (r), (v))
#define	MD4(sc, r, c, s)	WR4((sc), (r), RD4((sc), (r)) & ~(c) | (s))

#define	CD2MD4(sc, u, r, c, s)	MD4((sc), DMC620_CLKDIV2_REG((u), (r)), (c), (s))
#define	CMD4(sc, u, r, c, s)	MD4((sc), DMC620_CLK_REG((u), (r)), (c), (s))

static int pmu_dmc620_counter_overflow_intr(void *arg);

uint32_t
pmu_dmc620_rd4(void *arg, u_int cntr, off_t reg)
{
	struct pmu_dmc620_softc *sc;
	uint32_t val;

	sc = (struct pmu_dmc620_softc *)arg;
	KASSERT(cntr < DMC620_COUNTERS_N, ("Wrong counter unit %d", cntr));

	val = RD4(sc, DMC620_REG(cntr, reg));
	return (val);
}

void
pmu_dmc620_wr4(void *arg, u_int cntr, off_t reg, uint32_t val)
{
	struct pmu_dmc620_softc *sc;

	sc = (struct pmu_dmc620_softc *)arg;
	KASSERT(cntr < DMC620_COUNTERS_N, ("Wrong counter unit %d", cntr));

	WR4(sc, DMC620_REG(cntr, reg), val);
}

static int
pmu_dmc620_acpi_probe(device_t dev)
{
	int err;

	err = ACPI_ID_PROBE(device_get_parent(dev), dev, pmu_dmc620_ids, NULL);
	if (err <= 0)
		device_set_desc(dev, "ARM DMC-620 Memory Controller PMU");

	return (err);
}

static int
pmu_dmc620_acpi_attach(device_t dev)
{
	struct pmu_dmc620_softc *sc;
	int domain, i, u;
	const char *dname;

	dname = device_get_name(dev);
	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	u = device_get_unit(dev);
	sc->sc_unit = u;

	/*
	 * Ampere Altra support NUMA emulation, but DMC-620 PMU units have no
	 * mapping. Emulate this with kenv/hints.
	 * Format "hint.pmu_dmc620.3.domain=1".
	 */
	if ((resource_int_value(dname, u, "domain", &domain) == 0 ||
	    bus_get_domain(dev, &domain) == 0) && domain < MAXMEMDOM) {
		sc->sc_domain = domain;
	}
	device_printf(dev, "domain=%d\n", domain);

	i = bus_alloc_resources(dev, pmu_dmc620_res_spec, sc->sc_res);
	if (i != 0) {
		device_printf(dev, "cannot allocate resources for device (%d)\n",
		    i);
		return (i);
	}
	/* Disable counter before enable interrupt. */
	for (i = 0; i < DMC620_CLKDIV2_COUNTERS_N; i++) {
		CD2MD4(sc, i, DMC620_COUNTER_CONTROL,
		    DMC620_COUNTER_CONTROL_ENABLE, 0);
	}
	for (i = 0; i < DMC620_CLK_COUNTERS_N; i++) {
		CMD4(sc, i, DMC620_COUNTER_CONTROL,
		    DMC620_COUNTER_CONTROL_ENABLE, 0);
	}

	/* Clear intr status. */
	WR4(sc, DMC620_OVERFLOW_STATUS_CLKDIV2, 0);
	WR4(sc, DMC620_OVERFLOW_STATUS_CLK, 0);

	if (sc->sc_res[1] != NULL && bus_setup_intr(dev, sc->sc_res[1],
	    INTR_TYPE_MISC | INTR_MPSAFE, pmu_dmc620_counter_overflow_intr,
	    NULL, sc, &sc->sc_ih)) {
		bus_release_resources(dev, pmu_dmc620_res_spec, sc->sc_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		return (ENXIO);
	}
	dmc620_pmc_register(u, sc, domain);
	return (0);
}

static int
pmu_dmc620_acpi_detach(device_t dev)
{
	struct pmu_dmc620_softc *sc;

	sc = device_get_softc(dev);
	dmc620_pmc_unregister(device_get_unit(dev));
	if (sc->sc_res[1] != NULL) {
		bus_teardown_intr(dev, sc->sc_res[1], sc->sc_ih);
	}
	bus_release_resources(dev, pmu_dmc620_res_spec, sc->sc_res);

	return (0);
}

static void
pmu_dmc620_clkdiv2_overflow(struct trapframe *tf, struct pmu_dmc620_softc *sc,
    u_int i)
{

	atomic_add_32(&sc->sc_clkdiv2_conters_hi[i], 1);
	/* Call dmc620 handler directly, because hook busy by arm64_intr. */
	dmc620_intr(tf, PMC_CLASS_DMC620_PMU_CD2, sc->sc_unit, i);
}

static void
pmu_dmc620_clk_overflow(struct trapframe *tf, struct pmu_dmc620_softc *sc,
    u_int i)
{

	atomic_add_32(&sc->sc_clk_conters_hi[i], 1);
	/* Call dmc620 handler directly, because hook busy by arm64_intr. */
	dmc620_intr(tf, PMC_CLASS_DMC620_PMU_C, sc->sc_unit, i);

}

static int
pmu_dmc620_counter_overflow_intr(void *arg)
{
	uint32_t clkdiv2_stat, clk_stat;
	struct pmu_dmc620_softc *sc;
	struct trapframe *tf;
	u_int i;

	tf = PCPU_GET(curthread)->td_intr_frame;
	sc = (struct pmu_dmc620_softc *) arg;
	clkdiv2_stat = RD4(sc, DMC620_OVERFLOW_STATUS_CLKDIV2);
	clk_stat = RD4(sc, DMC620_OVERFLOW_STATUS_CLK);

	if ((clkdiv2_stat == 0) && (clk_stat == 0))
		return (FILTER_STRAY);
	/* Stop and save states of all counters. */
	for (i = 0; i < DMC620_COUNTERS_N; i++) {
		sc->sc_saved_control[i] = RD4(sc, DMC620_REG(i,
		    DMC620_COUNTER_CONTROL));
		WR4(sc, DMC620_REG(i, DMC620_COUNTER_CONTROL),
		    sc->sc_saved_control[i] & ~DMC620_COUNTER_CONTROL_ENABLE);
	}

	if (clkdiv2_stat != 0) {
		for (i = 0; i < DMC620_CLKDIV2_COUNTERS_N; i++) {
			if ((clkdiv2_stat & (1 << i)) == 0)
				continue;
			pmu_dmc620_clkdiv2_overflow(tf, sc, i);
		}
		WR4(sc, DMC620_OVERFLOW_STATUS_CLKDIV2, 0);
	}
	if (clk_stat != 0) {
		for (i = 0; i < DMC620_CLK_COUNTERS_N; i++) {
			if ((clk_stat & (1 << i)) == 0)
				continue;
			pmu_dmc620_clk_overflow(tf, sc, i);
		}
		WR4(sc, DMC620_OVERFLOW_STATUS_CLK, 0);
	}

	/* Restore states of all counters. */
	for (i = 0; i < DMC620_COUNTERS_N; i++) {
		WR4(sc, DMC620_REG(i, DMC620_COUNTER_CONTROL),
		    sc->sc_saved_control[i]);
	}

	return (FILTER_HANDLED);
}

static device_method_t pmu_dmc620_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			pmu_dmc620_acpi_probe),
	DEVMETHOD(device_attach,		pmu_dmc620_acpi_attach),
	DEVMETHOD(device_detach,		pmu_dmc620_acpi_detach),

	/* End */
	DEVMETHOD_END
};

static driver_t pmu_dmc620_acpi_driver = {
	"pmu_dmc620",
	pmu_dmc620_acpi_methods,
	sizeof(struct pmu_dmc620_softc),
};

DRIVER_MODULE(pmu_dmc620, acpi, pmu_dmc620_acpi_driver, 0, 0);
/* Reverse dependency. hwpmc needs DMC-620 on ARM64. */
MODULE_DEPEND(pmc, pmu_dmc620, 1, 1, 1);
MODULE_VERSION(pmu_dmc620, 1);
