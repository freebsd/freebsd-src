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
 *
 * Almost entirely rewritten to add multi-format/channels mixing support.
 *
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

MALLOC_DEFINE(M_VCHANFEEDER, "vchanfeed", "pcm vchan feeder");

/*
 * Default speed / format
 */
#define VCHAN_DEFAULT_SPEED	48000
#define VCHAN_DEFAULT_AFMT	(AFMT_S16_LE | AFMT_STEREO)
#define VCHAN_DEFAULT_STRFMT	"s16le"

typedef uint32_t (*feed_vchan_mixer)(uint8_t *, uint8_t *, uint32_t);

struct vchinfo {
	uint32_t spd, fmt, fmts[2], blksz, bps, run;
	struct pcm_channel *channel, *parent;
	struct pcmchan_caps caps;
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
	return count;								\
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
		return EINVAL;

	channels = (f->desc->out & AFMT_STEREO) ? 2 : 1;

	for (i = 0; i < sizeof(feed_vchan_info_tbl) /
	    sizeof(feed_vchan_info_tbl[0]); i++) {
		if ((f->desc->out & ~AFMT_STEREO) ==
		    feed_vchan_info_tbl[i].format) {
		    	f->data = (void *)FVCHAN_DATA(i, channels);
			return 0;
		}
	}

	return -1;
}

static int
feed_vchan(struct pcm_feeder *f, struct pcm_channel *c, uint8_t *b,
						uint32_t count, void *source)
{
	struct feed_vchan_info *info;
	struct snd_dbuf *src = source;
	struct pcmchan_children *cce;
	struct pcm_channel *ch;
	uint32_t cnt, mcnt, rcnt, sz;
	uint8_t *tmp;

	sz = sndbuf_getsize(src);
	if (sz < count)
		count = sz;

	info = &feed_vchan_info_tbl[FVCHAN_INFOIDX((intptr_t)f->data)];
	sz = info->bps * FVCHAN_CHANNELS((intptr_t)f->data);
	count -= count % sz;
	if (count < sz)
		return 0;

	/*
	 * we are going to use our source as a temporary buffer since it's
	 * got no other purpose.  we obtain our data by traversing the channel
	 * list of children and calling vchan_mix_* to mix count bytes from
	 * each into our destination buffer, b
	 */
	tmp = sndbuf_getbuf(src);
	rcnt = 0;
	mcnt = 0;

	SLIST_FOREACH(cce, &c->children, link) {
		ch = cce->channel;
		CHN_LOCK(ch);
		if (!(ch->flags & CHN_F_TRIGGERED)) {
			CHN_UNLOCK(ch);
			continue;
		}
		if ((ch->flags & CHN_F_MAPPED) && !(ch->flags & CHN_F_CLOSING))
			sndbuf_acquire(ch->bufsoft, NULL,
			    sndbuf_getfree(ch->bufsoft));
		if (rcnt == 0)
			rcnt = FEEDER_FEED(ch->feeder, ch, b, count,
			ch->bufsoft);
		else {
			cnt = FEEDER_FEED(ch->feeder, ch, tmp, count,
			    ch->bufsoft);
			cnt -= cnt % sz;
			if (cnt != 0) {
				if (mcnt++ == 0 && rcnt < count)
					memset(b + rcnt,
					    sndbuf_zerodata(f->desc->out),
					    count - rcnt);
				cnt = info->mix(b, tmp, cnt);
				if (cnt > rcnt)
					rcnt = cnt;
			}
		}
		CHN_UNLOCK(ch);
	}

	if (++c->feedcount == 0)
		c->feedcount = 2;

	return rcnt;
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
vchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct vchinfo *ch;
	struct pcm_channel *parent = devinfo;

	KASSERT(dir == PCMDIR_PLAY, ("vchan_init: bad direction"));
	ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
	ch->parent = parent;
	ch->channel = c;
	ch->fmt = AFMT_U8;
	ch->spd = DSP_DEFAULT_SPEED;
	ch->blksz = 2048;

	c->flags |= CHN_F_VIRTUAL;

	return ch;
}

