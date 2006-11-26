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
 *
 * feeder_volume, a long 'Lost Technology' rather than a new feature.
 */

#include <dev/sound/pcm/sound.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

MALLOC_DEFINE(M_VOLUMEFEEDER, "volumefeed", "pcm volume feeder");

#define FVOL_TRACE(x...) /* device_printf(c->dev, x) */
#define FVOL_TEST(x, y...) /* if (x) FVOL_TRACE(y) */

#define FVOL_RESOLUTION		6 /* 6bit volume resolution */
#define FVOL_CLAMP(val)		(((val) << FVOL_RESOLUTION) / 100)
#define FVOL_LEFT(val)		FVOL_CLAMP((val) & 0x7f)
#define FVOL_RIGHT(val)		FVOL_LEFT((val) >> 8)
#define FVOL_MAX		(1 << FVOL_RESOLUTION)
#define FVOL_CALC(sval, vval)	(((sval) * (vval)) >> FVOL_RESOLUTION)

struct feed_volume_info;

typedef uint32_t (*feed_volume_filter)(struct feed_volume_info *,
						uint8_t *, int *, uint32_t);

struct feed_volume_info {
	uint32_t bps, channels;
	feed_volume_filter filter;
};

#define FEEDER_VOLUME_FILTER(FMTBIT, VOL_INTCAST, SIGN, SIGNS, ENDIAN, ENDIANS)	\
static uint32_t									\
feed_volume_filter_##SIGNS##FMTBIT##ENDIANS(struct feed_volume_info *info,	\
					uint8_t *b, int *vol, uint32_t count)	\
{										\
	uint32_t bps;								\
	int32_t j;								\
	int i;									\
										\
	bps = info->bps;							\
	i = count;								\
	b += i;									\
	while (i > 0) {								\
		b -= bps;							\
		i -= bps;							\
		j = PCM_READ_##SIGN##FMTBIT##_##ENDIAN(b);			\
		j = FVOL_CALC((VOL_INTCAST)j, vol[(i / bps) & 1]);		\
		PCM_WRITE_##SIGN##FMTBIT##_##ENDIAN(b, j);			\
	}									\
	return count;								\
}

FEEDER_VOLUME_FILTER(8, int32_t, S, s, NE, ne)
FEEDER_VOLUME_FILTER(16, int32_t, S, s, LE, le)
FEEDER_VOLUME_FILTER(24, int32_t, S, s, LE, le)
FEEDER_VOLUME_FILTER(32, intpcm_t, S, s, LE, le)
FEEDER_VOLUME_FILTER(16, int32_t, S, s, BE, be)
FEEDER_VOLUME_FILTER(24, int32_t, S, s, BE, be)
FEEDER_VOLUME_FILTER(32, intpcm_t, S, s, BE, be)
/* unsigned */
FEEDER_VOLUME_FILTER(8, int32_t, U, u, NE, ne)
FEEDER_VOLUME_FILTER(16, int32_t, U, u, LE, le)
FEEDER_VOLUME_FILTER(24, int32_t, U, u, LE, le)
FEEDER_VOLUME_FILTER(32, intpcm_t, U, u, LE, le)
FEEDER_VOLUME_FILTER(16, int32_t, U, u, BE, be)
FEEDER_VOLUME_FILTER(24, int32_t, U, u, BE, be)
FEEDER_VOLUME_FILTER(32, intpcm_t, U, u, BE, be)

