/*-
 * Copyright (c) 2001 Cameron Grant <cg@FreeBSD.org>
 * Copyright (c) 2006 Ariff Abdullah <ariff@FreeBSD.org>
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

/* Almost entirely rewritten to add multi-format/channels mixing support. */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD: src/sys/dev/sound/pcm/vchan.c,v 1.36.6.1 2008/11/25 02:59:29 kensmith Exp $");

MALLOC_DEFINE(M_VCHANFEEDER, "vchanfeed", "pcm vchan feeder");

typedef uint32_t (*feed_vchan_mixer)(uint8_t *, uint8_t *, uint32_t);

struct vchinfo {
	struct pcm_channel *channel;
	struct pcmchan_caps caps;
	uint32_t fmtlist[2];
	int trigger;
};

/* support everything (mono / stereo), except a-law / mu-law */
static struct afmtstr_table vchan_supported_fmts[] = {
	{    "u8", AFMT_U8     }, {    "s8", AFMT_S8     },
	{ "s16le", AFMT_S16_LE }, { "s16be", AFMT_S16_BE },
	{ "u16le", AFMT_U16_LE }, { "u16be", AFMT_U16_BE },
	{ "s24le", AFMT_S24_LE }, { "s24be", AFMT_S24_BE },
	{ "u24le", AFMT_U24_LE }, { "u24be", AFMT_U24_BE },
	{ "s32le", AFMT_S32_LE }, { "s32be", AFMT_S32_BE },
	{ "u32le", AFMT_U32_LE }, { "u32be", AFMT_U32_BE },
	{    NULL, 0           },
};

/* alias table, shorter. */
static const struct {
	char *alias, *fmtstr;
} vchan_fmtstralias[] = {
	{  "8", "u8"    }, { "16", "s16le" },
	{ "24", "s24le" }, { "32", "s32le" },
	{ NULL, NULL    },
};

#define vchan_valid_format(fmt) \
	afmt2afmtstr(vchan_supported_fmts, fmt, NULL, 0, 0, \
	AFMTSTR_STEREO_RETURN)
#define vchan_valid_strformat(strfmt) \
	afmtstr2afmt(vchan_supported_fmts, strfmt, AFMTSTR_STEREO_RETURN);

/*
 * Need specialized WRITE macros since 32bit might involved saturation
 * if calculation is done within 32bit arithmetic.
 */
#define VCHAN_PCM_WRITE_S8_NE(b8, val)		PCM_WRITE_S8(b8, val)
#define VCHAN_PCM_WRITE_S16_LE(b8, val)		PCM_WRITE_S16_LE(b8, val)
#define VCHAN_PCM_WRITE_S24_LE(b8, val)		PCM_WRITE_S24_LE(b8, val)
#define VCHAN_PCM_WRITE_S32_LE(b8, val)		_PCM_WRITE_S32_LE(b8, val)
#define VCHAN_PCM_WRITE_S16_BE(b8, val)		PCM_WRITE_S16_BE(b8, val)
#define VCHAN_PCM_WRITE_S24_BE(b8, val)		PCM_WRITE_S24_BE(b8, val)
#define VCHAN_PCM_WRITE_S32_BE(b8, val)		_PCM_WRITE_S32_BE(b8, val)
#define VCHAN_PCM_WRITE_U8_NE(b8, val)		PCM_WRITE_U8(b8, val)
#define VCHAN_PCM_WRITE_U16_LE(b8, val)		PCM_WRITE_U16_LE(b8, val)
#define VCHAN_PCM_WRITE_U24_LE(b8, val)		PCM_WRITE_U24_LE(b8, val)
#define VCHAN_PCM_WRITE_U32_LE(b8, val)		_PCM_WRITE_U32_LE(b8, val)
#define VCHAN_PCM_WRITE_U16_BE(b8, val)		PCM_WRITE_U16_BE(b8, val)
#define VCHAN_PCM_WRITE_U24_BE(b8, val)		PCM_WRITE_U24_BE(b8, val)
#define VCHAN_PCM_WRITE_U32_BE(b8, val)		_PCM_WRITE_U32_BE(b8, val)

