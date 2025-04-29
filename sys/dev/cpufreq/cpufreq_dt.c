/*-
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.Org>
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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
 */

/*
 * Generic DT based cpufreq driver
 */

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

#include <dev/clk/clk.h>
#include <dev/regulator/regulator.h>

#include "cpufreq_if.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, "cpufreq_dt: " msg);
#else
#define DPRINTF(dev, msg...)
#endif

enum opp_version {
	OPP_V1 = 1,
	OPP_V2,
};

struct cpufreq_dt_opp {
	uint64_t	freq;
	uint32_t	uvolt_target;
	uint32_t	uvolt_min;
	uint32_t	uvolt_max;
	uint32_t	uamps;
	uint32_t	clk_latency;
	bool		turbo_mode;
	bool		opp_suspend;
};

#define	CPUFREQ_DT_HAVE_REGULATOR(sc)	((sc)->reg != NULL)

struct cpufreq_dt_softc {
	device_t dev;
	clk_t clk;
	regulator_t reg;

	struct cpufreq_dt_opp *opp;
	ssize_t nopp;

	int cpu;
	cpuset_t cpus;
};

static void
cpufreq_dt_notify(device_t dev, uint64_t freq)
{
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
}

static const struct cpufreq_dt_opp *
cpufreq_dt_find_opp(device_t dev, uint64_t freq)
{
	struct cpufreq_dt_softc *sc;
	uint64_t diff, best_diff;
	ssize_t n, best_n;

	sc = device_get_softc(dev);

	diff = 0;
	best_diff = ~0;
	DPRINTF(dev, "Looking for freq %ju\n", freq);
	for (n = 0; n < sc->nopp; n++) {
		diff = abs64((int64_t)sc->opp[n].freq - (int64_t)freq);
		DPRINTF(dev, "Testing %ju, diff is %ju\n", sc->opp[n].freq, diff);
		if (diff < best_diff) {
			best_diff = diff;
			best_n = n;
			DPRINTF(dev, "%ju is best for now\n", sc->opp[n].freq);
		}
	}

	DPRINTF(dev, "Will use %ju\n", sc->opp[best_n].freq);
	return (&sc->opp[best_n]);
}

static void
cpufreq_dt_opp_to_setting(device_t dev, const struct cpufreq_dt_opp *opp,
    struct cf_setting *set)
{

	memset(set, 0, sizeof(*set));
	set->freq = opp->freq / 1000000;
	set->volts = opp->uvolt_target / 1000;
	set->power = CPUFREQ_VAL_UNKNOWN;
	set->lat = opp->clk_latency;
	set->dev = dev;
}

static int
cpufreq_dt_get(device_t dev, struct cf_setting *set)
{
	struct cpufreq_dt_softc *sc;
	const struct cpufreq_dt_opp *opp;
	uint64_t freq;

	sc = device_get_softc(dev);

	DPRINTF(dev, "cpufreq_dt_get\n");
	if (clk_get_freq(sc->clk, &freq) != 0)
		return (ENXIO);

	opp = cpufreq_dt_find_opp(dev, freq);
	if (opp == NULL) {
		device_printf(dev, "Can't find the current freq in opp\n");
		return (ENOENT);
	}

	cpufreq_dt_opp_to_setting(dev, opp, set);

	DPRINTF(dev, "Current freq %dMhz\n", set->freq);
	return (0);
}

