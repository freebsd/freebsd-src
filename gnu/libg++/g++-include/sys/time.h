#ifndef __libgxx_sys_time_h

extern "C"
{
#ifdef __sys_time_h_recursive
#include_next <sys/time.h>
#else
#define __sys_time_h_recursive

// Some system (e.g Ultrix) define these in sys/time.h.

#ifndef __time_h_recursive
#define time __hide_time
#define clock __hide_clock
#define gmtime __hide_gmtime
#define localtime __hide_localtime
#define ctime __hide_ctime
#define asctime __hide_asctime
#define strftime __hide_strftime
#endif

#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]resourcetime.h"
#else
#include_next <sys/time.h>
#endif
#undef __sys_time_h_recursive

#define __libgxx_sys_time_h 1

#undef clock
#undef ctime
#undef gmtime
#undef localtime
#undef time
#undef asctime
#undef strftime 
#endif
}

#endif 
