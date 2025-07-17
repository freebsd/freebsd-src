/*-
 * Copyright (c) 2016 Michal Meloun <mmel@FreeBSD.org>
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
/*
 * Thermometer and thermal zones driver for Tegra SoCs.
 * Calibration data and algo are taken from Linux, because this part of SoC
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

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/nvidia/tegra_efuse.h>
#include <dt-bindings/thermal/tegra124-soctherm.h>
#include "tegra_soctherm_if.h"

/* Per sensors registers - base is 0x0c0*/
#define	TSENSOR_CONFIG0				0x000
#define	 TSENSOR_CONFIG0_TALL(x)			(((x) & 0xFFFFF) << 8)
#define	 TSENSOR_CONFIG0_STATUS_CLR			(1 << 5)
#define	 TSENSOR_CONFIG0_TCALC_OVERFLOW			(1 << 4)
#define	 TSENSOR_CONFIG0_OVERFLOW			(1 << 3)
#define	 TSENSOR_CONFIG0_CPTR_OVERFLOW			(1 << 2)
#define	 TSENSOR_CONFIG0_RO_SEL				(1 << 1)
#define	 TSENSOR_CONFIG0_STOP				(1 << 0)

#define	TSENSOR_CONFIG1				0x004
#define	 TSENSOR_CONFIG1_TEMP_ENABLE			(1U << 31)
#define	 TSENSOR_CONFIG1_TEN_COUNT(x)			(((x) & 0x3F) << 24)
#define	 TSENSOR_CONFIG1_TIDDQ_EN(x)			(((x) & 0x3F) << 15)
#define	 TSENSOR_CONFIG1_TSAMPLE(x)			(((x) & 0x3FF) << 0)

#define	TSENSOR_CONFIG2				0x008
#define	TSENSOR_CONFIG2_THERMA(x)			(((x) & 0xFFFF) << 16)
#define	TSENSOR_CONFIG2_THERMB(x)			(((x) & 0xFFFF) << 0)

#define	TSENSOR_STATUS0				0x00c
#define	 TSENSOR_STATUS0_CAPTURE_VALID			(1U << 31)
#define	 TSENSOR_STATUS0_CAPTURE(x)			(((x) >> 0) & 0xffff)

#define	TSENSOR_STATUS1				0x010
#define	 TSENSOR_STATUS1_TEMP_VALID			(1U << 31)
#define	 TSENSOR_STATUS1_TEMP(x)			(((x) >> 0) & 0xffff)

#define	TSENSOR_STATUS2				0x014
#define	 TSENSOR_STATUS2_TEMP_MAX(x)			(((x) >> 16) & 0xffff)
#define	 TSENSOR_STATUS2_TEMP_MIN(x)			(((x) >>  0) & 0xffff)


/* Readbacks */
#define	READBACK_VALUE(x)				(((x) >> 8) & 0xff)
#define	READBACK_ADD_HALF				(1 << 7)
#define	READBACK_NEGATE					(1 << 0)

/* Global registers */
#define	TSENSOR_PDIV				0x1c0
#define	TSENSOR_HOTSPOT_OFF			0x1c4
#define	TSENSOR_TEMP1				0x1c8
#define	TSENSOR_TEMP2				0x1cc

/* Fuses */
#define	 FUSE_TSENSOR_CALIB_CP_TS_BASE_SHIFT		0
#define	 FUSE_TSENSOR_CALIB_CP_TS_BASE_BITS		13
#define	 FUSE_TSENSOR_CALIB_FT_TS_BASE_SHIFT		13
#define	 FUSE_TSENSOR_CALIB_FT_TS_BASE_BITS		13

/* Layout is different for Tegra124 and Tegra210 */
#define	FUSE_TSENSOR_COMMON			0x180
#define	 TEGRA124_FUSE_COMMON_CP_TS_BASE(x)		(((x) >>  0) & 0x3ff)
#define	 TEGRA124_FUSE_COMMON_FT_TS_BASE(x)		(((x) >> 10) & 0x7ff)
#define	 TEGRA124_FUSE_COMMON_SHIFT_FT_SHIFT		21
#define	 TEGRA124_FUSE_COMMON_SHIFT_FT_BITS 		5

