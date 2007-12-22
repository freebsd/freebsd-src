/*-
 * Copyright (c) 1999 Cameron Grant <cg@FreeBSD.org>
 * Copyright (c) 2003 Orion Hodson <orion@FreeBSD.org>
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
 * 2006-02-21:
 * ==========
 *
 * Major cleanup and overhaul to remove much redundant codes.
 * Highlights:
 *	1) Support for signed / unsigned 16, 24 and 32 bit,
 *	   big / little endian,
 *	2) Unlimited channels.
 *
 * 2005-06-11:
 * ==========
 *
 * *New* and rewritten soft sample rate converter supporting arbitrary sample
 * rates, fine grained scaling/coefficients and a unified up/down stereo
 * converter. Most of the disclaimers from orion's notes also applies
 * here, regarding linear interpolation deficiencies and pre/post
 * anti-aliasing filtering issues. This version comes with a much simpler and
 * tighter interface, although it works almost exactly like the older one.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 * This new implementation is fully dedicated in memory of Cameron Grant,  *
 * the creator of the magnificent, highly addictive feeder infrastructure. *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Orion's notes:
 * =============
 *
 * This rate conversion code uses linear interpolation without any
 * pre- or post- interpolation filtering to combat aliasing.  This
 * greatly limits the sound quality and should be addressed at some
 * stage in the future.
 * 
 * Since this accuracy of interpolation is sensitive and examination
 * of the algorithm output is harder from the kernel, the code is
 * designed to be compiled in the kernel and in a userland test
 * harness.  This is done by selectively including and excluding code
 * with several portions based on whether _KERNEL is defined.  It's a
 * little ugly, but exceedingly useful.  The testsuite and its
 * revisions can be found at:
 *		http://people.freebsd.org/~orion/files/feedrate/
 *
 * Special thanks to Ken Marx for exposing flaws in the code and for
 * testing revisions.
 */

#include <dev/sound/pcm/sound.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

#define RATE_ASSERT(x, y)	/* KASSERT(x,y) */
#define RATE_TEST(x, y)		/* if (!(x)) printf y */
#define RATE_TRACE(x...)	/* printf(x) */

MALLOC_DEFINE(M_RATEFEEDER, "ratefeed", "pcm rate feeder");

/*
 * Don't overflow 32bit integer, since everything is done
 * within 32bit arithmetic.
 */
#define RATE_FACTOR_MIN		1
#define RATE_FACTOR_MAX		PCM_S24_MAX
#define RATE_FACTOR_SAFE(val)	(!((val) < RATE_FACTOR_MIN || \
				(val) > RATE_FACTOR_MAX))

struct feed_rate_info;

typedef uint32_t (*feed_rate_converter)(struct feed_rate_info *,
							uint8_t *, uint32_t);

struct feed_rate_info {
	uint32_t src, dst;	/* rounded source / destination rates */
	uint32_t rsrc, rdst;	/* original source / destination rates */
	uint32_t gx, gy;	/* interpolation / decimation ratio */
	uint32_t alpha;		/* interpolation distance */
	uint32_t pos, bpos;	/* current sample / buffer positions */
	uint32_t bufsz;		/* total buffer size limit */
	uint32_t bufsz_init;	/* allocated buffer size */
	uint32_t channels;	/* total channels */
	uint32_t bps;		/* bytes-per-sample */
#ifdef FEEDRATE_STRAY
	uint32_t stray;		/* stray bytes */
#endif
	uint8_t  *buffer;
	feed_rate_converter convert;
};

int feeder_rate_min = FEEDRATE_RATEMIN;
int feeder_rate_max = FEEDRATE_RATEMAX;
int feeder_rate_round = FEEDRATE_ROUNDHZ;

TUNABLE_INT("hw.snd.feeder_rate_min", &feeder_rate_min);
TUNABLE_INT("hw.snd.feeder_rate_max", &feeder_rate_max);
TUNABLE_INT("hw.snd.feeder_rate_round", &feeder_rate_round);

static int
sysctl_hw_snd_feeder_rate_min(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = feeder_rate_min;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (RATE_FACTOR_SAFE(val) && val < feeder_rate_max)
		feeder_rate_min = val;
	else
		err = EINVAL;
	return (err);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_rate_min, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_rate_min, "I",
	"minimum allowable rate");

static int
sysctl_hw_snd_feeder_rate_max(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = feeder_rate_max;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (RATE_FACTOR_SAFE(val) && val > feeder_rate_min)
		feeder_rate_max = val;
	else
		err = EINVAL;
	return (err);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_rate_max, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_rate_max, "I",
	"maximum allowable rate");

