
/*
 * sound/sb_mixer.c
 * 
 * The low level mixer driver for the SoundBlaster Pro and SB16 cards.
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

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SB) && !defined(EXCLUDE_SBPRO)
#define __SB_MIXER_C__

#include "sb.h"
#include "sb_mixer.h"
#undef SB_TEST_IRQ

extern int      sbc_base;

static int      mixer_initialized = 0;

static int      supported_rec_devices;
static int      supported_devices;
static int      recmask = 0;
static int      mixer_model;
static int      mixer_caps;
static mixer_tab *iomap;

void
sb_setmixer (unsigned int port, unsigned int value)
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  OUTB ((unsigned char) (port & 0xff), MIXER_ADDR);	/* Select register */
  tenmicrosec ();
  OUTB ((unsigned char) (value & 0xff), MIXER_DATA);
  tenmicrosec ();
  RESTORE_INTR (flags);
}

int
sb_getmixer (unsigned int port)
{
  int             val;
  unsigned long   flags;

  DISABLE_INTR (flags);
  OUTB ((unsigned char) (port & 0xff), MIXER_ADDR);	/* Select register */
  tenmicrosec ();
  val = INB (MIXER_DATA);
  tenmicrosec ();
  RESTORE_INTR (flags);

  return val;
}

void
sb_mixer_set_stereo (int mode)
{
  if (!mixer_initialized)
    return;

  sb_setmixer (OUT_FILTER, ((sb_getmixer (OUT_FILTER) & ~STEREO_DAC)
			    | (mode ? STEREO_DAC : MONO_DAC)));
}

static int
detect_mixer (void)
{
  /*
   * Detect the mixer by changing parameters of two volume channels. If the
   * values read back match with the values written, the mixer is there (is
   * it?)
   */
  sb_setmixer (FM_VOL, 0xff);
  sb_setmixer (VOC_VOL, 0x33);

  if (sb_getmixer (FM_VOL) != 0xff)
    return 0;			/* No match */
  if (sb_getmixer (VOC_VOL) != 0x33)
    return 0;

  return 1;
}

static void
change_bits (unsigned char *regval, int dev, int chn, int newval)
{
  unsigned char   mask;
  int             shift;

  mask = (1 << (*iomap)[dev][chn].nbits) - 1;
  newval = ((newval * mask) + 50) / 100;	/* Scale it */

  shift = (*iomap)[dev][chn].bitoffs - (*iomap)[dev][LEFT_CHN].nbits + 1;

  *regval &= ~(mask << shift);	/* Filter out the previous value */
  *regval |= (newval & mask) << shift;	/* Set the new value */
}

static int
sb_mixer_get (int dev)
{
  if (!((1 << dev) & supported_devices))
    return RET_ERROR (EINVAL);

  return levels[dev];
}

static int
sb_mixer_set (int dev, int value)
{
  int             left = value & 0x000000ff;
  int             right = (value & 0x0000ff00) >> 8;

  int             regoffs;
  unsigned char   val;

  if (left > 100)
    left = 100;
  if (right > 100)
    right = 100;

  if (dev > 31)
    return RET_ERROR (EINVAL);

  if (!(supported_devices & (1 << dev)))	/* Not supported */
    return RET_ERROR (EINVAL);

  regoffs = (*iomap)[dev][LEFT_CHN].regno;

  if (regoffs == 0)
    return RET_ERROR (EINVAL);

  val = sb_getmixer (regoffs);
  change_bits (&val, dev, LEFT_CHN, left);

  levels[dev] = left | (left << 8);

  if ((*iomap)[dev][RIGHT_CHN].regno != regoffs)	/* Change register */
    {
      sb_setmixer (regoffs, val);	/* Save the old one */
      regoffs = (*iomap)[dev][RIGHT_CHN].regno;

      if (regoffs == 0)
	return left | (left << 8);	/* Just left channel present */

      val = sb_getmixer (regoffs);	/* Read the new one */
    }

  change_bits (&val, dev, RIGHT_CHN, right);
  sb_setmixer (regoffs, val);

  levels[dev] = left | (right << 8);
  return left | (right << 8);
}

static void
set_recsrc (int src)
{
  sb_setmixer (RECORD_SRC, (sb_getmixer (RECORD_SRC) & ~7) | (src & 0x7));
}

