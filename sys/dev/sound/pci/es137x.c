/*
 * Support the ENSONIQ AudioPCI board and Creative Labs SoundBlaster PCI
 * boards based on the ES1370, ES1371 and ES1373 chips.
 *
 * Copyright (c) 1999 Russell Cattelan <cattelan@thebarn.com>
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 *
 * $FreeBSD$
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

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <sys/sysctl.h>

static int debug = 0;
SYSCTL_INT(_debug, OID_AUTO, es_debug, CTLFLAG_RW, &debug, 0, "");

#define MEM_MAP_REG 0x14

/* PCI IDs of supported chips */
#define ES1370_PCI_ID 0x50001274
#define ES1371_PCI_ID 0x13711274
#define ES1371_PCI_ID2 0x13713274
#define ES1371_PCI_ID3 0x58801274

#define ES_BUFFSIZE 4096

/* device private data */
struct es_info;

struct es_chinfo {
	struct es_info *parent;
	pcm_channel *channel;
	snd_dbuf *buffer;
	int dir, num;
	u_int32_t fmt;
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
	/* Contents of board's registers */
	u_long		ctrl;
	u_long		sctrl;
	struct es_chinfo pch, rch;
};

/* -------------------------------------------------------------------- */

/* prototypes */
static void     es_intr(void *);

static void	es1371_wrcodec(void *, int, u_int32_t);
static u_int32_t es1371_rdcodec(void *, int);
static u_int	es1371_wait_src_ready(struct es_info *);
static void	es1371_src_write(struct es_info *, u_short, unsigned short);
static u_int	es1371_adc_rate(struct es_info *, u_int, int);
static u_int	es1371_dac_rate(struct es_info *, u_int, int);
static int	es1371_init(struct es_info *es, int);
static int      es1370_init(struct es_info *);
static int      es1370_wrcodec(struct es_info *, u_char, u_char);

/* channel interface */
static void *eschan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int   eschan_setdir(void *data, int dir);
static int   eschan_setformat(void *data, u_int32_t format);
static int   eschan1370_setspeed(void *data, u_int32_t speed);
static int   eschan1371_setspeed(void *data, u_int32_t speed);
static int   eschan_setblocksize(void *data, u_int32_t blocksize);
static int   eschan_trigger(void *data, int go);
static int   eschan_getptr(void *data);
static pcmchan_caps *eschan_getcaps(void *data);

static u_int32_t es_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static pcmchan_caps es_playcaps = {4000, 48000, es_playfmt, 0};

static u_int32_t es_recfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static pcmchan_caps es_reccaps = {4000, 48000, es_recfmt, 0};

static pcm_channel es1370_chantemplate = {
	eschan_init,
	eschan_setdir,
	eschan_setformat,
	eschan1370_setspeed,
	eschan_setblocksize,
	eschan_trigger,
	eschan_getptr,
	eschan_getcaps,
	NULL, 			/* free */
	NULL, 			/* nop1 */
	NULL, 			/* nop2 */
	NULL, 			/* nop3 */
	NULL, 			/* nop4 */
	NULL, 			/* nop5 */
	NULL, 			/* nop6 */
	NULL, 			/* nop7 */
};

static pcm_channel es1371_chantemplate = {
	eschan_init,
	eschan_setdir,
	eschan_setformat,
	eschan1371_setspeed,
	eschan_setblocksize,
	eschan_trigger,
	eschan_getptr,
	eschan_getcaps,
	NULL, 			/* free */
	NULL, 			/* nop1 */
	NULL, 			/* nop2 */
	NULL, 			/* nop3 */
	NULL, 			/* nop4 */
	NULL, 			/* nop5 */
	NULL, 			/* nop6 */
	NULL, 			/* nop7 */
};

/* -------------------------------------------------------------------- */

/* The es1370 mixer interface */

static int es1370_mixinit(snd_mixer *m);
static int es1370_mixset(snd_mixer *m, unsigned dev, unsigned left, unsigned right);
static int es1370_mixsetrecsrc(snd_mixer *m, u_int32_t src);

static snd_mixer es1370_mixer = {
	"AudioPCI 1370 mixer",
	es1370_mixinit,
	NULL,
	NULL,
	es1370_mixset,
	es1370_mixsetrecsrc,
};

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

static int
es1370_mixinit(snd_mixer *m)
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
es1370_mixset(snd_mixer *m, unsigned dev, unsigned left, unsigned right)
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
es1370_mixsetrecsrc(snd_mixer *m, u_int32_t src)
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

