/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
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
#include <sys/gpio.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/gpio/gpiobusvar.h>

#include "opt_snd.h"
#include <dev/sound/pcm/sound.h>
#include <dev/sound/fdt/audio_dai.h>
#include "audio_dai_if.h"

#define	SYSCLK_CTL		0x00c
#define	 AIF1CLK_ENA			(1 << 11)
#define	 AIF1CLK_SRC_MASK		(3 << 8)
#define	 AIF1CLK_SRC_PLL		(2 << 8)
#define	 SYSCLK_ENA			(1 << 3)
#define	 SYSCLK_SRC			(1 << 0)

#define	MOD_CLK_ENA		0x010
#define	MOD_RST_CTL		0x014
#define	MOD_AIF1			(1 << 15)
#define	MOD_ADC				(1 << 3)
#define	MOD_DAC				(1 << 2)

#define	SYS_SR_CTRL		0x018
#define	 AIF1_FS_MASK		(0xf << 12)
#define	  AIF_FS_48KHZ		(8 << 12)

#define	AIF1CLK_CTRL		0x040
#define	 AIF1_MSTR_MOD		(1 << 15)
#define	 AIF1_BCLK_INV		(1 << 14)
#define	 AIF1_LRCK_INV		(1 << 13)
#define	 AIF1_BCLK_DIV_MASK	(0xf << 9)
#define	  AIF1_BCLK_DIV_16	(6 << 9)
#define	 AIF1_LRCK_DIV_MASK	(7 << 6)
#define	  AIF1_LRCK_DIV_16	(0 << 6)
#define	  AIF1_LRCK_DIV_64	(2 << 6)
#define	 AIF1_WORD_SIZ_MASK	(3 << 4)
#define	  AIF1_WORD_SIZ_16	(1 << 4)
#define	 AIF1_DATA_FMT_MASK	(3 << 2)
#define	  AIF1_DATA_FMT_I2S	(0 << 2)
#define	  AIF1_DATA_FMT_LJ	(1 << 2)
#define	  AIF1_DATA_FMT_RJ	(2 << 2)
#define	  AIF1_DATA_FMT_DSP	(3 << 2)

#define	AIF1_ADCDAT_CTRL	0x044
#define		AIF1_ADC0L_ENA		(1 << 15)
#define		AIF1_ADC0R_ENA		(1 << 14)

#define	AIF1_DACDAT_CTRL	0x048
#define	 AIF1_DAC0L_ENA		(1 << 15)
#define	 AIF1_DAC0R_ENA		(1 << 14)

#define	AIF1_MXR_SRC		0x04c
#define		AIF1L_MXR_SRC_MASK	(0xf << 12)
#define		AIF1L_MXR_SRC_AIF1	(0x8 << 12)
#define		AIF1L_MXR_SRC_ADC	(0x2 << 12)
#define		AIF1R_MXR_SRC_MASK	(0xf << 8)
#define		AIF1R_MXR_SRC_AIF1	(0x8 << 8)
#define		AIF1R_MXR_SRC_ADC	(0x2 << 8)

#define	ADC_DIG_CTRL		0x100
#define	 ADC_DIG_CTRL_ENAD	(1 << 15)

#define	HMIC_CTRL1		0x110
#define	 HMIC_CTRL1_N_MASK		(0xf << 8)
#define	 HMIC_CTRL1_N(n)		(((n) & 0xf) << 8)
#define	 HMIC_CTRL1_JACK_IN_IRQ_EN	(1 << 4)
#define	 HMIC_CTRL1_JACK_OUT_IRQ_EN	(1 << 3)
#define	 HMIC_CTRL1_MIC_DET_IRQ_EN	(1 << 0)

#define	HMIC_CTRL2		0x114
#define	 HMIC_CTRL2_MDATA_THRES	__BITS(12,8)

#define	HMIC_STS		0x118
#define	 HMIC_STS_MIC_PRESENT	(1 << 6)
#define	 HMIC_STS_JACK_DET_OIRQ	(1 << 4)
#define	 HMIC_STS_JACK_DET_IIRQ	(1 << 3)
#define	 HMIC_STS_MIC_DET_ST	(1 << 0)

#define	DAC_DIG_CTRL		0x120
#define	 DAC_DIG_CTRL_ENDA	(1 << 15)

