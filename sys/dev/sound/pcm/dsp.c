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

#include <sys/param.h>
#include <sys/queue.h>

#include <dev/sound/pcm/sound.h>

#define OLDPCM_IOCTL

static d_open_t dsp_open;
static d_close_t dsp_close;
static d_read_t dsp_read;
static d_write_t dsp_write;
static d_ioctl_t dsp_ioctl;
static d_poll_t dsp_poll;
static d_mmap_t dsp_mmap;

static struct cdevsw dsp_cdevsw = {
	/* open */	dsp_open,
	/* close */	dsp_close,
	/* read */	dsp_read,
	/* write */	dsp_write,
	/* ioctl */	dsp_ioctl,
	/* poll */	dsp_poll,
	/* mmap */	dsp_mmap,
	/* strategy */	nostrategy,
	/* name */	"dsp",
	/* maj */	SND_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

#ifdef USING_DEVFS
static eventhandler_tag dsp_ehtag;
#endif

static struct snddev_info *
dsp_get_info(dev_t dev)
{
	struct snddev_info *d;
	int unit;

	unit = PCMUNIT(dev);
	if (unit >= devclass_get_maxunit(pcm_devclass))
		return NULL;
	d = devclass_get_softc(pcm_devclass, unit);

	return d;
}

/*
 * return the channels channels associated with an open device instance.
 * set the priority if the device is simplex and one direction (only) is
 * specified.
 * lock channels specified.
 */
static int
getchns(dev_t dev, struct pcm_channel **rdch, struct pcm_channel **wrch, u_int32_t prio)
{
	struct snddev_info *d;

	d = dsp_get_info(dev);
	snd_mtxlock(d->lock);
	d->inprog++;
	KASSERT((d->flags & SD_F_PRIO_SET) != SD_F_PRIO_SET, \
		("getchns: read and write both prioritised"));

	if ((d->flags & SD_F_PRIO_SET) == 0 && (prio != (SD_F_PRIO_RD | SD_F_PRIO_WR)))
		d->flags |= prio & (SD_F_PRIO_RD | SD_F_PRIO_WR);

	*rdch = dev->si_drv1;
	*wrch = dev->si_drv2;
	if ((d->flags & SD_F_SIMPLEX) && (d->flags & SD_F_PRIO_SET)) {
		if (prio) {
			if (*rdch && d->flags & SD_F_PRIO_WR) {
				dev->si_drv1 = NULL;
				*rdch = d->fakechan;
			} else if (*wrch && d->flags & SD_F_PRIO_RD) {
				dev->si_drv2 = NULL;
				*wrch = d->fakechan;
			}
		}

		d->fakechan->flags |= CHN_F_BUSY;
	}

	if (*rdch && *rdch != d->fakechan && (prio & SD_F_PRIO_RD))
		CHN_LOCK(*rdch);
	if (*wrch && *wrch != d->fakechan && (prio & SD_F_PRIO_WR))
		CHN_LOCK(*wrch);
	snd_mtxunlock(d->lock);

	return 0;
}

/* unlock specified channels */
static void
relchns(dev_t dev, struct pcm_channel *rdch, struct pcm_channel *wrch, u_int32_t prio)
{
	struct snddev_info *d;

	d = dsp_get_info(dev);
	if (wrch && wrch != d->fakechan && (prio & SD_F_PRIO_WR))
		CHN_UNLOCK(wrch);
	if (rdch && rdch != d->fakechan && (prio & SD_F_PRIO_RD))
		CHN_UNLOCK(rdch);
	snd_mtxlock(d->lock);
	d->inprog--;
	snd_mtxunlock(d->lock);
}

static int
dsp_open(dev_t i_dev, int flags, int mode, struct proc *p)
{
	struct pcm_channel *rdch, *wrch;
	struct snddev_info *d;
	intrmask_t s;
	u_int32_t fmt;
	int devtype;

	s = spltty();
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

	default:
		panic("impossible devtype %d", devtype);
	}

	/* lock snddev so nobody else can monkey with it */
	snd_mtxlock(d->lock);
	if ((d->flags & SD_F_SIMPLEX) && (i_dev->si_drv1 || i_dev->si_drv2)) {
		/* simplex device, already open, exit */
		snd_mtxunlock(d->lock);
		splx(s);
		return EBUSY;
	}

	/*  if we get here, the open request is valid */
	rdch = i_dev->si_drv1;
	wrch = i_dev->si_drv2;

