/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/*
 * Driver for the Winbond W83793G hardware monitor.
 *
 * The hardware monitor supports the following sensors:
 * - 6 temperature sensors
 *   - 4 with 1/4 integer precision
 *   - 2 with integer precision
 * - 11 voltage sensors
 * - 12 fan sensors
 *   - FanIn 6-12 are on multifunction pins, so may not be enabled.
 *   8 DC/PWM fan outputs for fan speed control
 * - Case open detection
 */

#define	WB_TD_BASE	0x1c
#define	WB_TLOW		0x22

#define	WB_VCORE_A	0x10
#define	WB_VCORE_B	0x11
#define	WB_VTT		0x12
#define	WB_VSEN1	0x14
#define	WB_VSEN2	0x15
#define	WB_VSEN3	0x16
#define	WB_VSEN4	0x17
#define	WB_5VDD		0x18
#define	WB_5VSB		0x19
#define	WB_VBAT		0x1a
#define	WB_VLOW		0x1b
#define	WB_FAN_BASE	0x23

#define	INT_STS1	0x41
#define	INT_STS2	0x42
#define	INT_STS3	0x43
#define	INT_STS4	0x44
#define	  CHASSIS	  0x40
#define	INT_STS5	0x45
#define	INT_MASK1	0x46
#define	INT_MASK2	0x47
#define	INT_MASK3	0x48
#define	INT_MASK4	0x49
#define	  CLR_CHS	  0x80
#define	INT_MASK5	0x4a

#define	WB_MFC		0x58	/* Multi-function pin control */
#define	  MFC_VIDBSEL	  0x80
#define	  MFC_SIB_SEL	  0x40
#define	  MFC_SID_SEL_M	  0x30
#define	  MFC_SID_VID	  0x00
#define	  MFC_SID_FANIN	  0x20
#define	  MFC_SIC_SEL_M	  0x0c
#define	  MFC_SIC_VID	  0x00
#define	  MFC_SIC_FANIN	  0x08
#define	  MFC_SIA_SEL	  0x02
#define	  MFC_FAN8SEL	  0x01
#define	WB_FANIN_CTRL	0x5c
#define	  FANIN_EN_12	  0x40
#define	  FANIN_EN_11	  0x20
#define	  FANIN_EN_10	  0x10
#define	  FANIN_EN_9	  0x08
#define	  FANIN_EN_8	  0x04
#define	  FANIN_EN_7	  0x02
#define	  FANIN_EN_6	  0x01
#define	WB_FANIN_SEL	0x5d
#define	WB_TD_MD	0x5e	/* TD mode select register */
#define	  TD_MD_M(n)	  (0x3 << ((n) * 2))
#define	  TD_MD_S(n)	  ((n) * 2)
#define	  TD_STOP_M	  0x0
#define	  TD_INT_MD	  0x1
#define	  TD_EXT_MD	  0x2
#define	WB_TR_MD	0x5f
#define	  TR2_MD	  0x2
#define	  TR1_MD	  0x1

#define	WB_TEMP_COUNT		6	/* Total temperature sensors */
#define	WB_TD_COUNT		4	/* Temp sensors with "low" part */
#define	WB_TR_COUNT		2
#define	WB_FAN_COUNT		12
#define	WB_FAN_ALWAYS_ON	5	/* First 5 are not controlled */
#define	WB_V_COUNT		11

static const struct wb_vsens {
	const char	*name;
	int		reg;
	int		scale;	/* Scale in millivolts */
	int		add;	/* Scale in millivolts */
	int		left_low;	/* left bit in VLOW, if applicable */
} voltages[] = {
	{ "v_core_a", WB_VCORE_A, 2, 0, 1 },
	{ "v_core_b", WB_VCORE_B, 2, 0, 3 },
	{ "v_tt", WB_VTT, 2, 0, 5 },
	{ "v_sen_1", WB_VSEN1, 16 },
	{ "v_sen_2", WB_VSEN2, 16 },
	{ "v_sen_3", WB_VSEN3, 16 },
	{ "v_sen_4", WB_VSEN4, 8 },
	{ "5v", WB_5VDD, 24, 150 },
	{ "5v_sb", WB_5VSB, 24, 150 },
	{ "v_bat", WB_VBAT, 16 }
};

