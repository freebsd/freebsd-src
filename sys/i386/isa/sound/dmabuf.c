/*
 * sound/dmabuf.c
 * 
 * The DMA buffer manager for digitized voice applications
 * 
 * Copyright by Hannu Savolainen 1993
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

#include "sound_calls.h"

#if !defined(EXCLUDE_AUDIO) || !defined(EXCLUDE_GUS)

#define MAX_SUB_BUFFERS		(32*MAX_REALTIME_FACTOR)

/*
 * The DSP channel can be used either for input or output. Variable
 * 'dma_mode' will be set when the program calls read or write first time
 * after open. Current version doesn't support mode changes without closing
 * and reopening the device. Support for this feature may be implemented in a
 * future version of this driver.
 */

#define DMODE_NONE		0
#define DMODE_OUTPUT		1
#define DMODE_INPUT		2

extern int sb_dsp_ok;
extern struct audio_operations sb_dsp_operations;
static int force_reset = 0;

DEFINE_WAIT_QUEUES (dev_sleeper[MAX_DSP_DEV], dev_sleep_flag[MAX_DSP_DEV]);

static int      dma_mode[MAX_DSP_DEV] =
{0};				/* DMODE_INPUT, DMODE_OUTPUT or DMODE_NONE */

static volatile int dmabuf_interrupted[MAX_DSP_DEV] =
{0};

/*
 * Pointers to raw buffers
 */

char           *snd_raw_buf[MAX_DSP_DEV][DSP_BUFFCOUNT] =
{
  {NULL}};
unsigned long   snd_raw_buf_phys[MAX_DSP_DEV][DSP_BUFFCOUNT];
int             snd_raw_count[MAX_DSP_DEV];

/*
 * Device state tables
 */

static int      dev_busy[MAX_DSP_DEV];
static int      dev_needs_restart[MAX_DSP_DEV];
static int      dev_modes[MAX_DSP_DEV];
static int      dev_active[MAX_DSP_DEV];
static int      dev_started[MAX_DSP_DEV];
static int      dev_qlen[MAX_DSP_DEV];
static int      dev_qhead[MAX_DSP_DEV];
static int      dev_qtail[MAX_DSP_DEV];
static int      dev_underrun[MAX_DSP_DEV];
static int      bufferalloc_done[MAX_DSP_DEV] =
{0};

/*
 * Logical buffers for each devices
 */

static int      dev_nbufs[MAX_DSP_DEV];		/* # of logical buffers ( >=

						   * sound_buffcounts[dev] */
static int      dev_counts[MAX_DSP_DEV][MAX_SUB_BUFFERS];
static int      dev_subdivision[MAX_DSP_DEV];
static unsigned long dev_buf_phys[MAX_DSP_DEV][MAX_SUB_BUFFERS];
static char    *dev_buf[MAX_DSP_DEV][MAX_SUB_BUFFERS] =
{
  {NULL}};
static int      dev_buffsize[MAX_DSP_DEV];

