/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
 * (C) 1997 Luigi Rizzo (luigi@iet.unipi.it)
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
#include <sys/sysctl.h>

#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

struct snddev_channel {
	SLIST_ENTRY(snddev_channel) link;
	struct pcm_channel *channel;
};

struct snddev_info {
	SLIST_HEAD(, snddev_channel) channels;
	struct pcm_channel *fakechan;
	unsigned devcount, playcount, reccount, vchancount;
	unsigned flags;
	int inprog;
	unsigned int bufsz;
	void *devinfo;
	device_t dev;
	char status[SND_STATUSLEN];
	struct sysctl_ctx_list sysctl_tree;
	struct sysctl_oid *sysctl_tree_top;
	void *lock;
};

devclass_t pcm_devclass;

int pcm_veto_load = 1;

#ifdef USING_DEVFS
int snd_unit = 0;
TUNABLE_INT("hw.snd.unit", &snd_unit);
#endif

int snd_maxautovchans = 0;
TUNABLE_INT("hw.snd.maxautovchans", &snd_maxautovchans);

SYSCTL_NODE(_hw, OID_AUTO, snd, CTLFLAG_RD, 0, "Sound driver");

static int sndstat_prepare_pcm(struct sbuf *s, device_t dev, int verbose);

struct sysctl_ctx_list *
snd_sysctl_tree(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);

	return &d->sysctl_tree;
}

struct sysctl_oid *
snd_sysctl_tree_top(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);

	return d->sysctl_tree_top;
}

void *
snd_mtxcreate(const char *desc, const char *type)
{
#ifdef USING_MUTEX
	struct mtx *m;

	m = malloc(sizeof(*m), M_DEVBUF, M_WAITOK | M_ZERO);
	if (m == NULL)
		return NULL;
	mtx_init(m, desc, type, MTX_RECURSE);
	return m;
#else
	return (void *)0xcafebabe;
#endif
}

void
snd_mtxfree(void *m)
{
#ifdef USING_MUTEX
	struct mtx *mtx = m;

	mtx_assert(mtx, MA_OWNED);
	mtx_destroy(mtx);
	free(mtx, M_DEVBUF);
#endif
}

void
snd_mtxassert(void *m)
{
#ifdef USING_MUTEX
#ifdef INVARIANTS
	struct mtx *mtx = m;

	mtx_assert(mtx, MA_OWNED);
#endif
#endif
}

void
snd_mtxlock(void *m)
{
#ifdef USING_MUTEX
	struct mtx *mtx = m;

	mtx_lock(mtx);
#endif
}

void
snd_mtxunlock(void *m)
{
#ifdef USING_MUTEX
	struct mtx *mtx = m;

	mtx_unlock(mtx);
#endif
}

int
snd_setup_intr(device_t dev, struct resource *res, int flags, driver_intr_t hand, void *param, void **cookiep)
{
#ifdef USING_MUTEX
	flags &= INTR_MPSAFE;
	flags |= INTR_TYPE_AV;
#else
	flags = INTR_TYPE_AV;
#endif
	return bus_setup_intr(dev, res, flags, hand, param, cookiep);
}

void
pcm_lock(struct snddev_info *d)
{
	snd_mtxlock(d->lock);
}

void
pcm_unlock(struct snddev_info *d)
{
	snd_mtxunlock(d->lock);
}

struct pcm_channel *
pcm_getfakechan(struct snddev_info *d)
{
	return d->fakechan;
}

/* return a locked channel */
struct pcm_channel *
pcm_chnalloc(struct snddev_info *d, int direction, pid_t pid, int chnum)
{
	struct pcm_channel *c;
    	struct snddev_channel *sce;
	int err;

	snd_mtxassert(d->lock);

	/* scan for a free channel */
	SLIST_FOREACH(sce, &d->channels, link) {
		c = sce->channel;
		CHN_LOCK(c);
		if ((c->direction == direction) && !(c->flags & CHN_F_BUSY)) {
			if (chnum == -1 || c->num == chnum) {
				c->flags |= CHN_F_BUSY;
				c->pid = pid;
				return c;
			}
		}
		CHN_UNLOCK(c);
	}

	/* no channel available */
	if (direction == PCMDIR_PLAY) {
		if ((d->vchancount > 0) && (d->vchancount < snd_maxautovchans)) {
			/* try to create a vchan */
			SLIST_FOREACH(sce, &d->channels, link) {
				c = sce->channel;
				if (!SLIST_EMPTY(&c->children)) {
					err = vchan_create(c);
					if (!err)
						return pcm_chnalloc(d, direction, pid, -1);
					else
						device_printf(d->dev, "vchan_create(%s) == %d\n", c->name, err);
				}
			}
		}
	}

	return NULL;
}