#define	 TEGRA210_FUSE_COMMON_CP_TS_BASE(x)		(((x) >>  11) & 0x3ff)
#define	 TEGRA210_FUSE_COMMON_FT_TS_BASE(x)		(((x) >> 21) & 0x7ff)
#define	 TEGRA210_FUSE_COMMON_SHIFT_CP_SHIFT		0
#define	 TEGRA210_FUSE_COMMON_SHIFT_CP_BITS		6
#define	 TEGRA210_FUSE_COMMON_SHIFT_FT_SHIFT		6
#define	 TEGRA210_FUSE_COMMON_SHIFT_FT_BITS 		5


/* Only for Tegra124 */
#define	FUSE_SPARE_REALIGNMENT_REG		0x1fc
#define	 FUSE_SPARE_REALIGNMENT_REG_SHIFT_CP_SHIFT 	0
#define	 FUSE_SPARE_REALIGNMENT_REG_SHIFT_CP_BITS 	6

#define	TEGRA124_NOMINAL_CALIB_FT	105
#define	TEGRA124_NOMINAL_CALIB_CP	25

#define	TEGRA210_NOMINAL_CALIB_FT	105
#define	TEGRA210_NOMINAL_CALIB_CP	25

#define	WR4(_sc, _r, _v)	bus_write_4((_sc)->mem_res, (_r), (_v))
#define	RD4(_sc, _r)		bus_read_4((_sc)->mem_res, (_r))

static struct sysctl_ctx_list soctherm_sysctl_ctx;

struct tsensor_cfg {
	uint32_t		tall;
	uint32_t		tsample;
	uint32_t		tiddq_en;
	uint32_t		ten_count;
	uint32_t		pdiv;
	uint32_t		tsample_ate;
	uint32_t		pdiv_ate;
};

struct soctherm_shared_cal {
	uint32_t		base_cp;
	uint32_t		base_ft;
	int32_t			actual_temp_cp;
	int32_t			actual_temp_ft;
};

struct tsensor {
	char 			*name;
	int			id;
	bus_addr_t		sensor_base;
	bus_addr_t		calib_fuse;
	int 			fuse_corr_alpha;
	int			fuse_corr_beta;

	int16_t			therm_a;
	int16_t			therm_b;
};

struct soctherm_soc;
struct soctherm_softc {
	device_t		dev;
	struct resource		*mem_res;
	struct resource		*irq_res;
	void			*irq_ih;

	clk_t			tsensor_clk;
	clk_t			soctherm_clk;
	hwreset_t		reset;

	struct soctherm_soc	*soc;
	struct soctherm_shared_cal shared_cal;
};

struct soctherm_soc {
	void			(*shared_cal)(struct soctherm_softc *sc);
	uint32_t		tsensor_pdiv;
	uint32_t		tsensor_hotspot_off;
	struct tsensor_cfg	*tsensor_cfg;
	struct tsensor		*tsensors;
	int			ntsensors;
};

/* Tegra124 config */

static struct tsensor_cfg t124_tsensor_config = {
	.tall = 16300,
	.tsample = 120,
	.tiddq_en = 1,
	.ten_count = 1,
	.pdiv = 8,
	.tsample_ate = 480,
	.pdiv_ate = 8
};

static struct tsensor t124_tsensors[] = {
	{
		.name = "cpu0",
		.id = TEGRA124_SOCTHERM_SENSOR_CPU,
		.sensor_base = 0x0c0,
		.calib_fuse = 0x098,
		.fuse_corr_alpha = 1135400,
		.fuse_corr_beta = -6266900,
	},
	{
		.name = "cpu1",
		.id = -1,
		.sensor_base = 0x0e0,
		.calib_fuse = 0x084,
		.fuse_corr_alpha = 1122220,
		.fuse_corr_beta = -5700700,
	},
	{
		.name = "cpu2",
		.id = -1,
		.sensor_base = 0x100,
		.calib_fuse = 0x088,
		.fuse_corr_alpha = 1127000,
		.fuse_corr_beta = -6768200,
	},
	{
		.name = "cpu3",
		.id = -1,
		.sensor_base = 0x120,
		.calib_fuse = 0x12c,
		.fuse_corr_alpha = 1110900,
		.fuse_corr_beta = -6232000,
	},
	{
		.name = "mem0",
		.id = TEGRA124_SOCTHERM_SENSOR_MEM,
		.sensor_base = 0x140,
		.calib_fuse = 0x158,
		.fuse_corr_alpha = 1122300,
		.fuse_corr_beta = -5936400,
	},
	{
		.name = "mem1",
		.id = -1,
		.sensor_base = 0x160,
		.calib_fuse = 0x15c,
		.fuse_corr_alpha = 1145700,
		.fuse_corr_beta = -7124600,
	},
	{
		.name = "gpu",
		.id = TEGRA124_SOCTHERM_SENSOR_GPU,
		.sensor_base = 0x180,
		.calib_fuse = 0x154,
		.fuse_corr_alpha = 1120100,
		.fuse_corr_beta = -6000500,
	},
	{
		.name = "pllX",
		.id = TEGRA124_SOCTHERM_SENSOR_PLLX,
		.sensor_base = 0x1a0,
		.calib_fuse = 0x160,
		.fuse_corr_alpha = 1106500,
		.fuse_corr_beta = -6729300,
	},
};

