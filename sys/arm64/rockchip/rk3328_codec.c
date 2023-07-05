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

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/syscon/syscon.h>

#include "syscon_if.h"

#include "opt_snd.h"
#include <dev/sound/pcm/sound.h>
#include <dev/sound/fdt/audio_dai.h>
#include "audio_dai_if.h"
#include "mixer_if.h"

#define	RKCODEC_MIXER_DEVS		(1 << SOUND_MIXER_VOLUME)

#define	GRF_SOC_CON2			0x0408
#define	 SOC_CON2_I2S_ACODEC_EN			(1 << 14)
#define	 SOC_CON2_I2S_ACODEC_EN_MASK		((1 << 14) << 16)
#define	GRF_SOC_CON10			0x0428
#define	 SOC_CON10_GPIOMUT			(1 << 1)
#define	 SOC_CON10_GPIOMUT_MASK			((1 << 1) << 16)
#define	 SOC_CON10_GPIOMUT_EN			(1 << 0)
#define	 SOC_CON10_GPIOMUT_EN_MASK		((1 << 0) << 16)

#define	CODEC_RESET			0x00
#define	 RESET_DIG_CORE_RST			(1 << 1)
#define	 RESET_SYS_RST				(1 << 0)
#define	CODEC_DAC_INIT_CTRL1		0x0c
#define	 DAC_INIT_CTRL1_DIRECTION_IN		(0 << 5)
#define	 DAC_INIT_CTRL1_DIRECTION_OUT		(1 << 5)
#define	 DAC_INIT_CTRL1_DAC_I2S_MODE_SLAVE	(0 << 4)
#define	 DAC_INIT_CTRL1_DAC_I2S_MODE_MASTER	(1 << 4)
#define	 DAC_INIT_CTRL1_MODE_MASK		(3 << 4)
#define	CODEC_DAC_INIT_CTRL2		0x10
#define	 DAC_INIT_CTRL2_DAC_VDL_16BITS		(0 << 5)
#define	 DAC_INIT_CTRL2_DAC_VDL_20BITS		(1 << 5)
#define	 DAC_INIT_CTRL2_DAC_VDL_24BITS		(2 << 5)
#define	 DAC_INIT_CTRL2_DAC_VDL_32BITS		(3 << 5)
#define	 DAC_INIT_CTRL2_DAC_VDL_MASK		(3 << 5)
#define	 DAC_INIT_CTRL2_DAC_MODE_RJM		(0 << 3)
#define	 DAC_INIT_CTRL2_DAC_MODE_LJM		(1 << 3)
#define	 DAC_INIT_CTRL2_DAC_MODE_I2S		(2 << 3)
#define	 DAC_INIT_CTRL2_DAC_MODE_PCM		(3 << 3)
#define	 DAC_INIT_CTRL2_DAC_MODE_MASK		(3 << 3)
#define	CODEC_DAC_INIT_CTRL3		0x14
#define	 DAC_INIT_CTRL3_WL_16BITS		(0 << 2)
#define	 DAC_INIT_CTRL3_WL_20BITS		(1 << 2)
#define	 DAC_INIT_CTRL3_WL_24BITS		(2 << 2)
#define	 DAC_INIT_CTRL3_WL_32BITS		(3 << 2)
#define	 DAC_INIT_CTRL3_WL_MASK			(3 << 2)
#define	 DAC_INIT_CTRL3_RST_MASK		(1 << 1)
#define	 DAC_INIT_CTRL3_RST_DIS			(1 << 1)
#define	 DAC_INIT_CTRL3_DAC_BCP_REVERSAL	(1 << 0)
#define	 DAC_INIT_CTRL3_DAC_BCP_NORMAL		(0 << 0)
#define	 DAC_INIT_CTRL3_DAC_BCP_MASK		(1 << 0)
#define	CODEC_DAC_PRECHARGE_CTRL	0x88
#define	 DAC_PRECHARGE_CTRL_DAC_CHARGE_PRECHARGE	(1 << 7)
#define	 DAC_PRECHARGE_CTRL_DAC_CHARGE_CURRENT_I	(1 << 0)
#define	 DAC_PRECHARGE_CTRL_DAC_CHARGE_CURRENT_ALL	(0x7f)
#define	CODEC_DAC_PWR_CTRL		0x8c
#define	 DAC_PWR_CTRL_DAC_PWR			(1 << 6)
#define	 DAC_PWR_CTRL_DACL_PATH_REFV		(1 << 5)
#define	 DAC_PWR_CTRL_HPOUTL_ZERO_CROSSING	(1 << 4)
#define	 DAC_PWR_CTRL_DACR_PATH_REFV		(1 << 1)
#define	 DAC_PWR_CTRL_HPOUTR_ZERO_CROSSING	(1 << 0)
#define	CODEC_DAC_CLK_CTRL		0x90
#define	 DAC_CLK_CTRL_DACL_REFV_ON		(1 << 7)
#define	 DAC_CLK_CTRL_DACL_CLK_ON		(1 << 6)
#define	 DAC_CLK_CTRL_DACL_ON			(1 << 5)
#define	 DAC_CLK_CTRL_DACL_INIT_ON		(1 << 4)
#define	 DAC_CLK_CTRL_DACR_REFV_ON		(1 << 3)
#define	 DAC_CLK_CTRL_DACR_CLK_ON		(1 << 2)
#define	 DAC_CLK_CTRL_DACR_ON			(1 << 1)
#define	 DAC_CLK_CTRL_DACR_INIT_ON		(1 << 0)
#define	CODEC_HPMIX_CTRL		0x94
#define	 HPMIX_CTRL_HPMIXL_EN			(1 << 6)
#define	 HPMIX_CTRL_HPMIXL_INIT_EN		(1 << 5)
#define	 HPMIX_CTRL_HPMIXL_INIT2_EN		(1 << 4)
#define	 HPMIX_CTRL_HPMIXR_EN			(1 << 2)
#define	 HPMIX_CTRL_HPMIXR_INIT_EN		(1 << 1)
#define	 HPMIX_CTRL_HPMIXR_INIT2_EN		(1 << 0)
#define	CODEC_DAC_SELECT		0x98
#define	 DAC_SELECT_DACL_SELECT			(1 << 4)
#define	 DAC_SELECT_DACR_SELECT			(1 << 0)
#define	CODEC_HPOUT_CTRL		0x9c
#define	 HPOUT_CTRL_HPOUTL_EN			(1 << 7)
#define	 HPOUT_CTRL_HPOUTL_INIT_EN		(1 << 6)
#define	 HPOUT_CTRL_HPOUTL_UNMUTE		(1 << 5)
#define	 HPOUT_CTRL_HPOUTR_EN			(1 << 4)
#define	 HPOUT_CTRL_HPOUTR_INIT_EN		(1 << 3)
#define	 HPOUT_CTRL_HPOUTR_UNMUTE		(1 << 2)
#define	CODEC_HPOUTL_GAIN_CTRL		0xa0
#define	CODEC_HPOUTR_GAIN_CTRL		0xa4
#define	CODEC_HPOUT_POP_CTRL		0xa8
#define	 HPOUT_POP_CTRL_HPOUTR_POP		(1 << 5)
#define	 HPOUT_POP_CTRL_HPOUTR_POP_XCHARGE	(1 << 4)
#define	 HPOUT_POP_CTRL_HPOUTL_POP		(1 << 1)
#define	 HPOUT_POP_CTRL_HPOUTL_POP_XCHARGE	(1 << 0)

