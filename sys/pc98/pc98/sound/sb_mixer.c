
/*
 * sound/sb_mixer.c
 *
 * The low level mixer driver for the SoundBlaster Pro and SB16 cards.
 *
 * Copyright by Hannu Savolainen 1994
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
 * Modified:
 *      Hunyue Yau      Jan 6 1994
 *      Added code to support the Sound Galaxy NX Pro mixer.
 *
 */

#ifdef PC98
#include <pc98/pc98/sound/sound_config.h>
#else
#include <i386/isa/sound/sound_config.h>
#endif

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SB) && !defined(EXCLUDE_SBPRO)
#define __SB_MIXER_C__

#include <i386/isa/sound/sb.h>
#include <i386/isa/sound/sb_mixer.h>
#undef SB_TEST_IRQ

extern int      sbc_base;
extern int      sbc_major;
extern int      Jazz16_detected;

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
  OUTB ((unsigned char) (port & 0xff), MIXER_ADDR);	/*
							 * Select register
							 */
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
  OUTB ((unsigned char) (port & 0xff), MIXER_ADDR);	/*
							 * Select register
							 */
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

/*
 * Returns:
 *      0       No mixer detected.
 *      1       Only a plain Sound Blaster Pro style mixer detected.
 *      2       The Sound Galaxy NX Pro mixer detected.
 */
static int
detect_mixer (void)
{
#ifdef __SGNXPRO__
  int             oldbass, oldtreble;

#endif
  int             retcode = 1;

  /*
   * Detect the mixer by changing parameters of two volume channels. If the
   * values read back match with the values written, the mixer is there (is
   * it?)
   */
  sb_setmixer (FM_VOL, 0xff);
  sb_setmixer (VOC_VOL, 0x33);

  if (sb_getmixer (FM_VOL) != 0xff)
    return 0;			/*
				 * No match
				 */
  if (sb_getmixer (VOC_VOL) != 0x33)
    return 0;

#ifdef __SGNXPRO__
  /* Attempt to detect the SG NX Pro by check for valid bass/treble
     * registers.
   */
  oldbass = sb_getmixer (BASS_LVL);
  oldtreble = sb_getmixer (TREBLE_LVL);

  sb_setmixer (BASS_LVL, 0xaa);
  sb_setmixer (TREBLE_LVL, 0x55);

  if ((sb_getmixer (BASS_LVL) != 0xaa) ||
      (sb_getmixer (TREBLE_LVL) != 0x55))
    {
      retcode = 1;		/* 1 == Only SB Pro detected */
    }
  else
    retcode = 2;		/* 2 == SG NX Pro detected */
  /* Restore register in either case since SG NX Pro has EEPROM with
   * 'preferred' values stored.
   */
  sb_setmixer (BASS_LVL, oldbass);
  sb_setmixer (TREBLE_LVL, oldtreble);

  /*
     * If the SB version is 3.X (SB Pro), assume we have a SG NX Pro 16.
     * In this case it's good idea to disable the Disney Sound Source
     * compatibility mode. It's useless and just causes noise every time the
     * LPT-port is accessed.
     *
     * Also place the card into WSS mode.
   */
  if (sbc_major == 3)
    {
      OUTB (0x01, sbc_base + 0x1c);
      OUTB (0x00, sbc_base + 0x1a);
    }

#endif
  return retcode;
}

static void
change_bits (unsigned char *regval, int dev, int chn, int newval)
{
  unsigned char   mask;
  int             shift;

  mask = (1 << (*iomap)[dev][chn].nbits) - 1;
  newval = (int) ((newval * mask) + 50) / 100;	/*
						 * Scale it
						 */

  shift = (*iomap)[dev][chn].bitoffs - (*iomap)[dev][LEFT_CHN].nbits + 1;

  *regval &= ~(mask << shift);	/*
				 * Filter out the previous value
				 */
  *regval |= (newval & mask) << shift;	/*
					 * Set the new value
					 */
}

static int
sb_mixer_get (int dev)
{
  if (!((1 << dev) & supported_devices))
    return RET_ERROR (EINVAL);

  return levels[dev];
}

