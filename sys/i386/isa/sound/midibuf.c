/*
 * linux/kernel/chr_drv/sound/midibuf.c
 * 
 * Device file manager for /dev/midi
 * 
 * NOTE! This part of the driver is currently just a stub.
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 * 
 * Based on the Midi driver for bsd386 by Mike Durian.
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

  if ((err = midi_devs[mpu401_dev]->open (mpu401_dev, mode)) < 0)
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
