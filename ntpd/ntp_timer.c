/*
 * ntp_timer.c - event timer support routines
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntp_machine.h"
#include "ntpd.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <signal.h>
#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if defined(HAVE_IO_COMPLETION_PORT)
# include "ntp_iocompletionport.h"
# include "ntp_timer.h"
#endif

#ifdef KERNEL_PLL
#include "ntp_syscall.h"
#endif /* KERNEL_PLL */

#ifdef OPENSSL
#include <openssl/rand.h>
#endif /* OPENSSL */

/*
 * These routines provide support for the event timer.	The timer is
 * implemented by an interrupt routine which sets a flag once every
 * 2**EVENT_TIMEOUT seconds (currently 4), and a timer routine which
 * is called when the mainline code gets around to seeing the flag.
 * The timer routine dispatches the clock adjustment code if its time
 * has come, then searches the timer queue for expiries which are
 * dispatched to the transmit procedure.  Finally, we call the hourly
 * procedure to do cleanup and print a message.
 */
volatile int interface_interval = 300;     /* update interface every 5 minutes as default */
	  
/*
 * Alarm flag. The mainline code imports this.
 */
volatile int alarm_flag;

/*
 * The counters and timeouts
 */
static  u_long interface_timer;	/* interface update timer */
static	u_long adjust_timer;	/* second timer */
static	u_long stats_timer;	/* stats timer */
static	u_long huffpuff_timer;	/* huff-n'-puff timer */
u_long	leapsec;		/* leapseconds countdown */
l_fp	sys_time;		/* current system time */
#ifdef OPENSSL
static	u_long revoke_timer;	/* keys revoke timer */
static	u_long keys_timer;	/* session key timer */
u_long	sys_revoke = KEY_REVOKE; /* keys revoke timeout (log2 s) */
u_long	sys_automax = NTP_AUTOMAX; /* key list timeout (log2 s) */
#endif /* OPENSSL */

/*
 * Statistics counter for the interested.
 */
volatile u_long alarm_overflow;

#define MINUTE	60
#define HOUR	(60 * MINUTE)
#define	DAY	(24 * HOUR)

u_long current_time;		/* seconds since startup */

/*
 * Stats.  Number of overflows and number of calls to transmit().
 */
u_long timer_timereset;
u_long timer_overflows;
u_long timer_xmtcalls;

#if defined(VMS)
static int vmstimer[2]; 	/* time for next timer AST */
static int vmsinc[2];		/* timer increment */
#endif /* VMS */

#if defined SYS_WINNT
static HANDLE WaitableTimerHandle = NULL;
#else
static	RETSIGTYPE alarming (int);
#endif /* SYS_WINNT */

#if !defined(VMS)
# if !defined SYS_WINNT || defined(SYS_CYGWIN32)
#  ifndef HAVE_TIMER_SETTIME
	struct itimerval itimer;
#  else 
	static timer_t ntpd_timerid;
	struct itimerspec itimer;
#  endif /* HAVE_TIMER_SETTIME */
# endif /* SYS_WINNT */
#endif /* VMS */

/*
 * reinit_timer - reinitialize interval timer.
 */
void 
reinit_timer(void)
{
#if !defined(SYS_WINNT) && !defined(VMS)
#  if defined(HAVE_TIMER_CREATE) && defined(HAVE_TIMER_SETTIME)
	timer_gettime(ntpd_timerid, &itimer);
	if (itimer.it_value.tv_sec < 0 || itimer.it_value.tv_sec > (1<<EVENT_TIMEOUT)) {
		itimer.it_value.tv_sec = (1<<EVENT_TIMEOUT);
	}
	if (itimer.it_value.tv_nsec < 0 ) {
		itimer.it_value.tv_nsec = 0;
	}
	if (itimer.it_value.tv_sec == 0 && itimer.it_value.tv_nsec == 0) {
		itimer.it_value.tv_sec = (1<<EVENT_TIMEOUT);
		itimer.it_value.tv_nsec = 0;
	}
	itimer.it_interval.tv_sec = (1<<EVENT_TIMEOUT);
	itimer.it_interval.tv_nsec = 0;
	timer_settime(ntpd_timerid, 0 /*!TIMER_ABSTIME*/, &itimer, NULL);
#  else
	getitimer(ITIMER_REAL, &itimer);
	if (itimer.it_value.tv_sec < 0 || itimer.it_value.tv_sec > (1<<EVENT_TIMEOUT)) {
		itimer.it_value.tv_sec = (1<<EVENT_TIMEOUT);
	}
	if (itimer.it_value.tv_usec < 0 ) {
		itimer.it_value.tv_usec = 0;
	}
	if (itimer.it_value.tv_sec == 0 && itimer.it_value.tv_usec == 0) {
		itimer.it_value.tv_sec = (1<<EVENT_TIMEOUT);
		itimer.it_value.tv_usec = 0;
	}
	itimer.it_interval.tv_sec = (1<<EVENT_TIMEOUT);
	itimer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
#  endif
# endif /* VMS */
}

