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
 * Driver for TI TCA6416 I2C GPIO expander module.
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
#define	IN_PORT_REG		0x00
#define	OUT_PORT_REG		0x02
#define	POLARITY_INV_REG	0x04
#define	CONF_REG		0x06

#define	NUM_PINS	16
#define	PINS_PER_REG	8
#define	PIN_CAPS				\
	(GPIO_PIN_OUTPUT | GPIO_PIN_INPUT	\
	| GPIO_PIN_PUSHPULL | GPIO_PIN_INVIN)

#ifdef DEBUG
#define	dbg_dev_printf(dev, fmt, args...)	\
	device_printf(dev, fmt, ##args)
#else
#define	dbg_dev_printf(dev, fmt, args...)
#endif

#define	TCA6416_BIT_FROM_PIN(pin)	(pin % PINS_PER_REG)
#define	TCA6416_REG_ADDR(pin, baseaddr)	(baseaddr | (pin / PINS_PER_REG))

struct tca6416_softc {
	device_t	dev;
	device_t	busdev;
	struct mtx	mtx;
	uint32_t	addr;
};

static int tca6416_read(device_t, uint8_t, uint8_t*);
static int tca6416_write(device_t, uint8_t, uint8_t);
static int tca6416_probe(device_t);
static int tca6416_attach(device_t);
static int tca6416_detach(device_t);
static device_t tca6416_get_bus(device_t);
static int tca6416_pin_max(device_t, int*);
static int tca6416_pin_getcaps(device_t, uint32_t, uint32_t*);
static int tca6416_pin_getflags(device_t, uint32_t, uint32_t*);
static int tca6416_pin_setflags(device_t, uint32_t, uint32_t);
static int tca6416_pin_getname(device_t, uint32_t, char*);
static int tca6416_pin_get(device_t, uint32_t, unsigned int*);
static int tca6416_pin_set(device_t, uint32_t, unsigned int);
static int tca6416_pin_toggle(device_t, uint32_t);
#ifdef DEBUG
static void tca6416_regdump_setup(device_t dev);
static int tca6416_regdump_sysctl(SYSCTL_HANDLER_ARGS);
#endif

static device_method_t tca6416_methods[] = {
	DEVMETHOD(device_probe,		tca6416_probe),
	DEVMETHOD(device_attach,	tca6416_attach),
	DEVMETHOD(device_detach,	tca6416_detach),

	/* GPIO methods */
	DEVMETHOD(gpio_get_bus,		tca6416_get_bus),
	DEVMETHOD(gpio_pin_max,		tca6416_pin_max),
	DEVMETHOD(gpio_pin_getcaps,	tca6416_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	tca6416_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	tca6416_pin_setflags),
	DEVMETHOD(gpio_pin_getname,	tca6416_pin_getname),
	DEVMETHOD(gpio_pin_get,		tca6416_pin_get),
	DEVMETHOD(gpio_pin_set,		tca6416_pin_set),
	DEVMETHOD(gpio_pin_toggle,	tca6416_pin_toggle),

	DEVMETHOD_END
};

static driver_t tca6416_driver = {
	"gpio",
	tca6416_methods,
	sizeof(struct tca6416_softc)
};

DRIVER_MODULE(tca6416, iicbus, tca6416_driver, 0, 0);
MODULE_VERSION(tca6416, 1);

static struct ofw_compat_data compat_data[] = {
	{"ti,tca6416",	1},
	{"ti,tca9539",	1},
	{0,0}
};

static int
tca6416_read(device_t dev, uint8_t reg, uint8_t *data)
{
	struct iic_msg msgs[2];
	struct tca6416_softc *sc;
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
tca6416_write(device_t dev, uint8_t reg, uint8_t val)
{
	struct iic_msg msg;
	struct tca6416_softc *sc;
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
tca6416_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TCA6416 I/O expander");
	return (BUS_PROBE_DEFAULT);
}

static int
tca6416_attach(device_t dev)
{
	struct tca6416_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->addr = iicbus_get_addr(dev);

	mtx_init(&sc->mtx, "tca6416 gpio", "gpio", MTX_DEF);

	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "Could not create busdev child\n");
		return (ENXIO);
	}

#ifdef DEBUG
	tca6416_regdump_setup(dev);
#endif

	return (0);
}