#ifdef JAZZ16
static char     smw_mix_regs[] =	/* Left mixer registers */
{
  0x0b,				/* SOUND_MIXER_VOLUME */
  0x0d,				/* SOUND_MIXER_BASS */
  0x0d,				/* SOUND_MIXER_TREBLE */
  0x05,				/* SOUND_MIXER_SYNTH */
  0x09,				/* SOUND_MIXER_PCM */
  0x00,				/* SOUND_MIXER_SPEAKER */
  0x03,				/* SOUND_MIXER_LINE */
  0x01,				/* SOUND_MIXER_MIC */
  0x07,				/* SOUND_MIXER_CD */
  0x00,				/* SOUND_MIXER_IMIX */
  0x00,				/* SOUND_MIXER_ALTPCM */
  0x00,				/* SOUND_MIXER_RECLEV */
  0x00,				/* SOUND_MIXER_IGAIN */
  0x00,				/* SOUND_MIXER_OGAIN */
  0x00,				/* SOUND_MIXER_LINE1 */
  0x00,				/* SOUND_MIXER_LINE2 */
  0x00				/* SOUND_MIXER_LINE3 */
};

static void
smw_mixer_init (void)
{
  int             i;

  sb_setmixer (0x00, 0x18);	/* Mute unused (Telephone) line */
  sb_setmixer (0x10, 0x38);	/* Config register 2 */

  supported_devices = 0;
  for (i = 0; i < sizeof (smw_mix_regs); i++)
    if (smw_mix_regs[i] != 0)
      supported_devices |= (1 << i);

  supported_rec_devices = supported_devices &
    ~(SOUND_MASK_BASS | SOUND_MASK_TREBLE | SOUND_MASK_PCM |
      SOUND_MASK_VOLUME);
}

static int
smw_mixer_set (int dev, int value)
{
  int             left = value & 0x000000ff;
  int             right = (value & 0x0000ff00) >> 8;
  int             reg, val;

  if (left > 100)
    left = 100;
  if (right > 100)
    right = 100;

  if (dev > 31)
    return RET_ERROR (EINVAL);

  if (!(supported_devices & (1 << dev)))	/* Not supported */
    return RET_ERROR (EINVAL);

  switch (dev)
    {
    case SOUND_MIXER_VOLUME:
      sb_setmixer (0x0b, 96 - (96 * left / 100));	/* 96=mute, 0=max */
      sb_setmixer (0x0c, 96 - (96 * right / 100));
      break;

    case SOUND_MIXER_BASS:
    case SOUND_MIXER_TREBLE:
      levels[dev] = left | (right << 8);

      /* Set left bass and treble values */
      val = ((levels[SOUND_MIXER_TREBLE] & 0xff) * 16 / 100) << 4;
      val |= ((levels[SOUND_MIXER_BASS] & 0xff) * 16 / 100) & 0x0f;
      sb_setmixer (0x0d, val);

      /* Set right bass and treble values */
      val = (((levels[SOUND_MIXER_TREBLE] >> 8) & 0xff) * 16 / 100) << 4;
      val |= (((levels[SOUND_MIXER_BASS] >> 8) & 0xff) * 16 / 100) & 0x0f;
      sb_setmixer (0x0e, val);
      break;

    default:
      reg = smw_mix_regs[dev];
      if (reg == 0)
	return RET_ERROR (EINVAL);
      sb_setmixer (reg, (24 - (24 * left / 100)) | 0x20);	/* 24=mute, 0=max */
      sb_setmixer (reg + 1, (24 - (24 * right / 100)) | 0x40);
    }

  levels[dev] = left | (right << 8);
  return left | (right << 8);
}

#endif

