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
 * $FreeBSD$
 */

#define SEQUENCER_C
#include <i386/isa/sound/sound_config.h>

#if NSND > 0

#ifdef CONFIG_SEQUENCER

#include <sys/select.h>

#include <i386/isa/sound/midi_ctrl.h>

static void seq_drain_midi_queues(void);
int
sequencer_poll (int dev, struct fileinfo *file, int events, select_table * wait);
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
 * The seq_mode gives the operating mode of the sequencer: 1 = level1 (the
 * default) 2 = level2 (extended capabilites)
 */

#define SEQ_1	1
#define SEQ_2	2
static int      seq_mode = SEQ_1;

static int     *seq_sleeper = NULL;
static volatile struct snd_wait seq_sleep_flag = {0};
static int     *midi_sleeper = NULL;
static volatile struct snd_wait midi_sleep_flag = {0};

static int      midi_opened[MAX_MIDI_DEV] = {0};
static int      midi_written[MAX_MIDI_DEV] = {0};

static u_long	prev_input_time = 0;
static int	prev_event_time;
static u_long	seq_time = 0;

#include <i386/isa/sound/tuning.h>

#define EV_SZ	8
#define IEV_SZ	8
static u_char *queue = NULL;
static u_char *iqueue = NULL;

static volatile int qhead = 0, qtail = 0, qlen = 0;
static volatile int iqhead = 0, iqtail = 0, iqlen = 0;
static volatile int seq_playing = 0;
static volatile int sequencer_busy = 0;
static int      output_treshold;
static int      pre_event_timeout;
static u_int synth_open_mask;

static int      seq_queue(u_char *note, char nonblock);
static void     seq_startplay(void);
static int      seq_sync(void);
static void     seq_reset(void);
static int      pmgr_present[MAX_SYNTH_DEV] =
{0};
static struct callout_handle sequencertimeout_ch
    = CALLOUT_HANDLE_INITIALIZER(&sequencertimeout_ch);

#if MAX_SYNTH_DEV > 15
#error Too many synthesizer devices enabled.
#endif

/*
 * sound_timer stuff -- originally in soundcard.c
 *
 * A negative value means a relative timeout in |count| ticks.
 * A positive value is used for what ?
 *
 * In any case, this is only used in sequencer.c
 */

static int      timer_running = 0;

void
request_sound_timer(int count)
{
    static int      current = 0;
    int             tmp = count;

    if (count < 0)
	sequencertimeout_ch = timeout(sequencer_timer, 0, -count);
    else {

	if (count < current)
	    current = 0;	/* Timer restarted */

	count = count - current;
	current = tmp;
	if (!count)
	    count = 1;
	sequencertimeout_ch = timeout(sequencer_timer, 0, count);
    }
    timer_running = 1;
}

void
sound_stop_timer(void)
{
    if (timer_running)
	untimeout( sequencer_timer, 0, sequencertimeout_ch);
    timer_running = 0;
}

int
sequencer_read(int dev, struct fileinfo * file, snd_rw_buf * buf, int count)
{
    int             c = count, p = 0;
    int             ev_len;
    u_long   flags;

    dev = dev >> 4;

    ev_len = seq_mode == SEQ_1 ? 4 : 8;

    if (dev)		/* Patch manager device */
	return pmgr_read(dev - 1, file, buf, count);

    flags = splhigh();
    if (!iqlen) {
	int             chn;


	midi_sleeper = &chn;
	DO_SLEEP(chn, midi_sleep_flag, pre_event_timeout); 

	if (!iqlen) {
	    splx(flags);
	    return 0;
	}
    }
    while (iqlen && c >= ev_len) {

	if (uiomove((char *) &iqueue[iqhead * IEV_SZ], ev_len, buf))
	    printf("sb: Bad copyout()!\n");
	p += ev_len;
	c -= ev_len;

	iqhead = (iqhead + 1) % SEQ_MAX_QUEUE;
	iqlen--;
    }
    splx(flags);

    return count - c;
}

static void
sequencer_midi_output(int dev)
{
    /*
     * Currently NOP
     */
}

void
seq_copy_to_input(u_char *event, int len)
{
    u_long   flags;

    /*
     * Verify that the len is valid for the current mode.
     */

    if (len != 4 && len != 8)
	return;
    if ((seq_mode == SEQ_1) != (len == 4))
	return;

    if (iqlen >= (SEQ_MAX_QUEUE - 1))
	return;		/* Overflow */

    flags = splhigh();
    bcopy(event, &iqueue[iqtail * IEV_SZ], len);
    iqlen++;
    iqtail = (iqtail + 1) % SEQ_MAX_QUEUE;

    if ((midi_sleep_flag.mode & WK_SLEEP)) {
	midi_sleep_flag.mode = WK_WAKEUP;
	wakeup(midi_sleeper);
    }
    splx(flags);
}

static void
sequencer_midi_input(int dev, u_char data)
{
    u_int    tstamp;
    u_char   event[4];

    if (data == 0xfe)	/* Ignore active sensing */
	return;

    tstamp = get_time() - seq_time;
    if (tstamp != prev_input_time) {
	tstamp = (tstamp << 8) | SEQ_WAIT;

	seq_copy_to_input((u_char *) &tstamp, 4);
	prev_input_time = tstamp;
    }
    event[0] = SEQ_MIDIPUTC;
    event[1] = data;
    event[2] = dev;
    event[3] = 0;

    seq_copy_to_input(event, 4);
}

void
seq_input_event(u_char *event, int len)
{
    u_long   this_time;

    if (seq_mode == SEQ_2)
	this_time = tmr->get_time(tmr_no);
    else
	this_time = get_time() - seq_time;

    if (this_time != prev_input_time) {
	u_char   tmp_event[8];

	tmp_event[0] = EV_TIMING;
	tmp_event[1] = TMR_WAIT_ABS;
	tmp_event[2] = 0;
	tmp_event[3] = 0;
	*(u_long *) &tmp_event[4] = this_time;

	seq_copy_to_input(tmp_event, 8);
	prev_input_time = this_time;
    }
    seq_copy_to_input(event, len);
}

