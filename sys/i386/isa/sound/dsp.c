/*
 * linux/kernel/chr_drv/sound/dsp.c
 * 
 * Device file manager for /dev/dsp
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 */

#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#ifndef EXCLUDE_AUDIO

#define ON		1
#define OFF		0

static int      wr_buff_no[MAX_DSP_DEV];	/* != -1, if there is a
						 * incomplete output block */
static int      wr_buff_size[MAX_DSP_DEV], wr_buf_ptr[MAX_DSP_DEV];
static char    *wr_dma_buf[MAX_DSP_DEV];

int
dsp_open (int dev, struct fileinfo *file, int bits)
{
  int             mode;
  int             ret;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  if ((ret = DMAbuf_open (dev, mode)) < 0)
    return ret;

  if (DMAbuf_ioctl (dev, SNDCTL_DSP_SAMPLESIZE, bits, 1) != bits)
    {
      dsp_release (dev, file);
      return RET_ERROR (ENXIO);
    }

  wr_buff_no[dev] = -1;

  return ret;
}

void
dsp_release (int dev, struct fileinfo *file)
{
  int             mode;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  if (wr_buff_no[dev] >= 0)
    {
      DMAbuf_start_output (dev, wr_buff_no[dev], wr_buf_ptr[dev]);

      wr_buff_no[dev] = -1;
    }

  DMAbuf_release (dev, mode);
}


int
dsp_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c, p, l;
  int             err;

  dev = dev >> 4;

  p = 0;
  c = count;

  if (!count)			/* Flush output */
    {
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buf_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
      return 0;
    }

  while (c)
    {				/* Perform output blocking */
      if (wr_buff_no[dev] < 0)	/* There is no incomplete buffers */
	{
	  if ((wr_buff_no[dev] = DMAbuf_getwrbuffer (dev, &wr_dma_buf[dev],
						   &wr_buff_size[dev])) < 0)
	    return wr_buff_no[dev];
	  wr_buf_ptr[dev] = 0;
	}

      l = c;
      if (l > (wr_buff_size[dev] - wr_buf_ptr[dev]))
	l = (wr_buff_size[dev] - wr_buf_ptr[dev]);

      if (!dsp_devs[dev]->copy_from_user)
	{			/* No device specific copy routine */
	  COPY_FROM_USER (&wr_dma_buf[dev][wr_buf_ptr[dev]], buf, p, l);
	}
      else
	dsp_devs[dev]->copy_from_user (dev,
			       wr_dma_buf[dev], wr_buf_ptr[dev], buf, p, l);

      c -= l;
      p += l;
      wr_buf_ptr[dev] += l;

      if (wr_buf_ptr[dev] >= wr_buff_size[dev])
	{
	  if ((err = DMAbuf_start_output (dev, wr_buff_no[dev], wr_buf_ptr[dev])) < 0)
	    return err;

	  wr_buff_no[dev] = -1;
	}

    }

  return count;
}


int
dsp_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c, p, l;
  char           *dmabuf;
  int             buff_no;

  dev = dev >> 4;
  p = 0;
  c = count;

  while (c)
    {
      if ((buff_no = DMAbuf_getrdbuffer (dev, &dmabuf, &l)) < 0)
	return buff_no;

      if (l > c)
	l = c;

      /* Insert any local processing here. */

      COPY_TO_USER (buf, 0, dmabuf, l);

      DMAbuf_rmchars (dev, buff_no, l);

      p += l;
      c -= l;
    }

  return count - c;
}

int
dsp_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg)
{

  dev = dev >> 4;

  switch (cmd)
    {
    case SNDCTL_DSP_SYNC:
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buf_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
      return DMAbuf_ioctl (dev, cmd, arg, 0);
      break;

    case SNDCTL_DSP_POST:
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buf_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
      return 0;
      break;

    case SNDCTL_DSP_RESET:
      wr_buff_no[dev] = -1;
      return DMAbuf_ioctl (dev, cmd, arg, 0);
      break;

    default:
      return DMAbuf_ioctl (dev, cmd, arg, 0);
    }
}

long
dsp_init (long mem_start)
{
  return mem_start;
}

#else
/* Stub version */
int
dsp_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
dsp_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
dsp_open (int dev, struct fileinfo *file, int bits)
{
  return RET_ERROR (ENXIO);
}

void
dsp_release (int dev, struct fileinfo *file)
  {
  };
int
dsp_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg)
{
  return RET_ERROR (EIO);
}

int
dsp_lseek (int dev, struct fileinfo *file, off_t offset, int orig)
{
  return RET_ERROR (EIO);
}

long
dsp_init (long mem_start)
{
  return mem_start;
}

#endif

#endif
