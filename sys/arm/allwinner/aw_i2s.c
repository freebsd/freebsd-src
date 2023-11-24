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
 */

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
#include <dev/extres/hwreset/hwreset.h>

#include "syscon_if.h"

#include "opt_snd.h"
#include <dev/sound/pcm/sound.h>
#include <dev/sound/fdt/audio_dai.h>
#include "audio_dai_if.h"

#define	FIFO_LEVEL	0x40

#define	DA_CTL		0x00
#define		DA_CTL_BCLK_OUT (1 << 18)	/* sun8i */
#define		DA_CLK_LRCK_OUT (1 << 17)	/* sun8i */
#define		DA_CTL_SDO_EN	(1 << 8)
#define		DA_CTL_MS	(1 << 5)	/* sun4i */
#define		DA_CTL_PCM	(1 << 4)	/* sun4i */
#define		DA_CTL_MODE_SEL_MASK	(3 << 4) /* sun8i */
#define		DA_CTL_MODE_SEL_PCM	(0 << 4) /* sun8i */
#define		DA_CTL_MODE_SEL_LJ	(1 << 4) /* sun8i */
#define		DA_CTL_MODE_SEL_RJ	(2 << 4) /* sun8i */
#define		DA_CTL_TXEN	(1 << 2)
#define		DA_CTL_RXEN	(1 << 1)
#define		DA_CTL_GEN	(1 << 0)
#define	DA_FAT0		0x04
#define		DA_FAT0_LRCK_PERIOD_MASK	(0x3ff << 8) /* sun8i */
#define		DA_FAT0_LRCK_PERIOD(n)		(((n) & 0x3fff) << 8) /* sun8i */
#define		DA_FAT0_LRCP_MASK	(1 << 7)
#define		DA_LRCP_NORMAL		(0 << 7)
#define		DA_LRCP_INVERTED	(1 << 7)
#define		DA_FAT0_BCP_MASK	(1 << 6)
#define		DA_BCP_NORMAL		(0 << 6)
#define		DA_BCP_INVERTED		(1 << 6)
#define		DA_FAT0_SR	__BITS(5,4)
#define		DA_FAT0_WSS	__BITS(3,2)
#define		DA_FAT0_FMT_MASK	(3 << 0)
#define		 DA_FMT_I2S	0
#define		 DA_FMT_LJ	1
#define		 DA_FMT_RJ	2
#define	DA_FAT1		0x08
#define	DA_ISTA		0x0c
#define		DA_ISTA_TXUI_INT	(1 << 6)
#define		DA_ISTA_TXEI_INT	(1 << 4)
#define		DA_ISTA_RXAI_INT	(1 << 0)
#define	DA_RXFIFO	0x10
#define	DA_FCTL		0x14
#define		DA_FCTL_HUB_EN	(1 << 31)
#define		DA_FCTL_FTX	(1 << 25)
#define		DA_FCTL_FRX	(1 << 24)
#define		DA_FCTL_TXTL_MASK	(0x7f << 12)
#define		DA_FCTL_TXTL(v)		(((v) & 0x7f) << 12)
#define		DA_FCTL_TXIM	(1 << 2)
#define	DA_FSTA		0x18
#define		DA_FSTA_TXE_CNT(v)	(((v) >> 16) & 0xff)
#define		DA_FSTA_RXA_CNT(v)	((v) & 0x3f)
#define	DA_INT		0x1c
#define		DA_INT_TX_DRQ	(1 << 7)
#define		DA_INT_TXUI_EN	(1 << 6)
#define		DA_INT_TXEI_EN	(1 << 4)
#define		DA_INT_RX_DRQ	(1 << 3)
#define		DA_INT_RXAI_EN	(1 << 0)
#define	DA_TXFIFO	0x20
#define	DA_CLKD		0x24
#define		DA_CLKD_MCLKO_EN_SUN8I (1 << 8)
#define		DA_CLKD_MCLKO_EN_SUN4I (1 << 7)
#define		DA_CLKD_BCLKDIV_SUN8I(n)	(((n) & 0xf) << 4)
#define		DA_CLKD_BCLKDIV_SUN8I_MASK	(0xf << 4)
#define		DA_CLKD_BCLKDIV_SUN4I(n)	(((n) & 7) << 4)
#define		DA_CLKD_BCLKDIV_SUN4I_MASK	(7 << 4)
#define		 DA_CLKD_BCLKDIV_8	3
#define		 DA_CLKD_BCLKDIV_16	5
#define		DA_CLKD_MCLKDIV(n)	(((n) & 0xff) << 0)
#define		DA_CLKD_MCLKDIV_MASK	(0xf << 0)
#define		 DA_CLKD_MCLKDIV_1	0
#define	DA_TXCNT	0x28
#define	DA_RXCNT	0x2c
#define	DA_CHCFG	0x30		/* sun8i */
#define		DA_CHCFG_TX_SLOT_HIZ	(1 << 9)
#define		DA_CHCFG_TXN_STATE	(1 << 8)
#define		DA_CHCFG_RX_SLOT_NUM_MASK	(7 << 4)
#define		DA_CHCFG_RX_SLOT_NUM(n)		(((n) & 7) << 4)
#define		DA_CHCFG_TX_SLOT_NUM_MASK	(7 << 0)
#define		DA_CHCFG_TX_SLOT_NUM(n)		(((n) & 7) << 0)

