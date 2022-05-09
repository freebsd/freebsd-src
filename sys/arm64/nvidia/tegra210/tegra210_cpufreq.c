/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/nvidia/tegra_efuse.h>

#include "cpufreq_if.h"

/* CPU voltage table entry */
struct speedo_entry {
	uint64_t		freq; 	/* Frequency point */
	int			c0; 	/* Coeeficient values for */
	int			c1;	/* quadratic equation: */
	int 			c2;	/* c2 * speedo^2 + c1 * speedo + c0 */
};

struct cpu_volt_def {
	int			min_uvolt;	/* Min allowed CPU voltage */
	int			max_uvolt;	/* Max allowed CPU voltage */
	int 			step_uvolt; 	/* Step of CPU voltage */
	int			speedo_scale;	/* Scaling factor for cvt */
	int			speedo_nitems;	/* Size of speedo table */
	struct speedo_entry	*speedo_tbl;	/* CPU voltage table */
};

struct cpu_speed_point {
	uint64_t		freq;		/* Frequecy */
	int			uvolt;		/* Requested voltage */
};

static struct speedo_entry tegra210_speedo_tbl[] =
{
	{204000000UL,	1007452, -23865, 370},
	{306000000UL,	1052709, -24875, 370},
	{408000000UL,	1099069, -25895, 370},
	{510000000UL,	1146534, -26905, 370},
	{612000000UL,	1195102, -27915, 370},
	{714000000UL,	1244773, -28925, 370},
	{816000000UL,	1295549, -29935, 370},
	{918000000UL,	1347428, -30955, 370},
	{1020000000UL,	1400411, -31965, 370},
	{1122000000UL,	1454497, -32975, 370},
	{1224000000UL,	1509687, -33985, 370},
	{1326000000UL,	1565981, -35005, 370},
	{1428000000UL,	1623379, -36015, 370},
	{1530000000UL,	1681880, -37025, 370},
	{1632000000UL,	1741485, -38035, 370},
	{1734000000UL,	1802194, -39055, 370},
	{1836000000UL,	1864006, -40065, 370},
	{1912500000UL,	1910780, -40815, 370},
	{2014500000UL,	1227000,      0,   0},
	{2218500000UL,	1227000,      0,   0},
};

static struct cpu_volt_def tegra210_cpu_volt_def =
{
	.min_uvolt = 900000,		/* 0.9 V */
	.max_uvolt = 1227000,		/* 1.227 */
	.step_uvolt =  10000,		/* 10 mV */
	.speedo_scale = 100,
	.speedo_nitems = nitems(tegra210_speedo_tbl),
	.speedo_tbl = tegra210_speedo_tbl,
};

static uint64_t cpu_max_freq[] = {
	1912500000UL,
	1912500000UL,
	2218500000UL,
	1785000000UL,
	1632000000UL,
	1912500000UL,
	2014500000UL,
	1734000000UL,
	1683000000UL,
	1555500000UL,
	1504500000UL,
};

static uint64_t cpu_freq_tbl[] = {
	 204000000UL,
	 306000000UL,
	 408000000UL,
	 510000000UL,
	 612000000UL,
	 714000000UL,
	 816000000UL,
	 918000000UL,
	1020000000UL,
	1122000000UL,
	1224000000UL,
	1326000000UL,
	1428000000UL,
	1530000000UL,
	1632000000UL,
	1734000000UL,
	1836000000UL,
	1912500000UL,
	2014500000UL,
	2218500000UL,
};

struct tegra210_cpufreq_softc {
	device_t		dev;
	phandle_t		node;

	clk_t			clk_cpu_g;
	clk_t			clk_pll_x;
	clk_t			clk_pll_p;
	clk_t			clk_dfll;

	int 			process_id;
	int 			speedo_id;
	int 			speedo_value;

	uint64_t		cpu_max_freq;
	struct cpu_volt_def	*cpu_def;
	struct cpu_speed_point	*speed_points;
	int			nspeed_points;

	struct cpu_speed_point	*act_speed_point;

	int			latency;
};

static int cpufreq_lowest_freq = 1;
TUNABLE_INT("hw.tegra210.cpufreq.lowest_freq", &cpufreq_lowest_freq);

#define	DIV_ROUND_CLOSEST(val, div)	(((val) + ((div) / 2)) / (div))

#define	ROUND_UP(val, div)	roundup(val, div)
#define	ROUND_DOWN(val, div)	rounddown(val, div)

/*
 * Compute requesetd voltage for given frequency and SoC process variations,
 * - compute base voltage from speedo value using speedo table
 * - round up voltage to next regulator step
 * - clamp it to regulator limits
 */
