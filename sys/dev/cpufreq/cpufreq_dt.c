/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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

/*
 * Generic DT based cpufreq driver
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/cpu.h>
#include <sys/cpuset.h>
#include <sys/smp.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/regulator/regulator.h>

#include "cpufreq_if.h"

struct cpufreq_dt_opp {
	uint32_t	freq_khz;
	uint32_t	voltage_uv;
};

struct cpufreq_dt_softc {
	clk_t clk;
	regulator_t reg;

	struct cpufreq_dt_opp *opp;
	ssize_t nopp;
	int clk_latency;

	cpuset_t cpus;
};

static void
cpufreq_dt_notify(device_t dev, uint64_t freq)
{
#ifdef __aarch64__
	struct cpufreq_dt_softc *sc;
	struct pcpu *pc;
	int cpu;

	sc = device_get_softc(dev);

	CPU_FOREACH(cpu) {
		if (CPU_ISSET(cpu, &sc->cpus)) {
			pc = pcpu_find(cpu);
			pc->pc_clock = freq;
		}
	}
#endif
}

static const struct cpufreq_dt_opp *
cpufreq_dt_find_opp(device_t dev, uint32_t freq_mhz)
{
	struct cpufreq_dt_softc *sc;
	ssize_t n;

	sc = device_get_softc(dev);

	for (n = 0; n < sc->nopp; n++)
		if (CPUFREQ_CMP(sc->opp[n].freq_khz / 1000, freq_mhz))
			return (&sc->opp[n]);

	return (NULL);
}

static void
cpufreq_dt_opp_to_setting(device_t dev, const struct cpufreq_dt_opp *opp,
    struct cf_setting *set)
{
	struct cpufreq_dt_softc *sc;

	sc = device_get_softc(dev);

	memset(set, 0, sizeof(*set));
	set->freq = opp->freq_khz / 1000;
	set->volts = opp->voltage_uv / 1000;
	set->power = CPUFREQ_VAL_UNKNOWN;
	set->lat = sc->clk_latency;
	set->dev = dev;
}

static int
cpufreq_dt_get(device_t dev, struct cf_setting *set)
{
	struct cpufreq_dt_softc *sc;
	const struct cpufreq_dt_opp *opp;
	uint64_t freq;

	sc = device_get_softc(dev);

	if (clk_get_freq(sc->clk, &freq) != 0)
		return (ENXIO);

	opp = cpufreq_dt_find_opp(dev, freq / 1000000);
	if (opp == NULL)
		return (ENOENT);

	cpufreq_dt_opp_to_setting(dev, opp, set);

	return (0);
}

static int
cpufreq_dt_set(device_t dev, const struct cf_setting *set)
{
	struct cpufreq_dt_softc *sc;
	const struct cpufreq_dt_opp *opp, *copp;
	uint64_t freq;
	int error;

	sc = device_get_softc(dev);

	if (clk_get_freq(sc->clk, &freq) != 0)
		return (ENXIO);

	copp = cpufreq_dt_find_opp(dev, freq / 1000000);
	if (copp == NULL)
		return (ENOENT);
	opp = cpufreq_dt_find_opp(dev, set->freq);
	if (opp == NULL)
		return (EINVAL);

	if (copp->voltage_uv < opp->voltage_uv) {
		error = regulator_set_voltage(sc->reg, opp->voltage_uv,
		    opp->voltage_uv);
		if (error != 0)
			return (ENXIO);
	}

	error = clk_set_freq(sc->clk, (uint64_t)opp->freq_khz * 1000, 0);
	if (error != 0) {
		/* Restore previous voltage (best effort) */
		(void)regulator_set_voltage(sc->reg, copp->voltage_uv,
		    copp->voltage_uv);
		return (ENXIO);
	}

	if (copp->voltage_uv > opp->voltage_uv) {
		error = regulator_set_voltage(sc->reg, opp->voltage_uv,
		    opp->voltage_uv);
		if (error != 0) {
			/* Restore previous CPU frequency (best effort) */
			(void)clk_set_freq(sc->clk,
			    (uint64_t)copp->freq_khz * 1000, 0);
			return (ENXIO);
		}
	}

	if (clk_get_freq(sc->clk, &freq) == 0)
		cpufreq_dt_notify(dev, freq);

	return (0);
}


static int
cpufreq_dt_type(device_t dev, int *type)
{
	if (type == NULL)
		return (EINVAL);

	*type = CPUFREQ_TYPE_ABSOLUTE;
	return (0);
}

static int
cpufreq_dt_settings(device_t dev, struct cf_setting *sets, int *count)
{
	struct cpufreq_dt_softc *sc;
	ssize_t n;

	if (sets == NULL || count == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);

	if (*count < sc->nopp) {
		*count = (int)sc->nopp;
		return (E2BIG);
	}

	for (n = 0; n < sc->nopp; n++)
		cpufreq_dt_opp_to_setting(dev, &sc->opp[n], &sets[n]);

	*count = (int)sc->nopp;

	return (0);
}

