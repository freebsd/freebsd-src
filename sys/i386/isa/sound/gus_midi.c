/*
 * linux/kernel/chr_drv/sound/gus2_midi.c
 * 
 * The low level driver for the GUS Midi Interface.
 * 
 * (C) 1992  Hannu Savolainen (hsavolai@cs.helsinki.fi) See COPYING for further
 * details. Should be distributed with this file.
 */

#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#include "gus_hw.h"

#if !defined(EXCLUDE_GUS) && !defined(EXCLUDE_MIDI)

static int      midi_busy = 0, input_opened = 0;
static int      my_dev;
static int      output_used = 0;
static volatile unsigned char gus_midi_control;

static unsigned char tmp_queue[256];
static volatile int qlen;
static volatile unsigned char qhead, qtail;
extern int      gus_base, gus_irq, gus_dma;

#define GUS_MIDI_STATUS()	INB(u_MidiStatus)

static int
gus_midi_open (int dev, int mode)
{

  if (midi_busy)
    {
      printk ("GUS: Midi busy\n");
      return RET_ERROR (EBUSY);
    }

  OUTB (MIDI_RESET, u_MidiControl);
  gus_delay ();

  gus_midi_control = 0;
  input_opened = 0;

  if (mode == OPEN_READ || mode == OPEN_READWRITE)
    {
      gus_midi_control |= MIDI_ENABLE_RCV;
      input_opened = 1;
    }

  if (mode == OPEN_WRITE || mode == OPEN_READWRITE)
    {
      gus_midi_control |= MIDI_ENABLE_XMIT;
    }

  OUTB (gus_midi_control, u_MidiControl);	/* Enable */

  midi_busy = 1;
  qlen = qhead = qtail = output_used = 0;

  return 0;
}

static int
dump_to_midi (unsigned char midi_byte)
{
  unsigned long   flags;
  int             ok = 0;

  output_used = 1;

  DISABLE_INTR (flags);

  if (GUS_MIDI_STATUS () & MIDI_XMIT_EMPTY)
    {
      ok = 1;
      OUTB (midi_byte, u_MidiData);
    }
  else
    {
      /* Enable Midi xmit interrupts (again) */
      gus_midi_control |= MIDI_ENABLE_XMIT;
      OUTB (gus_midi_control, u_MidiControl);
    }

  RESTORE_INTR (flags);
  return ok;
}

static void
gus_midi_close (int dev)
{
  /* Reset FIFO pointers, disable intrs */

  OUTB (MIDI_RESET, u_MidiControl);
  midi_busy = 0;
}

static int
gus_midi_out (int dev, unsigned char midi_byte)
{

  unsigned long   flags;

  /*
   * Drain the local queue first
   */

  DISABLE_INTR (flags);

  while (qlen && dump_to_midi (tmp_queue[qhead]))
    {
      qlen--;
      qhead++;
    }

  RESTORE_INTR (flags);

  /*
   * Output the byte if the local queue is empty.
   */

  if (!qlen)
    if (dump_to_midi (midi_byte))
      return 1;			/* OK */

  /*
   * Put to the local queue
   */

  if (qlen >= 256)
    return 0;			/* Local queue full */

  DISABLE_INTR (flags);

  tmp_queue[qtail] = midi_byte;
  qlen++;
  qtail++;

  RESTORE_INTR (flags);

  return 1;
}

static int
gus_midi_start_read (int dev)
{
  return 0;
}

static int
gus_midi_end_read (int dev)
{
  return 0;
}

static int
gus_midi_ioctl (int dev, unsigned cmd, unsigned arg)
{
  return RET_ERROR (EINVAL);
}

static void
gus_midi_kick (int dev)
{
}

static int
gus_midi_buffer_status (int dev)
{
  unsigned long   flags;

  if (!output_used)
    return 0;

  DISABLE_INTR (flags);

  if (qlen && dump_to_midi (tmp_queue[qhead]))
    {
      qlen--;
      qhead++;
    }

  RESTORE_INTR (flags);

  return (qlen > 0) | !(GUS_MIDI_STATUS () & MIDI_XMIT_EMPTY);
}

static struct midi_operations gus_midi_operations =
{
  {"Gravis UltraSound", 0},
  gus_midi_open,
  gus_midi_close,
  gus_midi_ioctl,
  gus_midi_out,
  gus_midi_start_read,
  gus_midi_end_read,
  gus_midi_kick,
  NULL,				/* command */
  gus_midi_buffer_status
};

long
gus_midi_init (long mem_start)
{
  OUTB (MIDI_RESET, u_MidiControl);

  my_dev = num_midis;
  midi_devs[num_midis++] = &gus_midi_operations;
  return mem_start;
}

void
gus_midi_interrupt (int dummy)
{
  unsigned char   stat, data;
  unsigned long   flags;

  DISABLE_INTR (flags);

  stat = GUS_MIDI_STATUS ();

  if (stat & MIDI_RCV_FULL)
    {
      data = INB (u_MidiData);
      if (input_opened)
	sequencer_midi_input (my_dev, data);
    }

  if (stat & MIDI_XMIT_EMPTY)
    {
      while (qlen && dump_to_midi (tmp_queue[qhead]))
	{
	  qlen--;
	  qhead++;
	}

      if (!qlen)
	{
	  /* Disable Midi output interrupts, since no data in the buffer */
	  gus_midi_control &= ~MIDI_ENABLE_XMIT;
	  OUTB (gus_midi_control, u_MidiControl);
	}
    }

  if (stat & MIDI_FRAME_ERR)
    printk ("Midi framing error\n");
  if (stat & MIDI_OVERRUN && input_opened)
    printk ("GUS: Midi input overrun\n");

  RESTORE_INTR (flags);
}

#endif

#endif