static int
cpufreq_dt_set(device_t dev, const struct cf_setting *set)
{
	struct cpufreq_dt_softc *sc;
	const struct cpufreq_dt_opp *opp, *copp;
	uint64_t freq;
	int uvolt, error;

	sc = device_get_softc(dev);

	DPRINTF(dev, "Working on cpu %d\n", sc->cpu);
	DPRINTF(dev, "We have %d cpu on this dev\n", CPU_COUNT(&sc->cpus));
	if (!CPU_ISSET(sc->cpu, &sc->cpus)) {
		DPRINTF(dev, "Not for this CPU\n");
		return (0);
	}

	if (clk_get_freq(sc->clk, &freq) != 0) {
		device_printf(dev, "Can't get current clk freq\n");
		return (ENXIO);
	}

	/*
	 * Only do the regulator work if it's required.
	 */
	if (CPUFREQ_DT_HAVE_REGULATOR(sc)) {
		/* Try to get current valtage by using regulator first. */
		error = regulator_get_voltage(sc->reg, &uvolt);
		if (error != 0) {
			/*
			 * Try oppoints table as backup way. However,
			 * this is insufficient because the actual processor
			 * frequency may not be in the table. PLL frequency
			 * granularity can be different that granularity of
			 * oppoint table.
			 */
			copp = cpufreq_dt_find_opp(sc->dev, freq);
			if (copp == NULL) {
				device_printf(dev,
				    "Can't find the current freq in opp\n");
				return (ENOENT);
			}
			uvolt = copp->uvolt_target;
		}
	} else
		uvolt = 0;

	opp = cpufreq_dt_find_opp(sc->dev, set->freq * 1000000);
	if (opp == NULL) {
		device_printf(dev, "Couldn't find an opp for this freq\n");
		return (EINVAL);
	}
	DPRINTF(sc->dev, "Current freq %ju, uvolt: %d\n", freq, uvolt);
	DPRINTF(sc->dev, "Target freq %ju, , uvolt: %d\n",
	    opp->freq, opp->uvolt_target);

	if (CPUFREQ_DT_HAVE_REGULATOR(sc) && (uvolt < opp->uvolt_target)) {
		DPRINTF(dev, "Changing regulator from %u to %u\n",
		    uvolt, opp->uvolt_target);
		error = regulator_set_voltage(sc->reg,
		    opp->uvolt_min,
		    opp->uvolt_max);
		if (error != 0) {
			DPRINTF(dev, "Failed, backout\n");
			return (ENXIO);
		}
	}

	DPRINTF(dev, "Setting clk to %ju\n", opp->freq);
	error = clk_set_freq(sc->clk, opp->freq, CLK_SET_ROUND_DOWN);
	if (error != 0) {
		DPRINTF(dev, "Failed, backout\n");
		/* Restore previous voltage (best effort) */
		if (CPUFREQ_DT_HAVE_REGULATOR(sc))
			error = regulator_set_voltage(sc->reg,
			    copp->uvolt_min,
			    copp->uvolt_max);
		return (ENXIO);
	}

	if (CPUFREQ_DT_HAVE_REGULATOR(sc) && (uvolt > opp->uvolt_target)) {
		DPRINTF(dev, "Changing regulator from %u to %u\n",
		    uvolt, opp->uvolt_target);
		error = regulator_set_voltage(sc->reg,
		    opp->uvolt_min,
		    opp->uvolt_max);
		if (error != 0) {
			DPRINTF(dev, "Failed to switch regulator to %d\n",
			    opp->uvolt_target);
			/* Restore previous CPU frequency (best effort) */
			(void)clk_set_freq(sc->clk, copp->freq, 0);
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

	DPRINTF(dev, "cpufreq_dt_settings\n");
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
	if (!OF_hasprop(node, "clocks"))
		return;

	if (!OF_hasprop(node, "operating-points") &&
	    !OF_hasprop(node, "operating-points-v2"))
		return;

	if (device_find_child(parent, "cpufreq_dt", -1) != NULL)
		return;

	if (BUS_ADD_CHILD(parent, 0, "cpufreq_dt", device_get_unit(parent))
	    == NULL)
		device_printf(parent, "add cpufreq_dt child failed\n");
}

static int
cpufreq_dt_probe(device_t dev)
{
	phandle_t node;

	node = ofw_bus_get_node(device_get_parent(dev));

	/*
	 * Note - supply isn't required here for probe; we'll check
	 * it out in more detail during attach.
	 */
	if (!OF_hasprop(node, "clocks"))
		return (ENXIO);

	if (!OF_hasprop(node, "operating-points") &&
	  !OF_hasprop(node, "operating-points-v2"))
		return (ENXIO);

	device_set_desc(dev, "Generic cpufreq driver");
	return (BUS_PROBE_GENERIC);
}

static int
cpufreq_dt_oppv1_parse(struct cpufreq_dt_softc *sc, phandle_t node)
{
	uint32_t *opp, lat;
	ssize_t n;

	sc->nopp = OF_getencprop_alloc_multi(node, "operating-points",
	    sizeof(uint32_t) * 2, (void **)&opp);
	if (sc->nopp == -1)
		return (ENXIO);

	if (OF_getencprop(node, "clock-latency", &lat, sizeof(lat)) == -1)
		lat = CPUFREQ_VAL_UNKNOWN;

	sc->opp = malloc(sizeof(*sc->opp) * sc->nopp, M_DEVBUF, M_WAITOK);

	for (n = 0; n < sc->nopp; n++) {
		sc->opp[n].freq = opp[n * 2 + 0] * 1000;
		sc->opp[n].uvolt_min = opp[n * 2 + 1];
		sc->opp[n].uvolt_max = sc->opp[n].uvolt_min;
		sc->opp[n].uvolt_target = sc->opp[n].uvolt_min;
		sc->opp[n].clk_latency = lat;

		if (bootverbose)
			device_printf(sc->dev, "%ju.%03ju MHz, %u uV\n",
			    sc->opp[n].freq / 1000000,
			    sc->opp[n].freq % 1000000,
			    sc->opp[n].uvolt_target);
	}
	free(opp, M_OFWPROP);

	return (0);
}

static int
cpufreq_dt_oppv2_parse(struct cpufreq_dt_softc *sc, phandle_t node)
{
	phandle_t opp, opp_table, opp_xref;
	pcell_t cell[2];
	uint32_t *volts, lat;
	int nvolt, i;

	/*
	 * operating-points-v2 does not require the voltage entries
	 * and a regulator.  So, it's OK if they're not there.
	 */
	if (OF_getencprop(node, "operating-points-v2", &opp_xref,
	    sizeof(opp_xref)) == -1) {
		device_printf(sc->dev, "Cannot get xref to oppv2 table\n");
		return (ENXIO);
	}

	opp_table = OF_node_from_xref(opp_xref);
	if (opp_table == opp_xref)
		return (ENXIO);

	if (!OF_hasprop(opp_table, "opp-shared") && mp_ncpus > 1) {
		device_printf(sc->dev, "Only opp-shared is supported\n");
		return (ENXIO);
	}

	for (opp = OF_child(opp_table); opp > 0; opp = OF_peer(opp))
		sc->nopp += 1;

	sc->opp = malloc(sizeof(*sc->opp) * sc->nopp, M_DEVBUF, M_WAITOK);

	for (i = 0, opp_table = OF_child(opp_table); opp_table > 0;
	     opp_table = OF_peer(opp_table), i++) {
		/* opp-hz is a required property */
		if (OF_getencprop(opp_table, "opp-hz", cell,
		    sizeof(cell)) == -1)
			continue;

		sc->opp[i].freq = cell[0];
		sc->opp[i].freq <<= 32;
		sc->opp[i].freq |= cell[1];

		if (OF_getencprop(opp_table, "clock-latency", &lat,
		    sizeof(lat)) == -1)
			sc->opp[i].clk_latency = CPUFREQ_VAL_UNKNOWN;
		else
			sc->opp[i].clk_latency = (int)lat;

		if (OF_hasprop(opp_table, "turbo-mode"))
			sc->opp[i].turbo_mode = true;
		if (OF_hasprop(opp_table, "opp-suspend"))
			sc->opp[i].opp_suspend = true;

		if (CPUFREQ_DT_HAVE_REGULATOR(sc)) {
			nvolt = OF_getencprop_alloc_multi(opp_table,
			    "opp-microvolt", sizeof(*volts), (void **)&volts);
			if (nvolt == 1) {
				sc->opp[i].uvolt_target = volts[0];
				sc->opp[i].uvolt_min = volts[0];
				sc->opp[i].uvolt_max = volts[0];
			} else if (nvolt == 3) {
				sc->opp[i].uvolt_target = volts[0];
				sc->opp[i].uvolt_min = volts[1];
				sc->opp[i].uvolt_max = volts[2];
			} else {
				device_printf(sc->dev,
				    "Wrong count of opp-microvolt property\n");
				OF_prop_free(volts);
				free(sc->opp, M_DEVBUF);
				return (ENXIO);
			}
			OF_prop_free(volts);
		} else {
			/* No regulator required; don't add anything */
			sc->opp[i].uvolt_target = 0;
			sc->opp[i].uvolt_min = 0;
			sc->opp[i].uvolt_max = 0;
		}

		if (bootverbose)
			device_printf(sc->dev, "%ju.%03ju Mhz (%u uV)\n",
			    sc->opp[i].freq / 1000000,
			    sc->opp[i].freq % 1000000,
			    sc->opp[i].uvolt_target);
	}
	return (0);
}

static int
cpufreq_dt_attach(device_t dev)
{
	struct cpufreq_dt_softc *sc;
	phandle_t node;
	phandle_t cnode, opp, copp;
	int cpu;
	uint64_t freq;
	int rv = 0;
	char device_type[16];
	enum opp_version version;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(device_get_parent(dev));
	sc->cpu = device_get_unit(device_get_parent(dev));
	sc->reg = NULL;

	DPRINTF(dev, "cpu=%d\n", sc->cpu);
	if (sc->cpu >= mp_ncpus) {
		device_printf(dev, "Not attaching as cpu is not present\n");
		rv = ENXIO;
		goto error;
	}

	/*
	 * Cache if we have the regulator supply but don't error out
	 * quite yet.  If it's operating-points-v2 then regulator
	 * and voltage entries are optional.
	 */
	if (regulator_get_by_ofw_property(dev, node, "cpu-supply",
	    &sc->reg) == 0)
		device_printf(dev, "Found cpu-supply\n");
	else if (regulator_get_by_ofw_property(dev, node, "cpu0-supply",
	    &sc->reg) == 0)
		device_printf(dev, "Found cpu0-supply\n");

	/*
	 * Determine which operating mode we're in.  Error out if we expect
	 * a regulator but we're not getting it.
	 */
	if (OF_hasprop(node, "operating-points"))
		version = OPP_V1;
	else if (OF_hasprop(node, "operating-points-v2"))
		version = OPP_V2;
	else {
		device_printf(dev,
		    "didn't find a valid operating-points or v2 node\n");
		rv = ENXIO;
		goto error;
	}

	/*
	 * Now, we only enforce needing a regulator for v1.
	 */
	if ((version == OPP_V1) && !CPUFREQ_DT_HAVE_REGULATOR(sc)) {
		device_printf(dev, "no regulator for %s\n",
		    ofw_bus_get_name(device_get_parent(dev)));
		rv = ENXIO;
		goto error;
	}

	if (clk_get_by_ofw_index(dev, node, 0, &sc->clk) != 0) {
		device_printf(dev, "no clock for %s\n",
		    ofw_bus_get_name(device_get_parent(dev)));
		rv = ENXIO;
		goto error;
	}

	if (version == OPP_V1) {
		rv = cpufreq_dt_oppv1_parse(sc, node);
		if (rv != 0) {
			device_printf(dev, "Failed to parse opp-v1 table\n");
			goto error;
		}
		OF_getencprop(node, "operating-points", &opp,
		    sizeof(opp));
	} else if (version == OPP_V2) {
		rv = cpufreq_dt_oppv2_parse(sc, node);
		if (rv != 0) {
			device_printf(dev, "Failed to parse opp-v2 table\n");
			goto error;
		}
		OF_getencprop(node, "operating-points-v2", &opp,
		    sizeof(opp));
	} else {
		device_printf(dev, "operating points version is incorrect\n");
		goto error;
	}

	/*
	 * Find all CPUs that share the same opp table
	 */
	CPU_ZERO(&sc->cpus);
	cnode = OF_parent(node);
	for (cpu = 0, cnode = OF_child(cnode); cnode > 0; cnode = OF_peer(cnode)) {
		if (OF_getprop(cnode, "device_type", device_type, sizeof(device_type)) <= 0)
			continue;
		if (strcmp(device_type, "cpu") != 0)
			continue;
		if (cpu == sc->cpu) {
			DPRINTF(dev, "Skipping our cpu\n");
			CPU_SET(cpu, &sc->cpus);
			cpu++;
			continue;
		}
		DPRINTF(dev, "Testing CPU %d\n", cpu);
		copp = -1;
		if (version == OPP_V1)
			OF_getencprop(cnode, "operating-points", &copp,
			    sizeof(copp));
		else if (version == OPP_V2)
			OF_getencprop(cnode, "operating-points-v2",
			    &copp, sizeof(copp));
		if (opp == copp) {
			DPRINTF(dev, "CPU %d is using the same opp as this one (%d)\n",
			    cpu, sc->cpu);
			CPU_SET(cpu, &sc->cpus);
		}
		cpu++;
	}

	if (clk_get_freq(sc->clk, &freq) == 0)
		cpufreq_dt_notify(dev, freq);

	cpufreq_register(dev);

	return (0);
error:
	if (CPUFREQ_DT_HAVE_REGULATOR(sc))
		regulator_release(sc->reg);
	return (rv);
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

DRIVER_MODULE(cpufreq_dt, cpu, cpufreq_dt_driver, 0, 0);
MODULE_VERSION(cpufreq_dt, 1);