static int
freq_to_voltage(struct tegra210_cpufreq_softc *sc, uint64_t freq)
{
	int uv, scale, min_uvolt, max_uvolt, step_uvolt;
	struct speedo_entry *ent;
	int i;

	/* Get speedo entry with higher frequency */
	ent = NULL;
	for (i = 0; i < sc->cpu_def->speedo_nitems; i++) {
		if (sc->cpu_def->speedo_tbl[i].freq >= freq) {
			ent = &sc->cpu_def->speedo_tbl[i];
			break;
		}
	}
	if (ent == NULL)
		ent = &sc->cpu_def->speedo_tbl[sc->cpu_def->speedo_nitems - 1];
	scale = sc->cpu_def->speedo_scale;


	/* uV = (c2 * speedo / scale + c1) * speedo / scale + c0) */
	uv = DIV_ROUND_CLOSEST(ent->c2 * sc->speedo_value, scale);
	uv = DIV_ROUND_CLOSEST((uv + ent->c1) * sc->speedo_value, scale) +
	    ent->c0;
	step_uvolt = sc->cpu_def->step_uvolt;
	/* Round up it to next regulator step */
	uv = ROUND_UP(uv, step_uvolt);

	/* Clamp result */
	min_uvolt = ROUND_UP(sc->cpu_def->min_uvolt, step_uvolt);
	max_uvolt = ROUND_DOWN(sc->cpu_def->max_uvolt, step_uvolt);
	if (uv < min_uvolt)
		uv =  min_uvolt;
	if (uv > max_uvolt)
		uv =  max_uvolt;
	return (uv);

}

static void
build_speed_points(struct tegra210_cpufreq_softc *sc) {
	int i;

	sc->nspeed_points = nitems(cpu_freq_tbl);
	sc->speed_points = malloc(sizeof(struct cpu_speed_point) *
	    sc->nspeed_points, M_DEVBUF, M_NOWAIT);
	for (i = 0; i < sc->nspeed_points; i++) {
		sc->speed_points[i].freq = cpu_freq_tbl[i];
		sc->speed_points[i].uvolt = freq_to_voltage(sc,
		    cpu_freq_tbl[i]);
	}
}

static struct cpu_speed_point *
get_speed_point(struct tegra210_cpufreq_softc *sc, uint64_t freq)
{
	int i;

	if (sc->speed_points[0].freq >= freq)
		return (sc->speed_points + 0);

	for (i = 0; i < sc->nspeed_points - 1; i++) {
		if (sc->speed_points[i + 1].freq > freq)
			return (sc->speed_points + i);
	}

	return (sc->speed_points + sc->nspeed_points - 1);
}

static int
tegra210_cpufreq_settings(device_t dev, struct cf_setting *sets, int *count)
{
	struct tegra210_cpufreq_softc *sc;
	int i, j;

	if (sets == NULL || count == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);
	memset(sets, CPUFREQ_VAL_UNKNOWN, sizeof(*sets) * (*count));

	for (i = 0, j = sc->nspeed_points - 1; j >= 0; j--) {
		if (sc->cpu_max_freq < sc->speed_points[j].freq)
			continue;
		sets[i].freq = sc->speed_points[j].freq / 1000000;
		sets[i].volts = sc->speed_points[j].uvolt / 1000;
		sets[i].lat = sc->latency;
		sets[i].dev = dev;
		i++;
	}
	*count = i;

	return (0);
}

static int
set_cpu_freq(struct tegra210_cpufreq_softc *sc, uint64_t freq)
{
	struct cpu_speed_point *point;
	int rv;

	point = get_speed_point(sc, freq);

	/* Set PLLX frequency */
	rv = clk_set_freq(sc->clk_pll_x, point->freq, CLK_SET_ROUND_DOWN);
	if (rv != 0) {
		device_printf(sc->dev, "Can't set CPU clock frequency\n");
		return (rv);
	}

	sc->act_speed_point = point;

	return (0);
}

static int
tegra210_cpufreq_set(device_t dev, const struct cf_setting *cf)
{
	struct tegra210_cpufreq_softc *sc;
	uint64_t freq;
	int rv;

	if (cf == NULL || cf->freq < 0)
		return (EINVAL);

	sc = device_get_softc(dev);

	freq = cf->freq;
	if (freq < cpufreq_lowest_freq)
		freq = cpufreq_lowest_freq;
	freq *= 1000000;
	if (freq >= sc->cpu_max_freq)
		freq = sc->cpu_max_freq;
	rv = set_cpu_freq(sc, freq);

	return (rv);
}

static int
tegra210_cpufreq_get(device_t dev, struct cf_setting *cf)
{
	struct tegra210_cpufreq_softc *sc;

	if (cf == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);
	memset(cf, CPUFREQ_VAL_UNKNOWN, sizeof(*cf));
	cf->dev = NULL;
	cf->freq = sc->act_speed_point->freq / 1000000;
	cf->volts = sc->act_speed_point->uvolt / 1000;
	/* Transition latency in us. */
	cf->lat = sc->latency;
	/* Driver providing this setting. */
	cf->dev = dev;

	return (0);
}


