/*
 * sound/dmabuf.c
 * 
 * The DMA buffer manager for digitized voice applications
 * 
 * Copyright by Hannu Savolainen 1993, 1994, 1995
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#include <i386/isa/sound/sound_config.h>

#if defined(CONFIG_AUDIO) || defined(CONFIG_GUS)

static int     *in_sleeper[MAX_AUDIO_DEV] = {NULL};
static volatile struct snd_wait in_sleep_flag[MAX_AUDIO_DEV] = {{0}};
static int     *out_sleeper[MAX_AUDIO_DEV] = {NULL};
static volatile struct snd_wait out_sleep_flag[MAX_AUDIO_DEV] = {{0}};

static int      ndmaps = 0;

#define MAX_DMAP (MAX_AUDIO_DEV*2)

static struct dma_buffparms dmaps[MAX_DMAP] = {{0}};
/*
 * Primitive way to allocate such a large array. Needs dynamic run-time
 * alloction.
 */

static int      space_in_queue(int dev);

static void     dma_reset_output(int dev);
static void     dma_reset_input(int dev);

static void
reorganize_buffers(int dev, struct dma_buffparms * dmap)
{
    /*
     * This routine breaks the physical device buffers to logical ones.
     */

    struct audio_operations *dsp_dev = audio_devs[dev];
    u_int        sr, nc;
    int             bsz, sz, n, i;

    if (dmap->fragment_size == 0) {
	/* Compute the fragment size using the default algorithm */

	sr = dsp_dev->ioctl(dev, SOUND_PCM_READ_RATE, 0, 1);
	nc = dsp_dev->ioctl(dev, SOUND_PCM_READ_CHANNELS, 0, 1);
	sz = dsp_dev->ioctl(dev, SOUND_PCM_READ_BITS, 0, 1);

	if (sz == 8)
	    dmap->neutral_byte = 254;
	else
	    dmap->neutral_byte = 0x00;

	if (sr < 1 || nc < 1 || sz < 1) {
	    printf("Warning: Invalid PCM parameters[%d] sr=%d, nc=%d, sz=%d\n",
		       dev, sr, nc, sz);
	    sr = DSP_DEFAULT_SPEED;
	    nc = 1;
	    sz = 8;
	}
	sz = sr * nc * sz;

	sz /= 8;	/* #bits -> #bytes */

	/*
	 * Compute a buffer size for time not exeeding 1 second.
	 * Usually this algorithm gives a buffer size for 0.5 to 1.0
	 * seconds of sound (using the current speed, sample size and
	 * #channels).
	 */

	bsz = dsp_dev->buffsize;
	while (bsz > sz)
	    bsz /= 2;

	if (bsz == dsp_dev->buffsize)
	    bsz /= 2;	/* Needs at least 2 buffers */

	if (dmap->subdivision == 0)	/* Not already set */
	    dmap->subdivision = 1;	/* Init to default value */
	else
	    bsz /= dmap->subdivision;

	if (bsz < 16)
	    bsz = 16;	/* Just a sanity check */

	dmap->fragment_size = bsz;
    } else {
	/*
	 * The process has specified the buffer sice with
	 * SNDCTL_DSP_SETFRAGMENT or the buffer sice computation has
	 * already been done.
	 */

	if (dmap->fragment_size > (audio_devs[dev]->buffsize / 2))
	    dmap->fragment_size = (audio_devs[dev]->buffsize / 2);
	bsz = dmap->fragment_size;
    }

    bsz &= ~0x03;		/* Force size which is multiple of 4 bytes */
#ifdef OS_DMA_ALIGN_CHECK
    OS_DMA_ALIGN_CHECK(bsz);
#endif

    n = dsp_dev->buffsize / bsz;

    if (n > MAX_SUB_BUFFERS)
	n = MAX_SUB_BUFFERS;

    if (n > dmap->max_fragments)
	n = dmap->max_fragments;
    dmap->nbufs = n;
    dmap->bytes_in_use = n * bsz;

    for (i = 0; i < dmap->nbufs; i++) {
	dmap->counts[i] = 0;
    }

    if (dmap->raw_buf)
	fillw (dmap->neutral_byte, dmap->raw_buf,
	       dmap->bytes_in_use/2);

    dmap->flags |= DMA_ALLOC_DONE;

}

static void
dma_init_buffers(int dev, struct dma_buffparms * dmap)
{
    if (dmap == audio_devs[dev]->dmap_out) {
	out_sleep_flag[dev].aborting = 0;
	out_sleep_flag[dev].mode = WK_NONE;
    } else {
	in_sleep_flag[dev].aborting = 0;
	in_sleep_flag[dev].mode = WK_NONE;
    }

    dmap->flags = DMA_BUSY;	/* Other flags off */
    dmap->qlen = dmap->qhead = dmap->qtail = 0;
    dmap->nbufs = 1;
    dmap->bytes_in_use = audio_devs[dev]->buffsize;

    dmap->dma_mode = DMODE_NONE;
    dmap->mapping_flags = 0;
    dmap->neutral_byte = 0x00;
}

static int
open_dmap(int dev, int mode, struct dma_buffparms * dmap, int chan)
{
    if (dmap->flags & DMA_BUSY)
	return -(EBUSY);

#ifdef RUNTIME_DMA_ALLOC
    {
	int             err;

	if ((err = sound_alloc_dmap(dev, dmap, chan)) < 0)
	    return err;
    }
#endif

    if (dmap->raw_buf == NULL)
	return -(ENOSPC);	/* Memory allocation failed during boot */

    if (0) {
	printf("Unable to grab(2) DMA%d for the audio driver\n", chan);
	return -(EBUSY);
    }
    dmap->open_mode = mode;
    dmap->subdivision = dmap->underrun_count = 0;
    dmap->fragment_size = 0;
    dmap->max_fragments = 65536;	/* Just a large value */
    dmap->byte_counter = 0;
    isa_dma_acquire(chan);

    dma_init_buffers(dev, dmap);

    return 0;
}

static void
close_dmap(int dev, struct dma_buffparms * dmap, int chan)
{
    if (dmap->flags & DMA_BUSY)
	dmap->dma_mode = DMODE_NONE;
    dmap->flags &= ~DMA_BUSY;
    isa_dma_release(chan);
#ifdef RUNTIME_DMA_ALLOC
    sound_free_dmap(dev, dmap);
#endif
}

