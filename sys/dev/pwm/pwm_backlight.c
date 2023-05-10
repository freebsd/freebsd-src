/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Emmanuel Vadot <manu@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/slicer.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/regulator/regulator.h>

#include <dev/backlight/backlight.h>

#include <dev/pwm/ofw_pwm.h>

#include "backlight_if.h"
#include "pwmbus_if.h"

struct pwm_backlight_softc {
	device_t	pwmdev;
	struct cdev	*cdev;

	pwm_channel_t	channel;
	uint32_t	*levels;
	ssize_t		nlevels;
	int		default_level;
	ssize_t		current_level;

	regulator_t	power_supply;
	uint64_t	period;
	uint64_t	duty;
	bool		enabled;
};

static int pwm_backlight_find_level_per_percent(struct pwm_backlight_softc *sc, int percent);

static struct ofw_compat_data compat_data[] = {
	{ "pwm-backlight",	1 },
	{ NULL,			0 }
};

static int
pwm_backlight_probe(device_t dev)
{

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "PWM Backlight");
	return (BUS_PROBE_DEFAULT);
}

static int
pwm_backlight_attach(device_t dev)
{
	struct pwm_backlight_softc *sc;
	phandle_t node;
	int rv;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	rv = pwm_get_by_ofw_propidx(dev, node, "pwms", 0, &sc->channel);
	if (rv != 0) {
		device_printf(dev, "Cannot map pwm channel %d\n", rv);
		return (ENXIO);
	}

	if (regulator_get_by_ofw_property(dev, 0, "power-supply",
	    &sc->power_supply) != 0) {
		device_printf(dev, "No power-supply property\n");
		return (ENXIO);
	}

	if (OF_hasprop(node, "brightness-levels")) {
		sc->nlevels = OF_getencprop_alloc(node, "brightness-levels",
		    (void **)&sc->levels);
		if (sc->nlevels <= 0) {
			device_printf(dev, "Cannot parse brightness levels\n");
			return (ENXIO);
		}
		sc->nlevels /= sizeof(uint32_t);

		if (OF_getencprop(node, "default-brightness-level",
		    &sc->default_level, sizeof(uint32_t)) <= 0) {
			device_printf(dev, "No default-brightness-level while brightness-levels is specified\n");
			return (ENXIO);
		} else {
			if (sc->default_level > sc->nlevels) {
				device_printf(dev, "default-brightness-level isn't present in brightness-levels range\n");
				return (ENXIO);
			}
			sc->channel->duty = sc->channel->period * sc->levels[sc->default_level] / 100;
		}

		if (bootverbose) {
			device_printf(dev, "Number of levels: %zd\n", sc->nlevels);
			device_printf(dev, "Configured period time: %ju\n", (uintmax_t)sc->channel->period);
			device_printf(dev, "Default duty cycle: %ju\n", (uintmax_t)sc->channel->duty);
		}
	} else {
		/* Get the current backlight level */
		PWMBUS_CHANNEL_GET_CONFIG(sc->channel->dev,
		    sc->channel->channel,
		    (unsigned int *)&sc->channel->period,
		    (unsigned int *)&sc->channel->duty);
		if (sc->channel->duty > sc->channel->period)
			sc->channel->duty = sc->channel->period;
		if (bootverbose) {
			device_printf(dev, "Configured period time: %ju\n", (uintmax_t)sc->channel->period);
			device_printf(dev, "Default duty cycle: %ju\n", (uintmax_t)sc->channel->duty);
		}
	}

	regulator_enable(sc->power_supply);
	sc->channel->enabled = true;
	PWMBUS_CHANNEL_CONFIG(sc->channel->dev, sc->channel->channel,
	    sc->channel->period, sc->channel->duty);
	PWMBUS_CHANNEL_ENABLE(sc->channel->dev, sc->channel->channel,
	    sc->channel->enabled);

	sc->current_level = pwm_backlight_find_level_per_percent(sc,
	    sc->channel->period / sc->channel->duty);
	sc->cdev = backlight_register("pwm_backlight", dev);
	if (sc->cdev == NULL)
		device_printf(dev, "Cannot register as a backlight\n");

	return (0);
}

