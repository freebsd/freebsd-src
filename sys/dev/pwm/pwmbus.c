/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pwm/pwmbus.h>

#include "pwmbus_if.h"
#include "pwm_if.h"

struct pwmbus_channel_data {
	int	reserved;
	char	*name;
};

struct pwmbus_softc {
	device_t	busdev;
	device_t	dev;

	int		nchannels;
};

static int
pwmbus_probe(device_t dev)
{

	device_set_desc(dev, "PWM bus");
	return (BUS_PROBE_GENERIC);
}

static int
pwmbus_attach(device_t dev)
{
	struct pwmbus_softc *sc;

	sc = device_get_softc(dev);
	sc->busdev = dev;
	sc->dev = device_get_parent(dev);

	if (PWM_CHANNEL_MAX(sc->dev, &sc->nchannels) != 0 ||
	    sc->nchannels == 0)
		return (ENXIO);

	if (bootverbose)
		device_printf(dev, "Registering %d channel(s)\n", sc->nchannels);
	bus_generic_probe(dev);

	return (bus_generic_attach(dev));
}

static int
pwmbus_detach(device_t dev)
{
	int rv;

	if ((rv = bus_generic_detach(dev)) == 0)
		rv = device_delete_children(dev);

	return (rv);
}

static int
pwmbus_channel_config(device_t bus, int channel, unsigned int period, unsigned int duty)
{
	struct pwmbus_softc *sc;

	sc = device_get_softc(bus);

	if (channel > sc->nchannels)
		return (EINVAL);

	return (PWM_CHANNEL_CONFIG(sc->dev, channel, period, duty));
}

static int
pwmbus_channel_get_config(device_t bus, int channel, unsigned int *period, unsigned int *duty)
{
	struct pwmbus_softc *sc;

	sc = device_get_softc(bus);

	if (channel > sc->nchannels)
		return (EINVAL);

	return (PWM_CHANNEL_GET_CONFIG(sc->dev, channel, period, duty));
}

static int
pwmbus_channel_set_flags(device_t bus, int channel, uint32_t flags)
{
	struct pwmbus_softc *sc;

	sc = device_get_softc(bus);

	if (channel > sc->nchannels)
		return (EINVAL);

	return (PWM_CHANNEL_SET_FLAGS(sc->dev, channel, flags));
}

static int
pwmbus_channel_get_flags(device_t bus, int channel, uint32_t *flags)
{
	struct pwmbus_softc *sc;

	sc = device_get_softc(bus);

	if (channel > sc->nchannels)
		return (EINVAL);

	return (PWM_CHANNEL_GET_FLAGS(sc->dev, channel, flags));
}

static int
pwmbus_channel_enable(device_t bus, int channel, bool enable)
{
	struct pwmbus_softc *sc;

	sc = device_get_softc(bus);

	if (channel > sc->nchannels)
		return (EINVAL);

	return (PWM_CHANNEL_ENABLE(sc->dev, channel, enable));
}

static int
pwmbus_channel_is_enabled(device_t bus, int channel, bool *enable)
{
	struct pwmbus_softc *sc;

	sc = device_get_softc(bus);

	if (channel > sc->nchannels)
		return (EINVAL);

	return (PWM_CHANNEL_IS_ENABLED(sc->dev, channel, enable));
}

static device_method_t pwmbus_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, pwmbus_probe),
	DEVMETHOD(device_attach, pwmbus_attach),
	DEVMETHOD(device_detach, pwmbus_detach),

	/* pwm interface */
	DEVMETHOD(pwmbus_channel_config, pwmbus_channel_config),
	DEVMETHOD(pwmbus_channel_get_config, pwmbus_channel_get_config),
	DEVMETHOD(pwmbus_channel_set_flags, pwmbus_channel_set_flags),
	DEVMETHOD(pwmbus_channel_get_flags, pwmbus_channel_get_flags),
	DEVMETHOD(pwmbus_channel_enable, pwmbus_channel_enable),
	DEVMETHOD(pwmbus_channel_is_enabled, pwmbus_channel_is_enabled),

	DEVMETHOD_END
};

static driver_t pwmbus_driver = {
	"pwmbus",
	pwmbus_methods,
	sizeof(struct pwmbus_softc),
};
static devclass_t pwmbus_devclass;

EARLY_DRIVER_MODULE(pwmbus, pwm, pwmbus_driver, pwmbus_devclass, 0, 0,
  BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(pwmbus, 1);