static int
sysctl_hw_snd_feeder_rate_round(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = feeder_rate_round;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return (err);
	if (val < FEEDRATE_ROUNDHZ_MIN || val > FEEDRATE_ROUNDHZ_MAX)
		err = EINVAL;
	else
		feeder_rate_round = val - (val % FEEDRATE_ROUNDHZ);
	return (err);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_rate_round, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_rate_round, "I",
	"sample rate converter rounding threshold");

#define FEEDER_RATE_CONVERT(FMTBIT, RATE_INTCAST, SIGN, SIGNS, ENDIAN, ENDIANS)	\
static uint32_t									\
feed_convert_##SIGNS##FMTBIT##ENDIANS(struct feed_rate_info *info,		\
						uint8_t *dst, uint32_t max)	\
{										\
	uint32_t ret, smpsz, ch, pos, bpos, gx, gy, alpha, d1, d2;		\
	int32_t x, y;								\
	int i;									\
	uint8_t *src, *sx, *sy;							\
										\
	ret = 0;								\
	alpha = info->alpha;							\
	gx = info->gx;								\
	gy = info->gy;								\
	pos = info->pos;							\
	bpos = info->bpos;							\
	src = info->buffer + pos;						\
	ch = info->channels;							\
	smpsz = PCM_##FMTBIT##_BPS * ch;					\
	for (;;) {								\
		if (alpha < gx) {						\
			alpha += gy;						\
			pos += smpsz;						\
			if (pos == bpos)					\
				break;						\
			src += smpsz;						\
		} else {							\
			alpha -= gx;						\
			d1 = (alpha << PCM_FXSHIFT) / gy;			\
			d2 = (1U << PCM_FXSHIFT) - d1;				\
			sx = src - smpsz;					\
			sy = src;						\
			i = ch;							\
			do {							\
				x = PCM_READ_##SIGN##FMTBIT##_##ENDIAN(sx);	\
				y = PCM_READ_##SIGN##FMTBIT##_##ENDIAN(sy);	\
				x = (((RATE_INTCAST)x * d1) +			\
				    ((RATE_INTCAST)y * d2)) >> PCM_FXSHIFT;	\
				PCM_WRITE_##SIGN##FMTBIT##_##ENDIAN(dst, x);	\
				dst += PCM_##FMTBIT##_BPS;			\
				sx += PCM_##FMTBIT##_BPS;			\
				sy += PCM_##FMTBIT##_BPS;			\
				ret += PCM_##FMTBIT##_BPS;			\
			} while (--i != 0);					\
			if (ret == max)						\
				break;						\
		}								\
	}									\
	info->alpha = alpha;							\
	info->pos = pos;							\
	return (ret);								\
}

FEEDER_RATE_CONVERT(8, int32_t, S, s, NE, ne)
FEEDER_RATE_CONVERT(16, int32_t, S, s, LE, le)
FEEDER_RATE_CONVERT(24, int32_t, S, s, LE, le)
FEEDER_RATE_CONVERT(32, intpcm_t, S, s, LE, le)
FEEDER_RATE_CONVERT(16, int32_t, S, s, BE, be)
FEEDER_RATE_CONVERT(24, int32_t, S, s, BE, be)
FEEDER_RATE_CONVERT(32, intpcm_t, S, s, BE, be)
FEEDER_RATE_CONVERT(8, int32_t, U, u, NE, ne)
FEEDER_RATE_CONVERT(16, int32_t, U, u, LE, le)
FEEDER_RATE_CONVERT(24, int32_t, U, u, LE, le)
FEEDER_RATE_CONVERT(32, intpcm_t, U, u, LE, le)
FEEDER_RATE_CONVERT(16, int32_t, U, u, BE, be)
FEEDER_RATE_CONVERT(24, int32_t, U, u, BE, be)
FEEDER_RATE_CONVERT(32, intpcm_t, U, u, BE, be)

static void
feed_speed_ratio(uint32_t src, uint32_t dst, uint32_t *gx, uint32_t *gy)
{
	uint32_t w, x = src, y = dst;

	while (y != 0) {
		w = x % y;
		x = y;
		y = w;
	}
	*gx = src / x;
	*gy = dst / x;
}