#define FEEDER_VCHAN_MIX(FMTBIT, VCHAN_INTCAST, SIGN, SIGNS, ENDIAN, ENDIANS)	\
static uint32_t									\
feed_vchan_mix_##SIGNS##FMTBIT##ENDIANS(uint8_t *to, uint8_t *tmp,		\
							uint32_t count)		\
{										\
	int32_t x, y;								\
	VCHAN_INTCAST z;							\
	int i;									\
										\
	i = count;								\
	tmp += i;								\
	to += i;								\
										\
	do {									\
		tmp -= PCM_##FMTBIT##_BPS;					\
		to -= PCM_##FMTBIT##_BPS;					\
		i -= PCM_##FMTBIT##_BPS;					\
		x = PCM_READ_##SIGN##FMTBIT##_##ENDIAN(tmp);			\
		y = PCM_READ_##SIGN##FMTBIT##_##ENDIAN(to);			\
		z = (VCHAN_INTCAST)x + y;					\
		x = PCM_CLAMP_##SIGN##FMTBIT(z);				\
		VCHAN_PCM_WRITE_##SIGN##FMTBIT##_##ENDIAN(to, x);		\
	} while (i != 0);							\
										\
	return (count);								\
}

FEEDER_VCHAN_MIX(8, int32_t, S, s, NE, ne)
FEEDER_VCHAN_MIX(16, int32_t, S, s, LE, le)
FEEDER_VCHAN_MIX(24, int32_t, S, s, LE, le)
FEEDER_VCHAN_MIX(32, intpcm_t, S, s, LE, le)
FEEDER_VCHAN_MIX(16, int32_t, S, s, BE, be)
FEEDER_VCHAN_MIX(24, int32_t, S, s, BE, be)
FEEDER_VCHAN_MIX(32, intpcm_t, S, s, BE, be)
FEEDER_VCHAN_MIX(8, int32_t, U, u, NE, ne)
FEEDER_VCHAN_MIX(16, int32_t, U, u, LE, le)
FEEDER_VCHAN_MIX(24, int32_t, U, u, LE, le)
FEEDER_VCHAN_MIX(32, intpcm_t, U, u, LE, le)
FEEDER_VCHAN_MIX(16, int32_t, U, u, BE, be)
FEEDER_VCHAN_MIX(24, int32_t, U, u, BE, be)
FEEDER_VCHAN_MIX(32, intpcm_t, U, u, BE, be)

struct feed_vchan_info {
	uint32_t format;
	int bps;
	feed_vchan_mixer mix;
};

static struct feed_vchan_info feed_vchan_info_tbl[] = {
	{ AFMT_S8,     PCM_8_BPS,  feed_vchan_mix_s8ne },
	{ AFMT_S16_LE, PCM_16_BPS, feed_vchan_mix_s16le },
	{ AFMT_S24_LE, PCM_24_BPS, feed_vchan_mix_s24le },
	{ AFMT_S32_LE, PCM_32_BPS, feed_vchan_mix_s32le },
	{ AFMT_S16_BE, PCM_16_BPS, feed_vchan_mix_s16be },
	{ AFMT_S24_BE, PCM_24_BPS, feed_vchan_mix_s24be },
	{ AFMT_S32_BE, PCM_32_BPS, feed_vchan_mix_s32be },
	{ AFMT_U8,     PCM_8_BPS,  feed_vchan_mix_u8ne  },
	{ AFMT_U16_LE, PCM_16_BPS, feed_vchan_mix_u16le },
	{ AFMT_U24_LE, PCM_24_BPS, feed_vchan_mix_u24le },
	{ AFMT_U32_LE, PCM_32_BPS, feed_vchan_mix_u32le },
	{ AFMT_U16_BE, PCM_16_BPS, feed_vchan_mix_u16be },
	{ AFMT_U24_BE, PCM_24_BPS, feed_vchan_mix_u24be },
	{ AFMT_U32_BE, PCM_32_BPS, feed_vchan_mix_u32be },
};

#define FVCHAN_DATA(i, c)	((intptr_t)((((i) & 0x1f) << 4) | ((c) & 0xf)))
#define FVCHAN_INFOIDX(m)	(((m) >> 4) & 0x1f)
#define FVCHAN_CHANNELS(m)	((m) & 0xf)

static int
feed_vchan_init(struct pcm_feeder *f)
{
	int i, channels;

	if (f->desc->out != f->desc->in)
		return (EINVAL);

	channels = (f->desc->out & AFMT_STEREO) ? 2 : 1;

	for (i = 0; i < sizeof(feed_vchan_info_tbl) /
	    sizeof(feed_vchan_info_tbl[0]); i++) {
		if ((f->desc->out & ~AFMT_STEREO) ==
		    feed_vchan_info_tbl[i].format) {
		    	f->data = (void *)FVCHAN_DATA(i, channels);
			return (0);
		}
	}

	return (-1);
}

