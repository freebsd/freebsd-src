/*
 * Support the ENSONIQ AudioPCI board based on the ES1370 and Codec
 * AK4531.
 *
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

#include "pci.h"
#include "pcm.h"

#include <dev/pcm/sound.h>
#include <dev/pcm/pci/es1370.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#if NPCI != 0

#define MEM_MAP_REG 0x14

/* PCI IDs of supported chips */
#define ES1370_PCI_ID 0x50001274

/* device private data */
struct es_info;

struct es_chinfo {
	struct es_info *parent;
	pcm_channel *channel;
	snd_dbuf *buffer;
	int dir;
	u_int32_t fmt;
};

struct es_info {
	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t	parent_dmat;

	/* Contents of board's registers */
	u_long		ctrl;
	u_long		sctrl;
	struct es_chinfo pch, rch;
};

/* -------------------------------------------------------------------- */

/* prototypes */
static int      es_init(struct es_info *);
static void     es_intr(void *);
static int      write_codec(struct es_info *, u_char, u_char);

/* channel interface */
static void *eschan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int eschan_setdir(void *data, int dir);
static int eschan_setformat(void *data, u_int32_t format);
static int eschan_setspeed(void *data, u_int32_t speed);
static int eschan_setblocksize(void *data, u_int32_t blocksize);
static int eschan_trigger(void *data, int go);
static int eschan_getptr(void *data);
static pcmchan_caps *eschan_getcaps(void *data);

static pcmchan_caps es_playcaps = {
	4000, 48000,
	AFMT_STEREO | AFMT_U8 | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcmchan_caps es_reccaps = {
	4000, 48000,
	AFMT_STEREO | AFMT_U8 | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcm_channel es_chantemplate = {
	eschan_init,
	eschan_setdir,
	eschan_setformat,
	eschan_setspeed,
	eschan_setblocksize,
	eschan_trigger,
	eschan_getptr,
	eschan_getcaps,
};

/* -------------------------------------------------------------------- */

/* The mixer interface */

static int es_mixinit(snd_mixer *m);
static int es_mixset(snd_mixer *m, unsigned dev, unsigned left, unsigned right);
static int es_mixsetrecsrc(snd_mixer *m, u_int32_t src);

static snd_mixer es_mixer = {
	"Ensoniq AudioPCI 1370 mixer",
	es_mixinit,
	es_mixset,
	es_mixsetrecsrc,
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
	[SOUND_MIXER_OGAIN]	= { 9, 0xf, 0x0, 0, 0x0000, 1 } };

static int
es_mixinit(snd_mixer *m)
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
es_mixset(snd_mixer *m, unsigned dev, unsigned left, unsigned right)
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
		write_codec(mix_getdevinfo(m), mixtable[dev].right, rr);
	}
	write_codec(mix_getdevinfo(m), mixtable[dev].left, rl);
	return l | (r << 8);
}

static int
es_mixsetrecsrc(snd_mixer *m, u_int32_t src)
{
	int i, j = 0;

	if (src == 0) src = 1 << SOUND_MIXER_MIC;
	src &= mix_getrecdevs(m);
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((src & (1 << i)) != 0) j |= mixtable[i].recmask;

	write_codec(mix_getdevinfo(m), CODEC_LIMIX1, j & 0x55);
	write_codec(mix_getdevinfo(m), CODEC_RIMIX1, j & 0xaa);
	write_codec(mix_getdevinfo(m), CODEC_LIMIX2, (j >> 8) & 0x17);
	write_codec(mix_getdevinfo(m), CODEC_RIMIX2, (j >> 8) & 0x0f);
	write_codec(mix_getdevinfo(m), CODEC_OMIX1, 0x7f);
	write_codec(mix_getdevinfo(m), CODEC_OMIX2, 0x3f);
	return src;
}

static int
write_codec(struct es_info *es, u_char i, u_char data)
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
	printf("pcm: write_codec timed out\n");
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
eschan_setspeed(void *data, u_int32_t speed)
{
	struct es_chinfo *ch = data;
	struct es_info *es = ch->parent;

	es->ctrl &= ~CTRL_PCLKDIV;
	es->ctrl |= DAC2_SRTODIV(speed) << CTRL_SH_PCLKDIV;
	bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);
	/* rec/play speeds locked together - should indicate in flags */
#if 0
	if (ch->direction == PCMDIR_PLAY) d->rec[0].speed = speed;
	else d->play[0].speed = speed;
