/*
 * sound/ad1848.c
 *
 * The low level driver for the AD1848/CS4248 codec chip which
 * is used for example in the MS Sound System.
 *
 * The CS4231 which is used in the GUS MAX and some other cards is
 * upwards compatible with AD1848 and this driver is able to drive it.
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
 * ad1848.c,v 1.3 1994/10/01 05:46:01 davidg Exp
 */

#define DEB(x)
#define DEB1(x)
#include "sound_config.h"

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_AD1848)

#define IMODE_NONE		0
#define IMODE_OUTPUT		1
#define IMODE_INPUT		2
#define IMODE_INIT		3
#define IMODE_MIDI		4

typedef struct
{
  int             base;
  int             irq;
  int             dma_capture, dma_playback;
  unsigned char   MCE_bit;

  int             speed;
  unsigned char   speed_bits;
  int             channels;
  int             audio_format;
  unsigned char   format_bits;

  int             xfer_count;
  int             irq_mode;
  int             intr_active;
  int             opened;
  char           *chip_name;
  int             mode;
}

ad1848_info;

static int      nr_ad1848_devs = 0;
static char     irq2dev[16] =
{-1, -1, -1, -1, -1, -1, -1, -1,
 -1, -1, -1, -1, -1, -1, -1, -1};

static int      ad_format_mask[3 /*devc->mode*/ ] =
{
  0,
  AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW,
  AFMT_U8 | AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW | AFMT_U16_LE | AFMT_IMA_ADPCM
};

static ad1848_info dev_info[MAX_AUDIO_DEV];

#define io_Index_Addr(d)	((d)->base)
#define io_Indexed_Data(d)	((d)->base+1)
#define io_Status(d)		((d)->base+2)
#define io_Polled_IO(d)		((d)->base+3)

static int      ad1848_open (int dev, int mode);
static void     ad1848_close (int dev);
static int      ad1848_ioctl (int dev, unsigned int cmd, unsigned int arg, int local);
static void     ad1848_output_block (int dev, unsigned long buf, int count, int intrflag, int dma_restart);
static void     ad1848_start_input (int dev, unsigned long buf, int count, int intrflag, int dma_restart);
static int      ad1848_prepare_for_IO (int dev, int bsize, int bcount);
static void     ad1848_reset (int dev);
static void     ad1848_halt (int dev);

static int
ad_read (ad1848_info * devc, int reg)
{
  unsigned long   flags;
  int             x;

  DISABLE_INTR (flags);
  OUTB ((unsigned char) (reg & 0xff) | devc->MCE_bit, io_Index_Addr (devc));
  x = INB (io_Indexed_Data (devc));
  /*  printk("(%02x<-%02x) ", reg|devc->MCE_bit, x); */
  RESTORE_INTR (flags);

  return x;
}

static void
ad_write (ad1848_info * devc, int reg, int data)
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  OUTB ((unsigned char) (reg & 0xff) | devc->MCE_bit, io_Index_Addr (devc));
  OUTB ((unsigned char) (data & 0xff), io_Indexed_Data (devc));
  /* printk("(%02x->%02x) ", reg|devc->MCE_bit, data); */
  RESTORE_INTR (flags);
}

static void
ad_set_MCE (ad1848_info * devc, int state)
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  if (state)
    devc->MCE_bit = 0x40;
  else
    devc->MCE_bit = 0x00;
  OUTB (devc->MCE_bit, io_Index_Addr (devc));
  RESTORE_INTR (flags);
}

static void
wait_for_calibration (ad1848_info * devc)
{
  int             timeout = 0;

  /*
 * Wait until the auto calibration process has finished.
 *
 * 1)	Wait until the chip becomes ready (reads don't return 0x80).
 * 2)	Wait until the ACI bit of I11 gets on and then off.
 */

  timeout = 100000;
  while (timeout > 0 && INB (devc->base) == 0x80)
    timeout--;
  if (INB (devc->base) == 0x80)
    printk ("ad1848: Auto calibration timed out(1).\n");

  timeout = 100000;
  while (timeout > 0 && !(ad_read (devc, 11) & 0x20))
    timeout--;
  if (!(ad_read (devc, 11) & 0x20))
    printk ("ad1848: Auto calibration timed out(2).\n");

  timeout = 100000;
  while (timeout > 0 && ad_read (devc, 11) & 0x20)
    timeout--;
  if (ad_read (devc, 11) & 0x20)
    printk ("ad1848: Auto calibration timed out(3).\n");
}