static __inline int
feed_vchan_rec(struct pcm_channel *c)
{
	struct pcm_channel *ch;
	struct snd_dbuf *b, *bs;
	int cnt, rdy;

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

	do {
		cnt = FEEDER_FEED(c->feeder->source, c, b->tmpbuf, cnt,
		    c->bufhard);
		if (cnt != 0) {
			sndbuf_acquire(b, b->tmpbuf, cnt);
			cnt = sndbuf_getfree(b);
		}
	} while (cnt != 0);

	/* Not enough data */
	if (b->rl < sndbuf_getbps(b)) {
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
		if (!(ch->flags & CHN_F_TRIGGERED)) {
			CHN_UNLOCK(ch);
			continue;
		}
		bs = ch->bufsoft;
		if (ch->flags & CHN_F_MAPPED)
			sndbuf_dispose(bs, NULL, sndbuf_getready(bs));
		cnt = sndbuf_getfree(bs);
		if (cnt < sndbuf_getbps(bs)) {
			CHN_UNLOCK(ch);
			continue;
		}
		do {
			cnt = FEEDER_FEED(ch->feeder, ch, bs->tmpbuf, cnt, b);
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

	/*
	 * Return 0 to bail out early from sndbuf_feed() loop.
	 * No need to increase feedcount counter since part of this
	 * feeder chains already include feed_root().
	 */
	return (0);
}

static int
feed_vchan(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
    uint32_t count, void *source)
{
	struct feed_vchan_info *info;
	struct snd_dbuf *src = source;
	struct pcm_channel *ch;
	uint32_t cnt, mcnt, rcnt, sz;
	uint8_t *tmp;

	if (c->direction == PCMDIR_REC)
		return (feed_vchan_rec(c));

	sz = sndbuf_getsize(src);
	if (sz < count)
		count = sz;

	info = &feed_vchan_info_tbl[FVCHAN_INFOIDX((intptr_t)f->data)];
	sz = info->bps * FVCHAN_CHANNELS((intptr_t)f->data);
	count -= count % sz;
	if (count < sz)
		return (0);

	/*
	 * we are going to use our source as a temporary buffer since it's
	 * got no other purpose.  we obtain our data by traversing the channel
	 * list of children and calling vchan_mix_* to mix count bytes from
	 * each into our destination buffer, b
	 */
	tmp = sndbuf_getbuf(src);
	rcnt = 0;
	mcnt = 0;

	CHN_FOREACH(ch, c, children.busy) {
		CHN_LOCK(ch);
		if (!(ch->flags & CHN_F_TRIGGERED)) {
			CHN_UNLOCK(ch);
			continue;
		}
		if ((ch->flags & CHN_F_MAPPED) && !(ch->flags & CHN_F_CLOSING))
			sndbuf_acquire(ch->bufsoft, NULL,
			    sndbuf_getfree(ch->bufsoft));
		if (rcnt == 0) {
			rcnt = FEEDER_FEED(ch->feeder, ch, b, count,
			    ch->bufsoft);
			rcnt -= rcnt % sz;
			mcnt = count - rcnt;
		} else {
			cnt = FEEDER_FEED(ch->feeder, ch, tmp, count,
			    ch->bufsoft);
			cnt -= cnt % sz;
			if (cnt != 0) {
				if (mcnt != 0) {
					memset(b + rcnt,
					    sndbuf_zerodata(f->desc->out),
					    mcnt);
					mcnt = 0;
				}
				cnt = info->mix(b, tmp, cnt);
				if (cnt > rcnt)
					rcnt = cnt;
			}
		}
		CHN_UNLOCK(ch);
	}

	if (++c->feedcount == 0)
		c->feedcount = 2;

	return (rcnt);
}

static struct pcm_feederdesc feeder_vchan_desc[] = {
	{FEEDER_MIXER, AFMT_S8, AFMT_S8, 0},
	{FEEDER_MIXER, AFMT_S16_LE, AFMT_S16_LE, 0},
	{FEEDER_MIXER, AFMT_S24_LE, AFMT_S24_LE, 0},
	{FEEDER_MIXER, AFMT_S32_LE, AFMT_S32_LE, 0},
	{FEEDER_MIXER, AFMT_S16_BE, AFMT_S16_BE, 0},
	{FEEDER_MIXER, AFMT_S24_BE, AFMT_S24_BE, 0},
	{FEEDER_MIXER, AFMT_S32_BE, AFMT_S32_BE, 0},
	{FEEDER_MIXER, AFMT_S8 | AFMT_STEREO, AFMT_S8 | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_S24_LE | AFMT_STEREO, AFMT_S24_LE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_S32_LE | AFMT_STEREO, AFMT_S32_LE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_S16_BE | AFMT_STEREO, AFMT_S16_BE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_S24_BE | AFMT_STEREO, AFMT_S24_BE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_S32_BE | AFMT_STEREO, AFMT_S32_BE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_U8, AFMT_U8, 0},
	{FEEDER_MIXER, AFMT_U16_LE, AFMT_U16_LE, 0},
	{FEEDER_MIXER, AFMT_U24_LE, AFMT_U24_LE, 0},
	{FEEDER_MIXER, AFMT_U32_LE, AFMT_U32_LE, 0},
	{FEEDER_MIXER, AFMT_U16_BE, AFMT_U16_BE, 0},
	{FEEDER_MIXER, AFMT_U24_BE, AFMT_U24_BE, 0},
	{FEEDER_MIXER, AFMT_U32_BE, AFMT_U32_BE, 0},
	{FEEDER_MIXER, AFMT_U8 | AFMT_STEREO, AFMT_U8 | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_U16_LE | AFMT_STEREO, AFMT_U16_LE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_U24_LE | AFMT_STEREO, AFMT_U24_LE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_U32_LE | AFMT_STEREO, AFMT_U32_LE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_U16_BE | AFMT_STEREO, AFMT_U16_BE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_U24_BE | AFMT_STEREO, AFMT_U24_BE | AFMT_STEREO, 0},
	{FEEDER_MIXER, AFMT_U32_BE | AFMT_STEREO, AFMT_U32_BE | AFMT_STEREO, 0},
	{0, 0, 0, 0},
};
static kobj_method_t feeder_vchan_methods[] = {
	KOBJMETHOD(feeder_init,		feed_vchan_init),
	KOBJMETHOD(feeder_feed,		feed_vchan),
	{0, 0}
};
FEEDER_DECLARE(feeder_vchan, 2, NULL);

/************************************************************/

static void *
vchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
    struct pcm_channel *c, int dir)
{
	struct vchinfo *ch;

	KASSERT(dir == PCMDIR_PLAY || dir == PCMDIR_REC,
	    ("vchan_init: bad direction"));
	KASSERT(c != NULL && c->parentchannel != NULL,
	    ("vchan_init: bad channels"));

	ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
	ch->channel = c;
	ch->trigger = PCMTRIG_STOP;

	c->flags |= CHN_F_VIRTUAL;

	return (ch);
}

static int
vchan_free(kobj_t obj, void *data)
{
	free(data, M_DEVBUF);

	return (0);
}

static int
vchan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct vchinfo *ch = data;

	if (fmtvalid(format, ch->fmtlist) == 0)
		return (-1);

	return (0);
}