static void
reorganize_buffers (int dev)
{
  /*
   * This routine breaks the physical device buffers to logical ones.
   */

  unsigned        i, p, n;
  unsigned        sr, nc, sz, bsz;

  sr = dsp_devs[dev]->ioctl (dev, SOUND_PCM_READ_RATE, 0, 1);
  nc = dsp_devs[dev]->ioctl (dev, SOUND_PCM_READ_CHANNELS, 0, 1);
  sz = dsp_devs[dev]->ioctl (dev, SOUND_PCM_READ_BITS, 0, 1);

  if (sr < 1 || nc < 1 || sz < 1)
    {
      printk ("SOUND: Invalid PCM parameters[%d] sr=%d, nc=%d, sz=%d\n", dev, sr, nc, sz);
      sr = DSP_DEFAULT_SPEED;
      nc = 1;
      sz = 8;
    }

  sz /= 8;			/* Convert # of bits -> # of bytes */

  sz = sr * nc * sz;

  /*
   * Compute a buffer size not exeeding 1 second.
   */

  bsz = sound_buffsizes[dev];

  while (bsz > sz)
    bsz >>= 1;			/* Divide by 2 */

  if (sound_buffcounts[dev] == 1 && bsz == sound_buffsizes[dev])
    bsz >>= 1;			/* Need at least 2 buffers */

  if (dev_subdivision[dev] == 0)
    dev_subdivision[dev] = 1;	/* Default value */

  bsz /= dev_subdivision[dev];	/* Use smaller buffers */

  if (bsz == 0)
    bsz = 4096;			/* Just a sanity check */

  while ((sound_buffsizes[dev] * sound_buffcounts[dev]) / bsz > MAX_SUB_BUFFERS)
    bsz <<= 1;			/* Too much buffers */

  dev_buffsize[dev] = bsz;
  n = 0;

  /*
   * Now computing addresses for the logical buffers
   */

  for (i = 0; i < snd_raw_count[dev]; i++)
    {
      p = 0;

      while ((p + bsz) <= sound_buffsizes[dev])
	{
	  dev_buf[dev][n] = snd_raw_buf[dev][i] + p;
	  dev_buf_phys[dev][n] = snd_raw_buf_phys[dev][i] + p;
	  p += bsz;
	  n++;
	}
    }

  dev_nbufs[dev] = n;

  for (i = 0; i < dev_nbufs[dev]; i++)
    {
      dev_counts[dev][i] = 0;
    }

  bufferalloc_done[dev] = 1;
}

static void
dma_init_buffers (int dev)
{
  RESET_WAIT_QUEUE (dev_sleeper[dev], dev_sleep_flag[dev]);
  dev_underrun[dev] = 0;

  dev_busy[dev] = 1;

  bufferalloc_done[dev] = 0;

  dev_active[dev] = dev_qlen[dev] = dev_qtail[dev] = dev_qhead[dev] = 0;
  dev_needs_restart[dev] = dev_started[dev] = 0;
  dma_mode[dev] = DMODE_NONE;
}

int
DMAbuf_open (int dev, int mode)
{
  int             retval;

  if (dev >= num_dspdevs)
    {
      printk ("PCM device %d not installed.\n", dev);
      return RET_ERROR (ENXIO);
    }

  if (dev_busy[dev])
    return RET_ERROR (EBUSY);

  if (!dsp_devs[dev])
    {
      printk ("DSP device %d not initialized\n", dev);
      return RET_ERROR (ENXIO);
    }

#ifdef USE_RUNTIME_DMAMEM
  sound_dma_malloc (dev);
#endif

  if (snd_raw_buf[dev][0] == NULL)
    return RET_ERROR (ENOSPC);	/* Memory allocation failed during boot */

  if ((retval = dsp_devs[dev]->open (dev, mode)) < 0)
    return retval;

  dev_modes[dev] = mode;
  dev_subdivision[dev] = 0;

  dma_init_buffers (dev);
  dsp_devs[dev]->ioctl (dev, SOUND_PCM_WRITE_BITS, 8, 1);
  dsp_devs[dev]->ioctl (dev, SOUND_PCM_WRITE_CHANNELS, 1, 1);
  dsp_devs[dev]->ioctl (dev, SOUND_PCM_WRITE_RATE, DSP_DEFAULT_SPEED, 1);

  return 0;
}

static void
dma_reset (int dev)
{
  int             retval;
  unsigned long   flags;

  if (!force_reset && sb_dsp_ok && dsp_devs[dev] == &sb_dsp_operations)
	return; /* We don't need this code for SB */

  DISABLE_INTR (flags);
  dsp_devs[dev]->reset (dev);
  dsp_devs[dev]->close (dev);

  if ((retval = dsp_devs[dev]->open (dev, dev_modes[dev])) < 0)
    printk ("Sound: Reset failed - Can't reopen device\n");
  RESTORE_INTR (flags);

  dma_init_buffers (dev);
  reorganize_buffers (dev);
}

