/*
 * linux/kernel/chr_drv/sound/sb_dsp.c
 * 
 * The low level driver for the SoundBlaster DS chips.
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 * 
 * The mixer support is based on the SB-BSD 1.5 driver by (C) Steve Haehnichen
 * <shaehnic@ucsd.edu>
 */

#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SB)

#undef SB_TEST_IRQ

#define DSP_RESET	(sbc_base + 0x6)
#define DSP_READ	(sbc_base + 0xA)
#define DSP_WRITE	(sbc_base + 0xC)
#define DSP_COMMAND	(sbc_base + 0xC)
#define DSP_STATUS	(sbc_base + 0xC)
#define DSP_DATA_AVAIL	(sbc_base + 0xE)
#define MIXER_ADDR	(sbc_base + 0x4)
#define MIXER_DATA	(sbc_base + 0x5)
#define OPL3_LEFT	(sbc_base + 0x0)
#define OPL3_RIGHT	(sbc_base + 0x2)
#define OPL3_BOTH	(sbc_base + 0x8)

static int      sbc_base = 0;
static int      sbc_irq = 0;

#define POSSIBLE_RECORDING_DEVICES	(SOUND_MASK_LINE | SOUND_MASK_MIC | SOUND_MASK_CD)

#define SUPPORTED_MIXER_DEVICES		(SOUND_MASK_SYNTH | SOUND_MASK_PCM | SOUND_MASK_LINE | SOUND_MASK_MIC | \
					 SOUND_MASK_CD | SOUND_MASK_VOLUME)

/*
 * Mixer registers
 * 
 * NOTE!	RECORD_SRC == IN_FILTER
 */

#define VOC_VOL		0x04
#define MIC_VOL		0x0A
#define MIC_MIX		0x0A
#define RECORD_SRC	0x0C
#define IN_FILTER	0x0C
#define OUT_FILTER	0x0E
#define MASTER_VOL	0x22
#define FM_VOL		0x26
#define CD_VOL		0x28
#define LINE_VOL	0x2E

#define FREQ_HI         (1 << 3)/* Use High-frequency ANFI filters */
#define FREQ_LOW        0	/* Use Low-frequency ANFI filters */
#define FILT_ON         0	/* Yes, 0 to turn it on, 1 for off */
#define FILT_OFF        (1 << 5)

/* Convenient byte masks */
#define B1(x)	((x) & 0x01)
#define B2(x)	((x) & 0x03)
#define B3(x)	((x) & 0x07)
#define B4(x)	((x) & 0x0f)
#define B5(x)	((x) & 0x1f)
#define B6(x)	((x) & 0x3f)
#define B7(x)	((x) & 0x7f)
#define B8(x)	((x) & 0xff)
#define F(x)	(!!(x))		/* 0 or 1 only */

#define MONO_DAC	0x00
#define STEREO_DAC	0x02

/* DSP Commands */

#define DSP_CMD_SPKON		0xD1
#define DSP_CMD_SPKOFF		0xD3

/*
 * The DSP channel can be used either for input or output. Variable
 * 'irq_mode' will be set when the program calls read or write first time
 * after open. Current version doesn't support mode changes without closing
 * and reopening the device. Support for this feature may be implemented in a
 * future version of this driver.
 */

#define IMODE_NONE		0
#define IMODE_OUTPUT		1
#define IMODE_INPUT		2
#define IMODE_INIT		3
#define IMODE_MIDI		4

#define NORMAL_MIDI	0
#define UART_MIDI	1

static int      sb_dsp_ok = 0;	/* Set to 1 after successful initialization */
static int      midi_disabled = 0;
static int      dsp_highspeed = 0, dsp_stereo = 0;
static int      dsp_current_speed = DSP_DEFAULT_SPEED;

#ifndef EXCLUDE_SBPRO
static int      rec_devices = SOUND_MASK_MIC;
static int      hi_filter = 0, filter_in = 0, filter_out = 0;

#endif

static int      midi_mode = NORMAL_MIDI;
static int      midi_busy = 0;	/* 1 if the process has output to MIDI */
static int      dsp_busy = 0;

static volatile int irq_mode = IMODE_NONE;	/* IMODE_INPUT, IMODE_OUTPUT
						 * or IMODE_NONE */
static volatile int irq_ok = 0;

static int      dsp_model = 1;	/* 1=SB, 2=SB Pro */
static int      duplex_midi = 0;
static int      my_dev = 0;

static volatile int intr_active = 0;

static int      dsp_speed (int);
static int      dsp_set_stereo (int mode);
static int      dsp_command (unsigned char val);

