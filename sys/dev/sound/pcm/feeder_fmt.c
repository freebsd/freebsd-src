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
 */

#include <dev/sound/pcm/sound.h>

#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

MALLOC_DEFINE(M_FMTFEEDER, "fmtfeed", "pcm format feeder");

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

static unsigned char alaw_to_ulaw[] = {
   42,   43,   40,   41,   46,   47,   44,   45,
   34,   35,   32,   33,   38,   39,   36,   37,
   57,   58,   55,   56,   61,   62,   59,   60,
   49,   50,   48,   48,   53,   54,   51,   52,
   10,   11,    8,    9,   14,   15,   12,   13,
    2,    3,    0,    1,    6,    7,    4,    5,
   26,   27,   24,   25,   30,   31,   28,   29,
   18,   19,   16,   17,   22,   23,   20,   21,
   98,   99,   96,   97,  102,  103,  100,  101,
   93,   93,   92,   92,   95,   95,   94,   94,
  116,  118,  112,  114,  124,  126,  120,  122,
  106,  107,  104,  105,  110,  111,  108,  109,
   72,   73,   70,   71,   76,   77,   74,   75,
   64,   65,   63,   63,   68,   69,   66,   67,
   86,   87,   84,   85,   90,   91,   88,   89,
   79,   79,   78,   78,   82,   83,   80,   81,
  170,  171,  168,  169,  174,  175,  172,  173,
  162,  163,  160,  161,  166,  167,  164,  165,
  185,  186,  183,  184,  189,  190,  187,  188,
  177,  178,  176,  176,  181,  182,  179,  180,
  138,  139,  136,  137,  142,  143,  140,  141,
  130,  131,  128,  129,  134,  135,  132,  133,
  154,  155,  152,  153,  158,  159,  156,  157,
  146,  147,  144,  145,  150,  151,  148,  149,
  226,  227,  224,  225,  230,  231,  228,  229,
  221,  221,  220,  220,  223,  223,  222,  222,
  244,  246,  240,  242,  252,  254,  248,  250,
  234,  235,  232,  233,  238,  239,  236,  237,
  200,  201,  198,  199,  204,  205,  202,  203,
  192,  193,  191,  191,  196,  197,  194,  195,
  214,  215,  212,  213,  218,  219,  216,  217,
  207,  207,  206,  206,  210,  211,  208,  209,
};

static unsigned char ulaw_to_alaw[] = {
   42,   43,   40,   41,   46,   47,   44,   45,
   34,   35,   32,   33,   38,   39,   36,   37,
   58,   59,   56,   57,   62,   63,   60,   61,
   50,   51,   48,   49,   54,   55,   52,   53,
   10,   11,    8,    9,   14,   15,   12,   13,
    2,    3,    0,    1,    6,    7,    4,    5,
   27,   24,   25,   30,   31,   28,   29,   18,
   19,   16,   17,   22,   23,   20,   21,  106,
  104,  105,  110,  111,  108,  109,   98,   99,
   96,   97,  102,  103,  100,  101,  122,  120,
  126,  127,  124,  125,  114,  115,  112,  113,
  118,  119,  116,  117,   75,   73,   79,   77,
   66,   67,   64,   65,   70,   71,   68,   69,
   90,   91,   88,   89,   94,   95,   92,   93,
   82,   82,   83,   83,   80,   80,   81,   81,
   86,   86,   87,   87,   84,   84,   85,   85,
  170,  171,  168,  169,  174,  175,  172,  173,
  162,  163,  160,  161,  166,  167,  164,  165,
  186,  187,  184,  185,  190,  191,  188,  189,
  178,  179,  176,  177,  182,  183,  180,  181,
  138,  139,  136,  137,  142,  143,  140,  141,
  130,  131,  128,  129,  134,  135,  132,  133,
  155,  152,  153,  158,  159,  156,  157,  146,
  147,  144,  145,  150,  151,  148,  149,  234,
  232,  233,  238,  239,  236,  237,  226,  227,
  224,  225,  230,  231,  228,  229,  250,  248,
  254,  255,  252,  253,  242,  243,  240,  241,
  246,  247,  244,  245,  203,  201,  207,  205,
  194,  195,  192,  193,  198,  199,  196,  197,
  218,  219,  216,  217,  222,  223,  220,  221,
  210,  210,  211,  211,  208,  208,  209,  209,
  214,  214,  215,  215,  212,  212,  213,  213,
};