static void
cpufreq_dt_identify(driver_t *driver, device_t parent)
{
	phandle_t node;

	/* Properties must be listed under node /cpus/cpu@0 */
	node = ofw_bus_get_node(parent);

	/* The cpu@0 node must have the following properties */
	if (!OF_hasprop(node, "operating-points") ||
	    !OF_hasprop(node, "clocks") ||
	    !OF_hasprop(node, "cpu-supply"))
		return;

	if (device_find_child(parent, "cpufreq_dt", -1) != NULL)
		return;

	if (BUS_ADD_CHILD(parent, 0, "cpufreq_dt", -1) == NULL)
		device_printf(parent, "add cpufreq_dt child failed\n");
}

static int
cpufreq_dt_probe(device_t dev)
{
	phandle_t node;

	node = ofw_bus_get_node(device_get_parent(dev));

	if (!OF_hasprop(node, "operating-points") ||
	    !OF_hasprop(node, "clocks") ||
	    !OF_hasprop(node, "cpu-supply"))
		return (ENXIO);

	device_set_desc(dev, "Generic cpufreq driver");
	return (BUS_PROBE_GENERIC);
}

static int
cpufreq_dt_attach(device_t dev)
{
	struct cpufreq_dt_softc *sc;
	uint32_t *opp, lat;
	phandle_t node, cnode;
	uint64_t freq;
	ssize_t n;
	int cpu;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(device_get_parent(dev));

	if (regulator_get_by_ofw_property(dev, node,
	    "cpu-supply", &sc->reg) != 0) {
		device_printf(dev, "no regulator for %s\n",
		    ofw_bus_get_name(device_get_parent(dev)));
		return (ENXIO);
	}

	if (clk_get_by_ofw_index(dev, node, 0, &sc->clk) != 0) {
		device_printf(dev, "no clock for %s\n",
		    ofw_bus_get_name(device_get_parent(dev)));
		regulator_release(sc->reg);
		return (ENXIO);
	}

	sc->nopp = OF_getencprop_alloc(node, "operating-points",
	    sizeof(*sc->opp), (void **)&opp);
	if (sc->nopp == -1)
		return (ENXIO);
	sc->opp = malloc(sizeof(*sc->opp) * sc->nopp, M_DEVBUF, M_WAITOK);
	for (n = 0; n < sc->nopp; n++) {
		sc->opp[n].freq_khz = opp[n * 2 + 0];
		sc->opp[n].voltage_uv = opp[n * 2 + 1];

		if (bootverbose)
			device_printf(dev, "%u.%03u MHz, %u uV\n",
			    sc->opp[n].freq_khz / 1000,
			    sc->opp[n].freq_khz % 1000,
			    sc->opp[n].voltage_uv);
	}
	free(opp, M_OFWPROP);

	if (OF_getencprop(node, "clock-latency", &lat, sizeof(lat)) == -1)
		sc->clk_latency = CPUFREQ_VAL_UNKNOWN;
	else
		sc->clk_latency = (int)lat;

	/*
	 * Find all CPUs that share the same voltage and CPU frequency
	 * controls. Start with the current node and move forward until
	 * the end is reached or a peer has an "operating-points" property.
	 */
	CPU_ZERO(&sc->cpus);
	cpu = device_get_unit(device_get_parent(dev));
	for (cnode = node; cnode > 0; cnode = OF_peer(cnode), cpu++) {
		if (cnode != node && OF_hasprop(cnode, "operating-points"))
			break;
		CPU_SET(cpu, &sc->cpus);
	}

	if (clk_get_freq(sc->clk, &freq) == 0)
		cpufreq_dt_notify(dev, freq);

	cpufreq_register(dev);

	return (0);
}


static device_method_t cpufreq_dt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	cpufreq_dt_identify),
	DEVMETHOD(device_probe,		cpufreq_dt_probe),
	DEVMETHOD(device_attach,	cpufreq_dt_attach),

	/* cpufreq interface */
	DEVMETHOD(cpufreq_drv_get,	cpufreq_dt_get),
	DEVMETHOD(cpufreq_drv_set,	cpufreq_dt_set),
	DEVMETHOD(cpufreq_drv_type,	cpufreq_dt_type),
	DEVMETHOD(cpufreq_drv_settings,	cpufreq_dt_settings),

	DEVMETHOD_END
};

static driver_t cpufreq_dt_driver = {
	"cpufreq_dt",
	cpufreq_dt_methods,
	sizeof(struct cpufreq_dt_softc),
};

static devclass_t cpufreq_dt_devclass;

DRIVER_MODULE(cpufreq_dt, cpu, cpufreq_dt_driver, cpufreq_dt_devclass, 0, 0);
MODULE_VERSION(cpufreq_dt, 1);