/*
 * init_timer - initialize the timer data structures
 */
void
init_timer(void)
{
	/*
	 * Initialize...
	 */
	alarm_flag = 0;
	alarm_overflow = 0;
	adjust_timer = 1;
	stats_timer = 0;
	huffpuff_timer = 0;
	interface_timer = 0;
	current_time = 0;
	timer_overflows = 0;
	timer_xmtcalls = 0;
	timer_timereset = 0;

#if !defined(SYS_WINNT)
	/*
	 * Set up the alarm interrupt.	The first comes 2**EVENT_TIMEOUT
	 * seconds from now and they continue on every 2**EVENT_TIMEOUT
	 * seconds.
	 */
# if !defined(VMS)
#  if defined(HAVE_TIMER_CREATE) && defined(HAVE_TIMER_SETTIME)
	if (timer_create (CLOCK_REALTIME, NULL, &ntpd_timerid) ==
#	ifdef SYS_VXWORKS
		ERROR
#	else
		-1
#	endif
	   )
	{
		fprintf (stderr, "timer create FAILED\n");
		exit (0);
	}
	(void) signal_no_reset(SIGALRM, alarming);
	itimer.it_interval.tv_sec = itimer.it_value.tv_sec = (1<<EVENT_TIMEOUT);
	itimer.it_interval.tv_nsec = itimer.it_value.tv_nsec = 0;
	timer_settime(ntpd_timerid, 0 /*!TIMER_ABSTIME*/, &itimer, NULL);
#  else
	(void) signal_no_reset(SIGALRM, alarming);
	itimer.it_interval.tv_sec = itimer.it_value.tv_sec = (1<<EVENT_TIMEOUT);
	itimer.it_interval.tv_usec = itimer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
#  endif
# else /* VMS */
	vmsinc[0] = 10000000;		/* 1 sec */
	vmsinc[1] = 0;
	lib$emul(&(1<<EVENT_TIMEOUT), &vmsinc, &0, &vmsinc);

	sys$gettim(&vmstimer);	/* that's "now" as abstime */

	lib$addx(&vmsinc, &vmstimer, &vmstimer);
	sys$setimr(0, &vmstimer, alarming, alarming, 0);
# endif /* VMS */
#else /* SYS_WINNT */
	/*
	 * Set up timer interrupts for every 2**EVENT_TIMEOUT seconds
	 * Under Windows/NT, 
	 */

	WaitableTimerHandle = CreateWaitableTimer(NULL, FALSE, NULL);
	if (WaitableTimerHandle == NULL) {
		msyslog(LOG_ERR, "CreateWaitableTimer failed: %m");
		exit(1);
	}
	else {
		DWORD Period = (1<<EVENT_TIMEOUT) * 1000;
		LARGE_INTEGER DueTime;
		DueTime.QuadPart = Period * 10000i64;
		if (!SetWaitableTimer(WaitableTimerHandle, &DueTime, Period, NULL, NULL, FALSE) != NO_ERROR) {
			msyslog(LOG_ERR, "SetWaitableTimer failed: %m");
			exit(1);
		}
	}

#endif /* SYS_WINNT */
}

#if defined(SYS_WINNT)
extern HANDLE 
get_timer_handle(void)
{
	return WaitableTimerHandle;
}
#endif

/*
 * timer - event timer
 */