/*****************************************************************************/

static int
feed_8to16le(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	int i, j, k;

	k = FEEDER_FEED(f->source, c, b, count / 2, source);
	j = k - 1;
	i = j * 2 + 1;
	while (i > 0 && j >= 0) {
		b[i--] = b[j--];
		b[i--] = 0;
	}
	return k * 2;
}

static struct pcm_feederdesc feeder_8to16le_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_8to16le_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_8to16le),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_8to16le, 0, NULL);

/*****************************************************************************/

static int
feed_16to8_init(struct pcm_feeder *f)
{
	f->data = malloc(FEEDBUFSZ, M_FMTFEEDER, M_NOWAIT | M_ZERO);
	return (f->data)? 0 : ENOMEM;
}

static int
feed_16to8_free(struct pcm_feeder *f)
{
	if (f->data)
		free(f->data, M_FMTFEEDER);
	f->data = NULL;
	return 0;
}

static int
feed_16leto8(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	u_int32_t i = 0, toget = count * 2;
	int j = 1, k;

	k = FEEDER_FEED(f->source, c, f->data, min(toget, FEEDBUFSZ), source);
	while (j < k) {
		b[i++] = ((u_int8_t *)f->data)[j];
		j += 2;
	}
	return i;
}

static struct pcm_feederdesc feeder_16leto8_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_16leto8_methods[] = {
    	KOBJMETHOD(feeder_init,		feed_16to8_init),
    	KOBJMETHOD(feeder_free,		feed_16to8_free),
    	KOBJMETHOD(feeder_feed,		feed_16leto8),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_16leto8, 1, NULL);

/*****************************************************************************/

static int
feed_monotostereo8(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	int i, j, k = FEEDER_FEED(f->source, c, b, count / 2, source);

	j = k - 1;
	i = j * 2 + 1;
	while (i > 0 && j >= 0) {
		b[i--] = b[j];
		b[i--] = b[j];
		j--;
	}
	return k * 2;
}

static struct pcm_feederdesc feeder_monotostereo8_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_S8 | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_monotostereo8_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_monotostereo8),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_monotostereo8, 0, NULL);

/*****************************************************************************/

static int
feed_monotostereo16(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	int i, j, k = FEEDER_FEED(f->source, c, b, count / 2, source);
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

static struct pcm_feederdesc feeder_monotostereo16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_U16_BE, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_BE, AFMT_S16_BE | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_monotostereo16_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_monotostereo16),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_monotostereo16, 0, NULL);

/*****************************************************************************/

static int
feed_stereotomono8_init(struct pcm_feeder *f)
{
	f->data = malloc(FEEDBUFSZ, M_FMTFEEDER, M_NOWAIT | M_ZERO);
	return (f->data)? 0 : ENOMEM;
}

static int
feed_stereotomono8_free(struct pcm_feeder *f)
{
	if (f->data)
		free(f->data, M_FMTFEEDER);
	f->data = NULL;
	return 0;
}

static int
feed_stereotomono8(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	u_int32_t i = 0, toget = count * 2;
	int j = 0, k;

	k = FEEDER_FEED(f->source, c, f->data, min(toget, FEEDBUFSZ), source);
	while (j < k) {
		b[i++] = ((u_int8_t *)f->data)[j];
		j += 2;
	}
	return i;
}

static struct pcm_feederdesc feeder_stereotomono8_desc[] = {
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_S8, 0},
	{0},
};
static kobj_method_t feeder_stereotomono8_methods[] = {
    	KOBJMETHOD(feeder_init,		feed_stereotomono8_init),
    	KOBJMETHOD(feeder_free,		feed_stereotomono8_free),
    	KOBJMETHOD(feeder_feed,		feed_stereotomono8),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_stereotomono8, 1, NULL);

