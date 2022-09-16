/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
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

/*
 * Driver for TI TCA64XX I2C GPIO expander module.
 *
 * This driver only supports basic functionality
 * (interrupt handling and polarity inversion were omitted).
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/gpio/gpiobusvar.h>

#include "gpio_if.h"

/* Base addresses of registers. LSB omitted. */

#define TCA64XX_PINS_PER_REG		8

#define TCA64XX_BIT_FROM_PIN(pin)	(pin % TCA64XX_PINS_PER_REG)
#define TCA64XX_REG_ADDR(pin, baseaddr)	(baseaddr | (pin / \
					TCA64XX_PINS_PER_REG))
#define	TCA64XX_PIN_CAPS		(GPIO_PIN_OUTPUT | GPIO_PIN_INPUT \
					| GPIO_PIN_PUSHPULL | GPIO_PIN_INVIN)

#define TCA6416_IN_PORT_REG		0x0
#define	TCA6416_OUT_PORT_REG		0x2
#define	TCA6416_POLARITY_INV_REG	0x4
#define	TCA6416_CONF_REG		0x6
#define	TCA6416_NUM_PINS		16

#define TCA6408_IN_PORT_REG		0x0
#define TCA6408_OUT_PORT_REG		0x1
#define TCA6408_POLARITY_INV_REG	0x2
#define TCA6408_CONF_REG		0x3
#define TCA6408_NUM_PINS		8

