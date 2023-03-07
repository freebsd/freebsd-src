/*-
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
 * MAX77620 PMIC driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/sx.h>

#include <machine/bus.h>

#include <dev/extres/regulator/regulator.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/mfd/max77620.h>

#include "clock_if.h"
#include "regdev_if.h"

#include "max77620.h"

static struct ofw_compat_data compat_data[] = {
	{"maxim,max77620",	1},
	{NULL,			0},
};

#define	LOCK(_sc)		sx_xlock(&(_sc)->lock)
#define	UNLOCK(_sc)		sx_xunlock(&(_sc)->lock)
#define	LOCK_INIT(_sc)		sx_init(&(_sc)->lock, "max77620")
#define	LOCK_DESTROY(_sc)	sx_destroy(&(_sc)->lock);
#define	ASSERT_LOCKED(_sc)	sx_assert(&(_sc)->lock, SA_XLOCKED);
#define	ASSERT_UNLOCKED(_sc)	sx_assert(&(_sc)->lock, SA_UNLOCKED);

#define	MAX77620_DEVICE_ID	0x0C

/*
 * Raw register access function.
 */
int
max77620_read(struct max77620_softc *sc, uint8_t reg, uint8_t *val)
{
	uint8_t addr;
	int rv;
	struct iic_msg msgs[2] = {
		{0, IIC_M_WR, 1, &addr},
		{0, IIC_M_RD, 1, val},
	};

	msgs[0].slave = sc->bus_addr;
	msgs[1].slave = sc->bus_addr;
	addr = reg;

	rv = iicbus_transfer(sc->dev, msgs, 2);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when reading reg 0x%02X, rv: %d\n", reg,  rv);
		return (EIO);
	}

	return (0);
}

int max77620_read_buf(struct max77620_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size)
{
	uint8_t addr;
	int rv;
	struct iic_msg msgs[2] = {
		{0, IIC_M_WR, 1, &addr},
		{0, IIC_M_RD, size, buf},
	};

	msgs[0].slave = sc->bus_addr;
	msgs[1].slave = sc->bus_addr;
	addr = reg;

	rv = iicbus_transfer(sc->dev, msgs, 2);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when reading reg 0x%02X, rv: %d\n", reg,  rv);
		return (EIO);
	}

	return (0);
}

int
max77620_write(struct max77620_softc *sc, uint8_t reg, uint8_t val)
{
	uint8_t data[2];
	int rv;

	struct iic_msg msgs[1] = {
		{0, IIC_M_WR, 2, data},
	};

	msgs[0].slave = sc->bus_addr;
	data[0] = reg;
	data[1] = val;

	rv = iicbus_transfer(sc->dev, msgs, 1);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when writing reg 0x%02X, rv: %d\n", reg, rv);
		return (EIO);
	}
	return (0);
}

int
max77620_write_buf(struct max77620_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size)
{
	uint8_t data[1];
	int rv;
	struct iic_msg msgs[2] = {
		{0, IIC_M_WR, 1, data},
		{0, IIC_M_WR | IIC_M_NOSTART, size, buf},
	};

	msgs[0].slave = sc->bus_addr;
	msgs[1].slave = sc->bus_addr;
	data[0] = reg;

	rv = iicbus_transfer(sc->dev, msgs, 2);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Error when writing reg 0x%02X, rv: %d\n", reg, rv);
		return (EIO);
	}
	return (0);
}

int
max77620_modify(struct max77620_softc *sc, uint8_t reg, uint8_t clear,
    uint8_t set)
{
	uint8_t val;
	int rv;

	rv = max77620_read(sc, reg, &val);
	if (rv != 0)
		return (rv);

	val &= ~clear;
	val |= set;

	rv = max77620_write(sc, reg, val);
	if (rv != 0)
		return (rv);

	return (0);
}

static int
max77620_parse_fps(struct max77620_softc *sc, int id, phandle_t node)
{
	int val;

	if (OF_getencprop(node, "maxim,shutdown-fps-time-period-us", &val,
	    sizeof(val)) >= 0) {
		val = min(val, MAX77620_FPS_PERIOD_MAX_US);
		val = max(val, MAX77620_FPS_PERIOD_MIN_US);
		sc->shutdown_fps[id] = val;
	}
	if (OF_getencprop(node, "maxim,suspend-fps-time-period-us", &val,
	    sizeof(val)) >= 0) {
		val = min(val, MAX77620_FPS_PERIOD_MAX_US);
		val = max(val, MAX77620_FPS_PERIOD_MIN_US);
		sc->suspend_fps[id] = val;
	}
	if (OF_getencprop(node, "maxim,fps-event-source", &val,
	    sizeof(val)) >= 0) {
		if (val > 2) {
			device_printf(sc->dev, "Invalid 'fps-event-source' "
			    "value: %d\n", val);
			return (EINVAL);
		}
		sc->event_source[id] = val;
	}
	return (0);
}

