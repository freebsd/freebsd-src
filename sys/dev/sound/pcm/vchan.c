/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * Copyright (c) 2001 Cameron Grant <cg@FreeBSD.org>
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

/* Almost entirely rewritten to add multi-format/channels mixing support. */

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>

/*
 * [ac3 , dts , linear , 0, linear, 0]
 */
#define FMTLIST_MAX		6
#define FMTLIST_OFFSET		4
#define DIGFMTS_MAX		2

#ifdef SND_DEBUG
static int snd_passthrough_verbose = 0;
SYSCTL_INT(_hw_snd, OID_AUTO, passthrough_verbose, CTLFLAG_RWTUN,
	&snd_passthrough_verbose, 0, "passthrough verbosity");

#endif

struct vchan_info {
	struct pcm_channel *channel;
	struct pcmchan_caps caps;
	uint32_t fmtlist[FMTLIST_MAX];
	int trigger;
};

bool snd_vchans_enable = true;

static void *
vchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
    struct pcm_channel *c, int dir)
{
	struct vchan_info *info;
	struct pcm_channel *p;
	uint32_t i, j, *fmtlist;

	KASSERT(dir == PCMDIR_PLAY || dir == PCMDIR_REC,
	    ("vchan_init: bad direction"));
	KASSERT(c != NULL && c->parentchannel != NULL,
	    ("vchan_init: bad channels"));

	info = malloc(sizeof(*info), M_DEVBUF, M_WAITOK | M_ZERO);
	info->channel = c;
	info->trigger = PCMTRIG_STOP;
	p = c->parentchannel;

	CHN_LOCK(p);

	fmtlist = chn_getcaps(p)->fmtlist;
	for (i = 0, j = 0; fmtlist[i] != 0 && j < DIGFMTS_MAX; i++) {
		if (fmtlist[i] & AFMT_PASSTHROUGH)
			info->fmtlist[j++] = fmtlist[i];
	}
	if (p->format & AFMT_VCHAN)
		info->fmtlist[j] = p->format;
	else
		info->fmtlist[j] = VCHAN_DEFAULT_FORMAT;
	info->caps.fmtlist = info->fmtlist +
	    ((p->flags & CHN_F_VCHAN_DYNAMIC) ? 0 : FMTLIST_OFFSET);

	CHN_UNLOCK(p);

	c->flags |= CHN_F_VIRTUAL;

	return (info);
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
	struct vchan_info *info;

	info = data;

	CHN_LOCKASSERT(info->channel);

	if (!snd_fmtvalid(format, info->caps.fmtlist))
		return (-1);

	return (0);
}

static uint32_t
vchan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct vchan_info *info;

	info = data;

	CHN_LOCKASSERT(info->channel);

	return (info->caps.maxspeed);
}

static int
vchan_trigger(kobj_t obj, void *data, int go)
{
	struct vchan_info *info;
	struct pcm_channel *c, *p;
	int ret, otrigger;

	info = data;
	c = info->channel;
	p = c->parentchannel;

	CHN_LOCKASSERT(c);
	if (!PCMTRIG_COMMON(go) || go == info->trigger)
		return (0);

	CHN_UNLOCK(c);
	CHN_LOCK(p);

	otrigger = info->trigger;
	info->trigger = go;

	switch (go) {
	case PCMTRIG_START:
		if (otrigger != PCMTRIG_START)
			CHN_INSERT_HEAD(p, c, children.busy);
		break;
	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		if (otrigger == PCMTRIG_START)
			CHN_REMOVE(p, c, children.busy);
		break;
	default:
		break;
	}

	ret = chn_notify(p, CHN_N_TRIGGER);

	CHN_LOCK(c);

	if (ret == 0 && go == PCMTRIG_START && VCHAN_SYNC_REQUIRED(c))
		ret = vchan_sync(c);

	CHN_UNLOCK(c);
	CHN_UNLOCK(p);
	CHN_LOCK(c);

	return (ret);
}

