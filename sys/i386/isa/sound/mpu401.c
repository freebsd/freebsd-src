/*
 * linux/kernel/chr_drv/sound/mpu401.c
 * 
 * The low level driver for Roland MPU-401 compatible Midi cards.
 * 
 * This version supports just the DUMB UART mode.
 * 
 * (C) 1993  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
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

static void
mpu401_input_loop (void)
{
  int             count;

  count = 10;

  while (count)			/* Not timed out */
    if (input_avail ())
      {
	unsigned char   c = mpu401_read ();

	count = 100;

	if (mpu401_opened & OPEN_READ)
	  sequencer_midi_input (my_dev, c);
      }
    else
      while (!input_avail () && count)
	count--;
}

void
mpuintr (int unit)
{
  unsigned char   c;

  if (input_avail ())
    mpu401_input_loop ();
}

/*
 * It looks like there is no input interrupts in the UART mode. Let's try
 * polling.
 */

static void
poll_mpu401 (unsigned long dummy)
{
  unsigned long   flags;

  static struct timer_list mpu401_timer =
  {NULL, 0, 0, poll_mpu401};

  if (!(mpu401_opened & OPEN_READ))
    return;			/* No longer required */

  DISABLE_INTR (flags);

  if (input_avail ())
    mpu401_input_loop ();

  mpu401_timer.expires = 1;
  add_timer (&mpu401_timer);	/* Come back later */

  RESTORE_INTR (flags);
}

static int
set_mpu401_irq (int interrupt_level)
{
  int             retcode;

#ifdef linux
  struct sigaction sa;

  sa.sa_handler = mpuintr;

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
      printk ("MPU-401: IRQ%d already in use\n", interrupt_level);
    }

#else
  /* #  error Unimplemented for this OS	 */
#endif
  return retcode;
}

static int
mpu401_open (int dev, int mode)
{
  if (mpu401_opened)
    {
      printk ("MPU-401: Midi busy\n");
      return RET_ERROR (EBUSY);
    }

  mpu401_input_loop ();

  mpu401_opened = mode;
  poll_mpu401 (0);		/* Enable input polling */

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
    mpu401_input_loop ();

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
  {"MPU-401", 0},
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

  printk ("snd5: <Roland MPU-401>");

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
   * Send the RESET command. Try twice if no success at the first time.
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
    mpu401_input_loop ();	/* Flush input before enabling interrupts */

  RESTORE_INTR (flags);

  return ok;
}


int
probe_mpu401 (struct address_info *hw_config)
{
  int             ok = 0;

  mpu401_base = hw_config->io_base;
  mpu401_irq = hw_config->irq;

  if (set_mpu401_irq (mpu401_irq) < 0)
    return 0;

  ok = reset_mpu401 ();

  mpu401_detected = ok;
  return ok;
}

#endif

#endif
