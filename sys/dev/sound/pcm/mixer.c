/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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

#include "mixer_if.h"

MALLOC_DEFINE(M_MIXER, "mixer", "mixer");

#define MIXER_NAMELEN	16
struct snd_mixer {
	KOBJ_FIELDS;
	const char *type;
	void *devinfo;
	int busy;
	int hwvol_muted;
	int hwvol_mixer;
	int hwvol_step;
	u_int32_t hwvol_mute_level;
	u_int32_t devs;
	u_int32_t recdevs;
	u_int32_t recsrc;
	u_int16_t level[32];
	char name[MIXER_NAMELEN];
	void *lock;
};

static u_int16_t snd_mixerdefaults[SOUND_MIXER_NRDEVICES] = {
	[SOUND_MIXER_VOLUME]	= 75,
	[SOUND_MIXER_BASS]	= 50,
	[SOUND_MIXER_TREBLE]	= 50,
	[SOUND_MIXER_SYNTH]	= 75,
	[SOUND_MIXER_PCM]	= 75,
	[SOUND_MIXER_SPEAKER]	= 75,
	[SOUND_MIXER_LINE]	= 75,
	[SOUND_MIXER_MIC] 	= 0,
	[SOUND_MIXER_CD]	= 75,
	[SOUND_MIXER_LINE1]	= 75,
	[SOUND_MIXER_VIDEO]	= 75,
	[SOUND_MIXER_RECLEV]	= 0,
	[SOUND_MIXER_OGAIN]	= 50,
};

static char* snd_mixernames[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

static d_open_t mixer_open;
static d_close_t mixer_close;

static struct cdevsw mixer_cdevsw = {
	/* open */	mixer_open,
	/* close */	mixer_close,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	mixer_ioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"mixer",
	/* maj */	SND_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

#ifdef USING_DEVFS
static eventhandler_tag mixer_ehtag;
#endif

static dev_t
mixer_get_devt(device_t dev)
{
	dev_t pdev;
	int unit;

	unit = device_get_unit(dev);
	pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_CTL, 0));

	return pdev;
}

#ifdef SND_DYNSYSCTL
static int
mixer_lookup(char *devname)
{
	int i;

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if (strncmp(devname, snd_mixernames[i],
		    strlen(snd_mixernames[i])) == 0)
			return i;
	return -1;
}
#endif

static int
mixer_set(struct snd_mixer *mixer, unsigned dev, unsigned lev)
{
	unsigned l, r;
	int v;

	if ((dev >= SOUND_MIXER_NRDEVICES) || (0 == (mixer->devs & (1 << dev))))
		return -1;

	l = min((lev & 0x00ff), 100);
	r = min(((lev & 0xff00) >> 8), 100);

	v = MIXER_SET(mixer, dev, l, r);
	if (v < 0)
		return -1;

	mixer->level[dev] = l | (r << 8);
	return 0;
}

static int
mixer_get(struct snd_mixer *mixer, int dev)
{
	if ((dev < SOUND_MIXER_NRDEVICES) && (mixer->devs & (1 << dev)))
		return mixer->level[dev];
	else return -1;
}

static int
mixer_setrecsrc(struct snd_mixer *mixer, u_int32_t src)
{
	src &= mixer->recdevs;
	if (src == 0)
		src = SOUND_MASK_MIC;
	mixer->recsrc = MIXER_SETRECSRC(mixer, src);
	return 0;
}

static int
mixer_getrecsrc(struct snd_mixer *mixer)
{
	return mixer->recsrc;
}

void
mix_setdevs(struct snd_mixer *m, u_int32_t v)
{
	m->devs = v;
}

void
mix_setrecdevs(struct snd_mixer *m, u_int32_t v)
{
	m->recdevs = v;
}

u_int32_t
mix_getdevs(struct snd_mixer *m)
{
	return m->devs;
}

u_int32_t
mix_getrecdevs(struct snd_mixer *m)
{
	return m->recdevs;
}

void *
mix_getdevinfo(struct snd_mixer *m)
{
	return m->devinfo;
}