int
sequencer_write(int dev, struct fileinfo * file, snd_rw_buf * buf, int count)
{
    u_char   event[EV_SZ], ev_code;
    int             p = 0, c, ev_size;
    int             err;
    int             mode = file->mode & O_ACCMODE;

    dev = dev >> 4;

    DEB(printf("sequencer_write(dev=%d, count=%d)\n", dev, count));

    if (mode == OPEN_READ)
	return -(EIO);

    if (dev)
	return pmgr_write(dev - 1, file, buf, count);

    c = count;

    while (c >= 4) {

	if (uiomove((char *) event, 4, buf))
	    printf("sb: Bad copyin()!\n");
	ev_code = event[0];

	if (ev_code == SEQ_FULLSIZE) {
	    int             err;

	    dev = *(u_short *) &event[2];
	    if (dev < 0 || dev >= max_synthdev)
		return -(ENXIO);

	    if (!(synth_open_mask & (1 << dev)))
		return -(ENXIO);

	    err = synth_devs[dev]->load_patch(dev,
			*(short *) &event[0], buf, p + 4, c, 0);
	    if (err < 0)
		return err;

	    return err;
	}
	if (ev_code >= 128) {
	    if (seq_mode == SEQ_2 && ev_code == SEQ_EXTENDED) {
		printf("Sequencer: Invalid level 2 event %x\n", ev_code);
		return -(EINVAL);
	    }
	    ev_size = 8;

	    if (c < ev_size) {
		if (!seq_playing)
		    seq_startplay();
		return count - c;
	    }
	    if (uiomove((char *) &event[4], 4, buf))
		printf("sb: Bad copyin()!\n");
	} else {
	    if (seq_mode == SEQ_2) {
		printf("Sequencer: 4 byte event in level 2 mode\n");
		return -(EINVAL);
	    }
	    ev_size = 4;
	}

	if (event[0] == SEQ_MIDIPUTC) {

	    if (!midi_opened[event[2]]) {
		int             mode;
		int             dev = event[2];

		if (dev >= max_mididev) {
		    printf("Sequencer Error: Nonexistent MIDI device %d\n",dev);
		    return -(ENXIO);
		}
		mode = file->mode & O_ACCMODE;

		if ((err = midi_devs[dev]->open(dev, mode,
			sequencer_midi_input, sequencer_midi_output)) < 0) {
		    seq_reset();
		    printf("Sequencer Error: Unable to open Midi #%d\n", dev);
		    return err;
		}
		midi_opened[dev] = 1;
	    }
	}
	if (!seq_queue(event, 0)) {
	    int             processed = count - c;

	    if (!seq_playing)
		seq_startplay();

	    return processed;
	}
	p += ev_size;
	c -= ev_size;
    }

    if (!seq_playing)
	seq_startplay();

    return count;	/* This will "eat" chunks shorter than 4 bytes
			 * (if written alone) Should we really do that ?
			 */
}

static int
seq_queue(u_char *note, char nonblock)
{

    /*
     * Test if there is space in the queue
     */

    if (qlen >= SEQ_MAX_QUEUE)
	if (!seq_playing)
	    seq_startplay();	/* Give chance to drain the queue */

    if (!nonblock && qlen >= SEQ_MAX_QUEUE &&
		!(seq_sleep_flag.mode & WK_SLEEP)) {
	/*
	 * Sleep until there is enough space on the queue
	 */

	int chn;


	seq_sleeper = &chn;
	DO_SLEEP(chn, seq_sleep_flag, 0);

    }
    if (qlen >= SEQ_MAX_QUEUE)
	return 0;	/* To be sure */
    bcopy(note, &queue[qtail * EV_SZ], EV_SZ);

    qtail = (qtail + 1) % SEQ_MAX_QUEUE;
    qlen++;

    return 1;
}

static int
extended_event(u_char *q)
{
    int             dev = q[2];

    if (dev < 0 || dev >= max_synthdev)
	return -(ENXIO);

    if (!(synth_open_mask & (1 << dev)))
	return -(ENXIO);

    switch (q[1]) {
    case SEQ_NOTEOFF:
	synth_devs[dev]->kill_note(dev, q[3], q[4], q[5]);
	break;

    case SEQ_NOTEON:
	if (q[4] > 127 && q[4] != 255)
	    return 0;

	synth_devs[dev]->start_note(dev, q[3], q[4], q[5]);
	break;

    case SEQ_PGMCHANGE:
	synth_devs[dev]->set_instr(dev, q[3], q[4]);
	break;

    case SEQ_AFTERTOUCH:
	synth_devs[dev]->aftertouch(dev, q[3], q[4]);
	break;

    case SEQ_BALANCE:
	synth_devs[dev]->panning(dev, q[3], (char) q[4]);
	break;

    case SEQ_CONTROLLER:
	synth_devs[dev]->controller(dev, q[3], q[4], *(short *) &q[5]);
	break;

    case SEQ_VOLMODE:
	if (synth_devs[dev]->volume_method != NULL)
	    synth_devs[dev]->volume_method(dev, q[3]);
	break;

    default:
	return -(EINVAL);
    }

    return 0;
}

static int
find_voice(int dev, int chn, int note)
{
    u_short  key;
    int             i;

    key = (chn << 8) | (note + 1);

    for (i = 0; i < synth_devs[dev]->alloc.max_voice; i++)
	if (synth_devs[dev]->alloc.map[i] == key)
	    return i;

    return -1;
}

