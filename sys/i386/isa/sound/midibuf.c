/*
 * sound/midibuf.c
 *
 * Device file manager for /dev/midi#
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

#include <i386/isa/sound/sound_config.h>

static void drain_midi_queue __P((int dev));

#if defined(CONFIGURE_SOUNDCARD) && !defined(EXCLUDE_MIDI)

/*
 * Don't make MAX_QUEUE_SIZE larger than 4000
 */

#define MAX_QUEUE_SIZE	4000

DEFINE_WAIT_QUEUES (midi_sleeper[MAX_MIDI_DEV], midi_sleep_flag[MAX_MIDI_DEV]);
DEFINE_WAIT_QUEUES (input_sleeper[MAX_MIDI_DEV], input_sleep_flag[MAX_MIDI_DEV]);

struct midi_buf
  {
    int             len, head, tail;
    unsigned char   queue[MAX_QUEUE_SIZE];
  };

struct midi_parms
  {
    int             prech_timeout;	/*
					 * Timeout before the first ch
					 */
  };

static struct midi_buf *midi_out_buf[MAX_MIDI_DEV] =
{NULL};
static struct midi_buf *midi_in_buf[MAX_MIDI_DEV] =
{NULL};
static struct midi_parms parms[MAX_MIDI_DEV];

static void     midi_poll (void *dummy);

DEFINE_TIMER (poll_timer, midi_poll);
static volatile int open_devs = 0;

#define DATA_AVAIL(q) (q->len)
#define SPACE_AVAIL(q) (MAX_QUEUE_SIZE - q->len)

#define QUEUE_BYTE(q, data) \
	if (SPACE_AVAIL(q)) \
	{ \
	  unsigned long flags; \
	  DISABLE_INTR(flags); \
	  q->queue[q->tail] = (data); \
	  q->len++; q->tail = (q->tail+1) % MAX_QUEUE_SIZE; \
	  RESTORE_INTR(flags); \
	}

#define REMOVE_BYTE(q, data) \
	if (DATA_AVAIL(q)) \
	{ \
	  unsigned long flags; \
	  DISABLE_INTR(flags); \
	  data = q->queue[q->head]; \
	  q->len--; q->head = (q->head+1) % MAX_QUEUE_SIZE; \
	  RESTORE_INTR(flags); \
	}

static void
drain_midi_queue (int dev)
{

  /*
   * Give the Midi driver time to drain its output queues
   */

  if (midi_devs[dev]->buffer_status != NULL)
    while (!PROCESS_ABORTING (midi_sleeper[dev], midi_sleep_flag[dev]) &&
	   midi_devs[dev]->buffer_status (dev))
      DO_SLEEP (midi_sleeper[dev], midi_sleep_flag[dev], HZ / 10);
}

static void
midi_input_intr (int dev, unsigned char data)
{
  if (midi_in_buf[dev] == NULL)
    return;

  if (data == 0xfe)		/*
				 * Active sensing
				 */
    return;			/*
				 * Ignore
				 */

  if (SPACE_AVAIL (midi_in_buf[dev]))
    {
      QUEUE_BYTE (midi_in_buf[dev], data);
      if (SOMEONE_WAITING (input_sleeper[dev], input_sleep_flag[dev]))
	WAKE_UP (input_sleeper[dev], input_sleep_flag[dev]);
    }
#if defined(__FreeBSD__)
  if (selinfo[dev].si_pid)
    selwakeup(&selinfo[dev]);
#endif
}

static void
midi_output_intr (int dev)
{
  /*
   * Currently NOP
   */
#if defined(__FreeBSD__)
  if (selinfo[dev].si_pid)
    selwakeup(&selinfo[dev]);
#endif
}

static void
midi_poll (void *dummy)
{
  unsigned long   flags;
  int             dev;

  DISABLE_INTR (flags);
  if (open_devs)
    {
      for (dev = 0; dev < num_midis; dev++)
	if (midi_out_buf[dev] != NULL)
	  {
	    while (DATA_AVAIL (midi_out_buf[dev]) &&
		   midi_devs[dev]->putc (dev,
			 midi_out_buf[dev]->queue[midi_out_buf[dev]->head]))
	      {
		midi_out_buf[dev]->head = (midi_out_buf[dev]->head + 1) % MAX_QUEUE_SIZE;
		midi_out_buf[dev]->len--;
	      }

	    if (DATA_AVAIL (midi_out_buf[dev]) < 100 &&
		SOMEONE_WAITING (midi_sleeper[dev], midi_sleep_flag[dev]))
	      WAKE_UP (midi_sleeper[dev], midi_sleep_flag[dev]);
	  }
      ACTIVATE_TIMER (poll_timer, midi_poll, 1);	/*
							 * Come back later
							 */
    }
  RESTORE_INTR (flags);
}