int
DMAbuf_open(int dev, int mode)
{
    int             retval;
    struct dma_buffparms *dmap_in = NULL;
    struct dma_buffparms *dmap_out = NULL;

    if (dev >= num_audiodevs) {
	printf("PCM device %d not installed.\n", dev);
	return -(ENXIO);
    }
    if (!audio_devs[dev]) {
	printf("PCM device %d not initialized\n", dev);
	return -(ENXIO);
    }
    if (!(audio_devs[dev]->flags & DMA_DUPLEX)) {
	audio_devs[dev]->dmap_in = audio_devs[dev]->dmap_out;
	audio_devs[dev]->dmachan2 = audio_devs[dev]->dmachan1;
    }
    if ((retval = audio_devs[dev]->open(dev, mode)) < 0)
	return retval;

    dmap_out = audio_devs[dev]->dmap_out;
    dmap_in = audio_devs[dev]->dmap_in;

    if ((retval = open_dmap(dev, mode, dmap_out, audio_devs[dev]->dmachan1)) < 0) {
	audio_devs[dev]->close(dev);
	return retval;
    }
    audio_devs[dev]->enable_bits = mode;

    if (audio_devs[dev]->flags & DMA_DUPLEX && dmap_out != dmap_in) {
	if ((retval = open_dmap(dev, mode, dmap_in, audio_devs[dev]->dmachan2)) < 0) {
	    audio_devs[dev]->close(dev);
	    close_dmap(dev, dmap_out, audio_devs[dev]->dmachan1);
	    return retval;
	}
    }
    audio_devs[dev]->open_mode = mode;
    audio_devs[dev]->go = 1;

    in_sleep_flag[dev].aborting = 0;
    in_sleep_flag[dev].mode = WK_NONE;

    out_sleep_flag[dev].aborting = 0;
    out_sleep_flag[dev].mode = WK_NONE;

    audio_devs[dev]->ioctl(dev, SOUND_PCM_WRITE_BITS, (ioctl_arg) 8, 1);
    audio_devs[dev]->ioctl(dev, SOUND_PCM_WRITE_CHANNELS, (ioctl_arg) 1, 1);
    audio_devs[dev]->ioctl(dev, SOUND_PCM_WRITE_RATE, (ioctl_arg) DSP_DEFAULT_SPEED, 1);

	return 0;
}

static void
dma_reset(int dev)
{
    u_long   flags;

    flags = splhigh();
    audio_devs[dev]->reset(dev);
    splx(flags);

    dma_reset_output(dev);

    if (audio_devs[dev]->flags & DMA_DUPLEX)
	dma_reset_input(dev);
}

static void
dma_reset_output(int dev)
{
    u_long   flags;

    flags = splhigh();
    if (!(audio_devs[dev]->flags & DMA_DUPLEX) ||
	!audio_devs[dev]->halt_output)
	audio_devs[dev]->reset(dev);
    else
	audio_devs[dev]->halt_output(dev);
    splx(flags);

    dma_init_buffers(dev, audio_devs[dev]->dmap_out);
    reorganize_buffers(dev, audio_devs[dev]->dmap_out);
}

static void
dma_reset_input(int dev)
{
    u_long   flags;

    flags = splhigh();
    if (!(audio_devs[dev]->flags & DMA_DUPLEX) ||
	!audio_devs[dev]->halt_input)
	audio_devs[dev]->reset(dev);
    else
	audio_devs[dev]->halt_input(dev);
    splx(flags);

    dma_init_buffers(dev, audio_devs[dev]->dmap_in);
    reorganize_buffers(dev, audio_devs[dev]->dmap_in);
}

static int
dma_sync(int dev)
{
    u_long   flags;
    int i = 0;

    if (!audio_devs[dev]->go && (!audio_devs[dev]->enable_bits & PCM_ENABLE_OUTPUT))
	return 0;

    if (audio_devs[dev]->dmap_out->dma_mode == DMODE_OUTPUT) {
	flags = splhigh();

	out_sleep_flag[dev].aborting = 0;
#ifdef ALLOW_BUFFER_MAPPING
	if(audio_devs[dev]->dmap_out->mapping_flags & DMA_MAP_MAPPED &&
           audio_devs[dev]->dmap_out->qlen) {
		splx(flags);
	
		return audio_devs[dev]->dmap_out->qlen;
	}

#endif
	while (!PROCESS_ABORTING (out_sleep_flag[dev])
	     && audio_devs[dev]->dmap_out->qlen){
	  int    chn;

	  out_sleeper[dev] = &chn;
	  DO_SLEEP1(chn, out_sleep_flag[dev], 10 * hz);
	  if (TIMED_OUT (out_sleep_flag[dev]) ) {
	        
		splx(flags);
	
		return audio_devs[dev]->dmap_out->qlen;

	  }
	}


	splx(flags);

	/*
	 * Some devices such as GUS have huge amount of on board RAM
	 * for the audio data. We have to wait until the device has
	 * finished playing.
	 */

	flags = splhigh();
	if (audio_devs[dev]->local_qlen) { /* Device has hidden buffers */
	    while (!(PROCESS_ABORTING (out_sleep_flag[dev]))
		   && audio_devs[dev]->local_qlen(dev)) {
		int      chn;
		out_sleeper[dev] = &chn;
		DO_SLEEP(chn, out_sleep_flag[dev], 10 * hz);

	    }
	}
	splx(flags);
    }
    return audio_devs[dev]->dmap_out->qlen;
}

int
DMAbuf_release(int dev, int mode)
{
    u_long   flags;

    if (!((out_sleep_flag[dev].aborting))
	    && (audio_devs[dev]->dmap_out->dma_mode == DMODE_OUTPUT)) {
	dma_sync(dev);
    }
    flags = splhigh();

    audio_devs[dev]->close(dev);

    close_dmap(dev, audio_devs[dev]->dmap_out, audio_devs[dev]->dmachan1);

    if (audio_devs[dev]->flags & DMA_DUPLEX)
	close_dmap(dev, audio_devs[dev]->dmap_in, audio_devs[dev]->dmachan2);
    audio_devs[dev]->open_mode = 0;

    splx(flags);

    return 0;
}