static void tegra124_shared_cal(struct soctherm_softc *sc);

static struct soctherm_soc tegra124_soc = {
	.shared_cal = tegra124_shared_cal,
	.tsensor_pdiv = 0x8888,
	.tsensor_hotspot_off = 0x00060600 ,
	.tsensor_cfg = &t124_tsensor_config,
	.tsensors = t124_tsensors,
	.ntsensors = nitems(t124_tsensors),
};

/* Tegra210 config */
static struct tsensor_cfg t210_tsensor_config = {
	.tall = 16300,
	.tsample = 120,
	.tiddq_en = 1,
	.ten_count = 1,
	.pdiv = 8,
	.tsample_ate = 480,
	.pdiv_ate = 8
};

static struct tsensor t210_tsensors[] = {
	{
		.name = "cpu0",
		.id = TEGRA124_SOCTHERM_SENSOR_CPU,
		.sensor_base = 0x0c0,
		.calib_fuse = 0x098,
		.fuse_corr_alpha = 1085000,
		.fuse_corr_beta = 3244200,
	},
	{
		.name = "cpu1",
		.id = -1,
		.sensor_base = 0x0e0,
		.calib_fuse = 0x084,
		.fuse_corr_alpha = 1126200,
		.fuse_corr_beta = -67500,
	},
	{
		.name = "cpu2",
		.id = -1,
		.sensor_base = 0x100,
		.calib_fuse = 0x088,
		.fuse_corr_alpha = 1098400,
		.fuse_corr_beta = 2251100,
	},
	{
		.name = "cpu3",
		.id = -1,
		.sensor_base = 0x120,
		.calib_fuse = 0x12c,
		.fuse_corr_alpha = 1108000,
		.fuse_corr_beta = 602700,
	},
	{
		.name = "mem0",
		.id = TEGRA124_SOCTHERM_SENSOR_MEM,
		.sensor_base = 0x140,
		.calib_fuse = 0x158,
		.fuse_corr_alpha = 1069200,
		.fuse_corr_beta = 3549900,
	},
	{
		.name = "mem1",
		.id = -1,
		.sensor_base = 0x160,
		.calib_fuse = 0x15c,
		.fuse_corr_alpha = 1173700,
		.fuse_corr_beta = -6263600,
	},
	{
		.name = "gpu",
		.id = TEGRA124_SOCTHERM_SENSOR_GPU,
		.sensor_base = 0x180,
		.calib_fuse = 0x154,
		.fuse_corr_alpha = 1074300,
		.fuse_corr_beta = 2734900,
	},
	{
		.name = "pllX",
		.id = TEGRA124_SOCTHERM_SENSOR_PLLX,
		.sensor_base = 0x1a0,
		.calib_fuse = 0x160,
		.fuse_corr_alpha = 1039700,
		.fuse_corr_beta = 6829100,
	},
};

static void tegra210_shared_cal(struct soctherm_softc *sc);

static struct soctherm_soc tegra210_soc = {
	.shared_cal = tegra210_shared_cal,
	.tsensor_pdiv = 0x8888,
	.tsensor_hotspot_off = 0x000A0500 ,
	.tsensor_cfg = &t210_tsensor_config,
	.tsensors = t210_tsensors,
	.ntsensors = nitems(t210_tsensors),
};

