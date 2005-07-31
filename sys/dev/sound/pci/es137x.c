/*-
 * Support the ENSONIQ AudioPCI board and Creative Labs SoundBlaster PCI
 * boards based on the ES1370, ES1371 and ES1373 chips.
 *
 * Copyright (c) 1999 Russell Cattelan <cattelan@thebarn.com>
 * Copyright (c) 1999 Cameron Grant <cg@freebsd.org>
 * Copyright (c) 1998 by Joachim Kuebart. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *	This product includes software developed by Joachim Kuebart.
 *
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Part of this code was heavily inspired by the linux driver from
 * Thomas Sailer (sailer@ife.ee.ethz.ch)
 * Just about everything has been touched and reworked in some way but
 * the all the underlying sequences/timing/register values are from
 * Thomas' code.
 *
*/

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <dev/sound/pci/es137x.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sys/sysctl.h>

#include "mixer_if.h"

SND_DECLARE_FILE("$FreeBSD$");

#define MEM_MAP_REG 0x14

/* PCI IDs of supported chips */
#define ES1370_PCI_ID 0x50001274
#define ES1371_PCI_ID 0x13711274
#define ES1371_PCI_ID2 0x13713274
#define CT5880_PCI_ID 0x58801274
#define CT4730_PCI_ID 0x89381102

#define ES1371REV_ES1371_A  0x02
#define ES1371REV_ES1371_B  0x09

#define ES1371REV_ES1373_8  0x08
#define ES1371REV_ES1373_A  0x04
#define ES1371REV_ES1373_B  0x06

#define ES1371REV_CT5880_A  0x07

#define CT5880REV_CT5880_C  0x02
#define CT5880REV_CT5880_D  0x03
#define CT5880REV_CT5880_E  0x04

#define CT4730REV_CT4730_A  0x00

#define ES_DEFAULT_BUFSZ 4096

/* device private data */
struct es_info;

struct es_chinfo {
	struct es_info *parent;
	struct pcm_channel *channel;
	struct snd_dbuf *buffer;
	int dir, num;
	u_int32_t fmt, blksz, bufsz;
};

struct es_info {
	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t	parent_dmat;

	struct resource *reg, *irq;
	int regtype, regid, irqid;
	void *ih;

	device_t dev;
	int num;
	int spdif_en;
	unsigned int bufsz;

	/* Contents of board's registers */
	u_long		ctrl;
	u_long		sctrl;
	struct es_chinfo pch, rch;
	struct mtx	*lock;
};

/* -------------------------------------------------------------------- */

/* prototypes */
static void     es_intr(void *);

static u_int	es1371_wait_src_ready(struct es_info *);
static void	es1371_src_write(struct es_info *, u_short, unsigned short);
static u_int	es1371_adc_rate(struct es_info *, u_int, int);
static u_int	es1371_dac_rate(struct es_info *, u_int, int);
static int	es1371_init(struct es_info *, device_t);
static int      es1370_init(struct es_info *);
static int      es1370_wrcodec(struct es_info *, u_char, u_char);

static u_int32_t es_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps es_playcaps = {4000, 48000, es_playfmt, 0};

static u_int32_t es_recfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps es_reccaps = {4000, 48000, es_recfmt, 0};

static const struct {
	unsigned        volidx:4;
	unsigned        left:4;
	unsigned        right:4;
	unsigned        stereo:1;
	unsigned        recmask:13;
	unsigned        avail:1;
}       mixtable[SOUND_MIXER_NRDEVICES] = {
	[SOUND_MIXER_VOLUME]	= { 0, 0x0, 0x1, 1, 0x0000, 1 },
	[SOUND_MIXER_PCM] 	= { 1, 0x2, 0x3, 1, 0x0400, 1 },
	[SOUND_MIXER_SYNTH]	= { 2, 0x4, 0x5, 1, 0x0060, 1 },
	[SOUND_MIXER_CD]	= { 3, 0x6, 0x7, 1, 0x0006, 1 },
	[SOUND_MIXER_LINE]	= { 4, 0x8, 0x9, 1, 0x0018, 1 },
	[SOUND_MIXER_LINE1]	= { 5, 0xa, 0xb, 1, 0x1800, 1 },
	[SOUND_MIXER_LINE2]	= { 6, 0xc, 0x0, 0, 0x0100, 1 },
	[SOUND_MIXER_LINE3]	= { 7, 0xd, 0x0, 0, 0x0200, 1 },
	[SOUND_MIXER_MIC]	= { 8, 0xe, 0x0, 0, 0x0001, 1 },
	[SOUND_MIXER_OGAIN]	= { 9, 0xf, 0x0, 0, 0x0000, 1 }
};

static u_int32_t
es_rd(struct es_info *es, int regno, int size)
{
	switch (size) {
	case 1:
		return bus_space_read_1(es->st, es->sh, regno);
	case 2:
		return bus_space_read_2(es->st, es->sh, regno);
	case 4:
		return bus_space_read_4(es->st, es->sh, regno);
	default:
		return 0xFFFFFFFF;
	}
}