static struct pcmchan_caps *
vchan_getcaps(kobj_t obj, void *data)
{
	struct vchan_info *info;
	struct pcm_channel *c;
	uint32_t pformat, pspeed, pflags, i;

	info = data;
	c = info->channel;
	pformat = c->parentchannel->format;
	pspeed = c->parentchannel->speed;
	pflags = c->parentchannel->flags;

	CHN_LOCKASSERT(c);

	if (pflags & CHN_F_VCHAN_DYNAMIC) {
		info->caps.fmtlist = info->fmtlist;
		if (pformat & AFMT_VCHAN) {
			for (i = 0; info->caps.fmtlist[i] != 0; i++) {
				if (info->caps.fmtlist[i] & AFMT_PASSTHROUGH)
					continue;
				break;
			}
			info->caps.fmtlist[i] = pformat;
		}
		if (c->format & AFMT_PASSTHROUGH)
			info->caps.minspeed = c->speed;
		else 
			info->caps.minspeed = pspeed;
		info->caps.maxspeed = info->caps.minspeed;
	} else {
		info->caps.fmtlist = info->fmtlist + FMTLIST_OFFSET;
		if (pformat & AFMT_VCHAN)
			info->caps.fmtlist[0] = pformat;
		else {
			device_printf(c->dev,
			    "%s(): invalid vchan format 0x%08x",
			    __func__, pformat);
			info->caps.fmtlist[0] = VCHAN_DEFAULT_FORMAT;
		}
		info->caps.minspeed = pspeed;
		info->caps.maxspeed = info->caps.minspeed;
	}

	return (&info->caps);
}

static struct pcmchan_matrix *
vchan_getmatrix(kobj_t obj, void *data, uint32_t format)
{

	return (feeder_matrix_format_map(format));
}

static kobj_method_t vchan_methods[] = {
	KOBJMETHOD(channel_init,		vchan_init),
	KOBJMETHOD(channel_free,		vchan_free),
	KOBJMETHOD(channel_setformat,		vchan_setformat),
	KOBJMETHOD(channel_setspeed,		vchan_setspeed),
	KOBJMETHOD(channel_trigger,		vchan_trigger),
	KOBJMETHOD(channel_getcaps,		vchan_getcaps),
	KOBJMETHOD(channel_getmatrix,		vchan_getmatrix),
	KOBJMETHOD_END
};
CHANNEL_DECLARE(vchan);

static int
sysctl_dev_pcm_vchans(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	int err, enabled, flag;

	bus_topo_lock();
	d = devclass_get_softc(pcm_devclass, VCHAN_SYSCTL_UNIT(oidp->oid_arg1));
	if (!PCM_REGISTERED(d)) {
		bus_topo_unlock();
		return (EINVAL);
	}
	bus_topo_unlock();

	PCM_LOCK(d);
	PCM_WAIT(d);

	switch (VCHAN_SYSCTL_DIR(oidp->oid_arg1)) {
	case VCHAN_PLAY:
		/* Exit if we do not support this direction. */
		if (d->playcount < 1) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		flag = SD_F_PVCHANS;
		break;
	case VCHAN_REC:
		if (d->reccount < 1) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		flag = SD_F_RVCHANS;
		break;
	default:
		PCM_UNLOCK(d);
		return (EINVAL);
	}

	enabled = (d->flags & flag) != 0;

	PCM_ACQUIRE(d);
	PCM_UNLOCK(d);

	err = sysctl_handle_int(oidp, &enabled, 0, req);
	if (err != 0 || req->newptr == NULL) {
		PCM_RELEASE_QUICK(d);
		return (err);
	}

	if (enabled <= 0)
		d->flags &= ~flag;
	else
		d->flags |= flag;

	PCM_RELEASE_QUICK(d);

	return (0);
}

