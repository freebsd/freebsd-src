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


#if defined(CONFIG_MIDI)

#include <sys/select.h>

/*
 * Don't make MAX_QUEUE_SIZE larger than 4000
 */

#define MAX_QUEUE_SIZE	4000
int
MIDIbuf_poll (int dev, struct fileinfo *file, int events, select_table * wait);

static void
drain_midi_queue(int dev);

static int     *midi_sleeper[MAX_MIDI_DEV] = {NULL};
static volatile struct snd_wait midi_sleep_flag[MAX_MIDI_DEV] = { {0}};
static int     *input_sleeper[MAX_MIDI_DEV] = {NULL};
static volatile struct snd_wait input_sleep_flag[MAX_MIDI_DEV] = { {0}};

struct midi_buf {
	int             len, head, tail;
	u_char   queue[MAX_QUEUE_SIZE];
};

struct midi_parms {
	int             prech_timeout;	/* Timeout before the first ch */
};

static struct midi_buf *midi_out_buf[MAX_MIDI_DEV] = {NULL};
static struct midi_buf *midi_in_buf[MAX_MIDI_DEV] = {NULL};
static struct midi_parms parms[MAX_MIDI_DEV];

static void     midi_poll(void  *dummy);

static volatile int open_devs = 0;

#define DATA_AVAIL(q) (q->len)
#define SPACE_AVAIL(q) (MAX_QUEUE_SIZE - q->len)

#define QUEUE_BYTE(q, data) \
	if (SPACE_AVAIL(q)) { \
	  u_long flags; \
	  flags = splhigh(); \
	  q->queue[q->tail] = (data); \
	  q->len++; q->tail = (q->tail+1) % MAX_QUEUE_SIZE; \
	  splx(flags); \
	}

#define REMOVE_BYTE(q, data) \
	if (DATA_AVAIL(q)) { \
	  u_long flags; \
	  flags = splhigh(); \
	  data = q->queue[q->head]; \
	  q->len--; q->head = (q->head+1) % MAX_QUEUE_SIZE; \
	  splx(flags); \
	}

static void
drain_midi_queue(int dev)
{

    /*
     * Give the Midi driver time to drain its output queues
     */

    if (midi_devs[dev]->buffer_status != NULL)
	while (!(PROCESS_ABORTING (midi_sleep_flag[dev])) &&
		   midi_devs[dev]->buffer_status(dev)) {
	    int    chn;

	    midi_sleeper[dev] = &chn;
	    DO_SLEEP(chn, midi_sleep_flag[dev], hz / 10);

	};
}

static void
midi_input_intr(int dev, u_char data)
{
	if (midi_in_buf[dev] == NULL)
		return;

	if (data == 0xfe)	/* Active sensing */
		return;		/* Ignore */

	if (SPACE_AVAIL(midi_in_buf[dev])) {
		QUEUE_BYTE(midi_in_buf[dev], data);
		if ((input_sleep_flag[dev].mode & WK_SLEEP)) {
			input_sleep_flag[dev].mode = WK_WAKEUP;
			wakeup(input_sleeper[dev]);
		};
	}
}

static void
midi_output_intr(int dev)
{
	/*
	 * Currently NOP
	 */
}

static void
midi_poll(void *dummy)
{
	u_long   flags;
	int             dev;

	flags = splhigh();
	if (open_devs) {
		for (dev = 0; dev < num_midis; dev++)
			if (midi_out_buf[dev] != NULL) {
				while (DATA_AVAIL(midi_out_buf[dev]) &&
				       midi_devs[dev]->putc(dev,
							    midi_out_buf[dev]->queue[midi_out_buf[dev]->head])) {
					midi_out_buf[dev]->head = (midi_out_buf[dev]->head + 1) % MAX_QUEUE_SIZE;
					midi_out_buf[dev]->len--;
				}

				if (DATA_AVAIL(midi_out_buf[dev]) < 100 &&
				    (midi_sleep_flag[dev].mode & WK_SLEEP)) {
					midi_sleep_flag[dev].mode = WK_WAKEUP;
					wakeup(midi_sleeper[dev]);
				};
			}
		timeout( midi_poll, 0, 1);;	/* Come back later */
	}
	splx(flags);
}

