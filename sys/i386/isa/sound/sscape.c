/*
 * sound/sscape.c
 *
 * Low level driver for Ensoniq Soundscape
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
 */

#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_SSCAPE)

#include "coproc.h"

/*
 *    I/O ports
 */
#define MIDI_DATA        0
#define MIDI_CTRL        1
#define HOST_CTRL        2
#define TX_READY		0x02
#define RX_READY		0x01
#define HOST_DATA        3
#define ODIE_ADDR        4
#define ODIE_DATA        5

/*
 *    Indirect registers
 */
#define GA_INTSTAT_REG   0
#define GA_INTENA_REG    1
#define GA_DMAA_REG      2
#define GA_DMAB_REG      3
#define GA_INTCFG_REG    4
#define GA_DMACFG_REG    5
#define GA_CDCFG_REG     6
#define GA_SMCFGA_REG    7
#define GA_SMCFGB_REG    8
#define GA_HMCTL_REG     9

/*
 * DMA channel identifiers (A and B)
 */
#define SSCAPE_DMA_A 		0
#define SSCAPE_DMA_B		1

#define PORT(name)	(devc->base+name)

/*
 * Host commands recognized by the OBP microcode
 */
#define CMD_GEN_HOST_ACK        0x80
#define CMD_GEN_MPU_ACK         0x81
#define CMD_GET_BOARD_TYPE      0x82
#define CMD_SET_CONTROL         0x88
#define CMD_GET_CONTROL         0x89
#define CMD_SET_MT32            0x96
#define CMD_GET_MT32            0x97
#define CMD_SET_EXTMIDI         0x9b
#define CMD_GET_EXTMIDI         0x9c

#define CMD_ACK			0x80

typedef struct sscape_info
  {
    int             base, irq, dma;
    int             ok;		/* Properly detected */
    int             dma_allocated;
    int             my_audiodev;
    int             opened;
  }

sscape_info;
static struct sscape_info dev_info =
{0};
static struct sscape_info *devc = &dev_info;

DEFINE_WAIT_QUEUE (sscape_sleeper, sscape_sleep_flag);

#ifdef REVEAL_SPEA
/* Spea and Reveal have assigned interrupt bits differently than Ensoniq */
static char     valid_interrupts[] =
{9, 7, 5, 15};

#else
static char     valid_interrupts[] =
{9, 5, 7, 10};

#endif

static unsigned char
sscape_read (struct sscape_info *devc, int reg)
{
  unsigned long   flags;
  unsigned char   val;

  DISABLE_INTR (flags);
  OUTB (reg, PORT (ODIE_ADDR));
  val = INB (PORT (ODIE_DATA));
  RESTORE_INTR (flags);
  return val;
}

static void
sscape_write (struct sscape_info *devc, int reg, int data)
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  OUTB (reg, PORT (ODIE_ADDR));
  OUTB (data, PORT (ODIE_DATA));
  RESTORE_INTR (flags);
}

static void
host_open (struct sscape_info *devc)
{
  OUTB (0x00, PORT (HOST_CTRL));	/* Put the board to the host mode */
}

static void
host_close (struct sscape_info *devc)
{
  OUTB (0x03, PORT (HOST_CTRL));	/* Put the board to the MIDI mode */
}

static int
host_write (struct sscape_info *devc, unsigned char *data, int count)
{
  unsigned long   flags;
  int             i, timeout;

  DISABLE_INTR (flags);

  /*
     * Send the command and data bytes
   */

  for (i = 0; i < count; i++)
    {
      for (timeout = 10000; timeout > 0; timeout--)
	if (INB (PORT (HOST_CTRL)) & TX_READY)
	  break;

      if (timeout <= 0)
	{
	  RESTORE_INTR (flags);
	  return 0;
	}

      OUTB (data[i], PORT (HOST_DATA));
    }


  RESTORE_INTR (flags);

  return 1;
}

static int
host_read (struct sscape_info *devc)
{
  unsigned long   flags;
  int             timeout;
  unsigned char   data;

  DISABLE_INTR (flags);

  /*
     * Read a byte
   */

  for (timeout = 10000; timeout > 0; timeout--)
    if (INB (PORT (HOST_CTRL)) & RX_READY)
      break;

  if (timeout <= 0)
    {
      RESTORE_INTR (flags);
      return -1;
    }

  data = INB (PORT (HOST_DATA));

  RESTORE_INTR (flags);

  return data;
}

