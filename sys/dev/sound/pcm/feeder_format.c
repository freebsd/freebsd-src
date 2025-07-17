/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2024-2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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
 * feeder_format: New generation of generic, any-to-any format converter, as
 *                long as the sample values can be read _and_ write.
 */

#ifdef _KERNEL
#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif
#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/pcm.h>
#include "feeder_if.h"

#define SND_USE_FXDIV
#include "snd_fxdiv_gen.h"
#endif

#define FEEDFORMAT_RESERVOIR	(SND_CHN_MAX * PCM_32_BPS)

struct feed_format_info {
	uint32_t ibps, obps;
	uint32_t ialign, oalign, channels;
	uint32_t rdfmt, wrfmt;
	uint8_t reservoir[FEEDFORMAT_RESERVOIR];
};

static int
feed_format_init(struct pcm_feeder *f)
{
	struct feed_format_info *info;

	if (f->desc->in == f->desc->out ||
	    AFMT_CHANNEL(f->desc->in) != AFMT_CHANNEL(f->desc->out))
		return (EINVAL);

	info = malloc(sizeof(*info), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (info == NULL)
		return (ENOMEM);

	info->channels = AFMT_CHANNEL(f->desc->in);

	info->ibps = AFMT_BPS(f->desc->in);
	info->ialign = info->ibps * info->channels;
	info->rdfmt = AFMT_ENCODING(f->desc->in);

	info->obps = AFMT_BPS(f->desc->out);
	info->oalign = info->obps * info->channels;
	info->wrfmt = AFMT_ENCODING(f->desc->out);

	f->data = info;

	return (0);
}

static int
feed_format_free(struct pcm_feeder *f)
{
	struct feed_format_info *info;

	info = f->data;
	if (info != NULL)
		free(info, M_DEVBUF);

	f->data = NULL;

	return (0);
}

static int
feed_format_set(struct pcm_feeder *f, int what, int value)
{
	struct feed_format_info *info;

	info = f->data;

	switch (what) {
	case FEEDFORMAT_CHANNELS:
		if (value < SND_CHN_MIN || value > SND_CHN_MAX)
			return (EINVAL);
		info->channels = (uint32_t)value;
		info->ialign = info->ibps * info->channels;
		info->oalign = info->obps * info->channels;
		break;
	default:
		return (EINVAL);
		break;
	}

	return (0);
}

static int
feed_format_feed(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
    uint32_t count, void *source)
{
	struct feed_format_info *info;
	intpcm_t v;
	uint32_t j;
	uint8_t *src, *dst;

	info = f->data;
	dst = b;
	count = SND_FXROUND(count, info->oalign);

	do {
		if (count < info->oalign)
			break;

		if (count < info->ialign) {
			src = info->reservoir;
			j = info->ialign;
		} else {
			if (info->ialign == info->oalign)
				j = count;
			else if (info->ialign > info->oalign)
				j = SND_FXROUND(count, info->ialign);
			else
				j = SND_FXDIV(count, info->oalign) *
				    info->ialign;
			src = dst + count - j;
		}

		j = SND_FXDIV(FEEDER_FEED(f->source, c, src, j, source),
		    info->ialign);
		if (j == 0)
			break;

		j *= info->channels;
		count -= j * info->obps;

		do {
			v = pcm_sample_read_norm(src, info->rdfmt);
			pcm_sample_write_norm(dst, v, info->wrfmt);
			dst += info->obps;
			src += info->ibps;
		} while (--j != 0);

	} while (count != 0);

	return (dst - b);
}

static struct pcm_feederdesc feeder_format_desc[] = {
	{ FEEDER_FORMAT, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 }
};

static kobj_method_t feeder_format_methods[] = {
	KOBJMETHOD(feeder_init,		feed_format_init),
	KOBJMETHOD(feeder_free,		feed_format_free),
	KOBJMETHOD(feeder_set,		feed_format_set),
	KOBJMETHOD(feeder_feed,		feed_format_feed),
	KOBJMETHOD_END
};

FEEDER_DECLARE(feeder_format, NULL);