static void
feed_rate_reset(struct feed_rate_info *info)
{
	info->src = info->rsrc - (info->rsrc %
	    ((feeder_rate_round > 0) ? feeder_rate_round : 1));
	info->dst = info->rdst - (info->rdst %
	    ((feeder_rate_round > 0) ? feeder_rate_round : 1));
	info->gx = 1;
	info->gy = 1;
	info->alpha = 0;
	info->channels = 1;
	info->bps = PCM_8_BPS;
	info->convert = NULL;
	info->bufsz = info->bufsz_init;
	info->pos = 1;
	info->bpos = 2;
#ifdef FEEDRATE_STRAY
	info->stray = 0;
#endif
}

static int
feed_rate_setup(struct pcm_feeder *f)
{
	struct feed_rate_info *info = f->data;
	static const struct {
		uint32_t format;	/* pcm / audio format */
		uint32_t bps;		/* bytes-per-sample, regardless of
					   total channels */
		feed_rate_converter convert;
	} convtbl[] = {
		{ AFMT_S8,     PCM_8_BPS,  feed_convert_s8ne  },
		{ AFMT_S16_LE, PCM_16_BPS, feed_convert_s16le },
		{ AFMT_S24_LE, PCM_24_BPS, feed_convert_s24le },
		{ AFMT_S32_LE, PCM_32_BPS, feed_convert_s32le },
		{ AFMT_S16_BE, PCM_16_BPS, feed_convert_s16be },
		{ AFMT_S24_BE, PCM_24_BPS, feed_convert_s24be },
		{ AFMT_S32_BE, PCM_32_BPS, feed_convert_s32be },
		{ AFMT_U8,     PCM_8_BPS,  feed_convert_u8ne  },
		{ AFMT_U16_LE, PCM_16_BPS, feed_convert_u16le },
		{ AFMT_U24_LE, PCM_24_BPS, feed_convert_u24le },
		{ AFMT_U32_LE, PCM_32_BPS, feed_convert_u32le },
		{ AFMT_U16_BE, PCM_16_BPS, feed_convert_u16be },
		{ AFMT_U24_BE, PCM_24_BPS, feed_convert_u24be },
		{ AFMT_U32_BE, PCM_32_BPS, feed_convert_u32be },
		{ 0, 0, NULL },
	};
	uint32_t i;

	feed_rate_reset(info);

	if (info->src != info->dst)
		feed_speed_ratio(info->src, info->dst, &info->gx, &info->gy);

	if (!(RATE_FACTOR_SAFE(info->gx) && RATE_FACTOR_SAFE(info->gy)))
		return (-1);

	for (i = 0; i < sizeof(convtbl) / sizeof(convtbl[0]); i++) {
		if (convtbl[i].format == 0)
			return (-1);
		if ((f->desc->out & ~AFMT_STEREO) == convtbl[i].format) {
			info->bps = convtbl[i].bps;
			info->convert = convtbl[i].convert;
			break;
		}
	}

	/*
	 * No need to interpolate/decimate, just do plain copy.
	 */
	if (info->gx == info->gy)
		info->convert = NULL;

	info->channels = (f->desc->out & AFMT_STEREO) ? 2 : 1;
	info->pos = info->bps * info->channels;
	info->bpos = info->pos << 1;
	info->bufsz -= info->bufsz % info->pos;

	memset(info->buffer, sndbuf_zerodata(f->desc->out), info->bpos);

	RATE_TRACE("%s: %u (%u) -> %u (%u) [%u/%u] , "
	    "format=0x%08x, channels=%u, bufsz=%u\n",
	    __func__, info->src, info->rsrc, info->dst, info->rdst,
	    info->gx, info->gy, f->desc->out, info->channels,
	    info->bufsz - info->pos);

	return (0);
}

static int
feed_rate_set(struct pcm_feeder *f, int what, int32_t value)
{
	struct feed_rate_info *info = f->data;

	if (value < feeder_rate_min || value > feeder_rate_max)
		return (-1);

	switch (what) {
	case FEEDRATE_SRC:
		info->rsrc = value;
		break;
	case FEEDRATE_DST:
		info->rdst = value;
		break;
	default:
		return (-1);
	}
	return (feed_rate_setup(f));
}

static int
feed_rate_get(struct pcm_feeder *f, int what)
{
	struct feed_rate_info *info = f->data;

	switch (what) {
	case FEEDRATE_SRC:
		return (info->rsrc);
	case FEEDRATE_DST:
		return (info->rdst);
	default:
		return (-1);
	}
	return (-1);
}

