/*-
 * Copyright (c) 1999 Cameron Grant <cg@freebsd.org>
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

#include <sys/param.h>
#include <sys/queue.h>

#include <dev/sound/pcm/sound.h>

SND_DECLARE_FILE("$FreeBSD$");

#define OLDPCM_IOCTL

static d_open_t dsp_open;
static d_close_t dsp_close;
static d_read_t dsp_read;
static d_write_t dsp_write;
static d_ioctl_t dsp_ioctl;
static d_poll_t dsp_poll;
static d_mmap_t dsp_mmap;

struct cdevsw dsp_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	dsp_open,
	.d_close =	dsp_close,
	.d_read =	dsp_read,
	.d_write =	dsp_write,
	.d_ioctl =	dsp_ioctl,
	.d_poll =	dsp_poll,
	.d_mmap =	dsp_mmap,
	.d_name =	"dsp",
};

#ifdef USING_DEVFS
static eventhandler_tag dsp_ehtag;
#endif

static int dsp_oss_syncgroup(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_syncgroup *group);
static int dsp_oss_syncstart(int sg_id);
static int dsp_oss_policy(struct pcm_channel *wrch, struct pcm_channel *rdch, int policy);
#ifdef OSSV4_EXPERIMENT
static int dsp_oss_cookedmode(struct pcm_channel *wrch, struct pcm_channel *rdch, int enabled);
static int dsp_oss_getchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map);
static int dsp_oss_setchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map);
static int dsp_oss_getlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label);
static int dsp_oss_setlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label);
static int dsp_oss_getsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song);
static int dsp_oss_setsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song);
static int dsp_oss_setname(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *name);
#endif

static struct snddev_info *
dsp_get_info(struct cdev *dev)
{
	struct snddev_info *d;
	int unit;

	unit = PCMUNIT(dev);
	if (unit >= devclass_get_maxunit(pcm_devclass))
		return NULL;
	d = devclass_get_softc(pcm_devclass, unit);

	return d;
}

static u_int32_t
dsp_get_flags(struct cdev *dev)
{
	device_t bdev;
	int unit;

	unit = PCMUNIT(dev);
	if (unit >= devclass_get_maxunit(pcm_devclass))
		return 0xffffffff;
	bdev = devclass_get_device(pcm_devclass, unit);

	return pcm_getflags(bdev);
}

static void
dsp_set_flags(struct cdev *dev, u_int32_t flags)
{
	device_t bdev;
	int unit;

	unit = PCMUNIT(dev);
	if (unit >= devclass_get_maxunit(pcm_devclass))
		return;
	bdev = devclass_get_device(pcm_devclass, unit);

	pcm_setflags(bdev, flags);
}

/*
 * return the channels associated with an open device instance.
 * set the priority if the device is simplex and one direction (only) is
 * specified.
 * lock channels specified.
 */
static int
getchns(struct cdev *dev, struct pcm_channel **rdch, struct pcm_channel **wrch, u_int32_t prio)
{
	struct snddev_info *d;
	u_int32_t flags;

	flags = dsp_get_flags(dev);
	d = dsp_get_info(dev);
	pcm_inprog(d, 1);
	pcm_lock(d);
	KASSERT((flags & SD_F_PRIO_SET) != SD_F_PRIO_SET, \
		("getchns: read and write both prioritised"));

	if ((flags & SD_F_PRIO_SET) == 0 && (prio != (SD_F_PRIO_RD | SD_F_PRIO_WR))) {
		flags |= prio & (SD_F_PRIO_RD | SD_F_PRIO_WR);
		dsp_set_flags(dev, flags);
	}

	*rdch = dev->si_drv1;
	*wrch = dev->si_drv2;
	if ((flags & SD_F_SIMPLEX) && (flags & SD_F_PRIO_SET)) {
		if (prio) {
			if (*rdch && flags & SD_F_PRIO_WR) {
				dev->si_drv1 = NULL;
				*rdch = pcm_getfakechan(d);
			} else if (*wrch && flags & SD_F_PRIO_RD) {
				dev->si_drv2 = NULL;
				*wrch = pcm_getfakechan(d);
			}
		}

		pcm_getfakechan(d)->flags |= CHN_F_BUSY;
	}
	pcm_unlock(d);

	if (*rdch && *rdch != pcm_getfakechan(d) && (prio & SD_F_PRIO_RD))
		CHN_LOCK(*rdch);
	if (*wrch && *wrch != pcm_getfakechan(d) && (prio & SD_F_PRIO_WR))
		CHN_LOCK(*wrch);

	return 0;
}

