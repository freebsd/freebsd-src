/*
 * Copyright by UWM - comments to soft-eng@cs.uwm.edu
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
 */
#define _MIDI_TABLE_C_
#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#ifndef EXCLUDE_CHIP_MIDI


static int      generic_midi_busy[MAX_MIDI_DEV];

long
CMIDI_init (long mem_start)
{

  int             i;
  int             n = num_midi_drivers;

  /*
   * int  n = sizeof (midi_supported) / sizeof( struct generic_midi_info );
   */
  for (i = 0; i < n; i++)
    {
      if (midi_supported[i].attach (mem_start))
	{
	  printk ("MIDI: Successfully attached %s\n", midi_supported[i].name);
	}

    }
  return (mem_start);
}


int
CMIDI_open (int dev, struct fileinfo *file)
{

  int             mode, err, retval;

  dev = dev >> 4;

  mode = file->mode & O_ACCMODE;


  if (generic_midi_busy[dev])
    return (RET_ERROR (EBUSY));


  if (dev >= num_generic_midis)
    {
      printk (" MIDI device %d not installed.\n", dev);
      return (ENXIO);
    }

  if (!generic_midi_devs[dev])
    {
      printk (" MIDI device %d not initialized\n", dev);
      return (ENXIO);
    }

  /* If all good and healthy, go ahead and issue call! */


  retval = generic_midi_devs[dev]->open (dev, mode);

  /* If everything ok, set device as busy */

  if (retval >= 0)
    generic_midi_busy[dev] = 1;

  return (retval);

}

int
CMIDI_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{

  int             retval;

  dev = dev >> 4;

  if (dev >= num_generic_midis)
    {
      printk (" MIDI device %d not installed.\n", dev);
      return (ENXIO);
    }

  /*
   * Make double sure of healthiness -- doubt Need we check this again??
   * 
   */

  if (!generic_midi_devs[dev])
    {
      printk (" MIDI device %d not initialized\n", dev);
      return (ENXIO);
    }

  /* If all good and healthy, go ahead and issue call! */


  retval = generic_midi_devs[dev]->write (dev, buf);

  return (retval);

}

int
CMIDI_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             retval;

  dev = dev >> 4;

  if (dev >= num_generic_midis)
    {
      printk (" MIDI device %d not installed.\n", dev);
      return (ENXIO);
    }

  /*
   * Make double sure of healthiness -- doubt Need we check this again??
   * 
   */

  if (!generic_midi_devs[dev])
    {
      printk (" MIDI device %d not initialized\n", dev);
      return (ENXIO);
    }

  /* If all good and healthy, go ahead and issue call! */


  retval = generic_midi_devs[dev]->read (dev, buf);

  return (retval);

}

int
CMIDI_close (int dev, struct fileinfo *file)
{

  int             retval;

  dev = dev >> 4;

  if (dev >= num_generic_midis)
    {
      printk (" MIDI device %d not installed.\n", dev);
      return (ENXIO);
    }

  /*
   * Make double sure of healthiness -- doubt Need we check this again??
   * 
   */

  if (!generic_midi_devs[dev])
    {
      printk (" MIDI device %d not initialized\n", dev);
      return (ENXIO);
    }

  /* If all good and healthy, go ahead and issue call! */


  generic_midi_devs[dev]->close (dev);

  generic_midi_busy[dev] = 0;	/* Free the device */

  return (0);

}

#endif

#endif