#define	DA_CHSEL_OFFSET(n)	(((n) & 3) << 12)	/* sun8i */
#define	DA_CHSEL_OFFSET_MASK	(3 << 12)	/* sun8i */
#define	DA_CHSEL_EN(n)		(((n) & 0xff) << 4)
#define	DA_CHSEL_EN_MASK	(0xff << 4)
#define	DA_CHSEL_SEL(n)		(((n) & 7) << 0)
#define	DA_CHSEL_SEL_MASK	(7 << 0)

#define	AUDIO_BUFFER_SIZE	48000 * 4

#define	AW_I2S_SAMPLE_RATE	48000
#define	AW_I2S_CLK_RATE		24576000

enum sunxi_i2s_type {
	SUNXI_I2S_SUN4I,
	SUNXI_I2S_SUN8I,
};

struct sunxi_i2s_config {
	const char	*name;
	enum sunxi_i2s_type type;
	bus_size_t	txchsel;
	bus_size_t	txchmap;
	bus_size_t	rxchsel;
	bus_size_t	rxchmap;
};

static const struct sunxi_i2s_config sun50i_a64_codec_config = {
	.name = "Audio Codec (digital part)",
	.type = SUNXI_I2S_SUN4I,
	.txchsel = 0x30,
	.txchmap = 0x34,
	.rxchsel = 0x38,
	.rxchmap = 0x3c,
};

static const struct sunxi_i2s_config sun8i_h3_config = {
	.name = "I2S/PCM controller",
	.type = SUNXI_I2S_SUN8I,
	.txchsel = 0x34,
	.txchmap = 0x44,
	.rxchsel = 0x54,
	.rxchmap = 0x58,
};

static const u_int sun4i_i2s_bclk_divmap[] = {
	[0] = 2,
	[1] = 4,
	[2] = 6,
	[3] = 8,
	[4] = 12,
	[5] = 16,
};

static const u_int sun4i_i2s_mclk_divmap[] = {
	[0] = 1,
	[1] = 2,
	[2] = 4,
	[3] = 6,
	[4] = 8,
	[5] = 12,
	[6] = 16,
	[7] = 24,
};

static const u_int sun8i_i2s_divmap[] = {
	[1] = 1,
	[2] = 2,
	[3] = 4,
	[4] = 6,
	[5] = 8,
	[6] = 12,
	[7] = 16,
	[8] = 24,
	[9] = 32,
	[10] = 48,
	[11] = 64,
	[12] = 96,
	[13] = 128,
	[14] = 176,
	[15] = 192,
};


static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun50i-a64-codec-i2s", (uintptr_t)&sun50i_a64_codec_config },
	{ "allwinner,sun8i-h3-i2s", (uintptr_t)&sun8i_h3_config },
	{ NULL,					0 }
};

