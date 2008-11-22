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

#define RATE_ASSERT(x, y) /* KASSERT(x,y) */
#define RATE_TEST(x, y)  /* if (!(x)) printf y */
#define RATE_TRACE(x...) /* printf(x) */

MALLOC_DEFINE(M_RATEFEEDER, "ratefeed", "pcm rate feeder");

#define FEEDBUFSZ	8192
#define ROUNDHZ		25
#define RATEMIN		4000
/* 8000 * 138 or 11025 * 100 . This is insane, indeed! */
#define RATEMAX		1102500
#define MINGAIN		92
#define MAXGAIN		96

#define FEEDRATE_CONVERT_64		0
#define FEEDRATE_CONVERT_SCALE64	1
#define FEEDRATE_CONVERT_SCALE32	2
#define FEEDRATE_CONVERT_PLAIN		3
#define FEEDRATE_CONVERT_FIXED		4
#define FEEDRATE_CONVERT_OPTIMAL	5
#define FEEDRATE_CONVERT_WORST		6

#define FEEDRATE_64_MAXROLL	32
#define FEEDRATE_32_MAXROLL	16

struct feed_rate_info {
	uint32_t src, dst;	/* rounded source / destination rates */
	uint32_t rsrc, rdst;	/* original source / destination rates */
	uint32_t gx, gy;	/* interpolation / decimation ratio */
	uint32_t alpha;		/* interpolation distance */
	uint32_t pos, bpos;	/* current sample / buffer positions */
	uint32_t bufsz;		/* total buffer size */
	uint32_t stray;		/* stray bytes */
	int32_t  scale, roll;	/* scale / roll factor */
	int16_t  *buffer;
	uint32_t (*convert)(struct feed_rate_info *, int16_t *, uint32_t);
};

static uint32_t
feed_convert_64(struct feed_rate_info *, int16_t *, uint32_t);
static uint32_t
feed_convert_scale64(struct feed_rate_info *, int16_t *, uint32_t);
static uint32_t
feed_convert_scale32(struct feed_rate_info *, int16_t *, uint32_t);
static uint32_t
feed_convert_plain(struct feed_rate_info *, int16_t *, uint32_t);

int feeder_rate_ratemin = RATEMIN;
int feeder_rate_ratemax = RATEMAX;
/*
 * See 'Feeder Scaling Type' below..
 */
static int feeder_rate_scaling = FEEDRATE_CONVERT_OPTIMAL;
static int feeder_rate_buffersize = FEEDBUFSZ & ~1;

#if 0
/* 
 * sysctls.. I love sysctls..
 */
TUNABLE_INT("hw.snd.feeder_rate_ratemin", &feeder_rate_ratemin);
TUNABLE_INT("hw.snd.feeder_rate_ratemax", &feeder_rate_ratemin);
TUNABLE_INT("hw.snd.feeder_rate_scaling", &feeder_rate_scaling);
TUNABLE_INT("hw.snd.feeder_rate_buffersize", &feeder_rate_buffersize);

static int
sysctl_hw_snd_feeder_rate_ratemin(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = feeder_rate_ratemin;
	err = sysctl_handle_int(oidp, &val, sizeof(val), req);
	if (val < 1 || val >= feeder_rate_ratemax)
		err = EINVAL;
	else
		feeder_rate_ratemin = val;
	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_rate_ratemin, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_rate_ratemin, "I", "");

static int
sysctl_hw_snd_feeder_rate_ratemax(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = feeder_rate_ratemax;
	err = sysctl_handle_int(oidp, &val, sizeof(val), req);
	if (val <= feeder_rate_ratemin || val > 0x7fffff)
		err = EINVAL;
	else
		feeder_rate_ratemax = val;
	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_rate_ratemax, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_rate_ratemax, "I", "");