	if (flags & FREAD) {
		/* open for read */
		if (rdch == NULL) {
			/* not already open, try to get a channel */
			rdch = pcm_chnalloc(d, PCMDIR_REC, p->p_pid);
			if (!rdch) {
				/* no channel available, exit */
				snd_mtxunlock(d->lock);
				splx(s);
				return EBUSY;
			}
			/* got a channel, already locked for us */
		} else {
			/* already open for read, exit */
			snd_mtxunlock(d->lock);
			splx(s);
			return EBUSY;
		}
	}

	if (flags & FWRITE) {
		/* open for write */
		if (wrch == NULL) {
			/* not already open, try to get a channel */
			wrch = pcm_chnalloc(d, PCMDIR_PLAY, p->p_pid);
			if (!wrch) {
				/* no channel available */
				if (rdch && (flags & FREAD)) {
					/* just opened a read channel, release it */
					pcm_chnrelease(rdch);
				}
				/* exit */
				snd_mtxunlock(d->lock);
				splx(s);
				return EBUSY;
			}
			/* got a channel, already locked for us */
		} else {
			/* already open for write */
			if (rdch && (flags & FREAD)) {
				/* just opened a read channel, release it */
				pcm_chnrelease(rdch);
			}
			/* exit */
			snd_mtxunlock(d->lock);
			splx(s);
			return EBUSY;
		}
	}

	i_dev->si_drv1 = rdch;
	i_dev->si_drv2 = wrch;
	snd_mtxunlock(d->lock);
	/* finished with snddev, new channels still locked */

	/* bump refcounts, reset and unlock any channels that we just opened */
	if (rdch) {
		if (flags & FREAD) {
	        	chn_reset(rdch, fmt);
			if (flags & O_NONBLOCK)
				rdch->flags |= CHN_F_NBIO;
		} else {
			CHN_LOCK(rdch);
			pcm_chnref(rdch, 1);
		}
	 	CHN_UNLOCK(rdch);
	}
	if (wrch) {
		if (flags & FWRITE) {
	        	chn_reset(wrch, fmt);
			if (flags & O_NONBLOCK)
				wrch->flags |= CHN_F_NBIO;
		} else {
			CHN_LOCK(wrch);
			pcm_chnref(wrch, 1);
		}
	 	CHN_UNLOCK(wrch);
	}
	splx(s);
	return 0;
}

static int
dsp_close(dev_t i_dev, int flags, int mode, struct proc *p)
{
	struct pcm_channel *rdch, *wrch;
	struct snddev_info *d;
	intrmask_t s;
	int exit;

	s = spltty();
	d = dsp_get_info(i_dev);
	snd_mtxlock(d->lock);
	rdch = i_dev->si_drv1;
	wrch = i_dev->si_drv2;

	exit = 0;

	/* decrement refcount for each channel, exit if nonzero */
	if (rdch) {
		CHN_LOCK(rdch);
		if (pcm_chnref(rdch, -1) > 0) {
			CHN_UNLOCK(rdch);
			exit = 1;
		}
	}
	if (wrch) {
		CHN_LOCK(wrch);
		if (pcm_chnref(wrch, -1) > 0) {
			CHN_UNLOCK(wrch);
			exit = 1;
		}
	}
	if (exit) {
		snd_mtxunlock(d->lock);
		splx(s);
		return 0;
	}

	/* both refcounts are zero, abort and release */

	if (d->fakechan)
		d->fakechan->flags = 0;

	i_dev->si_drv1 = NULL;
	i_dev->si_drv2 = NULL;

	d->flags &= ~SD_F_TRANSIENT;
	snd_mtxunlock(d->lock);

	if (rdch) {
		chn_abort(rdch); /* won't sleep */
		rdch->flags &= ~(CHN_F_RUNNING | CHN_F_MAPPED | CHN_F_DEAD);
		chn_reset(rdch, 0);
		pcm_chnrelease(rdch);
	}
	if (wrch) {
		chn_flush(wrch); /* may sleep */
		wrch->flags &= ~(CHN_F_RUNNING | CHN_F_MAPPED | CHN_F_DEAD);
		chn_reset(wrch, 0);
		pcm_chnrelease(wrch);
	}

	splx(s);
	return 0;
}

static int
dsp_read(dev_t i_dev, struct uio *buf, int flag)
{
	struct pcm_channel *rdch, *wrch;
	intrmask_t s;
	int ret;

	s = spltty();
	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_RD);

	KASSERT(rdch, ("dsp_read: nonexistant channel"));
	KASSERT(rdch->flags & CHN_F_BUSY, ("dsp_read: nonbusy channel"));

	if (rdch->flags & (CHN_F_MAPPED | CHN_F_DEAD)) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_RD);
		splx(s);
		return EINVAL;
	}
	if (!(rdch->flags & CHN_F_RUNNING))
		rdch->flags |= CHN_F_RUNNING;
	ret = chn_read(rdch, buf);
	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD);

	splx(s);
	return ret;
}