static int
dma_sync (int dev)
{
  unsigned long   flags;
  unsigned long   time;
  int             timed_out;

  if (dma_mode[dev] == DMODE_OUTPUT)
    {
      DISABLE_INTR (flags);

      timed_out = 0;
      time = GET_TIME ();

      while ((!(PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev]) ||
		dmabuf_interrupted[dev]) && !timed_out)
	     && dev_qlen[dev])
	{
	  DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], 10 * HZ);
	  if ((GET_TIME () - time) > (10 * HZ))
	    timed_out = 1;
	}
      RESTORE_INTR (flags);

      /*
       * Some devices such as GUS have huge amount of on board RAM for the
       * audio data. We have to wait util the device has finished playing.
       */

      DISABLE_INTR (flags);
      if (dsp_devs[dev]->has_output_drained)	/* Device has hidden buffers */
	{
	  while (!(PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev]) ||
		   dmabuf_interrupted[dev])
		 && !dsp_devs[dev]->has_output_drained (dev))
	    {
	      DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], HZ / 4);
	    }
	}
      RESTORE_INTR (flags);
    }
  return dev_qlen[dev];
}

int
DMAbuf_release (int dev, int mode)
{

  if (!(PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev]) ||
	dmabuf_interrupted[dev])
      && (dma_mode[dev] == DMODE_OUTPUT))
    {
      dma_sync (dev);
    }

#ifdef USE_RUNTIME_DMAMEM
  sound_dma_free (dev);
#endif

  dsp_devs[dev]->reset (dev);

  dsp_devs[dev]->close (dev);

  dma_mode[dev] = DMODE_NONE;
  dev_busy[dev] = 0;

  return 0;
}

int
DMAbuf_getrdbuffer (int dev, char **buf, int *len)
{
  unsigned long   flags;
  int             err = EIO;

  DISABLE_INTR (flags);
  if (!dev_qlen[dev])
    {
      if (dev_needs_restart[dev])
	{
	  dma_reset (dev);
	  dev_needs_restart[dev] = 0;
	}

      if (dma_mode[dev] == DMODE_OUTPUT)	/* Was output -> direction change */
	{
	  dma_sync (dev);
	  dma_reset (dev);
	  dma_mode[dev] = DMODE_NONE;
	}

      if (!bufferalloc_done[dev])
	reorganize_buffers (dev);

      if (!dma_mode[dev])
	{
	  int             err;

	  if ((err = dsp_devs[dev]->prepare_for_input (dev,
				    dev_buffsize[dev], dev_nbufs[dev])) < 0)
	    {
	      RESTORE_INTR (flags);
	      return err;
	    }
	  dma_mode[dev] = DMODE_INPUT;
	}

      if (!dev_active[dev])
	{
	  dsp_devs[dev]->start_input (dev, dev_buf_phys[dev][dev_qtail[dev]],
				      dev_buffsize[dev], 0,
				      !sound_dma_automode[dev] ||
				      !dev_started[dev]);
	  dev_active[dev] = 1;
	  dev_started[dev] = 1;
	}

      /* Wait for the next block */
      DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], 2 * HZ);
      if (TIMED_OUT (dev_sleeper[dev], dev_sleep_flag[dev]))
	{
	  printk ("Sound: DMA timed out - IRQ/DRQ config error?\n");
	  err = EIO;
	  SET_ABORT_FLAG (dev_sleeper[dev], dev_sleep_flag[dev]);
	}
      else
	err = EINTR;
    }
  RESTORE_INTR (flags);

  if (!dev_qlen[dev])
    return RET_ERROR (err);

  *buf = &dev_buf[dev][dev_qhead[dev]][dev_counts[dev][dev_qhead[dev]]];
  *len = dev_buffsize[dev] - dev_counts[dev][dev_qhead[dev]];

  return dev_qhead[dev];
}

int
DMAbuf_rmchars (int dev, int buff_no, int c)
{
  int             p = dev_counts[dev][dev_qhead[dev]] + c;

  if (p >= dev_buffsize[dev])
    {				/* This buffer is now empty */
      dev_counts[dev][dev_qhead[dev]] = 0;
      dev_qlen[dev]--;
      dev_qhead[dev] = (dev_qhead[dev] + 1) % dev_nbufs[dev];
    }
  else
    dev_counts[dev][dev_qhead[dev]] = p;

  return 0;
}

