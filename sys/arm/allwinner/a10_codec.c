/*-
 * Copyright (c) 2014-2016 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * Allwinner A10/A20 Audio Codec
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>

#include <machine/bus.h>

#include <dev/sound/pcm/sound.h>
#include <dev/sound/chip.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>

#include "sunxi_dma_if.h"
#include "mixer_if.h"
#include "gpio_if.h"

#define	TX_TRIG_LEVEL	0xf
#define	RX_TRIG_LEVEL	0x7
#define	DRQ_CLR_CNT	0x3

#define	AC_DAC_DPC	0x00
#define	 DAC_DPC_EN_DA			0x80000000
#define	AC_DAC_FIFOC	0x04
#define	 DAC_FIFOC_FS_SHIFT		29
#define	 DAC_FIFOC_FS_MASK		(7U << DAC_FIFOC_FS_SHIFT)
#define	  DAC_FS_48KHZ			0
#define	  DAC_FS_32KHZ			1
#define	  DAC_FS_24KHZ			2
#define	  DAC_FS_16KHZ			3
#define	  DAC_FS_12KHZ			4
#define	  DAC_FS_8KHZ			5
#define	  DAC_FS_192KHZ			6
#define	  DAC_FS_96KHZ			7
#define	 DAC_FIFOC_FIFO_MODE_SHIFT	24
#define	 DAC_FIFOC_FIFO_MODE_MASK	(3U << DAC_FIFOC_FIFO_MODE_SHIFT)
#define	  FIFO_MODE_24_31_8		0
#define	  FIFO_MODE_16_31_16		0
#define	  FIFO_MODE_16_15_0		1
#define	 DAC_FIFOC_DRQ_CLR_CNT_SHIFT	21
#define	 DAC_FIFOC_DRQ_CLR_CNT_MASK	(3U << DAC_FIFOC_DRQ_CLR_CNT_SHIFT)
#define	 DAC_FIFOC_TX_TRIG_LEVEL_SHIFT	8
#define	 DAC_FIFOC_TX_TRIG_LEVEL_MASK	(0x7f << DAC_FIFOC_TX_TRIG_LEVEL_SHIFT)
#define	 DAC_FIFOC_MONO_EN		(1U << 6)
#define	 DAC_FIFOC_TX_BITS		(1U << 5)
#define	 DAC_FIFOC_DRQ_EN		(1U << 4)
#define	 DAC_FIFOC_FIFO_FLUSH		(1U << 0)
#define	AC_DAC_FIFOS	0x08
#define	AC_DAC_TXDATA	0x0c
#define	AC_DAC_ACTL	0x10
#define	 DAC_ACTL_DACAREN		(1U << 31)
#define	 DAC_ACTL_DACALEN		(1U << 30)
#define	 DAC_ACTL_MIXEN			(1U << 29)
#define	 DAC_ACTL_DACPAS		(1U << 8)
#define	 DAC_ACTL_PAMUTE		(1U << 6)
#define	 DAC_ACTL_PAVOL_SHIFT		0
#define	 DAC_ACTL_PAVOL_MASK		(0x3f << DAC_ACTL_PAVOL_SHIFT)
#define	AC_ADC_FIFOC	0x1c
#define	 ADC_FIFOC_FS_SHIFT		29
#define	 ADC_FIFOC_FS_MASK		(7U << ADC_FIFOC_FS_SHIFT)
#define	  ADC_FS_48KHZ		0
#define	 ADC_FIFOC_EN_AD		(1U << 28)
#define	 ADC_FIFOC_RX_FIFO_MODE		(1U << 24)
#define	 ADC_FIFOC_RX_TRIG_LEVEL_SHIFT	8
#define	 ADC_FIFOC_RX_TRIG_LEVEL_MASK	(0x1f << ADC_FIFOC_RX_TRIG_LEVEL_SHIFT)
#define	 ADC_FIFOC_MONO_EN		(1U << 7)
#define	 ADC_FIFOC_RX_BITS		(1U << 6)
#define	 ADC_FIFOC_DRQ_EN		(1U << 4)
#define	 ADC_FIFOC_FIFO_FLUSH		(1U << 1)
#define	AC_ADC_FIFOS	0x20
#define	AC_ADC_RXDATA	0x24
#define	AC_ADC_ACTL	0x28
#define	 ADC_ACTL_ADCREN		(1U << 31)
#define	 ADC_ACTL_ADCLEN		(1U << 30)
#define	 ADC_ACTL_PREG1EN		(1U << 29)
#define	 ADC_ACTL_PREG2EN		(1U << 28)
#define	 ADC_ACTL_VMICEN		(1U << 27)
#define	 ADC_ACTL_ADCG_SHIFT		20
#define	 ADC_ACTL_ADCG_MASK		(7U << ADC_ACTL_ADCG_SHIFT)
#define	 ADC_ACTL_ADCIS_SHIFT		17
#define	 ADC_ACTL_ADCIS_MASK		(7U << ADC_ACTL_ADCIS_SHIFT)
#define	  ADC_IS_LINEIN			0
#define	  ADC_IS_FMIN			1
#define	  ADC_IS_MIC1			2
#define	  ADC_IS_MIC2			3
#define	  ADC_IS_MIC1_L_MIC2_R		4
#define	  ADC_IS_MIC1_LR_MIC2_LR	5
#define	  ADC_IS_OMIX			6
#define	  ADC_IS_LINEIN_L_MIC1_R	7
#define	 ADC_ACTL_LNRDF			(1U << 16)
#define	 ADC_ACTL_LNPREG_SHIFT		13
#define	 ADC_ACTL_LNPREG_MASK		(7U << ADC_ACTL_LNPREG_SHIFT)
#define	 ADC_ACTL_PA_EN			(1U << 4)
#define	 ADC_ACTL_DDE			(1U << 3)
#define	AC_DAC_CNT	0x30
#define	AC_ADC_CNT	0x34

static uint32_t a10codec_fmt[] = {
	SND_FORMAT(AFMT_S16_LE, 1, 0),
	SND_FORMAT(AFMT_S16_LE, 2, 0),
	0
};

static struct pcmchan_caps a10codec_pcaps = { 8000, 192000, a10codec_fmt, 0 };
static struct pcmchan_caps a10codec_rcaps = { 8000, 48000, a10codec_fmt, 0 };

struct a10codec_info;

struct a10codec_chinfo {
	struct snd_dbuf		*buffer;
	struct pcm_channel	*channel;	
	struct a10codec_info	*parent;
	bus_dmamap_t		dmamap;
	void			*dmaaddr;
	bus_addr_t		physaddr;
	bus_size_t		fifo;
	device_t		dmac;
	void			*dmachan;

	int			dir;
	int			run;
	uint32_t		pos;
	uint32_t		format;
	uint32_t		blocksize;
	uint32_t		speed;
};

struct a10codec_info {
	device_t		dev;
	struct resource		*res[2];
	struct mtx		*lock;
	bus_dma_tag_t		dmat;
	unsigned		dmasize;
	void			*ih;

	unsigned		drqtype_codec;
	unsigned		drqtype_sdram;

	struct a10codec_chinfo	play;
	struct a10codec_chinfo	rec;
};

static struct resource_spec a10codec_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

#define	CODEC_READ(sc, reg)		bus_read_4((sc)->res[0], (reg))
#define	CODEC_WRITE(sc, reg, val)	bus_write_4((sc)->res[0], (reg), (val))

/*
 * Mixer interface
 */

