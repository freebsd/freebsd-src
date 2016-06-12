/*-
 * Copyright (c) 2015-2016 Emmanuel Vadot <manu@freebsd.org>
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
* X-Power AXP209 PMU for Allwinner SoCs
*/
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/clock.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/gpio.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/gpio/gpiobusvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/allwinner/axp209reg.h>

#include "iicbus_if.h"
#include "gpio_if.h"

struct axp209_softc {
	device_t		dev;
	uint32_t		addr;
	struct resource *	res[1];
	void *			intrcookie;
	struct intr_config_hook	intr_hook;
	device_t		gpiodev;
	struct mtx		mtx;
};

/* GPIO3 is different, don't expose it for now */
static const struct {
	const char *name;
	uint8_t	ctrl_reg;
} axp209_pins[] = {
	{ "GPIO0", AXP209_GPIO0_CTRL },
	{ "GPIO1", AXP209_GPIO1_CTRL },
	{ "GPIO2", AXP209_GPIO2_CTRL },
};

static struct resource_spec axp_res_spec[] = {
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1,			0,	0 }
};

#define	AXP_LOCK(sc)	mtx_lock(&(sc)->mtx)
#define	AXP_UNLOCK(sc)	mtx_unlock(&(sc)->mtx)

static int
axp209_read(device_t dev, uint8_t reg, uint8_t *data, uint8_t size)
{
	struct axp209_softc *sc = device_get_softc(dev);
	struct iic_msg msg[2];

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
axp209_write(device_t dev, uint8_t reg, uint8_t data)
{
	uint8_t buffer[2];
	struct axp209_softc *sc = device_get_softc(dev);
	struct iic_msg msg;

	buffer[0] = reg;
	buffer[1] = data;

	msg.slave = sc->addr;
	msg.flags = IIC_M_WR;
	msg.len = 2;
	msg.buf = buffer;

	return (iicbus_transfer(dev, &msg, 1));
}

static int
axp209_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = arg1;
	enum axp209_sensor sensor = arg2;
	uint8_t data[2];
	int val, error;

	switch (sensor) {
	case AXP209_TEMP:
		error = axp209_read(dev, AXP209_TEMPMON, data, 2);
		if (error != 0)
			return (error);

		/* Temperature is between -144.7C and 264.8C, step +0.1C */
		val = (AXP209_SENSOR_H(data[0]) | AXP209_SENSOR_L(data[1])) -
		    AXP209_TEMPMON_MIN + AXP209_0C_TO_K;
		break;
	case AXP209_ACVOLT:
		error = axp209_read(dev, AXP209_ACIN_VOLTAGE, data, 2);
		if (error != 0)
			return (error);

		val = (AXP209_SENSOR_H(data[0]) | AXP209_SENSOR_L(data[1])) *
		    AXP209_VOLT_STEP;
		break;
	case AXP209_ACCURRENT:
		error = axp209_read(dev, AXP209_ACIN_CURRENT, data, 2);
		if (error != 0)
			return (error);

		val = (AXP209_SENSOR_H(data[0]) | AXP209_SENSOR_L(data[1])) *
		    AXP209_ACCURRENT_STEP;
		break;
	case AXP209_VBUSVOLT:
		error = axp209_read(dev, AXP209_VBUS_VOLTAGE, data, 2);
		if (error != 0)
			return (error);

		val = (AXP209_SENSOR_H(data[0]) | AXP209_SENSOR_L(data[1])) *
		    AXP209_VOLT_STEP;
		break;
	case AXP209_VBUSCURRENT:
		error = axp209_read(dev, AXP209_VBUS_CURRENT, data, 2);
		if (error != 0)
			return (error);

		val = (AXP209_SENSOR_H(data[0]) | AXP209_SENSOR_L(data[1])) *
		    AXP209_VBUSCURRENT_STEP;
		break;
	case AXP209_BATVOLT:
		error = axp209_read(dev, AXP209_BAT_VOLTAGE, data, 2);
		if (error != 0)
			return (error);

		val = (AXP209_SENSOR_H(data[0]) | AXP209_SENSOR_L(data[1])) *
		    AXP209_BATVOLT_STEP;
		break;
	case AXP209_BATCHARGECURRENT:
		error = axp209_read(dev, AXP209_BAT_CHARGE_CURRENT, data, 2);
		if (error != 0)
			return (error);

		val = (AXP209_SENSOR_H(data[0]) | AXP209_SENSOR_L(data[1])) *
		    AXP209_BATCURRENT_STEP;
		break;
	case AXP209_BATDISCHARGECURRENT:
		error = axp209_read(dev, AXP209_BAT_DISCHARGE_CURRENT, data, 2);
		if (error != 0)
			return (error);

		val = (AXP209_SENSOR_BAT_H(data[0]) |
		    AXP209_SENSOR_BAT_L(data[1])) * AXP209_BATCURRENT_STEP;
		break;
	default:
		return (ENOENT);
	}

	return sysctl_handle_opaque(oidp, &val, sizeof(val), req);
}

