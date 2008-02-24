/*-
 * Copyright (c) 1999 Cameron Grant <cg@FreeBSD.org>
 * Copyright (c) 2005 Ariff Abdullah <ariff@FreeBSD.org>
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
 */

/*
 * *New* and rewritten soft format converter, supporting 24/32bit pcm data,
 * simplified and optimized.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 * This new implementation is fully dedicated in memory of Cameron Grant,  *
 * the creator of the magnificent, highly addictive feeder infrastructure. *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */

#include <dev/sound/pcm/sound.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD: src/sys/dev/sound/pcm/feeder_fmt.c,v 1.23 2007/06/02 13:07:44 joel Exp $");

static int feeder_fmt_stereodownmix = 0;
TUNABLE_INT("hw.snd.feeder_fmt_stereodownmix", &feeder_fmt_stereodownmix);
#ifdef SND_DEBUG
SYSCTL_INT(_hw_snd, OID_AUTO, feeder_fmt_stereodownmix, CTLFLAG_RW,
	&feeder_fmt_stereodownmix, 1, "averaging stereo downmix");
#endif

#define FEEDFMT_RESERVOIR	8	/* 32bit stereo */

static uint8_t ulaw_to_u8_tbl[] = {
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

static uint8_t alaw_to_u8_tbl[] = {
  108,  109,  106,  107,  112,  113,  110,  111,
  100,  101,   98,   99,  104,  105,  102,  103,
  118,  118,  117,  117,  120,  120,  119,  119,
  114,  114,  113,  113,  116,  116,  115,  115,
   43,   47,   35,   39,   59,   63,   51,   55,
   11,   15,    3,    7,   27,   31,   19,   23,
   86,   88,   82,   84,   94,   96,   90,   92,
   70,   72,   66,   68,   78,   80,   74,   76,
  127,  127,  127,  127,  127,  127,  127,  127,
  127,  127,  127,  127,  127,  127,  127,  127,
  128,  128,  128,  128,  128,  128,  128,  128,
  128,  128,  128,  128,  128,  128,  128,  128,
  123,  123,  123,  123,  124,  124,  124,  124,
  121,  121,  121,  121,  122,  122,  122,  122,
  126,  126,  126,  126,  126,  126,  126,  126,
  125,  125,  125,  125,  125,  125,  125,  125,
  148,  147,  150,  149,  144,  143,  146,  145,
  156,  155,  158,  157,  152,  151,  154,  153,
  138,  138,  139,  139,  136,  136,  137,  137,
  142,  142,  143,  143,  140,  140,  141,  141,
  213,  209,  221,  217,  197,  193,  205,  201,
  245,  241,  253,  249,  229,  225,  237,  233,
  170,  168,  174,  172,  162,  160,  166,  164,
  186,  184,  190,  188,  178,  176,  182,  180,
  129,  129,  129,  129,  129,  129,  129,  129,
  129,  129,  129,  129,  129,  129,  129,  129,
  128,  128,  128,  128,  128,  128,  128,  128,
  128,  128,  128,  128,  128,  128,  128,  128,
  133,  133,  133,  133,  132,  132,  132,  132,
  135,  135,  135,  135,  134,  134,  134,  134,
  130,  130,  130,  130,  130,  130,  130,  130,
  131,  131,  131,  131,  131,  131,  131,  131,
};

static uint8_t u8_to_ulaw_tbl[] = {
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

static uint8_t u8_to_alaw_tbl[] = {
   42,   42,   42,   42,   42,   43,   43,   43,
   43,   40,   40,   40,   40,   41,   41,   41,
   41,   46,   46,   46,   46,   47,   47,   47,
   47,   44,   44,   44,   44,   45,   45,   45,
   45,   34,   34,   34,   34,   35,   35,   35,
   35,   32,   32,   32,   32,   33,   33,   33,
   33,   38,   38,   38,   38,   39,   39,   39,
   39,   36,   36,   36,   36,   37,   37,   37,
   37,   58,   58,   59,   59,   56,   56,   57,
   57,   62,   62,   63,   63,   60,   60,   61,
   61,   50,   50,   51,   51,   48,   48,   49,
   49,   54,   54,   55,   55,   52,   52,   53,
   53,   10,   11,    8,    9,   14,   15,   12,
   13,    2,    3,    0,    1,    6,    7,    4,
    5,   24,   30,   28,   18,   16,   22,   20,
  106,  110,   98,  102,  122,  114,   75,   90,
  213,  197,  245,  253,  229,  225,  237,  233,
  149,  151,  145,  147,  157,  159,  153,  155,
  133,  132,  135,  134,  129,  128,  131,  130,
  141,  140,  143,  142,  137,  136,  139,  138,
  181,  181,  180,  180,  183,  183,  182,  182,
  177,  177,  176,  176,  179,  179,  178,  178,
  189,  189,  188,  188,  191,  191,  190,  190,
  185,  185,  184,  184,  187,  187,  186,  186,
  165,  165,  165,  165,  164,  164,  164,  164,
  167,  167,  167,  167,  166,  166,  166,  166,
  161,  161,  161,  161,  160,  160,  160,  160,
  163,  163,  163,  163,  162,  162,  162,  162,
  173,  173,  173,  173,  172,  172,  172,  172,
  175,  175,  175,  175,  174,  174,  174,  174,
  169,  169,  169,  169,  168,  168,  168,  168,
  171,  171,  171,  171,  170,  170,  170,  170,
};

static int
feed_table_8(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int j, sign, k;
	uint8_t *tbl = (uint8_t *)f->data;

	if (count < PCM_8_BPS)
		return (0);

	k = FEEDER_FEED(f->source, c, b, count, source);
	if (k < PCM_8_BPS)
		return (0);

	j = k;
	sign = (f->desc->out & AFMT_SIGNED) ? 0x80 : 0x00;

	do {
		j--;
		b[j] = tbl[b[j]] ^ sign;
	} while (j != 0);

	return (k);
}

static int
feed_table_16(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, sign, k;
	uint8_t *tbl = (uint8_t *)f->data;

	if (count < PCM_16_BPS)
		return (0);

	k = FEEDER_FEED(f->source, c, b, count >> 1, source);
	if (k < PCM_8_BPS)
		return (0);

	i = k;
	k <<= 1;
	j = k;
	sign = (f->desc->out & AFMT_SIGNED) ? 0x80 : 0x00;

	if (f->desc->out & AFMT_BIGENDIAN) {
		do {
			b[--j] = 0;
			b[--j] = tbl[b[--i]] ^ sign;
		} while (i != 0);
	} else {
		do {
			b[--j] = tbl[b[--i]] ^ sign;
			b[--j] = 0;
		} while (i != 0);
	}

	return (k);
}

static int
feed_table_xlaw(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int j, sign, k;
	uint8_t *tbl = (uint8_t *)f->data;

	if (count < PCM_8_BPS)
		return (0);

	k = FEEDER_FEED(f->source, c, b, count, source);
	if (k < PCM_8_BPS)
		return (0);

	j = k ;
	sign = (f->desc->in & AFMT_SIGNED) ? 0x80 : 0x00;

	do {
		j--;
		b[j] = tbl[b[j] ^ sign];
	} while (j != 0);

	return (k);
}

static struct pcm_feederdesc feeder_ulawto8_desc[] = {
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_ulawto8_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_table_8),
	{0, 0}
};
FEEDER_DECLARE(feeder_ulawto8, 0, ulaw_to_u8_tbl);

static struct pcm_feederdesc feeder_alawto8_desc[] = {
	{FEEDER_FMT, AFMT_A_LAW, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_A_LAW, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_alawto8_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_table_8),
	{0, 0}
};
FEEDER_DECLARE(feeder_alawto8, 0, alaw_to_u8_tbl);

static struct pcm_feederdesc feeder_ulawto16_desc[] = {
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_ulawto16_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_table_16),
	{0, 0}
};
FEEDER_DECLARE(feeder_ulawto16, 0, ulaw_to_u8_tbl);