static int
vchan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct vchinfo *ch = data;
	struct pcm_channel *p = ch->channel->parentchannel;

	return (sndbuf_getspd(p->bufsoft));
}

static int
vchan_trigger(kobj_t obj, void *data, int go)
{
	struct vchinfo *ch = data;
	struct pcm_channel *c, *p;
	int err, otrigger;

	if (!PCMTRIG_COMMON(go) || go == ch->trigger)
		return (0);

	c = ch->channel;
	p = c->parentchannel;
	otrigger = ch->trigger;
	ch->trigger = go;

	CHN_UNLOCK(c);
	CHN_LOCK(p);

	switch (go) {
	case PCMTRIG_START:
		if (otrigger != PCMTRIG_START) {
			CHN_INSERT_HEAD(p, c, children.busy);
		}
		break;
	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		if (otrigger == PCMTRIG_START) {
			CHN_REMOVE(p, c, children.busy);
		}
		break;
	default:
		break;
	}

	err = chn_notify(p, CHN_N_TRIGGER);
	CHN_UNLOCK(p);
	CHN_LOCK(c);

	return (err);
}

static struct pcmchan_caps *
vchan_getcaps(kobj_t obj, void *data)
{
	struct vchinfo *ch = data;
	struct pcm_channel *c, *p;
	uint32_t fmt;

	c = ch->channel;
	p = c->parentchannel;
	ch->caps.minspeed = sndbuf_getspd(p->bufsoft);
	ch->caps.maxspeed = ch->caps.minspeed;
	ch->caps.caps = 0;
	ch->fmtlist[1] = 0;
	fmt = sndbuf_getfmt(p->bufsoft);
	if (fmt != vchan_valid_format(fmt)) {
		device_printf(c->dev,
			    "%s: WARNING: invalid vchan format! (0x%08x)\n",
			    __func__, fmt);
		fmt = VCHAN_DEFAULT_AFMT;
	}
	ch->fmtlist[0] = fmt;
	ch->caps.fmtlist = ch->fmtlist;

	return (&ch->caps);
}

