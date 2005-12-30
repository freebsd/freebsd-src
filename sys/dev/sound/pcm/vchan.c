/*-
 * Copyright (c) 2001 Cameron Grant <cg@freebsd.org>
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

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>
#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

/*
 * Default speed
 */
#define VCHAN_DEFAULT_SPEED	44100

extern int feeder_rate_ratemin;
extern int feeder_rate_ratemax;

struct vchinfo {
	u_int32_t spd, fmt, blksz, bps, run;
	struct pcm_channel *channel, *parent;
	struct pcmchan_caps caps;
};

static u_int32_t vchan_fmt[] = {
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static int
vchan_mix_s16(int16_t *to, int16_t *tmp, unsigned int count)
{
	/*
	 * to is the output buffer, tmp is the input buffer
	 * count is the number of 16bit samples to mix
	 */
	int i;
	int x;

	for(i = 0; i < count; i++) {
		x = to[i];
		x += tmp[i];
		if (x < -32768) {
			/* printf("%d + %d = %d (u)\n", to[i], tmp[i], x); */
			x = -32768;
		}
		if (x > 32767) {
			/* printf("%d + %d = %d (o)\n", to[i], tmp[i], x); */
			x = 32767;
		}
		to[i] = x & 0x0000ffff;
	}
	return 0;
}

static int
feed_vchan_s16(struct pcm_feeder *f, struct pcm_channel *c, u_int8_t *b, u_int32_t count, void *source)
{
	/* we're going to abuse things a bit */
	struct snd_dbuf *src = source;
	struct pcmchan_children *cce;
	struct pcm_channel *ch;
	uint32_t sz;
	int16_t *tmp, *dst;
	unsigned int cnt, rcnt = 0;

	#if 0
	if (sndbuf_getsize(src) < count)
		panic("feed_vchan_s16(%s): tmp buffer size %d < count %d, flags = 0x%x",
		    c->name, sndbuf_getsize(src), count, c->flags);
	#endif
	sz = sndbuf_getsize(src);
	if (sz < count)
		count = sz;
	count &= ~1;
	if (count < 2)
		return 0;
	bzero(b, count);

	/*
	 * we are going to use our source as a temporary buffer since it's
	 * got no other purpose.  we obtain our data by traversing the channel
	 * list of children and calling vchan_mix_* to mix count bytes from each
	 * into our destination buffer, b
	 */
	dst = (int16_t *)b;
	tmp = (int16_t *)sndbuf_getbuf(src);
	bzero(tmp, count);
	SLIST_FOREACH(cce, &c->children, link) {
		ch = cce->channel;
   		CHN_LOCK(ch);
		if (ch->flags & CHN_F_TRIGGERED) {
			if (ch->flags & CHN_F_MAPPED)
				sndbuf_acquire(ch->bufsoft, NULL, sndbuf_getfree(ch->bufsoft));
			cnt = FEEDER_FEED(ch->feeder, ch, (u_int8_t *)tmp, count, ch->bufsoft);
			vchan_mix_s16(dst, tmp, cnt >> 1);
			if (cnt > rcnt)
				rcnt = cnt;
		}
   		CHN_UNLOCK(ch);
	}

	return rcnt & ~1;
}

static struct pcm_feederdesc feeder_vchan_s16_desc[] = {
	{FEEDER_MIXER, AFMT_S16_LE | AFMT_STEREO, AFMT_S16_LE | AFMT_STEREO, 0},
	{0},
};
static kobj_method_t feeder_vchan_s16_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_vchan_s16),
	{ 0, 0 }
};
FEEDER_DECLARE(feeder_vchan_s16, 2, NULL);

/************************************************************/

static void *
vchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct vchinfo *ch;
	struct pcm_channel *parent = devinfo;

	KASSERT(dir == PCMDIR_PLAY, ("vchan_init: bad direction"));
	ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!ch)
		return NULL;
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
	return 0;
}

static int
vchan_setformat(kobj_t obj, void *data, u_int32_t format)
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
vchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
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
vchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
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

	ch->caps.minspeed = sndbuf_getspd(ch->parent->bufsoft);
	ch->caps.maxspeed = ch->caps.minspeed;
	ch->caps.fmtlist = vchan_fmt;
	ch->caps.caps = 0;

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
	{ 0, 0 }
};
CHANNEL_DECLARE(vchan);

