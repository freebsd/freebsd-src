/*
 * sound/sound_switch.c
 *
 * The system call switch
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

struct sbc_device
  {
    int             usecount;
  };

static struct sbc_device sbc_devices[SND_NDEVS] =
{
  {0}};

static int      in_use = 0;	/*


				 * *  * * Total # of open device files
				 * (excluding * * * minor 0)   */

/*
 * /dev/sndstatus -device
 */
static char    *status_buf = NULL;
static int      status_len, status_ptr;
static int      status_busy = 0;

static int
put_status (char *s)
{
  int             l;

  for (l = 0; l < 256, s[l]; l++);	/*
					 * l=strlen(s);
					 */

  if (status_len + l >= 4000)
    return 0;

  memcpy (&status_buf[status_len], s, l);
  status_len += l;

  return 1;
}

static int
put_status_int (unsigned int val, int radix)
{
  int             l, v;

  static char     hx[] = "0123456789abcdef";
  char            buf[11];

  if (!val)
    return put_status ("0");

  l = 0;
  buf[10] = 0;

  while (val)
    {
      v = val % radix;
      val = val / radix;

      buf[9 - l] = hx[v];
      l++;
    }

  if (status_len + l >= 4000)
    return 0;

  memcpy (&status_buf[status_len], &buf[10 - l], l);
  status_len += l;

  return 1;
}

static void
init_status (void)
{
  /*
   * Write the status information to the status_buf and update status_len.
   * There is a limit of 4000 bytes for the data.
   */

  int             i;

  status_ptr = 0;

  put_status ("Sound Driver:" SOUND_VERSION_STRING
	      " (" SOUND_CONFIG_DATE " " SOUND_CONFIG_BY "@"
	      SOUND_CONFIG_HOST "." SOUND_CONFIG_DOMAIN ")"
	      "\n");

  if (!put_status ("Config options: "))
    return;
  if (!put_status_int (SELECTED_SOUND_OPTIONS, 16))
    return;

  if (!put_status ("\n\nInstalled drivers: \n"))
    return;

  for (i = 0; i < (num_sound_drivers - 1); i++)
    {
      if (!put_status ("Type "))
	return;
      if (!put_status_int (sound_drivers[i].card_type, 10))
	return;
      if (!put_status (": "))
	return;
      if (!put_status (sound_drivers[i].name))
	return;

      if (!put_status ("\n"))
	return;
    }

  if (!put_status ("\n\nCard config: \n"))
    return;

  for (i = 0; i < (num_sound_cards - 1); i++)
    {
      int             drv;

      if (!snd_installed_cards[i].enabled)
	if (!put_status ("("))
	  return;

      /*
       * if (!put_status_int(snd_installed_cards[i].card_type, 10)) return;
       * if (!put_status (": ")) return;
       */

      if ((drv = snd_find_driver (snd_installed_cards[i].card_type)) != -1)
	if (!put_status (sound_drivers[drv].name))
	  return;

      if (!put_status (" at 0x"))
	return;
      if (!put_status_int (snd_installed_cards[i].config.io_base, 16))
	return;
      if (!put_status (" irq "))
	return;
      if (!put_status_int (snd_installed_cards[i].config.irq, 10))
	return;
      if (!put_status (" drq "))
	return;
      if (!put_status_int (snd_installed_cards[i].config.dma, 10))
	return;

      if (!snd_installed_cards[i].enabled)
	if (!put_status (")"))
	  return;

      if (!put_status ("\n"))
	return;
    }

  if (!put_status ("\nPCM devices:\n"))
    return;

  for (i = 0; i < num_audiodevs; i++)
    {
      if (!put_status_int (i, 10))
	return;
      if (!put_status (": "))
	return;
      if (!put_status (audio_devs[i]->name))
	return;
      if (!put_status ("\n"))
	return;
    }

  if (!put_status ("\nSynth devices:\n"))
    return;

  for (i = 0; i < num_synths; i++)
    {
      if (!put_status_int (i, 10))
	return;
      if (!put_status (": "))
	return;
      if (!put_status (synth_devs[i]->info->name))
	return;
      if (!put_status ("\n"))
	return;
    }

  if (!put_status ("\nMidi devices:\n"))
    return;

  for (i = 0; i < num_midis; i++)
    {
      if (!put_status_int (i, 10))
	return;
      if (!put_status (": "))
	return;
      if (!put_status (midi_devs[i]->info.name))
	return;
      if (!put_status ("\n"))
	return;
    }

  if (!put_status ("\nMIDI Timers:\n"))
    return;

  for (i = 0; i < num_sound_timers; i++)
    {
      if (!put_status_int (i, 10))
	return;
      if (!put_status (": "))
	return;
      if (!put_status (sound_timer_devs[i]->info.name))
	return;
      if (!put_status ("\n"))
	return;
    }

  if (!put_status ("\n"))
    return;
  if (!put_status_int (num_mixers, 10))
    return;
  if (!put_status (" mixer(s) installed\n"))
    return;
}

static int
read_status (snd_rw_buf * buf, int count)
{
  /*
   * Return at most 'count' bytes from the status_buf.
   */
  int             l, c;

  l = count;
  c = status_len - status_ptr;

  if (l > c)
    l = c;
  if (l <= 0)
    return 0;

  COPY_TO_USER (buf, 0, &status_buf[status_ptr], l);
  status_ptr += l;

  return l;
}