/* unlock specified channels */
static void
relchns(struct cdev *dev, struct pcm_channel *rdch, struct pcm_channel *wrch, u_int32_t prio)
{
	struct snddev_info *d;

	d = dsp_get_info(dev);
	if (wrch && wrch != pcm_getfakechan(d) && (prio & SD_F_PRIO_WR))
		CHN_UNLOCK(wrch);
	if (rdch && rdch != pcm_getfakechan(d) && (prio & SD_F_PRIO_RD))
		CHN_UNLOCK(rdch);
	pcm_inprog(d, -1);
}

static int
dsp_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct pcm_channel *rdch, *wrch;
	struct snddev_info *d;
	u_int32_t fmt;
	int devtype;
	int error;
	int chnum;

	if (i_dev == NULL || td == NULL)
		return ENODEV;

	if ((flags & (FREAD | FWRITE)) == 0)
		return EINVAL;

	d = dsp_get_info(i_dev);
	devtype = PCMDEV(i_dev);
	chnum = -1;

	/* decide default format */
	switch (devtype) {
	case SND_DEV_DSP16:
		fmt = AFMT_S16_LE;
		break;

	case SND_DEV_DSP:
		fmt = AFMT_U8;
		break;

	case SND_DEV_AUDIO:
		fmt = AFMT_MU_LAW;
		break;

	case SND_DEV_NORESET:
		fmt = 0;
		break;

	case SND_DEV_DSPREC:
		fmt = AFMT_U8;
		if (flags & FWRITE)
			return EINVAL;
		chnum = PCMCHAN(i_dev);
		break;

	default:
		panic("impossible devtype %d", devtype);
	}

	/* lock snddev so nobody else can monkey with it */
	pcm_lock(d);

	rdch = i_dev->si_drv1;
	wrch = i_dev->si_drv2;

	if (rdch || wrch || ((dsp_get_flags(i_dev) & SD_F_SIMPLEX) &&
		    (flags & (FREAD | FWRITE)) == (FREAD | FWRITE))) {
		/* simplex or not, better safe than sorry. */
		pcm_unlock(d);
		return EBUSY;
	}

	/*
	 * if we get here, the open request is valid- either:
	 *   * we were previously not open
	 *   * we were open for play xor record and the opener wants
	 *     the non-open direction
	 */
	if (flags & FREAD) {
		/* open for read */
		pcm_unlock(d);
		error = pcm_chnalloc(d, &rdch, PCMDIR_REC, td->td_proc->p_pid, chnum);
		if (error != 0 && error != EBUSY && chnum != -1 && (flags & FWRITE))
			error = pcm_chnalloc(d, &rdch, PCMDIR_REC, td->td_proc->p_pid, -1);

		if (error == 0 && (chn_reset(rdch, fmt) ||
				(fmt && chn_setspeed(rdch, DSP_DEFAULT_SPEED))))
			error = ENODEV;

		if (error != 0) {
			if (rdch)
				pcm_chnrelease(rdch);
			return error;
		}

		if (flags & O_NONBLOCK)
			rdch->flags |= CHN_F_NBIO;
		pcm_chnref(rdch, 1);
	 	CHN_UNLOCK(rdch);
		pcm_lock(d);
	}

	if (flags & FWRITE) {
	    /* open for write */
	    pcm_unlock(d);
	    error = pcm_chnalloc(d, &wrch, PCMDIR_PLAY, td->td_proc->p_pid, chnum);
	    if (error != 0 && error != EBUSY && chnum != -1 && (flags & FREAD))
	    	error = pcm_chnalloc(d, &wrch, PCMDIR_PLAY, td->td_proc->p_pid, -1);

	    if (error == 0 && (chn_reset(wrch, fmt) ||
	    		(fmt && chn_setspeed(wrch, DSP_DEFAULT_SPEED))))
		error = ENODEV;

	    if (error != 0) {
		if (wrch)
		    pcm_chnrelease(wrch);
		if (rdch) {
		    /*
		     * Lock, deref and release previously created record channel
		     */
		    CHN_LOCK(rdch);
		    pcm_chnref(rdch, -1);
		    pcm_chnrelease(rdch);
		}

		return error;
	    }

	    if (flags & O_NONBLOCK)
		wrch->flags |= CHN_F_NBIO;
	    pcm_chnref(wrch, 1);
	    CHN_UNLOCK(wrch);
	    pcm_lock(d);
	}

	i_dev->si_drv1 = rdch;
	i_dev->si_drv2 = wrch;

	pcm_unlock(d);
	return 0;
}