static int
pwm_backlight_detach(device_t dev)
{
	struct pwm_backlight_softc *sc;

	sc = device_get_softc(dev);
	if (sc->nlevels > 0)
		OF_prop_free(sc->levels);
	regulator_disable(sc->power_supply);
	backlight_destroy(sc->cdev);
	return (0);
}

static int
pwm_backlight_find_level_per_percent(struct pwm_backlight_softc *sc, int percent)
{
	int i;
	int diff;

	if (percent < 0 || percent > 100)
		return (-1);

	for (i = 0, diff = 0; i < sc->nlevels; i++) {
		if (sc->levels[i] == percent)
			return (i);
		else if (sc->levels[i] < percent)
			diff = percent - sc->levels[i];
		else {
			if (diff < abs((percent - sc->levels[i])))
				return (i - 1);
			else
				return (i);
		}
	}

	return (-1);
}

static int
pwm_backlight_update_status(device_t dev, struct backlight_props *props)
{
	struct pwm_backlight_softc *sc;
	int reg_status;
	int error;

	sc = device_get_softc(dev);

	if (sc->nlevels != 0) {
		error = pwm_backlight_find_level_per_percent(sc,
		    props->brightness);
		if (error < 0)
			return (ERANGE);
		sc->current_level = error;
		sc->channel->duty = sc->channel->period *
			sc->levels[sc->current_level] / 100;
	} else {
		if (props->brightness > 100 || props->brightness < 0)
			return (ERANGE);
		sc->channel->duty = sc->channel->period *
			props->brightness / 100;
	}
	sc->channel->enabled = true;
	PWMBUS_CHANNEL_CONFIG(sc->channel->dev, sc->channel->channel,
	    sc->channel->period, sc->channel->duty);
	PWMBUS_CHANNEL_ENABLE(sc->channel->dev, sc->channel->channel,
	    sc->channel->enabled);
	error = regulator_status(sc->power_supply, &reg_status);
	if (error != 0)
		device_printf(dev,
		    "Cannot get power-supply status: %d\n", error);
	else {
		if (props->brightness > 0) {
			if (reg_status != REGULATOR_STATUS_ENABLED)
				regulator_enable(sc->power_supply);
		} else {
			if (reg_status == REGULATOR_STATUS_ENABLED)
				regulator_disable(sc->power_supply);
		}
	}

	return (0);
}

static int
pwm_backlight_get_status(device_t dev, struct backlight_props *props)
{
	struct pwm_backlight_softc *sc;
	int i;

	sc = device_get_softc(dev);

	if (sc->nlevels != 0) {
		props->brightness = sc->levels[sc->current_level];
		props->nlevels = sc->nlevels;
		for (i = 0; i < sc->nlevels; i++)
			props->levels[i] = sc->levels[i];
	} else {
		props->brightness = sc->channel->duty * 100 / sc->channel->period;
		props->nlevels = 0;
	}
	return (0);
}

static int
pwm_backlight_get_info(device_t dev, struct backlight_info *info)
{

	info->type = BACKLIGHT_TYPE_PANEL;
	strlcpy(info->name, "pwm-backlight", BACKLIGHTMAXNAMELENGTH);
	return (0);
}

static device_method_t pwm_backlight_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, pwm_backlight_probe),
	DEVMETHOD(device_attach, pwm_backlight_attach),
	DEVMETHOD(device_detach, pwm_backlight_detach),

	/* backlight interface */
	DEVMETHOD(backlight_update_status, pwm_backlight_update_status),
	DEVMETHOD(backlight_get_status, pwm_backlight_get_status),
	DEVMETHOD(backlight_get_info, pwm_backlight_get_info),
	DEVMETHOD_END
};

driver_t pwm_backlight_driver = {
	"pwm_backlight",
	pwm_backlight_methods,
	sizeof(struct pwm_backlight_softc),
};

DRIVER_MODULE(pwm_backlight, simplebus, pwm_backlight_driver, 0, 0);
MODULE_DEPEND(pwm_backlight, backlight, 1, 1, 1);
OFWBUS_PNP_INFO(compat_data);