static int
activate_recording(int dev, struct dma_buffparms * dmap)
{
    if (!(audio_devs[dev]->enable_bits & PCM_ENABLE_INPUT))
	return 0;

    if (dmap->flags & DMA_RESTART) {
	dma_reset_input(dev);
	dmap->flags &= ~DMA_RESTART;
    }
    if (dmap->dma_mode == DMODE_OUTPUT) {	/* Direction change */
	dma_sync(dev);
	dma_reset(dev);
	dmap->dma_mode = DMODE_NONE;
    }
    if (!(dmap->flags & DMA_ALLOC_DONE))
	reorganize_buffers(dev, dmap);

    if (!dmap->dma_mode) {
	int             err;

	if ((err = audio_devs[dev]->prepare_for_input(dev,
			   dmap->fragment_size, dmap->nbufs)) < 0) {
	    return err;
	}
	dmap->dma_mode = DMODE_INPUT;
    }
    if (!(dmap->flags & DMA_ACTIVE)) {
	audio_devs[dev]->start_input(dev,
		dmap->raw_buf_phys + dmap->qtail * dmap->fragment_size,
		dmap->fragment_size, 0,
		!(audio_devs[dev]->flags & DMA_AUTOMODE) ||
		     !(dmap->flags & DMA_STARTED));
	dmap->flags |= DMA_ACTIVE | DMA_STARTED;
	if (audio_devs[dev]->trigger)
	    audio_devs[dev]->trigger(dev,
		    audio_devs[dev]->enable_bits * audio_devs[dev]->go);
    }
    return 0;
}

int
DMAbuf_getrdbuffer(int dev, char **buf, int *len, int dontblock)
{
    u_long   flags;
    int             err = EIO;
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_in;

    flags = splhigh();
#ifdef ALLOW_BUFFER_MAPPING
    if (audio_devs[dev]->dmap_in->mapping_flags & DMA_MAP_MAPPED) {
	printf("Sound: Can't read from mmapped device (1)\n");
	return -(EINVAL);
    } else
#endif
    if (!dmap->qlen) {
	int             timeout;

	if ((err = activate_recording(dev, dmap)) < 0) {
	    splx(flags);
	    return err;
	}
	/* Wait for the next block */

	if (dontblock) {
	    splx(flags);
	    return -(EAGAIN);
	}
	if (!(audio_devs[dev]->enable_bits & PCM_ENABLE_INPUT) &
		audio_devs[dev]->go) {
	    splx(flags);
	    return -(EAGAIN);
	}
	if (!audio_devs[dev]->go)
	    timeout = 0;
	else
	    timeout = 2 * hz;

	{
	    int  chn;

	    in_sleeper[dev] = &chn;
	    DO_SLEEP(chn, in_sleep_flag[dev], timeout);

	};
	/* XXX note -- nobody seems to set the mode to WK_TIMEOUT - lr */
	if ((in_sleep_flag[dev].mode & WK_TIMEOUT)) {
	    /* XXX hey, we are in splhigh here ? lr 970705 */
	    printf("Sound: DMA (input) timed out - IRQ/DRQ config error?\n");
	    err = EIO;
	    audio_devs[dev]->reset(dev);
	    in_sleep_flag[dev].aborting = 1;
	} else
	    err = EINTR;
    }
    splx(flags);

    if (!dmap->qlen)
	return -(err);

    *buf = &dmap->raw_buf[dmap->qhead * dmap->fragment_size + dmap->counts[dmap->qhead]];
    *len = dmap->fragment_size - dmap->counts[dmap->qhead];

    return dmap->qhead;
}

int
DMAbuf_rmchars(int dev, int buff_no, int c)
{
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_in;

    int             p = dmap->counts[dmap->qhead] + c;

#ifdef ALLOW_BUFFER_MAPPING
    if (audio_devs[dev]->dmap_in->mapping_flags & DMA_MAP_MAPPED) {
	printf("Sound: Can't read from mmapped device (2)\n");
	return -(EINVAL);
    } else
#endif
    if (p >= dmap->fragment_size) {	/* This buffer is completely empty */
	dmap->counts[dmap->qhead] = 0;
	if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs)
	    printf("\nSound: Audio queue1 corrupted for dev%d (%d/%d)\n",
		   dev, dmap->qlen, dmap->nbufs);
	dmap->qlen--;
	dmap->qhead = (dmap->qhead + 1) % dmap->nbufs;
    } else
	dmap->counts[dmap->qhead] = p;

    return 0;
}

static int
dma_subdivide(int dev, struct dma_buffparms * dmap, ioctl_arg arg, int fact)
{
    if (fact == 0) {
	fact = dmap->subdivision;
	if (fact == 0)
	    fact = 1;
	return *(int *) arg = fact;
    }
    if (dmap->subdivision != 0 || dmap->fragment_size)/* Loo late to change */
	return -(EINVAL);

    if (fact > MAX_REALTIME_FACTOR)
	return -(EINVAL);

    if (fact != 1 && fact != 2 && fact != 4 && fact != 8 && fact != 16)
	return -(EINVAL);

    dmap->subdivision = fact;
    return *(int *) arg = fact;
}

static int
dma_set_fragment(int dev, struct dma_buffparms * dmap, ioctl_arg arg, int fact)
{
    int             bytes, count;

    if (fact == 0)
	return -(EIO);

    if (dmap->subdivision != 0 || dmap->fragment_size)/* Loo late to change */
	return -(EINVAL);

    bytes = fact & 0xffff;
    count = (fact >> 16) & 0xffff;

    if (count == 0)
	count = MAX_SUB_BUFFERS;

#if amancio
    if (bytes < 4 || bytes > 17)	/* <16 || > 128k */
	return -(EINVAL);
#endif

    if (count < 2)
	return -(EINVAL);

#ifdef OS_DMA_MINBITS
    if (bytes < OS_DMA_MINBITS)
	bytes = OS_DMA_MINBITS;
#endif

    dmap->fragment_size = (1 << bytes);

    dmap->max_fragments = count;

    if (dmap->fragment_size > audio_devs[dev]->buffsize)
	dmap->fragment_size = audio_devs[dev]->buffsize;

    if (dmap->fragment_size == audio_devs[dev]->buffsize &&
	    audio_devs[dev]->flags & DMA_AUTOMODE)
	dmap->fragment_size /= 2;	/* Needs at least 2 buffers */

    dmap->subdivision = 1;	/* Disable SNDCTL_DSP_SUBDIVIDE */
    return *(int *) arg = bytes | (count << 16);
}

