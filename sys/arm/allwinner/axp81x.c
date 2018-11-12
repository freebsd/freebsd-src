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
 * X-Powers AXP813/818 PMU for Allwinner SoCs
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

MALLOC_DEFINE(M_AXP81X_REG, "AXP81x regulator", "AXP81x power regulator");

#define	AXP_ICTYPE		0x03
#define	AXP_POWERCTL1		0x10
#define	 AXP_POWERCTL1_DCDC2	(1 << 1)
#define	AXP_POWERCTL2		0x12
#define	 AXP_POWERCTL2_DC1SW	(1 << 7)
#define	AXP_VOLTCTL_DCDC2	0x21
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
#define	AXP_GPIO1_CTRL		0x92
#define	 AXP_GPIO_FUNC		(0x7 << 0)
#define	 AXP_GPIO_FUNC_SHIFT	0
#define	 AXP_GPIO_FUNC_DRVLO	0
#define	 AXP_GPIO_FUNC_DRVHI	1
#define	 AXP_GPIO_FUNC_INPUT	2
#define	AXP_GPIO_SIGBIT		0x94
#define	AXP_GPIO_PD		0x97

static const struct {
	const char *name;
	uint8_t	ctrl_reg;
} axp81x_pins[] = {
	{ "GPIO0", AXP_GPIO0_CTRL },
	{ "GPIO1", AXP_GPIO1_CTRL },
};

static struct ofw_compat_data compat_data[] = {
	{ "x-powers,axp813",			1 },
	{ "x-powers,axp818",			1 },
	{ NULL,					0 }
};

static struct resource_spec axp81x_spec[] = {
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

struct axp81x_regdef {
	intptr_t		id;
	char			*name;
	char			*supply_name;
	uint8_t			enable_reg;
	uint8_t			enable_mask;
	uint8_t			voltage_reg;
	int			voltage_min;
	int			voltage_max;
	int			voltage_step1;
	int			voltage_nstep1;
	int			voltage_step2;
	int			voltage_nstep2;
};

enum axp81x_reg_id {
	AXP81X_REG_ID_DC1SW,
	AXP81X_REG_ID_DCDC2,
};

static struct axp81x_regdef axp81x_regdefs[] = {
	{
		.id = AXP81X_REG_ID_DC1SW,
		.name = "dc1sw",
		.enable_reg = AXP_POWERCTL2,
		.enable_mask = AXP_POWERCTL2_DC1SW,
	},
	{
		.id = AXP81X_REG_ID_DCDC2,
		.name = "dcdc2",
		.enable_reg = AXP_POWERCTL1,
		.enable_mask = AXP_POWERCTL1_DCDC2,
		.voltage_reg = AXP_VOLTCTL_DCDC2,
		.voltage_min = 500,
		.voltage_max = 1300,
		.voltage_step1 = 10,
		.voltage_nstep1 = 70,
		.voltage_step2 = 20,
		.voltage_nstep2 = 5,
	},
};

struct axp81x_softc;

struct axp81x_reg_sc {
	struct regnode		*regnode;
	device_t		base_dev;
	struct axp81x_regdef	*def;
	phandle_t		xref;
	struct regnode_std_param *param;
};

struct axp81x_softc {
	struct resource		*res;
	uint16_t		addr;
	void			*ih;
	device_t		gpiodev;
	struct mtx		mtx;
	int			busy;

