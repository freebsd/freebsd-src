/*-
 * Copyright (c) 2005 Ariff Abdullah 
 *        <skywizard@MyBSD.org.my> All rights reserved.
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

static int
feed_volume_s16(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
		uint32_t count, void *source)
{
	int i, j, k, vol[2];
	int16_t *buf;

	k = FEEDER_FEED(f->source, c, b, count & ~1, source);
	if (k < 2) {
#if 0
		device_printf(c->dev, "%s: Not enough data (Got: %d bytes)\n",
				__func__, k);
#endif
		return 0;
	}
#if 0
	if (k & 1)
		device_printf(c->dev, "%s: Bytes not 16bit aligned.\n", __func__);
#endif
	k &= ~1;
	i = k >> 1;
	buf = (int16_t *)b;
	vol[0] = c->volume & 0x7f;
	vol[1] = (c->volume >> 8) & 0x7f;
	while (i > 0) {
		i--;
		j = (vol[i & 1] * buf[i]) / 100;
		if (j > 32767)
			j = 32767;
		if (j < -32768)
			j = -32768;
		buf[i] = j;
	}
	return k;
}

static struct pcm_feederdesc feeder_volume_s16_desc[] = {
	{FEEDER_VOLUME, AFMT_S16_LE|AFMT_STEREO, AFMT_S16_LE|AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_volume_s16_methods[] = {
    	KOBJMETHOD(feeder_feed, feed_volume_s16),
	{0, 0}
};
FEEDER_DECLARE(feeder_volume_s16, 2, NULL);