static int
alloc_voice(int dev, int chn, int note)
{
    u_short  key;
    int             voice;

    key = (chn << 8) | (note + 1);

    voice = synth_devs[dev]->alloc_voice(dev, chn, note,
			     &synth_devs[dev]->alloc);
    synth_devs[dev]->alloc.map[voice] = key;
    synth_devs[dev]->alloc.alloc_times[voice] =
	    synth_devs[dev]->alloc.timestamp++;
    return voice;
}

static void
seq_chn_voice_event(u_char *event)
{
    u_char   dev = event[1];
    u_char   cmd = event[2];
    u_char   chn = event[3];
    u_char   note = event[4];
    u_char   parm = event[5];
    int             voice = -1;

    if ((int) dev > max_synthdev)
	return;
    if (!(synth_open_mask & (1 << dev)))
	return;
    if (!synth_devs[dev])
	return;

    if (seq_mode == SEQ_2) {
	if (synth_devs[dev]->alloc_voice)
	    voice = find_voice(dev, chn, note);

	if (cmd == MIDI_NOTEON && parm == 0) {
	    cmd = MIDI_NOTEOFF;
	    parm = 64;
	}
    }
    switch (cmd) {
    case MIDI_NOTEON:
	if (note > 127 && note != 255)	/* Not a seq2 feature */
	    return;

	if (voice == -1 && seq_mode == SEQ_2 && synth_devs[dev]->alloc_voice) {	
	    /* Internal synthesizer (FM, GUS, etc) */
	    voice = alloc_voice(dev, chn, note);
	}
	if (voice == -1)
	    voice = chn;

	if (seq_mode == SEQ_2 && (int) dev < num_synths) {
	    /*
	     * The MIDI channel 10 is a percussive channel. Use
	     * the note number to select the proper patch (128 to
	     * 255) to play.
	     */

	    if (chn == 9) {
		synth_devs[dev]->set_instr(dev, voice, 128 + note);
		note = 60;	/* Middle C */

	    }
	}
	if (seq_mode == SEQ_2)
	    synth_devs[dev]->setup_voice(dev, voice, chn);
	synth_devs[dev]->start_note(dev, voice, note, parm);
	break;

    case MIDI_NOTEOFF:
	if (voice == -1)
	    voice = chn;
	synth_devs[dev]->kill_note(dev, voice, note, parm);
	break;

    case MIDI_KEY_PRESSURE:
	if (voice == -1)
	    voice = chn;
	synth_devs[dev]->aftertouch(dev, voice, parm);
	break;

    default:;
    }
}

static void
seq_chn_common_event(u_char *event)
{
    u_char   dev = event[1];
    u_char   cmd = event[2];
    u_char   chn = event[3];
    u_char   p1 = event[4];

    /* u_char   p2 = event[5]; */
    u_short  w14 = *(short *) &event[6];

    if ((int) dev > max_synthdev)
	return;
    if (!(synth_open_mask & (1 << dev)))
	return;
    if (!synth_devs[dev])
	return;

    switch (cmd) {
    case MIDI_PGM_CHANGE:
	if (seq_mode == SEQ_2) {
	    synth_devs[dev]->chn_info[chn].pgm_num = p1;
	    if ((int) dev >= num_synths)
		synth_devs[dev]->set_instr(dev, chn, p1);
	} else
	    synth_devs[dev]->set_instr(dev, chn, p1);

	break;

    case MIDI_CTL_CHANGE:
	if (seq_mode == SEQ_2) {
	    if (chn > 15 || p1 > 127)
		break;

	    synth_devs[dev]->chn_info[chn].controllers[p1] = w14 & 0x7f;

	    if (p1 < 32)	/* Setting MSB should clear LSB to 0 */
		synth_devs[dev]->chn_info[chn].controllers[p1 + 32] = 0;

	    if ((int) dev < num_synths) {
		int             val = w14 & 0x7f;
		int             i, key;

		if (p1 < 64) {	/* Combine MSB and LSB */
		    val = ((synth_devs[dev]->
			   chn_info[chn].controllers[p1 & ~32] & 0x7f) << 7)
			| (synth_devs[dev]->
			   chn_info[chn].controllers[p1 | 32] & 0x7f);
		    p1 &= ~32;
		}
		/* Handle all playing notes on this channel */

		key = ((int) chn << 8);

		for (i = 0; i < synth_devs[dev]->alloc.max_voice; i++)
		    if ((synth_devs[dev]->alloc.map[i] & 0xff00) == key)
			synth_devs[dev]->controller(dev, i, p1, val);
	    } else
		synth_devs[dev]->controller(dev, chn, p1, w14);
	} else		/* Mode 1 */
	    synth_devs[dev]->controller(dev, chn, p1, w14);
	break;

    case MIDI_PITCH_BEND:
	if (seq_mode == SEQ_2) {
	    synth_devs[dev]->chn_info[chn].bender_value = w14;

	    if ((int) dev < num_synths) {	/* Handle all playing
						 * notes on this channel */
		int             i, key;

		key = (chn << 8);

		for (i = 0; i < synth_devs[dev]->alloc.max_voice; i++)
		    if ((synth_devs[dev]->alloc.map[i] & 0xff00) == key)
			synth_devs[dev]->bender(dev, i, w14);
	    } else
		synth_devs[dev]->bender(dev, chn, w14);
	} else		/* MODE 1 */
	    synth_devs[dev]->bender(dev, chn, w14);
	break;

    default:;
    }
}