static int
a10codec_mixer_init(struct snd_mixer *m)
{
	struct a10codec_info *sc = mix_getdevinfo(m);
	pcell_t prop[4];
	phandle_t node;
	device_t gpio;
	uint32_t val;
	ssize_t len;
	int pin;

	mix_setdevs(m, SOUND_MASK_VOLUME | SOUND_MASK_LINE | SOUND_MASK_RECLEV);
	mix_setrecdevs(m, SOUND_MASK_LINE | SOUND_MASK_LINE1 | SOUND_MASK_MIC);

	/* Unmute input source to PA */
	val = CODEC_READ(sc, AC_DAC_ACTL);
	val |= DAC_ACTL_PAMUTE;
	CODEC_WRITE(sc, AC_DAC_ACTL, val);

	/* Enable PA */
	val = CODEC_READ(sc, AC_ADC_ACTL);
	val |= ADC_ACTL_PA_EN;
	CODEC_WRITE(sc, AC_ADC_ACTL, val);

	/* Unmute PA */
	node = ofw_bus_get_node(sc->dev);
	len = OF_getencprop(node, "allwinner,pa-gpios", prop, sizeof(prop));
	if (len > 0 && (len / sizeof(prop[0])) == 4) {
		gpio = OF_device_from_xref(prop[0]);
		if (gpio != NULL) {
			pin = prop[1] * 32 + prop[2];
			GPIO_PIN_SETFLAGS(gpio, pin, GPIO_PIN_OUTPUT);
			GPIO_PIN_SET(gpio, pin, GPIO_PIN_LOW);
		}
	}

	return (0);
}