static int
max77620_parse_fdt(struct max77620_softc *sc, phandle_t node)
{
	 phandle_t fpsnode;
	 char fps_name[6];
	 int i, rv;

	for (i = 0; i < MAX77620_FPS_COUNT; i++) {
		sc->shutdown_fps[i] = -1;
		sc->suspend_fps[i] = -1;
		sc->event_source[i] = -1;
	}

	fpsnode = ofw_bus_find_child(node, "fps");
	if (fpsnode > 0) {
		for (i = 0; i < MAX77620_FPS_COUNT; i++) {
			sprintf(fps_name, "fps%d", i);
			node = ofw_bus_find_child(node, fps_name);
			if (node <= 0)
				continue;
			rv = max77620_parse_fps(sc, i, node);
			if (rv != 0)
				return (rv);
		}
	}
	return (0);
}

static int
max77620_get_version(struct max77620_softc *sc)
{
	uint8_t buf[6];
	int i;
	int rv;

	/* Verify ID string (5 bytes ). */
	for (i = 0; i <= 6; i++) {
		rv = RD1(sc, MAX77620_REG_CID0 + i , buf + i);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot read chip ID: %d\n", rv);
			return (ENXIO);
		}
	}
	if (bootverbose) {
		device_printf(sc->dev,
		    " ID: [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n",
		    buf[0], buf[1], buf[2], buf[3]);
	}
	device_printf(sc->dev, " MAX77620 version - OTP: 0x%02X, ES: 0x%02X\n",
	    buf[4], buf[5]);

	return (0);
}

static uint8_t
max77620_encode_fps_period(struct max77620_softc *sc, int val)
{
	uint8_t i;
	int period;

	period = MAX77620_FPS_PERIOD_MIN_US;
	for (i = 0; i < 7; i++) {
		if (period >= val)
			return (i);
		period *= 2;
	}
	return (i);
}

static int
max77620_init(struct max77620_softc *sc)
{
	uint8_t mask, val, tmp;
	int i, rv;

	mask = 0;
	val = 0;
	for (i = 0; i < MAX77620_FPS_COUNT; i++) {
		if (sc->shutdown_fps[i] != -1) {
			mask |= MAX77620_FPS_TIME_PERIOD_MASK;
			tmp  = max77620_encode_fps_period(sc,
			    sc->shutdown_fps[i]);
			val |= (tmp << MAX77620_FPS_TIME_PERIOD_SHIFT) &
			    MAX77620_FPS_TIME_PERIOD_MASK;
		}

		if (sc->event_source[i] != -1) {
			mask |= MAX77620_FPS_EN_SRC_MASK;
			tmp = sc->event_source[i];
			val |= (tmp << MAX77620_FPS_EN_SRC_SHIFT) &
			    MAX77620_FPS_EN_SRC_MASK;
			if (sc->event_source[i] == 2) {
				mask |= MAX77620_FPS_ENFPS_SW_MASK;
				val |= MAX77620_FPS_ENFPS_SW;
			}

		}
		rv = RM1(sc, MAX77620_REG_FPS_CFG0 + i, mask, val);
		if (rv != 0) {
			device_printf(sc->dev, "I/O error: %d\n", rv);
			return (ENXIO);
		}
	}

	/* Global mask interrupts */
	rv = RM1(sc, MAX77620_REG_INTENLBT, 0x81, 0x81);
	rv = RM1(sc, MAX77620_REG_IRQTOPM, 0x81, 0x81);
	if (rv != 0)
		return (ENXIO);
	return (0);
}
#ifdef notyet
static void
max77620_intr(void *arg)
{
	struct max77620_softc *sc;
	uint8_t intenlbt, intlbt, irqtop, irqtopm, irqsd, irqmasksd;
	uint8_t irq_lvl2_l0_7, irq_lvl2_l8, irq_lvl2_gpio, irq_msk_l0_7, irq_msk_l8;
	uint8_t onoffirq, onoffirqm;

	sc = (struct max77620_softc *)arg;
	/* XXX Finish temperature alarms. */
	RD1(sc, MAX77620_REG_INTENLBT, &intenlbt);
	RD1(sc, MAX77620_REG_INTLBT, &intlbt);

	RD1(sc, MAX77620_REG_IRQTOP, &irqtop);
	RD1(sc, MAX77620_REG_IRQTOPM, &irqtopm);
	RD1(sc, MAX77620_REG_IRQSD, &irqsd);
	RD1(sc, MAX77620_REG_IRQMASKSD, &irqmasksd);
	RD1(sc, MAX77620_REG_IRQ_LVL2_L0_7, &irq_lvl2_l0_7);
	RD1(sc, MAX77620_REG_IRQ_MSK_L0_7, &irq_msk_l0_7);
	RD1(sc, MAX77620_REG_IRQ_LVL2_L8, &irq_lvl2_l8);
	RD1(sc, MAX77620_REG_IRQ_MSK_L8, &irq_msk_l8);
	RD1(sc, MAX77620_REG_IRQ_LVL2_GPIO, &irq_lvl2_gpio);
	RD1(sc, MAX77620_REG_ONOFFIRQ, &onoffirq);
	RD1(sc, MAX77620_REG_ONOFFIRQM, &onoffirqm);
	printf("%s: intlbt: 0x%02X, intenlbt: 0x%02X\n", __func__, intlbt, intenlbt);
	printf("%s: irqtop: 0x%02X, irqtopm: 0x%02X\n", __func__, irqtop, irqtopm);
	printf("%s: irqsd: 0x%02X, irqmasksd: 0x%02X\n", __func__, irqsd, irqmasksd);
	printf("%s: onoffirq: 0x%02X, onoffirqm: 0x%02X\n", __func__, onoffirq, onoffirqm);
	printf("%s: irq_lvl2_l0_7: 0x%02X, irq_msk_l0_7: 0x%02X\n", __func__, irq_lvl2_l0_7, irq_msk_l0_7);
	printf("%s: irq_lvl2_l8: 0x%02X, irq_msk_l8: 0x%02X\n", __func__, irq_lvl2_l8, irq_msk_l8);
	printf("%s: irq_lvl2_gpio: 0x%02X\n", __func__, irq_lvl2_gpio);
}
#endif