static struct ofw_compat_data compat_data[] = {
	{"nvidia,tegra124-soctherm", (uintptr_t)&tegra124_soc},
	{"nvidia,tegra210-soctherm", (uintptr_t)&tegra210_soc},
	{NULL,				0},
};

/* Extract signed integer bitfield from register */
static int
extract_signed(uint32_t reg, int shift, int bits)
{
	int32_t val;
	uint32_t mask;

	mask = (1 << bits) - 1;
	val = ((reg >> shift) & mask) << (32 - bits);
	val >>= 32 - bits;
	return ((int32_t)val);
}

static inline
int64_t div64_s64_precise(int64_t a, int64_t b)
{
	int64_t r, al;

	al = a << 16;
	r = (al * 2 + 1) / (2 * b);
	return (r >> 16);
}

static void
tegra124_shared_cal(struct soctherm_softc *sc)
{
	uint32_t val;
	int calib_cp, calib_ft;
	struct soctherm_shared_cal *cal;

	cal = &sc->shared_cal;
	val = tegra_fuse_read_4(FUSE_TSENSOR_COMMON);
	cal->base_cp = TEGRA124_FUSE_COMMON_CP_TS_BASE(val);
	cal->base_ft = TEGRA124_FUSE_COMMON_FT_TS_BASE(val);

	calib_ft = extract_signed(val,
	    TEGRA124_FUSE_COMMON_SHIFT_FT_SHIFT,
	    TEGRA124_FUSE_COMMON_SHIFT_FT_BITS);

	val = tegra_fuse_read_4(FUSE_SPARE_REALIGNMENT_REG);
	calib_cp = extract_signed(val,
	    FUSE_SPARE_REALIGNMENT_REG_SHIFT_CP_SHIFT,
	    FUSE_SPARE_REALIGNMENT_REG_SHIFT_CP_BITS);

	cal->actual_temp_cp = 2 * TEGRA124_NOMINAL_CALIB_CP + calib_cp;
	cal->actual_temp_ft = 2 * TEGRA124_NOMINAL_CALIB_FT + calib_ft;
#ifdef DEBUG
	printf("%s: base_cp: %u, base_ft: %d,"
	    " actual_temp_cp: %d, actual_temp_ft: %d\n",
	    __func__, cal->base_cp, cal->base_ft,
	    cal->actual_temp_cp, cal->actual_temp_ft);
#endif
}

static void
tegra210_shared_cal(struct soctherm_softc *sc)
{
	uint32_t val;
	int calib_cp, calib_ft;
	struct soctherm_shared_cal *cal;

	cal = &sc->shared_cal;

	val = tegra_fuse_read_4(FUSE_TSENSOR_COMMON);
	cal->base_cp = TEGRA210_FUSE_COMMON_CP_TS_BASE(val);
	cal->base_ft = TEGRA210_FUSE_COMMON_FT_TS_BASE(val);

	calib_ft = extract_signed(val,
	    TEGRA210_FUSE_COMMON_SHIFT_FT_SHIFT,
	    TEGRA210_FUSE_COMMON_SHIFT_FT_BITS);
	calib_cp = extract_signed(val,
	    TEGRA210_FUSE_COMMON_SHIFT_CP_SHIFT,
	    TEGRA210_FUSE_COMMON_SHIFT_CP_BITS);

	cal->actual_temp_cp = 2 * TEGRA210_NOMINAL_CALIB_CP + calib_cp;
	cal->actual_temp_ft = 2 * TEGRA210_NOMINAL_CALIB_FT + calib_ft;
#ifdef DEBUG
	printf("%s: base_cp: %u, base_ft: %d,"
	    " actual_temp_cp: %d, actual_temp_ft: %d\n",
	    __func__, cal->base_cp, cal->base_ft,
	    cal->actual_temp_cp, cal->actual_temp_ft);
#endif
}

