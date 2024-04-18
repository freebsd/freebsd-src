/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005-2009 Ariff Abdullah <ariff@FreeBSD.org>
 * Portions Copyright (c) Ryan Beasley <ryan.beasley@gmail.com> - GSoC 2006
 * Copyright (c) 1999 Cameron Grant <cg@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2024 The FreeBSD Foundation
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

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/pcm/sound.h>
#include <sys/ctype.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/sysent.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

struct dsp_cdevpriv {
	struct snddev_info *sc;
	struct pcm_channel *rdch;
	struct pcm_channel *wrch;
	struct pcm_channel *volch;
	int simplex;
};

static int dsp_mmap_allow_prot_exec = 0;
SYSCTL_INT(_hw_snd, OID_AUTO, compat_linux_mmap, CTLFLAG_RWTUN,
    &dsp_mmap_allow_prot_exec, 0,
    "linux mmap compatibility (-1=force disable 0=auto 1=force enable)");

static int dsp_basename_clone = 1;
SYSCTL_INT(_hw_snd, OID_AUTO, basename_clone, CTLFLAG_RWTUN,
    &dsp_basename_clone, 0,
    "DSP basename cloning (0: Disable; 1: Enabled)");

#define DSP_REGISTERED(x)	(PCM_REGISTERED(x) && (x)->dsp_dev != NULL)

#define OLDPCM_IOCTL

static d_open_t dsp_open;
static d_read_t dsp_read;
static d_write_t dsp_write;
static d_ioctl_t dsp_ioctl;
static d_poll_t dsp_poll;
static d_mmap_t dsp_mmap;
static d_mmap_single_t dsp_mmap_single;

struct cdevsw dsp_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	dsp_open,
	.d_read =	dsp_read,
	.d_write =	dsp_write,
	.d_ioctl =	dsp_ioctl,
	.d_poll =	dsp_poll,
	.d_mmap =	dsp_mmap,
	.d_mmap_single = dsp_mmap_single,
	.d_name =	"dsp",
};

static eventhandler_tag dsp_ehtag = NULL;

static int dsp_oss_syncgroup(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_syncgroup *group);
static int dsp_oss_syncstart(int sg_id);
static int dsp_oss_policy(struct pcm_channel *wrch, struct pcm_channel *rdch, int policy);
static int dsp_oss_cookedmode(struct pcm_channel *wrch, struct pcm_channel *rdch, int enabled);
static int dsp_oss_getchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map);
static int dsp_oss_setchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map);
static int dsp_oss_getchannelmask(struct pcm_channel *wrch, struct pcm_channel *rdch, int *mask);
#ifdef OSSV4_EXPERIMENT
static int dsp_oss_getlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label);
static int dsp_oss_setlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label);
static int dsp_oss_getsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song);
static int dsp_oss_setsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song);
static int dsp_oss_setname(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *name);
#endif

int
dsp_make_dev(device_t dev)
{
	struct make_dev_args devargs;
	struct snddev_info *sc;
	int err, unit;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	make_dev_args_init(&devargs);
	devargs.mda_devsw = &dsp_cdevsw;
	devargs.mda_uid = UID_ROOT;
	devargs.mda_gid = GID_WHEEL;
	devargs.mda_mode = 0666;
	devargs.mda_si_drv1 = sc;
	err = make_dev_s(&devargs, &sc->dsp_dev, "dsp%d", unit);
	if (err != 0) {
		device_printf(dev, "failed to create dsp%d: error %d",
		    unit, err);
		return (ENXIO);
	}

	return (0);
}

void
dsp_destroy_dev(device_t dev)
{
	struct snddev_info *d;

	d = device_get_softc(dev);
	destroy_dev_sched(d->dsp_dev);
}

static void
getchns(struct dsp_cdevpriv *priv, uint32_t prio)
{
	struct snddev_info *d;
	struct pcm_channel *ch;
	uint32_t flags;

	if (priv->simplex) {
		d = priv->sc;
		if (!PCM_REGISTERED(d))
			return;
		PCM_LOCK(d);
		PCM_WAIT(d);
		PCM_ACQUIRE(d);
		/*
		 * Note: order is important -
		 *       pcm flags -> prio query flags -> wild guess
		 */
		ch = NULL;
		flags = pcm_getflags(d->dev);
		if (flags & SD_F_PRIO_WR) {
			ch = priv->rdch;
		} else if (flags & SD_F_PRIO_RD) {
			ch = priv->wrch;
		} else if (prio & SD_F_PRIO_WR) {
			ch = priv->rdch;
			flags |= SD_F_PRIO_WR;
		} else if (prio & SD_F_PRIO_RD) {
			ch = priv->wrch;
			flags |= SD_F_PRIO_RD;
		} else if (priv->wrch != NULL) {
			ch = priv->rdch;
			flags |= SD_F_PRIO_WR;
		} else if (priv->rdch != NULL) {
			ch = priv->wrch;
			flags |= SD_F_PRIO_RD;
		}
		pcm_setflags(d->dev, flags);
		if (ch != NULL) {
			CHN_LOCK(ch);
			pcm_chnref(ch, -1);
			pcm_chnrelease(ch);
		}
		PCM_RELEASE(d);
		PCM_UNLOCK(d);
	}

	if (priv->rdch != NULL && (prio & SD_F_PRIO_RD))
		CHN_LOCK(priv->rdch);
	if (priv->wrch != NULL && (prio & SD_F_PRIO_WR))
		CHN_LOCK(priv->wrch);
}

static void
relchns(struct dsp_cdevpriv *priv, uint32_t prio)
{
	if (priv->rdch != NULL && (prio & SD_F_PRIO_RD))
		CHN_UNLOCK(priv->rdch);
	if (priv->wrch != NULL && (prio & SD_F_PRIO_WR))
		CHN_UNLOCK(priv->wrch);
}

/* duplex / simplex cdev type */
enum {
	DSP_CDEV_TYPE_RDONLY,		/* simplex read-only (record)   */
	DSP_CDEV_TYPE_WRONLY,		/* simplex write-only (play)    */
	DSP_CDEV_TYPE_RDWR		/* duplex read, write, or both  */
};

#define DSP_F_VALID(x)		((x) & (FREAD | FWRITE))
#define DSP_F_DUPLEX(x)		(((x) & (FREAD | FWRITE)) == (FREAD | FWRITE))
#define DSP_F_SIMPLEX(x)	(!DSP_F_DUPLEX(x))
#define DSP_F_READ(x)		((x) & FREAD)
#define DSP_F_WRITE(x)		((x) & FWRITE)

