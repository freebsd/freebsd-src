/*
 * sound/sound_timer.c
 *
 * Timer for the level 2 interface of the /dev/sequencer. Uses the
 * 80 and 320 usec timers of OPL-3 (PAS16 only) and GUS.
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

#define SEQUENCER_C
#include "sound_config.h"

#ifdef CONFIGURE_SOUNDCARD

#if !defined(EXCLUDE_SEQUENCER) && (!defined(EXCLUDE_GUS) || (!defined(EXCLUDE_PAS) && !defined(EXCLUDE_YM3812)))

static volatile int initialized = 0, opened = 0, tmr_running = 0;
static volatile time_t tmr_offs, tmr_ctr;
static volatile unsigned long ticks_offs;
static volatile int curr_tempo, curr_timebase;
static volatile unsigned long curr_ticks;
static volatile unsigned long next_event_time;
static unsigned long prev_event_time;
static volatile int select_addr, data_addr;
static volatile int curr_timer = 0;
static volatile unsigned long usecs_per_tmr;	/* Length of the current interval */


static void
timer_command (unsigned int addr, unsigned int val)
{
  int             i;

  OUTB ((unsigned char) (addr & 0xff), select_addr);

  for (i = 0; i < 2; i++)
    INB (select_addr);

  OUTB ((unsigned char) (val & 0xff), data_addr);

  for (i = 0; i < 2; i++)
    INB (select_addr);
}

static void
arm_timer (int timer, unsigned int interval)
{

  curr_timer = timer;

  if (timer == 1)
    {
      gus_write8 (0x46, 256 - interval);	/* Set counter for timer 1 */
      gus_write8 (0x45, 0x04);	/* Enable timer 1 IRQ */
      timer_command (0x04, 0x01);	/* Start timer 1 */
    }
  else
    {
      gus_write8 (0x47, 256 - interval);	/* Set counter for timer 2 */
      gus_write8 (0x45, 0x08);	/* Enable timer 2 IRQ */
      timer_command (0x04, 0x02);	/* Start timer 2 */
    }
}

static unsigned long
tmr2ticks (int tmr_value)
{
  /*
     *    Convert timer ticks to MIDI ticks
   */

  unsigned long   tmp;
  unsigned long   scale;

  tmp = tmr_value * usecs_per_tmr;	/* Convert to usecs */

  scale = (60 * 1000000) / (curr_tempo * curr_timebase);	/* usecs per MIDI tick */

  return (tmp + (scale / 2)) / scale;
}

static void
reprogram_timer (void)
{
  unsigned long   usecs_per_tick;
  int             timer_no, resolution;
  int             divisor;

  usecs_per_tick = (60 * 1000000) / (curr_tempo * curr_timebase);

  /*
     * Don't kill the system by setting too high timer rate
   */
  if (usecs_per_tick < 2000)
    usecs_per_tick = 2000;

  if (usecs_per_tick > (256 * 80))
    {
      timer_no = 2;
      resolution = 320;		/* usec */
    }
  else
    {
      timer_no = 1;
      resolution = 80;		/* usec */
    }

  divisor = (usecs_per_tick + (resolution / 2)) / resolution;
  usecs_per_tmr = divisor * resolution;

  arm_timer (timer_no, divisor);
}

static void
tmr_reset (void)
{
  unsigned long   flags;

  DISABLE_INTR (flags);
  tmr_offs = 0;
  ticks_offs = 0;
  tmr_ctr = 0;
  next_event_time = 0xffffffff;
  prev_event_time = 0;
  curr_ticks = 0;
  RESTORE_INTR (flags);
}

static int
timer_open (int dev, int mode)
{
  if (opened)
    return RET_ERROR (EBUSY);

  tmr_reset ();
  curr_tempo = 60;
  curr_timebase = HZ;
  opened = 1;
  reprogram_timer ();

  return 0;
}

static void
timer_close (int dev)
{
  opened = tmr_running = 0;
  gus_write8 (0x45, 0);		/* Disable both timers */
}

