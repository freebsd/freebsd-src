/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <gnu/dev/sound/pci/emu10k1.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <sys/queue.h>

/* -------------------------------------------------------------------- */

#define EMU10K1_PCI_ID 	0x00021102
#define EMU_BUFFSIZE	4096
#define EMU_CHANS	4
#undef EMUDEBUG

struct emu_memblk {
	SLIST_ENTRY(emu_memblk) link;
	void *buf;
	u_int32_t pte_start, pte_size;
};

struct emu_mem {
	u_int8_t bmap[MAXPAGES / 8];
	u_int32_t *ptb_pages;
	void *silent_page;
       	SLIST_HEAD(, emu_memblk) blocks;
};

struct emu_voice {
	int vnum;
	int b16:1, stereo:1, busy:1, running:1, ismaster:1;
	int speed;
	int start, end, vol;
	u_int32_t buf;
	struct emu_voice *slave;
	pcm_channel *channel;
};

struct sc_info;

/* channel registers */
struct sc_pchinfo {
	int spd, fmt, run;
	struct emu_voice *master, *slave;
	snd_dbuf *buffer;
	pcm_channel *channel;
	struct sc_info *parent;
};

struct sc_rchinfo {
	int spd, fmt, run, num;
	u_int32_t idxreg, basereg, sizereg, setupreg, irqmask;
	snd_dbuf *buffer;
	pcm_channel *channel;
	struct sc_info *parent;
};

/* device private data */
struct sc_info {
	device_t	dev;
	u_int32_t 	type, rev;
	u_int32_t	tos_link:1, APS:1;

	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t parent_dmat;

	struct resource *reg, *irq;
	int		regtype, regid, irqid;
	void		*ih;

	int timer;
	int pnum, rnum;
	struct emu_mem mem;
	struct emu_voice voice[64];
	struct sc_pchinfo pch[EMU_CHANS];
	struct sc_rchinfo rch[3];
};

/* -------------------------------------------------------------------- */

/*
 * prototypes
 */

/* channel interface */
static void *emupchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int emupchan_setdir(void *data, int dir);
static int emupchan_setformat(void *data, u_int32_t format);
static int emupchan_setspeed(void *data, u_int32_t speed);
static int emupchan_setblocksize(void *data, u_int32_t blocksize);
static int emupchan_trigger(void *data, int go);
static int emupchan_getptr(void *data);
static pcmchan_caps *emupchan_getcaps(void *data);

/* channel interface */
static void *emurchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int emurchan_setdir(void *data, int dir);
static int emurchan_setformat(void *data, u_int32_t format);
static int emurchan_setspeed(void *data, u_int32_t speed);
static int emurchan_setblocksize(void *data, u_int32_t blocksize);
static int emurchan_trigger(void *data, int go);
static int emurchan_getptr(void *data);
static pcmchan_caps *emurchan_getcaps(void *data);

/* talk to the codec - called from ac97.c */
static u_int32_t emu_rdcd(void *, int);
static void emu_wrcd(void *, int, u_int32_t);

/* stuff */
static int emu_init(struct sc_info *);
static void emu_intr(void *);
static void *emu_malloc(struct sc_info *sc, u_int32_t sz);
static void *emu_memalloc(struct sc_info *sc, u_int32_t sz);
#ifdef notyet
static int emu_memfree(struct sc_info *sc, void *buf);
#endif
static int emu_memstart(struct sc_info *sc, void *buf);
#ifdef EMUDEBUG
static void emu_vdump(struct sc_info *sc, struct emu_voice *v);
#endif

/* talk to the card */
static u_int32_t emu_rd(struct sc_info *, int, int);
static void emu_wr(struct sc_info *, int, u_int32_t, int);

/* -------------------------------------------------------------------- */