static struct audio_operations ad1848_pcm_operations[MAX_AUDIO_DEV] =
{
  {
    "Generic AD1848 codec",
    DMA_AUTOMODE,
    AFMT_U8,			/* Will be set later */
    NULL,
    ad1848_open,
    ad1848_close,
    ad1848_output_block,
    ad1848_start_input,
    ad1848_ioctl,
    ad1848_prepare_for_IO,
    ad1848_prepare_for_IO,
    ad1848_reset,
    ad1848_halt,
    NULL,
    NULL
  }};

static int
ad1848_open (int dev, int mode)
{
  int             err;
  ad1848_info    *devc = NULL;
  unsigned long   flags;

  DEB (printk ("ad1848_open(int mode = %X)\n", mode));

  if (dev < 0 || dev >= num_audiodevs)
    return RET_ERROR (ENXIO);

  devc = (ad1848_info *) audio_devs[dev]->devc;

  DISABLE_INTR (flags);
  if (devc->opened)
    {
      RESTORE_INTR (flags);
      printk ("ad1848: Already opened\n");
      return RET_ERROR (EBUSY);
    }

  if (devc->irq)		/* Not managed by another driver */
    if ((err = snd_set_irq_handler (devc->irq, adintr)) < 0)
      {
	printk ("ad1848: IRQ in use\n");
	RESTORE_INTR (flags);
	return err;
      }

  if (DMAbuf_open_dma (dev) < 0)
    {
      RESTORE_INTR (flags);
      printk ("ad1848: DMA in use\n");
      return RET_ERROR (EBUSY);
    }

  devc->intr_active = 0;
  devc->opened = 1;
  RESTORE_INTR (flags);

  return 0;
}

static void
ad1848_close (int dev)
{
  unsigned long   flags;
  ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

  DEB (printk ("ad1848_close(void)\n"));

  DISABLE_INTR (flags);

  devc->intr_active = 0;
  if (devc->irq)		/* Not managed by another driver */
    snd_release_irq (devc->irq);
  ad1848_reset (dev);
  DMAbuf_close_dma (dev);
  devc->opened = 0;

  RESTORE_INTR (flags);
}

static int
set_speed (ad1848_info * devc, int arg)
{
  /*
 * The sampling speed is encoded in the least significant nibble of I8. The
 * LSB selects the clock source (0=24.576 MHz, 1=16.9344 Mhz) and other
 * three bits select the divisor (indirectly):
 *
 * The available speeds are in the following table. Keep the speeds in
 * the increasing order.
 */
  typedef struct
    {
      int             speed;
      unsigned char   bits;
    }
  speed_struct;

  static speed_struct speed_table[] =
  {
    {5510, (0 << 1) | 1},
    {5510, (0 << 1) | 1},
    {6620, (7 << 1) | 1},
    {8000, (0 << 1) | 0},
    {9600, (7 << 1) | 0},
    {11025, (1 << 1) | 1},
    {16000, (1 << 1) | 0},
    {18900, (2 << 1) | 1},
    {22050, (3 << 1) | 1},
    {27420, (2 << 1) | 0},
    {32000, (3 << 1) | 0},
    {33075, (6 << 1) | 1},
    {37800, (4 << 1) | 1},
    {44100, (5 << 1) | 1},
    {48000, (6 << 1) | 0}
  };

  int             i, n, selected = -1;

  n = sizeof (speed_table) / sizeof (speed_struct);

  if (arg < speed_table[0].speed)
    selected = 0;
  if (arg > speed_table[n - 1].speed)
    selected = n - 1;

  for (i = 1 /*really*/ ; selected == -1 && i < n; i++)
    if (speed_table[i].speed == arg)
      selected = i;
    else if (speed_table[i].speed > arg)
      {
	int             diff1, diff2;

	diff1 = arg - speed_table[i - 1].speed;
	diff2 = speed_table[i].speed - arg;

	if (diff1 < diff2)
	  selected = i - 1;
	else
	  selected = i;
      }

  if (selected == -1)
    {
      printk ("ad1848: Can't find speed???\n");
      selected = 3;
    }

  devc->speed = speed_table[selected].speed;
  devc->speed_bits = speed_table[selected].bits;
  return devc->speed;
}

static int
set_channels (ad1848_info * devc, int arg)
{
  if (arg != 1 && arg != 2)
    return devc->channels;

  devc->channels = arg;
  return arg;
}