static int
host_command1 (struct sscape_info *devc, int cmd)
{
  unsigned char   buf[10];

  buf[0] = (unsigned char) (cmd & 0xff);

  return host_write (devc, buf, 1);
}

static int
host_command2 (struct sscape_info *devc, int cmd, int parm1)
{
  unsigned char   buf[10];

  buf[0] = (unsigned char) (cmd & 0xff);
  buf[1] = (unsigned char) (parm1 & 0xff);

  return host_write (devc, buf, 2);
}

static int
host_command3 (struct sscape_info *devc, int cmd, int parm1, int parm2)
{
  unsigned char   buf[10];

  buf[0] = (unsigned char) (cmd & 0xff);
  buf[1] = (unsigned char) (parm1 & 0xff);
  buf[2] = (unsigned char) (parm2 & 0xff);

  return host_write (devc, buf, 3);
}

static void
set_mt32 (struct sscape_info *devc, int value)
{
  host_open (devc);
  host_command2 (devc, CMD_SET_MT32,
		 value ? 1 : 0);
  if (host_read (devc) != CMD_ACK)
    {
      printk ("SNDSCAPE: Setting MT32 mode failed\n");
    }
  host_close (devc);
}

static int
get_board_type (struct sscape_info *devc)
{
  int             tmp;

  host_open (devc);
  if (!host_command1 (devc, CMD_GET_BOARD_TYPE))
    tmp = -1;
  else
    tmp = host_read (devc);
  host_close (devc);
  return tmp;
}

void
sscapeintr (INT_HANDLER_PARMS (irq, dummy))
{
  unsigned char   bits, tmp;
  static int      debug = 0;

  printk ("sscapeintr(0x%02x)\n", (bits = sscape_read (devc, GA_INTSTAT_REG)));
  if (SOMEONE_WAITING (sscape_sleeper, sscape_sleep_flag))
    {
      WAKE_UP (sscape_sleeper, sscape_sleep_flag);
    }

  if (bits & 0x02)		/* Host interface interrupt */
    {
      printk ("SSCAPE: Host interrupt, data=%02x\n", host_read (devc));
    }

#if (!defined(EXCLUDE_MPU401) || !defined(EXCLUDE_MPU_EMU)) && !defined(EXCLUDE_MIDI)
  if (bits & 0x01)
    {
      mpuintr (INT_HANDLER_CALL (irq));
      if (debug++ > 10)		/* Temporary debugging hack */
	{
	  sscape_write (devc, GA_INTENA_REG, 0x00);	/* Disable all interrupts */
	}
    }
#endif

  /*
     * Acknowledge interrupts (toggle the interrupt bits)
   */

  tmp = sscape_read (devc, GA_INTENA_REG);
  sscape_write (devc, GA_INTENA_REG, (~bits & 0x0e) | (tmp & 0xf1));

}

static void
sscape_enable_intr (struct sscape_info *devc, unsigned intr_bits)
{
  unsigned char   temp, orig;

  temp = orig = sscape_read (devc, GA_INTENA_REG);
  temp |= intr_bits;
  temp |= 0x80;			/* Master IRQ enable */

  if (temp == orig)
    return;			/* No change */

  sscape_write (devc, GA_INTENA_REG, temp);
}

static void
sscape_disable_intr (struct sscape_info *devc, unsigned intr_bits)
{
  unsigned char   temp, orig;

  temp = orig = sscape_read (devc, GA_INTENA_REG);
  temp &= ~intr_bits;
  if ((temp & ~0x80) == 0x00)
    temp = 0x00;		/* Master IRQ disable */
  if (temp == orig)
    return;			/* No change */

  sscape_write (devc, GA_INTENA_REG, temp);
}

static void
do_dma (struct sscape_info *devc, int dma_chan, unsigned long buf, int blk_size, int mode)
{
  unsigned char   temp;

  if (dma_chan != SSCAPE_DMA_A)
    {
      printk ("SSCAPE: Tried to use DMA channel  != A. Why?\n");
      return;
    }

  DMAbuf_start_dma (devc->my_audiodev,
		    buf,
		    blk_size, mode);

  temp = devc->dma << 4;	/* Setup DMA channel select bits */
  if (devc->dma <= 3)
    temp |= 0x80;		/* 8 bit DMA channel */

  temp |= 1;			/* Trigger DMA */
  sscape_write (devc, GA_DMAA_REG, temp);
  temp &= 0xfe;			/* Clear DMA trigger */
  sscape_write (devc, GA_DMAA_REG, temp);
}

