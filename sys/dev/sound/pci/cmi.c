/*
 * Copyright (c) 2000 Orion Hodson <O.Hodson@cs.ucl.ac.uk>
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
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHERIN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THEPOSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This driver exists largely as a result of other people's efforts.
 * Much of register handling is based on NetBSD CMI8x38 audio driver
 * by Takuya Shiozaki <AoiMoe@imou.to>.  Chen-Li Tien
 * <cltien@cmedia.com.tw> clarified points regarding the DMA related
 * registers and the 8738 mixer devices.  His Linux was driver a also
 * useful reference point.
 *
 * TODO: MIDI / suspend / resume
 *
 * SPDIF contributed by Gerhard Gonter <gonter@whisky.wu-wien.ac.at>.
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pci/cmireg.h>
#include <dev/sound/isa/sb.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include "mixer_if.h"

/* Supported chip ID's */
#define CMI8338A_PCI_ID   0x010013f6
#define CMI8338B_PCI_ID   0x010113f6
#define CMI8738_PCI_ID    0x011113f6
#define CMI8738B_PCI_ID   0x011213f6

/* Buffer size max is 64k for permitted DMA boundaries */
#define CMI_BUFFER_SIZE      16384

/* Interrupts per length of buffer */
#define CMI_INTR_PER_BUFFER      2

/* Clarify meaning of named defines in cmireg.h */
#define CMPCI_REG_DMA0_MAX_SAMPLES  CMPCI_REG_DMA0_BYTES
#define CMPCI_REG_DMA0_INTR_SAMPLES CMPCI_REG_DMA0_SAMPLES
#define CMPCI_REG_DMA1_MAX_SAMPLES  CMPCI_REG_DMA1_BYTES
#define CMPCI_REG_DMA1_INTR_SAMPLES CMPCI_REG_DMA1_SAMPLES

/* Our indication of custom mixer control */
#define CMPCI_NON_SB16_CONTROL		0xff

/* Debugging macro's */
#ifndef DEB
#define DEB(x) /* x */
#endif /* DEB */

#ifndef DEBMIX
#define DEBMIX(x) /* x */
#endif  /* DEBMIX */

/* ------------------------------------------------------------------------- */
/* Structures */

struct cmi_info;

struct cmi_chinfo {
	struct cmi_info *parent;
	struct pcm_channel *channel;
	struct snd_dbuf *buffer;
	int dir;
	int bps; /* bytes per sample */
	u_int32_t fmt, spd, phys_buf;
	u_int32_t dma_configured;
};

struct cmi_info {
	device_t dev;
	u_int32_t type, rev;

	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t parent_dmat;
	struct resource *reg, *irq;
	int	regid, irqid;
	void *ih;

	struct cmi_chinfo pch, rch;
};

/* Channel caps */

static u_int32_t cmi_fmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static struct pcmchan_caps cmi_caps = {5512, 48000, cmi_fmt, 0};

/* ------------------------------------------------------------------------- */
/* Register Utilities */

static u_int32_t
cmi_rd(struct cmi_info *cmi, int regno, int size)
{
	switch (size) {
	case 1:
		return bus_space_read_1(cmi->st, cmi->sh, regno);
	case 2:
		return bus_space_read_2(cmi->st, cmi->sh, regno);
	case 4:
		return bus_space_read_4(cmi->st, cmi->sh, regno);
	default:
		DEB(printf("cmi_rd: failed 0x%04x %d\n", regno, size));
		return 0xFFFFFFFF;
	}
}

