/* machines.c - provide special support for peculiar architectures
 *
 * Real bummers unite !
 *
 * $FreeBSD$
 */

#include "ntp_stdlib.h"

#ifdef SYS_PTX
#include <sys/types.h>
#include <sys/procstats.h>

int
settimeofday(tvp)
	struct timeval *tvp;
{
	return (stime(&tvp->tv_sec));	/* lie as bad as SysVR4 */
}

int
gettimeofday(tvp)
	struct timeval *tvp;
{
	/*
	 * hi, this is Sequents sneak path to get to a clock
	 * this is also the most logical syscall for such a function
	 */
	return (get_process_stats(tvp, PS_SELF, (struct procstats *) 0,
				  (struct procstats *) 0));
}
#endif

#if !defined(NTP_POSIX_SOURCE) || defined(NTP_NEED_BOPS)
void
ntp_memset(a, x, c)
	char *a;
	int x, c;
{
	while (c-- > 0)
		*a++ = x;
}
#endif /*POSIX*/

#if defined(USE_CLOCK_SETTIME)

#include <time.h>

int
settimeofday(tvp)
	struct timeval *tvp;
{
	struct timespec ts;

	/* Convert timeval to timespec */
	ts.tv_sec = tvp->tv_sec;
	ts.tv_nsec = 1000 *  tvp->tv_usec;

	return clock_settime(CLOCK_REALTIME, &ts);
}
#endif /* USE_CLOCK_SETTIME */
