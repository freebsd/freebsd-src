/*
 * ports/winnt/include/sys/time.h
 *
 * routines declared in Unix systems' sys/time.h
 */

#ifndef SYS_TIME_H
#define SYS_TIME_H

#include "ntp_types.h"
#include <time.h>
#include <sys/timeb.h>

#if defined(_MSC_VER) && _MSC_VER < 1900 
typedef struct timespec {
	time_t	tv_sec;
	long	tv_nsec;
} timespec_t;
#endif

#define TIMEOFDAY	0	/* getclock() clktyp arg */
extern int getclock(int, struct timespec *ts);
extern int gettimeofday(struct timeval *, void *);
extern int settimeofday(struct timeval *);
extern void init_win_precise_time(void);

#endif /* SYS_TIME_H */