#if 0
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
	SLIST_FOREACH(sce, &d->channels, link) {
		c = sce->channel;
		CHN_LOCK(c);
		if (c->direction == PCMDIR_PLAY) {
			if (c->flags & CHN_F_VIRTUAL) {
				if (req->newptr != NULL &&
						(c->flags & CHN_F_BUSY)) {
					CHN_UNLOCK(c);
					return EBUSY;
				}
				if (ch == NULL)
					ch = c->parentchannel;
			}
		}
		CHN_UNLOCK(c);
	}
	if (ch != NULL) {
		CHN_LOCK(ch);
		newspd = ch->speed;
		CHN_UNLOCK(ch);
	}
	err = sysctl_handle_int(oidp, &newspd, sizeof(newspd), req);
	if (err == 0 && req->newptr != NULL) {
		if (ch == NULL || newspd < 1 ||
				newspd < feeder_rate_ratemin ||
				newspd > feeder_rate_ratemax)
			return EINVAL;
		if (pcm_inprog(d, 1) != 1) {
			pcm_inprog(d, -1);
			return EINPROGRESS;
		}
		CHN_LOCK(ch);
		caps = chn_getcaps(ch);
		if (caps == NULL || newspd < caps->minspeed ||
				newspd > caps->maxspeed) {
			CHN_UNLOCK(ch);
			pcm_inprog(d, -1);
			return EINVAL;
		}
		if (newspd != ch->speed) {
			err = chn_setspeed(ch, newspd);
			/*
			 * Try to avoid FEEDER_RATE on parent channel if the
			 * requested value is not supported by the hardware.
			 */
			if (!err && (ch->feederflags & (1 << FEEDER_RATE))) {
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
		pcm_inprog(d, -1);
	}
	return err;
}
#endif
#endif

/* virtual channel interface */

int
vchan_create(struct pcm_channel *parent)
{
    	struct snddev_info *d = parent->parentsnddev;
	struct pcmchan_children *pce;
	struct pcm_channel *child, *fake;
	struct pcmchan_caps *parent_caps;
	int err, first, speed = 0;

	if (!(parent->flags & CHN_F_BUSY))
		return EBUSY;


	CHN_UNLOCK(parent);

	pce = malloc(sizeof(*pce), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!pce) {
   		CHN_LOCK(parent);
		return ENOMEM;
	}

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

		if (!err)
			err = chn_reset(parent, AFMT_STEREO | AFMT_S16_LE);

		if (!err) {
			fake = pcm_getfakechan(d);
			if (fake != NULL) {
				/*
				 * Avoid querying kernel hint, use saved value
				 * from fake channel.
				 */
				CHN_UNLOCK(parent);
				CHN_LOCK(fake);
				speed = fake->speed;
				CHN_UNLOCK(fake);
				CHN_LOCK(parent);
			}

			/*
			 * This is very sad. Few soundcards advertised as being
			 * able to do (insanely) higher/lower speed, but in
			 * reality, they simply can't. At least, we give user chance
			 * to set sane value via kernel hints or sysctl.
			 */
			if (speed < 1) {
				int r;
				CHN_UNLOCK(parent);
				r = resource_int_value(device_get_name(parent->dev),
							device_get_unit(parent->dev),
								"vchanrate", &speed);
				CHN_LOCK(parent);
				if (r != 0)
					speed = VCHAN_DEFAULT_SPEED;
			}

			/*
			 * Limit speed based on driver caps.
			 * This is supposed to help fixed rate, non-VRA
			 * AC97 cards, but.. (see below)
			 */
			if (speed < parent_caps->minspeed)
				speed = parent_caps->minspeed;
			if (speed > parent_caps->maxspeed)
				speed = parent_caps->maxspeed;

			/*
			 * We still need to limit the speed between
			 * feeder_rate_ratemin <-> feeder_rate_ratemax. This is
			 * just an escape goat if all of the above failed
			 * miserably.
			 */
			if (speed < feeder_rate_ratemin)
				speed = feeder_rate_ratemin;
			if (speed > feeder_rate_ratemax)
				speed = feeder_rate_ratemax;

			err = chn_setspeed(parent, speed);
			/*
			 * Try to avoid FEEDER_RATE on parent channel if the
			 * requested value is not supported by the hardware.
			 */
			if (!err && (parent->feederflags & (1 << FEEDER_RATE))) {
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
				CHN_UNLOCK(fake);
				CHN_LOCK(parent);
			}
		}
		
		if (err) {
			SLIST_REMOVE(&parent->children, pce, pcmchan_children, link);
			parent->flags &= ~CHN_F_HAS_VCHAN;
			CHN_UNLOCK(parent);
			free(pce, M_DEVBUF);
			pcm_chn_remove(d, child);
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
	int err, last;

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
			if (sce->dsp_devt)
				destroy_dev(sce->dsp_devt);
			if (sce->dspW_devt)
				destroy_dev(sce->dspW_devt);
			if (sce->audio_devt)
				destroy_dev(sce->audio_devt);
			if (sce->dspr_devt)
				destroy_dev(sce->dspr_devt);
			break;
		}
	}
	SLIST_REMOVE(&parent->children, pce, pcmchan_children, link);
	free(pce, M_DEVBUF);

	last = SLIST_EMPTY(&parent->children);
	if (last) {
		parent->flags &= ~CHN_F_BUSY;
		parent->flags &= ~CHN_F_HAS_VCHAN;
	}

	/* remove us from our grandparent's channel list */
	err = pcm_chn_remove(d, c);

	CHN_UNLOCK(parent);
	/* destroy ourselves */
	if (!err)
		err = pcm_chn_destroy(c);

#if 0
	if (!err && last) {
		CHN_LOCK(parent);
		chn_reset(parent, chn_getcaps(parent)->fmtlist[0]);
		chn_setspeed(parent, chn_getcaps(parent)->minspeed);
		CHN_UNLOCK(parent);
	}
#endif

	return err;
}

int
vchan_initsys(device_t dev)
{
#ifdef SND_DYNSYSCTL
	struct snddev_info *d;

    	d = device_get_softc(dev);
	SYSCTL_ADD_PROC(snd_sysctl_tree(dev), SYSCTL_CHILDREN(snd_sysctl_tree_top(dev)),
            OID_AUTO, "vchans", CTLTYPE_INT | CTLFLAG_RW, d, sizeof(d),
	    sysctl_hw_snd_vchans, "I", "");
#if 0
	SYSCTL_ADD_PROC(snd_sysctl_tree(dev), SYSCTL_CHILDREN(snd_sysctl_tree_top(dev)),
	    OID_AUTO, "vchanrate", CTLTYPE_INT | CTLFLAG_RW, d, sizeof(d),
	    sysctl_hw_snd_vchanrate, "I", "");
#endif
#endif

	return 0;
}
