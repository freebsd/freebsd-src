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
 * Allwinner thermal sensor controller
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/reboot.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <arm/allwinner/aw_sid.h>

#define	THS_CTRL0		0x00
#define	THS_CTRL1		0x04
#define	 ADC_CALI_EN		(1 << 17)
#define	THS_CTRL2		0x40
#define	 SENSOR_ACQ1_SHIFT	16
#define	 SENSOR2_EN		(1 << 2)
#define	 SENSOR1_EN		(1 << 1)
#define	 SENSOR0_EN		(1 << 0)
#define	THS_INTC		0x44
#define	THS_INTS		0x48
#define	 THS2_DATA_IRQ_STS	(1 << 10)
#define	 THS1_DATA_IRQ_STS	(1 << 9)
#define	 THS0_DATA_IRQ_STS	(1 << 8)
#define	 SHUT_INT2_STS		(1 << 6)
#define	 SHUT_INT1_STS		(1 << 5)
#define	 SHUT_INT0_STS		(1 << 4)
#define	 ALARM_INT2_STS		(1 << 2)
#define	 ALARM_INT1_STS		(1 << 1)
#define	 ALARM_INT0_STS		(1 << 0)
#define	THS_FILTER		0x70
#define	THS_CALIB0		0x74
#define	THS_CALIB1		0x78
#define	THS_DATA0		0x80
#define	THS_DATA1		0x84
#define	THS_DATA2		0x88
#define	 DATA_MASK		0xfff

#define	A83T_ADC_ACQUIRE_TIME	0x17
#define	A83T_FILTER		0x4
#define	A83T_INTC		0x1000
#define	A83T_TEMP_BASE		2719000
#define	A83T_TEMP_DIV		14186
#define	A83T_CLK_RATE		24000000

#define	A64_ADC_ACQUIRE_TIME	0x190
#define	A64_FILTER		0x6
#define A64_INTC		0x18000
#define	A64_TEMP_BASE		2170000
#define	A64_TEMP_DIV		8560
#define	A64_CLK_RATE		4000000

#define	TEMP_C_TO_K		273
#define	SENSOR_ENABLE_ALL	(SENSOR0_EN|SENSOR1_EN|SENSOR2_EN)
#define	SHUT_INT_ALL		(SHUT_INT0_STS|SHUT_INT1_STS|SHUT_INT2_STS)

#define	MAX_SENSORS	3

struct aw_thermal_sensor {
	const char		*name;
	const char		*desc;
};

struct aw_thermal_config {
	struct aw_thermal_sensor	sensors[MAX_SENSORS];
	int				nsensors;
	uint64_t			clk_rate;
	uint32_t			adc_acquire_time;
	uint32_t			filter;
	uint32_t			intc;
	uint32_t			temp_base;
	uint32_t			temp_div;
};

static const struct aw_thermal_config a83t_config = {
	.nsensors = 3,
	.sensors = {
		[0] = {
			.name = "cluster0",
			.desc = "CPU cluster 0 temperature",
		},
		[1] = {
			.name = "cluster1",
			.desc = "CPU cluster 1 temperature",
		},
		[2] = {
			.name = "gpu",
			.desc = "GPU temperature",
		},
	},
	.clk_rate = A83T_CLK_RATE,
	.adc_acquire_time = A83T_ADC_ACQUIRE_TIME,
	.filter = A83T_FILTER,
	.intc = A83T_INTC,
	.temp_base = A83T_TEMP_BASE,
	.temp_div = A83T_TEMP_DIV,
};

static const struct aw_thermal_config a64_config = {
	.nsensors = 3,
	.sensors = {
		[0] = {
			.name = "cpu",
			.desc = "CPU temperature",
		},
		[1] = {
			.name = "gpu1",
			.desc = "GPU temperature 1",
		},
		[2] = {
			.name = "gpu2",
			.desc = "GPU temperature 2",
		},
	},
	.clk_rate = A64_CLK_RATE,
	.adc_acquire_time = A64_ADC_ACQUIRE_TIME,
	.filter = A64_FILTER,
	.intc = A64_INTC,
	.temp_base = A64_TEMP_BASE,
	.temp_div = A64_TEMP_DIV,
};

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun8i-a83t-ts",	(uintptr_t)&a83t_config },
	{ "allwinner,sun50i-a64-ts",	(uintptr_t)&a64_config },
	{ NULL,				(uintptr_t)NULL }
};

#define	THS_CONF(d)		\
	(void *)ofw_bus_search_compatible((d), compat_data)->ocd_data

struct aw_thermal_softc {
	struct resource			*res[2];
	struct aw_thermal_config	*conf;
};

static struct resource_spec aw_thermal_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

#define	RD4(sc, reg)		bus_read_4((sc)->res[0], (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res[0], (reg), (val))