static int
es1370_wrcodec(struct es_info *es, u_char i, u_char data)
{
	int		wait = 100;	/* 100 msec timeout */

	do {
		if ((bus_space_read_4(es->st, es->sh, ES1370_REG_STATUS) &
		      STAT_CSTAT) == 0) {
			bus_space_write_2(es->st, es->sh, ES1370_REG_CODEC,
				((u_short)i << CODEC_INDEX_SHIFT) | data);
			return 0;
		}
		DELAY(1000);
	} while (--wait);
	printf("pcm: es1370_wrcodec timed out\n");
	return -1;
}

/* -------------------------------------------------------------------- */

/* channel interface */
static void *
eschan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct es_info *es = devinfo;
	struct es_chinfo *ch = (dir == PCMDIR_PLAY)? &es->pch : &es->rch;

	ch->parent = es;
	ch->channel = c;
	ch->buffer = b;
	ch->buffer->bufsize = ES_BUFFSIZE;
	ch->num = ch->parent->num++;
	if (chn_allocbuf(ch->buffer, es->parent_dmat) == -1) return NULL;
	return ch;
}

static int
eschan_setdir(void *data, int dir)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;

	if (dir == PCMDIR_PLAY) {
		bus_space_write_1(es->st, es->sh, ES1370_REG_MEMPAGE,
				  ES1370_REG_DAC2_FRAMEADR >> 8);
		bus_space_write_4(es->st, es->sh, ES1370_REG_DAC2_FRAMEADR & 0xff,
				  vtophys(ch->buffer->buf));
		bus_space_write_4(es->st, es->sh, ES1370_REG_DAC2_FRAMECNT & 0xff,
				  (ch->buffer->bufsize >> 2) - 1);
	} else {
		bus_space_write_1(es->st, es->sh, ES1370_REG_MEMPAGE,
				  ES1370_REG_ADC_FRAMEADR >> 8);
		bus_space_write_4(es->st, es->sh, ES1370_REG_ADC_FRAMEADR & 0xff,
				  vtophys(ch->buffer->buf));
		bus_space_write_4(es->st, es->sh, ES1370_REG_ADC_FRAMECNT & 0xff,
				  (ch->buffer->bufsize >> 2) - 1);
	}
	ch->dir = dir;
	return 0;
}

static int
eschan_setformat(void *data, u_int32_t format)
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
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);
	ch->fmt = format;
	return 0;
}

static int
eschan1370_setspeed(void *data, u_int32_t speed)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;

	es->ctrl &= ~CTRL_PCLKDIV;
	es->ctrl |= DAC2_SRTODIV(speed) << CTRL_SH_PCLKDIV;
	bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
	/* rec/play speeds locked together - should indicate in flags */
	return speed; /* XXX calc real speed */
}

int
eschan1371_setspeed(void *data, u_int32_t speed)
{
  	struct es_chinfo *ch = data;
  	struct es_info *es = ch->parent;

	if (ch->dir == PCMDIR_PLAY) {
  		return es1371_dac_rate(es, speed, 3 - ch->num); /* play */
	} else {
  		return es1371_adc_rate(es, speed, 1); /* record */
	}
}

static int
eschan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
eschan_trigger(void *data, int go)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;
	unsigned ss, cnt;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	ss = 1;
	ss <<= (ch->fmt & AFMT_STEREO)? 1 : 0;
	ss <<= (ch->fmt & AFMT_16BIT)? 1 : 0;
	cnt = ch->buffer->dl / ss - 1;

	if (ch->dir == PCMDIR_PLAY) {
		if (go == PCMTRIG_START) {
			int b = (ch->fmt & AFMT_S16_LE)? 2 : 1;
			es->ctrl |= CTRL_DAC2_EN;
			es->sctrl &= ~(SCTRL_P2ENDINC | SCTRL_P2STINC |
				       SCTRL_P2LOOPSEL | SCTRL_P2PAUSE |
				       SCTRL_P2DACSEN);
			es->sctrl |= SCTRL_P2INTEN | (b << SCTRL_SH_P2ENDINC);
			bus_space_write_4(es->st, es->sh,
					  ES1370_REG_DAC2_SCOUNT, cnt);
			/* start at beginning of buffer */
			bus_space_write_4(es->st, es->sh, ES1370_REG_MEMPAGE,
					  ES1370_REG_DAC2_FRAMECNT >> 8);
			bus_space_write_4(es->st, es->sh,
					  ES1370_REG_DAC2_FRAMECNT & 0xff,
				  	  (ch->buffer->bufsize >> 2) - 1);
		} else es->ctrl &= ~CTRL_DAC2_EN;
	} else {
		if (go == PCMTRIG_START) {
			es->ctrl |= CTRL_ADC_EN;
			es->sctrl &= ~SCTRL_R1LOOPSEL;
			es->sctrl |= SCTRL_R1INTEN;
			bus_space_write_4(es->st, es->sh,
					  ES1370_REG_ADC_SCOUNT, cnt);
			/* start at beginning of buffer */
			bus_space_write_4(es->st, es->sh, ES1370_REG_MEMPAGE,
					  ES1370_REG_ADC_FRAMECNT >> 8);
			bus_space_write_4(es->st, es->sh,
					  ES1370_REG_ADC_FRAMECNT & 0xff,
				  	  (ch->buffer->bufsize >> 2) - 1);
		} else es->ctrl &= ~CTRL_ADC_EN;
	}
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);
	bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
	return 0;
}