static struct pcm_feederdesc feeder_alawto16_desc[] = {
	{FEEDER_FMT, AFMT_A_LAW, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_A_LAW, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_A_LAW, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_A_LAW, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_alawto16_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_table_16),
	{0, 0}
};
FEEDER_DECLARE(feeder_alawto16, 0, alaw_to_u8_tbl);

static struct pcm_feederdesc feeder_8toulaw_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_MU_LAW | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_MU_LAW | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_8toulaw_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_table_xlaw),
	{0, 0}
};
FEEDER_DECLARE(feeder_8toulaw, 0, u8_to_ulaw_tbl);

static struct pcm_feederdesc feeder_8toalaw_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_A_LAW, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_A_LAW | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_A_LAW, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_A_LAW | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_8toalaw_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_table_xlaw),
	{0, 0}
};
FEEDER_DECLARE(feeder_8toalaw, 0, u8_to_alaw_tbl);

/*
 * All conversion done in byte level to preserve endianess.
 */

#define FEEDFMT_SWAP_SIGN(f)	(((((f)->desc->in & AFMT_SIGNED) == 0) !=	\
				(((f)->desc->out & AFMT_SIGNED) == 0))		\
				? 0x80 : 0x00)

/*
 * Bit conversion
 */

#define FBIT_DATA(i, o, c)	((intptr_t)((((c) & 0xf) << 6) | \
				(((i) & 0x7) << 3) | ((o) & 0x7)))