/* release a locked channel and unlock it */
int
pcm_chnrelease(struct pcm_channel *c)
{
	CHN_LOCKASSERT(c);
	c->flags &= ~CHN_F_BUSY;
	c->pid = -1;
	CHN_UNLOCK(c);
	return 0;
}

int
pcm_chnref(struct pcm_channel *c, int ref)
{
	int r;

	CHN_LOCKASSERT(c);
	c->refcount += ref;
	r = c->refcount;
	return r;
}

int
pcm_inprog(struct snddev_info *d, int delta)
{
	d->inprog += delta;
	return d->inprog;
}

static void
pcm_setmaxautovchans(struct snddev_info *d, int num)
{
	struct pcm_channel *c;
    	struct snddev_channel *sce;
	int err, done;

	if (num > 0 && d->vchancount == 0) {
		SLIST_FOREACH(sce, &d->channels, link) {
			c = sce->channel;
			if ((c->direction == PCMDIR_PLAY) && !(c->flags & CHN_F_BUSY)) {
				c->flags |= CHN_F_BUSY;
				err = vchan_create(c);
				if (err) {
					device_printf(d->dev, "vchan_create(%s) == %d\n", c->name, err);
					c->flags &= ~CHN_F_BUSY;
				}
				return;
			}
		}
	}
	if (num == 0 && d->vchancount > 0) {
		done = 0;
		while (!done) {
			done = 1;
			SLIST_FOREACH(sce, &d->channels, link) {
				c = sce->channel;
				if ((c->flags & CHN_F_VIRTUAL) && !(c->flags & CHN_F_BUSY)) {
					done = 0;
					err = vchan_destroy(c);
					if (err)
						device_printf(d->dev, "vchan_destroy(%s) == %d\n", c->name, err);
					goto restart;
				}
			}
restart:
		}
	}
}

#ifdef USING_DEVFS
static int
sysctl_hw_snd_unit(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	int error, unit;

	unit = snd_unit;
	error = sysctl_handle_int(oidp, &unit, sizeof(unit), req);
	if (error == 0 && req->newptr != NULL) {
		if (unit < 0 || unit >= devclass_get_maxunit(pcm_devclass))
			return EINVAL;
		d = devclass_get_softc(pcm_devclass, unit);
		if (d == NULL || SLIST_EMPTY(&d->channels))
			return EINVAL;
		snd_unit = unit;
	}
	return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, unit, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_snd_unit, "I", "");
#endif

static int
sysctl_hw_snd_maxautovchans(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
	int i, v, error;

	v = snd_maxautovchans;
	error = sysctl_handle_int(oidp, &v, sizeof(v), req);
	if (error == 0 && req->newptr != NULL) {
		if (v < 0 || v >= SND_MAXVCHANS)
			return EINVAL;
		if (v != snd_maxautovchans) {
			for (i = 0; i < devclass_get_maxunit(pcm_devclass); i++) {
				d = devclass_get_softc(pcm_devclass, i);
				if (!d)
					continue;
				pcm_setmaxautovchans(d, v);
			}
		}
		snd_maxautovchans = v;
	}
	return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, maxautovchans, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_snd_maxautovchans, "I", "");