static void
es_wr(struct es_info *es, int regno, u_int32_t data, int size)
{

	switch (size) {
	case 1:
		bus_space_write_1(es->st, es->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(es->st, es->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(es->st, es->sh, regno, data);
		break;
	}
}

/* -------------------------------------------------------------------- */
/* The es1370 mixer interface */

static int
es1370_mixinit(struct snd_mixer *m)
{
	int i;
	u_int32_t v;

	v = 0;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if (mixtable[i].avail) v |= (1 << i);
	mix_setdevs(m, v);
	v = 0;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if (mixtable[i].recmask) v |= (1 << i);
	mix_setrecdevs(m, v);
	return 0;
}

static int
es1370_mixset(struct snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	int l, r, rl, rr;

	if (!mixtable[dev].avail) return -1;
	l = left;
	r = mixtable[dev].stereo? right : l;
	if (mixtable[dev].left == 0xf) {
		rl = (l < 2)? 0x80 : 7 - (l - 2) / 14;
	} else {
		rl = (l < 10)? 0x80 : 15 - (l - 10) / 6;
	}
	if (mixtable[dev].stereo) {
		rr = (r < 10)? 0x80 : 15 - (r - 10) / 6;
		es1370_wrcodec(mix_getdevinfo(m), mixtable[dev].right, rr);
	}
	es1370_wrcodec(mix_getdevinfo(m), mixtable[dev].left, rl);
	return l | (r << 8);
}

static int
es1370_mixsetrecsrc(struct snd_mixer *m, u_int32_t src)
{
	int i, j = 0;

	if (src == 0) src = 1 << SOUND_MIXER_MIC;
	src &= mix_getrecdevs(m);
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((src & (1 << i)) != 0) j |= mixtable[i].recmask;

	es1370_wrcodec(mix_getdevinfo(m), CODEC_LIMIX1, j & 0x55);
	es1370_wrcodec(mix_getdevinfo(m), CODEC_RIMIX1, j & 0xaa);
	es1370_wrcodec(mix_getdevinfo(m), CODEC_LIMIX2, (j >> 8) & 0x17);
	es1370_wrcodec(mix_getdevinfo(m), CODEC_RIMIX2, (j >> 8) & 0x0f);
	es1370_wrcodec(mix_getdevinfo(m), CODEC_OMIX1, 0x7f);
	es1370_wrcodec(mix_getdevinfo(m), CODEC_OMIX2, 0x3f);
	return src;
}

static kobj_method_t es1370_mixer_methods[] = {
    	KOBJMETHOD(mixer_init,		es1370_mixinit),
    	KOBJMETHOD(mixer_set,		es1370_mixset),
    	KOBJMETHOD(mixer_setrecsrc,	es1370_mixsetrecsrc),
	{ 0, 0 }
};
MIXER_DECLARE(es1370_mixer);

/* -------------------------------------------------------------------- */

static int
es1370_wrcodec(struct es_info *es, u_char i, u_char data)
{
	u_int t;

	for (t = 0; t < 0x1000; t++) {
		if ((es_rd(es, ES1370_REG_STATUS, 4) &
		      STAT_CSTAT) == 0) {
			es_wr(es, ES1370_REG_CODEC,
				((u_short)i << CODEC_INDEX_SHIFT) | data, 2);
			return 0;
		}
		DELAY(1);
	}
	printf("pcm: es1370_wrcodec timed out\n");
	return -1;
}

/* -------------------------------------------------------------------- */

/* channel interface */
static void *
eschan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct es_info *es = devinfo;
	struct es_chinfo *ch = (dir == PCMDIR_PLAY)? &es->pch : &es->rch;

	snd_mtxlock(es->lock);
	ch->parent = es;
	ch->channel = c;
	ch->buffer = b;
	ch->bufsz = es->bufsz;
	ch->blksz = ch->bufsz / 2;
	ch->num = ch->parent->num++;
	snd_mtxunlock(es->lock);
	if (sndbuf_alloc(ch->buffer, es->parent_dmat, ch->bufsz) != 0)
		return NULL;
	return ch;
}

static int
eschan_setdir(kobj_t obj, void *data, int dir)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;

	if (dir == PCMDIR_PLAY) {
		es_wr(es, ES1370_REG_MEMPAGE, ES1370_REG_DAC2_FRAMEADR >> 8, 1);
		es_wr(es, ES1370_REG_DAC2_FRAMEADR & 0xff, sndbuf_getbufaddr(ch->buffer), 4);
		es_wr(es, ES1370_REG_DAC2_FRAMECNT & 0xff, (ch->bufsz >> 2) - 1, 4);
	} else {
		es_wr(es, ES1370_REG_MEMPAGE, ES1370_REG_ADC_FRAMEADR >> 8, 1);
		es_wr(es, ES1370_REG_ADC_FRAMEADR & 0xff, sndbuf_getbufaddr(ch->buffer), 4);
		es_wr(es, ES1370_REG_ADC_FRAMECNT & 0xff, (ch->bufsz >> 2) - 1, 4);
	}
	ch->dir = dir;
	return 0;
}