static const struct a10codec_mixer {
	unsigned reg;
	unsigned mask;
	unsigned shift;
} a10codec_mixers[SOUND_MIXER_NRDEVICES] = {
	[SOUND_MIXER_VOLUME]	= { AC_DAC_ACTL, DAC_ACTL_PAVOL_MASK,
				    DAC_ACTL_PAVOL_SHIFT },
	[SOUND_MIXER_LINE]	= { AC_ADC_ACTL, ADC_ACTL_LNPREG_MASK,
				    ADC_ACTL_LNPREG_SHIFT },
	[SOUND_MIXER_RECLEV]	= { AC_ADC_ACTL, ADC_ACTL_ADCG_MASK,
				    ADC_ACTL_ADCG_SHIFT },
}; 

static int
a10codec_mixer_set(struct snd_mixer *m, unsigned dev, unsigned left,
    unsigned right)
{
	struct a10codec_info *sc = mix_getdevinfo(m);
	uint32_t val;
	unsigned nvol, max;

	max = a10codec_mixers[dev].mask >> a10codec_mixers[dev].shift;
	nvol = (left * max) / 100;

	val = CODEC_READ(sc, a10codec_mixers[dev].reg);
	val &= ~a10codec_mixers[dev].mask;
	val |= (nvol << a10codec_mixers[dev].shift);
	CODEC_WRITE(sc, a10codec_mixers[dev].reg, val);

	left = right = (left * 100) / max;
	return (left | (right << 8));
}

static uint32_t
a10codec_mixer_setrecsrc(struct snd_mixer *m, uint32_t src)
{
	struct a10codec_info *sc = mix_getdevinfo(m);
	uint32_t val;

	val = CODEC_READ(sc, AC_ADC_ACTL);

	switch (src) {
	case SOUND_MASK_LINE:	/* line-in */
		val &= ~ADC_ACTL_ADCIS_MASK;
		val |= (ADC_IS_LINEIN << ADC_ACTL_ADCIS_SHIFT);
		break;
	case SOUND_MASK_MIC:	/* MIC1 */
		val &= ~ADC_ACTL_ADCIS_MASK;
		val |= (ADC_IS_MIC1 << ADC_ACTL_ADCIS_SHIFT);
		break;
	case SOUND_MASK_LINE1:	/* MIC2 */
		val &= ~ADC_ACTL_ADCIS_MASK;
		val |= (ADC_IS_MIC2 << ADC_ACTL_ADCIS_SHIFT);
		break;
	default:
		break;
	}

	CODEC_WRITE(sc, AC_ADC_ACTL, val);

	switch ((val & ADC_ACTL_ADCIS_MASK) >> ADC_ACTL_ADCIS_SHIFT) {
	case ADC_IS_LINEIN:
		return (SOUND_MASK_LINE);
	case ADC_IS_MIC1:
		return (SOUND_MASK_MIC);
	case ADC_IS_MIC2:
		return (SOUND_MASK_LINE1);
	default:
		return (0);
	}
}

static kobj_method_t a10codec_mixer_methods[] = {
	KOBJMETHOD(mixer_init,		a10codec_mixer_init),
	KOBJMETHOD(mixer_set,		a10codec_mixer_set),
	KOBJMETHOD(mixer_setrecsrc,	a10codec_mixer_setrecsrc),
	KOBJMETHOD_END
};
MIXER_DECLARE(a10codec_mixer);


/*
 * Channel interface
 */

static void
a10codec_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct a10codec_chinfo *ch = arg;

	if (error != 0)
		return;

	ch->physaddr = segs[0].ds_addr;
}