static void
axp209_shutdown(void *devp, int howto)
{
	device_t dev;

	if (!(howto & RB_POWEROFF))
		return;
	dev = (device_t)devp;

	if (bootverbose)
		device_printf(dev, "Shutdown AXP209\n");

	axp209_write(dev, AXP209_SHUTBAT, AXP209_SHUTBAT_SHUTDOWN);
}

static void
axp_intr(void *arg)
{
	struct axp209_softc *sc;
	uint8_t reg;

	sc = arg;

	axp209_read(sc->dev, AXP209_IRQ1_STATUS, &reg, 1);
	if (reg) {
		if (reg & AXP209_IRQ1_AC_OVERVOLT)
			devctl_notify("PMU", "AC", "overvoltage", NULL);
		if (reg & AXP209_IRQ1_VBUS_OVERVOLT)
			devctl_notify("PMU", "USB", "overvoltage", NULL);
		if (reg & AXP209_IRQ1_VBUS_LOW)
			devctl_notify("PMU", "USB", "undervoltage", NULL);
		if (reg & AXP209_IRQ1_AC_CONN)
			devctl_notify("PMU", "AC", "plugged", NULL);
		if (reg & AXP209_IRQ1_AC_DISCONN)
			devctl_notify("PMU", "AC", "unplugged", NULL);
		if (reg & AXP209_IRQ1_VBUS_CONN)
			devctl_notify("PMU", "USB", "plugged", NULL);
		if (reg & AXP209_IRQ1_VBUS_DISCONN)
			devctl_notify("PMU", "USB", "unplugged", NULL);
		axp209_write(sc->dev, AXP209_IRQ1_STATUS, AXP209_IRQ_ACK);
	}

	axp209_read(sc->dev, AXP209_IRQ2_STATUS, &reg, 1);
	if (reg) {
		if (reg & AXP209_IRQ2_BATT_CHARGED)
			devctl_notify("PMU", "Battery", "charged", NULL);
		if (reg & AXP209_IRQ2_BATT_CHARGING)
			devctl_notify("PMU", "Battery", "charging", NULL);
		if (reg & AXP209_IRQ2_BATT_CONN)
			devctl_notify("PMU", "Battery", "connected", NULL);
		if (reg & AXP209_IRQ2_BATT_DISCONN)
			devctl_notify("PMU", "Battery", "disconnected", NULL);
		if (reg & AXP209_IRQ2_BATT_TEMP_LOW)
			devctl_notify("PMU", "Battery", "low temp", NULL);
		if (reg & AXP209_IRQ2_BATT_TEMP_OVER)
			devctl_notify("PMU", "Battery", "high temp", NULL);
		axp209_write(sc->dev, AXP209_IRQ2_STATUS, AXP209_IRQ_ACK);
	}

	axp209_read(sc->dev, AXP209_IRQ3_STATUS, &reg, 1);
	if (reg) {
		if (reg & AXP209_IRQ3_PEK_SHORT)
			shutdown_nice(RB_POWEROFF);
		axp209_write(sc->dev, AXP209_IRQ3_STATUS, AXP209_IRQ_ACK);
	}

	axp209_read(sc->dev, AXP209_IRQ4_STATUS, &reg, 1);
	if (reg) {
		axp209_write(sc->dev, AXP209_IRQ4_STATUS, AXP209_IRQ_ACK);
	}

	axp209_read(sc->dev, AXP209_IRQ5_STATUS, &reg, 1);
	if (reg) {
		axp209_write(sc->dev, AXP209_IRQ5_STATUS, AXP209_IRQ_ACK);
	}
}

