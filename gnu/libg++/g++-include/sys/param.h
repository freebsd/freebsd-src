#ifndef __libgxx_sys_param_h

extern "C"
{
#ifdef __sys_param_h_recursive
#include_next <sys/param.h>
#else
#define __sys_param_h_recursive

#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]param.h"
#else
#include_next <sys/param.h>
#endif

#undef setbit /* Conflicts with Integer::setbit(). */

#define __libgxx_sys_param_h 1
#endif
}

#endif