#define FBIT_OUTBPS(m)		((m) & 0x7)
#define FBIT_INBPS(m)		FBIT_OUTBPS((m) >> 3)
#define FBIT_CHANNELS(m)	(((m) >> 6) & 0xf)

static int
feed_updownbit_init(struct pcm_feeder *f)
{
	int ibps, obps, channels;

	if (f->desc->in == f->desc->out || (f->desc->in & AFMT_STEREO) !=
	    (f->desc->out & AFMT_STEREO))
		return (-1);

	channels = (f->desc->in & AFMT_STEREO) ? 2 : 1;

	if (f->desc->in & AFMT_32BIT)
		ibps = PCM_32_BPS;
	else if (f->desc->in & AFMT_24BIT)
		ibps = PCM_24_BPS;
	else if (f->desc->in & AFMT_16BIT)
		ibps = PCM_16_BPS;
	else
		ibps = PCM_8_BPS;

	if (f->desc->out & AFMT_32BIT)
		obps = PCM_32_BPS;
	else if (f->desc->out & AFMT_24BIT)
		obps = PCM_24_BPS;
	else if (f->desc->out & AFMT_16BIT)
		obps = PCM_16_BPS;
	else
		obps = PCM_8_BPS;

	f->data = (void *)FBIT_DATA(ibps, obps, channels);

	return (0);
}

static int
feed_upbit(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k, sign, ibps, ialign, obps, oalign, pad;
	uint8_t *src, *dst;

	ibps = FBIT_INBPS((intptr_t)f->data);
	obps = FBIT_OUTBPS((intptr_t)f->data);

	ialign = ibps * FBIT_CHANNELS((intptr_t)f->data);
	oalign = obps * FBIT_CHANNELS((intptr_t)f->data);

	if (count < oalign)
		return (0);

	k = FEEDER_FEED(f->source, c, b, (count / oalign) * ialign, source);
	if (k < ialign)
		return (0);

	k -= k % ialign;
	j = (k / ibps) * obps;
	pad = obps - ibps;
	src = b + k;
	dst = b + j;
	sign = FEEDFMT_SWAP_SIGN(f);

	if (f->desc->out & AFMT_BIGENDIAN) {
		do {
			i = pad;
			do {
				*--dst = 0;
			} while (--i != 0);
			i = ibps;
			while (--i != 0)
				*--dst = *--src;
			*--dst = *--src ^ sign;
		} while (dst != b);
	} else {
		do {
			*--dst = *--src ^ sign;
			i = ibps;
			while (--i != 0)
				*--dst = *--src;
			i = pad;
			do {
				*--dst = 0;
			} while (--i != 0);
		} while (dst != b);
	}

	return (j);
}

