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

#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#if !defined(EXCLUDE_AUDIO) || !defined(EXCLUDE_GUS)

DEFINE_WAIT_QUEUES (dev_sleeper[MAX_AUDIO_DEV], dev_sleep_flag[MAX_AUDIO_DEV]);

static struct dma_buffparms dmaps[MAX_AUDIO_DEV] =
{
  {0}};				/*

				   * Primitive way to allocate
				   * such a large array.
				   * Needs dynamic run-time alloction.
				 */

static void
reorganize_buffers (int dev)
{
  /*
   * This routine breaks the physical device buffers to logical ones.
   */

  struct dma_buffparms *dmap = audio_devs[dev]->dmap;
  struct audio_operations *dsp_dev = audio_devs[dev];

  unsigned        i, p, n;
  unsigned        sr, nc, sz, bsz;

  if (dmap->fragment_size == 0)
    {				/* Compute the fragment size using the default algorithm */

      sr = dsp_dev->ioctl (dev, SOUND_PCM_READ_RATE, 0, 1);
      nc = dsp_dev->ioctl (dev, SOUND_PCM_READ_CHANNELS, 0, 1);
      sz = dsp_dev->ioctl (dev, SOUND_PCM_READ_BITS, 0, 1);

      if (sr < 1 || nc < 1 || sz < 1)
	{
	  printk ("Warning: Invalid PCM parameters[%d] sr=%d, nc=%d, sz=%d\n",
		  dev, sr, nc, sz);
	  sr = DSP_DEFAULT_SPEED;
	  nc = 1;
	  sz = 8;
	}

      sz = sr * nc * sz;

      sz /= 8;			/* #bits -> #bytes */

      /*
         * Compute a buffer size for time not exeeding 1 second.
         * Usually this algorithm gives a buffer size for 0.5 to 1.0 seconds
         * of sound (using the current speed, sample size and #channels).
       */

      bsz = dsp_dev->buffsize;
      while (bsz > sz)
	bsz /= 2;

      if (dsp_dev->buffcount == 1 && bsz == dsp_dev->buffsize)
	bsz /= 2;		/* Needs at least 2 buffers */

      if (dmap->subdivision == 0)	/* Not already set */
	dmap->subdivision = 1;	/* Init to default value */
      else
	bsz /= dmap->subdivision;

      if (bsz < 16)
	bsz = 16;		/* Just a sanity check */

      while ((dsp_dev->buffsize * dsp_dev->buffcount) / bsz > MAX_SUB_BUFFERS)
	bsz *= 2;

      dmap->fragment_size = bsz;
    }
  else
    {
      /*
         * The process has specified the buffer sice with SNDCTL_DSP_SETFRAGMENT or
         * the buffer sice computation has already been done.
       */
      if (dmap->fragment_size > (audio_devs[dev]->buffsize / 2))
	dmap->fragment_size = (audio_devs[dev]->buffsize / 2);
      bsz = dmap->fragment_size;
    }

  bsz &= ~0x03;			/* Force size which is multiple of 4 bytes */

  /*
   * Now computing addresses for the logical buffers
   */

  n = 0;
  for (i = 0; i < dmap->raw_count &&
       n < dmap->max_fragments &&
       n < MAX_SUB_BUFFERS; i++)
    {
      p = 0;

      while ((p + bsz) <= dsp_dev->buffsize &&
	     n < dmap->max_fragments &&
	     n < MAX_SUB_BUFFERS)
	{
	  dmap->buf[n] = dmap->raw_buf[i] + p;
	  dmap->buf_phys[n] = dmap->raw_buf_phys[i] + p;
	  p += bsz;
	  n++;
	}
    }

  dmap->nbufs = n;
  dmap->bytes_in_use = n * bsz;

  for (i = 0; i < dmap->nbufs; i++)
    {
      dmap->counts[i] = 0;
    }

  dmap->flags |= DMA_ALLOC_DONE;
}

static void
dma_init_buffers (int dev)
{
  struct dma_buffparms *dmap = audio_devs[dev]->dmap = &dmaps[dev];

  RESET_WAIT_QUEUE (dev_sleeper[dev], dev_sleep_flag[dev]);

  dmap->flags = DMA_BUSY;	/* Other flags off */
  dmap->qlen = dmap->qhead = dmap->qtail = 0;
  dmap->nbufs = 1;
  dmap->bytes_in_use = audio_devs[dev]->buffsize;

  dmap->dma_mode = DMODE_NONE;
}

