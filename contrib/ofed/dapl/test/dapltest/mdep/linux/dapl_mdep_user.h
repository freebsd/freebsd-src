/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#ifndef __DAPL_MDEP_USER_H__
#define __DAPL_MDEP_USER_H__

/* include files */

#include <ctype.h>
#include <stdlib.h>
#include <inttypes.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/times.h>
#include <sys/time.h>

/* inet_ntoa */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Default Device Name */
#define DT_MdepDeviceName    "ofa-v2-ib0"

/* Boolean */
typedef int     bool;

#define true (1)
#define false (0)

#ifndef __BASE_FILE__
#define __BASE_FILE__ __FILE__
#endif

#ifndef _INLINE_
#define _INLINE_  __inline__
#endif

/*
 * Locks
 */

typedef pthread_mutex_t DT_Mdep_LockType;

/* Wait object used for inter thread communication */

typedef struct
{
    bool		signaled;
    pthread_cond_t	cv;
    pthread_mutex_t	lock;
} DT_WAIT_OBJECT;

/*
 * Thread types
 */
typedef pthread_t   DT_Mdep_ThreadHandleType;
typedef void      (*DT_Mdep_ThreadFunction) (void *param);
typedef void *      DT_Mdep_Thread_Start_Routine_Return_Type;
#define DT_MDEP_DEFAULT_STACK_SIZE 65536

typedef struct
{
    void			(*function) (void *);
    void			*param;
    DT_Mdep_ThreadHandleType	thread_handle;
    unsigned int    		stacksize;
    pthread_attr_t		attr;	    /* Thread attributes */
} Thread;

/*
 * System information
 *
 */

typedef struct
{
    unsigned long int		system;
    unsigned long int		user;
    unsigned long int		idle;
} DT_CpuStat;

/*
 * Timing
 */
#ifdef RDTSC_TIMERS
typedef unsigned long long int 		DT_Mdep_TimeStamp;

static _INLINE_ DT_Mdep_TimeStamp
DT_Mdep_GetTimeStamp ( void )
{
#if defined(__GNUC__) && defined(__i386__)
    DT_Mdep_TimeStamp x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
#else
#ifdef __ia64__
    unsigned long ret;
    __asm__ __volatile__ ("mov %0=ar.itc" : "=r"(ret));
    return ret;
#else
#if defined(__PPC__) || defined(__PPC64__)
    unsigned int tbl, tbu0, tbu1;
    do {
         __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
         __asm__ __volatile__ ("mftb %0" : "=r"(tbl));
         __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
    } while (tbu0 != tbu1);
    return (((unsigned long long)tbu0) << 32) | tbl;
#else
#if defined(__x86_64__)
      unsigned int __a,__d; 
      asm volatile("rdtsc" : "=a" (__a), "=d" (__d)); 
      return ((unsigned long)__a) | (((unsigned long)__d)<<32);
#else
#error "Linux CPU architecture - unimplemented"
#endif
#endif
#endif
#endif
}
#else /* !RDTSC_TIMERS */
/* 
 * Get timestamp, microseconds, (relative to some fixed point)
 */
typedef double DT_Mdep_TimeStamp;

static _INLINE_ DT_Mdep_TimeStamp
DT_Mdep_GetTimeStamp ( void )
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000 + tv.tv_usec);
}
#endif


/*
 * Define long format types to be used in *printf format strings.  We
 * use the C string constant concatenation ability to define 64 bit
 * formats, which unfortunatly are non standard in the C compiler
 * world. E.g. %llx for gcc, %I64x for Windows
 */

#if defined(__x86_64__) || defined(__ia64__)
#define F64d   "%ld"
#define F64u   "%lu"
#define F64x   "%lx"
#define F64X   "%lX"
#else
#define F64d   "%lld"
#define F64u   "%llu"
#define F64x   "%llx"
#define F64X   "%llX"
#endif
/*
 * Define notion of a LONG LONG 0
 */
#define LZERO 0ULL

/* Mdep function defines */
#define DT_Mdep_Debug(N, _X_) \
do { \
      if (DT_dapltest_debug >= (N)) \
        { \
          DT_Mdep_printf _X_; \
        } \
} while (0)
#define DT_Mdep_printf printf

#define DT_Mdep_flush() fflush(NULL)

/*
 * Release processor to reschedule
 */
#define DT_Mdep_yield pthread_yield

#endif