static int
feed_rate_init(struct pcm_feeder *f)
{
	struct feed_rate_info *info;

	if (f->desc->out != f->desc->in)
		return (EINVAL);

	info = malloc(sizeof(*info), M_RATEFEEDER, M_NOWAIT | M_ZERO);
	if (info == NULL)
		return (ENOMEM);
	/*
	 * bufsz = sample from last cycle + conversion space
	 */
	info->bufsz_init = 8 + feeder_buffersize;
	info->buffer = malloc(info->bufsz_init, M_RATEFEEDER,
	    M_NOWAIT | M_ZERO);
	if (info->buffer == NULL) {
		free(info, M_RATEFEEDER);
		return (ENOMEM);
	}
	info->rsrc = DSP_DEFAULT_SPEED;
	info->rdst = DSP_DEFAULT_SPEED;
	f->data = info;
	return (feed_rate_setup(f));
}

static int
feed_rate_free(struct pcm_feeder *f)
{
	struct feed_rate_info *info = f->data;

	if (info != NULL) {
		if (info->buffer != NULL)
			free(info->buffer, M_RATEFEEDER);
		free(info, M_RATEFEEDER);
	}
	f->data = NULL;
	return (0);
}

static int
feed_rate(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
						uint32_t count, void *source)
{
	struct feed_rate_info *info = f->data;
	uint32_t i, smpsz;
	int32_t fetch, slot;

	if (info->convert == NULL)
		return (FEEDER_FEED(f->source, c, b, count, source));

	/*
	 * This loop has been optimized to generalize both up / down
	 * sampling without causing missing samples or excessive buffer
	 * feeding. The tricky part is to calculate *precise* (slot) value
	 * needed for the entire conversion space since we are bound to
	 * return and fill up the buffer according to the requested 'count'.
	 * Too much feeding will cause the extra buffer stay within temporary
	 * circular buffer forever and always manifest itself as a truncated
	 * sound during end of playback / recording. Too few, and we end up
	 * with possible underruns and waste of cpu cycles.
	 *
	 * 'Stray' management exist to combat with possible unaligned
	 * buffering by the caller.
	 */
	smpsz = info->bps * info->channels;
	RATE_TEST(count >= smpsz && (count % smpsz) == 0,
	    ("%s: Count size not sample integral (%d)\n", __func__, count));
	if (count < smpsz)
		return (0);
	count -= count % smpsz;
	/*
	 * This slot count formula will stay here for the next million years
	 * to come. This is the key of our circular buffering precision.
	 */
	slot = (((info->gx * (count / smpsz)) + info->gy - info->alpha - 1) /
	    info->gy) * smpsz;
	RATE_TEST((slot % smpsz) == 0,
	    ("%s: Slot count not sample integral (%d)\n", __func__, slot));
#ifdef FEEDRATE_STRAY
	RATE_TEST(info->stray == 0, ("%s: [1] Stray bytes: %u\n", __func__,
	    info->stray));
#endif
	if (info->pos != smpsz && info->bpos - info->pos == smpsz &&
	    info->bpos + slot > info->bufsz) {
		/*
		 * Copy last unit sample and its previous to
		 * beginning of buffer.
		 */
		bcopy(info->buffer + info->pos - smpsz, info->buffer,
		    smpsz << 1);
		info->pos = smpsz;
		info->bpos = smpsz << 1;
	}
	RATE_ASSERT(slot >= 0, ("%s: Negative Slot: %d\n", __func__, slot));
	i = 0;
	for (;;) {
		for (;;) {
			fetch = info->bufsz - info->bpos;
#ifdef FEEDRATE_STRAY
			fetch -= info->stray;
#endif
			RATE_ASSERT(fetch >= 0,
			    ("%s: [1] Buffer overrun: %d > %d\n", __func__,
			    info->bpos, info->bufsz));
			if (slot < fetch)
				fetch = slot;
#ifdef FEEDRATE_STRAY
			if (fetch < 1)
#else
			if (fetch < smpsz)
#endif
				break;
			RATE_ASSERT((int)(info->bpos
#ifdef FEEDRATE_STRAY
			    - info->stray
#endif
			    ) >= 0 &&
			    (info->bpos  - info->stray) < info->bufsz,
			    ("%s: DANGER - BUFFER OVERRUN! bufsz=%d, pos=%d\n",
			    __func__, info->bufsz, info->bpos
#ifdef FEEDRATE_STRAY
			    - info->stray
#endif
			    ));
			fetch = FEEDER_FEED(f->source, c,
			    info->buffer + info->bpos
#ifdef FEEDRATE_STRAY
			    - info->stray
#endif
			    , fetch, source);
#ifdef FEEDRATE_STRAY
			info->stray = 0;
			if (fetch == 0)
#else
			if (fetch < smpsz)
#endif
				break;
			RATE_TEST((fetch % smpsz) == 0,
			    ("%s: Fetch size not sample integral (%d)\n",
			    __func__, fetch));
#ifdef FEEDRATE_STRAY
			info->stray += fetch % smpsz;
			RATE_TEST(info->stray == 0,
			    ("%s: Stray bytes detected (%d)\n", __func__,
			    info->stray));
#endif
			fetch -= fetch % smpsz;
			info->bpos += fetch;
			slot -= fetch;
			RATE_ASSERT(slot >= 0, ("%s: Negative Slot: %d\n",
			    __func__, slot));
			if (slot == 0 || info->bpos == info->bufsz)
				break;
		}
		if (info->pos == info->bpos) {
			RATE_TEST(info->pos == smpsz,
			    ("%s: EOF while in progress\n", __func__));
			break;
		}
		RATE_ASSERT(info->pos <= info->bpos,
		    ("%s: [2] Buffer overrun: %d > %d\n", __func__, info->pos,
		    info->bpos));
		RATE_ASSERT(info->pos < info->bpos,
		    ("%s: Zero buffer!\n", __func__));
		RATE_ASSERT(((info->bpos - info->pos) % smpsz) == 0,
		    ("%s: Buffer not sample integral (%d)\n", __func__,
		    info->bpos - info->pos));
		i += info->convert(info, b + i, count - i);
		RATE_ASSERT(info->pos <= info->bpos,
		    ("%s: [3] Buffer overrun: %d > %d\n", __func__, info->pos,
		    info->bpos));
		if (info->pos == info->bpos) {
			/*
			 * End of buffer cycle. Copy last unit sample
			 * to beginning of buffer so next cycle can
			 * interpolate using it.
			 */
#ifdef FEEDRATE_STRAY
			RATE_TEST(info->stray == 0,
			    ("%s: [2] Stray bytes: %u\n", __func__,
			    info->stray));
#endif
			bcopy(info->buffer + info->pos - smpsz, info->buffer,
			    smpsz);
			info->bpos = smpsz;
			info->pos = smpsz;
		}
		if (i == count)
			break;
	}

	RATE_TEST((slot == 0 && count == i) || (slot > 0 && count > i &&
	    info->pos == info->bpos && info->pos == smpsz),
	    ("%s: Inconsistent slot/count! "
	    "Count Expect: %u , Got: %u, Slot Left: %d\n", __func__, count, i,
	    slot));

#ifdef FEEDRATE_STRAY
	RATE_TEST(info->stray == 0, ("%s: [3] Stray bytes: %u\n", __func__,
	    info->stray));
#endif

	return (i);
}

