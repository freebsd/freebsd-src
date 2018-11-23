/*-
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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
 * X-Powers AXP803/813/818 PMU for Allwinner SoCs
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/gpio.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/gpio/gpiobusvar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/regulator/regulator.h>

#include "gpio_if.h"
#include "iicbus_if.h"
#include "regdev_if.h"

MALLOC_DEFINE(M_AXP8XX_REG, "AXP8xx regulator", "AXP8xx power regulator");

#define	AXP_POWERSRC		0x00
#define	 AXP_POWERSRC_ACIN	(1 << 7)
#define	 AXP_POWERSRC_VBUS	(1 << 5)
#define	 AXP_POWERSRC_VBAT	(1 << 3)
#define	 AXP_POWERSRC_CHARING	(1 << 2)
#define	 AXP_POWERSRC_SHORTED	(1 << 1)
#define	 AXP_POWERSRC_STARTUP	(1 << 0)
#define	AXP_ICTYPE		0x03
#define	AXP_POWERCTL1		0x10
#define	 AXP_POWERCTL1_DCDC7	(1 << 6)	/* AXP813/818 only */
#define	 AXP_POWERCTL1_DCDC6	(1 << 5)
#define	 AXP_POWERCTL1_DCDC5	(1 << 4)
#define	 AXP_POWERCTL1_DCDC4	(1 << 3)
#define	 AXP_POWERCTL1_DCDC3	(1 << 2)
#define	 AXP_POWERCTL1_DCDC2	(1 << 1)
#define	 AXP_POWERCTL1_DCDC1	(1 << 0)
#define	AXP_POWERCTL2		0x12
#define	 AXP_POWERCTL2_DC1SW	(1 << 7)	/* AXP803 only */
#define	 AXP_POWERCTL2_DLDO4	(1 << 6)
#define	 AXP_POWERCTL2_DLDO3	(1 << 5)
#define	 AXP_POWERCTL2_DLDO2	(1 << 4)
#define	 AXP_POWERCTL2_DLDO1	(1 << 3)
#define	 AXP_POWERCTL2_ELDO3	(1 << 2)
#define	 AXP_POWERCTL2_ELDO2	(1 << 1)
#define	 AXP_POWERCTL2_ELDO1	(1 << 0)
#define	AXP_POWERCTL3		0x13
#define	 AXP_POWERCTL3_ALDO3	(1 << 7)
#define	 AXP_POWERCTL3_ALDO2	(1 << 6)
#define	 AXP_POWERCTL3_ALDO1	(1 << 5)
#define	 AXP_POWERCTL3_FLDO3	(1 << 4)	/* AXP813/818 only */
#define	 AXP_POWERCTL3_FLDO2	(1 << 3)
#define	 AXP_POWERCTL3_FLDO1	(1 << 2)
#define	AXP_VOLTCTL_DLDO1	0x15
#define	AXP_VOLTCTL_DLDO2	0x16
#define	AXP_VOLTCTL_DLDO3	0x17
#define	AXP_VOLTCTL_DLDO4	0x18
#define	AXP_VOLTCTL_ELDO1	0x19
#define	AXP_VOLTCTL_ELDO2	0x1A
#define	AXP_VOLTCTL_ELDO3	0x1B
#define	AXP_VOLTCTL_FLDO1	0x1C
#define	AXP_VOLTCTL_FLDO2	0x1D
#define	AXP_VOLTCTL_DCDC1	0x20
#define	AXP_VOLTCTL_DCDC2	0x21
#define	AXP_VOLTCTL_DCDC3	0x22
#define	AXP_VOLTCTL_DCDC4	0x23
#define	AXP_VOLTCTL_DCDC5	0x24
#define	AXP_VOLTCTL_DCDC6	0x25
#define	AXP_VOLTCTL_DCDC7	0x26
#define	AXP_VOLTCTL_ALDO1	0x28
#define	AXP_VOLTCTL_ALDO2	0x29
#define	AXP_VOLTCTL_ALDO3	0x2A
#define	 AXP_VOLTCTL_STATUS	(1 << 7)
#define	 AXP_VOLTCTL_MASK	0x7f
#define	AXP_POWERBAT		0x32
#define	 AXP_POWERBAT_SHUTDOWN	(1 << 7)
#define	AXP_IRQEN1		0x40
#define	AXP_IRQEN2		0x41
#define	AXP_IRQEN3		0x42
#define	AXP_IRQEN4		0x43
#define	AXP_IRQEN5		0x44
#define	 AXP_IRQEN5_POKSIRQ	(1 << 4)
#define	AXP_IRQEN6		0x45
#define	AXP_IRQSTAT5		0x4c
#define	 AXP_IRQSTAT5_POKSIRQ	(1 << 4)
#define	AXP_GPIO0_CTRL		0x90
#define	AXP_GPIO0LDO_CTRL	0x91
#define	AXP_GPIO1_CTRL		0x92
#define	AXP_GPIO1LDO_CTRL	0x93
#define	 AXP_GPIO_FUNC		(0x7 << 0)
#define	 AXP_GPIO_FUNC_SHIFT	0
#define	 AXP_GPIO_FUNC_DRVLO	0
#define	 AXP_GPIO_FUNC_DRVHI	1
#define	 AXP_GPIO_FUNC_INPUT	2
#define	 AXP_GPIO_FUNC_LDO_ON	3
#define	 AXP_GPIO_FUNC_LDO_OFF	4
#define	AXP_GPIO_SIGBIT		0x94
#define	AXP_GPIO_PD		0x97