#define	DEFAULT_RATE			(48000 * 256)

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3328-codec",	1},
	{ NULL,				0 }
};

struct rkcodec_softc {
	device_t	dev;
	struct resource	*res;
	struct mtx	mtx;
	clk_t		mclk;
	clk_t		pclk;
	struct syscon	*grf;
	u_int	regaddr;	/* address for the sysctl */
};

#define	RKCODEC_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	RKCODEC_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	RKCODEC_READ(sc, reg)		bus_read_4((sc)->res, (reg))
#define	RKCODEC_WRITE(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int rkcodec_probe(device_t dev);
static int rkcodec_attach(device_t dev);
static int rkcodec_detach(device_t dev);

static void
rkcodec_set_power(struct rkcodec_softc *sc, bool poweron)
{
	uint32_t val;
	val = RKCODEC_READ(sc, CODEC_DAC_PRECHARGE_CTRL);
	if (poweron)
		val |= DAC_PRECHARGE_CTRL_DAC_CHARGE_PRECHARGE;
	else
		val &= ~(DAC_PRECHARGE_CTRL_DAC_CHARGE_PRECHARGE);
	RKCODEC_WRITE(sc, CODEC_DAC_PRECHARGE_CTRL, val);

	DELAY(10000);

	val = RKCODEC_READ(sc, CODEC_DAC_PRECHARGE_CTRL);
	if (poweron)
		val |= DAC_PRECHARGE_CTRL_DAC_CHARGE_CURRENT_ALL;
	else
		val &= ~(DAC_PRECHARGE_CTRL_DAC_CHARGE_CURRENT_ALL);
	RKCODEC_WRITE(sc, CODEC_DAC_PRECHARGE_CTRL, val);

}