static const struct {
	int type;
	char *name;
	char *sep;
	char *alias;
	int use_sep;
	int hw;
	int max;
	int volctl;
	uint32_t fmt, spd;
	int query;
} dsp_cdevs[] = {
	{ SND_DEV_DSP,         "dsp",    ".", NULL, 0, 0, 0, 0,
	  SND_FORMAT(AFMT_U8, 1, 0),     DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
	{ SND_DEV_AUDIO,       "audio",  ".", NULL, 0, 0, 0, 0,
	  SND_FORMAT(AFMT_MU_LAW, 1, 0), DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
	{ SND_DEV_DSP16,       "dspW",   ".", NULL, 0, 0, 0, 0,
	  SND_FORMAT(AFMT_S16_LE, 1, 0), DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
	{ SND_DEV_DSPHW_PLAY,  "dsp",   ".p", NULL, 1, 1, SND_MAXHWCHAN, 1,
	  SND_FORMAT(AFMT_S16_LE, 2, 0), 48000, DSP_CDEV_TYPE_WRONLY },
	{ SND_DEV_DSPHW_VPLAY, "dsp",  ".vp", NULL, 1, 1, SND_MAXVCHANS, 1,
	  SND_FORMAT(AFMT_S16_LE, 2, 0), 48000, DSP_CDEV_TYPE_WRONLY },
	{ SND_DEV_DSPHW_REC,   "dsp",   ".r", NULL, 1, 1, SND_MAXHWCHAN, 1,
	  SND_FORMAT(AFMT_S16_LE, 2, 0), 48000, DSP_CDEV_TYPE_RDONLY },
	{ SND_DEV_DSPHW_VREC,  "dsp",  ".vr", NULL, 1, 1, SND_MAXVCHANS, 1,
	  SND_FORMAT(AFMT_S16_LE, 2, 0), 48000, DSP_CDEV_TYPE_RDONLY },
	{ SND_DEV_DSPHW_CD,    "dspcd",  ".", NULL, 0, 0, 0, 0,
	  SND_FORMAT(AFMT_S16_LE, 2, 0), 44100, DSP_CDEV_TYPE_RDWR   },
	/* Low priority, OSSv4 aliases. */
	{ SND_DEV_DSP,      "dsp_ac3",   ".", "dsp", 0, 0, 0, 0,
	  SND_FORMAT(AFMT_U8, 1, 0),     DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
	{ SND_DEV_DSP,     "dsp_mmap",   ".", "dsp", 0, 0, 0, 0,
	  SND_FORMAT(AFMT_U8, 1, 0),     DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
	{ SND_DEV_DSP,  "dsp_multich",   ".", "dsp", 0, 0, 0, 0,
	  SND_FORMAT(AFMT_U8, 1, 0),     DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
	{ SND_DEV_DSP, "dsp_spdifout",   ".", "dsp", 0, 0, 0, 0,
	  SND_FORMAT(AFMT_U8, 1, 0),     DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
	{ SND_DEV_DSP,  "dsp_spdifin",   ".", "dsp", 0, 0, 0, 0,
	  SND_FORMAT(AFMT_U8, 1, 0),     DSP_DEFAULT_SPEED,
	  DSP_CDEV_TYPE_RDWR },
};

static void
dsp_close(void *data)
{
	struct dsp_cdevpriv *priv = data;
	struct pcm_channel *rdch, *wrch, *volch;
	struct snddev_info *d;
	int sg_ids, rdref, wdref;

	if (priv == NULL)
		return;

	d = priv->sc;
	/* At this point pcm_unregister() will destroy all channels anyway. */
	if (PCM_DETACHING(d))
		goto skip;

	PCM_GIANT_ENTER(d);

	PCM_LOCK(d);
	PCM_WAIT(d);
	PCM_ACQUIRE(d);

	rdch = priv->rdch;
	wrch = priv->wrch;
	volch = priv->volch;

	rdref = -1;
	wdref = -1;

	if (volch != NULL) {
		if (volch == rdch)
			rdref--;
		else if (volch == wrch)
			wdref--;
		else {
			CHN_LOCK(volch);
			pcm_chnref(volch, -1);
			CHN_UNLOCK(volch);
		}
	}

	if (rdch != NULL)
		CHN_REMOVE(d, rdch, channels.pcm.opened);
	if (wrch != NULL)
		CHN_REMOVE(d, wrch, channels.pcm.opened);

	if (rdch != NULL || wrch != NULL) {
		PCM_UNLOCK(d);
		if (rdch != NULL) {
			/*
			 * The channel itself need not be locked because:
			 *   a)  Adding a channel to a syncgroup happens only
			 *       in dsp_ioctl(), which cannot run concurrently
			 *       to dsp_close().
			 *   b)  The syncmember pointer (sm) is protected by
			 *       the global syncgroup list lock.
			 *   c)  A channel can't just disappear, invalidating
			 *       pointers, unless it's closed/dereferenced
			 *       first.
			 */
			PCM_SG_LOCK();
			sg_ids = chn_syncdestroy(rdch);
			PCM_SG_UNLOCK();
			if (sg_ids != 0)
				free_unr(pcmsg_unrhdr, sg_ids);

			CHN_LOCK(rdch);
			pcm_chnref(rdch, rdref);
			chn_abort(rdch); /* won't sleep */
			rdch->flags &= ~(CHN_F_RUNNING | CHN_F_MMAP |
			    CHN_F_DEAD | CHN_F_EXCLUSIVE);
			chn_reset(rdch, 0, 0);
			pcm_chnrelease(rdch);
		}
		if (wrch != NULL) {
			/*
			 * Please see block above.
			 */
			PCM_SG_LOCK();
			sg_ids = chn_syncdestroy(wrch);
			PCM_SG_UNLOCK();
			if (sg_ids != 0)
				free_unr(pcmsg_unrhdr, sg_ids);

			CHN_LOCK(wrch);
			pcm_chnref(wrch, wdref);
			chn_flush(wrch); /* may sleep */
			wrch->flags &= ~(CHN_F_RUNNING | CHN_F_MMAP |
			    CHN_F_DEAD | CHN_F_EXCLUSIVE);
			chn_reset(wrch, 0, 0);
			pcm_chnrelease(wrch);
		}
		PCM_LOCK(d);
	}

	PCM_RELEASE(d);
	PCM_UNLOCK(d);

	PCM_GIANT_LEAVE(d);
skip:
	free(priv, M_DEVBUF);
	priv = NULL;
}

#define DSP_FIXUP_ERROR()		do {				\
	prio = pcm_getflags(d->dev);					\
	if (!DSP_F_VALID(flags))					\
		error = EINVAL;						\
	if (!DSP_F_DUPLEX(flags) &&					\
	    ((DSP_F_READ(flags) && d->reccount == 0) ||			\
	    (DSP_F_WRITE(flags) && d->playcount == 0)))			\
		error = ENOTSUP;					\
	else if (!DSP_F_DUPLEX(flags) && (prio & SD_F_SIMPLEX) &&	\
	    ((DSP_F_READ(flags) && (prio & SD_F_PRIO_WR)) ||		\
	    (DSP_F_WRITE(flags) && (prio & SD_F_PRIO_RD))))		\
		error = EBUSY;						\
} while (0)

static int
dsp_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct dsp_cdevpriv *priv;
	struct pcm_channel *rdch, *wrch;
	struct snddev_info *d;
	uint32_t fmt, spd, prio;
	int error, rderror, wrerror;

	/* Kind of impossible.. */
	if (i_dev == NULL || td == NULL)
		return (ENODEV);

	d = i_dev->si_drv1;
	if (PCM_DETACHING(d) || !PCM_REGISTERED(d))
		return (EBADF);

	priv = malloc(sizeof(*priv), M_DEVBUF, M_WAITOK | M_ZERO);
	priv->sc = d;
	priv->rdch = NULL;
	priv->wrch = NULL;
	priv->volch = NULL;
	priv->simplex = (pcm_getflags(d->dev) & SD_F_SIMPLEX) ? 1 : 0;

	error = devfs_set_cdevpriv(priv, dsp_close);
	if (error != 0)
		return (error);

	PCM_GIANT_ENTER(d);

	/* Lock snddev so nobody else can monkey with it. */
	PCM_LOCK(d);
	PCM_WAIT(d);

	error = 0;
	DSP_FIXUP_ERROR();
	if (error != 0) {
		PCM_UNLOCK(d);
		PCM_GIANT_EXIT(d);
		return (error);
	}

	/*
	 * That is just enough. Acquire and unlock pcm lock so
	 * the other will just have to wait until we finish doing
	 * everything.
	 */
	PCM_ACQUIRE(d);
	PCM_UNLOCK(d);

	fmt = SND_FORMAT(AFMT_U8, 1, 0);
	spd = DSP_DEFAULT_SPEED;

	rdch = NULL;
	wrch = NULL;
	rderror = 0;
	wrerror = 0;

	if (DSP_F_READ(flags)) {
		/* open for read */
		rderror = pcm_chnalloc(d, &rdch, PCMDIR_REC,
		    td->td_proc->p_pid, td->td_proc->p_comm, -1);

		if (rderror == 0 && chn_reset(rdch, fmt, spd) != 0)
			rderror = ENXIO;

		if (rderror != 0) {
			if (rdch != NULL)
				pcm_chnrelease(rdch);
			if (!DSP_F_DUPLEX(flags)) {
				PCM_RELEASE_QUICK(d);
				PCM_GIANT_EXIT(d);
				return (rderror);
			}
			rdch = NULL;
		} else {
			if (flags & O_NONBLOCK)
				rdch->flags |= CHN_F_NBIO;
			if (flags & O_EXCL)
				rdch->flags |= CHN_F_EXCLUSIVE;
			pcm_chnref(rdch, 1);
			chn_vpc_reset(rdch, SND_VOL_C_PCM, 0);
		 	CHN_UNLOCK(rdch);
		}
	}

	if (DSP_F_WRITE(flags)) {
		/* open for write */
		wrerror = pcm_chnalloc(d, &wrch, PCMDIR_PLAY,
		    td->td_proc->p_pid, td->td_proc->p_comm, -1);

		if (wrerror == 0 && chn_reset(wrch, fmt, spd) != 0)
			wrerror = ENXIO;

		if (wrerror != 0) {
			if (wrch != NULL)
				pcm_chnrelease(wrch);
			if (!DSP_F_DUPLEX(flags)) {
				if (rdch != NULL) {
					/*
					 * Lock, deref and release previously
					 * created record channel
					 */
					CHN_LOCK(rdch);
					pcm_chnref(rdch, -1);
					pcm_chnrelease(rdch);
				}
				PCM_RELEASE_QUICK(d);
				PCM_GIANT_EXIT(d);
				return (wrerror);
			}
			wrch = NULL;
		} else {
			if (flags & O_NONBLOCK)
				wrch->flags |= CHN_F_NBIO;
			if (flags & O_EXCL)
				wrch->flags |= CHN_F_EXCLUSIVE;
			pcm_chnref(wrch, 1);
			chn_vpc_reset(wrch, SND_VOL_C_PCM, 0);
			CHN_UNLOCK(wrch);
		}
	}

	PCM_LOCK(d);

	if (wrch == NULL && rdch == NULL) {
		PCM_RELEASE(d);
		PCM_UNLOCK(d);
		PCM_GIANT_EXIT(d);
		if (wrerror != 0)
			return (wrerror);
		if (rderror != 0)
			return (rderror);
		return (EINVAL);
	}
	if (rdch != NULL)
		CHN_INSERT_HEAD(d, rdch, channels.pcm.opened);
	if (wrch != NULL)
		CHN_INSERT_HEAD(d, wrch, channels.pcm.opened);
	priv->rdch = rdch;
	priv->wrch = wrch;

	PCM_RELEASE(d);
	PCM_UNLOCK(d);

	PCM_GIANT_LEAVE(d);

	return (0);
}

static __inline int
dsp_io_ops(struct dsp_cdevpriv *priv, struct uio *buf)
{
	struct snddev_info *d;
	struct pcm_channel **ch;
	int (*chn_io)(struct pcm_channel *, struct uio *);
	int prio, ret;
	pid_t runpid;

	KASSERT(buf != NULL &&
	    (buf->uio_rw == UIO_READ || buf->uio_rw == UIO_WRITE),
	    ("%s(): io train wreck!", __func__));

	d = priv->sc;
	if (PCM_DETACHING(d) || !DSP_REGISTERED(d))
		return (EBADF);

	PCM_GIANT_ENTER(d);

	switch (buf->uio_rw) {
	case UIO_READ:
		prio = SD_F_PRIO_RD;
		ch = &priv->rdch;
		chn_io = chn_read;
		break;
	case UIO_WRITE:
		prio = SD_F_PRIO_WR;
		ch = &priv->wrch;
		chn_io = chn_write;
		break;
	default:
		panic("invalid/corrupted uio direction: %d", buf->uio_rw);
		break;
	}

	runpid = buf->uio_td->td_proc->p_pid;

	getchns(priv, prio);

	if (*ch == NULL || !((*ch)->flags & CHN_F_BUSY)) {
		if (priv->rdch != NULL || priv->wrch != NULL)
			relchns(priv, prio);
		PCM_GIANT_EXIT(d);
		return (EBADF);
	}

	if (((*ch)->flags & (CHN_F_MMAP | CHN_F_DEAD)) ||
	    (((*ch)->flags & CHN_F_RUNNING) && (*ch)->pid != runpid)) {
		relchns(priv, prio);
		PCM_GIANT_EXIT(d);
		return (EINVAL);
	} else if (!((*ch)->flags & CHN_F_RUNNING)) {
		(*ch)->flags |= CHN_F_RUNNING;
		(*ch)->pid = runpid;
	}

	/*
	 * chn_read/write must give up channel lock in order to copy bytes
	 * from/to userland, so up the "in progress" counter to make sure
	 * someone else doesn't come along and muss up the buffer.
	 */
	++(*ch)->inprog;
	ret = chn_io(*ch, buf);
	--(*ch)->inprog;

	CHN_BROADCAST(&(*ch)->cv);

	relchns(priv, prio);

	PCM_GIANT_LEAVE(d);

	return (ret);
}

static int
dsp_read(struct cdev *i_dev, struct uio *buf, int flag)
{
	struct dsp_cdevpriv *priv;
	int err;

	if ((err = devfs_get_cdevpriv((void **)&priv)) != 0)
		return (err);
	return (dsp_io_ops(priv, buf));
}

static int
dsp_write(struct cdev *i_dev, struct uio *buf, int flag)
{
	struct dsp_cdevpriv *priv;
	int err;

	if ((err = devfs_get_cdevpriv((void **)&priv)) != 0)
		return (err);
	return (dsp_io_ops(priv, buf));
}

static int
dsp_get_volume_channel(struct dsp_cdevpriv *priv, struct pcm_channel **volch)
{
	struct snddev_info *d;
	struct pcm_channel *c;
	int unit;

	KASSERT(volch != NULL,
	    ("%s(): NULL query priv=%p volch=%p", __func__, priv, volch));

	d = priv->sc;
	if (!PCM_REGISTERED(d)) {
		*volch = NULL;
		return (EINVAL);
	}

	PCM_UNLOCKASSERT(d);

	*volch = NULL;

	c = priv->volch;
	if (c != NULL) {
		if (!(c->feederflags & (1 << FEEDER_VOLUME)))
			return (-1);
		*volch = c;
		return (0);
	}

	PCM_LOCK(d);
	PCM_WAIT(d);
	PCM_ACQUIRE(d);

	unit = dev2unit(d->dsp_dev);

	CHN_FOREACH(c, d, channels.pcm) {
		CHN_LOCK(c);
		if (c->unit != unit) {
			CHN_UNLOCK(c);
			continue;
		}
		*volch = c;
		pcm_chnref(c, 1);
		priv->volch = c;
		CHN_UNLOCK(c);
		PCM_RELEASE(d);
		PCM_UNLOCK(d);
		return ((c->feederflags & (1 << FEEDER_VOLUME)) ? 0 : -1);
	}

	PCM_RELEASE(d);
	PCM_UNLOCK(d);

	return (EINVAL);
}

static int
dsp_ioctl_channel(struct dsp_cdevpriv *priv, struct pcm_channel *volch,
    u_long cmd, caddr_t arg)
{
	struct snddev_info *d;
	struct pcm_channel *rdch, *wrch;
	int j, devtype, ret;
	int left, right, center, mute;

	d = priv->sc;
	if (!PCM_REGISTERED(d) || !(pcm_getflags(d->dev) & SD_F_VPC))
		return (-1);

	PCM_UNLOCKASSERT(d);

	j = cmd & 0xff;

	rdch = priv->rdch;
	wrch = priv->wrch;

	/* No specific channel, look into cache */
	if (volch == NULL)
		volch = priv->volch;

	/* Look harder */
	if (volch == NULL) {
		if (j == SOUND_MIXER_RECLEV && rdch != NULL)
			volch = rdch;
		else if (j == SOUND_MIXER_PCM && wrch != NULL)
			volch = wrch;
	}

	devtype = PCMDEV(d->dsp_dev);

	/* Look super harder */
	if (volch == NULL &&
	    (devtype == SND_DEV_DSPHW_PLAY || devtype == SND_DEV_DSPHW_VPLAY ||
	    devtype == SND_DEV_DSPHW_REC || devtype == SND_DEV_DSPHW_VREC)) {
		ret = dsp_get_volume_channel(priv, &volch);
		if (ret != 0)
			return (ret);
		if (volch == NULL)
			return (EINVAL);
	}

	/* Final validation */
	if (volch == NULL)
		return (EINVAL);

	CHN_LOCK(volch);
	if (!(volch->feederflags & (1 << FEEDER_VOLUME))) {
		CHN_UNLOCK(volch);
		return (EINVAL);
	}

	switch (cmd & ~0xff) {
	case MIXER_WRITE(0):
		switch (j) {
		case SOUND_MIXER_MUTE:
			if (volch->direction == PCMDIR_REC) {
				chn_setmute_multi(volch, SND_VOL_C_PCM, (*(int *)arg & SOUND_MASK_RECLEV) != 0);
			} else {
				chn_setmute_multi(volch, SND_VOL_C_PCM, (*(int *)arg & SOUND_MASK_PCM) != 0);
			}
			break;
		case SOUND_MIXER_PCM:
			if (volch->direction != PCMDIR_PLAY)
				break;
			left = *(int *)arg & 0x7f;
			right = ((*(int *)arg) >> 8) & 0x7f;
			center = (left + right) >> 1;
			chn_setvolume_multi(volch, SND_VOL_C_PCM,
			    left, right, center);
			break;
		case SOUND_MIXER_RECLEV:
			if (volch->direction != PCMDIR_REC)
				break;
			left = *(int *)arg & 0x7f;
			right = ((*(int *)arg) >> 8) & 0x7f;
			center = (left + right) >> 1;
			chn_setvolume_multi(volch, SND_VOL_C_PCM,
			    left, right, center);
			break;
		default:
			/* ignore all other mixer writes */
			break;
		}
		break;

	case MIXER_READ(0):
		switch (j) {
		case SOUND_MIXER_MUTE:
			mute = CHN_GETMUTE(volch, SND_VOL_C_PCM, SND_CHN_T_FL) ||
			    CHN_GETMUTE(volch, SND_VOL_C_PCM, SND_CHN_T_FR);
			if (volch->direction == PCMDIR_REC) {
				*(int *)arg = mute << SOUND_MIXER_RECLEV;
			} else {
				*(int *)arg = mute << SOUND_MIXER_PCM;
			}
			break;
		case SOUND_MIXER_PCM:
			if (volch->direction != PCMDIR_PLAY)
				break;
			*(int *)arg = CHN_GETVOLUME(volch,
			    SND_VOL_C_PCM, SND_CHN_T_FL);
			*(int *)arg |= CHN_GETVOLUME(volch,
			    SND_VOL_C_PCM, SND_CHN_T_FR) << 8;
			break;
		case SOUND_MIXER_RECLEV:
			if (volch->direction != PCMDIR_REC)
				break;
			*(int *)arg = CHN_GETVOLUME(volch,
			    SND_VOL_C_PCM, SND_CHN_T_FL);
			*(int *)arg |= CHN_GETVOLUME(volch,
			    SND_VOL_C_PCM, SND_CHN_T_FR) << 8;
			break;
		case SOUND_MIXER_DEVMASK:
		case SOUND_MIXER_CAPS:
		case SOUND_MIXER_STEREODEVS:
			if (volch->direction == PCMDIR_REC)
				*(int *)arg = SOUND_MASK_RECLEV;
			else
				*(int *)arg = SOUND_MASK_PCM;
			break;
		default:
			*(int *)arg = 0;
			break;
		}
		break;

	default:
		break;
	}
	CHN_UNLOCK(volch);
	return (0);
}

static int
dsp_ioctl(struct cdev *i_dev, u_long cmd, caddr_t arg, int mode,
    struct thread *td)
{
	struct dsp_cdevpriv *priv;
    	struct pcm_channel *chn, *rdch, *wrch;
	struct snddev_info *d;
	u_long xcmd;
	int *arg_i, ret, tmp, err;

	if ((err = devfs_get_cdevpriv((void **)&priv)) != 0)
		return (err);

	d = priv->sc;
	if (PCM_DETACHING(d) || !DSP_REGISTERED(d))
		return (EBADF);

	PCM_GIANT_ENTER(d);

	arg_i = (int *)arg;
	ret = 0;
	xcmd = 0;
	chn = NULL;

	if (IOCGROUP(cmd) == 'M') {
		if (cmd == OSS_GETVERSION) {
			*arg_i = SOUND_VERSION;
			PCM_GIANT_EXIT(d);
			return (0);
		}
		ret = dsp_ioctl_channel(priv, priv->volch, cmd, arg);
		if (ret != -1) {
			PCM_GIANT_EXIT(d);
			return (ret);
		}

		if (d->mixer_dev != NULL) {
			PCM_ACQUIRE_QUICK(d);
			ret = mixer_ioctl_cmd(d->mixer_dev, cmd, arg, -1, td,
			    MIXER_CMD_DIRECT);
			PCM_RELEASE_QUICK(d);
		} else
			ret = EBADF;

		PCM_GIANT_EXIT(d);

		return (ret);
	}

	/*
	 * Certain ioctls may be made on any type of device (audio, mixer,
	 * and MIDI).  Handle those special cases here.
	 */
	if (IOCGROUP(cmd) == 'X') {
		PCM_ACQUIRE_QUICK(d);
		switch(cmd) {
		case SNDCTL_SYSINFO:
			sound_oss_sysinfo((oss_sysinfo *)arg);
			break;
		case SNDCTL_CARDINFO:
			ret = sound_oss_card_info((oss_card_info *)arg);
			break;
		case SNDCTL_AUDIOINFO:
		case SNDCTL_AUDIOINFO_EX:
		case SNDCTL_ENGINEINFO:
			ret = dsp_oss_audioinfo(i_dev, (oss_audioinfo *)arg);
			break;
		case SNDCTL_MIXERINFO:
			ret = mixer_oss_mixerinfo(i_dev, (oss_mixerinfo *)arg);
			break;
		default:
			ret = EINVAL;
		}
		PCM_RELEASE_QUICK(d);
		PCM_GIANT_EXIT(d);
		return (ret);
	}

	getchns(priv, 0);
	rdch = priv->rdch;
	wrch = priv->wrch;

	if (wrch != NULL && (wrch->flags & CHN_F_DEAD))
		wrch = NULL;
	if (rdch != NULL && (rdch->flags & CHN_F_DEAD))
		rdch = NULL;

	if (wrch == NULL && rdch == NULL) {
		PCM_GIANT_EXIT(d);
		return (EINVAL);
	}

    	switch(cmd) {
#ifdef OLDPCM_IOCTL
    	/*
     	 * we start with the new ioctl interface.
     	 */
    	case AIONWRITE:	/* how many bytes can write ? */
		if (wrch) {
			CHN_LOCK(wrch);
/*
		if (wrch && wrch->bufhard.dl)
			while (chn_wrfeed(wrch) == 0);
*/
			*arg_i = sndbuf_getfree(wrch->bufsoft);
			CHN_UNLOCK(wrch);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case AIOSSIZE:     /* set the current blocksize */
		{
	    		struct snd_size *p = (struct snd_size *)arg;

			p->play_size = 0;
			p->rec_size = 0;
			PCM_ACQUIRE_QUICK(d);
	    		if (wrch) {
				CHN_LOCK(wrch);
				chn_setblocksize(wrch, 2, p->play_size);
				p->play_size = sndbuf_getblksz(wrch->bufsoft);
				CHN_UNLOCK(wrch);
			}
	    		if (rdch) {
				CHN_LOCK(rdch);
				chn_setblocksize(rdch, 2, p->rec_size);
				p->rec_size = sndbuf_getblksz(rdch->bufsoft);
				CHN_UNLOCK(rdch);
			}
			PCM_RELEASE_QUICK(d);
		}
		break;
    	case AIOGSIZE:	/* get the current blocksize */
		{
	    		struct snd_size *p = (struct snd_size *)arg;

	    		if (wrch) {
				CHN_LOCK(wrch);
				p->play_size = sndbuf_getblksz(wrch->bufsoft);
				CHN_UNLOCK(wrch);
			}
	    		if (rdch) {
				CHN_LOCK(rdch);
				p->rec_size = sndbuf_getblksz(rdch->bufsoft);
				CHN_UNLOCK(rdch);
			}
		}
		break;

    	case AIOSFMT:
    	case AIOGFMT:
		{
	    		snd_chan_param *p = (snd_chan_param *)arg;

			if (cmd == AIOSFMT &&
			    ((p->play_format != 0 && p->play_rate == 0) ||
			    (p->rec_format != 0 && p->rec_rate == 0))) {
				ret = EINVAL;
				break;
			}
			PCM_ACQUIRE_QUICK(d);
	    		if (wrch) {
				CHN_LOCK(wrch);
				if (cmd == AIOSFMT && p->play_format != 0) {
					chn_setformat(wrch,
					    SND_FORMAT(p->play_format,
					    AFMT_CHANNEL(wrch->format),
					    AFMT_EXTCHANNEL(wrch->format)));
					chn_setspeed(wrch, p->play_rate);
				}
	    			p->play_rate = wrch->speed;
	    			p->play_format = AFMT_ENCODING(wrch->format);
				CHN_UNLOCK(wrch);
			} else {
	    			p->play_rate = 0;
	    			p->play_format = 0;
	    		}
	    		if (rdch) {
				CHN_LOCK(rdch);
				if (cmd == AIOSFMT && p->rec_format != 0) {
					chn_setformat(rdch,
					    SND_FORMAT(p->rec_format,
					    AFMT_CHANNEL(rdch->format),
					    AFMT_EXTCHANNEL(rdch->format)));
					chn_setspeed(rdch, p->rec_rate);
				}
				p->rec_rate = rdch->speed;
				p->rec_format = AFMT_ENCODING(rdch->format);
				CHN_UNLOCK(rdch);
			} else {
	    			p->rec_rate = 0;
	    			p->rec_format = 0;
	    		}
			PCM_RELEASE_QUICK(d);
		}
		break;

    	case AIOGCAP:     /* get capabilities */
		{
	    		snd_capabilities *p = (snd_capabilities *)arg;
			struct pcmchan_caps *pcaps = NULL, *rcaps = NULL;
			struct cdev *pdev;

			PCM_LOCK(d);
			if (rdch) {
				CHN_LOCK(rdch);
				rcaps = chn_getcaps(rdch);
			}
			if (wrch) {
				CHN_LOCK(wrch);
				pcaps = chn_getcaps(wrch);
			}
	    		p->rate_min = max(rcaps? rcaps->minspeed : 0,
	                      		  pcaps? pcaps->minspeed : 0);
	    		p->rate_max = min(rcaps? rcaps->maxspeed : 1000000,
	                      		  pcaps? pcaps->maxspeed : 1000000);
	    		p->bufsize = min(rdch? sndbuf_getsize(rdch->bufsoft) : 1000000,
	                     		 wrch? sndbuf_getsize(wrch->bufsoft) : 1000000);
			/* XXX bad on sb16 */
	    		p->formats = (rdch? chn_getformats(rdch) : 0xffffffff) &
			 	     (wrch? chn_getformats(wrch) : 0xffffffff);
			if (rdch && wrch) {
				p->formats |=
				    (pcm_getflags(d->dev) & SD_F_SIMPLEX) ? 0 :
				    AFMT_FULLDUPLEX;
			}
			pdev = d->mixer_dev;
	    		p->mixers = 1; /* default: one mixer */
	    		p->inputs = pdev->si_drv1? mix_getdevs(pdev->si_drv1) : 0;
	    		p->left = p->right = 100;
			if (wrch)
				CHN_UNLOCK(wrch);
			if (rdch)
				CHN_UNLOCK(rdch);
			PCM_UNLOCK(d);
		}
		break;

    	case AIOSTOP:
		if (*arg_i == AIOSYNC_PLAY && wrch) {
			CHN_LOCK(wrch);
			*arg_i = chn_abort(wrch);
			CHN_UNLOCK(wrch);
		} else if (*arg_i == AIOSYNC_CAPTURE && rdch) {
			CHN_LOCK(rdch);
			*arg_i = chn_abort(rdch);
			CHN_UNLOCK(rdch);
		} else {
	   	 	printf("AIOSTOP: bad channel 0x%x\n", *arg_i);
	    		*arg_i = 0;
		}
		break;

    	case AIOSYNC:
		printf("AIOSYNC chan 0x%03lx pos %lu unimplemented\n",
	    		((snd_sync_parm *)arg)->chan, ((snd_sync_parm *)arg)->pos);
		break;
#endif
	/*
	 * here follow the standard ioctls (filio.h etc.)
	 */
    	case FIONREAD: /* get # bytes to read */
		if (rdch) {
			CHN_LOCK(rdch);
/*			if (rdch && rdch->bufhard.dl)
				while (chn_rdfeed(rdch) == 0);
*/
			*arg_i = sndbuf_getready(rdch->bufsoft);
			CHN_UNLOCK(rdch);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case FIOASYNC: /*set/clear async i/o */
		DEB( printf("FIOASYNC\n") ; )
		break;

    	case SNDCTL_DSP_NONBLOCK: /* set non-blocking i/o */
    	case FIONBIO: /* set/clear non-blocking i/o */
		if (rdch) {
			CHN_LOCK(rdch);
			if (cmd == SNDCTL_DSP_NONBLOCK || *arg_i)
				rdch->flags |= CHN_F_NBIO;
			else
				rdch->flags &= ~CHN_F_NBIO;
			CHN_UNLOCK(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
			if (cmd == SNDCTL_DSP_NONBLOCK || *arg_i)
				wrch->flags |= CHN_F_NBIO;
			else
				wrch->flags &= ~CHN_F_NBIO;
			CHN_UNLOCK(wrch);
		}
		break;

    	/*
	 * Finally, here is the linux-compatible ioctl interface
	 */
#define THE_REAL_SNDCTL_DSP_GETBLKSIZE _IOWR('P', 4, int)
    	case THE_REAL_SNDCTL_DSP_GETBLKSIZE:
    	case SNDCTL_DSP_GETBLKSIZE:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = sndbuf_getblksz(chn->bufsoft);
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_SETBLKSIZE:
		RANGE(*arg_i, 16, 65536);
		PCM_ACQUIRE_QUICK(d);
		if (wrch) {
			CHN_LOCK(wrch);
			chn_setblocksize(wrch, 2, *arg_i);
			CHN_UNLOCK(wrch);
		}
		if (rdch) {
			CHN_LOCK(rdch);
			chn_setblocksize(rdch, 2, *arg_i);
			CHN_UNLOCK(rdch);
		}
		PCM_RELEASE_QUICK(d);
		break;

    	case SNDCTL_DSP_RESET:
		DEB(printf("dsp reset\n"));
		if (wrch) {
			CHN_LOCK(wrch);
			chn_abort(wrch);
			chn_resetbuf(wrch);
			CHN_UNLOCK(wrch);
		}
		if (rdch) {
			CHN_LOCK(rdch);
			chn_abort(rdch);
			chn_resetbuf(rdch);
			CHN_UNLOCK(rdch);
		}
		break;

    	case SNDCTL_DSP_SYNC:
		DEB(printf("dsp sync\n"));
		/* chn_sync may sleep */
		if (wrch) {
			CHN_LOCK(wrch);
			chn_sync(wrch, 0);
			CHN_UNLOCK(wrch);
		}
		break;

    	case SNDCTL_DSP_SPEED:
		/* chn_setspeed may sleep */
		tmp = 0;
		PCM_ACQUIRE_QUICK(d);
		if (wrch) {
			CHN_LOCK(wrch);
			ret = chn_setspeed(wrch, *arg_i);
			tmp = wrch->speed;
			CHN_UNLOCK(wrch);
		}
		if (rdch && ret == 0) {
			CHN_LOCK(rdch);
			ret = chn_setspeed(rdch, *arg_i);
			if (tmp == 0)
				tmp = rdch->speed;
			CHN_UNLOCK(rdch);
		}
		PCM_RELEASE_QUICK(d);
		*arg_i = tmp;
		break;

    	case SOUND_PCM_READ_RATE:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = chn->speed;
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_STEREO:
		tmp = -1;
		*arg_i = (*arg_i)? 2 : 1;
		PCM_ACQUIRE_QUICK(d);
		if (wrch) {
			CHN_LOCK(wrch);
			ret = chn_setformat(wrch,
			    SND_FORMAT(wrch->format, *arg_i, 0));
			tmp = (AFMT_CHANNEL(wrch->format) > 1)? 1 : 0;
			CHN_UNLOCK(wrch);
		}
		if (rdch && ret == 0) {
			CHN_LOCK(rdch);
			ret = chn_setformat(rdch,
			    SND_FORMAT(rdch->format, *arg_i, 0));
			if (tmp == -1)
				tmp = (AFMT_CHANNEL(rdch->format) > 1)? 1 : 0;
			CHN_UNLOCK(rdch);
		}
		PCM_RELEASE_QUICK(d);
		*arg_i = tmp;
		break;

    	case SOUND_PCM_WRITE_CHANNELS:
/*	case SNDCTL_DSP_CHANNELS: ( == SOUND_PCM_WRITE_CHANNELS) */
		if (*arg_i < 0 || *arg_i > AFMT_CHANNEL_MAX) {
			*arg_i = 0;
			ret = EINVAL;
			break;
		}
		if (*arg_i != 0) {
			uint32_t ext = 0;

			tmp = 0;
			/*
			 * Map channel number to surround sound formats.
			 * Devices that need bitperfect mode to operate
			 * (e.g. more than SND_CHN_MAX channels) are not
			 * subject to any mapping.
			 */
			if (!(pcm_getflags(d->dev) & SD_F_BITPERFECT)) {
				struct pcmchan_matrix *m;

				if (*arg_i > SND_CHN_MAX)
					*arg_i = SND_CHN_MAX;

				m = feeder_matrix_default_channel_map(*arg_i);
				if (m != NULL)
					ext = m->ext;
			}

			PCM_ACQUIRE_QUICK(d);
	  		if (wrch) {
				CHN_LOCK(wrch);
				ret = chn_setformat(wrch,
				    SND_FORMAT(wrch->format, *arg_i, ext));
				tmp = AFMT_CHANNEL(wrch->format);
				CHN_UNLOCK(wrch);
			}
			if (rdch && ret == 0) {
				CHN_LOCK(rdch);
				ret = chn_setformat(rdch,
				    SND_FORMAT(rdch->format, *arg_i, ext));
				if (tmp == 0)
					tmp = AFMT_CHANNEL(rdch->format);
				CHN_UNLOCK(rdch);
			}
			PCM_RELEASE_QUICK(d);
			*arg_i = tmp;
		} else {
			chn = wrch ? wrch : rdch;
			CHN_LOCK(chn);
			*arg_i = AFMT_CHANNEL(chn->format);
			CHN_UNLOCK(chn);
		}
		break;

    	case SOUND_PCM_READ_CHANNELS:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = AFMT_CHANNEL(chn->format);
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETFMTS:	/* returns a mask of supported fmts */
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = chn_getformats(chn);
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_SETFMT:	/* sets _one_ format */
		if (*arg_i != AFMT_QUERY) {
			tmp = 0;
			PCM_ACQUIRE_QUICK(d);
			if (wrch) {
				CHN_LOCK(wrch);
				ret = chn_setformat(wrch, SND_FORMAT(*arg_i,
				    AFMT_CHANNEL(wrch->format),
				    AFMT_EXTCHANNEL(wrch->format)));
				tmp = wrch->format;
				CHN_UNLOCK(wrch);
			}
			if (rdch && ret == 0) {
				CHN_LOCK(rdch);
				ret = chn_setformat(rdch, SND_FORMAT(*arg_i,
				    AFMT_CHANNEL(rdch->format),
				    AFMT_EXTCHANNEL(rdch->format)));
				if (tmp == 0)
					tmp = rdch->format;
				CHN_UNLOCK(rdch);
			}
			PCM_RELEASE_QUICK(d);
			*arg_i = AFMT_ENCODING(tmp);
		} else {
			chn = wrch ? wrch : rdch;
			CHN_LOCK(chn);
			*arg_i = AFMT_ENCODING(chn->format);
			CHN_UNLOCK(chn);
		}
		break;

    	case SNDCTL_DSP_SETFRAGMENT:
		DEB(printf("SNDCTL_DSP_SETFRAGMENT 0x%08x\n", *(int *)arg));
		{
			uint32_t fragln = (*arg_i) & 0x0000ffff;
			uint32_t maxfrags = ((*arg_i) & 0xffff0000) >> 16;
			uint32_t fragsz;
			uint32_t r_maxfrags, r_fragsz;

			RANGE(fragln, 4, 16);
			fragsz = 1 << fragln;

			if (maxfrags == 0)
				maxfrags = CHN_2NDBUFMAXSIZE / fragsz;
			if (maxfrags < 2)
				maxfrags = 2;
			if (maxfrags * fragsz > CHN_2NDBUFMAXSIZE)
				maxfrags = CHN_2NDBUFMAXSIZE / fragsz;

			DEB(printf("SNDCTL_DSP_SETFRAGMENT %d frags, %d sz\n", maxfrags, fragsz));
			PCM_ACQUIRE_QUICK(d);
		    	if (rdch) {
				CHN_LOCK(rdch);
				ret = chn_setblocksize(rdch, maxfrags, fragsz);
				r_maxfrags = sndbuf_getblkcnt(rdch->bufsoft);
				r_fragsz = sndbuf_getblksz(rdch->bufsoft);
				CHN_UNLOCK(rdch);
			} else {
				r_maxfrags = maxfrags;
				r_fragsz = fragsz;
			}
		    	if (wrch && ret == 0) {
				CHN_LOCK(wrch);
				ret = chn_setblocksize(wrch, maxfrags, fragsz);
 				maxfrags = sndbuf_getblkcnt(wrch->bufsoft);
				fragsz = sndbuf_getblksz(wrch->bufsoft);
				CHN_UNLOCK(wrch);
			} else { /* use whatever came from the read channel */
				maxfrags = r_maxfrags;
				fragsz = r_fragsz;
			}
			PCM_RELEASE_QUICK(d);

			fragln = 0;
			while (fragsz > 1) {
				fragln++;
				fragsz >>= 1;
			}
	    		*arg_i = (maxfrags << 16) | fragln;
		}
		break;

    	case SNDCTL_DSP_GETISPACE:
		/* return the size of data available in the input queue */
		{
	    		audio_buf_info *a = (audio_buf_info *)arg;
	    		if (rdch) {
	        		struct snd_dbuf *bs = rdch->bufsoft;

				CHN_LOCK(rdch);
				a->bytes = sndbuf_getready(bs);
	        		a->fragments = a->bytes / sndbuf_getblksz(bs);
	        		a->fragstotal = sndbuf_getblkcnt(bs);
	        		a->fragsize = sndbuf_getblksz(bs);
				CHN_UNLOCK(rdch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETOSPACE:
		/* return space available in the output queue */
		{
	    		audio_buf_info *a = (audio_buf_info *)arg;
	    		if (wrch) {
	        		struct snd_dbuf *bs = wrch->bufsoft;

				CHN_LOCK(wrch);
				/* XXX abusive DMA update: chn_wrupdate(wrch); */
				a->bytes = sndbuf_getfree(bs);
	        		a->fragments = a->bytes / sndbuf_getblksz(bs);
	        		a->fragstotal = sndbuf_getblkcnt(bs);
	        		a->fragsize = sndbuf_getblksz(bs);
				CHN_UNLOCK(wrch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETIPTR:
		{
	    		count_info *a = (count_info *)arg;
	    		if (rdch) {
	        		struct snd_dbuf *bs = rdch->bufsoft;

				CHN_LOCK(rdch);
				/* XXX abusive DMA update: chn_rdupdate(rdch); */
	        		a->bytes = sndbuf_gettotal(bs);
	        		a->blocks = sndbuf_getblocks(bs) - rdch->blocks;
	        		a->ptr = sndbuf_getfreeptr(bs);
				rdch->blocks = sndbuf_getblocks(bs);
				CHN_UNLOCK(rdch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETOPTR:
		{
	    		count_info *a = (count_info *)arg;
	    		if (wrch) {
	        		struct snd_dbuf *bs = wrch->bufsoft;

				CHN_LOCK(wrch);
				/* XXX abusive DMA update: chn_wrupdate(wrch); */
	        		a->bytes = sndbuf_gettotal(bs);
	        		a->blocks = sndbuf_getblocks(bs) - wrch->blocks;
	        		a->ptr = sndbuf_getreadyptr(bs);
				wrch->blocks = sndbuf_getblocks(bs);
				CHN_UNLOCK(wrch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETCAPS:
		PCM_LOCK(d);
		*arg_i = PCM_CAP_REALTIME | PCM_CAP_MMAP | PCM_CAP_TRIGGER;
		if (rdch && wrch && !(pcm_getflags(d->dev) & SD_F_SIMPLEX))
			*arg_i |= PCM_CAP_DUPLEX;
		if (rdch && (rdch->flags & CHN_F_VIRTUAL) != 0)
			*arg_i |= PCM_CAP_VIRTUAL;
		if (wrch && (wrch->flags & CHN_F_VIRTUAL) != 0)
			*arg_i |= PCM_CAP_VIRTUAL;
		PCM_UNLOCK(d);
		break;

    	case SOUND_PCM_READ_BITS:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			if (chn->format & AFMT_8BIT)
				*arg_i = 8;
			else if (chn->format & AFMT_16BIT)
				*arg_i = 16;
			else if (chn->format & AFMT_24BIT)
				*arg_i = 24;
			else if (chn->format & AFMT_32BIT)
				*arg_i = 32;
			else
				ret = EINVAL;
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_SETTRIGGER:
		if (rdch) {
			CHN_LOCK(rdch);
			rdch->flags &= ~CHN_F_NOTRIGGER;
		    	if (*arg_i & PCM_ENABLE_INPUT)
				chn_start(rdch, 1);
			else {
				chn_abort(rdch);
				chn_resetbuf(rdch);
				rdch->flags |= CHN_F_NOTRIGGER;
			}
			CHN_UNLOCK(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
			wrch->flags &= ~CHN_F_NOTRIGGER;
		    	if (*arg_i & PCM_ENABLE_OUTPUT)
				chn_start(wrch, 1);
			else {
				chn_abort(wrch);
				chn_resetbuf(wrch);
				wrch->flags |= CHN_F_NOTRIGGER;
			}
			CHN_UNLOCK(wrch);
		}
		break;

    	case SNDCTL_DSP_GETTRIGGER:
		*arg_i = 0;
		if (wrch) {
			CHN_LOCK(wrch);
			if (wrch->flags & CHN_F_TRIGGERED)
				*arg_i |= PCM_ENABLE_OUTPUT;
			CHN_UNLOCK(wrch);
		}
		if (rdch) {
			CHN_LOCK(rdch);
			if (rdch->flags & CHN_F_TRIGGERED)
				*arg_i |= PCM_ENABLE_INPUT;
			CHN_UNLOCK(rdch);
		}
		break;

	case SNDCTL_DSP_GETODELAY:
		if (wrch) {
	        	struct snd_dbuf *bs = wrch->bufsoft;

			CHN_LOCK(wrch);
			/* XXX abusive DMA update: chn_wrupdate(wrch); */
			*arg_i = sndbuf_getready(bs);
			CHN_UNLOCK(wrch);
		} else
			ret = EINVAL;
		break;

    	case SNDCTL_DSP_POST:
		if (wrch) {
			CHN_LOCK(wrch);
			wrch->flags &= ~CHN_F_NOTRIGGER;
			chn_start(wrch, 1);
			CHN_UNLOCK(wrch);
		}
		break;

	case SNDCTL_DSP_SETDUPLEX:
		/*
		 * switch to full-duplex mode if card is in half-duplex
		 * mode and is able to work in full-duplex mode
		 */
		PCM_LOCK(d);
		if (rdch && wrch && (pcm_getflags(d->dev) & SD_F_SIMPLEX))
			pcm_setflags(d->dev, pcm_getflags(d->dev)^SD_F_SIMPLEX);
		PCM_UNLOCK(d);
		break;

	/*
	 * The following four ioctls are simple wrappers around mixer_ioctl
	 * with no further processing.  xcmd is short for "translated
	 * command".
	 */
	case SNDCTL_DSP_GETRECVOL:
		if (xcmd == 0) {
			xcmd = SOUND_MIXER_READ_RECLEV;
			chn = rdch;
		}
		/* FALLTHROUGH */
	case SNDCTL_DSP_SETRECVOL:
		if (xcmd == 0) {
			xcmd = SOUND_MIXER_WRITE_RECLEV;
			chn = rdch;
		}
		/* FALLTHROUGH */
	case SNDCTL_DSP_GETPLAYVOL:
		if (xcmd == 0) {
			xcmd = SOUND_MIXER_READ_PCM;
			chn = wrch;
		}
		/* FALLTHROUGH */
	case SNDCTL_DSP_SETPLAYVOL:
		if (xcmd == 0) {
			xcmd = SOUND_MIXER_WRITE_PCM;
			chn = wrch;
		}

		ret = dsp_ioctl_channel(priv, chn, xcmd, arg);
		if (ret != -1) {
			PCM_GIANT_EXIT(d);
			return (ret);
		}

		if (d->mixer_dev != NULL) {
			PCM_ACQUIRE_QUICK(d);
			ret = mixer_ioctl_cmd(d->mixer_dev, xcmd, arg, -1, td,
			    MIXER_CMD_DIRECT);
			PCM_RELEASE_QUICK(d);
		} else
			ret = ENOTSUP;

		break;

	case SNDCTL_DSP_GET_RECSRC_NAMES:
	case SNDCTL_DSP_GET_RECSRC:
	case SNDCTL_DSP_SET_RECSRC:
		if (d->mixer_dev != NULL) {
			PCM_ACQUIRE_QUICK(d);
			ret = mixer_ioctl_cmd(d->mixer_dev, cmd, arg, -1, td,
			    MIXER_CMD_DIRECT);
			PCM_RELEASE_QUICK(d);
		} else
			ret = ENOTSUP;
		break;

	/*
	 * The following 3 ioctls aren't very useful at the moment.  For
	 * now, only a single channel is associated with a cdev (/dev/dspN
	 * instance), so there's only a single output routing to use (i.e.,
	 * the wrch bound to this cdev).
	 */
	case SNDCTL_DSP_GET_PLAYTGT_NAMES:
		{
			oss_mixer_enuminfo *ei;
			ei = (oss_mixer_enuminfo *)arg;
			ei->dev = 0;
			ei->ctrl = 0;
			ei->version = 0; /* static for now */
			ei->strindex[0] = 0;

			if (wrch != NULL) {
				ei->nvalues = 1;
				strlcpy(ei->strings, wrch->name,
					sizeof(ei->strings));
			} else {
				ei->nvalues = 0;
				ei->strings[0] = '\0';
			}
		}
		break;
	case SNDCTL_DSP_GET_PLAYTGT:
	case SNDCTL_DSP_SET_PLAYTGT:	/* yes, they are the same for now */
		/*
		 * Re: SET_PLAYTGT
		 *   OSSv4: "The value that was accepted by the device will
		 *   be returned back in the variable pointed by the
		 *   argument."
		 */
		if (wrch != NULL)
			*arg_i = 0;
		else
			ret = EINVAL;
		break;

	case SNDCTL_DSP_SILENCE:
	/*
	 * Flush the software (pre-feed) buffer, but try to minimize playback
	 * interruption.  (I.e., record unplayed samples with intent to
	 * restore by SNDCTL_DSP_SKIP.) Intended for application "pause"
	 * functionality.
	 */
		if (wrch == NULL)
			ret = EINVAL;
		else {
			struct snd_dbuf *bs;
			CHN_LOCK(wrch);
			while (wrch->inprog != 0)
				cv_wait(&wrch->cv, wrch->lock);
			bs = wrch->bufsoft;
			if ((bs->shadbuf != NULL) && (sndbuf_getready(bs) > 0)) {
				bs->sl = sndbuf_getready(bs);
				sndbuf_dispose(bs, bs->shadbuf, sndbuf_getready(bs));
				sndbuf_fillsilence(bs);
				chn_start(wrch, 0);
			}
			CHN_UNLOCK(wrch);
		}
		break;

	case SNDCTL_DSP_SKIP:
	/*
	 * OSSv4 docs: "This ioctl call discards all unplayed samples in the
	 * playback buffer by moving the current write position immediately
	 * before the point where the device is currently reading the samples."
	 */
		if (wrch == NULL)
			ret = EINVAL;
		else {
			struct snd_dbuf *bs;
			CHN_LOCK(wrch);
			while (wrch->inprog != 0)
				cv_wait(&wrch->cv, wrch->lock);
			bs = wrch->bufsoft;
			if ((bs->shadbuf != NULL) && (bs->sl > 0)) {
				sndbuf_softreset(bs);
				sndbuf_acquire(bs, bs->shadbuf, bs->sl);
				bs->sl = 0;
				chn_start(wrch, 0);
			}
			CHN_UNLOCK(wrch);
		}
		break;

	case SNDCTL_DSP_CURRENT_OPTR:
	case SNDCTL_DSP_CURRENT_IPTR:
	/**
	 * @note Changing formats resets the buffer counters, which differs
	 * 	 from the 4Front drivers.  However, I don't expect this to be
	 * 	 much of a problem.
	 *
	 * @note In a test where @c CURRENT_OPTR is called immediately after write
	 * 	 returns, this driver is about 32K samples behind whereas
	 * 	 4Front's is about 8K samples behind.  Should determine source
	 * 	 of discrepancy, even if only out of curiosity.
	 *
	 * @todo Actually test SNDCTL_DSP_CURRENT_IPTR.
	 */
		chn = (cmd == SNDCTL_DSP_CURRENT_OPTR) ? wrch : rdch;
		if (chn == NULL) 
			ret = EINVAL;
		else {
			struct snd_dbuf *bs;
			/* int tmp; */

			oss_count_t *oc = (oss_count_t *)arg;

			CHN_LOCK(chn);
			bs = chn->bufsoft;
#if 0
			tmp = (sndbuf_getsize(b) + chn_getptr(chn) - sndbuf_gethwptr(b)) % sndbuf_getsize(b);
			oc->samples = (sndbuf_gettotal(b) + tmp) / sndbuf_getalign(b);
			oc->fifo_samples = (sndbuf_getready(b) - tmp) / sndbuf_getalign(b);
#else
			oc->samples = sndbuf_gettotal(bs) / sndbuf_getalign(bs);
			oc->fifo_samples = sndbuf_getready(bs) / sndbuf_getalign(bs);
#endif
			CHN_UNLOCK(chn);
		}
		break;

	case SNDCTL_DSP_HALT_OUTPUT:
	case SNDCTL_DSP_HALT_INPUT:
		chn = (cmd == SNDCTL_DSP_HALT_OUTPUT) ? wrch : rdch;
		if (chn == NULL)
			ret = EINVAL;
		else {
			CHN_LOCK(chn);
			chn_abort(chn);
			CHN_UNLOCK(chn);
		}
		break;

	case SNDCTL_DSP_LOW_WATER:
	/*
	 * Set the number of bytes required to attract attention by
	 * select/poll.
	 */
		if (wrch != NULL) {
			CHN_LOCK(wrch);
			wrch->lw = (*arg_i > 1) ? *arg_i : 1;
			CHN_UNLOCK(wrch);
		}
		if (rdch != NULL) {
			CHN_LOCK(rdch);
			rdch->lw = (*arg_i > 1) ? *arg_i : 1;
			CHN_UNLOCK(rdch);
		}
		break;

	case SNDCTL_DSP_GETERROR:
	/*
	 * OSSv4 docs:  "All errors and counters will automatically be
	 * cleared to zeroes after the call so each call will return only
	 * the errors that occurred after the previous invocation. ... The
	 * play_underruns and rec_overrun fields are the only useful fields
	 * returned by OSS 4.0."
	 */
		{
			audio_errinfo *ei = (audio_errinfo *)arg;

			bzero((void *)ei, sizeof(*ei));

			if (wrch != NULL) {
				CHN_LOCK(wrch);
				ei->play_underruns = wrch->xruns;
				wrch->xruns = 0;
				CHN_UNLOCK(wrch);
			}
			if (rdch != NULL) {
				CHN_LOCK(rdch);
				ei->rec_overruns = rdch->xruns;
				rdch->xruns = 0;
				CHN_UNLOCK(rdch);
			}
		}
		break;

	case SNDCTL_DSP_SYNCGROUP:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_syncgroup(wrch, rdch, (oss_syncgroup *)arg);
		PCM_RELEASE_QUICK(d);
		break;

	case SNDCTL_DSP_SYNCSTART:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_syncstart(*arg_i);
		PCM_RELEASE_QUICK(d);
		break;

	case SNDCTL_DSP_POLICY:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_policy(wrch, rdch, *arg_i);
		PCM_RELEASE_QUICK(d);
		break;

	case SNDCTL_DSP_COOKEDMODE:
		PCM_ACQUIRE_QUICK(d);
		if (!(pcm_getflags(d->dev) & SD_F_BITPERFECT))
			ret = dsp_oss_cookedmode(wrch, rdch, *arg_i);
		PCM_RELEASE_QUICK(d);
		break;
	case SNDCTL_DSP_GET_CHNORDER:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_getchnorder(wrch, rdch, (unsigned long long *)arg);
		PCM_RELEASE_QUICK(d);
		break;
	case SNDCTL_DSP_SET_CHNORDER:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_setchnorder(wrch, rdch, (unsigned long long *)arg);
		PCM_RELEASE_QUICK(d);
		break;
	case SNDCTL_DSP_GETCHANNELMASK:		/* XXX vlc */
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_getchannelmask(wrch, rdch, (int *)arg);
		PCM_RELEASE_QUICK(d);
		break;
	case SNDCTL_DSP_BIND_CHANNEL:		/* XXX what?!? */
		ret = EINVAL;
		break;
#ifdef	OSSV4_EXPERIMENT
	/*
	 * XXX The following ioctls are not yet supported and just return
	 * EINVAL.
	 */
	case SNDCTL_DSP_GETOPEAKS:
	case SNDCTL_DSP_GETIPEAKS:
		chn = (cmd == SNDCTL_DSP_GETOPEAKS) ? wrch : rdch;
		if (chn == NULL)
			ret = EINVAL;
		else {
			oss_peaks_t *op = (oss_peaks_t *)arg;
			int lpeak, rpeak;

			CHN_LOCK(chn);
			ret = chn_getpeaks(chn, &lpeak, &rpeak);
			if (ret == -1)
				ret = EINVAL;
			else {
				(*op)[0] = lpeak;
				(*op)[1] = rpeak;
			}
			CHN_UNLOCK(chn);
		}
		break;

	/*
	 * XXX Once implemented, revisit this for proper cv protection
	 *     (if necessary).
	 */
	case SNDCTL_GETLABEL:
		ret = dsp_oss_getlabel(wrch, rdch, (oss_label_t *)arg);
		break;
	case SNDCTL_SETLABEL:
		ret = dsp_oss_setlabel(wrch, rdch, (oss_label_t *)arg);
		break;
	case SNDCTL_GETSONG:
		ret = dsp_oss_getsong(wrch, rdch, (oss_longname_t *)arg);
		break;
	case SNDCTL_SETSONG:
		ret = dsp_oss_setsong(wrch, rdch, (oss_longname_t *)arg);
		break;
	case SNDCTL_SETNAME:
		ret = dsp_oss_setname(wrch, rdch, (oss_longname_t *)arg);
		break;
#if 0
	/**
	 * @note The S/PDIF interface ioctls, @c SNDCTL_DSP_READCTL and
	 * @c SNDCTL_DSP_WRITECTL have been omitted at the suggestion of
	 * 4Front Technologies.
	 */
	case SNDCTL_DSP_READCTL:
	case SNDCTL_DSP_WRITECTL:
		ret = EINVAL;
		break;
#endif	/* !0 (explicitly omitted ioctls) */

#endif	/* !OSSV4_EXPERIMENT */
    	case SNDCTL_DSP_MAPINBUF:
    	case SNDCTL_DSP_MAPOUTBUF:
    	case SNDCTL_DSP_SETSYNCRO:
		/* undocumented */

    	case SNDCTL_DSP_SUBDIVIDE:
    	case SOUND_PCM_WRITE_FILTER:
    	case SOUND_PCM_READ_FILTER:
		/* dunno what these do, don't sound important */

    	default:
		DEB(printf("default ioctl fn 0x%08lx fail\n", cmd));
		ret = EINVAL;
		break;
    	}

	PCM_GIANT_LEAVE(d);

    	return (ret);
}

static int
dsp_poll(struct cdev *i_dev, int events, struct thread *td)
{
	struct dsp_cdevpriv *priv;
	struct snddev_info *d;
	struct pcm_channel *wrch, *rdch;
	int ret, e, err;

	if ((err = devfs_get_cdevpriv((void **)&priv)) != 0)
		return (err);
	d = priv->sc;
	if (PCM_DETACHING(d) || !DSP_REGISTERED(d)) {
		/* XXX many clients don't understand POLLNVAL */
		return (events & (POLLHUP | POLLPRI | POLLIN |
		    POLLRDNORM | POLLOUT | POLLWRNORM));
	}
	PCM_GIANT_ENTER(d);

	ret = 0;

	getchns(priv, SD_F_PRIO_RD | SD_F_PRIO_WR);
	wrch = priv->wrch;
	rdch = priv->rdch;

	if (wrch != NULL && !(wrch->flags & CHN_F_DEAD)) {
		e = (events & (POLLOUT | POLLWRNORM));
		if (e)
			ret |= chn_poll(wrch, e, td);
	}

	if (rdch != NULL && !(rdch->flags & CHN_F_DEAD)) {
		e = (events & (POLLIN | POLLRDNORM));
		if (e)
			ret |= chn_poll(rdch, e, td);
	}

	relchns(priv, SD_F_PRIO_RD | SD_F_PRIO_WR);

	PCM_GIANT_LEAVE(d);

	return (ret);
}

static int
dsp_mmap(struct cdev *i_dev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int nprot, vm_memattr_t *memattr)
{

	/*
	 * offset is in range due to checks in dsp_mmap_single().
	 * XXX memattr is not honored.
	 */
	*paddr = vtophys(offset);
	return (0);
}

static int
dsp_mmap_single(struct cdev *i_dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	struct dsp_cdevpriv *priv;
	struct snddev_info *d;
	struct pcm_channel *wrch, *rdch, *c;
	int err;

	/*
	 * Reject PROT_EXEC by default. It just doesn't makes sense.
	 * Unfortunately, we have to give up this one due to linux_mmap
	 * changes.
	 *
	 * https://lists.freebsd.org/pipermail/freebsd-emulation/2007-June/003698.html
	 *
	 */
#ifdef SV_ABI_LINUX
	if ((nprot & PROT_EXEC) && (dsp_mmap_allow_prot_exec < 0 ||
	    (dsp_mmap_allow_prot_exec == 0 &&
	    SV_CURPROC_ABI() != SV_ABI_LINUX)))
#else
	if ((nprot & PROT_EXEC) && dsp_mmap_allow_prot_exec < 1)
#endif
		return (EINVAL);

	/*
	 * PROT_READ (alone) selects the input buffer.
	 * PROT_WRITE (alone) selects the output buffer.
	 * PROT_WRITE|PROT_READ together select the output buffer.
	 */
	if ((nprot & (PROT_READ | PROT_WRITE)) == 0)
		return (EINVAL);

	if ((err = devfs_get_cdevpriv((void **)&priv)) != 0)
		return (err);
	d = priv->sc;
	if (PCM_DETACHING(d) || !DSP_REGISTERED(d))
		return (EINVAL);

	PCM_GIANT_ENTER(d);

	getchns(priv, SD_F_PRIO_RD | SD_F_PRIO_WR);
	wrch = priv->wrch;
	rdch = priv->rdch;

	c = ((nprot & PROT_WRITE) != 0) ? wrch : rdch;
	if (c == NULL || (c->flags & CHN_F_MMAP_INVALID) ||
	    (*offset  + size) > sndbuf_getallocsize(c->bufsoft) ||
	    (wrch != NULL && (wrch->flags & CHN_F_MMAP_INVALID)) ||
	    (rdch != NULL && (rdch->flags & CHN_F_MMAP_INVALID))) {
		relchns(priv, SD_F_PRIO_RD | SD_F_PRIO_WR);
		PCM_GIANT_EXIT(d);
		return (EINVAL);
	}

	if (wrch != NULL)
		wrch->flags |= CHN_F_MMAP;
	if (rdch != NULL)
		rdch->flags |= CHN_F_MMAP;

	*offset = (uintptr_t)sndbuf_getbufofs(c->bufsoft, *offset);
	relchns(priv, SD_F_PRIO_RD | SD_F_PRIO_WR);
	*object = vm_pager_allocate(OBJT_DEVICE, i_dev,
	    size, nprot, *offset, curthread->td_ucred);

	PCM_GIANT_LEAVE(d);

	if (*object == NULL)
		 return (EINVAL);
	return (0);
}

static void
dsp_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	struct snddev_info *d;
	int i;

	if (*dev != NULL)
		return;
	if (strcmp(name, "dsp") == 0 && dsp_basename_clone)
		goto found;
	for (i = 0; i < nitems(dsp_cdevs); i++) {
		if (dsp_cdevs[i].alias != NULL &&
		    strcmp(name, dsp_cdevs[i].name) == 0)
			goto found;
	}
	return;
found:
	d = devclass_get_softc(pcm_devclass, snd_unit);
	if (!PCM_REGISTERED(d))
		return;
	*dev = d->dsp_dev;
	dev_ref(*dev);
}

static void
dsp_sysinit(void *p)
{
	if (dsp_ehtag != NULL)
		return;
	/* initialize unit numbering */
	snd_unit_init();
	dsp_ehtag = EVENTHANDLER_REGISTER(dev_clone, dsp_clone, 0, 1000);
}

static void
dsp_sysuninit(void *p)
{
	if (dsp_ehtag == NULL)
		return;
	EVENTHANDLER_DEREGISTER(dev_clone, dsp_ehtag);
	dsp_ehtag = NULL;
}

SYSINIT(dsp_sysinit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, dsp_sysinit, NULL);
SYSUNINIT(dsp_sysuninit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, dsp_sysuninit, NULL);

char *
dsp_unit2name(char *buf, size_t len, int unit)
{
	int i, dtype;

	KASSERT(buf != NULL && len != 0,
	    ("bogus buf=%p len=%ju", buf, (uintmax_t)len));

	dtype = snd_unit2d(unit);

	for (i = 0; i < nitems(dsp_cdevs); i++) {
		if (dtype != dsp_cdevs[i].type || dsp_cdevs[i].alias != NULL)
			continue;
		snprintf(buf, len, "%s%d%s%d", dsp_cdevs[i].name,
		    snd_unit2u(unit), dsp_cdevs[i].sep, snd_unit2c(unit));
		return (buf);
	}

	return (NULL);
}

static int
dsp_oss_audioinfo_cb(void *data, void *arg)
{
	struct dsp_cdevpriv *priv = data;
	struct pcm_channel *ch = arg;

	if (DSP_REGISTERED(priv->sc) && (ch == priv->rdch || ch == priv->wrch))
		return (1);

	return (0);
}

/**
 * @brief Handler for SNDCTL_AUDIOINFO.
 *
 * Gathers information about the audio device specified in ai->dev.  If
 * ai->dev == -1, then this function gathers information about the current
 * device.  If the call comes in on a non-audio device and ai->dev == -1,
 * return EINVAL.
 *
 * This routine is supposed to go practically straight to the hardware,
 * getting capabilities directly from the sound card driver, side-stepping
 * the intermediate channel interface.
 *
 * @note
 * Calling threads must not hold any snddev_info or pcm_channel locks.
 * 
 * @param dev		device on which the ioctl was issued
 * @param ai		ioctl request data container
 *
 * @retval 0		success
 * @retval EINVAL	ai->dev specifies an invalid device
 *
 * @todo Verify correctness of Doxygen tags.  ;)
 */
int
dsp_oss_audioinfo(struct cdev *i_dev, oss_audioinfo *ai)
{
	struct pcmchan_caps *caps;
	struct pcm_channel *ch;
	struct snddev_info *d;
	uint32_t fmts;
	int i, nchan, *rates, minch, maxch, unit;
	char *devname, buf[CHN_NAMELEN];

	/*
	 * If probing the device that received the ioctl, make sure it's a
	 * DSP device.  (Users may use this ioctl with /dev/mixer and
	 * /dev/midi.)
	 */
	if (ai->dev == -1 && i_dev->si_devsw != &dsp_cdevsw)
		return (EINVAL);

	ch = NULL;
	devname = NULL;
	nchan = 0;
	bzero(buf, sizeof(buf));

	/*
	 * Search for the requested audio device (channel).  Start by
	 * iterating over pcm devices.
	 */ 
	for (unit = 0; pcm_devclass != NULL &&
	    unit < devclass_get_maxunit(pcm_devclass); unit++) {
		d = devclass_get_softc(pcm_devclass, unit);
		if (!PCM_REGISTERED(d))
			continue;

		/* XXX Need Giant magic entry ??? */

		/* See the note in function docblock */
		PCM_UNLOCKASSERT(d);
		PCM_LOCK(d);

		CHN_FOREACH(ch, d, channels.pcm) {
			CHN_UNLOCKASSERT(ch);
			CHN_LOCK(ch);
			if (ai->dev == -1) {
				if (devfs_foreach_cdevpriv(i_dev,
				    dsp_oss_audioinfo_cb, ch) != 0) {
					devname = dsp_unit2name(buf,
					    sizeof(buf), ch->unit);
				}
			} else if (ai->dev == nchan) {
				devname = dsp_unit2name(buf, sizeof(buf),
				    ch->unit);
			}
			if (devname != NULL)
				break;
			CHN_UNLOCK(ch);
			++nchan;
		}

		if (devname != NULL) {
			/*
			 * At this point, the following synchronization stuff
			 * has happened:
			 * - a specific PCM device is locked.
			 * - a specific audio channel has been locked, so be
			 *   sure to unlock when exiting;
			 */

			caps = chn_getcaps(ch);

			/*
			 * With all handles collected, zero out the user's
			 * container and begin filling in its fields.
			 */
			bzero((void *)ai, sizeof(oss_audioinfo));

			ai->dev = nchan;
			strlcpy(ai->name, ch->name,  sizeof(ai->name));

			if ((ch->flags & CHN_F_BUSY) == 0)
				ai->busy = 0;
			else
				ai->busy = (ch->direction == PCMDIR_PLAY) ? OPEN_WRITE : OPEN_READ;

			/**
			 * @note
			 * @c cmd - OSSv4 docs: "Only supported under Linux at
			 *    this moment." Cop-out, I know, but I'll save
			 *    running around in the process table for later.
			 *    Is there a risk of leaking information?
			 */
			ai->pid = ch->pid;

			/*
			 * These flags stolen from SNDCTL_DSP_GETCAPS handler.
			 * Note, however, that a single channel operates in
			 * only one direction, so PCM_CAP_DUPLEX is out.
			 */
			/**
			 * @todo @c SNDCTL_AUDIOINFO::caps - Make drivers keep
			 *       these in pcmchan::caps?
			 */
			ai->caps = PCM_CAP_REALTIME | PCM_CAP_MMAP | PCM_CAP_TRIGGER |
			    ((ch->flags & CHN_F_VIRTUAL) ? PCM_CAP_VIRTUAL : 0) |
			    ((ch->direction == PCMDIR_PLAY) ? PCM_CAP_OUTPUT : PCM_CAP_INPUT);

			/*
			 * Collect formats supported @b natively by the
			 * device.  Also determine min/max channels.  (I.e.,
			 * mono, stereo, or both?)
			 *
			 * If any channel is stereo, maxch = 2;
			 * if all channels are stereo, minch = 2, too;
			 * if any channel is mono, minch = 1;
			 * and if all channels are mono, maxch = 1.
			 */
			minch = 0;
			maxch = 0;
			fmts = 0;
			for (i = 0; caps->fmtlist[i]; i++) {
				fmts |= caps->fmtlist[i];
				if (AFMT_CHANNEL(caps->fmtlist[i]) > 1) {
					minch = (minch == 0) ? 2 : minch;
					maxch = 2;
				} else {
					minch = 1;
					maxch = (maxch == 0) ? 1 : maxch;
				}
			}

			if (ch->direction == PCMDIR_PLAY)
				ai->oformats = fmts;
			else
				ai->iformats = fmts;

			/**
			 * @note
			 * @c magic - OSSv4 docs: "Reserved for internal use
			 *    by OSS."
			 *
			 * @par
			 * @c card_number - OSSv4 docs: "Number of the sound
			 *    card where this device belongs or -1 if this
			 *    information is not available.  Applications
			 *    should normally not use this field for any
			 *    purpose."
			 */
			ai->card_number = -1;
			/**
			 * @todo @c song_name - depends first on
			 *          SNDCTL_[GS]ETSONG @todo @c label - depends
			 *          on SNDCTL_[GS]ETLABEL
			 * @todo @c port_number - routing information?
			 */
			ai->port_number = -1;
			ai->mixer_dev = (d->mixer_dev != NULL) ? unit : -1;
			/**
			 * @note
			 * @c real_device - OSSv4 docs:  "Obsolete."
			 */
			ai->real_device = -1;
			snprintf(ai->devnode, sizeof(ai->devnode), "/dev/dsp%d", unit);
			ai->enabled = device_is_attached(d->dev) ? 1 : 0;
			/**
			 * @note
			 * @c flags - OSSv4 docs: "Reserved for future use."
			 *
			 * @note
			 * @c binding - OSSv4 docs: "Reserved for future use."
			 *
			 * @todo @c handle - haven't decided how to generate
			 *       this yet; bus, vendor, device IDs?
			 */
			ai->min_rate = caps->minspeed;
			ai->max_rate = caps->maxspeed;

			ai->min_channels = minch;
			ai->max_channels = maxch;

			ai->nrates = chn_getrates(ch, &rates);
			if (ai->nrates > OSS_MAX_SAMPLE_RATES)
				ai->nrates = OSS_MAX_SAMPLE_RATES;

			for (i = 0; i < ai->nrates; i++)
				ai->rates[i] = rates[i];
			
			ai->next_play_engine = 0;
			ai->next_rec_engine = 0;

			CHN_UNLOCK(ch);
		}

		PCM_UNLOCK(d);

		if (devname != NULL)
			return (0);
	}

	/* Exhausted the search -- nothing is locked, so return. */
	return (EINVAL);
}

/**
 * @brief Assigns a PCM channel to a sync group.
 *
 * Sync groups are used to enable audio operations on multiple devices
 * simultaneously.  They may be used with any number of devices and may
 * span across applications.  Devices are added to groups with
 * the SNDCTL_DSP_SYNCGROUP ioctl, and operations are triggered with the
 * SNDCTL_DSP_SYNCSTART ioctl.
 *
 * If the @c id field of the @c group parameter is set to zero, then a new
 * sync group is created.  Otherwise, wrch and rdch (if set) are added to
 * the group specified.
 *
 * @todo As far as memory allocation, should we assume that things are
 * 	 okay and allocate with M_WAITOK before acquiring channel locks,
 * 	 freeing later if not?
 *
 * @param wrch	output channel associated w/ device (if any)
 * @param rdch	input channel associated w/ device (if any)
 * @param group Sync group parameters
 *
 * @retval 0		success
 * @retval non-zero	error to be propagated upstream
 */
static int
dsp_oss_syncgroup(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_syncgroup *group)
{
	struct pcmchan_syncmember *smrd, *smwr;
	struct pcmchan_syncgroup *sg;
	int ret, sg_ids[3];

	smrd = NULL;
	smwr = NULL;
	sg = NULL;
	ret = 0;

	/*
	 * Free_unr() may sleep, so store released syncgroup IDs until after
	 * all locks are released.
	 */
	sg_ids[0] = sg_ids[1] = sg_ids[2] = 0;

	PCM_SG_LOCK();

	/*
	 * - Insert channel(s) into group's member list.
	 * - Set CHN_F_NOTRIGGER on channel(s).
	 * - Stop channel(s).  
	 */

	/*
	 * If device's channels are already mapped to a group, unmap them.
	 */
	if (wrch) {
		CHN_LOCK(wrch);
		sg_ids[0] = chn_syncdestroy(wrch);
	}

	if (rdch) {
		CHN_LOCK(rdch);
		sg_ids[1] = chn_syncdestroy(rdch);
	}

	/*
	 * Verify that mode matches character device properites.
	 *  - Bail if PCM_ENABLE_OUTPUT && wrch == NULL.
	 *  - Bail if PCM_ENABLE_INPUT && rdch == NULL.
	 */
	if (((wrch == NULL) && (group->mode & PCM_ENABLE_OUTPUT)) ||
	    ((rdch == NULL) && (group->mode & PCM_ENABLE_INPUT))) {
		ret = EINVAL;
		goto out;
	}

	/*
	 * An id of zero indicates the user wants to create a new
	 * syncgroup.
	 */
	if (group->id == 0) {
		sg = (struct pcmchan_syncgroup *)malloc(sizeof(*sg), M_DEVBUF, M_NOWAIT);
		if (sg != NULL) {
			SLIST_INIT(&sg->members);
			sg->id = alloc_unr(pcmsg_unrhdr);

			group->id = sg->id;
			SLIST_INSERT_HEAD(&snd_pcm_syncgroups, sg, link);
		} else
			ret = ENOMEM;
	} else {
		SLIST_FOREACH(sg, &snd_pcm_syncgroups, link) {
			if (sg->id == group->id)
				break;
		}
		if (sg == NULL)
			ret = EINVAL;
	}

	/* Couldn't create or find a syncgroup.  Fail. */
	if (sg == NULL)
		goto out;

	/*
	 * Allocate a syncmember, assign it and a channel together, and
	 * insert into syncgroup.
	 */
	if (group->mode & PCM_ENABLE_INPUT) {
		smrd = (struct pcmchan_syncmember *)malloc(sizeof(*smrd), M_DEVBUF, M_NOWAIT);
		if (smrd == NULL) {
			ret = ENOMEM;
			goto out;
		}

		SLIST_INSERT_HEAD(&sg->members, smrd, link);
		smrd->parent = sg;
		smrd->ch = rdch;

		chn_abort(rdch);
		rdch->flags |= CHN_F_NOTRIGGER;
		rdch->sm = smrd;
	}

	if (group->mode & PCM_ENABLE_OUTPUT) {
		smwr = (struct pcmchan_syncmember *)malloc(sizeof(*smwr), M_DEVBUF, M_NOWAIT);
		if (smwr == NULL) {
			ret = ENOMEM;
			goto out;
		}

		SLIST_INSERT_HEAD(&sg->members, smwr, link);
		smwr->parent = sg;
		smwr->ch = wrch;

		chn_abort(wrch);
		wrch->flags |= CHN_F_NOTRIGGER;
		wrch->sm = smwr;
	}

out:
	if (ret != 0) {
		if (smrd != NULL)
			free(smrd, M_DEVBUF);
		if ((sg != NULL) && SLIST_EMPTY(&sg->members)) {
			sg_ids[2] = sg->id;
			SLIST_REMOVE(&snd_pcm_syncgroups, sg, pcmchan_syncgroup, link);
			free(sg, M_DEVBUF);
		}

		if (wrch)
			wrch->sm = NULL;
		if (rdch)
			rdch->sm = NULL;
	}

	if (wrch)
		CHN_UNLOCK(wrch);
	if (rdch)
		CHN_UNLOCK(rdch);

	PCM_SG_UNLOCK();

	if (sg_ids[0])
		free_unr(pcmsg_unrhdr, sg_ids[0]);
	if (sg_ids[1])
		free_unr(pcmsg_unrhdr, sg_ids[1]);
	if (sg_ids[2])
		free_unr(pcmsg_unrhdr, sg_ids[2]);

	return (ret);
}

/**
 * @brief Launch a sync group into action
 *
 * Sync groups are established via SNDCTL_DSP_SYNCGROUP.  This function
 * iterates over all members, triggering them along the way.
 *
 * @note Caller must not hold any channel locks.
 *
 * @param sg_id	sync group identifier
 *
 * @retval 0	success
 * @retval non-zero	error worthy of propagating upstream to user
 */
static int
dsp_oss_syncstart(int sg_id)
{
	struct pcmchan_syncmember *sm, *sm_tmp;
	struct pcmchan_syncgroup *sg;
	struct pcm_channel *c;
	int ret, needlocks;

	/* Get the synclists lock */
	PCM_SG_LOCK();

	do {
		ret = 0;
		needlocks = 0;

		/* Search for syncgroup by ID */
		SLIST_FOREACH(sg, &snd_pcm_syncgroups, link) {
			if (sg->id == sg_id)
				break;
		}

		/* Return EINVAL if not found */
		if (sg == NULL) {
			ret = EINVAL;
			break;
		}

		/* Any removals resulting in an empty group should've handled this */
		KASSERT(!SLIST_EMPTY(&sg->members), ("found empty syncgroup"));

		/*
		 * Attempt to lock all member channels - if any are already
		 * locked, unlock those acquired, sleep for a bit, and try
		 * again.
		 */
		SLIST_FOREACH(sm, &sg->members, link) {
			if (CHN_TRYLOCK(sm->ch) == 0) {
				int timo = hz * 5/1000; 
				if (timo < 1)
					timo = 1;

				/* Release all locked channels so far, retry */
				SLIST_FOREACH(sm_tmp, &sg->members, link) {
					/* sm is the member already locked */
					if (sm == sm_tmp)
						break;
					CHN_UNLOCK(sm_tmp->ch);
				}

				/** @todo Is PRIBIO correct/ */
				ret = msleep(sm, &snd_pcm_syncgroups_mtx,
				    PRIBIO | PCATCH, "pcmsg", timo);
				if (ret == EINTR || ret == ERESTART)
					break;

				needlocks = 1;
				ret = 0; /* Assumes ret == EAGAIN... */
			}
		}
	} while (needlocks && ret == 0);

	/* Proceed only if no errors encountered. */
	if (ret == 0) {
		/* Launch channels */
		while ((sm = SLIST_FIRST(&sg->members)) != NULL) {
			SLIST_REMOVE_HEAD(&sg->members, link);

			c = sm->ch;
			c->sm = NULL;
			chn_start(c, 1);
			c->flags &= ~CHN_F_NOTRIGGER;
			CHN_UNLOCK(c);

			free(sm, M_DEVBUF);
		}

		SLIST_REMOVE(&snd_pcm_syncgroups, sg, pcmchan_syncgroup, link);
		free(sg, M_DEVBUF);
	}

	PCM_SG_UNLOCK();

	/*
	 * Free_unr() may sleep, so be sure to give up the syncgroup lock
	 * first.
	 */
	if (ret == 0)
		free_unr(pcmsg_unrhdr, sg_id);

	return (ret);
}

/**
 * @brief Handler for SNDCTL_DSP_POLICY
 *
 * The SNDCTL_DSP_POLICY ioctl is a simpler interface to control fragment
 * size and count like with SNDCTL_DSP_SETFRAGMENT.  Instead of the user
 * specifying those two parameters, s/he simply selects a number from 0..10
 * which corresponds to a buffer size.  Smaller numbers request smaller
 * buffers with lower latencies (at greater overhead from more frequent
 * interrupts), while greater numbers behave in the opposite manner.
 *
 * The 4Front spec states that a value of 5 should be the default.  However,
 * this implementation deviates slightly by using a linear scale without
 * consulting drivers.  I.e., even though drivers may have different default
 * buffer sizes, a policy argument of 5 will have the same result across
 * all drivers.
 *
 * See http://manuals.opensound.com/developer/SNDCTL_DSP_POLICY.html for
 * more information.
 *
 * @todo When SNDCTL_DSP_COOKEDMODE is supported, it'll be necessary to
 * 	 work with hardware drivers directly.
 *
 * @note PCM channel arguments must not be locked by caller.
 *
 * @param wrch	Pointer to opened playback channel (optional; may be NULL)
 * @param rdch	" recording channel (optional; may be NULL)
 * @param policy Integer from [0:10]
 *
 * @retval 0	constant (for now)
 */
static int
dsp_oss_policy(struct pcm_channel *wrch, struct pcm_channel *rdch, int policy)
{
	int ret;

	if (policy < CHN_POLICY_MIN || policy > CHN_POLICY_MAX)
		return (EIO);

	/* Default: success */
	ret = 0;

	if (rdch) {
		CHN_LOCK(rdch);
		ret = chn_setlatency(rdch, policy);
		CHN_UNLOCK(rdch);
	}

	if (wrch && ret == 0) {
		CHN_LOCK(wrch);
		ret = chn_setlatency(wrch, policy);
		CHN_UNLOCK(wrch);
	}

	if (ret)
		ret = EIO;

	return (ret);
}

/**
 * @brief Enable or disable "cooked" mode
 *
 * This is a handler for @c SNDCTL_DSP_COOKEDMODE.  When in cooked mode, which
 * is the default, the sound system handles rate and format conversions
 * automatically (ex: user writing 11025Hz/8 bit/unsigned but card only
 * operates with 44100Hz/16bit/signed samples).
 *
 * Disabling cooked mode is intended for applications wanting to mmap()
 * a sound card's buffer space directly, bypassing the FreeBSD 2-stage
 * feeder architecture, presumably to gain as much control over audio
 * hardware as possible.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_DSP_COOKEDMODE.html
 * for more details.
 *
 * @param wrch		playback channel (optional; may be NULL)
 * @param rdch		recording channel (optional; may be NULL)
 * @param enabled	0 = raw mode, 1 = cooked mode
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_cookedmode(struct pcm_channel *wrch, struct pcm_channel *rdch, int enabled)
{

	/*
	 * XXX I just don't get it. Why don't they call it
	 * "BITPERFECT" ~ SNDCTL_DSP_BITPERFECT !?!?.
	 * This is just plain so confusing, incoherent,
	 * <insert any non-printable characters here>.
	 */
	if (!(enabled == 1 || enabled == 0))
		return (EINVAL);

	/*
	 * I won't give in. I'm inverting its logic here and now.
	 * Brag all you want, but "BITPERFECT" should be the better
	 * term here.
	 */
	enabled ^= 0x00000001;

	if (wrch != NULL) {
		CHN_LOCK(wrch);
		wrch->flags &= ~CHN_F_BITPERFECT;
		wrch->flags |= (enabled != 0) ? CHN_F_BITPERFECT : 0x00000000;
		CHN_UNLOCK(wrch);
	}

	if (rdch != NULL) {
		CHN_LOCK(rdch);
		rdch->flags &= ~CHN_F_BITPERFECT;
		rdch->flags |= (enabled != 0) ? CHN_F_BITPERFECT : 0x00000000;
		CHN_UNLOCK(rdch);
	}

	return (0);
}

/**
 * @brief Retrieve channel interleaving order
 *
 * This is the handler for @c SNDCTL_DSP_GET_CHNORDER.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_DSP_GET_CHNORDER.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_DSP_GET_CHNORDER.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param map	channel map (result will be stored there)
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_getchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map)
{
	struct pcm_channel *ch;
	int ret;

	ch = (wrch != NULL) ? wrch : rdch;
	if (ch != NULL) {
		CHN_LOCK(ch);
		ret = chn_oss_getorder(ch, map);
		CHN_UNLOCK(ch);
	} else
		ret = EINVAL;

	return (ret);
}

/**
 * @brief Specify channel interleaving order
 *
 * This is the handler for @c SNDCTL_DSP_SET_CHNORDER.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support @c SNDCTL_DSP_SET_CHNORDER.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param map	channel map
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map)
{
	int ret;

	ret = 0;

	if (wrch != NULL) {
		CHN_LOCK(wrch);
		ret = chn_oss_setorder(wrch, map);
		CHN_UNLOCK(wrch);
	}

	if (ret == 0 && rdch != NULL) {
		CHN_LOCK(rdch);
		ret = chn_oss_setorder(rdch, map);
		CHN_UNLOCK(rdch);
	}

	return (ret);
}

static int
dsp_oss_getchannelmask(struct pcm_channel *wrch, struct pcm_channel *rdch,
    int *mask)
{
	struct pcm_channel *ch;
	uint32_t chnmask;
	int ret;

	chnmask = 0;
	ch = (wrch != NULL) ? wrch : rdch;

	if (ch != NULL) {
		CHN_LOCK(ch);
		ret = chn_oss_getmask(ch, &chnmask);
		CHN_UNLOCK(ch);
	} else
		ret = EINVAL;

	if (ret == 0)
		*mask = chnmask;

	return (ret);
}

#ifdef OSSV4_EXPERIMENT
/**
 * @brief Retrieve an audio device's label
 *
 * This is a handler for the @c SNDCTL_GETLABEL ioctl.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_GETLABEL.html
 * for more details.
 *
 * From Hannu@4Front:  "For example ossxmix (just like some HW mixer
 * consoles) can show variable "labels" for certain controls. By default
 * the application name (say quake) is shown as the label but
 * applications may change the labels themselves."
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support @c SNDCTL_GETLABEL.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param label	label gets copied here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_getlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label)
{
	return (EINVAL);
}

/**
 * @brief Specify an audio device's label
 *
 * This is a handler for the @c SNDCTL_SETLABEL ioctl.  Please see the
 * comments for @c dsp_oss_getlabel immediately above.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_GETLABEL.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_SETLABEL.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param label	label gets copied from here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label)
{
	return (EINVAL);
}

/**
 * @brief Retrieve name of currently played song
 *
 * This is a handler for the @c SNDCTL_GETSONG ioctl.  Audio players could
 * tell the system the name of the currently playing song, which would be
 * visible in @c /dev/sndstat.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_GETSONG.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_GETSONG.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param song	song name gets copied here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_getsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song)
{
	return (EINVAL);
}

/**
 * @brief Retrieve name of currently played song
 *
 * This is a handler for the @c SNDCTL_SETSONG ioctl.  Audio players could
 * tell the system the name of the currently playing song, which would be
 * visible in @c /dev/sndstat.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_SETSONG.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_SETSONG.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param song	song name gets copied from here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song)
{
	return (EINVAL);
}

/**
 * @brief Rename a device
 *
 * This is a handler for the @c SNDCTL_SETNAME ioctl.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_SETNAME.html for
 * more details.
 *
 * From Hannu@4Front:  "This call is used to change the device name
 * reported in /dev/sndstat and ossinfo. So instead of  using some generic
 * 'OSS loopback audio (MIDI) driver' the device may be given a meaningfull
 * name depending on the current context (for example 'OSS virtual wave table
 * synth' or 'VoIP link to London')."
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_SETNAME.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param name	new device name gets copied from here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setname(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *name)
{
	return (EINVAL);
}
#endif	/* !OSSV4_EXPERIMENT */