int
DMAbuf_open (int dev, int mode)
{
  int             retval;
  struct dma_buffparms *dmap = NULL;

  if (dev >= num_audiodevs)
    {
      printk ("PCM device %d not installed.\n", dev);
      return RET_ERROR (ENXIO);
    }

  if (!audio_devs[dev])
    {
      printk ("PCM device %d not initialized\n", dev);
      return RET_ERROR (ENXIO);
    }

  dmap = audio_devs[dev]->dmap = &dmaps[dev];

  if (dmap->flags & DMA_BUSY)
    return RET_ERROR (EBUSY);

#ifdef USE_RUNTIME_DMAMEM
  dmap->raw_buf[0] = NULL;
  sound_dma_malloc (dev);
#endif

  if (dmap->raw_buf[0] == NULL)
    return RET_ERROR (ENOSPC);	/* Memory allocation failed during boot */

  if ((retval = audio_devs[dev]->open (dev, mode)) < 0)
    return retval;

  dmap->open_mode = mode;
  dmap->subdivision = dmap->underrun_count = 0;
  dmap->fragment_size = 0;
  dmap->max_fragments = 65536;	/* Just a large value */

  dma_init_buffers (dev);
  audio_devs[dev]->ioctl (dev, SOUND_PCM_WRITE_BITS, 8, 1);
  audio_devs[dev]->ioctl (dev, SOUND_PCM_WRITE_CHANNELS, 1, 1);
  audio_devs[dev]->ioctl (dev, SOUND_PCM_WRITE_RATE, DSP_DEFAULT_SPEED, 1);

  return 0;
}

static void
dma_reset (int dev)
{
  int             retval;
  unsigned long   flags;

  DISABLE_INTR (flags);

  audio_devs[dev]->reset (dev);
  audio_devs[dev]->close (dev);

  if ((retval = audio_devs[dev]->open (dev, audio_devs[dev]->dmap->open_mode)) < 0)
    printk ("Sound: Reset failed - Can't reopen device\n");
  RESTORE_INTR (flags);

  dma_init_buffers (dev);
  reorganize_buffers (dev);
}

static int
dma_sync (int dev)
{
  unsigned long   flags;

  if (audio_devs[dev]->dmap->dma_mode == DMODE_OUTPUT)
    {
      DISABLE_INTR (flags);

      while (!PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev])
	     && audio_devs[dev]->dmap->qlen)
	{
	  DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], 10 * HZ);
	  if (TIMED_OUT (dev_sleeper[dev], dev_sleep_flag[dev]))
	    {
	      RESTORE_INTR (flags);
	      return audio_devs[dev]->dmap->qlen;
	    }
	}
      RESTORE_INTR (flags);

      /*
       * Some devices such as GUS have huge amount of on board RAM for the
       * audio data. We have to wait until the device has finished playing.
       */

      DISABLE_INTR (flags);
      if (audio_devs[dev]->local_qlen)	/* Device has hidden buffers */
	{
	  while (!(PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev]))
		 && audio_devs[dev]->local_qlen (dev))
	    {
	      DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], HZ);
	    }
	}
      RESTORE_INTR (flags);
    }
  return audio_devs[dev]->dmap->qlen;
}

int
DMAbuf_release (int dev, int mode)
{
  unsigned long   flags;

  if (!(PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev]))
      && (audio_devs[dev]->dmap->dma_mode == DMODE_OUTPUT))
    {
      dma_sync (dev);
    }

#ifdef USE_RUNTIME_DMAMEM
  sound_dma_free (dev);
#endif

  DISABLE_INTR (flags);
  audio_devs[dev]->reset (dev);

  audio_devs[dev]->close (dev);

  audio_devs[dev]->dmap->dma_mode = DMODE_NONE;
  audio_devs[dev]->dmap->flags &= ~DMA_BUSY;
  RESTORE_INTR (flags);

  return 0;
}