static void
rkcodec_set_mute(struct rkcodec_softc *sc, bool muted)
{
	uint32_t val;
	val = SOC_CON10_GPIOMUT_MASK;
	if (!muted)
		val |= SOC_CON10_GPIOMUT;
	SYSCON_WRITE_4(sc->grf, GRF_SOC_CON10, val);
}

static void
rkcodec_reset(struct rkcodec_softc *sc)
{

	RKCODEC_WRITE(sc, CODEC_RESET, 0);
	DELAY(10000);
	RKCODEC_WRITE(sc, CODEC_RESET, RESET_DIG_CORE_RST | RESET_SYS_RST);
}

static int
rkcodec_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK3328 CODEC");
	return (BUS_PROBE_DEFAULT);
}

static int
rkcodec_attach(device_t dev)
{
	struct rkcodec_softc *sc;
	int error, rid;
	phandle_t node;
	uint32_t val;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->res) {
		device_printf(dev, "could not allocate resource for device\n");
		error = ENXIO;
		goto fail;
	}

	node = ofw_bus_get_node(dev);
	if (syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &sc->grf) != 0) {
		device_printf(dev, "cannot get rockchip,grf handle\n");
		return (ENXIO);
	}

	val = SOC_CON2_I2S_ACODEC_EN | SOC_CON2_I2S_ACODEC_EN_MASK;
	SYSCON_WRITE_4(sc->grf, GRF_SOC_CON2, val);

	val = 0 | SOC_CON10_GPIOMUT_EN_MASK;
	SYSCON_WRITE_4(sc->grf, GRF_SOC_CON10, val);

	error = clk_get_by_ofw_name(dev, 0, "pclk", &sc->pclk);
	if (error != 0) {
		device_printf(dev, "could not get pclk clock\n");
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, 0, "mclk", &sc->mclk);
	if (error != 0) {
		device_printf(dev, "could not get mclk clock\n");
		goto fail;
	}

	error = clk_enable(sc->pclk);
	if (error != 0) {
		device_printf(sc->dev, "could not enable pclk clock\n");
		goto fail;
	}

	error = clk_enable(sc->mclk);
	if (error != 0) {
		device_printf(sc->dev, "could not enable mclk clock\n");
		goto fail;
	}

#if 0
	error = clk_set_freq(sc->mclk, DEFAULT_RATE, 0);
	if (error != 0) {
		device_printf(sc->dev, "could not set frequency for mclk clock\n");
		goto fail;
	}