int
DMAbuf_read (int dev, snd_rw_buf * user_buf, int count)
{
  char           *dmabuf;
  int             buff_no, c, err;

  /*
   * This routine returns at most 'count' bytes from the dsp input buffers.
   * Returns negative value if there is an error.
   */

  if ((buff_no = DMAbuf_getrdbuffer (dev, &dmabuf, &c)) < 0)
    return buff_no;

  if (c > count)
    c = count;

  COPY_TO_USER (user_buf, 0, dmabuf, c);

  if ((err = DMAbuf_rmchars (dev, buff_no, c)) < 0)
    return err;
  return c;

}

int
DMAbuf_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  switch (cmd)
    {
    case SNDCTL_DSP_RESET:
      force_reset = 1;
      dma_reset (dev);
      force_reset = 0;
      return 0;
      break;

    case SNDCTL_DSP_SYNC:
      dma_sync (dev);
      dma_reset (dev);
      return 0;
      break;

    case SNDCTL_DSP_GETBLKSIZE:
      if (!bufferalloc_done[dev])
	reorganize_buffers (dev);

      return IOCTL_OUT (arg, dev_buffsize[dev]);
      break;

    case SNDCTL_DSP_SUBDIVIDE:
      {
	int             fact = IOCTL_IN (arg);

	if (fact == 0)
	  {
	    fact = dev_subdivision[dev];
	    if (fact == 0)
	      fact = 1;
	    return IOCTL_OUT (arg, fact);
	  }

	if (dev_subdivision[dev] != 0)	/* Too late to change */
	  return RET_ERROR (EINVAL);

	if (fact > MAX_REALTIME_FACTOR)
	  return RET_ERROR (EINVAL);

	if (fact != 1 && fact != 2 && fact != 4 && fact != 8 && fact != 16)
	  return RET_ERROR (EINVAL);

	dev_subdivision[dev] = fact;
	return IOCTL_OUT (arg, fact);
      }
      break;

    default:
      return dsp_devs[dev]->ioctl (dev, cmd, arg, local);
    }

  return RET_ERROR (EIO);
}

int
DMAbuf_getwrbuffer (int dev, char **buf, int *size)
{
  unsigned long   flags;
  int             err = EIO;

  if (dma_mode[dev] == DMODE_INPUT)	/* Was input -> Direction change */
    {
      dma_reset (dev);
      dma_mode[dev] = DMODE_NONE;
    }
  else if (dev_needs_restart[dev])	/* Restart buffering */
    {
      dma_sync (dev);
      dma_reset (dev);
    }

  dev_needs_restart[dev] = 0;

  if (!bufferalloc_done[dev])
    reorganize_buffers (dev);

  if (!dma_mode[dev])
    {
      int             err;

      dma_mode[dev] = DMODE_OUTPUT;
      if ((err = dsp_devs[dev]->prepare_for_output (dev,
				    dev_buffsize[dev], dev_nbufs[dev])) < 0)
	return err;
    }


  DISABLE_INTR (flags);

  RESET_WAIT_QUEUE (dev_sleeper[dev], dev_sleep_flag[dev]);

  if (dev_qlen[dev] == dev_nbufs[dev])
    {
      if (!dev_active[dev])
	{
	  printk ("Soundcard warning: DMA not activated %d/%d\n",
		  dev_qlen[dev], dev_nbufs[dev]);
	  return RET_ERROR (EIO);
	}

      /* Wait for free space */
      DO_SLEEP (dev_sleeper[dev], dev_sleep_flag[dev], 2 * HZ);
      if (TIMED_OUT (dev_sleeper[dev], dev_sleep_flag[dev]))
	{
	  printk ("Sound: DMA timed out - IRQ/DRQ config error?\n");
	  err = EIO;
	  SET_ABORT_FLAG (dev_sleeper[dev], dev_sleep_flag[dev]);
	}
      else if (PROCESS_ABORTING (dev_sleeper[dev], dev_sleep_flag[dev]))
	err = EINTR;
    }
  RESTORE_INTR (flags);

  if (dev_qlen[dev] == dev_nbufs[dev])
    return RET_ERROR (err);	/* We have got signal (?) */

  *buf = dev_buf[dev][dev_qtail[dev]];
  *size = dev_buffsize[dev];
  dev_counts[dev][dev_qtail[dev]] = 0;

  return dev_qtail[dev];
}

