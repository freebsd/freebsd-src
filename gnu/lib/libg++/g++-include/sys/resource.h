#ifndef __libgxx_sys_resource_h

extern "C"
{
#ifdef __sys_resource_h_recursive
#include_next <sys/resource.h>
#else
#include <_G_config.h>
#define __sys_resource_h_recursive
#include <sys/time.h>

#ifdef VMS
#include "GNU_CC_INCLUDE:[sys]resource.h"
#else
#include_next <sys/resource.h>
#endif

#define __libgxx_sys_resource_h 1

int getrusage(int, struct rusage*);
int getrlimit (int resource, struct rlimit *rlp);
int setrlimit _G_ARGS((int resource, const struct rlimit *rlp));
long      ulimit(int, long);
int       getpriority(int, int);
int       setpriority(int, int, int);
#endif
}

#endif 