#endif

	/* TODO: handle mute-gpios */

	rkcodec_reset(sc);
	rkcodec_set_power(sc, true);

	val = RKCODEC_READ(sc, CODEC_DAC_PWR_CTRL);
	val |= DAC_PWR_CTRL_DAC_PWR;
	RKCODEC_WRITE(sc, CODEC_DAC_PWR_CTRL, val);
	DELAY(1000);

	val |= DAC_PWR_CTRL_DACL_PATH_REFV |
	    DAC_PWR_CTRL_DACR_PATH_REFV;
	RKCODEC_WRITE(sc, CODEC_DAC_PWR_CTRL, val);
	DELAY(1000);

	val |= DAC_PWR_CTRL_HPOUTL_ZERO_CROSSING |
	    DAC_PWR_CTRL_HPOUTR_ZERO_CROSSING;
	RKCODEC_WRITE(sc, CODEC_DAC_PWR_CTRL, val);
	DELAY(1000);

	val = RKCODEC_READ(sc, CODEC_HPOUT_POP_CTRL);
	val |= HPOUT_POP_CTRL_HPOUTR_POP | HPOUT_POP_CTRL_HPOUTL_POP;
	val &= ~(HPOUT_POP_CTRL_HPOUTR_POP_XCHARGE | HPOUT_POP_CTRL_HPOUTL_POP_XCHARGE);
	RKCODEC_WRITE(sc, CODEC_HPOUT_POP_CTRL, val);
	DELAY(1000);

	val = RKCODEC_READ(sc, CODEC_HPMIX_CTRL);
	val |= HPMIX_CTRL_HPMIXL_EN | HPMIX_CTRL_HPMIXR_EN;
	RKCODEC_WRITE(sc, CODEC_HPMIX_CTRL, val);
	DELAY(1000);

	val |= HPMIX_CTRL_HPMIXL_INIT_EN | HPMIX_CTRL_HPMIXR_INIT_EN;
	RKCODEC_WRITE(sc, CODEC_HPMIX_CTRL, val);
	DELAY(1000);

	val = RKCODEC_READ(sc, CODEC_HPOUT_CTRL);
	val |= HPOUT_CTRL_HPOUTL_EN | HPOUT_CTRL_HPOUTR_EN;
	RKCODEC_WRITE(sc, CODEC_HPOUT_CTRL, val);
	DELAY(1000);

	val |= HPOUT_CTRL_HPOUTL_INIT_EN | HPOUT_CTRL_HPOUTR_INIT_EN;
	RKCODEC_WRITE(sc, CODEC_HPOUT_CTRL, val);
	DELAY(1000);

	val = RKCODEC_READ(sc, CODEC_DAC_CLK_CTRL);
	val |= DAC_CLK_CTRL_DACL_REFV_ON | DAC_CLK_CTRL_DACR_REFV_ON;
	RKCODEC_WRITE(sc, CODEC_DAC_CLK_CTRL, val);
	DELAY(1000);

	val |= DAC_CLK_CTRL_DACL_CLK_ON | DAC_CLK_CTRL_DACR_CLK_ON;
	RKCODEC_WRITE(sc, CODEC_DAC_CLK_CTRL, val);
	DELAY(1000);

	val |= DAC_CLK_CTRL_DACL_ON | DAC_CLK_CTRL_DACR_ON;
	RKCODEC_WRITE(sc, CODEC_DAC_CLK_CTRL, val);
	DELAY(1000);

	val |= DAC_CLK_CTRL_DACL_INIT_ON | DAC_CLK_CTRL_DACR_INIT_ON;
	RKCODEC_WRITE(sc, CODEC_DAC_CLK_CTRL, val);
	DELAY(1000);

	val = RKCODEC_READ(sc, CODEC_DAC_SELECT);
	val |= DAC_SELECT_DACL_SELECT | DAC_SELECT_DACR_SELECT;
	RKCODEC_WRITE(sc, CODEC_DAC_SELECT, val);
	DELAY(1000);

	val = RKCODEC_READ(sc, CODEC_HPMIX_CTRL);
	val |= HPMIX_CTRL_HPMIXL_INIT2_EN | HPMIX_CTRL_HPMIXR_INIT2_EN;
	RKCODEC_WRITE(sc, CODEC_HPMIX_CTRL, val);
	DELAY(1000);

	val = RKCODEC_READ(sc, CODEC_HPOUT_CTRL);
	val |= HPOUT_CTRL_HPOUTL_UNMUTE | HPOUT_CTRL_HPOUTR_UNMUTE;
	RKCODEC_WRITE(sc, CODEC_HPOUT_CTRL, val);
	DELAY(1000);

	RKCODEC_WRITE(sc, CODEC_HPOUTL_GAIN_CTRL, 0x18);
	RKCODEC_WRITE(sc, CODEC_HPOUTR_GAIN_CTRL, 0x18);
	DELAY(1000);

	rkcodec_set_mute(sc, false);

	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);

