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
#include <i386/isa/sound/sound_config.h>
#include <i386/isa/sound/midi_ctrl.h>

extern void seq_drain_midi_queues __P((void));

#ifdef CONFIGURE_SOUNDCARD

#ifndef EXCLUDE_SEQUENCER

static int      sequencer_ok = 0;
static struct sound_timer_operations *tmr;
static int      tmr_no = -1;	/* Currently selected timer */
static int      pending_timer = -1;	/* For timer change operation */

/*
 * Local counts for number of synth and MIDI devices. These are initialized
 * by the sequencer_open.
 */
static int      max_mididev = 0;
static int      max_synthdev = 0;

/*
 * The seq_mode gives the operating mode of the sequencer:
 *      1 = level1 (the default)
 *      2 = level2 (extended capabilites)
 */

#define SEQ_1	1
#define SEQ_2	2
static int      seq_mode = SEQ_1;

DEFINE_WAIT_QUEUE (seq_sleeper, seq_sleep_flag);
DEFINE_WAIT_QUEUE (midi_sleeper, midi_sleep_flag);

static int      midi_opened[MAX_MIDI_DEV] =
{0};
static int      midi_written[MAX_MIDI_DEV] =
{0};

static unsigned long   prev_input_time = 0;
static int             prev_event_time;
static unsigned long   seq_time = 0;

#include <i386/isa/sound/tuning.h>

#define EV_SZ	8
#define IEV_SZ	8
static unsigned char *queue = NULL;
static unsigned char *iqueue = NULL;

static volatile int qhead = 0, qtail = 0, qlen = 0;
static volatile int iqhead = 0, iqtail = 0, iqlen = 0;
static volatile int seq_playing = 0;
static int      sequencer_busy = 0;
static int      output_treshold;
static int      pre_event_timeout;
static unsigned synth_open_mask;

static int      seq_queue (unsigned char *note, char nonblock);
static void     seq_startplay (void);
static int      seq_sync (void);
static void     seq_reset (void);
static int      pmgr_present[MAX_SYNTH_DEV] =
{0};

#if MAX_SYNTH_DEV > 15
#error Too many synthesizer devices enabled.
#endif

int
sequencer_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             c = count, p = 0;
  int             ev_len;
  unsigned long   flags;

  dev = dev >> 4;

  ev_len = seq_mode == SEQ_1 ? 4 : 8;

  if (dev)			/*
				 * Patch manager device
				 */
    return pmgr_read (dev - 1, file, buf, count);

  DISABLE_INTR (flags);
  if (!iqlen)
    {
      if (ISSET_FILE_FLAG (file, O_NONBLOCK))
	{
	  RESTORE_INTR (flags);
	  return RET_ERROR (EAGAIN);
	}

      DO_SLEEP (midi_sleeper, midi_sleep_flag, pre_event_timeout);

      if (!iqlen)
	{
	  RESTORE_INTR (flags);
	  return 0;
	}
    }

  while (iqlen && c >= ev_len)
    {

      COPY_TO_USER (buf, p, &iqueue[iqhead * IEV_SZ], ev_len);
      p += ev_len;
      c -= ev_len;

      iqhead = (iqhead + 1) % SEQ_MAX_QUEUE;
      iqlen--;
    }
  RESTORE_INTR (flags);

  return count - c;
}

static void
sequencer_midi_output (int dev)
{
  /*
   * Currently NOP
   */
}

void
seq_copy_to_input (unsigned char *event, int len)
{
  unsigned long   flags;

  /*
     * Verify that the len is valid for the current mode.
   */

  if (len != 4 && len != 8)
    return;
  if ((seq_mode == SEQ_1) != (len == 4))
    return;

  if (iqlen >= (SEQ_MAX_QUEUE - 1))
    return;			/* Overflow */

  DISABLE_INTR (flags);
  memcpy (&iqueue[iqtail * IEV_SZ], event, len);
  iqlen++;
  iqtail = (iqtail + 1) % SEQ_MAX_QUEUE;

  if (SOMEONE_WAITING (midi_sleeper, midi_sleep_flag))
    {
      WAKE_UP (midi_sleeper, midi_sleep_flag);
    }
  RESTORE_INTR (flags);
#if defined(__FreeBSD__)
  if (selinfo[0].si_pid)
    selwakeup(&selinfo[0]);
#endif
}

static void
sequencer_midi_input (int dev, unsigned char data)
{
  unsigned int    tstamp;
  unsigned char   event[4];

  if (data == 0xfe)		/* Ignore active sensing */
    return;

  tstamp = GET_TIME () - seq_time;
  if (tstamp != prev_input_time)
    {
      tstamp = (tstamp << 8) | SEQ_WAIT;

      seq_copy_to_input ((unsigned char *) &tstamp, 4);
      prev_input_time = tstamp;
    }

  event[0] = SEQ_MIDIPUTC;
  event[1] = data;
  event[2] = dev;
  event[3] = 0;

  seq_copy_to_input (event, 4);
}