static int
sysctl_hw_snd_feeder_rate_scaling(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = feeder_rate_scaling;
	err = sysctl_handle_int(oidp, &val, sizeof(val), req);
	/*
	 *      Feeder Scaling Type
	 *      ===================
	 *
	 *	1. Plain 64bit (high precision)
	 *	2. 64bit scaling (high precision, CPU friendly, but can
	 *	   cause gain up/down).
	 *	3. 32bit scaling (somehow can cause hz roundup, gain
	 *	   up/down).
	 *	4. Plain copy (default if src == dst. Except if src == dst,
	 *	   this is the worst / silly conversion method!).
	 *
	 *	Sysctl options:-
	 *
	 *	0 - Plain 64bit - no fallback.
	 *	1 - 64bit scaling - no fallback.
	 *	2 - 32bit scaling - no fallback.
	 *	3 - Plain copy - no fallback.
	 *	4 - Fixed rate. Means that, choose optimal conversion method
	 *	    without causing hz roundup.
	 *	    32bit scaling (as long as hz roundup does not occur),
	 *	    64bit scaling, Plain 64bit.
	 *	5 - Optimal / CPU friendly (DEFAULT).
	 *	    32bit scaling, 64bit scaling, Plain 64bit
	 *	6 - Optimal to worst, no 64bit arithmetic involved.
	 *	    32bit scaling, Plain copy.
	 */
	if (val < FEEDRATE_CONVERT_64 || val > FEEDRATE_CONVERT_WORST)
		err = EINVAL;
	else
		feeder_rate_scaling = val;
	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_rate_scaling, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_rate_scaling, "I", "");

static int
sysctl_hw_snd_feeder_rate_buffersize(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = feeder_rate_buffersize;
	err = sysctl_handle_int(oidp, &val, sizeof(val), req);
	/*
	 * Don't waste too much kernel space
	 */
	if (val < 2 || val > 65536)
		err = EINVAL;
	else
		feeder_rate_buffersize = val & ~1;
	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_rate_buffersize, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_rate_buffersize, "I", "");
#endif

static void
feed_speed_ratio(uint32_t x, uint32_t y, uint32_t *gx, uint32_t *gy)
{
	uint32_t w, src = x, dst = y;

	while (y != 0) {
		w = x % y;
		x = y;
		y = w;
	}
	*gx = src / x;
	*gy = dst / x;
}

static void
feed_scale_roll(uint32_t dst, int32_t *scale, int32_t *roll, int32_t max)
{
	int64_t k, tscale;
	int32_t j, troll;

	*scale = *roll = -1;
	for (j = MAXGAIN; j >= MINGAIN; j -= 3) {
		for (troll = 0; troll < max; troll++) {
			tscale = (1 << troll) / dst;
			k = (tscale * dst * 100) >> troll;
			if (k > j && k <= 100) {
				*scale = tscale;
				*roll = troll;
				return;
			}
		}
	}
}

static int
feed_get_best_coef(uint32_t *src, uint32_t *dst, uint32_t *gx, uint32_t *gy,
			int32_t *scale, int32_t *roll)
{
	uint32_t tsrc, tdst, sscale, dscale;
	int32_t tscale, troll;
	int i, j, hzmin, hzmax;

	*scale = *roll = -1;
	for (i = 0; i < 2; i++) {
		hzmin = (ROUNDHZ * i) + 1;
		hzmax = hzmin + ROUNDHZ;
		for (j = hzmin; j < hzmax; j++) {
			tsrc = *src - (*src % j);
			tdst = *dst;
			if (tsrc < 1 || tdst < 1)
				goto coef_failed;
			feed_speed_ratio(tsrc, tdst, &sscale, &dscale);
			feed_scale_roll(dscale, &tscale, &troll,
						FEEDRATE_32_MAXROLL);
			if (tscale != -1 && troll != -1) {
				*src = tsrc;
				*gx = sscale;
				*gy = dscale;
				*scale = tscale;
				*roll = troll;
				return j;
			}
		}
		for (j = hzmin; j < hzmax; j++) {
			tsrc = *src - (*src % j);
			tdst = *dst - (*dst % j);
			if (tsrc < 1 || tdst < 1)
				goto coef_failed;
			feed_speed_ratio(tsrc, tdst, &sscale, &dscale);
			feed_scale_roll(dscale, &tscale, &troll,
						FEEDRATE_32_MAXROLL);
			if (tscale != -1 && troll != -1) {
				*src = tsrc;
				*dst = tdst;
				*gx = sscale;
				*gy = dscale;
				*scale = tscale;
				*roll = troll;
				return j;
			}
		}
		for (j = hzmin; j < hzmax; j++) {
			tsrc = *src;
			tdst = *dst - (*dst % j);
			if (tsrc < 1 || tdst < 1)
				goto coef_failed;
			feed_speed_ratio(tsrc, tdst, &sscale, &dscale);
			feed_scale_roll(dscale, &tscale, &troll,
						FEEDRATE_32_MAXROLL);
			if (tscale != -1 && troll != -1) {
				*src = tsrc;
				*dst = tdst;
				*gx = sscale;
				*gy = dscale;
				*scale = tscale;
				*roll = troll;
				return j;
			}
		}
	}
coef_failed:
	feed_speed_ratio(*src, *dst, gx, gy);
	feed_scale_roll(*gy, scale, roll, FEEDRATE_32_MAXROLL);
	return 0;
}