static kobj_method_t vchan_methods[] = {
	KOBJMETHOD(channel_init,		vchan_init),
	KOBJMETHOD(channel_free,		vchan_free),
	KOBJMETHOD(channel_setformat,		vchan_setformat),
	KOBJMETHOD(channel_setspeed,		vchan_setspeed),
	KOBJMETHOD(channel_trigger,		vchan_trigger),
	KOBJMETHOD(channel_getcaps,		vchan_getcaps),
	{0, 0}
};
CHANNEL_DECLARE(vchan);

/* 
 * On the fly vchan rate settings
 */
#ifdef SND_DYNSYSCTL
static int
sysctl_hw_snd_vchanrate(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	struct pcm_channel *c, *ch = NULL;
	struct pcmchan_caps *caps;
	int *vchanrate, vchancount, direction, err, newspd;

	d = devclass_get_softc(pcm_devclass, VCHAN_SYSCTL_UNIT(oidp->oid_arg1));
	if (!PCM_REGISTERED(d) || !(d->flags & SD_F_AUTOVCHAN))
		return (EINVAL);

	pcm_lock(d);
	PCM_WAIT(d);

	switch (VCHAN_SYSCTL_DIR(oidp->oid_arg1)) {
	case VCHAN_PLAY:
		direction = PCMDIR_PLAY;
		vchancount = d->pvchancount;
		vchanrate = &d->pvchanrate;
		break;
	case VCHAN_REC:
		direction = PCMDIR_REC;
		vchancount = d->rvchancount;
		vchanrate = &d->rvchanrate;
		break;
	default:
		pcm_unlock(d);
		return (EINVAL);
		break;
	}

	if (vchancount < 1) {
		pcm_unlock(d);
		return (EINVAL);
	}

	PCM_ACQUIRE(d);
	pcm_unlock(d);

	newspd = 0;

	CHN_FOREACH(c, d, channels.pcm) {
		CHN_LOCK(c);
		if (c->direction == direction) {
			if (c->flags & CHN_F_VIRTUAL) {
				/* Sanity check */
				if (ch != NULL && ch != c->parentchannel) {
					CHN_UNLOCK(c);
					PCM_RELEASE_QUICK(d);
					return (EINVAL);
				}
			} else if (c->flags & CHN_F_HAS_VCHAN) {
				/* No way!! */
				if (ch != NULL) {
					CHN_UNLOCK(c);
					PCM_RELEASE_QUICK(d);
					return (EINVAL);
				}
				ch = c;
				newspd = ch->speed;
			}
		}
		CHN_UNLOCK(c);
	}
	if (ch == NULL) {
		PCM_RELEASE_QUICK(d);
		return (EINVAL);
	}

	err = sysctl_handle_int(oidp, &newspd, 0, req);
	if (err == 0 && req->newptr != NULL) {
		if (newspd < 1 || newspd < feeder_rate_min ||
		    newspd > feeder_rate_max) {
			PCM_RELEASE_QUICK(d);
			return (EINVAL);
		}
		CHN_LOCK(ch);
		if (feeder_rate_round) {
			caps = chn_getcaps(ch);
			if (caps == NULL || newspd < caps->minspeed ||
			    newspd > caps->maxspeed) {
				CHN_UNLOCK(ch);
				PCM_RELEASE_QUICK(d);
				return (EINVAL);
			}
		}
		if (CHN_STOPPED(ch) && newspd != ch->speed) {
			err = chn_setspeed(ch, newspd);
			/*
			 * Try to avoid FEEDER_RATE on parent channel if the
			 * requested value is not supported by the hardware.
			 */
			if (!err && feeder_rate_round &&
			    (ch->feederflags & (1 << FEEDER_RATE))) {
				newspd = sndbuf_getspd(ch->bufhard);
				err = chn_setspeed(ch, newspd);
			}
			if (err == 0)
				*vchanrate = newspd;
		}
		CHN_UNLOCK(ch);
	}

	PCM_RELEASE_QUICK(d);

	return (err);
}