int
MIDIbuf_open (int dev, struct fileinfo *file)
{
  int             mode, err;
  unsigned long   flags;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  if (num_midis > MAX_MIDI_DEV)
    {
      printk ("Sound: FATAL ERROR: Too many midi interfaces\n");
      num_midis = MAX_MIDI_DEV;
    }

  if (dev < 0 || dev >= num_midis)
    {
      printk ("Sound: Nonexistent MIDI interface %d\n", dev);
      return RET_ERROR (ENXIO);
    }

  /*
     *    Interrupts disabled. Be careful
   */

  DISABLE_INTR (flags);
  if ((err = midi_devs[dev]->open (dev, mode,
				   midi_input_intr, midi_output_intr)) < 0)
    {
      RESTORE_INTR (flags);
      return err;
    }

  parms[dev].prech_timeout = 0;

  RESET_WAIT_QUEUE (midi_sleeper[dev], midi_sleep_flag[dev]);
  RESET_WAIT_QUEUE (input_sleeper[dev], input_sleep_flag[dev]);

  midi_in_buf[dev] = (struct midi_buf *) KERNEL_MALLOC (sizeof (struct midi_buf));

  if (midi_in_buf[dev] == NULL)
    {
      printk ("midi: Can't allocate buffer\n");
      midi_devs[dev]->close (dev);
      RESTORE_INTR (flags);
      return RET_ERROR (EIO);
    }
  midi_in_buf[dev]->len = midi_in_buf[dev]->head = midi_in_buf[dev]->tail = 0;

  midi_out_buf[dev] = (struct midi_buf *) KERNEL_MALLOC (sizeof (struct midi_buf));

  if (midi_out_buf[dev] == NULL)
    {
      printk ("midi: Can't allocate buffer\n");
      midi_devs[dev]->close (dev);
      KERNEL_FREE (midi_in_buf[dev]);
      midi_in_buf[dev] = NULL;
      RESTORE_INTR (flags);
      return RET_ERROR (EIO);
    }
  midi_out_buf[dev]->len = midi_out_buf[dev]->head = midi_out_buf[dev]->tail = 0;
  if (!open_devs)
    ACTIVATE_TIMER (poll_timer, midi_poll, 1);	/*
						 * Come back later
						 */
  open_devs++;
  RESTORE_INTR (flags);

  return err;
}

void
MIDIbuf_release (int dev, struct fileinfo *file)
{
  int             mode;
  unsigned long   flags;

  dev = dev >> 4;
  mode = file->mode & O_ACCMODE;

  DISABLE_INTR (flags);

  /*
     * Wait until the queue is empty
   */

  if (mode != OPEN_READ)
    {
      midi_devs[dev]->putc (dev, 0xfe);		/*
						   * Active sensing to shut the
						   * devices
						 */

      while (!PROCESS_ABORTING (midi_sleeper[dev], midi_sleep_flag[dev]) &&
	     DATA_AVAIL (midi_out_buf[dev]))
	DO_SLEEP (midi_sleeper[dev], midi_sleep_flag[dev], 0);	/*
								 * Sync
								 */

      drain_midi_queue (dev);	/*
				 * Ensure the output queues are empty
				 */
    }

  midi_devs[dev]->close (dev);
  KERNEL_FREE (midi_in_buf[dev]);
  KERNEL_FREE (midi_out_buf[dev]);
  midi_in_buf[dev] = NULL;
  midi_out_buf[dev] = NULL;
  open_devs--;
  RESTORE_INTR (flags);
}

