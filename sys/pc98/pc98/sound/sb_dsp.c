/*
 * sound/sb_dsp.c
 *
 * The low level driver for the SoundBlaster DSP chip (SB1.0 to 2.1, SB Pro).
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
 *      Added code to support Sound Galaxy NX Pro
 *
 *      JRA Gibson      April 1995
 *      Code added for MV ProSonic/Jazz 16 in 16 bit mode
 */

#include <i386/isa/sound/sound_config.h>

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SB)

#include <i386/isa/sound/sb.h>
#include <i386/isa/sound/sb_mixer.h>
#undef SB_TEST_IRQ

int             sbc_base = 0;
static int      sbc_irq = 0;
static int      open_mode = 0;	/* Read, write or both */
int             Jazz16_detected = 0;

/*
 * The DSP channel can be used either for input or output. Variable
 * 'sb_irq_mode' will be set when the program calls read or write first time
 * after open. Current version doesn't support mode changes without closing
 * and reopening the device. Support for this feature may be implemented in a
 * future version of this driver.
 */

int             sb_dsp_ok = 0;	/*


				 * *  * * Set to 1 after successful
				 * initialization  *  */
static int      midi_disabled = 0;
int             sb_dsp_highspeed = 0;
int             sbc_major = 1, sbc_minor = 0;	/*


						   * *  * * DSP version   */
static int      dsp_stereo = 0;
static int      dsp_current_speed = DSP_DEFAULT_SPEED;
static int      sb16 = 0;
static int      irq_verified = 0;

int             sb_midi_mode = NORMAL_MIDI;
int             sb_midi_busy = 0;	/*


					 * *  * * 1 if the process has output
					 * to *  * MIDI   */
int             sb_dsp_busy = 0;

volatile int    sb_irq_mode = IMODE_NONE;	/*


						 * *  * * IMODE_INPUT, *
						 * IMODE_OUTPUT * * or *
						 * IMODE_NONE   */
static volatile int irq_ok = 0;

#ifdef JAZZ16
/* 16 bit support
 */

static int      dsp_16bit = 0;
static int      dma8 = 1;
static int      dma16 = 5;

static int      dsp_set_bits (int arg);
static int      initialize_ProSonic16 (void);

/* end of 16 bit support
 */
#endif

int             sb_duplex_midi = 0;
static int      my_dev = 0;

volatile int    sb_intr_active = 0;

static int      dsp_speed (int);
static int      dsp_set_stereo (int mode);

#if !defined(EXCLUDE_MIDI) || !defined(EXCLUDE_AUDIO)

/*
 * Common code for the midi and pcm functions
 */

int
sb_dsp_command (unsigned char val)
{
  int             i;
  unsigned long   limit;

  limit = GET_TIME () + HZ / 10;	/*
					   * The timeout is 0.1 secods
					 */

  /*
   * Note! the i<500000 is an emergency exit. The sb_dsp_command() is sometimes
   * called while interrupts are disabled. This means that the timer is
   * disabled also. However the timeout situation is a abnormal condition.
   * Normally the DSP should be ready to accept commands after just couple of
   * loops.
   */

  for (i = 0; i < 500000 && GET_TIME () < limit; i++)
    {
      if ((INB (DSP_STATUS) & 0x80) == 0)
	{
	  OUTB (val, DSP_COMMAND);
	  return 1;
	}
    }

  printk ("SoundBlaster: DSP Command(%x) Timeout.\n", val);
  printk ("IRQ conflict???\n");
  return 0;
}