static int
eschan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;

	if (ch->dir == PCMDIR_PLAY) {
		es->sctrl &= ~SCTRL_P2FMT;
		if (format & AFMT_S16_LE) es->sctrl |= SCTRL_P2SEB;
		if (format & AFMT_STEREO) es->sctrl |= SCTRL_P2SMB;
	} else {
		es->sctrl &= ~SCTRL_R1FMT;
		if (format & AFMT_S16_LE) es->sctrl |= SCTRL_R1SEB;
		if (format & AFMT_STEREO) es->sctrl |= SCTRL_R1SMB;
	}
	es_wr(es, ES1370_REG_SERIAL_CONTROL, es->sctrl, 4);
	ch->fmt = format;
	return 0;
}

static int
eschan1370_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;

	es->ctrl &= ~CTRL_PCLKDIV;
	es->ctrl |= DAC2_SRTODIV(speed) << CTRL_SH_PCLKDIV;
	es_wr(es, ES1370_REG_CONTROL, es->ctrl, 4);
	/* rec/play speeds locked together - should indicate in flags */
	return speed; /* XXX calc real speed */
}

static int
eschan1371_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
  	struct es_chinfo *ch = data;
  	struct es_info *es = ch->parent;
	int i, delta;

	if (ch->dir == PCMDIR_PLAY)
  		i = es1371_dac_rate(es, speed, 3 - ch->num); /* play */
	else
  		i = es1371_adc_rate(es, speed, 1); /* record */
	delta = (speed > i) ? speed - i : i - speed;
	if (delta < 2)
		return speed;
	return i;
}

static int
eschan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
  	struct es_chinfo *ch = data;

	ch->blksz = blocksize;
	ch->bufsz = ch->blksz * 2;
	sndbuf_resize(ch->buffer, 2, ch->blksz);
	return ch->blksz;
}

static int
eschan_trigger(kobj_t obj, void *data, int go)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;
	unsigned cnt;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	cnt = (ch->blksz / sndbuf_getbps(ch->buffer)) - 1;

	if (ch->dir == PCMDIR_PLAY) {
		if (go == PCMTRIG_START) {
			int b = (ch->fmt & AFMT_S16_LE)? 2 : 1;
			es->ctrl |= CTRL_DAC2_EN;
			es->sctrl &= ~(SCTRL_P2ENDINC | SCTRL_P2STINC | SCTRL_P2LOOPSEL | SCTRL_P2PAUSE | SCTRL_P2DACSEN);
			es->sctrl |= SCTRL_P2INTEN | (b << SCTRL_SH_P2ENDINC);
			es_wr(es, ES1370_REG_DAC2_SCOUNT, cnt, 4);
			/* start at beginning of buffer */
			es_wr(es, ES1370_REG_MEMPAGE, ES1370_REG_DAC2_FRAMECNT >> 8, 4);
			es_wr(es, ES1370_REG_DAC2_FRAMECNT & 0xff, (ch->bufsz >> 2) - 1, 4);
		} else es->ctrl &= ~CTRL_DAC2_EN;
	} else {
		if (go == PCMTRIG_START) {
			es->ctrl |= CTRL_ADC_EN;
			es->sctrl &= ~SCTRL_R1LOOPSEL;
			es->sctrl |= SCTRL_R1INTEN;
			es_wr(es, ES1370_REG_ADC_SCOUNT, cnt, 4);
			/* start at beginning of buffer */
			es_wr(es, ES1370_REG_MEMPAGE, ES1370_REG_ADC_FRAMECNT >> 8, 4);
			es_wr(es, ES1370_REG_ADC_FRAMECNT & 0xff, (ch->bufsz >> 2) - 1, 4);
		} else es->ctrl &= ~CTRL_ADC_EN;
	}
	es_wr(es, ES1370_REG_SERIAL_CONTROL, es->sctrl, 4);
	es_wr(es, ES1370_REG_CONTROL, es->ctrl, 4);
	return 0;
}

static int
eschan_getptr(kobj_t obj, void *data)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;
	u_int32_t reg, cnt;

	if (ch->dir == PCMDIR_PLAY)
		reg = ES1370_REG_DAC2_FRAMECNT;
	else
		reg = ES1370_REG_ADC_FRAMECNT;
	es_wr(es, ES1370_REG_MEMPAGE, reg >> 8, 4);
	cnt = es_rd(es, reg & 0x000000ff, 4) >> 16;
	/* cnt is longwords */
	return cnt << 2;
}

static struct pcmchan_caps *
eschan_getcaps(kobj_t obj, void *data)
{
	struct es_chinfo *ch = data;
	return (ch->dir == PCMDIR_PLAY)? &es_playcaps : &es_reccaps;
}

