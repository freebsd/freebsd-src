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
	int b16:1, stereo:1, busy:1, running:1, ismaster:1, istracker:1;
	int speed;
	int start, end, vol;
	u_int32_t buf;
	struct emu_voice *slave, *tracker;
	pcm_channel *channel;
};

struct sc_info;

/* channel registers */
struct sc_chinfo {
	int spd, dir, fmt;
	struct emu_voice *master, *slave, *tracker;
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

	struct emu_mem mem;
	struct emu_voice voice[64];
	struct sc_chinfo pch, rch;
};

/* -------------------------------------------------------------------- */

/*
 * prototypes
 */

/* channel interface */
static void *emuchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int emuchan_setdir(void *data, int dir);
static int emuchan_setformat(void *data, u_int32_t format);
static int emuchan_setspeed(void *data, u_int32_t speed);
static int emuchan_setblocksize(void *data, u_int32_t blocksize);
static int emuchan_trigger(void *data, int go);
static int emuchan_getptr(void *data);
static pcmchan_caps *emuchan_getcaps(void *data);

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

static pcmchan_caps emu_reccaps = {
	4000, 48000,
	AFMT_STEREO | AFMT_U8 | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcmchan_caps emu_playcaps = {
	4000, 48000,
	AFMT_STEREO | AFMT_U8 | AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE
};

static pcm_channel emu_chantemplate = {
	emuchan_init,
	emuchan_setdir,
	emuchan_setformat,
	emuchan_setspeed,
	emuchan_setblocksize,
	emuchan_trigger,
	emuchan_getptr,
	emuchan_getcaps,
};

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

static void
emu_enastop(struct sc_info *sc, char channel, int enable)
{
	int reg = (channel & 0x20)? SOLEH : SOLEL;
	channel &= 0x1f;
	reg |= 1 << 24;
	reg |= channel << 16;
	emu_wrptr(sc, 0, reg, enable);
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
emu_vinit(struct sc_info *sc, struct emu_voice *m, struct emu_voice *s, struct emu_voice *t,
	  u_int32_t sz, pcm_channel *c)
{
	void *buf;

	buf = emu_memalloc(sc, sz);
	if (buf == NULL)
		return -1;
	m->start = emu_memstart(sc, buf) * EMUPAGESIZE;
	m->end = m->start + sz;
	m->channel = NULL;
	m->speed = 0;
	m->b16 = 0;
	m->stereo = 0;
	m->running = 0;
	m->ismaster = 1;
	m->istracker = 0;
	m->vol = 0xff;
	m->buf = vtophys(buf);
	m->slave = s;
	m->tracker = t;
	if (s != NULL) {
		s->start = m->start;
		s->end = m->end;
		s->channel = NULL;
		s->speed = 0;
		s->b16 = 0;
		s->stereo = 0;
		s->running = 0;
		s->ismaster = 0;
		s->istracker = 0;
		s->vol = m->vol;
		s->buf = m->buf;
		s->slave = NULL;
		s->tracker = NULL;
	}
	if (t != NULL) {
		t->start = m->start;
		t->end = t->start + sz / 2;
		t->channel = c;
		t->speed = 0;
		t->b16 = 0;
		t->stereo = 0;
		t->running = 0;
		t->ismaster = 0;
		t->istracker = 1;
		t->vol = 0;
		t->buf = m->buf;
		t->slave = NULL;
		t->tracker = NULL;
	}
	if (c != NULL) {
		c->buffer.buf = buf;
		c->buffer.bufsize = sz;
	}
	return 0;
}

static void
emu_vsetup(struct sc_chinfo *ch)
{
	struct emu_voice *v = ch->master;

	if (ch->fmt) {
		v->b16 = (ch->fmt & AFMT_16BIT)? 1 : 0;
		v->stereo = (ch->fmt & AFMT_STEREO)? 1 : 0;
		if (v->slave != NULL) {
			v->slave->b16 = v->b16;
			v->slave->stereo = v->stereo;
		}
		if (v->tracker != NULL) {
			v->tracker->b16 = v->b16;
			v->tracker->stereo = v->stereo;
		}
	}
	if (ch->spd) {
		v->speed = ch->spd;
		if (v->slave != NULL)
			v->slave->speed = v->speed;
		if (v->tracker != NULL)
			v->tracker->speed = v->speed;
	}
}

static void
emu_vwrite(struct sc_info *sc, struct emu_voice *v)
{
	int s, l, r, p, x;
	u_int32_t sa, ea, start = 0, val = 0, v2 = 0, sample, silent_page, i;

	s = (v->stereo? 1 : 0) + (v->b16? 1 : 0);
	sa = v->start >> s;
	ea = v->end >> s;
	l = r = x = v->vol;
	if (v->stereo) {
		l = v->ismaster? l : 0;
		r = v->ismaster? 0 : r;
	}
	p = emu_rate_to_pitch(v->speed) >> 8;
	sample = v->b16? 0 : 0x80808080;

   	emu_wrptr(sc, v->vnum, DCYSUSV, ENV_OFF);
	emu_wrptr(sc, v->vnum, VTFT, VTFT_FILTERTARGET_MASK);
	emu_wrptr(sc, v->vnum, CVCF, CVCF_CURRENTFILTER_MASK);
	emu_wrptr(sc, v->vnum, FXRT, 0xd01c0000);

	emu_wrptr(sc, v->vnum, PTRX, (x << 8) | r);
	if (v->ismaster) {
		val = 0x20;
		if (v->stereo) {
			val <<= 1;
			emu_wrptr(sc, v->vnum, CPF, CPF_STEREO_MASK);
			emu_wrptr(sc, v->slave->vnum, CPF, CPF_STEREO_MASK);
		} else
			emu_wrptr(sc, v->vnum, CPF, 0);
		sample = 0x80808080;
		if (!v->b16)
			val <<= 1;
		val -= 4;
		/*
		 * mono 8bit:   	val = 0x3c
		 * stereo 8bit:         val = 0x7c
		 * mono 16bit:          val = 0x1c
		 * stereo 16bit:        val = 0x3c
		 */
		if (v->stereo) {
			v2 = 0x3c << 16;
			emu_wrptr(sc, v->vnum, CCR, v2);
			emu_wrptr(sc, v->slave->vnum, CCR, val << 16);
			emu_wrptr(sc, v->slave->vnum, CDE, sample);
			emu_wrptr(sc, v->slave->vnum, CDF, sample);
			start = sa + val / 2;
		} else {
			v2 = 0x1c << 16;
			emu_wrptr(sc, v->vnum, CCR, v2);
			emu_wrptr(sc, v->vnum, CDE, sample);
			emu_wrptr(sc, v->vnum, CDF, sample);
			start = sa + val;
		}
		val <<= 25;
		val |= v2;
		/*
		 * mono 8bit:   	val = 0x781c0000
		 * stereo 8bit:         val = 0xf83c0000
		 * mono 16bit:          val = 0x381c0000
		 * stereo 16bit:        val = 0x783c0000
		 */
		start |= CCCA_INTERPROM_0;
	}
	emu_wrptr(sc, v->vnum, DSL, ea);
	emu_wrptr(sc, v->vnum, PSST, sa | (l << 24));
	emu_wrptr(sc, v->vnum, CCCA, start | (v->b16? 0 : CCCA_8BITSELECT));

	emu_wrptr(sc, v->vnum, Z1, 0);
	emu_wrptr(sc, v->vnum, Z2, 0);

	silent_page = ((u_int32_t)v->buf << 1) | (v->start / EMUPAGESIZE);
	emu_wrptr(sc, v->vnum, MAPA, silent_page);
	emu_wrptr(sc, v->vnum, MAPB, silent_page);

	if (v->ismaster)
		emu_wrptr(sc, v->vnum, CCR, val);

	for (i = CD0; i < CDF; i++)
		emu_wrptr(sc, v->vnum, i, sample);

	emu_wrptr(sc, v->vnum, ATKHLDV, ATKHLDV_HOLDTIME_MASK | ATKHLDV_ATTACKTIME_MASK);
	emu_wrptr(sc, v->vnum, LFOVAL1, 0x8000);
	emu_wrptr(sc, v->vnum, ATKHLDM, 0);
	emu_wrptr(sc, v->vnum, DCYSUSM, DCYSUSM_DECAYTIME_MASK);
	emu_wrptr(sc, v->vnum, LFOVAL2, 0x8000);
	emu_wrptr(sc, v->vnum, IP, p);
	emu_wrptr(sc, v->vnum, PEFE, 0x7f);
	emu_wrptr(sc, v->vnum, FMMOD, 0);
	emu_wrptr(sc, v->vnum, TREMFRQ, 0);
	emu_wrptr(sc, v->vnum, FM2FRQ2, 0);
	emu_wrptr(sc, v->vnum, ENVVAL, 0xbfff);
	emu_wrptr(sc, v->vnum, ENVVOL, 0xbfff);
	emu_wrptr(sc, v->vnum, IFATN, IFATN_FILTERCUTOFF_MASK);

	if (v->slave != NULL)
		emu_vwrite(sc, v->slave);
	if (v->tracker != NULL)
		emu_vwrite(sc, v->tracker);
}

#define IP_TO_CP(ip) ((ip == 0) ? 0 : (((0x00001000uL | (ip & 0x00000FFFL)) << (((ip >> 12) & 0x000FL) + 4)) & 0xFFFF0000uL))
static void
emu_vtrigger(struct sc_info *sc, struct emu_voice *v, int go)
{
	u_int32_t pitch_target;
	if (go) {
		pitch_target = IP_TO_CP((emu_rate_to_pitch(v->speed) >> 8)) >> 16;
		emu_wrptr(sc, v->vnum, PTRX_PITCHTARGET, pitch_target);
		emu_wrptr(sc, v->vnum, CPF_CURRENTPITCH, pitch_target);
		emu_wrptr(sc, v->vnum, VTFT, 0xffff);
		emu_wrptr(sc, v->vnum, CVCF, 0xffff);
		emu_enastop(sc, v->vnum, 0);
		emu_enaint(sc, v->vnum, v->istracker);
		emu_wrptr(sc, v->vnum, DCYSUSV, ENV_ON | 0x00007f7f);
	} else {
		emu_wrptr(sc, v->vnum, IFATN, 0xffff);
		emu_wrptr(sc, v->vnum, IP, 0);
		emu_wrptr(sc, v->vnum, VTFT, 0xffff);
		emu_wrptr(sc, v->vnum, CPF_CURRENTPITCH, 0);
		emu_enaint(sc, v->vnum, 0);
	}
	if (v->slave != NULL)
		emu_vtrigger(sc, v->slave, go);
	if (v->tracker != NULL)
		emu_vtrigger(sc, v->tracker, go);
}

static int
emu_vpos(struct sc_info *sc, struct emu_voice *v)
{
	int s;
	s = (v->b16? 1 : 0) + (v->stereo? 1 : 0);
	return ((emu_rdptr(sc, v->vnum, CCCA_CURRADDR) >> s) - v->start);
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
emuchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_chinfo *ch;

	ch = (dir == PCMDIR_PLAY)? &sc->pch : &sc->rch;
	ch->buffer = b;
	ch->parent = sc;
	ch->channel = c;
	ch->master = emu_valloc(sc);
	ch->slave = emu_valloc(sc);
	ch->tracker = emu_valloc(sc);
	if (emu_vinit(sc, ch->master, ch->slave, ch->tracker, EMU_BUFFSIZE, ch->channel))
		return NULL;
	else
		return ch;
}

static int
emuchan_setdir(void *data, int dir)
{
	struct sc_chinfo *ch = data;

	ch->dir = dir;
	return 0;
}

static int
emuchan_setformat(void *data, u_int32_t format)
{
	struct sc_chinfo *ch = data;

	ch->fmt = format;
	return 0;
}

static int
emuchan_setspeed(void *data, u_int32_t speed)
{
	struct sc_chinfo *ch = data;

	ch->spd = speed;
	return ch->spd;
}

static int
emuchan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
emuchan_trigger(void *data, int go)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc  = ch->parent;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	if (go == PCMTRIG_START) {
		emu_vsetup(ch);
		emu_vwrite(sc, ch->master);
#ifdef EMUDEBUG
		printf("start [%d bit, %s, %d hz]\n",
			ch->master->b16? 16 : 8,
			ch->master->stereo? "stereo" : "mono",
			ch->master->speed);
		emu_vdump(sc, ch->master);
		emu_vdump(sc, ch->slave);
#endif
	}
	emu_vtrigger(sc, ch->master, (go == PCMTRIG_START)? 1 : 0);
	return 0;
}

static int
emuchan_getptr(void *data)
{
	struct sc_chinfo *ch = data;
	struct sc_info *sc = ch->parent;

	return emu_vpos(sc, ch->master);
}

static pcmchan_caps *
emuchan_getcaps(void *data)
{
	struct sc_chinfo *ch = data;

	return (ch->dir == PCMDIR_PLAY)? &emu_playcaps : &emu_reccaps;
}

/* The interrupt handler */
static void
emu_intr(void *p)
{
	struct sc_info *sc = (struct sc_info *)p;
	u_int32_t stat, i;

	do {
		stat = emu_rd(sc, IPR, 4);

		/* process irq */
		for (i = 0; i < 64; i++) {
			if (emu_testint(sc, i)) {
				if (sc->voice[i].channel)
					chn_intr(sc->voice[i].channel);
				else
					device_printf(sc->dev, "bad irq voice %d\n", i);
				emu_clrint(sc, i);
			}
		}

		emu_wr(sc, IPR, stat, 4);
	} while (stat);
}

/* -------------------------------------------------------------------- */

static void *
emu_malloc(struct sc_info *sc, u_int32_t sz)
{
	void *buf;
	bus_dmamap_t map;

	if (bus_dmamem_alloc(sc->parent_dmat, &buf, BUS_DMA_NOWAIT, &map))
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
	start = 0;
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
	emu_wrptr(sc, 0, MICBS, 0);
	emu_wrptr(sc, 0, MICBA, 0);
	emu_wrptr(sc, 0, FXBS, 0);
	emu_wrptr(sc, 0, FXBA, 0);
	emu_wrptr(sc, 0, ADCBS, ADCBS_BUFSIZE_NONE);
	emu_wrptr(sc, 0, ADCBA, 0);

	/* disable channel interrupt */
	emu_wr(sc, INTE, DISABLE, 4);
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
		sc->voice[ch].running = 0;
		sc->voice[ch].b16 = 0;
		sc->voice[ch].stereo = 0;
		sc->voice[ch].speed = 0;
		sc->voice[ch].start = 0;
		sc->voice[ch].end = 0;
		sc->voice[ch].channel = NULL;
       }

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
	emu_wrptr(sc, 0, TCBS, 4);	/* taken from original driver */

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
	sc->type = pci_get_devid(dev);
	sc->rev = pci_get_revid(dev);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	mapped = 0;
	/* Xemu dfr: is this strictly necessary? */
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

	if (pcm_register(dev, sc, 1, 0)) goto bad;
	pcm_addchan(dev, PCMDIR_PLAY, &emu_chantemplate, sc);
	/* pcm_addchan(dev, PCMDIR_REC, &emu_chantemplate, sc); */

	pcm_setstatus(dev, status);

	return 0;

bad:
	if (sc->reg) bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	if (sc->ih) bus_teardown_intr(dev, sc->irq, sc->ih);
	if (sc->irq) bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	free(sc, M_DEVBUF);
	return ENXIO;
}

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

DRIVER_MODULE(emu, pci, emu_driver, pcm_devclass, 0, 0);