static int
dsp_close(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct pcm_channel *rdch, *wrch;
	struct snddev_info *d;
	int refs, sg_ids[2];

	d = dsp_get_info(i_dev);
	pcm_lock(d);
	rdch = i_dev->si_drv1;
	wrch = i_dev->si_drv2;
	pcm_unlock(d);

	/*
	 * Free_unr() may sleep, so store released syncgroup IDs until after
	 * all locks are released.
	 */
	sg_ids[0] = sg_ids[1] = 0;

	if (rdch || wrch) {
		refs = 0;
		if (rdch) {
			/*
			 * The channel itself need not be locked because:
			 *   a)  Adding a channel to a syncgroup happens only in dsp_ioctl(),
			 *       which cannot run concurrently to dsp_close().
			 *   b)  The syncmember pointer (sm) is protected by the global
			 *       syncgroup list lock.
			 *   c)  A channel can't just disappear, invalidating pointers,
			 *       unless it's closed/dereferenced first.
			 */
			PCM_SG_LOCK();
			sg_ids[0] = chn_syncdestroy(rdch);
			PCM_SG_UNLOCK();

			CHN_LOCK(rdch);
			refs += pcm_chnref(rdch, -1);
			chn_abort(rdch); /* won't sleep */
			rdch->flags &= ~(CHN_F_RUNNING | CHN_F_MAPPED | CHN_F_DEAD);
			chn_reset(rdch, 0);
			pcm_chnrelease(rdch);
		}
		if (wrch) {
			/*
			 * Please see block above.
			 */
			PCM_SG_LOCK();
			sg_ids[1] = chn_syncdestroy(wrch);
			PCM_SG_UNLOCK();

			CHN_LOCK(wrch);
			refs += pcm_chnref(wrch, -1);
			/*
			 * XXX: Maybe the right behaviour is to abort on non_block.
			 * It seems that mplayer flushes the audio queue by quickly
			 * closing and re-opening.  In FBSD, there's a long pause
			 * while the audio queue flushes that I presume isn't there in
			 * linux.
			 */
			chn_flush(wrch); /* may sleep */
			wrch->flags &= ~(CHN_F_RUNNING | CHN_F_MAPPED | CHN_F_DEAD);
			chn_reset(wrch, 0);
			pcm_chnrelease(wrch);
		}

		pcm_lock(d);
		if (rdch)
			i_dev->si_drv1 = NULL;
		if (wrch)
			i_dev->si_drv2 = NULL;
		/*
		 * If there are no more references, release the channels.
		 */
		if (refs == 0 && i_dev->si_drv1 == NULL &&
			    i_dev->si_drv2 == NULL) {
			if (pcm_getfakechan(d))
				pcm_getfakechan(d)->flags = 0;
			/* What is this?!? */
			dsp_set_flags(i_dev, dsp_get_flags(i_dev) & ~SD_F_TRANSIENT);
		}
		pcm_unlock(d);
	}


	if (sg_ids[0])
		free_unr(pcmsg_unrhdr, sg_ids[0]);
	if (sg_ids[1])
		free_unr(pcmsg_unrhdr, sg_ids[1]);

	return 0;
}

static int
dsp_read(struct cdev *i_dev, struct uio *buf, int flag)
{
	struct pcm_channel *rdch, *wrch;
	int ret;

	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_RD);

	KASSERT(rdch, ("dsp_read: nonexistant channel"));
	KASSERT(rdch->flags & CHN_F_BUSY, ("dsp_read: nonbusy channel"));

	if (rdch->flags & (CHN_F_MAPPED | CHN_F_DEAD)) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_RD);
		return EINVAL;
	}
	if (!(rdch->flags & CHN_F_RUNNING))
		rdch->flags |= CHN_F_RUNNING;
	ret = chn_read(rdch, buf);
	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD);

	return ret;
}

static int
dsp_write(struct cdev *i_dev, struct uio *buf, int flag)
{
	struct pcm_channel *rdch, *wrch;
	int ret;

	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_WR);

	KASSERT(wrch, ("dsp_write: nonexistant channel"));
	KASSERT(wrch->flags & CHN_F_BUSY, ("dsp_write: nonbusy channel"));

	if (wrch->flags & (CHN_F_MAPPED | CHN_F_DEAD)) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_WR);
		return EINVAL;
	}
	if (!(wrch->flags & CHN_F_RUNNING))
		wrch->flags |= CHN_F_RUNNING;

	/*
	 * Chn_write() must give up channel lock in order to copy bytes from
	 * userland, so up the "in progress" counter to make sure someone
	 * else doesn't come along and muss up the buffer.
	 */
	++wrch->inprog;
	ret = chn_write(wrch, buf);
	--wrch->inprog;
	cv_signal(&wrch->cv);

	relchns(i_dev, rdch, wrch, SD_F_PRIO_WR);

	return ret;
}