static int
get_buffer_pointer(int dev, int chan, struct dma_buffparms * dmap)
{
 int pos;
 u_long  flags;
 
  flags = splhigh();

  if (!(dmap->flags & DMA_ACTIVE))
    pos = 0;
  else
    {
      pos = isa_dmastatus(chan);
    }

  splx(flags);

  pos = dmap->bytes_in_use - pos ;
  if (audio_devs[dev]->flags & DMA_AUTOMODE)
    return  pos;
  else
    {
      pos = dmap->fragment_size - pos;
      if (pos < 0)
        return 0;
      return pos;
    }


}

int
DMAbuf_ioctl(int dev, u_int cmd, ioctl_arg arg, int local)
{
    struct dma_buffparms *dmap_out = audio_devs[dev]->dmap_out;
    struct dma_buffparms *dmap_in = audio_devs[dev]->dmap_in;

    switch (cmd) {


    case SNDCTL_DSP_RESET:
	dma_reset(dev);
	return 0;
	break;

    case SNDCTL_DSP_SYNC:
	dma_sync(dev);
	dma_reset(dev);
	return 0;
	break;

    case SNDCTL_DSP_GETBLKSIZE:
	if (!(dmap_out->flags & DMA_ALLOC_DONE))
	    reorganize_buffers(dev, dmap_out);

	return *(int *) arg = dmap_out->fragment_size;
	break;

    case SNDCTL_DSP_SETBLKSIZE:
	{
	    int             size = (*(int *) arg);

	    if (!(dmap_out->flags & DMA_ALLOC_DONE) && size) {
		if ((size >> 16) > 0 ) 
		    dmap_out->fragment_size = size >> 16;
		else {
		    dmap_out->fragment_size = size;
		}
		dmap_out->max_fragments = 8888;

		size &= 0xffff;

		if (audio_devs[dev]->flags & DMA_DUPLEX) {
		    dmap_in->fragment_size = size;
		    dmap_in->max_fragments = 8888;
		}
		return 0;

	    } else
		return -(EINVAL);	/* Too late to change */

	}
	break;

    case SNDCTL_DSP_SUBDIVIDE:
	{
	    int             fact = (*(int *) arg);
	    int             ret;

	    ret = dma_subdivide(dev, dmap_out, arg, fact);
	    if (ret < 0)
		return ret;

	    if (audio_devs[dev]->flags & DMA_DUPLEX)
		ret = dma_subdivide(dev, dmap_in, arg, fact);

	    return ret;
	}
	break;

    case SNDCTL_DSP_SETFRAGMENT:
	{
	    int             fact = (*(int *) arg);
	    int             ret;

	    ret = dma_set_fragment(dev, dmap_out, arg, fact);
	    if (ret < 0)
		return ret;

	    if (audio_devs[dev]->flags & DMA_DUPLEX)
		ret = dma_set_fragment(dev, dmap_in, arg, fact);

	    return ret;
	}
	break;

    case SNDCTL_DSP_GETISPACE:
    case SNDCTL_DSP_GETOSPACE:
	if (!local)
	    return -(EINVAL);
	else {
	    struct dma_buffparms *dmap = dmap_out;

	    audio_buf_info *info = (audio_buf_info *) arg;

	    if (cmd == SNDCTL_DSP_GETISPACE && audio_devs[dev]->flags & DMA_DUPLEX)
		dmap = dmap_in;

#ifdef ALLOW_BUFFER_MAPPING
	    if (dmap->mapping_flags & DMA_MAP_MAPPED)
		return -(EINVAL);
#endif

	    if (!(dmap->flags & DMA_ALLOC_DONE))
		reorganize_buffers(dev, dmap);

	    info->fragstotal = dmap->nbufs;

	    if (cmd == SNDCTL_DSP_GETISPACE)
		info->fragments = dmap->qlen;
	    else {
		if (!space_in_queue(dev))
		    info->fragments = 0;
		else {
		    info->fragments = dmap->nbufs - dmap->qlen;
		    if (audio_devs[dev]->local_qlen) {
			int             tmp = audio_devs[dev]->local_qlen(dev);

			if (tmp & info->fragments)
			    tmp--;	/* This buffer has been counted twice */
			info->fragments -= tmp;
		    }
		}
	    }

	    if (info->fragments < 0)
		info->fragments = 0;
	    else if (info->fragments > dmap->nbufs)
		info->fragments = dmap->nbufs;

	    info->fragsize = dmap->fragment_size;
	    info->bytes = info->fragments * dmap->fragment_size;

	    if (cmd == SNDCTL_DSP_GETISPACE && dmap->qlen)
		info->bytes -= dmap->counts[dmap->qhead];
	}
	return 0;

    case SNDCTL_DSP_SETTRIGGER:
	{
	    u_long   flags;

	    int   bits = (*(int *) arg) & audio_devs[dev]->open_mode;
	    int   changed;

	    if (audio_devs[dev]->trigger == NULL)
		return -(EINVAL);

	    if (!(audio_devs[dev]->flags & DMA_DUPLEX))
		if ((bits & PCM_ENABLE_INPUT) && (bits & PCM_ENABLE_OUTPUT)) {
		    printf("Sound: Device doesn't have full duplex capability\n");
		    return -(EINVAL);
		}
	    flags = splhigh();
	    changed = audio_devs[dev]->enable_bits ^ bits;

	    if ((changed & bits) & PCM_ENABLE_INPUT && audio_devs[dev]->go) {
		if (!(dmap_in->flags & DMA_ALLOC_DONE))
		    reorganize_buffers(dev, dmap_in);
		    activate_recording(dev, dmap_in);
	    }
#ifdef ALLOW_BUFFER_MAPPING
	    if ((changed & bits) & PCM_ENABLE_OUTPUT &&
		    dmap_out->mapping_flags & DMA_MAP_MAPPED &&
		    audio_devs[dev]->go) {
		if (!(dmap_out->flags & DMA_ALLOC_DONE))
		    reorganize_buffers(dev, dmap_out);

		 audio_devs[dev]->prepare_for_output (dev,
			     dmap_out->fragment_size, dmap_out->nbufs);

		dmap_out->counts[dmap_out->qhead] = dmap_out->fragment_size;
		DMAbuf_start_output(dev, 0, dmap_out->fragment_size);
                dmap_out->dma_mode = DMODE_OUTPUT; 
	    }
#endif

	    audio_devs[dev]->enable_bits = bits;
	    if (changed && audio_devs[dev]->trigger)
		audio_devs[dev]->trigger(dev, bits * audio_devs[dev]->go);
	    splx(flags);
	}
    case SNDCTL_DSP_GETTRIGGER:
	return *(int *) arg = audio_devs[dev]->enable_bits;
	break;

    case SNDCTL_DSP_SETSYNCRO:

	if (!audio_devs[dev]->trigger)
	    return -(EINVAL);

	audio_devs[dev]->trigger(dev, 0);
	audio_devs[dev]->go = 0;
	return 0;
	break;

    case SNDCTL_DSP_GETIPTR:
	{
	    count_info      info;
	    u_long   flags;

	    flags = splhigh();
	    info.bytes = audio_devs[dev]->dmap_in->byte_counter;
	    info.ptr = get_buffer_pointer(dev, audio_devs[dev]->dmachan2, audio_devs[dev]->dmap_in);
	    info.blocks = audio_devs[dev]->dmap_in->qlen;
	    info.bytes += info.ptr;

	    bcopy((char *) &info, &(((char *) arg)[0]), sizeof(info));

#ifdef ALLOW_BUFFER_MAPPING
	    if (audio_devs[dev]->dmap_in->mapping_flags & DMA_MAP_MAPPED)
		audio_devs[dev]->dmap_in->qlen = 0;	/* Ack interrupts */
#endif
	    splx(flags);
	    return 0;
	}
	break;

    case SNDCTL_DSP_GETOPTR:
	{
	    count_info      info;
	    u_long   flags;

	    flags = splhigh();
	    info.bytes = audio_devs[dev]->dmap_out->byte_counter;
	    info.ptr = get_buffer_pointer(dev, audio_devs[dev]->dmachan1, audio_devs[dev]->dmap_out);
	    info.blocks = audio_devs[dev]->dmap_out->qlen;
	    info.bytes += info.ptr;
	    bcopy((char *) &info, &(((char *) arg)[0]), sizeof(info));

#ifdef ALLOW_BUFFER_MAPPING
	    if (audio_devs[dev]->dmap_out->mapping_flags & DMA_MAP_MAPPED)
		audio_devs[dev]->dmap_out->qlen = 0;	/* Ack interrupts */
#endif
	    splx(flags);
	    return 0;
	}
	break;

    default:
	return audio_devs[dev]->ioctl(dev, cmd, arg, local);
    }
}