static void
tsensor_calibration(struct soctherm_softc *sc, struct tsensor *sensor)
{
	uint32_t val;
	int mult, div, calib_cp, calib_ft;
	int actual_tsensor_ft, actual_tsensor_cp, delta_sens, delta_temp;
	int temp_a, temp_b;
	struct tsensor_cfg *cfg;
	struct soctherm_shared_cal *cal;
	int64_t tmp;

	cfg = sc->soc->tsensor_cfg;
	cal = &sc->shared_cal;

	val =  tegra_fuse_read_4(sensor->calib_fuse);
	calib_cp = extract_signed(val,
	    FUSE_TSENSOR_CALIB_CP_TS_BASE_SHIFT,
	    FUSE_TSENSOR_CALIB_CP_TS_BASE_BITS);
	actual_tsensor_cp = cal->base_cp * 64 + calib_cp;

	calib_ft = extract_signed(val,
	    FUSE_TSENSOR_CALIB_FT_TS_BASE_SHIFT,
	    FUSE_TSENSOR_CALIB_FT_TS_BASE_BITS);
	actual_tsensor_ft = cal->base_ft * 32 + calib_ft;

	delta_sens = actual_tsensor_ft - actual_tsensor_cp;
	delta_temp = cal->actual_temp_ft - cal->actual_temp_cp;
	mult = cfg->pdiv * cfg->tsample_ate;
	div = cfg->tsample * cfg->pdiv_ate;

	temp_a = div64_s64_precise((int64_t) delta_temp * (1LL << 13) * mult,
				   (int64_t) delta_sens * div);

	tmp = (int64_t)actual_tsensor_ft * cal->actual_temp_cp -
	      (int64_t)actual_tsensor_cp * cal->actual_temp_ft;
	temp_b = div64_s64_precise(tmp, (int64_t)delta_sens);

	temp_a = div64_s64_precise((int64_t)temp_a * sensor->fuse_corr_alpha,
				   1000000);
	temp_b = div64_s64_precise((int64_t)temp_b * sensor->fuse_corr_alpha +
				   sensor->fuse_corr_beta, 1000000);
	sensor->therm_a = (int16_t)temp_a;
	sensor->therm_b = (int16_t)temp_b;
#ifdef DEBUG
	printf("%s: sensor %s fuse: 0x%08X (0x%04X, 0x%04X)"
	    " calib_cp: %d(0x%04X), calib_ft: %d(0x%04X)\n",
	    __func__, sensor->name, val, val & 0x1FFF, (val >> 13) & 0x1FFF,
	    calib_cp, calib_cp, calib_ft, calib_ft);
	printf("therma: 0x%04X(%d), thermb: 0x%04X(%d)\n",
	    (uint16_t)sensor->therm_a, sensor->therm_a,
	    (uint16_t)sensor->therm_b, sensor->therm_b);
#endif
}

static void
soctherm_init_tsensor(struct soctherm_softc *sc, struct tsensor *sensor)
{
	struct tsensor_cfg *cfg;
	uint32_t val;

	cfg = sc->soc->tsensor_cfg;
	tsensor_calibration(sc, sensor);

	val = RD4(sc, sensor->sensor_base + TSENSOR_CONFIG0);
	val |= TSENSOR_CONFIG0_STOP;
	val |= TSENSOR_CONFIG0_STATUS_CLR;
	WR4(sc, sensor->sensor_base + TSENSOR_CONFIG0, val);

	val = TSENSOR_CONFIG0_TALL(cfg->tall);
	val |= TSENSOR_CONFIG0_STOP;
	WR4(sc, sensor->sensor_base + TSENSOR_CONFIG0, val);

	val = TSENSOR_CONFIG1_TSAMPLE(cfg->tsample - 1);
	val |= TSENSOR_CONFIG1_TIDDQ_EN(cfg->tiddq_en);
	val |= TSENSOR_CONFIG1_TEN_COUNT(cfg->ten_count);
	val |= TSENSOR_CONFIG1_TEMP_ENABLE;
	WR4(sc, sensor->sensor_base + TSENSOR_CONFIG1, val);

	val = TSENSOR_CONFIG2_THERMA((uint16_t)sensor->therm_a) |
	     TSENSOR_CONFIG2_THERMB((uint16_t)sensor->therm_b);
	WR4(sc, sensor->sensor_base + TSENSOR_CONFIG2, val);

	val = RD4(sc, sensor->sensor_base + TSENSOR_CONFIG0);
	val &= ~TSENSOR_CONFIG0_STOP;
	WR4(sc, sensor->sensor_base + TSENSOR_CONFIG0, val);
#ifdef DEBUG
	printf(" Sensor: %s  cfg:0x%08X, 0x%08X, 0x%08X,"
	    " sts:0x%08X, 0x%08X, 0x%08X\n", sensor->name,
	    RD4(sc, sensor->sensor_base + TSENSOR_CONFIG0),
	    RD4(sc, sensor->sensor_base + TSENSOR_CONFIG1),
	    RD4(sc, sensor->sensor_base + TSENSOR_CONFIG2),
	    RD4(sc, sensor->sensor_base + TSENSOR_STATUS0),
	    RD4(sc, sensor->sensor_base + TSENSOR_STATUS1),
	    RD4(sc, sensor->sensor_base + TSENSOR_STATUS2)
	    );
#endif
}