static int
dsp_ioctl(struct cdev *i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
    	struct pcm_channel *chn, *rdch, *wrch;
	struct snddev_info *d;
	int kill;
    	int ret = 0, *arg_i = (int *)arg, tmp;
	int xcmd;

	xcmd = 0;

	/*
	 * this is an evil hack to allow broken apps to perform mixer ioctls
	 * on dsp devices.
	 */

	d = dsp_get_info(i_dev);
	if (IOCGROUP(cmd) == 'M') {
		/*
		 * This is at least, a bug to bug compatible with OSS.
		 */
		if (d->mixer_dev != NULL)
			return mixer_ioctl(d->mixer_dev, cmd, arg, -1, td);
		else
			return EBADF;
	}

	/*
	 * Certain ioctls may be made on any type of device (audio, mixer,
	 * and MIDI).  Handle those special cases here.
	 */
	if (IOCGROUP(cmd) == 'X') {
		switch(cmd) {
		case SNDCTL_SYSINFO:
			sound_oss_sysinfo((oss_sysinfo *)arg);
			break;
		case SNDCTL_AUDIOINFO:
			ret = dsp_oss_audioinfo(i_dev, (oss_audioinfo *)arg);
			break;
		case SNDCTL_MIXERINFO:
			ret = mixer_oss_mixerinfo(i_dev, (oss_mixerinfo *)arg);
			break;
		default:
			ret = EINVAL;
		}

		return ret;
	}

	getchns(i_dev, &rdch, &wrch, 0);

	kill = 0;
	if (wrch && (wrch->flags & CHN_F_DEAD))
		kill |= 1;
	if (rdch && (rdch->flags & CHN_F_DEAD))
		kill |= 2;
	if (kill == 3) {
		relchns(i_dev, rdch, wrch, 0);
		return EINVAL;
	}
	if (kill & 1)
		wrch = NULL;
	if (kill & 2)
		rdch = NULL;
	
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
	    		if (wrch) {
				CHN_LOCK(wrch);
				if (cmd == AIOSFMT && p->play_format != 0) {
					chn_setformat(wrch, p->play_format);
					chn_setspeed(wrch, p->play_rate);
				}
	    			p->play_rate = wrch->speed;
	    			p->play_format = wrch->format;
				CHN_UNLOCK(wrch);
			} else {
	    			p->play_rate = 0;
	    			p->play_format = 0;
	    		}
	    		if (rdch) {
				CHN_LOCK(rdch);
				if (cmd == AIOSFMT && p->rec_format != 0) {
					chn_setformat(rdch, p->rec_format);
					chn_setspeed(rdch, p->rec_rate);
				}
				p->rec_rate = rdch->speed;
				p->rec_format = rdch->format;
				CHN_UNLOCK(rdch);
			} else {
	    			p->rec_rate = 0;
	    			p->rec_format = 0;
	    		}
		}
		break;

    	case AIOGCAP:     /* get capabilities */
		{
	    		snd_capabilities *p = (snd_capabilities *)arg;
			struct pcmchan_caps *pcaps = NULL, *rcaps = NULL;
			struct cdev *pdev;

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
			if (rdch && wrch)
				p->formats |= (dsp_get_flags(i_dev) & SD_F_SIMPLEX)? 0 : AFMT_FULLDUPLEX;
			pdev = d->mixer_dev;
	    		p->mixers = 1; /* default: one mixer */
	    		p->inputs = pdev->si_drv1? mix_getdevs(pdev->si_drv1) : 0;
	    		p->left = p->right = 100;
			if (rdch)
				CHN_UNLOCK(rdch);
			if (wrch)
				CHN_UNLOCK(wrch);
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
		break ;

    	case SNDCTL_DSP_SETBLKSIZE:
		RANGE(*arg_i, 16, 65536);
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
			chn_sync(wrch, sndbuf_getsize(wrch->bufsoft) - 4);
			CHN_UNLOCK(wrch);
		}
		break;

    	case SNDCTL_DSP_SPEED:
		/* chn_setspeed may sleep */
		tmp = 0;
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
		*arg_i = (*arg_i)? AFMT_STEREO : 0;
		if (wrch) {
			CHN_LOCK(wrch);
			ret = chn_setformat(wrch, (wrch->format & ~AFMT_STEREO) | *arg_i);
			tmp = (wrch->format & AFMT_STEREO)? 1 : 0;
			CHN_UNLOCK(wrch);
		}
		if (rdch && ret == 0) {
			CHN_LOCK(rdch);
			ret = chn_setformat(rdch, (rdch->format & ~AFMT_STEREO) | *arg_i);
			if (tmp == -1)
				tmp = (rdch->format & AFMT_STEREO)? 1 : 0;
			CHN_UNLOCK(rdch);
		}
		*arg_i = tmp;
		break;

    	case SOUND_PCM_WRITE_CHANNELS:
/*	case SNDCTL_DSP_CHANNELS: ( == SOUND_PCM_WRITE_CHANNELS) */
		if (*arg_i != 0) {
			tmp = 0;
			*arg_i = (*arg_i != 1)? AFMT_STEREO : 0;
	  		if (wrch) {
				CHN_LOCK(wrch);
				ret = chn_setformat(wrch, (wrch->format & ~AFMT_STEREO) | *arg_i);
				tmp = (wrch->format & AFMT_STEREO)? 2 : 1;
				CHN_UNLOCK(wrch);
			}
			if (rdch && ret == 0) {
				CHN_LOCK(rdch);
				ret = chn_setformat(rdch, (rdch->format & ~AFMT_STEREO) | *arg_i);
				if (tmp == 0)
					tmp = (rdch->format & AFMT_STEREO)? 2 : 1;
				CHN_UNLOCK(rdch);
			}
			*arg_i = tmp;
		} else {
			chn = wrch ? wrch : rdch;
			CHN_LOCK(chn);
			*arg_i = (chn->format & AFMT_STEREO) ? 2 : 1;
			CHN_UNLOCK(chn);
		}
		break;

    	case SOUND_PCM_READ_CHANNELS:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = (chn->format & AFMT_STEREO) ? 2 : 1;
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
		break ;

    	case SNDCTL_DSP_SETFMT:	/* sets _one_ format */
		if ((*arg_i != AFMT_QUERY)) {
			tmp = 0;
			if (wrch) {
				CHN_LOCK(wrch);
				ret = chn_setformat(wrch, (*arg_i) | (wrch->format & AFMT_STEREO));
				tmp = wrch->format & ~AFMT_STEREO;
				CHN_UNLOCK(wrch);
			}
			if (rdch && ret == 0) {
				CHN_LOCK(rdch);
				ret = chn_setformat(rdch, (*arg_i) | (rdch->format & AFMT_STEREO));
				if (tmp == 0)
					tmp = rdch->format & ~AFMT_STEREO;
				CHN_UNLOCK(rdch);
			}
			*arg_i = tmp;
		} else {
			chn = wrch ? wrch : rdch;
			CHN_LOCK(chn);
			*arg_i = chn->format & ~AFMT_STEREO;
			CHN_UNLOCK(chn);
		}
		break;

    	case SNDCTL_DSP_SETFRAGMENT:
		DEB(printf("SNDCTL_DSP_SETFRAGMENT 0x%08x\n", *(int *)arg));
		{
			u_int32_t fragln = (*arg_i) & 0x0000ffff;
			u_int32_t maxfrags = ((*arg_i) & 0xffff0000) >> 16;
			u_int32_t fragsz;
			u_int32_t r_maxfrags, r_fragsz;

			RANGE(fragln, 4, 16);
			fragsz = 1 << fragln;

			if (maxfrags == 0)
				maxfrags = CHN_2NDBUFMAXSIZE / fragsz;
			if (maxfrags < 2)
				maxfrags = 2;
			if (maxfrags * fragsz > CHN_2NDBUFMAXSIZE)
				maxfrags = CHN_2NDBUFMAXSIZE / fragsz;

			DEB(printf("SNDCTL_DSP_SETFRAGMENT %d frags, %d sz\n", maxfrags, fragsz));
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
	    		}
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
	    		}
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
	        		a->ptr = sndbuf_getreadyptr(bs);
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
		*arg_i = DSP_CAP_REALTIME | DSP_CAP_MMAP | DSP_CAP_TRIGGER;
		if (rdch && wrch && !(dsp_get_flags(i_dev) & SD_F_SIMPLEX))
			*arg_i |= DSP_CAP_DUPLEX;
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
			rdch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_INPUT)
				chn_start(rdch, 1);
			else
				rdch->flags |= CHN_F_NOTRIGGER;
			CHN_UNLOCK(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
			wrch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_OUTPUT)
				chn_start(wrch, 1);
			else
				wrch->flags |= CHN_F_NOTRIGGER;
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
			struct snd_dbuf *b = wrch->bufhard;
	        	struct snd_dbuf *bs = wrch->bufsoft;

			CHN_LOCK(wrch);
			/* XXX abusive DMA update: chn_wrupdate(wrch); */
			*arg_i = sndbuf_getready(b) + sndbuf_getready(bs);
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
		if (rdch && wrch && (dsp_get_flags(i_dev) & SD_F_SIMPLEX))
			dsp_set_flags(i_dev, dsp_get_flags(i_dev)^SD_F_SIMPLEX);
		break;

	/*
	 * The following four ioctls are simple wrappers around mixer_ioctl
	 * with no further processing.  xcmd is short for "translated
	 * command".
	 */
	case SNDCTL_DSP_GETRECVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_READ_RECLEV;
		/* FALLTHROUGH */
	case SNDCTL_DSP_SETRECVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_WRITE_RECLEV;
		/* FALLTHROUGH */
	case SNDCTL_DSP_GETPLAYVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_READ_PCM;
		/* FALLTHROUGH */
	case SNDCTL_DSP_SETPLAYVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_WRITE_PCM;

		if (d->mixer_dev != NULL)
			ret = mixer_ioctl(d->mixer_dev, xcmd, arg, -1, td);
		else
			ret = ENOTSUP;
		break;

	case SNDCTL_DSP_GET_RECSRC_NAMES:
	case SNDCTL_DSP_GET_RECSRC:
	case SNDCTL_DSP_SET_RECSRC:
		if (d->mixer_dev != NULL)
			ret = mixer_ioctl(d->mixer_dev, cmd, arg, -1, td);
		else
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
			oc->samples = (sndbuf_gettotal(b) + tmp) / sndbuf_getbps(b);
			oc->fifo_samples = (sndbuf_getready(b) - tmp) / sndbuf_getbps(b);