static void
feed_rate_reset(struct feed_rate_info *info)
{
	info->scale = -1;
	info->roll = -1;
	info->src = info->rsrc;
	info->dst = info->rdst;
	info->gx = 0;
	info->gy = 0;
}

static int
feed_rate_setup(struct pcm_feeder *f)
{
	struct feed_rate_info *info = f->data;
	int r = 0;

	info->pos = 2;
	info->bpos = 4;
	info->alpha = 0;
	info->stray = 0;
	feed_rate_reset(info);
	if (info->src == info->dst) {
		/*
		 * No conversion ever needed. Just do plain copy.
		 */
		info->convert = feed_convert_plain;
		info->gx = 1;
		info->gy = 1;
	} else {
		switch (feeder_rate_scaling) {
			case FEEDRATE_CONVERT_64:
				feed_speed_ratio(info->src, info->dst,
					&info->gx, &info->gy);
				info->convert = feed_convert_64;
				break;
			case FEEDRATE_CONVERT_SCALE64:
				feed_speed_ratio(info->src, info->dst,
					&info->gx, &info->gy);
				feed_scale_roll(info->gy, &info->scale,
					&info->roll, FEEDRATE_64_MAXROLL);
				if (info->scale == -1 || info->roll == -1)
					return -1;
				info->convert = feed_convert_scale64;
				break;
			case FEEDRATE_CONVERT_SCALE32:
				r = feed_get_best_coef(&info->src, &info->dst,
					&info->gx, &info->gy, &info->scale,
					&info->roll);
				if (r == 0)
					return -1;
				info->convert = feed_convert_scale32;
				break;
			case FEEDRATE_CONVERT_PLAIN:
				feed_speed_ratio(info->src, info->dst,
					&info->gx, &info->gy);
				info->convert = feed_convert_plain;
				break;
			case FEEDRATE_CONVERT_FIXED:
				r = feed_get_best_coef(&info->src, &info->dst,
					&info->gx, &info->gy, &info->scale,
					&info->roll);
				if (r != 0 && info->src == info->rsrc &&
						info->dst == info->rdst)
					info->convert = feed_convert_scale32;
				else {
					/* Fallback */
					feed_rate_reset(info);
					feed_speed_ratio(info->src, info->dst,
						&info->gx, &info->gy);
					feed_scale_roll(info->gy, &info->scale,
						&info->roll, FEEDRATE_64_MAXROLL);
					if (info->scale != -1 && info->roll != -1)
						info->convert = feed_convert_scale64;
					else
						info->convert = feed_convert_64;
				}
				break;
			case FEEDRATE_CONVERT_OPTIMAL:
				r = feed_get_best_coef(&info->src, &info->dst,
					&info->gx, &info->gy, &info->scale,
					&info->roll);
				if (r != 0)
					info->convert = feed_convert_scale32;
				else {
					/* Fallback */
					feed_rate_reset(info);
					feed_speed_ratio(info->src, info->dst,
						&info->gx, &info->gy);
					feed_scale_roll(info->gy, &info->scale,
						&info->roll, FEEDRATE_64_MAXROLL);
					if (info->scale != -1 && info->roll != -1)
						info->convert = feed_convert_scale64;
					else
						info->convert = feed_convert_64;
				}
				break;
			case FEEDRATE_CONVERT_WORST:
				r = feed_get_best_coef(&info->src, &info->dst,
					&info->gx, &info->gy, &info->scale,
					&info->roll);
				if (r != 0)
					info->convert = feed_convert_scale32;
				else {
					/* Fallback */
					feed_rate_reset(info);
					feed_speed_ratio(info->src, info->dst,
						&info->gx, &info->gy);
					info->convert = feed_convert_plain;
				}
				break;
			default:
				return -1;
				break;
		}
		/* No way! */
		if (info->gx == 0 || info->gy == 0)
			return -1;
		/*
		 * No need to interpolate/decimate, just do plain copy.
		 * This probably caused by Hz roundup.
		 */
		if (info->gx == info->gy)
			info->convert = feed_convert_plain;
	}
	return 0;
}

