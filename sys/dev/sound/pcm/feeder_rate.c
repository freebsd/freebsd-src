/*
 * Copyright (c) 2003 Orion Hodson <orion@freebsd.org>
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
 * MAINTAINER: Orion Hodson <orion@freebsd.org>
 *
 * This rate conversion code uses linear interpolation without any
 * pre- or post- interpolation filtering to combat aliasing.  This
 * greatly limits the sound quality and should be addressed at some
 * stage in the future.
 * 
 * Since this accuracy of interpolation is sensitive and examination
 * of the algorithm output is harder from the kernel, th code is
 * designed to be compiled in the kernel and in a userland test
 * harness.  This is done by selectively including and excluding code
 * with several portions based on whether _KERNEL is defined.  It's a
 * little ugly, but exceedingly useful.  The testsuite and its
 * revisions can be found at:
 *		http://people.freebsd.org/~orion/feedrate/
 *
 * Special thanks to Ken Marx for exposing flaws in the code and for
 * testing revisions.
 */

#ifdef _KERNEL

#include <dev/sound/pcm/sound.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

#endif /* _KERNEL */

MALLOC_DEFINE(M_RATEFEEDER, "ratefeed", "pcm rate feeder");

#ifndef RATE_ASSERT
#define RATE_ASSERT(x, y) /* KASSERT(x) */
#endif /* RATE_ASSERT */

#ifndef RATE_TRACE
#define RATE_TRACE(x...)  /* printf(x) */
#endif

/*****************************************************************************/

/* The following coefficients are coupled.  They are chosen to be
 * guarantee calculable factors for the interpolation routine.  They
 * have been tested over the range of RATEMIN-RATEMAX Hz.  Decreasing
 * the granularity increases the required buffer size and affects the
 * gain values at different points in the space.  These values were
 * found by running the test program with -p (probe) and some trial
 * and error.
 *
 * ROUNDHZ	the granularity of sample rates (fits n*11025 and n*8000).
 * FEEDBUFSZ	the amount of buffer space.
 * MINGAIN	the minimum acceptable gain in coefficients search.
 */
#define ROUNDHZ			   25
#define FEEDBUFSZ 		 8192
#define MINGAIN			   92

#define RATEMIN  		 4000
#define RATEMAX 		48000

struct feed_rate_info;

typedef int (*rate_convert_method)(struct feed_rate_info *, 
				   uint32_t, uint32_t, int16_t *);

static int 
convert_stereo_up(struct feed_rate_info *info, 
		  uint32_t src_ticks, uint32_t dst_ticks, int16_t *dst);

static int
convert_stereo_down(struct feed_rate_info *info, 
		    uint32_t src_ticks, uint32_t dst_ticks, int16_t *dst);

struct feed_rate_info {
	uint32_t src, dst;	/* source and destination rates */
	uint16_t buffer_ticks;	/* number of available samples in buffer */
	uint16_t buffer_pos;	/* next available sample in buffer */
	uint16_t rounds; 	/* maximum number of cycle rounds w buffer */
	uint16_t alpha;		/* interpolation distance */
        uint16_t sscale;        /* src clock scale */
        uint16_t dscale;        /* dst clock scale */
        uint16_t mscale;        /* scale factor to avoid divide per sample */
        uint16_t mroll;         /* roll to again avoid divide per sample */
	uint16_t channels;	/* 1 = mono, 2 = stereo */

	rate_convert_method convert;
    	int16_t  buffer[FEEDBUFSZ];
};

#define bytes_per_sample		2
#define src_ticks_per_cycle(info)	(info->dscale * info->rounds)
#define dst_ticks_per_cycle(info)	(info->sscale * info->rounds)
#define bytes_per_tick(info)		(info->channels * bytes_per_sample)
#define src_bytes_per_cycle(info) 					      \
        		(src_ticks_per_cycle(info) * bytes_per_tick(info))
#define dst_bytes_per_cycle(info) 					      \
        		(dst_ticks_per_cycle(info) * bytes_per_tick(info))

static uint32_t
gcd(uint32_t x, uint32_t y)
{
	uint32_t w;
	while (y != 0) {
		w = x % y;
		x = y;
		y = w;
	}
	return x;
}