#else
			oc->samples = sndbuf_gettotal(bs) / sndbuf_getbps(bs);
			oc->fifo_samples = sndbuf_getready(bs) / sndbuf_getbps(bs);
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
	 * play_underruns and rec_overrun fields are the only usefull fields
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
		ret = dsp_oss_syncgroup(wrch, rdch, (oss_syncgroup *)arg);
		break;

	case SNDCTL_DSP_SYNCSTART:
		ret = dsp_oss_syncstart(*arg_i);
		break;

	case SNDCTL_DSP_POLICY:
		ret = dsp_oss_policy(wrch, rdch, *arg_i);
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

	case SNDCTL_DSP_COOKEDMODE:
		ret = dsp_oss_cookedmode(wrch, rdch, *arg_i);
		break;
	case SNDCTL_DSP_GET_CHNORDER:
		ret = dsp_oss_getchnorder(wrch, rdch, (unsigned long long *)arg);
		break;
	case SNDCTL_DSP_SET_CHNORDER:
		ret = dsp_oss_setchnorder(wrch, rdch, (unsigned long long *)arg);
		break;
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
	 * @note The SNDCTL_CARDINFO ioctl was omitted per 4Front developer
	 * documentation.  "The usability of this call is very limited. It's
	 * provided only for completeness of the API. OSS API doesn't have
	 * any concept of card. Any information returned by this ioctl calld
	 * is reserved exclusively for the utility programs included in the
	 * OSS package. Applications should not try to use for this
	 * information in any ways."
	 */
	case SNDCTL_CARDINFO:
		ret = EINVAL;
		break;
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
	relchns(i_dev, rdch, wrch, 0);
    	return ret;
}

static int
dsp_poll(struct cdev *i_dev, int events, struct thread *td)
{
	struct pcm_channel *wrch = NULL, *rdch = NULL;
	int ret, e;

	ret = 0;
	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	if (wrch) {
		e = (events & (POLLOUT | POLLWRNORM));
		if (e)
			ret |= chn_poll(wrch, e, td);
	}
	if (rdch) {
		e = (events & (POLLIN | POLLRDNORM));
		if (e)
			ret |= chn_poll(rdch, e, td);
	}
	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	return ret;
}

static int
dsp_mmap(struct cdev *i_dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{
	struct pcm_channel *wrch = NULL, *rdch = NULL, *c;

	if (nprot & PROT_EXEC)
		return -1;

	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);
#if 0
	/*
	 * XXX the linux api uses the nprot to select read/write buffer
	 * our vm system doesn't allow this, so force write buffer
	 */

	if (wrch && (nprot & PROT_WRITE)) {
		c = wrch;
	} else if (rdch && (nprot & PROT_READ)) {
		c = rdch;
	} else {
		return -1;
	}
#else
	c = wrch;
#endif

	if (c == NULL) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);
		return -1;
	}

	if (offset >= sndbuf_getsize(c->bufsoft)) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);
		return -1;
	}

	if (!(c->flags & CHN_F_MAPPED))
		c->flags |= CHN_F_MAPPED;

	*paddr = vtophys(sndbuf_getbufofs(c->bufsoft, offset));
	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	return 0;
}

#ifdef USING_DEVFS

/*
 * Clone logic is this:
 * x E X = {dsp, dspW, audio}
 * x -> x${sysctl("hw.snd.unit")}
 * xN->
 *    for i N = 1 to channels of device N
 *    	if xN.i isn't busy, return its dev_t
 */