int
DMAbuf_getrdbuffer (int dev, char **buf, int *len, int dontblock)
{
  unsigned long   flags;
  int             err = EIO;
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

  DISABLE_INTR (flags);
  if (!dmap->qlen)
    {
      if (dmap->flags & DMA_RESTART)
	{
	  dma_reset (dev);
	  dmap->flags &= ~DMA_RESTART;
	}

      if (dmap->dma_mode == DMODE_OUTPUT)	/* Direction change */
	{
	  dma_sync (dev);
	  dma_reset (dev);
	  dmap->dma_mode = DMODE_NONE;
	}

      if (!(dmap->flags & DMA_ALLOC_DONE))
	reorganize_buffers (dev);

      if (!dmap->dma_mode)
	{
	  int             err;

	  if ((err = audio_devs[dev]->prepare_for_input (dev,
				     dmap->fragment_size, dmap->nbufs)) < 0)
	    {
	      RESTORE_INTR (flags);
	      return err;
	    }
	  dmap->dma_mode = DMODE_INPUT;
	}

      if (!(dmap->flags & DMA_ACTIVE))
	{
	  audio_devs[dev]->start_input (dev, dmap->buf_phys[dmap->qtail],
					dmap->fragment_size, 0,
				 !(audio_devs[dev]->flags & DMA_AUTOMODE) ||
					!(dmap->flags & DMA_STARTED));
	  dmap->flags |= DMA_ACTIVE | DMA_STARTED;
	}

      if (dontblock)
	{
	  RESTORE_INTR (flags);
#if defined(__FreeBSD__)
	  return RET_ERROR (EWOULDBLOCK);
#else
	  return RET_ERROR (EAGAIN);
#endif
	}

      /* Wait for the next block */

      DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], 2 * HZ);
      if (TIMED_OUT (dev_sleeper[dev], dev_sleep_flag[dev]))
	{
	  printk ("Sound: DMA timed out - IRQ/DRQ config error?\n");
	  dma_reset (dev);
	  err = EIO;
	  SET_ABORT_FLAG (dev_sleeper[dev], dev_sleep_flag[dev]);
	}
      else
	err = EINTR;
    }
  RESTORE_INTR (flags);

  if (!dmap->qlen)
    return RET_ERROR (err);

  *buf = &dmap->buf[dmap->qhead][dmap->counts[dmap->qhead]];
  *len = dmap->fragment_size - dmap->counts[dmap->qhead];

  return dmap->qhead;
}

int
DMAbuf_rmchars (int dev, int buff_no, int c)
{
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

  int             p = dmap->counts[dmap->qhead] + c;

  if (p >= dmap->fragment_size)
    {				/* This buffer is completely empty */
      dmap->counts[dmap->qhead] = 0;
      if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs)
	printk ("\nSound: Audio queue1 corrupted for dev%d (%d/%d)\n",
		dev, dmap->qlen, dmap->nbufs);
      dmap->qlen--;
      dmap->qhead = (dmap->qhead + 1) % dmap->nbufs;
    }
  else
    dmap->counts[dmap->qhead] = p;

  return 0;
}

