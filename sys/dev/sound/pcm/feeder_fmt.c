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
 *
 * *New* and rewritten soft format converter, supporting 24/32bit pcm data,
 * simplified and optimized.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 * This new implementation is fully dedicated in memory of Cameron Grant,  *
 * the creator of magnificent, highly addictive feeder infrastructure.     *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */

#include <dev/sound/pcm/sound.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

MALLOC_DEFINE(M_FMTFEEDER, "fmtfeed", "pcm format feeder");

#define FEEDBUFSZ	8192
#define FEEDBUF24SZ	8190

#define FMT_TRACE(x...) /* printf(x) */
#define FMT_TEST(x, y...) /* if (x) FMT_TRACE(y) */
#define FMT_ALIGNBYTE(x) /* x */

/*
 * Sign inverted ulaw/alaw -> 8 table
 */
static uint8_t ulaw_to_s8_tbl[] = {
  131,  135,  139,  143,  147,  151,  155,  159,
  163,  167,  171,  175,  179,  183,  187,  191,
  194,  196,  198,  200,  202,  204,  206,  208,
  210,  212,  214,  216,  218,  220,  222,  224,
  226,  227,  228,  229,  230,  231,  232,  233,
  234,  235,  236,  237,  238,  239,  240,  241,
  241,  242,  242,  243,  243,  244,  244,  245,
  245,  246,  246,  247,  247,  248,  248,  249,
  249,  249,  250,  250,  250,  250,  251,  251,
  251,  251,  252,  252,  252,  252,  253,  253,
  253,  253,  253,  253,  254,  254,  254,  254,
  254,  254,  254,  254,  255,  255,  255,  255,
  255,  255,  255,  255,  255,  255,  255,  255,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
  125,  121,  117,  113,  109,  105,  101,   97,
   93,   89,   85,   81,   77,   73,   69,   65,
   62,   60,   58,   56,   54,   52,   50,   48,
   46,   44,   42,   40,   38,   36,   34,   32,
   30,   29,   28,   27,   26,   25,   24,   23,
   22,   21,   20,   19,   18,   17,   16,   15,
   15,   14,   14,   13,   13,   12,   12,   11,
   11,   10,   10,    9,    9,    8,    8,    7,
    7,    7,    6,    6,    6,    6,    5,    5,
    5,    5,    4,    4,    4,    4,    3,    3,
    3,    3,    3,    3,    2,    2,    2,    2,
    2,    2,    2,    2,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
};