static int
feed_rate_setup(struct pcm_feeder *f)
{
	struct feed_rate_info *info = f->data;
        uint32_t mscale, mroll, l, r, g;
	
	/* Beat sample rates down by greatest common divisor */
	g = gcd(info->src, info->dst);
	info->sscale = info->dst / g;
	info->dscale = info->src / g;

	info->alpha = 0;
	info->buffer_ticks = 0; 
	info->buffer_pos = 0;

	/* Pick suitable conversion routine */
	if (info->src > info->dst) {
		info->convert = convert_stereo_down;
	} else {
		info->convert = convert_stereo_up;
	}

	/*
	 * Determine number of conversion rounds that will fit into
	 * buffer.  NB Must set info->rounds to one before using
	 * src_ticks_per_cycle here since it used by src_ticks_per_cycle.  
	 */
	info->rounds = 1;	
	r = (FEEDBUFSZ - bytes_per_tick(info)) / 
		(src_ticks_per_cycle(info) * bytes_per_tick(info));
	if (r == 0) {
		RATE_TRACE("Insufficient buffer space for conversion %d -> %d "
			   "(%d < %d)\n", info->src, info->dst, FEEDBUFSZ,
			   src_ticks_per_cycle(info) * bytes_per_tick(info));
		return -1;
	}
	info->rounds = r;
	
	/*
	 * Find scale and roll combination that allows us to trade 
	 * costly divide operations in the main loop for multiply-rolls.
	 */
        for (l = 96; l >= MINGAIN; l -= 3) {
		for (mroll = 0; mroll < 16; mroll ++) {
			mscale = (1 << mroll) / info->sscale;

                        r = (mscale * info->sscale * 100) >> mroll;
                        if (r > l && r <= 100) {
                                info->mscale = mscale;
                                info->mroll = mroll;
                                RATE_TRACE("Converting %d to %d with "
					   "mscale = %d and mroll = %d "
					   "(gain = %d / 100)\n",
					   info->src, info->dst,
					   info->mscale, info->mroll, r);
                                return 0;
                        }
                }
        }
	
	RATE_TRACE("Failed to find a converter within %d%% gain for "
		   "%d to %d.\n", l, info->src, info->dst);

        return -2;
}