int
mixer_init(device_t dev, kobj_class_t cls, void *devinfo)
{
	struct snd_mixer *m;
	u_int16_t v;
	dev_t pdev;
	int i, unit;

	m = (struct snd_mixer *)kobj_create(cls, M_MIXER, M_WAITOK | M_ZERO);
	snprintf(m->name, MIXER_NAMELEN, "%s:mixer", device_get_nameunit(dev));
	m->lock = snd_mtxcreate(m->name);
	m->type = cls->name;
	m->devinfo = devinfo;
	m->busy = 0;

	if (MIXER_INIT(m))
		goto bad;

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		v = snd_mixerdefaults[i];
		mixer_set(m, i, v | (v << 8));
	}

	mixer_setrecsrc(m, SOUND_MASK_MIC);

	unit = device_get_unit(dev);
	pdev = make_dev(&mixer_cdevsw, PCMMKMINOR(unit, SND_DEV_CTL, 0),
		 UID_ROOT, GID_WHEEL, 0666, "mixer%d", unit);
	pdev->si_drv1 = m;

	return 0;

bad:
	snd_mtxlock(m->lock);
	snd_mtxfree(m->lock);
	kobj_delete((kobj_t)m, M_MIXER);
	return -1;
}

int
mixer_uninit(device_t dev)
{
	int i;
	struct snd_mixer *m;
	dev_t pdev;

	pdev = mixer_get_devt(dev);
	m = pdev->si_drv1;
	snd_mtxlock(m->lock);

	if (m->busy) {
		snd_mtxunlock(m->lock);
		return EBUSY;
	}

	pdev->si_drv1 = NULL;
	destroy_dev(pdev);

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		mixer_set(m, i, 0);

	mixer_setrecsrc(m, SOUND_MASK_MIC);

	MIXER_UNINIT(m);

	snd_mtxfree(m->lock);
	kobj_delete((kobj_t)m, M_MIXER);

	return 0;
}

int
mixer_reinit(device_t dev)
{
	struct snd_mixer *m;
	dev_t pdev;
	int i;

	pdev = mixer_get_devt(dev);
	m = pdev->si_drv1;
	snd_mtxlock(m->lock);

	i = MIXER_REINIT(m);
	if (i) {
		snd_mtxunlock(m->lock);
		return i;
	}

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		mixer_set(m, i, m->level[i]);

	mixer_setrecsrc(m, m->recsrc);
	snd_mtxunlock(m->lock);

	return 0;
}

#ifdef SND_DYNSYSCTL
static int
sysctl_hw_snd_hwvol_mixer(SYSCTL_HANDLER_ARGS)
{
	char devname[32];
	int error, dev;
	struct snd_mixer *m;

	m = oidp->oid_arg1;
	snd_mtxlock(m->lock);
	strncpy(devname, snd_mixernames[m->hwvol_mixer], sizeof(devname));
	error = sysctl_handle_string(oidp, &devname[0], sizeof(devname), req);
	if (error == 0 && req->newptr != NULL) {
		dev = mixer_lookup(devname);
		if (dev == -1) {
			snd_mtxunlock(m->lock);
			return EINVAL;
		}
		else if (dev != m->hwvol_mixer) {
			m->hwvol_mixer = dev;
			m->hwvol_muted = 0;
		}
	}
	snd_mtxunlock(m->lock);
	return error;
}
#endif

int
mixer_hwvol_init(device_t dev)
{
	struct snddev_info *d;
	struct snd_mixer *m;
	dev_t pdev;

	d = device_get_softc(dev);
	pdev = mixer_get_devt(dev);
	m = pdev->si_drv1;
	snd_mtxlock(m->lock);

	m->hwvol_mixer = SOUND_MIXER_VOLUME;
	m->hwvol_step = 5;
#ifdef SND_DYNSYSCTL
	SYSCTL_ADD_INT(&d->sysctl_tree, SYSCTL_CHILDREN(d->sysctl_tree_top),
            OID_AUTO, "hwvol_step", CTLFLAG_RW, &m->hwvol_step, 0, "");
	SYSCTL_ADD_PROC(&d->sysctl_tree, SYSCTL_CHILDREN(d->sysctl_tree_top),
            OID_AUTO, "hwvol_mixer", CTLTYPE_STRING | CTLFLAG_RW, m, 0,
	    sysctl_hw_snd_hwvol_mixer, "A", "")
#endif
	snd_mtxunlock(m->lock);
	return 0;
}

