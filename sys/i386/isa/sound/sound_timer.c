/*
 * sound/sound_timer.c
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

#if NSND > 0

#if defined(CONFIG_SEQUENCER)

static volatile int initialized = 0, opened = 0, tmr_running = 0;
static volatile time_t tmr_offs, tmr_ctr;
static volatile u_long ticks_offs;
static volatile int curr_tempo, curr_timebase;
static volatile u_long curr_ticks;
static volatile u_long next_event_time;
static u_long prev_event_time;
static volatile u_long usecs_per_tmr;	/* Length of the current interval */

static struct sound_lowlev_timer *tmr = NULL;

static u_long
tmr2ticks(int tmr_value)
{
    /*
     * Convert timer ticks to MIDI ticks
     */

    u_long   tmp;
    u_long   scale;

    tmp = tmr_value * usecs_per_tmr;	/* Convert to usecs */

    scale = (60 * 1000000) / (curr_tempo * curr_timebase);	/* usecs per MIDI tick */

    return (tmp + (scale / 2)) / scale;
}

static void
reprogram_timer(void)
{
    u_long   usecs_per_tick;

    usecs_per_tick = (60 * 1000000) / (curr_tempo * curr_timebase);

    /*
     * Don't kill the system by setting too high timer rate
     */
    if (usecs_per_tick < 2000)
	usecs_per_tick = 2000;

    usecs_per_tmr = tmr->tmr_start(tmr->dev, usecs_per_tick);
}

void
sound_timer_syncinterval(u_int new_usecs)
{
    /*
     * This routine is called by the hardware level if the clock
     * frequency has changed for some reason.
     */
    tmr_offs = tmr_ctr;
    ticks_offs += tmr2ticks(tmr_ctr);
    tmr_ctr = 0;

    usecs_per_tmr = new_usecs;
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
timer_open(int dev, int mode)
{
    if (opened)
	return -(EBUSY);

    tmr_reset();
    curr_tempo = 60;
    curr_timebase = hz;
    opened = 1;
    reprogram_timer();

    return 0;
}

static void
timer_close(int dev)
{
    opened = tmr_running = 0;
    tmr->tmr_disable(tmr->dev);
}

static int
timer_event(int dev, u_char *event)
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
	reprogram_timer();
	break;

    case TMR_STOP:
	tmr_running = 0;
	break;

    case TMR_CONTINUE:
	tmr_running = 1;
	reprogram_timer();
	break;

    case TMR_TEMPO:
	if (parm) {
	    if (parm < 8)
		parm = 8;
	    if (parm > 250)
		parm = 250;
	    tmr_offs = tmr_ctr;
	    ticks_offs += tmr2ticks(tmr_ctr);
	    tmr_ctr = 0;
	    curr_tempo = parm;
	    reprogram_timer();
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
timer_get_time(int dev)
{
    if (!opened)
	return 0;

    return curr_ticks;
}

static int
timer_ioctl(int dev, u_int cmd, ioctl_arg arg)
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
		if (val < 1)
		    val = 1;
		if (val > 1000)
		    val = 1000;
		curr_timebase = val;
	    }
	    return *(int *) arg = curr_timebase;
	}
	break;

    case SNDCTL_TMR_TEMPO:
	{
	    int             val = (*(int *) arg);

	    if (val) {
		if (val < 8)
		    val = 8;
		if (val > 250)
		    val = 250;
		tmr_offs = tmr_ctr;
		ticks_offs += tmr2ticks(tmr_ctr);
		tmr_ctr = 0;
		curr_tempo = val;
		reprogram_timer();
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
timer_arm(int dev, long time)
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
	{"GUS Timer", 0},
	1,			/* Priority */
	0,			/* Local device link */
	timer_open,
	timer_close,
	timer_event,
	timer_get_time,
	timer_ioctl,
	timer_arm
};

void
sound_timer_interrupt(void)
{
    if (!opened)
	return;

    tmr->tmr_restart(tmr->dev);

    if (!tmr_running)
	return;

    tmr_ctr++;
    curr_ticks = ticks_offs + tmr2ticks(tmr_ctr);

    if (curr_ticks >= next_event_time) {
	next_event_time = 0xffffffff;
	sequencer_timer(0);
    }
}

void
sound_timer_init(struct sound_lowlev_timer * t, char *name)
{
    int             n;

    if (initialized || t == NULL)
	return;		/* There is already a similar timer */

    initialized = 1;
    tmr = t;

    if (num_sound_timers >= MAX_TIMER_DEV)
	n = 0;		/* Overwrite the system timer */
    else
	n = num_sound_timers++;

    strcpy(sound_timer.info.name, name);

    sound_timer_devs[n] = &sound_timer;
}

#endif
#endif