void
sbintr (INT_HANDLER_PARMS (irq, dummy))
{
  int             status;

#ifndef EXCLUDE_SBPRO
  if (sb16)
    {
      unsigned char   src = sb_getmixer (IRQ_STAT);	/* Interrupt source register */

#ifndef EXCLUDE_SB16
      if (src & 3)
	sb16_dsp_interrupt (irq);

#ifndef EXCLUDE_MIDI
      if (src & 4)
	sb16midiintr (irq);	/*
				 * SB MPU401 interrupt
				 */
#endif

#endif

      if (!(src & 1))
	return;			/*
				 * Not a DSP interupt
				 */
    }
#endif

  status = INB (DSP_DATA_AVAIL);	/*
					   * Clear interrupt
					 */

  if (sb_intr_active)
    switch (sb_irq_mode)
      {
      case IMODE_OUTPUT:
	sb_intr_active = 0;
	DMAbuf_outputintr (my_dev, 1);
	break;

      case IMODE_INPUT:
	sb_intr_active = 0;
	DMAbuf_inputintr (my_dev);
	/*
	 * A complete buffer has been input. Let's start new one
	 */
	break;

      case IMODE_INIT:
	sb_intr_active = 0;
	irq_ok = 1;
	break;

      case IMODE_MIDI:
#ifndef EXCLUDE_MIDI
	sb_midi_interrupt (irq);
#endif
	break;

      default:
	printk ("SoundBlaster: Unexpected interrupt\n");
      }
}

static int      sb_irq_usecount = 0;

int
sb_get_irq (void)
{
  int             ok;

  if (!sb_irq_usecount)
    if ((ok = snd_set_irq_handler (sbc_irq, sbintr, "SoundBlaster")) < 0)
      return ok;

  sb_irq_usecount++;

  return 0;
}

void
sb_free_irq (void)
{
  if (!sb_irq_usecount)
    return;

  sb_irq_usecount--;

  if (!sb_irq_usecount)
    snd_release_irq (sbc_irq);
}

int
sb_reset_dsp (void)
{
  int             loopc;

  OUTB (1, DSP_RESET);
  tenmicrosec ();
  OUTB (0, DSP_RESET);
  tenmicrosec ();
  tenmicrosec ();
  tenmicrosec ();

  for (loopc = 0; loopc < 1000 && !(INB (DSP_DATA_AVAIL) & 0x80); loopc++);	/*
										 * Wait
										 * for
										 * data
										 * *
										 * available
										 * status
										 */

  if (INB (DSP_READ) != 0xAA)
    return 0;			/*
				 * Sorry
				 */

  return 1;
}

#endif

#ifndef EXCLUDE_AUDIO

static void
dsp_speaker (char state)
{
  if (state)
    sb_dsp_command (DSP_CMD_SPKON);
  else
    sb_dsp_command (DSP_CMD_SPKOFF);
}

static int
dsp_speed (int speed)
{
  unsigned char   tconst;
  unsigned long   flags;
  int             max_speed = 44100;

  if (speed < 4000)
    speed = 4000;

  /*
     * Older SB models don't support higher speeds than 22050.
   */

  if (sbc_major < 2 ||
      (sbc_major == 2 && sbc_minor == 0))
    max_speed = 22050;

  /*
     * SB models earlier than SB Pro have low limit for the input speed.
   */
  if (open_mode != OPEN_WRITE)	/* Recording is possible */
    if (sbc_major < 3)		/* Limited input speed with these cards */
      if (sbc_major == 2 && sbc_minor > 0)
	max_speed = 15000;
      else
	max_speed = 13000;

  if (speed > max_speed)
    speed = max_speed;		/*
				 * Invalid speed
				 */

  /* Logitech SoundMan Games and Jazz16 cards can support 44.1kHz stereo */
#if !defined (SM_GAMES)
  /*
   * Max. stereo speed is 22050
   */
  if (dsp_stereo && speed > 22050 && Jazz16_detected == 0)
    speed = 22050;
#endif

  if ((speed > 22050) && sb_midi_busy)
    {
      printk ("SB Warning: High speed DSP not possible simultaneously with MIDI output\n");
      speed = 22050;
    }

  if (dsp_stereo)
    speed *= 2;

  /*
   * Now the speed should be valid
   */

  if (speed > 22050)
    {				/*
				 * High speed mode
				 */
      int             tmp;

      tconst = (unsigned char) ((65536 -
				 ((256000000 + speed / 2) / speed)) >> 8);
      sb_dsp_highspeed = 1;

      DISABLE_INTR (flags);
      if (sb_dsp_command (0x40))
	sb_dsp_command (tconst);
      RESTORE_INTR (flags);

      tmp = 65536 - (tconst << 8);
      speed = (256000000 + tmp / 2) / tmp;
    }
  else
    {
      int             tmp;

      sb_dsp_highspeed = 0;
      tconst = (256 - ((1000000 + speed / 2) / speed)) & 0xff;

      DISABLE_INTR (flags);
      if (sb_dsp_command (0x40))	/*
					 * Set time constant
					 */
	sb_dsp_command (tconst);
      RESTORE_INTR (flags);

      tmp = 256 - tconst;
      speed = (1000000 + tmp / 2) / tmp;
    }

  if (dsp_stereo)
    speed /= 2;

  dsp_current_speed = speed;
  return speed;
}