static int
seq_timing_event(u_char *event)
{
    u_char   cmd = event[1];
    u_int    parm = *(int *) &event[4];

    if (seq_mode == SEQ_2) {
	int             ret;

	if ((ret = tmr->event(tmr_no, event)) == TIMER_ARMED) {
	    if ((SEQ_MAX_QUEUE - qlen) >= output_treshold) {
		u_long   flags;

		flags = splhigh();
		if ((seq_sleep_flag.mode & WK_SLEEP)) {
		    seq_sleep_flag.mode = WK_WAKEUP;
		    wakeup(seq_sleeper);
		}
		splx(flags);
	    }
	}
	return ret;
    }
    switch (cmd) {
    case TMR_WAIT_REL:
	parm += prev_event_time;

	/*
	 * NOTE!  No break here. Execution of TMR_WAIT_REL continues
	 * in the next case (TMR_WAIT_ABS)
	 */

    case TMR_WAIT_ABS:
	if (parm > 0) {
	    long            time;

	    seq_playing = 1;
	    time = parm;
	    prev_event_time = time;

	    request_sound_timer(time);

	    if ((SEQ_MAX_QUEUE - qlen) >= output_treshold) {
		u_long   flags;

		flags = splhigh();
		if ((seq_sleep_flag.mode & WK_SLEEP)) {
		    seq_sleep_flag.mode = WK_WAKEUP;
		    wakeup(seq_sleeper);
		}
		splx(flags);
	    }
	    return TIMER_ARMED;
	}
	break;

    case TMR_START:
	seq_time = get_time();
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
	    seq_copy_to_input(event, 8);
	else {
	    parm = (parm << 8 | SEQ_ECHO);
	    seq_copy_to_input((u_char *) &parm, 4);
	}
	break;

    default:;
    }

    return TIMER_NOT_ARMED;
}

static void
seq_local_event(u_char *event)
{
    u_char   cmd = event[1];
    u_int    parm = *((u_int *) &event[4]);

    switch (cmd) {
    case LOCL_STARTAUDIO:
#ifdef CONFIG_AUDIO
	DMAbuf_start_devices(parm);
#endif
	break;

    default:;
    }
}

static void
seq_sysex_message(u_char *event)
{
    int             dev = event[1];
    int             i, l = 0;
    u_char  *buf = &event[2];

    if ((int) dev > max_synthdev)
	return;
    if (!(synth_open_mask & (1 << dev)))
	return;
    if (!synth_devs[dev])
	return;
    if (!synth_devs[dev]->send_sysex)
	return;

    l = 0;
    for (i = 0; i < 6 && buf[i] != 0xff; i++)
	l = i + 1;

    if (l > 0)
	synth_devs[dev]->send_sysex(dev, buf, l);
}

static int
play_event(u_char *q)
{
    /*
     * NOTE! This routine returns 0 = normal event played. 1 = Timer
     * armed. Suspend playback until timer callback. 2 = MIDI output
     * buffer full. Restore queue and suspend until timer
     */
    u_long  *delay;

    switch (q[0]) {
    case SEQ_NOTEOFF:
	if (synth_open_mask & (1 << 0))
	    if (synth_devs[0])
		synth_devs[0]->kill_note(0, q[1], 255, q[3]);
	break;

    case SEQ_NOTEON:
	if (q[4] < 128 || q[4] == 255)
	    if (synth_open_mask & (1 << 0))
		if (synth_devs[0])
		    synth_devs[0]->start_note(0, q[1], q[2], q[3]);
	break;

    case SEQ_WAIT:
	delay = (u_long *) q;	/* Bytes 1 to 3 are
					 * containing the * delay in
					 * get_time() */
	*delay = (*delay >> 8) & 0xffffff;

	if (*delay > 0) {
	    long            time;

	    seq_playing = 1;
	    time = *delay;
	    prev_event_time = time;

	    request_sound_timer(time);

	    if ((SEQ_MAX_QUEUE - qlen) >= output_treshold) {
		u_long   flags;

		flags = splhigh();
		if ((seq_sleep_flag.mode & WK_SLEEP)) {
		    seq_sleep_flag.mode = WK_WAKEUP;
		    wakeup(seq_sleeper);
		}
		splx(flags);
	    }
	    /*
	     * The timer is now active and will reinvoke this
	     * function after the timer expires. Return to the
	     * caller now.
	     */
	    return 1;
	}
	break;

    case SEQ_PGMCHANGE:
	if (synth_open_mask & (1 << 0))
	    if (synth_devs[0])
		synth_devs[0]->set_instr(0, q[1], q[2]);
	break;

    case SEQ_SYNCTIMER:	/* Reset timer */
	seq_time = get_time();
	prev_input_time = 0;
	prev_event_time = 0;
	break;

    case SEQ_MIDIPUTC:	/* Put a midi character */
	if (midi_opened[q[2]]) {
	    int             dev;

	    dev = q[2];

	    if (!midi_devs[dev]->putc(dev, q[1])) {
		/*
		 * Output FIFO is full. Wait one timer cycle and try again.
		 */

		seq_playing = 1;
		request_sound_timer(-1);
		return 2;
	    } else
		midi_written[dev] = 1;
	}
	break;

    case SEQ_ECHO:
	seq_copy_to_input(q, 4);	/* Echo back to the process */
	break;

    case SEQ_PRIVATE:
	if ((int) q[1] < max_synthdev)
	    synth_devs[q[1]]->hw_control(q[1], q);
	break;

    case SEQ_EXTENDED:
	extended_event(q);
	break;

    case EV_CHN_VOICE:
	seq_chn_voice_event(q);
	break;

    case EV_CHN_COMMON:
	seq_chn_common_event(q);
	break;

    case EV_TIMING:
	if (seq_timing_event(q) == TIMER_ARMED) {
	    return 1;
	}
	break;

    case EV_SEQ_LOCAL:
	seq_local_event(q);
	break;

    case EV_SYSEX:
	seq_sysex_message(q);
	break;

    default:;
    }

    return 0;
}