#ifdef DEBUG
#define	dbg_dev_printf(dev, fmt, args...)	\
	device_printf(dev, fmt, ##args)
#else
#define	dbg_dev_printf(dev, fmt, args...)
#endif

enum chip_type{
	TCA6416_TYPE = 1,
	TCA6408_TYPE
};

struct tca64xx_softc {
	device_t	dev;
	device_t	busdev;
	enum chip_type	chip;
	struct mtx	mtx;
	uint32_t	addr;
	uint8_t		num_pins;
	uint8_t 	in_port_reg;
	uint8_t		out_port_reg;
	uint8_t		polarity_inv_reg;
	uint8_t		conf_reg;
	uint8_t		pin_caps;
};

static int tca64xx_read(device_t, uint8_t, uint8_t *);
static int tca64xx_write(device_t, uint8_t, uint8_t);
static int tca64xx_probe(device_t);
static int tca64xx_attach(device_t);
static int tca64xx_detach(device_t);
static device_t tca64xx_get_bus(device_t);
static int tca64xx_pin_max(device_t, int *);
static int tca64xx_pin_getcaps(device_t, uint32_t, uint32_t *);
static int tca64xx_pin_getflags(device_t, uint32_t, uint32_t *);
static int tca64xx_pin_setflags(device_t, uint32_t, uint32_t);
static int tca64xx_pin_getname(device_t, uint32_t, char *);
static int tca64xx_pin_get(device_t, uint32_t, unsigned int *);
static int tca64xx_pin_set(device_t, uint32_t, unsigned int);
static int tca64xx_pin_toggle(device_t, uint32_t);
#ifdef DEBUG
static void tca6408_regdump_setup(device_t dev);
static void tca6416_regdump_setup(device_t dev);
static int tca64xx_regdump_sysctl(SYSCTL_HANDLER_ARGS);
#endif

static device_method_t tca64xx_methods[] = {
	DEVMETHOD(device_probe,		tca64xx_probe),
	DEVMETHOD(device_attach,	tca64xx_attach),
	DEVMETHOD(device_detach,	tca64xx_detach),

	/* GPIO methods */
	DEVMETHOD(gpio_get_bus,		tca64xx_get_bus),
	DEVMETHOD(gpio_pin_max,		tca64xx_pin_max),
	DEVMETHOD(gpio_pin_getcaps,	tca64xx_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	tca64xx_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	tca64xx_pin_setflags),
	DEVMETHOD(gpio_pin_getname,	tca64xx_pin_getname),
	DEVMETHOD(gpio_pin_get,		tca64xx_pin_get),
	DEVMETHOD(gpio_pin_set,		tca64xx_pin_set),
	DEVMETHOD(gpio_pin_toggle,	tca64xx_pin_toggle),

	DEVMETHOD_END
};

static driver_t tca64xx_driver = {
	"gpio",
	tca64xx_methods,
	sizeof(struct tca64xx_softc)
};

DRIVER_MODULE(tca64xx, iicbus, tca64xx_driver, 0, 0);
MODULE_VERSION(tca64xx, 1);

static struct ofw_compat_data compat_data[] = {
	{"nxp,pca9555",	TCA6416_TYPE},
	{"ti,tca6408",	TCA6408_TYPE},
	{"ti,tca6416",	TCA6416_TYPE},
	{"ti,tca9539",	TCA6416_TYPE},
	{0,0}
};

static int
tca64xx_read(device_t dev, uint8_t reg, uint8_t *data)
{
	struct iic_msg msgs[2];
	struct tca64xx_softc *sc;
	int error;

	sc = device_get_softc(dev);
	if (data == NULL)
		return (EINVAL);

	msgs[0].slave = sc->addr;
	msgs[0].flags = IIC_M_WR | IIC_M_NOSTOP;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	msgs[1].slave = sc->addr;
	msgs[1].flags = IIC_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = data;

	error = iicbus_transfer_excl(dev, msgs, 2, IIC_WAIT);
	return (iic2errno(error));
}

static int
tca64xx_write(device_t dev, uint8_t reg, uint8_t val)
{
	struct iic_msg msg;
	struct tca64xx_softc *sc;
	int error;
	uint8_t buffer[2] = {reg, val};

	sc = device_get_softc(dev);

	msg.slave = sc->addr;
	msg.flags = IIC_M_WR;
	msg.len = 2;
	msg.buf = buffer;

	error = iicbus_transfer_excl(dev, &msg, 1, IIC_WAIT);
	return (iic2errno(error));
}

static int
tca64xx_probe(device_t dev)
{
	const struct ofw_compat_data *compat_ptr;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	compat_ptr = ofw_bus_search_compatible(dev, compat_data);

	switch (compat_ptr->ocd_data) {
	case TCA6416_TYPE:
		device_set_desc(dev, "TCA6416 I/O expander");
		break;
	case TCA6408_TYPE:
		device_set_desc(dev, "TCA6408 I/O expander");
		break;
	default:
		return (ENXIO);
	}

	return (BUS_PROBE_DEFAULT);
}

static int
tca64xx_attach(device_t dev)
{
	struct tca64xx_softc *sc;
	const struct ofw_compat_data *compat_ptr;

	sc = device_get_softc(dev);
	compat_ptr = ofw_bus_search_compatible(dev, compat_data);

	switch (compat_ptr->ocd_data) {
	case TCA6416_TYPE:
		sc->in_port_reg = TCA6416_IN_PORT_REG;
		sc->out_port_reg = TCA6416_OUT_PORT_REG;
		sc->polarity_inv_reg = TCA6416_POLARITY_INV_REG;
		sc->conf_reg = TCA6416_CONF_REG;
		sc->num_pins = TCA6416_NUM_PINS;
		break;
	case TCA6408_TYPE:
		sc->in_port_reg = TCA6408_IN_PORT_REG;
		sc->out_port_reg = TCA6408_OUT_PORT_REG;
		sc->polarity_inv_reg = TCA6408_POLARITY_INV_REG;
		sc->conf_reg = TCA6408_CONF_REG;
		sc->num_pins = TCA6408_NUM_PINS;
		break;
	default:
		__assert_unreachable();
	}

	sc->pin_caps = TCA64XX_PIN_CAPS;
	sc->chip = compat_ptr->ocd_data;
	sc->dev = dev;
	sc->addr = iicbus_get_addr(dev);

	mtx_init(&sc->mtx, "tca64xx gpio", "gpio", MTX_DEF);
	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Could not create busdev child\n");
		return (ENXIO);
	}

	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), dev);

#ifdef DEBUG
	switch (sc->chip) {
	case TCA6416_TYPE:
		tca6416_regdump_setup(dev);
		break;
	case TCA6408_TYPE:
		tca6408_regdump_setup(dev);
		break;
	default:
		__assert_unreachable();
	}