static void
cmi_wr(struct cmi_info *cmi, int regno, u_int32_t data, int size)
{
	switch (size) {
	case 1:
		bus_space_write_1(cmi->st, cmi->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(cmi->st, cmi->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(cmi->st, cmi->sh, regno, data);
		break;
	}
	DELAY(10);
}

static void
cmi_partial_wr4(struct cmi_info *cmi,
		int reg, int shift, u_int32_t mask, u_int32_t val)
{
	u_int32_t r;

	r = cmi_rd(cmi, reg, 4);
	r &= ~(mask << shift);
	r |= val << shift;
	cmi_wr(cmi, reg, r, 4);
}

static void
cmi_clr4(struct cmi_info *cmi, int reg, u_int32_t mask)
{
	u_int32_t r;

	r = cmi_rd(cmi, reg, 4);
	r &= ~mask;
	cmi_wr(cmi, reg, r, 4);
}

static void
cmi_set4(struct cmi_info *cmi, int reg, u_int32_t mask)
{
	u_int32_t r;

	r = cmi_rd(cmi, reg, 4);
	r |= mask;
	cmi_wr(cmi, reg, r, 4);
}

/* ------------------------------------------------------------------------- */
/* Rate Mapping */

static int cmi_rates[] = {5512, 8000, 11025, 16000,
			  22050, 32000, 44100, 48000};
#define NUM_CMI_RATES (sizeof(cmi_rates)/sizeof(cmi_rates[0]))

/* cmpci_rate_to_regvalue returns sampling freq selector for FCR1
 * register - reg order is 5k,11k,22k,44k,8k,16k,32k,48k */

static u_int32_t
cmpci_rate_to_regvalue(int rate)
{
	int i, r;

	for(i = 0; i < NUM_CMI_RATES - 1; i++) {
		if (rate < ((cmi_rates[i] + cmi_rates[i + 1]) / 2)) {
			break;
		}
	}

	DEB(printf("cmpci_rate_to_regvalue: %d -> %d\n", rate, cmi_rates[i]));

	r = ((i >> 1) | (i << 2)) & 0x07;
	return r;
}

static int
cmpci_regvalue_to_rate(u_int32_t r)
{
	int i;

	i = ((r << 1) | (r >> 2)) & 0x07;
	DEB(printf("cmpci_regvalue_to_rate: %d -> %d\n", r, i));
	return cmi_rates[i];
}

/* ------------------------------------------------------------------------- */
/* ADC/DAC control */

static void
cmi_dac_start(struct cmi_info *cmi, struct cmi_chinfo *ch)
{
	if (ch->dma_configured == 0) {
		u_int32_t s, i, sz;
		ch->phys_buf = vtophys(sndbuf_getbuf(ch->buffer));
		sz = (u_int32_t)sndbuf_getsize(ch->buffer);
		s = (sz + 1) / ch->bps - 1;
		i = (sz + 1) / (ch->bps * CMI_INTR_PER_BUFFER) - 1;
		cmi_wr(cmi, CMPCI_REG_DMA0_BASE, ch->phys_buf, 4);
		cmi_wr(cmi, CMPCI_REG_DMA0_MAX_SAMPLES, s, 2);
		cmi_wr(cmi, CMPCI_REG_DMA0_INTR_SAMPLES, i, 2);
		ch->dma_configured = 1;
		DEB(printf("cmi_dac_start: dma prog\n"));
	}
	cmi_clr4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_DIR);
	cmi_clr4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_PAUSE);
	cmi_set4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_ENABLE);
	cmi_set4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
}

static void
cmi_dac_stop(struct cmi_info *cmi)
{
	cmi_clr4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
	cmi_clr4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_ENABLE);
}

static void
cmi_dac_reset(struct cmi_info *cmi, struct cmi_chinfo *ch)
{
	cmi_dac_stop(cmi);
	cmi_set4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_RESET);
	cmi_clr4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_RESET);
	ch->dma_configured = 0;
	DEB(printf("cmi_dac_reset\n"));
}

