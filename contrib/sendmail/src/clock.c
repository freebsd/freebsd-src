/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: clock.c,v 8.52.18.18 2001/08/14 16:07:04 ca Exp $";
#endif /* ! lint */

#include <sendmail.h>

#ifndef sigmask
# define sigmask(s)	(1 << ((s) - 1))
#endif /* ! sigmask */

static SIGFUNC_DECL	sm_tick __P((int));
static void		endsleep __P((void));


/*
**  SETEVENT -- set an event to happen at a specific time.
**
**	Events are stored in a sorted list for fast processing.
**	An event only applies to the process that set it.
**
**	Parameters:
**		intvl -- intvl until next event occurs.
**		func -- function to call on event.
**		arg -- argument to func on event.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

static EVENT	*volatile EventQueue;		/* head of event queue */
static EVENT	*volatile FreeEventList;	/* list of free events */

EVENT *
setevent(intvl, func, arg)
	time_t intvl;
	void (*func)();
	int arg;
{
	register EVENT *ev;

	if (intvl <= 0)
	{
		syserr("554 5.3.0 setevent: intvl=%ld\n", intvl);
		return NULL;
	}

	ENTER_CRITICAL();
	if (FreeEventList == NULL)
	{
		FreeEventList = (EVENT *) xalloc(sizeof *FreeEventList);
		FreeEventList->ev_link = NULL;
	}
	LEAVE_CRITICAL();

	ev = sigsafe_setevent(intvl, func, arg);

	if (tTd(5, 5))
		dprintf("setevent: intvl=%ld, for=%ld, func=%lx, arg=%d, ev=%lx\n",
			(long) intvl, (long) (curtime() + intvl),
			(u_long) func, arg,
			ev == NULL ? 0 : (u_long) ev);

	return ev;
}

/*
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
*/

EVENT *
sigsafe_setevent(intvl, func, arg)
	time_t intvl;
	void (*func)();
	int arg;
{
	register EVENT **evp;
	register EVENT *ev;
	auto time_t now;
	int wasblocked;

	if (intvl <= 0)
		return NULL;

	wasblocked = blocksignal(SIGALRM);
	now = curtime();

	/* search event queue for correct position */
	for (evp = (EVENT **) (&EventQueue);
	     (ev = *evp) != NULL;
	     evp = &ev->ev_link)
	{
		if (ev->ev_time >= now + intvl)
			break;
	}

	ENTER_CRITICAL();
	if (FreeEventList == NULL)
	{
		/*
		**  This shouldn't happen.  If called from setevent(),
		**  we have just malloced a FreeEventList entry.  If
		**  called from a signal handler, it should have been
		**  from an existing event which sm_tick() just added to the
		**  FreeEventList.
		*/

		LEAVE_CRITICAL();
		return NULL;
	}
	else
	{
		ev = FreeEventList;
		FreeEventList = ev->ev_link;
	}
	LEAVE_CRITICAL();

	/* insert new event */
	ev->ev_time = now + intvl;
	ev->ev_func = func;
	ev->ev_arg = arg;
	ev->ev_pid = getpid();
	ENTER_CRITICAL();
	ev->ev_link = *evp;
	*evp = ev;
	LEAVE_CRITICAL();

	(void) setsignal(SIGALRM, sm_tick);
	intvl = EventQueue->ev_time - now;
	(void) alarm((unsigned) intvl < 1 ? 1 : intvl);
	if (wasblocked == 0)
		(void) releasesignal(SIGALRM);
	return ev;
}
/*
**  CLREVENT -- remove an event from the event queue.
**
**	Parameters:
**		ev -- pointer to event to remove.
**
**	Returns:
**		none.
**
**	Side Effects:
**		arranges for event ev to not happen.
*/