#endif

	return (0);
}

static int
tca64xx_detach(device_t dev)
{
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	if (sc->busdev != NULL)
		gpiobus_detach_bus(sc->busdev);

	mtx_destroy(&sc->mtx);

	return (0);
}

static device_t
tca64xx_get_bus(device_t dev)
{
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
tca64xx_pin_max(device_t dev __unused, int *maxpin)
{
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	if (maxpin == NULL)
		return (EINVAL);

	*maxpin = sc->num_pins-1;

	return (0);
}

static int
tca64xx_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->num_pins || caps == NULL)
		return (EINVAL);
	*caps = sc->pin_caps;

	return (0);
}

static int
tca64xx_pin_getflags(device_t dev, uint32_t pin, uint32_t *pflags)
{
	int error;
	uint8_t bit, val, addr;
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	bit = TCA64XX_BIT_FROM_PIN(pin);

	if (pin >= sc->num_pins || pflags == NULL)
		return (EINVAL);

	addr = TCA64XX_REG_ADDR(pin, sc->conf_reg);
	error = tca64xx_read(dev, addr, &val);
	if (error != 0)
		return (error);

	*pflags = (val & (1 << bit)) ? GPIO_PIN_INPUT : GPIO_PIN_OUTPUT;

	addr = TCA64XX_REG_ADDR(pin, sc->polarity_inv_reg);
	error = tca64xx_read(dev, addr, &val);
	if (error != 0)
		return (error);

	if (val & (1 << bit))
		*pflags |= GPIO_PIN_INVIN;

	return (0);
}

static int
tca64xx_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	uint8_t bit, val, addr, pins, inv_val;
	int error;
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	pins = sc->num_pins;
	bit = TCA64XX_BIT_FROM_PIN(pin);

	if (pin >= pins)
		return (EINVAL);
	mtx_lock(&sc->mtx);

	addr = TCA64XX_REG_ADDR(pin, sc->conf_reg);
	error = tca64xx_read(dev, addr, &val);
	if (error != 0)
		goto fail;

	addr = TCA64XX_REG_ADDR(pin, sc->polarity_inv_reg);
	error = tca64xx_read(dev, addr, &inv_val);
	if (error != 0)
		goto fail;

	if (flags & GPIO_PIN_INPUT)
		val |= (1 << bit);
	else if (flags & GPIO_PIN_OUTPUT)
		val &= ~(1 << bit);

	if (flags & GPIO_PIN_INVIN)
		inv_val |= (1 << bit);
	else
		inv_val &= ~(1 << bit);

	addr = TCA64XX_REG_ADDR(pin, sc->conf_reg);
	error = tca64xx_write(dev, addr, val);
	if (error != 0)
		goto fail;

	addr = TCA64XX_REG_ADDR(pin, sc->polarity_inv_reg);
	error = tca64xx_write(dev, addr, val);

fail:
	mtx_unlock(&sc->mtx);
	return (error);
}

static int
tca64xx_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	if (pin >= sc->num_pins || name == NULL)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "gpio_P%d%d", pin / TCA64XX_PINS_PER_REG,
	    pin % TCA64XX_PINS_PER_REG);

	return (0);
}

static int
tca64xx_pin_get(device_t dev, uint32_t pin, unsigned int *pval)
{
	uint8_t bit, addr, pins, reg_pvalue;
	int error;
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	pins = sc->num_pins;
	addr = TCA64XX_REG_ADDR(pin, sc->in_port_reg);
	bit = TCA64XX_BIT_FROM_PIN(pin);

	if (pin >= pins || pval == NULL)
		return (EINVAL);

	dbg_dev_printf(dev, "Reading pin %u pvalue.", pin);

	error = tca64xx_read(dev, addr, &reg_pvalue);
	if (error != 0)
		return (error);
	*pval = reg_pvalue & (1 << bit) ? 1 : 0;

	return (0);
}

