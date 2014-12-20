#ifndef _CLOCKSTUFF_H
#define _CLOCKSTUFF_H

#include <time.h>
#include <sys\timeb.h>

#include "ntp_fp.h"
#include "ntp_syslog.h"


void init_winnt_time(void);
void reset_winnt_time(void);
void lock_thread_to_processor(HANDLE);

/* 100ns intervals between 1/1/1601 and 1/1/1970 as reported by
 * SystemTimeToFileTime()
 */

#define FILETIME_1970     0x019db1ded53e8000
#define HECTONANOSECONDS  10000000

/*
 * Multimedia Timer
 */

void set_mm_timer(int);

enum {
	MM_TIMER_LORES,
	MM_TIMER_HIRES
};

/*
 * get_sys_time_as_filetime is a function pointer to
 * either GetSystemTimeAsFileTime provided by Windows
 * or ntpd's interpolating replacement.
 */
typedef void (WINAPI *PGSTAFT)(LPFILETIME pftResult);
extern PGSTAFT get_sys_time_as_filetime;
extern PGSTAFT pGetSystemTimePreciseAsFileTime;

void lock_thread_to_processor(HANDLE);

#endif