static int
dsp_set_stereo (int mode)
{
  dsp_stereo = 0;

#ifdef EXCLUDE_SBPRO
  return 0;
#else
  if (sbc_major < 3 || sb16)
    return 0;			/*
				 * Sorry no stereo
				 */

  if (mode && sb_midi_busy)
    {
      printk ("SB Warning: Stereo DSP not possible simultaneously with MIDI output\n");
      return 0;
    }

  dsp_stereo = !!mode;
  return dsp_stereo;
#endif
}

static void
sb_dsp_output_block (int dev, unsigned long buf, int count,
		     int intrflag, int restart_dma)
{
  unsigned long   flags;

  if (!sb_irq_mode)
    dsp_speaker (ON);

  sb_irq_mode = IMODE_OUTPUT;
  DMAbuf_start_dma (dev, buf, count, DMA_MODE_WRITE);

  if (audio_devs[dev]->dmachan > 3)
    count >>= 1;
  count--;

  if (sb_dsp_highspeed)
    {
      DISABLE_INTR (flags);
      if (sb_dsp_command (0x48))	/*
					   * High speed size
					 */
	{
	  sb_dsp_command ((unsigned char) (count & 0xff));
	  sb_dsp_command ((unsigned char) ((count >> 8) & 0xff));
	  sb_dsp_command (0x91);	/*
					   * High speed 8 bit DAC
					 */
	}
      else
	printk ("SB Error: Unable to start (high speed) DAC\n");
      RESTORE_INTR (flags);
    }
  else
    {
      DISABLE_INTR (flags);
      if (sb_dsp_command (0x14))	/*
					   * 8-bit DAC (DMA)
					 */
	{
	  sb_dsp_command ((unsigned char) (count & 0xff));
	  sb_dsp_command ((unsigned char) ((count >> 8) & 0xff));
	}
      else
	printk ("SB Error: Unable to start DAC\n");
      RESTORE_INTR (flags);
    }
  sb_intr_active = 1;
}

static void
sb_dsp_start_input (int dev, unsigned long buf, int count, int intrflag,
		    int restart_dma)
{
  /*
   * Start a DMA input to the buffer pointed by dmaqtail
   */

  unsigned long   flags;

  if (!sb_irq_mode)
    dsp_speaker (OFF);

  sb_irq_mode = IMODE_INPUT;
  DMAbuf_start_dma (dev, buf, count, DMA_MODE_READ);

  if (audio_devs[dev]->dmachan > 3)
    count >>= 1;
  count--;

  if (sb_dsp_highspeed)
    {
      DISABLE_INTR (flags);
      if (sb_dsp_command (0x48))	/*
					   * High speed size
					 */
	{
	  sb_dsp_command ((unsigned char) (count & 0xff));
	  sb_dsp_command ((unsigned char) ((count >> 8) & 0xff));
	  sb_dsp_command (0x99);	/*
					   * High speed 8 bit ADC
					 */
	}
      else
	printk ("SB Error: Unable to start (high speed) ADC\n");
      RESTORE_INTR (flags);
    }
  else
    {
      DISABLE_INTR (flags);
      if (sb_dsp_command (0x24))	/*
					   * 8-bit ADC (DMA)
					 */
	{
	  sb_dsp_command ((unsigned char) (count & 0xff));
	  sb_dsp_command ((unsigned char) ((count >> 8) & 0xff));
	}
      else
	printk ("SB Error: Unable to start ADC\n");
      RESTORE_INTR (flags);
    }

  sb_intr_active = 1;
}