static void
seq_startplay(void)
{
    u_long   flags;
    int             this_one, action;

    while (qlen > 0) {

	flags = splhigh();
	qhead = ((this_one = qhead) + 1) % SEQ_MAX_QUEUE;
	qlen--;
	splx(flags);

	seq_playing = 1;

	if ((action = play_event(&queue[this_one * EV_SZ]))) {
	    /*
	     * Suspend playback. Next timer routine invokes this routine again
	     */
	    if (action == 2) {
		qlen++;
		qhead = this_one;
	    }
	    return;
	}
    }

    seq_playing = 0;

    if ((SEQ_MAX_QUEUE - qlen) >= output_treshold) {
	u_long   flags;

	flags = splhigh();
	if ((seq_sleep_flag.mode & WK_SLEEP)) {
	    seq_sleep_flag.mode = WK_WAKEUP;
	    wakeup(seq_sleeper);
	}
	splx(flags);
    }
}

static void
reset_controllers(int dev, u_char *controller, int update_dev)
{

    int             i;

    for (i = 0; i < 128; i++)
	controller[i] = ctrl_def_values[i];
}

static void
setup_mode2(void)
{
    int             dev;

    max_synthdev = num_synths;

    for (dev = 0; dev < num_midis; dev++)
	if (midi_devs[dev]->converter != NULL)
	    synth_devs[max_synthdev++] = midi_devs[dev]->converter;
    for (dev = 0; dev < max_synthdev; dev++) {
	int             chn;

	for (chn = 0; chn < 16; chn++) {
	    synth_devs[dev]->chn_info[chn].pgm_num = 0;
	    reset_controllers(dev,
		     synth_devs[dev]->chn_info[chn].controllers, 0);
	    synth_devs[dev]->chn_info[chn].bender_value = (1<<7); /* Neutral */
	}
    }

    max_mididev = 0;
    seq_mode = SEQ_2;
}

int
sequencer_open(int dev, struct fileinfo * file)
{
    int             retval, mode, i;
    int             level, tmp;
    u_long   flags;

    level = ((dev & 0x0f) == SND_DEV_SEQ2) ? 2 : 1;

    dev = dev >> 4;
    mode = file->mode & O_ACCMODE;

    DEB(printf("sequencer_open(dev=%d)\n", dev));

    if (!sequencer_ok) {
	printf("Soundcard: Sequencer not initialized\n");
	return -(ENXIO);
    }
    if (dev) {		/* Patch manager device */
	int             err;

	printf("Patch manager interface is currently broken. Sorry\n");
	return -(ENXIO);

	dev--;

	if (dev >= MAX_SYNTH_DEV)
	    return -(ENXIO);
	if (pmgr_present[dev])
	    return -(EBUSY);
	if ((err = pmgr_open(dev)) < 0)
	    return err;

	pmgr_present[dev] = 1;
	return err;
    }
    flags = splhigh();
    if (sequencer_busy) {
	printf("Sequencer busy\n");
	splx(flags);
	return -(EBUSY);
    }
    sequencer_busy = 1;
    splx(flags);

    max_mididev = num_midis;
    max_synthdev = num_synths;
    pre_event_timeout = 0;
    seq_mode = SEQ_1;

    if (pending_timer != -1) {
	tmr_no = pending_timer;
	pending_timer = -1;
    }
    if (tmr_no == -1) {	/* Not selected yet */
	int             i, best;

	best = -1;
	for (i = 0; i < num_sound_timers; i++)
	    if (sound_timer_devs[i]->priority > best) {
		tmr_no = i;
		best = sound_timer_devs[i]->priority;
	    }
	    if (tmr_no == -1)	/* Should not be */
		tmr_no = 0;
    }
    tmr = sound_timer_devs[tmr_no];

    if (level == 2) {
	if (tmr == NULL) {
	    printf("sequencer: No timer for level 2\n");
	    sequencer_busy = 0;
	    return -(ENXIO);
	}
	setup_mode2();
    }
    if (seq_mode == SEQ_1 && (mode == OPEN_READ || mode == OPEN_READWRITE))
	if (!max_mididev) {
	    printf("Sequencer: No Midi devices. Input not possible\n");
	    sequencer_busy = 0;
	    return -(ENXIO);
	}
    if (!max_synthdev && !max_mididev)
	return -(ENXIO);

    synth_open_mask = 0;

    for (i = 0; i < max_mididev; i++) {
	midi_opened[i] = 0;
	midi_written[i] = 0;
    }

    /*
     * if (mode == OPEN_WRITE || mode == OPEN_READWRITE)
     */
    for (i = 0; i < max_synthdev; i++)	/* Open synth devices */
	if ((tmp = synth_devs[i]->open(i, mode)) < 0) {
	    printf("Sequencer: Warning! Cannot open synth device #%d (%d)\n",
		i, tmp);
	    if (synth_devs[i]->midi_dev)
		printf("(Maps to MIDI dev #%d)\n", synth_devs[i]->midi_dev);
	} else {
	    synth_open_mask |= (1 << i);
	    if (synth_devs[i]->midi_dev)	/* Is a midi interface */
		midi_opened[synth_devs[i]->midi_dev] = 1;
	}

    seq_time = get_time();
    prev_input_time = 0;
    prev_event_time = 0;

    if (seq_mode == SEQ_1 && (mode == OPEN_READ || mode == OPEN_READWRITE)) {
	/* Initialize midi input devices */
	for (i = 0; i < max_mididev; i++)
	    if (!midi_opened[i]) {
		if ((retval = midi_devs[i]->open(i, mode,
			 sequencer_midi_input, sequencer_midi_output)) >= 0)
		    midi_opened[i] = 1;
	    }
    }
    if (seq_mode == SEQ_2) {
	tmr->open(tmr_no, seq_mode);
    }
    seq_sleep_flag.aborting = 0;
    seq_sleep_flag.mode = WK_NONE;
    midi_sleep_flag.aborting = 0;
    midi_sleep_flag.mode = WK_NONE;
    output_treshold = SEQ_MAX_QUEUE / 2;

    for (i = 0; i < num_synths; i++)
	if (pmgr_present[i])
	    pmgr_inform(i, PM_E_OPENED, 0, 0, 0, 0);

    return 0;
}