static void
dsp_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	struct cdev *pdev;
	struct snddev_info *pcm_dev;
	struct snddev_channel *pcm_chan;
	int i, unit, devtype;
	static int devtypes[3] = {SND_DEV_DSP, SND_DEV_DSP16, SND_DEV_AUDIO};
	static char *devnames[3] = {"dsp", "dspW", "audio"};

	if (*dev != NULL)
		return;
	if (pcm_devclass == NULL)
		return;

	devtype = 0;
	unit = -1;
	for (i = 0; (i < 3) && (unit == -1); i++) {
		devtype = devtypes[i];
		if (strcmp(name, devnames[i]) == 0) {
			unit = snd_unit;
		} else {
			if (dev_stdclone(name, NULL, devnames[i], &unit) != 1)
				unit = -1;
		}
	}
	if (unit == -1 || unit >= devclass_get_maxunit(pcm_devclass))
		return;

	pcm_dev = devclass_get_softc(pcm_devclass, unit);

	if (pcm_dev == NULL)
		return;

	SLIST_FOREACH(pcm_chan, &pcm_dev->channels, link) {

		switch(devtype) {
			case SND_DEV_DSP:
				pdev = pcm_chan->dsp_devt;
				break;
			case SND_DEV_DSP16:
				pdev = pcm_chan->dspW_devt;
				break;
			case SND_DEV_AUDIO:
				pdev = pcm_chan->audio_devt;
				break;
			default:
				panic("Unknown devtype %d", devtype);
		}

		if ((pdev != NULL) && (pdev->si_drv1 == NULL) && (pdev->si_drv2 == NULL)) {
			*dev = pdev;
			dev_ref(*dev);
			return;
		}
	}
}

static void
dsp_sysinit(void *p)
{
	dsp_ehtag = EVENTHANDLER_REGISTER(dev_clone, dsp_clone, 0, 1000);
}

static void
dsp_sysuninit(void *p)
{
	if (dsp_ehtag != NULL)
		EVENTHANDLER_DEREGISTER(dev_clone, dsp_ehtag);
}

SYSINIT(dsp_sysinit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, dsp_sysinit, NULL);
SYSUNINIT(dsp_sysuninit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, dsp_sysuninit, NULL);
#endif

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
 * Note, however, that the usefulness of this command is significantly
 * decreased when requesting info about any device other than the one serving
 * the request. While each snddev_channel refers to a specific device node,
 * the converse is *not* true.  Currently, when a sound device node is opened,
 * the sound subsystem scans for an available audio channel (or channels, if
 * opened in read+write) and then assigns them to the si_drv[12] private
 * data fields.  As a result, any information returned linking a channel to
 * a specific character device isn't necessarily accurate.
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
	struct snddev_channel *sce;
	struct pcmchan_caps *caps;
	struct pcm_channel *ch;
	struct snddev_info *d;
	struct cdev *t_cdev;
	uint32_t fmts;
	int i, nchan, ret, *rates, minch, maxch;

	/*
	 * If probing the device that received the ioctl, make sure it's a
	 * DSP device.  (Users may use this ioctl with /dev/mixer and
	 * /dev/midi.)
	 */
	if ((ai->dev == -1) && (i_dev->si_devsw != &dsp_cdevsw))
		return EINVAL;

	ch = NULL;
	t_cdev = NULL;
	nchan = 0;
	ret = 0;
	
	/*
	 * Search for the requested audio device (channel).  Start by
	 * iterating over pcm devices.
	 */ 
	for (i = 0; i < devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (d == NULL)
			continue;

		/* See the note in function docblock */
		mtx_assert(d->lock, MA_NOTOWNED);
		pcm_inprog(d, 1);
		pcm_lock(d);

		SLIST_FOREACH(sce, &d->channels, link) {
			ch = sce->channel;
			mtx_assert(ch->lock, MA_NOTOWNED);
			CHN_LOCK(ch);
			if (ai->dev == -1) {
				if ((ch == i_dev->si_drv1) ||	/* record ch */
				    (ch == i_dev->si_drv2)) {	/* playback ch */
					t_cdev = i_dev;
					goto dspfound;
				}
			} else if (ai->dev == nchan) {
				t_cdev = sce->dsp_devt;
				goto dspfound;
			}
			CHN_UNLOCK(ch);
			++nchan;
		}

		pcm_unlock(d);
		pcm_inprog(d, -1);
	}

	/* Exhausted the search -- nothing is locked, so return. */
	return EINVAL;