struct pcm_channel *
pcm_chn_create(struct snddev_info *d, struct pcm_channel *parent, kobj_class_t cls, int dir, void *devinfo)
{
	struct pcm_channel *ch;
	char *dirs;
    	int err, *pnum;

	switch(dir) {
	case PCMDIR_PLAY:
		dirs = "play";
		pnum = &d->playcount;
		break;

	case PCMDIR_REC:
		dirs = "record";
		pnum = &d->reccount;
		break;

	case PCMDIR_VIRTUAL:
		dirs = "virtual";
		dir = PCMDIR_PLAY;
		pnum = &d->vchancount;
		break;

	default:
		return NULL;
	}

	ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!ch)
		return NULL;

	ch->methods = kobj_create(cls, M_DEVBUF, M_WAITOK);
	if (!ch->methods) {
		free(ch, M_DEVBUF);

		return NULL;
	}

	ch->num = (*pnum)++;

	ch->pid = -1;
	ch->parentsnddev = d;
	ch->parentchannel = parent;
	ch->dev = d->dev;
	snprintf(ch->name, 32, "%s:%s:%d", device_get_nameunit(d->dev), dirs, ch->num);

	err = chn_init(ch, devinfo, dir);
	if (err) {
		device_printf(d->dev, "chn_init(%s) failed: err = %d\n", ch->name, err);
		kobj_delete(ch->methods, M_DEVBUF);
		free(ch, M_DEVBUF);
		(*pnum)--;

		return NULL;
	}

	return ch;
}

int
pcm_chn_destroy(struct pcm_channel *ch)
{
	struct snddev_info *d;
	int err;

	d = ch->parentsnddev;
	err = chn_kill(ch);
	if (err) {
		device_printf(d->dev, "chn_kill(%s) failed, err = %d\n", ch->name, err);
		return err;
	}

	if (ch->direction == PCMDIR_REC)
		d->reccount--;
	else if (ch->flags & CHN_F_VIRTUAL)
		d->vchancount--;
	else
		d->playcount--;

	kobj_delete(ch->methods, M_DEVBUF);
	free(ch, M_DEVBUF);

	return 0;
}

int
pcm_chn_add(struct snddev_info *d, struct pcm_channel *ch, int mkdev)
{
    	struct snddev_channel *sce, *tmp, *after;
    	int unit = device_get_unit(d->dev);

	snd_mtxlock(d->lock);

	sce = malloc(sizeof(*sce), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!sce) {
		snd_mtxunlock(d->lock);
		return ENOMEM;
	}

	sce->channel = ch;
	if (SLIST_EMPTY(&d->channels)) {
		SLIST_INSERT_HEAD(&d->channels, sce, link);
	} else {
		after = NULL;
		SLIST_FOREACH(tmp, &d->channels, link) {
			after = tmp;
		}
		SLIST_INSERT_AFTER(after, sce, link);
	}

	if (mkdev) {
		dsp_register(unit, d->devcount++);
		if (ch->direction == PCMDIR_REC)
			dsp_registerrec(unit, ch->num);
	}

	snd_mtxunlock(d->lock);

	return 0;
}

int
pcm_chn_remove(struct snddev_info *d, struct pcm_channel *ch, int rmdev)
{
    	struct snddev_channel *sce;
    	int unit = device_get_unit(d->dev);

	snd_mtxlock(d->lock);
	SLIST_FOREACH(sce, &d->channels, link) {
		if (sce->channel == ch)
			goto gotit;
	}
	snd_mtxunlock(d->lock);
	return EINVAL;
gotit:
	SLIST_REMOVE(&d->channels, sce, snddev_channel, link);
	free(sce, M_DEVBUF);

	if (rmdev) {
		dsp_unregister(unit, --d->devcount);
		if (ch->direction == PCMDIR_REC)
			dsp_unregisterrec(unit, ch->num);
	}
	snd_mtxunlock(d->lock);

	return 0;
}