static void
seq_drain_midi_queues(void)
{
    int             i, n;

    /*
     * Give the Midi drivers time to drain their output queues
     */

    n = 1;

    while (!(seq_sleep_flag.aborting) && n) {
	n = 0;

	for (i = 0; i < max_mididev; i++)
	    if (midi_opened[i] && midi_written[i])
		if (midi_devs[i]->buffer_status != NULL)
		    if (midi_devs[i]->buffer_status(i))
						n++;

	/*
	 * Let's have a delay
	 */
	if (n) {
	    int   chn;

	    seq_sleeper = &chn;
	    DO_SLEEP(chn, seq_sleep_flag, hz / 10);

	}
    }
}

void
sequencer_release(int dev, struct fileinfo * file)
{
    int             i;
    int             mode = file->mode & O_ACCMODE;

    dev = dev >> 4;

    DEB(printf("sequencer_release(dev=%d)\n", dev));

    if (dev) {		/* Patch manager device */
	dev--;
	pmgr_release(dev);
	pmgr_present[dev] = 0;
	return;
    }
    /*
     * Wait until the queue is empty (if we don't have nonblock)
     */

    if (mode != OPEN_READ && !0)
	while (!(seq_sleep_flag.aborting) && qlen) {
	    seq_sync();
	}

    if (mode != OPEN_READ)
	seq_drain_midi_queues(); /* Ensure the output queues are empty */
    seq_reset();
    if (mode != OPEN_READ)
	seq_drain_midi_queues();	/* Flush the all notes off messages */

    for (i = 0; i < max_synthdev; i++)
	if (synth_open_mask & (1 << i))	/* Actually opened */
	    if (synth_devs[i]) {
		synth_devs[i]->close(i);

		if (synth_devs[i]->midi_dev)
		    midi_opened[synth_devs[i]->midi_dev] = 0;
	    }
    for (i = 0; i < num_synths; i++)
	if (pmgr_present[i])
	    pmgr_inform(i, PM_E_CLOSED, 0, 0, 0, 0);

    for (i = 0; i < max_mididev; i++)
	if (midi_opened[i])
	    midi_devs[i]->close(i);

    if (seq_mode == SEQ_2)
	tmr->close(tmr_no);

    sequencer_busy = 0;
}

static int
seq_sync(void)
{
    u_long   flags;

    if (qlen && !seq_playing && !(seq_sleep_flag.aborting))
	seq_startplay();

    flags = splhigh();
    if (qlen && !(seq_sleep_flag.mode & WK_SLEEP)) {
	int  chn;

	seq_sleeper = &chn;
	DO_SLEEP(chn, seq_sleep_flag, 0);

    }
    splx(flags);

    return qlen;
}

static void
midi_outc(int dev, u_char data)
{
    /*
     * NOTE! Calls sleep(). Don't call this from interrupt.
     */

    int             n;
    u_long   flags;

    /*
     * This routine sends one byte to the Midi channel. If the output
     * Fifo is full, it waits until there is space in the queue
     */

    n = 3 * hz;		/* Timeout */

    flags = splhigh();
    while (n && !midi_devs[dev]->putc(dev, data)) {
	int    chn;
	seq_sleeper = &chn;
	DO_SLEEP(chn, seq_sleep_flag, 4);

	n--;
    }
    splx(flags);
}

static void
seq_reset(void)
{
    /*
     * NOTE! Calls sleep(). Don't call this from interrupt.
     */

    int             i;
    int             chn;
    u_long   flags;

    sound_stop_timer();
    seq_time = get_time();
    prev_input_time = 0;
    prev_event_time = 0;

    qlen = qhead = qtail = 0;
    iqlen = iqhead = iqtail = 0;

    for (i = 0; i < max_synthdev; i++)
	if (synth_open_mask & (1 << i))
	    if (synth_devs[i])
		synth_devs[i]->reset(i);

    if (seq_mode == SEQ_2) {
	for (chn = 0; chn < 16; chn++)
	    for (i = 0; i < max_synthdev; i++)
		if ( (synth_open_mask & (1 << i)) && (synth_devs[i]) ) {
		    synth_devs[i]->controller(i, chn,123,0);/* All notes off */
		    synth_devs[i]->controller(i, chn,121,0);/* Reset all ctl */
		    synth_devs[i]->bender(i, chn, 1 << 13); /* Bender off */
		}

    } else {					/* seq_mode == SEQ_1 */
	for (i = 0; i < max_mididev; i++)
	    if (midi_written[i]) {
		/* Midi used. Some notes may still be playing */
		/*
		 * Sending just a ACTIVE SENSING message
		 * should be enough to stop all playing
		 * notes. Since there are devices not
		 * recognizing the active sensing, we have to
		 * send some all notes off messages also.
		 */
		midi_outc(i, 0xfe);

		for (chn = 0; chn < 16; chn++) {
		    midi_outc(i, (u_char) (0xb0 + (chn & 0x0f))); /* control change */
		    midi_outc(i, 0x7b);	/* All notes off */
		    midi_outc(i, 0);	/* Dummy parameter */
		}

		midi_devs[i]->close(i);

		midi_written[i] = 0;
		midi_opened[i] = 0;
	    }
    }

    seq_playing = 0;

    flags = splhigh();
    if ((seq_sleep_flag.mode & WK_SLEEP)) {
	seq_sleep_flag.mode = WK_WAKEUP;
	wakeup(seq_sleeper);
    }
    splx(flags);

}

static void
seq_panic(void)
{
    /*
     * This routine is called by the application in case the user wants
     * to reset the system to the default state.
     */

    seq_reset();

    /*
     * Since some of the devices don't recognize the active sensing and
     * all notes off messages, we have to shut all notes manually.
     * 
     * TO BE IMPLEMENTED LATER
     */

    /*
     * Also return the controllers to their default states
     */
}

