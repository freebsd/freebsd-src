/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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

/*
 * Thermometer and thermal zones driver for RockChip SoCs.
 * Calibration data are taken from Linux, because this part of SoC
 * is undocumented in TRM.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/syscon/syscon.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "syscon_if.h"
#include "rk_tsadc_if.h"

/* Global registers */
#define	TSADC_USER_CON				0x000
#define	TSADC_AUTO_CON				0x004
#define	 TSADC_AUTO_CON_POL_HI				(1 << 8)
#define	 TSADC_AUTO_SRC_EN(x)				(1 << (4 + (x)))
#define	 TSADC_AUTO_Q_SEL				(1 << 1) /* V3 only */
#define	 TSADC_AUTO_CON_AUTO				(1 << 0)

#define	TSADC_INT_EN				0x008
#define	 TSADC_INT_EN_2CRU_EN_SRC(x)			(1 << (8 + (x)))
#define	 TSADC_INT_EN_2GPIO_EN_SRC(x)			(1 << (4 + (x)))
#define	TSADC_INT_PD				0x00c
#define	TSADC_DATA(x)				(0x20 + (x) * 0x04)
#define	TSADC_COMP_INT(x)			(0x30 + (x) * 0x04)
#define	 TSADC_COMP_INT_SRC_EN(x)			(1 << (0 + (x)))
#define	TSADC_COMP_SHUT(x)			(0x40 + (x) * 0x04)
#define	TSADC_HIGHT_INT_DEBOUNCE		0x060
#define	TSADC_HIGHT_TSHUT_DEBOUNCE		0x064
#define	TSADC_AUTO_PERIOD			0x068
#define	TSADC_AUTO_PERIOD_HT			0x06c
#define	TSADC_COMP0_LOW_INT			0x080	/* V3 only */
#define	TSADC_COMP1_LOW_INT			0x084	/* V3 only */

/* GFR Bits */
#define	GRF_SARADC_TESTBIT			0x0e644
#define	 GRF_SARADC_TESTBIT_ON				(0x10001 << 2)
#define GRF_TSADC_TESTBIT_L			0x0e648
#define	 GRF_TSADC_VCM_EN_L				(0x10001 << 7)
#define	GRF_TSADC_TESTBIT_H			0x0e64c
#define	 GRF_TSADC_VCM_EN_H				(0x10001 << 7)
#define	 GRF_TSADC_TESTBIT_H_ON				(0x10001 << 2)

#define	WR4(_sc, _r, _v)	bus_write_4((_sc)->mem_res, (_r), (_v))
#define	RD4(_sc, _r)		bus_read_4((_sc)->mem_res, (_r))

static struct sysctl_ctx_list tsadc_sysctl_ctx;

struct tsensor {
	char 			*name;
	int			id;
	int			channel;
};

struct rk_calib_entry {
	uint32_t	raw;
	int		temp;
};

struct tsadc_calib_info {
	struct rk_calib_entry	*table;
	int			nentries;
};

struct tsadc_conf {
	int			use_syscon;
	int			q_sel_ntc;
	int			shutdown_temp;
	int			shutdown_mode;
	int			shutdown_pol;
	struct tsensor		*tsensors;
	int			ntsensors;
	struct tsadc_calib_info	calib_info;
};

struct tsadc_softc {
	device_t		dev;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*irq_ih;

	clk_t			tsadc_clk;
	clk_t			apb_pclk_clk;
	hwreset_array_t		hwreset;
	struct syscon		*grf;

	struct tsadc_conf	*conf;

	int			shutdown_temp;
	int			shutdown_mode;
	int			shutdown_pol;

	int			alarm_temp;
};