static void
a10codec_transfer(struct a10codec_chinfo *ch)
{
	bus_addr_t src, dst;
	int error;

	if (ch->dir == PCMDIR_PLAY) {
		src = ch->physaddr + ch->pos;
		dst = ch->fifo;
	} else {
		src = ch->fifo;
		dst = ch->physaddr + ch->pos;
	}

	error = SUNXI_DMA_TRANSFER(ch->dmac, ch->dmachan, src, dst,
	    ch->blocksize);
	if (error) {
		ch->run = 0;
		device_printf(ch->parent->dev, "DMA transfer failed: %d\n",
		    error);
	}
}

static void
a10codec_dmaconfig(struct a10codec_chinfo *ch)
{
	struct a10codec_info *sc = ch->parent;
	struct sunxi_dma_config conf;

	memset(&conf, 0, sizeof(conf));
	conf.src_width = conf.dst_width = 16;
	conf.src_burst_len = conf.dst_burst_len = 4;

	if (ch->dir == PCMDIR_PLAY) {
		conf.dst_noincr = true;
		conf.src_drqtype = sc->drqtype_sdram;
		conf.dst_drqtype = sc->drqtype_codec;
	} else {
		conf.src_noincr = true;
		conf.src_drqtype = sc->drqtype_codec;
		conf.dst_drqtype = sc->drqtype_sdram;
	}

	SUNXI_DMA_SET_CONFIG(ch->dmac, ch->dmachan, &conf);
}

static void
a10codec_dmaintr(void *priv)
{
	struct a10codec_chinfo *ch = priv;
	unsigned bufsize;

	bufsize = sndbuf_getsize(ch->buffer);

	ch->pos += ch->blocksize;
	if (ch->pos >= bufsize)
		ch->pos -= bufsize;

	if (ch->run) {
		chn_intr(ch->channel);
		a10codec_transfer(ch);
	}
}

static unsigned
a10codec_fs(struct a10codec_chinfo *ch)
{
	switch (ch->speed) {
	case 48000:
		return (DAC_FS_48KHZ);
	case 24000:
		return (DAC_FS_24KHZ);
	case 12000:
		return (DAC_FS_12KHZ);
	case 192000:
		return (DAC_FS_192KHZ);
	case 32000:
		return (DAC_FS_32KHZ);
	case 16000:
		return (DAC_FS_16KHZ);
	case 8000:
		return (DAC_FS_8KHZ);
	case 96000:
		return (DAC_FS_96KHZ);
	default:
		return (DAC_FS_48KHZ);
	}
}

static void
a10codec_start(struct a10codec_chinfo *ch)
{
	struct a10codec_info *sc = ch->parent;
	uint32_t val;

	ch->pos = 0;

	if (ch->dir == PCMDIR_PLAY) {
		/* Flush DAC FIFO */
		CODEC_WRITE(sc, AC_DAC_FIFOC, DAC_FIFOC_FIFO_FLUSH);

		/* Clear DAC FIFO status */
		CODEC_WRITE(sc, AC_DAC_FIFOS, CODEC_READ(sc, AC_DAC_FIFOS));

		/* Enable DAC analog left/right channels and output mixer */
		val = CODEC_READ(sc, AC_DAC_ACTL);
		val |= DAC_ACTL_DACAREN;
		val |= DAC_ACTL_DACALEN;
		val |= DAC_ACTL_DACPAS;
		CODEC_WRITE(sc, AC_DAC_ACTL, val);

		/* Configure DAC DMA channel */
		a10codec_dmaconfig(ch);

		/* Configure DAC FIFO */
		CODEC_WRITE(sc, AC_DAC_FIFOC,
		    (AFMT_CHANNEL(ch->format) == 1 ? DAC_FIFOC_MONO_EN : 0) |
		    (a10codec_fs(ch) << DAC_FIFOC_FS_SHIFT) |
		    (FIFO_MODE_16_15_0 << DAC_FIFOC_FIFO_MODE_SHIFT) |
		    (DRQ_CLR_CNT << DAC_FIFOC_DRQ_CLR_CNT_SHIFT) |
		    (TX_TRIG_LEVEL << DAC_FIFOC_TX_TRIG_LEVEL_SHIFT));

		/* Enable DAC DRQ */
		val = CODEC_READ(sc, AC_DAC_FIFOC);
		val |= DAC_FIFOC_DRQ_EN;
		CODEC_WRITE(sc, AC_DAC_FIFOC, val);
	} else {
		/* Flush ADC FIFO */
		CODEC_WRITE(sc, AC_ADC_FIFOC, ADC_FIFOC_FIFO_FLUSH);

		/* Clear ADC FIFO status */
		CODEC_WRITE(sc, AC_ADC_FIFOS, CODEC_READ(sc, AC_ADC_FIFOS));

		/* Enable ADC analog left/right channels, MIC1 preamp,
		 * and VMIC pin voltage
		 */
		val = CODEC_READ(sc, AC_ADC_ACTL);
		val |= ADC_ACTL_ADCREN;
		val |= ADC_ACTL_ADCLEN;
		val |= ADC_ACTL_PREG1EN;
		val |= ADC_ACTL_VMICEN;
		CODEC_WRITE(sc, AC_ADC_ACTL, val);

		/* Configure ADC DMA channel */
		a10codec_dmaconfig(ch);

		/* Configure ADC FIFO */
		CODEC_WRITE(sc, AC_ADC_FIFOC,
		    ADC_FIFOC_EN_AD |
		    ADC_FIFOC_RX_FIFO_MODE |
		    (AFMT_CHANNEL(ch->format) == 1 ? ADC_FIFOC_MONO_EN : 0) |
		    (a10codec_fs(ch) << ADC_FIFOC_FS_SHIFT) |
		    (RX_TRIG_LEVEL << ADC_FIFOC_RX_TRIG_LEVEL_SHIFT));

		/* Enable ADC DRQ */
		val = CODEC_READ(sc, AC_ADC_FIFOC);
		val |= ADC_FIFOC_DRQ_EN;
		CODEC_WRITE(sc, AC_ADC_FIFOC, val);
	}

	/* Start DMA transfer */
	a10codec_transfer(ch);
}