static int
soctherm_convert_raw(uint32_t val)
{
	int32_t t;

	t = READBACK_VALUE(val) * 1000;
	if (val & READBACK_ADD_HALF)
		t += 500;
	if (val & READBACK_NEGATE)
		t *= -1;

	return (t);
}

static int
soctherm_read_temp(struct soctherm_softc *sc, struct tsensor *sensor, int *temp)
{
	int timeout;
	uint32_t val;

	/* wait for valid sample */
	for (timeout = 100; timeout > 0; timeout--) {
		val = RD4(sc, sensor->sensor_base + TSENSOR_STATUS1);
		if ((val & TSENSOR_STATUS1_TEMP_VALID) != 0)
			break;
		DELAY(100);
	}
	if (timeout <= 0)
		device_printf(sc->dev, "Sensor %s timeouted\n", sensor->name);
	*temp = soctherm_convert_raw(val);
#ifdef DEBUG
	printf("%s: Raw: 0x%08X, temp: %d\n", __func__, val, *temp);
	printf(" Sensor: %s  cfg:0x%08X, 0x%08X, 0x%08X,"
	    " sts:0x%08X, 0x%08X, 0x%08X\n", sensor->name,
	    RD4(sc, sensor->sensor_base + TSENSOR_CONFIG0),
	    RD4(sc, sensor->sensor_base + TSENSOR_CONFIG1),
	    RD4(sc, sensor->sensor_base + TSENSOR_CONFIG2),
	    RD4(sc, sensor->sensor_base + TSENSOR_STATUS0),
	    RD4(sc, sensor->sensor_base + TSENSOR_STATUS1),
	    RD4(sc, sensor->sensor_base + TSENSOR_STATUS2)
	    );
#endif
	return (0);
}

static int
soctherm_get_temp(device_t dev, device_t cdev, uintptr_t id, int *val)
{
	struct soctherm_softc *sc;
	int i;

	sc = device_get_softc(dev);
	/* The direct sensor map starts at 0x100 */
	if (id >= 0x100) {
		id -= 0x100;
		if (id >= sc->soc->ntsensors)
			return (ERANGE);
		return(soctherm_read_temp(sc, sc->soc->tsensors + id, val));
	}
	/* Linux (DT) compatible thermal zones */
	for (i = 0; i < sc->soc->ntsensors; i++) {
		if (sc->soc->tsensors->id == id) {
			return(soctherm_read_temp(sc, sc->soc->tsensors + id,
			    val));
		}
	}
	return (ERANGE);
}

static int
soctherm_sysctl_temperature(SYSCTL_HANDLER_ARGS)
{
	struct soctherm_softc *sc;
	int val;
	int rv;
	int id;

	/* Write request */
	if (req->newptr != NULL)
		return (EINVAL);

	sc = arg1;
	id = arg2;

	if (id >= sc->soc->ntsensors)
		return (ERANGE);
	rv =  soctherm_read_temp(sc, sc->soc->tsensors + id, &val);
	if (rv != 0)
		return (rv);

	val = val / 100;
	val +=  2731;
	rv = sysctl_handle_int(oidp, &val, 0, req);
	return (rv);
}