static int
tca6416_detach(device_t dev)
{
	struct tca6416_softc *sc;

	sc = device_get_softc(dev);

	if (sc->busdev != NULL)
		gpiobus_detach_bus(sc->busdev);

	mtx_destroy(&sc->mtx);

	return (0);
}

static device_t
tca6416_get_bus(device_t dev)
{
	struct tca6416_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
tca6416_pin_max(device_t dev __unused, int *maxpin)
{

	if (maxpin == NULL)
		return (EINVAL);

	*maxpin = NUM_PINS - 1;
	return (0);
}

static int
tca6416_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{

	if (pin >= NUM_PINS || caps == NULL)
		return (EINVAL);

	*caps = PIN_CAPS;
	return (0);
}

static int
tca6416_pin_getflags(device_t dev, uint32_t pin, uint32_t *pflags)
{
	int error;
	uint8_t reg_addr, reg_bit, val;

	if (pin >= NUM_PINS || pflags == NULL)
		return (EINVAL);

	reg_addr = TCA6416_REG_ADDR(pin, CONF_REG);
	reg_bit = TCA6416_BIT_FROM_PIN(pin);

	error = tca6416_read(dev, reg_addr, &val);
	if (error != 0)
		return (error);

	*pflags = (val & (1 << reg_bit))
	    ? GPIO_PIN_INPUT : GPIO_PIN_OUTPUT;

	reg_addr = TCA6416_REG_ADDR(pin, POLARITY_INV_REG);

	error = tca6416_read(dev, reg_addr, &val);
	if (error != 0)
		return (error);

	if (val & (1 << reg_bit))
		*pflags |= GPIO_PIN_INVIN;

	return (0);
}

static int
tca6416_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	uint8_t reg_addr, inv_reg_addr, reg_bit, val, inv_val;
	struct tca6416_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if (pin >= NUM_PINS)
		return (EINVAL);

	reg_addr = TCA6416_REG_ADDR(pin, CONF_REG);
	inv_reg_addr = TCA6416_REG_ADDR(pin, POLARITY_INV_REG);
	reg_bit = TCA6416_BIT_FROM_PIN(pin);

	mtx_lock(&sc->mtx);

	error = tca6416_read(dev, reg_addr, &val);
	if (error != 0)
		goto fail;
	error = tca6416_read(dev, inv_reg_addr, &inv_val);
	if (error != 0)
		goto fail;

	if (flags & GPIO_PIN_INPUT)
		val |= (1 << reg_bit);
	else if (flags & GPIO_PIN_OUTPUT)
		val &= ~(1 << reg_bit);

	if (flags & GPIO_PIN_INVIN)
		inv_val |= (1 << reg_bit);
	else
		inv_val &= ~(1 << reg_bit);

	error = tca6416_write(dev, reg_addr, val);
	if (error != 0)
		goto fail;
	error = tca6416_write(dev, inv_reg_addr, val);

fail:
	mtx_unlock(&sc->mtx);
	return (error);
}

static int
tca6416_pin_getname(device_t dev, uint32_t pin, char *name)
{

	if (pin >= NUM_PINS || name == NULL)
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "gpio_P%d%d", pin / PINS_PER_REG,
	    pin % PINS_PER_REG);

	return (0);
}

static int
tca6416_pin_get(device_t dev, uint32_t pin, unsigned int *pval)
{
	uint8_t reg_bit, reg_addr, reg_pvalue;
	int error;

	if (pin >= NUM_PINS || pval == NULL)
		return (EINVAL);

	reg_bit = TCA6416_BIT_FROM_PIN(pin);
	reg_addr = TCA6416_REG_ADDR(pin, IN_PORT_REG);

	dbg_dev_printf(dev, "Reading pin %u pvalue.", pin);

	error = tca6416_read(dev, reg_addr, &reg_pvalue);
	if (error != 0)
		return (error);

	*pval = reg_pvalue & (1 << reg_bit) ? 1 : 0;

	return (0);
}