static int
verify_mpu (struct sscape_info *devc)
{
  /*
     * The SoundScape board could be in three modes (MPU, 8250 and host).
     * If the card is not in the MPU mode, enabling the MPU driver will
     * cause infinite loop (the driver believes that there is always some
     * received data in the buffer.
     *
     * Detect this by looking if there are more than 10 received MIDI bytes
     * (0x00) in the buffer.
   */

  int             i;

  for (i = 0; i < 10; i++)
    {
      if (INB (devc->base + HOST_CTRL) & 0x80)
	return 1;

      if (INB (devc->base) != 0x00)
	return 1;
    }

  printk ("SoundScape: The device is not in the MPU-401 mode\n");
  return 0;
}

static int
sscape_coproc_open (void *dev_info, int sub_device)
{
  if (sub_device == COPR_MIDI)
    {
      set_mt32 (devc, 0);
      if (!verify_mpu (devc))
	return RET_ERROR (EIO);
    }

  return 0;
}

static void
sscape_coproc_close (void *dev_info, int sub_device)
{
  struct sscape_info *devc = dev_info;
  unsigned long   flags;

  DISABLE_INTR (flags);
  if (devc->dma_allocated)
    {
      sscape_write (devc, GA_DMAA_REG, 0x20);	/* DMA channel disabled */
#ifndef EXCLUDE_NATIVE_PCM
      DMAbuf_close_dma (devc->my_audiodev);
#endif
      devc->dma_allocated = 0;
    }
  RESET_WAIT_QUEUE (sscape_sleeper, sscape_sleep_flag);
  RESTORE_INTR (flags);

  return;
}

static void
sscape_coproc_reset (void *dev_info)
{
}

static int
sscape_download_boot (struct sscape_info *devc, unsigned char *block, int size, int flag)
{
  unsigned long   flags;
  unsigned char   temp;
  int             done, timeout;

  if (flag & CPF_FIRST)
    {
      /*
         * First block. Have to allocate DMA and to reset the board
         * before continuing.
       */

      DISABLE_INTR (flags);
      if (devc->dma_allocated == 0)
	{
#ifndef EXCLUDE_NATIVE_PCM
	  if (DMAbuf_open_dma (devc->my_audiodev) < 0)
	    {
	      RESTORE_INTR (flags);
	      return 0;
	    }
#endif

	  devc->dma_allocated = 1;
	}
      RESTORE_INTR (flags);

      sscape_write (devc, GA_HMCTL_REG,
		    (temp = sscape_read (devc, GA_HMCTL_REG)) & 0x3f);	/*Reset */

      for (timeout = 10000; timeout > 0; timeout--)
	sscape_read (devc, GA_HMCTL_REG);	/* Delay */

      /* Take board out of reset */
      sscape_write (devc, GA_HMCTL_REG,
		    (temp = sscape_read (devc, GA_HMCTL_REG)) | 0x80);
    }

  /*
     * Transfer one code block using DMA
   */
  memcpy (audio_devs[devc->my_audiodev]->dmap->raw_buf[0], block, size);

  DISABLE_INTR (flags);
/******** INTERRUPTS DISABLED NOW ********/
  do_dma (devc, SSCAPE_DMA_A,
	  audio_devs[devc->my_audiodev]->dmap->raw_buf_phys[0],
	  size, DMA_MODE_WRITE);

  /*
   * Wait until transfer completes.
   */
  RESET_WAIT_QUEUE (sscape_sleeper, sscape_sleep_flag);
  done = 0;
  timeout = 100;
  while (!done && timeout-- > 0)
    {
      int             resid;

      DO_SLEEP (sscape_sleeper, sscape_sleep_flag, 1);
      clear_dma_ff (devc->dma);
      if ((resid = get_dma_residue (devc->dma)) == 0)
	done = 1;
    }

  RESTORE_INTR (flags);
  if (!done)
    return 0;

  if (flag & CPF_LAST)
    {
      /*
         * Take the board out of reset
       */
      OUTB (0x00, PORT (HOST_CTRL));
      OUTB (0x00, PORT (MIDI_CTRL));

      temp = sscape_read (devc, GA_HMCTL_REG);
      temp |= 0x40;
      sscape_write (devc, GA_HMCTL_REG, temp);	/* Kickstart the board */

      /*
         * Wait until the ODB wakes up
       */

      DISABLE_INTR (flags);
      done = 0;
      timeout = 5 * HZ;
      while (!done && timeout-- > 0)
	{
	  DO_SLEEP (sscape_sleeper, sscape_sleep_flag, 1);
	  if (INB (PORT (HOST_DATA)) == 0xff)	/* OBP startup acknowledge */
	    done = 1;
	}
      RESTORE_INTR (flags);
      if (!done)
	{
	  printk ("SoundScape: The OBP didn't respond after code download\n");
	  return 0;
	}

      DISABLE_INTR (flags);
      done = 0;
      timeout = 5 * HZ;
      while (!done && timeout-- > 0)
	{
	  DO_SLEEP (sscape_sleeper, sscape_sleep_flag, 1);
	  if (INB (PORT (HOST_DATA)) == 0xfe)	/* Host startup acknowledge */
	    done = 1;
	}
      RESTORE_INTR (flags);
      if (!done)
	{
	  printk ("SoundScape: OBP Initialization failed.\n");
	  return 0;
	}

      printk ("SoundScape board of type %d initialized OK\n",
	      get_board_type (devc));

#ifdef SSCAPE_DEBUG3
      /*
         * Temporary debugging aid. Print contents of the registers after
         * downloading the code.
       */
      {
	int             i;

	for (i = 0; i < 13; i++)
	  printk ("I%d = %02x (new value)\n", i, sscape_read (devc, i));
      }
#endif

    }

  return 1;
}