static int
timer_event (int dev, unsigned char *event)
{
  unsigned char   cmd = event[1];
  unsigned long   parm = *(int *) &event[4];

  switch (cmd)
    {
    case TMR_WAIT_REL:
      parm += prev_event_time;
    case TMR_WAIT_ABS:
      if (parm > 0)
	{
	  long            time;

	  if (parm <= curr_ticks)	/* It's the time */
	    return TIMER_NOT_ARMED;

	  time = parm;
	  next_event_time = prev_event_time = time;

	  return TIMER_ARMED;
	}
      break;

    case TMR_START:
      tmr_reset ();
      tmr_running = 1;
      reprogram_timer ();
      break;

    case TMR_STOP:
      tmr_running = 0;
      break;

    case TMR_CONTINUE:
      tmr_running = 1;
      reprogram_timer ();
      break;

    case TMR_TEMPO:
      if (parm)
	{
	  if (parm < 8)
	    parm = 8;
	  if (parm > 250)
	    parm = 250;
	  tmr_offs = tmr_ctr;
	  ticks_offs += tmr2ticks (tmr_ctr);
	  tmr_ctr = 0;
	  curr_tempo = parm;
	  reprogram_timer ();
	}
      break;

    case TMR_ECHO:
      seq_copy_to_input (event, 8);
      break;

    default:;
    }

  return TIMER_NOT_ARMED;
}

static unsigned long
timer_get_time (int dev)
{
  if (!opened)
    return 0;

  return curr_ticks;
}

static int
timer_ioctl (int dev,
	     unsigned int cmd, unsigned int arg)
{
  switch (cmd)
    {
    case SNDCTL_TMR_SOURCE:
      return IOCTL_OUT (arg, TMR_INTERNAL);
      break;

    case SNDCTL_TMR_START:
      tmr_reset ();
      tmr_running = 1;
      return 0;
      break;

    case SNDCTL_TMR_STOP:
      tmr_running = 0;
      return 0;
      break;

    case SNDCTL_TMR_CONTINUE:
      tmr_running = 1;
      return 0;
      break;

    case SNDCTL_TMR_TIMEBASE:
      {
	int             val = IOCTL_IN (arg);

	if (val)
	  {
	    if (val < 1)
	      val = 1;
	    if (val > 1000)
	      val = 1000;
	    curr_timebase = val;
	  }

	return IOCTL_OUT (arg, curr_timebase);
      }
      break;

    case SNDCTL_TMR_TEMPO:
      {
	int             val = IOCTL_IN (arg);

	if (val)
	  {
	    if (val < 8)
	      val = 8;
	    if (val > 250)
	      val = 250;
	    tmr_offs = tmr_ctr;
	    ticks_offs += tmr2ticks (tmr_ctr);
	    tmr_ctr = 0;
	    curr_tempo = val;
	    reprogram_timer ();
	  }

	return IOCTL_OUT (arg, curr_tempo);
      }
      break;

    case SNDCTL_SEQ_CTRLRATE:
      if (IOCTL_IN (arg) != 0)	/* Can't change */
	return RET_ERROR (EINVAL);

      return IOCTL_OUT (arg, ((curr_tempo * curr_timebase) + 30) / 60);
      break;

    case SNDCTL_TMR_METRONOME:
      /* NOP */
      break;

    default:
    }

  return RET_ERROR (EINVAL);
}

static void
timer_arm (int dev, long time)
{
  if (time < 0)
    time = curr_ticks + 1;
  else if (time <= curr_ticks)	/* It's the time */
    return;

  next_event_time = prev_event_time = time;

  return;
}

static struct sound_timer_operations sound_timer =
{
  {"OPL-3/GUS Timer", 0},
  1,				/* Priority */
  0,				/* Local device link */
  timer_open,
  timer_close,
  timer_event,
  timer_get_time,
  timer_ioctl,
  timer_arm
};

void
sound_timer_interrupt (void)
{
  gus_write8 (0x45, 0);		/* Ack IRQ */
  timer_command (4, 0x80);	/* Reset IRQ flags */

  if (!opened)
    return;

  if (curr_timer == 1)
    gus_write8 (0x45, 0x04);	/* Start timer 1 again */
  else
    gus_write8 (0x45, 0x08);	/* Start timer 2 again */

  if (!tmr_running)
    return;

  tmr_ctr++;
  curr_ticks = ticks_offs + tmr2ticks (tmr_ctr);

  if (curr_ticks >= next_event_time)
    {
      next_event_time = 0xffffffff;
      sequencer_timer ();
    }
}

void
sound_timer_init (int io_base)
{
  int             n;

  if (initialized)
    return;			/* There is already a similar timer */

  select_addr = io_base;
  data_addr = io_base + 1;

  initialized = 1;

#if 1
  if (num_sound_timers >= MAX_TIMER_DEV)
    n = 0;			/* Overwrite the system timer */
  else
    n = num_sound_timers++;
#else
  n = 0;
#endif

  sound_timer_devs[n] = &sound_timer;
}

#endif
#endif