void
seq_input_event (unsigned char *event, int len)
{
  unsigned long   this_time;

  if (seq_mode == SEQ_2)
    this_time = tmr->get_time (tmr_no);
  else
    this_time = GET_TIME () - seq_time;

  if (this_time != prev_input_time)
    {
      unsigned char   tmp_event[8];

      tmp_event[0] = EV_TIMING;
      tmp_event[1] = TMR_WAIT_ABS;
      tmp_event[2] = 0;
      tmp_event[3] = 0;
      *(unsigned long *) &tmp_event[4] = this_time;

      seq_copy_to_input (tmp_event, 8);
      prev_input_time = this_time;
    }

  seq_copy_to_input (event, len);
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

  if (dev)			/*
				 * Patch manager device
				 */
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
	  if (dev < 0 || dev >= max_synthdev)
	    return RET_ERROR (ENXIO);

	  if (!(synth_open_mask & (1 << dev)))
	    return RET_ERROR (ENXIO);

	  err = synth_devs[dev]->load_patch (dev, *(short *) &event[0], buf, p + 4, c, 0);
	  if (err < 0)
	    return err;

	  return err;
	}

      if (ev_code >= 128)
	{
	  if (seq_mode == SEQ_2 && ev_code == SEQ_EXTENDED)
	    {
	      printk ("Sequencer: Invalid level 2 event %x\n", ev_code);
	      return RET_ERROR (EINVAL);
	    }

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
	{
	  if (seq_mode == SEQ_2)
	    {
	      printk ("Sequencer: 4 byte event in level 2 mode\n");
	      return RET_ERROR (EINVAL);
	    }
	  ev_size = 4;
	}

      if (event[0] == SEQ_MIDIPUTC)
	{

	  if (!midi_opened[event[2]])
	    {
	      int             mode;
	      int             dev = event[2];

	      if (dev >= max_mididev)
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

      if (!seq_queue (event, ISSET_FILE_FLAG (file, O_NONBLOCK)))
	{
	  int             processed = count - c;

	  if (!seq_playing)
	    seq_startplay ();

	  if (!processed && ISSET_FILE_FLAG (file, O_NONBLOCK))
	    return RET_ERROR (EAGAIN);
	  else
	    return processed;
	}

      p += ev_size;
      c -= ev_size;
    }

  if (!seq_playing)
    seq_startplay ();

  return count;			/* This will "eat" chunks shorter than 4 bytes (if written
				   * alone) Should we really do that ?
				 */
}

static int
seq_queue (unsigned char *note, char nonblock)
{

  /*
   * Test if there is space in the queue
   */

  if (qlen >= SEQ_MAX_QUEUE)
    if (!seq_playing)
      seq_startplay ();		/*
				 * Give chance to drain the queue
				 */

  if (!nonblock && qlen >= SEQ_MAX_QUEUE && !SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
    {
      /*
       * Sleep until there is enough space on the queue
       */
      DO_SLEEP (seq_sleeper, seq_sleep_flag, 0);
    }

  if (qlen >= SEQ_MAX_QUEUE)
    {
      return 0;			/*
				 * To be sure
				 */
    }
  memcpy (&queue[qtail * EV_SZ], note, EV_SZ);

  qtail = (qtail + 1) % SEQ_MAX_QUEUE;
  qlen++;

  return 1;
}

static int
extended_event (unsigned char *q)
{
  int             dev = q[2];

  if (dev < 0 || dev >= max_synthdev)
    return RET_ERROR (ENXIO);

  if (!(synth_open_mask & (1 << dev)))
    return RET_ERROR (ENXIO);

  switch (q[1])
    {
    case SEQ_NOTEOFF:
      synth_devs[dev]->kill_note (dev, q[3], q[4], q[5]);
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

    case SEQ_VOLMODE:
      if (synth_devs[dev]->volume_method != NULL)
	synth_devs[dev]->volume_method (dev, q[3]);
      break;

    default:
      return RET_ERROR (EINVAL);
    }

  return 0;
}

static int
find_voice (int dev, int chn, int note)
{
  unsigned short  key;
  int             i;

  key = (chn << 8) | (note + 1);

  for (i = 0; i < synth_devs[dev]->alloc.max_voice; i++)
    if (synth_devs[dev]->alloc.map[i] == key)
      return i;

  return -1;
}

static int
alloc_voice (int dev, int chn, int note)
{
  unsigned short  key;
  int             voice;

  key = (chn << 8) | (note + 1);

  voice = synth_devs[dev]->alloc_voice (dev, chn, note,
					&synth_devs[dev]->alloc);
  synth_devs[dev]->alloc.map[voice] = key;
  synth_devs[dev]->alloc.alloc_times[voice] =
    synth_devs[dev]->alloc.timestamp++;
  return voice;
}

static void
seq_chn_voice_event (unsigned char *event)
{
  unsigned char   dev = event[1];
  unsigned char   cmd = event[2];
  unsigned char   chn = event[3];
  unsigned char   note = event[4];
  unsigned char   parm = event[5];
  int             voice = -1;

  if ((int) dev > max_synthdev)
    return;
  if (!(synth_open_mask & (1 << dev)))
    return;
  if (!synth_devs[dev])
    return;

  if (seq_mode == SEQ_2)
    {
      if (synth_devs[dev]->alloc_voice)
	voice = find_voice (dev, chn, note);

      if (cmd == MIDI_NOTEON && parm == 0)
	{
	  cmd = MIDI_NOTEOFF;
	  parm = 64;
	}
    }

  switch (cmd)
    {
    case MIDI_NOTEON:
      if (note > 127 && note != 255)	/* Not a seq2 feature */
	return;

      if (voice == -1 && seq_mode == SEQ_2 && synth_devs[dev]->alloc_voice)
	{			/* Internal synthesizer (FM, GUS, etc) */
	  voice = alloc_voice (dev, chn, note);
	}

      if (voice == -1)
	voice = chn;

      if (seq_mode == SEQ_2 && dev < num_synths)
	{
	  /*
	     * The MIDI channel 10 is a percussive channel. Use the note
	     * number to select the proper patch (128 to 255) to play.
	   */

	  if (chn == 9)
	    {
	      synth_devs[dev]->set_instr (dev, voice, 128 + note);
	      note = 60;	/* Middle C */

	    }
	}

      if (seq_mode == SEQ_2)
	{
	  synth_devs[dev]->setup_voice (dev, voice, chn);
	}

      synth_devs[dev]->start_note (dev, voice, note, parm);
      break;

    case MIDI_NOTEOFF:
      if (voice == -1)
	voice = chn;
      synth_devs[dev]->kill_note (dev, voice, note, parm);
      break;

    case MIDI_KEY_PRESSURE:
      if (voice == -1)
	voice = chn;
      synth_devs[dev]->aftertouch (dev, voice, parm);
      break;

    default:;
    }
}

static void
seq_chn_common_event (unsigned char *event)
{
  unsigned char   dev = event[1];
  unsigned char   cmd = event[2];
  unsigned char   chn = event[3];
  unsigned char   p1 = event[4];

  /* unsigned char   p2 = event[5]; */
  unsigned short  w14 = *(short *) &event[6];

  if ((int) dev > max_synthdev)
    return;
  if (!(synth_open_mask & (1 << dev)))
    return;
  if (!synth_devs[dev])
    return;

  switch (cmd)
    {
    case MIDI_PGM_CHANGE:
      if (seq_mode == SEQ_2)
	{
	  synth_devs[dev]->chn_info[chn].pgm_num = p1;
	  if (dev >= num_synths)
	    synth_devs[dev]->set_instr (dev, chn, p1);
	}
      else
	synth_devs[dev]->set_instr (dev, chn, p1);

      break;

    case MIDI_CTL_CHANGE:

      if (seq_mode == SEQ_2)
	{
	  if (chn > 15 || p1 > 127)
	    break;

	  synth_devs[dev]->chn_info[chn].controllers[p1] = w14 & 0x7f;

	  if (dev < num_synths)
	    {
	      int             val = w14 & 0x7f;
	      int             i, key;

	      if (p1 < 64)	/* Combine MSB and LSB */
		{
		  val = ((synth_devs[dev]->
			  chn_info[chn].controllers[p1 & ~32] & 0x7f) << 7)
		    | (synth_devs[dev]->
		       chn_info[chn].controllers[p1 | 32] & 0x7f);
		  p1 &= ~32;
		}

	      /* Handle all playing notes on this channel */

	      key = (chn << 8);

	      for (i = 0; i < synth_devs[dev]->alloc.max_voice; i++)
		if ((synth_devs[dev]->alloc.map[i] & 0xff00) == key)
		  synth_devs[dev]->controller (dev, i, p1, val);
	    }
	  else
	    synth_devs[dev]->controller (dev, chn, p1, w14);
	}
      else			/* Mode 1 */
	synth_devs[dev]->controller (dev, chn, p1, w14);
      break;

    case MIDI_PITCH_BEND:
      if (seq_mode == SEQ_2)
	{
	  synth_devs[dev]->chn_info[chn].bender_value = w14;

	  if (dev < num_synths)
	    {			/* Handle all playing notes on this channel */
	      int             i, key;

	      key = (chn << 8);

	      for (i = 0; i < synth_devs[dev]->alloc.max_voice; i++)
		if ((synth_devs[dev]->alloc.map[i] & 0xff00) == key)
		  synth_devs[dev]->bender (dev, i, w14);
	    }
	  else
	    synth_devs[dev]->bender (dev, chn, w14);
	}
      else			/* MODE 1 */
	synth_devs[dev]->bender (dev, chn, w14);
      break;

    default:;
    }
}

static int
seq_timing_event (unsigned char *event)
{
  unsigned char   cmd = event[1];
  unsigned int    parm = *(int *) &event[4];

  if (seq_mode == SEQ_2)
    {
      int             ret;

      if ((ret = tmr->event (tmr_no, event)) == TIMER_ARMED)
	{
	  if ((SEQ_MAX_QUEUE - qlen) >= output_treshold)
	    {
	      unsigned long   flags;

	      DISABLE_INTR (flags);
	      if (SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
		{
		  WAKE_UP (seq_sleeper, seq_sleep_flag);
		}
	      RESTORE_INTR (flags);
#if defined(__FreeBSD__)
		/* must issue a wakeup for anyone waiting (select) XXX */
#endif
	    }
	}
      return ret;
    }

  switch (cmd)
    {
    case TMR_WAIT_REL:
      parm += prev_event_time;

      /*
         * NOTE!  No break here. Execution of TMR_WAIT_REL continues in the
         * next case (TMR_WAIT_ABS)
       */

    case TMR_WAIT_ABS:
      if (parm > 0)
	{
	  long            time;

	  seq_playing = 1;
	  time = parm;
	  prev_event_time = time;

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
#if defined(__FreeBSD__)
		/* must issue a wakeup for select XXX */
#endif
	    }

	  return TIMER_ARMED;
	}
      break;

    case TMR_START:
      seq_time = GET_TIME ();
      prev_input_time = 0;
      prev_event_time = 0;
      break;

    case TMR_STOP:
      break;

    case TMR_CONTINUE:
      break;

    case TMR_TEMPO:
      break;

    case TMR_ECHO:
      if (seq_mode == SEQ_2)
	seq_copy_to_input (event, 8);
      else
	{
	  parm = (parm << 8 | SEQ_ECHO);
	  seq_copy_to_input ((unsigned char *) &parm, 4);
	}
      break;

    default:;
    }

  return TIMER_NOT_ARMED;
}

static void
seq_local_event (unsigned char *event)
{
  /* unsigned char   cmd = event[1]; */

  printk ("seq_local_event() called. WHY????????\n");
}

static int
play_event (unsigned char *q)
{
  /*
     * NOTE! This routine returns
     *   0 = normal event played.
     *   1 = Timer armed. Suspend playback until timer callback.
     *   2 = MIDI output buffer full. Restore queue and suspend until timer
   */
  unsigned long  *delay;

  switch (q[0])
    {
    case SEQ_NOTEOFF:
      if (synth_open_mask & (1 << 0))
	if (synth_devs[0])
	  synth_devs[0]->kill_note (0, q[1], 255, q[3]);
      break;

    case SEQ_NOTEON:
      if (q[4] < 128 || q[4] == 255)
	if (synth_open_mask & (1 << 0))
	  if (synth_devs[0])
	    synth_devs[0]->start_note (0, q[1], q[2], q[3]);
      break;

    case SEQ_WAIT:
      delay = (unsigned long *) q;	/*
					 * Bytes 1 to 3 are containing the *
					 * delay in GET_TIME()
					 */
      *delay = (*delay >> 8) & 0xffffff;

      if (*delay > 0)
	{
	  long            time;

	  seq_playing = 1;
	  time = *delay;
	  prev_event_time = time;

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
#if defined(__FreeBSD__)
		/* must issue a wakeup for selects XXX */
#endif
	    }
	  /*
	     * The timer is now active and will reinvoke this function
	     * after the timer expires. Return to the caller now.
	   */
	  return 1;
	}
      break;

    case SEQ_PGMCHANGE:
      if (synth_open_mask & (1 << 0))
	if (synth_devs[0])
	  synth_devs[0]->set_instr (0, q[1], q[2]);
      break;

    case SEQ_SYNCTIMER:	/*
				   * Reset timer
				 */
      seq_time = GET_TIME ();
      prev_input_time = 0;
      prev_event_time = 0;
      break;

    case SEQ_MIDIPUTC:		/*
				 * Put a midi character
				 */
      if (midi_opened[q[2]])
	{
	  int             dev;

	  dev = q[2];

	  if (!midi_devs[dev]->putc (dev, q[1]))
	    {
	      /*
	         * Output FIFO is full. Wait one timer cycle and try again.
	       */

	      seq_playing = 1;
	      request_sound_timer (-1);
	      return 2;
	    }
	  else
	    midi_written[dev] = 1;
	}
      break;

    case SEQ_ECHO:
      seq_copy_to_input (q, 4);	/*
				   * Echo back to the process
				 */
      break;

    case SEQ_PRIVATE:
      if ((int) q[1] < max_synthdev)
	synth_devs[q[1]]->hw_control (q[1], q);
      break;

    case SEQ_EXTENDED:
      extended_event (q);
      break;

    case EV_CHN_VOICE:
      seq_chn_voice_event (q);
      break;

    case EV_CHN_COMMON:
      seq_chn_common_event (q);
      break;

    case EV_TIMING:
      if (seq_timing_event (q) == TIMER_ARMED)
	{
	  return 1;
	}
      break;

    case EV_SEQ_LOCAL:
      seq_local_event (q);
      break;

    default:;
    }

  return 0;
}

static void
seq_startplay (void)
{
  unsigned long   flags;
  int             this_one, action;

  while (qlen > 0)
    {

      DISABLE_INTR (flags);
      qhead = ((this_one = qhead) + 1) % SEQ_MAX_QUEUE;
      qlen--;
      RESTORE_INTR (flags);

      seq_playing = 1;

      if ((action = play_event (&queue[this_one * EV_SZ])))
	{			/* Suspend playback. Next timer routine invokes this routine again */
	  if (action == 2)
	    {
	      qlen++;
	      qhead = this_one;
	    }
	  return;
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
#if defined(__FreeBSD__)
	/* must issue a wakeup for selects XXX */
#endif
    }
}

static void
reset_controllers (int dev, unsigned char *controller, int update_dev)
{

  int             i;

  for (i = 0; i < 128; i++)
    controller[i] = ctrl_def_values[i];
}

static void
setup_mode2 (void)
{
  int             dev;

  max_synthdev = num_synths;

  for (dev = 0; dev < num_midis; dev++)
    if (midi_devs[dev]->converter != NULL)
      {
	synth_devs[max_synthdev++] =
	  midi_devs[dev]->converter;
      }

  for (dev = 0; dev < max_synthdev; dev++)
    {
      int             chn;

      for (chn = 0; chn < 16; chn++)
	{
	  synth_devs[dev]->chn_info[chn].pgm_num = 0;
	  reset_controllers (dev,
			     synth_devs[dev]->chn_info[chn].controllers,
			     0);
	  synth_devs[dev]->chn_info[chn].bender_value = (1 << 7);	/* Neutral */
	}
    }

  max_mididev = 0;
  seq_mode = SEQ_2;
}

int
sequencer_open (int dev, struct fileinfo *file)
{
  int             retval, mode, i;
  int             level, tmp;

  level = ((dev & 0x0f) == SND_DEV_SEQ2) ? 2 : 1;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  DEB (printk ("sequencer_open(dev=%d)\n", dev));

  if (!sequencer_ok)
    {
      printk ("Soundcard: Sequencer not initialized\n");
      return RET_ERROR (ENXIO);
    }

  if (dev)			/*
				 * Patch manager device
				 */
    {
      int             err;

      dev--;

      if (dev >= MAX_SYNTH_DEV)
	return RET_ERROR (ENXIO);
      if (pmgr_present[dev])
	return RET_ERROR (EBUSY);
      if ((err = pmgr_open (dev)) < 0)
	return err;		/*
				 * Failed
				 */

      pmgr_present[dev] = 1;
      return err;
    }

  if (sequencer_busy)
    {
      printk ("Sequencer busy\n");
      return RET_ERROR (EBUSY);
    }

  max_mididev = num_midis;
  max_synthdev = num_synths;
  pre_event_timeout = 0;
  seq_mode = SEQ_1;

  if (pending_timer != -1)
    {
      tmr_no = pending_timer;
      pending_timer = -1;
    }

  if (tmr_no == -1)		/* Not selected yet */
    {
      int             i, best;

      best = -1;
      for (i = 0; i < num_sound_timers; i++)
	if (sound_timer_devs[i]->priority > best)
	  {
	    tmr_no = i;
	    best = sound_timer_devs[i]->priority;
	  }

      if (tmr_no == -1)		/* Should not be */
	tmr_no = 0;
    }

  tmr = sound_timer_devs[tmr_no];

  if (level == 2)
    {
      if (tmr == NULL)
	{
	  printk ("sequencer: No timer for level 2\n");
	  return RET_ERROR (ENXIO);
	}
      setup_mode2 ();
    }

  if (seq_mode == SEQ_1 && (mode == OPEN_READ || mode == OPEN_READWRITE))
    if (!max_mididev)
      {
	printk ("Sequencer: No Midi devices. Input not possible\n");
	return RET_ERROR (ENXIO);
      }

  if (!max_synthdev && !max_mididev)
    return RET_ERROR (ENXIO);

  synth_open_mask = 0;

  for (i = 0; i < max_mididev; i++)
    {
      midi_opened[i] = 0;
      midi_written[i] = 0;
    }

  /*
   * if (mode == OPEN_WRITE || mode == OPEN_READWRITE)
   */
  for (i = 0; i < max_synthdev; i++)	/*
					 * Open synth devices
					 */
    if ((tmp = synth_devs[i]->open (i, mode)) < 0)
      {
	printk ("Sequencer: Warning! Cannot open synth device #%d (%d)\n", i, tmp);
	if (synth_devs[i]->midi_dev)
	  printk ("(Maps to MIDI dev #%d)\n", synth_devs[i]->midi_dev);
      }
    else
      {
	synth_open_mask |= (1 << i);
	if (synth_devs[i]->midi_dev)	/*
					 * Is a midi interface
					 */
	  midi_opened[synth_devs[i]->midi_dev] = 1;
      }

  seq_time = GET_TIME ();
  prev_input_time = 0;
  prev_event_time = 0;

  if (seq_mode == SEQ_1 && (mode == OPEN_READ || mode == OPEN_READWRITE))
    {				/*
				 * Initialize midi input devices
				 */
      for (i = 0; i < max_mididev; i++)
	if (!midi_opened[i])
	  {
	    if ((retval = midi_devs[i]->open (i, mode,
			 sequencer_midi_input, sequencer_midi_output)) >= 0)
	      midi_opened[i] = 1;
	  }
    }

  if (seq_mode == SEQ_2)
    {
      tmr->open (tmr_no, seq_mode);
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

  while (!PROCESS_ABORTING (seq_sleeper, seq_sleep_flag) && n)
    {
      n = 0;

      for (i = 0; i < max_mididev; i++)
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

  if (dev)			/*
				 * Patch manager device
				 */
    {
      dev--;
      pmgr_release (dev);
      pmgr_present[dev] = 0;
      return;
    }

  /*
   * * Wait until the queue is empty (if we don't have nonblock)
   */

  if (mode != OPEN_READ && !ISSET_FILE_FLAG (file, O_NONBLOCK))
    while (!PROCESS_ABORTING (seq_sleeper, seq_sleep_flag) && qlen)
      {
	seq_sync ();
      }

  if (mode != OPEN_READ)
    seq_drain_midi_queues ();	/*
				 * Ensure the output queues are empty
				 */
  seq_reset ();
  if (mode != OPEN_READ)
    seq_drain_midi_queues ();	/*
				 * Flush the all notes off messages
				 */

  for (i = 0; i < max_synthdev; i++)
    if (synth_open_mask & (1 << i))	/*
					 * Actually opened
					 */
      if (synth_devs[i])
	{
	  synth_devs[i]->close (i);

	  if (synth_devs[i]->midi_dev)
	    midi_opened[synth_devs[i]->midi_dev] = 0;
	}

  for (i = 0; i < num_synths; i++)
    if (pmgr_present[i])
      pmgr_inform (i, PM_E_CLOSED, 0, 0, 0, 0);

  for (i = 0; i < max_mididev; i++)
    if (midi_opened[i])
      midi_devs[i]->close (i);

  if (seq_mode == SEQ_2)
    tmr->close (tmr_no);

  sequencer_busy = 0;
}

static int
seq_sync (void)
{
  unsigned long   flags;

  if (qlen && !seq_playing && !PROCESS_ABORTING (seq_sleeper, seq_sleep_flag))
    seq_startplay ();

  DISABLE_INTR (flags);
  if (qlen && !SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
    {
      DO_SLEEP (seq_sleeper, seq_sleep_flag, 0);
    }
  RESTORE_INTR (flags);

  return qlen;
}

static void
midi_outc (int dev, unsigned char data)
{
  /*
   * NOTE! Calls sleep(). Don't call this from interrupt.
   */

  int             n;
  unsigned long   flags;

  /*
   * This routine sends one byte to the Midi channel.
   * If the output Fifo is full, it waits until there
   * is space in the queue
   */

  n = 3 * HZ;			/* Timeout */

  DISABLE_INTR (flags);
  while (n && !midi_devs[dev]->putc (dev, data))
    {
      DO_SLEEP (seq_sleeper, seq_sleep_flag, 4);
      n--;
    }
  RESTORE_INTR (flags);
}

static void
seq_reset (void)
{
  /*
   * NOTE! Calls sleep(). Don't call this from interrupt.
   */

  int             i;
  int             chn;
  unsigned long   flags;

  sound_stop_timer ();
  seq_time = GET_TIME ();
  prev_input_time = 0;
  prev_event_time = 0;

  qlen = qhead = qtail = 0;
  iqlen = iqhead = iqtail = 0;

  for (i = 0; i < max_synthdev; i++)
    if (synth_open_mask & (1 << i))
      if (synth_devs[i])
	synth_devs[i]->reset (i);

  if (seq_mode == SEQ_2)
    {

      for (chn = 0; chn < 16; chn++)
	for (i = 0; i < max_synthdev; i++)
	  if (synth_open_mask & (1 << i))
	    if (synth_devs[i])
	      {
		synth_devs[i]->controller (i, chn, 123, 0);	/* All notes off */
		synth_devs[i]->controller (i, chn, 121, 0);	/* Reset all ctl */
		synth_devs[i]->bender (i, chn, 1 << 13);	/* Bender off */
	      }

    }
  else
    /* seq_mode == SEQ_1 */
    {
      for (i = 0; i < max_mididev; i++)
	if (midi_written[i])	/*
				 * Midi used. Some notes may still be playing
				 */
	  {
	    /*
	       *      Sending just a ACTIVE SENSING message should be enough to stop all
	       *      playing notes. Since there are devices not recognizing the
	       *      active sensing, we have to send some all notes off messages also.
	     */
	    midi_outc (i, 0xfe);

	    for (chn = 0; chn < 16; chn++)
	      {
		midi_outc (i,
			   (unsigned char) (0xb0 + (chn & 0x0f)));	/* control change */
		midi_outc (i, 0x7b);	/* All notes off */
		midi_outc (i, 0);	/* Dummy parameter */
	      }

	    midi_devs[i]->close (i);

	    midi_written[i] = 0;
	    midi_opened[i] = 0;
	  }
    }

  seq_playing = 0;

  DISABLE_INTR (flags);
  if (SOMEONE_WAITING (seq_sleeper, seq_sleep_flag))
    {
      /*      printk ("Sequencer Warning: Unexpected sleeping process - Waking up\n"); */
      WAKE_UP (seq_sleeper, seq_sleep_flag);
    }
  RESTORE_INTR (flags);

}

static void
seq_panic (void)
{
  /*
     * This routine is called by the application in case the user
     * wants to reset the system to the default state.
   */

  seq_reset ();

  /*
     * Since some of the devices don't recognize the active sensing and
     * all notes off messages, we have to shut all notes manually.
     *
     *      TO BE IMPLEMENTED LATER
   */

  /*
     * Also return the controllers to their default states
   */
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
    case SNDCTL_TMR_TIMEBASE:
    case SNDCTL_TMR_TEMPO:
    case SNDCTL_TMR_START:
    case SNDCTL_TMR_STOP:
    case SNDCTL_TMR_CONTINUE:
    case SNDCTL_TMR_METRONOME:
    case SNDCTL_TMR_SOURCE:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      if (seq_mode != SEQ_2)
	return RET_ERROR (EINVAL);
      return tmr->ioctl (tmr_no, cmd, arg);
      break;

    case SNDCTL_TMR_SELECT:
      if (dev)			/* Patch manager */
	return RET_ERROR (EIO);

      if (seq_mode != SEQ_2)
	return RET_ERROR (EINVAL);
      pending_timer = IOCTL_IN (arg);

      if (pending_timer < 0 || pending_timer >= num_sound_timers)
	{
	  pending_timer = -1;
	  return RET_ERROR (EINVAL);
	}

      return IOCTL_OUT (arg, pending_timer);
      break;

    case SNDCTL_SEQ_PANIC:
      seq_panic ();
      break;

    case SNDCTL_SEQ_SYNC:
      if (dev)			/*
				 * Patch manager
				 */
	return RET_ERROR (EIO);

      if (mode == OPEN_READ)
	return 0;
      while (qlen && !PROCESS_ABORTING (seq_sleeper, seq_sleep_flag))
	seq_sync ();
      if (qlen)
	return RET_ERROR (EINTR);
      else
	return 0;
      break;

    case SNDCTL_SEQ_RESET:
      if (dev)			/*
				 * Patch manager
				 */
	return RET_ERROR (EIO);

      seq_reset ();
      return 0;
      break;

    case SNDCTL_SEQ_TESTMIDI:
      if (dev)			/*
				 * Patch manager
				 */
	return RET_ERROR (EIO);

      midi_dev = IOCTL_IN (arg);
      if (midi_dev >= max_mididev)
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
      if (dev)			/*
				 * Patch manager
				 */
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

      /*
       * If *arg == 0, just return the current rate
       */
      if (seq_mode == SEQ_2)
	return tmr->ioctl (tmr_no, cmd, arg);

      if (IOCTL_IN (arg) != 0)
	return RET_ERROR (EINVAL);

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
      return IOCTL_OUT (arg, max_synthdev);
      break;

    case SNDCTL_SEQ_NRMIDIS:
      return IOCTL_OUT (arg, max_mididev);
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

	if (dev < 0 || dev >= max_synthdev)
	  return RET_ERROR (ENXIO);

	if (!(synth_open_mask & (1 << dev)) && !orig_dev)
	  return RET_ERROR (EBUSY);

	return synth_devs[dev]->ioctl (dev, cmd, arg);
      }
      break;

    case SNDCTL_SEQ_OUTOFBAND:
      {
	struct seq_event_rec event;
	unsigned long   flags;

	IOCTL_FROM_USER ((char *) &event, (char *) arg, 0, sizeof (event));

	DISABLE_INTR (flags);
	play_event (event.arr);
	RESTORE_INTR (flags);

	return 0;
      }
      break;

    case SNDCTL_MIDI_INFO:
      {
	struct midi_info inf;
	int             dev;

	IOCTL_FROM_USER ((char *) &inf, (char *) arg, 0, sizeof (inf));
	dev = inf.device;

	if (dev < 0 || dev >= max_mididev)
	  return RET_ERROR (ENXIO);

	IOCTL_TO_USER ((char *) arg, 0, (char *) &(midi_devs[dev]->info), sizeof (inf));
	return 0;
      }
      break;

    case SNDCTL_PMGR_IFACE:
      {
	struct patmgr_info *inf;
	int             dev, err;

	if ((inf = (struct patmgr_info *) KERNEL_MALLOC (sizeof (*inf))) == NULL)
	  {
	    printk ("patmgr: Can't allocate memory for a message\n");
	    return RET_ERROR (EIO);
	  }

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

	if ((inf = (struct patmgr_info *) KERNEL_MALLOC (sizeof (*inf))) == NULL)
	  {
	    printk ("patmgr: Can't allocate memory for a message\n");
	    return RET_ERROR (EIO);
	  }

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

	if (dev)		/*
				 * Patch manager
				 */
	  return RET_ERROR (EIO);

	if (tmp < 1)
	  tmp = 1;
	if (tmp >= SEQ_MAX_QUEUE)
	  tmp = SEQ_MAX_QUEUE - 1;
	output_treshold = tmp;
	return 0;
      }
      break;

    case SNDCTL_MIDI_PRETIME:
      {
	int             val = IOCTL_IN (arg);

	if (val < 0)
	  val = 0;

	val = (HZ * val) / 10;
	pre_event_timeout = val;
	return IOCTL_OUT (arg, val);
      }
      break;

    default:
      if (dev)			/*
				 * Patch manager
				 */
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
sequencer_poll (int dev, struct fileinfo *file, int events, select_table * wait)
{
  unsigned long   flags;
  int revents = 0;

  dev = dev >> 4;

  DISABLE_INTR (flags);

  if (events & (POLLIN | POLLRDNORM))
    if (!iqlen)
      selrecord(wait, &selinfo[dev]);
    else {
      revents |= events & (POLLIN | POLLRDNORM);
      midi_sleep_flag.mode &= ~WK_SLEEP;
    }

  if (events & (POLLOUT | POLLWRNORM))
    if (qlen >= SEQ_MAX_QUEUE)
      selrecord(wait, &selinfo[dev]);
    else {
      revents |= events & (POLLOUT | POLLWRNORM);
      seq_sleep_flag.mode &= ~WK_SLEEP;
    }

  RESTORE_INTR (flags);

  return (revents);
}

#endif

void
sequencer_timer (void *arg)
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
  };

#define BASE_OCTAVE	5

  octave = note_num / 12;
  note = note_num % 12;

  note_freq = notes[note];

  if (octave < BASE_OCTAVE)
    note_freq >>= (BASE_OCTAVE - octave);
  else if (octave > BASE_OCTAVE)
    note_freq <<= (octave - BASE_OCTAVE);

  /*
   * note_freq >>= 1;
   */

  return note_freq;
}

unsigned long
compute_finetune (unsigned long base_freq, int bend, int range)
{
  unsigned long   amount;
  int             negative, semitones, cents, multiplier = 1;

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

  /*
     if (bend > 2399)
     bend = 2399;
   */
  while (bend > 2399)
    {
      multiplier *= 4;
      bend -= 2400;
    }

  semitones = bend / 100;
  cents = bend % 100;

  amount = (int) (semitone_tuning[semitones] * multiplier * cent_tuning[cents])
    / 10000;

  if (negative)
    return (base_freq * 10000) / amount;	/*
						 * Bend down
						 */
  else
    return (base_freq * amount) / 10000;	/*
						 * Bend up
						 */
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
/*
 * Stub version
 */
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

#ifdef ALLOW_SELECT
int
sequencer_poll (int dev, struct fileinfo *file, int events, select_table * wait)
{
  return (events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM | POLLHUP);
}

#endif

#endif

#endif
