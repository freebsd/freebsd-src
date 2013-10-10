/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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
* TPS65217 PMIC companion chip for AM335x SoC sitting on I2C bus
*/
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/clock.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "iicbus_if.h"

#define TPS65217A		0x7
#define TPS65217B		0xF
#define TPS65217C		0xE
#define TPS65217D		0x6

/* TPS65217 Reisters */
#define TPS65217_CHIPID_REG	0x00
#define TPS65217_STATUS_REG	0x0A

#define MAX_IIC_DATA_SIZE	2


struct am335x_pmic_softc {
	device_t		sc_dev;
	uint32_t		sc_addr;
	struct intr_config_hook enum_hook;
};

static int
am335x_pmic_read(device_t dev, uint8_t addr, uint8_t *data, uint8_t size)
{
	struct am335x_pmic_softc *sc = device_get_softc(dev);
	struct iic_msg msg[] = {
		{ sc->sc_addr, IIC_M_WR, 1, &addr },
		{ sc->sc_addr, IIC_M_RD, size, data },
	};
	return (iicbus_transfer(dev, msg, 2));
}

#ifdef notyet
static int
am335x_pmic_write(device_t dev, uint8_t address, uint8_t *data, uint8_t size)
{
	uint8_t buffer[MAX_IIC_DATA_SIZE + 1];
	struct am335x_pmic_softc *sc = device_get_softc(dev);
	struct iic_msg msg[] = {
		{ sc->sc_addr, IIC_M_WR, size + 1, buffer },
	};

	if (size > MAX_IIC_DATA_SIZE)
		return (ENOMEM);

	buffer[0] = address;
	memcpy(buffer + 1, data, size);

	return (iicbus_transfer(dev, msg, 1));
}
#endif

static int
am335x_pmic_probe(device_t dev)
{
	struct am335x_pmic_softc *sc;

	if (!ofw_bus_is_compatible(dev, "ti,am335x-pmic"))
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	device_set_desc(dev, "TI TPS65217 Power Management IC");

	return (0);
}

static void
am335x_pmic_start(void *xdev)
{
	struct am335x_pmic_softc *sc;
	device_t dev = (device_t)xdev;
	uint8_t reg;
	char name[20];
	char pwr[4][11] = {"Unknown", "USB", "AC", "USB and AC"};

	sc = device_get_softc(dev);

	am335x_pmic_read(dev, TPS65217_CHIPID_REG, &reg, 1);
	switch (reg>>4) {
		case TPS65217A:
			sprintf(name, "TPS65217A ver 1.%u", reg & 0xF);
			break;
		case TPS65217B:
			sprintf(name, "TPS65217B ver 1.%u", reg & 0xF);
			break;
		case TPS65217C:
			sprintf(name, "TPS65217C ver 1.%u", reg & 0xF);
			break;
		case TPS65217D:
			sprintf(name, "TPS65217D ver 1.%u", reg & 0xF);
			break;
		default:
			sprintf(name, "Unknown PMIC");
	}

	am335x_pmic_read(dev, TPS65217_STATUS_REG, &reg, 1);
	device_printf(dev, "%s powered by %s\n", name, pwr[(reg>>2)&0x03]);

	config_intrhook_disestablish(&sc->enum_hook);
}

static int
am335x_pmic_attach(device_t dev)
{
	struct am335x_pmic_softc *sc;

	sc = device_get_softc(dev);

	sc->enum_hook.ich_func = am335x_pmic_start;
	sc->enum_hook.ich_arg = dev;

	if (config_intrhook_establish(&sc->enum_hook) != 0)
		return (ENOMEM);

	return (0);
}

static device_method_t am335x_pmic_methods[] = {
	DEVMETHOD(device_probe,		am335x_pmic_probe),
	DEVMETHOD(device_attach,	am335x_pmic_attach),
	{0, 0},
};

static driver_t am335x_pmic_driver = {
	"am335x_pmic",
	am335x_pmic_methods,
	sizeof(struct am335x_pmic_softc),
};

static devclass_t am335x_pmic_devclass;

DRIVER_MODULE(am335x_pmic, iicbus, am335x_pmic_driver, am335x_pmic_devclass, 0, 0);
MODULE_VERSION(am335x_pmic, 1);
MODULE_DEPEND(am335x_pmic, iicbus, 1, 1, 1);