static void
a10codec_stop(struct a10codec_chinfo *ch)
{
	struct a10codec_info *sc = ch->parent;
	uint32_t val;

	/* Disable DMA channel */
	SUNXI_DMA_HALT(ch->dmac, ch->dmachan);

	if (ch->dir == PCMDIR_PLAY) {
		/* Disable DAC analog left/right channels and output mixer */
		val = CODEC_READ(sc, AC_DAC_ACTL);
		val &= ~DAC_ACTL_DACAREN;
		val &= ~DAC_ACTL_DACALEN;
		val &= ~DAC_ACTL_DACPAS;
		CODEC_WRITE(sc, AC_DAC_ACTL, val);

		/* Disable DAC DRQ */
		CODEC_WRITE(sc, AC_DAC_FIFOC, 0);
	} else {
		/* Disable ADC analog left/right channels, MIC1 preamp,
		 * and VMIC pin voltage
		 */
		val = CODEC_READ(sc, AC_ADC_ACTL);
		val &= ~ADC_ACTL_ADCREN;
		val &= ~ADC_ACTL_ADCLEN;
		val &= ~ADC_ACTL_PREG1EN;
		val &= ~ADC_ACTL_VMICEN;
		CODEC_WRITE(sc, AC_ADC_ACTL, val);

		/* Disable ADC DRQ */
		CODEC_WRITE(sc, AC_ADC_FIFOC, 0);
	}
}

static void *
a10codec_chan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
    struct pcm_channel *c, int dir)
{
	struct a10codec_info *sc = devinfo;
	struct a10codec_chinfo *ch = dir == PCMDIR_PLAY ? &sc->play : &sc->rec;
	int error;

	ch->parent = sc;
	ch->channel = c;
	ch->buffer = b;
	ch->dir = dir;
	ch->fifo = rman_get_start(sc->res[0]) +
	    (dir == PCMDIR_REC ? AC_ADC_RXDATA : AC_DAC_TXDATA);

	ch->dmac = devclass_get_device(devclass_find("a10dmac"), 0);
	if (ch->dmac == NULL) {
		device_printf(sc->dev, "cannot find DMA controller\n");
		return (NULL);
	}
	ch->dmachan = SUNXI_DMA_ALLOC(ch->dmac, false, a10codec_dmaintr, ch);
	if (ch->dmachan == NULL) {
		device_printf(sc->dev, "cannot allocate DMA channel\n");
		return (NULL);
	}