static void
dsp_cleanup (void)
{
  sb_intr_active = 0;
}

static int
sb_dsp_prepare_for_input (int dev, int bsize, int bcount)
{
  dsp_cleanup ();
  dsp_speaker (OFF);

  if (sbc_major == 3)		/*
				 * SB Pro
				 */
    {
#ifdef JAZZ16
      /* Select correct dma channel
         * for 16/8 bit acccess
       */
      audio_devs[my_dev]->dmachan = dsp_16bit ? dma16 : dma8;
      if (dsp_stereo)
	sb_dsp_command (dsp_16bit ? 0xac : 0xa8);
      else
	sb_dsp_command (dsp_16bit ? 0xa4 : 0xa0);
#else
      /* 8 bit only cards use this
       */
      if (dsp_stereo)
	sb_dsp_command (0xa8);
      else
	sb_dsp_command (0xa0);
#endif
      dsp_speed (dsp_current_speed);	/*
					 * Speed must be recalculated if
					 * #channels * changes
					 */
    }
  return 0;
}

static int
sb_dsp_prepare_for_output (int dev, int bsize, int bcount)
{
  dsp_cleanup ();
  dsp_speaker (ON);

#ifndef EXCLUDE_SBPRO
  if (sbc_major == 3)		/*
				 * SB Pro
				 */
    {
#ifdef JAZZ16
      /* 16 bit specific instructions
       */
      audio_devs[my_dev]->dmachan = dsp_16bit ? dma16 : dma8;
      if (Jazz16_detected != 2)	/* SM Wave */
	sb_mixer_set_stereo (dsp_stereo);
      if (dsp_stereo)
	sb_dsp_command (dsp_16bit ? 0xac : 0xa8);
      else
	sb_dsp_command (dsp_16bit ? 0xa4 : 0xa0);
#else
      sb_mixer_set_stereo (dsp_stereo);
#endif
      dsp_speed (dsp_current_speed);	/*
					 * Speed must be recalculated if
					 * #channels * changes
					 */
    }
#endif
  return 0;
}

static void
sb_dsp_halt_xfer (int dev)
{
}

static int
verify_irq (void)
{
#if 0
  DEFINE_WAIT_QUEUE (testq, testf);

  irq_ok = 0;

  if (sb_get_irq () == -1)
    {
      printk ("*** SB Error: Irq %d already in use\n", sbc_irq);
      return 0;
    }


  sb_irq_mode = IMODE_INIT;

  sb_dsp_command (0xf2);	/*
				 * This should cause immediate interrupt
				 */

  DO_SLEEP (testq, testf, HZ / 5);

  sb_free_irq ();

  if (!irq_ok)
    {
      printk ("SB Warning: IRQ%d test not passed!", sbc_irq);
      irq_ok = 1;
    }
#else
  irq_ok = 1;
#endif
  return irq_ok;
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

  if (sb_intr_active || (sb_midi_busy && sb_midi_mode == UART_MIDI))
    {
      printk ("SB: PCM not possible during MIDI input\n");
      return RET_ERROR (EBUSY);
    }

  if (!irq_verified)
    {
      verify_irq ();
      irq_verified = 1;
    }
  else if (!irq_ok)
    printk ("SB Warning: Incorrect IRQ setting %d\n",
	    sbc_irq);

  retval = sb_get_irq ();
  if (retval)
    return retval;

  /* Allocate 8 bit dma
   */
  if (DMAbuf_open_dma (dev) < 0)
    {
      sb_free_irq ();
      printk ("SB: DMA Busy\n");
      return RET_ERROR (EBUSY);
    }
#ifdef JAZZ16
  /* Allocate 16 bit dma
   */
  if (Jazz16_detected != 0)
    if (dma16 != dma8)
      {
	if (ALLOC_DMA_CHN (dma16, "Jazz16 16 bit"))
	  {
	    sb_free_irq ();
	    RELEASE_DMA_CHN (dma8);
	    return RET_ERROR (EBUSY);
	  }
      }
#endif

  sb_irq_mode = IMODE_NONE;

  sb_dsp_busy = 1;
  open_mode = mode;

  return 0;
}

