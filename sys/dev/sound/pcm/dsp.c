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

static int getchns(snddev_info *d, int chan, pcm_channel **rdch, pcm_channel **wrch);

static pcm_channel *
allocchn(snddev_info *d, int direction)
{
	pcm_channel *chns = (direction == PCMDIR_PLAY)? d->play : d->rec;
	int i, cnt = (direction == PCMDIR_PLAY)? d->playcount : d->reccount;
	for (i = 0; i < cnt; i++) {
		if (!(chns[i].flags & (CHN_F_BUSY | CHN_F_DEAD))) {
			chns[i].flags |= CHN_F_BUSY;
			return &chns[i];
		}
	}
	return NULL;
}

static int
getchns(snddev_info *d, int chan, pcm_channel **rdch, pcm_channel **wrch)
{
	KASSERT((d->flags & SD_F_PRIO_SET) != SD_F_PRIO_SET, \
		("getchns: read and write both prioritised"));

	if ((d->flags & SD_F_SIMPLEX) && (d->flags & SD_F_PRIO_SET)) {
		*rdch = (d->flags & SD_F_PRIO_RD)? d->arec[chan] : &d->fakechan;
		*wrch = (d->flags & SD_F_PRIO_WR)? d->aplay[chan] : &d->fakechan;
		d->fakechan.flags |= CHN_F_BUSY;
	} else {
		*rdch = d->arec[chan];
		*wrch = d->aplay[chan];
	}
	return 0;
}

static void
setchns(snddev_info *d, int chan)
{
	KASSERT((d->flags & SD_F_PRIO_SET) != SD_F_PRIO_SET, \
		("getchns: read and write both prioritised"));
	d->flags |= SD_F_DIR_SET;
	if (d->swap) d->swap(d->devinfo, (d->flags & SD_F_PRIO_WR)? PCMDIR_PLAY : PCMDIR_REC);
}

int
dsp_open(snddev_info *d, int chan, int oflags, int devtype)
{
	pcm_channel *rdch, *wrch;
	u_int32_t fmt;

	if (chan >= d->chancount) return ENODEV;
	if ((d->flags & SD_F_SIMPLEX) && (d->ref[chan] > 0)) return EBUSY;
	rdch = d->arec[chan];
	wrch = d->aplay[chan];
	if (oflags & FREAD) {
		if (rdch == NULL) {
			rdch = allocchn(d, PCMDIR_REC);
			if (!rdch) return EBUSY;
		} else return EBUSY;
	}
	if (oflags & FWRITE) {
		if (wrch == NULL) {
			wrch = allocchn(d, PCMDIR_PLAY);
			if (!wrch) {
				if (rdch && (oflags & FREAD))
					rdch->flags &= ~CHN_F_BUSY;
				return EBUSY;
			}
		} else return EBUSY;
	}
	d->aplay[chan] = wrch;
	d->arec[chan] = rdch;
	d->ref[chan]++;
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
		return ENXIO;
	}

	if (rdch && (oflags & FREAD)) {
	        chn_reset(rdch, fmt);
		if (oflags & O_NONBLOCK) rdch->flags |= CHN_F_NBIO;
	}
	if (wrch && (oflags & FWRITE)) {
	        chn_reset(wrch, fmt);
		if (oflags & O_NONBLOCK) wrch->flags |= CHN_F_NBIO;
	}
	return 0;
}

int
dsp_close(snddev_info *d, int chan, int devtype)
{
	pcm_channel *rdch, *wrch;

	d->ref[chan] = 0;
#if 0
	/* enable this if/when every close() is propagated here */
	if (d->ref[chan]) return 0;
#endif
	d->flags &= ~SD_F_TRANSIENT;
	rdch = d->arec[chan];
	wrch = d->aplay[chan];

	if (rdch) {
		chn_abort(rdch);
		rdch->flags &= ~(CHN_F_BUSY | CHN_F_RUNNING | CHN_F_MAPPED);
		chn_reset(rdch, 0);
	}
	if (wrch) {
		chn_flush(wrch);
		wrch->flags &= ~(CHN_F_BUSY | CHN_F_RUNNING | CHN_F_MAPPED);
		chn_reset(wrch, 0);
	}
	d->aplay[chan] = NULL;
	d->arec[chan] = NULL;
	return 0;
}