int
DMAbuf_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

  switch (cmd)
    {
    case SNDCTL_DSP_RESET:
      dma_reset (dev);
      return 0;
      break;

    case SNDCTL_DSP_SYNC:
      dma_sync (dev);
      dma_reset (dev);
      return 0;
      break;

    case SNDCTL_DSP_GETBLKSIZE:
      if (!(dmap->flags & DMA_ALLOC_DONE))
	reorganize_buffers (dev);

      return IOCTL_OUT (arg, dmap->fragment_size);
      break;

    case SNDCTL_DSP_SETBLKSIZE:
      {
        int size = IOCTL_IN(arg);
        
        if(!(dmap->flags & DMA_ALLOC_DONE) && size)
          {
            dmap->fragment_size = size;
	    return 0;
          }
        else
          return RET_ERROR (EINVAL);  /* Too late to change */
      }
      break;

    case SNDCTL_DSP_SUBDIVIDE:
      {
	int             fact = IOCTL_IN (arg);

	if (fact == 0)
	  {
	    fact = dmap->subdivision;
	    if (fact == 0)
	      fact = 1;
	    return IOCTL_OUT (arg, fact);
	  }

	if (dmap->subdivision != 0 ||
	    dmap->fragment_size)	/* Loo late to change */
	  return RET_ERROR (EINVAL);

	if (fact > MAX_REALTIME_FACTOR)
	  return RET_ERROR (EINVAL);

	if (fact != 1 && fact != 2 && fact != 4 && fact != 8 && fact != 16)
	  return RET_ERROR (EINVAL);

	dmap->subdivision = fact;
	return IOCTL_OUT (arg, fact);
      }
      break;

    case SNDCTL_DSP_SETFRAGMENT:
      {
	int             fact = IOCTL_IN (arg);
	int             bytes, count;

	if (fact == 0)
	  return RET_ERROR (EIO);

	if (dmap->subdivision != 0 ||
	    dmap->fragment_size)	/* Loo late to change */
	  return RET_ERROR (EINVAL);

	bytes = fact & 0xffff;
	count = (fact >> 16) & 0xffff;

	if (count == 0)
	  count = MAX_SUB_BUFFERS;

	if (bytes < 7 || bytes > 17)	/* <64 || > 128k */
	  return RET_ERROR (EINVAL);

	if (count < 2)
	  return RET_ERROR (EINVAL);

	dmap->fragment_size = (1 << bytes);
	dmap->max_fragments = count;

	if (dmap->fragment_size > audio_devs[dev]->buffsize)
	  dmap->fragment_size = audio_devs[dev]->buffsize;

	if (dmap->fragment_size == audio_devs[dev]->buffsize &&
	    audio_devs[dev]->flags & DMA_AUTOMODE)
	  dmap->fragment_size /= 2;	/* Needs at least 2 buffers */

	dmap->subdivision = 1;	/* Disable SNDCTL_DSP_SUBDIVIDE */
	return IOCTL_OUT (arg, bytes | (count << 16));
      }
      break;

    case SNDCTL_DSP_GETISPACE:
    case SNDCTL_DSP_GETOSPACE:
      if (!local)
	return RET_ERROR (EINVAL);

      {
	audio_buf_info *info = (audio_buf_info *) arg;

	info->fragments = dmap->qlen;
	info->fragsize = dmap->fragment_size;
	info->bytes = dmap->qlen * dmap->fragment_size;
      }
      return 0;

    default:
      return audio_devs[dev]->ioctl (dev, cmd, arg, local);
    }

}

static int
space_in_queue (int dev)
{
  int             len, max, tmp;
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

  if (dmap->qlen >= dmap->nbufs)	/* No space at all */
    return 0;

  /*
     * Verify that there are no more pending buffers than the limit
     * defined by the process.
   */

  max = dmap->max_fragments;
  len = dmap->qlen;

  if (audio_devs[dev]->local_qlen)
    {
      tmp = audio_devs[dev]->local_qlen (dev);
      if (tmp & len)
	tmp--;			/*
				   * This buffer has been counted twice
				 */
      len += tmp;
    }

  if (len >= max)
    return 0;
  return 1;
}

int
DMAbuf_getwrbuffer (int dev, char **buf, int *size, int dontblock)
{
  unsigned long   flags;
  int             abort, err = EIO;
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

  if (dmap->dma_mode == DMODE_INPUT)	/* Direction change */
    {
      dma_reset (dev);
      dmap->dma_mode = DMODE_NONE;
    }
  else if (dmap->flags & DMA_RESTART)	/* Restart buffering */
    {
      dma_sync (dev);
      dma_reset (dev);
    }

  dmap->flags &= ~DMA_RESTART;

  if (!(dmap->flags & DMA_ALLOC_DONE))
    reorganize_buffers (dev);

  if (!dmap->dma_mode)
    {
      int             err;

      dmap->dma_mode = DMODE_OUTPUT;
      if ((err = audio_devs[dev]->prepare_for_output (dev,
				     dmap->fragment_size, dmap->nbufs)) < 0)
	return err;
    }

  DISABLE_INTR (flags);

  abort = 0;
  while (!space_in_queue (dev) &&
	 !abort)
    {

      if (dontblock)
	{
	  RESTORE_INTR (flags);
	  return RET_ERROR (EAGAIN);
	}

      /*
       * Wait for free space
       */
      DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], 2 * HZ);
      if (TIMED_OUT (dev_sleeper[dev], dev_sleep_flag[dev]))
	{
	  printk ("Sound: DMA timed out - IRQ/DRQ config error?\n");
	  dma_reset (dev);
	  err = EIO;
	  abort = 1;
	  SET_ABORT_FLAG (dev_sleeper[dev], dev_sleep_flag[dev]);
	}
      else if (PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev]))
	{
	  err = EINTR;
	  abort = 1;
	}
    }
  RESTORE_INTR (flags);

  if (!space_in_queue (dev))
    {
      return RET_ERROR (err);	/* Caught a signal ? */
    }

  *buf = dmap->buf[dmap->qtail];
  *size = dmap->fragment_size;
  dmap->counts[dmap->qtail] = 0;

  return dmap->qtail;
}