int
sound_read_sw (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  DEB (printk ("sound_read_sw(dev=%d, count=%d)\n", dev, count));

  switch (dev & 0x0f)
    {
    case SND_DEV_STATUS:
      return read_status (buf, count);
      break;

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
      return audio_read (dev, file, buf, count);
      break;

    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
      return sequencer_read (dev, file, buf, count);
      break;

#ifndef EXCLUDE_MIDI
    case SND_DEV_MIDIN:
      return MIDIbuf_read (dev, file, buf, count);
#endif

#ifndef EXCLUDE_PSS
    case SND_DEV_PSS:
      return pss_read (dev, file, buf, count);
#endif

    default:
      printk ("Sound: Undefined minor device %d\n", dev);
    }

  return RET_ERROR (EPERM);
}

int
sound_write_sw (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{

  DEB (printk ("sound_write_sw(dev=%d, count=%d)\n", dev, count));

  switch (dev & 0x0f)
    {

    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
      return sequencer_write (dev, file, buf, count);
      break;

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
      return audio_write (dev, file, buf, count);
      break;

#ifndef EXCLUDE_MIDI
    case SND_DEV_MIDIN:
      return MIDIbuf_write (dev, file, buf, count);
#endif

#ifndef EXCLUDE_PSS
    case SND_DEV_PSS:
      return pss_write (dev, file, buf, count);
#endif

    default:
      return RET_ERROR (EPERM);
    }

  return count;
}

int
sound_open_sw (int dev, struct fileinfo *file)
{
  int             retval;

  DEB (printk ("sound_open_sw(dev=%d) : usecount=%d\n", dev, sbc_devices[dev].usecount));

  if ((dev >= SND_NDEVS) || (dev < 0))
    {
      printk ("Invalid minor device %d\n", dev);
      return RET_ERROR (ENXIO);
    }

  switch (dev & 0x0f)
    {
    case SND_DEV_STATUS:
      if (status_busy)
	return RET_ERROR (EBUSY);
      status_busy = 1;
      if ((status_buf = (char *) KERNEL_MALLOC (4000)) == NULL)
	return RET_ERROR (EIO);
      status_len = status_ptr = 0;
      init_status ();
      break;

    case SND_DEV_CTL:
      return 0;
      break;

    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
      if ((retval = sequencer_open (dev, file)) < 0)
	return retval;
      break;

#ifndef EXCLUDE_MIDI
    case SND_DEV_MIDIN:
      if ((retval = MIDIbuf_open (dev, file)) < 0)
	return retval;
      break;
#endif

#ifndef EXCLUDE_PSS
    case SND_DEV_PSS:
      if ((retval = pss_open (dev, file)) < 0)
	return retval;
      break;
#endif

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
      if ((retval = audio_open (dev, file)) < 0)
	return retval;
      break;

    default:
      printk ("Invalid minor device %d\n", dev);
      return RET_ERROR (ENXIO);
    }

  sbc_devices[dev].usecount++;
  in_use++;

  return 0;
}

void
sound_release_sw (int dev, struct fileinfo *file)
{

  DEB (printk ("sound_release_sw(dev=%d)\n", dev));

  switch (dev & 0x0f)
    {
    case SND_DEV_STATUS:
      if (status_buf)
	KERNEL_FREE (status_buf);
      status_buf = NULL;
      status_busy = 0;
      break;

    case SND_DEV_CTL:
      break;

    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
      sequencer_release (dev, file);
      break;

#ifndef EXCLUDE_MIDI
    case SND_DEV_MIDIN:
      MIDIbuf_release (dev, file);
      break;
#endif

#ifndef EXCLUDE_PSS
    case SND_DEV_PSS:
      pss_release (dev, file);
      break;
#endif

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
      audio_release (dev, file);
      break;

    default:
      printk ("Sound error: Releasing unknown device 0x%02x\n", dev);
    }

  sbc_devices[dev].usecount--;
  in_use--;
}

int
sound_ioctl_sw (int dev, struct fileinfo *file,
		unsigned int cmd, unsigned long arg)
{
  DEB (printk ("sound_ioctl_sw(dev=%d, cmd=0x%x, arg=0x%x)\n", dev, cmd, arg));

  if ((dev & 0x0f) != SND_DEV_CTL && num_mixers > 0)
    if ((cmd >> 8) & 0xff == 'M')	/*
					 * Mixer ioctl
					 */
      return mixer_devs[0]->ioctl (0, cmd, arg);

  switch (dev & 0x0f)
    {

    case SND_DEV_CTL:

      if (!num_mixers)
	return RET_ERROR (ENXIO);

      dev = dev >> 4;

      if (dev >= num_mixers)
	return RET_ERROR (ENXIO);

      return mixer_devs[dev]->ioctl (dev, cmd, arg);
      break;

    case SND_DEV_SEQ:
    case SND_DEV_SEQ2:
      return sequencer_ioctl (dev, file, cmd, arg);
      break;

    case SND_DEV_DSP:
    case SND_DEV_DSP16:
    case SND_DEV_AUDIO:
      return audio_ioctl (dev, file, cmd, arg);
      break;

#ifndef EXCLUDE_MIDI
    case SND_DEV_MIDIN:
      return MIDIbuf_ioctl (dev, file, cmd, arg);
      break;
#endif

#ifndef EXCLUDE_PSS
    case SND_DEV_PSS:
      return pss_ioctl (dev, file, cmd, arg);
      break;
#endif

    default:
      return RET_ERROR (EPERM);
      break;
    }

  return RET_ERROR (EPERM);
}

#endif
