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
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/sound/pcm/feeder.c,v 1.8.2.2 2000/07/19 21:18:46 cg Exp $
 */

#include <dev/sound/pcm/sound.h>

static int chn_addfeeder(pcm_channel *c, pcm_feeder *f);
static int chn_removefeeder(pcm_channel *c);

#define FEEDBUFSZ	8192

static unsigned char ulaw_to_u8[] = {
     3,    7,   11,   15,   19,   23,   27,   31,
    35,   39,   43,   47,   51,   55,   59,   63,
    66,   68,   70,   72,   74,   76,   78,   80,
    82,   84,   86,   88,   90,   92,   94,   96,
    98,   99,  100,  101,  102,  103,  104,  105,
   106,  107,  108,  109,  110,  111,  112,  113,
   113,  114,  114,  115,  115,  116,  116,  117,
   117,  118,  118,  119,  119,  120,  120,  121,
   121,  121,  122,  122,  122,  122,  123,  123,
   123,  123,  124,  124,  124,  124,  125,  125,
   125,  125,  125,  125,  126,  126,  126,  126,
   126,  126,  126,  126,  127,  127,  127,  127,
   127,  127,  127,  127,  127,  127,  127,  127,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
   253,  249,  245,  241,  237,  233,  229,  225,
   221,  217,  213,  209,  205,  201,  197,  193,
   190,  188,  186,  184,  182,  180,  178,  176,
   174,  172,  170,  168,  166,  164,  162,  160,
   158,  157,  156,  155,  154,  153,  152,  151,
   150,  149,  148,  147,  146,  145,  144,  143,
   143,  142,  142,  141,  141,  140,  140,  139,
   139,  138,  138,  137,  137,  136,  136,  135,
   135,  135,  134,  134,  134,  134,  133,  133,
   133,  133,  132,  132,  132,  132,  131,  131,
   131,  131,  131,  131,  130,  130,  130,  130,
   130,  130,  130,  130,  129,  129,  129,  129,
   129,  129,  129,  129,  129,  129,  129,  129,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
   128,  128,  128,  128,  128,  128,  128,  128,
};

static unsigned char u8_to_ulaw[] = {
     0,    0,    0,    0,    0,    1,    1,    1,
     1,    2,    2,    2,    2,    3,    3,    3,
     3,    4,    4,    4,    4,    5,    5,    5,
     5,    6,    6,    6,    6,    7,    7,    7,
     7,    8,    8,    8,    8,    9,    9,    9,
     9,   10,   10,   10,   10,   11,   11,   11,
    11,   12,   12,   12,   12,   13,   13,   13,
    13,   14,   14,   14,   14,   15,   15,   15,
    15,   16,   16,   17,   17,   18,   18,   19,
    19,   20,   20,   21,   21,   22,   22,   23,
    23,   24,   24,   25,   25,   26,   26,   27,
    27,   28,   28,   29,   29,   30,   30,   31,
    31,   32,   33,   34,   35,   36,   37,   38,
    39,   40,   41,   42,   43,   44,   45,   46,
    47,   49,   51,   53,   55,   57,   59,   61,
    63,   66,   70,   74,   78,   84,   92,  104,
   254,  231,  219,  211,  205,  201,  197,  193,
   190,  188,  186,  184,  182,  180,  178,  176,
   175,  174,  173,  172,  171,  170,  169,  168,
   167,  166,  165,  164,  163,  162,  161,  160,
   159,  159,  158,  158,  157,  157,  156,  156,
   155,  155,  154,  154,  153,  153,  152,  152,
   151,  151,  150,  150,  149,  149,  148,  148,
   147,  147,  146,  146,  145,  145,  144,  144,
   143,  143,  143,  143,  142,  142,  142,  142,
   141,  141,  141,  141,  140,  140,  140,  140,
   139,  139,  139,  139,  138,  138,  138,  138,
   137,  137,  137,  137,  136,  136,  136,  136,
   135,  135,  135,  135,  134,  134,  134,  134,
   133,  133,  133,  133,  132,  132,  132,  132,
   131,  131,  131,  131,  130,  130,  130,  130,
   129,  129,  129,  129,  128,  128,  128,  128,
};

/*****************************************************************************/

static int
feed_root(pcm_feeder *feeder, pcm_channel *ch, u_int8_t *buffer, u_int32_t count, struct uio *stream)
{
	int ret, c = 0, s;
	KASSERT(count, ("feed_root: count == 0"));
	count &= ~((1 << ch->align) - 1);
	KASSERT(count, ("feed_root: aligned count == 0"));
	s = spltty();
	count = min(count, stream->uio_resid);
	if (count) {
		ret = uiomove(buffer, count, stream);
		KASSERT(ret == 0, ("feed_root: uiomove failed"));
	}
	splx(s);
	return c + count;
}
pcm_feeder feeder_root = { "root", 0, NULL, NULL, feed_root };

/*****************************************************************************/

