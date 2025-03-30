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

#ifdef _KERNEL
#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif
#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/pcm.h>
#include <dev/sound/pcm/vchan.h>
#include "feeder_if.h"

#define SND_USE_FXDIV
#include "snd_fxdiv_gen.h"
#endif

#undef SND_FEEDER_MULTIFORMAT
#define SND_FEEDER_MULTIFORMAT	1

struct feed_mixer_info {
	uint32_t format;
	uint32_t channels;
	int bps;
};

__always_inline static void
feed_mixer_apply(uint8_t *src, uint8_t *dst, uint32_t count, const uint32_t fmt)
{
	intpcm32_t z;
	intpcm_t x, y;

	src += count;
	dst += count;

	do {
		src -= AFMT_BPS(fmt);
		dst -= AFMT_BPS(fmt);
		count -= AFMT_BPS(fmt);
		x = pcm_sample_read_calc(src, fmt);
		y = pcm_sample_read_calc(dst, fmt);
		z = INTPCM_T(x) + y;
		x = pcm_clamp_calc(z, fmt);
		pcm_sample_write(dst, x, fmt);
	} while (count != 0);
}

static int
feed_mixer_init(struct pcm_feeder *f)
{
	struct feed_mixer_info *info;

	if (f->desc->in != f->desc->out)
		return (EINVAL);

	info = malloc(sizeof(*info), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (info == NULL)
		return (ENOMEM);

	info->format = AFMT_ENCODING(f->desc->in);
	info->channels = AFMT_CHANNEL(f->desc->in);
	info->bps = AFMT_BPS(f->desc->in);

	f->data = info;

	return (0);
}

static int
feed_mixer_free(struct pcm_feeder *f)
{
	struct feed_mixer_info *info;

	info = f->data;
	if (info != NULL)
		free(info, M_DEVBUF);

	f->data = NULL;

	return (0);
}

static int
feed_mixer_set(struct pcm_feeder *f, int what, int value)
{
	struct feed_mixer_info *info;

	info = f->data;

	switch (what) {
	case FEEDMIXER_CHANNELS:
		if (value < SND_CHN_MIN || value > SND_CHN_MAX)
			return (EINVAL);
		info->channels = (uint32_t)value;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static __inline int
feed_mixer_rec(struct pcm_channel *c)
{
	struct pcm_channel *ch;
	struct snd_dbuf *b, *bs;
	uint32_t cnt, maxfeed;
	int rdy;

	/*
	 * Reset ready and moving pointer. We're not using bufsoft
	 * anywhere since its sole purpose is to become the primary
	 * distributor for the recorded buffer and also as an interrupt
	 * threshold progress indicator.
	 */
	b = c->bufsoft;
	b->rp = 0;
	b->rl = 0;
	cnt = sndbuf_getsize(b);
	maxfeed = SND_FXROUND(SND_FXDIV_MAX, sndbuf_getalign(b));

	do {
		cnt = FEEDER_FEED(c->feeder->source, c, b->tmpbuf,
		    min(cnt, maxfeed), c->bufhard);
		if (cnt != 0) {
			sndbuf_acquire(b, b->tmpbuf, cnt);
			cnt = sndbuf_getfree(b);
		}
	} while (cnt != 0);

	/* Not enough data */
	if (b->rl < sndbuf_getalign(b)) {
		b->rl = 0;
		return (0);
	}

	/*
	 * Keep track of ready and moving pointer since we will use
	 * bufsoft over and over again, pretending nothing has happened.
	 */
	rdy = b->rl;

	CHN_FOREACH(ch, c, children.busy) {
		CHN_LOCK(ch);
		if (CHN_STOPPED(ch) || (ch->flags & CHN_F_DIRTY)) {
			CHN_UNLOCK(ch);
			continue;
		}
#ifdef SND_DEBUG
		if ((c->flags & CHN_F_DIRTY) && VCHAN_SYNC_REQUIRED(ch)) {
			if (vchan_sync(ch) != 0) {
				CHN_UNLOCK(ch);
				continue;
			}
		}
#endif
		bs = ch->bufsoft;
		if (ch->flags & CHN_F_MMAP)
			sndbuf_dispose(bs, NULL, sndbuf_getready(bs));
		cnt = sndbuf_getfree(bs);
		if (cnt < sndbuf_getalign(bs)) {
			CHN_UNLOCK(ch);
			continue;
		}
		maxfeed = SND_FXROUND(SND_FXDIV_MAX, sndbuf_getalign(bs));
		do {
			cnt = FEEDER_FEED(ch->feeder, ch, bs->tmpbuf,
			    min(cnt, maxfeed), b);
			if (cnt != 0) {
				sndbuf_acquire(bs, bs->tmpbuf, cnt);
				cnt = sndbuf_getfree(bs);
			}
		} while (cnt != 0);
		/*
		 * Not entirely flushed out...
		 */
		if (b->rl != 0)
			ch->xruns++;
		CHN_UNLOCK(ch);
		/*
		 * Rewind buffer position for next virtual channel.
		 */
		b->rp = 0;
		b->rl = rdy;
	}

	/*
	 * Set ready pointer to indicate that our children are ready
	 * to be woken up, also as an interrupt threshold progress
	 * indicator.
	 */
	b->rl = 1;

	c->flags &= ~CHN_F_DIRTY;

	/*
	 * Return 0 to bail out early from sndbuf_feed() loop.
	 * No need to increase feedcount counter since part of this
	 * feeder chains already include feed_root().
	 */
	return (0);
}

static int
feed_mixer_feed(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
    uint32_t count, void *source)
{
	struct feed_mixer_info *info;
	struct snd_dbuf *src = source;
	struct pcm_channel *ch;
	uint32_t cnt, mcnt, rcnt, sz;
	int passthrough;
	uint8_t *tmp;

	if (c->direction == PCMDIR_REC)
		return (feed_mixer_rec(c));

	sz = sndbuf_getsize(src);
	if (sz < count)
		count = sz;

	info = f->data;
	sz = info->bps * info->channels;
	count = SND_FXROUND(count, sz);
	if (count < sz)
		return (0);

	/*
	 * We are going to use our source as a temporary buffer since it's
	 * got no other purpose.  We obtain our data by traversing the channel
	 * list of children and calling mixer function to mix count bytes from
	 * each into our destination buffer, b.
	 */
	tmp = sndbuf_getbuf(src);
	rcnt = 0;
	mcnt = 0;
	passthrough = 0;	/* 'passthrough' / 'exclusive' marker */

	CHN_FOREACH(ch, c, children.busy) {
		CHN_LOCK(ch);
		if (CHN_STOPPED(ch) || (ch->flags & CHN_F_DIRTY)) {
			CHN_UNLOCK(ch);
			continue;
		}
#ifdef SND_DEBUG
		if ((c->flags & CHN_F_DIRTY) && VCHAN_SYNC_REQUIRED(ch)) {
			if (vchan_sync(ch) != 0) {
				CHN_UNLOCK(ch);
				continue;
			}
		}
#endif
		if ((ch->flags & CHN_F_MMAP) && !(ch->flags & CHN_F_CLOSING))
			sndbuf_acquire(ch->bufsoft, NULL,
			    sndbuf_getfree(ch->bufsoft));
		if (c->flags & CHN_F_PASSTHROUGH) {
			/*
			 * Passthrough. Dump the first digital/passthrough
			 * channel into destination buffer, and the rest into
			 * nothingness (mute effect).
			 */
			if (passthrough == 0 &&
			    (ch->format & AFMT_PASSTHROUGH)) {
				rcnt = SND_FXROUND(FEEDER_FEED(ch->feeder, ch,
				    b, count, ch->bufsoft), sz);
				passthrough = 1;
			} else
				FEEDER_FEED(ch->feeder, ch, tmp, count,
				    ch->bufsoft);
		} else if (c->flags & CHN_F_EXCLUSIVE) {
			/*
			 * Exclusive. Dump the first 'exclusive' channel into
			 * destination buffer, and the rest into nothingness
			 * (mute effect).
			 */
			if (passthrough == 0 && (ch->flags & CHN_F_EXCLUSIVE)) {
				rcnt = SND_FXROUND(FEEDER_FEED(ch->feeder, ch,
				    b, count, ch->bufsoft), sz);
				passthrough = 1;
			} else
				FEEDER_FEED(ch->feeder, ch, tmp, count,
				    ch->bufsoft);
		} else {
			if (rcnt == 0) {
				rcnt = SND_FXROUND(FEEDER_FEED(ch->feeder, ch,
				    b, count, ch->bufsoft), sz);
				mcnt = count - rcnt;
			} else {
				cnt = SND_FXROUND(FEEDER_FEED(ch->feeder, ch,
				    tmp, count, ch->bufsoft), sz);
				if (cnt != 0) {
					if (mcnt != 0) {
						memset(b + rcnt,
						    sndbuf_zerodata(
						    f->desc->out), mcnt);
						mcnt = 0;
					}
					switch (info->format) {
					case AFMT_S16_NE:
						feed_mixer_apply(tmp, b, cnt,
						    AFMT_S16_NE);
						break;
					case AFMT_S24_NE:
						feed_mixer_apply(tmp, b, cnt,
						    AFMT_S24_NE);
						break;
					case AFMT_S32_NE:
						feed_mixer_apply(tmp, b, cnt,
						    AFMT_S32_NE);
						break;
					default:
						feed_mixer_apply(tmp, b, cnt,
						    info->format);
						break;
					}
					if (cnt > rcnt)
						rcnt = cnt;
				}
			}
		}
		CHN_UNLOCK(ch);
	}

	if (++c->feedcount == 0)
		c->feedcount = 2;

	c->flags &= ~CHN_F_DIRTY;

	return (rcnt);
}

static struct pcm_feederdesc feeder_mixer_desc[] = {
	{ FEEDER_MIXER, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 }
};

static kobj_method_t feeder_mixer_methods[] = {
	KOBJMETHOD(feeder_init,		feed_mixer_init),
	KOBJMETHOD(feeder_free,		feed_mixer_free),
	KOBJMETHOD(feeder_set,		feed_mixer_set),
	KOBJMETHOD(feeder_feed,		feed_mixer_feed),
	KOBJMETHOD_END
};

FEEDER_DECLARE(feeder_mixer, NULL);