static int
download_boot_block (void *dev_info, copr_buffer * buf)
{
  if (buf->len <= 0 || buf->len > sizeof (buf->data))
    return RET_ERROR (EINVAL);

  if (!sscape_download_boot (devc, buf->data, buf->len, buf->flags))
    {
      printk ("SSCAPE: Unable to load microcode block to the OBP.\n");
      return RET_ERROR (EIO);
    }

  return 0;
}

static int
sscape_coproc_ioctl (void *dev_info, unsigned int cmd, unsigned int arg, int local)
{

  switch (cmd)
    {
    case SNDCTL_COPR_RESET:
      sscape_coproc_reset (dev_info);
      return 0;
      break;

    case SNDCTL_COPR_LOAD:
      {
	copr_buffer    *buf;
	int             err;

	buf = (copr_buffer *) KERNEL_MALLOC (sizeof (copr_buffer));
	IOCTL_FROM_USER ((char *) buf, (char *) arg, 0, sizeof (*buf));
	err = download_boot_block (dev_info, buf);
	KERNEL_FREE (buf);
	return err;
      }
      break;

    default:
      return RET_ERROR (EINVAL);
    }

  return RET_ERROR (EINVAL);
}

static coproc_operations sscape_coproc_operations =
{
  "SoundScape M68K",
  sscape_coproc_open,
  sscape_coproc_close,
  sscape_coproc_ioctl,
  sscape_coproc_reset,
  &dev_info
};

static int
sscape_audio_open (int dev, int mode)
{
  unsigned long   flags;
  sscape_info    *devc = (sscape_info *) audio_devs[dev]->devc;

  DISABLE_INTR (flags);
  if (devc->opened)
    {
      RESTORE_INTR (flags);
      return RET_ERROR (EBUSY);
    }

  if (devc->dma_allocated == 0)
    {
      int             err;

      if ((err = DMAbuf_open_dma (devc->my_audiodev)) < 0)
	{
	  RESTORE_INTR (flags);
	  return err;
	}

      devc->dma_allocated = 1;
    }

  devc->opened = 1;
  RESTORE_INTR (flags);
#ifdef SSCAPE_DEBUG4
  /*
     * Temporary debugging aid. Print contents of the registers
     * when the device is opened.
   */
  {
    int             i;

    for (i = 0; i < 13; i++)
      printk ("I%d = %02x\n", i, sscape_read (devc, i));
  }
#endif

  return 0;
}