#ifndef EXCLUDE_SBPRO
static void     setmixer (unsigned char port, unsigned char value);
static int      getmixer (unsigned char port);
static void     init_mixer (void);
static int      detect_mixer (void);

#endif

#if !defined(EXCLUDE_MIDI) || !defined(EXCLUDE_AUDIO)

/* Common code for the midi and pcm functions */

static int
dsp_command (unsigned char val)
{
  int             i, limit;

  limit = GET_TIME () + 10;	/* The timeout is 0.1 secods */

  /*
   * Note! the i<5000000 is an emergency exit. The dsp_command() is sometimes
   * called while interrupts are disabled. This means that the timer is
   * disabled also. However the timeout situation is a abnormal condition.
   * Normally the DSP should be ready to accept commands after just couple of
   * loops.
   */

  for (i = 0; i < 5000000 && GET_TIME () < limit; i++)
    {
      if ((INB (DSP_STATUS) & 0x80) == 0)
	{
	  OUTB (val, DSP_COMMAND);
	  return 1;
	}
    }

  printk ("SoundBlaster: DSP Command(%02x) Timeout.\n", val);
  printk ("IRQ conflict???\n");
  return 0;
}

void
sbintr (int unused)
{
  int             status, data;

  status = INB (DSP_DATA_AVAIL);/* Clear interrupt */

  if (intr_active)
    switch (irq_mode)
      {
      case IMODE_OUTPUT:
	intr_active = 0;
	DMAbuf_outputintr (my_dev);
	break;

      case IMODE_INPUT:
	intr_active = 0;
	DMAbuf_inputintr (my_dev);
	/* A complete buffer has been input. Let's start new one */
	break;

      case IMODE_INIT:
	intr_active = 0;
	irq_ok = 1;
	break;

      case IMODE_MIDI:
	printk ("+");
	data = INB (DSP_READ);
	printk ("%02x", data);

	break;

      default:
	printk ("SoundBlaster: Unexpected interrupt\n");
      }
}

static int
set_dsp_irq (int interrupt_level)
{
  int             retcode;

#ifdef linux
  struct sigaction sa;

  sa.sa_handler = sbintr;

#ifdef SND_SA_INTERRUPT
  sa.sa_flags = SA_INTERRUPT;
#else
  sa.sa_flags = 0;
#endif

  sa.sa_mask = 0;
  sa.sa_restorer = NULL;

  retcode = irqaction (interrupt_level, &sa);

  if (retcode < 0)
    {
      printk ("SoundBlaster: IRQ%d already in use\n", interrupt_level);
    }

#else
  /* #  error Unimplemented for this OS	 */
#endif
  return retcode;
}

static int
reset_dsp (void)
{
  int             loopc;

  OUTB (1, DSP_RESET);
  tenmicrosec ();
  OUTB (0, DSP_RESET);
  tenmicrosec ();
  tenmicrosec ();
  tenmicrosec ();

  for (loopc = 0; loopc < 1000 && !(INB (DSP_DATA_AVAIL) & 0x80); loopc++);	/* Wait for data
										 * available status */

  if (INB (DSP_READ) != 0xAA)
    return 0;			/* Sorry */

  return 1;
}

#endif

#ifndef EXCLUDE_AUDIO

static void
dsp_speaker (char state)
{
  if (state)
    dsp_command (DSP_CMD_SPKON);
  else
    dsp_command (DSP_CMD_SPKOFF);
}

static int
dsp_speed (int speed)
{
  unsigned char   tconst;
  unsigned long   flags;


  if (speed < 4000)
    speed = 4000;

  if (speed > 44100)
    speed = 44100;		/* Invalid speed */

  if (dsp_model == 1 && speed > 22050)
    speed = 22050;
  /* SB Classic doesn't support higher speed */


  if (dsp_stereo && speed > 22050)
    speed = 22050;
  /* Max. stereo speed is 22050 */

  if ((speed > 22050) && midi_busy)
    {
      printk ("SB Warning: High speed DSP not possible simultaneously with MIDI output\n");
      speed = 22050;
    }

  if (dsp_stereo)
    speed <<= 1;

  /* Now the speed should be valid */

  if (speed > 22050)
    {				/* High speed mode */
      tconst = (unsigned char) ((65536 - (256000000 / speed)) >> 8);
      dsp_highspeed = 1;

      DISABLE_INTR (flags);
      if (dsp_command (0x40))
	dsp_command (tconst);
      RESTORE_INTR (flags);

      speed = (256000000 / (65536 - (tconst << 8)));
    }
  else
    {
      dsp_highspeed = 0;
      tconst = (256 - (1000000 / speed)) & 0xff;

      DISABLE_INTR (flags);
      if (dsp_command (0x40))	/* Set time constant */
	dsp_command (tconst);
      RESTORE_INTR (flags);

      speed = 1000000 / (256 - tconst);
    }

  if (dsp_stereo)
    speed >>= 1;

  dsp_current_speed = speed;
  return speed;
}