static int
sysctl_hw_snd_vchanformat(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	struct pcm_channel *c, *ch = NULL;
	uint32_t newfmt, spd;
	int *vchanformat, vchancount, direction, err, i;
	char fmtstr[AFMTSTR_MAXSZ];

	d = devclass_get_softc(pcm_devclass, VCHAN_SYSCTL_UNIT(oidp->oid_arg1));
	if (!PCM_REGISTERED(d) || !(d->flags & SD_F_AUTOVCHAN))
		return (EINVAL);

	pcm_lock(d);
	PCM_WAIT(d);

	switch (VCHAN_SYSCTL_DIR(oidp->oid_arg1)) {
	case VCHAN_PLAY:
		direction = PCMDIR_PLAY;
		vchancount = d->pvchancount;
		vchanformat = &d->pvchanformat;
		break;
	case VCHAN_REC:
		direction = PCMDIR_REC;
		vchancount = d->rvchancount;
		vchanformat = &d->rvchanformat;
		break;
	default:
		pcm_unlock(d);
		return (EINVAL);
		break;
	}

	if (vchancount < 1) {
		pcm_unlock(d);
		return (EINVAL);
	}

	PCM_ACQUIRE(d);
	pcm_unlock(d);

	CHN_FOREACH(c, d, channels.pcm) {
		CHN_LOCK(c);
		if (c->direction == direction) {
			if (c->flags & CHN_F_VIRTUAL) {
				/* Sanity check */
				if (ch != NULL && ch != c->parentchannel) {
					CHN_UNLOCK(c);
					PCM_RELEASE_QUICK(d);
					return (EINVAL);
				}
			} else if (c->flags & CHN_F_HAS_VCHAN) {
				/* No way!! */
				if (ch != NULL) {
					CHN_UNLOCK(c);
					PCM_RELEASE_QUICK(d);
					return (EINVAL);
				}
				ch = c;
				if (ch->format !=
				    afmt2afmtstr(vchan_supported_fmts,
				    ch->format, fmtstr, sizeof(fmtstr),
				    AFMTSTR_FULL, AFMTSTR_STEREO_RETURN)) {
					strlcpy(fmtstr, VCHAN_DEFAULT_STRFMT,
					    sizeof(fmtstr));
				}
			}
		}
		CHN_UNLOCK(c);
	}
	if (ch == NULL) {
		PCM_RELEASE_QUICK(d);
		return (EINVAL);
	}

	err = sysctl_handle_string(oidp, fmtstr, sizeof(fmtstr), req);
	if (err == 0 && req->newptr != NULL) {
		for (i = 0; vchan_fmtstralias[i].alias != NULL; i++) {
			if (strcmp(fmtstr, vchan_fmtstralias[i].alias) == 0) {
				strlcpy(fmtstr, vchan_fmtstralias[i].fmtstr,
				    sizeof(fmtstr));
				break;
			}
		}
		newfmt = vchan_valid_strformat(fmtstr);
		if (newfmt == 0) {
			PCM_RELEASE_QUICK(d);
			return (EINVAL);
		}
		CHN_LOCK(ch);
		if (CHN_STOPPED(ch) && newfmt != ch->format) {
			/* Get channel speed, before chn_reset() screw it. */
			spd = ch->speed;
			err = chn_reset(ch, newfmt);
			if (err == 0)
				err = chn_setspeed(ch, spd);
			if (err == 0)
				*vchanformat = newfmt;
		}
		CHN_UNLOCK(ch);
	}

	PCM_RELEASE_QUICK(d);

	return (err);
}
#endif

/* virtual channel interface */

#define VCHAN_FMT_HINT(x)	((x) == PCMDIR_PLAY_VIRTUAL) ?		\
				"play.vchanformat" : "rec.vchanformat"
#define VCHAN_SPD_HINT(x)	((x) == PCMDIR_PLAY_VIRTUAL) ?		\
				"play.vchanrate" : "rec.vchanrate"

