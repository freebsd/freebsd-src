#ifndef __libgxx_sys_times_h

extern "C"
{
#ifdef __sys_times_h_recursive
#include_next <sys/times.h>
#else
#define __sys_times_h_recursive
#include_next <sys/times.h>
#define __libgxx_sys_times_h 1

#include <_G_config.h>

extern _G_clock_t times _G_ARGS((struct tms*));

#endif
}


#endif