/*
 * DMAbuf_start_devices() is called by the /dev/music driver to start one or
 * more audio devices at desired moment.
 */

void
DMAbuf_start_devices(u_int devmask)
{
    int             dev;

    for (dev = 0; dev < num_audiodevs; dev++)
	if (devmask & (1 << dev))
	    if (audio_devs[dev]->open_mode != 0)
		if (!audio_devs[dev]->go) {
		    /* OK to start the device */
		    audio_devs[dev]->go = 1;

		    if (audio_devs[dev]->trigger)
			audio_devs[dev]->trigger(dev,
			    audio_devs[dev]->enable_bits * audio_devs[dev]->go);
		}
}

static int
space_in_queue(int dev)
{
    int             len, max, tmp;
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_out;
    if (dmap->qlen >= dmap->nbufs)	/* No space at all */
	return 0;

    /*
     * Verify that there are no more pending buffers than the limit
     * defined by the process.
     */

    max = dmap->max_fragments;
    len = dmap->qlen;

    if (audio_devs[dev]->local_qlen) {
	tmp = audio_devs[dev]->local_qlen(dev);
	if (tmp & len)
	    tmp--;	/* This buffer has been counted twice */
	len += tmp;
    }

    if (len >= max)
	return 0;
    return 1;
}

int
DMAbuf_getwrbuffer(int dev, char **buf, int *size, int dontblock)
{
    u_long   flags;
    int             abort, err = EIO;
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_out;

#ifdef ALLOW_BUFFER_MAPPING
    if (audio_devs[dev]->dmap_out->mapping_flags & DMA_MAP_MAPPED) {
	printf("Sound: Can't write to mmapped device (3)\n");
	return -(EINVAL);
    }
#endif

    if (dmap->dma_mode == DMODE_INPUT) {	/* Direction change */
	dma_reset(dev);
	dmap->dma_mode = DMODE_NONE;
    } else if (dmap->flags & DMA_RESTART) {	/* Restart buffering */
	dma_sync(dev);
	dma_reset_output(dev);
    }
    dmap->flags &= ~DMA_RESTART;

    if (!(dmap->flags & DMA_ALLOC_DONE))
	reorganize_buffers(dev, dmap);

    if (!dmap->dma_mode) {
	int             err;

	dmap->dma_mode = DMODE_OUTPUT;
	if ((err = audio_devs[dev]->prepare_for_output(dev,
			 dmap->fragment_size, dmap->nbufs)) < 0)
	    return err;
    }
    flags = splhigh();

    abort = 0;
    while (!space_in_queue(dev) && !abort) {
	int             timeout;

	if (dontblock) {
	    splx(flags);
	    return -(EAGAIN);
	}
	if (!(audio_devs[dev]->enable_bits & PCM_ENABLE_OUTPUT) &&
		    audio_devs[dev]->go) {
	    splx(flags);
	    return -(EAGAIN);
	}
	/*
	 * Wait for free space
	 */
	if (!audio_devs[dev]->go)
	    timeout = 0;
	else
	    timeout = 2 * hz;

	{
	    int  chn;

	    out_sleep_flag[dev].mode = WK_SLEEP;
	    out_sleeper[dev] = &chn;
	    DO_SLEEP2(chn, out_sleep_flag[dev], timeout);

	if ((out_sleep_flag[dev].mode & WK_TIMEOUT)) {
	    printf("Sound: DMA (output) timed out - IRQ/DRQ config error?\n");
	    err = EIO;
	    abort = 1;
	    out_sleep_flag[dev].aborting = 1;
	    audio_devs[dev]->reset(dev);
	} else if ((out_sleep_flag[dev].aborting) || 
		    CURSIG(curproc)) {
	    err = EINTR;
	    abort = 1;
	}
	}
    }
    splx(flags);

    if (!space_in_queue(dev)) {
	return -(err);	/* Caught a signal ? */
    }
    *buf = dmap->raw_buf + dmap->qtail * dmap->fragment_size;
    *size = dmap->fragment_size;
    dmap->counts[dmap->qtail] = 0;
    return dmap->qtail;
}

