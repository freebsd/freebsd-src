/*
 * Copyright (c) 1983, 1995-1997 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)clock.c	8.24 (Berkeley) 4/19/97";
#endif /* not lint */

# include "sendmail.h"

# ifndef sigmask
#  define sigmask(s)	(1 << ((s) - 1))
# endif

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

EVENT	*FreeEventList;		/* list of free events */

static SIGFUNC_DECL	tick __P((int));

EVENT *
setevent(intvl, func, arg)
	time_t intvl;
	void (*func)();
	int arg;
{
	register EVENT **evp;
	register EVENT *ev;
	auto time_t now;
	int wasblocked;

	if (intvl <= 0)
	{
		syserr("554 setevent: intvl=%ld\n", intvl);
		return (NULL);
	}

	wasblocked = blocksignal(SIGALRM);
	(void) time(&now);

	/* search event queue for correct position */
	for (evp = &EventQueue; (ev = *evp) != NULL; evp = &ev->ev_link)
	{
		if (ev->ev_time >= now + intvl)
			break;
	}

	/* insert new event */
	ev = FreeEventList;
	if (ev == NULL)
		ev = (EVENT *) xalloc(sizeof *ev);
	else
		FreeEventList = ev->ev_link;
	ev->ev_time = now + intvl;
	ev->ev_func = func;
	ev->ev_arg = arg;
	ev->ev_pid = getpid();
	ev->ev_link = *evp;
	*evp = ev;

	if (tTd(5, 5))
		printf("setevent: intvl=%ld, for=%ld, func=%lx, arg=%d, ev=%lx\n",
			intvl, now + intvl, (u_long) func, arg, (u_long) ev);

	setsignal(SIGALRM, tick);
	intvl = EventQueue->ev_time - now;
	(void) alarm((unsigned) intvl < 1 ? 1 : intvl);
	if (wasblocked == 0)
		(void) releasesignal(SIGALRM);
	return (ev);
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
		printf("clrevent: ev=%lx\n", (u_long) ev);
	if (ev == NULL)
		return;

	/* find the parent event */
	wasblocked = blocksignal(SIGALRM);
	for (evp = &EventQueue; *evp != NULL; evp = &(*evp)->ev_link)
	{
		if (*evp == ev)
			break;
	}

	/* now remove it */
	if (*evp != NULL)
	{
		*evp = ev->ev_link;
		ev->ev_link = FreeEventList;
		FreeEventList = ev;
	}

	/* restore clocks and pick up anything spare */
	if (wasblocked == 0)
		releasesignal(SIGALRM);
	if (EventQueue != NULL)
		kill(getpid(), SIGALRM);
}
/*
**  TICK -- take a clock tick
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
*/

static SIGFUNC_DECL
tick(arg)
	int arg;
{
	register time_t now;
	register EVENT *ev;
	int mypid = getpid();
	int olderrno = errno;

	(void) alarm(0);
	now = curtime();

	if (tTd(5, 4))
		printf("tick: now=%ld\n", now);

	/* reset signal in case System V semantics */
	(void) setsignal(SIGALRM, tick);
	while ((ev = EventQueue) != NULL &&
	       (ev->ev_time <= now || ev->ev_pid != mypid))
	{
		void (*f)();
		int arg;
		int pid;

		/* process the event on the top of the queue */
		ev = EventQueue;
		EventQueue = EventQueue->ev_link;
		if (tTd(5, 6))
			printf("tick: ev=%lx, func=%lx, arg=%d, pid=%d\n",
				(u_long) ev, (u_long) ev->ev_func,
				ev->ev_arg, ev->ev_pid);

		/* we must be careful in here because ev_func may not return */
		f = ev->ev_func;
		arg = ev->ev_arg;
		pid = ev->ev_pid;
		ev->ev_link = FreeEventList;
		FreeEventList = ev;
		if (pid != getpid())
			continue;
		if (EventQueue != NULL)
		{
			if (EventQueue->ev_time > now)
				(void) alarm((unsigned) (EventQueue->ev_time - now));
			else
				(void) alarm(3);
		}

		/* call ev_func */
		errno = olderrno;
		(*f)(arg);
		(void) alarm(0);
		now = curtime();
	}
	if (EventQueue != NULL)
		(void) alarm((unsigned) (EventQueue->ev_time - now));
	errno = olderrno;
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

static bool	SleepDone;
static void	endsleep();

#ifndef SLEEP_T
# define SLEEP_T	unsigned int
#endif

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
		pause();
	if (was_held > 0)
		blocksignal(SIGALRM);
	return (SLEEP_T) 0;
}

static void
endsleep()
{
	SleepDone = TRUE;
}