static int
eschan_getptr(void *data)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;
	u_int32_t reg, cnt;

	if (ch->dir == PCMDIR_PLAY)
		reg = ES1370_REG_DAC2_FRAMECNT;
	else
		reg = ES1370_REG_ADC_FRAMECNT;

	bus_space_write_4(es->st, es->sh, ES1370_REG_MEMPAGE, reg >> 8);
	cnt = bus_space_read_4(es->st, es->sh, reg & 0x000000ff) >> 16;
	/* cnt is longwords */
	return cnt << 2;
}

static pcmchan_caps *
eschan_getcaps(void *data)
{
	struct es_chinfo *ch = data;
	return (ch->dir == PCMDIR_PLAY)? &es_playcaps : &es_reccaps;
}

/* The interrupt handler */
static void
es_intr(void *p)
{
	struct es_info *es = p;
	unsigned	intsrc, sctrl;

	intsrc = bus_space_read_4(es->st, es->sh, ES1370_REG_STATUS);
	if ((intsrc & STAT_INTR) == 0) return;

	sctrl = es->sctrl;
	if (intsrc & STAT_ADC)  sctrl &= ~SCTRL_R1INTEN;
	if (intsrc & STAT_DAC1)	sctrl &= ~SCTRL_P1INTEN;
	if (intsrc & STAT_DAC2)	sctrl &= ~SCTRL_P2INTEN;

	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, sctrl);
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);

	if (intsrc & STAT_ADC) chn_intr(es->rch.channel);
	if (intsrc & STAT_DAC1);
	if (intsrc & STAT_DAC2)	chn_intr(es->pch.channel);
}

/* ES1370 specific */
static int
es1370_init(struct es_info *es)
{
	es->ctrl = CTRL_CDC_EN | CTRL_SERR_DIS |
		(DAC2_SRTODIV(DSP_DEFAULT_SPEED) << CTRL_SH_PCLKDIV);
	bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);

	es->sctrl = 0;
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);

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
es1371_init(struct es_info *es, int rev)
{
	int idx;

	if (debug > 0) printf("es_init\n");

	es->num = 0;
	es->ctrl = 0;
	es->sctrl = 0;
	/* initialize the chips */
	if (rev == 7 || rev >= 9 || rev == 2) {
#define ES1371_BINTSUMM_OFF 0x07
		bus_space_write_4(es->st, es->sh, ES1371_BINTSUMM_OFF, 0x20);
		if (debug > 0) printf("es_init rev == 7 || rev >= 9\n");
	} else { /* pre ac97 2.1 card */
		bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
		if (debug > 0) printf("es_init pre ac97 2.1\n");
	}
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);
	bus_space_write_4(es->st, es->sh, ES1371_REG_LEGACY, 0);
	/* AC'97 warm reset to start the bitclk */
	bus_space_write_4(es->st, es->sh, ES1371_REG_LEGACY, es->ctrl | ES1371_SYNC_RES);
	DELAY(2000);
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->ctrl);
	/* Init the sample rate converter */
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, ES1371_DIS_SRC);
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
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, 0);

	return (0);
}

static void
es1371_wrcodec(void *s, int addr, u_int32_t data)
{
    	int sl;
    	unsigned t, x;
	struct es_info *es = (struct es_info*)s;

	if (debug > 0) printf("wrcodec addr 0x%x data 0x%x\n", addr, data);

	for (t = 0; t < 0x1000; t++)
	  	if (!(bus_space_read_4(es->st, es->sh,(ES1371_REG_CODEC & CODEC_WIP))))
			break;
	sl = spltty();
	/* save the current state for later */
 	x = bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE);
	/* enable SRC state data in SRC mux */
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE,
	  	(es1371_wait_src_ready(s) &
	   	(ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1)));
	/* wait for a SAFE time to write addr/data and then do it, dammit */
	for (t = 0; t < 0x1000; t++)
	  	if ((bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE) & 0x00070000) == 0x00010000)
			break;

	if (debug > 2) printf("one b_s_w: 0x%x 0x%x 0x%x\n", es->sh, ES1371_REG_CODEC,
			 ((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) |
			 ((data << CODEC_PODAT_SHIFT) & CODEC_PODAT_MASK));

	bus_space_write_4(es->st, es->sh,ES1371_REG_CODEC,
			  ((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) |
			  ((data << CODEC_PODAT_SHIFT) & CODEC_PODAT_MASK));
	/* restore SRC reg */
	es1371_wait_src_ready(s);
	if (debug > 2) printf("two b_s_w: 0x%x 0x%x 0x%x\n", es->sh, ES1371_REG_SMPRATE, x);
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, x);
	splx(sl);
}