static struct rk_calib_entry rk3288_calib_data[] = {
	{3800, -40000},
	{3792, -35000},
	{3783, -30000},
	{3774, -25000},
	{3765, -20000},
	{3756, -15000},
	{3747, -10000},
	{3737, -5000},
	{3728, 0},
	{3718, 5000},
	{3708, 10000},
	{3698, 15000},
	{3688, 20000},
	{3678, 25000},
	{3667, 30000},
	{3656, 35000},
	{3645, 40000},
	{3634, 45000},
	{3623, 50000},
	{3611, 55000},
	{3600, 60000},
	{3588, 65000},
	{3575, 70000},
	{3563, 75000},
	{3550, 80000},
	{3537, 85000},
	{3524, 90000},
	{3510, 95000},
	{3496, 100000},
	{3482, 105000},
	{3467, 110000},
	{3452, 115000},
	{3437, 120000},
	{3421, 125000},
};

struct tsensor rk3288_tsensors[] = {
	{ .channel = 0, .id = 2, .name = "reserved"},
	{ .channel = 1, .id = 0, .name = "CPU"},
	{ .channel = 2, .id = 1, .name = "GPU"},
};

struct tsadc_conf rk3288_tsadc_conf = {
	.use_syscon =		0,
	.q_sel_ntc =		0,
	.shutdown_temp =	95000,
	.shutdown_mode =	1, /* GPIO */
	.shutdown_pol =		0, /* Low  */
	.tsensors = 		rk3288_tsensors,
	.ntsensors = 		nitems(rk3288_tsensors),
	.calib_info = 	{
			.table = rk3288_calib_data,
			.nentries = nitems(rk3288_calib_data),
	}
};

static struct rk_calib_entry rk3328_calib_data[] = {
	{296, -40000},
	{304, -35000},
	{313, -30000},
	{331, -20000},
	{340, -15000},
	{349, -10000},
	{359, -5000},
	{368, 0},
	{378, 5000},
	{388, 10000},
	{398, 15000},
	{408, 20000},
	{418, 25000},
	{429, 30000},
	{440, 35000},
	{451, 40000},
	{462, 45000},
	{473, 50000},
	{485, 55000},
	{496, 60000},
	{508, 65000},
	{521, 70000},
	{533, 75000},
	{546, 80000},
	{559, 85000},
	{572, 90000},
	{586, 95000},
	{600, 100000},
	{614, 105000},
	{629, 110000},
	{644, 115000},
	{659, 120000},
	{675, 125000},
};

static struct tsensor rk3328_tsensors[] = {
	{ .channel = 0, .id = 0, .name = "CPU"},
};

static struct tsadc_conf rk3328_tsadc_conf = {
	.use_syscon =		0,
	.q_sel_ntc =		1,
	.shutdown_temp =	95000,
	.shutdown_mode =	0, /* CRU */
	.shutdown_pol =		0, /* Low  */
	.tsensors = 		rk3328_tsensors,
	.ntsensors = 		nitems(rk3328_tsensors),
	.calib_info = 	{
			.table = rk3328_calib_data,
			.nentries = nitems(rk3328_calib_data),
	}
};

static struct rk_calib_entry rk3399_calib_data[] = {
	{402, -40000},
	{410, -35000},
	{419, -30000},
	{427, -25000},
	{436, -20000},
	{444, -15000},
	{453, -10000},
	{461, -5000},
	{470, 0},
	{478, 5000},
	{487, 10000},
	{496, 15000},
	{504, 20000},
	{513, 25000},
	{521, 30000},
	{530, 35000},
	{538, 40000},
	{547, 45000},
	{555, 50000},
	{564, 55000},
	{573, 60000},
	{581, 65000},
	{590, 70000},
	{599, 75000},
	{607, 80000},
	{616, 85000},
	{624, 90000},
	{633, 95000},
	{642, 100000},
	{650, 105000},
	{659, 110000},
	{668, 115000},
	{677, 120000},
	{685, 125000},
};

static struct tsensor rk3399_tsensors[] = {
	{ .channel = 0, .id = 0, .name = "CPU"},
	{ .channel = 1, .id = 1, .name = "GPU"},
};

