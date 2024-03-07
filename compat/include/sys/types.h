/*
 * Public domain
 * sys/types.h compatibility shim
 */

#include_next <sys/types.h>

#ifndef DIFFCOMPAT_SYS_TYPES_H
#define DIFFCOMPAT_SYS_TYPES_H

#if !defined(__dead)
#define __dead      __attribute__((__noreturn__))
#endif

#endif