static void
cmi_adc_start(struct cmi_info *cmi, struct cmi_chinfo *ch)
{
	if (ch->dma_configured == 0) {
		u_int32_t s, i, sz;
		ch->phys_buf = vtophys(sndbuf_getbuf(ch->buffer));
		sz = (u_int32_t)sndbuf_getsize(ch->buffer);
		s = (sz + 1) / ch->bps - 1;
		i = (sz + 1) / (ch->bps * CMI_INTR_PER_BUFFER) - 1;

		cmi_wr(cmi, CMPCI_REG_DMA1_BASE, ch->phys_buf, 4);
		cmi_wr(cmi, CMPCI_REG_DMA1_MAX_SAMPLES, s, 2);
		cmi_wr(cmi, CMPCI_REG_DMA1_INTR_SAMPLES, i, 2);
		ch->dma_configured = 1;
		DEB(printf("cmi_adc_start: dma prog\n"));
	}

	cmi_set4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_DIR);
	cmi_clr4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_PAUSE);
	cmi_set4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_ENABLE);
	cmi_set4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
}

static void
cmi_adc_stop(struct cmi_info *cmi)
{
	cmi_clr4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
	cmi_clr4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_ENABLE);
}

static void
cmi_adc_reset(struct cmi_info *cmi, struct cmi_chinfo *ch)
{
	cmi_adc_stop(cmi);
	cmi_set4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_RESET);
	cmi_clr4(cmi, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_RESET);
	ch->dma_configured = 0;
	DEB(printf("cmi_adc_reset\n"));
}

static void
cmi_spdif_speed(struct cmi_info *cmi, int speed) {
	u_int32_t fcr1, lcr, mcr;

	if (speed >= 44100) {
		fcr1 = CMPCI_REG_SPDIF0_ENABLE;
		lcr  = CMPCI_REG_XSPDIF_ENABLE;
		mcr  = (speed == 48000) ?
			CMPCI_REG_W_SPDIF_48L | CMPCI_REG_SPDIF_48K : 0;
	} else {
		fcr1 = mcr = lcr = 0;
	}

	cmi_partial_wr4(cmi, CMPCI_REG_MISC, 0,
			CMPCI_REG_W_SPDIF_48L | CMPCI_REG_SPDIF_48K, mcr);
	cmi_partial_wr4(cmi, CMPCI_REG_LEGACY_CTRL, 0,
			CMPCI_REG_XSPDIF_ENABLE, lcr);
	cmi_partial_wr4(cmi, CMPCI_REG_FUNC_1, 0,
			CMPCI_REG_SPDIF0_ENABLE, fcr1);
}

/* ------------------------------------------------------------------------- */
/* Channel Interface implementation */

static void *
cmichan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct cmi_info  *cmi = devinfo;
	struct cmi_chinfo *ch = (dir == PCMDIR_PLAY) ? &cmi->pch : &cmi->rch;

	ch->parent  = cmi;
	ch->channel = c;
	ch->bps     = 1;
	ch->fmt     = AFMT_U8;
	ch->spd     = DSP_DEFAULT_SPEED;
	ch->dma_configured = 0;
	ch->buffer  = b;
	if (sndbuf_alloc(ch->buffer, cmi->parent_dmat, CMI_BUFFER_SIZE) != 0) {
		DEB(printf("cmichan_init failed\n"));
		return NULL;
	}

	ch->dir = dir;
	if (dir == PCMDIR_PLAY) {
		cmi_clr4(ch->parent, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_DIR);
	} else {
		cmi_set4(ch->parent, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_DIR);
	}

	return ch;
}

static int
cmichan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct cmi_chinfo *ch = data;
	u_int32_t f;

	if (format & AFMT_S16_LE) {
		f = CMPCI_REG_FORMAT_16BIT;
		ch->bps = 2;
	} else {
		f = CMPCI_REG_FORMAT_8BIT;
		ch->bps = 1;
	}

	if (format & AFMT_STEREO) {
		f |= CMPCI_REG_FORMAT_STEREO;
		ch->bps *= 2;
	} else {
		f |= CMPCI_REG_FORMAT_MONO;
	}

	if (ch->dir == PCMDIR_PLAY) {
		cmi_partial_wr4(ch->parent,
				CMPCI_REG_CHANNEL_FORMAT,
				CMPCI_REG_CH0_FORMAT_SHIFT,
				CMPCI_REG_CH0_FORMAT_MASK,
				f);
	} else {
		cmi_partial_wr4(ch->parent,
				CMPCI_REG_CHANNEL_FORMAT,
				CMPCI_REG_CH1_FORMAT_SHIFT,
				CMPCI_REG_CH1_FORMAT_MASK,
				f);
	}
	ch->fmt = format;
	ch->dma_configured = 0;

	return 0;
}