static void
sb_dsp_close (int dev)
{
#ifdef JAZZ16
  /* Release 16 bit dma channel
   */
  if (Jazz16_detected)
    RELEASE_DMA_CHN (dma16);
#endif

  DMAbuf_close_dma (dev);
  sb_free_irq ();
  dsp_cleanup ();
  dsp_speaker (OFF);
  sb_dsp_busy = 0;
  sb_dsp_highspeed = 0;
  open_mode = 0;
}

#ifdef JAZZ16
/* Function dsp_set_bits() only required for 16 bit cards
 */
static int
dsp_set_bits (int arg)
{
  if (arg)
    if (Jazz16_detected == 0)
      dsp_16bit = 0;
    else
      switch (arg)
	{
	case 8:
	  dsp_16bit = 0;
	  break;
	case 16:
	  dsp_16bit = 1;
	  break;
	default:
	  dsp_16bit = 0;
	}
  return dsp_16bit ? 16 : 8;
}

#endif /* ifdef JAZZ16 */

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
      if (local)
	return dsp_set_stereo (arg - 1) + 1;
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

#ifdef JAZZ16
      /* Word size specific cases here.
         * SNDCTL_DSP_SETFMT=SOUND_PCM_WRITE_BITS
       */
    case SNDCTL_DSP_SETFMT:
      if (local)
	return dsp_set_bits (arg);
      return IOCTL_OUT (arg, dsp_set_bits (IOCTL_IN (arg)));
      break;

    case SOUND_PCM_READ_BITS:
      if (local)
	return dsp_16bit ? 16 : 8;
      return IOCTL_OUT (arg, dsp_16bit ? 16 : 8);
      break;
#else
    case SOUND_PCM_WRITE_BITS:
    case SOUND_PCM_READ_BITS:
      if (local)
	return 8;
      return IOCTL_OUT (arg, 8);	/*
					   * Only 8 bits/sample supported
					 */
      break;
#endif /* ifdef JAZZ16  */

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

  sb_reset_dsp ();
  dsp_speed (dsp_current_speed);
  dsp_cleanup ();

  RESTORE_INTR (flags);
}

#endif


#ifdef JAZZ16

/*
 * Initialization of a Media Vision ProSonic 16 Soundcard.
 * The function initializes a ProSonic 16 like PROS.EXE does for DOS. It sets
 * the base address, the DMA-channels, interrupts and enables the joystickport.
 *
 * Also used by Jazz 16 (same card, different name)
 *
 * written 1994 by Rainer Vranken
 * E-Mail: rvranken@polaris.informatik.uni-essen.de
 */


#ifndef MPU_BASE		/* take default values if not specified */
#define MPU_BASE 0x330
#endif
#ifndef MPU_IRQ
#define MPU_IRQ 9
#endif

unsigned int
get_sb_byte (void)
{
  int             i;

  for (i = 1000; i; i--)
    if (INB (DSP_DATA_AVAIL) & 0x80)
      {
	return INB (DSP_READ);
      }

  return 0xffff;
}

#ifdef SM_WAVE
/*
 * Logitech Soundman Wave detection and initialization by Hannu Savolainen.
 *
 * There is a microcontroller (8031) in the SM Wave card for MIDI emulation.
 * it's located at address MPU_BASE+4.  MPU_BASE+7 is a SM Wave specific
 * control register for MC reset, SCSI, OPL4 and DSP (future expansion)
 * address decoding. Otherwise the SM Wave is just a ordinary MV Jazz16
 * based soundcard.
 */