void
clrevent(ev)
	register EVENT *ev;
{
	register EVENT **evp;
	int wasblocked;

	if (tTd(5, 5))
		dprintf("clrevent: ev=%lx\n", (u_long) ev);
	if (ev == NULL)
		return;

	/* find the parent event */
	wasblocked = blocksignal(SIGALRM);
	for (evp = (EVENT **) (&EventQueue);
	     *evp != NULL;
	     evp = &(*evp)->ev_link)
	{
		if (*evp == ev)
			break;
	}

	/* now remove it */
	if (*evp != NULL)
	{
		ENTER_CRITICAL();
		*evp = ev->ev_link;
		ev->ev_link = FreeEventList;
		FreeEventList = ev;
		LEAVE_CRITICAL();
	}

	/* restore clocks and pick up anything spare */
	if (wasblocked == 0)
		(void) releasesignal(SIGALRM);
	if (EventQueue != NULL)
		(void) kill(getpid(), SIGALRM);
	else
	{
		/* nothing left in event queue, no need for an alarm */
		(void) alarm(0);
	}
}
/*
**  CLEAR_EVENTS -- remove all events from the event queue.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
clear_events()
{
	register EVENT *ev;
	int wasblocked;

	if (tTd(5, 5))
		dprintf("clear_events: EventQueue=%lx\n", (u_long) EventQueue);

	if (EventQueue == NULL)
		return;

	/* nothing will be left in event queue, no need for an alarm */
	(void) alarm(0);
	wasblocked = blocksignal(SIGALRM);

	/* find the end of the EventQueue */
	for (ev = EventQueue; ev->ev_link != NULL; ev = ev->ev_link)
		continue;

	ENTER_CRITICAL();
	ev->ev_link = FreeEventList;
	FreeEventList = EventQueue;
	EventQueue = NULL;
	LEAVE_CRITICAL();

	/* restore clocks and pick up anything spare */
	if (wasblocked == 0)
		(void) releasesignal(SIGALRM);
}
/*
**  SM_TICK -- take a clock sm_tick
**
**	Called by the alarm clock.  This routine runs events as needed.
**	Always called as a signal handler, so we assume that SIGALRM
**	has been blocked.
**
**	Parameters:
**		One that is ignored; for compatibility with signal handlers.
**
**	Returns:
**		none.
**
**	Side Effects:
**		calls the next function in EventQueue.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
*/

/* ARGSUSED */
static SIGFUNC_DECL
sm_tick(sig)
	int sig;
{
	register time_t now;
	register EVENT *ev;
	pid_t mypid;
	int save_errno = errno;

	(void) alarm(0);

	FIX_SYSV_SIGNAL(sig, sm_tick);

	errno = save_errno;
	CHECK_CRITICAL(sig);

	mypid = getpid();
	while (PendingSignal != 0)
	{
		int sigbit = 0;
		int sig = 0;

		if (bitset(PEND_SIGHUP, PendingSignal))
		{
			sigbit = PEND_SIGHUP;
			sig = SIGHUP;
		}
		else if (bitset(PEND_SIGINT, PendingSignal))
		{
			sigbit = PEND_SIGINT;
			sig = SIGINT;
		}
		else if (bitset(PEND_SIGTERM, PendingSignal))
		{
			sigbit = PEND_SIGTERM;
			sig = SIGTERM;
		}
		else if (bitset(PEND_SIGUSR1, PendingSignal))
		{
			sigbit = PEND_SIGUSR1;
			sig = SIGUSR1;
		}
		else
		{
			/* If we get here, we are in trouble */
			abort();
		}
		PendingSignal &= ~sigbit;
		kill(mypid, sig);
	}

	now = curtime();
	if (tTd(5, 4))
		dprintf("sm_tick: now=%ld\n", (long) now);

	while ((ev = EventQueue) != NULL &&
	       (ev->ev_time <= now || ev->ev_pid != mypid))
	{
		void (*f)();
		int arg;
		pid_t pid;

		/* process the event on the top of the queue */
		ENTER_CRITICAL();
		ev = EventQueue;
		EventQueue = EventQueue->ev_link;
		LEAVE_CRITICAL();
		if (tTd(5, 6))
			dprintf("sm_tick: ev=%lx, func=%lx, arg=%d, pid=%d\n",
				(u_long) ev, (u_long) ev->ev_func,
				ev->ev_arg, ev->ev_pid);

		/* we must be careful in here because ev_func may not return */
		f = ev->ev_func;
		arg = ev->ev_arg;
		pid = ev->ev_pid;
		ENTER_CRITICAL();
		ev->ev_link = FreeEventList;
		FreeEventList = ev;
		LEAVE_CRITICAL();
		if (pid != mypid)
			continue;
		if (EventQueue != NULL)
		{
			if (EventQueue->ev_time > now)
				(void) alarm((unsigned) (EventQueue->ev_time - now));
			else
				(void) alarm(3);
		}

		/* call ev_func */
		errno = save_errno;
		(*f)(arg);
		(void) alarm(0);
		now = curtime();
	}
	if (EventQueue != NULL)
		(void) alarm((unsigned) (EventQueue->ev_time - now));
	errno = save_errno;
	return SIGFUNC_RETURN;
}
/*
**  PEND_SIGNAL -- Add a signal to the pending signal list
**
**	Parameters:
**		sig -- signal to add
**
**	Returns:
**		none.
**
**	NOTE:	THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
**		ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
**		DOING.
*/