static struct tsadc_conf rk3399_tsadc_conf = {
	.use_syscon =		1,
	.q_sel_ntc =		1,
	.shutdown_temp =	95000,
	.shutdown_mode =	1, /* GPIO */
	.shutdown_pol =		0, /* Low  */
	.tsensors = 		rk3399_tsensors,
	.ntsensors = 		nitems(rk3399_tsensors),
	.calib_info = 	{
			.table = rk3399_calib_data,
			.nentries = nitems(rk3399_calib_data),
	}
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3288-tsadc",	(uintptr_t)&rk3288_tsadc_conf},
	{"rockchip,rk3328-tsadc",	(uintptr_t)&rk3328_tsadc_conf},
	{"rockchip,rk3399-tsadc",	(uintptr_t)&rk3399_tsadc_conf},
	{NULL,		0}
};

static uint32_t
tsadc_temp_to_raw(struct tsadc_softc *sc, int temp)
{
	struct rk_calib_entry *tbl;
	int denom, ntbl, raw, i;

	tbl = sc->conf->calib_info.table;
	ntbl = sc->conf->calib_info.nentries;

	if (temp <= tbl[0].temp)
		return (tbl[0].raw);

	if (temp >= tbl[ntbl - 1].temp)
		return (tbl[ntbl - 1].raw);

	for (i = 1; i < (ntbl - 1); i++) {
		/* Exact match */
		if (temp == tbl[i].temp)
			return (tbl[i].raw);
		if (temp < tbl[i].temp)
			break;
	}

	/*
	* Translated value is between i and i - 1 table entries.
	* Do linear interpolation for it.
	*/
	raw = (int)tbl[i - 1].raw - (int)tbl[i].raw;
	raw *= temp - tbl[i - 1].temp;
	denom = tbl[i - 1].temp - tbl[i].temp;
	raw = tbl[i - 1].raw + raw / denom;
	return (raw);
}

static int
tsadc_raw_to_temp(struct tsadc_softc *sc, uint32_t raw)
{
	struct rk_calib_entry *tbl;
	int denom, ntbl, temp, i;
	bool descending;

	tbl = sc->conf->calib_info.table;
	ntbl = sc->conf->calib_info.nentries;
	descending = tbl[0].raw > tbl[1].raw;

	if (descending) {
		/* Raw column is in descending order. */
		if (raw >= tbl[0].raw)
			return (tbl[0].temp);
		if (raw <= tbl[ntbl - 1].raw)
			return (tbl[ntbl - 1].temp);

		for (i = ntbl - 2; i > 0; i--) {
			/* Exact match */
			if (raw == tbl[i].raw)
				return (tbl[i].temp);
			if (raw < tbl[i].raw)
				break;
		}
	} else {
		/* Raw column is in ascending order. */
		if (raw <= tbl[0].raw)
			return (tbl[0].temp);
		if (raw >= tbl[ntbl - 1].raw)
			return (tbl[ntbl - 1].temp);
		for (i = 1; i < (ntbl - 1); i++) {
			/* Exact match */
			if (raw == tbl[i].raw)
				return (tbl[i].temp);
			if (raw < tbl[i].raw)
				break;
		}
	}

	/*
	* Translated value is between i and i - 1 table entries.
	* Do linear interpolation for it.
	*/
	temp  = (int)tbl[i - 1].temp - (int)tbl[i].temp;
	temp *= raw - tbl[i - 1].raw;
	denom = tbl[i - 1].raw - tbl[i].raw;
	temp = tbl[i - 1].temp + temp / denom;
	return (temp);
}