int
DMAbuf_start_output (int dev, int buff_no, int l)
{
  if (buff_no != dev_qtail[dev])
    printk ("Soundcard warning: DMA buffers out of sync %d != %d\n", buff_no, dev_qtail[dev]);

  dev_qlen[dev]++;

  dev_counts[dev][dev_qtail[dev]] = l;

  dev_needs_restart[dev] = (l != dev_buffsize[dev]);

  dev_qtail[dev] = (dev_qtail[dev] + 1) % dev_nbufs[dev];

  if (!dev_active[dev])
    {
      dev_active[dev] = 1;
      dsp_devs[dev]->output_block (dev, dev_buf_phys[dev][dev_qhead[dev]],
				   dev_counts[dev][dev_qhead[dev]], 0,
			     !sound_dma_automode[dev] || !dev_started[dev]);
      dev_started[dev] = 1;
    }

  return 0;
}

int
DMAbuf_start_dma (int dev, unsigned long physaddr, int count, int dma_mode)
{
  int             chan = sound_dsp_dmachan[dev];
  unsigned long   flags;

  /*
   * This function is not as portable as it should be.
   */

  /*
   * The count must be one less than the actual size. This is handled by
   * set_dma_addr()
   */

  if (sound_dma_automode[dev])
    {				/* Auto restart mode. Transfer the whole
				 * buffer */
#ifdef linux
      DISABLE_INTR (flags);
      disable_dma (chan);
      clear_dma_ff (chan);
      set_dma_mode (chan, dma_mode | DMA_AUTOINIT);
      set_dma_addr (chan, (caddr_t)snd_raw_buf_phys[dev][0]);
      set_dma_count (chan, (unsigned)sound_buffsizes[dev]);
      enable_dma ((unsigned)chan);
      RESTORE_INTR (flags);
#else

#ifdef __386BSD__
      printk ("sound: Invalid DMA mode for device %d\n", dev);

      isa_dmastart ((dma_mode == DMA_MODE_READ) ? B_READ : B_WRITE,
		    (caddr_t)snd_raw_buf_phys[dev][0],
		    sound_buffsizes[dev],
		    chan);
#else
#if defined(ISC) || defined(SCO)
#ifndef DMAMODE_AUTO
      printk ("sound: Invalid DMA mode for device %d\n", dev);
#endif
      dma_param (chan, ((dma_mode == DMA_MODE_READ) ? DMA_Rdmode : DMA_Wrmode)
#ifdef DMAMODE_AUTO
		 | DMAMODE_AUTO
#endif
		 ,
		 snd_raw_buf_phys[dev][0], count);
      dma_enable (chan);
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
#ifdef __386BSD__
      isa_dmastart ((dma_mode == DMA_MODE_READ) ? B_READ : B_WRITE,
		    (caddr_t)physaddr,
		    count,
		    chan);
#else

#if defined(ISC) || defined(SCO)
      dma_param (chan, ((dma_mode == DMA_MODE_READ) ? DMA_Rdmode : DMA_Wrmode),
		 physaddr, count);
      dma_enable (chan);
#else
#error This routine is not valid for this OS.
#endif /* !ISC */
#endif

#endif
    }

  return count;
}

long
DMAbuf_init (long mem_start)
{
  int             i;

  /*
   * In this version the DMA buffer allocation is done by sound_mem_init()
   * which is called by init/main.c
   */

  for (i = 0; i < MAX_DSP_DEV; i++)
    {
      dev_qlen[i] = 0;
      dev_qhead[i] = 0;
      dev_qtail[i] = 0;
      dev_active[i] = 0;
      dev_busy[i] = 0;
      bufferalloc_done[i] = 0;
    }

  return mem_start;
}