static uint8_t alaw_to_s8_tbl[] = {
  236,  237,  234,  235,  240,  241,  238,  239,
  228,  229,  226,  227,  232,  233,  230,  231,
  246,  246,  245,  245,  248,  248,  247,  247,
  242,  242,  241,  241,  244,  244,  243,  243,
  171,  175,  163,  167,  187,  191,  179,  183,
  139,  143,  131,  135,  155,  159,  147,  151,
  214,  216,  210,  212,  222,  224,  218,  220,
  198,  200,  194,  196,  206,  208,  202,  204,
  255,  255,  255,  255,  255,  255,  255,  255,
  255,  255,  255,  255,  255,  255,  255,  255,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
  251,  251,  251,  251,  252,  252,  252,  252,
  249,  249,  249,  249,  250,  250,  250,  250,
  254,  254,  254,  254,  254,  254,  254,  254,
  253,  253,  253,  253,  253,  253,  253,  253,
   20,   19,   22,   21,   16,   15,   18,   17,
   28,   27,   30,   29,   24,   23,   26,   25,
   10,   10,   11,   11,    8,    8,    9,    9,
   14,   14,   15,   15,   12,   12,   13,   13,
   85,   81,   93,   89,   69,   65,   77,   73,
  117,  113,  125,  121,  101,   97,  109,  105,
   42,   40,   46,   44,   34,   32,   38,   36,
   58,   56,   62,   60,   50,   48,   54,   52,
    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    5,    5,    5,    5,    4,    4,    4,    4,
    7,    7,    7,    7,    6,    6,    6,    6,
    2,    2,    2,    2,    2,    2,    2,    2,
    3,    3,    3,    3,    3,    3,    3,    3,
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
feed_table_u8(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int j, k = FEEDER_FEED(f->source, c, b, count, source);
	uint8_t *tbl = (uint8_t *)f->data;
	
	j = k;
	while (j > 0) {
		j--;
		b[j] = tbl[b[j]] ^ 0x80;
	}
	return k;
}

static int
feed_table_s16le(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k = FEEDER_FEED(f->source, c, b, count >> 1, source);
	uint8_t *tbl = (uint8_t *)f->data;
	
	i = k;
	k <<= 1;
	j = k;
	while (i > 0) {
		b[--j] = tbl[b[--i]];
		b[--j] = 0;
	}
	return k;
}

static int
feed_table_xlaw(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int j, k = FEEDER_FEED(f->source, c, b, count, source);
	uint8_t *tbl = (uint8_t *)f->data;

	j = k;
	while (j > 0) {
		j--;
		b[j] = tbl[b[j]];
	}
	return k;
}

static struct pcm_feederdesc feeder_ulawtou8_desc[] = {
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_MU_LAW|AFMT_STEREO, AFMT_U8|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_ulawtou8_methods[] = {
	KOBJMETHOD(feeder_feed, feed_table_u8),
	{0, 0}
};
FEEDER_DECLARE(feeder_ulawtou8, 0, ulaw_to_s8_tbl);

static struct pcm_feederdesc feeder_alawtou8_desc[] = {
	{FEEDER_FMT, AFMT_A_LAW, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_A_LAW|AFMT_STEREO, AFMT_U8|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_alawtou8_methods[] = {
	KOBJMETHOD(feeder_feed, feed_table_u8),
	{0, 0}
};
FEEDER_DECLARE(feeder_alawtou8, 0, alaw_to_s8_tbl);

static struct pcm_feederdesc feeder_ulawtos16le_desc[] = {
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_MU_LAW|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_ulawtos16le_methods[] = {
	KOBJMETHOD(feeder_feed, feed_table_s16le),
	{0, 0}
};
FEEDER_DECLARE(feeder_ulawtos16le, 0, ulaw_to_s8_tbl);

static struct pcm_feederdesc feeder_alawtos16le_desc[] = {
	{FEEDER_FMT, AFMT_A_LAW, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_A_LAW|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_alawtos16le_methods[] = {
	KOBJMETHOD(feeder_feed, feed_table_s16le),
	{0, 0}
};
FEEDER_DECLARE(feeder_alawtos16le, 0, alaw_to_s8_tbl);

static struct pcm_feederdesc feeder_u8toulaw_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_U8|AFMT_STEREO, AFMT_MU_LAW|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_u8toulaw_methods[] = {
	KOBJMETHOD(feeder_feed, feed_table_xlaw),
	{0, 0}
};
FEEDER_DECLARE(feeder_u8toulaw, 0, u8_to_ulaw_tbl);

static struct pcm_feederdesc feeder_u8toalaw_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_A_LAW, 0},
	{FEEDER_FMT, AFMT_U8|AFMT_STEREO, AFMT_A_LAW|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_u8toalaw_methods[] = {
	KOBJMETHOD(feeder_feed, feed_table_xlaw),
	{0, 0}
};
FEEDER_DECLARE(feeder_u8toalaw, 0, u8_to_alaw_tbl);

/*
 * Conversion rules:-
 *	1. BE -> LE
 *	2. if fmt == u8 , u8 -> s8 (economical)
 *	3. Xle -> 16le
 *	4. if fmt != u8 && fmt == u16le , u16le -> s16le
 *	4. s16le mono -> s16le stereo
 *
 * All conversion done in byte level to preserve endianess.
 */

static int
feed_common_init(struct pcm_feeder *f)
{
	f->data = malloc(FEEDBUFSZ, M_FMTFEEDER, M_NOWAIT|M_ZERO);
	if (f->data == NULL)
		return ENOMEM;
	return 0;
}

static int
feed_common_free(struct pcm_feeder *f)
{
	if (f->data)
		free(f->data, M_FMTFEEDER);
	f->data = NULL;
	return 0;
}

/*
 * Bit conversion
 */
static int
feed_8to16le(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k = FEEDER_FEED(f->source, c, b, count >> 1, source);
	
	i = k;
	k <<= 1;
	j = k;
	while (i > 0) {
		b[--j] = b[--i];
		b[--j] = 0;
	}
	return k;
}
static struct pcm_feederdesc feeder_8to16le_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U8|AFMT_STEREO, AFMT_U16_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S8|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_8to16le_methods[] = {
	KOBJMETHOD(feeder_feed, feed_8to16le),
	{0, 0}
};
FEEDER_DECLARE(feeder_8to16le, 0, NULL);

static int
feed_16leto8(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k;
	uint8_t *src = (uint8_t *)f->data;
	
	k = count << 1;
	k = FEEDER_FEED(f->source, c, src, min(k, FEEDBUFSZ), source);
	if (k < 2) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k & 1, "%s: Bytes not 16bit aligned.\n", __func__);
	FMT_ALIGNBYTE(k &= ~1);
	i = k;
	j = k >> 1;
	while (i > 0) {
		b[--j] = src[--i];
		i--;
	}
	return k >> 1;
}
static struct pcm_feederdesc feeder_16leto8_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U16_LE|AFMT_STEREO, AFMT_U8|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S16_LE|AFMT_STEREO, AFMT_S8|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_16leto8_methods[] = {
	KOBJMETHOD(feeder_init, feed_common_init),
	KOBJMETHOD(feeder_free, feed_common_free),
	KOBJMETHOD(feeder_feed, feed_16leto8),
	{0, 0}
};
FEEDER_DECLARE(feeder_16leto8, 0, NULL);

static int
feed_16leto24le(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k;

	k = (count / 3) << 1;
	k = FEEDER_FEED(f->source, c, b, k, source);
	if (k < 2) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k & 1, "%s: Bytes not 16bit aligned.\n", __func__);
	FMT_ALIGNBYTE(k &= ~1);
	i = k;
	j = (k >> 1) * 3;
	k = j;
	while (i > 0) {
		b[--j] = b[--i];
		b[--j] = b[--i];
		b[--j] = 0;
	}
	return k;
}
static struct pcm_feederdesc feeder_16leto24le_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE|AFMT_STEREO, AFMT_U24_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE|AFMT_STEREO, AFMT_S24_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_16leto24le_methods[] = {
	KOBJMETHOD(feeder_feed, feed_16leto24le),
	{0, 0}
};
FEEDER_DECLARE(feeder_16leto24le, 0, NULL);

static int
feed_24leto16le(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k;
	uint8_t *src = (uint8_t *)f->data;

	k = (count * 3) >> 1;
	k = FEEDER_FEED(f->source, c, src, min(k, FEEDBUF24SZ), source);
	if (k < 3) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k % 3, "%s: Bytes not 24bit aligned.\n", __func__);
	FMT_ALIGNBYTE(k -= k % 3);
	i = (k / 3) << 1;
	j = i;
	while (i > 0) {
		b[--i] = src[--k];
		b[--i] = src[--k];
		k--;
	}
	return j;
}
static struct pcm_feederdesc feeder_24leto16le_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U24_LE|AFMT_STEREO, AFMT_U16_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S24_LE|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_24leto16le_methods[] = {
	KOBJMETHOD(feeder_init, feed_common_init),
	KOBJMETHOD(feeder_free, feed_common_free),
	KOBJMETHOD(feeder_feed, feed_24leto16le),
	{0, 0}
};
FEEDER_DECLARE(feeder_24leto16le, 1, NULL);

static int
feed_16leto32le(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k = FEEDER_FEED(f->source, c, b, count >> 1, source);
	if (k < 2) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k & 1, "%s: Bytes not 16bit aligned.\n", __func__);
	FMT_ALIGNBYTE(k &= ~1);
	i = k;
	j = k << 1;
	k = j;
	while (i > 0) {
		b[--j] = b[--i];
		b[--j] = b[--i];
		b[--j] = 0;
		b[--j] = 0;
	}
	return k;
}
static struct pcm_feederdesc feeder_16leto32le_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE|AFMT_STEREO, AFMT_U32_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE|AFMT_STEREO, AFMT_S32_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_16leto32le_methods[] = {
	KOBJMETHOD(feeder_feed, feed_16leto32le),
	{0, 0}
};
FEEDER_DECLARE(feeder_16leto32le, 0, NULL);

static int
feed_32leto16le(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k;
	uint8_t *src = (uint8_t *)f->data;

	k = count << 1;
	k = FEEDER_FEED(f->source, c, src, min(k, FEEDBUFSZ), source);
	if (k < 4) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k & 3, "%s: Bytes not 32bit aligned.\n", __func__);
	FMT_ALIGNBYTE(k &= ~3);
	i = k;
	k >>= 1;
	j = k;
	while (i > 0) {
		b[--j] = src[--i];
		b[--j] = src[--i];
		i -= 2;
	}
	return k;
}
static struct pcm_feederdesc feeder_32leto16le_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U32_LE|AFMT_STEREO, AFMT_U16_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S32_LE|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_32leto16le_methods[] = {
	KOBJMETHOD(feeder_init, feed_common_init),
	KOBJMETHOD(feeder_free, feed_common_free),
	KOBJMETHOD(feeder_feed, feed_32leto16le),
	{0, 0}
};
FEEDER_DECLARE(feeder_32leto16le, 1, NULL);
/*
 * Bit conversion end
 */

/*
 * Channel conversion (mono -> stereo)
 */
static int
feed_monotostereo8(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k = FEEDER_FEED(f->source, c, b, count >> 1, source);

	i = k;
	j = k << 1;
	while (i > 0) {
		b[--j] = b[--i];
		b[--j] = b[i];
	}
	return k << 1;
}
static struct pcm_feederdesc feeder_monotostereo8_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U8|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S8|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_monotostereo8_methods[] = {
	KOBJMETHOD(feeder_feed, feed_monotostereo8),
	{0, 0}
};
FEEDER_DECLARE(feeder_monotostereo8, 0, NULL);

static int
feed_monotostereo16(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k = FEEDER_FEED(f->source, c, b, count >> 1, source);
	uint8_t l, m;

	if (k < 2) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k & 1, "%s: Bytes not 16bit aligned.\n", __func__);
	FMT_ALIGNBYTE(k &= ~1);
	i = k;
	j = k << 1;
	while (i > 0) {
		l = b[--i];
		m = b[--i];
		b[--j] = l;
		b[--j] = m;
		b[--j] = l;
		b[--j] = m;
	}
	return k << 1;
}
static struct pcm_feederdesc feeder_monotostereo16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U16_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S16_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U16_BE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S16_BE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_monotostereo16_methods[] = {
	KOBJMETHOD(feeder_feed, feed_monotostereo16),
	{0, 0}
};
FEEDER_DECLARE(feeder_monotostereo16, 0, NULL);
/*
 * Channel conversion (mono -> stereo) end
 */

/*
 * Channel conversion (stereo -> mono)
 */
static int
feed_stereotomono8(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k;
	uint8_t *src = (uint8_t *)f->data;

	k = count << 1;
	k = FEEDER_FEED(f->source, c, src, min(k, FEEDBUFSZ), source);
	if (k < 2) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k & 1, "%s: Bytes not 8bit (stereo) aligned.\n", __func__);
	FMT_ALIGNBYTE(k &= ~1);
	i = k >> 1;
	j = i;
	while (i > 0) {
		k--;
		b[--i] = src[--k];
	}
	return j;
}
static struct pcm_feederdesc feeder_stereotomono8_desc[] = {
	{FEEDER_FMT, AFMT_U8|AFMT_STEREO, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8|AFMT_STEREO, AFMT_S8, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_stereotomono8_methods[] = {
	KOBJMETHOD(feeder_init,	feed_common_init),
	KOBJMETHOD(feeder_free,	feed_common_free),
	KOBJMETHOD(feeder_feed,	feed_stereotomono8),
	{0, 0}
};
FEEDER_DECLARE(feeder_stereotomono8, 0, NULL);

static int
feed_stereotomono16(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j, k;
	uint8_t *src = (uint8_t *)f->data;

	k = count << 1;
	k = FEEDER_FEED(f->source, c, src, min(k, FEEDBUFSZ), source);
	if (k < 4) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
		return 0;
	}
	FMT_TEST(k & 3, "%s: Bytes not 16bit (stereo) aligned.\n", __func__);
	FMT_ALIGNBYTE(k &= ~3);
	i = k >> 1;
	j = i;
	while (i > 0) {
		k -= 2;
		b[--i] = src[--k];
		b[--i] = src[--k];
	}
	return j;
}
static struct pcm_feederdesc feeder_stereotomono16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE|AFMT_STEREO, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE|AFMT_STEREO, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_BE|AFMT_STEREO, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE|AFMT_STEREO, AFMT_S16_BE, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_stereotomono16_methods[] = {
	KOBJMETHOD(feeder_init,	feed_common_init),
	KOBJMETHOD(feeder_free,	feed_common_free),
	KOBJMETHOD(feeder_feed,	feed_stereotomono16),
	{0, 0}
};
FEEDER_DECLARE(feeder_stereotomono16, 0, NULL);
/*
 * Channel conversion (stereo -> mono) end
 */

/*
 * Sign conversion
 */
static int
feed_sign8(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j = FEEDER_FEED(f->source, c, b, count, source);

	i = j;
	while (i > 0)
		b[--i] ^= 0x80;
	return j;
}
static struct pcm_feederdesc feeder_sign8_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U8|AFMT_STEREO, AFMT_S8|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8|AFMT_STEREO, AFMT_U8|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_sign8_methods[] = {
	KOBJMETHOD(feeder_feed, feed_sign8),
	{0, 0}
};
FEEDER_DECLARE(feeder_sign8, 0, NULL);

static int
feed_sign16le(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j = FEEDER_FEED(f->source, c, b, count, source);

	if (j < 2) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, j);
		return 0;
	}
	FMT_TEST(j & 1, "%s: Bytes not 16bit aligned.\n", __func__);
	FMT_ALIGNBYTE(j &= ~1);
	i = j;
	while (i > 0) {
		b[--i] ^= 0x80;
		i--;
	}
	return j;
}
static struct pcm_feederdesc feeder_sign16le_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE|AFMT_STEREO, AFMT_U16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_sign16le_methods[] = {
	KOBJMETHOD(feeder_feed, feed_sign16le),
	{0, 0}
};
FEEDER_DECLARE(feeder_sign16le, 0, NULL);
/*
 * Sign conversion end.
 */