static struct pcm_feederdesc feeder_8to16_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_8to16_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_upbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_8to16, 0, NULL);

static struct pcm_feederdesc feeder_8to24_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_8to24_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_upbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_8to24, 0, NULL);

static struct pcm_feederdesc feeder_8to32_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U8, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_8to32_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_upbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_8to32, 0, NULL);

static struct pcm_feederdesc feeder_16to24_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_16to24_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_upbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_16to24, 0, NULL);

static struct pcm_feederdesc feeder_16to32_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_16to32_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_upbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_16to32, 0, NULL);

static struct pcm_feederdesc feeder_24to32_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_LE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_24to32_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_upbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_24to32, 0, NULL);

static int
feed_downbit(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k, sign, be, ibps, ialign, obps, oalign,dump;
	uint8_t *src, *dst, *end;
	uint8_t reservoir[FEEDFMT_RESERVOIR];

	ibps = FBIT_INBPS((intptr_t)f->data);
	obps = FBIT_OUTBPS((intptr_t)f->data);

	ialign = ibps * FBIT_CHANNELS((intptr_t)f->data);
	oalign = obps * FBIT_CHANNELS((intptr_t)f->data);

	if (count < oalign)
		return (0);

	dst = b;
	dump = ibps - obps;
	sign = FEEDFMT_SWAP_SIGN(f);
	be = (f->desc->in & AFMT_BIGENDIAN) ? 1 : 0;
	k = count - (count % oalign);

	do {
		if (k < oalign)
			break;

		if (k < ialign) {
			src = reservoir;
			j = ialign;
		} else {
			src = dst;
			j = k;
		}

		j = FEEDER_FEED(f->source, c, src, j - (j % ialign), source);
		if (j < ialign)
			break;

		j -= j % ialign;
		j *= obps;
		j /= ibps;
		end = dst + j;

		if (be != 0) {
			do {
				*dst++ = *src++ ^ sign;
				i = obps;
				while (--i != 0)
					*dst++ = *src++;
				src += dump;
			} while (dst != end);
		} else {
			do {
				src += dump;
				i = obps;
				while (--i != 0)
					*dst++ = *src++;
				*dst++ = *src++ ^ sign;
			} while (dst != end);
		}

		k -= j;
	} while (k != 0);

	return (dst - b);
}

static struct pcm_feederdesc feeder_16to8_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_16to8_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_downbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_16to8, 0, NULL);

static struct pcm_feederdesc feeder_24to8_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_24to8_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_downbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_24to8, 0, NULL);

static struct pcm_feederdesc feeder_24to16_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_24to16_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_downbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_24to16, 0, NULL);

static struct pcm_feederdesc feeder_32to8_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_32to8_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_downbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_32to8, 0, NULL);

static struct pcm_feederdesc feeder_32to16_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_32to16_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_downbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_32to16, 0, NULL);

static struct pcm_feederdesc feeder_32to24_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_LE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_32to24_methods[] = {
	KOBJMETHOD(feeder_init,		feed_updownbit_init),
	KOBJMETHOD(feeder_feed,		feed_downbit),
	{0, 0}
};
FEEDER_DECLARE(feeder_32to24, 0, NULL);
/*
 * Bit conversion end
 */

/*
 * Channel conversion (mono -> stereo)
 */