static int
set_format (ad1848_info * devc, int arg)
{

  static struct format_tbl
    {
      int             format;
      unsigned char   bits;
    }
  format2bits     [] =
  {
    {
      0, 0
    }
    ,
    {
      AFMT_MU_LAW, 1
    }
    ,
    {
      AFMT_A_LAW, 3
    }
    ,
    {
      AFMT_IMA_ADPCM, 5
    }
    ,
    {
      AFMT_U8, 0
    }
    ,
    {
      AFMT_S16_LE, 2
    }
    ,
    {
      AFMT_S16_BE, 6
    }
    ,
    {
      AFMT_S8, 0
    }
    ,
    {
      AFMT_U16_LE, 0
    }
    ,
    {
      AFMT_U16_BE, 0
    }
  };
  int             i, n = sizeof (format2bits) / sizeof (struct format_tbl);

  if (!(arg & ad_format_mask[devc->mode]))
    arg = AFMT_U8;

  devc->audio_format = arg;

  for (i = 0; i < n; i++)
    if (format2bits[i].format == arg)
      {
	if ((devc->format_bits = format2bits[i].bits) == 0)
	  return devc->audio_format = AFMT_U8;	/* Was not supported */

	return arg;
      }

  /* Still hanging here. Something must be terribly wrong */
  devc->format_bits = 0;
  return devc->audio_format = AFMT_U8;
}

static int
ad1848_ioctl (int dev, unsigned int cmd, unsigned int arg, int local)
{
  ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

  switch (cmd)
    {
    case SOUND_PCM_WRITE_RATE:
      if (local)
	return set_speed (devc, arg);
      return IOCTL_OUT (arg, set_speed (devc, IOCTL_IN (arg)));

    case SOUND_PCM_READ_RATE:
      if (local)
	return devc->speed;
      return IOCTL_OUT (arg, devc->speed);

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
	return devc->channels;
      return IOCTL_OUT (arg, devc->channels);

    case SNDCTL_DSP_SAMPLESIZE:
      if (local)
	return set_format (devc, arg);
      return IOCTL_OUT (arg, set_format (devc, IOCTL_IN (arg)));

    case SOUND_PCM_READ_BITS:
      if (local)
	return devc->audio_format;
      return IOCTL_OUT (arg, devc->audio_format);

    default:;
    }
  return RET_ERROR (EINVAL);
}

static void
ad1848_output_block (int dev, unsigned long buf, int count, int intrflag, int dma_restart)
{
  unsigned long   flags, cnt;
  ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

  cnt = count;

  if (devc->audio_format == AFMT_IMA_ADPCM)
    {
      cnt /= 4;
    }
  else
    {
      if (devc->audio_format & (AFMT_S16_LE | AFMT_S16_BE))	/* 16 bit data */
	cnt >>= 1;
    }
  if (devc->channels > 1)
    cnt >>= 1;
  cnt--;

  if (audio_devs[dev]->flags & DMA_AUTOMODE &&
      intrflag &&
      cnt == devc->xfer_count)
    {
      devc->irq_mode = IMODE_OUTPUT;
      devc->intr_active = 1;
      return;			/*
				 * Auto DMA mode on. No need to react
				 */
    }
  DISABLE_INTR (flags);

  if (dma_restart)
    {
      ad1848_halt (dev);
      DMAbuf_start_dma (dev, buf, count, DMA_MODE_WRITE);
    }

  ad_set_MCE (devc, 1);

  ad_write (devc, 15, (unsigned char) (cnt & 0xff));
  ad_write (devc, 14, (unsigned char) ((cnt >> 8) & 0xff));


  ad_write (devc, 9, 0x0d);	/*
				 * Playback enable, single DMA channel mode,
				 * auto calibration on.
				 */

  ad_set_MCE (devc, 0);		/*
				 * Starts the calibration process and
				 * enters playback mode after it.
				 */
  wait_for_calibration (devc);

  devc->xfer_count = cnt;
  devc->irq_mode = IMODE_OUTPUT;
  devc->intr_active = 1;
  RESTORE_INTR (flags);
}

