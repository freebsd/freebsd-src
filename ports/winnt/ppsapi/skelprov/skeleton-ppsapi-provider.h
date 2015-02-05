//
// serialpps-ppsapi-provider.h
//
// For this tiny project the single header serves as a precompiled header
// as well, meaning all the bulky headers are included before or within it.
// Within, in this case.
//

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
/* Prevent inclusion of winsock.h in windows.h */
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_  
#endif
#include <windows.h>
typedef DWORD u_int32;
#include "sys/time.h"
#include "../../include/timepps.h"

#ifndef UNUSED
#define UNUSED(item)	((void)(item))
#endif