static void
sscape_audio_close (int dev)
{
  unsigned long   flags;
  sscape_info    *devc = (sscape_info *) audio_devs[dev]->devc;

  DEB (printk ("sscape_audio_close(void)\n"));

  DISABLE_INTR (flags);

  if (devc->dma_allocated)
    {
      sscape_write (devc, GA_DMAA_REG, 0x20);	/* DMA channel disabled */
      DMAbuf_close_dma (dev);
      devc->dma_allocated = 0;
    }
  devc->opened = 0;

  RESTORE_INTR (flags);
}

static int
set_speed (sscape_info * devc, int arg)
{
  return 8000;
}

static int
set_channels (sscape_info * devc, int arg)
{
  return 1;
}

static int
set_format (sscape_info * devc, int arg)
{
  return AFMT_U8;
}

static int
sscape_audio_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  sscape_info    *devc = (sscape_info *) audio_devs[dev]->devc;

  switch (cmd)
    {
    case SOUND_PCM_WRITE_RATE:
      if (local)
	return set_speed (devc, arg);
      return IOCTL_OUT (arg, set_speed (devc, IOCTL_IN (arg)));

    case SOUND_PCM_READ_RATE:
      if (local)
	return 8000;
      return IOCTL_OUT (arg, 8000);

    case SNDCTL_DSP_STEREO:
      if (local)
	return set_channels (devc, arg + 1) - 1;
      return IOCTL_OUT (arg, set_channels (devc, IOCTL_IN (arg) + 1) - 1);

    case SOUND_PCM_WRITE_CHANNELS:
      if (local)
	return set_channels (devc, arg);
      return IOCTL_OUT (arg, set_channels (devc, IOCTL_IN (arg)));

    case SOUND_PCM_READ_CHANNELS:
      if (local)
	return 1;
      return IOCTL_OUT (arg, 1);

    case SNDCTL_DSP_SAMPLESIZE:
      if (local)
	return set_format (devc, arg);
      return IOCTL_OUT (arg, set_format (devc, IOCTL_IN (arg)));

    case SOUND_PCM_READ_BITS:
      if (local)
	return 8;
      return IOCTL_OUT (arg, 8);

    default:;
    }
  return RET_ERROR (EINVAL);
}

static void
sscape_audio_output_block (int dev, unsigned long buf, int count, int intrflag, int dma_restart)
{
}

static void
sscape_audio_start_input (int dev, unsigned long buf, int count, int intrflag, int dma_restart)
{
}

static int
sscape_audio_prepare_for_input (int dev, int bsize, int bcount)
{
  return 0;
}

static int
sscape_audio_prepare_for_output (int dev, int bsize, int bcount)
{
  return 0;
}

static void
sscape_audio_halt (int dev)
{
}

static void
sscape_audio_reset (int dev)
{
  sscape_audio_halt (dev);
}

static struct audio_operations sscape_audio_operations =
{
  "Ensoniq SoundScape channel A",
  0,
  AFMT_U8 | AFMT_S16_LE,
  NULL,
  sscape_audio_open,
  sscape_audio_close,
  sscape_audio_output_block,
  sscape_audio_start_input,
  sscape_audio_ioctl,
  sscape_audio_prepare_for_input,
  sscape_audio_prepare_for_output,
  sscape_audio_reset,
  sscape_audio_halt,
  NULL,
  NULL
};

long
attach_sscape (long mem_start, struct address_info *hw_config)
{
  int             my_dev;

#ifndef SSCAPE_REGS
  /*
     * Config register values for Spea/V7 Media FX and Ensoniq S-2000.
     * These values are card
     * dependent. If you have another SoundScape based card, you have to
     * find the correct values. Do the following:
     *  - Compile this driver with SSCAPE_DEBUG1 defined.
     *  - Shut down and power off your machine.
     *  - Boot with DOS so that the SSINIT.EXE program is run.
     *  - Warm boot to {Linux|SYSV|BSD} and write down the lines displayed
     *    when detecting the SoundScape.
     *  - Modify the following list to use the values printed during boot.
     *    Undefine the SSCAPE_DEBUG1
   */
#define SSCAPE_REGS { \
/* I0 */	0x00, \
		0xf0, /* Note! Ignored. Set always to 0xf0 */ \
		0x20, /* Note! Ignored. Set always to 0x20 */ \
		0x20, /* Note! Ignored. Set always to 0x20 */ \
		0xf5, /* Ignored */ \
		0x10, \
		0x00, \
		0x2e, /* I7 MEM config A. Likely to vary between models */ \
		0x00, /* I8 MEM config A. Likely to vary between models */ \
/* I9 */	0x40 /* Ignored */ \
	}