static const struct {
	const char *name;
	uint8_t	ctrl_reg;
} axp8xx_pins[] = {
	{ "GPIO0", AXP_GPIO0_CTRL },
	{ "GPIO1", AXP_GPIO1_CTRL },
};

enum AXP8XX_TYPE {
	AXP803 = 1,
	AXP813,
};

static struct ofw_compat_data compat_data[] = {
	{ "x-powers,axp803",			AXP803 },
	{ "x-powers,axp813",			AXP813 },
	{ "x-powers,axp818",			AXP813 },
	{ NULL,					0 }
};

static struct resource_spec axp8xx_spec[] = {
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

struct axp8xx_regdef {
	intptr_t		id;
	char			*name;
	char			*supply_name;
	uint8_t			enable_reg;
	uint8_t			enable_mask;
	uint8_t			enable_value;
	uint8_t			disable_value;
	uint8_t			voltage_reg;
	int			voltage_min;
	int			voltage_max;
	int			voltage_step1;
	int			voltage_nstep1;
	int			voltage_step2;
	int			voltage_nstep2;
};

enum axp8xx_reg_id {
	AXP8XX_REG_ID_DCDC1 = 100,
	AXP8XX_REG_ID_DCDC2,
	AXP8XX_REG_ID_DCDC3,
	AXP8XX_REG_ID_DCDC4,
	AXP8XX_REG_ID_DCDC5,
	AXP8XX_REG_ID_DCDC6,
	AXP813_REG_ID_DCDC7,
	AXP803_REG_ID_DC1SW,
	AXP8XX_REG_ID_DLDO1,
	AXP8XX_REG_ID_DLDO2,
	AXP8XX_REG_ID_DLDO3,
	AXP8XX_REG_ID_DLDO4,
	AXP8XX_REG_ID_ELDO1,
	AXP8XX_REG_ID_ELDO2,
	AXP8XX_REG_ID_ELDO3,
	AXP8XX_REG_ID_ALDO1,
	AXP8XX_REG_ID_ALDO2,
	AXP8XX_REG_ID_ALDO3,
	AXP8XX_REG_ID_FLDO1,
	AXP8XX_REG_ID_FLDO2,
	AXP813_REG_ID_FLDO3,
	AXP8XX_REG_ID_GPIO0_LDO,
	AXP8XX_REG_ID_GPIO1_LDO,
};

static struct axp8xx_regdef axp803_regdefs[] = {
	{
		.id = AXP803_REG_ID_DC1SW,
		.name = "dc1sw",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_DC1SW,
		.enable_value = AXP_POWERCTL2_DC1SW,
	},
};

static struct axp8xx_regdef axp813_regdefs[] = {
	{
		.id = AXP813_REG_ID_DCDC7,
		.name = "dcdc7",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = (uint8_t) AXP_POWERCTL1_DCDC7,
		.enable_value = AXP_POWERCTL1_DCDC7,
		.voltage_reg = AXP_VOLTCTL_DCDC7,
		.voltage_min = 600,
		.voltage_max = 1520,
		.voltage_step1 = 10,
		.voltage_nstep1 = 50,
		.voltage_step2 = 20,
		.voltage_nstep2 = 21,
	},
};

static struct axp8xx_regdef axp8xx_common_regdefs[] = {
	{
		.id = AXP8XX_REG_ID_DCDC1,
		.name = "dcdc1",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = (uint8_t) AXP_POWERCTL1_DCDC1,
		.enable_value = AXP_POWERCTL1_DCDC1,
		.voltage_reg = AXP_VOLTCTL_DCDC1,
		.voltage_min = 1600,
		.voltage_max = 3400,
		.voltage_step1 = 100,
		.voltage_nstep1 = 18,
	},
	{
		.id = AXP8XX_REG_ID_DCDC2,
		.name = "dcdc2",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = (uint8_t) AXP_POWERCTL1_DCDC2,
		.enable_value = AXP_POWERCTL1_DCDC2,
		.voltage_reg = AXP_VOLTCTL_DCDC2,
		.voltage_min = 500,
		.voltage_max = 1300,
		.voltage_step1 = 10,
		.voltage_nstep1 = 70,
		.voltage_step2 = 20,
		.voltage_nstep2 = 5,
	},
	{
		.id = AXP8XX_REG_ID_DCDC3,
		.name = "dcdc3",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = (uint8_t) AXP_POWERCTL1_DCDC3,
		.enable_value = AXP_POWERCTL1_DCDC3,
		.voltage_reg = AXP_VOLTCTL_DCDC3,
		.voltage_min = 500,
		.voltage_max = 1300,
		.voltage_step1 = 10,
		.voltage_nstep1 = 70,
		.voltage_step2 = 20,
		.voltage_nstep2 = 5,
	},
	{
		.id = AXP8XX_REG_ID_DCDC4,
		.name = "dcdc4",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = (uint8_t) AXP_POWERCTL1_DCDC4,
		.enable_value = AXP_POWERCTL1_DCDC4,
		.voltage_reg = AXP_VOLTCTL_DCDC4,
		.voltage_min = 500,
		.voltage_max = 1300,
		.voltage_step1 = 10,
		.voltage_nstep1 = 70,
		.voltage_step2 = 20,
		.voltage_nstep2 = 5,
	},
	{
		.id = AXP8XX_REG_ID_DCDC5,
		.name = "dcdc5",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = (uint8_t) AXP_POWERCTL1_DCDC5,
		.enable_value = AXP_POWERCTL1_DCDC5,
		.voltage_reg = AXP_VOLTCTL_DCDC5,
		.voltage_min = 800,
		.voltage_max = 1840,
		.voltage_step1 = 10,
		.voltage_nstep1 = 42,
		.voltage_step2 = 20,
		.voltage_nstep2 = 36,
	},
	{
		.id = AXP8XX_REG_ID_DCDC6,
		.name = "dcdc6",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = (uint8_t) AXP_POWERCTL1_DCDC6,
		.enable_value = AXP_POWERCTL1_DCDC6,
		.voltage_reg = AXP_VOLTCTL_DCDC6,
		.voltage_min = 600,
		.voltage_max = 1520,
		.voltage_step1 = 10,
		.voltage_nstep1 = 50,
		.voltage_step2 = 20,
		.voltage_nstep2 = 21,
	},
	{
		.id = AXP8XX_REG_ID_DLDO1,
		.name = "dldo1",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_DLDO1,
		.enable_value = AXP_POWERCTL2_DLDO1,
		.voltage_reg = AXP_VOLTCTL_DLDO1,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
	{
		.id = AXP8XX_REG_ID_DLDO2,
		.name = "dldo2",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_DLDO2,
		.enable_value = AXP_POWERCTL2_DLDO2,
		.voltage_reg = AXP_VOLTCTL_DLDO2,
		.voltage_min = 700,
		.voltage_max = 4200,
		.voltage_step1 = 100,
		.voltage_nstep1 = 27,
		.voltage_step2 = 200,
		.voltage_nstep2 = 4,
	},
	{
		.id = AXP8XX_REG_ID_DLDO3,
		.name = "dldo3",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_DLDO3,
		.enable_value = AXP_POWERCTL2_DLDO3,
		.voltage_reg = AXP_VOLTCTL_DLDO3,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
	{
		.id = AXP8XX_REG_ID_DLDO4,
		.name = "dldo4",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_DLDO4,
		.enable_value = AXP_POWERCTL2_DLDO4,
		.voltage_reg = AXP_VOLTCTL_DLDO4,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
	{
		.id = AXP8XX_REG_ID_ALDO1,
		.name = "aldo1",
		.enable_reg = AXP_POWERCTL3,
		.enable_mask = (uint8_t) AXP_POWERCTL3_ALDO1,
		.enable_value = AXP_POWERCTL3_ALDO1,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
	{
		.id = AXP8XX_REG_ID_ALDO2,
		.name = "aldo2",
		.enable_reg = AXP_POWERCTL3,
		.enable_mask = (uint8_t) AXP_POWERCTL3_ALDO2,
		.enable_value = AXP_POWERCTL3_ALDO2,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
	{
		.id = AXP8XX_REG_ID_ALDO3,
		.name = "aldo3",
		.enable_reg = AXP_POWERCTL3,
		.enable_mask = (uint8_t) AXP_POWERCTL3_ALDO3,
		.enable_value = AXP_POWERCTL3_ALDO3,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
	{
		.id = AXP8XX_REG_ID_ELDO1,
		.name = "eldo1",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_ELDO1,
		.enable_value = AXP_POWERCTL2_ELDO1,
		.voltage_min = 700,
		.voltage_max = 1900,
		.voltage_step1 = 50,
		.voltage_nstep1 = 24,
	},
	{
		.id = AXP8XX_REG_ID_ELDO2,
		.name = "eldo2",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_ELDO2,
		.enable_value = AXP_POWERCTL2_ELDO2,
		.voltage_min = 700,
		.voltage_max = 1900,
		.voltage_step1 = 50,
		.voltage_nstep1 = 24,
	},
	{
		.id = AXP8XX_REG_ID_ELDO3,
		.name = "eldo3",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = (uint8_t) AXP_POWERCTL2_ELDO3,
		.enable_value = AXP_POWERCTL2_ELDO3,
		.voltage_min = 700,
		.voltage_max = 1900,
		.voltage_step1 = 50,
		.voltage_nstep1 = 24,
	},
	{
		.id = AXP8XX_REG_ID_FLDO1,
		.name = "fldo1",
		.enable_reg = AXP_POWERCTL3,
		.enable_mask = (uint8_t) ~AXP_POWERCTL3_FLDO1,
		.enable_value = AXP_POWERCTL3_FLDO1,
		.voltage_min = 700,
		.voltage_max = 1450,
		.voltage_step1 = 50,
		.voltage_nstep1 = 15,
	},
	{
		.id = AXP8XX_REG_ID_FLDO2,
		.name = "fldo2",
		.enable_reg = AXP_POWERCTL3,
		.enable_mask = (uint8_t) AXP_POWERCTL3_FLDO2,
		.enable_value = AXP_POWERCTL3_FLDO2,
		.voltage_min = 700,
		.voltage_max = 1450,
		.voltage_step1 = 50,
		.voltage_nstep1 = 15,
	},
	{
		.id = AXP8XX_REG_ID_GPIO0_LDO,
		.name = "ldo-io0",
		.enable_reg = AXP_GPIO0_CTRL,
		.enable_mask = (uint8_t) AXP_GPIO_FUNC,
		.enable_value = AXP_GPIO_FUNC_LDO_ON,
		.disable_value = AXP_GPIO_FUNC_LDO_OFF,
		.voltage_reg = AXP_GPIO0LDO_CTRL,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
	{
		.id = AXP8XX_REG_ID_GPIO1_LDO,
		.name = "ldo-io1",
		.enable_reg = AXP_GPIO1_CTRL,
		.enable_mask = (uint8_t) AXP_GPIO_FUNC,
		.enable_value = AXP_GPIO_FUNC_LDO_ON,
		.disable_value = AXP_GPIO_FUNC_LDO_OFF,
		.voltage_reg = AXP_GPIO1LDO_CTRL,
		.voltage_min = 700,
		.voltage_max = 3300,
		.voltage_step1 = 100,
		.voltage_nstep1 = 26,
	},
};

struct axp8xx_softc;

struct axp8xx_reg_sc {
	struct regnode		*regnode;
	device_t		base_dev;
	struct axp8xx_regdef	*def;
	phandle_t		xref;
	struct regnode_std_param *param;
};

struct axp8xx_softc {
	struct resource		*res;
	uint16_t		addr;
	void			*ih;
	device_t		gpiodev;
	struct mtx		mtx;
	int			busy;

	int			type;

	/* Regulators */
	struct axp8xx_reg_sc	**regs;
	int			nregs;
};

#define	AXP_LOCK(sc)	mtx_lock(&(sc)->mtx)
#define	AXP_UNLOCK(sc)	mtx_unlock(&(sc)->mtx)

static int
axp8xx_read(device_t dev, uint8_t reg, uint8_t *data, uint8_t size)
{
	struct axp8xx_softc *sc;
	struct iic_msg msg[2];

	sc = device_get_softc(dev);

	msg[0].slave = sc->addr;
	msg[0].flags = IIC_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].slave = sc->addr;
	msg[1].flags = IIC_M_RD;
	msg[1].len = size;
	msg[1].buf = data;

	return (iicbus_transfer(dev, msg, 2));
}

static int
axp8xx_write(device_t dev, uint8_t reg, uint8_t val)
{
	struct axp8xx_softc *sc;
	struct iic_msg msg[2];

	sc = device_get_softc(dev);

	msg[0].slave = sc->addr;
	msg[0].flags = IIC_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].slave = sc->addr;
	msg[1].flags = IIC_M_WR;
	msg[1].len = 1;
	msg[1].buf = &val;

	return (iicbus_transfer(dev, msg, 2));
}

static int
axp8xx_regnode_init(struct regnode *regnode)
{
	return (0);
}

static int
axp8xx_regnode_enable(struct regnode *regnode, bool enable, int *udelay)
{
	struct axp8xx_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	if (bootverbose)
		device_printf(sc->base_dev, "%sable %s (%s)\n",
		    enable ? "En" : "Dis",
		    regnode_get_name(regnode),
		    sc->def->name);

	axp8xx_read(sc->base_dev, sc->def->enable_reg, &val, 1);
	val &= ~sc->def->enable_mask;
	if (enable)
		val |= sc->def->enable_value;
	else {
		if (sc->def->disable_value)
			val |= sc->def->disable_value;
		else
			val &= ~sc->def->enable_value;
	}
	axp8xx_write(sc->base_dev, sc->def->enable_reg, val);

	*udelay = 0;

	return (0);
}

static void
axp8xx_regnode_reg_to_voltage(struct axp8xx_reg_sc *sc, uint8_t val, int *uv)
{
	if (val < sc->def->voltage_nstep1)
		*uv = sc->def->voltage_min + val * sc->def->voltage_step1;
	else
		*uv = sc->def->voltage_min +
		    (sc->def->voltage_nstep1 * sc->def->voltage_step1) +
		    ((val - sc->def->voltage_nstep1) * sc->def->voltage_step2);
	*uv *= 1000;
}

static int
axp8xx_regnode_voltage_to_reg(struct axp8xx_reg_sc *sc, int min_uvolt,
    int max_uvolt, uint8_t *val)
{
	uint8_t nval;
	int nstep, uvolt;

	nval = 0;
	uvolt = sc->def->voltage_min * 1000;

	for (nstep = 0; nstep < sc->def->voltage_nstep1 && uvolt < min_uvolt;
	     nstep++) {
		++nval;
		uvolt += (sc->def->voltage_step1 * 1000);
	}
	for (nstep = 0; nstep < sc->def->voltage_nstep2 && uvolt < min_uvolt;
	     nstep++) {
		++nval;
		uvolt += (sc->def->voltage_step2 * 1000);
	}
	if (uvolt > max_uvolt)
		return (EINVAL);

	*val = nval;
	return (0);
}

static int
axp8xx_regnode_set_voltage(struct regnode *regnode, int min_uvolt,
    int max_uvolt, int *udelay)
{
	struct axp8xx_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	if (bootverbose)
		device_printf(sc->base_dev, "Setting %s (%s) to %d<->%d\n",
		    regnode_get_name(regnode),
		    sc->def->name,
		    min_uvolt, max_uvolt);

	if (sc->def->voltage_step1 == 0)
		return (ENXIO);

	if (axp8xx_regnode_voltage_to_reg(sc, min_uvolt, max_uvolt, &val) != 0)
		return (ERANGE);

	axp8xx_write(sc->base_dev, sc->def->voltage_reg, val);

	*udelay = 0;

	return (0);
}

static int
axp8xx_regnode_get_voltage(struct regnode *regnode, int *uvolt)
{
	struct axp8xx_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	if (!sc->def->voltage_step1 || !sc->def->voltage_step2)
		return (ENXIO);

	axp8xx_read(sc->base_dev, sc->def->voltage_reg, &val, 1);
	axp8xx_regnode_reg_to_voltage(sc, val & AXP_VOLTCTL_MASK, uvolt);

	return (0);
}

static regnode_method_t axp8xx_regnode_methods[] = {
	/* Regulator interface */
	REGNODEMETHOD(regnode_init,		axp8xx_regnode_init),
	REGNODEMETHOD(regnode_enable,		axp8xx_regnode_enable),
	REGNODEMETHOD(regnode_set_voltage,	axp8xx_regnode_set_voltage),
	REGNODEMETHOD(regnode_get_voltage,	axp8xx_regnode_get_voltage),
	REGNODEMETHOD_END
};
DEFINE_CLASS_1(axp8xx_regnode, axp8xx_regnode_class, axp8xx_regnode_methods,
    sizeof(struct axp8xx_reg_sc), regnode_class);

static void
axp8xx_shutdown(void *devp, int howto)
{
	device_t dev;

	if ((howto & RB_POWEROFF) == 0)
		return;

	dev = devp;

	if (bootverbose)
		device_printf(dev, "Shutdown Axp8xx\n");

	axp8xx_write(dev, AXP_POWERBAT, AXP_POWERBAT_SHUTDOWN);
}

static void
axp8xx_intr(void *arg)
{
	device_t dev;
	uint8_t val;
	int error;

	dev = arg;

	error = axp8xx_read(dev, AXP_IRQSTAT5, &val, 1);
	if (error != 0)
		return;

	if (val != 0) {
		if ((val & AXP_IRQSTAT5_POKSIRQ) != 0) {
			if (bootverbose)
				device_printf(dev, "Power button pressed\n");
			shutdown_nice(RB_POWEROFF);
		}
		/* Acknowledge */
		axp8xx_write(dev, AXP_IRQSTAT5, val);
	}
}

static device_t
axp8xx_gpio_get_bus(device_t dev)
{
	struct axp8xx_softc *sc;

	sc = device_get_softc(dev);

	return (sc->gpiodev);
}

static int
axp8xx_gpio_pin_max(device_t dev, int *maxpin)
{
	*maxpin = nitems(axp8xx_pins) - 1;

	return (0);
}

static int
axp8xx_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	if (pin >= nitems(axp8xx_pins))
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "%s", axp8xx_pins[pin].name);

	return (0);
}

static int
axp8xx_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	if (pin >= nitems(axp8xx_pins))
		return (EINVAL);

	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

	return (0);
}

static int
axp8xx_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct axp8xx_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp8xx_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp8xx_read(dev, axp8xx_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = (data & AXP_GPIO_FUNC) >> AXP_GPIO_FUNC_SHIFT;
		if (func == AXP_GPIO_FUNC_INPUT)
			*flags = GPIO_PIN_INPUT;
		else if (func == AXP_GPIO_FUNC_DRVLO ||
		    func == AXP_GPIO_FUNC_DRVHI)
			*flags = GPIO_PIN_OUTPUT;
		else
			*flags = 0;
	}
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp8xx_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct axp8xx_softc *sc;
	uint8_t data;
	int error;

	if (pin >= nitems(axp8xx_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp8xx_read(dev, axp8xx_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		data &= ~AXP_GPIO_FUNC;
		if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) != 0) {
			if ((flags & GPIO_PIN_OUTPUT) == 0)
				data |= AXP_GPIO_FUNC_INPUT;
		}
		error = axp8xx_write(dev, axp8xx_pins[pin].ctrl_reg, data);
	}
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp8xx_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct axp8xx_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp8xx_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp8xx_read(dev, axp8xx_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = (data & AXP_GPIO_FUNC) >> AXP_GPIO_FUNC_SHIFT;
		switch (func) {
		case AXP_GPIO_FUNC_DRVLO:
			*val = 0;
			break;
		case AXP_GPIO_FUNC_DRVHI:
			*val = 1;
			break;
		case AXP_GPIO_FUNC_INPUT:
			error = axp8xx_read(dev, AXP_GPIO_SIGBIT, &data, 1);
			if (error == 0)
				*val = (data & (1 << pin)) ? 1 : 0;
			break;
		default:
			error = EIO;
			break;
		}
	}
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp8xx_gpio_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct axp8xx_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp8xx_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp8xx_read(dev, axp8xx_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = (data & AXP_GPIO_FUNC) >> AXP_GPIO_FUNC_SHIFT;
		switch (func) {
		case AXP_GPIO_FUNC_DRVLO:
		case AXP_GPIO_FUNC_DRVHI:
			data &= ~AXP_GPIO_FUNC;
			data |= (val << AXP_GPIO_FUNC_SHIFT);
			break;
		default:
			error = EIO;
			break;
		}
	}
	if (error == 0)
		error = axp8xx_write(dev, axp8xx_pins[pin].ctrl_reg, data);
	AXP_UNLOCK(sc);

	return (error);
}


static int
axp8xx_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct axp8xx_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp8xx_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp8xx_read(dev, axp8xx_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = (data & AXP_GPIO_FUNC) >> AXP_GPIO_FUNC_SHIFT;
		switch (func) {
		case AXP_GPIO_FUNC_DRVLO:
			data &= ~AXP_GPIO_FUNC;
			data |= (AXP_GPIO_FUNC_DRVHI << AXP_GPIO_FUNC_SHIFT);
			break;
		case AXP_GPIO_FUNC_DRVHI:
			data &= ~AXP_GPIO_FUNC;
			data |= (AXP_GPIO_FUNC_DRVLO << AXP_GPIO_FUNC_SHIFT);
			break;
		default:
			error = EIO;
			break;
		}
	}
	if (error == 0)
		error = axp8xx_write(dev, axp8xx_pins[pin].ctrl_reg, data);
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp8xx_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent,
    int gcells, pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	if (gpios[0] >= nitems(axp8xx_pins))
		return (EINVAL);