static void
ad1848_start_input (int dev, unsigned long buf, int count, int intrflag, int dma_restart)
{
  unsigned long   flags, cnt;
  ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

  /* int             count_reg = (devc->mode == 1) ? 14 : 30; */

  cnt = count;
  if (devc->audio_format == AFMT_IMA_ADPCM)
    {
      cnt /= 4;
    }
  else
    {
      if (devc->audio_format & (AFMT_S16_LE | AFMT_S16_BE))	/* 16 bit data */
	cnt >>= 1;
    }
  if (devc->channels > 1)
    cnt >>= 1;
  cnt--;

  if (audio_devs[dev]->flags & DMA_AUTOMODE &&
      intrflag &&
      cnt == devc->xfer_count)
    {
      devc->irq_mode = IMODE_INPUT;
      devc->intr_active = 1;
      return;			/*
				 * Auto DMA mode on. No need to react
				 */
    }
  DISABLE_INTR (flags);

  if (dma_restart)
    {
      ad1848_halt (dev);
      DMAbuf_start_dma (dev, buf, count, DMA_MODE_READ);
    }

  ad_set_MCE (devc, 1);
#if 0
  ad_write (devc, count_reg + 1, (unsigned char) (cnt & 0xff));
  ad_write (devc, count_reg, (unsigned char) ((cnt >> 8) & 0xff));
#else
  ad_write (devc, 15, (unsigned char) (cnt & 0xff));
  ad_write (devc, 14, (unsigned char) ((cnt >> 8) & 0xff));
  if (devc->mode == 2)
    {
      ad_write (devc, 31, (unsigned char) (cnt & 0xff));
      ad_write (devc, 32, (unsigned char) ((cnt >> 8) & 0xff));
    }
#endif

  ad_write (devc, 9, 0x0e);	/*
				 * Capture enable, single DMA channel mode,
				 * auto calibration on.
				 */

  ad_set_MCE (devc, 0);		/*
				 * Starts the calibration process and
				 * enters playback mode after it.
				 */
  wait_for_calibration (devc);

  devc->xfer_count = cnt;
  devc->irq_mode = IMODE_INPUT;
  devc->intr_active = 1;
  RESTORE_INTR (flags);
}

static int
ad1848_prepare_for_IO (int dev, int bsize, int bcount)
{
  int             timeout;
  unsigned char   fs;
  unsigned long   flags;
  ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

  DISABLE_INTR (flags);
  ad_set_MCE (devc, 1);		/* Enables changes to the format select reg */
  fs = devc->speed_bits | (devc->format_bits << 5);

  if (devc->channels > 1)
    fs |= 0x10;

  ad_write (devc, 8, fs);
  /*
   * Write to I8 starts resynchronization. Wait until it completes.
   */
  timeout = 10000;
  while (timeout > 0 && INB (devc->base) == 0x80)
    timeout--;

  ad_set_MCE (devc, 0);		/*
				 * Starts the calibration process and
				 * enters playback mode after it.
				 */
  wait_for_calibration (devc);
  RESTORE_INTR (flags);

  /*
 * If mode == 2 (CS4231), set I28 also. It's the capture format register.
 */
  if (devc->mode == 2)
    {
      ad_set_MCE (devc, 1);
      ad_write (devc, 28, fs);

      /*
   * Write to I28 starts resynchronization. Wait until it completes.
   */
      timeout = 10000;
      while (timeout > 0 && INB (devc->base) == 0x80)
	timeout--;

      ad_set_MCE (devc, 0);	/*
				 * Starts the calibration process and
				 * enters playback mode after it.
				 */
      wait_for_calibration (devc);
      RESTORE_INTR (flags);
    }
  devc->xfer_count = 0;
  return 0;
}

static void
ad1848_reset (int dev)
{
  ad1848_halt (dev);
}

static void
ad1848_halt (int dev)
{
  ad1848_info    *devc = (ad1848_info *) audio_devs[dev]->devc;

  ad_write (devc, 9, 0);	/* Clear the PEN and CEN bits (among others) */
  OUTB (0, io_Status (devc));	/* Clear interrupt status */
}