static int
sysctl_dev_pcm_vchanmode(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	struct pcm_channel *c;
	uint32_t dflags;
	int *vchanmode, direction, ret;
	char dtype[16];

	bus_topo_lock();
	d = devclass_get_softc(pcm_devclass, VCHAN_SYSCTL_UNIT(oidp->oid_arg1));
	if (!PCM_REGISTERED(d)) {
		bus_topo_unlock();
		return (EINVAL);
	}
	bus_topo_unlock();

	PCM_LOCK(d);
	PCM_WAIT(d);

	switch (VCHAN_SYSCTL_DIR(oidp->oid_arg1)) {
	case VCHAN_PLAY:
		if ((d->flags & SD_F_PVCHANS) == 0) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		direction = PCMDIR_PLAY;
		vchanmode = &d->pvchanmode;
		break;
	case VCHAN_REC:
		if ((d->flags & SD_F_RVCHANS) == 0) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		direction = PCMDIR_REC;
		vchanmode = &d->rvchanmode;
		break;
	default:
		PCM_UNLOCK(d);
		return (EINVAL);
	}

	PCM_ACQUIRE(d);
	PCM_UNLOCK(d);

	if (*vchanmode & CHN_F_VCHAN_PASSTHROUGH)
		strlcpy(dtype, "passthrough", sizeof(dtype));
	else if (*vchanmode & CHN_F_VCHAN_ADAPTIVE)
		strlcpy(dtype, "adaptive", sizeof(dtype));
	else
		strlcpy(dtype, "fixed", sizeof(dtype));

	ret = sysctl_handle_string(oidp, dtype, sizeof(dtype), req);
	if (ret != 0 || req->newptr == NULL) {
		PCM_RELEASE_QUICK(d);
		return (ret);
	}

	if (strcasecmp(dtype, "passthrough") == 0 || strcmp(dtype, "1") == 0)
		dflags = CHN_F_VCHAN_PASSTHROUGH;
	else if (strcasecmp(dtype, "adaptive") == 0 || strcmp(dtype, "2") == 0)
		dflags = CHN_F_VCHAN_ADAPTIVE;
	else if (strcasecmp(dtype, "fixed") == 0 || strcmp(dtype, "0") == 0)
		dflags = 0;
	else {
		PCM_RELEASE_QUICK(d);
		return (EINVAL);
	}

	CHN_FOREACH(c, d, channels.pcm.primary) {
		CHN_LOCK(c);
		if (c->direction != direction ||
		    dflags == (c->flags & CHN_F_VCHAN_DYNAMIC) ||
		    (c->flags & CHN_F_PASSTHROUGH)) {
			CHN_UNLOCK(c);
			continue;
		}
		c->flags &= ~CHN_F_VCHAN_DYNAMIC;
		c->flags |= dflags;
		CHN_UNLOCK(c);
		*vchanmode = dflags;
	}

	PCM_RELEASE_QUICK(d);

	return (ret);
}

/* 
 * On the fly vchan rate/format settings
 */

#define VCHAN_ACCESSIBLE(c)	(!((c)->flags & (CHN_F_PASSTHROUGH |	\
				 CHN_F_EXCLUSIVE)) &&			\
				 (((c)->flags & CHN_F_VCHAN_DYNAMIC) ||	\
				 CHN_STOPPED(c)))