int
DMAbuf_start_output(int dev, int buff_no, int l)
{
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_out;

    /*
     * Bypass buffering if using mmaped access
     */

#ifdef ALLOW_BUFFER_MAPPING
    if (audio_devs[dev]->dmap_out->mapping_flags & DMA_MAP_MAPPED) {
	l = dmap->fragment_size;
	dmap->counts[dmap->qtail] = l;
	dmap->flags &= ~DMA_RESTART;
	dmap->qtail = (dmap->qtail + 1) % dmap->nbufs;
    } else
#else
    if (dmap != NULL)
#endif
    {

	if (buff_no != dmap->qtail)
	    printf("Sound warning: DMA buffers out of sync %d != %d\n", buff_no, dmap->qtail);

	dmap->qlen++;
	if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs)
	    printf("\nSound: Audio queue2 corrupted for dev%d (%d/%d)\n",
		       dev, dmap->qlen, dmap->nbufs);

	dmap->counts[dmap->qtail] = l;

	if ((l != dmap->fragment_size) &&
		    ((audio_devs[dev]->flags & DMA_AUTOMODE) &&
		     audio_devs[dev]->flags & NEEDS_RESTART))
	    dmap->flags |= DMA_RESTART;
	else
	    dmap->flags &= ~DMA_RESTART;

	dmap->qtail = (dmap->qtail + 1) % dmap->nbufs;
    }
    if (!(dmap->flags & DMA_ACTIVE)) {
	dmap->flags |= DMA_ACTIVE;
	audio_devs[dev]->output_block(dev, dmap->raw_buf_phys +
		  dmap->qhead * dmap->fragment_size,
		  dmap->counts[dmap->qhead], 0,
		 !(audio_devs[dev]->flags & DMA_AUTOMODE) ||
				  !(dmap->flags & DMA_STARTED));
	dmap->flags |= DMA_STARTED;
	if (audio_devs[dev]->trigger)
	    audio_devs[dev]->trigger(dev,
			audio_devs[dev]->enable_bits * audio_devs[dev]->go);
    }
    return 0;
}

int
DMAbuf_start_dma(int dev, u_long physaddr, int count, int dma_mode)
{
    int             chan;
    struct dma_buffparms *dmap;

    if (dma_mode == 1) {
	chan = audio_devs[dev]->dmachan1;
	dmap = audio_devs[dev]->dmap_out;

    } else {
	chan = audio_devs[dev]->dmachan2;
	dmap = audio_devs[dev]->dmap_in;
    }

    /*
     * The count must be one less than the actual size. This is handled
     * by set_dma_addr()
     */

#ifndef PSEUDO_DMA_AUTOINIT
    if (audio_devs[dev]->flags & DMA_AUTOMODE) {
	/* Auto restart mode. Transfer the whole buffer */
	isa_dmastart(B_RAW | ((dma_mode == 0) ? B_READ : B_WRITE),
	     (caddr_t) dmap->raw_buf_phys, dmap->bytes_in_use, chan);

    } else
#endif
    {
	isa_dmastart((dma_mode == 0) ? B_READ : B_WRITE,
		 (caddr_t) physaddr, count, chan);
    }
    return count;
}

void
DMAbuf_init()
{
    int             dev;

    /*
     * NOTE! This routine could be called several times.
     * XXX is it ok to make it run only the first time ? -- lr970710
     */

    for (dev = 0; dev < num_audiodevs; dev++)
	if (audio_devs[dev]->dmap_out == NULL) {
	    audio_devs[dev]->dmap_out =
	    audio_devs[dev]->dmap_in = &dmaps[ndmaps++];

	    if (audio_devs[dev]->flags & DMA_DUPLEX)
		audio_devs[dev]->dmap_in = &dmaps[ndmaps++];
	}
}

