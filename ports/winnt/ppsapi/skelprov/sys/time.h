/*
 * This sys/time.h is part of ppsapi-prov skeleton sample source code
 * for a Windows PPSAPI provider DLL.  It was adapted from
 * ports/winnt/include/sys/time.h in ntpd.
 */

#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <windows.h>
#include <time.h>

typedef struct timespec {
	time_t	tv_sec;
	long	tv_nsec;
} timespec_t;

#endif /* SYS_TIME_H */