static int
tegra210_cpufreq_type(device_t dev, int *type)
{

	if (type == NULL)
		return (EINVAL);
	*type = CPUFREQ_TYPE_ABSOLUTE;

	return (0);
}

static int
get_fdt_resources(struct tegra210_cpufreq_softc *sc, phandle_t node)
{
	int rv;
	device_t parent_dev;

	parent_dev =  device_get_parent(sc->dev);

	rv = clk_get_by_ofw_name(parent_dev, 0, "cpu_g", &sc->clk_cpu_g);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'cpu_g' clock: %d\n", rv);
		return (ENXIO);
	}

	rv = clk_get_by_ofw_name(parent_dev, 0, "pll_x", &sc->clk_pll_x);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pll_x' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(parent_dev, 0, "pll_p", &sc->clk_pll_p);
	if (rv != 0) {
		device_printf(parent_dev, "Cannot get 'pll_p' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(parent_dev, 0, "dfll", &sc->clk_dfll);

	/* XXX DPLL is not implemented yet */
#if 0
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'dfll' clock\n");
		return (ENXIO);
	}
#endif
	return (0);
}

static void
tegra210_cpufreq_identify(driver_t *driver, device_t parent)
{
	phandle_t root;

	root = OF_finddevice("/");
	if (!ofw_bus_node_is_compatible(root, "nvidia,tegra210"))
		return;

	if (device_get_unit(parent) != 0)
		return;
	if (device_find_child(parent, "tegra210_cpufreq", -1) != NULL)
		return;
	if (BUS_ADD_CHILD(parent, 0, "tegra210_cpufreq", -1) == NULL)
		device_printf(parent, "add child failed\n");
}

static int
tegra210_cpufreq_probe(device_t dev)
{

	device_set_desc(dev, "CPU Frequency Control");

	return (0);
}

static int
tegra210_cpufreq_attach(device_t dev)
{
	struct tegra210_cpufreq_softc *sc;
	uint64_t freq;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->node = ofw_bus_get_node(device_get_parent(dev));

	sc->process_id = tegra_sku_info.cpu_process_id;
	sc->speedo_id = tegra_sku_info.cpu_speedo_id;
	sc->speedo_value = tegra_sku_info.cpu_speedo_value;

	sc->cpu_def = &tegra210_cpu_volt_def;

	rv = get_fdt_resources(sc, sc->node);
	if (rv !=  0) {
		return (rv);
	}

	build_speed_points(sc);

	rv = clk_get_freq(sc->clk_cpu_g, &freq);
	if (rv != 0) {
		device_printf(dev, "Can't get CPU clock frequency\n");
		return (rv);
	}
	if (sc->speedo_id < nitems(cpu_max_freq))
		sc->cpu_max_freq = cpu_max_freq[sc->speedo_id];
	else
		sc->cpu_max_freq = cpu_max_freq[0];
	sc->act_speed_point = get_speed_point(sc, freq);

	/* Set safe startup CPU frequency. */
	rv = set_cpu_freq(sc, 1632000000);
	if (rv != 0) {
		device_printf(dev, "Can't set initial CPU clock frequency\n");
		return (rv);
	}

	/* This device is controlled by cpufreq(4). */
	cpufreq_register(dev);

	return (0);
}

static int
tegra210_cpufreq_detach(device_t dev)
{
	struct tegra210_cpufreq_softc *sc;

	sc = device_get_softc(dev);
	cpufreq_unregister(dev);

	if (sc->clk_cpu_g != NULL)
		clk_release(sc->clk_cpu_g);
	if (sc->clk_pll_x != NULL)
		clk_release(sc->clk_pll_x);
	if (sc->clk_pll_p != NULL)
		clk_release(sc->clk_pll_p);
	if (sc->clk_dfll != NULL)
		clk_release(sc->clk_dfll);
	return (0);
}

static device_method_t tegra210_cpufreq_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	tegra210_cpufreq_identify),
	DEVMETHOD(device_probe,		tegra210_cpufreq_probe),
	DEVMETHOD(device_attach,	tegra210_cpufreq_attach),
	DEVMETHOD(device_detach,	tegra210_cpufreq_detach),

	/* cpufreq interface */
	DEVMETHOD(cpufreq_drv_set,	tegra210_cpufreq_set),
	DEVMETHOD(cpufreq_drv_get,	tegra210_cpufreq_get),
	DEVMETHOD(cpufreq_drv_settings,	tegra210_cpufreq_settings),
	DEVMETHOD(cpufreq_drv_type,	tegra210_cpufreq_type),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(tegra210_cpufreq, tegra210_cpufreq_driver,
    tegra210_cpufreq_methods, sizeof(struct tegra210_cpufreq_softc));
DRIVER_MODULE(tegra210_cpufreq, cpu, tegra210_cpufreq_driver, NULL, NULL);