static int
feed_8to16(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	int i, j, k;
	k = f->source->feed(f->source, c, b, count / 2, stream);
	j = k - 1;
	i = j * 2 + 1;
	while (i > 0 && j >= 0) {
		b[i--] = b[j--];
		b[i--] = 0;
	}
	return k * 2;
}
static pcm_feeder feeder_8to16 = { "8to16", 0, NULL, NULL, feed_8to16 };

/*****************************************************************************/

static int
feed_16to8_init(pcm_feeder *f)
{
	f->data = malloc(FEEDBUFSZ, M_DEVBUF, M_NOWAIT);
	return (f->data == NULL);
}

static int
feed_16to8_free(pcm_feeder *f)
{
	if (f->data) free(f->data, M_DEVBUF);
	f->data = NULL;
	return 0;
}

static int
feed_16to8le(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	u_int32_t i = 0, toget = count * 2;
	int j = 1, k;
	k = f->source->feed(f->source, c, f->data, min(toget, FEEDBUFSZ), stream);
	while (j < k) {
		b[i++] = ((u_int8_t *)f->data)[j];
		j += 2;
	}
	return i;
}
static pcm_feeder feeder_16to8le =
	{ "16to8le", 1, feed_16to8_init, feed_16to8_free, feed_16to8le };

/*****************************************************************************/

static int
feed_monotostereo8(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	int i, j, k = f->source->feed(f->source, c, b, count / 2, stream);
	j = k - 1;
	i = j * 2 + 1;
	while (i > 0 && j >= 0) {
		b[i--] = b[j];
		b[i--] = b[j];
		j--;
	}
	return k * 2;
}
static pcm_feeder feeder_monotostereo8 =
	{ "monotostereo8", 0, NULL, NULL, feed_monotostereo8 };

/*****************************************************************************/

static int
feed_stereotomono8_init(pcm_feeder *f)
{
	f->data = malloc(FEEDBUFSZ, M_DEVBUF, M_NOWAIT);
	return (f->data == NULL);
}

static int
feed_stereotomono8_free(pcm_feeder *f)
{
	if (f->data) free(f->data, M_DEVBUF);
	f->data = NULL;
	return 0;
}

static int
feed_stereotomono8(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	u_int32_t i = 0, toget = count * 2;
	int j = 0, k;
	k = f->source->feed(f->source, c, f->data, min(toget, FEEDBUFSZ), stream);
	while (j < k) {
		b[i++] = ((u_int8_t *)f->data)[j];
		j += 2;
	}
	return i;
}
static pcm_feeder feeder_stereotomono8 =
	{ "stereotomono8", 1, feed_stereotomono8_init, feed_stereotomono8_free,
	feed_stereotomono8 };

/*****************************************************************************/

static int
feed_endian(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	u_int8_t t;
	int i = 0, j = f->source->feed(f->source, c, b, count, stream);
	while (i < j) {
		t = b[i];
		b[i] = b[i + 1];
		b[i + 1] = t;
		i += 2;
	}
	return i;
}
static pcm_feeder feeder_endian = { "endian", -1, NULL, NULL, feed_endian };

/*****************************************************************************/

static int
feed_sign(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	int i = 0, j = f->source->feed(f->source, c, b, count, stream);
	int ssz = (int)f->data, ofs = ssz - 1;
	while (i < j) {
		b[i + ofs] ^= 0x80;
		i += ssz;
	}
	return i;
}
static pcm_feeder feeder_sign8 =
	{ "sign8", 0, NULL, NULL, feed_sign, (void *)1 };
static pcm_feeder feeder_sign16 =
	{ "sign16", -1, NULL, NULL, feed_sign, (void *)2 };

/*****************************************************************************/

static int
feed_table(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	int i = 0, j = f->source->feed(f->source, c, b, count, stream);
	while (i < j) {
		b[i] = ((u_int8_t *)f->data)[b[i]];
		i++;
	}
	return i;
}
static pcm_feeder feeder_ulawtou8 =
	{ "ulawtou8", 0, NULL, NULL, feed_table, ulaw_to_u8 };
static pcm_feeder feeder_u8toulaw =
	{ "u8toulaw", 0, NULL, NULL, feed_table, u8_to_ulaw };

/*****************************************************************************/

struct fmtspec {
	int stereo;
	int sign;
	int bit16;
	int bigendian;
	int ulaw;
	int bad;
};

struct fmtcvt {
	pcm_feeder *f;
	struct fmtspec ispec, ospec;
};