void
mixer_hwvol_mute(device_t dev)
{
	struct snd_mixer *m;
	dev_t pdev;

	pdev = mixer_get_devt(dev);
	m = pdev->si_drv1;
	snd_mtxlock(m->lock);
	if (m->hwvol_muted) {
		m->hwvol_muted = 0;
		mixer_set(m, m->hwvol_mixer, m->hwvol_mute_level);
	} else {
		m->hwvol_muted++;
		m->hwvol_mute_level = mixer_get(m, m->hwvol_mixer);
		mixer_set(m, m->hwvol_mixer, 0);
	}
	snd_mtxunlock(m->lock);
}

void
mixer_hwvol_step(device_t dev, int left_step, int right_step)
{
	struct snd_mixer *m;
	int level, left, right;
	dev_t pdev;

	pdev = mixer_get_devt(dev);
	m = pdev->si_drv1;
	snd_mtxlock(m->lock);
	if (m->hwvol_muted) {
		m->hwvol_muted = 0;
		level = m->hwvol_mute_level;
	} else
		level = mixer_get(m, m->hwvol_mixer);
	if (level != -1) {
		left = level & 0xff;
		right = level >> 8;
		left += left_step * m->hwvol_step;
		if (left < 0)
			left = 0;
		right += right_step * m->hwvol_step;
		if (right < 0)
			right = 0;
		mixer_set(m, m->hwvol_mixer, left | right << 8);
	}
	snd_mtxunlock(m->lock);
}

/* ----------------------------------------------------------------------- */

static int
mixer_open(dev_t i_dev, int flags, int mode, struct proc *p)
{
	struct snd_mixer *m;
	intrmask_t s;

	s = spltty();
	m = i_dev->si_drv1;
	if (m->busy) {
		splx(s);
		return EBUSY;
	}
	m->busy = 1;

	splx(s);
	return 0;
}

static int
mixer_close(dev_t i_dev, int flags, int mode, struct proc *p)
{
	struct snd_mixer *m;
	intrmask_t s;

	s = spltty();
	m = i_dev->si_drv1;
	if (!m->busy) {
		splx(s);
		return EBADF;
	}
	m->busy = 0;

	splx(s);
	return 0;
}

int
mixer_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc *p)
{
	struct snd_mixer *m;
	intrmask_t s;
	int ret, *arg_i = (int *)arg;
	int v = -1, j = cmd & 0xff;

	m = i_dev->si_drv1;
	if (!m->busy)
		return EBADF;

	s = spltty();
	snd_mtxlock(m->lock);
	if ((cmd & MIXER_WRITE(0)) == MIXER_WRITE(0)) {
		if (j == SOUND_MIXER_RECSRC)
			ret = mixer_setrecsrc(m, *arg_i);
		else
			ret = mixer_set(m, j, *arg_i);
		snd_mtxunlock(m->lock);
		splx(s);
		return (ret == 0)? 0 : ENXIO;
	}

    	if ((cmd & MIXER_READ(0)) == MIXER_READ(0)) {
		switch (j) {
    		case SOUND_MIXER_DEVMASK:
    		case SOUND_MIXER_CAPS:
    		case SOUND_MIXER_STEREODEVS:
			v = mix_getdevs(m);
			break;

    		case SOUND_MIXER_RECMASK:
			v = mix_getrecdevs(m);
			break;

    		case SOUND_MIXER_RECSRC:
			v = mixer_getrecsrc(m);
			break;

		default:
			v = mixer_get(m, j);
		}
		*arg_i = v;
		snd_mtxunlock(m->lock);
		return (v != -1)? 0 : ENXIO;
	}
	snd_mtxunlock(m->lock);
	splx(s);
	return ENXIO;
}

#ifdef USING_DEVFS
static void
mixer_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	dev_t pdev;

	if (*dev != NODEV)
		return;
	if (strcmp(name, "mixer") == 0) {
		pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(snd_unit, SND_DEV_CTL, 0));
		if (pdev->si_flags & SI_NAMED)
			*dev = pdev;
	}
}

static void
mixer_sysinit(void *p)
{
	mixer_ehtag = EVENTHANDLER_REGISTER(dev_clone, mixer_clone, 0, 1000);
}

static void
mixer_sysuninit(void *p)
{
	if (mixer_ehtag != NULL)
		EVENTHANDLER_DEREGISTER(dev_clone, mixer_ehtag);
}

SYSINIT(mixer_sysinit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, mixer_sysinit, NULL);
SYSUNINIT(mixer_sysuninit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, mixer_sysuninit, NULL);
#endif


