/*
 * sound/audio.c
 *
 * Device file manager for /dev/audio
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
#ifndef EXCLUDE_AUDIO

#include "ulaw.h"
#include "coproc.h"

#define ON		1
#define OFF		0

static int      wr_buff_no[MAX_AUDIO_DEV];	/*

						 * != -1, if there is
						 * a incomplete output
						 * block in the queue.
						 */
static int      wr_buff_size[MAX_AUDIO_DEV], wr_buff_ptr[MAX_AUDIO_DEV];

static int      audio_mode[MAX_AUDIO_DEV];
static int      dev_nblock[MAX_AUDIO_DEV];	/* 1 if in noblocking mode */

#define		AM_NONE		0
#define		AM_WRITE	1
#define 	AM_READ		2

static char    *wr_dma_buf[MAX_AUDIO_DEV];
static int      audio_format[MAX_AUDIO_DEV];
static int      local_conversion[MAX_AUDIO_DEV];

static int
set_format (int dev, int fmt)
{
  if (fmt != AFMT_QUERY)
    {

      local_conversion[dev] = 0;

      if (!(audio_devs[dev]->format_mask & fmt))	/* Not supported */
	if (fmt == AFMT_MU_LAW)
	  {
	    fmt = AFMT_U8;
	    local_conversion[dev] = AFMT_MU_LAW;
	  }
	else
	  fmt = AFMT_U8;	/* This is always supported */

      audio_format[dev] = DMAbuf_ioctl (dev, SNDCTL_DSP_SETFMT, fmt, 1);
    }

  if (local_conversion[dev])	/* This shadows the HW format */
    return local_conversion[dev];

  return audio_format[dev];
}

int
audio_open (int dev, struct fileinfo *file)
{
  int             ret;
  int             bits;
  int             dev_type = dev & 0x0f;
  int             mode = file->mode & O_ACCMODE;

  dev = dev >> 4;

  if (dev_type == SND_DEV_DSP16)
    bits = 16;
  else
    bits = 8;

  if ((ret = DMAbuf_open (dev, mode)) < 0)
    return ret;

  if (audio_devs[dev]->coproc)
    if ((ret = audio_devs[dev]->coproc->
	 open (audio_devs[dev]->coproc->devc, COPR_PCM)) < 0)
      {
	audio_release (dev, file);
	printk ("Sound: Can't access coprocessor device\n");

	return ret;
      }

  local_conversion[dev] = 0;

  if (DMAbuf_ioctl (dev, SNDCTL_DSP_SETFMT, bits, 1) != bits)
    {
      audio_release (dev, file);
      return RET_ERROR (ENXIO);
    }

  if (dev_type == SND_DEV_AUDIO)
    {
      set_format (dev, AFMT_MU_LAW);
    }
  else
    set_format (dev, bits);

  wr_buff_no[dev] = -1;
  audio_mode[dev] = AM_NONE;
  wr_buff_size[dev] = wr_buff_ptr[dev] = 0;
  dev_nblock[dev] = 0;

  return ret;
}

void
audio_release (int dev, struct fileinfo *file)
{
  int             mode;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  if (wr_buff_no[dev] >= 0)
    {
      DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

      wr_buff_no[dev] = -1;
    }

  if (audio_devs[dev]->coproc)
    audio_devs[dev]->coproc->close (audio_devs[dev]->coproc->devc, COPR_PCM);
  DMAbuf_release (dev, mode);
}

#ifdef NO_INLINE_ASM
static void
translate_bytes (const unsigned char *table, unsigned char *buff, unsigned long n)
{
  unsigned long   i;

  for (i = 0; i < n; ++i)
    buff[i] = table[buff[i]];
}

#else
static inline void
translate_bytes (const void *table, void *buff, unsigned long n)
{
  __asm__ ("cld\n"
	   "1:\tlodsb\n\t"
	   "xlatb\n\t"
	   "stosb\n\t"
"loop 1b\n\t":
:	   "b" ((long) table), "c" (n), "D" ((long) buff), "S" ((long) buff)
:	   "bx", "cx", "di", "si", "ax");
}

#endif

