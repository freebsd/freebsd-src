/*
 * getclock.c - Emulate Unix getclock(3) nanosecond interface for libntp/ntpd
 */
#include "config.h"

#include "ntp_unixtime.h"
#include "clockstuff.h"
#include "ntp_stdlib.h"

/*
 * getclock() is in libntp.  To use interpolation, 
 * ports/winnt/ntpd/nt_clockstuff.c overrides GetSystemTimeAsFileTime 
 * via the pointer get_sys_time_as_filetime.
 */
PGSTAFT get_sys_time_as_filetime;
PGSTAFT pGetSystemTimePreciseAsFileTime;


int
getclock(
	int		clktyp,
	struct timespec *ts
	)
{
	union {
		FILETIME ft;
		ULONGLONG ull;
	} uNow;

	if (clktyp != TIMEOFDAY) {
		TRACE(1, ("getclock() supports only TIMEOFDAY clktyp\n"));
		errno = EINVAL;
		return -1;
	}

	if (NULL == get_sys_time_as_filetime)
		init_win_precise_time();
	(*get_sys_time_as_filetime)(&uNow.ft);

	/* 
	 * Convert the hecto-nano second time to timespec format
	 */
	uNow.ull -= FILETIME_1970;
	ts->tv_sec = (time_t)( uNow.ull / HECTONANOSECONDS);
	ts->tv_nsec = (long)(( uNow.ull % HECTONANOSECONDS) * 100);

	return 0;
}


void
init_win_precise_time(void)
{
	HANDLE	hDll;
	FARPROC	pfn;

	hDll = LoadLibrary("kernel32");
	pfn = GetProcAddress(hDll, "GetSystemTimePreciseAsFileTime");
	if (NULL != pfn) {
		pGetSystemTimePreciseAsFileTime = (PGSTAFT)pfn;
		get_sys_time_as_filetime = pGetSystemTimePreciseAsFileTime;
	} else {
		get_sys_time_as_filetime = &GetSystemTimeAsFileTime;
	}
}