static struct pcm_feederdesc feeder_rate_desc[] = {
	{FEEDER_RATE, AFMT_S8, AFMT_S8, 0},
	{FEEDER_RATE, AFMT_S16_LE, AFMT_S16_LE, 0},
	{FEEDER_RATE, AFMT_S24_LE, AFMT_S24_LE, 0},
	{FEEDER_RATE, AFMT_S32_LE, AFMT_S32_LE, 0},
	{FEEDER_RATE, AFMT_S16_BE, AFMT_S16_BE, 0},
	{FEEDER_RATE, AFMT_S24_BE, AFMT_S24_BE, 0},
	{FEEDER_RATE, AFMT_S32_BE, AFMT_S32_BE, 0},
	{FEEDER_RATE, AFMT_S8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_S24_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_S32_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_S24_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_S32_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_U8, AFMT_U8, 0},
	{FEEDER_RATE, AFMT_U16_LE, AFMT_U16_LE, 0},
	{FEEDER_RATE, AFMT_U24_LE, AFMT_U24_LE, 0},
	{FEEDER_RATE, AFMT_U32_LE, AFMT_U32_LE, 0},
	{FEEDER_RATE, AFMT_U16_BE, AFMT_U16_BE, 0},
	{FEEDER_RATE, AFMT_U24_BE, AFMT_U24_BE, 0},
	{FEEDER_RATE, AFMT_U32_BE, AFMT_U32_BE, 0},
	{FEEDER_RATE, AFMT_U8 | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_U24_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_U32_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_U24_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_RATE, AFMT_U32_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};

static kobj_method_t feeder_rate_methods[] = {
	KOBJMETHOD(feeder_init,		feed_rate_init),
	KOBJMETHOD(feeder_free,		feed_rate_free),
	KOBJMETHOD(feeder_set,		feed_rate_set),
	KOBJMETHOD(feeder_get,		feed_rate_get),
	KOBJMETHOD(feeder_feed,		feed_rate),
	{0, 0}
};

FEEDER_DECLARE(feeder_rate, 2, NULL);