static void
smw_putmem (int base, int addr, unsigned char val)
{
  unsigned long   flags;

  DISABLE_INTR (flags);

  OUTB (addr & 0xff, base + 1);	/* Low address bits */
  OUTB (addr >> 8, base + 2);	/* High address bits */
  OUTB (val, base);		/* Data */

  RESTORE_INTR (flags);
}

static unsigned char
smw_getmem (int base, int addr)
{
  unsigned long   flags;
  unsigned char   val;

  DISABLE_INTR (flags);

  OUTB (addr & 0xff, base + 1);	/* Low address bits */
  OUTB (addr >> 8, base + 2);	/* High address bits */
  val = INB (base);		/* Data */

  RESTORE_INTR (flags);
  return val;
}

static int
initialize_smw (void)
{
#ifdef SMW_MIDI0001_INCLUDED
#include <i386/isa/sound/smw-midi0001.h>
#else
  unsigned char   smw_ucode[1];
  int             smw_ucodeLen = 0;

#endif

  int             mp_base = MPU_BASE + 4;	/* Microcontroller base */
  int             i;
  unsigned char   control;

  /*
     *  Reset the microcontroller so that the RAM can be accessed
   */

  control = INB (MPU_BASE + 7);
  OUTB (control | 3, MPU_BASE + 7);	/* Set last two bits to 1 (?) */
  OUTB ((control & 0xfe) | 2, MPU_BASE + 7);	/* xxxxxxx0 resets the mc */

  for (i = 0; i < 300; i++)	/* Wait at least 1ms */
    tenmicrosec ();

  OUTB (control & 0xfc, MPU_BASE + 7);	/* xxxxxx00 enables RAM */

  /*
     *  Detect microcontroller by probing the 8k RAM area
   */
  smw_putmem (mp_base, 0, 0x00);
  smw_putmem (mp_base, 1, 0xff);
  tenmicrosec ();

  if (smw_getmem (mp_base, 0) != 0x00 || smw_getmem (mp_base, 1) != 0xff)
    {
      printk ("\nSM Wave: No microcontroller RAM detected (%02x, %02x)\n",
	      smw_getmem (mp_base, 0), smw_getmem (mp_base, 1));
      return 0;			/* No RAM */
    }

  /*
     *  There is RAM so assume it's really a SM Wave
   */

#ifdef SMW_MIDI0001_INCLUDED
  if (smw_ucodeLen != 8192)
    {
      printk ("\nSM Wave: Invalid microcode (MIDI0001.BIN) length\n");
      return 1;
    }
#endif

  /*
     *  Download microcode
   */

  for (i = 0; i < 8192; i++)
    smw_putmem (mp_base, i, smw_ucode[i]);

  /*
     *  Verify microcode
   */

  for (i = 0; i < 8192; i++)
    if (smw_getmem (mp_base, i) != smw_ucode[i])
      {
	printk ("SM Wave: Microcode verification failed\n");
	return 0;
      }

  control = 0;
#ifdef SMW_SCSI_IRQ
  /*
     * Set the SCSI interrupt (IRQ2/9, IRQ3 or IRQ10). The SCSI interrupt
     * is disabled by default.
     *
     * Btw the Zilog 5380 SCSI controller is located at MPU base + 0x10.
   */
  {
    static unsigned char scsi_irq_bits[] =
    {0, 0, 3, 1, 0, 0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0};

    control |= scsi_irq_bits[SMW_SCSI_IRQ] << 6;
  }
#endif

#ifdef SMW_OPL4_ENABLE
  /*
     *  Make the OPL4 chip visible on the PC bus at 0x380.
     *
     *  There is no need to enable this feature since VoxWare
     *  doesn't support OPL4 yet. Also there is no RAM in SM Wave so
     *  enabling OPL4 is pretty useless.
   */
  control |= 0x10;		/* Uses IRQ12 if bit 0x20 == 0 */
  /* control |= 0x20;      Uncomment this if you want to use IRQ7 */
#endif

  OUTB (control | 0x03, MPU_BASE + 7);	/* xxxxxx11 restarts */
  return 1;
}