static int
cmichan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct cmi_chinfo *ch = data;
	u_int32_t r, rsp;

	r = cmpci_rate_to_regvalue(speed);
	if (ch->dir == PCMDIR_PLAY) {
		if (speed < 44100) /* disable if req before rate change */
			cmi_spdif_speed(ch->parent, speed);
		cmi_partial_wr4(ch->parent,
				CMPCI_REG_FUNC_1,
				CMPCI_REG_DAC_FS_SHIFT,
				CMPCI_REG_DAC_FS_MASK,
				r);
		if (speed >= 44100) /* enable if req after rate change */
			cmi_spdif_speed(ch->parent, speed);
		rsp = cmi_rd(ch->parent, CMPCI_REG_FUNC_1, 4);
		rsp >>= CMPCI_REG_DAC_FS_SHIFT;
		rsp &= 	CMPCI_REG_DAC_FS_MASK;
	} else {
		cmi_partial_wr4(ch->parent,
				CMPCI_REG_FUNC_1,
				CMPCI_REG_ADC_FS_SHIFT,
				CMPCI_REG_ADC_FS_MASK,
				r);
		rsp = cmi_rd(ch->parent, CMPCI_REG_FUNC_1, 4);
		rsp >>= CMPCI_REG_ADC_FS_SHIFT;
		rsp &= 	CMPCI_REG_ADC_FS_MASK;
	}
	ch->spd = cmpci_regvalue_to_rate(r);

	DEB(printf("cmichan_setspeed (%s) %d -> %d (%d)\n",
		   (ch->dir == PCMDIR_PLAY) ? "play" : "rec",
		   speed, ch->spd, cmpci_regvalue_to_rate(rsp)));

	return ch->spd;
}

static int
cmichan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	struct cmi_chinfo *ch = data;

	/* user has requested interrupts every blocksize bytes */
	if (blocksize > CMI_BUFFER_SIZE / CMI_INTR_PER_BUFFER) {
		blocksize = CMI_BUFFER_SIZE / CMI_INTR_PER_BUFFER;
	}
	sndbuf_resize(ch->buffer, CMI_INTR_PER_BUFFER, blocksize);

	ch->dma_configured  = 0;
	return sndbuf_getsize(ch->buffer);
}

static int
cmichan_trigger(kobj_t obj, void *data, int go)
{
	struct cmi_chinfo *ch = data;
	struct cmi_info  *cmi = ch->parent;

	if (ch->dir == PCMDIR_PLAY) {
		switch(go) {
		case PCMTRIG_START:
			cmi_dac_start(cmi, ch);
			break;
		case PCMTRIG_ABORT:
			cmi_dac_reset(cmi, ch);
			break;
		}
	} else {
		switch(go) {
		case PCMTRIG_START:
			cmi_adc_start(cmi, ch);
			break;
		case PCMTRIG_ABORT:
			cmi_adc_reset(cmi, ch);
			break;
		}
	}
	return 0;
}

static int
cmichan_getptr(kobj_t obj, void *data)
{
	struct cmi_chinfo *ch = data;
	struct cmi_info *cmi = ch->parent;
	u_int32_t physptr, bufptr, sz;

	if (ch->dir == PCMDIR_PLAY) {
		physptr = cmi_rd(cmi, CMPCI_REG_DMA0_BASE, 4);
	} else {
		physptr = cmi_rd(cmi, CMPCI_REG_DMA1_BASE, 4);
	}

	sz = sndbuf_getsize(ch->buffer);
	bufptr  = (physptr - ch->phys_buf + sz - ch->bps) % sz;

	return bufptr;
}