static int
sysctl_dev_pcm_vchanrate(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	struct pcm_channel *c, *ch;
	int *vchanrate, direction, ret, newspd, restart;

	bus_topo_lock();
	d = devclass_get_softc(pcm_devclass, VCHAN_SYSCTL_UNIT(oidp->oid_arg1));
	if (!PCM_REGISTERED(d)) {
		bus_topo_unlock();
		return (EINVAL);
	}
	bus_topo_unlock();

	PCM_LOCK(d);
	PCM_WAIT(d);

	switch (VCHAN_SYSCTL_DIR(oidp->oid_arg1)) {
	case VCHAN_PLAY:
		if ((d->flags & SD_F_PVCHANS) == 0) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		direction = PCMDIR_PLAY;
		vchanrate = &d->pvchanrate;
		break;
	case VCHAN_REC:
		if ((d->flags & SD_F_RVCHANS) == 0) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		direction = PCMDIR_REC;
		vchanrate = &d->rvchanrate;
		break;
	default:
		PCM_UNLOCK(d);
		return (EINVAL);
	}

	PCM_ACQUIRE(d);
	PCM_UNLOCK(d);

	newspd = *vchanrate;

	ret = sysctl_handle_int(oidp, &newspd, 0, req);
	if (ret != 0 || req->newptr == NULL) {
		PCM_RELEASE_QUICK(d);
		return (ret);
	}

	if (newspd < feeder_rate_min || newspd > feeder_rate_max) {
		PCM_RELEASE_QUICK(d);
		return (EINVAL);
	}

	CHN_FOREACH(c, d, channels.pcm.primary) {
		CHN_LOCK(c);
		if (c->direction != direction) {
			CHN_UNLOCK(c);
			continue;
		}

		if (newspd != c->speed && VCHAN_ACCESSIBLE(c)) {
			if (CHN_STARTED(c)) {
				chn_abort(c);
				restart = 1;
			} else
				restart = 0;

			ret = chn_reset(c, c->format, newspd);
			if (ret == 0) {
				if (restart != 0) {
					CHN_FOREACH(ch, c, children.busy) {
						CHN_LOCK(ch);
						if (VCHAN_SYNC_REQUIRED(ch))
							vchan_sync(ch);
						CHN_UNLOCK(ch);
					}
					c->flags |= CHN_F_DIRTY;
					ret = chn_start(c, 1);
				}
			}
		}
		*vchanrate = sndbuf_getspd(c->bufsoft);

		CHN_UNLOCK(c);
	}

	PCM_RELEASE_QUICK(d);

	return (ret);
}

static int
sysctl_dev_pcm_vchanformat(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	struct pcm_channel *c, *ch;
	uint32_t newfmt;
	int *vchanformat, direction, ret, restart;
	char fmtstr[AFMTSTR_LEN];

	bus_topo_lock();
	d = devclass_get_softc(pcm_devclass, VCHAN_SYSCTL_UNIT(oidp->oid_arg1));
	if (!PCM_REGISTERED(d)) {
		bus_topo_unlock();
		return (EINVAL);
	}
	bus_topo_unlock();

	PCM_LOCK(d);
	PCM_WAIT(d);

	switch (VCHAN_SYSCTL_DIR(oidp->oid_arg1)) {
	case VCHAN_PLAY:
		if ((d->flags & SD_F_PVCHANS) == 0) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		direction = PCMDIR_PLAY;
		vchanformat = &d->pvchanformat;
		break;
	case VCHAN_REC:
		if ((d->flags & SD_F_RVCHANS) == 0) {
			PCM_UNLOCK(d);
			return (ENODEV);
		}
		direction = PCMDIR_REC;
		vchanformat = &d->rvchanformat;
		break;
	default:
		PCM_UNLOCK(d);
		return (EINVAL);
	}

	PCM_ACQUIRE(d);
	PCM_UNLOCK(d);

	bzero(fmtstr, sizeof(fmtstr));

	if (snd_afmt2str(*vchanformat, fmtstr, sizeof(fmtstr)) != *vchanformat)
		strlcpy(fmtstr, "<ERROR>", sizeof(fmtstr));

	ret = sysctl_handle_string(oidp, fmtstr, sizeof(fmtstr), req);
	if (ret != 0 || req->newptr == NULL) {
		PCM_RELEASE_QUICK(d);
		return (ret);
	}

	newfmt = snd_str2afmt(fmtstr);
	if (newfmt == 0 || !(newfmt & AFMT_VCHAN)) {
		PCM_RELEASE_QUICK(d);
		return (EINVAL);
	}

	CHN_FOREACH(c, d, channels.pcm.primary) {
		CHN_LOCK(c);
		if (c->direction != direction) {
			CHN_UNLOCK(c);
			continue;
		}
		if (newfmt != c->format && VCHAN_ACCESSIBLE(c)) {
			if (CHN_STARTED(c)) {
				chn_abort(c);
				restart = 1;
			} else
				restart = 0;

			ret = chn_reset(c, newfmt, c->speed);
			if (ret == 0) {
				if (restart != 0) {
					CHN_FOREACH(ch, c, children.busy) {
						CHN_LOCK(ch);
						if (VCHAN_SYNC_REQUIRED(ch))
							vchan_sync(ch);
						CHN_UNLOCK(ch);
					}
					c->flags |= CHN_F_DIRTY;
					ret = chn_start(c, 1);
				}
			}
		}
		*vchanformat = sndbuf_getfmt(c->bufsoft);

		CHN_UNLOCK(c);
	}

	PCM_RELEASE_QUICK(d);

	return (ret);
}