	error = bus_dmamem_alloc(sc->dmat, &ch->dmaaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &ch->dmamap);
	if (error != 0) {
		device_printf(sc->dev, "cannot allocate channel buffer\n");
		return (NULL);
	}
	error = bus_dmamap_load(sc->dmat, ch->dmamap, ch->dmaaddr,
	    sc->dmasize, a10codec_dmamap_cb, ch, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->dev, "cannot load DMA map\n");
		return (NULL);
	}
	memset(ch->dmaaddr, 0, sc->dmasize);

	if (sndbuf_setup(ch->buffer, ch->dmaaddr, sc->dmasize) != 0) {
		device_printf(sc->dev, "cannot setup sndbuf\n");
		return (NULL);
	}

	return (ch);
}

static int
a10codec_chan_free(kobj_t obj, void *data)
{
	struct a10codec_chinfo *ch = data;
	struct a10codec_info *sc = ch->parent;

	SUNXI_DMA_FREE(ch->dmac, ch->dmachan);
	bus_dmamap_unload(sc->dmat, ch->dmamap);
	bus_dmamem_free(sc->dmat, ch->dmaaddr, ch->dmamap);

	return (0);
}

static int
a10codec_chan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct a10codec_chinfo *ch = data;

	ch->format = format;

	return (0);
}

static uint32_t
a10codec_chan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct a10codec_chinfo *ch = data;

	/*
	 * The codec supports full duplex operation but both DAC and ADC
	 * use the same source clock (PLL2). Limit the available speeds to
	 * those supported by a 24576000 Hz input.
	 */
	switch (speed) {
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
		ch->speed = speed;
		break;
	case 96000:
	case 192000:
		/* 96 KHz / 192 KHz mode only supported for playback */
		if (ch->dir == PCMDIR_PLAY) {
			ch->speed = speed;
		} else {
			ch->speed = 48000;
		}
		break;
	case 44100:
		ch->speed = 48000;
		break;
	case 22050:
		ch->speed = 24000;
		break;
	case 11025:
		ch->speed = 12000;
		break;
	default:
		ch->speed = 48000;
		break;
	}

	return (ch->speed);
}

static uint32_t
a10codec_chan_setblocksize(kobj_t obj, void *data, uint32_t blocksize)
{
	struct a10codec_chinfo *ch = data;

	ch->blocksize = blocksize & ~3;

	return (ch->blocksize);
}

static int
a10codec_chan_trigger(kobj_t obj, void *data, int go)
{
	struct a10codec_chinfo *ch = data;
	struct a10codec_info *sc = ch->parent;

	if (!PCMTRIG_COMMON(go))
		return (0);

	snd_mtxlock(sc->lock);
	switch (go) {
	case PCMTRIG_START:
		ch->run = 1;
		a10codec_start(ch);
		break;
	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		ch->run = 0;
		a10codec_stop(ch);
		break;
	default:
		break;
	}
	snd_mtxunlock(sc->lock);

	return (0);
}

static uint32_t
a10codec_chan_getptr(kobj_t obj, void *data)
{
	struct a10codec_chinfo *ch = data;

	return (ch->pos);
}

static struct pcmchan_caps *
a10codec_chan_getcaps(kobj_t obj, void *data)
{
	struct a10codec_chinfo *ch = data;

	if (ch->dir == PCMDIR_PLAY) {
		return (&a10codec_pcaps);
	} else {
		return (&a10codec_rcaps);
	}
}

static kobj_method_t a10codec_chan_methods[] = {
	KOBJMETHOD(channel_init,		a10codec_chan_init),
	KOBJMETHOD(channel_free,		a10codec_chan_free),
	KOBJMETHOD(channel_setformat,		a10codec_chan_setformat),
	KOBJMETHOD(channel_setspeed,		a10codec_chan_setspeed),
	KOBJMETHOD(channel_setblocksize,	a10codec_chan_setblocksize),
	KOBJMETHOD(channel_trigger,		a10codec_chan_trigger),
	KOBJMETHOD(channel_getptr,		a10codec_chan_getptr),
	KOBJMETHOD(channel_getcaps,		a10codec_chan_getcaps),
	KOBJMETHOD_END
};
CHANNEL_DECLARE(a10codec_chan);


/*
 * Device interface
 */

static int
a10codec_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "allwinner,sun7i-a20-codec"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner Audio Codec");
	return (BUS_PROBE_DEFAULT);
}