int
dsp_read(snddev_info *d, int chan, struct uio *buf, int flag)
{
	pcm_channel *rdch, *wrch;

	if (!(d->flags & SD_F_PRIO_SET)) d->flags |= SD_F_PRIO_RD;
	if (!(d->flags & SD_F_DIR_SET)) setchns(d, chan);
	getchns(d, chan, &rdch, &wrch);
	KASSERT(rdch, ("dsp_read: nonexistant channel"));
	KASSERT(rdch->flags & CHN_F_BUSY, ("dsp_read: nonbusy channel"));
	if (rdch->flags & (CHN_F_MAPPED | CHN_F_DEAD)) return EINVAL;
	if (!(rdch->flags & CHN_F_RUNNING))
		rdch->flags |= CHN_F_RUNNING;
	return chn_read(rdch, buf);
}

int
dsp_write(snddev_info *d, int chan, struct uio *buf, int flag)
{
	pcm_channel *rdch, *wrch;

	if (!(d->flags & SD_F_PRIO_SET)) d->flags |= SD_F_PRIO_WR;
	if (!(d->flags & SD_F_DIR_SET)) setchns(d, chan);
	getchns(d, chan, &rdch, &wrch);
	KASSERT(wrch, ("dsp_write: nonexistant channel"));
	KASSERT(wrch->flags & CHN_F_BUSY, ("dsp_write: nonbusy channel"));
	if (wrch->flags & (CHN_F_MAPPED | CHN_F_DEAD)) return EINVAL;
	if (!(wrch->flags & CHN_F_RUNNING))
		wrch->flags |= CHN_F_RUNNING;
	return chn_write(wrch, buf);
}

