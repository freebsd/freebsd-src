#ifndef LINT
static const char rcsid[] = "$Id: gettimeofday.c,v 1.1.2.2 2002/07/12 00:49:51 marka Exp $";
#endif

#include "port_before.h"
#include <stdio.h>
#include <syslog.h>
#include <sys/time.h>
#include "port_after.h"

#if !defined(NEED_GETTIMEOFDAY)
/*
 * gettimeofday() occasionally returns invalid tv_usec on some platforms.
 */
#define MILLION 1000000
#undef gettimeofday

int
isc__gettimeofday(struct timeval *tp, struct timezone *tzp) {
	int res;

	res = gettimeofday(tp, tzp);
	if (res < 0)
		return (res);
	if (tp == NULL)
		return (res);
	if (tp->tv_usec < 0) {
		do {
			tp->tv_usec += MILLION;
			tp->tv_sec--;
		} while (tp->tv_usec < 0);
		goto log;
	} else if (tp->tv_usec > MILLION) {
		do {
			tp->tv_usec -= MILLION;
			tp->tv_sec++;
		} while (tp->tv_usec > MILLION);
		goto log;
	}
	return (res);
 log:
	syslog(LOG_ERR, "gettimeofday: tv_usec out of range\n");
	return (res);
}
#else
int
gettimeofday(struct timeval *tvp, struct _TIMEZONE *tzp) {
	time_t clock, time(time_t *);

	if (time(&clock) == (time_t) -1)
		return (-1);
	if (tvp) {
		tvp->tv_sec = clock;
		tvp->tv_usec = 0;
	}
	if (tzp) {
		tzp->tz_minuteswest = 0;
		tzp->tz_dsttime = 0;
	}
	return (0);
}
#endif /*NEED_GETTIMEOFDAY*/