static int
max77620_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "MAX77620 PMIC");
	return (BUS_PROBE_DEFAULT);
}

static int
max77620_attach(device_t dev)
{
	struct max77620_softc *sc;
	int rv, rid;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->bus_addr = iicbus_get_addr(dev);
	node = ofw_bus_get_node(sc->dev);
	rv = 0;
	LOCK_INIT(sc);

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
#ifdef notyet /* Interrupt parent is not implemented */
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate interrupt.\n");
		rv = ENXIO;
		goto fail;
	}
#endif
	rv = max77620_parse_fdt(sc, node);
	if (rv != 0)
		goto fail;

	rv = max77620_get_version(sc);
	if (rv != 0)
		goto fail;

	rv = max77620_init(sc);
	if (rv != 0)
		goto fail;
	rv = max77620_regulator_attach(sc, node);
	if (rv != 0)
		goto fail;
	rv = max77620_gpio_attach(sc, node);
	if (rv != 0)
		goto fail;

	rv = max77620_rtc_create(sc, node);
	if (rv != 0)
		goto fail;

	fdt_pinctrl_register(dev, NULL);
	fdt_pinctrl_configure_by_name(dev, "default");

	/* Setup interrupt. */
#ifdef notyet
	rv = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, max77620_intr, sc, &sc->irq_h);
	if (rv) {
		device_printf(dev, "Cannot setup interrupt.\n");
		goto fail;
	}
#endif
	return (bus_generic_attach(dev));

fail:
	if (sc->irq_h != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_h);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	LOCK_DESTROY(sc);
	return (rv);
}

static int
max77620_detach(device_t dev)
{
	struct max77620_softc *sc;

	sc = device_get_softc(dev);
	if (sc->irq_h != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_h);
	if (sc->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	LOCK_DESTROY(sc);

	return (bus_generic_detach(dev));
}

static phandle_t
max77620_gpio_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the GPIO bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static device_method_t max77620_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		max77620_probe),
	DEVMETHOD(device_attach,	max77620_attach),
	DEVMETHOD(device_detach,	max77620_detach),

	/* Regdev interface */
	DEVMETHOD(regdev_map,		max77620_regulator_map),

	/* GPIO protocol interface */
	DEVMETHOD(gpio_get_bus,		max77620_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		max77620_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	max77620_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	max77620_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	max77620_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	max77620_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		max77620_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		max77620_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	max77620_gpio_pin_toggle),
	DEVMETHOD(gpio_map_gpios,	max77620_gpio_map_gpios),

	/* fdt_pinctrl interface */
	DEVMETHOD(fdt_pinctrl_configure, max77620_pinmux_configure),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	max77620_gpio_get_node),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(gpio, max77620_driver, max77620_methods,
    sizeof(struct max77620_softc));
EARLY_DRIVER_MODULE(max77620, iicbus, max77620_driver, NULL, NULL, 74);
