/*
 * sound/mpu401.c
 *
 * The low level driver for Roland MPU-401 compatible Midi cards.
 *
 * This version supports just the DUMB UART mode.
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

#if !defined(EXCLUDE_MPU401) && !defined(EXCLUDE_MIDI)

#define	DATAPORT   (mpu401_base)/* MPU-401 Data I/O Port on IBM */
#define	COMDPORT   (mpu401_base+1)	/* MPU-401 Command Port on IBM */
#define	STATPORT   (mpu401_base+1)	/* MPU-401 Status Port on IBM */

#define mpu401_status()		INB(STATPORT)
#define input_avail()		(!(mpu401_status()&INPUT_AVAIL))
#define output_ready()		(!(mpu401_status()&OUTPUT_READY))
#define mpu401_cmd(cmd)		OUTB(cmd, COMDPORT)
#define mpu401_read()		INB(DATAPORT)
#define mpu401_write(byte)	OUTB(byte, DATAPORT)

#define	OUTPUT_READY	0x40	/* Mask for Data Read Redy Bit */
#define	INPUT_AVAIL	0x80	/* Mask for Data Send Ready Bit */
#define	MPU_ACK		0xFE	/* MPU-401 Acknowledge Response */
#define	MPU_RESET	0xFF	/* MPU-401 Total Reset Command */
#define	UART_MODE_ON	0x3F	/* MPU-401 "Dumb UART Mode" */

static int      mpu401_opened = 0;
static int      mpu401_base = 0x330;
static int      mpu401_irq;
static int      mpu401_detected = 0;
static int      my_dev;

static int      reset_mpu401 (void);
static void     (*midi_input_intr) (int dev, unsigned char data);

void
mpuintr (int unit)
{
  while (input_avail ())
    {
      unsigned char   c = mpu401_read ();

      if (mpu401_opened & OPEN_READ)
	midi_input_intr (my_dev, c);
    }
}

static int
mpu401_open (int dev, int mode,
	     void            (*input) (int dev, unsigned char data),
	     void            (*output) (int dev)
)
{
  if (mpu401_opened)
    {
      printk ("MPU-401: Midi busy\n");
      return RET_ERROR (EBUSY);
    }

  mpuintr (0);

  midi_input_intr = input;
  mpu401_opened = mode;

  return 0;
}

static void
mpu401_close (int dev)
{
  mpu401_opened = 0;
}

static int
mpu401_out (int dev, unsigned char midi_byte)
{
  int             timeout;
  unsigned long   flags;

  /*
   * Test for input since pending input seems to block the output.
   */

  DISABLE_INTR (flags);

  if (input_avail ())
    mpuintr (0);

  RESTORE_INTR (flags);

  /*
   * Sometimes it takes about 13000 loops before the output becomes ready
   * (After reset). Normally it takes just about 10 loops.
   */

  for (timeout = 30000; timeout > 0 && !output_ready (); timeout--);	/* Wait */

  if (!output_ready ())
    {
      printk ("MPU-401: Timeout\n");
      return 0;
    }

  mpu401_write (midi_byte);
  return 1;
}

static int
mpu401_command (int dev, unsigned char midi_byte)
{
  return 1;
}

static int
mpu401_start_read (int dev)
{
  return 0;
}

static int
mpu401_end_read (int dev)
{
  return 0;
}

static int
mpu401_ioctl (int dev, unsigned cmd, unsigned arg)
{
  return RET_ERROR (EINVAL);
}

static void
mpu401_kick (int dev)
{
}

static int
mpu401_buffer_status (int dev)
{
  return 0;			/* No data in buffers */
}

static struct midi_operations mpu401_operations =
{
  {"MPU-401", 0, 0, SNDCARD_MPU401},
  mpu401_open,
  mpu401_close,
  mpu401_ioctl,
  mpu401_out,
  mpu401_start_read,
  mpu401_end_read,
  mpu401_kick,
  mpu401_command,
  mpu401_buffer_status
};


long
attach_mpu401 (long mem_start, struct address_info *hw_config)
{
  int             ok, timeout;
  unsigned long   flags;

  mpu401_base = hw_config->io_base;
  mpu401_irq = hw_config->irq;

  if (!mpu401_detected)
    return RET_ERROR (EIO);

  DISABLE_INTR (flags);
  for (timeout = 30000; timeout < 0 && !output_ready (); timeout--);	/* Wait */
  mpu401_cmd (UART_MODE_ON);

  ok = 0;
  for (timeout = 50000; timeout > 0 && !ok; timeout--)
    if (input_avail ())
      if (mpu401_read () == MPU_ACK)
	ok = 1;

  RESTORE_INTR (flags);

#ifdef __FreeBSD__
  printk ("snd5: <Roland MPU-401>");
#else
  printk (" <Roland MPU-401>");
#endif

  my_dev = num_midis;
  mpu401_dev = num_midis;
  midi_devs[num_midis++] = &mpu401_operations;
  return mem_start;
}

static int
reset_mpu401 (void)
{
  unsigned long   flags;
  int             ok, timeout, n;

  /*
   * Send the RESET command. Try again if no success at the first time.
   */

  ok = 0;

  DISABLE_INTR (flags);

  for (n = 0; n < 2 && !ok; n++)
    {
      for (timeout = 30000; timeout < 0 && !output_ready (); timeout--);	/* Wait */
      mpu401_cmd (MPU_RESET);	/* Send MPU-401 RESET Command */

      /*
       * Wait at least 25 msec. This method is not accurate so let's make the
       * loop bit longer. Cannot sleep since this is called during boot.
       */

      for (timeout = 50000; timeout > 0 && !ok; timeout--)
	if (input_avail ())
	  if (mpu401_read () == MPU_ACK)
	    ok = 1;

    }

  mpu401_opened = 0;
  if (ok)
    mpuintr (0);		/* Flush input before enabling interrupts */

  RESTORE_INTR (flags);

  return ok;
}


int
probe_mpu401 (struct address_info *hw_config)
{
  int             ok = 0;

  mpu401_base = hw_config->io_base;
  mpu401_irq = hw_config->irq;

  if (snd_set_irq_handler (mpu401_irq, mpuintr) < 0)
    return 0;

  ok = reset_mpu401 ();

  mpu401_detected = ok;
  return ok;
}

#endif

#endif
