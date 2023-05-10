/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

#include "opt_snd.h"
#include <dev/sound/pcm/sound.h>
#include <dev/sound/fdt/audio_dai.h>
#include "audio_dai_if.h"

static struct ofw_compat_data compat_data[] = {
	{ "dummy-codec",	1},
	{ NULL,			0 }
};

struct dummy_codec_softc {
	device_t	dev;
};

static int dummy_codec_probe(device_t dev);
static int dummy_codec_attach(device_t dev);
static int dummy_codec_detach(device_t dev);

static int
dummy_codec_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Dummy Codec");
	return (BUS_PROBE_DEFAULT);
}

static int
dummy_codec_attach(device_t dev)
{
	struct dummy_codec_softc *sc;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}

static int
dummy_codec_detach(device_t dev)
{

	return (0);
}

static int
dummy_codec_dai_init(device_t dev, uint32_t format)
{

	return (0);
}

static device_method_t dummy_codec_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dummy_codec_probe),
	DEVMETHOD(device_attach,	dummy_codec_attach),
	DEVMETHOD(device_detach,	dummy_codec_detach),

	DEVMETHOD(audio_dai_init,	dummy_codec_dai_init),

	DEVMETHOD_END
};

static driver_t dummy_codec_driver = {
	"dummycodec",
	dummy_codec_methods,
	sizeof(struct dummy_codec_softc),
};

DRIVER_MODULE(dummy_codec, simplebus, dummy_codec_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