dspfound:
	/* Should've found the device, but something isn't right */
	if (t_cdev == NULL) {
		ret = EINVAL;
		goto out;
	}

	/*
	 * At this point, the following synchronization stuff has happened:
	 *   - a specific PCM device is locked and its "in progress
	 *     operations" counter has been incremented, so be sure to unlock
	 *     and decrement when exiting;
	 *   - a specific audio channel has been locked, so be sure to unlock
	 *     when exiting;
	 */

	caps = chn_getcaps(ch);

	/*
	 * With all handles collected, zero out the user's container and
	 * begin filling in its fields.
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
	 * @c cmd - OSSv4 docs: "Only supported under Linux at this moment."
	 * 	Cop-out, I know, but I'll save running around in the process
	 * 	table for later.  Is there a risk of leaking information?
	 */
	ai->pid = ch->pid;
	
	/*
	 * These flags stolen from SNDCTL_DSP_GETCAPS handler.  Note, however,
	 * that a single channel operates in only one direction, so
	 * DSP_CAP_DUPLEX is out.
	 */
	/**
	 * @todo @c SNDCTL_AUDIOINFO::caps - Make drivers keep these in
	 * 	 pcmchan::caps?
	 */
	ai->caps = DSP_CAP_REALTIME | DSP_CAP_MMAP | DSP_CAP_TRIGGER;

	/*
	 * Collect formats supported @b natively by the device.  Also
	 * determine min/max channels.  (I.e., mono, stereo, or both?)
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
		if (caps->fmtlist[i] & AFMT_STEREO) {
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
	 * @c magic - OSSv4 docs: "Reserved for internal use by OSS."
	 *
	 * @par
	 * @c card_number - OSSv4 docs: "Number of the sound card where this
	 * 	device belongs or -1 if this information is not available.
	 * 	Applications should normally not use this field for any
	 * 	purpose."
	 */
	ai->card_number = -1;
	/**
	 * @todo @c song_name - depends first on SNDCTL_[GS]ETSONG
	 * @todo @c label - depends on SNDCTL_[GS]ETLABEL
	 * @todo @c port_number - routing information?
	 */
	ai->port_number = -1;
	ai->mixer_dev = (d->mixer_dev != NULL) ? PCMUNIT(d->mixer_dev) : -1;
	/**
	 * @note
	 * @c real_device - OSSv4 docs:  "Obsolete."
	 */
	ai->real_device = -1;
	strlcpy(ai->devnode, t_cdev->si_name, sizeof(ai->devnode));
	ai->enabled = device_is_attached(d->dev) ? 1 : 0;
	/**
	 * @note
	 * @c flags - OSSv4 docs: "Reserved for future use."
	 *
	 * @note
	 * @c binding - OSSv4 docs: "Reserved for future use."
	 *
	 * @todo @c handle - haven't decided how to generate this yet; bus,
	 * 	vendor, device IDs?
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

out:
	CHN_UNLOCK(ch);
	pcm_unlock(d);
	pcm_inprog(d, -1);

	return ret;
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

	return ret;
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
				ret = msleep(sm, &snd_pcm_syncgroups_mtx, PRIBIO | PCATCH, "pcmsgrp", timo);
				if (ret == EINTR || ret == ERESTART)
					break;

				needlocks = 1;
				ret = 0; /* Assumes ret == EWOULDBLOCK... */
			}
		}
	} while (needlocks && ret == 0);

	/* Proceed only if no errors encountered. */
	if (ret == 0) {
		/* Launch channels */
		while((sm = SLIST_FIRST(&sg->members)) != NULL) {
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

	return ret;
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
	int fragln, fragsz, maxfrags, ret;

	/* Default: success */
	ret = 0;

	/* Scale policy [0..10] to fragment size [2^4..2^16]. */
	fragln = policy;
	RANGE(fragln, 0, 10);
	fragln += 4;
	fragsz = 1 << fragln;

	maxfrags = CHN_2NDBUFMAXSIZE / fragsz;

	if (rdch) {
		CHN_LOCK(rdch);
		ret = chn_setblocksize(rdch, maxfrags, fragsz);
		CHN_UNLOCK(rdch);
	}

	if (wrch && ret == 0) {
		CHN_LOCK(wrch);
		ret = chn_setblocksize(wrch, maxfrags, fragsz);
		CHN_UNLOCK(wrch);
	}

	return ret;
}

#ifdef OSSV4_EXPERIMENT
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
 * @note Currently, this function is just a stub that always returns EINVAL.
 *
 * @todo Figure out how to and actually implement this.
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
	return EINVAL;
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
	return EINVAL;
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
	return EINVAL;
}

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
	return EINVAL;
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
	return EINVAL;
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
	return EINVAL;
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
	return EINVAL;
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
	return EINVAL;
}
#endif	/* !OSSV4_EXPERIMENT */
