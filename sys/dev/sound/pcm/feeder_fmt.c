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
static pcm_feeder feeder_8to16le = {
	"8to16le",
	0,
	desc_8to16le,
	NULL,
	NULL,
	NULL,
	feed_8to16le,
};
FEEDER_DECLARE(feeder_8to16le);

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
static pcm_feeder feeder_16leto8 = {
	"16leto8",
	1,
	desc_16leto8,
	feed_16to8_init,
	feed_16to8_free,
	NULL,
	feed_16leto8,
};
FEEDER_DECLARE(feeder_16leto8);

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
static pcm_feeder feeder_monotostereo8 = {
	"monotostereo8",
	0,
	desc_monotostereo8,
	NULL,
	NULL,
	NULL,
	feed_monotostereo8,
};
FEEDER_DECLARE(feeder_monotostereo8);

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
static pcm_feeder feeder_monotostereo16 = {
	"monotostereo16",
	0,
	desc_monotostereo16,
	NULL,
	NULL,
	NULL,
	feed_monotostereo16,
};
FEEDER_DECLARE(feeder_monotostereo16);

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
static pcm_feeder feeder_stereotomono8 = {
	"stereotomono8",
	1,
	desc_stereotomono8,
	feed_stereotomono8_init,
	feed_stereotomono8_free,
	NULL,
	feed_stereotomono8,
};
FEEDER_DECLARE(feeder_stereotomono8);

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
static pcm_feeder feeder_stereotomono16 = {
	"stereotomono16",
	1,
	desc_stereotomono16,
	feed_stereotomono16_init,
	feed_stereotomono16_free,
	NULL,
	feed_stereotomono16,
};
FEEDER_DECLARE(feeder_stereotomono16);

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
static pcm_feeder feeder_endian = {
	"endian",
	-1,
	desc_endian,
	NULL,
	NULL,
	NULL,
	feed_endian,
};
FEEDER_DECLARE(feeder_endian);

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
static pcm_feeder feeder_sign8 = {
	"sign8",
	0,
	desc_sign8,
	NULL,
	NULL,
	NULL,
	feed_sign,
	(void *)1,
};
FEEDER_DECLARE(feeder_sign8);

static struct pcm_feederdesc desc_sign16le[] = {
	{FEEDER_FMT, AFMT_U16_LE, AFMT_S16_LE, 0},
	{FEEDER_FMT, AFMT_U16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_FMT, AFMT_S16_LE, AFMT_U16_LE, 0},
	{FEEDER_FMT, AFMT_S16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_sign16le = {
	"sign16le",
	-1,
	desc_sign16le,
	NULL,
	NULL,
	NULL,
	feed_sign,
	(void *)2,
};
FEEDER_DECLARE(feeder_sign16le);

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
static pcm_feeder feeder_ulawtou8 = {
	"ulawtou8",
	0,
	desc_ulawtou8,
	NULL,
	NULL,
	NULL,
	feed_table,
	ulaw_to_u8,
};
FEEDER_DECLARE(feeder_ulawtou8);

static struct pcm_feederdesc desc_u8toulaw[] = {
	{FEEDER_FMT, AFMT_U8, AFMT_MU_LAW, 0},
	{FEEDER_FMT, AFMT_U8 | AFMT_STEREO, AFMT_MU_LAW | AFMT_STEREO, 0},
	{0},
};
static pcm_feeder feeder_u8toulaw = {
	"u8toulaw",
	0,
	desc_u8toulaw,
	NULL,
	NULL,
	NULL,
	feed_table,
	u8_to_ulaw,
};
FEEDER_DECLARE(feeder_u8toulaw);


