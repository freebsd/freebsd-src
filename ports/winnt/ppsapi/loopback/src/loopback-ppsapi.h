//
// serialpps-ppsapi-provider.h
//
// For this tiny project the single header serves as a precompiled header
// as well, meaning all the bulky headers are included before or within it.
// Within, in this case.
//

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <windows.h>
typedef          __int32 int32;
typedef unsigned __int32 u_int32;
typedef          __int64 int64;
typedef unsigned __int64 u_int64;
#include "timepps.h"

#ifndef UNUSED
#define UNUSED(item)	((void)(item))
#endif

/* PPS data structure as captured by the serial line I/O system. This
 * must match the local definition in 'ntp_iocompletionport.c' or
 * 'Bad Things (tm)' are bound to happen.
 */
struct PpsData {
	u_long		cc_assert;
	u_long		cc_clear;
	ntp_fp_t	ts_assert;
	ntp_fp_t	ts_clear;
};
typedef struct PpsData PPSData_t;

/* prototypes imported from the NTPD executable */
__declspec(dllimport) HANDLE WINAPI ntp_pps_attach_device(HANDLE hndIo);
__declspec(dllimport) void   WINAPI ntp_pps_detach_device(HANDLE ppsHandle);
__declspec(dllimport) BOOL   WINAPI ntp_pps_read(HANDLE ppsHandle, void*, size_t);

/* prototypes exported to the NTPD executable */
__declspec(dllexport) int WINAPI prov_time_pps_create(HANDLE, pps_handle_t*);
__declspec(dllexport) int WINAPI prov_time_pps_destroy(pps_unit_t*, void*);
__declspec(dllexport) int WINAPI prov_time_pps_setparams(pps_unit_t*, void*,
					const pps_params_t*);
__declspec(dllexport) int WINAPI prov_time_pps_fetch(pps_unit_t*, void*,
					const int, pps_info_t*, const struct timespec*);
__declspec(dllexport) int WINAPI prov_time_pps_kcbind(pps_unit_t*, void*, const int, const int, const int);
__declspec(dllexport) int WINAPI ppsapi_prov_init(int, pcreate_pps_handle,
					ppps_ntp_timestamp_from_counter, char*, size_t,
					char*, size_t);