static int
set_recmask (int mask)
{
  int             devmask, i;
  unsigned char   regimageL, regimageR;

  devmask = mask & supported_rec_devices;

  switch (mixer_model)
    {
    case 3:

      if (devmask != SOUND_MASK_MIC &&
	  devmask != SOUND_MASK_LINE &&
	  devmask != SOUND_MASK_CD)
	{			/* More than one devices selected. Drop the
				 * previous selection */
	  devmask &= ~recmask;
	}

      if (devmask != SOUND_MASK_MIC &&
	  devmask != SOUND_MASK_LINE &&
	  devmask != SOUND_MASK_CD)
	{			/* More than one devices selected. Default to
				 * mic */
	  devmask = SOUND_MASK_MIC;
	}


      if (devmask ^ recmask)	/* Input source changed */
	{
	  switch (devmask)
	    {

	    case SOUND_MASK_MIC:
	      set_recsrc (SRC_MIC);
	      break;

	    case SOUND_MASK_LINE:
	      set_recsrc (SRC_LINE);
	      break;

	    case SOUND_MASK_CD:
	      set_recsrc (SRC_CD);
	      break;

	    default:
	      set_recsrc (SRC_MIC);
	    }
	}

      break;

    case 4:
      if (!devmask)
	devmask = SOUND_MASK_MIC;

      regimageL = regimageR = 0;
      for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
	if ((1 << i) & devmask)
	  {
	    regimageL |= sb16_recmasks_L[i];
	    regimageR |= sb16_recmasks_R[i];
	  }
      sb_setmixer (SB16_IMASK_L, regimageL);
      sb_setmixer (SB16_IMASK_R, regimageR);
      break;
    }

  recmask = devmask;
  return recmask;
}

static int
sb_mixer_ioctl (int dev, unsigned int cmd, unsigned int arg)
{
  if (((cmd >> 8) & 0xff) == 'M')
    {
      if (cmd & IOC_IN)
	switch (cmd & 0xff)
	  {
	  case SOUND_MIXER_RECSRC:
	    return IOCTL_OUT (arg, set_recmask (IOCTL_IN (arg)));
	    break;

	  default:
	    return IOCTL_OUT (arg, sb_mixer_set (cmd & 0xff, IOCTL_IN (arg)));
	  }
      else
	switch (cmd & 0xff)	/* Return parameters */
	  {

	  case SOUND_MIXER_RECSRC:
	    return IOCTL_OUT (arg, recmask);
	    break;

	  case SOUND_MIXER_DEVMASK:
	    return IOCTL_OUT (arg, supported_devices);
	    break;

	  case SOUND_MIXER_STEREODEVS:
	    return IOCTL_OUT (arg, supported_devices &
			      ~(SOUND_MASK_MIC | SOUND_MASK_SPEAKER));
	    break;

	  case SOUND_MIXER_RECMASK:
	    return IOCTL_OUT (arg, supported_rec_devices);
	    break;

	  case SOUND_MIXER_CAPS:
	    return IOCTL_OUT (arg, mixer_caps);
	    break;

	  default:
	    return IOCTL_OUT (arg, sb_mixer_get (cmd & 0xff));
	  }
    }
  else
    return RET_ERROR (EINVAL);
}

static struct mixer_operations sb_mixer_operations =
{
  sb_mixer_ioctl
};

static void
sb_mixer_reset (void)
{
  int             i;

  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
    sb_mixer_set (i, levels[i]);
  set_recmask (SOUND_MASK_MIC);
}

void
sb_mixer_init (int major_model)
{
  sb_setmixer (0x00, 0);	/* Reset mixer */

  if (!detect_mixer ())
    return;			/* No mixer. Why? */

  mixer_initialized = 1;
  mixer_model = major_model;

  switch (major_model)
    {
    case 3:
      mixer_caps = SOUND_CAP_EXCL_INPUT;
      supported_devices = SBPRO_MIXER_DEVICES;
      supported_rec_devices = SBPRO_RECORDING_DEVICES;
      iomap = &sbpro_mix;
      break;

    case 4:
      mixer_caps = 0;
      supported_devices = SB16_MIXER_DEVICES;
      supported_rec_devices = SB16_RECORDING_DEVICES;
      iomap = &sb16_mix;
      break;

    default:
      printk ("SB Warning: Unsupported mixer type\n");
      return;
    }

  mixer_devs[num_mixers++] = &sb_mixer_operations;
  sb_mixer_reset ();
}

#endif