#endif

  unsigned long   flags;
  static unsigned char regs[10] = SSCAPE_REGS;

  int             i, irq_bits = 0xff;

  if (!probe_sscape (hw_config))
    return mem_start;

  printk (" <Ensoniq Soundscape>");

  for (i = 0; i < sizeof (valid_interrupts); i++)
    if (hw_config->irq == valid_interrupts[i])
      {
	irq_bits = i;
	break;
      }

  if (hw_config->irq > 15 || (regs[4] = irq_bits == 0xff))
    {
      printk ("Invalid IRQ%d\n", hw_config->irq);
      return mem_start;
    }

  DISABLE_INTR (flags);

  for (i = 1; i < 10; i++)
    switch (i)
      {
      case 1:			/* Host interrupt enable */
	sscape_write (devc, i, 0xf0);	/* All interrupts enabled */
	break;

      case 2:			/* DMA A status/trigger register */
      case 3:			/* DMA B status/trigger register */
	sscape_write (devc, i, 0x20);	/* DMA channel disabled */
	break;

      case 4:			/* Host interrupt config reg */
	sscape_write (devc, i, 0xf0 | (irq_bits << 2) | irq_bits);
	break;

      case 5:			/* Don't destroy CD-ROM DMA config bits (0xc0) */
	sscape_write (devc, i, (regs[i] & 0x3f) |
		      (sscape_read (devc, i) & 0x0c));
	break;

      case 6:			/* CD-ROM config. Don't touch. */
	break;

      case 9:			/* Master control reg. Don't modify CR-ROM bits. Disable SB emul */
	sscape_write (devc, i,
		      (sscape_read (devc, i) & 0xf0) | 0x00);
	break;

      default:
	sscape_write (devc, i, regs[i]);
      }

  RESTORE_INTR (flags);

#ifdef SSCAPE_DEBUG2
  /*
     * Temporary debugging aid. Print contents of the registers after
     * changing them.
   */
  {
    int             i;

    for (i = 0; i < 13; i++)
      printk ("I%d = %02x (new value)\n", i, sscape_read (devc, i));
  }
#endif

#if !defined(EXCLUDE_MIDI) && !defined(EXCLUDE_MPU_EMU)
  hw_config->always_detect = 1;
  if (probe_mpu401 (hw_config))
    {
      int             prev_devs;

      prev_devs = num_midis;
      mem_start = attach_mpu401 (mem_start, hw_config);

      if (num_midis == (prev_devs + 1))		/* The MPU driver installed itself */
	midi_devs[prev_devs]->coproc = &sscape_coproc_operations;
    }
#endif

#ifndef EXCLUDE_NATIVE_PCM
  /* Not supported yet */

#ifndef EXCLUDE_AUDIO
  if (num_audiodevs < MAX_AUDIO_DEV)
    {
      audio_devs[my_dev = num_audiodevs++] = &sscape_audio_operations;
      audio_devs[my_dev]->dmachan = hw_config->dma;
      audio_devs[my_dev]->buffcount = 1;
      audio_devs[my_dev]->buffsize = DSP_BUFFSIZE;
      audio_devs[my_dev]->devc = devc;
      devc->my_audiodev = my_dev;
      devc->opened = 0;
      audio_devs[my_dev]->coproc = &sscape_coproc_operations;
      if (snd_set_irq_handler (hw_config->irq, sscapeintr, "SoundScape") < 0)
	printk ("Error: Can't allocate IRQ for SoundScape\n");

      sscape_write (devc, GA_INTENA_REG, 0x80);		/* Master IRQ enable */
    }
  else
    printk ("SoundScape: More than enough audio devices detected\n");
#endif
#endif
  devc->ok = 1;
  return mem_start;
}