static int
soctherm_init_sysctl(struct soctherm_softc *sc)
{
	int i;
	struct sysctl_oid *oid, *tmp;

	sysctl_ctx_init(&soctherm_sysctl_ctx);
	/* create node for hw.temp */
	oid = SYSCTL_ADD_NODE(&soctherm_sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, "temperature",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	if (oid == NULL)
		return (ENXIO);

	/* Add sensors */
	for (i = sc->soc->ntsensors  - 1; i >= 0; i--) {
		tmp = SYSCTL_ADD_PROC(&soctherm_sysctl_ctx,
		    SYSCTL_CHILDREN(oid), OID_AUTO, sc->soc->tsensors[i].name,
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc, i,
		    soctherm_sysctl_temperature, "IK", "SoC Temperature");
		if (tmp == NULL)
			return (ENXIO);
	}

	return (0);
}

static int
soctherm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Tegra temperature sensors");
	return (BUS_PROBE_DEFAULT);
}

static int
soctherm_attach(device_t dev)
{
	struct soctherm_softc *sc;
	phandle_t node;
	int i, rid, rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->soc = (struct soctherm_soc *)ofw_bus_search_compatible(dev,
	   compat_data)->ocd_data;
	node = ofw_bus_get_node(sc->dev);

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

/*
	if ((bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC,
	    soctherm_intr, NULL, sc, &sc->irq_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		goto fail;
	}
*/

	/* OWF resources */
	rv = hwreset_get_by_ofw_name(dev, 0, "soctherm", &sc->reset);
	if (rv != 0) {
		device_printf(dev, "Cannot get fuse reset\n");
		goto fail;
	}
	rv = clk_get_by_ofw_name(dev, 0, "tsensor", &sc->tsensor_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'tsensor' clock: %d\n", rv);
		goto fail;
	}
	rv = clk_get_by_ofw_name(dev, 0, "soctherm", &sc->soctherm_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'soctherm' clock: %d\n", rv);
		goto fail;
	}

	rv = hwreset_assert(sc->reset);
	if (rv != 0) {
		device_printf(dev, "Cannot assert reset\n");
		goto fail;
	}
	rv = clk_enable(sc->tsensor_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot enable 'tsensor' clock: %d\n", rv);
		goto fail;
	}
	rv = clk_enable(sc->soctherm_clk);
	if (rv != 0) {
		device_printf(dev, "Cannot enable 'soctherm' clock: %d\n", rv);
		goto fail;
	}
	rv = hwreset_deassert(sc->reset);
	if (rv != 0) {
		device_printf(dev, "Cannot clear reset\n");
		goto fail;
	}

	sc->soc->shared_cal(sc);

	WR4(sc, TSENSOR_PDIV, sc->soc->tsensor_pdiv);
	WR4(sc, TSENSOR_HOTSPOT_OFF, sc->soc->tsensor_hotspot_off);

	for (i = 0; i < sc->soc->ntsensors; i++)
		soctherm_init_tsensor(sc, sc->soc->tsensors + i);

	rv = soctherm_init_sysctl(sc);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot initialize sysctls\n");
		goto fail;
	}

	OF_device_register_xref(OF_xref_from_node(node), dev);
	bus_attach_children(dev);
	return (0);

fail:
	if (sc->irq_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_ih);
	sysctl_ctx_free(&soctherm_sysctl_ctx);
	if (sc->tsensor_clk != NULL)
		clk_release(sc->tsensor_clk);
	if (sc->soctherm_clk != NULL)
		clk_release(sc->soctherm_clk);
	if (sc->reset != NULL)
		hwreset_release(sc->reset);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (ENXIO);
}

static int
soctherm_detach(device_t dev)
{
	struct soctherm_softc *sc;
	sc = device_get_softc(dev);

	if (sc->irq_ih != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_ih);
	sysctl_ctx_free(&soctherm_sysctl_ctx);
	if (sc->tsensor_clk != NULL)
		clk_release(sc->tsensor_clk);
	if (sc->soctherm_clk != NULL)
		clk_release(sc->soctherm_clk);
	if (sc->reset != NULL)
		hwreset_release(sc->reset);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (ENXIO);
}

static device_method_t tegra_soctherm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			soctherm_probe),
	DEVMETHOD(device_attach,		soctherm_attach),
	DEVMETHOD(device_detach,		soctherm_detach),

	/* SOCTHERM interface */
	DEVMETHOD(tegra_soctherm_get_temperature, soctherm_get_temp),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(soctherm, tegra_soctherm_driver, tegra_soctherm_methods,
    sizeof(struct soctherm_softc));
EARLY_DRIVER_MODULE(tegra_soctherm, simplebus, tegra_soctherm_driver,
    NULL, NULL, 79);