int
ad1848_detect (int io_base)
{

#define AUDIO_DDB(x)	x

  unsigned char   tmp;
  int             i;
  ad1848_info    *devc = &dev_info[nr_ad1848_devs];
  unsigned char   tmp1 = 0xff, tmp2 = 0xff;

  if (nr_ad1848_devs >= MAX_AUDIO_DEV)
    return 0;

  devc->base = io_base;
  devc->MCE_bit = 0x40;
  devc->irq = 0;
  devc->dma_capture = 0;
  devc->dma_playback = 0;
  devc->opened = 0;
  devc->chip_name = "AD1848";
  devc->mode = 1;		/* MODE1 = original AD1848 */

  /*
 * Check that the I/O address is in use.
 *
 * The bit 0x80 of the base I/O port is known to be 0 after the
 * chip has performed it's power on initialization. Just assume
 * this has happened before the OS is starting.
 *
 * If the I/O address is unused, it typically returns 0xff.
 */

  if ((INB (devc->base) & 0x80) != 0x00)	/* Not a AD1884 */
    {
      AUDIO_DDB (printk ("ad_detect_A\n"));
      return 0;
    }

  /*
 * Test if it's possible to change contents of the indirect registers.
 * Registers 0 and 1 are ADC volume registers. The bit 0x10 is read only
 * so try to avoid using it.
*/

  ad_write (devc, 0, 0xaa);
  ad_write (devc, 1, 0x45);	/* 0x55 with bit 0x10 clear */

  if ((tmp1 = ad_read (devc, 0)) != 0xaa || (tmp2 = ad_read (devc, 1)) != 0x45)
    {
      AUDIO_DDB (printk ("ad_detect_B (%x/%x)\n", tmp1, tmp2));
      return 0;
    }

  ad_write (devc, 0, 0x45);
  ad_write (devc, 1, 0xaa);

  if ((tmp1 = ad_read (devc, 0)) != 0x45 || (tmp2 = ad_read (devc, 1)) != 0xaa)
    {
      AUDIO_DDB (printk ("ad_detect_C (%x/%x)\n", tmp1, tmp2));
      return 0;
    }

  /*
 * The indirect register I12 has some read only bits. Lets
 * try to change them.
 */

  tmp = ad_read (devc, 12);
  ad_write (devc, 12, (~tmp) & 0x0f);

  if ((tmp & 0x0f) != ((tmp1 = ad_read (devc, 12)) & 0x0f))
    {
      AUDIO_DDB (printk ("ad_detect_D (%x)\n", tmp1));
      return 0;
    }

  /*
 * NOTE! Last 4 bits of the reg I12 tell the chip revision.
 *	 0x01=RevB and 0x0A=RevC.
 */

  /*
 * The original AD1848/CS4248 has just 15 indirect registers. This means
 * that I0 and I16 should return the same value (etc.).
 * Ensure that the Mode2 enable bit of I12 is 0. Otherwise this test fails
 * with CS4231.
 */

  ad_write (devc, 12, 0);	/* Mode2=disabled */

  for (i = 0; i < 16; i++)
    if ((tmp1 = ad_read (devc, i)) != (tmp2 = ad_read (devc, i + 16)))
      {
	AUDIO_DDB (printk ("ad_detect_F(%d/%x/%x)\n", i, tmp1, tmp2));
	return 0;
      }

  /*
 * Try to switch the chip to mode2 (CS4231) by setting the MODE2 bit (0x40).
 * The bit 0x80 is always 1 in CS4248 and CS4231.
 */

  ad_write (devc, 12, 0x40);	/* Set mode2, clear 0x80 */

  tmp1 = ad_read (devc, 12);
  if (tmp1 & 0x80)
    devc->chip_name = "CS4248";

  if ((tmp1 & 0xc0) == (0x80 | 0x40))
    {
      /*
	 *	CS4231 detected - is it?
	 *
	 *	Verify that setting I0 doesn't change I16.
	 */
      ad_write (devc, 16, 0);	/* Set I16 to known value */

      ad_write (devc, 0, 0x45);
      if ((tmp1 = ad_read (devc, 16)) != 0x45)	/* No change -> CS4231? */
	{

	  ad_write (devc, 0, 0xaa);
	  if ((tmp1 = ad_read (devc, 16)) == 0xaa)	/* Rotten bits? */
	    {
	      AUDIO_DDB (printk ("ad_detect_H(%x)\n", tmp1));
	      return 0;
	    }

	  /*
	 *	It's a CS4231 - So what!
	 *	(Mode2 will be supported later)
	 */
	  devc->chip_name = "CS4231";
	  devc->mode = 2;
	}
    }

  return 1;
}