int
vchan_create(struct pcm_channel *parent, int num)
{
	struct snddev_info *d = parent->parentsnddev;
	struct pcm_channel *ch, *tmp, *after;
	struct pcmchan_caps *parent_caps;
	uint32_t vchanfmt;
	int err, first, speed, r;
	int direction;

	PCM_BUSYASSERT(d);

	if (!(parent->flags & CHN_F_BUSY))
		return (EBUSY);

	if (parent->direction == PCMDIR_PLAY) {
		direction = PCMDIR_PLAY_VIRTUAL;
		vchanfmt = d->pvchanformat;
		speed = d->pvchanrate;
	} else if (parent->direction == PCMDIR_REC) {
		direction = PCMDIR_REC_VIRTUAL;
		vchanfmt = d->rvchanformat;
		speed = d->rvchanrate;
	} else
		return (EINVAL);
	CHN_UNLOCK(parent);

	/* create a new playback channel */
	pcm_lock(d);
	ch = pcm_chn_create(d, parent, &vchan_class, direction, num, parent);
	if (ch == NULL) {
		pcm_unlock(d);
		CHN_LOCK(parent);
		return (ENODEV);
	}

	/* add us to our grandparent's channel list */
	err = pcm_chn_add(d, ch);
	pcm_unlock(d);
	if (err) {
		pcm_chn_destroy(ch);
		CHN_LOCK(parent);
		return (err);
	}

	CHN_LOCK(parent);
	/* add us to our parent channel's children */
	first = CHN_EMPTY(parent, children);
	after = NULL;
	CHN_FOREACH(tmp, parent, children) {
		if (CHN_CHAN(tmp) > CHN_CHAN(ch))
			after = tmp;
		else if (CHN_CHAN(tmp) < CHN_CHAN(ch))
			break;
	}
	if (after != NULL) {
		CHN_INSERT_AFTER(after, ch, children);
	} else {
		CHN_INSERT_HEAD(parent, ch, children);
	}
	parent->flags |= CHN_F_HAS_VCHAN;

	if (first) {
		parent_caps = chn_getcaps(parent);
		if (parent_caps == NULL)
			err = EINVAL;

		if (!err) {
			if (vchanfmt == 0) {
				const char *vfmt;

				CHN_UNLOCK(parent);
				r = resource_string_value(
				    device_get_name(parent->dev),
				    device_get_unit(parent->dev),
				    VCHAN_FMT_HINT(direction),
				    &vfmt);
				CHN_LOCK(parent);
				if (r != 0)
					vfmt = NULL;
				if (vfmt != NULL) {
					vchanfmt = vchan_valid_strformat(vfmt);
					for (r = 0; vchanfmt == 0 &&
					    vchan_fmtstralias[r].alias != NULL;
					    r++) {
						if (strcmp(vfmt, vchan_fmtstralias[r].alias) == 0) {
							vchanfmt = vchan_valid_strformat(vchan_fmtstralias[r].fmtstr);
							break;
						}
					}
				}
				if (vchanfmt == 0)
					vchanfmt = VCHAN_DEFAULT_AFMT;
			}
			err = chn_reset(parent, vchanfmt);
		}

		if (!err) {
			/*
			 * This is very sad. Few soundcards advertised as being
			 * able to do (insanely) higher/lower speed, but in
			 * reality, they simply can't. At least, we give user chance
			 * to set sane value via kernel hints or sysctl.
			 */
			if (speed < 1) {
				CHN_UNLOCK(parent);
				r = resource_int_value(
				    device_get_name(parent->dev),
				    device_get_unit(parent->dev),
				    VCHAN_SPD_HINT(direction),
				    &speed);
				CHN_LOCK(parent);
				if (r != 0) {
					/*
					 * No saved value, no hint, NOTHING.
					 *
					 * Workaround for sb16 running
					 * poorly at 45k / 49k.
					 */
					switch (parent_caps->maxspeed) {
					case 45000:
					case 49000:
						speed = 44100;
						break;
					default:
						speed = VCHAN_DEFAULT_SPEED;
						if (speed > parent_caps->maxspeed)
							speed = parent_caps->maxspeed;
						break;
					}
					if (speed < parent_caps->minspeed)
						speed = parent_caps->minspeed;
				}
			}

			if (feeder_rate_round) {
				/*
				 * Limit speed based on driver caps.
				 * This is supposed to help fixed rate, non-VRA
				 * AC97 cards, but.. (see below)
				 */
				if (speed < parent_caps->minspeed)
					speed = parent_caps->minspeed;
				if (speed > parent_caps->maxspeed)
					speed = parent_caps->maxspeed;
			}

			/*
			 * We still need to limit the speed between
			 * feeder_rate_min <-> feeder_rate_max. This is
			 * just an escape goat if all of the above failed
			 * miserably.
			 */
			if (speed < feeder_rate_min)
				speed = feeder_rate_min;
			if (speed > feeder_rate_max)
				speed = feeder_rate_max;

			err = chn_setspeed(parent, speed);
			/*
			 * Try to avoid FEEDER_RATE on parent channel if the
			 * requested value is not supported by the hardware.
			 */
			if (!err && feeder_rate_round &&
			    (parent->feederflags & (1 << FEEDER_RATE))) {
				speed = sndbuf_getspd(parent->bufhard);
				err = chn_setspeed(parent, speed);
			}

			if (!err) {
				/*
				 * Save new value.
				 */
				CHN_UNLOCK(parent);
				if (direction == PCMDIR_PLAY_VIRTUAL) {
					d->pvchanformat = vchanfmt;
					d->pvchanrate = speed;
				} else {
					d->rvchanformat = vchanfmt;
					d->rvchanrate = speed;
				}
				CHN_LOCK(parent);
			}
		}
		
		if (err) {
			CHN_REMOVE(parent, ch, children);
			parent->flags &= ~CHN_F_HAS_VCHAN;
			CHN_UNLOCK(parent);
			pcm_lock(d);
			if (pcm_chn_remove(d, ch) == 0) {
				pcm_unlock(d);
				pcm_chn_destroy(ch);
			} else
				pcm_unlock(d);
			CHN_LOCK(parent);
			return (err);
		}
	}

	return (0);
}