void
DMAbuf_outputintr(int dev, int event_type)
{
    /*
     * Event types: 0 = DMA transfer done. Device still has more data in
     * the local buffer. 1 = DMA transfer done. Device doesn't have local
     * buffer or it's empty now. 2 = No DMA transfer but the device has
     * now more space in it's local buffer.
     */

    u_long   flags;
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_out;
    dmap->byte_counter += dmap->counts[dmap->qhead];
#ifdef OS_DMA_INTR
    sound_dma_intr(dev, audio_devs[dev]->dmap_out, audio_devs[dev]->dmachan1);
#endif
#ifdef ALLOW_BUFFER_MAPPING
    if (dmap->mapping_flags & DMA_MAP_MAPPED) {
	/* mmapped access */

	int             p = dmap->fragment_size * dmap->qhead;

	dmap->qhead = (dmap->qhead + 1) % dmap->nbufs;
	dmap->qlen++;	/* Yes increment it (don't decrement) */
	dmap->flags &= ~DMA_ACTIVE;
	dmap->counts[dmap->qhead] = dmap->fragment_size;

	if (!(audio_devs[dev]->flags & DMA_AUTOMODE)) {
	    audio_devs[dev]->output_block(dev, dmap->raw_buf_phys +
		      dmap->qhead * dmap->fragment_size,
		       dmap->counts[dmap->qhead], 1,
		      !(audio_devs[dev]->flags & DMA_AUTOMODE));
	    if (audio_devs[dev]->trigger)
		audio_devs[dev]->trigger(dev,
			 audio_devs[dev]->enable_bits * audio_devs[dev]->go);
	}
#ifdef PSEUDO_DMA_AUTOINIT
	else {
	    DMAbuf_start_dma(dev, dmap->raw_buf_phys +
		    dmap->qhead * dmap->fragment_size,
		     dmap->counts[dmap->qhead], 1);
	}
#endif
	dmap->flags |= DMA_ACTIVE;

    } else
#endif
    if (event_type != 2) {
	if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs) {
	    printf("\nSound: Audio queue3 corrupted for dev%d (%d/%d)\n",
		   dev, dmap->qlen, dmap->nbufs);
	    return;
	}
	isa_dmadone(0, 0, 0, audio_devs[dev]->dmachan1);

	dmap->qlen--;
	dmap->qhead = (dmap->qhead + 1) % dmap->nbufs;
	dmap->flags &= ~DMA_ACTIVE;
	if (dmap->qlen) {
          /* if (!(audio_devs[dev]->flags & NEEDS_RESTART)) */
	    {
		audio_devs[dev]->output_block(dev, dmap->raw_buf_phys +
			  dmap->qhead * dmap->fragment_size,
			   dmap->counts[dmap->qhead], 1,
			  !(audio_devs[dev]->flags & DMA_AUTOMODE));
		if (audio_devs[dev]->trigger)
		    audio_devs[dev]->trigger(dev,
			 audio_devs[dev]->enable_bits * audio_devs[dev]->go);
	    }

#ifdef PSEUDO_DMA_AUTOINIT
	    /* else */
	    {
		DMAbuf_start_dma(dev, dmap->raw_buf_phys +
			   dmap->qhead * dmap->fragment_size,
			   dmap->counts[dmap->qhead], 1);
	    }
#endif
	    dmap->flags |= DMA_ACTIVE;
	} else if (event_type == 1) {
	    dmap->underrun_count++;
	    if ((audio_devs[dev]->flags & DMA_DUPLEX) &&
		    audio_devs[dev]->halt_output)
		audio_devs[dev]->halt_output(dev);
	    else
		audio_devs[dev]->halt_xfer(dev);

	    if ((audio_devs[dev]->flags & DMA_AUTOMODE) &&
		    audio_devs[dev]->flags & NEEDS_RESTART)
		dmap->flags |= DMA_RESTART;
	    else
		dmap->flags &= ~DMA_RESTART;
	}
    }			/* event_type != 2 */
    flags = splhigh();

    if ((out_sleep_flag[dev].mode & WK_SLEEP)) {
	out_sleep_flag[dev].mode = WK_WAKEUP;
	wakeup(out_sleeper[dev]);
    }

    if(selinfo[dev].si_pid) {
	selwakeup(&selinfo[dev]);
    }

    splx(flags);
}

void
DMAbuf_inputintr(int dev)
{
    u_long   flags;
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_in;

    dmap->byte_counter += dmap->fragment_size;

#ifdef OS_DMA_INTR
    sound_dma_intr(dev, audio_devs[dev]->dmap_in, audio_devs[dev]->dmachan2);
#endif
      isa_dmadone(0, 0, 0, audio_devs[dev]->dmachan2);

#ifdef ALLOW_BUFFER_MAPPING
    if (dmap->mapping_flags & DMA_MAP_MAPPED) {
	dmap->qtail = (dmap->qtail + 1) % dmap->nbufs;
	dmap->qlen++;

	if (!(audio_devs[dev]->flags & NEEDS_RESTART)) {
	    audio_devs[dev]->start_input(dev, dmap->raw_buf_phys +
		     dmap->qtail * dmap->fragment_size,
		     dmap->fragment_size, 1,
		     !(audio_devs[dev]->flags & DMA_AUTOMODE));
	    if (audio_devs[dev]->trigger)
		audio_devs[dev]->trigger(dev,
		    audio_devs[dev]->enable_bits * audio_devs[dev]->go);
	}
#ifdef PSEUDO_DMA_AUTOINIT
	else {
	    DMAbuf_start_dma(dev, dmap->raw_buf_phys +
		    dmap->qtail * dmap->fragment_size,
		    dmap->counts[dmap->qtail], 0);
	}
#endif

	dmap->flags |= DMA_ACTIVE;
    } else
#endif
    if (dmap->qlen == (dmap->nbufs - 1)) {
	/* printf ("Sound: Recording overrun\n"); */
	dmap->underrun_count++;
	if ((audio_devs[dev]->flags & DMA_DUPLEX) &&
		audio_devs[dev]->halt_input)
	    audio_devs[dev]->halt_input(dev);
	else
	    audio_devs[dev]->halt_xfer(dev);

	dmap->flags &= ~DMA_ACTIVE;
	if (audio_devs[dev]->flags & DMA_AUTOMODE)
	    dmap->flags |= DMA_RESTART;
	else
	    dmap->flags &= ~DMA_RESTART;
    } else {
	dmap->qlen++;
	if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs)
	    printf("\nSound: Audio queue4 corrupted for dev%d (%d/%d)\n",
		       dev, dmap->qlen, dmap->nbufs);
	dmap->qtail = (dmap->qtail + 1) % dmap->nbufs;

	/* if (!(audio_devs[dev]->flags & DMA_AUTOMODE)) */
	{
	    audio_devs[dev]->start_input(dev, dmap->raw_buf_phys +
			  dmap->qtail * dmap->fragment_size,
			 dmap->fragment_size, 1,
			  !(audio_devs[dev]->flags & DMA_AUTOMODE));
	    if (audio_devs[dev]->trigger)
		audio_devs[dev]->trigger(dev,
			 audio_devs[dev]->enable_bits * audio_devs[dev]->go);
	}
#ifdef PSEUDO_DMA_AUTOINIT
	/* else */
	{
	    DMAbuf_start_dma(dev, dmap->raw_buf_phys +
		    dmap->qtail * dmap->fragment_size,
		     dmap->counts[dmap->qtail], 0);
	}
#endif

	dmap->flags |= DMA_ACTIVE;
    }

    flags = splhigh();
    if ((in_sleep_flag[dev].mode & WK_SLEEP)) {
	in_sleep_flag[dev].mode = WK_WAKEUP;
	wakeup(in_sleeper[dev]);
    }
    if (selinfo[dev].si_pid)
	selwakeup(&selinfo[dev]);
    splx(flags);
}