#define	DAC_MXR_SRC		0x130
#define	 DACL_MXR_SRC_MASK		(0xf << 12)
#define	  DACL_MXR_SRC_AIF1_DAC0L (0x8 << 12)
#define	 DACR_MXR_SRC_MASK		(0xf << 8)
#define	  DACR_MXR_SRC_AIF1_DAC0R (0x8 << 8)

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun8i-a33-codec",	1},
	{ NULL,				0 }
};

static struct resource_spec sun8i_codec_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

struct sun8i_codec_softc {
	device_t	dev;
	struct resource	*res[2];
	struct mtx	mtx;
	clk_t		clk_gate;
	clk_t		clk_mod;
	void *		intrhand;
};

#define	CODEC_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	CODEC_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	CODEC_READ(sc, reg)		bus_read_4((sc)->res[0], (reg))
#define	CODEC_WRITE(sc, reg, val)	bus_write_4((sc)->res[0], (reg), (val))

static int sun8i_codec_probe(device_t dev);
static int sun8i_codec_attach(device_t dev);
static int sun8i_codec_detach(device_t dev);

static int
sun8i_codec_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Codec");
	return (BUS_PROBE_DEFAULT);
}

static int
sun8i_codec_attach(device_t dev)
{
	struct sun8i_codec_softc *sc;
	int error;
	uint32_t val;
	struct gpiobus_pin *pa_pin;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, sun8i_codec_spec, sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, 0, "mod", &sc->clk_mod);
	if (error != 0) {
		device_printf(dev, "cannot get \"mod\" clock\n");
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, 0, "bus", &sc->clk_gate);
	if (error != 0) {
		device_printf(dev, "cannot get \"bus\" clock\n");
		goto fail;
	}

	error = clk_enable(sc->clk_gate);
	if (error != 0) {
		device_printf(dev, "cannot enable \"bus\" clock\n");
		goto fail;
	}

	/* Enable clocks */
	val = CODEC_READ(sc, SYSCLK_CTL);
	val |= AIF1CLK_ENA;
	val &= ~AIF1CLK_SRC_MASK;
	val |= AIF1CLK_SRC_PLL;
	val |= SYSCLK_ENA;
	val &= ~SYSCLK_SRC;
	CODEC_WRITE(sc, SYSCLK_CTL, val);
	CODEC_WRITE(sc, MOD_CLK_ENA, MOD_AIF1 | MOD_ADC | MOD_DAC);
	CODEC_WRITE(sc, MOD_RST_CTL, MOD_AIF1 | MOD_ADC | MOD_DAC);

	/* Enable digital parts */
	CODEC_WRITE(sc, DAC_DIG_CTRL, DAC_DIG_CTRL_ENDA);
	CODEC_WRITE(sc, ADC_DIG_CTRL, ADC_DIG_CTRL_ENAD);

	/* Set AIF1 to 48 kHz */
	val = CODEC_READ(sc, SYS_SR_CTRL);
	val &= ~AIF1_FS_MASK;
	val |= AIF_FS_48KHZ;
	CODEC_WRITE(sc, SYS_SR_CTRL, val);

	/* Set AIF1 to 16-bit */
	val = CODEC_READ(sc, AIF1CLK_CTRL);
	val &= ~AIF1_WORD_SIZ_MASK;
	val |= AIF1_WORD_SIZ_16;
	CODEC_WRITE(sc, AIF1CLK_CTRL, val);

	/* Enable AIF1 DAC timelot 0 */
	val = CODEC_READ(sc, AIF1_DACDAT_CTRL);
	val |= AIF1_DAC0L_ENA;
	val |= AIF1_DAC0R_ENA;
	CODEC_WRITE(sc, AIF1_DACDAT_CTRL, val);

	/* Enable AIF1 ADC timelot 0 */
	val = CODEC_READ(sc, AIF1_ADCDAT_CTRL);
	val |= AIF1_ADC0L_ENA;
	val |= AIF1_ADC0R_ENA;
	CODEC_WRITE(sc, AIF1_ADCDAT_CTRL, val);

	/* DAC mixer source select */
	val = CODEC_READ(sc, DAC_MXR_SRC);
	val &= ~DACL_MXR_SRC_MASK;
	val |= DACL_MXR_SRC_AIF1_DAC0L;
	val &= ~DACR_MXR_SRC_MASK;
	val |= DACR_MXR_SRC_AIF1_DAC0R;
	CODEC_WRITE(sc, DAC_MXR_SRC, val);

	/* ADC mixer source select */
	val = CODEC_READ(sc, AIF1_MXR_SRC);
	val &= ~AIF1L_MXR_SRC_MASK;
	val |= AIF1L_MXR_SRC_ADC;
	val &= ~AIF1R_MXR_SRC_MASK;
	val |= AIF1R_MXR_SRC_ADC;
	CODEC_WRITE(sc, AIF1_MXR_SRC, val);

	/* Enable PA power */
	/* Unmute PA */
	if (gpio_pin_get_by_ofw_property(dev, node, "allwinner,pa-gpios",
	    &pa_pin) == 0) {
		error = gpio_pin_set_active(pa_pin, 1);
		if (error != 0)
			device_printf(dev, "failed to unmute PA\n");
	}

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);