static int
feed_monotostereo(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int bps, i, j, k, l;
	uint8_t v;

	bps = (int)((intptr_t)f->data);
	if (count < (bps << 1))
		return (0);

	j = FEEDER_FEED(f->source, c, b, (count - (count % bps)) >> 1, source);
	if (j < bps)
		return (0);

	j -= j % bps;
	i = j << 1;
	l = i;

	do {
		k = bps;
		do {
			v = b[--j];
			b[--i] = v;
			b[i - bps] = v;
		} while (--k != 0);
		i -= bps;
	} while (i != 0);

	return (l);
}

static struct pcm_feederdesc feeder_monotostereo8_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_MU_LAW | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_A_LAW, AFMT_A_LAW | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_monotostereo8_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_monotostereo),
	{0, 0}
};
FEEDER_DECLARE(feeder_monotostereo8, 0, (void *)PCM_8_BPS);

static struct pcm_feederdesc feeder_monotostereo16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_monotostereo16_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_monotostereo),
	{0, 0}
};
FEEDER_DECLARE(feeder_monotostereo16, 0, (void *)PCM_16_BPS);

static struct pcm_feederdesc feeder_monotostereo24_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_S24_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_monotostereo24_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_monotostereo),
	{0, 0}
};
FEEDER_DECLARE(feeder_monotostereo24, 0, (void *)PCM_24_BPS);

static struct pcm_feederdesc feeder_monotostereo32_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_U32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_S32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_monotostereo32_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_monotostereo),
	{0, 0}
};
FEEDER_DECLARE(feeder_monotostereo32, 0, (void *)PCM_32_BPS);
/*
 * Channel conversion (mono -> stereo) end
 */

/*
 * Channel conversion (stereo -> mono)
 */
#define FEEDER_FMT_STEREODOWNMIX(FMTBIT, FMT_INTCAST, SIGN,			\
						SIGNS, ENDIAN, ENDIANS)		\