static int
dsp_set_stereo (int mode)
{
  dsp_stereo = 0;

  if (dsp_model == 1)
    return 0;			/* Sorry no stereo */

  if (mode && midi_busy)
    {
      printk ("SB Warning: Stereo DSP not possible simultaneously with MIDI output\n");
      return 0;
    }

  dsp_stereo = !!mode;

#ifndef EXCLUDE_SBPRO
  setmixer (OUT_FILTER, ((getmixer (OUT_FILTER) & ~STEREO_DAC)
			 | (mode ? STEREO_DAC : MONO_DAC)));
#endif
  dsp_speed (dsp_current_speed);/* Speed must be recalculated if #channels
				 * changes */
  return mode;
}

static void
sb_dsp_output_block (int dev, unsigned long buf, int count, int intrflag)
{
  unsigned long   flags;

  if (!irq_mode)
    dsp_speaker (ON);

  irq_mode = IMODE_OUTPUT;
  DMAbuf_start_dma (dev, buf, count, DMA_MODE_WRITE);

  if (sound_dsp_dmachan[dev] > 3)
    count >>= 1;
  count--;

  if (dsp_highspeed)
    {
      DISABLE_INTR (flags);
      if (dsp_command (0x48))	/* High speed size */
	{
	  dsp_command (count & 0xff);
	  dsp_command ((count >> 8) & 0xff);
	  dsp_command (0x91);	/* High speed 8 bit DAC */
	}
      else
	printk ("SB Error: Unable to start (high speed) DAC\n");
      RESTORE_INTR (flags);
    }
  else
    {
      DISABLE_INTR (flags);
      if (dsp_command (0x14))	/* 8-bit DAC (DMA) */
	{
	  dsp_command (count & 0xff);
	  dsp_command ((count >> 8) & 0xff);
	}
      else
	printk ("SB Error: Unable to start DAC\n");
      RESTORE_INTR (flags);
    }
  intr_active = 1;
}

static void
sb_dsp_start_input (int dev, unsigned long buf, int count, int intrflag)
{
  /* Start a DMA input to the buffer pointed by dmaqtail */

  unsigned long   flags;

  if (!irq_mode)
    dsp_speaker (OFF);

  irq_mode = IMODE_INPUT;
  DMAbuf_start_dma (dev, buf, count, DMA_MODE_READ);

  if (sound_dsp_dmachan[dev] > 3)
    count >>= 1;
  count--;

  if (dsp_highspeed)
    {
      DISABLE_INTR (flags);
      if (dsp_command (0x48))	/* High speed size */
	{
	  dsp_command (count & 0xff);
	  dsp_command ((count >> 8) & 0xff);
	  dsp_command (0x99);	/* High speed 8 bit ADC */
	}
      else
	printk ("SB Error: Unable to start (high speed) ADC\n");
      RESTORE_INTR (flags);
    }
  else
    {
      DISABLE_INTR (flags);
      if (dsp_command (0x24))	/* 8-bit ADC (DMA) */
	{
	  dsp_command (count & 0xff);
	  dsp_command ((count >> 8) & 0xff);
	}
      else
	printk ("SB Error: Unable to start ADC\n");
      RESTORE_INTR (flags);
    }

  intr_active = 1;
}

static void
dsp_cleanup (void)
{
  intr_active = 0;
}

static int
sb_dsp_prepare_for_input (int dev, int bsize, int bcount)
{
  dsp_cleanup ();
  dsp_speaker (OFF);
  return 0;
}

static int
sb_dsp_prepare_for_output (int dev, int bsize, int bcount)
{
  dsp_cleanup ();
  dsp_speaker (ON);
  return 0;
}

static void
sb_dsp_halt_xfer (int dev)
{
}