void
pend_signal(sig)
	int sig;
{
	int sigbit;
	int save_errno = errno;

	/*
	**  Don't want to interrupt something critical, hence delay
	**  the alarm for one second.  Hopefully, by then we
	**  will be out of the critical section.  If not, then
	**  we will just delay again.  The events to be run will
	**  still all be run, maybe just a little bit late.
	*/

	switch (sig)
	{
	  case SIGHUP:
		sigbit = PEND_SIGHUP;
		break;

	  case SIGINT:
		sigbit = PEND_SIGINT;
		break;

	  case SIGTERM:
		sigbit = PEND_SIGTERM;
		break;

	  case SIGUSR1:
		sigbit = PEND_SIGUSR1;
		break;

	  case SIGALRM:
		/* don't have to pend these */
		sigbit = 0;
		break;

	  default:
		/* If we get here, we are in trouble */
		abort();

		/* NOTREACHED */
		/* shut up stupid compiler warning on HP-UX 11 */
		sigbit = 0;
		break;
	}

	if (sigbit != 0)
		PendingSignal |= sigbit;
	(void) setsignal(SIGALRM, sm_tick);
	(void) alarm(1);
	errno = save_errno;
}
/*
**  SM_SIGNAL_NOOP -- A signal no-op function
**
**	Parameters:
**		sig -- signal received
**
**	Returns:
**		SIGFUNC_RETURN
*/

/* ARGSUSED */
SIGFUNC_DECL
sm_signal_noop(sig)
	int sig;
{
	int save_errno = errno;

	FIX_SYSV_SIGNAL(sig, sm_signal_noop);
	errno = save_errno;
	return SIGFUNC_RETURN;
}
/*
**  SLEEP -- a version of sleep that works with this stuff
**
**	Because sleep uses the alarm facility, I must reimplement
**	it here.
**
**	Parameters:
**		intvl -- time to sleep.
**
**	Returns:
**		none.
**
**	Side Effects:
**		waits for intvl time.  However, other events can
**		be run during that interval.
*/


static bool	volatile SleepDone;

#ifndef SLEEP_T
# define SLEEP_T	unsigned int
#endif /* ! SLEEP_T */

SLEEP_T
sleep(intvl)
	unsigned int intvl;
{
	int was_held;

	if (intvl == 0)
		return (SLEEP_T) 0;
	SleepDone = FALSE;
	(void) setevent((time_t) intvl, endsleep, 0);
	was_held = releasesignal(SIGALRM);
	while (!SleepDone)
		(void) pause();
	if (was_held > 0)
		(void) blocksignal(SIGALRM);
	return (SLEEP_T) 0;
}

static void
endsleep()
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	SleepDone = TRUE;
}