static int
aw_thermal_init(struct aw_thermal_softc *sc)
{
	uint32_t calib0, calib1;
	int error;

	/* Read calibration settings from SRAM */
	error = aw_sid_read_tscalib(&calib0, &calib1);
	if (error != 0)
		return (error);

	/* Write calibration settings to thermal controller */
	WR4(sc, THS_CALIB0, calib0);
	WR4(sc, THS_CALIB1, calib1);

	/* Configure ADC acquire time (CLK_IN/(N+1)) and enable sensors */
	WR4(sc, THS_CTRL1, ADC_CALI_EN);
	WR4(sc, THS_CTRL0, sc->conf->adc_acquire_time);
	WR4(sc, THS_CTRL2, sc->conf->adc_acquire_time << SENSOR_ACQ1_SHIFT);

	/* Enable average filter */
	WR4(sc, THS_FILTER, sc->conf->filter);

	/* Enable interrupts */
	WR4(sc, THS_INTS, RD4(sc, THS_INTS));
	WR4(sc, THS_INTC, sc->conf->intc | SHUT_INT_ALL);

	/* Enable sensors */
	WR4(sc, THS_CTRL2, RD4(sc, THS_CTRL2) | SENSOR_ENABLE_ALL);

	return (0);
}

static int
aw_thermal_reg_to_temp(struct aw_thermal_softc *sc, uint32_t val)
{
	return ((sc->conf->temp_base - val * 1000) / sc->conf->temp_div);
}

static int
aw_thermal_gettemp(struct aw_thermal_softc *sc, int sensor)
{
	uint32_t val;

	val = RD4(sc, THS_DATA0 + (sensor * 4));

	return (aw_thermal_reg_to_temp(sc, val) + TEMP_C_TO_K);
}

static int
aw_thermal_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct aw_thermal_softc *sc;
	int sensor, val;

	sc = arg1;
	sensor = arg2;

	val = aw_thermal_gettemp(sc, sensor);

	return sysctl_handle_opaque(oidp, &val, sizeof(val), req);
}

static void
aw_thermal_intr(void *arg)
{
	struct aw_thermal_softc *sc;
	device_t dev;
	uint32_t ints;

	dev = arg;
	sc = device_get_softc(dev);

	ints = RD4(sc, THS_INTS);
	WR4(sc, THS_INTS, ints);

	if ((ints & SHUT_INT_ALL) != 0) {
		device_printf(dev,
		   "WARNING - current temperature exceeds safe limits\n");
		shutdown_nice(RB_POWEROFF);
	}
}

static int
aw_thermal_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (THS_CONF(dev) == NULL)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Thermal Sensor Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_thermal_attach(device_t dev)
{
	struct aw_thermal_softc *sc;
	clk_t clk_ahb, clk_ths;
	hwreset_t rst;
	int i, error;
	void *ih;

	sc = device_get_softc(dev);
	clk_ahb = clk_ths = NULL;
	rst = NULL;
	ih = NULL;

	sc->conf = THS_CONF(dev);

	if (bus_alloc_resources(dev, aw_thermal_spec, sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	if (clk_get_by_ofw_name(dev, 0, "ahb", &clk_ahb) == 0) {
		error = clk_enable(clk_ahb);
		if (error != 0) {
			device_printf(dev, "cannot enable ahb clock\n");
			goto fail;
		}
	}
	if (clk_get_by_ofw_name(dev, 0, "ths", &clk_ths) == 0) {
		error = clk_set_freq(clk_ths, sc->conf->clk_rate, 0);
		if (error != 0) {
			device_printf(dev, "cannot set ths clock rate\n");
			goto fail;
		}
		error = clk_enable(clk_ths);
		if (error != 0) {
			device_printf(dev, "cannot enable ths clock\n");
			goto fail;
		}
	}
	if (hwreset_get_by_ofw_idx(dev, 0, 0, &rst) == 0) {
		error = hwreset_deassert(rst);
		if (error != 0) {
			device_printf(dev, "cannot de-assert reset\n");
			goto fail;
		}
	}

	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, aw_thermal_intr, dev, &ih);
	if (error != 0) {
		device_printf(dev, "cannot setup interrupt handler\n");
		goto fail;
	}

	if (aw_thermal_init(sc) != 0)
		goto fail;

	for (i = 0; i < sc->conf->nsensors; i++)
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, sc->conf->sensors[i].name,
		    CTLTYPE_INT | CTLFLAG_RD,
		    sc, i, aw_thermal_sysctl, "IK0",
		    sc->conf->sensors[i].desc);

	return (0);

fail:
	if (ih != NULL)
		bus_teardown_intr(dev, sc->res[1], ih);
	if (rst != NULL)
		hwreset_release(rst);
	if (clk_ahb != NULL)
		clk_release(clk_ahb);
	if (clk_ths != NULL)
		clk_release(clk_ths);
	bus_release_resources(dev, aw_thermal_spec, sc->res);

	return (ENXIO);
}

static device_method_t aw_thermal_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_thermal_probe),
	DEVMETHOD(device_attach,	aw_thermal_attach),

	DEVMETHOD_END
};

static driver_t aw_thermal_driver = {
	"aw_thermal",
	aw_thermal_methods,
	sizeof(struct aw_thermal_softc),
};

static devclass_t aw_thermal_devclass;

DRIVER_MODULE(aw_thermal, simplebus, aw_thermal_driver, aw_thermal_devclass,
    0, 0);
MODULE_VERSION(aw_thermal, 1);