int
MIDIbuf_open(int dev, struct fileinfo * file)
{
	int             mode, err;

	dev = dev >> 4;
	mode = file->mode & O_ACCMODE;

	if (num_midis > MAX_MIDI_DEV) {
		printf("Sound: FATAL ERROR: Too many midi interfaces\n");
		num_midis = MAX_MIDI_DEV;
	}
	if (dev < 0 || dev >= num_midis) {
		printf("Sound: Nonexistent MIDI interface %d\n", dev);
		return -(ENXIO);
	}
	/*
	 * Interrupts disabled. Be careful
	 */

	if ((err = midi_devs[dev]->open(dev, mode,
				  midi_input_intr, midi_output_intr)) < 0) {
		return err;
	}
	parms[dev].prech_timeout = 0;

	midi_in_buf[dev] = (struct midi_buf *) malloc(sizeof(struct midi_buf), M_TEMP, M_WAITOK);

	if (midi_in_buf[dev] == NULL) {
		printf("midi: Can't allocate buffer\n");
		midi_devs[dev]->close(dev);
		return -(EIO);
	}
	midi_in_buf[dev]->len = midi_in_buf[dev]->head = midi_in_buf[dev]->tail = 0;

	midi_out_buf[dev] = (struct midi_buf *) malloc(sizeof(struct midi_buf), M_TEMP, M_WAITOK);

	if (midi_out_buf[dev] == NULL) {
		printf("midi: Can't allocate buffer\n");
		midi_devs[dev]->close(dev);
		free(midi_in_buf[dev], M_TEMP);
		midi_in_buf[dev] = NULL;
		return -(EIO);
	}
	midi_out_buf[dev]->len = midi_out_buf[dev]->head = midi_out_buf[dev]->tail = 0;
	open_devs++;

	{
		midi_sleep_flag[dev].aborting = 0;
		midi_sleep_flag[dev].mode = WK_NONE;
	};
	{
		input_sleep_flag[dev].aborting = 0;
		input_sleep_flag[dev].mode = WK_NONE;
	};

	if (open_devs < 2) {	/* This was first open */
		{
		};

		timeout( midi_poll, 0, 1);;	/* Start polling */
	}
	return err;
}

void
MIDIbuf_release(int dev, struct fileinfo * file)
{
	int             mode;
	u_long   flags;

	dev = dev >> 4;
	mode = file->mode & O_ACCMODE;

	if (dev < 0 || dev >= num_midis)
		return;

	flags = splhigh();

	/*
	 * Wait until the queue is empty
	 */

	if (mode != OPEN_READ) {
		midi_devs[dev]->putc(dev, 0xfe);	/* Active sensing to
							 * shut the devices */

		while (!(PROCESS_ABORTING (midi_sleep_flag[dev])) &&
		       DATA_AVAIL(midi_out_buf[dev])) {
			int   chn;
			midi_sleeper[dev] = &chn;
			DO_SLEEP(chn, midi_sleep_flag[dev], 0);

		};		/* Sync */

		drain_midi_queue(dev);	/* Ensure the output queues are empty */
	}
	splx(flags);

	midi_devs[dev]->close(dev);

	free(midi_in_buf[dev], M_TEMP);
	free(midi_out_buf[dev], M_TEMP);
	midi_in_buf[dev] = NULL;
	midi_out_buf[dev] = NULL;
	if (open_devs < 2) {
	};
	open_devs--;
}