static void
tsadc_init_tsensor(struct tsadc_softc *sc, struct tsensor *sensor)
{
	uint32_t val;

	/* Shutdown mode */
	val = RD4(sc, TSADC_INT_EN);
	if (sc->shutdown_mode != 0) {
		/* Signal shutdown of GPIO pin */
		val &= ~TSADC_INT_EN_2CRU_EN_SRC(sensor->channel);
		val |= TSADC_INT_EN_2GPIO_EN_SRC(sensor->channel);
	} else {
		val |= TSADC_INT_EN_2CRU_EN_SRC(sensor->channel);
		val &= ~TSADC_INT_EN_2GPIO_EN_SRC(sensor->channel);
	}
	WR4(sc, TSADC_INT_EN, val);

	/* Shutdown temperature */
	val =  tsadc_raw_to_temp(sc, sc->shutdown_temp);
	WR4(sc, TSADC_COMP_SHUT(sensor->channel), val);
	val = RD4(sc, TSADC_AUTO_CON);
	val |= TSADC_AUTO_SRC_EN(sensor->channel);
	WR4(sc, TSADC_AUTO_CON, val);

	/* Alarm temperature */
	val =  tsadc_temp_to_raw(sc, sc->alarm_temp);
	WR4(sc, TSADC_COMP_INT(sensor->channel), val);
	val = RD4(sc, TSADC_INT_EN);
	val |= TSADC_COMP_INT_SRC_EN(sensor->channel);
	WR4(sc, TSADC_INT_EN, val);
}

static void
tsadc_init(struct tsadc_softc *sc)
{
	uint32_t val;

	/* Common part */
	val = 0;	/* XXX Is this right? */
	if (sc->shutdown_pol != 0)
		val |= TSADC_AUTO_CON_POL_HI;
	else
		val &= ~TSADC_AUTO_CON_POL_HI;
	if (sc->conf->q_sel_ntc)
		val |= TSADC_AUTO_Q_SEL;
	WR4(sc, TSADC_AUTO_CON, val);

	if (!sc->conf->use_syscon) {
		/* V2 init */
		WR4(sc, TSADC_AUTO_PERIOD, 250); 	/* 250 ms */
		WR4(sc, TSADC_AUTO_PERIOD_HT, 50);	/*  50 ms */
		WR4(sc, TSADC_HIGHT_INT_DEBOUNCE, 4);
		WR4(sc, TSADC_HIGHT_TSHUT_DEBOUNCE, 4);
	} else {
		/* V3 init */
		if (sc->grf == NULL) {
			/* Errata: adjust interleave to working value */
			WR4(sc, TSADC_USER_CON, 13 << 6); 	/* 13 clks */
		} else {
			SYSCON_WRITE_4(sc->grf, GRF_TSADC_TESTBIT_L,
			    GRF_TSADC_VCM_EN_L);
			SYSCON_WRITE_4(sc->grf, GRF_TSADC_TESTBIT_H,
			    GRF_TSADC_VCM_EN_H);
			DELAY(30);  /* 15 usec min */

			SYSCON_WRITE_4(sc->grf, GRF_SARADC_TESTBIT,
			    GRF_SARADC_TESTBIT_ON);
			SYSCON_WRITE_4(sc->grf, GRF_TSADC_TESTBIT_H,
			    GRF_TSADC_TESTBIT_H_ON);
			DELAY(180);  /* 90 usec min */
		}
		WR4(sc, TSADC_AUTO_PERIOD, 1875); 	/* 2.5 ms */
		WR4(sc, TSADC_AUTO_PERIOD_HT, 1875);	/* 2.5 ms */
		WR4(sc, TSADC_HIGHT_INT_DEBOUNCE, 4);
		WR4(sc, TSADC_HIGHT_TSHUT_DEBOUNCE, 4);
	}
}

static int
tsadc_read_temp(struct tsadc_softc *sc, struct tsensor *sensor, int *temp)
{
	uint32_t val;

	val = RD4(sc, TSADC_DATA(sensor->channel));
	*temp = tsadc_raw_to_temp(sc, val);

#ifdef DEBUG
	printf("%s: Sensor(id: %d, ch: %d), temp: %d\n", __func__,
	    sensor->id, sensor->channel, *temp);
	printf(" status: 0x%08X, 0x%08X\n",
	    RD4(sc, TSADC_USER_CON),
	    RD4(sc, TSADC_AUTO_CON));
	printf(" Data: 0x%08X, 0x%08X, 0x%08X\n",
	    RD4(sc, TSADC_DATA(sensor->channel)),
	    RD4(sc, TSADC_COMP_INT(sensor->channel)),
	    RD4(sc, TSADC_COMP_SHUT(sensor->channel)));
#endif
	return (0);
}