static int
a10codec_attach(device_t dev)
{
	struct a10codec_info *sc;
	char status[SND_STATUSLEN];
	clk_t clk_apb, clk_codec;
	uint32_t val;
	int error;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);
	sc->dev = dev;
	sc->lock = snd_mtxcreate(device_get_nameunit(dev), "a10codec softc");

	if (bus_alloc_resources(dev, a10codec_spec, sc->res)) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	/* XXX DRQ types should come from FDT, but how? */
	if (ofw_bus_is_compatible(dev, "allwinner,sun4i-a10-codec") ||
	    ofw_bus_is_compatible(dev, "allwinner,sun7i-a20-codec")) {
		sc->drqtype_codec = 19;
		sc->drqtype_sdram = 22;
	} else {
		device_printf(dev, "DRQ types not known for this SoC\n");
		error = ENXIO;
		goto fail;
	}

	sc->dmasize = 131072;
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),
	    4, sc->dmasize,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    sc->dmasize, 1,		/* maxsize, nsegs */
	    sc->dmasize, 0,		/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->dmat);
	if (error != 0) {
		device_printf(dev, "cannot create DMA tag\n");
		goto fail;
	}

	/* Get clocks */
	error = clk_get_by_ofw_name(dev, "apb", &clk_apb);
	if (error != 0) {
		device_printf(dev, "cannot find apb clock\n");
		goto fail;
	}
	error = clk_get_by_ofw_name(dev, "codec", &clk_codec);
	if (error != 0) {
		device_printf(dev, "cannot find codec clock\n");
		goto fail;
	}

	/* Gating APB clock for codec */
	error = clk_enable(clk_apb);
	if (error != 0) {
		device_printf(dev, "cannot enable apb clock\n");
		goto fail;
	}
	/* Activate audio codec clock. According to the A10 and A20 user
	 * manuals, Audio_pll can be either 24.576MHz or 22.5792MHz. Most
	 * audio sampling rates require an 24.576MHz input clock with the
	 * exception of 44.1kHz, 22.05kHz, and 11.025kHz. Unfortunately,
	 * both capture and playback use the same clock source so to
	 * safely support independent full duplex operation, we use a fixed
	 * 24.576MHz clock source and don't advertise native support for
	 * the three sampling rates that require a 22.5792MHz input.
	 */
	error = clk_set_freq(clk_codec, 24576000, CLK_SET_ROUND_DOWN);
	if (error != 0) {
		device_printf(dev, "cannot set codec clock frequency\n");
		goto fail;
	}
	/* Enable audio codec clock */
	error = clk_enable(clk_codec);
	if (error != 0) {
		device_printf(dev, "cannot enable codec clock\n");
		goto fail;
	}

	/* Enable DAC */
	val = CODEC_READ(sc, AC_DAC_DPC);
	val |= DAC_DPC_EN_DA;
	CODEC_WRITE(sc, AC_DAC_DPC, val);

#ifdef notdef
	error = snd_setup_intr(dev, sc->irq, INTR_MPSAFE, a10codec_intr, sc,
	    &sc->ih);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler\n");
		goto fail;
	}
#endif

	if (mixer_init(dev, &a10codec_mixer_class, sc)) {
		device_printf(dev, "mixer_init failed\n");
		goto fail;
	}

	pcm_setflags(dev, pcm_getflags(dev) | SD_F_MPSAFE);

	if (pcm_register(dev, sc, 1, 1)) {
		device_printf(dev, "pcm_register failed\n");
		goto fail;
	}

	pcm_addchan(dev, PCMDIR_PLAY, &a10codec_chan_class, sc);
	pcm_addchan(dev, PCMDIR_REC, &a10codec_chan_class, sc);

	snprintf(status, SND_STATUSLEN, "at %s", ofw_bus_get_name(dev));
	pcm_setstatus(dev, status);

	return (0);

fail:
	bus_release_resources(dev, a10codec_spec, sc->res);
	snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);

	return (error);
}

static device_method_t a10codec_pcm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		a10codec_probe),
	DEVMETHOD(device_attach,	a10codec_attach),

	DEVMETHOD_END
};

static driver_t a10codec_pcm_driver = {
	"pcm",
	a10codec_pcm_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(a10codec, simplebus, a10codec_pcm_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(a10codec, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(a10codec, 1);