	*pin = gpios[0];
	*flags = gpios[1];

	return (0);
}

static phandle_t
axp8xx_get_node(device_t dev, device_t bus)
{
	return (ofw_bus_get_node(dev));
}

static struct axp8xx_reg_sc *
axp8xx_reg_attach(device_t dev, phandle_t node,
    struct axp8xx_regdef *def)
{
	struct axp8xx_reg_sc *reg_sc;
	struct regnode_init_def initdef;
	struct regnode *regnode;

	memset(&initdef, 0, sizeof(initdef));
	if (regulator_parse_ofw_stdparam(dev, node, &initdef) != 0)
		return (NULL);
	if (initdef.std_param.min_uvolt == 0)
		initdef.std_param.min_uvolt = def->voltage_min * 1000;
	if (initdef.std_param.max_uvolt == 0)
		initdef.std_param.max_uvolt = def->voltage_max * 1000;
	initdef.id = def->id;
	initdef.ofw_node = node;
	regnode = regnode_create(dev, &axp8xx_regnode_class, &initdef);
	if (regnode == NULL) {
		device_printf(dev, "cannot create regulator\n");
		return (NULL);
	}

	reg_sc = regnode_get_softc(regnode);
	reg_sc->regnode = regnode;
	reg_sc->base_dev = dev;
	reg_sc->def = def;
	reg_sc->xref = OF_xref_from_node(node);
	reg_sc->param = regnode_get_stdparam(regnode);