int
DMAbuf_start_output (int dev, int buff_no, int l)
{
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

  if (buff_no != dmap->qtail)
    printk ("Sound warning: DMA buffers out of sync %d != %d\n", buff_no, dmap->qtail);

  dmap->qlen++;
  if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs)
    printk ("\nSound: Audio queue2 corrupted for dev%d (%d/%d)\n",
	    dev, dmap->qlen, dmap->nbufs);

  dmap->counts[dmap->qtail] = l;

  if ((l != dmap->fragment_size) &&
      ((audio_devs[dev]->flags & DMA_AUTOMODE) &&
       audio_devs[dev]->flags & NEEDS_RESTART))
    dmap->flags |= DMA_RESTART;
  else
    dmap->flags &= ~DMA_RESTART;

  dmap->qtail = (dmap->qtail + 1) % dmap->nbufs;

  if (!(dmap->flags & DMA_ACTIVE))
    {
      dmap->flags |= DMA_ACTIVE;
      audio_devs[dev]->output_block (dev, dmap->buf_phys[dmap->qhead],
				     dmap->counts[dmap->qhead], 0,
				 !(audio_devs[dev]->flags & DMA_AUTOMODE) ||
				     !(dmap->flags & DMA_STARTED));
      dmap->flags |= DMA_STARTED;
    }

  return 0;
}

int
DMAbuf_start_dma (int dev, unsigned long physaddr, int count, int dma_mode)
{
  int             chan = audio_devs[dev]->dmachan;
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;
  unsigned long   flags;

  /*
   * This function is not as portable as it should be.
   */

  /*
   * The count must be one less than the actual size. This is handled by
   * set_dma_addr()
   */

  if (audio_devs[dev]->flags & DMA_AUTOMODE)
    {				/*
				 * Auto restart mode. Transfer the whole *
				 * buffer
				 */
#ifdef linux
      DISABLE_INTR (flags);
      disable_dma (chan);
      clear_dma_ff (chan);
      set_dma_mode (chan, dma_mode | DMA_AUTOINIT);
      set_dma_addr (chan, dmap->raw_buf_phys[0]);
      set_dma_count (chan, dmap->bytes_in_use);
      enable_dma (chan);
      RESTORE_INTR (flags);
#else

#if defined(__FreeBSD__)

      isa_dmastart (B_RAW | ((dma_mode == DMA_MODE_READ) ? B_READ : B_WRITE),
		    (caddr_t)dmap->raw_buf_phys[0],
		    dmap->bytes_in_use,
		    chan);
#else /* else __FreeBSD__ */
#if defined(GENERIC_SYSV)
#ifndef DMAMODE_AUTO
      printk ("sound: Invalid DMA mode for device %d\n", dev);
#endif
#if defined(SVR42)

      /*
         ** send full count to snd_dma_prog, it will take care of subtracting
         ** one if it is required.
       */
      snd_dma_prog (chan, dmap->raw_buf_phys[0], dmap->bytes_in_use,
		    dma_mode, TRUE);

#else /* !SVR42 */
      dma_param (chan, ((dma_mode == DMA_MODE_READ) ? DMA_Rdmode : DMA_Wrmode)
#ifdef DMAMODE_AUTO
		 | DMAMODE_AUTO
#endif
		 ,
		 dmap->raw_buf_phys[0], dmap->bytes_in_use - 1);
      dma_enable (chan);
#endif /*  ! SVR42 */
#else
#error This routine is not valid for this OS.
#endif
#endif

#endif
    }
  else
    {
#ifdef linux
      DISABLE_INTR (flags);
      disable_dma (chan);
      clear_dma_ff (chan);
      set_dma_mode (chan, dma_mode);
      set_dma_addr (chan, physaddr);
      set_dma_count (chan, count);
      enable_dma (chan);
      RESTORE_INTR (flags);
#else
#if defined(__FreeBSD__)
      isa_dmastart ((dma_mode == DMA_MODE_READ) ? B_READ : B_WRITE,
		    (caddr_t)physaddr,
		    count,
		    chan);
#else /* FreeBSD */

#if defined(GENERIC_SYSV)
#if defined(SVR42)

      snd_dma_prog (chan, physaddr, count, dma_mode, FALSE);

#else /* ! SVR42 */
      dma_param (chan, ((dma_mode == DMA_MODE_READ) ? DMA_Rdmode : DMA_Wrmode),
		 physaddr, count);
      dma_enable (chan);
#endif /* SVR42 */
#else
#error This routine is not valid for this OS.
#endif /* GENERIC_SYSV */
#endif

#endif
    }

  return count;
}

