#define _PAS2_PCM_C_
/*
 * linux/kernel/chr_drv/sound/pas2_pcm.c
 * 
 * The low level driver for the Pro Audio Spectrum ADC/DAC.
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) Craig Metz
 * (cmetz@thor.tjhsst.edu) See COPYING for further details. Should be
 * distributed with this file.
 */

#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#include "pas.h"

#if !defined(EXCLUDE_PAS) && !defined(EXCLUDE_AUDIO)

#define TRACE(WHAT)		/* (WHAT) */

#define PAS_PCM_INTRBITS (0x08)
/* Sample buffer timer interrupt enable */

#define PCM_NON	0
#define PCM_DAC	1
#define PCM_ADC	2

static unsigned long pcm_speed = 0;	/* sampling rate */
static unsigned char pcm_channels = 1;	/* channels/sample (1 or 2) */
static unsigned char pcm_bits = 8;	/* bits/sample (8 or 16) */
static unsigned char pcm_filter = 0;	/* filter FLAG */
static unsigned char pcm_mode = PCM_NON;
static unsigned long pcm_count = 0;
static unsigned short pcm_bitsok = 8;	/* mask of OK bits */
static int      my_devnum = 0;

int
pcm_set_speed (int arg)
{
  int             foo, tmp;
  unsigned long   flags;

  if (arg > 44100)
    arg = 44100;
  if (arg < 5000)
    arg = 5000;

  foo = 1193180 / arg;
  arg = 1193180 / foo;

  if (pcm_channels & 2)
    foo = foo >> 1;

  pcm_speed = arg;

  tmp = pas_read (FILTER_FREQUENCY);

  DISABLE_INTR (flags);

  pas_write (tmp & ~(F_F_PCM_RATE_COUNTER | F_F_PCM_BUFFER_COUNTER), FILTER_FREQUENCY);
  pas_write (S_C_C_SAMPLE_RATE | S_C_C_LSB_THEN_MSB | S_C_C_SQUARE_WAVE, SAMPLE_COUNTER_CONTROL);
  pas_write (foo & 0xff, SAMPLE_RATE_TIMER);
  pas_write ((foo >> 8) & 0xff, SAMPLE_RATE_TIMER);
  pas_write (tmp, FILTER_FREQUENCY);

  RESTORE_INTR (flags);

  return pcm_speed;
}

int
pcm_set_channels (int arg)
{

  if ((arg != 1) && (arg != 2))
    return pcm_channels;

  if (arg != pcm_channels)
    {
      pas_write (pas_read (PCM_CONTROL) ^ P_C_PCM_MONO, PCM_CONTROL);

      pcm_channels = arg;
      pcm_set_speed (pcm_speed);/* The speed must be reinitialized */
    }

  return pcm_channels;
}

int
pcm_set_bits (int arg)
{
  if ((arg & pcm_bitsok) != arg)
    return pcm_bits;

  if (arg != pcm_bits)
    {
      pas_write (pas_read (SYSTEM_CONFIGURATION_2) ^ S_C_2_PCM_16_BIT, SYSTEM_CONFIGURATION_2);

      pcm_bits = arg;
    }

  return pcm_bits;
}