static struct resource_spec aw_i2s_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

struct aw_i2s_softc {
	device_t	dev;
	struct resource	*res[2];
	struct mtx	mtx;
	clk_t		clk;
	struct sunxi_i2s_config	*cfg;
	void *		intrhand;
	/* pointers to playback/capture buffers */
	uint32_t	play_ptr;
	uint32_t	rec_ptr;
};

#define	I2S_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	I2S_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	I2S_READ(sc, reg)	bus_read_4((sc)->res[0], (reg))
#define	I2S_WRITE(sc, reg, val)	bus_write_4((sc)->res[0], (reg), (val))
#define	I2S_TYPE(sc)		((sc)->cfg->type)

static int aw_i2s_probe(device_t dev);
static int aw_i2s_attach(device_t dev);
static int aw_i2s_detach(device_t dev);

static u_int
sunxi_i2s_div_to_regval(const u_int *divmap, u_int divmaplen, u_int div)
{
	u_int n;

	for (n = 0; n < divmaplen; n++)
		if (divmap[n] == div)
			return n;

	return -1;
}

static uint32_t sc_fmt[] = {
	SND_FORMAT(AFMT_S16_LE, 2, 0),
	0
};
static struct pcmchan_caps aw_i2s_caps = {AW_I2S_SAMPLE_RATE, AW_I2S_SAMPLE_RATE, sc_fmt, 0};


static int
aw_i2s_init(struct aw_i2s_softc *sc)
{
	uint32_t val;
	int error;

	error = clk_enable(sc->clk);
	if (error != 0) {
		device_printf(sc->dev, "cannot enable mod clock\n");
		return (ENXIO);
	}

	/* Reset */
	val = I2S_READ(sc, DA_CTL);
	val &= ~(DA_CTL_TXEN|DA_CTL_RXEN|DA_CTL_GEN);
	I2S_WRITE(sc, DA_CTL, val);

	val = I2S_READ(sc, DA_FCTL);
	val &= ~(DA_FCTL_FTX|DA_FCTL_FRX);
	val &= ~(DA_FCTL_TXTL_MASK);
	val |= DA_FCTL_TXTL(FIFO_LEVEL);
	I2S_WRITE(sc, DA_FCTL, val);

	I2S_WRITE(sc, DA_TXCNT, 0);
	I2S_WRITE(sc, DA_RXCNT, 0);

	/* Enable */
	val = I2S_READ(sc, DA_CTL);
	val |= DA_CTL_GEN;
	I2S_WRITE(sc, DA_CTL, val);
	val |= DA_CTL_SDO_EN;
	I2S_WRITE(sc, DA_CTL, val);

	/* Setup channels */
	I2S_WRITE(sc, sc->cfg->txchmap, 0x76543210);
	val = I2S_READ(sc, sc->cfg->txchsel);
	val &= ~DA_CHSEL_EN_MASK;
	val |= DA_CHSEL_EN(3);
	val &= ~DA_CHSEL_SEL_MASK;
	val |= DA_CHSEL_SEL(1);
	I2S_WRITE(sc, sc->cfg->txchsel, val);
	I2S_WRITE(sc, sc->cfg->rxchmap, 0x76543210);
	val = I2S_READ(sc, sc->cfg->rxchsel);
	val &= ~DA_CHSEL_EN_MASK;
	val |= DA_CHSEL_EN(3);
	val &= ~DA_CHSEL_SEL_MASK;
	val |= DA_CHSEL_SEL(1);
	I2S_WRITE(sc, sc->cfg->rxchsel, val);

	if (I2S_TYPE(sc) == SUNXI_I2S_SUN8I) {
		val = I2S_READ(sc, DA_CHCFG);
		val &= ~DA_CHCFG_TX_SLOT_NUM_MASK;
		val |= DA_CHCFG_TX_SLOT_NUM(1);
		val &= ~DA_CHCFG_RX_SLOT_NUM_MASK;
		val |= DA_CHCFG_RX_SLOT_NUM(1);
		I2S_WRITE(sc, DA_CHCFG, val);
	}

	return (0);
}