static int
vchan_free(kobj_t obj, void *data)
{
	free(data, M_DEVBUF);
	return 0;
}

static int
vchan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct vchinfo *ch = data;
	struct pcm_channel *parent = ch->parent;
	struct pcm_channel *channel = ch->channel;

	ch->fmt = format;
	ch->bps = 1;
	ch->bps <<= (ch->fmt & AFMT_STEREO)? 1 : 0;
	if (ch->fmt & AFMT_16BIT)
		ch->bps <<= 1;
	else if (ch->fmt & AFMT_24BIT)
		ch->bps *= 3;
	else if (ch->fmt & AFMT_32BIT)
		ch->bps <<= 2;
	CHN_UNLOCK(channel);
	chn_notify(parent, CHN_N_FORMAT);
	CHN_LOCK(channel);
	sndbuf_setfmt(channel->bufsoft, format);
	return 0;
}

static int
vchan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct vchinfo *ch = data;
	struct pcm_channel *parent = ch->parent;
	struct pcm_channel *channel = ch->channel;

	ch->spd = speed;
	CHN_UNLOCK(channel);
	CHN_LOCK(parent);
	speed = sndbuf_getspd(parent->bufsoft);
	CHN_UNLOCK(parent);
	CHN_LOCK(channel);
	return speed;
}

static int
vchan_setblocksize(kobj_t obj, void *data, uint32_t blocksize)
{
	struct vchinfo *ch = data;
	struct pcm_channel *channel = ch->channel;
	struct pcm_channel *parent = ch->parent;
	/* struct pcm_channel *channel = ch->channel; */
	int prate, crate;

	ch->blksz = blocksize;
	/* CHN_UNLOCK(channel); */
	sndbuf_setblksz(channel->bufhard, blocksize);
	chn_notify(parent, CHN_N_BLOCKSIZE);
	CHN_LOCK(parent);
	/* CHN_LOCK(channel); */

	crate = ch->spd * ch->bps;
	prate = sndbuf_getspd(parent->bufsoft) * sndbuf_getbps(parent->bufsoft);
	blocksize = sndbuf_getblksz(parent->bufsoft);
	CHN_UNLOCK(parent);
	blocksize *= prate;
	blocksize /= crate;
	blocksize += ch->bps;
	prate = 0;
	while (blocksize >> prate)
		prate++;
	blocksize = 1 << (prate - 1);
	blocksize -= blocksize % ch->bps;
	/* XXX screwed !@#$ */
	if (blocksize < ch->bps)
		blocksize = 4096 - (4096 % ch->bps);

	return blocksize;
}

static int
vchan_trigger(kobj_t obj, void *data, int go)
{
	struct vchinfo *ch = data;
	struct pcm_channel *parent = ch->parent;
	struct pcm_channel *channel = ch->channel;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	ch->run = (go == PCMTRIG_START)? 1 : 0;
	CHN_UNLOCK(channel);
	chn_notify(parent, CHN_N_TRIGGER);
	CHN_LOCK(channel);

	return 0;
}

static struct pcmchan_caps *
vchan_getcaps(kobj_t obj, void *data)
{
	struct vchinfo *ch = data;
	uint32_t fmt;

	ch->caps.minspeed = sndbuf_getspd(ch->parent->bufsoft);
	ch->caps.maxspeed = ch->caps.minspeed;
	ch->caps.caps = 0;
	ch->fmts[1] = 0;
	fmt = sndbuf_getfmt(ch->parent->bufsoft);
	if (fmt != vchan_valid_format(fmt)) {
		device_printf(ch->parent->dev,
			    "%s: WARNING: invalid vchan format! (0x%08x)\n",
			    __func__, fmt);
		fmt = VCHAN_DEFAULT_AFMT;
	}
	ch->fmts[0] = fmt;
	ch->caps.fmtlist = ch->fmts;

	return &ch->caps;
}