struct w83793g_softc {
	device_t	sc_dev;

};

static device_probe_t	w83793g_probe;
static device_attach_t	w83793g_attach;
static device_detach_t	w83793g_detach;
static int w83793g_temp_sysctl(SYSCTL_HANDLER_ARGS);
static int w83793g_fan_sysctl(SYSCTL_HANDLER_ARGS);
static int w83793g_voltage_sysctl(SYSCTL_HANDLER_ARGS);
static int w83793g_case_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t w83793g_methods[] = {
	DEVMETHOD(device_probe,		w83793g_probe),
	DEVMETHOD(device_attach,	w83793g_attach),
	DEVMETHOD(device_detach,	w83793g_detach),

	DEVMETHOD_END
};

static struct ofw_compat_data compat[] = {
	{ "winbond,w83793", 1 },
	{ NULL, 0 }
};

DEFINE_CLASS_0(w83793g, w83793g_driver, w83793g_methods,
    sizeof(struct w83793g_softc));
DRIVER_MODULE(w83793g, iicbus, w83793g_driver, NULL, NULL);
MODULE_VERSION(w83793g, 1);
MODULE_DEPEND(w83793g, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
IICBUS_FDT_PNP_INFO(compat);

static int
w83793g_readreg(device_t dev, int reg, uint8_t *output)
{
	return (iicdev_readfrom(dev, reg, output, sizeof(*output), IIC_WAIT));
}

static int
w83793g_writereg(device_t dev, int reg, uint8_t *output)
{
	return (iicdev_writeto(dev, reg, output, sizeof(*output), IIC_WAIT));
}

static bool
temp_enabled(struct w83793g_softc *sc, int sensor)
{
	uint8_t reg;
	int error;

	if (sensor < WB_TD_COUNT) {
		error = w83793g_readreg(sc->sc_dev, WB_TD_MD, &reg);
		if (error != 0)
			return (false);
		return ((reg & TD_MD_M(sensor)) != 0);
	} else {
		error = w83793g_readreg(sc->sc_dev, WB_TR_MD, &reg);
		sensor -= WB_TD_COUNT;
		if (error != 0)
			return (false);
		return ((reg & (1 << sensor)) != 0);
	}
}

static bool
fan_enabled(struct w83793g_softc *sc, int fan)
{
	int error;
	uint8_t fanin_ctl;

	if (fan < WB_FAN_ALWAYS_ON)
		return (true);

	error = w83793g_readreg(sc->sc_dev, WB_FANIN_CTRL, &fanin_ctl);
	if (error != 0)
		return (false);

	fan -= WB_FAN_ALWAYS_ON;

	return ((fanin_ctl & (1 << fan)) != 0);
}

static int
w83793g_probe(device_t dev)
{
	if (ofw_bus_search_compatible(dev, compat)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Winbond W83793 Hardware Monitor");

	return (BUS_PROBE_DEFAULT);
}

static int
w83793g_attach(device_t dev)
{
	struct w83793g_softc *sc = device_get_softc(dev);
	struct sysctl_oid *root = device_get_sysctl_tree(dev);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *node;
	int i;

	sc->sc_dev = dev;
	node = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(root), OID_AUTO, "voltages",
	    CTLFLAG_RD, NULL, NULL);
	for (i = 0; i < nitems(voltages); i++) {
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO,
		    voltages[i].name, CTLTYPE_INT | CTLFLAG_RD, sc,
		    i, w83793g_voltage_sysctl, "I",
		    "voltage (millivolts)");
	}
	node = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(root), OID_AUTO, "temp",
	    CTLFLAG_RD, NULL, NULL);
	for (i = 0; i < WB_TEMP_COUNT; i++) {
		/* Only supports single-digit sensors. */
		char name[sizeof("sensor_") + 1];

		if (!temp_enabled(sc, i))
			continue;
		snprintf(name, sizeof(name), "sensor_%d", i);
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, name,
		    CTLTYPE_INT | CTLFLAG_RD, sc, WB_TD_BASE + i,
		    w83793g_temp_sysctl, "IK2", NULL);
	}
	node = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(root), OID_AUTO, "fans",
	    CTLFLAG_RD, NULL, NULL);
	for (i = 0; i < WB_FAN_COUNT; i++) {
		/* Supports up to 12 fans */
		char name[sizeof("fan_") + 2];

		if (!fan_enabled(sc, i))
			continue;
		snprintf(name, sizeof(name), "fan_%d", i);
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, name,
		    CTLTYPE_INT | CTLFLAG_RD, sc, WB_FAN_BASE + i,
		    w83793g_fan_sysctl, "I", NULL);
	}
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(root), OID_AUTO, "chassis_open",
	    CTLTYPE_U8 | CTLFLAG_RD, sc, 0, w83793g_case_sysctl, "CU",
	    "report if the chassis_open was latched");
	return (0);
}