static int
aw_i2s_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Allwinner I2S");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_i2s_attach(device_t dev)
{
	struct aw_i2s_softc *sc;
	int error;
	phandle_t node;
	hwreset_t rst;
	clk_t clk;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->cfg  = (void*)ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (bus_alloc_resources(dev, aw_i2s_spec, sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, 0, "mod", &sc->clk);
	if (error != 0) {
		device_printf(dev, "cannot get i2s_clk clock\n");
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, 0, "apb", &clk);
	if (error != 0) {
		device_printf(dev, "cannot get APB clock\n");
		goto fail;
	}

	error = clk_enable(clk);
	if (error != 0) {
		device_printf(dev, "cannot enable APB clock\n");
		goto fail;
	}

	if (hwreset_get_by_ofw_idx(dev, 0, 0, &rst) == 0) {
		error = hwreset_deassert(rst);
		if (error != 0) {
			device_printf(dev, "cannot de-assert reset\n");
			goto fail;
		}
	}

	aw_i2s_init(sc);

	node = ofw_bus_get_node(dev);
	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);

fail:
	aw_i2s_detach(dev);
	return (error);
}

static int
aw_i2s_detach(device_t dev)
{
	struct aw_i2s_softc *i2s;

	i2s = device_get_softc(dev);

	if (i2s->clk)
		clk_release(i2s->clk);

	if (i2s->intrhand != NULL)
		bus_teardown_intr(i2s->dev, i2s->res[1], i2s->intrhand);

	bus_release_resources(dev, aw_i2s_spec, i2s->res);
	mtx_destroy(&i2s->mtx);

	return (0);
}

static int
aw_i2s_dai_init(device_t dev, uint32_t format)
{
	struct aw_i2s_softc *sc;
	int fmt, pol;
	uint32_t ctl, fat0, chsel;
	u_int offset;

	sc = device_get_softc(dev);

	fmt = AUDIO_DAI_FORMAT_FORMAT(format);
	pol = AUDIO_DAI_FORMAT_POLARITY(format);

	ctl = I2S_READ(sc, DA_CTL);
	fat0 = I2S_READ(sc, DA_FAT0);

	if (I2S_TYPE(sc) == SUNXI_I2S_SUN4I) {
		fat0 &= ~DA_FAT0_FMT_MASK;
		switch (fmt) {
		case AUDIO_DAI_FORMAT_I2S:
			fat0 |= DA_FMT_I2S;
			break;
		case AUDIO_DAI_FORMAT_RJ:
			fat0 |= DA_FMT_RJ;
			break;
		case AUDIO_DAI_FORMAT_LJ:
			fat0 |= DA_FMT_LJ;
			break;
		default:
			return EINVAL;
		}
		ctl &= ~DA_CTL_PCM;
	} else {
		ctl &= ~DA_CTL_MODE_SEL_MASK;
		switch (fmt) {
		case AUDIO_DAI_FORMAT_I2S:
			ctl |= DA_CTL_MODE_SEL_LJ;
			offset = 1;
			break;
		case AUDIO_DAI_FORMAT_LJ:
			ctl |= DA_CTL_MODE_SEL_LJ;
			offset = 0;
			break;
		case AUDIO_DAI_FORMAT_RJ:
			ctl |= DA_CTL_MODE_SEL_RJ;
			offset = 0;
			break;
		case AUDIO_DAI_FORMAT_DSPA:
			ctl |= DA_CTL_MODE_SEL_PCM;
			offset = 1;
			break;
		case AUDIO_DAI_FORMAT_DSPB:
			ctl |= DA_CTL_MODE_SEL_PCM;
			offset = 0;
			break;
		default:
			return EINVAL;
		}

		chsel = I2S_READ(sc, sc->cfg->txchsel);
		chsel &= ~DA_CHSEL_OFFSET_MASK;
		chsel |= DA_CHSEL_OFFSET(offset);
		I2S_WRITE(sc, sc->cfg->txchsel, chsel);

		chsel = I2S_READ(sc, sc->cfg->rxchsel);
		chsel &= ~DA_CHSEL_OFFSET_MASK;
		chsel |= DA_CHSEL_OFFSET(offset);
		I2S_WRITE(sc, sc->cfg->rxchsel, chsel);
	}

	fat0 &= ~(DA_FAT0_LRCP_MASK|DA_FAT0_BCP_MASK);
	if (I2S_TYPE(sc) == SUNXI_I2S_SUN4I) {
		if (AUDIO_DAI_POLARITY_INVERTED_BCLK(pol))
			fat0 |= DA_BCP_INVERTED;
		if (AUDIO_DAI_POLARITY_INVERTED_FRAME(pol))
			fat0 |= DA_LRCP_INVERTED;
	} else {
		if (AUDIO_DAI_POLARITY_INVERTED_BCLK(pol))
			fat0 |= DA_BCP_INVERTED;
		if (!AUDIO_DAI_POLARITY_INVERTED_FRAME(pol))
			fat0 |= DA_LRCP_INVERTED;

		fat0 &= ~DA_FAT0_LRCK_PERIOD_MASK;
		fat0 |= DA_FAT0_LRCK_PERIOD(32 - 1);
	}

	I2S_WRITE(sc, DA_CTL, ctl);
	I2S_WRITE(sc, DA_FAT0, fat0);

	return (0);
}