static void
cmi_intr(void *data)
{
	struct cmi_info *cmi = data;
	u_int32_t intrstat;

	intrstat = cmi_rd(cmi, CMPCI_REG_INTR_STATUS, 4);
	if ((intrstat & CMPCI_REG_ANY_INTR) == 0) {
		return;
	}

	/* Disable interrupts */
	if (intrstat & CMPCI_REG_CH0_INTR) {
		cmi_clr4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
	}

	if (intrstat & CMPCI_REG_CH1_INTR) {
		cmi_clr4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
	}

	DEB(printf("cmi_intr - play %d rec %d\n",
		   intrstat & CMPCI_REG_CH0_INTR,
		   (intrstat & CMPCI_REG_CH1_INTR)>>1));

	/* Signal interrupts to channel */
	if (intrstat & CMPCI_REG_CH0_INTR) {
		chn_intr(cmi->pch.channel);
	}

	if (intrstat & CMPCI_REG_CH1_INTR) {
		chn_intr(cmi->rch.channel);
	}

	/* Enable interrupts */
	if (intrstat & CMPCI_REG_CH0_INTR) {
		cmi_set4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
	}

	if (intrstat & CMPCI_REG_CH1_INTR) {
		cmi_set4(cmi, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
	}

	return;
}

static struct pcmchan_caps *
cmichan_getcaps(kobj_t obj, void *data)
{
	return &cmi_caps;
}

static kobj_method_t cmichan_methods[] = {
    	KOBJMETHOD(channel_init,		cmichan_init),
    	KOBJMETHOD(channel_setformat,		cmichan_setformat),
    	KOBJMETHOD(channel_setspeed,		cmichan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	cmichan_setblocksize),
    	KOBJMETHOD(channel_trigger,		cmichan_trigger),
    	KOBJMETHOD(channel_getptr,		cmichan_getptr),
    	KOBJMETHOD(channel_getcaps,		cmichan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(cmichan);

/* ------------------------------------------------------------------------- */
/* Mixer - sb16 with kinks */

static void
cmimix_wr(struct cmi_info *cmi, u_int8_t port, u_int8_t val)
{
	cmi_wr(cmi, CMPCI_REG_SBADDR, port, 1);
	cmi_wr(cmi, CMPCI_REG_SBDATA, val, 1);
}

static u_int8_t
cmimix_rd(struct cmi_info *cmi, u_int8_t port)
{
	cmi_wr(cmi, CMPCI_REG_SBADDR, port, 1);
	return (u_int8_t)cmi_rd(cmi, CMPCI_REG_SBDATA, 1);
}

struct sb16props {
	u_int8_t  rreg;     /* right reg chan register */
	u_int8_t  stereo:1; /* (no explanation needed, honest) */
	u_int8_t  rec:1;    /* recording source */
	u_int8_t  bits:3;   /* num bits to represent maximum gain rep */
	u_int8_t  oselect;  /* output select mask */
	u_int8_t  iselect;  /* right input select mask */
} static const cmt[SOUND_MIXER_NRDEVICES] = {
	[SOUND_MIXER_SYNTH]   = {CMPCI_SB16_MIXER_FM_R,      1, 1, 5,
				 CMPCI_SB16_SW_FM,   CMPCI_SB16_MIXER_FM_SRC_R},
	[SOUND_MIXER_CD]      = {CMPCI_SB16_MIXER_CDDA_R,    1, 1, 5,
				 CMPCI_SB16_SW_CD,   CMPCI_SB16_MIXER_CD_SRC_R},
	[SOUND_MIXER_LINE]    = {CMPCI_SB16_MIXER_LINE_R,    1, 1, 5,
				 CMPCI_SB16_SW_LINE, CMPCI_SB16_MIXER_LINE_SRC_R},
	[SOUND_MIXER_MIC]     = {CMPCI_SB16_MIXER_MIC,       0, 1, 5,
				 CMPCI_SB16_SW_MIC,  CMPCI_SB16_MIXER_MIC_SRC},
	[SOUND_MIXER_SPEAKER] = {CMPCI_SB16_MIXER_SPEAKER,  0, 0, 2, 0, 0},
	[SOUND_MIXER_PCM]     = {CMPCI_SB16_MIXER_VOICE_R,  1, 0, 5, 0, 0},
	[SOUND_MIXER_VOLUME]  = {CMPCI_SB16_MIXER_MASTER_R, 1, 0, 5, 0, 0},
	/* These controls are not implemented in CMI8738, but maybe at a
	   future date.  They are not documented in C-Media documentation,
	   though appear in other drivers for future h/w (ALSA, Linux, NetBSD).
	*/
	[SOUND_MIXER_IGAIN]   = {CMPCI_SB16_MIXER_INGAIN_R,  1, 0, 2, 0, 0},
	[SOUND_MIXER_OGAIN]   = {CMPCI_SB16_MIXER_OUTGAIN_R, 1, 0, 2, 0, 0},
	[SOUND_MIXER_BASS]    = {CMPCI_SB16_MIXER_BASS_R,    1, 0, 4, 0, 0},
	[SOUND_MIXER_TREBLE]  = {CMPCI_SB16_MIXER_TREBLE_R,  1, 0, 4, 0, 0},
	/* The mic pre-amp is implemented with non-SB16 compatible registers. */
	[SOUND_MIXER_MONITOR]  = {CMPCI_NON_SB16_CONTROL,     0, 1, 4, 0},
};

#define MIXER_GAIN_REG_RTOL(r) (r - 1)

static int
cmimix_init(struct snd_mixer *m)
{
	struct cmi_info *cmi = mix_getdevinfo(m);
	u_int32_t i,v;

	v = 0;
	for(i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (cmt[i].bits) v |= 1 << i;
	}
	mix_setdevs(m, v);
	v = 0;
	for(i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (cmt[i].rec)  v |= 1 << i;
	}
	mix_setrecdevs(m, v);

	cmimix_wr(cmi, CMPCI_SB16_MIXER_RESET, 0);
	cmimix_wr(cmi, CMPCI_SB16_MIXER_ADCMIX_L, 0);
	cmimix_wr(cmi, CMPCI_SB16_MIXER_ADCMIX_R, 0);
	cmimix_wr(cmi, CMPCI_SB16_MIXER_OUTMIX,
		  CMPCI_SB16_SW_CD | CMPCI_SB16_SW_MIC | CMPCI_SB16_SW_LINE);
	return 0;
}

static int
cmimix_set(struct snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	struct cmi_info *cmi = mix_getdevinfo(m);
	u_int32_t r, l, max;
	u_int8_t  v;

	max = (1 << cmt[dev].bits) - 1;

	if (cmt[dev].rreg == CMPCI_NON_SB16_CONTROL) {
		/* For time being this can only be one thing (mic in mic/aux reg) */
		u_int8_t v;
		v = cmi_rd(cmi, CMPCI_REG_AUX_MIC, 1) & 0xf0;
		l = left * max / 100;
		/* 3 bit gain with LSB MICGAIN off(1),on(1) -> 4 bit value*/
		v |= ((l << 1) | (~l >> 3)) & 0x0f;
		cmi_wr(cmi, CMPCI_REG_AUX_MIC, v, 1);
		return 0;
	}

	l  = (left * max / 100) << (8 - cmt[dev].bits);
	if (cmt[dev].stereo) {
		r = (right * max / 100) << (8 - cmt[dev].bits);
		cmimix_wr(cmi, MIXER_GAIN_REG_RTOL(cmt[dev].rreg), l);
		cmimix_wr(cmi, cmt[dev].rreg, r);
		DEBMIX(printf("Mixer stereo write dev %d reg 0x%02x "\
			      "value 0x%02x:0x%02x\n",
			      dev, MIXER_GAIN_REG_RTOL(cmt[dev].rreg), l, r));
	} else {
		r = l;
		cmimix_wr(cmi, cmt[dev].rreg, l);
		DEBMIX(printf("Mixer mono write dev %d reg 0x%02x " \
			      "value 0x%02x:0x%02x\n",
			      dev, cmt[dev].rreg, l, l));
	}

	/* Zero gain does not mute channel from output, but this does... */
	v = cmimix_rd(cmi, CMPCI_SB16_MIXER_OUTMIX);
	if (l == 0 && r == 0) {
		v &= ~cmt[dev].oselect;
	} else {
		v |= cmt[dev].oselect;
	}
	cmimix_wr(cmi,  CMPCI_SB16_MIXER_OUTMIX, v);

	return 0;
}

static int
cmimix_setrecsrc(struct snd_mixer *m, u_int32_t src)
{
	struct cmi_info *cmi = mix_getdevinfo(m);
	u_int32_t i, ml, sl;

	ml = sl = 0;
	for(i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if ((1<<i) & src) {
			if (cmt[i].stereo) {
				sl |= cmt[i].iselect;
			} else {
				ml |= cmt[i].iselect;
			}
		}
	}
	cmimix_wr(cmi, CMPCI_SB16_MIXER_ADCMIX_R, sl|ml);
	DEBMIX(printf("cmimix_setrecsrc: reg 0x%02x val 0x%02x\n",
		      CMPCI_SB16_MIXER_ADCMIX_R, sl|ml));
	ml = CMPCI_SB16_MIXER_SRC_R_TO_L(ml);
	cmimix_wr(cmi, CMPCI_SB16_MIXER_ADCMIX_L, sl|ml);
	DEBMIX(printf("cmimix_setrecsrc: reg 0x%02x val 0x%02x\n",
		      CMPCI_SB16_MIXER_ADCMIX_L, sl|ml));

	return src;
}

static kobj_method_t cmi_mixer_methods[] = {
	KOBJMETHOD(mixer_init,	cmimix_init),
	KOBJMETHOD(mixer_set,	cmimix_set),
	KOBJMETHOD(mixer_setrecsrc,	cmimix_setrecsrc),
	{ 0, 0 }
};
MIXER_DECLARE(cmi_mixer);

/* ------------------------------------------------------------------------- */
/* Power and reset */

static void
cmi_power(struct cmi_info *cmi, int state)
{
	switch (state) {
	case 0: /* full power */
		cmi_clr4(cmi, CMPCI_REG_MISC, CMPCI_REG_POWER_DOWN);
		break;
	default:
		/* power off */
		cmi_set4(cmi, CMPCI_REG_MISC, CMPCI_REG_POWER_DOWN);
		break;
	}
}

/* ------------------------------------------------------------------------- */
/* Bus and device registration */
static int
cmi_probe(device_t dev)
{
	switch(pci_get_devid(dev)) {
	case CMI8338A_PCI_ID:
		device_set_desc(dev, "CMedia CMI8338A");
		return 0;
	case CMI8338B_PCI_ID:
		device_set_desc(dev, "CMedia CMI8338B");
		return 0;
	case CMI8738_PCI_ID:
		device_set_desc(dev, "CMedia CMI8738");
		return 0;
	case CMI8738B_PCI_ID:
		device_set_desc(dev, "CMedia CMI8738B");
		return 0;
	default:
		return ENXIO;
	}
}

static int
cmi_attach(device_t dev)
{
	struct snddev_info *d;
	struct cmi_info *cmi;
	u_int32_t data;
	char status[SND_STATUSLEN];

	d = device_get_softc(dev);

	if ((cmi = malloc(sizeof(struct cmi_info), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	bzero(cmi, sizeof(*cmi));

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	cmi->regid = PCIR_MAPS;
	cmi->reg = bus_alloc_resource(dev, SYS_RES_IOPORT, &cmi->regid,
				      0, BUS_SPACE_UNRESTRICTED, 1, RF_ACTIVE);
	if (!cmi->reg) {
		device_printf(dev, "cmi_attach: Cannot allocate bus resource\n");
		goto bad;
	}
	cmi->st = rman_get_bustag(cmi->reg);
	cmi->sh = rman_get_bushandle(cmi->reg);

	cmi->irqid = 0;
	cmi->irq   = bus_alloc_resource(dev, SYS_RES_IRQ, &cmi->irqid,
					0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!cmi->irq ||
	    snd_setup_intr(dev, cmi->irq, 0, cmi_intr, cmi, &cmi->ih)) {
		device_printf(dev, "cmi_attach: Unable to map interrupt\n");
		goto bad;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/CMI_BUFFER_SIZE, /*nsegments*/1,
			       /*maxsegz*/0x3ffff, /*flags*/0,
			       &cmi->parent_dmat) != 0) {
		device_printf(dev, "cmi_attach: Unable to create dma tag\n");
		goto bad;
	}

	cmi_power(cmi, 0);
	/* Disable interrupts and channels */
	cmi_clr4(cmi, CMPCI_REG_INTR_CTRL,
		 CMPCI_REG_CH0_INTR_ENABLE |
		 CMPCI_REG_CH1_INTR_ENABLE |
		 CMPCI_REG_TDMA_INTR_ENABLE);
	cmi_clr4(cmi, CMPCI_REG_FUNC_0,
		 CMPCI_REG_CH0_ENABLE | CMPCI_REG_CH1_ENABLE);

	if (mixer_init(dev, &cmi_mixer_class, cmi))
		goto bad;

	if (pcm_register(dev, cmi, 1, 1))
		goto bad;

	pcm_addchan(dev, PCMDIR_PLAY, &cmichan_class, cmi);
	pcm_addchan(dev, PCMDIR_REC, &cmichan_class, cmi);

	snprintf(status, SND_STATUSLEN, "at io 0x%lx irq %ld",
		 rman_get_start(cmi->reg), rman_get_start(cmi->irq));
	pcm_setstatus(dev, status);

	DEB(printf("cmi_attach: succeeded\n"));
	return 0;

 bad:
	if (cmi->parent_dmat) bus_dma_tag_destroy(cmi->parent_dmat);
	if (cmi->ih) bus_teardown_intr(dev, cmi->irq, cmi->ih);
	if (cmi->irq) bus_release_resource(dev, SYS_RES_IRQ, cmi->irqid, cmi->irq);
	if (cmi->reg) bus_release_resource(dev, SYS_RES_IOPORT,
					   cmi->regid, cmi->reg);
	if (cmi) free(cmi, M_DEVBUF);

	return ENXIO;
}

static int
cmi_detach(device_t dev)
{
	struct cmi_info *cmi;
	int r;

	r = pcm_unregister(dev);
	if (r) return r;

	cmi = pcm_getdevinfo(dev);
	cmi_power(cmi, 3);
	bus_dma_tag_destroy(cmi->parent_dmat);
	bus_teardown_intr(dev, cmi->irq, cmi->ih);
	bus_release_resource(dev, SYS_RES_IRQ, cmi->irqid, cmi->irq);
	bus_release_resource(dev, SYS_RES_IOPORT, cmi->regid, cmi->reg);
	free(cmi, M_DEVBUF);

	return 0;
}

static device_method_t cmi_methods[] = {
	DEVMETHOD(device_probe,         cmi_probe),
	DEVMETHOD(device_attach,        cmi_attach),
	DEVMETHOD(device_detach,        cmi_detach),
	DEVMETHOD(device_resume,        bus_generic_resume),
	DEVMETHOD(device_suspend,       bus_generic_suspend),
	{ 0, 0 }
};

static driver_t cmi_driver = {
	"pcm",
	cmi_methods,
	sizeof(struct snddev_info)
};

static devclass_t pcm_devclass;
DRIVER_MODULE(snd_cmipci, pci, cmi_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_cmipci, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_cmipci, 1);
