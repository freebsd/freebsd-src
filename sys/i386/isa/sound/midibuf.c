/*
 * linux/kernel/chr_drv/sound/midibuf.c
 * 
 * Device file manager for /dev/midi
 * 
 * NOTE! This part of the driver is currently just a stub.
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

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_MPU401)

#if 0
#include "midiioctl.h"
#include "midivar.h"
#endif

static int      midibuf_busy = 0;

int
MIDIbuf_open (int dev, struct fileinfo *file)
{
  int             mode, err;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  if (midibuf_busy)
    return RET_ERROR (EBUSY);

  if (!mpu401_dev)
    {
      printk ("Midi: MPU-401 compatible Midi interface not present\n");
      return RET_ERROR (ENXIO);
    }

  if ((err = midi_devs[mpu401_dev]->open (mpu401_dev, mode, NULL, NULL)) < 0)
    return err;

  midibuf_busy = 1;

  return RET_ERROR (ENXIO);
}

void
MIDIbuf_release (int dev, struct fileinfo *file)
{
  int             mode;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  midi_devs[mpu401_dev]->close (mpu401_dev);
  midibuf_busy = 0;
}

int
MIDIbuf_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{

  dev = dev >> 4;

  return count;
}


int
MIDIbuf_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  dev = dev >> 4;

  return RET_ERROR (EIO);
}

int
MIDIbuf_ioctl (int dev, struct fileinfo *file,
	       unsigned int cmd, unsigned int arg)
{
  dev = dev >> 4;

  switch (cmd)
    {

    default:
      return midi_devs[0]->ioctl (dev, cmd, arg);
    }
}

void
MIDIbuf_bytes_received (int dev, unsigned char *buf, int count)
{
}

long
MIDIbuf_init (long mem_start)
{
  return mem_start;
}

#endif