static kobj_method_t vchan_methods[] = {
	KOBJMETHOD(channel_init,		vchan_init),
	KOBJMETHOD(channel_free,		vchan_free),
	KOBJMETHOD(channel_setformat,		vchan_setformat),
	KOBJMETHOD(channel_setspeed,		vchan_setspeed),
	KOBJMETHOD(channel_setblocksize,	vchan_setblocksize),
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
	struct snddev_channel *sce;
	struct pcm_channel *c, *ch = NULL, *fake;
	struct pcmchan_caps *caps;
	int err = 0;
	int newspd = 0;

	d = oidp->oid_arg1;
	if (!(d->flags & SD_F_AUTOVCHAN) || d->vchancount < 1)
		return EINVAL;
	if (pcm_inprog(d, 1) != 1 && req->newptr != NULL) {
		pcm_inprog(d, -1);
		return EINPROGRESS;
	}
	SLIST_FOREACH(sce, &d->channels, link) {
		c = sce->channel;
		CHN_LOCK(c);
		if (c->direction == PCMDIR_PLAY) {
			if (c->flags & CHN_F_VIRTUAL) {
				/* Sanity check */
				if (ch != NULL && ch != c->parentchannel) {
					CHN_UNLOCK(c);
					pcm_inprog(d, -1);
					return EINVAL;
				}
				if (req->newptr != NULL &&
						(c->flags & CHN_F_BUSY)) {
					CHN_UNLOCK(c);
					pcm_inprog(d, -1);
					return EBUSY;
				}
			} else if (c->flags & CHN_F_HAS_VCHAN) {
				/* No way!! */
				if (ch != NULL) {
					CHN_UNLOCK(c);
					pcm_inprog(d, -1);
					return EINVAL;
				}
				ch = c;
				newspd = ch->speed;
			}
		}
		CHN_UNLOCK(c);
	}
	if (ch == NULL) {
		pcm_inprog(d, -1);
		return EINVAL;
	}
	err = sysctl_handle_int(oidp, &newspd, sizeof(newspd), req);
	if (err == 0 && req->newptr != NULL) {
		if (newspd < 1 || newspd < feeder_rate_min ||
				newspd > feeder_rate_max) {
			pcm_inprog(d, -1);
			return EINVAL;
		}
		CHN_LOCK(ch);
		if (feeder_rate_round) {
			caps = chn_getcaps(ch);
			if (caps == NULL || newspd < caps->minspeed ||
					newspd > caps->maxspeed) {
				CHN_UNLOCK(ch);
				pcm_inprog(d, -1);
				return EINVAL;
			}
		}
		if (newspd != ch->speed) {
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
			CHN_UNLOCK(ch);
			if (err == 0) {
				fake = pcm_getfakechan(d);
				if (fake != NULL) {
					CHN_LOCK(fake);
					fake->speed = newspd;
					CHN_UNLOCK(fake);
				}
			}
		} else
			CHN_UNLOCK(ch);
	}
	pcm_inprog(d, -1);
	return err;
}