static int
tsadc_get_temp(device_t dev, device_t cdev, uintptr_t id, int *val)
{
	struct tsadc_softc *sc;
	int i, rv;

	sc = device_get_softc(dev);

	if (id >= sc->conf->ntsensors)
		return (ERANGE);

	for (i = 0; i < sc->conf->ntsensors; i++) {
		if (sc->conf->tsensors->id == id) {
			rv =tsadc_read_temp(sc, sc->conf->tsensors + id, val);
			return (rv);
		}
	}
	return (ERANGE);
}

static int
tsadc_sysctl_temperature(SYSCTL_HANDLER_ARGS)
{
	struct tsadc_softc *sc;
	int val;
	int rv;
	int id;

	/* Write request */
	if (req->newptr != NULL)
		return (EINVAL);

	sc = arg1;
	id = arg2;

	if (id >= sc->conf->ntsensors)
		return (ERANGE);
	rv =  tsadc_read_temp(sc, sc->conf->tsensors + id, &val);
	if (rv != 0)
		return (rv);

	val = val / 100;
	val +=  2731;
	rv = sysctl_handle_int(oidp, &val, 0, req);
	return (rv);
}

static int
tsadc_init_sysctl(struct tsadc_softc *sc)
{
	int i;
	struct sysctl_oid *oid, *tmp;

	sysctl_ctx_init(&tsadc_sysctl_ctx);
	/* create node for hw.temp */
	oid = SYSCTL_ADD_NODE(&tsadc_sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, "temperature",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	if (oid == NULL)
		return (ENXIO);

	/* Add sensors */
	for (i = sc->conf->ntsensors  - 1; i >= 0; i--) {
		tmp = SYSCTL_ADD_PROC(&tsadc_sysctl_ctx,
		    SYSCTL_CHILDREN(oid), OID_AUTO, sc->conf->tsensors[i].name,
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc, i,
		    tsadc_sysctl_temperature, "IK", "SoC Temperature");
		if (tmp == NULL)
			return (ENXIO);
	}

	return (0);
}

static int
tsadc_intr(void *arg)
{
	struct tsadc_softc *sc;
	uint32_t val;

	sc = (struct tsadc_softc *)arg;

	val = RD4(sc, TSADC_INT_PD);
	WR4(sc, TSADC_INT_PD, val);

	/* XXX Handle shutdown and alarm interrupts. */
	if (val & 0x00F0) {
		device_printf(sc->dev, "Alarm: device temperature "
		    "is above of shutdown level.\n");
	} else if (val & 0x000F) {
		device_printf(sc->dev, "Alarm: device temperature "
		    "is above of alarm level.\n");
	}
	return (FILTER_HANDLED);
}

static int
tsadc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip temperature sensors");
	return (BUS_PROBE_DEFAULT);
}