static int
tca6416_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct tca6416_softc *sc;
	uint8_t reg_addr, reg_bit, reg_value;
	int error;

	sc = device_get_softc(dev);

	if (pin >= NUM_PINS)
		return (EINVAL);

	reg_addr = TCA6416_REG_ADDR(pin , OUT_PORT_REG);
	reg_bit = TCA6416_BIT_FROM_PIN(pin);

	dbg_dev_printf(dev, "Setting pin: %u to %u\n", pin, val);

	mtx_lock(&sc->mtx);

	error = tca6416_read(dev, reg_addr, &reg_value);
	if (error != 0) {
		dbg_dev_printf(dev, "Failed to read from register.\n");
		mtx_unlock(&sc->mtx);
		return (error);
	}

	if (val != 0)
		reg_value |= (1 << reg_bit);
	else
		reg_value &= ~(1 << reg_bit);


	error = tca6416_write(dev, reg_addr, reg_value);
	if (error != 0) {
		dbg_dev_printf(dev, "Could not write to register.\n");
		mtx_unlock(&sc->mtx);
		return (error);
	}

	mtx_unlock(&sc->mtx);

	return (0);
}

static int
tca6416_pin_toggle(device_t dev, uint32_t pin)
{
	struct tca6416_softc *sc;
	int error;
	uint8_t reg_addr, reg_bit, reg_value;

	sc = device_get_softc(dev);

	if (pin >= NUM_PINS)
		return (EINVAL);

	reg_addr = TCA6416_REG_ADDR(pin, OUT_PORT_REG);
	reg_bit = TCA6416_BIT_FROM_PIN(pin);

	dbg_dev_printf(dev, "Toggling pin: %d\n", pin);

	mtx_lock(&sc->mtx);

	error = tca6416_read(dev, reg_addr, &reg_value);
	if (error != 0) {
		mtx_unlock(&sc->mtx);
		dbg_dev_printf(dev, "Cannot read from register.\n");
		return (error);
	}

	reg_value ^= (1 << reg_bit);

	error = tca6416_write(dev, reg_addr, reg_value);
	if (error != 0)
		dbg_dev_printf(dev, "Cannot write to register.\n");

	mtx_unlock(&sc->mtx);

	return (error);
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
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, IN_PORT_REG,
	    tca6416_regdump_sysctl, "A", "Input port 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "in_reg_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    IN_PORT_REG | 1, tca6416_regdump_sysctl, "A", "Input port 2");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "out_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, OUT_PORT_REG,
	    tca6416_regdump_sysctl, "A", "Output port 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "out_reg_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev, OUT_PORT_REG
	    | 1, tca6416_regdump_sysctl, "A", "Output port 2");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "pol_inv_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    POLARITY_INV_REG, tca6416_regdump_sysctl, "A", "Polarity inv 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "pol_inv_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    POLARITY_INV_REG | 1, tca6416_regdump_sysctl, "A",
	    "Polarity inv 2");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "conf_reg_1",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    CONF_REG, tca6416_regdump_sysctl, "A", "Configuration 1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(node), OID_AUTO, "conf_reg_2",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
	    CONF_REG | 1, tca6416_regdump_sysctl, "A", "Configuration 2");
}

static int
tca6416_regdump_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	char buf[5];
	int len, error;
	uint8_t reg, regval;

	dev = (device_t)arg1;
	reg = (uint8_t)arg2;

	error = tca6416_read(dev, reg, &regval);
	if (error != 0) {
		return (error);
	}

	len = snprintf(buf, 5, "0x%02x", regval);

	error = sysctl_handle_string(oidp, buf, len, req);

	return (error);
}
#endif