static int
w83793g_detach(device_t dev)
{
	return (ENXIO);
}

static int
w83793g_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct w83793g_softc *sc = arg1;
	int reg = arg2;
	int temp;
	int error;
	int8_t t_reg;
	uint8_t t_low;

	error = w83793g_readreg(sc->sc_dev, reg, &t_reg);
	if (error != 0)
		return (error);

	if (reg < WB_TD_BASE + WB_TD_COUNT) {
		error = w83793g_readreg(sc->sc_dev, WB_TLOW, &t_low);
		if (error != 0)
			return (error);
	} else
		t_low = 0;

	temp = (int)t_reg * 100;
	temp += (t_low >> (2 * (reg - WB_TD_BASE)) & 0x3) * 25;
	temp += 27315;	/* Convert celsius to kelvin */

	error = sysctl_handle_int(oidp, &temp, 0, req);

	return (error);
}

static int
w83793g_fan_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct w83793g_softc *sc = arg1;
	int reg = arg2;
	int count;
	int error;
	uint8_t reg_vals[2];	/* Fan count is 2 bytes */

	error = iicdev_readfrom(sc->sc_dev, reg, reg_vals, sizeof(reg_vals),
	    IIC_WAIT);
	if (error != 0)
		return (error);

	count = ((int)reg_vals[0] << 8) | reg_vals[1];
	error = sysctl_handle_int(oidp, &count, 0, req);

	return (error);
}

static int
w83793g_voltage_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct w83793g_softc *sc = arg1;
	const struct wb_vsens *sensor;
	int index = arg2;
	int volts;
	int error;
	uint8_t v_reg;
	uint8_t v_low;

	sensor = &voltages[index];
	error = w83793g_readreg(sc->sc_dev, sensor->reg, &v_reg);
	if (error != 0)
		return (error);

	volts = v_reg;
	if (sensor->left_low != 0) {
		volts <<= 2;
		error = w83793g_readreg(sc->sc_dev, WB_VLOW, &v_low);
		if (error != 0)
			return (error);
		volts |= (v_low >> (sensor->left_low - 1) & 0x3);
	}

	volts *= sensor->scale;
	volts += sensor->add;

	error = sysctl_handle_int(oidp, &volts, 0, req);

	return (error);
}

static int
w83793g_case_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct w83793g_softc *sc = arg1;
	int error;
	uint8_t reg;
	bool chassis;

	error = w83793g_readreg(sc->sc_dev, INT_STS4, &reg);
	if (error != 0)
		return (error);

	chassis = ((reg & CHASSIS) != 0);

	return (sysctl_handle_bool(oidp, &chassis, 0, req));
}