int
MIDIbuf_write(int dev, struct fileinfo * file, snd_rw_buf * buf, int count)
{
	u_long   flags;
	int             c, n, i;
	u_char   tmp_data;

	dev = dev >> 4;

	if (!count)
		return 0;

	flags = splhigh();

	c = 0;

	while (c < count) {
		n = SPACE_AVAIL(midi_out_buf[dev]);

		if (n == 0) {	/* No space just now. We have to sleep */

			{
				int  chn;

				midi_sleeper[dev] = &chn;
				DO_SLEEP(chn, midi_sleep_flag[dev], 0);
			};

			if (PROCESS_ABORTING(midi_sleep_flag[dev])) {
				splx(flags);
				return -(EINTR);
			}
			n = SPACE_AVAIL(midi_out_buf[dev]);
		}
		if (n > (count - c))
			n = count - c;

		for (i = 0; i < n; i++) {

			if (uiomove((char *) &tmp_data, 1, buf)) {
				printf("sb: Bad copyin()!\n");
			};
			QUEUE_BYTE(midi_out_buf[dev], tmp_data);
			c++;
		}
	}

	splx(flags);

	return c;
}


int
MIDIbuf_read(int dev, struct fileinfo * file, snd_rw_buf * buf, int count)
{
	int             n, c = 0;
	u_long   flags;
	u_char   tmp_data;

	dev = dev >> 4;

	flags = splhigh();

	if (!DATA_AVAIL(midi_in_buf[dev])) {	/* No data yet, wait */

		{
			int   chn;


			input_sleeper[dev] = &chn;
			DO_SLEEP(chn, input_sleep_flag[dev],  
				 parms[dev].prech_timeout);

		};
		if (PROCESS_ABORTING(input_sleep_flag[dev]))
			c = -(EINTR);	/* The user is getting restless */
	}
	if (c == 0 && DATA_AVAIL(midi_in_buf[dev])) {	/* Got some bytes */
		n = DATA_AVAIL(midi_in_buf[dev]);
		if (n > count)
			n = count;
		c = 0;

		while (c < n) {
			REMOVE_BYTE(midi_in_buf[dev], tmp_data);

			if (uiomove((char *) &tmp_data, 1, buf)) {
				printf("sb: Bad copyout()!\n");
			};
			c++;
		}
	}
	splx(flags);

	return c;
}

int
MIDIbuf_ioctl(int dev, struct fileinfo * file, u_int cmd, ioctl_arg arg)
{
	int             val;

	dev = dev >> 4;

	if (((cmd >> 8) & 0xff) == 'C') {
		if (midi_devs[dev]->coproc)	/* Coprocessor ioctl */
			return midi_devs[dev]->coproc->ioctl(midi_devs[dev]->coproc->devc, cmd, arg, 0);
		else
			printf("/dev/midi%d: No coprocessor for this device\n", dev);

		return -(ENXIO);
	} else
		switch (cmd) {

		case SNDCTL_MIDI_PRETIME:
			val = (int) (*(int *) arg);
			if (val < 0)
				val = 0;

			val = (hz * val) / 10;
			parms[dev].prech_timeout = val;
			return *(int *) arg = val;
			break;

		default:
			return midi_devs[dev]->ioctl(dev, cmd, arg);
		}
}

#ifdef ALLOW_POLL
int
MIDIbuf_poll (int dev, struct fileinfo *file, int events, select_table * wait)
{
  int revents = 0;

  dev = dev >> 4;

  if (events & (POLLIN | POLLRDNORM))
    if (!DATA_AVAIL (midi_in_buf[dev]))
      selrecord(wait, &selinfo[dev]);
    else
      revents |= events & (POLLIN | POLLRDNORM);

  if (events & (POLLOUT | POLLWRNORM))
    if (SPACE_AVAIL (midi_out_buf[dev]))
      selrecord(wait, &selinfo[dev]);
    else
      revents |= events & (POLLOUT | POLLWRNORM);

  return revents;
}

#endif /* ALLOW_SELECT */



#endif