int
MIDIbuf_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  unsigned long   flags;
  int             c, n, i;
  unsigned char   tmp_data;

  dev = dev >> 4;

  if (!count)
    return 0;

  DISABLE_INTR (flags);

  c = 0;

  while (c < count)
    {
      n = SPACE_AVAIL (midi_out_buf[dev]);

      if (n == 0)		/*
				 * No space just now. We have to sleep
				 */
	{
	  DO_SLEEP (midi_sleeper[dev], midi_sleep_flag[dev], 0);
	  if (PROCESS_ABORTING (midi_sleeper[dev], midi_sleep_flag[dev]))
	    {
	      RESTORE_INTR (flags);
	      return RET_ERROR (EINTR);
	    }

	  n = SPACE_AVAIL (midi_out_buf[dev]);
	}

      if (n > (count - c))
	n = count - c;

      for (i = 0; i < n; i++)
	{
	  COPY_FROM_USER (&tmp_data, buf, c, 1);
	  QUEUE_BYTE (midi_out_buf[dev], tmp_data);
	  c++;
	}
    }

  RESTORE_INTR (flags);

  return c;
}


int
MIDIbuf_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count)
{
  int             n, c = 0;
  unsigned long   flags;
  unsigned char   tmp_data;

  dev = dev >> 4;

  DISABLE_INTR (flags);

  if (!DATA_AVAIL (midi_in_buf[dev]))	/*
					 * No data yet, wait
					 */
    {
      DO_SLEEP (input_sleeper[dev], input_sleep_flag[dev],
		parms[dev].prech_timeout);
      if (PROCESS_ABORTING (input_sleeper[dev], input_sleep_flag[dev]))
	c = RET_ERROR (EINTR);	/*
				 * The user is getting restless
				 */
    }

  if (c == 0 && DATA_AVAIL (midi_in_buf[dev]))	/*
						 * Got some bytes
						 */
    {
      n = DATA_AVAIL (midi_in_buf[dev]);
      if (n > count)
	n = count;
      c = 0;

      while (c < n)
	{
	  REMOVE_BYTE (midi_in_buf[dev], tmp_data);
	  COPY_TO_USER (buf, c, &tmp_data, 1);
	  c++;
	}
    }

  RESTORE_INTR (flags);

  return c;
}

int
MIDIbuf_ioctl (int dev, struct fileinfo *file,
	       unsigned int cmd, unsigned int arg)
{
  int             val;

  dev = dev >> 4;

  if (((cmd >> 8) & 0xff) == 'C')
    {
      if (midi_devs[dev]->coproc)	/* Coprocessor ioctl */
	return midi_devs[dev]->coproc->ioctl (midi_devs[dev]->coproc->devc, cmd, arg, 0);
      else
	printk ("/dev/midi%d: No coprocessor for this device\n", dev);

      return RET_ERROR (EREMOTEIO);
    }
  else
    switch (cmd)
      {

      case SNDCTL_MIDI_PRETIME:
	val = IOCTL_IN (arg);
	if (val < 0)
	  val = 0;

	val = (HZ * val) / 10;
	parms[dev].prech_timeout = val;
	return IOCTL_OUT (arg, val);
	break;

      default:
	return midi_devs[dev]->ioctl (dev, cmd, arg);
      }
}

#ifdef ALLOW_SELECT
int
MIDIbuf_select (int dev, struct fileinfo *file, int sel_type, select_table * wait)
{
  dev = dev >> 4;

  switch (sel_type)
    {
    case SEL_IN:
      if (!DATA_AVAIL (midi_in_buf[dev]))
	{
#if defined(__FreeBSD__)
	  selrecord(wait, &selinfo[dev]);
#else
	  input_sleep_flag[dev].mode = WK_SLEEP;
	  select_wait (&input_sleeper[dev], wait);
#endif
	  return 0;
	}
      return 1;
      break;

    case SEL_OUT:
      if (SPACE_AVAIL (midi_out_buf[dev]))
	{
#if defined(__FreeBSD__)
	  selrecord(wait, &selinfo[dev]);
#else
	  midi_sleep_flag[dev].mode = WK_SLEEP;
	  select_wait (&midi_sleeper[dev], wait);
#endif
	  return 0;
	}
      return 1;
      break;

    case SEL_EX:
      return 0;
    }

  return 0;
}

#endif /* ALLOW_SELECT */

long
MIDIbuf_init (long mem_start)
{
  return mem_start;
}

#endif
