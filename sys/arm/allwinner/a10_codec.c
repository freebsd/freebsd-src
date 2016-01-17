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

#include "opt_bus.h"

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

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "sunxi_dma_if.h"
#include "mixer_if.h"
#include "gpio_if.h"

#include "a10_clk.h"
#include "a10_dmac.h"

#define TX_TRIG_LEVEL	0xf
#define RX_TRIG_LEVEL	0x7
#define DRQ_CLR_CNT	0x3

#define AC_DAC_DPC	0x00
#define  DAC_DPC_EN_DA			0x80000000
#define AC_DAC_FIFOC	0x04
#define  DAC_FIFOC_FS_SHIFT		29
#define  DAC_FIFOC_FS_MASK		(7U << DAC_FIFOC_FS_SHIFT)
#define   DAC_FS_48KHZ			0
#define  DAC_FIFOC_FIFO_MODE_SHIFT	24
#define  DAC_FIFOC_FIFO_MODE_MASK	(3U << DAC_FIFOC_FIFO_MODE_SHIFT)
#define   FIFO_MODE_16_15_0		1
#define  DAC_FIFOC_DRQ_CLR_CNT_SHIFT	21
#define  DAC_FIFOC_DRQ_CLR_CNT_MASK	(3U << DAC_FIFOC_DRQ_CLR_CNT_SHIFT)
#define  DAC_FIFOC_TX_TRIG_LEVEL_SHIFT	8
#define  DAC_FIFOC_TX_TRIG_LEVEL_MASK	(0x7f << DAC_FIFOC_TX_TRIG_LEVEL_SHIFT)
#define  DAC_FIFOC_DRQ_EN		(1U << 4)
#define  DAC_FIFOC_FIFO_FLUSH		(1U << 0)
#define AC_DAC_FIFOS	0x08
#define AC_DAC_TXDATA	0x0c
#define AC_DAC_ACTL	0x10
#define  DAC_ACTL_DACAREN		(1U << 31)
#define  DAC_ACTL_DACALEN		(1U << 30)
#define  DAC_ACTL_MIXEN			(1U << 29)
#define  DAC_ACTL_DACPAS		(1U << 8)
#define  DAC_ACTL_PAMUTE		(1U << 6)
#define  DAC_ACTL_PAVOL_SHIFT		0
#define  DAC_ACTL_PAVOL_MASK		(0x3f << DAC_ACTL_PAVOL_SHIFT)
#define AC_ADC_FIFOC	0x1c
#define  ADC_FIFOC_FS_SHIFT		29
#define  ADC_FIFOC_FS_MASK		(7U << ADC_FIFOC_FS_SHIFT)
#define   ADC_FS_48KHZ		0
#define  ADC_FIFOC_EN_AD		(1U << 28)
#define  ADC_FIFOC_RX_FIFO_MODE		(1U << 24)
#define  ADC_FIFOC_RX_TRIG_LEVEL_SHIFT	8
#define  ADC_FIFOC_RX_TRIG_LEVEL_MASK	(0x1f << ADC_FIFOC_RX_TRIG_LEVEL_SHIFT)
#define  ADC_FIFOC_DRQ_EN		(1U << 4)
#define  ADC_FIFOC_FIFO_FLUSH		(1U << 1)
#define AC_ADC_FIFOS	0x20
#define AC_ADC_RXDATA	0x24
#define AC_ADC_ACTL	0x28
#define  ADC_ACTL_ADCREN		(1U << 31)
#define  ADC_ACTL_ADCLEN		(1U << 30)
#define  ADC_ACTL_PREG1EN		(1U << 29)
#define  ADC_ACTL_PREG2EN		(1U << 28)
#define  ADC_ACTL_VMICEN		(1U << 27)
#define  ADC_ACTL_ADCG_SHIFT		20
#define  ADC_ACTL_ADCG_MASK		(7U << ADC_ACTL_ADCG_SHIFT)
#define  ADC_ACTL_ADCIS_SHIFT		17
#define  ADC_ACTL_ADCIS_MASK		(7U << ADC_ACTL_ADCIS_SHIFT)
#define   ADC_IS_LINEIN			0
#define   ADC_IS_FMIN			1
#define   ADC_IS_MIC1			2
#define   ADC_IS_MIC2			3
#define   ADC_IS_MIC1_L_MIC2_R		4
#define   ADC_IS_MIC1_LR_MIC2_LR	5
#define   ADC_IS_OMIX			6
#define   ADC_IS_LINEIN_L_MIC1_R	7
#define  ADC_ACTL_LNRDF			(1U << 16)
#define  ADC_ACTL_LNPREG_SHIFT		13
#define  ADC_ACTL_LNPREG_MASK		(7U << ADC_ACTL_LNPREG_SHIFT)
#define  ADC_ACTL_PA_EN			(1U << 4)
#define  ADC_ACTL_DDE			(1U << 3)
#define AC_DAC_CNT	0x30
#define AC_ADC_CNT	0x34