static int
sb_mixer_set (int dev, int value)
{
  int             left = value & 0x000000ff;
  int             right = (value & 0x0000ff00) >> 8;

  int             regoffs;
  unsigned char   val;

#ifdef JAZZ16
  if (Jazz16_detected == 2)
    return smw_mixer_set (dev, value);
#endif

  if (left > 100)
    left = 100;
  if (right > 100)
    right = 100;

  if (dev > 31)
    return RET_ERROR (EINVAL);

  if (!(supported_devices & (1 << dev)))	/*
						 * Not supported
						 */
    return RET_ERROR (EINVAL);

  regoffs = (*iomap)[dev][LEFT_CHN].regno;

  if (regoffs == 0)
    return RET_ERROR (EINVAL);

  val = sb_getmixer (regoffs);
  change_bits (&val, dev, LEFT_CHN, left);

  levels[dev] = left | (left << 8);

  if ((*iomap)[dev][RIGHT_CHN].regno != regoffs)	/*
							 * Change register
							 */
    {
      sb_setmixer (regoffs, val);	/*
					 * Save the old one
					 */
      regoffs = (*iomap)[dev][RIGHT_CHN].regno;

      if (regoffs == 0)
	return left | (left << 8);	/*
					 * Just left channel present
					 */

      val = sb_getmixer (regoffs);	/*
					 * Read the new one
					 */
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
	{			/*
				 * More than one devices selected. Drop the *
				 * previous selection
				 */
	  devmask &= ~recmask;
	}

      if (devmask != SOUND_MASK_MIC &&
	  devmask != SOUND_MASK_LINE &&
	  devmask != SOUND_MASK_CD)
	{			/*
				 * More than one devices selected. Default to
				 * * mic
				 */
	  devmask = SOUND_MASK_MIC;
	}


      if (devmask ^ recmask)	/*
				 * Input source changed
				 */
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
	switch (cmd & 0xff)	/*
				 * Return parameters
				 */
	  {

	  case SOUND_MIXER_RECSRC:
	    return IOCTL_OUT (arg, recmask);
	    break;

	  case SOUND_MIXER_DEVMASK:
	    return IOCTL_OUT (arg, supported_devices);
	    break;

	  case SOUND_MIXER_STEREODEVS:
	    if (Jazz16_detected)
	      return IOCTL_OUT (arg, supported_devices);
	    else
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
  "SoundBlaster",
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

/*
 * Returns a code depending on whether a SG NX Pro was detected.
 * 1 == Plain SB Pro
 * 2 == SG NX Pro detected.
 * 3 == SB16
 *
 * Used to update message.
 */
int
sb_mixer_init (int major_model)
{
  int             mixer_type = 0;

  sb_setmixer (0x00, 0);	/* Reset mixer */

  if (!(mixer_type = detect_mixer ()))
    return 0;			/* No mixer. Why? */

  mixer_initialized = 1;
  mixer_model = major_model;

  switch (major_model)
    {
    case 3:
      mixer_caps = SOUND_CAP_EXCL_INPUT;

#ifdef JAZZ16
      if (Jazz16_detected == 2)	/* SM Wave */
	{
	  supported_devices = 0;
	  supported_rec_devices = 0;
	  iomap = &sbpro_mix;
	  smw_mixer_init ();
	  mixer_type = 1;
	}
      else
#endif
#ifdef __SGNXPRO__
      if (mixer_type == 2)	/* A SGNXPRO was detected */
	{
	  supported_devices = SGNXPRO_MIXER_DEVICES;
	  supported_rec_devices = SGNXPRO_RECORDING_DEVICES;
	  iomap = &sgnxpro_mix;
	}
      else
#endif
	{
	  supported_devices = SBPRO_MIXER_DEVICES;
	  supported_rec_devices = SBPRO_RECORDING_DEVICES;
	  iomap = &sbpro_mix;
	  mixer_type = 1;
	}
      break;

    case 4:
      mixer_caps = 0;
      supported_devices = SB16_MIXER_DEVICES;
      supported_rec_devices = SB16_RECORDING_DEVICES;
      iomap = &sb16_mix;
      mixer_type = 3;
      break;

    default:
      printk ("SB Warning: Unsupported mixer type\n");
      return 0;
    }

  if (num_mixers < MAX_MIXER_DEV)
    mixer_devs[num_mixers++] = &sb_mixer_operations;
  sb_mixer_reset ();
  return mixer_type;
}

#endif