int
pcm_addchan(device_t dev, int dir, kobj_class_t cls, void *devinfo)
{
    	struct snddev_info *d = device_get_softc(dev);
	struct pcm_channel *ch;
    	int err;

	ch = pcm_chn_create(d, NULL, cls, dir, devinfo);
	if (!ch) {
		device_printf(d->dev, "pcm_chn_create(%s, %d, %p) failed\n", cls->name, dir, devinfo);
		return ENODEV;
	}

	err = pcm_chn_add(d, ch, 1);
	if (err) {
		device_printf(d->dev, "pcm_chn_add(%s) failed, err=%d\n", ch->name, err);
		pcm_chn_destroy(ch);
		return err;
	}

	if (snd_maxautovchans > 0 && (d->flags & SD_F_AUTOVCHAN)) {
		ch->flags |= CHN_F_BUSY;
		err = vchan_create(ch);
		if (err) {
			device_printf(d->dev, "vchan_create(%s) == %d\n", ch->name, err);
			ch->flags &= ~CHN_F_BUSY;
		}
	}

	return err;
}

static int
pcm_killchan(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);
    	struct snddev_channel *sce;

	snd_mtxlock(d->lock);
	sce = SLIST_FIRST(&d->channels);
	snd_mtxunlock(d->lock);

	return pcm_chn_remove(d, sce->channel, 1);
}

int
pcm_setstatus(device_t dev, char *str)
{
    	struct snddev_info *d = device_get_softc(dev);

	snd_mtxlock(d->lock);
	strncpy(d->status, str, SND_STATUSLEN);
	snd_mtxunlock(d->lock);
	return 0;
}

u_int32_t
pcm_getflags(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);

	return d->flags;
}

void
pcm_setflags(device_t dev, u_int32_t val)
{
    	struct snddev_info *d = device_get_softc(dev);

	d->flags = val;
}

void *
pcm_getdevinfo(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);

	return d->devinfo;
}

unsigned int
pcm_getbuffersize(device_t dev, unsigned int min, unsigned int deflt, unsigned int max)
{
    	struct snddev_info *d = device_get_softc(dev);
	int sz, x;

	sz = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev), "buffersize", &sz) == 0) {
		x = sz;
		RANGE(sz, min, max);
		if (x != sz)
			device_printf(dev, "'buffersize=%d' hint is out of range (%d-%d), using %d\n", x, min, max, sz);
		x = min;
		while (x < sz)
			x <<= 1;
		if (x > sz)
			x >>= 1;
		if (x != sz) {
			device_printf(dev, "'buffersize=%d' hint is not a power of 2, using %d\n", sz, x);
			sz = x;
		}
	} else {
		sz = deflt;
	}

	d->bufsz = sz;

	return sz;
}

int
pcm_register(device_t dev, void *devinfo, int numplay, int numrec)
{
    	struct snddev_info *d = device_get_softc(dev);

	if (pcm_veto_load) {
		device_printf(dev, "disabled due to an error while initialising: %d\n", pcm_veto_load);

		return EINVAL;
	}

	d->lock = snd_mtxcreate(device_get_nameunit(dev), "sound cdev");
	snd_mtxlock(d->lock);

	d->flags = 0;
	d->dev = dev;
	d->devinfo = devinfo;
	d->devcount = 0;
	d->reccount = 0;
	d->playcount = 0;
	d->vchancount = 0;
	d->inprog = 0;

	if (((numplay == 0) || (numrec == 0)) && (numplay != numrec))
		d->flags |= SD_F_SIMPLEX;

	d->fakechan = fkchan_setup(dev);
	chn_init(d->fakechan, NULL, 0);

#ifdef SND_DYNSYSCTL
	sysctl_ctx_init(&d->sysctl_tree);
	d->sysctl_tree_top = SYSCTL_ADD_NODE(&d->sysctl_tree,
				 SYSCTL_STATIC_CHILDREN(_hw_snd), OID_AUTO,
				 device_get_nameunit(dev), CTLFLAG_RD, 0, "");
	if (d->sysctl_tree_top == NULL) {
		sysctl_ctx_free(&d->sysctl_tree);
		goto no;
	}
	SYSCTL_ADD_INT(snd_sysctl_tree(dev), SYSCTL_CHILDREN(snd_sysctl_tree_top(dev)),
            OID_AUTO, "buffersize", CTLFLAG_RD, &d->bufsz, 0, "");
#endif
	if (numplay > 0)
		vchan_initsys(dev);
	if (numplay == 1)
		d->flags |= SD_F_AUTOVCHAN;

	snd_mtxunlock(d->lock);
	sndstat_register(dev, d->status, sndstat_prepare_pcm);
    	return 0;
no:
	snd_mtxfree(d->lock);
	return ENXIO;
}