static void									\
SIGNS##FMTBIT##ENDIANS##_stereodownmix(uint8_t *dst, uint8_t *sx, uint8_t *sy)	\
{										\
	int32_t v;								\
										\
	v = ((FMT_INTCAST)PCM_READ_##SIGN##FMTBIT##_##ENDIAN(sx) +		\
	    PCM_READ_##SIGN##FMTBIT##_##ENDIAN(sy)) >> 1;			\
	PCM_WRITE_##SIGN##FMTBIT##_##ENDIAN(dst, v);				\
}

FEEDER_FMT_STEREODOWNMIX(8, int32_t, S, s, NE, ne);
FEEDER_FMT_STEREODOWNMIX(16, int32_t, S, s, LE, le);
FEEDER_FMT_STEREODOWNMIX(24, int32_t, S, s, LE, le);
FEEDER_FMT_STEREODOWNMIX(32, intpcm_t, S, s, LE, le);
FEEDER_FMT_STEREODOWNMIX(16, int32_t, S, s, BE, be);
FEEDER_FMT_STEREODOWNMIX(24, int32_t, S, s, BE, be);
FEEDER_FMT_STEREODOWNMIX(32, intpcm_t, S, s, BE, be);
FEEDER_FMT_STEREODOWNMIX(8, int32_t, U, u, NE, ne);
FEEDER_FMT_STEREODOWNMIX(16, int32_t, U, u, LE, le);
FEEDER_FMT_STEREODOWNMIX(24, int32_t, U, u, LE, le);
FEEDER_FMT_STEREODOWNMIX(32, intpcm_t, U, u, LE, le);
FEEDER_FMT_STEREODOWNMIX(16, int32_t, U, u, BE, be);
FEEDER_FMT_STEREODOWNMIX(24, int32_t, U, u, BE, be);
FEEDER_FMT_STEREODOWNMIX(32, intpcm_t, U, u, BE, be);

static void
ulaw_stereodownmix(uint8_t *dst, uint8_t *sx, uint8_t *sy)
{
	uint8_t v;

	v = ((uint32_t)ulaw_to_u8_tbl[*sx] + ulaw_to_u8_tbl[*sy]) >> 1;
	*dst = u8_to_ulaw_tbl[v];
}

static void
alaw_stereodownmix(uint8_t *dst, uint8_t *sx, uint8_t *sy)
{
	uint8_t v;

	v = ((uint32_t)alaw_to_u8_tbl[*sx] + alaw_to_u8_tbl[*sy]) >> 1;
	*dst = u8_to_alaw_tbl[v];
}

typedef void (*feed_fmt_stereodownmix_filter)(uint8_t *,
						uint8_t *, uint8_t *);

struct feed_fmt_stereodownmix_info {
	uint32_t format;
	int bps;
	feed_fmt_stereodownmix_filter func[2];
};

static struct feed_fmt_stereodownmix_info feed_fmt_stereodownmix_tbl[] = {
	{ AFMT_S8,     PCM_8_BPS,  { NULL, s8ne_stereodownmix  }},
	{ AFMT_S16_LE, PCM_16_BPS, { NULL, s16le_stereodownmix }},
	{ AFMT_S16_BE, PCM_16_BPS, { NULL, s16be_stereodownmix }},
	{ AFMT_S24_LE, PCM_24_BPS, { NULL, s24le_stereodownmix }},
	{ AFMT_S24_BE, PCM_24_BPS, { NULL, s24be_stereodownmix }},
	{ AFMT_S32_LE, PCM_32_BPS, { NULL, s32le_stereodownmix }},
	{ AFMT_S32_BE, PCM_32_BPS, { NULL, s32be_stereodownmix }},
	{ AFMT_U8,     PCM_8_BPS,  { NULL, u8ne_stereodownmix  }},
	{ AFMT_A_LAW,  PCM_8_BPS,  { NULL, alaw_stereodownmix  }},
	{ AFMT_MU_LAW, PCM_8_BPS,  { NULL, ulaw_stereodownmix  }},
	{ AFMT_U16_LE, PCM_16_BPS, { NULL, u16le_stereodownmix }},
	{ AFMT_U16_BE, PCM_16_BPS, { NULL, u16be_stereodownmix }},
	{ AFMT_U24_LE, PCM_24_BPS, { NULL, u24le_stereodownmix }},
	{ AFMT_U24_BE, PCM_24_BPS, { NULL, u24be_stereodownmix }},
	{ AFMT_U32_LE, PCM_32_BPS, { NULL, u32le_stereodownmix }},
	{ AFMT_U32_BE, PCM_32_BPS, { NULL, u32be_stereodownmix }},
};

#define FSM_DATA(i, j)	((intptr_t)((((i) & 0x1f) << 1) | ((j) & 0x1)))
#define FSM_INFOIDX(m)	(((m) >> 1) & 0x1f)
#define FSM_FUNCIDX(m)	((m) & 0x1)

static int
feed_stereotomono_init(struct pcm_feeder *f)
{
	int i, funcidx;

	if (!(f->desc->in & AFMT_STEREO) || (f->desc->out & AFMT_STEREO))
		return (-1);

	funcidx = (feeder_fmt_stereodownmix != 0) ? 1 : 0;

	for (i = 0; i < sizeof(feed_fmt_stereodownmix_tbl) /
	    sizeof(feed_fmt_stereodownmix_tbl[0]); i++) {
		if (f->desc->out == feed_fmt_stereodownmix_tbl[i].format) {
			f->data = (void *)FSM_DATA(i, funcidx);
			return (0);
		}
	}

	return (-1);
}

static int
feed_stereotomono(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	struct feed_fmt_stereodownmix_info *info;
	feed_fmt_stereodownmix_filter stereodownmix;
	int i, j, k, ibps, obps;
	uint8_t *src, *dst, *end;
	uint8_t reservoir[FEEDFMT_RESERVOIR];

	info = &feed_fmt_stereodownmix_tbl[FSM_INFOIDX((intptr_t)f->data)];
	obps = info->bps;

	if (count < obps)
		return (0);

	stereodownmix = info->func[FSM_FUNCIDX((intptr_t)f->data)];
	ibps = obps << 1;
	dst = b;
	k = count - (count % obps);

	do {
		if (k < obps)
			break;

		if (k < ibps) {
			src = reservoir;
			j = ibps;
		} else {
			src = dst;
			j = k;
		}

		j = FEEDER_FEED(f->source, c, src, j - (j % ibps), source);
		if (j < ibps)
			break;

		j -= j % ibps;
		j >>= 1;
		end = dst + j;

		if (stereodownmix != NULL) {
			do {
				stereodownmix(dst, src, src + obps);
				dst += obps;
				src += ibps;
			} while (dst != end);
		} else {
			do {
				i = obps;
				do {
					*dst++ = *src++;
				} while (--i != 0);
				src += obps;
			} while (dst != end);
		}

		k -= j;
	} while (k != 0);

	return (dst - b);
}

static struct pcm_feederdesc feeder_stereotomono8_desc[] = {
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_A_LAW, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_stereotomono8_methods[] = {
	KOBJMETHOD(feeder_init,		feed_stereotomono_init),
	KOBJMETHOD(feeder_feed,		feed_stereotomono),
	{0, 0}
};
FEEDER_DECLARE(feeder_stereotomono8, 0, NULL);

static struct pcm_feederdesc feeder_stereotomono16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_stereotomono16_methods[] = {
	KOBJMETHOD(feeder_init,		feed_stereotomono_init),
	KOBJMETHOD(feeder_feed,		feed_stereotomono),
	{0, 0}
};
FEEDER_DECLARE(feeder_stereotomono16, 0, NULL);

static struct pcm_feederdesc feeder_stereotomono24_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_S24_BE, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_stereotomono24_methods[] = {
	KOBJMETHOD(feeder_init,		feed_stereotomono_init),
	KOBJMETHOD(feeder_feed,		feed_stereotomono),
	{0, 0}
};
FEEDER_DECLARE(feeder_stereotomono24, 0, NULL);

static struct pcm_feederdesc feeder_stereotomono32_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_S32_BE, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_stereotomono32_methods[] = {
	KOBJMETHOD(feeder_init,		feed_stereotomono_init),
	KOBJMETHOD(feeder_feed,		feed_stereotomono),
	{0, 0}
};
FEEDER_DECLARE(feeder_stereotomono32, 0, NULL);
/*
 * Channel conversion (stereo -> mono) end
 */

/*
 * Sign conversion
 */
static int
feed_sign(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, bps, ofs;

	bps = (int)((intptr_t)f->data);
	if (count < bps)
		return (0);

	i = FEEDER_FEED(f->source, c, b, count - (count % bps), source);
	if (i < bps)
		return (0);

	i -= i % bps;
	j = i;
	ofs = (f->desc->in & AFMT_BIGENDIAN) ? bps : 1;

	do {
		b[i - ofs] ^= 0x80;
		i -= bps;
	} while (i != 0);

	return (j);
}
static struct pcm_feederdesc feeder_sign8_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_sign8_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_sign),
	{0, 0}
};
FEEDER_DECLARE(feeder_sign8, 0, (void *)PCM_8_BPS);