int
sequencer_ioctl(int dev, struct fileinfo * file,
		u_int cmd, ioctl_arg arg)
{
    int             midi_dev, orig_dev;
    int             mode = file->mode & O_ACCMODE;

    orig_dev = dev = dev >> 4;

    switch (cmd) {
    case SNDCTL_TMR_TIMEBASE:
    case SNDCTL_TMR_TEMPO:
    case SNDCTL_TMR_START:
    case SNDCTL_TMR_STOP:
    case SNDCTL_TMR_CONTINUE:
    case SNDCTL_TMR_METRONOME:
    case SNDCTL_TMR_SOURCE:
	if (dev)	/* Patch manager */
	    return -(EIO);

	if (seq_mode != SEQ_2)
	    return -(EINVAL);
	return tmr->ioctl(tmr_no, cmd, arg);
	break;

    case SNDCTL_TMR_SELECT:
	if (dev)	/* Patch manager */
	    return -(EIO);

	if (seq_mode != SEQ_2)
	    return -(EINVAL);
	pending_timer = (*(int *) arg);

	if (pending_timer < 0 || pending_timer >= num_sound_timers) {
	    pending_timer = -1;
	    return -(EINVAL);
	}
	return *(int *) arg = pending_timer;
	break;

    case SNDCTL_SEQ_PANIC:
	seq_panic();
	break;

    case SNDCTL_SEQ_SYNC:
	if (dev)	/* Patch manager */
	    return -(EIO);

	if (mode == OPEN_READ)
	    return 0;
	while (qlen && !(seq_sleep_flag.aborting))
	    seq_sync();
	if (qlen)
	    return -(EINTR);
	else
	    return 0;
	break;

    case SNDCTL_SEQ_RESET:
	if (dev)	/* Patch manager */
	    return -(EIO);

	seq_reset();
	return 0;
	break;

    case SNDCTL_SEQ_TESTMIDI:
	if (dev)	/* Patch manager */
	    return -(EIO);

	midi_dev = (*(int *) arg);
	if (midi_dev >= max_mididev)
	    return -(ENXIO);

	if (!midi_opened[midi_dev]) {
	    int             err, mode;

	    mode = file->mode & O_ACCMODE;
	    if ((err = midi_devs[midi_dev]->open(midi_dev, mode,
		       sequencer_midi_input, sequencer_midi_output)) < 0)
		return err;
	}
	midi_opened[midi_dev] = 1;

	return 0;
	break;

    case SNDCTL_SEQ_GETINCOUNT:
	if (dev)	/* Patch manager */
	    return -(EIO);

	if (mode == OPEN_WRITE)
	    return 0;
	return *(int *) arg = iqlen;
	break;

    case SNDCTL_SEQ_GETOUTCOUNT:

	if (mode == OPEN_READ)
	    return 0;
	return *(int *) arg = SEQ_MAX_QUEUE - qlen;
	break;

    case SNDCTL_SEQ_CTRLRATE:
	if (dev)	/* Patch manager */
	    return -(EIO);

	/*
	 * If *arg == 0, just return the current rate
	 */
	if (seq_mode == SEQ_2)
	    return tmr->ioctl(tmr_no, cmd, arg);

	if ((*(int *) arg) != 0)
	    return -(EINVAL);

	return *(int *) arg = hz;
	break;

    case SNDCTL_SEQ_RESETSAMPLES:
	{
	    int             err;

	    dev = (*(int *) arg);
	    if (dev < 0 || dev >= num_synths)
		return -(ENXIO);
	    if (!(synth_open_mask & (1 << dev)) && !orig_dev)
		return -(EBUSY);
	    if (!orig_dev && pmgr_present[dev])
		pmgr_inform(dev, PM_E_PATCH_RESET, 0, 0, 0, 0);

	    err = synth_devs[dev]->ioctl(dev, cmd, arg);
	    return err;
	}
	break;

    case SNDCTL_SEQ_NRSYNTHS:
	return *(int *) arg = max_synthdev;
	break;

    case SNDCTL_SEQ_NRMIDIS:
	return *(int *) arg = max_mididev;
	break;

    case SNDCTL_SYNTH_MEMAVL:
	{
	    int             dev = (*(int *) arg);

	    if (dev < 0 || dev >= num_synths)
		return -(ENXIO);

	    if (!(synth_open_mask & (1 << dev)) && !orig_dev)
		return -(EBUSY);

	    return *(int *) arg = synth_devs[dev]->ioctl(dev, cmd, arg);
	}
	break;

    case SNDCTL_FM_4OP_ENABLE:
	{
	    int             dev = (*(int *) arg);

	    if (dev < 0 || dev >= num_synths)
		return -(ENXIO);

	    if (!(synth_open_mask & (1 << dev)))
		return -(ENXIO);

	    synth_devs[dev]->ioctl(dev, cmd, arg);
	    return 0;
	}
	break;

    case SNDCTL_SYNTH_INFO:
	{
	    struct synth_info inf;
	    int             dev;

	    bcopy(&(((char *) arg)[0]), (char *) &inf, sizeof(inf));
	    dev = inf.device;

	    if (dev < 0 || dev >= max_synthdev)
		return -(ENXIO);

	    if (!(synth_open_mask & (1 << dev)) && !orig_dev)
		return -(EBUSY);

	    return synth_devs[dev]->ioctl(dev, cmd, arg);
	}
	break;

    case SNDCTL_SEQ_OUTOFBAND:
	{
	    struct seq_event_rec event;
	    u_long   flags;

	    bcopy(&(((char *) arg)[0]), (char *) &event, sizeof(event));

	    flags = splhigh();
	    play_event(event.arr);
	    splx(flags);

	    return 0;
	}
	break;

    case SNDCTL_MIDI_INFO:
	{
	    struct midi_info inf;
	    int             dev;

	    bcopy(&(((char *) arg)[0]), (char *) &inf, sizeof(inf));
	    dev = inf.device;

	    if (dev < 0 || dev >= max_mididev)
		return -(ENXIO);

	    bcopy((char *) &(midi_devs[dev]->info), &(((char *) arg)[0]), sizeof(inf));
	    return 0;
	}
	break;

    case SNDCTL_PMGR_IFACE:
	{
	    struct patmgr_info *inf;
	    int             dev, err;

	    if ((inf = (struct patmgr_info *) malloc(sizeof(*inf), M_TEMP, M_WAITOK)) == NULL) {
		printf("patmgr: Can't allocate memory for a message\n");
		return -(EIO);
	    }
	    bcopy(&(((char *) arg)[0]), (char *) inf, sizeof(*inf));
	    dev = inf->device;

	    if (dev < 0 || dev >= num_synths) {
		free(inf, M_TEMP);
		return -(ENXIO);
	    }
	    if (!synth_devs[dev]->pmgr_interface) {
		free(inf, M_TEMP);
		return -(ENXIO);
	    }
	    if ((err = synth_devs[dev]->pmgr_interface(dev, inf)) == -1) {
		free(inf, M_TEMP);
		return err;
	    }
	    bcopy((char *) inf, &(((char *) arg)[0]), sizeof(*inf));
	    free(inf, M_TEMP);
	    return 0;
	}
	break;

    case SNDCTL_PMGR_ACCESS:
	{
	    struct patmgr_info *inf;
	    int             dev, err;

	    if ((inf = (struct patmgr_info *) malloc(sizeof(*inf), M_TEMP, M_WAITOK)) == NULL) {
		printf("patmgr: Can't allocate memory for a message\n");
		return -(EIO);
	    }
	    bcopy(&(((char *) arg)[0]), (char *) inf, sizeof(*inf));
	    dev = inf->device;

	    if (dev < 0 || dev >= num_synths) {
		free(inf, M_TEMP);
		return -(ENXIO);
	    }
	    if (!pmgr_present[dev]) {
		free(inf, M_TEMP);
		return -(ESRCH);
	    }
	    if ((err = pmgr_access(dev, inf)) < 0) {
		free(inf, M_TEMP);
		return err;
	    }
	    bcopy((char *) inf, &(((char *) arg)[0]), sizeof(*inf));
	    free(inf, M_TEMP);
	    return 0;
	}
	break;

    case SNDCTL_SEQ_THRESHOLD:
	{
	    int             tmp = (*(int *) arg);

	    if (dev)/* Patch manager */
		return -(EIO);

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
	    int             val = (*(int *) arg);

	    if (val < 0)
		val = 0;

	    val = (hz * val) / 10;
	    pre_event_timeout = val;
	    return *(int *) arg = val;
	}
	break;

    default:
	if (dev)	/* Patch manager */
	    return -(EIO);

	if (mode == OPEN_READ)
	    return -(EIO);

	if (!synth_devs[0])
	    return -(ENXIO);
	if (!(synth_open_mask & (1 << 0)))
	    return -(ENXIO);
	return synth_devs[0]->ioctl(0, cmd, arg);
	break;
    }

    return -(EINVAL);
}