int
audio_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c, p, l;
  int             err;

  dev = dev >> 4;

  p = 0;
  c = count;

  if (audio_mode[dev] == AM_READ)	/*
					 * Direction changed
					 */
    {
      wr_buff_no[dev] = -1;
    }

  audio_mode[dev] = AM_WRITE;

  if (!count)			/*
				 * Flush output
				 */
    {
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
      return 0;
    }

  while (c)
    {				/*
				 * Perform output blocking
				 */
      if (wr_buff_no[dev] < 0)	/*
				 * There is no incomplete buffers
				 */
	{
	  if ((wr_buff_no[dev] = DMAbuf_getwrbuffer (dev, &wr_dma_buf[dev],
						     &wr_buff_size[dev],
						     dev_nblock[dev])) < 0)
	    {
	      /* Handle nonblocking mode */
#if defined(__FreeBSD__)
	      if (dev_nblock[dev] && wr_buff_no[dev] == RET_ERROR (EWOULDBLOCK))
		return wr_buff_no[dev];	/*
					 * XXX Return error, write() will
					 * supply # of accepted bytes.
					 * In fact, in FreeBSD the check
					 * above should not be needed
					 */
#else
	      if (dev_nblock[dev] && wr_buff_no[dev] == RET_ERROR (EAGAIN))
		return p;	/* No more space. Return # of accepted bytes */
#endif
	      return wr_buff_no[dev];
	    }
	  wr_buff_ptr[dev] = 0;
	}

      l = c;
      if (l > (wr_buff_size[dev] - wr_buff_ptr[dev]))
	l = (wr_buff_size[dev] - wr_buff_ptr[dev]);

      if (!audio_devs[dev]->copy_from_user)
	{			/*
				 * No device specific copy routine
				 */
	  COPY_FROM_USER (&wr_dma_buf[dev][wr_buff_ptr[dev]], buf, p, l);
	}
      else
	audio_devs[dev]->copy_from_user (dev,
			      wr_dma_buf[dev], wr_buff_ptr[dev], buf, p, l);


      /*
       * Insert local processing here
       */

      if (local_conversion[dev] == AFMT_MU_LAW)
	{
#ifdef linux
	  /*
	   * This just allows interrupts while the conversion is running
	   */
	  __asm__ ("sti");
#endif
	  translate_bytes (ulaw_dsp, (unsigned char *) &wr_dma_buf[dev][wr_buff_ptr[dev]], l);
	}

      c -= l;
      p += l;
      wr_buff_ptr[dev] += l;

      if (wr_buff_ptr[dev] >= wr_buff_size[dev])
	{
	  if ((err = DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev])) < 0)
	    {
	      return err;
	    }

	  wr_buff_no[dev] = -1;
	}

    }

  return count;
}

int
audio_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c, p, l;
  char           *dmabuf;
  int             buff_no;

  dev = dev >> 4;
  p = 0;
  c = count;

  if (audio_mode[dev] == AM_WRITE)
    {
      if (wr_buff_no[dev] >= 0)
	{
	  DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

	  wr_buff_no[dev] = -1;
	}
    }

  audio_mode[dev] = AM_READ;

  while (c)
    {
      if ((buff_no = DMAbuf_getrdbuffer (dev, &dmabuf, &l,
					 dev_nblock[dev])) < 0)
	{
	  /* Nonblocking mode handling. Return current # of bytes */

#if defined(__FreeBSD__)
	  if (dev_nblock[dev] && buff_no == RET_ERROR (EWOULDBLOCK))
	    return buff_no;	/*
	    			 * XXX Return error, read() will supply
	    			 * # of bytes actually read. In fact,
	    			 * in FreeBSD the check above should not
	    			 * be needed
	    			 */
#else
	  if (dev_nblock[dev] && buff_no == RET_ERROR (EAGAIN))
	    return p;
#endif

	  return buff_no;
	}

      if (l > c)
	l = c;

      /*
       * Insert any local processing here.
       */

      if (local_conversion[dev] == AFMT_MU_LAW)
	{
#ifdef linux
	  /*
	   * This just allows interrupts while the conversion is running
	   */
	  __asm__ ("sti");
#endif

	  translate_bytes (dsp_ulaw, (unsigned char *) dmabuf, l);
	}

      COPY_TO_USER (buf, p, dmabuf, l);

      DMAbuf_rmchars (dev, buff_no, l);

      p += l;
      c -= l;
    }

  return count - c;
}

