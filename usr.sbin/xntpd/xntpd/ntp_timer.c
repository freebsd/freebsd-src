/* ntp_timer.c,v 3.1 1993/07/06 01:11:29 jbj Exp
 * ntp_event.c - event timer support routines
 */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/signal.h>

#include "ntpd.h"
#include "ntp_stdlib.h"

/*
 * These routines provide support for the event timer.  The timer is
 * implemented by an interrupt routine which sets a flag once every
 * 2**EVENT_TIMEOUT seconds (currently 4), and a timer routine which
 * is called when the mainline code gets around to seeing the flag.
 * The timer routine dispatches the clock adjustment code if its time
 * has come, then searches the timer queue for expiries which are
 * dispatched to the transmit procedure.  Finally, we call the hourly
 * procedure to do cleanup and print a message.
 */

/*
 * Alarm flag.  The mainline code imports this.
 */
int alarm_flag;

/*
 * adjust and hourly counters
 */
static	U_LONG adjust_timer;
static	U_LONG hourly_timer;

/*
 * Imported from the leap module.  The leap timer.
 */
extern U_LONG leap_timer;

/*
 * Statistics counter for the interested.
 */
U_LONG alarm_overflow;

#define	HOUR	(60*60)

/*
 * Current_time holds the number of seconds since we started, in
 * increments of 2**EVENT_TIMEOUT seconds.  The timer queue is the
 * hash into which we sort timer entries.
 */
U_LONG current_time;
struct event timerqueue[TIMER_NSLOTS];

/*
 * Stats.  Number of overflows and number of calls to transmit().
 */
U_LONG timer_timereset;
U_LONG timer_overflows;
U_LONG timer_xmtcalls;

static	RETSIGTYPE alarming	P((int));

/*
 * init_timer - initialize the timer data structures
 */
void
init_timer()
{
	register int i;
	struct itimerval itimer;

	/*
	 * Initialize...
	 */
	alarm_flag = 0;
	alarm_overflow = 0;
	adjust_timer = (1<<CLOCK_ADJ);
	hourly_timer = HOUR;
	current_time = 0;
	timer_overflows = 0;
	timer_xmtcalls = 0;
	timer_timereset = 0;

	for (i = 0; i < TIMER_NSLOTS; i++) {
		/*
		 * Queue pointers should point at themselves.  Event
		 * times must be set to 0 since this is used to
		 * detect the queue end.
		 */
		timerqueue[i].next = &timerqueue[i];
		timerqueue[i].prev = &timerqueue[i];
		timerqueue[i].event_time = 0;
	}

	/*
	 * Set up the alarm interrupt.  The first comes 2**EVENT_TIMEOUT
	 * seconds from now and they continue on every 2**EVENT_TIMEOUT
	 * seconds.
	 */
	(void) signal_no_reset(SIGALRM, alarming);
	itimer.it_interval.tv_sec = itimer.it_value.tv_sec = (1<<EVENT_TIMEOUT);
	itimer.it_interval.tv_usec = itimer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
}



/*
 * timer - dispatch anyone who needs to be
 */
void
timer()
{
	register struct event *ev;
	register struct event *tq;

	current_time += (1<<EVENT_TIMEOUT);

	/*
	 * Adjustment timeout first
	 */
	if (adjust_timer <= current_time) {
		adjust_timer += (1<<CLOCK_ADJ);
		adj_host_clock();
	}

	/*
	 * Leap timer next.
	 */
	if (leap_timer != 0 && leap_timer <= current_time)
		leap_process();

	/*
	 * Now dispatch any peers whose event timer has expired.
	 */
	tq = &timerqueue[TIMER_SLOT(current_time)];
	ev = tq->next;
	while (ev->event_time != 0
	    && ev->event_time < (current_time + (1<<EVENT_TIMEOUT))) {
		tq->next = ev->next;
		tq->next->prev = tq;
		ev->prev = ev->next = 0;
		timer_xmtcalls++;
		ev->event_handler(ev->peer);
		ev = tq->next;
	}

	/*
	 * Finally, call the hourly routine
	 */
	if (hourly_timer <= current_time) {
		hourly_timer += HOUR;
		hourly_stats();
	}
}


/*
 * alarming - tell the world we've been alarmed
 */
static RETSIGTYPE
alarming(sig)
int sig;
{
	extern int initializing;	/* from main line code */

	if (initializing)
		return;
	if (alarm_flag)
		alarm_overflow++;
	else
		alarm_flag++;
}


/*
 * timer_clr_stats - clear timer module stat counters
 */
void
timer_clr_stats()
{
	timer_overflows = 0;
	timer_xmtcalls = 0;
	timer_timereset = current_time;
}