fail:
	rkcodec_detach(dev);
	return (error);
}

static int
rkcodec_detach(device_t dev)
{
	struct rkcodec_softc *sc;

	sc = device_get_softc(dev);

	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->res);
	mtx_destroy(&sc->mtx);

	return (0);
}

static int
rkcodec_mixer_init(struct snd_mixer *m)
{

	mix_setdevs(m, RKCODEC_MIXER_DEVS);

	return (0);
}

static int
rkcodec_mixer_uninit(struct snd_mixer *m)
{

	return (0);
}

static int
rkcodec_mixer_reinit(struct snd_mixer *m)
{

	return (0);
}

static int
rkcodec_mixer_set(struct snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	struct rkcodec_softc *sc;
	struct mtx *mixer_lock;
	uint8_t do_unlock;

	sc = device_get_softc(mix_getdevinfo(m));
	mixer_lock = mixer_get_lock(m);

	if (mtx_owned(mixer_lock)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		mtx_lock(mixer_lock);
	}

	right = left;

	RKCODEC_LOCK(sc);
	switch(dev) {
	case SOUND_MIXER_VOLUME:
		printf("[%s] %s:%d\n", __func__, __FILE__, __LINE__);
		break;

	case SOUND_MIXER_MIC:
		printf("[%s] %s:%d\n", __func__, __FILE__, __LINE__);
		break;
	default:
		break;
	}
	RKCODEC_UNLOCK(sc);

	if (do_unlock) {
		mtx_unlock(mixer_lock);
	}

	return (left | (right << 8));
}

static unsigned
rkcodec_mixer_setrecsrc(struct snd_mixer *m, unsigned src)
{

	return (0);
}

static kobj_method_t rkcodec_mixer_methods[] = {
	KOBJMETHOD(mixer_init,		rkcodec_mixer_init),
	KOBJMETHOD(mixer_uninit,	rkcodec_mixer_uninit),
	KOBJMETHOD(mixer_reinit,	rkcodec_mixer_reinit),
	KOBJMETHOD(mixer_set,		rkcodec_mixer_set),
	KOBJMETHOD(mixer_setrecsrc,	rkcodec_mixer_setrecsrc),
	KOBJMETHOD_END
};

MIXER_DECLARE(rkcodec_mixer);