/* virtual channel interface */

#define VCHAN_FMT_HINT(x)	((x) == PCMDIR_PLAY_VIRTUAL) ?		\
				"play.vchanformat" : "rec.vchanformat"
#define VCHAN_SPD_HINT(x)	((x) == PCMDIR_PLAY_VIRTUAL) ?		\
				"play.vchanrate" : "rec.vchanrate"

int
vchan_create(struct pcm_channel *parent, struct pcm_channel **child)
{
	struct snddev_info *d;
	struct pcm_channel *ch;
	struct pcmchan_caps *parent_caps;
	uint32_t vchanfmt, vchanspd;
	int ret, direction;

	ret = 0;
	d = parent->parentsnddev;

	PCM_BUSYASSERT(d);
	CHN_LOCKASSERT(parent);

	if (!(parent->direction == PCMDIR_PLAY ||
	    parent->direction == PCMDIR_REC))
		return (EINVAL);

	CHN_UNLOCK(parent);
	PCM_LOCK(d);

	if (parent->direction == PCMDIR_PLAY) {
		direction = PCMDIR_PLAY_VIRTUAL;
		vchanfmt = d->pvchanformat;
		vchanspd = d->pvchanrate;
	} else {
		direction = PCMDIR_REC_VIRTUAL;
		vchanfmt = d->rvchanformat;
		vchanspd = d->rvchanrate;
	}

	/* create a new playback channel */
	ch = chn_init(d, parent, &vchan_class, direction, parent);
	if (ch == NULL) {
		PCM_UNLOCK(d);
		CHN_LOCK(parent);
		return (ENODEV);
	}
	PCM_UNLOCK(d);

	CHN_LOCK(parent);
	CHN_INSERT_SORT_ASCEND(parent, ch, children);

	*child = ch;

	if (parent->flags & CHN_F_HAS_VCHAN)
		return (0);

	parent->flags |= CHN_F_HAS_VCHAN | CHN_F_BUSY;

	parent_caps = chn_getcaps(parent);
	if (parent_caps == NULL) {
		ret = EINVAL;
		goto fail;
	}

	if ((ret = chn_reset(parent, vchanfmt, vchanspd)) != 0)
		goto fail;

	/*
	 * If the parent channel supports digital format,
	 * enable passthrough mode.
	 */
	if (snd_fmtvalid(AFMT_PASSTHROUGH, parent_caps->fmtlist)) {
		parent->flags &= ~CHN_F_VCHAN_DYNAMIC;
		parent->flags |= CHN_F_VCHAN_PASSTHROUGH;
	}

	return (ret);

fail:
	CHN_LOCK(ch);
	vchan_destroy(ch);
	*child = NULL;

	return (ret);
}

int
vchan_destroy(struct pcm_channel *c)
{
	struct pcm_channel *parent;

	KASSERT(c != NULL && c->parentchannel != NULL &&
	    c->parentsnddev != NULL, ("%s(): invalid channel=%p",
	    __func__, c));

	CHN_LOCKASSERT(c);

	parent = c->parentchannel;

	PCM_BUSYASSERT(c->parentsnddev);
	CHN_LOCKASSERT(parent);

	CHN_UNLOCK(c);

	/* remove us from our parent's children list */
	CHN_REMOVE(parent, c, children);
	if (CHN_EMPTY(parent, children)) {
		parent->flags &= ~(CHN_F_BUSY | CHN_F_HAS_VCHAN);
		chn_reset(parent, parent->format, parent->speed);
	}

	CHN_UNLOCK(parent);

	/* destroy ourselves */
	chn_kill(c);

	CHN_LOCK(parent);

	return (0);
}