static u_int32_t emu_rfmt_ac97[] = {
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static u_int32_t emu_rfmt_mic[] = {
	AFMT_U8,
	0
};

static u_int32_t emu_rfmt_efx[] = {
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static pcmchan_caps emu_reccaps[3] = {
	{8000, 48000, emu_rfmt_ac97, 0},
	{8000, 8000, emu_rfmt_mic, 0},
	{48000, 48000, emu_rfmt_efx, 0},
};

static u_int32_t emu_pfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static pcmchan_caps emu_playcaps = {4000, 48000, emu_pfmt, 0};

static pcm_channel emu_chantemplate = {
	emupchan_init,
	emupchan_setdir,
	emupchan_setformat,
	emupchan_setspeed,
	emupchan_setblocksize,
	emupchan_trigger,
	emupchan_getptr,
	emupchan_getcaps,
};

static pcm_channel emur_chantemplate = {
	emurchan_init,
	emurchan_setdir,
	emurchan_setformat,
	emurchan_setspeed,
	emurchan_setblocksize,
	emurchan_trigger,
	emurchan_getptr,
	emurchan_getcaps,
};

static int adcspeed[8] = {48000, 44100, 32000, 24000, 22050, 16000, 11025, 8000};

/* -------------------------------------------------------------------- */
/* Hardware */
static u_int32_t
emu_rd(struct sc_info *sc, int regno, int size)
{
	switch (size) {
	case 1:
		return bus_space_read_1(sc->st, sc->sh, regno);
	case 2:
		return bus_space_read_2(sc->st, sc->sh, regno);
	case 4:
		return bus_space_read_4(sc->st, sc->sh, regno);
	default:
		return 0xffffffff;
	}
}

static void
emu_wr(struct sc_info *sc, int regno, u_int32_t data, int size)
{
	switch (size) {
	case 1:
		bus_space_write_1(sc->st, sc->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(sc->st, sc->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(sc->st, sc->sh, regno, data);
		break;
	}
}

static u_int32_t
emu_rdptr(struct sc_info *sc, int chn, int reg)
{
        u_int32_t ptr, val, mask, size, offset;

        ptr = ((reg << 16) & PTR_ADDRESS_MASK) | (chn & PTR_CHANNELNUM_MASK);
        emu_wr(sc, PTR, ptr, 4);
        val = emu_rd(sc, DATA, 4);
        if (reg & 0xff000000) {
                size = (reg >> 24) & 0x3f;
                offset = (reg >> 16) & 0x1f;
                mask = ((1 << size) - 1) << offset;
                val &= mask;
		val >>= offset;
	}
        return val;
}

static void
emu_wrptr(struct sc_info *sc, int chn, int reg, u_int32_t data)
{
        u_int32_t ptr, mask, size, offset;

        ptr = ((reg << 16) & PTR_ADDRESS_MASK) | (chn & PTR_CHANNELNUM_MASK);
        emu_wr(sc, PTR, ptr, 4);
        if (reg & 0xff000000) {
                size = (reg >> 24) & 0x3f;
                offset = (reg >> 16) & 0x1f;
                mask = ((1 << size) - 1) << offset;
		data <<= offset;
                data &= mask;
		data |= emu_rd(sc, DATA, 4) & ~mask;
	}
        emu_wr(sc, DATA, data, 4);
}

static void
emu_wrefx(struct sc_info *sc, unsigned int pc, unsigned int data)
{
	emu_wrptr(sc, 0, MICROCODEBASE + pc, data);
}

/* ac97 codec */
static u_int32_t
emu_rdcd(void *devinfo, int regno)
{
	struct sc_info *sc = (struct sc_info *)devinfo;

	emu_wr(sc, AC97ADDRESS, regno, 1);
	return emu_rd(sc, AC97DATA, 2);
}

static void
emu_wrcd(void *devinfo, int regno, u_int32_t data)
{
	struct sc_info *sc = (struct sc_info *)devinfo;

	emu_wr(sc, AC97ADDRESS, regno, 1);
	emu_wr(sc, AC97DATA, data, 2);
}

#if 0
/* playback channel interrupts */
static u_int32_t
emu_testint(struct sc_info *sc, char channel)
{
	int reg = (channel & 0x20)? CLIPH : CLIPL;
	channel &= 0x1f;
	reg |= 1 << 24;
	reg |= channel << 16;
	return emu_rdptr(sc, 0, reg);
}

static void
emu_clrint(struct sc_info *sc, char channel)
{
	int reg = (channel & 0x20)? CLIPH : CLIPL;
	channel &= 0x1f;
	reg |= 1 << 24;
	reg |= channel << 16;
	emu_wrptr(sc, 0, reg, 1);
}

static void
emu_enaint(struct sc_info *sc, char channel, int enable)
{
	int reg = (channel & 0x20)? CLIEH : CLIEL;
	channel &= 0x1f;
	reg |= 1 << 24;
	reg |= channel << 16;
	emu_wrptr(sc, 0, reg, enable);
}
#endif

/* stuff */
static int
emu_enatimer(struct sc_info *sc, int go)
{
	u_int32_t x;
	if (go) {
		if (sc->timer++ == 0) {
			emu_wr(sc, TIMER, 256, 2);
			x = emu_rd(sc, INTE, 4);
			x |= INTE_INTERVALTIMERENB;
			emu_wr(sc, INTE, x, 4);
		}
	} else {
		sc->timer = 0;
		x = emu_rd(sc, INTE, 4);
		x &= ~INTE_INTERVALTIMERENB;
		emu_wr(sc, INTE, x, 4);
	}
	return 0;
}

static void
emu_enastop(struct sc_info *sc, char channel, int enable)
{
	int reg = (channel & 0x20)? SOLEH : SOLEL;
	channel &= 0x1f;
	reg |= 1 << 24;
	reg |= channel << 16;
	emu_wrptr(sc, 0, reg, enable);
}

static int
emu_recval(int speed) {
	int val;

	val = 0;
	while (val < 7 && speed < adcspeed[val])
		val++;
	return val;
}

static u_int32_t
emu_rate_to_pitch(u_int32_t rate)
{
	static u_int32_t logMagTable[128] = {
		0x00000, 0x02dfc, 0x05b9e, 0x088e6, 0x0b5d6, 0x0e26f, 0x10eb3, 0x13aa2,
		0x1663f, 0x1918a, 0x1bc84, 0x1e72e, 0x2118b, 0x23b9a, 0x2655d, 0x28ed5,
		0x2b803, 0x2e0e8, 0x30985, 0x331db, 0x359eb, 0x381b6, 0x3a93d, 0x3d081,
		0x3f782, 0x41e42, 0x444c1, 0x46b01, 0x49101, 0x4b6c4, 0x4dc49, 0x50191,
		0x5269e, 0x54b6f, 0x57006, 0x59463, 0x5b888, 0x5dc74, 0x60029, 0x623a7,
		0x646ee, 0x66a00, 0x68cdd, 0x6af86, 0x6d1fa, 0x6f43c, 0x7164b, 0x73829,
		0x759d4, 0x77b4f, 0x79c9a, 0x7bdb5, 0x7dea1, 0x7ff5e, 0x81fed, 0x8404e,
		0x86082, 0x88089, 0x8a064, 0x8c014, 0x8df98, 0x8fef1, 0x91e20, 0x93d26,
		0x95c01, 0x97ab4, 0x9993e, 0x9b79f, 0x9d5d9, 0x9f3ec, 0xa11d8, 0xa2f9d,
		0xa4d3c, 0xa6ab5, 0xa8808, 0xaa537, 0xac241, 0xadf26, 0xafbe7, 0xb1885,
		0xb3500, 0xb5157, 0xb6d8c, 0xb899f, 0xba58f, 0xbc15e, 0xbdd0c, 0xbf899,
		0xc1404, 0xc2f50, 0xc4a7b, 0xc6587, 0xc8073, 0xc9b3f, 0xcb5ed, 0xcd07c,
		0xceaec, 0xd053f, 0xd1f73, 0xd398a, 0xd5384, 0xd6d60, 0xd8720, 0xda0c3,
		0xdba4a, 0xdd3b4, 0xded03, 0xe0636, 0xe1f4e, 0xe384a, 0xe512c, 0xe69f3,
		0xe829f, 0xe9b31, 0xeb3a9, 0xecc08, 0xee44c, 0xefc78, 0xf148a, 0xf2c83,
		0xf4463, 0xf5c2a, 0xf73da, 0xf8b71, 0xfa2f0, 0xfba57, 0xfd1a7, 0xfe8df
	};
	static char logSlopeTable[128] = {
		0x5c, 0x5c, 0x5b, 0x5a, 0x5a, 0x59, 0x58, 0x58,
		0x57, 0x56, 0x56, 0x55, 0x55, 0x54, 0x53, 0x53,
		0x52, 0x52, 0x51, 0x51, 0x50, 0x50, 0x4f, 0x4f,
		0x4e, 0x4d, 0x4d, 0x4d, 0x4c, 0x4c, 0x4b, 0x4b,
		0x4a, 0x4a, 0x49, 0x49, 0x48, 0x48, 0x47, 0x47,
		0x47, 0x46, 0x46, 0x45, 0x45, 0x45, 0x44, 0x44,
		0x43, 0x43, 0x43, 0x42, 0x42, 0x42, 0x41, 0x41,
		0x41, 0x40, 0x40, 0x40, 0x3f, 0x3f, 0x3f, 0x3e,
		0x3e, 0x3e, 0x3d, 0x3d, 0x3d, 0x3c, 0x3c, 0x3c,
		0x3b, 0x3b, 0x3b, 0x3b, 0x3a, 0x3a, 0x3a, 0x39,
		0x39, 0x39, 0x39, 0x38, 0x38, 0x38, 0x38, 0x37,
		0x37, 0x37, 0x37, 0x36, 0x36, 0x36, 0x36, 0x35,
		0x35, 0x35, 0x35, 0x34, 0x34, 0x34, 0x34, 0x34,
		0x33, 0x33, 0x33, 0x33, 0x32, 0x32, 0x32, 0x32,
		0x32, 0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x30,
		0x30, 0x30, 0x30, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f
	};
	int i;

	if (rate == 0)
		return 0;	/* Bail out if no leading "1" */
	rate *= 11185;	/* Scale 48000 to 0x20002380 */
	for (i = 31; i > 0; i--) {
		if (rate & 0x80000000) {	/* Detect leading "1" */
			return (((u_int32_t) (i - 15) << 20) +
			       logMagTable[0x7f & (rate >> 24)] +
				      (0x7f & (rate >> 17)) *
			     logSlopeTable[0x7f & (rate >> 24)]);
		}
		rate <<= 1;
	}

	return 0;		/* Should never reach this point */
}

static u_int32_t
emu_rate_to_linearpitch(u_int32_t rate)
{
	rate = (rate << 8) / 375;
	return (rate >> 1) + (rate & 1);
}

static struct emu_voice *
emu_valloc(struct sc_info *sc)
{
	struct emu_voice *v;
	int i;

	v = NULL;
	for (i = 0; i < 64 && sc->voice[i].busy; i++);
	if (i < 64) {
		v = &sc->voice[i];
		v->busy = 1;
	}
	return v;
}

static int
emu_vinit(struct sc_info *sc, struct emu_voice *m, struct emu_voice *s,
	  u_int32_t sz, pcm_channel *c)
{
	void *buf;

	buf = emu_memalloc(sc, sz);
	if (buf == NULL)
		return -1;
	if (c != NULL) {
		c->buffer.buf = buf;
		c->buffer.bufsize = sz;
	}
	m->start = emu_memstart(sc, buf) * EMUPAGESIZE;
	m->end = m->start + sz;
	m->channel = NULL;
	m->speed = 0;
	m->b16 = 0;
	m->stereo = 0;
	m->running = 0;
	m->ismaster = 1;
	m->vol = 0xff;
	m->buf = vtophys(buf);
	m->slave = s;
	if (s != NULL) {
		s->start = m->start;
		s->end = m->end;
		s->channel = NULL;
		s->speed = 0;
		s->b16 = 0;
		s->stereo = 0;
		s->running = 0;
		s->ismaster = 0;
		s->vol = m->vol;
		s->buf = m->buf;
		s->slave = NULL;
	}
	return 0;
}

static void
emu_vsetup(struct sc_pchinfo *ch)
{
	struct emu_voice *v = ch->master;

	if (ch->fmt) {
		v->b16 = (ch->fmt & AFMT_16BIT)? 1 : 0;
		v->stereo = (ch->fmt & AFMT_STEREO)? 1 : 0;
		if (v->slave != NULL) {
			v->slave->b16 = v->b16;
			v->slave->stereo = v->stereo;
		}
	}
	if (ch->spd) {
		v->speed = ch->spd;
		if (v->slave != NULL)
			v->slave->speed = v->speed;
	}
}

static void
emu_vwrite(struct sc_info *sc, struct emu_voice *v)
{
	int s;
	int l, r, x, y;
	u_int32_t sa, ea, start, val, silent_page;

	s = (v->stereo? 1 : 0) + (v->b16? 1 : 0);

	sa = v->start >> s;
	ea = v->end >> s;

	l = r = x = y = v->vol;
	if (v->stereo) {
		l = v->ismaster? l : 0;
		r = v->ismaster? 0 : r;
	}

	emu_wrptr(sc, v->vnum, CPF, v->stereo? CPF_STEREO_MASK : 0);
	val = v->stereo? 28 : 30;
	val *= v->b16? 1 : 2;
	start = sa + val;

	emu_wrptr(sc, v->vnum, FXRT, 0xd01c0000);

	emu_wrptr(sc, v->vnum, PTRX, (x << 8) | r);
	emu_wrptr(sc, v->vnum, DSL, ea | (y << 24));
	emu_wrptr(sc, v->vnum, PSST, sa | (l << 24));
	emu_wrptr(sc, v->vnum, CCCA, start | (v->b16? 0 : CCCA_8BITSELECT));

	emu_wrptr(sc, v->vnum, Z1, 0);
	emu_wrptr(sc, v->vnum, Z2, 0);

	silent_page = ((u_int32_t)vtophys(sc->mem.silent_page) << 1) | MAP_PTI_MASK;
	emu_wrptr(sc, v->vnum, MAPA, silent_page);
	emu_wrptr(sc, v->vnum, MAPB, silent_page);

	emu_wrptr(sc, v->vnum, CVCF, CVCF_CURRENTFILTER_MASK);
	emu_wrptr(sc, v->vnum, VTFT, VTFT_FILTERTARGET_MASK);
	emu_wrptr(sc, v->vnum, ATKHLDM, 0);
	emu_wrptr(sc, v->vnum, DCYSUSM, DCYSUSM_DECAYTIME_MASK);
	emu_wrptr(sc, v->vnum, LFOVAL1, 0x8000);
	emu_wrptr(sc, v->vnum, LFOVAL2, 0x8000);
	emu_wrptr(sc, v->vnum, FMMOD, 0);
	emu_wrptr(sc, v->vnum, TREMFRQ, 0);
	emu_wrptr(sc, v->vnum, FM2FRQ2, 0);
	emu_wrptr(sc, v->vnum, ENVVAL, 0x8000);

	emu_wrptr(sc, v->vnum, ATKHLDV, ATKHLDV_HOLDTIME_MASK | ATKHLDV_ATTACKTIME_MASK);
	emu_wrptr(sc, v->vnum, ENVVOL, 0x8000);

	emu_wrptr(sc, v->vnum, PEFE_FILTERAMOUNT, 0x7f);
	emu_wrptr(sc, v->vnum, PEFE_PITCHAMOUNT, 0);

	if (v->slave != NULL)
		emu_vwrite(sc, v->slave);
}

static void
emu_vtrigger(struct sc_info *sc, struct emu_voice *v, int go)
{
	u_int32_t pitch_target, initial_pitch;
	u_int32_t cra, cs, ccis;
	u_int32_t sample, i;

	if (go) {
		cra = 64;
		cs = v->stereo? 4 : 2;
		ccis = v->stereo? 28 : 30;
		ccis *= v->b16? 1 : 2;
		sample = v->b16? 0x00000000 : 0x80808080;

		for (i = 0; i < cs; i++)
			emu_wrptr(sc, v->vnum, CD0 + i, sample);
		emu_wrptr(sc, v->vnum, CCR_CACHEINVALIDSIZE, 0);
		emu_wrptr(sc, v->vnum, CCR_READADDRESS, cra);
		emu_wrptr(sc, v->vnum, CCR_CACHEINVALIDSIZE, ccis);

		emu_wrptr(sc, v->vnum, IFATN, 0xff00);
		emu_wrptr(sc, v->vnum, VTFT, 0xffffffff);
		emu_wrptr(sc, v->vnum, CVCF, 0xffffffff);
		emu_wrptr(sc, v->vnum, DCYSUSV, 0x00007f7f);
		emu_enastop(sc, v->vnum, 0);

		pitch_target = emu_rate_to_linearpitch(v->speed);
		initial_pitch = emu_rate_to_pitch(v->speed) >> 8;
		emu_wrptr(sc, v->vnum, PTRX_PITCHTARGET, pitch_target);
		emu_wrptr(sc, v->vnum, CPF_CURRENTPITCH, pitch_target);
		emu_wrptr(sc, v->vnum, IP, initial_pitch);
	} else {
		emu_wrptr(sc, v->vnum, PTRX_PITCHTARGET, 0);
		emu_wrptr(sc, v->vnum, CPF_CURRENTPITCH, 0);
		emu_wrptr(sc, v->vnum, IFATN, 0xffff);
		emu_wrptr(sc, v->vnum, VTFT, 0x0000ffff);
		emu_wrptr(sc, v->vnum, CVCF, 0x0000ffff);
		emu_wrptr(sc, v->vnum, IP, 0);
		emu_enastop(sc, v->vnum, 1);
	}
	if (v->slave != NULL)
		emu_vtrigger(sc, v->slave, go);
}

static int
emu_vpos(struct sc_info *sc, struct emu_voice *v)
{
	int s, ptr;

	s = (v->b16? 1 : 0) + (v->stereo? 1 : 0);
	ptr = (emu_rdptr(sc, v->vnum, CCCA_CURRADDR) - (v->start >> s)) << s;
	return ptr & ~0x0000001f;
}

#ifdef EMUDEBUG
static void
emu_vdump(struct sc_info *sc, struct emu_voice *v)
{
	char *regname[] = { "cpf", "ptrx", "cvcf", "vtft", "z2", "z1", "psst", "dsl",
			    "ccca", "ccr", "clp", "fxrt", "mapa", "mapb", NULL, NULL,
			    "envvol", "atkhldv", "dcysusv", "lfoval1",
			    "envval", "atkhldm", "dcysusm", "lfoval2",
			    "ip", "ifatn", "pefe", "fmmod", "tremfrq", "fmfrq2",
			    "tempenv" };
	int i, x;

	printf("voice number %d\n", v->vnum);
	for (i = 0, x = 0; i <= 0x1e; i++) {
		if (regname[i] == NULL)
			continue;
		printf("%s\t[%08x]", regname[i], emu_rdptr(sc, v->vnum, i));
		printf("%s", (x == 2)? "\n" : "\t");
		x++;
		if (x > 2)
			x = 0;
	}
	printf("\n\n");
}
#endif

/* channel interface */
void *
emupchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_pchinfo *ch;

	KASSERT(dir == PCMDIR_PLAY, ("emupchan_init: bad direction"));
	ch = &sc->pch[sc->pnum++];
	ch->buffer = b;
	ch->parent = sc;
	ch->channel = c;
	ch->master = emu_valloc(sc);
	ch->slave = emu_valloc(sc);
	if (emu_vinit(sc, ch->master, ch->slave, EMU_BUFFSIZE, ch->channel))
		return NULL;
	else
		return ch;
}

static int
emupchan_setdir(void *data, int dir)
{
	return 0;
}

static int
emupchan_setformat(void *data, u_int32_t format)
{
	struct sc_pchinfo *ch = data;

	ch->fmt = format;
	return 0;
}

static int
emupchan_setspeed(void *data, u_int32_t speed)
{
	struct sc_pchinfo *ch = data;

	ch->spd = speed;
	return ch->spd;
}

static int
emupchan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
emupchan_trigger(void *data, int go)
{
	struct sc_pchinfo *ch = data;
	struct sc_info *sc  = ch->parent;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	if (go == PCMTRIG_START) {
		emu_vsetup(ch);
		emu_vwrite(sc, ch->master);
		emu_enatimer(sc, 1);
#ifdef EMUDEBUG
		printf("start [%d bit, %s, %d hz]\n",
			ch->master->b16? 16 : 8,
			ch->master->stereo? "stereo" : "mono",
			ch->master->speed);
		emu_vdump(sc, ch->master);
		emu_vdump(sc, ch->slave);
#endif
	}
	ch->run = (go == PCMTRIG_START)? 1 : 0;
	emu_vtrigger(sc, ch->master, ch->run);
	return 0;
}

static int
emupchan_getptr(void *data)
{
	struct sc_pchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	return emu_vpos(sc, ch->master);
}

static pcmchan_caps *
emupchan_getcaps(void *data)
{
	return &emu_playcaps;
}

/* channel interface */
static void *
emurchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_rchinfo *ch;

	KASSERT(dir == PCMDIR_REC, ("emurchan_init: bad direction"));
	ch = &sc->rch[sc->rnum];
	ch->buffer = b;
	ch->buffer->bufsize = EMU_BUFFSIZE;
	ch->parent = sc;
	ch->channel = c;
	ch->fmt = AFMT_U8;
	ch->spd = 8000;
	ch->num = sc->rnum;
	switch(sc->rnum) {
	case 0:
		ch->idxreg = ADCIDX;
		ch->basereg = ADCBA;
		ch->sizereg = ADCBS;
		ch->setupreg = ADCCR;
		ch->irqmask = INTE_ADCBUFENABLE;
		break;

	case 1:
		ch->idxreg = MICIDX;
		ch->basereg = MICBA;
		ch->sizereg = MICBS;
		ch->setupreg = 0;
		ch->irqmask = INTE_MICBUFENABLE;
		break;

	case 2:
		ch->idxreg = FXIDX;
		ch->basereg = FXBA;
		ch->sizereg = FXBS;
		ch->setupreg = FXWC;
		ch->irqmask = INTE_EFXBUFENABLE;
		break;
	}
	sc->rnum++;
	if (chn_allocbuf(ch->buffer, sc->parent_dmat) == -1)
		return NULL;
	else {
		emu_wrptr(sc, 0, ch->basereg, vtophys(ch->buffer->buf));
		emu_wrptr(sc, 0, ch->sizereg, 0); /* off */
		return ch;
	}
}

static int
emurchan_setdir(void *data, int dir)
{
	return 0;
}

static int
emurchan_setformat(void *data, u_int32_t format)
{
	struct sc_rchinfo *ch = data;

	ch->fmt = format;
	return 0;
}

static int
emurchan_setspeed(void *data, u_int32_t speed)
{
	struct sc_rchinfo *ch = data;

	if (ch->num == 0)
		speed = adcspeed[emu_recval(speed)];
	if (ch->num == 1)
		speed = 8000;
	if (ch->num == 2)
		speed = 48000;
	ch->spd = speed;
	return ch->spd;
}

static int
emurchan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

/* semantic note: must start at beginning of buffer */
static int
emurchan_trigger(void *data, int go)
{
	struct sc_rchinfo *ch = data;
	struct sc_info *sc = ch->parent;
	u_int32_t val;

	switch(go) {
	case PCMTRIG_START:
		ch->run = 1;
		emu_wrptr(sc, 0, ch->sizereg, ADCBS_BUFSIZE_4096);
		if (ch->num == 0) {
			val = ADCCR_LCHANENABLE;
			if (ch->fmt & AFMT_STEREO)
				val |= ADCCR_RCHANENABLE;
			val |= emu_recval(ch->spd);
			emu_wrptr(sc, 0, ch->setupreg, val);
		}
		val = emu_rd(sc, INTE, 4);
		val |= ch->irqmask;
		emu_wr(sc, INTE, val, 4);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		ch->run = 0;
		emu_wrptr(sc, 0, ch->sizereg, 0);
		if (ch->setupreg)
			emu_wrptr(sc, 0, ch->setupreg, 0);
		val = emu_rd(sc, INTE, 4);
		val &= ~ch->irqmask;
		emu_wr(sc, INTE, val, 4);
		break;

	case PCMTRIG_EMLDMAWR:
	case PCMTRIG_EMLDMARD:
	default:
		break;
	}

	return 0;
}

static int
emurchan_getptr(void *data)
{
	struct sc_rchinfo *ch = data;
	struct sc_info *sc = ch->parent;

	return emu_rdptr(sc, 0, ch->idxreg) & 0x0000ffff;
}

static pcmchan_caps *
emurchan_getcaps(void *data)
{
	struct sc_rchinfo *ch = data;

	return &emu_reccaps[ch->num];
}

/* The interrupt handler */
static void
emu_intr(void *p)
{
	struct sc_info *sc = (struct sc_info *)p;
	u_int32_t stat, ack, i, x;

	while (1) {
		stat = emu_rd(sc, IPR, 4);
		if (stat == 0)
			break;
		ack = 0;

		/* process irq */
		if (stat & IPR_INTERVALTIMER) {
			ack |= IPR_INTERVALTIMER;
			x = 0;
			for (i = 0; i < EMU_CHANS; i++) {
				if (sc->pch[i].run) {
					x = 1;
					chn_intr(sc->pch[i].channel);
				}
			}
			if (x == 0)
				emu_enatimer(sc, 0);
		}


		if (stat & (IPR_ADCBUFFULL | IPR_ADCBUFHALFFULL)) {
			ack |= stat & (IPR_ADCBUFFULL | IPR_ADCBUFHALFFULL);
			if (sc->rch[0].channel)
				chn_intr(sc->rch[0].channel);
		}
		if (stat & (IPR_MICBUFFULL | IPR_MICBUFHALFFULL)) {
			ack |= stat & (IPR_MICBUFFULL | IPR_MICBUFHALFFULL);
			if (sc->rch[1].channel)
				chn_intr(sc->rch[1].channel);
		}
		if (stat & (IPR_EFXBUFFULL | IPR_EFXBUFHALFFULL)) {
			ack |= stat & (IPR_EFXBUFFULL | IPR_EFXBUFHALFFULL);
			if (sc->rch[2].channel)
				chn_intr(sc->rch[2].channel);
		}
		if (stat & IPR_PCIERROR) {
			ack |= IPR_PCIERROR;
			device_printf(sc->dev, "pci error\n");
			/* we still get an nmi with ecc ram even if we ack this */
		}
		if (stat & IPR_SAMPLERATETRACKER) {
			ack |= IPR_SAMPLERATETRACKER;
			device_printf(sc->dev, "sample rate tracker lock status change\n");
		}

		if (stat & ~ack)
			device_printf(sc->dev, "dodgy irq: %x (harmless)\n", stat & ~ack);

		emu_wr(sc, IPR, stat, 4);
	}
}

/* -------------------------------------------------------------------- */

static void
emu_setmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	void **phys = arg;

	*phys = error? 0 : (void *)segs->ds_addr;

	if (bootverbose) {
		printf("emu: setmap (%lx, %lx), nseg=%d, error=%d\n",
		       (unsigned long)segs->ds_addr, (unsigned long)segs->ds_len,
		       nseg, error);
	}
}

static void *
emu_malloc(struct sc_info *sc, u_int32_t sz)
{
	void *buf, *phys = 0;
	bus_dmamap_t map;

	if (bus_dmamem_alloc(sc->parent_dmat, &buf, BUS_DMA_NOWAIT, &map))
		return NULL;
	if (bus_dmamap_load(sc->parent_dmat, map, buf, sz, emu_setmap, &phys, 0)
	    || !phys)
		return NULL;
	return buf;
}

static void
emu_free(struct sc_info *sc, void *buf)
{
	bus_dmamem_free(sc->parent_dmat, buf, NULL);
}

static void *
emu_memalloc(struct sc_info *sc, u_int32_t sz)
{
	u_int32_t blksz, start, idx, ofs, tmp, found;
	struct emu_mem *mem = &sc->mem;
	struct emu_memblk *blk;
	void *buf;

	blksz = sz / EMUPAGESIZE;
	if (sz > (blksz * EMUPAGESIZE))
		blksz++;
	/* find a free block in the bitmap */
	found = 0;
	start = 1;
	while (!found && start + blksz < MAXPAGES) {
		found = 1;
		for (idx = start; idx < start + blksz; idx++)
			if (mem->bmap[idx >> 3] & (1 << (idx & 7)))
				found = 0;
		if (!found)
			start++;
	}
	if (!found)
		return NULL;
	blk = malloc(sizeof(*blk), M_DEVBUF, M_NOWAIT);
	if (blk == NULL)
		return NULL;
	buf = emu_malloc(sc, sz);
	if (buf == NULL) {
		free(blk, M_DEVBUF);
		return NULL;
	}
	blk->buf = buf;
	blk->pte_start = start;
	blk->pte_size = blksz;
	/* printf("buf %p, pte_start %d, pte_size %d\n", blk->buf, blk->pte_start, blk->pte_size); */
	ofs = 0;
	for (idx = start; idx < start + blksz; idx++) {
		mem->bmap[idx >> 3] |= 1 << (idx & 7);
		tmp = (u_int32_t)vtophys((u_int8_t *)buf + ofs);
		/* printf("pte[%d] -> %x phys, %x virt\n", idx, tmp, ((u_int32_t)buf) + ofs); */
		mem->ptb_pages[idx] = (tmp << 1) | idx;
		ofs += EMUPAGESIZE;
	}
	SLIST_INSERT_HEAD(&mem->blocks, blk, link);
	return buf;
}

#ifdef notyet
static int
emu_memfree(struct sc_info *sc, void *buf)
{
	u_int32_t idx, tmp;
	struct emu_mem *mem = &sc->mem;
	struct emu_memblk *blk, *i;

	blk = NULL;
	SLIST_FOREACH(i, &mem->blocks, link) {
		if (i->buf == buf)
			blk = i;
	}
	if (blk == NULL)
		return EINVAL;
	SLIST_REMOVE(&mem->blocks, blk, emu_memblk, link);
	emu_free(sc, buf);
	tmp = (u_int32_t)vtophys(sc->mem.silent_page) << 1;
	for (idx = blk->pte_start; idx < blk->pte_start + blk->pte_size; idx++) {
		mem->bmap[idx >> 3] &= ~(1 << (idx & 7));
		mem->ptb_pages[idx] = tmp | idx;
	}
	free(blk, M_DEVBUF);
	return 0;
}
#endif

static int
emu_memstart(struct sc_info *sc, void *buf)
{
	struct emu_mem *mem = &sc->mem;
	struct emu_memblk *blk, *i;

	blk = NULL;
	SLIST_FOREACH(i, &mem->blocks, link) {
		if (i->buf == buf)
			blk = i;
	}
	if (blk == NULL)
		return -EINVAL;
	return blk->pte_start;
}

static void
emu_addefxop(struct sc_info *sc, int op, int z, int w, int x, int y, u_int32_t *pc)
{
	emu_wrefx(sc, (*pc) * 2, (x << 10) | y);
	emu_wrefx(sc, (*pc) * 2 + 1, (op << 20) | (z << 10) | w);
	(*pc)++;
}

static void
emu_initefx(struct sc_info *sc)
{
	int i;
	u_int32_t pc = 16;

	for (i = 0; i < 512; i++) {
		emu_wrefx(sc, i * 2, 0x10040);
		emu_wrefx(sc, i * 2 + 1, 0x610040);
	}

	for (i = 0; i < 256; i++)
		emu_wrptr(sc, 0, FXGPREGBASE + i, 0);

	/* FX-8010 DSP Registers:
	   FX Bus
	     0x000-0x00f : 16 registers
	   Input
	     0x010/0x011 : AC97 Codec (l/r)
	     0x012/0x013 : ADC, S/PDIF (l/r)
	     0x014/0x015 : Mic(left), Zoom (l/r)
	     0x016/0x017 : APS S/PDIF?? (l/r)
	   Output
	     0x020/0x021 : AC97 Output (l/r)
	     0x022/0x023 : TOS link out (l/r)
	     0x024/0x025 : ??? (l/r)
	     0x026/0x027 : LiveDrive Headphone (l/r)
	     0x028/0x029 : Rear Channel (l/r)
	     0x02a/0x02b : ADC Recording Buffer (l/r)
	   Constants
	     0x040 - 0x044 = 0 - 4
	     0x045 = 0x8, 0x046 = 0x10, 0x047 = 0x20
	     0x048 = 0x100, 0x049 = 0x10000, 0x04a = 0x80000
	     0x04b = 0x10000000, 0x04c = 0x20000000, 0x04d = 0x40000000
	     0x04e = 0x80000000, 0x04f = 0x7fffffff
	   Temporary Values
	     0x056 : Accumulator
	     0x058 : Noise source?
	     0x059 : Noise source?
	   General Purpose Registers
	     0x100 - 0x1ff
	   Tank Memory Data Registers
	     0x200 - 0x2ff
	   Tank Memory Address Registers
	     0x300 - 0x3ff
	     */

	/* Operators:
	   0 : z := w + (x * y >> 31)
	   4 : z := w + x * y
	   6 : z := w + x + y
	   */

	/* Routing - this will be configurable in later version */

	/* GPR[0/1] = FX * 4 + SPDIF-in */
	emu_addefxop(sc, 4, 0x100, 0x12, 0, 0x44, &pc);
	emu_addefxop(sc, 4, 0x101, 0x13, 1, 0x44, &pc);
	/* GPR[0/1] += APS-input */
	emu_addefxop(sc, 6, 0x100, 0x100, 0x40, sc->APS ? 0x16 : 0x40, &pc);
	emu_addefxop(sc, 6, 0x101, 0x101, 0x40, sc->APS ? 0x17 : 0x40, &pc);
	/* FrontOut (AC97) = GPR[0/1] */
	emu_addefxop(sc, 6, 0x20, 0x40, 0x40, 0x100, &pc);
	emu_addefxop(sc, 6, 0x21, 0x40, 0x41, 0x101, &pc);
	/* RearOut = (GPR[0/1] * RearVolume) >> 31 */
	/*   RearVolume = GRP[0x10/0x11] */
	emu_addefxop(sc, 0, 0x28, 0x40, 0x110, 0x100, &pc);
	emu_addefxop(sc, 0, 0x29, 0x40, 0x111, 0x101, &pc);
	/* TOS out = GPR[0/1] */
	emu_addefxop(sc, 6, 0x22, 0x40, 0x40, 0x100, &pc);
	emu_addefxop(sc, 6, 0x23, 0x40, 0x40, 0x101, &pc);
	/* Mute Out2 */
	emu_addefxop(sc, 6, 0x24, 0x40, 0x40, 0x40, &pc);
	emu_addefxop(sc, 6, 0x25, 0x40, 0x40, 0x40, &pc);
	/* Mute Out3 */
	emu_addefxop(sc, 6, 0x26, 0x40, 0x40, 0x40, &pc);
	emu_addefxop(sc, 6, 0x27, 0x40, 0x40, 0x40, &pc);
	/* Input0 (AC97) -> Record */
	emu_addefxop(sc, 6, 0x2a, 0x40, 0x40, 0x10, &pc);
	emu_addefxop(sc, 6, 0x2b, 0x40, 0x40, 0x11, &pc);

	emu_wrptr(sc, 0, DBG, 0);
}

/* Probe and attach the card */
static int
emu_init(struct sc_info *sc)
{
	u_int32_t spcs, ch, tmp, i;

   	/* disable audio and lock cache */
	emu_wr(sc, HCFG, HCFG_LOCKSOUNDCACHE | HCFG_LOCKTANKCACHE | HCFG_MUTEBUTTONENABLE, 4);

	/* reset recording buffers */
	emu_wrptr(sc, 0, MICBS, ADCBS_BUFSIZE_NONE);
	emu_wrptr(sc, 0, MICBA, 0);
	emu_wrptr(sc, 0, FXBS, ADCBS_BUFSIZE_NONE);
	emu_wrptr(sc, 0, FXBA, 0);
	emu_wrptr(sc, 0, ADCBS, ADCBS_BUFSIZE_NONE);
	emu_wrptr(sc, 0, ADCBA, 0);

	/* disable channel interrupt */
	emu_wr(sc, INTE, INTE_INTERVALTIMERENB | INTE_SAMPLERATETRACKER | INTE_PCIERRORENABLE, 4);
	emu_wrptr(sc, 0, CLIEL, 0);
	emu_wrptr(sc, 0, CLIEH, 0);
	emu_wrptr(sc, 0, SOLEL, 0);
	emu_wrptr(sc, 0, SOLEH, 0);

	/* init envelope engine */
	for (ch = 0; ch < NUM_G; ch++) {
		emu_wrptr(sc, ch, DCYSUSV, ENV_OFF);
		emu_wrptr(sc, ch, IP, 0);
		emu_wrptr(sc, ch, VTFT, 0xffff);
		emu_wrptr(sc, ch, CVCF, 0xffff);
		emu_wrptr(sc, ch, PTRX, 0);
		emu_wrptr(sc, ch, CPF, 0);
		emu_wrptr(sc, ch, CCR, 0);

		emu_wrptr(sc, ch, PSST, 0);
		emu_wrptr(sc, ch, DSL, 0x10);
		emu_wrptr(sc, ch, CCCA, 0);
		emu_wrptr(sc, ch, Z1, 0);
		emu_wrptr(sc, ch, Z2, 0);
		emu_wrptr(sc, ch, FXRT, 0xd01c0000);

		emu_wrptr(sc, ch, ATKHLDM, 0);
		emu_wrptr(sc, ch, DCYSUSM, 0);
		emu_wrptr(sc, ch, IFATN, 0xffff);
		emu_wrptr(sc, ch, PEFE, 0);
		emu_wrptr(sc, ch, FMMOD, 0);
		emu_wrptr(sc, ch, TREMFRQ, 24);	/* 1 Hz */
		emu_wrptr(sc, ch, FM2FRQ2, 24);	/* 1 Hz */
		emu_wrptr(sc, ch, TEMPENV, 0);

		/*** these are last so OFF prevents writing ***/
		emu_wrptr(sc, ch, LFOVAL2, 0);
		emu_wrptr(sc, ch, LFOVAL1, 0);
		emu_wrptr(sc, ch, ATKHLDV, 0);
		emu_wrptr(sc, ch, ENVVOL, 0);
		emu_wrptr(sc, ch, ENVVAL, 0);

		sc->voice[ch].vnum = ch;
		sc->voice[ch].slave = NULL;
		sc->voice[ch].busy = 0;
		sc->voice[ch].ismaster = 0;
		sc->voice[ch].running = 0;
		sc->voice[ch].b16 = 0;
		sc->voice[ch].stereo = 0;
		sc->voice[ch].speed = 0;
		sc->voice[ch].start = 0;
		sc->voice[ch].end = 0;
		sc->voice[ch].channel = NULL;
       }
       sc->pnum = sc->rnum = 0;

	/*
	 *  Init to 0x02109204 :
	 *  Clock accuracy    = 0     (1000ppm)
	 *  Sample Rate       = 2     (48kHz)
	 *  Audio Channel     = 1     (Left of 2)
	 *  Source Number     = 0     (Unspecified)
	 *  Generation Status = 1     (Original for Cat Code 12)
	 *  Cat Code          = 12    (Digital Signal Mixer)
	 *  Mode              = 0     (Mode 0)
	 *  Emphasis          = 0     (None)
	 *  CP                = 1     (Copyright unasserted)
	 *  AN                = 0     (Audio data)
	 *  P                 = 0     (Consumer)
	 */
	spcs = SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
	       SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
	       SPCS_GENERATIONSTATUS | 0x00001200 | 0x00000000 |
	       SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT;
	emu_wrptr(sc, 0, SPCS0, spcs);
	emu_wrptr(sc, 0, SPCS1, spcs);
	emu_wrptr(sc, 0, SPCS2, spcs);

	emu_initefx(sc);

	SLIST_INIT(&sc->mem.blocks);
	sc->mem.ptb_pages = emu_malloc(sc, MAXPAGES * sizeof(u_int32_t));
	if (sc->mem.ptb_pages == NULL)
		return -1;

	sc->mem.silent_page = emu_malloc(sc, EMUPAGESIZE);
	if (sc->mem.silent_page == NULL) {
		emu_free(sc, sc->mem.ptb_pages);
		return -1;
	}
	/* Clear page with silence & setup all pointers to this page */
	bzero(sc->mem.silent_page, EMUPAGESIZE);
	tmp = (u_int32_t)vtophys(sc->mem.silent_page) << 1;
	for (i = 0; i < MAXPAGES; i++)
		sc->mem.ptb_pages[i] = tmp | i;

	emu_wrptr(sc, 0, PTB, vtophys(sc->mem.ptb_pages));
	emu_wrptr(sc, 0, TCB, 0);	/* taken from original driver */
	emu_wrptr(sc, 0, TCBS, 0);	/* taken from original driver */

	for (ch = 0; ch < NUM_G; ch++) {
		emu_wrptr(sc, ch, MAPA, tmp | MAP_PTI_MASK);
		emu_wrptr(sc, ch, MAPB, tmp | MAP_PTI_MASK);
	}

	/* emu_memalloc(sc, EMUPAGESIZE); */
	/*
	 *  Hokay, now enable the AUD bit
	 *   Enable Audio = 1
	 *   Mute Disable Audio = 0
	 *   Lock Tank Memory = 1
	 *   Lock Sound Memory = 0
	 *   Auto Mute = 1
	 */
	tmp = HCFG_AUDIOENABLE | HCFG_LOCKTANKCACHE | HCFG_AUTOMUTE;
	if (sc->rev >= 6)
		tmp |= HCFG_JOYENABLE;
	emu_wr(sc, HCFG, tmp, 4);

	/* TOSLink detection */
	sc->tos_link = 0;
	tmp = emu_rd(sc, HCFG, 4);
	if (tmp & (HCFG_GPINPUT0 | HCFG_GPINPUT1)) {
		emu_wr(sc, HCFG, tmp | 0x800, 4);
		DELAY(50);
		if (tmp != (emu_rd(sc, HCFG, 4) & ~0x800)) {
			sc->tos_link = 1;
			emu_wr(sc, HCFG, tmp, 4);
		}
	}

	return 0;
}

static int
emu_pci_probe(device_t dev)
{
	char *s = NULL;

	switch (pci_get_devid(dev)) {
	case EMU10K1_PCI_ID:
		s = "Creative EMU10K1";
		break;
	}

	if (s) device_set_desc(dev, s);
	return s? 0 : ENXIO;
}

static int
emu_pci_attach(device_t dev)
{
	snddev_info    *d;
	u_int32_t	data;
	struct sc_info *sc;
	struct ac97_info *codec;
	int		i, mapped;
	char 		status[SND_STATUSLEN];

	d = device_get_softc(dev);
	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	bzero(sc, sizeof(*sc));
	sc->dev = dev;
	sc->type = pci_get_devid(dev);
	sc->rev = pci_get_revid(dev);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	mapped = 0;
	for (i = 0; (mapped == 0) && (i < PCI_MAXMAPS_0); i++) {
		sc->regid = PCIR_MAPS + i*4;
		sc->regtype = SYS_RES_MEMORY;
		sc->reg = bus_alloc_resource(dev, sc->regtype, &sc->regid,
					     0, ~0, 1, RF_ACTIVE);
		if (!sc->reg) {
			sc->regtype = SYS_RES_IOPORT;
			sc->reg = bus_alloc_resource(dev, sc->regtype,
						     &sc->regid, 0, ~0, 1,
						     RF_ACTIVE);
		}
		if (sc->reg) {
			sc->st = rman_get_bustag(sc->reg);
			sc->sh = rman_get_bushandle(sc->reg);
			mapped++;
		}
	}

	if (mapped == 0) {
		device_printf(dev, "unable to map register space\n");
		goto bad;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/1 << 31, /* can only access 0-2gb */
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/262144, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &sc->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	if (emu_init(sc) == -1) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}

	codec = ac97_create(dev, sc, NULL, emu_rdcd, emu_wrcd);
	if (codec == NULL) goto bad;
	if (mixer_init(d, &ac97_mixer, codec) == -1) goto bad;

	sc->irqid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqid,
				 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->irq ||
	    bus_setup_intr(dev, sc->irq, INTR_TYPE_TTY, emu_intr, sc, &sc->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	snprintf(status, SND_STATUSLEN, "at %s 0x%lx irq %ld",
		 (sc->regtype == SYS_RES_IOPORT)? "io" : "memory",
		 rman_get_start(sc->reg), rman_get_start(sc->irq));

	if (pcm_register(dev, sc, EMU_CHANS, 3)) goto bad;
	for (i = 0; i < EMU_CHANS; i++)
		pcm_addchan(dev, PCMDIR_PLAY, &emu_chantemplate, sc);
	for (i = 0; i < 3; i++)
		pcm_addchan(dev, PCMDIR_REC, &emur_chantemplate, sc);

	pcm_setstatus(dev, status);

	return 0;

bad:
	if (sc->reg) bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	if (sc->ih) bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->irq) bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	free(sc, M_DEVBUF);
	return ENXIO;
}

/* add suspend, resume, unload */
static device_method_t emu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		emu_pci_probe),
	DEVMETHOD(device_attach,	emu_pci_attach),

	{ 0, 0 }
};

static driver_t emu_driver = {
	"pcm",
	emu_methods,
	sizeof(snddev_info),
};

static devclass_t pcm_devclass;

DRIVER_MODULE(snd_emu10k1, pci, emu_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_emu10k1, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_emu10k1, 1);

/* dummy driver to silence the joystick device */
static int
emujoy_pci_probe(device_t dev)
{
	char *s = NULL;

	switch (pci_get_devid(dev)) {
	case 0x70021102:
		s = "Creative EMU10K1 Joystick";
		device_quiet(dev);
		break;
	}

	if (s) device_set_desc(dev, s);
	return s? 0 : ENXIO;
}

static int
emujoy_pci_attach(device_t dev)
{
	return 0;
}

static device_method_t emujoy_methods[] = {
	DEVMETHOD(device_probe,		emujoy_pci_probe),
	DEVMETHOD(device_attach,	emujoy_pci_attach),

	{ 0, 0 }
};

static driver_t emujoy_driver = {
	"emujoy",
	emujoy_methods,
	8,
};

static devclass_t emujoy_devclass;

DRIVER_MODULE(emujoy, pci, emujoy_driver, emujoy_devclass, 0, 0);