static int
aw_i2s_dai_intr(device_t dev, struct snd_dbuf *play_buf, struct snd_dbuf *rec_buf)
{
	struct aw_i2s_softc *sc;
	int ret = 0;
	uint32_t val, status;

	sc = device_get_softc(dev);

	I2S_LOCK(sc);

	status = I2S_READ(sc, DA_ISTA);
	/* Clear interrupts */
	// device_printf(sc->dev, "status: %08x\n", status);
	I2S_WRITE(sc, DA_ISTA, status);

	if (status & DA_ISTA_TXEI_INT) {
		uint8_t *samples;
		uint32_t count, size, readyptr, written, empty;

		val  = I2S_READ(sc, DA_FSTA);
		empty = DA_FSTA_TXE_CNT(val);
		count = sndbuf_getready(play_buf);
		size = sndbuf_getsize(play_buf);
		readyptr = sndbuf_getreadyptr(play_buf);

		samples = (uint8_t*)sndbuf_getbuf(play_buf);
		written = 0;
		if (empty > count / 2)
			empty = count / 2;
		for (; empty > 0; empty--) {
			val = (samples[readyptr++ % size] << 16);
			val |= (samples[readyptr++ % size] << 24);
			written += 2;
			I2S_WRITE(sc, DA_TXFIFO, val);
		}
		sc->play_ptr += written;
		sc->play_ptr %= size;
		ret |= AUDIO_DAI_PLAY_INTR;
	}

	if (status & DA_ISTA_RXAI_INT) {
		uint8_t *samples;
		uint32_t count, size, freeptr, recorded, available;

		val  = I2S_READ(sc, DA_FSTA);
		available = DA_FSTA_RXA_CNT(val);

		count = sndbuf_getfree(rec_buf);
		size = sndbuf_getsize(rec_buf);
		freeptr = sndbuf_getfreeptr(rec_buf);
		samples = (uint8_t*)sndbuf_getbuf(rec_buf);
		recorded = 0;
		if (available > count / 2)
			available = count / 2;

		for (; available > 0; available--) {
			val = I2S_READ(sc, DA_RXFIFO);
			samples[freeptr++ % size] = (val >> 16) & 0xff;
			samples[freeptr++ % size] = (val >> 24) & 0xff;
			recorded += 2;
		}
		sc->rec_ptr += recorded;
		sc->rec_ptr %= size;
		ret |= AUDIO_DAI_REC_INTR;
	}

	I2S_UNLOCK(sc);

	return (ret);
}

static struct pcmchan_caps *
aw_i2s_dai_get_caps(device_t dev)
{
	return (&aw_i2s_caps);
}

