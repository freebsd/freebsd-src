/*
 * sound/sequencer.c
 * 
 * The sequencer personality manager.
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

#ifndef EXCLUDE_SEQUENCER

static int      sequencer_ok = 0;

DEFINE_WAIT_QUEUE (seq_sleeper, seq_sleep_flag);
/* DEFINE_WAIT_QUEUE (midi_sleeper, midi_sleep_flag); */
#define midi_sleeper seq_sleeper
#define midi_sleep_flag seq_sleep_flag

static int      midi_opened[MAX_MIDI_DEV] =
{0};				/* 1 if the process has opened MIDI */
static int      midi_written[MAX_MIDI_DEV] =
{0};

long            seq_time = 0;	/* Reference point for the timer */

#include "tuning.h"

#define EV_SZ	8
#define IEV_SZ	4
static unsigned char *queue = NULL;	/* SEQ_MAX_QUEUE * EV_SZ bytes */
static unsigned char *iqueue = NULL;	/* SEQ_MAX_QUEUE * IEV_SZ bytes */

static volatile int qhead = 0, qtail = 0, qlen = 0;
static volatile int iqhead = 0, iqtail = 0, iqlen = 0;
static volatile int seq_playing = 0;
static int      sequencer_busy = 0;
static int      output_treshold;
static unsigned synth_open_mask;

static int      seq_queue (unsigned char *note);
static void     seq_startplay (void);
static int      seq_sync (void);
static void     seq_reset (void);
static int      pmgr_present[MAX_SYNTH_DEV] =
{0};

#if MAX_SYNTH_DEV > 15
#error Too many synthesizer devices
#endif

int
sequencer_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c = count, p = 0;

  dev = dev >> 4;

  if (dev)			/* Patch manager device */
    return pmgr_read (dev - 1, file, buf, count);

  while (c > 3)
    {
      if (!iqlen)
	{
	  DO_SLEEP (midi_sleeper, midi_sleep_flag, 0);

	  if (!iqlen)
	    return count - c;
	}

      COPY_TO_USER (buf, p, &iqueue[iqhead * IEV_SZ], IEV_SZ);
      p += 4;
      c -= 4;

      iqhead = (iqhead + 1) % SEQ_MAX_QUEUE;
      iqlen--;
    }

  return count - c;
}

static void
sequencer_midi_output (int dev)
{
  /* Currently NOP */
}

static void
copy_to_input (unsigned char *event)
{
  unsigned long   flags;

  if (iqlen >= (SEQ_MAX_QUEUE - 1))
    return;			/* Overflow */

  memcpy (&iqueue[iqtail * IEV_SZ], event, IEV_SZ);
  iqlen++;
  iqtail = (iqtail + 1) % SEQ_MAX_QUEUE;

  DISABLE_INTR (flags);
  if (SOMEONE_WAITING (midi_sleeper, midi_sleep_flag))
    {
      WAKE_UP (midi_sleeper, midi_sleep_flag);
    }
  RESTORE_INTR (flags);
}

static void
sequencer_midi_input (int dev, unsigned char data)
{
  int             tstamp;
  unsigned char   event[4];

  if (data == 0xfe)		/* Active sensing */
    return;			/* Ignore */

  tstamp = GET_TIME () - seq_time;	/* Time since open() */
  tstamp = (tstamp << 8) | SEQ_WAIT;

  copy_to_input ((unsigned char *) &tstamp);

  event[0] = SEQ_MIDIPUTC;
  event[1] = data;
  event[2] = dev;
  event[3] = 0;

  copy_to_input (event);
}