static int
sb_dsp_open (int dev, int mode)
{
  int             retval;

  if (!sb_dsp_ok)
    {
      printk ("SB Error: SoundBlaster board not installed\n");
      return RET_ERROR (ENXIO);
    }

  if (!irq_ok)
    {
      printk ("SB Error: Incorrect IRQ setting (%d)\n", sbc_irq);
      return RET_ERROR (ENXIO);
    }

  if (intr_active || (midi_busy && midi_mode == UART_MIDI))
    {
      printk ("SB: PCM not possible during MIDI input\n");
      return RET_ERROR (EBUSY);
    }

  if (mode != OPEN_READ && mode != OPEN_WRITE)
    {
      printk ("SoundBlaster error: DAC and ACD not possible simultaneously\n");
      return RET_ERROR (EINVAL);
    }

  retval = set_dsp_irq (sbc_irq);
  if (retval)
    return retval;

  if (!DMAbuf_open_dma (dev))
    {
      RELEASE_IRQ (sbc_irq);
      printk ("SB: DMA Busy\n");
      return RET_ERROR (EBUSY);
    }

  dsp_set_stereo (OFF);
  dsp_speed (DSP_DEFAULT_SPEED);
  irq_mode = IMODE_NONE;

  dsp_busy = 1;

  return 0;
}

static void
sb_dsp_close (int dev)
{
  DMAbuf_close_dma (dev);
  RELEASE_IRQ (sbc_irq);
  dsp_cleanup ();
  dsp_speed (DSP_DEFAULT_SPEED);
  dsp_set_stereo (OFF);
  dsp_speaker (OFF);
  dsp_busy = 0;
}

