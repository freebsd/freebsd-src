/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018-2021 Emmanuel Vadot <manu@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/pmic/rockchip/rk805reg.h>
#include <dev/iicbus/pmic/rockchip/rk808reg.h>
#include <dev/iicbus/pmic/rockchip/rk8xx.h>

#include "clock_if.h"
#include "regdev_if.h"

int
rk8xx_read(device_t dev, uint8_t reg, uint8_t *data, uint8_t size)
{
	int err;

	err = iicdev_readfrom(dev, reg, data, size, IIC_INTRWAIT);
	return (err);
}

int
rk8xx_write(device_t dev, uint8_t reg, uint8_t *data, uint8_t size)
{

	return (iicdev_writeto(dev, reg, data, size, IIC_INTRWAIT));
}

static void
rk8xx_start(void *pdev)
{
	struct rk8xx_softc *sc;
	device_t dev;
	uint8_t data[2];
	int err;

	dev = pdev;
	sc = device_get_softc(dev);
	sc->dev = dev;

	/* No version register in RK808 */
	if (bootverbose && sc->type == RK805) {
		err = rk8xx_read(dev, RK805_CHIP_NAME, data, 1);
		if (err != 0) {
			device_printf(dev, "Cannot read chip name reg\n");
			return;
		}
		err = rk8xx_read(dev, RK805_CHIP_VER, data + 1, 1);
		if (err != 0) {
			device_printf(dev, "Cannot read chip version reg\n");
			return;
		}
		device_printf(dev, "Chip Name: %x\n",
		    data[0] << 4 | ((data[1] >> 4) & 0xf));
		device_printf(dev, "Chip Version: %x\n", data[1] & 0xf);
	}

	/* Register this as a 1Hz clock */
	clock_register(dev, 1000000);

	config_intrhook_disestablish(&sc->intr_hook);
}

int
rk8xx_attach(struct rk8xx_softc *sc)
{
	int error;

	error = rk8xx_attach_clocks(sc);
	if (error != 0)
		return (error);

	sc->intr_hook.ich_func = rk8xx_start;
	sc->intr_hook.ich_arg = sc->dev;
	if (config_intrhook_establish(&sc->intr_hook) != 0)
		return (ENOMEM);

	rk8xx_attach_regulators(sc);

	return (0);
}

static int
rk8xx_detach(device_t dev)
{

	/* We cannot detach regulators */
	return (EBUSY);
}

static device_method_t rk8xx_methods[] = {
	DEVMETHOD(device_detach,	rk8xx_detach),

	/* regdev interface */
	DEVMETHOD(regdev_map,		rk8xx_map),

	/* Clock interface */
	DEVMETHOD(clock_gettime,	rk8xx_gettime),
	DEVMETHOD(clock_settime,	rk8xx_settime),

	DEVMETHOD_END
};

DEFINE_CLASS_0(rk8xx, rk8xx_driver, rk8xx_methods,
    sizeof(struct rk8xx_softc));