int
sequencer_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  unsigned char   event[EV_SZ], ev_code;
  int             p = 0, c, ev_size;
  int             err;
  int             mode = file->mode & O_ACCMODE;

  dev = dev >> 4;

  DEB (printk ("sequencer_write(dev=%d, count=%d)\n", dev, count));

  if (mode == OPEN_READ)
    return RET_ERROR (EIO);

  if (dev)			/* Patch manager device */
    return pmgr_write (dev - 1, file, buf, count);

  c = count;

  while (c >= 4)
    {
      COPY_FROM_USER (event, buf, p, 4);
      ev_code = event[0];

      if (ev_code == SEQ_FULLSIZE)
	{
	  int             err;

	  dev = *(unsigned short *) &event[2];
	  if (dev < 0 || dev >= num_synths)
	    return RET_ERROR (ENXIO);

	  if (!(synth_open_mask & (1 << dev)))
	    return RET_ERROR (ENXIO);

	  err = synth_devs[dev]->load_patch (dev, *(short *) &event[0], buf, p + 4, c, 0);
	  if (err < 0)
	    return err;

	  return err;
	}

      if (ev_code == SEQ_EXTENDED || ev_code == SEQ_PRIVATE)
	{

	  ev_size = 8;

	  if (c < ev_size)
	    {
	      if (!seq_playing)
		seq_startplay ();
	      return count - c;
	    }

	  COPY_FROM_USER (&event[4], buf, p + 4, 4);

	}
      else
	ev_size = 4;

      if (event[0] == SEQ_MIDIPUTC)
	{

	  if (!midi_opened[event[2]])
	    {
	      int             mode;
	      int             dev = event[2];

	      if (dev >= num_midis)
		{
		  printk ("Sequencer Error: Nonexistent MIDI device %d\n", dev);
		  return RET_ERROR (ENXIO);
		}

	      mode = file->mode & O_ACCMODE;

	      if ((err = midi_devs[dev]->open (dev, mode,
			  sequencer_midi_input, sequencer_midi_output)) < 0)
		{
		  seq_reset ();
		  printk ("Sequencer Error: Unable to open Midi #%d\n", dev);
		  return err;
		}

	      midi_opened[dev] = 1;
	    }

	}

      if (!seq_queue (event))
	{

	  if (!seq_playing)
	    seq_startplay ();
	  return count - c;
	}

      p += ev_size;
      c -= ev_size;
    }

  if (!seq_playing)
    seq_startplay ();

  return count;
}