static uint32_t a10codec_format[] = {
	SND_FORMAT(AFMT_S16_LE, 2, 0),
	0
};

static struct pcmchan_caps a10codec_caps = { 48000, 48000, a10codec_format, 0 };

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
	struct resource		*res;
	struct resource 	*irq;
	struct mtx		*lock;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	bus_dma_tag_t		dmat;
	unsigned		dmasize;
	void			*ih;

	unsigned		drqtype_codec;
	unsigned		drqtype_sdram;

	struct a10codec_chinfo	play;
	struct a10codec_chinfo	rec;
};

#define CODEC_READ(sc, reg)		\
    bus_space_read_4((sc)->bst, (sc)->bsh, (reg))
#define CODEC_WRITE(sc, reg, val)	\
    bus_space_write_4((sc)->bst, (sc)->bsh, (reg), (val))

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
	len = OF_getencprop(node, "pamute-gpio", prop, sizeof(prop));
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
		device_printf(ch->parent->dev, "DMA transfer failed: %d\n", error);
	}
}

static void
a10codec_dmaconfig(struct a10codec_chinfo *ch)
{
	struct a10codec_info *sc = ch->parent;
	uint32_t conf;

	conf = AWIN_DMA_CTL_BC_REMAINING |
	    (AWIN_DMA_CTL_DATA_WIDTH_16 << AWIN_DMA_CTL_DST_DATA_WIDTH_SHIFT) |
	    (AWIN_DMA_CTL_BURST_LEN_4 << AWIN_DMA_CTL_DST_BURST_LEN_SHIFT) |
	    (AWIN_DMA_CTL_DATA_WIDTH_16 << AWIN_DMA_CTL_SRC_DATA_WIDTH_SHIFT) |
	    (AWIN_DMA_CTL_BURST_LEN_4 << AWIN_DMA_CTL_SRC_BURST_LEN_SHIFT);

	if (ch->dir == PCMDIR_PLAY) {
		conf |= AWIN_NDMA_CTL_DST_ADDR_NOINCR;
		conf |= (sc->drqtype_sdram << AWIN_DMA_CTL_SRC_DRQ_TYPE_SHIFT);
		conf |= (sc->drqtype_codec << AWIN_DMA_CTL_DST_DRQ_TYPE_SHIFT);
	} else {
		conf |= AWIN_NDMA_CTL_SRC_ADDR_NOINCR;
		conf |= (sc->drqtype_codec << AWIN_DMA_CTL_SRC_DRQ_TYPE_SHIFT);
		conf |= (sc->drqtype_sdram << AWIN_DMA_CTL_DST_DRQ_TYPE_SHIFT);
	}

	SUNXI_DMA_SET_CONFIG(ch->dmac, ch->dmachan, conf);
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
		    (DAC_FS_48KHZ << DAC_FIFOC_FS_SHIFT) |
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

		/* Enable ADC analog left/right channels */
		val = CODEC_READ(sc, AC_ADC_ACTL);
		val |= ADC_ACTL_ADCREN;
		val |= ADC_ACTL_ADCLEN;
		CODEC_WRITE(sc, AC_ADC_ACTL, val);

		/* Configure ADC DMA channel */
		a10codec_dmaconfig(ch);

		/* Configure ADC FIFO */
		CODEC_WRITE(sc, AC_ADC_FIFOC,
		    ADC_FIFOC_EN_AD |
		    ADC_FIFOC_RX_FIFO_MODE |
		    (ADC_FS_48KHZ << ADC_FIFOC_FS_SHIFT) |
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
		/* Disable ADC analog left/right channels */
		val = CODEC_READ(sc, AC_ADC_ACTL);
		val &= ~ADC_ACTL_ADCREN;
		val &= ~ADC_ACTL_ADCLEN;
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
	ch->fifo = rman_get_start(sc->res) +
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

	ch->speed = 48000;

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
	return (&a10codec_caps);
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
	int rid, error;
	uint32_t val;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);
	sc->dev = dev;
	sc->lock = snd_mtxcreate(device_get_nameunit(dev), "a10codec softc");

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "unable to map memory\n");
		error = ENXIO;
		goto fail;
	}
	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq == NULL) {
		device_printf(dev, "cannot allocate IRQ resources\n");
		error = ENXIO;
		goto fail;
	}

	if (ofw_bus_is_compatible(dev, "allwinner,sun7i-a20-codec")) {
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

	/* Activate audio codec clock */
	a10_clk_codec_activate();

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
	if (sc->res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->res);
	if (sc->irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);

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