int
pcm_unregister(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);
    	struct snddev_channel *sce;
	struct pcm_channel *ch;

	snd_mtxlock(d->lock);
	if (d->inprog) {
		device_printf(dev, "unregister: operation in progress\n");
		snd_mtxunlock(d->lock);
		return EBUSY;
	}
	if (sndstat_busy() != 0) {
		device_printf(dev, "unregister: sndstat busy\n");
		snd_mtxunlock(d->lock);
		return EBUSY;
	}
	SLIST_FOREACH(sce, &d->channels, link) {
		ch = sce->channel;
		if (ch->refcount > 0) {
			device_printf(dev, "unregister: channel %s busy (pid %d)\n", ch->name, ch->pid);
			snd_mtxunlock(d->lock);
			return EBUSY;
		}
	}
	if (mixer_uninit(dev)) {
		device_printf(dev, "unregister: mixer busy\n");
		snd_mtxunlock(d->lock);
		return EBUSY;
	}

#ifdef SND_DYNSYSCTL
	d->sysctl_tree_top = NULL;
	sysctl_ctx_free(&d->sysctl_tree);
#endif
	while (!SLIST_EMPTY(&d->channels))
		pcm_killchan(dev);

	chn_kill(d->fakechan);
	fkchan_kill(d->fakechan);

	snd_mtxfree(d->lock);
	return 0;
}

/************************************************************************/

static int
sndstat_prepare_pcm(struct sbuf *s, device_t dev, int verbose)
{
    	struct snddev_info *d;
    	struct snddev_channel *sce;
	struct pcm_channel *c;
	struct pcm_feeder *f;
    	int pc, rc, vc;

	if (verbose < 1)
		return 0;

	d = device_get_softc(dev);
	if (!d)
		return ENXIO;

	snd_mtxlock(d->lock);
	if (!SLIST_EMPTY(&d->channels)) {
		pc = rc = vc = 0;
		SLIST_FOREACH(sce, &d->channels, link) {
			c = sce->channel;
			if (c->direction == PCMDIR_PLAY) {
				if (c->flags & CHN_F_VIRTUAL)
					vc++;
				else
					pc++;
			} else
				rc++;
		}
		sbuf_printf(s, " (%dp/%dr/%dv channels%s%s)", d->playcount, d->reccount, d->vchancount,
				(d->flags & SD_F_SIMPLEX)? "" : " duplex",
#ifdef USING_DEVFS
				(device_get_unit(dev) == snd_unit)? " default" : ""
#else
				""
#endif
				);
		if (verbose <= 1)
			goto skipverbose;
		SLIST_FOREACH(sce, &d->channels, link) {
			c = sce->channel;
			sbuf_printf(s, "\n\t");

			sbuf_printf(s, "%s[%s]: ", c->parentchannel? c->parentchannel->name : "", c->name);
			sbuf_printf(s, "spd %d", c->speed);
			if (c->speed != sndbuf_getspd(c->bufhard))
				sbuf_printf(s, "/%d", sndbuf_getspd(c->bufhard));
			sbuf_printf(s, ", fmt 0x%08x", c->format);
			if (c->format != sndbuf_getfmt(c->bufhard))
				sbuf_printf(s, "/0x%08x", sndbuf_getfmt(c->bufhard));
			sbuf_printf(s, ", flags %08x", c->flags);
			if (c->pid != -1)
				sbuf_printf(s, ", pid %d", c->pid);
			sbuf_printf(s, "\n\t");

			if (c->bufhard != NULL && c->bufsoft != NULL) {
				sbuf_printf(s, "interrupts %d, ", c->interrupts);
				if (c->direction == PCMDIR_REC)
					sbuf_printf(s, "overruns %d, hfree %d, sfree %d",
						c->xruns, sndbuf_getfree(c->bufhard), sndbuf_getfree(c->bufsoft));
				else
					sbuf_printf(s, "underruns %d, ready %d",
						c->xruns, sndbuf_getready(c->bufsoft));
				sbuf_printf(s, "\n\t");
			}

			sbuf_printf(s, "{%s}", (c->direction == PCMDIR_REC)? "hardware" : "userland");
			sbuf_printf(s, " -> ");
			f = c->feeder;
			while (f->source != NULL)
				f = f->source;
			while (f != NULL) {
				sbuf_printf(s, "%s", f->class->name);
				if (f->desc->type == FEEDER_FMT)
					sbuf_printf(s, "(0x%08x -> 0x%08x)", f->desc->in, f->desc->out);
				if (f->desc->type == FEEDER_RATE)
					sbuf_printf(s, "(%d -> %d)", FEEDER_GET(f, FEEDRATE_SRC), FEEDER_GET(f, FEEDRATE_DST));
				if (f->desc->type == FEEDER_ROOT || f->desc->type == FEEDER_MIXER)
					sbuf_printf(s, "(0x%08x)", f->desc->out);
				sbuf_printf(s, " -> ");
				f = f->parent;
			}
			sbuf_printf(s, "{%s}", (c->direction == PCMDIR_REC)? "userland" : "hardware");
		}
skipverbose:
	} else
		sbuf_printf(s, " (mixer only)");
	snd_mtxunlock(d->lock);

	return 0;
}