static int
tsadc_attach(device_t dev)
{
	struct tsadc_softc *sc;
	phandle_t node;
	uint32_t val;
	int i, rid, rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(sc->dev);
	sc->conf = (struct tsadc_conf *)
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	sc->alarm_temp = 90000;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		goto fail;
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resources\n");
		goto fail;
	}

	if ((bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    tsadc_intr, NULL, sc, &sc->irq_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		goto fail;
	}

	/* FDT resources */
	rv = hwreset_array_get_ofw(dev, 0, &sc->hwreset);
	if (rv != 0) {
		device_printf(dev, "Cannot get resets\n");
		goto fail;
	}
	rv = clk_get_by_ofw_name(dev, 0, "tsadc", &sc->tsadc_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'tsadc' clock: %d\n", rv);
		goto fail;
	}
	rv = clk_get_by_ofw_name(dev, 0, "apb_pclk", &sc->apb_pclk_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'apb_pclk' clock: %d\n", rv);
		goto fail;
	}

	/* grf is optional */
	rv = syscon_get_by_ofw_property(dev, node, "rockchip,grf", &sc->grf);
	if (rv != 0 && rv != ENOENT) {
		device_printf(dev, "Cannot get 'grf' syscon: %d\n", rv);
		goto fail;
	}

	rv = OF_getencprop(node, "rockchip,hw-tshut-temp",
	    &sc->shutdown_temp, sizeof(sc->shutdown_temp));
	if (rv <= 0)
		sc->shutdown_temp = sc->conf->shutdown_temp;

	rv = OF_getencprop(node, "rockchip,hw-tshut-mode",
	    &sc->shutdown_mode, sizeof(sc->shutdown_mode));
	if (rv <= 0)
		sc->shutdown_mode = sc->conf->shutdown_mode;

	rv = OF_getencprop(node, "rockchip,hw-tshut-polarity",
	    &sc->shutdown_pol, sizeof(sc->shutdown_pol));
	if (rv <= 0)
		sc->shutdown_pol = sc->conf->shutdown_pol;

	/* Wakeup controller */
	rv = hwreset_array_assert(sc->hwreset);
	if (rv != 0) {
		device_printf(dev, "Cannot assert reset\n");
		goto fail;
	}

	/* Set the assigned clocks parent and freq */
	rv = clk_set_assigned(sc->dev, node);
	if (rv != 0 && rv != ENOENT) {
		device_printf(dev, "clk_set_assigned failed\n");
		goto fail;
	}

	rv = clk_enable(sc->tsadc_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot enable 'tsadc_clk' clock: %d\n", rv);
		goto fail;
	}
	rv = clk_enable(sc->apb_pclk_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot enable 'apb_pclk' clock: %d\n", rv);
		goto fail;
	}
	rv = hwreset_array_deassert(sc->hwreset);
	if (rv != 0) {
		device_printf(dev, "Cannot deassert reset\n");
		goto fail;
	}

	tsadc_init(sc);
	for (i = 0; i < sc->conf->ntsensors; i++)
		tsadc_init_tsensor(sc, sc->conf->tsensors + i);

	/* Enable auto mode */
	val = RD4(sc, TSADC_AUTO_CON);
	val |= TSADC_AUTO_CON_AUTO;
	WR4(sc, TSADC_AUTO_CON, val);

	rv = tsadc_init_sysctl(sc);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot initialize sysctls\n");
		goto fail_sysctl;
	}

	OF_device_register_xref(OF_xref_from_node(node), dev);
	return (bus_generic_attach(dev));

fail_sysctl:
	sysctl_ctx_free(&tsadc_sysctl_ctx);
fail:
	if (sc->irq_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_ih);
	if (sc->tsadc_clk != NULL)
		clk_release(sc->tsadc_clk);
	if (sc->apb_pclk_clk != NULL)
		clk_release(sc->apb_pclk_clk);
	if (sc->hwreset != NULL)
		hwreset_array_release(sc->hwreset);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (ENXIO);
}

static int
tsadc_detach(device_t dev)
{
	struct tsadc_softc *sc;
	sc = device_get_softc(dev);

	if (sc->irq_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_ih);
	sysctl_ctx_free(&tsadc_sysctl_ctx);
	if (sc->tsadc_clk != NULL)
		clk_release(sc->tsadc_clk);
	if (sc->apb_pclk_clk != NULL)
		clk_release(sc->apb_pclk_clk);
	if (sc->hwreset != NULL)
		hwreset_array_release(sc->hwreset);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (ENXIO);
}

static device_method_t rk_tsadc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			tsadc_probe),
	DEVMETHOD(device_attach,		tsadc_attach),
	DEVMETHOD(device_detach,		tsadc_detach),

	/* TSADC interface */
	DEVMETHOD(rk_tsadc_get_temperature,	tsadc_get_temp),

	DEVMETHOD_END
};

static devclass_t rk_tsadc_devclass;
static DEFINE_CLASS_0(rk_tsadc, rk_tsadc_driver, rk_tsadc_methods,
    sizeof(struct tsadc_softc));
EARLY_DRIVER_MODULE(rk_tsadc, simplebus, rk_tsadc_driver,
    rk_tsadc_devclass, NULL, NULL, BUS_PASS_TIMER + BUS_PASS_ORDER_LAST);