static device_t
axp209_gpio_get_bus(device_t dev)
{
	struct axp209_softc *sc;

	sc = device_get_softc(dev);

	return (sc->gpiodev);
}

static int
axp209_gpio_pin_max(device_t dev, int *maxpin)
{
	*maxpin = nitems(axp209_pins) - 1;

	return (0);
}

static int
axp209_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	if (pin >= nitems(axp209_pins))
		return (EINVAL);

	snprintf(name, GPIOMAXNAME, "%s", axp209_pins[pin].name);

	return (0);
}

static int
axp209_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	if (pin >= nitems(axp209_pins))
		return (EINVAL);

	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

	return (0);
}

static int
axp209_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct axp209_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp209_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp209_read(dev, axp209_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = data & AXP209_GPIO_FUNC_MASK;
		if (func == AXP209_GPIO_FUNC_INPUT)
			*flags = GPIO_PIN_INPUT;
		else if (func == AXP209_GPIO_FUNC_DRVLO ||
		    func == AXP209_GPIO_FUNC_DRVHI)
			*flags = GPIO_PIN_OUTPUT;
		else
			*flags = 0;
	}
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp209_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct axp209_softc *sc;
	uint8_t data;
	int error;

	if (pin >= nitems(axp209_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp209_read(dev, axp209_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		data &= ~AXP209_GPIO_FUNC_MASK;
		if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) != 0) {
			if ((flags & GPIO_PIN_OUTPUT) == 0)
				data |= AXP209_GPIO_FUNC_INPUT;
		}
		error = axp209_write(dev, axp209_pins[pin].ctrl_reg, data);
	}
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp209_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct axp209_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp209_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp209_read(dev, axp209_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = data & AXP209_GPIO_FUNC_MASK;
		switch (func) {
		case AXP209_GPIO_FUNC_DRVLO:
			*val = 0;
			break;
		case AXP209_GPIO_FUNC_DRVHI:
			*val = 1;
			break;
		case AXP209_GPIO_FUNC_INPUT:
			error = axp209_read(dev, AXP209_GPIO_STATUS, &data, 1);
			if (error == 0)
				*val = (data & AXP209_GPIO_DATA(pin)) ? 1 : 0;
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
axp209_gpio_pin_set(device_t dev, uint32_t pin, unsigned int val)
{
	struct axp209_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp209_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp209_read(dev, axp209_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = data & AXP209_GPIO_FUNC_MASK;
		switch (func) {
		case AXP209_GPIO_FUNC_DRVLO:
		case AXP209_GPIO_FUNC_DRVHI:
			/* GPIO2 can't be set to 1 */
			if (pin == 2 && val == 1) {
				error = EINVAL;
				break;
			}
			data &= ~AXP209_GPIO_FUNC_MASK;
			data |= val;
			break;
		default:
			error = EIO;
			break;
		}
	}
	if (error == 0)
		error = axp209_write(dev, axp209_pins[pin].ctrl_reg, data);
	AXP_UNLOCK(sc);

	return (error);
}


static int
axp209_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct axp209_softc *sc;
	uint8_t data, func;
	int error;

	if (pin >= nitems(axp209_pins))
		return (EINVAL);

	sc = device_get_softc(dev);

	AXP_LOCK(sc);
	error = axp209_read(dev, axp209_pins[pin].ctrl_reg, &data, 1);
	if (error == 0) {
		func = data & AXP209_GPIO_FUNC_MASK;
		switch (func) {
		case AXP209_GPIO_FUNC_DRVLO:
			/* Pin 2 can't be set to 1*/
			if (pin == 2) {
				error = EINVAL;
				break;
			}
			data &= ~AXP209_GPIO_FUNC_MASK;
			data |= AXP209_GPIO_FUNC_DRVHI;
			break;
		case AXP209_GPIO_FUNC_DRVHI:
			data &= ~AXP209_GPIO_FUNC_MASK;
			data |= AXP209_GPIO_FUNC_DRVLO;
			break;
		default:
			error = EIO;
			break;
		}
	}
	if (error == 0)
		error = axp209_write(dev, axp209_pins[pin].ctrl_reg, data);
	AXP_UNLOCK(sc);

	return (error);
}

static int
axp209_gpio_map_gpios(device_t bus, phandle_t dev, phandle_t gparent,
    int gcells, pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{
	if (gpios[0] >= nitems(axp209_pins))
		return (EINVAL);

	*pin = gpios[0];
	*flags = gpios[1];

	return (0);
}

static phandle_t
axp209_get_node(device_t dev, device_t bus)
{
	return (ofw_bus_get_node(dev));
}

static void
axp209_start(void *pdev)
{
	device_t dev;
	struct axp209_softc *sc;
	const char *pwr_name[] = {"Battery", "AC", "USB", "AC and USB"};
	uint8_t data;
	uint8_t pwr_src;

	dev = pdev;

	sc = device_get_softc(dev);
	sc->addr = iicbus_get_addr(dev);
	sc->dev = dev;

	if (bootverbose) {
		/*
		 * Read the Power State register.
		 * Shift the AC presence into bit 0.
		 * Shift the Battery presence into bit 1.
		 */
		axp209_read(dev, AXP209_PSR, &data, 1);
		pwr_src = ((data & AXP209_PSR_ACIN) >> AXP209_PSR_ACIN_SHIFT) |
		    ((data & AXP209_PSR_VBUS) >> (AXP209_PSR_VBUS_SHIFT - 1));

		device_printf(dev, "AXP209 Powered by %s\n",
		    pwr_name[pwr_src]);
	}

	/* Only enable interrupts that we are interested in */
	axp209_write(dev, AXP209_IRQ1_ENABLE,
	    AXP209_IRQ1_AC_OVERVOLT |
	    AXP209_IRQ1_AC_DISCONN |
	    AXP209_IRQ1_AC_CONN |
	    AXP209_IRQ1_VBUS_OVERVOLT |
	    AXP209_IRQ1_VBUS_DISCONN |
	    AXP209_IRQ1_VBUS_CONN);
	axp209_write(dev, AXP209_IRQ2_ENABLE,
	    AXP209_IRQ2_BATT_CONN |
	    AXP209_IRQ2_BATT_DISCONN |
	    AXP209_IRQ2_BATT_CHARGE_ACCT_ON |
	    AXP209_IRQ2_BATT_CHARGE_ACCT_OFF |
	    AXP209_IRQ2_BATT_CHARGING |
	    AXP209_IRQ2_BATT_CHARGED |
	    AXP209_IRQ2_BATT_TEMP_OVER |
	    AXP209_IRQ2_BATT_TEMP_LOW);
	axp209_write(dev, AXP209_IRQ3_ENABLE,
	    AXP209_IRQ3_PEK_SHORT | AXP209_IRQ3_PEK_LONG);
	axp209_write(dev, AXP209_IRQ4_ENABLE, AXP209_IRQ4_APS_LOW_2);
	axp209_write(dev, AXP209_IRQ5_ENABLE, 0x0);

	EVENTHANDLER_REGISTER(shutdown_final, axp209_shutdown, dev,
	    SHUTDOWN_PRI_LAST);

	/* Enable ADC sensors */
	if (axp209_write(dev, AXP209_ADC_ENABLE1,
	    AXP209_ADC1_BATVOLT | AXP209_ADC1_BATCURRENT |
	    AXP209_ADC1_ACVOLT | AXP209_ADC1_ACCURRENT |
	    AXP209_ADC1_VBUSVOLT | AXP209_ADC1_VBUSCURRENT) != -1) {
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "acvolt",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, AXP209_ACVOLT, axp209_sysctl, "I",
		    "AC Voltage (microVolt)");
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "accurrent",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, AXP209_ACCURRENT, axp209_sysctl, "I",
		    "AC Current (microAmpere)");
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "vbusvolt",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, AXP209_VBUSVOLT, axp209_sysctl, "I",
		    "VBUS Voltage (microVolt)");
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "vbuscurrent",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, AXP209_VBUSCURRENT, axp209_sysctl, "I",
		    "VBUS Current (microAmpere)");
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "batvolt",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, AXP209_BATVOLT, axp209_sysctl, "I",
		    "Battery Voltage (microVolt)");
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "batchargecurrent",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, AXP209_BATCHARGECURRENT, axp209_sysctl, "I",
		    "Battery Charging Current (microAmpere)");
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "batdischargecurrent",
		    CTLTYPE_INT | CTLFLAG_RD,
		    dev, AXP209_BATDISCHARGECURRENT, axp209_sysctl, "I",
		    "Battery Discharging Current (microAmpere)");
	} else {
		device_printf(dev, "Couldn't enable ADC sensors\n");
	}

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "temp",
	    CTLTYPE_INT | CTLFLAG_RD,
	    dev, AXP209_TEMP, axp209_sysctl, "IK", "Internal temperature");

	if ((bus_setup_intr(dev, sc->res[0], INTR_TYPE_MISC | INTR_MPSAFE,
	      NULL, axp_intr, sc, &sc->intrcookie)))
		device_printf(dev, "unable to register interrupt handler\n");

	config_intrhook_disestablish(&sc->intr_hook);
}