void
timer(void)
{
	register struct peer *peer, *next_peer;
	u_int	n;

	/*
	 * The basic timerevent is one second. This is used to adjust
	 * the system clock in time and frequency, implement the
	 * kiss-o'-deatch function and implement the association
	 * polling function..
	 */
	current_time++;
	get_systime(&sys_time);
	if (adjust_timer <= current_time) {
		adjust_timer += 1;
		adj_host_clock();
#ifdef REFCLOCK
		for (n = 0; n < NTP_HASH_SIZE; n++) {
			for (peer = peer_hash[n]; peer != 0; peer = next_peer) {
				next_peer = peer->next;
				if (peer->flags & FLAG_REFCLOCK)
					refclock_timer(peer);
			}
		}
#endif /* REFCLOCK */
	}

	/*
	 * Now dispatch any peers whose event timer has expired. Be
	 * careful here, since the peer structure might go away as the
	 * result of the call.
	 */
	for (n = 0; n < NTP_HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != 0; peer = next_peer) {
			next_peer = peer->next;
			if (peer->action && peer->nextaction <=
			    current_time)
				peer->action(peer);

			/*
			 * Restrain the non-burst packet rate not more
			 * than one packet every 16 seconds. This is
			 * usually tripped using iburst and minpoll of
			 * 128 s or less.
			 */
			if (peer->throttle > 0)
				peer->throttle--;
			if (peer->nextdate <= current_time) {
#ifdef REFCLOCK
				if (peer->flags & FLAG_REFCLOCK)
					refclock_transmit(peer);
				else
					transmit(peer);
#else /* REFCLOCK */
				transmit(peer);
#endif /* REFCLOCK */
			}
		}
	}

	/*
	 * Orphan mode is active when enabled and when no servers less
	 * than the orphan stratum are available. A server with no other
	 * synchronization source is an orphan. It shows offset zero and
	 * reference ID the loopback address.
	 */
	if (sys_orphan < STRATUM_UNSPEC && sys_peer == NULL) {
		if (sys_leap == LEAP_NOTINSYNC) {
			sys_leap = LEAP_NOWARNING;
#ifdef OPENSSL
			if (crypto_flags)	
				crypto_update();
#endif /* OPENSSL */
		}
		sys_stratum = (u_char)sys_orphan;
		if (sys_stratum > 1)
			sys_refid = htonl(LOOPBACKADR);
		else
			memcpy(&sys_refid, "LOOP", 4);
		sys_offset = 0;
		sys_rootdelay = 0;
		sys_rootdisp = 0;
	}

	/*
	 * Leapseconds. If a leap is pending, decrement the time
	 * remaining. If less than one day remains, set the leap bits.
	 * When no time remains, clear the leap bits and increment the
	 * TAI. If kernel suppport is not available, do the leap
	 * crudely. Note a leap cannot be pending unless the clock is
	 * set.
	 */
	if (leapsec > 0) {
		leapsec--;
		if (leapsec == 0) {
			sys_leap = LEAP_NOWARNING;
			sys_tai = leap_tai;
#ifdef KERNEL_PLL
			if (!(pll_control && kern_enable))
				step_systime(-1.0);
#else /* KERNEL_PLL */
#ifndef SYS_WINNT /* WinNT port has its own leap second handling */
			step_systime(-1.0);
#endif /* SYS_WINNT */
#endif /* KERNEL_PLL */
			report_event(EVNT_LEAP, NULL, NULL);
		} else {
			if (leapsec < DAY)
				sys_leap = LEAP_ADDSECOND;
			if (leap_tai > 0)
				sys_tai = leap_tai - 1;
		}
	}

	/*
	 * Update huff-n'-puff filter.
	 */
	if (huffpuff_timer <= current_time) {
		huffpuff_timer += HUFFPUFF;
		huffpuff();
	}

#ifdef OPENSSL
	/*
	 * Garbage collect expired keys.
	 */
	if (keys_timer <= current_time) {
		keys_timer += 1 << sys_automax;
		auth_agekeys();
	}

	/*
	 * Garbage collect key list and generate new private value. The
	 * timer runs only after initial synchronization and fires about
	 * once per day.
	 */
	if (revoke_timer <= current_time && sys_leap !=
	    LEAP_NOTINSYNC) {
		revoke_timer += 1 << sys_revoke;
		RAND_bytes((u_char *)&sys_private, 4);
	}
#endif /* OPENSSL */

	/*
	 * Interface update timer
	 */
	if (interface_interval && interface_timer <= current_time) {

		timer_interfacetimeout(current_time +
		    interface_interval);
		DPRINTF(2, ("timer: interface update\n"));
		interface_update(NULL, NULL);
	}
	
	/*
	 * Finally, write hourly stats.
	 */
	if (stats_timer <= current_time) {
		stats_timer += HOUR;
		write_stats();
		if (sys_tai != 0 && sys_time.l_ui > leap_expire)
			report_event(EVNT_LEAPVAL, NULL, NULL);
	}
}


#ifndef SYS_WINNT
/*
 * alarming - tell the world we've been alarmed
 */
static RETSIGTYPE
alarming(
	int sig
	)
{
#if !defined(VMS)
	if (initializing)
		return;
	if (alarm_flag)
		alarm_overflow++;
	else
		alarm_flag++;
#else /* VMS AST routine */
	if (!initializing) {
		if (alarm_flag) alarm_overflow++;
		else alarm_flag = 1;	/* increment is no good */
	}
	lib$addx(&vmsinc,&vmstimer,&vmstimer);
	sys$setimr(0,&vmstimer,alarming,alarming,0);
#endif /* VMS */
}
#endif /* SYS_WINNT */

void
timer_interfacetimeout(u_long timeout)
{
	interface_timer = timeout;
}


/*
 * timer_clr_stats - clear timer module stat counters
 */
void
timer_clr_stats(void)
{
	timer_overflows = 0;
	timer_xmtcalls = 0;
	timer_timereset = current_time;
}