/************************************************************************/

#ifdef SND_DYNSYSCTL
int
sysctl_hw_snd_vchans(SYSCTL_HANDLER_ARGS)
{
	struct snddev_info *d;
    	struct snddev_channel *sce;
	struct pcm_channel *c;
	int err, oldcnt, newcnt, cnt;

	d = oidp->oid_arg1;

	pcm_lock(d);
	cnt = 0;
	SLIST_FOREACH(sce, &d->channels, link) {
		c = sce->channel;
		if ((c->direction == PCMDIR_PLAY) && (c->flags & CHN_F_VIRTUAL))
			cnt++;
	}
	oldcnt = cnt;
	newcnt = cnt;

	err = sysctl_handle_int(oidp, &newcnt, sizeof(newcnt), req);
	if (err == 0 && req->newptr != NULL) {
		if (newcnt < 0 || newcnt > SND_MAXVCHANS) {
			pcm_unlock(d);
			return EINVAL;
		}

		if (newcnt > cnt) {
			/* add new vchans - find a parent channel first */
			SLIST_FOREACH(sce, &d->channels, link) {
				c = sce->channel;
				/* not a candidate if not a play channel */
				if (c->direction != PCMDIR_PLAY)
					goto addskip;
				/* not a candidate if a virtual channel */
				if (c->flags & CHN_F_VIRTUAL)
					goto addskip;
				/* not a candidate if it's in use */
				if ((c->flags & CHN_F_BUSY) && (SLIST_EMPTY(&c->children)))
					goto addskip;
				/*
				 * if we get here we're a nonvirtual play channel, and either
				 * 1) not busy
				 * 2) busy with children, not directly open
				 *
				 * thus we can add children
				 */
				goto addok;
addskip:
			}
			pcm_unlock(d);
			return EBUSY;
addok:
			c->flags |= CHN_F_BUSY;
			while (err == 0 && newcnt > cnt) {
				err = vchan_create(c);
				if (err == 0)
					cnt++;
			}
			if (SLIST_EMPTY(&c->children))
				c->flags &= ~CHN_F_BUSY;
		} else if (newcnt < cnt) {
			while (err == 0 && newcnt < cnt) {
				SLIST_FOREACH(sce, &d->channels, link) {
					c = sce->channel;
					if ((c->flags & (CHN_F_BUSY | CHN_F_VIRTUAL)) == CHN_F_VIRTUAL)
						goto remok;
				}
				pcm_unlock(d);
				return EINVAL;
remok:
				err = vchan_destroy(c);
				if (err == 0)
					cnt--;
			}
		}
	}

	pcm_unlock(d);
	return err;
}
#endif

/************************************************************************/

static moduledata_t sndpcm_mod = {
	"snd_pcm",
	NULL,
	NULL
};
DECLARE_MODULE(snd_pcm, sndpcm_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(snd_pcm, PCM_MODVER);