long
DMAbuf_init (long mem_start)
{
  int             dev;

#if defined(SVR42)
  snd_dma_init ();
#endif /* SVR42 */

  /*
     * NOTE! This routine could be called several times.
   */

  for (dev = 0; dev < num_audiodevs; dev++)
    audio_devs[dev]->dmap = &dmaps[dev];
  return mem_start;
}

void
DMAbuf_outputintr (int dev, int event_type)
{
  /*
     * Event types:
     *  0 = DMA transfer done. Device still has more data in the local
     *      buffer.
     *  1 = DMA transfer done. Device doesn't have local buffer or it's
     *      empty now.
     *  2 = No DMA transfer but the device has now more space in it's local
     *      buffer.
   */

  unsigned long   flags;
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

#if defined(SVR42)
  snd_dma_intr (audio_devs[dev]->dmachan);
#endif /* SVR42 */

  if (event_type != 2)
    {
      if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs)
	{
	  printk ("\nSound: Audio queue3 corrupted for dev%d (%d/%d)\n",
		  dev, dmap->qlen, dmap->nbufs);
	  return;
	}

      dmap->qlen--;
      dmap->qhead = (dmap->qhead + 1) % dmap->nbufs;
      dmap->flags &= ~DMA_ACTIVE;

      if (dmap->qlen)
	{
	  audio_devs[dev]->output_block (dev, dmap->buf_phys[dmap->qhead],
					 dmap->counts[dmap->qhead], 1,
				  !(audio_devs[dev]->flags & DMA_AUTOMODE));
	  dmap->flags |= DMA_ACTIVE;
	}
      else if (event_type == 1)
	{
	  dmap->underrun_count++;
	  audio_devs[dev]->halt_xfer (dev);
	  if ((audio_devs[dev]->flags & DMA_AUTOMODE) &&
	      audio_devs[dev]->flags & NEEDS_RESTART)
	    dmap->flags |= DMA_RESTART;
	  else
	    dmap->flags &= ~DMA_RESTART;
	}
    }				/* event_type != 2 */

  DISABLE_INTR (flags);
  if (SOMEONE_WAITING (dev_sleeper[dev], dev_sleep_flag[dev]))
    {
      WAKE_UP (dev_sleeper[dev], dev_sleep_flag[dev]);
    }
  RESTORE_INTR (flags);
#if defined(__FreeBSD__)
  if(selinfo[dev].si_pid)
    selwakeup(&selinfo[dev]);
#endif
}

void
DMAbuf_inputintr (int dev)
{
  unsigned long   flags;
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;

#if defined(SVR42)
  snd_dma_intr (audio_devs[dev]->dmachan);
#endif /* SVR42 */

  if (dmap->qlen == (dmap->nbufs - 1))
    {
#if !defined(__FreeBSD__)	/* ignore console message. */
      printk ("Sound: Recording overrun\n");
#endif
      dmap->underrun_count++;
      audio_devs[dev]->halt_xfer (dev);
      dmap->flags &= ~DMA_ACTIVE;
      if (audio_devs[dev]->flags & DMA_AUTOMODE)
	dmap->flags |= DMA_RESTART;
      else
	dmap->flags &= ~DMA_RESTART;
    }
  else
    {
      dmap->qlen++;
      if (dmap->qlen <= 0 || dmap->qlen > dmap->nbufs)
	printk ("\nSound: Audio queue4 corrupted for dev%d (%d/%d)\n",
		dev, dmap->qlen, dmap->nbufs);
      dmap->qtail = (dmap->qtail + 1) % dmap->nbufs;

      audio_devs[dev]->start_input (dev, dmap->buf_phys[dmap->qtail],
				    dmap->fragment_size, 1,
				  !(audio_devs[dev]->flags & DMA_AUTOMODE));
      dmap->flags |= DMA_ACTIVE;
    }

  DISABLE_INTR (flags);
  if (SOMEONE_WAITING (dev_sleeper[dev], dev_sleep_flag[dev]))
    {
      WAKE_UP (dev_sleeper[dev], dev_sleep_flag[dev]);
    }
  RESTORE_INTR (flags);
#if defined(__FreeBSD__)
  if(selinfo[dev].si_pid)
    selwakeup(&selinfo[dev]);
#endif
}