fail:
	sun8i_codec_detach(dev);
	return (error);
}

static int
sun8i_codec_detach(device_t dev)
{
	struct sun8i_codec_softc *sc;

	sc = device_get_softc(dev);

	if (sc->clk_gate)
		clk_release(sc->clk_gate);

	if (sc->clk_mod)
		clk_release(sc->clk_mod);

	if (sc->intrhand != NULL)
		bus_teardown_intr(sc->dev, sc->res[1], sc->intrhand);

	bus_release_resources(dev, sun8i_codec_spec, sc->res);
	mtx_destroy(&sc->mtx);

	return (0);
}

static int
sun8i_codec_dai_init(device_t dev, uint32_t format)
{
	struct sun8i_codec_softc *sc;
	int fmt, pol, clk;
	uint32_t val;

	sc = device_get_softc(dev);

	fmt = AUDIO_DAI_FORMAT_FORMAT(format);
	pol = AUDIO_DAI_FORMAT_POLARITY(format);
	clk = AUDIO_DAI_FORMAT_CLOCK(format);

	val = CODEC_READ(sc, AIF1CLK_CTRL);

	val &= ~AIF1_DATA_FMT_MASK;
	switch (fmt) {
	case AUDIO_DAI_FORMAT_I2S:
		val |= AIF1_DATA_FMT_I2S;
		break;
	case AUDIO_DAI_FORMAT_RJ:
		val |= AIF1_DATA_FMT_RJ;
		break;
	case AUDIO_DAI_FORMAT_LJ:
		val |= AIF1_DATA_FMT_LJ;
		break;
	case AUDIO_DAI_FORMAT_DSPA:
	case AUDIO_DAI_FORMAT_DSPB:
		val |= AIF1_DATA_FMT_DSP;
		break;
	default:
		return EINVAL;
	}

	val &= ~(AIF1_BCLK_INV|AIF1_LRCK_INV);
	/* Codec LRCK polarity is inverted (datasheet is wrong) */
	if (!AUDIO_DAI_POLARITY_INVERTED_FRAME(pol))
		val |= AIF1_LRCK_INV;
	if (AUDIO_DAI_POLARITY_INVERTED_BCLK(pol))
		val |= AIF1_BCLK_INV;

	switch (clk) {
	case AUDIO_DAI_CLOCK_CBM_CFM:
		val &= ~AIF1_MSTR_MOD;	/* codec is master */
		break;
	case AUDIO_DAI_CLOCK_CBS_CFS:
		val |= AIF1_MSTR_MOD;	/* codec is slave */
		break;
	default:
		return EINVAL;
	}

	val &= ~AIF1_LRCK_DIV_MASK;
	val |= AIF1_LRCK_DIV_64;

	val &= ~AIF1_BCLK_DIV_MASK;
	val |= AIF1_BCLK_DIV_16;

	CODEC_WRITE(sc, AIF1CLK_CTRL, val);

	return (0);
}

static int
sun8i_codec_dai_trigger(device_t dev, int go, int pcm_dir)
{

	return (0);
}

static int
sun8i_codec_dai_setup_mixer(device_t dev, device_t pcmdev)
{

	/* Do nothing for now */
	return (0);
}


static device_method_t sun8i_codec_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sun8i_codec_probe),
	DEVMETHOD(device_attach,	sun8i_codec_attach),
	DEVMETHOD(device_detach,	sun8i_codec_detach),

	DEVMETHOD(audio_dai_init,	sun8i_codec_dai_init),
	DEVMETHOD(audio_dai_setup_mixer,	sun8i_codec_dai_setup_mixer),
	DEVMETHOD(audio_dai_trigger,	sun8i_codec_dai_trigger),

	DEVMETHOD_END
};

static driver_t sun8i_codec_driver = {
	"sun8icodec",
	sun8i_codec_methods,
	sizeof(struct sun8i_codec_softc),
};

DRIVER_MODULE(sun8i_codec, simplebus, sun8i_codec_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