	regnode_register(regnode);

	return (reg_sc);
}

static int
axp8xx_regdev_map(device_t dev, phandle_t xref, int ncells, pcell_t *cells,
    intptr_t *num)
{
	struct axp8xx_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < sc->nregs; i++) {
		if (sc->regs[i] == NULL)
			continue;
		if (sc->regs[i]->xref == xref) {
			*num = sc->regs[i]->def->id;
			return (0);
		}
	}

	return (ENXIO);
}

static int
axp8xx_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	switch (ofw_bus_search_compatible(dev, compat_data)->ocd_data)
	{
	case AXP803:
		device_set_desc(dev, "X-Powers AXP803 Power Management Unit");
		break;
	case AXP813:
		device_set_desc(dev, "X-Powers AXP813 Power Management Unit");
		break;
	default:
		return (ENXIO);
	}

	return (BUS_PROBE_DEFAULT);
}

static int
axp8xx_attach(device_t dev)
{
	struct axp8xx_softc *sc;
	struct axp8xx_reg_sc *reg;
	uint8_t chip_id;
	phandle_t rnode, child;
	int error, i;

	sc = device_get_softc(dev);

	sc->addr = iicbus_get_addr(dev);
	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	error = bus_alloc_resources(dev, axp8xx_spec, &sc->res);
	if (error != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (error);
	}

	if (bootverbose) {
		axp8xx_read(dev, AXP_ICTYPE, &chip_id, 1);
		device_printf(dev, "chip ID 0x%02x\n", chip_id);
	}

	sc->nregs = nitems(axp8xx_common_regdefs);
	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	switch (sc->type) {
	case AXP803:
		sc->nregs += nitems(axp803_regdefs);
		break;
	case AXP813:
		sc->nregs += nitems(axp813_regdefs);
		break;
	}
	sc->regs = malloc(sizeof(struct axp8xx_reg_sc *) * sc->nregs,
	    M_AXP8XX_REG, M_WAITOK | M_ZERO);

	/* Attach known regulators that exist in the DT */
	rnode = ofw_bus_find_child(ofw_bus_get_node(dev), "regulators");
	if (rnode > 0) {
		for (i = 0; i < sc->nregs; i++) {
			char *regname;
			struct axp8xx_regdef *regdef;

			if (i <= nitems(axp8xx_common_regdefs)) {
				regname = axp8xx_common_regdefs[i].name;
				regdef = &axp8xx_common_regdefs[i];
			} else {
				int off;

				off = i - nitems(axp8xx_common_regdefs);
				switch (sc->type) {
				case AXP803:
					regname = axp803_regdefs[off].name;
					regdef = &axp803_regdefs[off];
					break;
				case AXP813:
					regname = axp813_regdefs[off].name;
					regdef = &axp813_regdefs[off];
					break;
				}
			}
			child = ofw_bus_find_child(rnode,
			    regname);
			if (child == 0)
				continue;
			reg = axp8xx_reg_attach(dev, child,
			    regdef);
			if (reg == NULL) {
				device_printf(dev,
				    "cannot attach regulator %s\n",
				    regname);
				continue;
			}
			sc->regs[i] = reg;
		}
	}

	/* Enable IRQ on short power key press */
	axp8xx_write(dev, AXP_IRQEN1, 0);
	axp8xx_write(dev, AXP_IRQEN2, 0);
	axp8xx_write(dev, AXP_IRQEN3, 0);
	axp8xx_write(dev, AXP_IRQEN4, 0);
	axp8xx_write(dev, AXP_IRQEN5, AXP_IRQEN5_POKSIRQ);
	axp8xx_write(dev, AXP_IRQEN6, 0);

	/* Install interrupt handler */
	error = bus_setup_intr(dev, sc->res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, axp8xx_intr, dev, &sc->ih);
	if (error != 0) {
		device_printf(dev, "cannot setup interrupt handler\n");
		return (error);
	}

	EVENTHANDLER_REGISTER(shutdown_final, axp8xx_shutdown, dev,
	    SHUTDOWN_PRI_LAST);

	sc->gpiodev = gpiobus_attach_bus(dev);

	return (0);
}