static int
dsp_write(dev_t i_dev, struct uio *buf, int flag)
{
	struct pcm_channel *rdch, *wrch;
	intrmask_t s;
	int ret;

	s = spltty();
	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_WR);

	KASSERT(wrch, ("dsp_write: nonexistant channel"));
	KASSERT(wrch->flags & CHN_F_BUSY, ("dsp_write: nonbusy channel"));

	if (wrch->flags & (CHN_F_MAPPED | CHN_F_DEAD)) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_WR);
		splx(s);
		return EINVAL;
	}
	if (!(wrch->flags & CHN_F_RUNNING))
		wrch->flags |= CHN_F_RUNNING;
	ret = chn_write(wrch, buf);
	relchns(i_dev, rdch, wrch, SD_F_PRIO_WR);

	splx(s);
	return ret;
}

static int
dsp_ioctl(dev_t i_dev, u_long cmd, caddr_t arg, int mode, struct proc *p)
{
    	struct pcm_channel *wrch, *rdch;
	struct snddev_info *d;
	intrmask_t s;
	int kill;
    	int ret = 0, *arg_i = (int *)arg, tmp;

	/*
	 * this is an evil hack to allow broken apps to perform mixer ioctls
	 * on dsp devices.
	 */

	if (IOCGROUP(cmd) == 'M') {
		dev_t pdev;

		pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(PCMUNIT(i_dev), SND_DEV_CTL, 0));
		return mixer_ioctl(pdev, cmd, arg, mode, p);
	}

    	s = spltty();
	d = dsp_get_info(i_dev);
	getchns(i_dev, &rdch, &wrch, 0);

	kill = 0;
	if (wrch && (wrch->flags & CHN_F_DEAD))
		kill |= 1;
	if (rdch && (rdch->flags & CHN_F_DEAD))
		kill |= 2;
	if (kill == 3) {
		relchns(i_dev, rdch, wrch, 0);
		splx(s);
		return EINVAL;
	}
	if (kill & 1)
		wrch = NULL;
	if (kill & 2)
		rdch = NULL;

    	/*
     	 * all routines are called with int. blocked. Make sure that
     	 * ints are re-enabled when calling slow or blocking functions!
     	 */
    	switch(cmd) {
#ifdef OLDPCM_IOCTL
    	/*
     	 * we start with the new ioctl interface.
     	 */
    	case AIONWRITE:	/* how many bytes can write ? */
/*
		if (wrch && wrch->bufhard.dl)
			while (chn_wrfeed(wrch) == 0);
*/
		*arg_i = wrch? sndbuf_getfree(wrch->bufsoft) : 0;
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

	    		if (wrch)
				p->play_size = sndbuf_getblksz(wrch->bufsoft);
	    		if (rdch)
				p->rec_size = sndbuf_getblksz(rdch->bufsoft);
		}
		break;

    	case AIOSFMT:
		{
	    		snd_chan_param *p = (snd_chan_param *)arg;

	    		if (wrch) {
				CHN_LOCK(wrch);
				chn_setformat(wrch, p->play_format);
				chn_setspeed(wrch, p->play_rate);
				CHN_UNLOCK(wrch);
	    		}
	    		if (rdch) {
				CHN_LOCK(rdch);
				chn_setformat(rdch, p->rec_format);
				chn_setspeed(rdch, p->rec_rate);
				CHN_UNLOCK(rdch);
	    		}
		}
		/* FALLTHROUGH */

    	case AIOGFMT:
		{
	    		snd_chan_param *p = (snd_chan_param *)arg;

	    		p->play_rate = wrch? wrch->speed : 0;
	    		p->rec_rate = rdch? rdch->speed : 0;
	    		p->play_format = wrch? wrch->format : 0;
	    		p->rec_format = rdch? rdch->format : 0;
		}
		break;

    	case AIOGCAP:     /* get capabilities */
		{
	    		snd_capabilities *p = (snd_capabilities *)arg;
			struct pcmchan_caps *pcaps = NULL, *rcaps = NULL;
			dev_t pdev;

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
				p->formats |= (d->flags & SD_F_SIMPLEX)? 0 : AFMT_FULLDUPLEX;
			pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(device_get_unit(d->dev), SND_DEV_CTL, 0));
	    		p->mixers = 1; /* default: one mixer */
	    		p->inputs = pdev->si_drv1? mix_getdevs(pdev->si_drv1) : 0;
	    		p->left = p->right = 100;
			if (wrch)
				CHN_UNLOCK(wrch);
			if (rdch)
				CHN_UNLOCK(rdch);
		}
		break;

    	case AIOSTOP:
		if (*arg_i == AIOSYNC_PLAY && wrch)
			*arg_i = chn_abort(wrch);
		else if (*arg_i == AIOSYNC_CAPTURE && rdch)
			*arg_i = chn_abort(rdch);
		else {
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
/*		if (rdch && rdch->bufhard.dl)
			while (chn_rdfeed(rdch) == 0);
*/		*arg_i = rdch? sndbuf_getready(rdch->bufsoft) : 0;
		break;

    	case FIOASYNC: /*set/clear async i/o */
		DEB( printf("FIOASYNC\n") ; )
		break;

    	case SNDCTL_DSP_NONBLOCK:
    	case FIONBIO: /* set/clear non-blocking i/o */
		if (rdch)
			rdch->flags &= ~CHN_F_NBIO;
		if (wrch)
			wrch->flags &= ~CHN_F_NBIO;
		if (*arg_i) {
		    	if (rdch)
				rdch->flags |= CHN_F_NBIO;
		    	if (wrch)
				wrch->flags |= CHN_F_NBIO;
		}
		break;

    	/*
	 * Finally, here is the linux-compatible ioctl interface
	 */
#define THE_REAL_SNDCTL_DSP_GETBLKSIZE _IOWR('P', 4, int)
    	case THE_REAL_SNDCTL_DSP_GETBLKSIZE:
    	case SNDCTL_DSP_GETBLKSIZE:
		if (wrch)
			*arg_i = sndbuf_getblksz(wrch->bufsoft);
		else if (rdch)
			*arg_i = sndbuf_getblksz(rdch->bufsoft);
		else
			*arg_i = 0;
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
		if (wrch)
			chn_abort(wrch);
		if (rdch)
			chn_abort(rdch);
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
		*arg_i = wrch? wrch->speed : rdch->speed;
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
		if (*arg_i == 1 || *arg_i == 2) {
			tmp = 0;
			*arg_i = (*arg_i == 2)? AFMT_STEREO : 0;
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
		} else
			*arg_i = 0;
		break;

    	case SOUND_PCM_READ_CHANNELS:
		*arg_i = ((wrch? wrch->format : rdch->format) & AFMT_STEREO)? 2 : 1;
		break;

    	case SNDCTL_DSP_GETFMTS:	/* returns a mask of supported fmts */
		*arg_i = wrch? chn_getformats(wrch) : chn_getformats(rdch);
		break ;

    	case SNDCTL_DSP_SETFMT:	/* sets _one_ format */
		/* XXX locking */
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
		} else
			*arg_i = (wrch? wrch->format : rdch->format) & ~AFMT_STEREO;
		break;

    	case SNDCTL_DSP_SETFRAGMENT:
		/* XXX locking */
		DEB(printf("SNDCTL_DSP_SETFRAGMENT 0x%08x\n", *(int *)arg));
		{
			u_int32_t fragln = (*arg_i) & 0x0000ffff;
			u_int32_t maxfrags = ((*arg_i) & 0xffff0000) >> 16;
			u_int32_t fragsz;

			RANGE(fragln, 4, 16);
			fragsz = 1 << fragln;

			if (maxfrags == 0)
				maxfrags = CHN_2NDBUFMAXSIZE / fragsz;
			if (maxfrags < 2) {
				ret = EINVAL;
				break;
			}
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

    	case SNDCTL_DSP_GETISPACE: /* XXX Space for reading? Makes no sense... */
		/* return the size of data available in the input queue */
		{
	    		audio_buf_info *a = (audio_buf_info *)arg;
	    		if (rdch) {
	        		struct snd_dbuf *bs = rdch->bufsoft;

				CHN_LOCK(rdch);
				chn_rdupdate(rdch);
				a->bytes = sndbuf_getfree(bs);
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
		if (rdch && wrch && !(d->flags & SD_F_SIMPLEX))
			*arg_i |= DSP_CAP_DUPLEX;
		break;

    	case SOUND_PCM_READ_BITS:
        	*arg_i = ((wrch? wrch->format : rdch->format) & AFMT_16BIT)? 16 : 8;
		break;

    	case SNDCTL_DSP_SETTRIGGER:
		if (rdch) {
			CHN_LOCK(rdch);
			rdch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_INPUT) {
				rdch->flags |= CHN_F_TRIGGERED;
				chn_start(rdch, 1);
			} else
				rdch->flags |= CHN_F_NOTRIGGER;
			CHN_UNLOCK(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
			wrch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_OUTPUT) {
				wrch->flags |= CHN_F_TRIGGERED;
				chn_start(wrch, 1);
			} else
				wrch->flags |= CHN_F_NOTRIGGER;
		 	CHN_UNLOCK(wrch);
		}
		break;

    	case SNDCTL_DSP_GETTRIGGER:
		*arg_i = 0;
		if (wrch && wrch->flags & CHN_F_TRIGGERED)
			*arg_i |= PCM_ENABLE_OUTPUT;
		if (rdch && rdch->flags & CHN_F_TRIGGERED)
			*arg_i |= PCM_ENABLE_INPUT;
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

    	case SNDCTL_DSP_MAPINBUF:
    	case SNDCTL_DSP_MAPOUTBUF:
    	case SNDCTL_DSP_SETSYNCRO:
		/* undocumented */

    	case SNDCTL_DSP_SUBDIVIDE:
    	case SOUND_PCM_WRITE_FILTER:
    	case SOUND_PCM_READ_FILTER:
		/* dunno what these do, don't sound important */
    	default:
		DEB(printf("default ioctl chan%d fn 0x%08lx fail\n", chan, cmd));
		ret = EINVAL;
		break;
    	}
	relchns(i_dev, rdch, wrch, 0);
	splx(s);
    	return ret;
}