static int
feed_rate_set(struct pcm_feeder *f, int what, int value)
{
	struct feed_rate_info *info = f->data;
	int rvalue;
	
	if (value < RATEMIN || value > RATEMAX) {
		return -1;
	}
	
	rvalue = (value / ROUNDHZ) * ROUNDHZ;
	if (value - rvalue > ROUNDHZ / 2) {
	    rvalue += ROUNDHZ;
	}
	
	switch(what) {
	case FEEDRATE_SRC:
		info->src = rvalue;
		break;
	case FEEDRATE_DST:
		info->dst = rvalue;
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

	switch(what) {
	case FEEDRATE_SRC:
		return info->src;
	case FEEDRATE_DST:
		return info->dst;
	default:
		return -1;
	}
	return -1;
}

static int
feed_rate_init(struct pcm_feeder *f)
{
	struct feed_rate_info *info;

	info = malloc(sizeof(*info), M_RATEFEEDER, M_WAITOK | M_ZERO);
	if (info == NULL)
		return ENOMEM;

	info->src = DSP_DEFAULT_SPEED;
	info->dst = DSP_DEFAULT_SPEED;
	info->channels = 2;

	f->data = info;
	return 0;
}

static int
feed_rate_free(struct pcm_feeder *f)
{
	struct feed_rate_info *info = f->data;

	if (info) {
		free(info, M_RATEFEEDER);
	}
	f->data = NULL;
	return 0;
}

static int
convert_stereo_up(struct feed_rate_info *info, 
		  uint32_t		 src_ticks, 
		  uint32_t		 dst_ticks, 
		  int16_t		*dst)
{
	uint32_t max_dst_ticks;
	int32_t alpha, dalpha, malpha, mroll, sp, dp, se, de, x, o;
	int16_t *src;

	sp = info->buffer_pos * 2;
	se = sp + src_ticks * 2;

	src = info->buffer;
	alpha = info->alpha * info->mscale;
	dalpha = info->dscale * info->mscale; /* Alpha increment */
	malpha = info->sscale * info->mscale; /* Maximum allowed alpha value */
	mroll = info->mroll;

	/*
	 * For efficiency the main conversion loop should only depend on
	 * one variable.  We use the state to work out the maximum number
	 * of output samples that are available and eliminate the checking of
	 * sp from the loop.
	 */
	max_dst_ticks = src_ticks * info->dst / info->src - alpha / dalpha;
	if (max_dst_ticks < dst_ticks) {
		dst_ticks = max_dst_ticks;
	}

	dp = 0;
	de = dst_ticks * 2;
	/*
	 * Unrolling this loop manually does not help much here because
	 * of the alpha, malpha comparison.
	 */
	while (dp < de) {
		o = malpha - alpha;
		x = alpha * src[sp + 2] + o * src[sp];
		dst[dp++] = x >> mroll;
		x = alpha * src[sp + 3] + o * src[sp + 1];
		dst[dp++] = x >> mroll;
		alpha += dalpha;
		if (alpha >= malpha) {
			alpha -= malpha;
			sp += 2;
		}
	}
	RATE_ASSERT(sp <= se, ("%s: Source overrun\n", __func__)); 

	info->buffer_pos = sp / info->channels;
	info->alpha = alpha / info->mscale;

	return dp / info->channels;
}

static int
convert_stereo_down(struct feed_rate_info *info, 
		    uint32_t		   src_ticks, 
		    uint32_t		   dst_ticks, 
		    int16_t		  *dst)
{
	int32_t alpha, dalpha, malpha, mroll, sp, dp, se, de, x, o, m, 
		mdalpha, mstep;
	int16_t *src;

	sp = info->buffer_pos * 2;
	se = sp + src_ticks * 2;

	src = info->buffer;
	alpha = info->alpha * info->mscale;
	dalpha = info->dscale * info->mscale; /* Alpha increment */
	malpha = info->sscale * info->mscale; /* Maximum allowed alpha value */
	mroll = info->mroll;

	dp = 0;
	de = dst_ticks * 2;

	m = dalpha / malpha;
	mstep = m * 2;
	mdalpha = dalpha - m * malpha;

	/*
	 * TODO: eliminate sp or dp from this loop comparison for a few 
	 * extra % performance.
	 */
	while (sp < se && dp < de) {
		o = malpha - alpha;
		x = alpha * src[sp + 2] + o * src[sp];
		dst[dp++] = x >> mroll;
		x = alpha * src[sp + 3] + o * src[sp + 1];
		dst[dp++] = x >> mroll;

		alpha += mdalpha;
		sp += mstep;
		if (alpha >= malpha) {
			alpha -= malpha;
			sp += 2;
		}
	}

	info->buffer_pos = sp / 2;
	info->alpha = alpha / info->mscale;

	RATE_ASSERT(info->buffer_pos <= info->buffer_ticks, 
		    ("%s: Source overrun\n", __func__)); 

	return dp / 2;
}

static int
feed_rate(struct pcm_feeder	*f, 
	  struct pcm_channel	*c, 
	  uint8_t		*b,
	  uint32_t		 count, 
	  void			*source)
{
	struct feed_rate_info *info = f->data;

	uint32_t done, s_ticks, d_ticks;
	done = 0;

	RATE_ASSERT(info->channels == 2, 
		    ("%s: channels (%d) != 2", __func__, info->channels));

	while (done < count) {
		/* Slurp in more data if input buffer is not full */
		while (info->buffer_ticks < src_ticks_per_cycle(info)) {
			uint8_t *u8b;
			int	 fetch;
			fetch = src_bytes_per_cycle(info) - 
				info->buffer_ticks * bytes_per_tick(info);
			u8b = (uint8_t*)info->buffer + 
				(info->buffer_ticks + 1) *
				bytes_per_tick(info);
			fetch = FEEDER_FEED(f->source, c, u8b, fetch, source);
			RATE_ASSERT(fetch % bytes_per_tick(info) == 0,
				    ("%s: fetched unaligned bytes (%d)",
				     __func__, fetch));
			info->buffer_ticks += fetch / bytes_per_tick(info);
			RATE_ASSERT(src_ticks_per_cycle(info) >= 
				    info->buffer_ticks,
				    ("%s: buffer overfilled (%d > %d).",
				     __func__, info->buffer_ticks, 
				 src_ticks_per_cycle(info)));
			if (fetch == 0)
				break;
		}

		/* Find amount of input buffer data that should be processed */
		d_ticks = (count - done) / bytes_per_tick(info);
		s_ticks = info->buffer_ticks - info->buffer_pos;
		if (info->buffer_ticks != src_ticks_per_cycle(info)) {
			if (s_ticks > 8)
				s_ticks -= 8;
			else
				s_ticks = 0;
		}

		d_ticks = info->convert(info, s_ticks, d_ticks,
					(int16_t*)(b + done));
		if (d_ticks == 0)
			break;
		done += d_ticks * bytes_per_tick(info);

		RATE_ASSERT(info->buffer_pos <= info->buffer_ticks,
			    ("%s: buffer_ticks too big\n", __func__));
		RATE_ASSERT(info->buffer_ticks <= src_ticks_per_cycle(info),
			    ("too many ticks %d /  %d\n",
			     info->buffer_ticks, src_ticks_per_cycle(info)));
		RATE_TRACE("%s: ticks %5d / %d pos %d\n", __func__,
			   info->buffer_ticks, src_ticks_per_cycle(info),
			   info->buffer_pos);

		if (src_ticks_per_cycle(info) <= info->buffer_pos) {
			/* End of cycle reached, copy last samples to start */
			uint8_t *u8b;
			u8b = (uint8_t*)info->buffer;
			bcopy(u8b + src_bytes_per_cycle(info), u8b, 
			      bytes_per_tick(info));

			RATE_ASSERT(info->alpha == 0,
				    ("%s: completed cycle with "
				     "alpha non-zero", __func__, info->alpha));
			
			info->buffer_pos = 0;
			info->buffer_ticks = 0;
		}
	}
	
	RATE_ASSERT(count >= done, 
		    ("%s: generated too many bytes of data (%d > %d).",
		     __func__, done, count));

	if (done != count) {
		RATE_TRACE("Only did %d of %d\n", done, count);
	}

	return done;
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