int
probe_sscape (struct address_info *hw_config)
{
  unsigned char   save;

  devc->base = hw_config->io_base;
  devc->irq = hw_config->irq;
  devc->dma = hw_config->dma;

  /*
     * First check that the address register of "ODIE" is
     * there and that it has exactly 4 writeable bits.
     * First 4 bits
   */
  if ((save = INB (PORT (ODIE_ADDR))) & 0xf0)
    return 0;

  OUTB (0x00, PORT (ODIE_ADDR));
  if (INB (PORT (ODIE_ADDR)) != 0x00)
    return 0;

  OUTB (0xff, PORT (ODIE_ADDR));
  if (INB (PORT (ODIE_ADDR)) != 0x0f)
    return 0;

  OUTB (save, PORT (ODIE_ADDR));

  /*
     * Now verify that some indirect registers return zero on some bits.
     * This may break the driver with some future revisions of "ODIE" but...
   */

  if (sscape_read (devc, 0) & 0x0c)
    return 0;

  if (sscape_read (devc, 1) & 0x0f)
    return 0;

  if (sscape_read (devc, 5) & 0x0f)
    return 0;

#ifdef SSCAPE_DEBUG1
  /*
     * Temporary debugging aid. Print contents of the registers before
     * changing them.
   */
  {
    int             i;

    for (i = 0; i < 13; i++)
      printk ("I%d = %02x (old value)\n", i, sscape_read (devc, i));
  }
#endif

  return 1;
}

int
probe_ss_ms_sound (struct address_info *hw_config)
{
  int             i, irq_bits = 0xff;

  if (devc->ok == 0)
    {
      printk ("SoundScape: Invalid initialization order.\n");
      return 0;
    }

  for (i = 0; i < sizeof (valid_interrupts); i++)
    if (hw_config->irq == valid_interrupts[i])
      {
	irq_bits = i;
	break;
      }
#ifdef REVEAL_SPEA
  {
    int             tmp, status = 0;
    int             cc;

    if (!((tmp = sscape_read (devc, GA_HMCTL_REG)) & 0xc0))
      {
	sscape_write (devc, GA_HMCTL_REG, tmp | 0x80);
	for (cc = 0; cc < 200000; ++cc)
	  INB (devc->base + ODIE_ADDR);
      }
  }
#endif

  if (hw_config->irq > 15 || irq_bits == 0xff)
    {
      printk ("SoundScape: Invalid MSS IRQ%d\n", hw_config->irq);
      return 0;
    }

  return ad1848_detect (hw_config->io_base);
}

long
attach_ss_ms_sound (long mem_start, struct address_info *hw_config)
{
  /*
     * This routine configures the SoundScape card for use with the
     * Win Sound System driver. The AD1848 codec interface uses the CD-ROM
     * config registers of the "ODIE".
   */

  int             i, irq_bits = 0xff;

#ifdef EXCLUDE_NATIVE_PCM
  int             prev_devs = num_audiodevs;

#endif

  /*
     * Setup the DMA polarity.
   */
  sscape_write (devc, GA_DMACFG_REG, 0x50);

  /*
     * Take the gate-arry off of the DMA channel.
   */
  sscape_write (devc, GA_DMAB_REG, 0x20);

  /*
     * Init the AD1848 (CD-ROM) config reg.
   */

  for (i = 0; i < sizeof (valid_interrupts); i++)
    if (hw_config->irq == valid_interrupts[i])
      {
	irq_bits = i;
	break;
      }

  sscape_write (devc, GA_CDCFG_REG, 0x89 | (hw_config->dma << 4) |
		(irq_bits << 1));

  if (hw_config->irq == devc->irq)
    printk ("SoundScape: Warning! The WSS mode can't share IRQ with MIDI\n");

  ad1848_init ("SoundScape", hw_config->io_base,
	       hw_config->irq,
	       hw_config->dma,
	       hw_config->dma);

#ifdef EXCLUDE_NATIVE_PCM
  if (num_audiodevs == (prev_devs + 1))		/* The AD1848 driver installed itself */
    audio_devs[prev_devs]->coproc = &sscape_coproc_operations;
#endif
#ifdef SSCAPE_DEBUG5
  /*
     * Temporary debugging aid. Print contents of the registers
     * after the AD1848 device has been initialized.
   */
  {
    int             i;

    for (i = 0; i < 13; i++)
      printk ("I%d = %02x\n", i, sscape_read (devc, i));
  }
#endif

  return mem_start;
}

#endif