void
DMAbuf_outputintr (int dev, int underrun_flag)
{
  unsigned long   flags;

  dev_qlen[dev]--;
  dev_qhead[dev] = (dev_qhead[dev] + 1) % dev_nbufs[dev];
  dev_active[dev] = 0;

  if (dev_qlen[dev])
    {
      dsp_devs[dev]->output_block (dev, dev_buf_phys[dev][dev_qhead[dev]],
				   dev_counts[dev][dev_qhead[dev]], 1,
				   !sound_dma_automode[dev]);
      dev_active[dev] = 1;
    }
  else if (underrun_flag)
    {
      dev_underrun[dev]++;
      dsp_devs[dev]->halt_xfer (dev);
      dev_needs_restart[dev] = 1;
    }

  DISABLE_INTR (flags);
  if (SOMEONE_WAITING (dev_sleeper[dev], dev_sleep_flag[dev]))
    {
      WAKE_UP (dev_sleeper[dev], dev_sleep_flag[dev]);
    }
  RESTORE_INTR (flags);
}

void
DMAbuf_inputintr (int dev)
{
  unsigned long   flags;

  if (!dev_busy[dev])
    {
      dsp_devs[dev]->close (dev);
    }
  else if (dev_qlen[dev] == (dev_nbufs[dev] - 1))
    {
      printk ("Sound: Recording overrun\n");
      dev_underrun[dev]++;
      dsp_devs[dev]->halt_xfer (dev);
      dev_active[dev] = 0;
      dev_needs_restart[dev] = 1;
    }
  else
    {
      dev_qlen[dev]++;
      dev_qtail[dev] = (dev_qtail[dev] + 1) % dev_nbufs[dev];

      dsp_devs[dev]->start_input (dev, dev_buf_phys[dev][dev_qtail[dev]],
				  dev_buffsize[dev], 1,
				  !sound_dma_automode[dev]);
      dev_active[dev] = 1;
    }

  DISABLE_INTR (flags);
  if (SOMEONE_WAITING (dev_sleeper[dev], dev_sleep_flag[dev]))
    {
      WAKE_UP (dev_sleeper[dev], dev_sleep_flag[dev]);
    }
  RESTORE_INTR (flags);
}

int
DMAbuf_open_dma (int dev)
{
  unsigned long   flags;
  int             chan = sound_dsp_dmachan[dev];

  if (ALLOC_DMA_CHN (chan))
    {
      printk ("Unable to grab DMA%d for the audio driver\n", chan);
      return 0;
    }

  DISABLE_INTR (flags);
#ifdef linux
  disable_dma (chan);
  clear_dma_ff (chan);
#endif
  RESTORE_INTR (flags);

  return 1;
}

void
DMAbuf_close_dma (int dev)
{
  int             chan = sound_dsp_dmachan[dev];

  DMAbuf_reset_dma (chan);
  RELEASE_DMA_CHN (chan);
}

void
DMAbuf_reset_dma (int chan)
{
}

/*
 * The sound_mem_init() is called by mem_init() immediately after mem_map is
 * initialized and before free_page_list is created.
 * 
 * This routine allocates DMA buffers at the end of available physical memory (
 * <16M) and marks pages reserved at mem_map.
 */

#else
/* Stub versions if audio services not included  */

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
DMAbuf_read (int dev, snd_rw_buf * user_buf, int count)
{
  return RET_ERROR (EIO);
}

int
DMAbuf_getwrbuffer (int dev, char **buf, int *size)
{
  return RET_ERROR (EIO);
}

int
DMAbuf_getrdbuffer (int dev, char **buf, int *len)
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
DMAbuf_open_dma (int chan)
{
  return RET_ERROR (ENXIO);
}

void
DMAbuf_close_dma (int chan)
{
  return;
}

void
DMAbuf_reset_dma (int chan)
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