int
vchan_destroy(struct pcm_channel *c)
{
	struct pcm_channel *parent = c->parentchannel;
	struct snddev_info *d = parent->parentsnddev;
	uint32_t spd;
	int err;

	PCM_BUSYASSERT(d);

	CHN_LOCK(parent);
	if (!(parent->flags & CHN_F_BUSY)) {
		CHN_UNLOCK(parent);
		return (EBUSY);
	}
	if (CHN_EMPTY(parent, children)) {
		CHN_UNLOCK(parent);
		return (EINVAL);
	}

	/* remove us from our parent's children list */
	CHN_REMOVE(parent, c, children);

	if (CHN_EMPTY(parent, children)) {
		parent->flags &= ~(CHN_F_BUSY | CHN_F_HAS_VCHAN);
		spd = parent->speed;
		if (chn_reset(parent, parent->format) == 0)
			chn_setspeed(parent, spd);
	}

	CHN_UNLOCK(parent);

	/* remove us from our grandparent's channel list */
	pcm_lock(d);
	err = pcm_chn_remove(d, c);
	pcm_unlock(d);

	/* destroy ourselves */
	if (!err)
		err = pcm_chn_destroy(c);

	return (err);
}

int
vchan_initsys(device_t dev)
{
#ifdef SND_DYNSYSCTL
	struct snddev_info *d;
	int unit;

	unit = device_get_unit(dev);
	d = device_get_softc(dev);

	/* Play */
	SYSCTL_ADD_PROC(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(d->play_sysctl_tree),
	    OID_AUTO, "vchans", CTLTYPE_INT | CTLFLAG_RW,
	    VCHAN_SYSCTL_DATA(unit, PLAY), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_hw_snd_vchans, "I", "total allocated virtual channel");
	SYSCTL_ADD_PROC(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(d->play_sysctl_tree),
	    OID_AUTO, "vchanrate", CTLTYPE_INT | CTLFLAG_RW,
	    VCHAN_SYSCTL_DATA(unit, PLAY), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_hw_snd_vchanrate, "I", "virtual channel mixing speed/rate");
	SYSCTL_ADD_PROC(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(d->play_sysctl_tree),
	    OID_AUTO, "vchanformat", CTLTYPE_STRING | CTLFLAG_RW,
	    VCHAN_SYSCTL_DATA(unit, PLAY), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_hw_snd_vchanformat, "A", "virtual channel format");
	/* Rec */
	SYSCTL_ADD_PROC(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(d->rec_sysctl_tree),
	    OID_AUTO, "vchans", CTLTYPE_INT | CTLFLAG_RW,
	    VCHAN_SYSCTL_DATA(unit, REC), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_hw_snd_vchans, "I", "total allocated virtual channel");
	SYSCTL_ADD_PROC(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(d->rec_sysctl_tree),
	    OID_AUTO, "vchanrate", CTLTYPE_INT | CTLFLAG_RW,
	    VCHAN_SYSCTL_DATA(unit, REC), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_hw_snd_vchanrate, "I", "virtual channel base speed/rate");
	SYSCTL_ADD_PROC(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(d->rec_sysctl_tree),
	    OID_AUTO, "vchanformat", CTLTYPE_STRING | CTLFLAG_RW,
	    VCHAN_SYSCTL_DATA(unit, REC), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_hw_snd_vchanformat, "A", "virtual channel format");
#endif

	return (0);
}
