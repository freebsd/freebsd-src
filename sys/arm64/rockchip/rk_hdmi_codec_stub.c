/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
 *
 * Minimal audio_dai-shim driver that matches the RK3399 dw-hdmi
 * controller node ("rockchip,rk3399-dw-hdmi") in the device tree.
 *
 * The base rk3399-rockpro64 DTS wires the simple-audio-card "RK3399-HDMI"
 * codec endpoint to the dw-hdmi controller (sound-dai = <&hdmi>).
 * Linux's dw-hdmi driver implements an audio codec component there.
 * FreeBSD currently has no dw-hdmi driver and no in-tree HDMI audio
 * codec, so audio_soc's codec-side phandle lookup resolves to a node
 * with no driver attached, leaving pcm0 (simple-audio-card) without
 * /dev/dsp.
 *
 * This stub claims the dw-hdmi controller node purely so it can:
 *   - register an OFW xref (lets audio_soc find a "codec" device by
 *     phandle), and
 *   - export an empty audio_dai_init method (satisfies audio_soc's
 *     DAI link probe).
 *
 * No MMIO / IRQ / clock resources are consumed.  The actual HDMI audio
 * packetizer programming (CTS/N, AudioInfoFrame) is still done from
 * rk_drm_hw.c during modeset, so this shim is intentionally a no-op
 * beyond binding completion.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "opt_snd.h"
#include <dev/sound/pcm/sound.h>
#include <dev/sound/fdt/audio_dai.h>
#include "audio_dai_if.h"

static struct ofw_compat_data rk_hdmi_codec_compat[] = {
	{ "rockchip,rk3399-dw-hdmi", 1 },
	{ NULL, 0 }
};

struct rk_hdmi_codec_softc {
	device_t dev;
};

static int
rk_hdmi_codec_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_search_compatible(dev, rk_hdmi_codec_compat)->ocd_data)
		return (ENXIO);
	device_set_desc(dev, "RK3399 dw-hdmi audio codec stub");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_hdmi_codec_attach(device_t dev)
{
	struct rk_hdmi_codec_softc *sc = device_get_softc(dev);
	phandle_t node;

	sc->dev = dev;
	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);
	return (0);
}

static int
rk_hdmi_codec_detach(device_t dev)
{
	return (0);
}

static int
rk_hdmi_codec_dai_init(device_t dev, uint32_t format)
{
	return (0);
}

static device_method_t rk_hdmi_codec_methods[] = {
	DEVMETHOD(device_probe,		rk_hdmi_codec_probe),
	DEVMETHOD(device_attach,	rk_hdmi_codec_attach),
	DEVMETHOD(device_detach,	rk_hdmi_codec_detach),

	DEVMETHOD(audio_dai_init,	rk_hdmi_codec_dai_init),

	DEVMETHOD_END
};

static driver_t rk_hdmi_codec_driver = {
	"rk_hdmi_codec",
	rk_hdmi_codec_methods,
	sizeof(struct rk_hdmi_codec_softc),
};

DRIVER_MODULE(rk_hdmi_codec, simplebus, rk_hdmi_codec_driver, 0, 0);
SIMPLEBUS_PNP_INFO(rk_hdmi_codec_compat);
MODULE_DEPEND(rk_hdmi_codec, sound, 1, 1, 1);
