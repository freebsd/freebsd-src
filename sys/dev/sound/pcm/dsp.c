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
 * return the channels channels associated with an open device instance.
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
	int rdref;
	int error;

	d = dsp_get_info(i_dev);
	devtype = PCMDEV(i_dev);

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
		if (mode & FWRITE) {
			return EINVAL;
		}
		break;

	default:
		panic("impossible devtype %d", devtype);
	}

	rdref = 0;

	/* lock snddev so nobody else can monkey with it */
	pcm_lock(d);

	rdch = i_dev->si_drv1;
	wrch = i_dev->si_drv2;

	if ((dsp_get_flags(i_dev) & SD_F_SIMPLEX) && (rdch || wrch)) {
		/* we're a simplex device and already open, no go */
		pcm_unlock(d);
		return EBUSY;
	}

	if (((flags & FREAD) && rdch) || ((flags & FWRITE) && wrch)) {
		/*
		 * device already open in one or both directions that
		 * the opener wants; we can't handle this.
		 */
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
		if (devtype == SND_DEV_DSPREC)
			rdch = pcm_chnalloc(d, PCMDIR_REC, td->td_proc->p_pid, PCMCHAN(i_dev));
		else
			rdch = pcm_chnalloc(d, PCMDIR_REC, td->td_proc->p_pid, -1);
		if (!rdch) {
			/* no channel available, exit */
			return EBUSY;
		}
		/* got a channel, already locked for us */
		if (chn_reset(rdch, fmt)) {
			pcm_chnrelease(rdch);
			i_dev->si_drv1 = NULL;
			return ENODEV;
		}

		pcm_lock(d);
		if (flags & O_NONBLOCK)
			rdch->flags |= CHN_F_NBIO;
		pcm_chnref(rdch, 1);
	 	CHN_UNLOCK(rdch);
		rdref = 1;
		/*
		 * Record channel created, ref'ed and unlocked
		 */
	}

	if (flags & FWRITE) {
	    /* open for write */
	    pcm_unlock(d);
	    wrch = pcm_chnalloc(d, PCMDIR_PLAY, td->td_proc->p_pid, -1);
	    error = 0;

	    if (!wrch)
		error = EBUSY; /* XXX Right return code? */
	    else if (chn_reset(wrch, fmt))
		error = ENODEV;

	    pcm_lock(d);
	    if (error != 0) {
		if (wrch) {
		    /*
		     * Free play channel
		     */
		    pcm_chnrelease(wrch);
		    i_dev->si_drv2 = NULL;
		}
		if (rdref) {
		    /*
		     * Lock, deref and release previously created record channel
		     */
		    CHN_LOCK(rdch);
		    pcm_chnref(rdch, -1);
		    pcm_chnrelease(rdch);
		    i_dev->si_drv1 = NULL;
		}

		pcm_unlock(d);
		return error;
	    }

	    if (flags & O_NONBLOCK)
		wrch->flags |= CHN_F_NBIO;
	    pcm_chnref(wrch, 1);
	    CHN_UNLOCK(wrch);
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
	int refs;

	d = dsp_get_info(i_dev);
	rdch = i_dev->si_drv1;
	wrch = i_dev->si_drv2;

	refs = 0;

	if (rdch) {
		CHN_LOCK(rdch);
		refs += pcm_chnref(rdch, -1);
		CHN_UNLOCK(rdch);
	}
	if (wrch) {
		CHN_LOCK(wrch);
		refs += pcm_chnref(wrch, -1);
		CHN_UNLOCK(wrch);
	}

	/*
	 * If there are no more references, release the channels.
	 */
	if ((rdch || wrch) && refs == 0) {

		pcm_lock(d);

		if (pcm_getfakechan(d))
			pcm_getfakechan(d)->flags = 0;

		i_dev->si_drv1 = NULL;
		i_dev->si_drv2 = NULL;

		dsp_set_flags(i_dev, dsp_get_flags(i_dev) & ~SD_F_TRANSIENT);

		pcm_unlock(d);

		if (rdch) {
			CHN_LOCK(rdch);
			chn_abort(rdch); /* won't sleep */
			rdch->flags &= ~(CHN_F_RUNNING | CHN_F_MAPPED | CHN_F_DEAD);
			chn_reset(rdch, 0);
			pcm_chnrelease(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
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
	}
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
	ret = chn_write(wrch, buf);
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

	/*
	 * this is an evil hack to allow broken apps to perform mixer ioctls
	 * on dsp devices.
	 */

	d = dsp_get_info(i_dev);
	if (IOCGROUP(cmd) == 'M')
		return mixer_ioctl(d->mixer_dev, cmd, arg, mode, td);

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
		CHN_LOCK(wrch);
/*
		if (wrch && wrch->bufhard.dl)
			while (chn_wrfeed(wrch) == 0);
*/
		*arg_i = wrch? sndbuf_getfree(wrch->bufsoft) : 0;
		CHN_UNLOCK(wrch);
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

	    		if (wrch) {
				CHN_LOCK(wrch);
				if (cmd == AIOSFMT) {
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
				if (cmd == AIOSFMT) {
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
		} else
			*arg_i = 0;
		break;

    	case FIOASYNC: /*set/clear async i/o */
		DEB( printf("FIOASYNC\n") ; )
		break;

    	case SNDCTL_DSP_NONBLOCK:
    	case FIONBIO: /* set/clear non-blocking i/o */
		if (rdch) {
			CHN_LOCK(rdch);
			if (*arg_i)
				rdch->flags |= CHN_F_NBIO;
			else
				rdch->flags &= ~CHN_F_NBIO;
			CHN_UNLOCK(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
			if (*arg_i)
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
		CHN_LOCK(chn);
		*arg_i = sndbuf_getblksz(chn->bufsoft);
		CHN_UNLOCK(chn);
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
		CHN_LOCK(chn);
		*arg_i = chn->speed;
		CHN_UNLOCK(chn);
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
		CHN_LOCK(chn);
		*arg_i = (chn->format & AFMT_STEREO) ? 2 : 1;
		CHN_UNLOCK(chn);
		break;

    	case SNDCTL_DSP_GETFMTS:	/* returns a mask of supported fmts */
		chn = wrch ? wrch : rdch;
		CHN_LOCK(chn);
		*arg_i = chn_getformats(chn);
		CHN_UNLOCK(chn);
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
				maxfrags = sndbuf_getblkcnt(rdch->bufsoft);
				fragsz = sndbuf_getblksz(rdch->bufsoft);
				CHN_UNLOCK(rdch);
			}
		    	if (wrch && ret == 0) {
				CHN_LOCK(wrch);
				ret = chn_setblocksize(wrch, maxfrags, fragsz);
 				maxfrags = sndbuf_getblkcnt(wrch->bufsoft);
				fragsz = sndbuf_getblksz(wrch->bufsoft);
				CHN_UNLOCK(wrch);
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
				chn_wrupdate(wrch);
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
				chn_rdupdate(rdch);
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
				chn_wrupdate(wrch);
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
			chn_wrupdate(wrch);
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
	int devtypes[3] = {SND_DEV_DSP, SND_DEV_DSP16, SND_DEV_AUDIO};
	char *devnames[3] = {"dsp", "dspW", "audio"};

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

		if ((pdev->si_drv1 == NULL) && (pdev->si_drv2 == NULL)) {
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