int
dsp_ioctl(snddev_info *d, int chan, u_long cmd, caddr_t arg)
{
    	int ret = 0, *arg_i = (int *)arg;
    	u_long s;
    	pcm_channel *wrch = NULL, *rdch = NULL;

	rdch = d->arec[chan];
	wrch = d->aplay[chan];

	if (rdch && (rdch->flags & CHN_F_DEAD))
		rdch = NULL;
	if (wrch && (wrch->flags & CHN_F_DEAD))
		wrch = NULL;
	if (!(rdch || wrch))
		return EINVAL;
    	/*
     	 * all routines are called with int. blocked. Make sure that
     	 * ints are re-enabled when calling slow or blocking functions!
     	 */
    	s = spltty();
    	switch(cmd) {
#ifdef OLDPCM_IOCTL
    	/*
     	 * we start with the new ioctl interface.
     	 */
    	case AIONWRITE:	/* how many bytes can write ? */
		if (wrch && wrch->buffer.dl)
			while (chn_wrfeed(wrch) > 0);
		*arg_i = wrch? wrch->buffer2nd.fl : 0;
		break;

    	case AIOSSIZE:     /* set the current blocksize */
		{
	    		struct snd_size *p = (struct snd_size *)arg;
	    		if (wrch)
				chn_setblocksize(wrch, 2, p->play_size);
	    		if (rdch)
				chn_setblocksize(rdch, 2, p->rec_size);
		}
		/* FALLTHROUGH */
    	case AIOGSIZE:	/* get the current blocksize */
		{
	    		struct snd_size *p = (struct snd_size *)arg;
	    		if (wrch) p->play_size = wrch->buffer2nd.blksz;
	    		if (rdch) p->rec_size = rdch->buffer2nd.blksz;
		}
		break;

    	case AIOSFMT:
		{
	    		snd_chan_param *p = (snd_chan_param *)arg;
	    		if (wrch) {
				chn_setformat(wrch, p->play_format);
				chn_setspeed(wrch, p->play_rate);
	    		}
	    		if (rdch) {
				chn_setformat(rdch, p->rec_format);
				chn_setspeed(rdch, p->rec_rate);
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
			pcmchan_caps *pcaps = NULL, *rcaps = NULL;
			if (rdch) rcaps = chn_getcaps(rdch);
			if (wrch) pcaps = chn_getcaps(wrch);
	    		p->rate_min = max(rcaps? rcaps->minspeed : 0,
	                      		  pcaps? pcaps->minspeed : 0);
	    		p->rate_max = min(rcaps? rcaps->maxspeed : 1000000,
	                      		  pcaps? pcaps->maxspeed : 1000000);
	    		p->bufsize = min(rdch? rdch->buffer2nd.bufsize : 1000000,
	                     		 wrch? wrch->buffer2nd.bufsize : 1000000);
			/* XXX bad on sb16 */
	    		p->formats = (rdch? chn_getformats(rdch) : 0xffffffff) &
			 	     (wrch? chn_getformats(wrch) : 0xffffffff);
			if (rdch && wrch)
				p->formats |= (d->flags & SD_F_SIMPLEX)? 0 : AFMT_FULLDUPLEX;
	    		p->mixers = 1; /* default: one mixer */
	    		p->inputs = d->mixer.devs;
	    		p->left = p->right = 100;
		}
		break;

    	case AIOSTOP:
		if (*arg_i == AIOSYNC_PLAY && wrch) *arg_i = chn_abort(wrch);
		else if (*arg_i == AIOSYNC_CAPTURE && rdch) *arg_i = chn_abort(rdch);
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
		if (rdch && rdch->buffer.dl)
			while (chn_rdfeed(rdch) > 0);
		*arg_i = rdch? rdch->buffer2nd.rl : 0;
		break;

    	case FIOASYNC: /*set/clear async i/o */
		DEB( printf("FIOASYNC\n") ; )
		break;

    	case SNDCTL_DSP_NONBLOCK:
    	case FIONBIO: /* set/clear non-blocking i/o */
		if (rdch) rdch->flags &= ~CHN_F_NBIO;
		if (wrch) wrch->flags &= ~CHN_F_NBIO;
		if (*arg_i) {
		    	if (rdch) rdch->flags |= CHN_F_NBIO;
		    	if (wrch) wrch->flags |= CHN_F_NBIO;
		}
		break;

    	/*
	 * Finally, here is the linux-compatible ioctl interface
	 */
#define THE_REAL_SNDCTL_DSP_GETBLKSIZE _IOWR('P', 4, int)
    	case THE_REAL_SNDCTL_DSP_GETBLKSIZE:
    	case SNDCTL_DSP_GETBLKSIZE:
		if (wrch)
			*arg_i = wrch->buffer2nd.blksz;
		else if (rdch)
			*arg_i = rdch->buffer2nd.blksz;
		else
			*arg_i = 0;
		break ;

    	case SNDCTL_DSP_SETBLKSIZE:
		RANGE(*arg_i, 16, 65536);
		if (wrch) chn_setblocksize(wrch, 2, *arg_i);
		if (rdch) chn_setblocksize(rdch, 2, *arg_i);
		break;

    	case SNDCTL_DSP_RESET:
		DEB(printf("dsp reset\n"));
		splx(s);
		if (wrch) chn_abort(wrch);
		if (rdch) chn_abort(rdch);
		break;

    	case SNDCTL_DSP_SYNC:
		DEB(printf("dsp sync\n"));
		splx(s);
		if (wrch) chn_sync(wrch, wrch->buffer2nd.bufsize - 4);
		break;

    	case SNDCTL_DSP_SPEED:
		splx(s);
		if (wrch)
			ret = chn_setspeed(wrch, *arg_i);
		if (rdch && ret == 0)
			ret = chn_setspeed(rdch, *arg_i);
		/* fallthru */

    	case SOUND_PCM_READ_RATE:
		*arg_i = wrch? wrch->speed : rdch->speed;
		break;

    	case SNDCTL_DSP_STEREO:
		splx(s);
		if (wrch)
			ret = chn_setformat(wrch, (wrch->format & ~AFMT_STEREO) |
					((*arg_i)? AFMT_STEREO : 0));
		if (rdch && ret == 0)
			ret = chn_setformat(rdch, (rdch->format & ~AFMT_STEREO) |
				        ((*arg_i)? AFMT_STEREO : 0));
		*arg_i = ((wrch? wrch->format : rdch->format) & AFMT_STEREO)? 1 : 0;
		break;

    	case SOUND_PCM_WRITE_CHANNELS:
/*	case SNDCTL_DSP_CHANNELS: ( == SOUND_PCM_WRITE_CHANNELS) */
		splx(s);
		if (*arg_i == 1 || *arg_i == 2) {
	  		if (wrch)
				ret = chn_setformat(wrch, (wrch->format & ~AFMT_STEREO) |
					((*arg_i == 2)? AFMT_STEREO : 0));
			if (rdch && ret == 0)
				ret = chn_setformat(rdch, (rdch->format & ~AFMT_STEREO) |
				        ((*arg_i == 2)? AFMT_STEREO : 0));
			*arg_i = ((wrch? wrch->format : rdch->format) & AFMT_STEREO)? 2 : 1;
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
		splx(s);
		if ((*arg_i != AFMT_QUERY)) {
			if (wrch)
				ret = chn_setformat(wrch, (*arg_i) | (wrch->format & AFMT_STEREO));
			if (rdch && ret == 0)
				ret = chn_setformat(rdch, (*arg_i) | (rdch->format & AFMT_STEREO));
		}
		*arg_i = (wrch? wrch->format: rdch->format) & ~AFMT_STEREO;
		break;

    	case SNDCTL_DSP_SUBDIVIDE:
		/* XXX watch out, this is RW! */
		printf("SNDCTL_DSP_SUBDIVIDE unimplemented\n");
		break;

    	case SNDCTL_DSP_SETFRAGMENT:
		DEB(printf("SNDCTL_DSP_SETFRAGMENT 0x%08x\n", *(int *)arg));
		{
			pcm_channel *c = wrch? wrch : rdch;
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
		    	if (rdch)
				ret = chn_setblocksize(rdch, maxfrags, fragsz);
		    	if (wrch && ret == 0)
				ret = chn_setblocksize(wrch, maxfrags, fragsz);

			fragsz = c->buffer2nd.blksz;
			fragln = 0;
			while (fragsz > 1) {
				fragln++;
				fragsz >>= 1;
			}
	    		*arg_i = (c->buffer2nd.blkcnt << 16) | fragln;
		}
		break;

    	case SNDCTL_DSP_GETISPACE: /* XXX Space for reading? Makes no sense... */
		/* return the size of data available in the input queue */
		{
	    		audio_buf_info *a = (audio_buf_info *)arg;
	    		if (rdch) {
	        		snd_dbuf *b = &rdch->buffer;
	        		snd_dbuf *bs = &rdch->buffer2nd;
				if (b->dl && !(rdch->flags & CHN_F_MAPPED))
					/*
					 * Suck up the secondary and DMA buffer.
					 * chn_rdfeed*() takes care of the alignment.
					 */
					while (chn_rdfeed(rdch) > 0);
				a->bytes = bs->rl;
	        		a->fragments = a->bytes / rdch->buffer2nd.blksz;
	        		a->fragstotal = rdch->buffer2nd.blkcnt;
	        		a->fragsize = rdch->buffer2nd.blksz;
	    		}
		}
		break;

    	case SNDCTL_DSP_GETOSPACE:
		/* return space available in the output queue */
		{
	    		audio_buf_info *a = (audio_buf_info *)arg;
	    		if (wrch) {
	        		snd_dbuf *b = &wrch->buffer;
	        		snd_dbuf *bs = &wrch->buffer2nd;
				if (b->dl && !(wrch->flags & CHN_F_MAPPED)) {
					/*
					 * Fill up the secondary and DMA buffer.
					 * chn_wrfeed*() takes care of the alignment.
					 * Check for underflow before writing into the buffers.
					 */
					chn_checkunderflow(wrch);
					while (chn_wrfeed(wrch) > 0);
				}
				a->bytes = bs->fl;
	        		a->fragments = a->bytes / wrch->buffer2nd.blksz;
	        		a->fragstotal = wrch->buffer2nd.blkcnt;
	        		a->fragsize = wrch->buffer2nd.blksz;
	    		}
		}
		break;

    	case SNDCTL_DSP_GETIPTR:
		{
	    		count_info *a = (count_info *)arg;
	    		if (rdch) {
	        		snd_dbuf *b = &rdch->buffer;
	        		snd_dbuf *bs = &rdch->buffer2nd;
	        		if (b->dl && !(rdch->flags & CHN_F_MAPPED))
					/*
					 * Suck up the secondary and DMA buffer.
					 * chn_rdfeed*() takes care of the alignment.
					 */
					while (chn_rdfeed(rdch) > 0);
	        		a->bytes = bs->total;
	        		a->blocks = rdch->blocks;
	        		a->ptr = bs->rp;
				rdch->blocks = 0;
	    		} else ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETOPTR:
		{
	    		count_info *a = (count_info *)arg;
	    		if (wrch) {
    	        		snd_dbuf *b = &wrch->buffer;
	        		snd_dbuf *bs = &wrch->buffer2nd;
				if (b->dl && !(wrch->flags & CHN_F_MAPPED)) {
					/*
					 * Fill up the secondary and DMA buffer.
					 * chn_wrfeed*() takes care of the alignment.
					 * Check for underflow before writing into the buffers.
					 */
					chn_checkunderflow(wrch);
					while (chn_wrfeed(wrch) > 0);
				}
	        		a->bytes = bs->total;
	        		a->blocks = wrch->blocks;
	        		a->ptr = bs->rp;
				wrch->blocks = 0;
	    		} else ret = EINVAL;
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
			rdch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_INPUT)
				rdch->flags |= CHN_F_TRIGGERED;
			else
				rdch->flags |= CHN_F_NOTRIGGER;
			chn_intr(rdch);
		}
		if (wrch) {
			wrch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_OUTPUT)
				wrch->flags |= CHN_F_TRIGGERED;
			else
				wrch->flags |= CHN_F_NOTRIGGER;
			chn_intr(wrch);
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
			snd_dbuf *b = &wrch->buffer;
	        	snd_dbuf *bs = &wrch->buffer2nd;
			if (b->dl) {
				chn_checkunderflow(wrch);
				if (!(wrch->flags & CHN_F_MAPPED))
					while (chn_wrfeed(wrch) > 0);
			}
			*arg_i = b->rl + bs->rl;
		} else
			ret = EINVAL;
		break;

    	case SNDCTL_DSP_MAPINBUF:
    	case SNDCTL_DSP_MAPOUTBUF:
    	case SNDCTL_DSP_SETSYNCRO:
		/* undocumented */

    	case SNDCTL_DSP_POST:
    	case SOUND_PCM_WRITE_FILTER:
    	case SOUND_PCM_READ_FILTER:
		/* dunno what these do, don't sound important */
    	default:
		DEB(printf("default ioctl chan%d fn 0x%08lx fail\n", chan, cmd));
		ret = EINVAL;
		break;
    	}
    	splx(s);
    	return ret;
}

int
dsp_poll(snddev_info *d, int chan, int events, struct proc *p)
{
	int ret = 0, e;
	pcm_channel *wrch = NULL, *rdch = NULL;

	getchns(d, chan, &rdch, &wrch);
	e = events & (POLLOUT | POLLWRNORM);
	if (wrch && e) ret |= chn_poll(wrch, e, p);
	e = events & (POLLIN | POLLRDNORM);
	if (rdch && e) ret |= chn_poll(rdch, e, p);
	return ret;
}

int
dsp_mmap(snddev_info *d, int chan, vm_offset_t offset, int nprot)
{
	pcm_channel *wrch = NULL, *rdch = NULL, *c = NULL;

	getchns(d, chan, &rdch, &wrch);
	/* XXX this is broken by line 204 of vm/device_pager.c, so force write buffer */
	if (1 || (wrch && (nprot & PROT_WRITE)))
		c = wrch;
	else if (rdch && (nprot & PROT_READ))
		c = rdch;
	if (c && (c->format == c->buffer.fmt)) {
		c->flags |= CHN_F_MAPPED;
		return atop(vtophys(c->buffer2nd.buf + offset));
	} else
		return -1;
}