static struct pcm_feederdesc feeder_sign16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_sign16_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_sign),
	{0, 0}
};
FEEDER_DECLARE(feeder_sign16, 0, (void *)PCM_16_BPS);

static struct pcm_feederdesc feeder_sign24_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_sign24_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_sign),
	{0, 0}
};
FEEDER_DECLARE(feeder_sign24, 0, (void *)PCM_24_BPS);

static struct pcm_feederdesc feeder_sign32_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_sign32_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_sign),
	{0, 0}
};
FEEDER_DECLARE(feeder_sign32, 0, (void *)PCM_32_BPS);

/*
 * Endian conversion
 */
static int
feed_endian(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k, bps;
	uint8_t *buf, v;

	bps = (int)((intptr_t)f->data);
	if (count < bps)
		return (0);

	k = FEEDER_FEED(f->source, c, b, count - (count % bps), source);
	if (k < bps)
		return (0);

	k -= k % bps;
	j = bps >> 1;
	buf = b + k;

	do {
		buf -= bps;
		i = j;
		do {
			v = buf[--i];
			buf[i] = buf[bps - i - 1];
			buf[bps - i - 1] = v;
		} while (i != 0);
	} while (buf != b);

	return (k);
}
static struct pcm_feederdesc feeder_endian16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_endian16_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_endian),
	{0, 0}
};
FEEDER_DECLARE(feeder_endian16, 0, (void *)PCM_16_BPS);

