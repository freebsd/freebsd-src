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

typedef struct timespec {
	time_t	tv_sec;
	long	tv_nsec;
} timespec_t;

#define TIMEOFDAY	0	/* getclock() clktyp arg */
extern int getclock(int, struct timespec *ts);
extern int gettimeofday(struct timeval *, void *);
extern int settimeofday(struct timeval *);
extern void init_win_precise_time(void);

#endif /* SYS_TIME_H */
