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
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/vchan.h>
#include <sys/sysctl.h>

devclass_t pcm_devclass;

#ifdef USING_DEVFS
int snd_unit = 0;
TUNABLE_INT("hw.snd.unit", &snd_unit);
#endif
int snd_autovchans = 0;
int snd_maxvchans = 0;
#if __FreeBSD_version > 500000
TUNABLE_INT("hw.snd.autovchans", &snd_autovchans);
TUNABLE_INT("hw.snd.maxvchans", &snd_maxvchans);
#else
TUNABLE_INT("hw.snd.autovchans", 0, snd_autovchans);
TUNABLE_INT("hw.snd.maxvchans", 0, snd_maxvchans);
#endif

SYSCTL_NODE(_hw, OID_AUTO, snd, CTLFLAG_RD, 0, "Sound driver");

void *
snd_mtxcreate(const char *desc)
{
#ifdef USING_MUTEX
	struct mtx *m;

	m = malloc(sizeof(*m), M_DEVBUF, M_WAITOK | M_ZERO);
	if (m == NULL)
		return NULL;
	mtx_init(m, desc, MTX_RECURSE);
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

/* return a locked channel */
struct pcm_channel *
pcm_chnalloc(struct snddev_info *d, int direction, pid_t pid)
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
			c->flags |= CHN_F_BUSY;
			c->pid = pid;
			return c;
		}
		CHN_UNLOCK(c);
	}

	/* no channel available */
	if (direction == PCMDIR_PLAY) {
		if ((d->vchancount > 0) && (d->vchancount < snd_maxvchans)) {
			/* try to create a vchan */
			SLIST_FOREACH(sce, &d->channels, link) {
				c = sce->channel;
				if (!SLIST_EMPTY(&c->children)) {
					err = vchan_create(c);
					if (!err)
						return pcm_chnalloc(d, direction, pid);
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
sysctl_hw_snd_autovchans(SYSCTL_HANDLER_ARGS)
{
	int v, error;

	v = snd_autovchans;
	error = sysctl_handle_int(oidp, &v, sizeof(v), req);
	if (error == 0 && req->newptr != NULL) {
		if (v < 0 || v >= SND_MAXVCHANS)
			return EINVAL;
		snd_autovchans = v;
	}
	return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, autovchans, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_snd_autovchans, "I", "");

static int
sysctl_hw_snd_maxvchans(SYSCTL_HANDLER_ARGS)
{
	int v, error;

	v = snd_maxvchans;
	error = sysctl_handle_int(oidp, &v, sizeof(v), req);
	if (error == 0 && req->newptr != NULL) {
		if (v < 0 || v >= SND_MAXVCHANS)
			return EINVAL;
		snd_maxvchans = v;
	}
	return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, maxvchans, CTLTYPE_INT | CTLFLAG_RW,
            0, sizeof(int), sysctl_hw_snd_maxvchans, "I", "");

struct pcm_channel *
pcm_chn_create(struct snddev_info *d, struct pcm_channel *parent, kobj_class_t cls, int dir, void *devinfo)
{
	struct pcm_channel *ch;
	char *dirs;
    	int err;

	switch(dir) {
	case PCMDIR_PLAY:
		dirs = "play";
		break;
	case PCMDIR_REC:
		dirs = "record";
		break;
	case PCMDIR_VIRTUAL:
		dirs = "virtual";
		dir = PCMDIR_PLAY;
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

	ch->pid = -1;
	ch->parentsnddev = d;
	ch->parentchannel = parent;
	snprintf(ch->name, 32, "%s:%d:%s", device_get_nameunit(d->dev), d->chancount, dirs);

	err = chn_init(ch, devinfo, dir);
	if (err) {
		device_printf(d->dev, "chn_init() for channel %d (%s) failed: err = %d\n", d->chancount, dirs, err);
		kobj_delete(ch->methods, M_DEVBUF);
		free(ch, M_DEVBUF);
		return NULL;
	}

	return ch;
}

int
pcm_chn_destroy(struct pcm_channel *ch)
{
	int err;

	err = chn_kill(ch);
	if (err) {
		device_printf(ch->parentsnddev->dev, "chn_kill() for %s failed, err = %d\n", ch->name, err);
		return err;
	}

	kobj_delete(ch->methods, M_DEVBUF);
	free(ch, M_DEVBUF);

	return 0;
}

int
pcm_chn_add(struct snddev_info *d, struct pcm_channel *ch, int mkdev)
{
    	struct snddev_channel *sce;
    	int unit = device_get_unit(d->dev);

	snd_mtxlock(d->lock);

	sce = malloc(sizeof(*sce), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!sce) {
		snd_mtxunlock(d->lock);
		return ENOMEM;
	}

	sce->channel = ch;
	SLIST_INSERT_HEAD(&d->channels, sce, link);

	if (mkdev)
		dsp_register(unit, d->devcount++);
    	d->chancount++;
	if (ch->flags & CHN_F_VIRTUAL)
		d->vchancount++;

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
	if (ch->flags & CHN_F_VIRTUAL)
		d->vchancount--;
	d->chancount--;
	SLIST_REMOVE(&d->channels, sce, snddev_channel, link);
	free(sce, M_DEVBUF);

	if (rmdev)
		dsp_unregister(unit, --d->devcount);
	snd_mtxunlock(d->lock);

	return 0;
}

int
pcm_addchan(device_t dev, int dir, kobj_class_t cls, void *devinfo)
{
    	struct snddev_info *d = device_get_softc(dev);
	struct pcm_channel *ch, *child;
	struct pcmchan_children *pce;
    	int i, err;

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

	if ((dir == PCMDIR_PLAY) && (d->flags & SD_F_AUTOVCHAN) && (snd_autovchans > 0)) {
		ch->flags |= CHN_F_BUSY;
		for (i = 0; err == 0 && i < snd_autovchans; i++)
			err = vchan_create(ch);
		if (err) {
			device_printf(d->dev, "vchan_create(%d) failed, err=%d\n", i - 1, err);
			SLIST_FOREACH(pce, &ch->children, link) {
				child = pce->channel;
				vchan_destroy(child);
			}
			return err;
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

/* This is the generic init routine */
int
pcm_register(device_t dev, void *devinfo, int numplay, int numrec)
{
    	struct snddev_info *d = device_get_softc(dev);

	d->lock = snd_mtxcreate(device_get_nameunit(dev));
	snd_mtxlock(d->lock);

	d->flags = 0;
	d->dev = dev;
	d->devinfo = devinfo;
	d->devcount = 0;
	d->chancount = 0;
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
#endif
	if (numplay > 0)
		vchan_initsys(d);
	if (numplay == 1)
		d->flags |= SD_F_AUTOVCHAN;

	snd_mtxunlock(d->lock);
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

	snd_mtxlock(d->lock);
	if (d->inprog) {
		device_printf(dev, "unregister: operation in progress");
		snd_mtxunlock(d->lock);
		return EBUSY;
	}
	SLIST_FOREACH(sce, &d->channels, link) {
		if (sce->channel->refcount > 0) {
			device_printf(dev, "unregister: channel busy");
			snd_mtxunlock(d->lock);
			return EBUSY;
		}
	}
	if (mixer_uninit(dev)) {
		device_printf(dev, "unregister: mixer busy");
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

static moduledata_t sndpcm_mod = {
	"snd_pcm",
	NULL,
	NULL
};
DECLARE_MODULE(snd_pcm, sndpcm_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(snd_pcm, PCM_MODVER);
