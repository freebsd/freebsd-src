/*
 * sound/sys_timer.c
 * 
 * The default timer for the Level 2 sequencer interface.
 * Uses the (100hz) timer of kernel.
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

#if	NSND > 0

#if defined(CONFIG_SEQUENCER)

static volatile int opened = 0, tmr_running = 0;
static volatile time_t tmr_offs, tmr_ctr;
static volatile u_long ticks_offs;
static volatile int curr_tempo, curr_timebase;
static volatile u_long curr_ticks;
static volatile u_long next_event_time;
static u_long prev_event_time;

static void     poll_def_tmr(void *dummy);

static u_long
tmr2ticks(int tmr_value)
{
    /*
     * Convert system timer ticks (hz) to MIDI ticks (divide # of MIDI
     * ticks/minute by # of system ticks/minute).
     */

    return ((tmr_value * curr_tempo * curr_timebase) + (30 * hz)) / (60 * hz);
}

static void
poll_def_tmr(void *dummy)
{

    if (opened) {

	timeout( poll_def_tmr, 0, 1);;

	if (tmr_running) {
	    tmr_ctr++;
	    curr_ticks = ticks_offs + tmr2ticks(tmr_ctr);

	    if (curr_ticks >= next_event_time) {
		next_event_time = 0xffffffff;
		sequencer_timer(0);
	    }
	}
    }
}

static void
tmr_reset(void)
{
    u_long   flags;

    flags = splhigh();
    tmr_offs = 0;
    ticks_offs = 0;
    tmr_ctr = 0;
    next_event_time = 0xffffffff;
    prev_event_time = 0;
    curr_ticks = 0;
    splx(flags);
}

static int
def_tmr_open(int dev, int mode)
{
    if (opened)
	return -(EBUSY);

    tmr_reset();
    curr_tempo = 60;
    curr_timebase = hz;
    opened = 1;

    timeout( poll_def_tmr, 0, 1);;

    return 0;
}

static void
def_tmr_close(int dev)
{
    opened = tmr_running = 0;
}

static int
def_tmr_event(int dev, u_char *event)
{
    u_char   cmd = event[1];
    u_long   parm = *(int *) &event[4];

    switch (cmd) {
    case TMR_WAIT_REL:
	parm += prev_event_time;
    case TMR_WAIT_ABS:
	if (parm > 0) {
	    long            time;

	    if (parm <= curr_ticks)	/* It's the time */
		return TIMER_NOT_ARMED;

	    time = parm;
	    next_event_time = prev_event_time = time;

	    return TIMER_ARMED;
	}
	break;

    case TMR_START:
	tmr_reset();
	tmr_running = 1;
	break;

    case TMR_STOP:
	tmr_running = 0;
	break;

    case TMR_CONTINUE:
	tmr_running = 1;
	break;

    case TMR_TEMPO:
	if (parm) {
	    RANGE (parm, 8, 360) ;
	    tmr_offs = tmr_ctr;
	    ticks_offs += tmr2ticks(tmr_ctr);
	    tmr_ctr = 0;
	    curr_tempo = parm;
	}
	break;

    case TMR_ECHO:
	seq_copy_to_input(event, 8);
	break;

    default:;
    }

    return TIMER_NOT_ARMED;
}

static u_long
def_tmr_get_time(int dev)
{
    if (!opened)
	return 0;

    return curr_ticks;
}

static int
def_tmr_ioctl(int dev, u_int cmd, ioctl_arg arg)
{
    switch (cmd) {
    case SNDCTL_TMR_SOURCE:
	return *(int *) arg = TMR_INTERNAL;
	break;

    case SNDCTL_TMR_START:
	tmr_reset();
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
	    int             val = (*(int *) arg);

	    if (val) {
		RANGE (val, 1, 1000) ;
		curr_timebase = val;
	    }
	    return *(int *) arg = curr_timebase;
	}
	break;

    case SNDCTL_TMR_TEMPO:
	{
	    int             val = (*(int *) arg);

	    if (val) {
		RANGE (val, 8, 250) ;
		tmr_offs = tmr_ctr;
		ticks_offs += tmr2ticks(tmr_ctr);
		tmr_ctr = 0;
		curr_tempo = val;
	    }
	    return *(int *) arg = curr_tempo;
	}
	break;

    case SNDCTL_SEQ_CTRLRATE:
	if ((*(int *) arg) != 0)	/* Can't change */
	    return -(EINVAL);

	return *(int *) arg = ((curr_tempo * curr_timebase) + 30) / 60;
	break;

    case SNDCTL_TMR_METRONOME:
	/* NOP */
	break;

    default:;
    }

    return -(EINVAL);
}

static void
def_tmr_arm(int dev, long time)
{
    if (time < 0)
	time = curr_ticks + 1;
    else if (time <= curr_ticks)	/* It's the time */
	return;

    next_event_time = prev_event_time = time;

    return;
}

struct sound_timer_operations default_sound_timer =
{
	{"System clock", 0},
	0,			/* Priority */
	0,			/* Local device link */
	def_tmr_open,
	def_tmr_close,
	def_tmr_event,
	def_tmr_get_time,
	def_tmr_ioctl,
	def_tmr_arm
};

#endif
#endif	/* NSND > 0 */