static int
sb_dsp_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  switch (cmd)
    {
    case SOUND_PCM_WRITE_RATE:
      if (local)
	return dsp_speed (arg);
      return IOCTL_OUT (arg, dsp_speed (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_RATE:
      if (local)
	return dsp_current_speed;
      return IOCTL_OUT (arg, dsp_current_speed);
      break;

    case SOUND_PCM_WRITE_CHANNELS:
      return IOCTL_OUT (arg, dsp_set_stereo (IOCTL_IN (arg) - 1) + 1);
      break;

    case SOUND_PCM_READ_CHANNELS:
      if (local)
	return dsp_stereo + 1;
      return IOCTL_OUT (arg, dsp_stereo + 1);
      break;

    case SNDCTL_DSP_STEREO:
      if (local)
	return dsp_set_stereo (arg);
      return IOCTL_OUT (arg, dsp_set_stereo (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_WRITE_BITS:
    case SOUND_PCM_READ_BITS:
      if (local)
	return 8;
      return IOCTL_OUT (arg, 8);/* Only 8 bits/sample supported */
      break;

    case SOUND_PCM_WRITE_FILTER:
    case SOUND_PCM_READ_FILTER:
      return RET_ERROR (EINVAL);
      break;

    default:
      return RET_ERROR (EINVAL);
    }

  return RET_ERROR (EINVAL);
}

static void
sb_dsp_reset (int dev)
{
  unsigned long   flags;

  DISABLE_INTR (flags);

  reset_dsp ();
  dsp_cleanup ();

  RESTORE_INTR (flags);
}

#endif

int
sb_dsp_detect (struct address_info *hw_config)
{
  sbc_base = hw_config->io_base;
  sbc_irq = hw_config->irq;

  if (sb_dsp_ok)
    return 0;			/* Already initialized */

  if (!reset_dsp ())
    return 0;

  return 1;			/* Detected */
}

#ifndef EXCLUDE_SBPRO

static void
setmixer (unsigned char port, unsigned char value)
{
  OUTB (port, MIXER_ADDR);	/* Select register */
  tenmicrosec ();
  OUTB (value, MIXER_DATA);
  tenmicrosec ();
}

static int
getmixer (unsigned char port)
{
  int             val;

  OUTB (port, MIXER_ADDR);	/* Select register */
  tenmicrosec ();
  val = INB (MIXER_DATA);
  tenmicrosec ();

  return val;
}

static int
detect_mixer (void)
{
  /*
   * Detect the mixer by changing parameters of two volume channels. If the
   * values read back match with the values written, the mixer is there (is
   * it?)
   */
  setmixer (FM_VOL, 0xff);
  setmixer (VOC_VOL, 0x33);

  if (getmixer (FM_VOL) != 0xff)
    return 0;			/* No match */
  if (getmixer (VOC_VOL) != 0x33)
    return 0;

  return 1;
}

static void
init_mixer (void)
{
  setmixer (MASTER_VOL, 0xbb);
  setmixer (VOC_VOL, 0x99);
  setmixer (LINE_VOL, 0xbb);
  setmixer (FM_VOL, 0x99);
  setmixer (CD_VOL, 0x11);
  setmixer (MIC_MIX, 0x11);
  setmixer (RECORD_SRC, 0x31);
  setmixer (OUT_FILTER, 0x31);
}

static void
set_filter (int record_source, int hifreq_filter, int filter_input, int filter_output)
{
  setmixer (RECORD_SRC, (record_source
			 | (hifreq_filter ? FREQ_HI : FREQ_LOW)
			 | (filter_input ? FILT_ON : FILT_OFF)));

  setmixer (OUT_FILTER, ((dsp_stereo ? STEREO_DAC : MONO_DAC)
			 | (filter_output ? FILT_ON : FILT_OFF)));

  hi_filter = hifreq_filter;
  filter_in = filter_input;
  filter_out = filter_output;
}

static int
mixer_output (int right_vol, int left_vol, int div, int device)
{
  int             left = ((left_vol * div) + 50) / 100;
  int             right = ((right_vol * div) + 50) / 100;

  setmixer (device, ((left & 0xf) << 4) | (right & 0xf));

  return (left_vol | (right_vol << 8));
}

static int
sbp_mixer_set (int whichDev, unsigned int level)
{
  int             left, right, devmask;

  left = level & 0x7f;
  right = (level & 0x7f00) >> 8;

  switch (whichDev)
    {
    case SOUND_MIXER_VOLUME:	/* Master volume (0-15) */
      return mixer_output (right, left, 15, MASTER_VOL);
      break;
    case SOUND_MIXER_SYNTH:	/* Internal synthesizer (0-15) */
      return mixer_output (right, left, 15, FM_VOL);
      break;
    case SOUND_MIXER_PCM:	/* PAS PCM (0-15) */
      return mixer_output (right, left, 15, VOC_VOL);
      break;
    case SOUND_MIXER_LINE:	/* External line (0-15) */
      return mixer_output (right, left, 15, LINE_VOL);
      break;
    case SOUND_MIXER_CD:	/* CD (0-15) */
      return mixer_output (right, left, 15, CD_VOL);
      break;
    case SOUND_MIXER_MIC:	/* External microphone (0-7) */
      return mixer_output (right, left, 7, MIC_VOL);
      break;

    case SOUND_MIXER_RECSRC:
      devmask = level & POSSIBLE_RECORDING_DEVICES;

      if (devmask != SOUND_MASK_MIC &&
	  devmask != SOUND_MASK_LINE &&
	  devmask != SOUND_MASK_CD)
	{			/* More than one devices selected. Drop the
				 * previous selection */
	  devmask &= ~rec_devices;
	}

      if (devmask != SOUND_MASK_MIC &&
	  devmask != SOUND_MASK_LINE &&
	  devmask != SOUND_MASK_CD)
	{			/* More than one devices selected. Default to
				 * mic */
	  devmask = SOUND_MASK_MIC;
	}

      if (devmask ^ rec_devices)/* Input source changed */
	{
	  switch (devmask)
	    {

	    case SOUND_MASK_MIC:
	      set_filter (SRC_MIC, hi_filter, filter_in, filter_out);
	      break;

	    case SOUND_MASK_LINE:
	      set_filter (SRC_LINE, hi_filter, filter_in, filter_out);
	      break;

	    case SOUND_MASK_CD:
	      set_filter (SRC_CD, hi_filter, filter_in, filter_out);
	      break;

	    default:
	      set_filter (SRC_MIC, hi_filter, filter_in, filter_out);
	    }
	}

      rec_devices = devmask;

      return rec_devices;
      break;

    default:
      return RET_ERROR (EINVAL);
    }

}

static int
mixer_input (int div, int device)
{
  int             level, left, right, half;

  level = getmixer (device);
  half = div / 2;

  left = ((((level & 0xf0) >> 4) * 100) + half) / div;
  right = (((level & 0x0f) * 100) + half) / div;

  return (right << 8) | left;
}

static int
sbp_mixer_get (int whichDev)
{

  switch (whichDev)
    {
    case SOUND_MIXER_VOLUME:	/* Master volume (0-15) */
      return mixer_input (15, MASTER_VOL);
      break;
    case SOUND_MIXER_SYNTH:	/* Internal synthesizer (0-15) */
      return mixer_input (15, FM_VOL);
      break;
    case SOUND_MIXER_PCM:	/* PAS PCM (0-15) */
      return mixer_input (15, VOC_VOL);
      break;
    case SOUND_MIXER_LINE:	/* External line (0-15) */
      return mixer_input (15, LINE_VOL);
      break;
    case SOUND_MIXER_CD:	/* CD (0-15) */
      return mixer_input (15, CD_VOL);
      break;
    case SOUND_MIXER_MIC:	/* External microphone (0-7) */
      return mixer_input (7, MIC_VOL);
      break;

    default:
      return RET_ERROR (EINVAL);
    }

}

/*
 * Sets mixer volume levels. All levels except mic are 0 to 15, mic is 7. See
 * sbinfo.doc for details on granularity and such. Basically, the mixer
 * forces the lowest bit high, effectively reducing the possible settings by
 * one half.  Yes, that's right, volume levels have 8 settings, and
 * microphone has four.  Sucks.
 */
static int
mixer_set_levels (struct sb_mixer_levels *user_l)
{
  struct sb_mixer_levels l;

  IOCTL_FROM_USER ((char *) &l, ((char *) user_l), 0, sizeof (l));

  if (l.master.l & ~0xF || l.master.r & ~0xF
      || l.line.l & ~0xF || l.line.r & ~0xF
      || l.voc.l & ~0xF || l.voc.r & ~0xF
      || l.fm.l & ~0xF || l.fm.r & ~0xF
      || l.cd.l & ~0xF || l.cd.r & ~0xF
      || l.mic & ~0x7)
    return (RET_ERROR (EINVAL));

  setmixer (MASTER_VOL, (l.master.l << 4) | l.master.r);
  setmixer (LINE_VOL, (l.line.l << 4) | l.line.r);
  setmixer (VOC_VOL, (l.voc.l << 4) | l.voc.r);
  setmixer (FM_VOL, (l.fm.l << 4) | l.fm.r);
  setmixer (CD_VOL, (l.cd.l << 4) | l.cd.r);
  setmixer (MIC_VOL, l.mic);
  return (0);
}

/*
 * This sets aspects of the Mixer that are not volume levels. (Recording
 * source, filter level, I/O filtering, and stereo.)
 */

static int
mixer_set_params (struct sb_mixer_params *user_p)
{
  struct sb_mixer_params p;

  IOCTL_FROM_USER ((char *) &p, (char *) user_p, 0, sizeof (p));

  if (p.record_source != SRC_MIC
      && p.record_source != SRC_CD
      && p.record_source != SRC_LINE)
    return (EINVAL);

  /*
   * I'm not sure if this is The Right Thing.  Should stereo be entirely
   * under control of DSP?  I like being able to toggle it while a sound is
   * playing, so I do this... because I can.
   */

  dsp_stereo = !!p.dsp_stereo;

  set_filter (p.record_source, p.hifreq_filter, p.filter_input, p.filter_output);

  switch (p.record_source)
    {

    case SRC_MIC:
      rec_devices = SOUND_MASK_MIC;
      break;

    case SRC_LINE:
      rec_devices = SOUND_MASK_LINE;
      break;

    case SRC_CD:
      rec_devices = SOUND_MASK_CD;
    }

  return (0);
}

/* Read the current mixer level settings into the user's struct. */
static int
mixer_get_levels (struct sb_mixer_levels *user_l)
{
  S_BYTE          val;
  struct sb_mixer_levels l;

  val = getmixer (MASTER_VOL);	/* Master */
  l.master.l = B4 (val >> 4);
  l.master.r = B4 (val);

  val = getmixer (LINE_VOL);	/* FM */
  l.line.l = B4 (val >> 4);
  l.line.r = B4 (val);

  val = getmixer (VOC_VOL);	/* DAC */
  l.voc.l = B4 (val >> 4);
  l.voc.r = B4 (val);

  val = getmixer (FM_VOL);	/* FM */
  l.fm.l = B4 (val >> 4);
  l.fm.r = B4 (val);

  val = getmixer (CD_VOL);	/* CD */
  l.cd.l = B4 (val >> 4);
  l.cd.r = B4 (val);

  val = getmixer (MIC_VOL);	/* Microphone */
  l.mic = B3 (val);

  IOCTL_TO_USER ((char *) user_l, 0, (char *) &l, sizeof (l));

  return (0);
}

/* Read the current mixer parameters into the user's struct. */
static int
mixer_get_params (struct sb_mixer_params *user_params)
{
  S_BYTE          val;
  struct sb_mixer_params params;

  val = getmixer (RECORD_SRC);
  params.record_source = val & 0x07;
  params.hifreq_filter = !!(val & FREQ_HI);
  params.filter_input = (val & FILT_OFF) ? OFF : ON;
  params.filter_output = (getmixer (OUT_FILTER) & FILT_OFF) ? OFF : ON;
  params.dsp_stereo = dsp_stereo;

  IOCTL_TO_USER ((char *) user_params, 0, (char *) &params, sizeof (params));
  return (0);
}

static int
sb_mixer_ioctl (int dev, unsigned int cmd, unsigned int arg)
{
  if (((cmd >> 8) & 0xff) == 'M')
    {
      if (cmd & IOC_IN)
	return IOCTL_OUT (arg, sbp_mixer_set (cmd & 0xff, IOCTL_IN (arg)));
      else
	{			/* Read parameters */

	  switch (cmd & 0xff)
	    {

	    case SOUND_MIXER_RECSRC:
	      return IOCTL_OUT (arg, rec_devices);
	      break;

	    case SOUND_MIXER_DEVMASK:
	      return IOCTL_OUT (arg, SUPPORTED_MIXER_DEVICES);
	      break;

	    case SOUND_MIXER_STEREODEVS:
	      return IOCTL_OUT (arg, SUPPORTED_MIXER_DEVICES & ~SOUND_MASK_MIC);
	      break;

	    case SOUND_MIXER_RECMASK:
	      return IOCTL_OUT (arg, POSSIBLE_RECORDING_DEVICES);
	      break;

	    case SOUND_MIXER_CAPS:
	      return IOCTL_OUT (arg, SOUND_CAP_EXCL_INPUT);
	      break;

	    default:
	      return IOCTL_OUT (arg, sbp_mixer_get (cmd & 0xff));
	    }
	}
    }
  else
    {
      switch (cmd)
	{
	case MIXER_IOCTL_SET_LEVELS:
	  return (mixer_set_levels ((struct sb_mixer_levels *) arg));
	case MIXER_IOCTL_SET_PARAMS:
	  return (mixer_set_params ((struct sb_mixer_params *) arg));
	case MIXER_IOCTL_READ_LEVELS:
	  return (mixer_get_levels ((struct sb_mixer_levels *) arg));
	case MIXER_IOCTL_READ_PARAMS:
	  return (mixer_get_params ((struct sb_mixer_params *) arg));
	case MIXER_IOCTL_RESET:
	  init_mixer ();
	  return (0);
	default:
	  return RET_ERROR (EINVAL);
	}
    }
}

/* End of mixer code */
#endif

#ifndef EXCLUDE_MIDI

/* Midi code */

static int
sb_midi_open (int dev, int mode)
{
  int             ret;

  if (!sb_dsp_ok)
    {
      printk ("SB Error: MIDI hardware not installed\n");
      return RET_ERROR (ENXIO);
    }

  if (mode != OPEN_WRITE && !duplex_midi)
    {
      printk ("SoundBlaster: Midi input not currently supported\n");
      return RET_ERROR (EPERM);
    }

  midi_mode = NORMAL_MIDI;
  if (mode != OPEN_WRITE)
    {
      if (dsp_busy || intr_active)
	return RET_ERROR (EBUSY);
      midi_mode = UART_MIDI;
    }

  if (dsp_highspeed || dsp_stereo)
    {
      printk ("SB Error: Midi output not possible during stereo or high speed audio\n");
      return RET_ERROR (EBUSY);
    }

  if (midi_mode == UART_MIDI)
    {
      irq_mode = IMODE_MIDI;

      reset_dsp ();
      dsp_speaker (OFF);

      if (!dsp_command (0x35))
	return RET_ERROR (EIO);	/* Enter the UART mode */
      intr_active = 1;

      if ((ret = set_dsp_irq (sbc_irq)) < 0)
	{
	  reset_dsp ();
	  return 0;		/* IRQ not free */
	}
    }

  midi_busy = 1;

  return 0;
}

static void
sb_midi_close (int dev)
{
  if (midi_mode == UART_MIDI)
    {
      reset_dsp ();		/* The only way to kill the UART mode */
      RELEASE_IRQ (sbc_irq);
    }
  intr_active = 0;
  midi_busy = 0;
}

static int
sb_midi_out (int dev, unsigned char midi_byte)
{
  unsigned long   flags;

  midi_busy = 1;		/* Kill all notes after close */

  if (midi_mode == NORMAL_MIDI)
    {
      DISABLE_INTR (flags);
      if (dsp_command (0x38))
	dsp_command (midi_byte);
      else
	printk ("SB Error: Unable to send a MIDI byte\n");
      RESTORE_INTR (flags);
    }
  else
    dsp_command (midi_byte);	/* UART write */

  return 1;
}

static int
sb_midi_start_read (int dev)
{
  if (midi_mode != UART_MIDI)
    {
      printk ("SoundBlaster: MIDI input not implemented.\n");
      return RET_ERROR (EPERM);
    }
  return 0;
}

static int
sb_midi_end_read (int dev)
{
  if (midi_mode == UART_MIDI)
    {
      reset_dsp ();
      intr_active = 0;
    }
  return 0;
}

static int
sb_midi_ioctl (int dev, unsigned cmd, unsigned arg)
{
  return RET_ERROR (EPERM);
}

/* End of midi code */
#endif

#ifndef EXCLUDE_AUDIO
static struct audio_operations sb_dsp_operations =
{
  "SoundBlaster",
  sb_dsp_open,
  sb_dsp_close,
  sb_dsp_output_block,
  sb_dsp_start_input,
  sb_dsp_ioctl,
  sb_dsp_prepare_for_input,
  sb_dsp_prepare_for_output,
  sb_dsp_reset,
  sb_dsp_halt_xfer,
  NULL,				/* has_output_drained */
  NULL				/* copy_from_user */
};

#endif

#ifndef EXCLUDE_SBPRO
static struct mixer_operations sb_mixer_operations =
{
  sb_mixer_ioctl
};

#endif

#ifndef EXCLUDE_MIDI
static struct midi_operations sb_midi_operations =
{
  {"SoundBlaster", 0},
  sb_midi_open,
  sb_midi_close,
  sb_midi_ioctl,
  sb_midi_out,
  sb_midi_start_read,
  sb_midi_end_read,
  NULL,				/* Kick */
  NULL,				/* command */
  NULL				/* buffer_status */
};

#endif

static int
verify_irq (void)
{
#if 0
  unsigned long   loop;

  irq_ok = 0;

  if (set_dsp_irq (sbc_irq) == -1)
    {
      printk ("*** SB Error: Irq %d already in use\n", sbc_irq);
      return 0;
    }


  irq_mode = IMODE_INIT;

  dsp_command (0xf2);		/* This should cause immediate interrupt */

  for (loop = 100000; loop > 0 && !irq_ok; loop--);

  RELEASE_IRQ (sbc_irq);

  if (!irq_ok)
    {
      printk ("SB Warning: IRQ test not passed!");
      irq_ok = 1;
    }
#else
  irq_ok = 1;
#endif
  return irq_ok;
}

long
sb_dsp_init (long mem_start, struct address_info *hw_config)
{
  int             i, major, minor;

  major = minor = 0;
  dsp_command (0xe1);		/* Get version */

  for (i = 1000; i; i--)
    {
      if (inb (DSP_DATA_AVAIL) & 0x80)
	{			/* wait for Data Ready */
	  if (major == 0)
	    major = inb (DSP_READ);
	  else
	    {
	      minor = inb (DSP_READ);
	      break;
	    }
	}
    }

#ifndef EXCLUDE_SBPRO
  if (detect_mixer ())
    {
      sprintf (sb_dsp_operations.name, "SoundBlaster Pro %d.%d", major, minor);
      init_mixer ();
#if SBC_DMA < 4
      /* This is a kludge for SB16 cards */
      if (major == 3)
	dsp_model = 2;		/* Do not enable if SB16 */
#endif
      mixer_devs[num_mixers++] = &sb_mixer_operations;

      if (major == 2 || major == 3)
	duplex_midi = 1;

#ifndef EXCLUDE_YM8312
      if (major > 3 || (major == 3 && minor > 0))	/* SB Pro2 or later */
	{
	  enable_opl3_mode (OPL3_LEFT, OPL3_RIGHT, OPL3_BOTH);
	}
#endif
    }
  else
#endif
    sprintf (sb_dsp_operations.name, "SoundBlaster %d.%d", major, minor);

  printk ("snd2: <%s>", sb_dsp_operations.name);

  if (!verify_irq ())
    return mem_start;

#ifndef EXCLUDE_AUDIO
  if (num_dspdevs < MAX_DSP_DEV)
    {
      dsp_devs[my_dev = num_dspdevs++] = &sb_dsp_operations;
      sound_buffcounts[my_dev] = DSP_BUFFCOUNT;
      sound_buffsizes[my_dev] = DSP_BUFFSIZE;
      sound_dsp_dmachan[my_dev] = hw_config->dma;
      sound_dma_automode[my_dev] = 0;
    }
  else
    printk ("SB: Too many DSP devices available\n");
#endif

#ifndef EXCLUDE_MIDI
  if (!midi_disabled)		/* Midi don't work in the SB emulation mode
				 * of PAS */
    midi_devs[num_midis++] = &sb_midi_operations;
#endif

  sb_dsp_ok = 1;
  printk("\n");
  return mem_start;
}

void
sb_dsp_disable_midi (void)
{
  midi_disabled = 1;
}

#endif