static int
aw_i2s_dai_trigger(device_t dev, int go, int pcm_dir)
{
	struct aw_i2s_softc 	*sc = device_get_softc(dev);
	uint32_t val;

	if ((pcm_dir != PCMDIR_PLAY) && (pcm_dir != PCMDIR_REC))
		return (EINVAL);

	switch (go) {
	case PCMTRIG_START:
		if (pcm_dir == PCMDIR_PLAY) {
			/* Flush FIFO */
			val = I2S_READ(sc, DA_FCTL);
			I2S_WRITE(sc, DA_FCTL, val | DA_FCTL_FTX);
			I2S_WRITE(sc, DA_FCTL, val & ~DA_FCTL_FTX);

			/* Reset TX sample counter */
			I2S_WRITE(sc, DA_TXCNT, 0);

			/* Enable TX block */
			val = I2S_READ(sc, DA_CTL);
			I2S_WRITE(sc, DA_CTL, val | DA_CTL_TXEN);

			/* Enable TX underrun interrupt */
			val = I2S_READ(sc, DA_INT);
			I2S_WRITE(sc, DA_INT, val | DA_INT_TXEI_EN);
		}

		if (pcm_dir == PCMDIR_REC) {
			/* Flush FIFO */
			val = I2S_READ(sc, DA_FCTL);
			I2S_WRITE(sc, DA_FCTL, val | DA_FCTL_FRX);
			I2S_WRITE(sc, DA_FCTL, val & ~DA_FCTL_FRX);

			/* Reset RX sample counter */
			I2S_WRITE(sc, DA_RXCNT, 0);

			/* Enable RX block */
			val = I2S_READ(sc, DA_CTL);
			I2S_WRITE(sc, DA_CTL, val | DA_CTL_RXEN);

			/* Enable RX data available interrupt */
			val = I2S_READ(sc, DA_INT);
			I2S_WRITE(sc, DA_INT, val | DA_INT_RXAI_EN);
		}

		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		I2S_LOCK(sc);

		if (pcm_dir == PCMDIR_PLAY) {
			/* Disable TX block */
			val = I2S_READ(sc, DA_CTL);
			I2S_WRITE(sc, DA_CTL, val & ~DA_CTL_TXEN);

			/* Enable TX underrun interrupt */
			val = I2S_READ(sc, DA_INT);
			I2S_WRITE(sc, DA_INT, val & ~DA_INT_TXEI_EN);

			sc->play_ptr = 0;
		} else {
			/* Disable RX block */
			val = I2S_READ(sc, DA_CTL);
			I2S_WRITE(sc, DA_CTL, val & ~DA_CTL_RXEN);

			/* Disable RX data available interrupt */
			val = I2S_READ(sc, DA_INT);
			I2S_WRITE(sc, DA_INT, val & ~DA_INT_RXAI_EN);

			sc->rec_ptr = 0;
		}

		I2S_UNLOCK(sc);
		break;
	}

	return (0);
}

static uint32_t
aw_i2s_dai_get_ptr(device_t dev, int pcm_dir)
{
	struct aw_i2s_softc *sc;
	uint32_t ptr;

	sc = device_get_softc(dev);

	I2S_LOCK(sc);
	if (pcm_dir == PCMDIR_PLAY)
		ptr = sc->play_ptr;
	else
		ptr = sc->rec_ptr;
	I2S_UNLOCK(sc);

	return ptr;
}

static int
aw_i2s_dai_setup_intr(device_t dev, driver_intr_t intr_handler, void *intr_arg)
{
	struct aw_i2s_softc 	*sc = device_get_softc(dev);

	if (bus_setup_intr(dev, sc->res[1],
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, intr_handler, intr_arg,
	    &sc->intrhand)) {
		device_printf(dev, "cannot setup interrupt handler\n");
		return (ENXIO);
	}

	return (0);
}

static uint32_t
aw_i2s_dai_set_chanformat(device_t dev, uint32_t format)
{

	return (0);
}

