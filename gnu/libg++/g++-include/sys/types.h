#ifndef __libgxx_sys_types_h

extern "C"
{
#ifdef __sys_types_h_recursive
#include_next <sys/types.h>
#else

#define __sys_types_h_recursive
#include <stddef.h>

#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]types.h"
#else
#include_next <sys/types.h>
#endif

#define __libgxx_sys_types_h 1

#endif
}


#endif