#endif

static int
initialize_ProSonic16 (void)
{
  int             x;
  static unsigned char int_translat[16] =
  {0, 0, 2, 3, 0, 1, 0, 4, 0, 2, 5, 0, 0, 0, 0, 6}, dma_translat[8] =
  {0, 1, 0, 2, 0, 3, 0, 4};

  OUTB (0xAF, 0x201);		/* ProSonic/Jazz16 wakeup */
  for (x = 0; x < 1000; ++x)	/* wait 10 milliseconds */
    tenmicrosec ();
  OUTB (0x50, 0x201);
  OUTB ((sbc_base & 0x70) | ((MPU_BASE & 0x30) >> 4), 0x201);

  if (sb_reset_dsp ())
    {				/* OK. We have at least a SB */

      /* Check the version number of ProSonic (I guess) */

      if (!sb_dsp_command (0xFA))
	return 1;
      if (get_sb_byte () != 0x12)
	return 1;

      if (sb_dsp_command (0xFB) &&	/* set DMA-channels and Interrupts */
	  sb_dsp_command ((dma_translat[JAZZ_DMA16] << 4) | dma_translat[SBC_DMA]) &&
      sb_dsp_command ((int_translat[MPU_IRQ] << 4) | int_translat[sbc_irq]))
	{
	  Jazz16_detected = 1;
#ifdef SM_WAVE
	  if (initialize_smw ())
	    Jazz16_detected = 2;
#endif
	  sb_dsp_disable_midi ();
	}

      return 1;			/* There was at least a SB */
    }
  return 0;			/* No SB or ProSonic16 detected */
}

#endif /* ifdef JAZZ16  */

int
sb_dsp_detect (struct address_info *hw_config)
{
  sbc_base = hw_config->io_base;
  sbc_irq = hw_config->irq;

  if (sb_dsp_ok)
    return 0;			/*
				 * Already initialized
				 */
#ifdef JAZZ16
  dma8 = hw_config->dma;
  dma16 = JAZZ_DMA16;

  if (!initialize_ProSonic16 ())
    return 0;
#else
  if (!sb_reset_dsp ())
    return 0;
#endif

#ifdef PC98
  switch (sbc_irq)
    {
    case 3:
      sb_setmixer (IRQ_NR, 1);
      break;
    case 5:
      sb_setmixer (IRQ_NR, 8);
      break;
    case 10:
      sb_setmixer (IRQ_NR, 2);
      break;
    }
  switch (hw_config->dma)
    {
    case 0:
      sb_setmixer (DMA_NR, 1);
      break;
    case 3:
      sb_setmixer (DMA_NR, 2);
      break;
    }
#endif

  return 1;			/*
				 * Detected
				 */
}

#ifndef EXCLUDE_AUDIO
static struct audio_operations sb_dsp_operations =
{
  "SoundBlaster",
  NOTHING_SPECIAL,
  AFMT_U8,			/* Just 8 bits. Poor old SB */
  NULL,
  sb_dsp_open,
  sb_dsp_close,
  sb_dsp_output_block,
  sb_dsp_start_input,
  sb_dsp_ioctl,
  sb_dsp_prepare_for_input,
  sb_dsp_prepare_for_output,
  sb_dsp_reset,
  sb_dsp_halt_xfer,
  NULL,				/* local_qlen */
  NULL				/* copy_from_user */
};

#endif