static u_int32_t
es1371_rdcodec(void *s, int addr)
{
  	int sl;
  	unsigned t, x;
  	struct es_info *es = (struct es_info *)s;

  	if (debug > 0) printf("rdcodec addr 0x%x ... ", addr);

  	for (t = 0; t < 0x1000; t++)
		if (!(x = bus_space_read_4(es->st, es->sh, ES1371_REG_CODEC) & CODEC_WIP))
	  		break;
   	if (debug > 0) printf("loop 1 t 0x%x x 0x%x ", t, x);

  	sl = spltty();

  	/* save the current state for later */
  	x = bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE);
  	/* enable SRC state data in SRC mux */
  	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE,
			  (es1371_wait_src_ready(s) &
			  (ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1)));
  	/* wait for a SAFE time to write addr/data and then do it, dammit */
  	for (t = 0; t < 0x5000; t++)
		if ((x = bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE) & 0x00070000) == 0x00010000)
	  		break;
  	if (debug > 0) printf("loop 2 t 0x%x x 0x%x ", t, x);
  	bus_space_write_4(es->st, es->sh, ES1371_REG_CODEC,
			  ((addr << CODEC_POADD_SHIFT) & CODEC_POADD_MASK) | CODEC_PORD);

  	/* restore SRC reg */
  	es1371_wait_src_ready(s);
  	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, x);

  	splx(sl);

  	/* now wait for the stinkin' data (RDY) */
  	for (t = 0; t < 0x1000; t++)
		if ((x = bus_space_read_4(es->st, es->sh, ES1371_REG_CODEC)) & CODEC_RDY)
	  		break;
  	if (debug > 0) printf("loop 3 t 0x%x 0x%x ret 0x%x\n", t, x, ((x & CODEC_PIDAT_MASK) >> CODEC_PIDAT_SHIFT));
  	return ((x & CODEC_PIDAT_MASK) >> CODEC_PIDAT_SHIFT);
}

static u_int
es1371_src_read(struct es_info *es, u_short reg)
{
  	unsigned int r;

  	r = es1371_wait_src_ready(es) &
		(ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1);
  	r |= ES1371_SRC_RAM_ADDRO(reg);
  	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE,r);
  	return ES1371_SRC_RAM_DATAI(es1371_wait_src_ready(es));
}

static void
es1371_src_write(struct es_info *es, u_short reg, u_short data){
	u_int r;

	r = es1371_wait_src_ready(es) &
		(ES1371_DIS_SRC | ES1371_DIS_P1 | ES1371_DIS_P2 | ES1371_DIS_R1);
	r |= ES1371_SRC_RAM_ADDRO(reg) |  ES1371_SRC_RAM_DATAO(data);
	/*	printf("es1371_src_write 0x%x 0x%x\n",ES1371_REG_SMPRATE,r | ES1371_SRC_RAM_WE); */
	bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, r | ES1371_SRC_RAM_WE);
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
		bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, r);
		es1371_src_write(es, dac + ES_SMPREG_INT_REGS,
			 	(es1371_src_read(es, dac + ES_SMPREG_INT_REGS) & 0x00ff) | ((freq >> 5) & 0xfc00));
		es1371_src_write(es, dac + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
		r = (es1371_wait_src_ready(es) & (ES1371_DIS_SRC | dis | ES1371_DIS_R1));
		bus_space_write_4(es->st, es->sh, ES1371_REG_SMPRATE, r);
  	}
  	return result;
}

