#ifndef __libgxx_sys_file_h

extern "C"
{
#ifdef __sys_file_h_recursive
#include_next <sys/file.h>
#else
#define fcntl __hide_fcntl
#define open  __hide_open
#define creat __hide_creat

#define __sys_file_h_recursive

#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]file.h"
#else
#include_next <sys/file.h>
#endif

#undef fcntl
#undef open
#undef creat

#define __libgxx_sys_file_h 1

#endif
}
#endif