static device_method_t axp8xx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		axp8xx_probe),
	DEVMETHOD(device_attach,	axp8xx_attach),

	/* GPIO interface */
	DEVMETHOD(gpio_get_bus,		axp8xx_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		axp8xx_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	axp8xx_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	axp8xx_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	axp8xx_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	axp8xx_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		axp8xx_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		axp8xx_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	axp8xx_gpio_pin_toggle),
	DEVMETHOD(gpio_map_gpios,	axp8xx_gpio_map_gpios),

	/* Regdev interface */
	DEVMETHOD(regdev_map,		axp8xx_regdev_map),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_node,	axp8xx_get_node),

	DEVMETHOD_END
};

static driver_t axp8xx_driver = {
	"axp8xx_pmu",
	axp8xx_methods,
	sizeof(struct axp8xx_softc),
};

static devclass_t axp8xx_devclass;
extern devclass_t ofwgpiobus_devclass, gpioc_devclass;
extern driver_t ofw_gpiobus_driver, gpioc_driver;

EARLY_DRIVER_MODULE(axp8xx, iicbus, axp8xx_driver, axp8xx_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
EARLY_DRIVER_MODULE(ofw_gpiobus, axp8xx_pmu, ofw_gpiobus_driver,
    ofwgpiobus_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
DRIVER_MODULE(gpioc, axp8xx_pmu, gpioc_driver, gpioc_devclass, 0, 0);
MODULE_VERSION(axp8xx, 1);
MODULE_DEPEND(axp8xx, iicbus, 1, 1, 1);