static int
sysctl_hw_snd_vchanformat(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	struct snddev_channel *sce;
	struct pcm_channel *c, *ch = NULL, *fake;
	uint32_t newfmt, spd;
	char fmtstr[AFMTSTR_MAXSZ];
	int err = 0, i;

	d = oidp->oid_arg1;
	if (!(d->flags & SD_F_AUTOVCHAN) || d->vchancount < 1)
		return EINVAL;
	if (pcm_inprog(d, 1) != 1 && req->newptr != NULL) {
		pcm_inprog(d, -1);
		return EINPROGRESS;
	}
	SLIST_FOREACH(sce, &d->channels, link) {
		c = sce->channel;
		CHN_LOCK(c);
		if (c->direction == PCMDIR_PLAY) {
			if (c->flags & CHN_F_VIRTUAL) {
				/* Sanity check */
				if (ch != NULL && ch != c->parentchannel) {
					CHN_UNLOCK(c);
					pcm_inprog(d, -1);
					return EINVAL;
				}
				if (req->newptr != NULL &&
						(c->flags & CHN_F_BUSY)) {
					CHN_UNLOCK(c);
					pcm_inprog(d, -1);
					return EBUSY;
				}
			} else if (c->flags & CHN_F_HAS_VCHAN) {
				/* No way!! */
				if (ch != NULL) {
					CHN_UNLOCK(c);
					pcm_inprog(d, -1);
					return EINVAL;
				}
				ch = c;
				if (ch->format != afmt2afmtstr(vchan_supported_fmts,
					    ch->format, fmtstr, sizeof(fmtstr),
					    AFMTSTR_FULL, AFMTSTR_STEREO_RETURN)) {
					strlcpy(fmtstr, VCHAN_DEFAULT_STRFMT, sizeof(fmtstr));
				}
			}
		}
		CHN_UNLOCK(c);
	}
	if (ch == NULL) {
		pcm_inprog(d, -1);
		return EINVAL;
	}
	err = sysctl_handle_string(oidp, fmtstr, sizeof(fmtstr), req);
	if (err == 0 && req->newptr != NULL) {
		for (i = 0; vchan_fmtstralias[i].alias != NULL; i++) {
			if (strcmp(fmtstr, vchan_fmtstralias[i].alias) == 0) {
				strlcpy(fmtstr, vchan_fmtstralias[i].fmtstr, sizeof(fmtstr));
				break;
			}
		}
		newfmt = vchan_valid_strformat(fmtstr);
		if (newfmt == 0) {
			pcm_inprog(d, -1);
			return EINVAL;
		}
		CHN_LOCK(ch);
		if (newfmt != ch->format) {
			/* Get channel speed, before chn_reset() screw it. */
			spd = ch->speed;
			err = chn_reset(ch, newfmt);
			if (err == 0)
				err = chn_setspeed(ch, spd);
			CHN_UNLOCK(ch);
			if (err == 0) {
				fake = pcm_getfakechan(d);
				if (fake != NULL) {
					CHN_LOCK(fake);
					fake->format = newfmt;
					CHN_UNLOCK(fake);
				}
			}
		} else
			CHN_UNLOCK(ch);
	}
	pcm_inprog(d, -1);
	return err;
}
#endif

/* virtual channel interface */