static int
axp209_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "x-powers,axp209"))
		return (ENXIO);

	device_set_desc(dev, "X-Powers AXP209 Power Management Unit");

	return (BUS_PROBE_DEFAULT);
}

static int
axp209_attach(device_t dev)
{
	struct axp209_softc *sc;

	sc = device_get_softc(dev);
	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, axp_res_spec, sc->res) != 0) {
		device_printf(dev, "can't allocate device resources\n");
		return (ENXIO);
	}

	sc->intr_hook.ich_func = axp209_start;
	sc->intr_hook.ich_arg = dev;

	if (config_intrhook_establish(&sc->intr_hook) != 0)
		return (ENOMEM);

	sc->gpiodev = gpiobus_attach_bus(dev);

	return (0);
}

static device_method_t axp209_methods[] = {
	DEVMETHOD(device_probe,		axp209_probe),
	DEVMETHOD(device_attach,	axp209_attach),

	/* GPIO interface */
	DEVMETHOD(gpio_get_bus,		axp209_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		axp209_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	axp209_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	axp209_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	axp209_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	axp209_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		axp209_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		axp209_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	axp209_gpio_pin_toggle),
	DEVMETHOD(gpio_map_gpios,	axp209_gpio_map_gpios),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_node,	axp209_get_node),

	DEVMETHOD_END
};

static driver_t axp209_driver = {
	"axp209_pmu",
	axp209_methods,
	sizeof(struct axp209_softc),
};

static devclass_t axp209_devclass;
extern devclass_t ofwgpiobus_devclass, gpioc_devclass;
extern driver_t ofw_gpiobus_driver, gpioc_driver;

EARLY_DRIVER_MODULE(axp209, iicbus, axp209_driver, axp209_devclass,
  0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(ofw_gpiobus, axp209_pmu, ofw_gpiobus_driver,
    ofwgpiobus_devclass, 0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(gpioc, axp209_pmu, gpioc_driver, gpioc_devclass,
    0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(axp209, 1);
MODULE_DEPEND(axp209, iicbus, 1, 1, 1);
