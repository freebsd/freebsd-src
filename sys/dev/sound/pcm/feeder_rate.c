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

#include "feeder_if.h"

MALLOC_DEFINE(M_RATEFEEDER, "ratefeed", "pcm rate feeder");

#define FEEDBUFSZ	8192
#undef FEEDER_DEBUG

struct feed_rate_info {
	u_int32_t src, dst;
	int srcpos, srcinc;
	int16_t *buffer;
	u_int16_t alpha;
};

static int
feed_rate_setup(struct pcm_feeder *f)
{
	struct feed_rate_info *info = f->data;

	info->srcinc = (info->src << 16) / info->dst;
	/* srcinc is 16.16 fixed point increment for srcpos for each dstpos */
	info->srcpos = 0;
	return 0;
}

static int
feed_rate_set(struct pcm_feeder *f, int what, int value)
{
	struct feed_rate_info *info = f->data;

	switch(what) {
	case FEEDRATE_SRC:
		info->src = value;
		break;
	case FEEDRATE_DST:
		info->dst = value;
		break;
	default:
		return -1;
	}
	return feed_rate_setup(f);
}

static int
feed_rate_init(struct pcm_feeder *f)
{
	struct feed_rate_info *info;

	info = malloc(sizeof(*info), M_RATEFEEDER, M_WAITOK | M_ZERO);
	if (info == NULL)
		return ENOMEM;
	info->buffer = malloc(FEEDBUFSZ, M_RATEFEEDER, M_WAITOK | M_ZERO);
	if (info->buffer == NULL) {
		free(info, M_RATEFEEDER);
		return ENOMEM;
	}
	info->src = DSP_DEFAULT_SPEED;
	info->dst = DSP_DEFAULT_SPEED;
	info->alpha = 0;
	f->data = info;
	return feed_rate_setup(f);
}

static int
feed_rate_free(struct pcm_feeder *f)
{
	struct feed_rate_info *info = f->data;

	if (info) {
		if (info->buffer)
			free(info->buffer, M_RATEFEEDER);
		free(info, M_RATEFEEDER);
	}
	f->data = NULL;
	return 0;
}

static int
feed_rate(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	struct feed_rate_info *info = f->data;
	int16_t *destbuf = (int16_t *)b;
	int fetch, v, alpha, hidelta, spos, dpos;

	/*
	 * at this point:
	 * info->srcpos is 24.8 fixed offset into the fetchbuffer.  0 <= srcpos <= 0xff
	 *
	 * our input and output are always AFMT_S16LE stereo.  this simplifies things.
	 */

	/*
	 * we start by fetching enough source data into our buffer to generate
	 * about as much as was requested.  we put it at offset 2 in the
	 * buffer so that we can interpolate from the last samples in the
	 * previous iteration- when we finish we will move our last samples
	 * to the start of the buffer.
	 */
	spos = 0;
	dpos = 0;

	/* fetch is in bytes */
	fetch = (count * info->srcinc) >> 16;
	fetch = min(fetch, FEEDBUFSZ - 4) & ~3;
	if (fetch == 0)
		return 0;
	fetch = FEEDER_FEED(f->source, c, ((u_int8_t *)info->buffer) + 4, fetch, source);
	fetch /= 2;

	alpha = info->alpha;
	hidelta = min(info->srcinc >> 16, 1) * 2;
	while ((spos + hidelta + 1) < fetch) {
		v = (info->buffer[spos] * (0xffff - alpha)) + (info->buffer[spos + hidelta] * alpha);
		destbuf[dpos++] = v >> 16;

		v = (info->buffer[spos + 1] * (0xffff - alpha)) + (info->buffer[spos + hidelta + 1] * alpha);
		destbuf[dpos++] = v >> 16;

		alpha += info->srcinc;
		spos += (alpha >> 16) * 2;
		alpha &= 0xffff;

	}
	info->alpha = alpha & 0xffff;
	info->buffer[0] = info->buffer[spos - hidelta];
	info->buffer[1] = info->buffer[spos - hidelta + 1];

	count = dpos * 2;
	return count;
}

static struct pcm_feederdesc feeder_rate_desc[] = {
	{FEEDER_RATE, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_rate_methods[] = {
    	KOBJMETHOD(feeder_init,		feed_rate_init),
    	KOBJMETHOD(feeder_free,		feed_rate_free),
    	KOBJMETHOD(feeder_set,		feed_rate_set),
    	KOBJMETHOD(feeder_feed,		feed_rate),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_rate, 2, NULL);


