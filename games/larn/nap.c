/* nap.c		 Larn is copyrighted 1986 by Noah Morgan. */
/* $FreeBSD: src/games/larn/nap.c,v 1.4 1999/11/16 02:57:23 billf Exp $ */
#include <signal.h>
#include <sys/types.h>
#ifdef SYSV
#include <sys/times.h>
#else
#ifdef BSD
#include <sys/timeb.h>
#endif BSD
#endif SYSV

/*
 *	routine to take a nap for n milliseconds
 */
nap(x)
	int x;
	{
	if (x<=0) return; /* eliminate chance for infinite loop */
	lflush();
#if 0
	if (x > 999) sleep(x/1000); else napms(x);
#else
	usleep(x*1000);
#endif
	}

#ifdef NONAP
napms(x)	/* do nothing */
	int x;
	{
	}
#else NONAP
#ifdef SYSV
/*	napms - sleep for time milliseconds - uses times() */
/* this assumes that times returns a relative time in 60ths of a second */
/* this will do horrible things if your times() returns seconds! */
napms(time)
	int time;
	{
	long matchclock, times();
	struct tms stats;

	if (time<=0) time=1; /* eliminate chance for infinite loop */
	if ((matchclock = times(&stats)) == -1 || matchclock == 0)
		return;	/* error, or BSD style times() */
	matchclock += (time / 17);		/*17 ms/tic is 1000 ms/sec / 60 tics/sec */

	while(matchclock < times(&stats))
		;
	}

#else not SYSV
#ifdef BSD
#ifdef SIGVTALRM
/* This must be BSD 4.2!  */
#include <sys/time.h>
#define bit(_a) (1<<((_a)-1))

static  nullf()
    {
    }

/*	napms - sleep for time milliseconds - uses setitimer() */
napms(time)
	int time;
    {
    struct itimerval    timeout;
    int     (*oldhandler) ();
    int     oldsig;

	if (time <= 0) return;

    timerclear(&timeout.it_interval);
    timeout.it_value.tv_sec = time / 1000;
    timeout.it_value.tv_usec = (time % 1000) * 1000;

    oldsig = sigblock(bit(SIGALRM));
    setitimer(ITIMER_REAL, &timeout, (struct itimerval *)0);
    oldhandler = signal(SIGALRM, nullf);
    sigpause(oldsig);
    signal(SIGALRM, oldhandler);
    sigsetmask(oldsig);
    }

#else
/*	napms - sleep for time milliseconds - uses ftime() */

static napms(time)
	int time;
	{
	/* assumed to be BSD UNIX */
	struct timeb _gtime;
	time_t matchtime;
	unsigned short matchmilli;
	struct timeb *tp = & _gtime;

	if (time <= 0) return;
	ftime(tp);
	matchmilli = tp->millitm + time;
	matchtime  = tp->time;
	while (matchmilli >= 1000)
		{
		++matchtime;
		matchmilli -= 1000;
		}

	while(1)
		{
		ftime(tp);
		if ((tp->time > matchtime) ||
			((tp->time == matchtime) && (tp->millitm >= matchmilli)))
			break;
		}
	}
#endif
#else not BSD
static napms(time) int time; {}	/* do nothing, forget it */
#endif BSD
#endif SYSV
#endif NONAP