int
DMAbuf_open_dma (int dev)
{
  unsigned long   flags;
  int             chan = audio_devs[dev]->dmachan;

  if (ALLOC_DMA_CHN (chan, audio_devs[dev]->name))
    {
      printk ("Unable to grab DMA%d for the audio driver\n", chan);
      return RET_ERROR (EBUSY);
    }

  DISABLE_INTR (flags);
#ifdef linux
  disable_dma (chan);
  clear_dma_ff (chan);
#endif
  RESTORE_INTR (flags);

  return 0;
}

void
DMAbuf_close_dma (int dev)
{
  int             chan = audio_devs[dev]->dmachan;

  DMAbuf_reset_dma (dev);
  RELEASE_DMA_CHN (chan);
}

void
DMAbuf_reset_dma (int dev)
{
#if 0
  int             chan = audio_devs[dev]->dmachan;

  disable_dma (chan);
#endif
}

#ifdef ALLOW_SELECT
int
DMAbuf_select (int dev, struct fileinfo *file, int sel_type, select_table * wait)
{
  struct dma_buffparms *dmap = audio_devs[dev]->dmap;
  unsigned long   flags;

  switch (sel_type)
    {
    case SEL_IN:
      if (dmap->dma_mode != DMODE_INPUT)
	return 0;

      DISABLE_INTR (flags);
      if (!dmap->qlen)
	{
#if defined(__FreeBSD__)
	  selrecord(wait, &selinfo[dev]);
#else
	  dev_sleep_flag[dev].mode = WK_SLEEP;
	  select_wait (&dev_sleeper[dev], wait);
#endif
	  RESTORE_INTR (flags);
	  return 0;
	}
      RESTORE_INTR (flags);
      return 1;
      break;

    case SEL_OUT:
      if (dmap->dma_mode == DMODE_INPUT)
	return 0;

      if (dmap->dma_mode == DMODE_NONE)
	return 1;

      DISABLE_INTR (flags);
      if (!space_in_queue (dev))
	{
#if defined(__FreeBSD__)
	  selrecord(wait, &selinfo[dev]);
#else
	  dev_sleep_flag[dev].mode = WK_SLEEP;
	  select_wait (&dev_sleeper[dev], wait);
#endif
	  RESTORE_INTR (flags);
	  return 0;
	}
      RESTORE_INTR (flags);
      return 1;
      break;

    case SEL_EX:
      return 0;
    }

  return 0;
}

#endif /* ALLOW_SELECT */

#else /* EXCLUDE_AUDIO */
/*
 * Stub versions if audio services not included
 */

int
DMAbuf_open (int dev, int mode)
{
  return RET_ERROR (ENXIO);
}

int
DMAbuf_release (int dev, int mode)
{
  return 0;
}

int
DMAbuf_getwrbuffer (int dev, char **buf, int *size, int dontblock)
{
  return RET_ERROR (EIO);
}

int
DMAbuf_getrdbuffer (int dev, char **buf, int *len, int dontblock)
{
  return RET_ERROR (EIO);
}

int
DMAbuf_rmchars (int dev, int buff_no, int c)
{
  return RET_ERROR (EIO);
}

int
DMAbuf_start_output (int dev, int buff_no, int l)
{
  return RET_ERROR (EIO);
}

int
DMAbuf_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  return RET_ERROR (EIO);
}

long
DMAbuf_init (long mem_start)
{
  return mem_start;
}

int
DMAbuf_start_dma (int dev, unsigned long physaddr, int count, int dma_mode)
{
  return RET_ERROR (EIO);
}

int
DMAbuf_open_dma (int dev)
{
  return RET_ERROR (ENXIO);
}

void
DMAbuf_close_dma (int dev)
{
  return;
}

void
DMAbuf_reset_dma (int dev)
{
  return;
}

void
DMAbuf_inputintr (int dev)
{
  return;
}

void
DMAbuf_outputintr (int dev, int underrun_flag)
{
  return;
}

#endif

#endif