int
#ifdef SND_DEBUG
vchan_passthrough(struct pcm_channel *c, const char *caller)
#else
vchan_sync(struct pcm_channel *c)
#endif
{
	int ret;

	KASSERT(c != NULL && c->parentchannel != NULL &&
	    (c->flags & CHN_F_VIRTUAL),
	    ("%s(): invalid passthrough", __func__));
	CHN_LOCKASSERT(c);
	CHN_LOCKASSERT(c->parentchannel);

	sndbuf_setspd(c->bufhard, c->parentchannel->speed);
	c->flags |= CHN_F_PASSTHROUGH;
	ret = feeder_chain(c);
	c->flags &= ~(CHN_F_DIRTY | CHN_F_PASSTHROUGH);
	if (ret != 0)
		c->flags |= CHN_F_DIRTY;

#ifdef SND_DEBUG
	if (snd_passthrough_verbose) {
		device_printf(c->dev, "%s(%s/%s) %s() -> re-sync err=%d\n",
		    __func__, c->name, c->comm, caller, ret);
	}
#endif

	return (ret);
}

static int
sysctl_hw_snd_vchans_enable(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	int i, v, error;

	v = snd_vchans_enable;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	bus_topo_lock();
	snd_vchans_enable = v >= 1;

	for (i = 0; pcm_devclass != NULL &&
	    i < devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (!PCM_REGISTERED(d))
			continue;
		PCM_ACQUIRE_QUICK(d);
		if (snd_vchans_enable) {
			if (d->playcount > 0)
				d->flags |= SD_F_PVCHANS;
			if (d->reccount > 0)
				d->flags |= SD_F_RVCHANS;
		} else
			d->flags &= ~(SD_F_PVCHANS | SD_F_RVCHANS);
		PCM_RELEASE_QUICK(d);
	}
	bus_topo_unlock();

	return (0);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, vchans_enable,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, 0, sizeof(int),
    sysctl_hw_snd_vchans_enable, "I", "global virtual channel switch");

void
vchan_initsys(device_t dev)
{
	struct snddev_info *d;
	int unit;

	unit = device_get_unit(dev);
	d = device_get_softc(dev);

	/* Play */
	SYSCTL_ADD_PROC(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(d->play_sysctl_tree),
	    OID_AUTO, "vchans", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, PLAY), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchans, "I", "virtual channels enabled");
	SYSCTL_ADD_PROC(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(d->play_sysctl_tree),
	    OID_AUTO, "vchanmode",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, PLAY), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchanmode, "A",
	    "vchan format/rate selection: 0=fixed, 1=passthrough, 2=adaptive");
	SYSCTL_ADD_PROC(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(d->play_sysctl_tree),
	    OID_AUTO, "vchanrate",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, PLAY), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchanrate, "I", "virtual channel mixing speed/rate");
	SYSCTL_ADD_PROC(&d->play_sysctl_ctx,
	    SYSCTL_CHILDREN(d->play_sysctl_tree),
	    OID_AUTO, "vchanformat",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, PLAY), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchanformat, "A", "virtual channel mixing format");
	/* Rec */
	SYSCTL_ADD_PROC(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(d->rec_sysctl_tree),
	    OID_AUTO, "vchans",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, REC), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchans, "I", "virtual channels enabled");
	SYSCTL_ADD_PROC(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(d->rec_sysctl_tree),
	    OID_AUTO, "vchanmode",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, REC), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchanmode, "A",
	    "vchan format/rate selection: 0=fixed, 1=passthrough, 2=adaptive");
	SYSCTL_ADD_PROC(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(d->rec_sysctl_tree),
	    OID_AUTO, "vchanrate",
	    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, REC), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchanrate, "I", "virtual channel mixing speed/rate");
	SYSCTL_ADD_PROC(&d->rec_sysctl_ctx,
	    SYSCTL_CHILDREN(d->rec_sysctl_tree),
	    OID_AUTO, "vchanformat",
	    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    VCHAN_SYSCTL_DATA(unit, REC), VCHAN_SYSCTL_DATA_SIZE,
	    sysctl_dev_pcm_vchanformat, "A", "virtual channel mixing format");
}