static kobj_method_t eschan1370_methods[] = {
    	KOBJMETHOD(channel_init,		eschan_init),
    	KOBJMETHOD(channel_setdir,		eschan_setdir),
    	KOBJMETHOD(channel_setformat,		eschan_setformat),
    	KOBJMETHOD(channel_setspeed,		eschan1370_setspeed),
    	KOBJMETHOD(channel_setblocksize,	eschan_setblocksize),
    	KOBJMETHOD(channel_trigger,		eschan_trigger),
    	KOBJMETHOD(channel_getptr,		eschan_getptr),
    	KOBJMETHOD(channel_getcaps,		eschan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(eschan1370);

static kobj_method_t eschan1371_methods[] = {
    	KOBJMETHOD(channel_init,		eschan_init),
    	KOBJMETHOD(channel_setdir,		eschan_setdir),
    	KOBJMETHOD(channel_setformat,		eschan_setformat),
    	KOBJMETHOD(channel_setspeed,		eschan1371_setspeed),
    	KOBJMETHOD(channel_setblocksize,	eschan_setblocksize),
    	KOBJMETHOD(channel_trigger,		eschan_trigger),
    	KOBJMETHOD(channel_getptr,		eschan_getptr),
    	KOBJMETHOD(channel_getcaps,		eschan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(eschan1371);

/* -------------------------------------------------------------------- */
/* The interrupt handler */
static void
es_intr(void *p)
{
	struct es_info *es = p;
	unsigned	intsrc, sctrl;

	snd_mtxlock(es->lock);
	intsrc = es_rd(es, ES1370_REG_STATUS, 4);
	if ((intsrc & STAT_INTR) == 0) {
		snd_mtxunlock(es->lock);
		return;
	}

	sctrl = es->sctrl;
	if (intsrc & STAT_ADC)  sctrl &= ~SCTRL_R1INTEN;
	if (intsrc & STAT_DAC1)	sctrl &= ~SCTRL_P1INTEN;
	if (intsrc & STAT_DAC2)	sctrl &= ~SCTRL_P2INTEN;

	es_wr(es, ES1370_REG_SERIAL_CONTROL, sctrl, 4);
	es_wr(es, ES1370_REG_SERIAL_CONTROL, es->sctrl, 4);
	snd_mtxunlock(es->lock);

	if (intsrc & STAT_ADC) chn_intr(es->rch.channel);
	if (intsrc & STAT_DAC1)
		;	/* nothing */
	if (intsrc & STAT_DAC2)	chn_intr(es->pch.channel);
}

/* ES1370 specific */
static int
es1370_init(struct es_info *es)
{
	es->ctrl = CTRL_CDC_EN | CTRL_SERR_DIS |
		(DAC2_SRTODIV(DSP_DEFAULT_SPEED) << CTRL_SH_PCLKDIV);
	es_wr(es, ES1370_REG_CONTROL, es->ctrl, 4);

	es->sctrl = 0;
	es_wr(es, ES1370_REG_SERIAL_CONTROL, es->sctrl, 4);

	es1370_wrcodec(es, CODEC_RES_PD, 3);/* No RST, PD */
	es1370_wrcodec(es, CODEC_CSEL, 0);	/* CODEC ADC and CODEC DAC use
					         * {LR,B}CLK2 and run off the LRCLK2
					         * PLL; program DAC_SYNC=0!  */
	es1370_wrcodec(es, CODEC_ADSEL, 0);/* Recording source is mixer */
	es1370_wrcodec(es, CODEC_MGAIN, 0);/* MIC amp is 0db */

	return 0;
}

/* ES1371 specific */
int
es1371_init(struct es_info *es, device_t dev)
{
	u_long cssr;
	int idx;
	int devid = pci_get_devid(dev);
	int revid = pci_get_revid(dev);

	es->num = 0;
	es->ctrl = 0;
	es->sctrl = 0;
	cssr = 0;
	if (devid == CT4730_PCI_ID) {
		/* XXX amplifier hack? */
		es->ctrl |= (1 << 16);
	}
	/* initialize the chips */
	es_wr(es, ES1370_REG_CONTROL, es->ctrl, 4);
	es_wr(es, ES1370_REG_SERIAL_CONTROL, es->sctrl, 4);
	es_wr(es, ES1371_REG_LEGACY, 0, 4);
	if ((devid == ES1371_PCI_ID && revid == ES1371REV_ES1373_8) ||
	    (devid == ES1371_PCI_ID && revid == ES1371REV_CT5880_A) ||
	    (devid == CT5880_PCI_ID && revid == CT5880REV_CT5880_C) ||
	    (devid == CT5880_PCI_ID && revid == CT5880REV_CT5880_D) ||
	    (devid == CT5880_PCI_ID && revid == CT5880REV_CT5880_E)) {
		cssr = 1 << 29;
		es_wr(es, ES1370_REG_STATUS, cssr, 4);
		DELAY(20000);
	}
	/* AC'97 warm reset to start the bitclk */
	es_wr(es, ES1370_REG_CONTROL, es->ctrl | ES1371_SYNC_RES, 4);
	DELAY(2000);
	es_wr(es, ES1370_REG_CONTROL, es->ctrl, 4);
	es1371_wait_src_ready(es);
	/* Init the sample rate converter */
	es_wr(es, ES1371_REG_SMPRATE, ES1371_DIS_SRC, 4);
	for (idx = 0; idx < 0x80; idx++)
		es1371_src_write(es, idx, 0);
	es1371_src_write(es, ES_SMPREG_DAC1 + ES_SMPREG_TRUNC_N,  16 << 4);
	es1371_src_write(es, ES_SMPREG_DAC1 + ES_SMPREG_INT_REGS, 16 << 10);
	es1371_src_write(es, ES_SMPREG_DAC2 + ES_SMPREG_TRUNC_N,  16 << 4);
	es1371_src_write(es, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS, 16 << 10);
	es1371_src_write(es, ES_SMPREG_VOL_ADC,                   1 << 12);
	es1371_src_write(es, ES_SMPREG_VOL_ADC  + 1,              1 << 12);
	es1371_src_write(es, ES_SMPREG_VOL_DAC1,                  1 << 12);
	es1371_src_write(es, ES_SMPREG_VOL_DAC1 + 1,              1 << 12);
	es1371_src_write(es, ES_SMPREG_VOL_DAC2,                  1 << 12);
	es1371_src_write(es, ES_SMPREG_VOL_DAC2 + 1,              1 << 12);
	es1371_adc_rate (es, 22050,                               1);
	es1371_dac_rate (es, 22050,                               1);
	es1371_dac_rate (es, 22050,                               2);
	/* WARNING:
	 * enabling the sample rate converter without properly programming
	 * its parameters causes the chip to lock up (the SRC busy bit will
	 * be stuck high, and I've found no way to rectify this other than
	 * power cycle)
	 */
	es1371_wait_src_ready(es);
	es_wr(es, ES1371_REG_SMPRATE, 0, 4);
	/* try to reset codec directly */
	es_wr(es, ES1371_REG_CODEC, 0, 4);
	es_wr(es, ES1370_REG_STATUS, cssr, 4);

	return (0);
}

/* -------------------------------------------------------------------- */

static int
es1371_wrcd(kobj_t obj, void *s, int addr, u_int32_t data)
{
    	unsigned t, x, orig;
	struct es_info *es = (struct es_info*)s;

	for (t = 0; t < 0x1000; t++)
	  	if (!es_rd(es, ES1371_REG_CODEC & CODEC_WIP, 4))
			break;
	/* save the current state for later */
 	x = orig = es_rd(es, ES1371_REG_SMPRATE, 4);
	/* enable SRC state data in SRC mux */
	es_wr(es, ES1371_REG_SMPRATE,
	  	(x &
	   	(ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1)) |
		0x00010000, 4);
	/* busy wait */
	for (t = 0; t < 0x1000; t++)
	  	if ((es_rd(es, ES1371_REG_SMPRATE, 4) & 0x00870000) == 0x00000000)
			break;
	/* wait for a SAFE time to write addr/data and then do it, dammit */
	for (t = 0; t < 0x1000; t++)
	  	if ((es_rd(es, ES1371_REG_SMPRATE, 4) & 0x00870000) == 0x00010000)
			break;

	es_wr(es, ES1371_REG_CODEC,
			  ((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) |
			  ((data << CODEC_PODAT_SHIFT) & CODEC_PODAT_MASK), 4);
	/* restore SRC reg */
	es1371_wait_src_ready(s);
	es_wr(es, ES1371_REG_SMPRATE, orig, 4);

	return 0;
}

static int
es1371_rdcd(kobj_t obj, void *s, int addr)
{
  	unsigned t, x = 0, orig;
  	struct es_info *es = (struct es_info *)s;

  	for (t = 0; t < 0x1000; t++)
		if (!(x = es_rd(es, ES1371_REG_CODEC, 4) & CODEC_WIP))
	  		break;

  	/* save the current state for later */
  	x = orig = es_rd(es, ES1371_REG_SMPRATE, 4);
  	/* enable SRC state data in SRC mux */
  	es_wr(es, ES1371_REG_SMPRATE,
			  (x &
			  (ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1)) |
			  0x00010000, 4);
	/* busy wait */
  	for (t = 0; t < 0x1000; t++)
		if ((x = es_rd(es, ES1371_REG_SMPRATE, 4) & 0x00870000) == 0x00000000)
	  		break;
  	/* wait for a SAFE time to write addr/data and then do it, dammit */
  	for (t = 0; t < 0x1000; t++)
		if ((x = es_rd(es, ES1371_REG_SMPRATE, 4) & 0x00870000) == 0x00010000)
	  		break;

  	es_wr(es, ES1371_REG_CODEC,
			  ((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) |
			  CODEC_PORD, 4);

  	/* restore SRC reg */
  	es1371_wait_src_ready(s);
  	es_wr(es, ES1371_REG_SMPRATE, orig, 4);

  	/* now wait for the stinkin' data (RDY) */
  	for (t = 0; t < 0x1000; t++)
		if ((x = es_rd(es, ES1371_REG_CODEC, 4)) & CODEC_RDY)
	  		break;

  	return ((x & CODEC_PIDAT_MASK) >> CODEC_PIDAT_SHIFT);
}

static kobj_method_t es1371_ac97_methods[] = {
    	KOBJMETHOD(ac97_read,		es1371_rdcd),
    	KOBJMETHOD(ac97_write,		es1371_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(es1371_ac97);

/* -------------------------------------------------------------------- */

static u_int
es1371_src_read(struct es_info *es, u_short reg)
{
  	unsigned int r;

  	r = es1371_wait_src_ready(es) &
		(ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1);
  	r |= ES1371_SRC_RAM_ADDRO(reg);
  	es_wr(es, ES1371_REG_SMPRATE, r, 4);
  	return ES1371_SRC_RAM_DATAI(es1371_wait_src_ready(es));
}

static void
es1371_src_write(struct es_info *es, u_short reg, u_short data)
{
	u_int r;

	r = es1371_wait_src_ready(es) &
		(ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1);
	r |= ES1371_SRC_RAM_ADDRO(reg) |  ES1371_SRC_RAM_DATAO(data);
	es_wr(es, ES1371_REG_SMPRATE, r | ES1371_SRC_RAM_WE, 4);
}

static u_int
es1371_adc_rate(struct es_info *es, u_int rate, int set)
{
  	u_int n, truncm, freq, result;

  	if (rate > 48000) rate = 48000;
  	if (rate < 4000) rate = 4000;
  	n = rate / 3000;
  	if ((1 << n) & ((1 << 15) | (1 << 13) | (1 << 11) | (1 << 9)))
		n--;
  	truncm = (21 * n - 1) | 1;
  	freq = ((48000UL << 15) / rate) * n;
  	result = (48000UL << 15) / (freq / n);
  	if (set) {
		if (rate >= 24000) {
	  		if (truncm > 239) truncm = 239;
	  		es1371_src_write(es, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,
				(((239 - truncm) >> 1) << 9) | (n << 4));
		} else {
	  		if (truncm > 119) truncm = 119;
	  		es1371_src_write(es, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,
				0x8000 | (((119 - truncm) >> 1) << 9) | (n << 4));
		}
		es1371_src_write(es, ES_SMPREG_ADC + ES_SMPREG_INT_REGS,
		 	(es1371_src_read(es, ES_SMPREG_ADC + ES_SMPREG_INT_REGS) &
		  	0x00ff) | ((freq >> 5) & 0xfc00));
		es1371_src_write(es, ES_SMPREG_ADC + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
		es1371_src_write(es, ES_SMPREG_VOL_ADC, n << 8);
		es1371_src_write(es, ES_SMPREG_VOL_ADC + 1, n << 8);
	}
	return result;
}

static u_int
es1371_dac_rate(struct es_info *es, u_int rate, int set)
{
  	u_int freq, r, result, dac, dis;

  	if (rate > 48000) rate = 48000;
  	if (rate < 4000) rate = 4000;
  	freq = (rate << 15) / 3000;
  	result = (freq * 3000) >> 15;
  	if (set) {
		dac = (set == 1)? ES_SMPREG_DAC1 : ES_SMPREG_DAC2;
		dis = (set == 1)? ES1371_DIS_P2 : ES1371_DIS_P1;

		r = (es1371_wait_src_ready(es) & (ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1));
		es_wr(es, ES1371_REG_SMPRATE, r, 4);
		es1371_src_write(es, dac + ES_SMPREG_INT_REGS,
			 	(es1371_src_read(es, dac + ES_SMPREG_INT_REGS) & 0x00ff) | ((freq >> 5) & 0xfc00));
		es1371_src_write(es, dac + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
		r = (es1371_wait_src_ready(es) & (ES1371_DIS_SRC | dis | ES1371_DIS_R1));
		es_wr(es, ES1371_REG_SMPRATE, r, 4);
  	}
  	return result;
}

static u_int
es1371_wait_src_ready(struct es_info *es)
{
  	u_int t, r;

  	for (t = 0; t < 0x1000; t++) {
		if (!((r = es_rd(es, ES1371_REG_SMPRATE, 4)) & ES1371_SRC_RAM_BUSY))
	  		return r;
		DELAY(1);
  	}
  	printf("es1371: wait src ready timeout 0x%x [0x%x]\n", ES1371_REG_SMPRATE, r);
  	return 0;
}

/* -------------------------------------------------------------------- */

/*
 * Probe and attach the card
 */

static int
es_pci_probe(device_t dev)
{
	switch(pci_get_devid(dev)) {
	case ES1370_PCI_ID:
		device_set_desc(dev, "AudioPCI ES1370");
		return BUS_PROBE_DEFAULT;

	case ES1371_PCI_ID:
		switch(pci_get_revid(dev)) {
		case ES1371REV_ES1371_A:
			device_set_desc(dev, "AudioPCI ES1371-A");
			return BUS_PROBE_DEFAULT;

		case ES1371REV_ES1371_B:
			device_set_desc(dev, "AudioPCI ES1371-B");
			return BUS_PROBE_DEFAULT;

		case ES1371REV_ES1373_A:
			device_set_desc(dev, "AudioPCI ES1373-A");
			return BUS_PROBE_DEFAULT;

		case ES1371REV_ES1373_B:
			device_set_desc(dev, "AudioPCI ES1373-B");
			return BUS_PROBE_DEFAULT;

		case ES1371REV_ES1373_8:
			device_set_desc(dev, "AudioPCI ES1373-8");
			return BUS_PROBE_DEFAULT;

		case ES1371REV_CT5880_A:
			device_set_desc(dev, "Creative CT5880-A");
			return BUS_PROBE_DEFAULT;

		default:
			device_set_desc(dev, "AudioPCI ES1371-?");
			device_printf(dev, "unknown revision %d -- please report to cg@freebsd.org\n", pci_get_revid(dev));
			return BUS_PROBE_DEFAULT;
		}

	case ES1371_PCI_ID2:
		device_set_desc(dev, "Strange AudioPCI ES1371-? (vid=3274)");
		device_printf(dev, "unknown revision %d -- please report to cg@freebsd.org\n", pci_get_revid(dev));
		return BUS_PROBE_DEFAULT;

	case CT4730_PCI_ID:
		switch(pci_get_revid(dev)) {
		case CT4730REV_CT4730_A:
			device_set_desc(dev, "Creative SB AudioPCI CT4730");
			return BUS_PROBE_DEFAULT;
		default:
			device_set_desc(dev, "Creative SB AudioPCI CT4730-?");
			device_printf(dev, "unknown revision %d -- please report to cg@freebsd.org\n", pci_get_revid(dev));
			return BUS_PROBE_DEFAULT;
		}

	case CT5880_PCI_ID:
		switch(pci_get_revid(dev)) {
		case CT5880REV_CT5880_C:
			device_set_desc(dev, "Creative CT5880-C");
			return BUS_PROBE_DEFAULT;

		case CT5880REV_CT5880_D:
			device_set_desc(dev, "Creative CT5880-D");
			return BUS_PROBE_DEFAULT;

		case CT5880REV_CT5880_E:
			device_set_desc(dev, "Creative CT5880-E");
			return BUS_PROBE_DEFAULT;

		default:
			device_set_desc(dev, "Creative CT5880-?");
			device_printf(dev, "unknown revision %d -- please report to cg@freebsd.org\n", pci_get_revid(dev));
			return BUS_PROBE_DEFAULT;
		}

	default:
		return ENXIO;
	}
}

#ifdef SND_DYNSYSCTL
static int
sysctl_es1371x_spdif_enable(SYSCTL_HANDLER_ARGS)
{
	struct es_info *es;
	device_t dev;
	int err, new_en, r;

	dev = oidp->oid_arg1;
	es = pcm_getdevinfo(dev);
	snd_mtxlock(es->lock);
	new_en = es->spdif_en;
	snd_mtxunlock(es->lock);
	err = sysctl_handle_int(oidp, &new_en, sizeof(new_en), req);

	if (err || req->newptr == NULL)
		return err;
	if (new_en < 0 || new_en > 1)
		return EINVAL;

	snd_mtxlock(es->lock);
	es->spdif_en = new_en;
	r = es_rd(es, ES1370_REG_STATUS, 4);
	if (new_en) {
		r |= ENABLE_SPDIF;
		es->ctrl |= SPDIFEN_B;
		es->ctrl |= RECEN_B;
	} else {
		r &= ~ENABLE_SPDIF;
		es->ctrl &= ~SPDIFEN_B;
		es->ctrl &= ~RECEN_B;
	}
	es_wr(es, ES1370_REG_CONTROL, es->ctrl, 4);
	es_wr(es, ES1370_REG_STATUS, r, 4);
	snd_mtxunlock(es->lock);
	return 0;
}

static int
sysctl_es1371x_latency_timer(SYSCTL_HANDLER_ARGS)
{
	struct es_info *es;
	device_t dev;
	int err, val;

	dev = oidp->oid_arg1;
	es = pcm_getdevinfo(dev);
	snd_mtxlock(es->lock);
	val = pci_read_config(dev, PCIR_LATTIMER, 1);
	snd_mtxunlock(es->lock);
	err = sysctl_handle_int(oidp, &val, sizeof(val), req);
	
	if (err || req->newptr == NULL)
		return err;
	if (val < 0 || val > 255)
		return EINVAL;

	snd_mtxlock(es->lock);
	pci_write_config(dev, PCIR_LATTIMER, val, 1);
	snd_mtxunlock(es->lock);
	return 0;
}
#endif /* SND_DYNSYSCTL */

static void
es_init_sysctls(device_t dev)
{
#ifdef SND_DYNSYSCTL
	struct es_info *es;
	int r, devid, revid;

	devid = pci_get_devid(dev);
	revid = pci_get_revid(dev);
	es = pcm_getdevinfo(dev);
	if ((devid == ES1371_PCI_ID && revid == ES1371REV_ES1373_8) ||
		    (devid == ES1371_PCI_ID && revid == ES1371REV_CT5880_A) ||
		    (devid == CT5880_PCI_ID && revid == CT5880REV_CT5880_C) ||
		    (devid == CT5880_PCI_ID && revid == CT5880REV_CT5880_D) ||
		    (devid == CT5880_PCI_ID && revid == CT5880REV_CT5880_E)) {
		r = es_rd(es, ES1370_REG_STATUS, 4);
		es->spdif_en = (r & ENABLE_SPDIF) ? 1 : 0;
		SYSCTL_ADD_PROC(snd_sysctl_tree(dev),
				SYSCTL_CHILDREN(snd_sysctl_tree_top(dev)),
				OID_AUTO, "spdif_enabled",
				CTLTYPE_INT | CTLFLAG_RW, dev, sizeof(dev),
				sysctl_es1371x_spdif_enable, "I",
				"Enable S/PDIF output on primary playback channel");
	}
	if (resource_int_value(device_get_name(dev),
			device_get_unit(dev), "latency_timer", &r) == 0 &&
			!(r < 0 || r > 255))
		pci_write_config(dev, PCIR_LATTIMER, r, 1);
	SYSCTL_ADD_PROC(snd_sysctl_tree(dev),
			SYSCTL_CHILDREN(snd_sysctl_tree_top(dev)),
			OID_AUTO, "latency_timer",
			CTLTYPE_INT | CTLFLAG_RW, dev, sizeof(dev),
			sysctl_es1371x_latency_timer, "I",
			"PCI Latency Timer configuration");
#endif /* SND_DYNSYSCTL */
}

static int
es_pci_attach(device_t dev)
{
	u_int32_t	data;
	struct es_info *es = 0;
	int		mapped;
	char		status[SND_STATUSLEN];
	struct ac97_info *codec = 0;
	kobj_class_t    ct = NULL;
	int devid, revid;

	if ((es = malloc(sizeof *es, M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}
	es->lock = snd_mtxcreate(device_get_nameunit(dev), "sound softc");
	es->dev = dev;
	mapped = 0;
	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);
	if (mapped == 0 && (data & PCIM_CMD_MEMEN)) {
		es->regid = MEM_MAP_REG;
		es->regtype = SYS_RES_MEMORY;
		es->reg = bus_alloc_resource_any(dev, es->regtype, &es->regid,
					 RF_ACTIVE);
		if (es->reg) {
			es->st = rman_get_bustag(es->reg);
			es->sh = rman_get_bushandle(es->reg);
			mapped++;
		}
	}
	if (mapped == 0 && (data & PCIM_CMD_PORTEN)) {
		es->regid = PCIR_BAR(0);
		es->regtype = SYS_RES_IOPORT;
		es->reg = bus_alloc_resource_any(dev, es->regtype, &es->regid,
					 RF_ACTIVE);
		if (es->reg) {
			es->st = rman_get_bustag(es->reg);
			es->sh = rman_get_bushandle(es->reg);
			mapped++;
		}
	}
	if (mapped == 0) {
		device_printf(dev, "unable to map register space\n");
		goto bad;
	}

	es->bufsz = pcm_getbuffersize(dev, 4096, ES_DEFAULT_BUFSZ, 65536);

	devid = pci_get_devid(dev);
	revid = pci_get_revid(dev);

	if (devid == ES1371_PCI_ID || devid == ES1371_PCI_ID2 ||
	    devid == CT5880_PCI_ID || devid == CT4730_PCI_ID) {
		if(-1 == es1371_init(es, dev)) {
			device_printf(dev, "unable to initialize the card\n");
			goto bad;
		}
		codec = AC97_CREATE(dev, es, es1371_ac97);
	  	if (codec == NULL) goto bad;
	  	/* our init routine does everything for us */
	  	/* set to NULL; flag mixer_init not to run the ac97_init */
	  	/*	  ac97_mixer.init = NULL;  */
		if (mixer_init(dev, ac97_getmixerclass(), codec)) goto bad;
		ct = &eschan1371_class;
	} else if (devid == ES1370_PCI_ID) {
	  	if (-1 == es1370_init(es)) {
			device_printf(dev, "unable to initialize the card\n");
			goto bad;
	  	}
	  	if (mixer_init(dev, &es1370_mixer_class, es)) goto bad;
		ct = &eschan1370_class;
	} else goto bad;

	es->irqid = 0;
	es->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &es->irqid,
				 RF_ACTIVE | RF_SHAREABLE);
	if (!es->irq || snd_setup_intr(dev, es->irq, INTR_MPSAFE, es_intr, es, &es->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/es->bufsz, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, /*lockfunc*/NULL,
		/*lockarg*/NULL, &es->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	snprintf(status, SND_STATUSLEN, "at %s 0x%lx irq %ld %s",
		 (es->regtype == SYS_RES_IOPORT)? "io" : "memory",
		 rman_get_start(es->reg), rman_get_start(es->irq),PCM_KLDSTRING(snd_es137x));

	if (pcm_register(dev, es, 1, 1)) goto bad;
	pcm_addchan(dev, PCMDIR_REC, ct, es);
	pcm_addchan(dev, PCMDIR_PLAY, ct, es);
	es_init_sysctls(dev);
	pcm_setstatus(dev, status);

	return 0;

 bad:
	if (es->parent_dmat) bus_dma_tag_destroy(es->parent_dmat);
	if (es->ih) bus_teardown_intr(dev, es->irq, es->ih);
	if (es->irq) bus_release_resource(dev, SYS_RES_IRQ, es->irqid, es->irq);
	if (codec) ac97_destroy(codec);
	if (es->reg) bus_release_resource(dev, es->regtype, es->regid, es->reg);
	if (es->lock) snd_mtxfree(es->lock);
	if (es) free(es, M_DEVBUF);
	return ENXIO;
}

static int
es_pci_detach(device_t dev)
{
	int r;
	struct es_info *es;

	r = pcm_unregister(dev);
	if (r) return r;

	es = pcm_getdevinfo(dev);
	bus_dma_tag_destroy(es->parent_dmat);
	bus_teardown_intr(dev, es->irq, es->ih);
	bus_release_resource(dev, SYS_RES_IRQ, es->irqid, es->irq);
	bus_release_resource(dev, es->regtype, es->regid, es->reg);
	snd_mtxfree(es->lock);
	free(es, M_DEVBUF);

	return 0;
}

static device_method_t es_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		es_pci_probe),
	DEVMETHOD(device_attach,	es_pci_attach),
	DEVMETHOD(device_detach,	es_pci_detach),

	{ 0, 0 }
};

static driver_t es_driver = {
	"pcm",
	es_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_es137x, pci, es_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_es137x, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(snd_es137x, 1);