static int
dsp_poll(dev_t i_dev, int events, struct proc *p)
{
	struct pcm_channel *wrch = NULL, *rdch = NULL;
	intrmask_t s;
	int ret, e;

	s = spltty();
	ret = 0;
	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	if (wrch) {
		e = (events & (POLLOUT | POLLWRNORM));
		if (e)
			ret |= chn_poll(wrch, e, p);
	}
	if (rdch) {
		e = (events & (POLLIN | POLLRDNORM));
		if (e)
			ret |= chn_poll(rdch, e, p);
	}
	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	splx(s);
	return ret;
}

static int
dsp_mmap(dev_t i_dev, vm_offset_t offset, int nprot)
{
	struct pcm_channel *wrch = NULL, *rdch = NULL, *c;
	intrmask_t s;
	int ret;

	if (nprot & PROT_EXEC)
		return -1;

	s = spltty();
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
		splx(s);
		return -1;
	}
#else
	c = wrch;
#endif

	if (c == NULL) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);
		splx(s);
		return -1;
	}

	c->flags |= CHN_F_MAPPED;
	ret = atop(vtophys(((char *)sndbuf_getbuf(c->bufsoft)) + offset));
	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	splx(s);
	return ret;
}

int
dsp_register(int unit, int channel)
{
	make_dev(&dsp_cdevsw, PCMMKMINOR(unit, SND_DEV_DSP, channel),
		 UID_ROOT, GID_WHEEL, 0666, "dsp%d.%d", unit, channel);
	make_dev(&dsp_cdevsw, PCMMKMINOR(unit, SND_DEV_DSP16, channel),
		 UID_ROOT, GID_WHEEL, 0666, "dspW%d.%d", unit, channel);
	make_dev(&dsp_cdevsw, PCMMKMINOR(unit, SND_DEV_AUDIO, channel),
		 UID_ROOT, GID_WHEEL, 0666, "audio%d.%d", unit, channel);

	return 0;
}

int
dsp_unregister(int unit, int channel)
{
	dev_t pdev;

	pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_DSP, channel));
	destroy_dev(pdev);
	pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_DSP16, channel));
	destroy_dev(pdev);
	pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(unit, SND_DEV_AUDIO, channel));
	destroy_dev(pdev);

	return 0;
}

#ifdef USING_DEVFS
static void
dsp_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	dev_t pdev;
	int i, cont, unit, devtype;
	int devtypes[3] = {SND_DEV_DSP, SND_DEV_DSP16, SND_DEV_AUDIO};
	char *devnames[3] = {"dsp", "dspW", "audio"};

	if (*dev != NODEV)
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

	cont = 1;
	for (i = 0; cont; i++) {
		pdev = makedev(SND_CDEV_MAJOR, PCMMKMINOR(unit, devtype, i));
		if (pdev->si_flags & SI_NAMED) {
			if ((pdev->si_drv1 == NULL) && (pdev->si_drv2 == NULL)) {
				*dev = pdev;
				return;
			}
		} else {
			cont = 0;
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