static u_int
es1371_wait_src_ready(struct es_info *es)
{
  	u_int t, r;

  	for (t = 0; t < 500; t++) {
		if (!((r = bus_space_read_4(es->st, es->sh, ES1371_REG_SMPRATE)) & ES1371_SRC_RAM_BUSY))
	  		return r;
		DELAY(1000);
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
	if (pci_get_devid(dev) == ES1370_PCI_ID) {
		device_set_desc(dev, "AudioPCI ES1370");
		return 0;
	} else if (pci_get_devid(dev) == ES1371_PCI_ID ||
		   pci_get_devid(dev) == ES1371_PCI_ID2 ||
		   pci_get_devid(dev) == ES1371_PCI_ID3) {
		device_set_desc(dev, "AudioPCI ES1371");
		return 0;
	}
	return ENXIO;
}

static int
es_pci_attach(device_t dev)
{
	u_int32_t	data;
	struct es_info *es = 0;
	int		mapped;
	char		status[SND_STATUSLEN];
	struct ac97_info *codec = 0;
	pcm_channel     *ct = NULL;

	if ((es = malloc(sizeof *es, M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}
	bzero(es, sizeof *es);

	es->dev = dev;
	mapped = 0;
	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);
	if (mapped == 0 && (data & PCIM_CMD_MEMEN)) {
		es->regid = MEM_MAP_REG;
		es->regtype = SYS_RES_MEMORY;
		es->reg = bus_alloc_resource(dev, es->regtype, &es->regid,
					 0, ~0, 1, RF_ACTIVE);
		if (es->reg) {
			es->st = rman_get_bustag(es->reg);
			es->sh = rman_get_bushandle(es->reg);
			mapped++;
		}
	}
	if (mapped == 0 && (data & PCIM_CMD_PORTEN)) {
		es->regid = PCIR_MAPS;
		es->regtype = SYS_RES_IOPORT;
		es->reg = bus_alloc_resource(dev, es->regtype, &es->regid,
					 0, ~0, 1, RF_ACTIVE);
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

	if (pci_get_devid(dev) == ES1371_PCI_ID ||
	    pci_get_devid(dev) == ES1371_PCI_ID2 ||
	    pci_get_devid(dev) == ES1371_PCI_ID3) {
		if(-1 == es1371_init(es, pci_get_revid(dev))) {
			device_printf(dev, "unable to initialize the card\n");
			goto bad;
		}
	  	codec = ac97_create(dev, es, NULL, es1371_rdcodec, es1371_wrcodec);
	  	if (codec == NULL) goto bad;
	  	/* our init routine does everything for us */
	  	/* set to NULL; flag mixer_init not to run the ac97_init */
	  	/*	  ac97_mixer.init = NULL;  */
		if (mixer_init(dev, &ac97_mixer, codec) == -1) goto bad;
		ct = &es1371_chantemplate;
	} else if (pci_get_devid(dev) == ES1370_PCI_ID) {
	  	if (-1 == es1370_init(es)) {
			device_printf(dev, "unable to initialize the card\n");
			goto bad;
	  	}
	  	mixer_init(dev, &es1370_mixer, es);
		ct = &es1370_chantemplate;
	} else goto bad;

	es->irqid = 0;
	es->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &es->irqid,
				 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!es->irq
	    || bus_setup_intr(dev, es->irq, INTR_TYPE_TTY, es_intr, es, &es->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/ES_BUFFSIZE, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &es->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	snprintf(status, SND_STATUSLEN, "at %s 0x%lx irq %ld",
		 (es->regtype == SYS_RES_IOPORT)? "io" : "memory",
		 rman_get_start(es->reg), rman_get_start(es->irq));

	if (pcm_register(dev, es, 1, 1)) goto bad;
	pcm_addchan(dev, PCMDIR_REC, ct, es);
	pcm_addchan(dev, PCMDIR_PLAY, ct, es);
	pcm_setstatus(dev, status);

	return 0;

 bad:
	if (codec) ac97_destroy(codec);
	if (es->reg) bus_release_resource(dev, es->regtype, es->regid, es->reg);
	if (es->ih) bus_teardown_intr(dev, es->irq, es->ih);
	if (es->irq) bus_release_resource(dev, SYS_RES_IRQ, es->irqid, es->irq);
	if (es->parent_dmat) bus_dma_tag_destroy(es->parent_dmat);
	if (es) free(es, M_DEVBUF);
	return ENXIO;
}

static int
es_pci_detach(device_t dev)
{
	int r;
	struct es_info *es;

	r = pcm_unregister(dev);
	if (r)
		return r;

	es = pcm_getdevinfo(dev);
	bus_release_resource(dev, es->regtype, es->regid, es->reg);
	bus_teardown_intr(dev, es->irq, es->ih);
	bus_release_resource(dev, SYS_RES_IRQ, es->irqid, es->irq);
	bus_dma_tag_destroy(es->parent_dmat);
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
	sizeof(snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(snd_es137x, pci, es_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_es137x, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_es137x, 1);