int
vchan_create(struct pcm_channel *parent)
{
	struct snddev_info *d = parent->parentsnddev;
	struct pcmchan_children *pce;
	struct pcm_channel *child, *fake;
	struct pcmchan_caps *parent_caps;
	uint32_t vchanfmt = 0;
	int err, first, speed = 0, r;

	if (!(parent->flags & CHN_F_BUSY))
		return EBUSY;


	CHN_UNLOCK(parent);

	pce = malloc(sizeof(*pce), M_DEVBUF, M_WAITOK | M_ZERO);

	/* create a new playback channel */
	child = pcm_chn_create(d, parent, &vchan_class, PCMDIR_VIRTUAL, parent);
	if (!child) {
		free(pce, M_DEVBUF);
		CHN_LOCK(parent);
		return ENODEV;
	}
	pce->channel = child;

	/* add us to our grandparent's channel list */
	/*
	 * XXX maybe we shouldn't always add the dev_t
 	 */
	err = pcm_chn_add(d, child);
	if (err) {
		pcm_chn_destroy(child);
		free(pce, M_DEVBUF);
		CHN_LOCK(parent);
		return err;
	}

	CHN_LOCK(parent);
	/* add us to our parent channel's children */
	first = SLIST_EMPTY(&parent->children);
	SLIST_INSERT_HEAD(&parent->children, pce, link);
	parent->flags |= CHN_F_HAS_VCHAN;

	if (first) {
		parent_caps = chn_getcaps(parent);
		if (parent_caps == NULL)
			err = EINVAL;

		fake = pcm_getfakechan(d);

		if (!err && fake != NULL) {
			/*
			 * Avoid querying kernel hint, use saved value
			 * from fake channel.
			 */
			CHN_UNLOCK(parent);
			CHN_LOCK(fake);
			speed = fake->speed;
			vchanfmt = fake->format;
			CHN_UNLOCK(fake);
			CHN_LOCK(parent);
		}

		if (!err) {
			if (vchanfmt == 0) {
				const char *vfmt;

				CHN_UNLOCK(parent);
				r = resource_string_value(device_get_name(parent->dev),
					device_get_unit(parent->dev),
					"vchanformat", &vfmt);
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
				r = resource_int_value(device_get_name(parent->dev),
							device_get_unit(parent->dev),
								"vchanrate", &speed);
				CHN_LOCK(parent);
				if (r != 0) {
					/*
					 * No saved value from fake channel,
					 * no hint, NOTHING.
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

			if (!err && fake != NULL) {
				/*
				 * Save new value to fake channel.
				 */
				CHN_UNLOCK(parent);
				CHN_LOCK(fake);
				fake->speed = speed;
				fake->format = vchanfmt;
				CHN_UNLOCK(fake);
				CHN_LOCK(parent);
			}
		}
		
		if (err) {
			SLIST_REMOVE(&parent->children, pce, pcmchan_children, link);
			parent->flags &= ~CHN_F_HAS_VCHAN;
			CHN_UNLOCK(parent);
			free(pce, M_DEVBUF);
			if (pcm_chn_remove(d, child) == 0)
				pcm_chn_destroy(child);
			CHN_LOCK(parent);
			return err;
		}
	}

	return 0;
}

int
vchan_destroy(struct pcm_channel *c)
{
	struct pcm_channel *parent = c->parentchannel;
	struct snddev_info *d = parent->parentsnddev;
	struct pcmchan_children *pce;
	struct snddev_channel *sce;
	uint32_t spd;
	int err;

	CHN_LOCK(parent);
	if (!(parent->flags & CHN_F_BUSY)) {
		CHN_UNLOCK(parent);
		return EBUSY;
	}
	if (SLIST_EMPTY(&parent->children)) {
		CHN_UNLOCK(parent);
		return EINVAL;
	}

	/* remove us from our parent's children list */
	SLIST_FOREACH(pce, &parent->children, link) {
		if (pce->channel == c)
			goto gotch;
	}
	CHN_UNLOCK(parent);
	return EINVAL;
gotch:
	SLIST_FOREACH(sce, &d->channels, link) {
		if (sce->channel == c) {
			if (sce->dsp_devt) {
				destroy_dev(sce->dsp_devt);
				sce->dsp_devt = NULL;
			}
			if (sce->dspW_devt) {
				destroy_dev(sce->dspW_devt);
				sce->dspW_devt = NULL;
			}
			if (sce->audio_devt) {
				destroy_dev(sce->audio_devt);
				sce->audio_devt = NULL;
			}
			if (sce->dspHW_devt) {
				destroy_dev(sce->dspHW_devt);
				sce->dspHW_devt = NULL;
			}
			d->devcount--;
			break;
		}
	}
	SLIST_REMOVE(&parent->children, pce, pcmchan_children, link);
	free(pce, M_DEVBUF);

	if (SLIST_EMPTY(&parent->children)) {
		parent->flags &= ~(CHN_F_BUSY | CHN_F_HAS_VCHAN);
		spd = parent->speed;
		if (chn_reset(parent, parent->format) == 0)
			chn_setspeed(parent, spd);
	}

	/* remove us from our grandparent's channel list */
	err = pcm_chn_remove(d, c);

	CHN_UNLOCK(parent);
	/* destroy ourselves */
	if (!err)
		err = pcm_chn_destroy(c);

	return err;
}

int
vchan_initsys(device_t dev)
{
#ifdef SND_DYNSYSCTL
	struct snddev_info *d;

	d = device_get_softc(dev);
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "vchans", CTLTYPE_INT | CTLFLAG_RW, d, sizeof(d),
	    sysctl_hw_snd_vchans, "I", "total allocated virtual channel");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "vchanrate", CTLTYPE_INT | CTLFLAG_RW, d, sizeof(d),
	    sysctl_hw_snd_vchanrate, "I", "virtual channel mixing speed/rate");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "vchanformat", CTLTYPE_STRING | CTLFLAG_RW, d, sizeof(d),
	    sysctl_hw_snd_vchanformat, "A", "virtual channel format");
#endif

	return 0;
}