static int
feed_volume_setup(struct pcm_feeder *f)
{
	struct feed_volume_info *info = f->data;
	static const struct {
		uint32_t format;	/* pcm / audio format */
		uint32_t bps;		/* bytes-per-sample, regardless of
					   total channels */
		feed_volume_filter filter;
	} voltbl[] = {
		{ AFMT_S8, PCM_8_BPS, feed_volume_filter_s8ne },
		{ AFMT_S16_LE, PCM_16_BPS, feed_volume_filter_s16le },
		{ AFMT_S24_LE, PCM_24_BPS, feed_volume_filter_s24le },
		{ AFMT_S32_LE, PCM_32_BPS, feed_volume_filter_s32le },
		{ AFMT_S16_BE, PCM_16_BPS, feed_volume_filter_s16be },
		{ AFMT_S24_BE, PCM_24_BPS, feed_volume_filter_s24be },
		{ AFMT_S32_BE, PCM_32_BPS, feed_volume_filter_s32be },
		/* unsigned */
		{ AFMT_U8, PCM_8_BPS, feed_volume_filter_u8ne },
		{ AFMT_U16_LE, PCM_16_BPS, feed_volume_filter_u16le },
		{ AFMT_U24_LE, PCM_24_BPS, feed_volume_filter_u24le },
		{ AFMT_U32_LE, PCM_32_BPS, feed_volume_filter_u32le },
		{ AFMT_U16_BE, PCM_16_BPS, feed_volume_filter_u16be },
		{ AFMT_U24_BE, PCM_24_BPS, feed_volume_filter_u24be },
		{ AFMT_U32_BE, PCM_32_BPS, feed_volume_filter_u32be },
		{ 0, 0, NULL },
	};
	uint32_t i;

	for (i = 0; i < sizeof(voltbl) / sizeof(*voltbl); i++) {
		if (voltbl[i].format == 0)
			return -1;
		if ((f->desc->out & ~AFMT_STEREO) == voltbl[i].format) {
			info->bps = voltbl[i].bps;
			info->filter = voltbl[i].filter;
			break;
		}
	}

	/* For now, this is mandatory! */
	info->channels = 2;

	return 0;
}

static int
feed_volume_init(struct pcm_feeder *f)
{
	struct feed_volume_info *info;

	if (f->desc->in != f->desc->out)
		return EINVAL;

	/* Mandatory */
	if (!(f->desc->out & AFMT_STEREO))
		return EINVAL;

	info = malloc(sizeof(*info), M_VOLUMEFEEDER, M_NOWAIT | M_ZERO);
	if (info == NULL)
		return ENOMEM;
	f->data = info;
	return feed_volume_setup(f);
}

static int
feed_volume_free(struct pcm_feeder *f)
{
	struct feed_volume_info *info = f->data;

	if (info)
		free(info, M_VOLUMEFEEDER);
	f->data = NULL;
	return 0;
}

static int
feed_volume(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
						uint32_t count, void *source)
{
	struct feed_volume_info *info = f->data;
	uint32_t k, smpsz;
	int vol[2];

	vol[0] = FVOL_LEFT(c->volume);
	vol[1] = FVOL_RIGHT(c->volume);

	if (vol[0] == FVOL_MAX && vol[1] == FVOL_MAX)
		return FEEDER_FEED(f->source, c, b, count, source);

	smpsz = info->bps * info->channels;
	if (count < smpsz)
		return 0;
	count -= count % smpsz;
	k = FEEDER_FEED(f->source, c, b, count, source);
	if (k < smpsz) {
		FVOL_TRACE("%s: Not enough data (Got: %u bytes)\n",
				__func__, k);
		return 0;
	}
	FVOL_TEST(k % smpsz, "%s: Bytes not %dbit (stereo) aligned.\n",
			__func__, info->bps << 3);
	k -= k % smpsz;
	return info->filter(info, b, vol, k);
}

static struct pcm_feederdesc feeder_volume_desc[] = {
	{FEEDER_VOLUME, AFMT_S8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S24_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S32_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S24_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_VOLUME, AFMT_S32_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	/* unsigned */
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
	KOBJMETHOD(feeder_free,		feed_volume_free),
	KOBJMETHOD(feeder_feed,		feed_volume),
	{0, 0}
};
FEEDER_DECLARE(feeder_volume, 2, NULL);