	/* Regulators */
	struct axp81x_reg_sc	**regs;
	int			nregs;
};

#define	AXP_LOCK(sc)	mtx_lock(&(sc)->mtx)
#define	AXP_UNLOCK(sc)	mtx_unlock(&(sc)->mtx)

static int
axp81x_read(device_t dev, uint8_t reg, uint8_t *data, uint8_t size)
{
	struct axp81x_softc *sc;
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
axp81x_write(device_t dev, uint8_t reg, uint8_t val)
{
	struct axp81x_softc *sc;
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
axp81x_regnode_init(struct regnode *regnode)
{
	return (0);
}

static int
axp81x_regnode_enable(struct regnode *regnode, bool enable, int *udelay)
{
	struct axp81x_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	axp81x_read(sc->base_dev, sc->def->enable_reg, &val, 1);
	if (enable)
		val |= sc->def->enable_mask;
	else
		val &= ~sc->def->enable_mask;
	axp81x_write(sc->base_dev, sc->def->enable_reg, val);

	*udelay = 0;

	return (0);
}

static void
axp81x_regnode_reg_to_voltage(struct axp81x_reg_sc *sc, uint8_t val, int *uv)
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
axp81x_regnode_voltage_to_reg(struct axp81x_reg_sc *sc, int min_uvolt,
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
axp81x_regnode_set_voltage(struct regnode *regnode, int min_uvolt,
    int max_uvolt, int *udelay)
{
	struct axp81x_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	if (!sc->def->voltage_step1 || !sc->def->voltage_step2)
		return (ENXIO);

	if (axp81x_regnode_voltage_to_reg(sc, min_uvolt, max_uvolt, &val) != 0)
		return (ERANGE);

	axp81x_write(sc->base_dev, sc->def->voltage_reg, val);

	*udelay = 0;

	return (0);
}

static int
axp81x_regnode_get_voltage(struct regnode *regnode, int *uvolt)
{
	struct axp81x_reg_sc *sc;
	uint8_t val;

	sc = regnode_get_softc(regnode);

	if (!sc->def->voltage_step1 || !sc->def->voltage_step2)
		return (ENXIO);

	axp81x_read(sc->base_dev, sc->def->voltage_reg, &val, 1);
	axp81x_regnode_reg_to_voltage(sc, val & AXP_VOLTCTL_MASK, uvolt);

	return (0);
}

static regnode_method_t axp81x_regnode_methods[] = {
	/* Regulator interface */
	REGNODEMETHOD(regnode_init,		axp81x_regnode_init),
	REGNODEMETHOD(regnode_enable,		axp81x_regnode_enable),
	REGNODEMETHOD(regnode_set_voltage,	axp81x_regnode_set_voltage),
	REGNODEMETHOD(regnode_get_voltage,	axp81x_regnode_get_voltage),
	REGNODEMETHOD_END
};
DEFINE_CLASS_1(axp81x_regnode, axp81x_regnode_class, axp81x_regnode_methods,
    sizeof(struct axp81x_reg_sc), regnode_class);

static void
axp81x_shutdown(void *devp, int howto)
{
	device_t dev;

	if ((howto & RB_POWEROFF) == 0)
		return;

	dev = devp;

	if (bootverbose)
		device_printf(dev, "Shutdown AXP81x\n");

	axp81x_write(dev, AXP_POWERBAT, AXP_POWERBAT_SHUTDOWN);
}

static void
axp81x_intr(void *arg)
{
	struct axp81x_softc *sc;
	device_t dev;
	uint8_t val;
	int error;

	dev = arg;
	sc = device_get_softc(dev);

	error = axp81x_read(dev, AXP_IRQSTAT5, &val, 1);
	if (error != 0)
		return;

	if (val != 0) {
		if ((val & AXP_IRQSTAT5_POKSIRQ) != 0) {
			if (bootverbose)
				device_printf(dev, "Power button pressed\n");
			shutdown_nice(RB_POWEROFF);
		}
		/* Acknowledge */
		axp81x_write(dev, AXP_IRQSTAT5, val);
	}
}

static device_t
axp81x_gpio_get_bus(device_t dev)
{
	struct axp81x_softc *sc;

	sc = device_get_softc(dev);

	return (sc->gpiodev);
}

static int
axp81x_gpio_pin_max(device_t dev, int *maxpin)
{
	*maxpin = nitems(axp81x_pins) - 1;

	return (0);
}

static int
axp81x_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	if (pin >= nitems(axp81x_pins))
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "%s", axp81x_pins[pin].name);

	return (0);
}

static int
axp81x_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	if (pin >= nitems(axp81x_pins))
		return (EINVAL);

	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

	return (0);
}

static int
axp81x_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct axp81x_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp81x_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp81x_read(dev, axp81x_pins[pin].ctrl_reg, &data, 1);
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
axp81x_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct axp81x_softc *sc;
	uint8_t data;
	int error;

	if (pin >= nitems(axp81x_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp81x_read(dev, axp81x_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		data &= ~AXP_GPIO_FUNC;
		if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) != 0) {
			if ((flags & GPIO_PIN_OUTPUT) == 0)
				data |= AXP_GPIO_FUNC_INPUT;
		}
		error = axp81x_write(dev, axp81x_pins[pin].ctrl_reg, data);
	}
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp81x_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct axp81x_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp81x_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp81x_read(dev, axp81x_pins[pin].ctrl_reg, &data, 1);
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
			error = axp81x_read(dev, AXP_GPIO_SIGBIT, &data, 1);
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
axp81x_gpio_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct axp81x_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp81x_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp81x_read(dev, axp81x_pins[pin].ctrl_reg, &data, 1);
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
		error = axp81x_write(dev, axp81x_pins[pin].ctrl_reg, data);
	AXP_UNLOCK(sc);

	return (error);
}


static int
axp81x_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct axp81x_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp81x_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp81x_read(dev, axp81x_pins[pin].ctrl_reg, &data, 1);
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
		error = axp81x_write(dev, axp81x_pins[pin].ctrl_reg, data);
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp81x_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent,
    int gcells, pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	if (gpios[0] >= nitems(axp81x_pins))
		return (EINVAL);

	*pin = gpios[0];
	*flags = gpios[1];

	return (0);
}

static phandle_t
axp81x_get_node(device_t dev, device_t bus)
{
	return (ofw_bus_get_node(dev));
}