static int
rkcodec_dai_init(device_t dev, uint32_t format)
{
	struct rkcodec_softc *sc;
	int fmt, pol, clk;
	uint32_t ctrl1, ctrl2, ctrl3;

	sc = device_get_softc(dev);

	fmt = AUDIO_DAI_FORMAT_FORMAT(format);
	pol = AUDIO_DAI_FORMAT_POLARITY(format);
	clk = AUDIO_DAI_FORMAT_CLOCK(format);

	ctrl1 = RKCODEC_READ(sc, CODEC_DAC_INIT_CTRL1);
	ctrl2 = RKCODEC_READ(sc, CODEC_DAC_INIT_CTRL2);
	ctrl3 = RKCODEC_READ(sc, CODEC_DAC_INIT_CTRL3);

	ctrl3 &= ~(DAC_INIT_CTRL3_DAC_BCP_MASK);
	switch (pol) {
	case AUDIO_DAI_POLARITY_IB_NF:
		ctrl3 |= DAC_INIT_CTRL3_DAC_BCP_REVERSAL;
		break;
	case AUDIO_DAI_POLARITY_NB_NF:
		ctrl3 |= DAC_INIT_CTRL3_DAC_BCP_NORMAL;
		break;
	default:
		return (EINVAL);
	}

	ctrl1 &= ~(DAC_INIT_CTRL1_MODE_MASK);
	switch (clk) {
	case AUDIO_DAI_CLOCK_CBM_CFM:
		ctrl1 |= DAC_INIT_CTRL1_DIRECTION_OUT |
		    DAC_INIT_CTRL1_DAC_I2S_MODE_SLAVE;
		break;
	case AUDIO_DAI_CLOCK_CBS_CFS:
		ctrl1 |= DAC_INIT_CTRL1_DIRECTION_IN |
		    DAC_INIT_CTRL1_DAC_I2S_MODE_SLAVE;
		break;
	default:
		return (EINVAL);
	}

	ctrl2 &= ~(DAC_INIT_CTRL2_DAC_VDL_MASK | DAC_INIT_CTRL2_DAC_MODE_MASK);
	ctrl2 |= DAC_INIT_CTRL2_DAC_VDL_16BITS;
	ctrl3 &= ~(DAC_INIT_CTRL3_WL_MASK);
	ctrl3 |= DAC_INIT_CTRL3_WL_32BITS;
	switch (fmt) {
	case AUDIO_DAI_FORMAT_I2S:
		ctrl2 |= DAC_INIT_CTRL2_DAC_MODE_I2S;
		break;
	case AUDIO_DAI_FORMAT_LJ:
		ctrl2 |= DAC_INIT_CTRL2_DAC_MODE_LJM;
		break;
	case AUDIO_DAI_FORMAT_RJ:
		ctrl2 |= DAC_INIT_CTRL2_DAC_MODE_RJM;
		break;
	default:
		return EINVAL;
	}

	ctrl3 &= ~(DAC_INIT_CTRL3_RST_MASK);
	ctrl3 |= DAC_INIT_CTRL3_RST_DIS;

	RKCODEC_WRITE(sc, CODEC_DAC_INIT_CTRL1, ctrl1);
	RKCODEC_WRITE(sc, CODEC_DAC_INIT_CTRL2, ctrl2);
	RKCODEC_WRITE(sc, CODEC_DAC_INIT_CTRL3, ctrl3);

	return (0);
}

static int
rkcodec_dai_trigger(device_t dev, int go, int pcm_dir)
{
	// struct rkcodec_softc 	*sc = device_get_softc(dev);

	if ((pcm_dir != PCMDIR_PLAY) && (pcm_dir != PCMDIR_REC))
		return (EINVAL);

	switch (go) {
	case PCMTRIG_START:
		if (pcm_dir == PCMDIR_PLAY) {
			printf("[%s] %s:%d\n", __func__, __FILE__, __LINE__);
		}
		else if (pcm_dir == PCMDIR_REC) {
			printf("[%s] %s:%d\n", __func__, __FILE__, __LINE__);
		}
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		if (pcm_dir == PCMDIR_PLAY) {
			printf("[%s] %s:%d\n", __func__, __FILE__, __LINE__);
		}
		else if (pcm_dir == PCMDIR_REC) {
			printf("[%s] %s:%d\n", __func__, __FILE__, __LINE__);
		}
		break;
	}

	return (0);
}

static int
rkcodec_dai_setup_mixer(device_t dev, device_t pcmdev)
{

	mixer_init(pcmdev, &rkcodec_mixer_class, dev);

	return (0);
}

static device_method_t rkcodec_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rkcodec_probe),
	DEVMETHOD(device_attach,	rkcodec_attach),
	DEVMETHOD(device_detach,	rkcodec_detach),

	DEVMETHOD(audio_dai_init,	rkcodec_dai_init),
	DEVMETHOD(audio_dai_setup_mixer,	rkcodec_dai_setup_mixer),
	DEVMETHOD(audio_dai_trigger,	rkcodec_dai_trigger),

	DEVMETHOD_END
};

static driver_t rkcodec_driver = {
	"rk3328codec",
	rkcodec_methods,
	sizeof(struct rkcodec_softc),
};

DRIVER_MODULE(rkcodec, simplebus, rkcodec_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