struct fmtcvt cvttab[] = {
	{&feeder_ulawtou8, 	{-1,  0, 0, 0,  1}, 	{-1,  0, 0, 0,  0}},
	{&feeder_u8toulaw, 	{-1,  0, 0, 0,  0}, 	{-1,  0, 0, 0,  1}},
	{&feeder_sign8,		{-1,  0, 0, 0,  0},	{-1,  1, 0, 0,  0}},
	{&feeder_sign8,		{-1,  1, 0, 0,  0},	{-1,  0, 0, 0,  0}},
	{&feeder_monotostereo8,	{ 0, -1, 0, 0, -1},	{ 1, -1, 0, 0, -1}},
	{&feeder_stereotomono8, { 1, -1, 0, 0, -1},	{ 0, -1, 0, 0, -1}},
	{&feeder_sign16,	{-1,  0, 1, 0,  0},	{-1,  1, 1, 0,  0}},
	{&feeder_sign16,	{-1,  1, 1, 0,  0},	{-1,  0, 1, 0,  0}},
	{&feeder_8to16,		{-1, -1, 0, 0,  0},	{-1, -1, 1, 0,  0}},
	{&feeder_16to8le,	{-1, -1, 1, 0,  0},	{-1, -1, 0, 0,  0}},
	{&feeder_endian,	{-1, -1, 1, 0,  0},	{-1, -1, 1, 1,  0}},
	{&feeder_endian,	{-1, -1, 1, 1,  0},	{-1, -1, 1, 0,  0}},
};
#define FEEDERTABSZ (sizeof(cvttab) / sizeof(struct fmtcvt))

static int
getspec(u_int32_t fmt, struct fmtspec *spec)
{
	spec->stereo = (fmt & AFMT_STEREO)? 1 : 0;
	spec->sign = (fmt & AFMT_SIGNED)? 1 : 0;
	spec->bit16 = (fmt & AFMT_16BIT)? 1 : 0;
	spec->bigendian = (fmt & AFMT_BIGENDIAN)? 1 : 0;
	spec->ulaw = (fmt & AFMT_MU_LAW)? 1 : 0;
	spec->bad = (fmt & (AFMT_A_LAW | AFMT_MPEG))? 1 : 0;
	return 0;
}

static int
cmp(int x, int y)
{
	return (x == -1 || x == y || y == -1)? 1 : 0;
}

static int
cmpspec(struct fmtspec *x, struct fmtspec *y)
{
	int i = 0;
	if (cmp(x->stereo, y->stereo)) i |= 0x01;
	if (cmp(x->sign, y->sign)) i |= 0x02;
	if (cmp(x->bit16, y->bit16)) i |= 0x04;
	if (cmp(x->bigendian, y->bigendian)) i |= 0x08;
	if (cmp(x->ulaw, y->ulaw)) i |= 0x10;
	return i;
}

static int
cvtapply(pcm_channel *c, struct fmtcvt *cvt, struct fmtspec *s)
{
	int i = cmpspec(s, &cvt->ospec);
	chn_addfeeder(c, cvt->f);
	if (cvt->ospec.stereo != -1) s->stereo = cvt->ospec.stereo;
	if (cvt->ospec.sign != -1) s->sign = cvt->ospec.sign;
	if (cvt->ospec.bit16 != -1) s->bit16 = cvt->ospec.bit16;
	if (cvt->ospec.bigendian != -1) s->bigendian = cvt->ospec.bigendian;
	if (cvt->ospec.ulaw != -1) s->ulaw = cvt->ospec.ulaw;
	return i;
}

int
chn_feedchain(pcm_channel *c)
{
	int i, chosen, iter;
	u_int32_t mask;
	struct fmtspec s, t;
	struct fmtcvt *e;

	while (chn_removefeeder(c) != -1);
	c->align = 0;
	if ((c->format & chn_getcaps(c)->formats) == c->format)
		return c->format;
	getspec(c->format, &s);
	if (s.bad) return -1;
	getspec(chn_getcaps(c)->bestfmt, &t);
	mask = (~cmpspec(&s, &t)) & 0x1f;
	iter = 0;
	do {
		if (mask == 0 || iter >= 8) break;
		chosen = -1;
		for (i = 0; i < FEEDERTABSZ && chosen == -1; i++) {
			e = &cvttab[i];
			if ((cmpspec(&s, &e->ispec) == 0x1f) &&
			   ((~cmpspec(&e->ispec, &e->ospec)) & mask))
			   chosen = i;
		}
		if (chosen != -1) mask &= cvtapply(c, &cvttab[chosen], &s);
		iter++;
	} while (chosen != -1);
	return (iter < 8)? chn_getcaps(c)->bestfmt : -1;
}

static int
chn_addfeeder(pcm_channel *c, pcm_feeder *f)
{
	pcm_feeder *n;
	n = malloc(sizeof(pcm_feeder), M_DEVBUF, M_NOWAIT);
	*n = *f;
	n->source = c->feeder;
	c->feeder = n;
	if (n->init) n->init(n);
	if (n->align > 0) c->align += n->align;
	else if (n->align < 0 && c->align < -n->align) c->align -= n->align;
	return 0;
}

static int
chn_removefeeder(pcm_channel *c)
{
	pcm_feeder *f;
	if (c->feeder == &feeder_root) return -1;
	f = c->feeder->source;
	if (c->feeder->free) c->feeder->free(c->feeder);
	free(c->feeder, M_DEVBUF);
	c->feeder = f;
	return 0;
}