#endif
	return speed; /* XXX calc real speed */
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
	unsigned cnt = ch->buffer->dl / ch->buffer->sample_size - 1;

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
		} else es->ctrl &= ~CTRL_DAC2_EN;
	} else {
		if (go == PCMTRIG_START) {
			es->ctrl |= CTRL_ADC_EN;
			es->sctrl &= ~SCTRL_R1LOOPSEL;
			es->sctrl |= SCTRL_R1INTEN;
			bus_space_write_4(es->st, es->sh,
					  ES1370_REG_ADC_SCOUNT, cnt);
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
	if (ch->dir == PCMDIR_PLAY) {
		bus_space_write_4(es->st, es->sh, ES1370_REG_MEMPAGE,
				  ES1370_REG_DAC2_FRAMECNT >> 8);
		return (bus_space_read_4(es->st, es->sh,
				         ES1370_REG_DAC2_FRAMECNT & 0xff) >> 14) & 0x3fffc;
	} else {
		bus_space_write_4(es->st, es->sh, ES1370_REG_MEMPAGE,
				  ES1370_REG_ADC_FRAMECNT >> 8);
		return (bus_space_read_4(es->st, es->sh,
				         ES1370_REG_ADC_FRAMECNT & 0xff) >> 14) & 0x3fffc;
	}
}

static pcmchan_caps *
eschan_getcaps(void *data)
{
	struct es_chinfo *ch = data;
	return (ch->dir == PCMDIR_PLAY)? &es_playcaps : &es_reccaps;
}

/* The interrupt handler */
static void
es_intr (void *p)
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

	if (intsrc & STAT_DAC2)	chn_intr(es->pch.channel);
	if (intsrc & STAT_ADC) chn_intr(es->rch.channel);
}

/* -------------------------------------------------------------------- */

/*
 * Probe and attach the card
 */

static int
es_init(struct es_info *es)
{
	es->ctrl = CTRL_CDC_EN | CTRL_SERR_DIS |
		(DAC2_SRTODIV(DSP_DEFAULT_SPEED) << CTRL_SH_PCLKDIV);
	bus_space_write_4(es->st, es->sh, ES1370_REG_CONTROL, es->ctrl);

	es->sctrl = 0;
	bus_space_write_4(es->st, es->sh, ES1370_REG_SERIAL_CONTROL, es->sctrl);

	write_codec(es, CODEC_RES_PD, 3);/* No RST, PD */
	write_codec(es, CODEC_CSEL, 0);	/* CODEC ADC and CODEC DAC use
					 * {LR,B}CLK2 and run off the LRCLK2
					 * PLL; program DAC_SYNC=0!  */
	write_codec(es, CODEC_ADSEL, 0);/* Recording source is mixer */
	write_codec(es, CODEC_MGAIN, 0);/* MIC amp is 0db */

	return 0;
}

static int
es_pci_probe(device_t dev)
{
	if (pci_get_devid(dev) == ES1370_PCI_ID) {
		device_set_desc(dev, "AudioPCI ES1370");
		return 0;
	}
	return ENXIO;
}

static int
es_pci_attach(device_t dev)
{
	snddev_info    *d;
	u_int32_t	data;
	struct es_info *es = 0;
	int		type = 0;
	int		regid;
	struct resource *reg = 0;
	int		mapped;
	int		irqid;
	struct resource *irq = 0;
	void		*ih = 0;
	char		status[SND_STATUSLEN];

	d = device_get_softc(dev);
	if ((es = malloc(sizeof *es, M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}
	bzero(es, sizeof *es);

	mapped = 0;
	data = pci_read_config(dev, PCIR_COMMAND, 2);
	if (mapped == 0 && (data & PCIM_CMD_MEMEN)) {
		regid = MEM_MAP_REG;
		type = SYS_RES_MEMORY;
		reg = bus_alloc_resource(dev, type, &regid,
					 0, ~0, 1, RF_ACTIVE);
		if (reg) {
			es->st = rman_get_bustag(reg);
			es->sh = rman_get_bushandle(reg);
			mapped++;
		}
	}
	if (mapped == 0 && (data & PCIM_CMD_PORTEN)) {
		regid = PCI_MAP_REG_START;
		type = SYS_RES_IOPORT;
		reg = bus_alloc_resource(dev, type, &regid,
					 0, ~0, 1, RF_ACTIVE);
		if (reg) {
			es->st = rman_get_bustag(reg);
			es->sh = rman_get_bushandle(reg);
			mapped++;
		}
	}
	if (mapped == 0) {
		device_printf(dev, "unable to map register space\n");
		goto bad;
	}

	if (es_init(es) == -1) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}
	mixer_init(d, &es_mixer, es);

	irqid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &irqid,
				 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!irq
	    || bus_setup_intr(dev, irq, INTR_TYPE_TTY, es_intr, es, &ih)) {
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
		 (type == SYS_RES_IOPORT)? "io" : "memory",
		 rman_get_start(reg), rman_get_start(irq));

	if (pcm_register(dev, es, 1, 1)) goto bad;
	pcm_addchan(dev, PCMDIR_REC, &es_chantemplate, es);
	pcm_addchan(dev, PCMDIR_PLAY, &es_chantemplate, es);
	pcm_setstatus(dev, status);

	return 0;

 bad:
	if (es) free(es, M_DEVBUF);
	if (reg) bus_release_resource(dev, type, regid, reg);
	if (ih) bus_teardown_intr(dev, irq, ih);
	if (irq) bus_release_resource(dev, SYS_RES_IRQ, irqid, irq);
	return ENXIO;
}

static device_method_t es_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		es_pci_probe),
	DEVMETHOD(device_attach,	es_pci_attach),

	{ 0, 0 }
};

static driver_t es_driver = {
	"pcm",
	es_methods,
	sizeof(snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(es, pci, es_driver, pcm_devclass, 0, 0);

#endif /* NPCI != 0 */