static int
pas_pcm_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  TRACE (printk ("pas2_pcm.c: static int pas_pcm_ioctl(unsigned int cmd = %X, unsigned int arg = %X)\n", cmd, arg));

  switch (cmd)
    {
    case SOUND_PCM_WRITE_RATE:
      if (local)
	return pcm_set_speed (arg);
      return IOCTL_OUT (arg, pcm_set_speed (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_RATE:
      if (local)
	return pcm_speed;
      return IOCTL_OUT (arg, pcm_speed);
      break;

    case SNDCTL_DSP_STEREO:
      if (local)
	return pcm_set_channels (arg + 1) - 1;
      return IOCTL_OUT (arg, pcm_set_channels (IOCTL_IN (arg) + 1) - 1);
      break;

    case SOUND_PCM_WRITE_CHANNELS:
      return IOCTL_OUT (arg, pcm_set_channels (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_CHANNELS:
      if (local)
	return pcm_channels;
      return IOCTL_OUT (arg, pcm_channels);
      break;

    case SNDCTL_DSP_SAMPLESIZE:
      if (local)
	return pcm_set_bits (arg);
      return IOCTL_OUT (arg, pcm_set_bits (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_BITS:
      if (local)
	return pcm_bits;
      return IOCTL_OUT (arg, pcm_bits);

    case SOUND_PCM_WRITE_FILTER:	/* NOT YET IMPLEMENTED */
      if (IOCTL_IN (arg) > 1)
	return IOCTL_OUT (arg, RET_ERROR (EINVAL));
      break;

      pcm_filter = IOCTL_IN (arg);
    case SOUND_PCM_READ_FILTER:
      return IOCTL_OUT (arg, pcm_filter);
      break;

    default:
      return RET_ERROR (EINVAL);
    }

  return RET_ERROR (EINVAL);
}

static void
pas_pcm_reset (int dev)
{
  TRACE (printk ("pas2_pcm.c: static void pas_pcm_reset(void)\n"));

  pas_write (pas_read (PCM_CONTROL) & ~P_C_PCM_ENABLE, PCM_CONTROL);
}

static int
pas_pcm_open (int dev, int mode)
{
  int             err;

  TRACE (printk ("pas2_pcm.c: static int pas_pcm_open(int mode = %X)\n", mode));

  if (mode != OPEN_READ && mode != OPEN_WRITE)
    {
      printk ("PAS2: Attempt to open PCM device for simultaneous read and write");
      return RET_ERROR (EINVAL);
    }

  if ((err = pas_set_intr (PAS_PCM_INTRBITS)) < 0)
    return err;

  if (!DMAbuf_open_dma (dev))
    {
      pas_remove_intr (PAS_PCM_INTRBITS);
      return RET_ERROR (EBUSY);
    }

  pcm_count = 0;

  pcm_set_bits (8);
  pcm_set_channels (1);
  pcm_set_speed (DSP_DEFAULT_SPEED);

  return 0;
}

static void
pas_pcm_close (int dev)
{
  unsigned long   flags;

  TRACE (printk ("pas2_pcm.c: static void pas_pcm_close(void)\n"));

  DISABLE_INTR (flags);

  pas_pcm_reset (dev);
  DMAbuf_close_dma (dev);
  pas_remove_intr (PAS_PCM_INTRBITS);
  pcm_mode = PCM_NON;

  RESTORE_INTR (flags);
}

static void
pas_pcm_output_block (int dev, unsigned long buf, int count, int intrflag)
{
  unsigned long   flags, cnt;

  TRACE (printk ("pas2_pcm.c: static void pas_pcm_output_block(char *buf = %P, int count = %X)\n", buf, count));

  cnt = count;
  if (sound_dsp_dmachan[dev] > 3)
    cnt >>= 1;
  cnt--;

  if (sound_dma_automode[dev] &&
      intrflag &&
      cnt == pcm_count)
    return;			/* Auto mode on. No need to react */

  DISABLE_INTR (flags);

  pas_write (pas_read (PCM_CONTROL) & ~P_C_PCM_ENABLE,
	     PCM_CONTROL);

  DMAbuf_start_dma (dev, buf, count, DMA_MODE_WRITE);

  if (sound_dsp_dmachan[dev] > 3)
    count >>= 1;
  count--;

  if (count != pcm_count)
    {
      pas_write (pas_read (FILTER_FREQUENCY) & ~F_F_PCM_BUFFER_COUNTER, FILTER_FREQUENCY);
      pas_write (S_C_C_SAMPLE_BUFFER | S_C_C_LSB_THEN_MSB | S_C_C_SQUARE_WAVE, SAMPLE_COUNTER_CONTROL);
      pas_write (count & 0xff, SAMPLE_BUFFER_COUNTER);
      pas_write ((count >> 8) & 0xff, SAMPLE_BUFFER_COUNTER);
      pas_write (pas_read (FILTER_FREQUENCY) | F_F_PCM_BUFFER_COUNTER, FILTER_FREQUENCY);

      pcm_count = count;
    }
  pas_write (pas_read (FILTER_FREQUENCY) | F_F_PCM_BUFFER_COUNTER | F_F_PCM_RATE_COUNTER, FILTER_FREQUENCY);
  pas_write (pas_read (PCM_CONTROL) | P_C_PCM_ENABLE | P_C_PCM_DAC_MODE, PCM_CONTROL);

  pcm_mode = PCM_DAC;

  RESTORE_INTR (flags);
}

static void
pas_pcm_start_input (int dev, unsigned long buf, int count, int intrflag)
{
  unsigned long   flags;
  int             cnt;

  TRACE (printk ("pas2_pcm.c: static void pas_pcm_start_input(char *buf = %P, int count = %X)\n", buf, count));

  cnt = count;
  if (sound_dsp_dmachan[dev] > 3)
    cnt >>= 1;
  cnt--;

  if (sound_dma_automode[my_devnum] &&
      intrflag &&
      cnt == pcm_count)
    return;			/* Auto mode on. No need to react */

  DISABLE_INTR (flags);

  DMAbuf_start_dma (dev, buf, count, DMA_MODE_READ);

  if (sound_dsp_dmachan[dev] > 3)
    count >>= 1;

  count--;

  if (count != pcm_count)
    {
      pas_write (pas_read (FILTER_FREQUENCY) & ~F_F_PCM_BUFFER_COUNTER, FILTER_FREQUENCY);
      pas_write (S_C_C_SAMPLE_BUFFER | S_C_C_LSB_THEN_MSB | S_C_C_SQUARE_WAVE, SAMPLE_COUNTER_CONTROL);
      pas_write (count & 0xff, SAMPLE_BUFFER_COUNTER);
      pas_write ((count >> 8) & 0xff, SAMPLE_BUFFER_COUNTER);
      pas_write (pas_read (FILTER_FREQUENCY) | F_F_PCM_BUFFER_COUNTER, FILTER_FREQUENCY);

      pcm_count = count;
    }
  pas_write (pas_read (FILTER_FREQUENCY) | F_F_PCM_BUFFER_COUNTER | F_F_PCM_RATE_COUNTER, FILTER_FREQUENCY);
  pas_write ((pas_read (PCM_CONTROL) | P_C_PCM_ENABLE) & ~P_C_PCM_DAC_MODE, PCM_CONTROL);

  pcm_mode = PCM_ADC;

  RESTORE_INTR (flags);
}

static int
pas_pcm_prepare_for_input (int dev, int bsize, int bcount)
{
  return 0;
}
static int
pas_pcm_prepare_for_output (int dev, int bsize, int bcount)
{
  return 0;
}

static struct audio_operations pas_pcm_operations =
{
  "Pro Audio Spectrum",
  pas_pcm_open,			/* */
  pas_pcm_close,		/* */
  pas_pcm_output_block,		/* */
  pas_pcm_start_input,		/* */
  pas_pcm_ioctl,		/* */
  pas_pcm_prepare_for_input,	/* */
  pas_pcm_prepare_for_output,	/* */
  pas_pcm_reset,		/* */
  pas_pcm_reset,		/* halt_xfer */
  NULL,				/* has_output_drained */
  NULL				/* copy_from_user */
};

long
pas_pcm_init (long mem_start, struct address_info *hw_config)
{
  TRACE (printk ("pas2_pcm.c: long pas_pcm_init(long mem_start = %X)\n", mem_start));

  pcm_bitsok = 8;
  if (pas_read (OPERATION_MODE_1) & O_M_1_PCM_TYPE)
    pcm_bitsok |= 16;

  pcm_set_speed (DSP_DEFAULT_SPEED);

  if (num_dspdevs < MAX_DSP_DEV)
    {
      dsp_devs[my_devnum = num_dspdevs++] = &pas_pcm_operations;
      sound_dsp_dmachan[my_devnum] = hw_config->dma;
      if (hw_config->dma > 3)
	{
	  sound_buffcounts[my_devnum] = 1;
	  sound_buffsizes[my_devnum] = 2 * 65536;
	  sound_dma_automode[my_devnum] = 1;
	}
      else
	{
	  sound_buffcounts[my_devnum] = 1;
	  sound_buffsizes[my_devnum] = DSP_BUFFSIZE;
	  sound_dma_automode[my_devnum] = 1;
	}
    }
  else
    printk ("PAS2: Too many PCM devices available\n");

  return mem_start;
}

void
pas_pcm_interrupt (unsigned char status, int cause)
{
  if (cause == 1)		/* PCM buffer done */
    {
      /*
       * Halt the PCM first. Otherwise we don't have time to start a new
       * block before the PCM chip proceeds to the next sample
       */

      if (!sound_dma_automode[my_devnum])
	{
	  pas_write (pas_read (PCM_CONTROL) & ~P_C_PCM_ENABLE,
		     PCM_CONTROL);
	}

      switch (pcm_mode)
	{

	case PCM_DAC:
	  DMAbuf_outputintr (my_devnum);
	  break;

	case PCM_ADC:
	  DMAbuf_inputintr (my_devnum);
	  break;

	default:
	  printk ("PAS: Unexpected PCM interrupt\n");
	}
    }
}

#endif

#endif