void
ad1848_init (char *name, int io_base, int irq, int dma_playback, int dma_capture)
{
  /*
 * NOTE! If irq < 0, there is another driver which has allocated the IRQ
 *	 so that this driver doesn't need to allocate/deallocate it.
 *	 The actually used IRQ is ABS(irq).
 */

  /*
 * Initial values for the indirect registers of CS4248/AD1848.
 */
  static int      init_values[] =
  {
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
    0x00, 0x08, 0x02, 0x00, 0xca, 0x00, 0x00, 0x00
  };
  int             i, my_dev;
  ad1848_info    *devc = &dev_info[nr_ad1848_devs];

  if (!ad1848_detect (io_base))
    return;

  devc->irq = (irq > 0) ? irq : 0;
  devc->dma_capture = dma_playback;
  devc->dma_playback = dma_capture;
  devc->opened = 0;

  if (nr_ad1848_devs != 0)
    {
      memcpy ((char *) &ad1848_pcm_operations[nr_ad1848_devs],
	      (char *) &ad1848_pcm_operations[0],
	      sizeof (struct audio_operations));
    }

  for (i = 0; i < 16; i++)
    ad_write (devc, i, init_values[i]);

  OUTB (0, io_Status (devc));	/* Clear pending interrupts */

#ifndef SCO
  sprintf (ad1848_pcm_operations[nr_ad1848_devs].name,
	   "%s (%s)", name, devc->chip_name);
#endif

  if (irq > 0)
    printk (" <%s>", ad1848_pcm_operations[nr_ad1848_devs].name);

  if (num_audiodevs < MAX_AUDIO_DEV)
    {
      audio_devs[my_dev = num_audiodevs++] = &ad1848_pcm_operations[nr_ad1848_devs];
      if (irq > 0)
	irq2dev[irq] = my_dev;
      else if (irq < 0)
	irq2dev[-irq] = my_dev;

      audio_devs[my_dev]->dmachan = dma_playback;
      audio_devs[my_dev]->buffcount = 1;
      audio_devs[my_dev]->buffsize = DSP_BUFFSIZE * 2;
      audio_devs[my_dev]->devc = devc;
      audio_devs[my_dev]->format_mask = ad_format_mask[devc->mode];
      nr_ad1848_devs++;
    }
  else
    printk ("AD1848: Too many PCM devices available\n");
}

void
adintr (int irq)
{
  unsigned char   status;
  ad1848_info    *devc;
  int             dev;

  if (irq < 0 || irq > 15)
    return;			/* Bogus irq */
  dev = irq2dev[irq];
  if (dev < 0 || dev >= num_audiodevs)
    return;			/* Bogus dev */

  devc = (ad1848_info *) audio_devs[dev]->devc;
  status = INB (io_Status (devc));

  if (status & 0x01)
    {
      if (devc->opened && devc->irq_mode == IMODE_OUTPUT)
	{
	  DMAbuf_outputintr (dev, 1);
	}

      if (devc->opened && devc->irq_mode == IMODE_INPUT)
	DMAbuf_inputintr (dev);
    }

  OUTB (0, io_Status (devc));	/* Clear interrupt status */
}

#endif
/*
 * Some extra code for the MS Sound System
 */
#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_MSS)

int
probe_ms_sound (struct address_info *hw_config)
{
  if ((INB (hw_config->io_base + 3) & 0x04) == 0)
    return 0;			/* WSS ID test failed */

  if (hw_config->irq > 11)
    return 0;

  if (hw_config->dma != 0 && hw_config->dma != 1 && hw_config->dma != 3)
	return 0;

  return ad1848_detect (hw_config->io_base + 4);
}

long
attach_ms_sound (long mem_start, struct address_info *hw_config)
{
  static unsigned char interrupt_bits[12] =
  {-1, -1, -1, -1, -1, -1, -1, 0x08, -1, 0x10, 0x18, 0x20};
  char            bits;

  static unsigned char dma_bits[4] = {1, 2, 0, 3};

  int             config_port = hw_config->io_base + 0, version_port = hw_config->io_base + 3;

  if (!ad1848_detect (hw_config->io_base + 4))
    return mem_start;

  /*
 * Set the IRQ and DMA addresses.
 */

  bits = interrupt_bits[hw_config->irq];
  if (bits == -1)
    return mem_start;

  OUTB (bits | 0x40, config_port);	/* Verify IRQ (I guess) */
  if ((INB (version_port) & 0x40) == 0)
    printk ("[IRQ?]");

  OUTB (bits | dma_bits[hw_config->dma], config_port);	/* Write IRQ+DMA setup */

  ad1848_init ("MS Sound System", hw_config->io_base + 4,
	       hw_config->irq,
	       hw_config->dma,
	       hw_config->dma);
  return mem_start;
}

#endif