/*****************************************************************************/

static int
feed_stereotomono16_init(struct pcm_feeder *f)
{
	f->data = malloc(FEEDBUFSZ, M_FMTFEEDER, M_NOWAIT | M_ZERO);
	return (f->data)? 0 : ENOMEM;
}

static int
feed_stereotomono16_free(struct pcm_feeder *f)
{
	if (f->data)
		free(f->data, M_FMTFEEDER);
	f->data = NULL;
	return 0;
}

static int
feed_stereotomono16(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	u_int32_t i = 0, toget = count * 2;
	int j = 0, k;

	k = FEEDER_FEED(f->source, c, f->data, min(toget, FEEDBUFSZ), source);
	while (j < k) {
		b[i++] = ((u_int8_t *)f->data)[j];
		b[i++] = ((u_int8_t *)f->data)[j + 1];
		j += 4;
	}
	return i;
}

static struct pcm_feederdesc feeder_stereotomono16_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_BE, 0},
	{FEEDER_FMT, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE, 0},
	{0},
};
static kobj_method_t feeder_stereotomono16_methods[] = {
    	KOBJMETHOD(feeder_init,		feed_stereotomono16_init),
    	KOBJMETHOD(feeder_free,		feed_stereotomono16_free),
    	KOBJMETHOD(feeder_feed,		feed_stereotomono16),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_stereotomono16, 1, NULL);

/*****************************************************************************/

static int
feed_endian(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	u_int8_t t;
	int i = 0, j = FEEDER_FEED(f->source, c, b, count, source);

	while (i < j) {
		t = b[i];
		b[i] = b[i + 1];
		b[i + 1] = t;
		i += 2;
	}
	return i;
}

static struct pcm_feederdesc feeder_endian_desc[] = {
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
static kobj_method_t feeder_endian_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_endian),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_endian, 0, NULL);

/*****************************************************************************/

static int
feed_sign(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	int i = 0, j = FEEDER_FEED(f->source, c, b, count, source);
	intptr_t ssz = (intptr_t)f->data, ofs = ssz - 1;

	while (i < j) {
		b[i + ofs] ^= 0x80;
		i += ssz;
	}
	return i;
}

static struct pcm_feederdesc feeder_sign8_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_S8, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S8, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_S8 | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_sign8_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_sign),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_sign8, 0, (void *)1);

static struct pcm_feederdesc feeder_sign16le_desc[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_sign16le_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_sign),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_sign16le, -1, (void *)2);

/*****************************************************************************/

static int
feed_table(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	int i = 0, j = FEEDER_FEED(f->source, c, b, count, source);

	while (i < j) {
		b[i] = ((u_int8_t *)f->data)[b[i]];
		i++;
	}
	return i;
}

static struct pcm_feederdesc feeder_ulawtou8_desc[] = {
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_U8, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_ulawtou8_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_table),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_ulawtou8, 0, ulaw_to_u8);

static struct pcm_feederdesc feeder_u8toulaw_desc[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_MU_LAW | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_u8toulaw_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_table),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_u8toulaw, 0, u8_to_ulaw);

static struct pcm_feederdesc feeder_alawtoulaw_desc[] = {
	{FEEDER_FMT, AFMT_A_LAW, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_A_LAW | AFMT_STEREO, AFMT_MU_LAW | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_alawtoulaw_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_table),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_alawtoulaw, 0, alaw_to_ulaw);

static struct pcm_feederdesc feeder_ulawtoalaw_desc[] = {
	{FEEDER_FMT, AFMT_MU_LAW, AFMT_A_LAW, 0},
	{FEEDER_FMT, AFMT_MU_LAW | AFMT_STEREO, AFMT_A_LAW | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_ulawtoalaw_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_table),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_ulawtoalaw, 0, ulaw_to_alaw);