static int
seq_queue (unsigned char *note)
{

  /* Test if there is space in the queue */

  if (qlen >= SEQ_MAX_QUEUE)
    if (!seq_playing)
      seq_startplay ();		/* Give chance to drain the queue */

  if (qlen >= SEQ_MAX_QUEUE && !SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
    {
      /* Sleep until there is enough space on the queue */
      DO_SLEEP (seq_sleeper, seq_sleep_flag, 0);
    }

  if (qlen >= SEQ_MAX_QUEUE)
    return 0;			/* To be sure */

  memcpy (&queue[qtail * EV_SZ], note, EV_SZ);

  qtail = (qtail + 1) % SEQ_MAX_QUEUE;
  qlen++;

  return 1;
}

static int
extended_event (unsigned char *q)
{
  int             dev = q[2];

  if (dev < 0 || dev >= num_synths)
    return RET_ERROR (ENXIO);

  if (!(synth_open_mask & (1 << dev)))
    return RET_ERROR (ENXIO);

  switch (q[1])
    {
    case SEQ_NOTEOFF:
      synth_devs[dev]->kill_note (dev, q[3], q[5]);
      break;

    case SEQ_NOTEON:
      if (q[4] > 127 && q[4] != 255)
	return 0;

      synth_devs[dev]->start_note (dev, q[3], q[4], q[5]);
      break;

    case SEQ_PGMCHANGE:
      synth_devs[dev]->set_instr (dev, q[3], q[4]);
      break;

    case SEQ_AFTERTOUCH:
      synth_devs[dev]->aftertouch (dev, q[3], q[4]);
      break;

    case SEQ_BALANCE:
      synth_devs[dev]->panning (dev, q[3], (char) q[4]);
      break;

    case SEQ_CONTROLLER:
      synth_devs[dev]->controller (dev, q[3], q[4], *(short *) &q[5]);
      break;

    default:
      return RET_ERROR (EINVAL);
    }

  return 0;
}

static void
seq_startplay (void)
{
  int             this_one;
  unsigned long  *delay;
  unsigned char  *q;

  while (qlen > 0)
    {
      qhead = ((this_one = qhead) + 1) % SEQ_MAX_QUEUE;
      qlen--;

      q = &queue[this_one * EV_SZ];

      switch (q[0])
	{
	case SEQ_NOTEOFF:
	  if (synth_open_mask & (1 << 0))
	    if (synth_devs[0])
	      synth_devs[0]->kill_note (0, q[1], q[3]);
	  break;

	case SEQ_NOTEON:
	  if (q[4] < 128 || q[4] == 255)
	    if (synth_open_mask & (1 << 0))
	      if (synth_devs[0])
		synth_devs[0]->start_note (0, q[1], q[2], q[3]);
	  break;

	case SEQ_WAIT:
	  delay = (unsigned long *) q;	/* Bytes 1 to 3 are containing the
					 * delay in GET_TIME() */
	  *delay = (*delay >> 8) & 0xffffff;

	  if (*delay > 0)
	    {
	      long            time;

	      seq_playing = 1;
	      time = *delay;

	      request_sound_timer (time);

	      if ((SEQ_MAX_QUEUE - qlen) >= output_treshold)
		{
		  unsigned long   flags;

		  DISABLE_INTR (flags);
		  if (SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
		    {
		      WAKE_UP (seq_sleeper, seq_sleep_flag);
		    }
		  RESTORE_INTR (flags);
		}
	      return;		/* Stop here. Timer routine will continue
				 * playing after the delay */
	    }
	  break;

	case SEQ_PGMCHANGE:
	  if (synth_open_mask & (1 << 0))
	    if (synth_devs[0])
	      synth_devs[0]->set_instr (0, q[1], q[2]);
	  break;

	case SEQ_SYNCTIMER:	/* Reset timer */
	  seq_time = GET_TIME ();
	  break;

	case SEQ_MIDIPUTC:	/* Put a midi character */
	  if (midi_opened[q[2]])
	    {
	      int             dev;

	      dev = q[2];

	      if (!midi_devs[dev]->putc (dev, q[1]))
		{
		  /*
		   * Output FIFO is full. Wait one timer cycle and try again.
		   */

		  qlen++;
		  qhead = this_one;	/* Restore queue */
		  seq_playing = 1;
		  request_sound_timer (-1);
		  return;
		}
	      else
		midi_written[dev] = 1;
	    }
	  break;

	case SEQ_ECHO:
	  copy_to_input (q);	/* Echo back to the process */
	  break;

	case SEQ_PRIVATE:
	  if (q[1] < num_synths)
	    synth_devs[q[1]]->hw_control (q[1], q);
	  break;

	case SEQ_EXTENDED:
	  extended_event (q);
	  break;

	default:;
	}

    }

  seq_playing = 0;

  if ((SEQ_MAX_QUEUE - qlen) >= output_treshold)
    {
      unsigned long   flags;

      DISABLE_INTR (flags);
      if (SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
	{
	  WAKE_UP (seq_sleeper, seq_sleep_flag);
	}
      RESTORE_INTR (flags);
    }

}

int
sequencer_open (int dev, struct fileinfo *file)
{
  int             retval, mode, i;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  DEB (printk ("sequencer_open(dev=%d)\n", dev));

  if (!sequencer_ok)
    {
      printk ("Soundcard: Sequencer not initialized\n");
      return RET_ERROR (ENXIO);
    }

  if (dev)			/* Patch manager device */
    {
      int             err;

      dev--;
      if (pmgr_present[dev])
	return RET_ERROR (EBUSY);
      if ((err = pmgr_open (dev)) < 0)
	return err;		/* Failed */

      pmgr_present[dev] = 1;
      return err;
    }

  if (sequencer_busy)
    {
      printk ("Sequencer busy\n");
      return RET_ERROR (EBUSY);
    }

  if (!(num_synths + num_midis))
    return RET_ERROR (ENXIO);

  synth_open_mask = 0;

  if (mode == OPEN_WRITE || mode == OPEN_READWRITE)
    for (i = 0; i < num_synths; i++)	/* Open synth devices */
      if (synth_devs[i]->open (i, mode) < 0)
	printk ("Sequencer: Warning! Cannot open synth device #%d\n", i);
      else
	synth_open_mask |= (1 << i);

  seq_time = GET_TIME ();

  for (i = 0; i < num_midis; i++)
    {
      midi_opened[i] = 0;
      midi_written[i] = 0;
    }

  if (mode == OPEN_READ || mode == OPEN_READWRITE)
    {				/* Initialize midi input devices */
      if (!num_midis)
	{
	  printk ("Sequencer: No Midi devices. Input not possible\n");
	  return RET_ERROR (ENXIO);
	}

      for (i = 0; i < num_midis; i++)
	{
	  if ((retval = midi_devs[i]->open (i, mode,
			 sequencer_midi_input, sequencer_midi_output)) >= 0)
	    midi_opened[i] = 1;
	}
    }

  sequencer_busy = 1;
  RESET_WAIT_QUEUE (seq_sleeper, seq_sleep_flag);
  RESET_WAIT_QUEUE (midi_sleeper, midi_sleep_flag);
  output_treshold = SEQ_MAX_QUEUE / 2;

  for (i = 0; i < num_synths; i++)
    if (pmgr_present[i])
      pmgr_inform (i, PM_E_OPENED, 0, 0, 0, 0);

  return 0;
}

void
seq_drain_midi_queues (void)
{
  int             i, n;

  /*
   * Give the Midi drivers time to drain their output queues
   */

  n = 1;

  while (!PROCESS_ABORTING (midi_sleeper, midi_sleep_flag) && n)
    {
      n = 0;

      for (i = 0; i < num_midis; i++)
	if (midi_opened[i] && midi_written[i])
	  if (midi_devs[i]->buffer_status != NULL)
	    if (midi_devs[i]->buffer_status (i))
	      n++;

      /*
       * Let's have a delay
       */
      if (n)
	{
	  DO_SLEEP (seq_sleeper, seq_sleep_flag, HZ / 10);
	}
    }
}

void
sequencer_release (int dev, struct fileinfo *file)
{
  int             i;
  int             mode = file->mode & O_ACCMODE;

  dev = dev >> 4;

  DEB (printk ("sequencer_release(dev=%d)\n", dev));

  if (dev)			/* Patch manager device */
    {
      dev--;
      pmgr_release (dev);
      pmgr_present[dev] = 0;
      return;
    }

  /*
     * Wait until the queue is empty
   */

  while (!PROCESS_ABORTING (seq_sleeper, seq_sleep_flag) && qlen)
    {
      seq_sync ();
    }

  if (mode != OPEN_READ)
    seq_drain_midi_queues ();	/* Ensure the output queues are empty */
  seq_reset ();
  if (mode != OPEN_READ)
    seq_drain_midi_queues ();	/* Flush the all notes off messages */

  for (i = 0; i < num_midis; i++)
    if (midi_opened[i])
      midi_devs[i]->close (i);

  if (mode == OPEN_WRITE || mode == OPEN_READWRITE)
    for (i = 0; i < num_synths; i++)
      if (synth_open_mask & (1 << i))	/* Actually opened */
	if (synth_devs[i])
	  synth_devs[i]->close (i);

  for (i = 0; i < num_synths; i++)
    if (pmgr_present[i])
      pmgr_inform (i, PM_E_CLOSED, 0, 0, 0, 0);

  sequencer_busy = 0;
}

static int
seq_sync (void)
{
  if (qlen && !seq_playing && !PROCESS_ABORTING (seq_sleeper, seq_sleep_flag))
    seq_startplay ();

  if (qlen && !SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))	/* Queue not empty */
    {
      DO_SLEEP (seq_sleeper, seq_sleep_flag, 0);
    }

  return qlen;
}

static void
midi_outc (int dev, unsigned char data)
{
  /*
   * NOTE! Calls sleep(). Don't call this from interrupt.
   */

  int             n;

  /* This routine sends one byte to the Midi channel. */
  /* If the output Fifo is full, it waits until there */
  /* is space in the queue */

  n = 300;			/* Timeout in jiffies */

  while (n && !midi_devs[dev]->putc (dev, data))
    {
      DO_SLEEP (seq_sleeper, seq_sleep_flag, 4);
      n--;
    }
}

static void
seq_reset (void)
{
  /*
   * NOTE! Calls sleep(). Don't call this from interrupt.
   */

  int             i, chn;

  sound_stop_timer ();

  qlen = qhead = qtail = 0;
  iqlen = iqhead = iqtail = 0;

  for (i = 0; i < num_synths; i++)
    if (synth_open_mask & (1 << i))
      if (synth_devs[i])
	synth_devs[i]->reset (i);

  for (i = 0; i < num_midis; i++)
    if (midi_written[i])	/* Midi used. Some notes may still be playing */
      {
	for (chn = 0; chn < 16; chn++)
	  {
	    midi_outc (i,
		       (unsigned char) (0xb0 + (chn & 0xff)));	/* Channel msg */
	    midi_outc (i, 0x7b);	/* All notes off */
	    midi_outc (i, 0);	/* Dummy parameter */
	  }

	midi_devs[i]->close (i);

	midi_written[i] = 0;
	midi_opened[i] = 0;
      }

  seq_playing = 0;

  if (SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
    printk ("Sequencer Warning: Unexpected sleeping process\n");

}

int
sequencer_ioctl (int dev, struct fileinfo *file,
		 unsigned int cmd, unsigned int arg)
{
  int             midi_dev, orig_dev;
  int             mode = file->mode & O_ACCMODE;

  orig_dev = dev = dev >> 4;

  switch (cmd)
    {

    case SNDCTL_SEQ_SYNC:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      if (mode == OPEN_READ)
	return 0;
      while (qlen && !PROCESS_ABORTING (seq_sleeper, seq_sleep_flag))
	seq_sync ();
      return 0;
      break;

    case SNDCTL_SEQ_RESET:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      seq_reset ();
      return 0;
      break;

    case SNDCTL_SEQ_TESTMIDI:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      midi_dev = IOCTL_IN (arg);
      if (midi_dev >= num_midis)
	return RET_ERROR (ENXIO);

      if (!midi_opened[midi_dev])
	{
	  int             err, mode;

	  mode = file->mode & O_ACCMODE;
	  if ((err = midi_devs[midi_dev]->open (midi_dev, mode,
						sequencer_midi_input,
						sequencer_midi_output)) < 0)
	    return err;
	}

      midi_opened[midi_dev] = 1;

      return 0;
      break;

    case SNDCTL_SEQ_GETINCOUNT:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      if (mode == OPEN_WRITE)
	return 0;
      return IOCTL_OUT (arg, iqlen);
      break;

    case SNDCTL_SEQ_GETOUTCOUNT:

      if (mode == OPEN_READ)
	return 0;
      return IOCTL_OUT (arg, SEQ_MAX_QUEUE - qlen);
      break;

    case SNDCTL_SEQ_CTRLRATE:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      /* If *arg == 0, just return the current rate */
      return IOCTL_OUT (arg, HZ);
      break;

    case SNDCTL_SEQ_RESETSAMPLES:
      dev = IOCTL_IN (arg);
      if (dev < 0 || dev >= num_synths)
	return RET_ERROR (ENXIO);

      if (!(synth_open_mask & (1 << dev)) && !orig_dev)
	return RET_ERROR (EBUSY);

      if (!orig_dev && pmgr_present[dev])
	pmgr_inform (dev, PM_E_PATCH_RESET, 0, 0, 0, 0);

      return synth_devs[dev]->ioctl (dev, cmd, arg);
      break;

    case SNDCTL_SEQ_NRSYNTHS:
      return IOCTL_OUT (arg, num_synths);
      break;

    case SNDCTL_SEQ_NRMIDIS:
      return IOCTL_OUT (arg, num_midis);
      break;

    case SNDCTL_SYNTH_MEMAVL:
      {
	int             dev = IOCTL_IN (arg);

	if (dev < 0 || dev >= num_synths)
	  return RET_ERROR (ENXIO);

	if (!(synth_open_mask & (1 << dev)) && !orig_dev)
	  return RET_ERROR (EBUSY);

	return IOCTL_OUT (arg, synth_devs[dev]->ioctl (dev, cmd, arg));
      }
      break;

    case SNDCTL_FM_4OP_ENABLE:
      {
	int             dev = IOCTL_IN (arg);

	if (dev < 0 || dev >= num_synths)
	  return RET_ERROR (ENXIO);

	if (!(synth_open_mask & (1 << dev)))
	  return RET_ERROR (ENXIO);

	synth_devs[dev]->ioctl (dev, cmd, arg);
	return 0;
      }
      break;

    case SNDCTL_SYNTH_INFO:
      {
	struct synth_info inf;
	int             dev;

	IOCTL_FROM_USER ((char *) &inf, (char *) arg, 0, sizeof (inf));
	dev = inf.device;

	if (dev < 0 || dev >= num_synths)
	  return RET_ERROR (ENXIO);

	if (!(synth_open_mask & (1 << dev)) && !orig_dev)
	  return RET_ERROR (EBUSY);

	return synth_devs[dev]->ioctl (dev, cmd, arg);
      }
      break;

    case SNDCTL_MIDI_INFO:
      {
	struct midi_info inf;
	int             dev;

	IOCTL_FROM_USER ((char *) &inf, (char *) arg, 0, sizeof (inf));
	dev = inf.device;

	if (dev < 0 || dev >= num_midis)
	  return RET_ERROR (ENXIO);

	IOCTL_TO_USER ((char *) arg, 0, (char *) &(midi_devs[dev]->info), sizeof (inf));
	return 0;
      }
      break;

    case SNDCTL_PMGR_IFACE:
      {
	struct patmgr_info *inf;
	int             dev, err;

	inf = (struct patmgr_info *) KERNEL_MALLOC (sizeof (*inf));

	IOCTL_FROM_USER ((char *) inf, (char *) arg, 0, sizeof (*inf));
	dev = inf->device;

	if (dev < 0 || dev >= num_synths)
	  {
	    KERNEL_FREE (inf);
	    return RET_ERROR (ENXIO);
	  }

	if (!synth_devs[dev]->pmgr_interface)
	  {
	    KERNEL_FREE (inf);
	    return RET_ERROR (ENXIO);
	  }

	if ((err = synth_devs[dev]->pmgr_interface (dev, inf)) == -1)
	  {
	    KERNEL_FREE (inf);
	    return err;
	  }

	IOCTL_TO_USER ((char *) arg, 0, (char *) inf, sizeof (*inf));
	KERNEL_FREE (inf);
	return 0;
      }
      break;

    case SNDCTL_PMGR_ACCESS:
      {
	struct patmgr_info *inf;
	int             dev, err;

	inf = (struct patmgr_info *) KERNEL_MALLOC (sizeof (*inf));

	IOCTL_FROM_USER ((char *) inf, (char *) arg, 0, sizeof (*inf));
	dev = inf->device;

	if (dev < 0 || dev >= num_synths)
	  {
	    KERNEL_FREE (inf);
	    return RET_ERROR (ENXIO);
	  }

	if (!pmgr_present[dev])
	  {
	    KERNEL_FREE (inf);
	    return RET_ERROR (ESRCH);
	  }

	if ((err = pmgr_access (dev, inf)) < 0)
	  {
	    KERNEL_FREE (inf);
	    return err;
	  }

	IOCTL_TO_USER ((char *) arg, 0, (char *) inf, sizeof (*inf));
	KERNEL_FREE (inf);
	return 0;
      }
      break;

    case SNDCTL_SEQ_TRESHOLD:
      {
	int             tmp = IOCTL_IN (arg);

	if (dev)		/* Patch manager */
	  return RET_ERROR (EIO);

	if (tmp < 1)
	  tmp = 1;
	if (tmp >= SEQ_MAX_QUEUE)
	  tmp = SEQ_MAX_QUEUE - 1;
	output_treshold = tmp;
	return 0;
      }
      break;

    default:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      if (mode == OPEN_READ)
	return RET_ERROR (EIO);

      if (!synth_devs[0])
	return RET_ERROR (ENXIO);
      if (!(synth_open_mask & (1 << 0)))
	return RET_ERROR (ENXIO);
      return synth_devs[0]->ioctl (0, cmd, arg);
      break;
    }

  return RET_ERROR (EINVAL);
}

#ifdef ALLOW_SELECT
int
sequencer_select (int dev, struct fileinfo *file, int sel_type, select_table * wait)
{
  dev = dev >> 4;

  switch (sel_type)
    {
    case SEL_IN:
      if (!iqlen)
	{
	  select_wait (&midi_sleeper, wait);
	  return 0;
	}
      return 1;

      break;

    case SEL_OUT:
      if (qlen >= SEQ_MAX_QUEUE)
	{
	  select_wait (&seq_sleeper, wait);
	  return 0;
	}
      return 1;
      break;

    case SEL_EX:
      return 0;
    }

  return 0;
}

#endif

void
sequencer_timer (void)
{
  seq_startplay ();
}

int
note_to_freq (int note_num)
{

  /*
   * This routine converts a midi note to a frequency (multiplied by 1000)
   */

  int             note, octave, note_freq;
  int             notes[] =
  {
    261632, 277189, 293671, 311132, 329632, 349232,
    369998, 391998, 415306, 440000, 466162, 493880
  };				/* Note freq*1000 for octave 5 */

#define BASE_OCTAVE	5

  octave = note_num / 12;
  note = note_num % 12;

  note_freq = notes[note];

  if (octave < BASE_OCTAVE)
    note_freq >>= (BASE_OCTAVE - octave);
  else if (octave > BASE_OCTAVE)
    note_freq <<= (octave - BASE_OCTAVE);

  /* note_freq >>= 1;    */

  return note_freq;
}

unsigned long
compute_finetune (unsigned long base_freq, int bend, int range)
{
  unsigned long   amount;
  int             negative, semitones, cents;

  if (!bend)
    return base_freq;
  if (!range)
    return base_freq;

  if (!base_freq)
    return base_freq;

  if (range >= 8192)
    range = 8191;

  bend = bend * range / 8192;
  if (!bend)
    return base_freq;

  negative = bend < 0 ? 1 : 0;

  if (bend < 0)
    bend *= -1;
  if (bend > range)
    bend = range;

  if (bend > 2399)
    bend = 2399;

  semitones = bend / 100;
  cents = bend % 100;

  amount = semitone_tuning[semitones] * cent_tuning[cents] / 10000;

  if (negative)
    return (base_freq * 10000) / amount;	/* Bend down */
  else
    return (base_freq * amount) / 10000;	/* Bend up */
}


long
sequencer_init (long mem_start)
{

  sequencer_ok = 1;
  PERMANENT_MALLOC (unsigned char *, queue, SEQ_MAX_QUEUE * EV_SZ, mem_start);
  PERMANENT_MALLOC (unsigned char *, iqueue, SEQ_MAX_QUEUE * IEV_SZ, mem_start);

  return mem_start;
}

#else
/* Stub version */
int
sequencer_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
sequencer_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  return RET_ERROR (EIO);
}

int
sequencer_open (int dev, struct fileinfo *file)
{
  return RET_ERROR (ENXIO);
}

void
sequencer_release (int dev, struct fileinfo *file)
{
}
int
sequencer_ioctl (int dev, struct fileinfo *file,
		 unsigned int cmd, unsigned int arg)
{
  return RET_ERROR (EIO);
}

int
sequencer_lseek (int dev, struct fileinfo *file, off_t offset, int orig)
{
  return RET_ERROR (EIO);
}

long
sequencer_init (long mem_start)
{
  return mem_start;
}

int
sequencer_select (int dev, struct fileinfo *file, int sel_type, select_table * wait)
{
  return RET_ERROR (EIO);
}

#endif

#endif