static int
tca64xx_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	uint8_t bit, addr, pins, value;
	int error;
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	pins = sc->num_pins;
	addr = TCA64XX_REG_ADDR(pin, sc->out_port_reg);
	bit = TCA64XX_BIT_FROM_PIN(pin);

	if (pin >= pins)
		return (EINVAL);

	dbg_dev_printf(dev, "Setting pin: %u to %u\n", pin, val);

	mtx_lock(&sc->mtx);

	error = tca64xx_read(dev, addr, &value);
	if (error != 0) {
		mtx_unlock(&sc->mtx);
		dbg_dev_printf(dev, "Failed to read from register.\n");
		return (error);
	}

	if (val != 0)
		value |= (1 << bit);
	else
		value &= ~(1 << bit);

	error = tca64xx_write(dev, addr, value);
	if (error != 0) {
		mtx_unlock(&sc->mtx);
		dbg_dev_printf(dev, "Could not write to register.\n");
		return (error);
	}

	mtx_unlock(&sc->mtx);

	return (0);
}

static int
tca64xx_pin_toggle(device_t dev, uint32_t pin)
{
	int error;
	uint8_t bit, addr, pins, value;
	struct tca64xx_softc *sc;

	sc = device_get_softc(dev);

	pins = sc->num_pins;
	addr = TCA64XX_REG_ADDR(pin, sc->out_port_reg);
	bit = TCA64XX_BIT_FROM_PIN(pin);

	if (pin >= pins)
		return (EINVAL);

	dbg_dev_printf(dev, "Toggling pin: %d\n", pin);

	mtx_lock(&sc->mtx);

	error = tca64xx_read(dev, addr, &value);
	if (error != 0) {
		mtx_unlock(&sc->mtx);
		dbg_dev_printf(dev, "Cannot read from register.\n");
		return (error);
	}

	value ^= (1 << bit);

	error = tca64xx_write(dev, addr, value);
	if (error != 0) {
		mtx_unlock(&sc->mtx);
		dbg_dev_printf(dev, "Cannot write to register.\n");
		return (error);
	}

	mtx_unlock(&sc->mtx);

	return (0);
}

#ifdef DEBUG
static void
tca6416_regdump_setup(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *node;

	ctx = device_get_sysctl_ctx(dev);
	node = device_get_sysctl_tree(dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "in_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_IN_PORT_REG, tca64xx_regdump_sysctl, "A", "Input port 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "in_reg_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_IN_PORT_REG | 1, tca64xx_regdump_sysctl, "A",
	    "Input port 2");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "out_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_OUT_PORT_REG, tca64xx_regdump_sysctl, "A",
	    "Output port 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "out_reg_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_OUT_PORT_REG | 1, tca64xx_regdump_sysctl, "A",
	    "Output port 2");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "pol_inv_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_POLARITY_INV_REG, tca64xx_regdump_sysctl, "A",
	    "Polarity inv 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "pol_inv_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_POLARITY_INV_REG | 1, tca64xx_regdump_sysctl, "A",
	    "Polarity inv 2");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "conf_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_CONF_REG, tca64xx_regdump_sysctl, "A", "Configuration 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "conf_reg_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6416_CONF_REG | 1, tca64xx_regdump_sysctl, "A",
	    "Configuration 2");
}

static void
tca6408_regdump_setup(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *node;

	ctx = device_get_sysctl_ctx(dev);
	node = device_get_sysctl_tree(dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "in_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6408_IN_PORT_REG, tca64xx_regdump_sysctl, "A", "Input port 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "out_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6408_OUT_PORT_REG, tca64xx_regdump_sysctl, "A",
	    "Output port 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "pol_inv_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6408_POLARITY_INV_REG, tca64xx_regdump_sysctl,
	    "A", "Polarity inv 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "conf_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    TCA6408_CONF_REG, tca64xx_regdump_sysctl, "A", "Configuration 1");
}

static int
tca64xx_regdump_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	char buf[5];
	int len, error;
	uint8_t reg, regval;

	dev = (device_t)arg1;
	reg = (uint8_t)arg2;

	error = tca64xx_read(dev, reg, &regval);
	if (error != 0) {
		return (error);
	}

	len = snprintf(buf, 5, "0x%02x", regval);

	error = sysctl_handle_string(oidp, buf, len, req);

	return (error);
}
#endif