long
sb_dsp_init (long mem_start, struct address_info *hw_config)
{
  int             i;
  int             mixer_type = 0;

  sbc_major = sbc_minor = 0;
  sb_dsp_command (0xe1);	/*
				 * Get version
				 */

  for (i = 1000; i; i--)
    {
      if (INB (DSP_DATA_AVAIL) & 0x80)
	{			/*
				 * wait for Data Ready
				 */
	  if (sbc_major == 0)
	    sbc_major = INB (DSP_READ);
	  else
	    {
	      sbc_minor = INB (DSP_READ);
	      break;
	    }
	}
    }

  if (sbc_major == 2 || sbc_major == 3)
    sb_duplex_midi = 1;

  if (sbc_major == 4)
    sb16 = 1;

#ifndef EXCLUDE_SBPRO
  if (sbc_major >= 3)
    mixer_type = sb_mixer_init (sbc_major);
#else
  if (sbc_major >= 3)
    printk ("\n\n\n\nNOTE! SB Pro support is required with your soundcard!\n\n\n");
#endif

#ifndef EXCLUDE_YM3812

#ifdef PC98
  if (sbc_major > 3 ||
      (sbc_major == 3 && INB (0x28d2) == 0x00))
#else
  if (sbc_major > 3 ||
      (sbc_major == 3 && INB (0x388) == 0x00))	/* Should be 0x06 if not OPL-3 */
#endif
    enable_opl3_mode (OPL3_LEFT, OPL3_RIGHT, OPL3_BOTH);
#endif

#ifndef EXCLUDE_AUDIO
  if (sbc_major >= 3)
    {
      if (Jazz16_detected)
	{
	  if (Jazz16_detected == 2)
	    sprintf (sb_dsp_operations.name, "SoundMan Wave %d.%d", sbc_major, sbc_minor);
	  else
	    sprintf (sb_dsp_operations.name, "MV Jazz16 %d.%d", sbc_major, sbc_minor);
	  sb_dsp_operations.format_mask |= AFMT_S16_LE;		/* Hurrah, 16 bits          */
	}
      else
#ifdef __SGNXPRO__
      if (mixer_type == 2)
	{
	  sprintf (sb_dsp_operations.name, "Sound Galaxy NX Pro %d.%d", sbc_major, sbc_minor);
	}
      else
#endif

      if (sbc_major == 4)
	{
	  sprintf (sb_dsp_operations.name, "SoundBlaster 16 %d.%d", sbc_major, sbc_minor);
	}
      else
	{
	  sprintf (sb_dsp_operations.name, "SoundBlaster Pro %d.%d", sbc_major, sbc_minor);
	}
    }
  else
    {
      sprintf (sb_dsp_operations.name, "SoundBlaster %d.%d", sbc_major, sbc_minor);
    }

#if defined(__FreeBSD__)
  printk ("sb0: <%s>", sb_dsp_operations.name);
#else
  printk (" <%s>", sb_dsp_operations.name);
#endif

#if !defined(EXCLUDE_SB16) && !defined(EXCLUDE_SBPRO)
  if (!sb16)			/*
				 * There is a better driver for SB16
				 */
#endif
    if (num_audiodevs < MAX_AUDIO_DEV)
      {
	audio_devs[my_dev = num_audiodevs++] = &sb_dsp_operations;
	audio_devs[my_dev]->buffcount = DSP_BUFFCOUNT;
	audio_devs[my_dev]->buffsize = (
		(sbc_major > 2 || sbc_major == 2 && sbc_minor > 0) ?
		16 : 8) * 1024;
	audio_devs[my_dev]->dmachan = hw_config->dma;
      }
    else
      printk ("SB: Too many DSP devices available\n");
#else
  printk (" <SoundBlaster (configured without audio support)>");
#endif

#ifndef EXCLUDE_MIDI
  if (!midi_disabled && !sb16)	/*
				 * Midi don't work in the SB emulation mode *
				 * of PAS, SB16 has better midi interface
				 */
    sb_midi_init (sbc_major);
#endif

  sb_dsp_ok = 1;
  return mem_start;
}

void
sb_dsp_disable_midi (void)
{
  midi_disabled = 1;
}

#endif