/*
 * Endian conversion.
 */
static int
feed_endian16(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j = FEEDER_FEED(f->source, c, b, count, source);
	uint8_t v;

	if (j < 2) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, j);
		return 0;
	}
	FMT_TEST(j & 1, "%s: Bytes not 16bit aligned.\n", __func__);
	FMT_ALIGNBYTE(j &= ~1);
	i = j;
	while (i > 0) {
		v = b[--i];
		b[i] = b[i - 1];
		b[--i] = v;
	}
	return j;
}
static struct pcm_feederdesc feeder_endian16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_U16_LE|AFMT_STEREO, AFMT_U16_BE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S16_BE, 0},
	{FEEDER_FMT, AFMT_S16_LE|AFMT_STEREO, AFMT_S16_BE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U16_BE|AFMT_STEREO, AFMT_U16_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S16_BE|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_endian16_methods[] = {
	KOBJMETHOD(feeder_feed, feed_endian16),
	{0, 0}
};
FEEDER_DECLARE(feeder_endian16, 0, NULL);

static int
feed_endian24(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j = FEEDER_FEED(f->source, c, b, count, source);
	uint8_t v;

	if (j < 3) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, j);
		return 0;
	}
	FMT_TEST(j % 3, "%s: Bytes not 24bit aligned.\n", __func__);
	FMT_ALIGNBYTE(j -= j % 3);
	i = j;
	while (i > 0) {
		v = b[--i];
		b[i] = b[i - 2];
		b[i -= 2] = v;
	}
	return j;
}
static struct pcm_feederdesc feeder_endian24_desc[] = {
	{FEEDER_FMT, AFMT_U24_LE, AFMT_U24_BE, 0},
	{FEEDER_FMT, AFMT_U24_LE|AFMT_STEREO, AFMT_U24_BE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_LE, AFMT_S24_BE, 0},
	{FEEDER_FMT, AFMT_S24_LE|AFMT_STEREO, AFMT_S24_BE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U24_BE, AFMT_U24_LE, 0},
	{FEEDER_FMT, AFMT_U24_BE|AFMT_STEREO, AFMT_U24_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S24_BE, AFMT_S24_LE, 0},
	{FEEDER_FMT, AFMT_S24_BE|AFMT_STEREO, AFMT_S24_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_endian24_methods[] = {
	KOBJMETHOD(feeder_feed, feed_endian24),
	{0, 0}
};
FEEDER_DECLARE(feeder_endian24, 0, NULL);

static int
feed_endian32(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	int i, j = FEEDER_FEED(f->source, c, b, count, source);
	uint8_t l, m;

	if (j < 4) {
		FMT_TRACE("%s: Not enough data (Got: %d bytes)\n",
				__func__, j);
		return 0;
	}
	FMT_TEST(j & 3, "%s: Bytes not 32bit aligned.\n", __func__);
	FMT_ALIGNBYTE(j &= ~3);
	i = j;
	while (i > 0) {
		l = b[--i];
		m = b[--i];
		b[i] = b[i - 1];
		b[i + 1] = b[i - 2];
		b[--i] = m;
		b[--i] = l;
	}
	return j;
}
static struct pcm_feederdesc feeder_endian32_desc[] = {
	{FEEDER_FMT, AFMT_U32_LE, AFMT_U32_BE, 0},
	{FEEDER_FMT, AFMT_U32_LE|AFMT_STEREO, AFMT_U32_BE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_LE, AFMT_S32_BE, 0},
	{FEEDER_FMT, AFMT_S32_LE|AFMT_STEREO, AFMT_S32_BE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U32_BE, AFMT_U32_LE, 0},
	{FEEDER_FMT, AFMT_U32_BE|AFMT_STEREO, AFMT_U32_LE|AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S32_BE, AFMT_S32_LE, 0},
	{FEEDER_FMT, AFMT_S32_BE|AFMT_STEREO, AFMT_S32_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_endian32_methods[] = {
	KOBJMETHOD(feeder_feed, feed_endian32),
	{0, 0}
};
FEEDER_DECLARE(feeder_endian32, 0, NULL);
/*
 * Endian conversion end
 */