int
audio_ioctl (int dev, struct fileinfo *file,
	     unsigned int cmd, unsigned int arg)
{

  dev = dev >> 4;

  if (((cmd >> 8) & 0xff) == 'C')
    {
      if (audio_devs[dev]->coproc)	/* Coprocessor ioctl */
	return audio_devs[dev]->coproc->ioctl (audio_devs[dev]->coproc->devc, cmd, arg, 0);
      else
	printk ("/dev/dsp%d: No coprocessor for this device\n", dev);

      return RET_ERROR (EREMOTEIO);
    }
  else
    switch (cmd)
      {
      case SNDCTL_DSP_SYNC:
	if (wr_buff_no[dev] >= 0)
	  {
	    DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

	    wr_buff_no[dev] = -1;
	  }
	return DMAbuf_ioctl (dev, cmd, arg, 0);
	break;

      case SNDCTL_DSP_POST:
	if (wr_buff_no[dev] >= 0)
	  {
	    DMAbuf_start_output (dev, wr_buff_no[dev], wr_buff_ptr[dev]);

	    wr_buff_no[dev] = -1;
	  }
	return 0;
	break;

      case SNDCTL_DSP_RESET:
	wr_buff_no[dev] = -1;
	return DMAbuf_ioctl (dev, cmd, arg, 0);
	break;

      case SNDCTL_DSP_GETFMTS:
	return IOCTL_OUT (arg, audio_devs[dev]->format_mask);
	break;

      case SNDCTL_DSP_SETFMT:
	return IOCTL_OUT (arg, set_format (dev, IOCTL_IN (arg)));

      case SNDCTL_DSP_GETISPACE:
	if (audio_mode[dev] == AM_WRITE)
	  return RET_ERROR (EBUSY);

	{
	  audio_buf_info  info;

	  int             err = DMAbuf_ioctl (dev, cmd, (unsigned long) &info, 1);

	  if (err < 0)
	    return err;

	  if (wr_buff_no[dev] != -1)
	    info.bytes += wr_buff_ptr[dev];

	  IOCTL_TO_USER ((char *) arg, 0, (char *) &info, sizeof (info));
	  return 0;
	}

      case SNDCTL_DSP_GETOSPACE:
	if (audio_mode[dev] == AM_READ)
	  return RET_ERROR (EBUSY);

	{
	  audio_buf_info  info;

	  int             err = DMAbuf_ioctl (dev, cmd, (unsigned long) &info, 1);

	  if (err < 0)
	    return err;

	  if (wr_buff_no[dev] != -1)
	    info.bytes += wr_buff_size[dev] - wr_buff_ptr[dev];

	  IOCTL_TO_USER ((char *) arg, 0, (char *) &info, sizeof (info));
	  return 0;
	}

      case SNDCTL_DSP_NONBLOCK:
	dev_nblock[dev] = 1;
	return 0;
	break;

#ifdef __FreeBSD__
      case FIONBIO:	/* XXX Is this the same in Linux? */
	if (*(int *)arg)
	  dev_nblock[dev] = 1;
	else
	  dev_nblock[dev] = 0;
	return 0;
	break;

      case FIOASYNC:
        return 0;	/* XXX Useful for ampling input notification? */
        break;
#endif

      default:
	return DMAbuf_ioctl (dev, cmd, arg, 0);
      }
}

long
audio_init (long mem_start)
{
  /*
     * NOTE! This routine could be called several times during boot.
   */
  return mem_start;
}

#ifdef ALLOW_SELECT
int
audio_select (int dev, struct fileinfo *file, int sel_type, select_table * wait)
{
  int             l;
  char           *dmabuf;

  dev = dev >> 4;

  switch (sel_type)
    {
    case SEL_IN:
      if (audio_mode[dev] != AM_READ &&	/* Wrong direction */
	  audio_mode[dev] != AM_NONE)
	return 0;

      if (DMAbuf_getrdbuffer (dev, &dmabuf, &l,
			      1 /* Don't block */ ) >= 0)
	return 1;		/* We have data */

      return DMAbuf_select (dev, file, sel_type, wait);
      break;

    case SEL_OUT:
      if (audio_mode[dev] != AM_WRITE &&	/* Wrong direction */
	  audio_mode[dev] != AM_NONE)
	return 0;

      if (wr_buff_no[dev] != -1)
	return 1;		/* There is space in the current buffer */

      return DMAbuf_select (dev, file, sel_type, wait);
      break;

    case SEL_EX:
      return 0;
    }

  return 0;
}

#endif /* ALLOW_SELECT */

#else /* EXCLUDE_AUDIO */
/*
 * Stub versions
 */

int
audio_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
audio_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
audio_open (int dev, struct fileinfo *file)
{
  return RET_ERROR (ENXIO);
}

void
audio_release (int dev, struct fileinfo *file)
{
};
int
audio_ioctl (int dev, struct fileinfo *file,
	     unsigned int cmd, unsigned int arg)
{
  return RET_ERROR (EIO);
}

int
audio_lseek (int dev, struct fileinfo *file, off_t offset, int orig)
{
  return RET_ERROR (EIO);
}

long
audio_init (long mem_start)
{
  return mem_start;
}

#endif

#endif
