/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/resource.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/regulator/regulator.h>
#include <dev/gpio/gpiobusvar.h>

#include "opt_snd.h"
#include <dev/sound/pcm/sound.h>
#include <dev/sound/fdt/audio_dai.h>
#include "audio_dai_if.h"

static struct ofw_compat_data compat_data[] = {
	{ "simple-audio-amplifier",	1},
	{ NULL,				0}
};

struct simple_amp_softc {
	device_t	dev;
	regulator_t	supply_vcc;
	gpio_pin_t	gpio_enable;
	bool		gpio_is_valid;
};

static int simple_amp_probe(device_t dev);
static int simple_amp_attach(device_t dev);
static int simple_amp_detach(device_t dev);

static int
simple_amp_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Simple Amplifier");
	return (BUS_PROBE_DEFAULT);
}

static int
simple_amp_attach(device_t dev)
{
	struct simple_amp_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	error = gpio_pin_get_by_ofw_property(dev, node,
	    "enable-gpios", &sc->gpio_enable);
	if (error != 0)
		sc->gpio_is_valid = false;
	else
		sc->gpio_is_valid = true;

	error = regulator_get_by_ofw_property(dev, 0, "VCC-supply",
	    &sc->supply_vcc);
	if (error != 0)
		device_printf(dev, "no VCC supply");

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}

static int
simple_amp_detach(device_t dev)
{

	return (0);
}

static int
simple_amp_dai_init(device_t dev, uint32_t format)
{

	return (0);
}

static int
simple_amp_dai_trigger(device_t dev, int go, int pcm_dir)
{
	struct simple_amp_softc	*sc;
	int error;

	if ((pcm_dir != PCMDIR_PLAY) && (pcm_dir != PCMDIR_REC))
		return (EINVAL);

	sc = device_get_softc(dev);
	error = 0;
	switch (go) {
	case PCMTRIG_START:
		if (sc->supply_vcc != NULL) {
			error = regulator_enable(sc->supply_vcc);
			if (error != 0) {
				device_printf(sc->dev,
				    "could not enable 'VCC' regulator\n");
				break;
			}
		}

		if (sc->gpio_is_valid) {
			error = gpio_pin_set_active(sc->gpio_enable, 1);
			if (error != 0) {
				device_printf(sc->dev,
				    "could not set 'gpio-enable' gpio\n");
				break;
			}
		}

		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		if (sc->gpio_is_valid) {
			error = gpio_pin_set_active(sc->gpio_enable, 0);
			if (error != 0) {
				device_printf(sc->dev,
				    "could not clear 'gpio-enable' gpio\n");
				break;
			}
		}

		if (sc->supply_vcc != NULL) {
			error = regulator_disable(sc->supply_vcc);
			if (error != 0) {
				device_printf(sc->dev,
				    "could not disable 'VCC' regulator\n");
				break;
			}
		}

		break;
	}

	return (error);
}

static device_method_t simple_amp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		simple_amp_probe),
	DEVMETHOD(device_attach,	simple_amp_attach),
	DEVMETHOD(device_detach,	simple_amp_detach),

	DEVMETHOD(audio_dai_init,	simple_amp_dai_init),
	DEVMETHOD(audio_dai_trigger,	simple_amp_dai_trigger),

	DEVMETHOD_END
};

static driver_t simple_amp_driver = {
	"simpleamp",
	simple_amp_methods,
	sizeof(struct simple_amp_softc),
};

DRIVER_MODULE(simple_amp, simplebus, simple_amp_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