static int
aw_i2s_dai_set_sysclk(device_t dev, unsigned int rate, int dai_dir)
{
	struct aw_i2s_softc *sc;
	int bclk_val, mclk_val;
	uint32_t val;
	int error;

	sc = device_get_softc(dev);

	error = clk_set_freq(sc->clk, AW_I2S_CLK_RATE, CLK_SET_ROUND_DOWN);
	if (error != 0) {
		device_printf(sc->dev,
		    "couldn't set mod clock rate to %u Hz: %d\n", AW_I2S_CLK_RATE, error);
		return error;
	}
	error = clk_enable(sc->clk);
	if (error != 0) {
		device_printf(sc->dev,
		    "couldn't enable mod clock: %d\n", error);
		return error;
	}

	const u_int bclk_prate = I2S_TYPE(sc) == SUNXI_I2S_SUN4I ? rate : AW_I2S_CLK_RATE;

	const u_int bclk_div = bclk_prate / (2 * 32 * AW_I2S_SAMPLE_RATE);
	const u_int mclk_div = AW_I2S_CLK_RATE / rate;

	if (I2S_TYPE(sc) == SUNXI_I2S_SUN4I) {
		bclk_val = sunxi_i2s_div_to_regval(sun4i_i2s_bclk_divmap,
		    nitems(sun4i_i2s_bclk_divmap), bclk_div);
		mclk_val = sunxi_i2s_div_to_regval(sun4i_i2s_mclk_divmap,
		    nitems(sun4i_i2s_mclk_divmap), mclk_div);
	} else {
		bclk_val = sunxi_i2s_div_to_regval(sun8i_i2s_divmap,
		    nitems(sun8i_i2s_divmap), bclk_div);
		mclk_val = sunxi_i2s_div_to_regval(sun8i_i2s_divmap,
		    nitems(sun8i_i2s_divmap), mclk_div);
	}
	if (bclk_val == -1 || mclk_val == -1) {
		device_printf(sc->dev, "couldn't configure bclk/mclk dividers\n");
		return EIO;
	}

	val = I2S_READ(sc, DA_CLKD);
	if (I2S_TYPE(sc) == SUNXI_I2S_SUN4I) {
		val |= DA_CLKD_MCLKO_EN_SUN4I;
		val &= ~DA_CLKD_BCLKDIV_SUN4I_MASK;
		val |= DA_CLKD_BCLKDIV_SUN4I(bclk_val);
	} else {
		val |= DA_CLKD_MCLKO_EN_SUN8I;
		val &= ~DA_CLKD_BCLKDIV_SUN8I_MASK;
		val |= DA_CLKD_BCLKDIV_SUN8I(bclk_val);
	}
	val &= ~DA_CLKD_MCLKDIV_MASK;
	val |= DA_CLKD_MCLKDIV(mclk_val);
	I2S_WRITE(sc, DA_CLKD, val);


	return (0);
}

static uint32_t
aw_i2s_dai_set_chanspeed(device_t dev, uint32_t speed)
{

	return (speed);
}

static device_method_t aw_i2s_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_i2s_probe),
	DEVMETHOD(device_attach,	aw_i2s_attach),
	DEVMETHOD(device_detach,	aw_i2s_detach),

	DEVMETHOD(audio_dai_init,	aw_i2s_dai_init),
	DEVMETHOD(audio_dai_setup_intr,	aw_i2s_dai_setup_intr),
	DEVMETHOD(audio_dai_set_sysclk,	aw_i2s_dai_set_sysclk),
	DEVMETHOD(audio_dai_set_chanspeed,	aw_i2s_dai_set_chanspeed),
	DEVMETHOD(audio_dai_set_chanformat,	aw_i2s_dai_set_chanformat),
	DEVMETHOD(audio_dai_intr,	aw_i2s_dai_intr),
	DEVMETHOD(audio_dai_get_caps,	aw_i2s_dai_get_caps),
	DEVMETHOD(audio_dai_trigger,	aw_i2s_dai_trigger),
	DEVMETHOD(audio_dai_get_ptr,	aw_i2s_dai_get_ptr),

	DEVMETHOD_END
};

static driver_t aw_i2s_driver = {
	"i2s",
	aw_i2s_methods,
	sizeof(struct aw_i2s_softc),
};

DRIVER_MODULE(aw_i2s, simplebus, aw_i2s_driver, 0, 0);
SIMPLEBUS_PNP_INFO(compat_data);
