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
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>

#define FEEDBUFSZ	8192
#undef FEEDER_DEBUG

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

struct feedertab_entry {
	SLIST_ENTRY(feedertab_entry) link;
	pcm_feeder *feeder;
	struct pcm_feederdesc *desc;

	int idx;
};
static SLIST_HEAD(, feedertab_entry) feedertab;

/*****************************************************************************/

void
feeder_register(void *p)
{
	pcm_feeder *f = p;
	struct feedertab_entry *fte;
	static int feedercnt = 0;
	int i;

	if (feedercnt == 0) {
		if (f->desc)
			panic("FIRST FEEDER NOT ROOT: %s\n", f->name);
		SLIST_INIT(&feedertab);
		fte = malloc(sizeof(*fte), M_DEVBUF, M_NOWAIT);
		fte->feeder = f;
		fte->desc = NULL;
		fte->idx = feedercnt;
		SLIST_INSERT_HEAD(&feedertab, fte, link);
		feedercnt++;
		return;
	}
	/* printf("installing feeder: %s\n", f->name); */

	i = 0;
	while ((feedercnt < MAXFEEDERS) && (f->desc[i].type > 0)) {
		fte = malloc(sizeof(*fte), M_DEVBUF, M_NOWAIT);
		fte->feeder = f;
		fte->desc = &f->desc[i];
		fte->idx = feedercnt;
		fte->desc->idx = feedercnt;
		SLIST_INSERT_HEAD(&feedertab, fte, link);
		i++;
	}
	feedercnt++;
	if (feedercnt >= MAXFEEDERS)
		printf("MAXFEEDERS exceeded\n");
}

/*****************************************************************************/

static int
feed_root(pcm_feeder *feeder, pcm_channel *ch, u_int8_t *buffer, u_int32_t count, struct uio *stream)
{
	int ret, s;

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

	return count;
}
static pcm_feeder feeder_root = { "root", 0, NULL, NULL, NULL, feed_root };
SYSINIT(feeder_root, SI_SUB_DRIVERS, SI_ORDER_FIRST, feeder_register, &feeder_root);

/*****************************************************************************/