#ifdef ALLOW_POLL
int
sequencer_poll (int dev, struct fileinfo *file, int events, select_table * wait)
{
  unsigned long   flags;
  int revents = 0;

  dev = dev >> 4;
  flags = splhigh();


  if (events & (POLLIN | POLLRDNORM)) {
    if (!iqlen)
      selrecord(wait, &selinfo[dev]);
    else {
      revents |= events & (POLLIN | POLLRDNORM);
      midi_sleep_flag.mode &= ~WK_SLEEP;
    }
  }
  if (events & (POLLOUT | POLLWRNORM)) {
    if (qlen >= SEQ_MAX_QUEUE)
      selrecord(wait, &selinfo[dev]);
    else {
      revents |= events & (POLLOUT | POLLWRNORM);
      seq_sleep_flag.mode &= ~WK_SLEEP;
    }
  }
  splx(flags);

  return (revents);
}

#endif


void
sequencer_timer(void *dummy)
{
    seq_startplay();
}

int
note_to_freq(int note_num)
{

    /*
     * This routine converts a midi note to a frequency (multiplied by
     * 1000)
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

u_long
compute_finetune(u_long base_freq, int bend, int range)
{
    u_long   amount;
    int             negative, semitones, cents, multiplier = 1;

    if (!bend)
	return base_freq;
    if (!range)
	return base_freq;

    if (!base_freq)
	return base_freq;

    if (range >= 8192)
	range = 8192;

    bend = bend * range / 8192;
    if (!bend)
	return base_freq;

    negative = bend < 0 ? 1 : 0;

    if (bend < 0)
	bend *= -1;
    if (bend > range)
	bend = range;

    /*
     * if (bend > 2399) bend = 2399;
     */
    while (bend > 2399) {
	multiplier *= 4;
	bend -= 2400;
    }

    semitones = bend / 100;
    cents = bend % 100;

    amount = (int) (semitone_tuning[semitones] * multiplier * cent_tuning[cents])
	    / 10000;

    if (negative)
	return (base_freq * 10000) / amount;	/* Bend down */
    else
	return (base_freq * amount) / 10000;	/* Bend up */
}


void
sequencer_init()
{

    sequencer_ok = 1;

    queue = (u_char *) malloc(SEQ_MAX_QUEUE * EV_SZ, M_DEVBUF, M_NOWAIT);
    if (!queue)
	panic("SOUND: Cannot allocate memory\n");

    iqueue = (u_char *) malloc(SEQ_MAX_QUEUE * IEV_SZ, M_DEVBUF, M_NOWAIT);
    if (!iqueue)
	panic("SOUND: Cannot allocate memory\n");
}

#endif
#endif