static struct axp81x_reg_sc *
axp81x_reg_attach(device_t dev, phandle_t node,
    struct axp81x_regdef *def)
{
	struct axp81x_reg_sc *reg_sc;
	struct regnode_init_def initdef;
	struct regnode *regnode;

	memset(&initdef, 0, sizeof(initdef));
	regulator_parse_ofw_stdparam(dev, node, &initdef);
	if (initdef.std_param.min_uvolt == 0)
		initdef.std_param.min_uvolt = def->voltage_min * 1000;
	if (initdef.std_param.max_uvolt == 0)
		initdef.std_param.max_uvolt = def->voltage_max * 1000;
	initdef.id = def->id;
	initdef.ofw_node = node;
	regnode = regnode_create(dev, &axp81x_regnode_class, &initdef);
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
axp81x_regdev_map(device_t dev, phandle_t xref, int ncells, pcell_t *cells,
    intptr_t *num)
{
	struct axp81x_softc *sc;
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
axp81x_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "X-Powers AXP81x Power Management Unit");

	return (BUS_PROBE_DEFAULT);
}

static int
axp81x_attach(device_t dev)
{
	struct axp81x_softc *sc;
	struct axp81x_reg_sc *reg;
	uint8_t chip_id;
	phandle_t rnode, child;
	int error, i;

	sc = device_get_softc(dev);

	sc->addr = iicbus_get_addr(dev);
	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	error = bus_alloc_resources(dev, axp81x_spec, &sc->res);
	if (error != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (error);
	}

	if (bootverbose) {
		axp81x_read(dev, AXP_ICTYPE, &chip_id, 1);
		device_printf(dev, "chip ID 0x%02x\n", chip_id);
	}

	sc->nregs = nitems(axp81x_regdefs);
	sc->regs = malloc(sizeof(struct axp81x_reg_sc *) * sc->nregs,
	    M_AXP81X_REG, M_WAITOK | M_ZERO);

	/* Attach known regulators that exist in the DT */
	rnode = ofw_bus_find_child(ofw_bus_get_node(dev), "regulators");
	if (rnode > 0) {
		for (i = 0; i < sc->nregs; i++) {
			child = ofw_bus_find_child(rnode,
			    axp81x_regdefs[i].name);
			if (child == 0)
				continue;
			reg = axp81x_reg_attach(dev, child, &axp81x_regdefs[i]);
			if (reg == NULL) {
				device_printf(dev,
				    "cannot attach regulator %s\n",
				    axp81x_regdefs[i].name);
				return (ENXIO);
			}
			sc->regs[i] = reg;
		}
	}

	/* Enable IRQ on short power key press */
	axp81x_write(dev, AXP_IRQEN1, 0);
	axp81x_write(dev, AXP_IRQEN2, 0);
	axp81x_write(dev, AXP_IRQEN3, 0);
	axp81x_write(dev, AXP_IRQEN4, 0);
	axp81x_write(dev, AXP_IRQEN5, AXP_IRQEN5_POKSIRQ);
	axp81x_write(dev, AXP_IRQEN6, 0);

	/* Install interrupt handler */
	error = bus_setup_intr(dev, sc->res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, axp81x_intr, dev, &sc->ih);
	if (error != 0) {
		device_printf(dev, "cannot setup interrupt handler\n");
		return (error);
	}

	EVENTHANDLER_REGISTER(shutdown_final, axp81x_shutdown, dev,
	    SHUTDOWN_PRI_LAST);

	sc->gpiodev = gpiobus_attach_bus(dev);

	return (0);
}

static device_method_t axp81x_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		axp81x_probe),
	DEVMETHOD(device_attach,	axp81x_attach),

	/* GPIO interface */
	DEVMETHOD(gpio_get_bus,		axp81x_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		axp81x_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	axp81x_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	axp81x_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	axp81x_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	axp81x_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		axp81x_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		axp81x_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	axp81x_gpio_pin_toggle),
	DEVMETHOD(gpio_map_gpios,	axp81x_gpio_map_gpios),

	/* Regdev interface */
	DEVMETHOD(regdev_map,		axp81x_regdev_map),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_node,	axp81x_get_node),

	DEVMETHOD_END
};

static driver_t axp81x_driver = {
	"axp81x_pmu",
	axp81x_methods,
	sizeof(struct axp81x_softc),
};

static devclass_t axp81x_devclass;
extern devclass_t ofwgpiobus_devclass, gpioc_devclass;
extern driver_t ofw_gpiobus_driver, gpioc_driver;

EARLY_DRIVER_MODULE(axp81x, iicbus, axp81x_driver, axp81x_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
EARLY_DRIVER_MODULE(ofw_gpiobus, axp81x_pmu, ofw_gpiobus_driver,
    ofwgpiobus_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LAST);
DRIVER_MODULE(gpioc, axp81x_pmu, gpioc_driver, gpioc_devclass, 0, 0);
MODULE_VERSION(axp81x, 1);
MODULE_DEPEND(axp81x, iicbus, 1, 1, 1);