static int
feed_rate_set(struct pcm_feeder *f, int what, int value)
{
	struct feed_rate_info *info = f->data;

	if (value < feeder_rate_ratemin || value > feeder_rate_ratemax)
		return -1;
	
	switch (what) {
		case FEEDRATE_SRC:
			info->rsrc = value;
			break;
		case FEEDRATE_DST:
			info->rdst = value;
			break;
		default:
			return -1;
	}
	return feed_rate_setup(f);
}

static int
feed_rate_get(struct pcm_feeder *f, int what)
{
	struct feed_rate_info *info = f->data;

	/*
	 * Return *real* src/dst rate.
	 */
	switch (what) {
		case FEEDRATE_SRC:
			return info->rsrc;
		case FEEDRATE_DST:
			return info->rdst;
		default:
			return -1;
	}
	return -1;
}

static int
feed_rate_init(struct pcm_feeder *f)
{
	struct feed_rate_info *info;

	info = malloc(sizeof(*info), M_RATEFEEDER, M_NOWAIT | M_ZERO);
	if (info == NULL)
		return ENOMEM;
	/*
	 * bufsz = sample from last cycle + conversion space
	 */
	info->bufsz = 2 + feeder_rate_buffersize;
	info->buffer = malloc(sizeof(*info->buffer) * info->bufsz,
					M_RATEFEEDER, M_NOWAIT | M_ZERO);
	if (info->buffer == NULL) {
		free(info, M_RATEFEEDER);
		return ENOMEM;
	}
	info->rsrc = DSP_DEFAULT_SPEED;
	info->rdst = DSP_DEFAULT_SPEED;
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

static uint32_t
feed_convert_64(struct feed_rate_info *info, int16_t *dst, uint32_t max)
{
	int64_t x, alpha, distance;
	uint32_t ret;
	int32_t pos, bpos, gx, gy;
	int16_t *src;
	/*
	 * Plain, straight forward 64bit arith. No bit-magic applied here.
	 */
	ret = 0;
	alpha = info->alpha;
	gx = info->gx;
	gy = info->gy;
	pos = info->pos;
	bpos = info->bpos;
	src = info->buffer;
	for (;;) {
		if (alpha < gx) {
			alpha += gy;
			pos += 2;
			if (pos == bpos)
				break;
		} else {
			alpha -= gx;
			distance = gy - alpha;
			x = (alpha * src[pos - 2]) + (distance * src[pos]);
			dst[ret++] = x / gy;
			x = (alpha * src[pos - 1]) + (distance * src[pos + 1]);
			dst[ret++] = x / gy;
			if (ret == max)
				break;
		}
	}
	info->alpha = alpha;
	info->pos = pos;
	return ret;
}

static uint32_t
feed_convert_scale64(struct feed_rate_info *info, int16_t *dst, uint32_t max)
{
	int64_t x, alpha, distance;
	uint32_t ret;
	int32_t pos, bpos, gx, gy, roll;
	int16_t *src;
	/*
	 * 64bit scaling.
	 */
	ret = 0;
	roll = info->roll;
	alpha = info->alpha * info->scale;
	gx = info->gx * info->scale;
	gy = info->gy * info->scale;
	pos = info->pos;
	bpos = info->bpos;
	src = info->buffer;
	for (;;) {
		if (alpha < gx) {
			alpha += gy;
			pos += 2;
			if (pos == bpos)
				break;
		} else {
			alpha -= gx;
			distance = gy - alpha;
			x = (alpha * src[pos - 2]) + (distance * src[pos]);
			dst[ret++] = x >> roll;
			x = (alpha * src[pos - 1]) + (distance * src[pos + 1]);
			dst[ret++] = x >> roll;
			if (ret == max)
				break;
		}
	}
	info->alpha = alpha / info->scale;
	info->pos = pos;
	return ret;
}

static uint32_t
feed_convert_scale32(struct feed_rate_info *info, int16_t *dst, uint32_t max)
{
	uint32_t ret;
	int32_t x, pos, bpos, gx, gy, alpha, roll, distance;
	int16_t *src;
	/*
	 * 32bit scaling.
	 */
	ret = 0;
	roll = info->roll;
	alpha = info->alpha * info->scale;
	gx = info->gx * info->scale;
	gy = info->gy * info->scale;
	pos = info->pos;
	bpos = info->bpos;
	src = info->buffer;
	for (;;) {
		if (alpha < gx) {
			alpha += gy;
			pos += 2;
			if (pos == bpos)
				break;
		} else {
			alpha -= gx;
			distance = gy - alpha;
			x = (alpha * src[pos - 2]) + (distance * src[pos]);
			dst[ret++] = x >> roll;
			x = (alpha * src[pos - 1]) + (distance * src[pos + 1]);
			dst[ret++] = x >> roll;
			if (ret == max)
				break;
		}
	}
	info->alpha = alpha / info->scale;
	info->pos = pos;
	return ret;
}

static uint32_t
feed_convert_plain(struct feed_rate_info *info, int16_t *dst, uint32_t max)
{
	uint32_t ret;
	int32_t pos, bpos, gx, gy, alpha;
	int16_t *src;
	/*
	 * Plain copy.
	 */
	ret = 0;
	gx = info->gx;
	gy = info->gy;
	alpha = info->alpha;
	pos = info->pos;
	bpos = info->bpos;
	src = info->buffer;
	for (;;) {
		if (alpha < gx) {
			alpha += gy;
			pos += 2;
			if (pos == bpos)
				break;
		} else {
			alpha -= gx;
			dst[ret++] = src[pos];
			dst[ret++] = src[pos + 1];
			if (ret == max)
				break;
		}
	}
	info->pos = pos;
	info->alpha = alpha;
	return ret;
}

static int32_t
feed_rate(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
			uint32_t count, void *source)
{
	struct feed_rate_info *info = f->data;
	uint32_t i;
	int32_t fetch, slot;
	int16_t *dst = (int16_t *)b;
	/*
	 * This loop has been optimized to generalize both up / down
	 * sampling without causing missing samples or excessive buffer
	 * feeding.
	 */
	RATE_TEST(count >= 4 && (count & 3) == 0,
		("%s: Count size not byte integral (%d)\n", __func__, count));
	if (count < 4)
		return 0;
	count >>= 1;
	count &= ~1;
	slot = (((info->gx * (count >> 1)) + info->gy - info->alpha - 1) / info->gy) << 1;
	RATE_TEST((slot & 1) == 0, ("%s: Slot count not sample integral (%d)\n",
						__func__, slot));
	/*
	 * Optimize buffer feeding aggressively to ensure calculated slot
	 * can be fitted nicely into available buffer free space, hence
	 * avoiding multiple feeding.
	 */
	RATE_TEST(info->stray == 0, ("%s: [1] Stray bytes: %u\n",
		__func__,info->stray));
	if (info->pos != 2 && info->bpos - info->pos == 2 &&
			info->bpos + slot > info->bufsz) {
		/*
		 * Copy last unit sample and its previous to
		 * beginning of buffer.
		 */
		info->buffer[0] = info->buffer[info->pos - 2];
		info->buffer[1] = info->buffer[info->pos - 1];
		info->buffer[2] = info->buffer[info->pos];
		info->buffer[3] = info->buffer[info->pos + 1];
		info->pos = 2;
		info->bpos = 4;
	}
	RATE_ASSERT(slot >= 0, ("%s: Negative Slot: %d\n",
			__func__, slot));
	i = 0;
	for (;;) {
		for (;;) {
			fetch = (info->bufsz - info->bpos) << 1;
			fetch -= info->stray;
			RATE_ASSERT(fetch >= 0,
				("%s: [1] Buffer overrun: %d > %d\n",
					__func__, info->bpos, info->bufsz));
			if ((slot << 1) < fetch)
				fetch = slot << 1;
			if (fetch > 0) {
				RATE_ASSERT(((info->bpos << 1) - info->stray) >= 0 &&
					((info->bpos << 1) - info->stray) < (info->bufsz << 1),
					("%s: DANGER - BUFFER OVERRUN! bufsz=%d, pos=%d\n", __func__,
					info->bufsz << 1, (info->bpos << 1) - info->stray));
				fetch = FEEDER_FEED(f->source, c,
						(uint8_t *)(info->buffer) + (info->bpos << 1) - info->stray,
						fetch, source);
				info->stray = 0;
				if (fetch == 0)
					break;
				RATE_TEST((fetch & 3) == 0,
					("%s: Fetch size not byte integral (%d)\n",
					__func__, fetch));
				info->stray += fetch & 3;
				RATE_TEST(info->stray == 0,
					("%s: Stray bytes detected (%d)\n",
					__func__, info->stray));
				fetch >>= 1;
				fetch &= ~1;
				info->bpos += fetch;
				slot -= fetch;
				RATE_ASSERT(slot >= 0,
					("%s: Negative Slot: %d\n", __func__,
						slot));
				if (slot == 0)
					break;
				if (info->bpos == info->bufsz)
					break;
			} else
				break;
		}
		if (info->pos == info->bpos) {
			RATE_TEST(info->pos == 2,
				("%s: EOF while in progress\n", __func__));
			break;
		}
		RATE_ASSERT(info->pos <= info->bpos,
			("%s: [2] Buffer overrun: %d > %d\n", __func__,
			info->pos, info->bpos));
		RATE_ASSERT(info->pos < info->bpos,
			("%s: Zero buffer!\n", __func__));
		RATE_ASSERT(((info->bpos - info->pos) & 1) == 0,
			("%s: Buffer not sample integral (%d)\n",
			__func__, info->bpos - info->pos));
		i += info->convert(info, dst + i, count - i);
		RATE_ASSERT(info->pos <= info->bpos,
				("%s: [3] Buffer overrun: %d > %d\n",
					__func__, info->pos, info->bpos));
		if (info->pos == info->bpos) {
			/*
			 * End of buffer cycle. Copy last unit sample
			 * to beginning of buffer so next cycle can
			 * interpolate using it.
			 */
			RATE_TEST(info->stray == 0, ("%s: [2] Stray bytes: %u\n", __func__, info->stray));
			info->buffer[0] = info->buffer[info->pos - 2];
			info->buffer[1] = info->buffer[info->pos - 1];
			info->bpos = 2;
			info->pos = 2;
		}
		if (i == count)
			break;
	}
#if 0
	RATE_TEST(count == i, ("Expect: %u , Got: %u\n", count << 1, i << 1));
#endif
	RATE_TEST(info->stray == 0, ("%s: [3] Stray bytes: %u\n", __func__, info->stray));
	return i << 1;
}

static struct pcm_feederdesc feeder_rate_desc[] = {
	{FEEDER_RATE, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
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