static struct pcm_feederdesc feeder_endian24_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_U24_LE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_S24_LE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_U24_BE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_S24_BE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_endian24_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_endian),
	{0, 0}
};
FEEDER_DECLARE(feeder_endian24, 0, (void *)PCM_24_BPS);

static struct pcm_feederdesc feeder_endian32_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_U32_LE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_S32_LE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_U32_BE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_S32_BE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_endian32_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_endian),
	{0, 0}
};
FEEDER_DECLARE(feeder_endian32, 0, (void *)PCM_32_BPS);
/*
 * Endian conversion end
 */

/*
 * L/R swap conversion
 */
static int
feed_swaplr(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
						uint32_t count, void *source)
{
	int i, j, bps, smpsz;
	uint8_t *buf, v;

	bps = (int)((intptr_t)f->data);
	smpsz = bps << 1;
	if (count < smpsz)
		return (0);

	j = FEEDER_FEED(f->source, c, b, count - (count % smpsz), source);
	if (j < smpsz)
		return (0);

	j -= j % smpsz;
	buf = b + j;

	do {
		buf -= smpsz;
		i = bps;
		do {
			v = buf[--i];
			buf[i] = buf[bps + i];
			buf[bps + i] = v;
		} while (i != 0);
	} while (buf != b);

	return (j);
}

static struct pcm_feederdesc feeder_swaplr8_desc[] = {
	{FEEDER_SWAPLR, AFMT_S8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_U8 | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_A_LAW | AFMT_STEREO, AFMT_A_LAW | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_MU_LAW | AFMT_STEREO, AFMT_A_LAW | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_swaplr8_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_swaplr),
	{0, 0}
};
FEEDER_DECLARE(feeder_swaplr8, -1, (void *)PCM_8_BPS);

static struct pcm_feederdesc feeder_swaplr16_desc[] = {
	{FEEDER_SWAPLR, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_swaplr16_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_swaplr),
	{0, 0}
};
FEEDER_DECLARE(feeder_swaplr16, -1, (void *)PCM_16_BPS);

static struct pcm_feederdesc feeder_swaplr24_desc[] = {
	{FEEDER_SWAPLR, AFMT_S24_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_S24_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_U24_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_U24_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_swaplr24_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_swaplr),
	{0, 0}
};
FEEDER_DECLARE(feeder_swaplr24, -1, (void *)PCM_24_BPS);

static struct pcm_feederdesc feeder_swaplr32_desc[] = {
	{FEEDER_SWAPLR, AFMT_S32_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_S32_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_U32_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_SWAPLR, AFMT_U32_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_swaplr32_methods[] = {
	KOBJMETHOD(feeder_feed,		feed_swaplr),
	{0, 0}
};
FEEDER_DECLARE(feeder_swaplr32, -1, (void *)PCM_32_BPS);
/*
 * L/R swap conversion end
 */