int
DMAbuf_open_dma(int dev)
{
    int             err;
    u_long   flags;
    flags = splhigh();

    if ((err = open_dmap(dev, OPEN_READWRITE, audio_devs[dev]->dmap_out,
		audio_devs[dev]->dmachan1)) < 0) {
	splx(flags);
	return -(EBUSY);
    }
    dma_init_buffers(dev, audio_devs[dev]->dmap_out);
    /* audio_devs[dev]->dmap_out->flags |= DMA_ALLOC_DONE; */
    audio_devs[dev]->dmap_out->fragment_size = audio_devs[dev]->buffsize;
    /* reorganize_buffers (dev, audio_devs[dev]->dmap_out); */

    if (audio_devs[dev]->flags & DMA_DUPLEX) {
	if ((err = open_dmap(dev, OPEN_READWRITE,
		audio_devs[dev]->dmap_in, audio_devs[dev]->dmachan2)) < 0) {
	    printf("Unable to grab DMA%d for the audio driver\n",
		   audio_devs[dev]->dmachan2);
	    close_dmap(dev, audio_devs[dev]->dmap_out,
		    audio_devs[dev]->dmachan1);
	    splx(flags);
	    return -(EBUSY);
	}
	dma_init_buffers(dev, audio_devs[dev]->dmap_in);
	/* audio_devs[dev]->dmap_in->flags |= DMA_ALLOC_DONE; */
	audio_devs[dev]->dmap_in->fragment_size = audio_devs[dev]->buffsize;
	/* reorganize_buffers (dev, audio_devs[dev]->dmap_in); */
    } else {
	audio_devs[dev]->dmap_in = audio_devs[dev]->dmap_out;
	audio_devs[dev]->dmachan2 = audio_devs[dev]->dmachan1;
    }

    splx(flags);
    return 0;
}

void
DMAbuf_close_dma(int dev)
{
    DMAbuf_reset_dma(dev);
    close_dmap(dev, audio_devs[dev]->dmap_out, audio_devs[dev]->dmachan1);

    if (audio_devs[dev]->flags & DMA_DUPLEX)
	close_dmap(dev, audio_devs[dev]->dmap_in, audio_devs[dev]->dmachan2);

}

void
DMAbuf_reset_dma(int dev)
{
}

#ifdef ALLOW_SELECT

int
DMAbuf_poll(int dev, struct fileinfo * file, int events, select_table * wait)
{
    struct dma_buffparms *dmap;
    u_long   flags;
    int revents = 0;

    dmap = audio_devs[dev]->dmap_in;
    
    if (events & (POLLIN | POLLRDNORM)) {
	if (dmap->dma_mode != DMODE_INPUT) {
	    if ((audio_devs[dev]->flags & DMA_DUPLEX) && !dmap->qlen &&
		audio_devs[dev]->enable_bits & PCM_ENABLE_INPUT &&
		audio_devs[dev]->go) {
		u_long   flags;
		
		flags = splhigh();
		
		activate_recording(dev, dmap);
		splx(flags);
		
	    }
	    return 0;
	}
	if (!dmap->qlen) {
	    flags = splhigh();

	    selrecord(wait, &selinfo[dev]);

	    splx(flags);

	    return 0;
	} else 
	  revents |= events & (POLLIN | POLLRDNORM);

    }

    if (events & (POLLOUT | POLLWRNORM)) {
	
	dmap = audio_devs[dev]->dmap_out;
	if (dmap->dma_mode == DMODE_INPUT)
	    return 0;
	
	if (dmap->dma_mode == DMODE_NONE)
	    return ( events & (POLLOUT | POLLWRNORM));
	
	if (dmap->mapping_flags & DMA_MAP_MAPPED) {
	    
	    if(dmap->qlen)
		return 1;
	    flags = splhigh();
	    selrecord(wait, &selinfo[dev]);
	    
	    splx(flags);
	    
	    return 0;
	    
	}
	if (!space_in_queue(dev)) {
	    flags = splhigh();
	    selrecord(wait, &selinfo[dev]);
	    splx(flags);

	} else 
	  revents |= events & (POLLOUT | POLLWRNORM);


    }

    return (revents);
}


#ifdef amancio
int
DMAbuf_select(int dev, struct fileinfo * file, int sel_type, select_table * wait)
{
    struct dma_buffparms *dmap = audio_devs[dev]->dmap_out;
    struct dma_buffparms *dmapin = audio_devs[dev]->dmap_in;
    u_long   flags;

    switch (sel_type) {
    case FREAD:
	if (dmapin->dma_mode != DMODE_INPUT)
	    return 0;

	if (!dmap->qlen) {
	    flags = splhigh();
	    selrecord(wait, &selinfo[dev]);
	    splx(flags);

	    return 0;
	}
	return 1;
	break;

    case FWRITE:
	if (dmap->dma_mode == DMODE_INPUT)
	    return 0;

	if (dmap->dma_mode == DMODE_NONE)
	    return 1;

	if (!space_in_queue(dev)) {
	    flags = splhigh();

	    selrecord(wait, &selinfo[dev]);
	    splx(flags);

	    return 0;
	}
	return 1;
	break;

    }

    return 0;
}

#endif				/* ALLOW_SELECT */
#endif

#else				/* CONFIG_AUDIO */
/*
 * Stub versions if audio services not included
 */

int
DMAbuf_open(int dev, int mode)
{
    return -(ENXIO);
}

int
DMAbuf_release(int dev, int mode)
{
    return 0;
}

int
DMAbuf_getwrbuffer(int dev, char **buf, int *size, int dontblock)
{
    return -(EIO);
}

int
DMAbuf_getrdbuffer(int dev, char **buf, int *len, int dontblock)
{
    return -(EIO);
}

int
DMAbuf_rmchars(int dev, int buff_no, int c)
{
    return -(EIO);
}

int
DMAbuf_start_output(int dev, int buff_no, int l)
{
    return -(EIO);
}

int
DMAbuf_ioctl(int dev, u_int cmd, ioctl_arg arg, int local)
{
    return -(EIO);
}

void
DMAbuf_init()
{
}

int
DMAbuf_start_dma(int dev, u_long physaddr, int count, int dma_mode)
{
    return -(EIO);
}

int
DMAbuf_open_dma(int dev)
{
    return -(ENXIO);
}

void
DMAbuf_close_dma(int dev)
{
    return;
}

void
DMAbuf_reset_dma(int dev)
{
    return;
}

void
DMAbuf_inputintr(int dev)
{
    return;
}

void
DMAbuf_outputintr(int dev, int underrun_flag)
{
    return;
}
#endif	/* CONFIG_AUDIO */
