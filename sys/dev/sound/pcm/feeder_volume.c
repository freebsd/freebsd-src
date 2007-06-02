/*-
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

/* feeder_volume, a long 'Lost Technology' rather than a new feature. */

#include <dev/sound/pcm/sound.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

#define FVOL_OSS_SCALE		100
#define FVOL_RESOLUTION		PCM_FXSHIFT
#define FVOL_CLAMP(val)		(((val) << FVOL_RESOLUTION) / FVOL_OSS_SCALE)
#define FVOL_LEFT(val)		FVOL_CLAMP((val) & 0x7f)
#define FVOL_RIGHT(val)		FVOL_LEFT((val) >> 8)
#define FVOL_MAX		(1U << FVOL_RESOLUTION)
#define FVOL_CALC(sval, vval)	(((sval) * (vval)) >> FVOL_RESOLUTION)

typedef uint32_t (*feed_volume_filter)(uint8_t *, uint32_t *, uint32_t);

#define FEEDER_VOLUME_FILTER(FMTBIT, VOL_INTCAST, SIGN, SIGNS, ENDIAN, ENDIANS)	\
static uint32_t									\
feed_volume_filter_##SIGNS##FMTBIT##ENDIANS(uint8_t *b, uint32_t *vol,		\
							uint32_t count)		\
{										\
	int32_t j;								\
	int i;									\
										\
	i = count;								\
	b += i;									\
										\
	do {									\
		b -= PCM_##FMTBIT##_BPS;					\
		i -= PCM_##FMTBIT##_BPS;					\
		j = PCM_READ_##SIGN##FMTBIT##_##ENDIAN(b);			\
		j = FVOL_CALC((VOL_INTCAST)j,					\
		    vol[(i / PCM_##FMTBIT##_BPS) & 1]);				\
		PCM_WRITE_##SIGN##FMTBIT##_##ENDIAN(b, j);			\
	} while (i != 0);							\
										\
	return (count);								\
}

FEEDER_VOLUME_FILTER(8, int32_t, S, s, NE, ne)
FEEDER_VOLUME_FILTER(16, int32_t, S, s, LE, le)
FEEDER_VOLUME_FILTER(24, int32_t, S, s, LE, le)
FEEDER_VOLUME_FILTER(32, intpcm_t, S, s, LE, le)
FEEDER_VOLUME_FILTER(16, int32_t, S, s, BE, be)
FEEDER_VOLUME_FILTER(24, int32_t, S, s, BE, be)
FEEDER_VOLUME_FILTER(32, intpcm_t, S, s, BE, be)
FEEDER_VOLUME_FILTER(8, int32_t, U, u, NE, ne)
FEEDER_VOLUME_FILTER(16, int32_t, U, u, LE, le)
FEEDER_VOLUME_FILTER(24, int32_t, U, u, LE, le)
FEEDER_VOLUME_FILTER(32, intpcm_t, U, u, LE, le)
FEEDER_VOLUME_FILTER(16, int32_t, U, u, BE, be)
FEEDER_VOLUME_FILTER(24, int32_t, U, u, BE, be)
FEEDER_VOLUME_FILTER(32, intpcm_t, U, u, BE, be)

struct feed_volume_info {
	uint32_t format;
	int bps;
	feed_volume_filter filter;
};

static struct feed_volume_info feed_volume_tbl[] = {
	{ AFMT_S8,     PCM_8_BPS,  feed_volume_filter_s8ne  },
	{ AFMT_S16_LE, PCM_16_BPS, feed_volume_filter_s16le },
	{ AFMT_S24_LE, PCM_24_BPS, feed_volume_filter_s24le },
	{ AFMT_S32_LE, PCM_32_BPS, feed_volume_filter_s32le },
	{ AFMT_S16_BE, PCM_16_BPS, feed_volume_filter_s16be },
	{ AFMT_S24_BE, PCM_24_BPS, feed_volume_filter_s24be },
	{ AFMT_S32_BE, PCM_32_BPS, feed_volume_filter_s32be },
	{ AFMT_U8,     PCM_8_BPS,  feed_volume_filter_u8ne  },
	{ AFMT_U16_LE, PCM_16_BPS, feed_volume_filter_u16le },
	{ AFMT_U24_LE, PCM_24_BPS, feed_volume_filter_u24le },
	{ AFMT_U32_LE, PCM_32_BPS, feed_volume_filter_u32le },
	{ AFMT_U16_BE, PCM_16_BPS, feed_volume_filter_u16be },
	{ AFMT_U24_BE, PCM_24_BPS, feed_volume_filter_u24be },
	{ AFMT_U32_BE, PCM_32_BPS, feed_volume_filter_u32be },
};

#define FVOL_DATA(i, c)		((intptr_t)((((i) & 0x1f) << 4) | ((c) & 0xf)))
#define FVOL_INFOIDX(m)		(((m) >> 4) & 0x1f)
#define FVOL_CHANNELS(m)	((m) & 0xf)

static int
feed_volume_init(struct pcm_feeder *f)
{
	int i, channels;

	if (f->desc->in != f->desc->out)
		return (EINVAL);

	/* For now, this is mandatory! */
	if (!(f->desc->out & AFMT_STEREO))
		return (EINVAL);

	channels = 2;

	for (i = 0; i < sizeof(feed_volume_tbl) / sizeof(feed_volume_tbl[0]);
	    i++) {
		if ((f->desc->out & ~AFMT_STEREO) ==
		    feed_volume_tbl[i].format) {
			f->data = (void *)FVOL_DATA(i, channels);
			return (0);
		}
	}

	return (-1);
}

static int
feed_volume(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
						uint32_t count, void *source)
{
	struct feed_volume_info *info;
	uint32_t vol[2];
	int k, smpsz;

	vol[0] = FVOL_LEFT(c->volume);
	vol[1] = FVOL_RIGHT(c->volume);

	if (vol[0] == FVOL_MAX && vol[1] == FVOL_MAX)
		return (FEEDER_FEED(f->source, c, b, count, source));

	info = &feed_volume_tbl[FVOL_INFOIDX((intptr_t)f->data)];
	smpsz = info->bps * FVOL_CHANNELS((intptr_t)f->data);
	if (count < smpsz)
		return (0);

	k = FEEDER_FEED(f->source, c, b, count - (count % smpsz), source);
	if (k < smpsz)
		return (0);

	k -= k % smpsz;
	return (info->filter(b, vol, k));
}

static struct pcm_feederdesc feeder_volume_desc[] = {
	{FEEDER_VOLUME, AFMT_S8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S24_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S32_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S24_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S32_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_U8 | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_U24_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_U32_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_U24_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_U32_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_volume_methods[] = {
	KOBJMETHOD(feeder_init,		feed_volume_init),
	KOBJMETHOD(feeder_feed,		feed_volume),
	{0, 0}
};
FEEDER_DECLARE(feeder_volume, 2, NULL);