static int
feed_8to16le(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
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

static struct pcm_feederdesc desc_8to16le[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_8to16le =
	{ "8to16le", 0, desc_8to16le, NULL, NULL, feed_8to16le };
SYSINIT(feeder_8to16le, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_8to16le);

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
feed_16leto8(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
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

static struct pcm_feederdesc desc_16leto8[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_16leto8 =
	{ "16leto8", 1, desc_16leto8, feed_16to8_init, feed_16to8_free, feed_16leto8 };
SYSINIT(feeder_16leto8, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_16leto8);

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

static struct pcm_feederdesc desc_monotostereo8[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S8 | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_monotostereo8 =
	{ "monotostereo8", 0, desc_monotostereo8, NULL, NULL, feed_monotostereo8 };
SYSINIT(feeder_monotostereo8, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_monotostereo8);

/*****************************************************************************/

static int
feed_monotostereo16(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	int i, j, k = f->source->feed(f->source, c, b, count / 2, stream);
	u_int8_t x, y;

	j = k - 1;
	i = j * 2 + 1;
	while (i > 3 && j >= 1) {
		x = b[j--];
		y = b[j--];
		b[i--] = x;
		b[i--] = y;
		b[i--] = x;
		b[i--] = y;
	}
	return k * 2;
}

static struct pcm_feederdesc desc_monotostereo16[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S16_BE | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_monotostereo16 =
	{ "monotostereo16", 0, desc_monotostereo16, NULL, NULL, feed_monotostereo16 };
SYSINIT(feeder_monotostereo16, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_monotostereo16);

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

static struct pcm_feederdesc desc_stereotomono8[] = {
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S8, 0},
	{0},
};
static pcm_feeder feeder_stereotomono8 =
	{ "stereotomono8", 1, desc_stereotomono8, feed_stereotomono8_init, feed_stereotomono8_free, feed_stereotomono8 };
SYSINIT(feeder_stereotomono8, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_stereotomono8);

/*****************************************************************************/

static int
feed_stereotomono16_init(pcm_feeder *f)
{
	f->data = malloc(FEEDBUFSZ, M_DEVBUF, M_NOWAIT);
	return (f->data == NULL);
}

static int
feed_stereotomono16_free(pcm_feeder *f)
{
	if (f->data) free(f->data, M_DEVBUF);
	f->data = NULL;
	return 0;
}

static int
feed_stereotomono16(pcm_feeder *f, pcm_channel *c, u_int8_t *b, u_int32_t count, struct uio *stream)
{
	u_int32_t i = 0, toget = count * 2;
	int j = 0, k;

	k = f->source->feed(f->source, c, f->data, min(toget, FEEDBUFSZ), stream);
	while (j < k) {
		b[i++] = ((u_int8_t *)f->data)[j];
		b[i++] = ((u_int8_t *)f->data)[j + 1];
		j += 4;
	}
	return i;
}

static struct pcm_feederdesc desc_stereotomono16[] = {
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE, 0},
	{0},
};
static pcm_feeder feeder_stereotomono16 =
	{ "stereotomono16", 1, desc_stereotomono16, feed_stereotomono16_init, feed_stereotomono16_free, feed_stereotomono16 };
SYSINIT(feeder_stereotomono16, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_stereotomono16);

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

static struct pcm_feederdesc desc_endian[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_endian = { "endian", -1, desc_endian, NULL, NULL, feed_endian };
SYSINIT(feeder_endian, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_endian);

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

static struct pcm_feederdesc desc_sign8[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_sign8 =
	{ "sign8", 0, desc_sign8, NULL, NULL, feed_sign, (void *)1 };
SYSINIT(feeder_sign8, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_sign8);

static struct pcm_feederdesc desc_sign16le[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_sign16le =
	{ "sign16le", -1, desc_sign16le, NULL, NULL, feed_sign, (void *)2 };
SYSINIT(feeder_sign16le, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_sign16le);

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

static struct pcm_feederdesc desc_ulawtou8[] = {
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_ulawtou8 =
	{ "ulawtou8", 0, desc_ulawtou8, NULL, NULL, feed_table, ulaw_to_u8 };
SYSINIT(feeder_ulawtou8, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_ulawtou8);

static struct pcm_feederdesc desc_u8toulaw[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_MU_LAW | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_u8toulaw =
	{ "u8toulaw", 0, desc_u8toulaw, NULL, NULL, feed_table, u8_to_ulaw };
SYSINIT(feeder_u8toulaw, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder_u8toulaw);

/*****************************************************************************/

static int
cmpdesc(struct pcm_feederdesc *n, struct pcm_feederdesc *m)
{
	return ((n->type == m->type) && (n->in == m->in) && (n->out == m->out) && (n->flags == m->flags));
}

pcm_feeder *
feeder_get(struct pcm_feederdesc *desc)
{
	struct feedertab_entry *fte;

	SLIST_FOREACH(fte, &feedertab, link) {
		if ((fte->desc != NULL) && cmpdesc(desc, fte->desc))
			return fte->feeder;
	}
	return NULL;
}

pcm_feeder *
feeder_getroot()
{
	struct feedertab_entry *fte;

	SLIST_FOREACH(fte, &feedertab, link) {
		if (fte->desc == NULL)
			return fte->feeder;
	}
	return NULL;
}

int
chn_removefeeder(pcm_channel *c)
{
	pcm_feeder *f;

	if (c->feeder->source == NULL)
		return -1;
	f = c->feeder->source;
	if (c->feeder->free)
		c->feeder->free(c->feeder);
	free(c->feeder, M_DEVBUF);
	c->feeder = f;
	return 0;
}

static int
chainok(pcm_feeder *test, pcm_feeder *stop)
{
	u_int32_t visited[MAXFEEDERS / 32];
	u_int32_t idx, mask;

	bzero(visited, sizeof(visited));
	while (test && (test != stop)) {
		idx = test->desc->idx;
		if (idx < 0)
			panic("bad idx %d", idx);
		if (idx >= MAXFEEDERS)
			panic("bad idx %d", idx);
		mask = 1 << (idx & 31);
		idx >>= 5;
		if (visited[idx] & mask)
			return 0;
		visited[idx] |= mask;
		test = test->source;
	}
	return 1;
}

static pcm_feeder *
feeder_fmtchain(u_int32_t *to, pcm_feeder *source, pcm_feeder *stop, int maxdepth)
{
	struct feedertab_entry *fte;
	pcm_feeder *try, *ret;
	struct pcm_feederdesc *trydesc;

	/* printf("trying %s...\n", source->name); */
	if (fmtvalid(source->desc->out, to)) {
		/* printf("got it\n"); */
		return source;
	}

	if (maxdepth < 0)
		return NULL;

	try = malloc(sizeof(*try), M_DEVBUF, M_NOWAIT);
	trydesc = malloc(sizeof(*trydesc), M_DEVBUF, M_NOWAIT);
	trydesc->type = FEEDER_FMT;
	trydesc->in = source->desc->out;
	trydesc->out = 0;
	trydesc->flags = 0;
	trydesc->idx = -1;

	SLIST_FOREACH(fte, &feedertab, link) {
		if ((fte->desc) && (fte->desc->in == source->desc->out)) {
			*try = *(fte->feeder);
			try->source = source;
			try->desc = trydesc;
			trydesc->out = fte->desc->out;
			trydesc->idx = fte->idx;
			ret = chainok(try, stop)? feeder_fmtchain(to, try, stop, maxdepth - 1) : NULL;
			if (ret != NULL)
				return ret;
		}
	}
	free(try, M_DEVBUF);
	free(trydesc, M_DEVBUF);
	/* printf("giving up %s...\n", source->name); */
	return NULL;
}

u_int32_t
chn_feedchain(pcm_channel *c, u_int32_t *to)
{
	pcm_feeder *try, *stop;
	int max;

	stop = c->feeder;
	try = NULL;
	max = 0;
	while (try == NULL && max < 8) {
		try = feeder_fmtchain(to, c->feeder, stop, max);
		max++;
	}
	if (try == NULL)
		return 0;
	c->feeder = try;
	c->align = 0;
#ifdef FEEDER_DEBUG
	printf("chain: ");
#endif
	while (try && (try != stop)) {
#ifdef FEEDER_DEBUG
		printf("%s [%d]", try->name, try->desc->idx);
		if (try->source)
			printf(" -> ");
#endif
		if (try->init)
			try->init(try);
		if (try->align > 0)
			c->align += try->align;
		else if (try->align < 0 && c->align < -try->align)
			c->align = -try->align;
		try = try->source;
	}
#ifdef FEEDER_DEBUG
	printf("%s [%d]\n", try->name, try->desc->idx);
#endif
	return c->feeder->desc->out;
}
