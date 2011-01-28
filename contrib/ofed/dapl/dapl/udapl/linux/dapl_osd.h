/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
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

/**********************************************************************
 * 
 * HEADER: dapl_osd.h
 *
 * PURPOSE: Operating System Dependent layer
 * Description:
 *	Provide OS dependent data structures & functions with
 *	a canonical DAPL interface. Designed to be portable
 *	and hide OS specific quirks of common functions.
 *
 * $Id:$
 **********************************************************************/

#ifndef _DAPL_OSD_H_
#define _DAPL_OSD_H_

/*
 * This file is defined for Linux systems only, including it on any
 * other build will cause an error
 */
#if !defined(__linux__) && !defined(__FreeBSD__)
#error UNDEFINED OS TYPE
#endif /* __linux__ || __freebsd__ */

#if !defined (__i386__) && !defined (__ia64__) && !defined(__x86_64__) && !defined(__PPC__) && !defined(__PPC64__)
#error UNDEFINED ARCH
#endif


#include <dat2/udat.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>			/* for printf */
#include <sys/time.h>
#include <syslog.h>
#include <netdb.h>			/* for getaddrinfo */
#include <infiniband/byteswap.h>

#include <sys/ioctl.h>  /* for IOCTL's */

#include "dapl_debug.h"

/*
 * Include files for setting up a network name
 */
#include <sys/socket.h> /* for socket(2) */
#include <net/if.h>     /* for struct ifreq */
#include <net/if_arp.h> /* for ARPHRD_ETHER */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include <sys/poll.h>
#include <signal.h>
#include <netinet/tcp.h>
#include <sys/utsname.h>
#include <fcntl.h>

#if !defined(SUSE_11) && !defined(REDHAT_EL5) && defined(__ia64__)
#include <asm/atomic.h>
#endif

#if defined(SUSE_11) && defined(__ia64__)
#include <asm/types.h>
#include <asm/intrinsics.h>
#endif

/* Useful debug definitions */
#ifndef STATIC
#define STATIC static
#endif /* STATIC */
#ifndef _INLINE_
#define _INLINE_ __inline__
#endif /* _INLINE_ */

#define LINUX_VERSION(a,b) (((a) << 16) + (b))

void dapl_os_init ( void );	/* initialization function */

#define dapl_os_panic(...) 			\
	do {					\
	     fprintf(stderr, "PANIC in %s:%i:%s\n", __FILE__, __LINE__, __func__); \
	     fprintf(stderr, __VA_ARGS__); 	\
             exit(1);				\
	} while(0)

#define dapl_ip_addr6(sockaddr) (((struct sockaddr_in6 *)sockaddr)->sin6_addr.s6_addr32)

/*
 * Atomic operations
 */

typedef volatile DAT_COUNT DAPL_ATOMIC;

/* atomic function prototypes */
STATIC _INLINE_ void /* DAT_COUNT */
dapl_os_atomic_inc (
	INOUT	DAPL_ATOMIC *v);

STATIC _INLINE_ void /* DAT_COUNT */
dapl_os_atomic_dec ( 
	INOUT	DAPL_ATOMIC *v);

STATIC _INLINE_ DAT_COUNT
dapl_os_atomic_assign (
    INOUT DAPL_ATOMIC *v,
    IN	DAT_COUNT match_value,
    IN	DAT_COUNT new_value );

#define dapl_os_atomic_read(v)	(*v)
#define dapl_os_atomic_set(v,i)	(*v = i)

int dapl_os_get_env_bool (
	char		*env_str );

int dapl_os_get_env_val (
	char		*env_str,
	int		def_val );



/* atomic functions */

/* dapl_os_atomic_inc
 *
 * get the current value of '*v', and then increment it.
 *
 * This is equivalent to an IB atomic fetch and add of 1,
 * except that a DAT_COUNT might be 32 bits, rather than 64
 * and it occurs in local memory.
 */

STATIC _INLINE_ void
dapl_os_atomic_inc (
	INOUT	DAPL_ATOMIC *v)
{
#ifdef __ia64__
	DAT_COUNT	old_value;
#if defined(REDHAT_EL5)
	old_value = __sync_fetch_and_add(v, 1); 
#elif !defined(REDHAT_EL4) && (OS_RELEASE >= LINUX_VERSION(2,6))
	IA64_FETCHADD(old_value,v,1,4,rel);
#else
	IA64_FETCHADD(old_value,v,1,4);
#endif
#elif defined(__PPC__) || defined(__PPC64__)
	int tmp;

    __asm__ __volatile__(
	"1:	lwarx	%0,0,%2\n\
		addic	%0,%0,1\n\
		stwcx.	%0,0,%2\n\
		bne-	1b"
	: "=&r" (tmp), "+m" (v)
	: "r" (&v)
	: "cc");
#else  /* !__ia64__ */
    __asm__ __volatile__ (
	"lock;" "incl %0"
	:"=m" (*v)
	:"m" (*v));
#endif
    return;
}


/* dapl_os_atomic_dec
 *
 * decrement the current value of '*v'. No return value is required.
 */

STATIC _INLINE_ void
dapl_os_atomic_dec ( 
	INOUT	DAPL_ATOMIC *v)
{
#ifdef __ia64__
	DAT_COUNT	old_value;
#if defined(REDHAT_EL5)
	old_value = __sync_fetch_and_sub(v, 1); 
#elif !defined(REDHAT_EL4) && (OS_RELEASE >= LINUX_VERSION(2,6))
	IA64_FETCHADD(old_value,v,-1,4,rel);
#else
	IA64_FETCHADD(old_value,v,-1,4);
#endif
#elif defined (__PPC__) || defined(__PPC64__)
	int tmp;

    __asm__ __volatile__(
	"1:	lwarx	%0,0,%2\n\
		addic	%0,%0,-1\n\
		stwcx.	%0,0,%2\n\
		bne-	1b"
	: "=&r" (tmp), "+m" (v)
	: "r" (&v)
	: "cc");
#else  /* !__ia64__ */
    __asm__ __volatile__ (
	"lock;" "decl %0"
	:"=m" (*v)
	:"m" (*v));
#endif
    return;
}


/* dapl_os_atomic_assign
 *
 * assign 'new_value' to '*v' if the current value
 * matches the provided 'match_value'.
 *
 * Make no assignment if there is no match.
 *
 * Return the current value in any case.
 *
 * This matches the IBTA atomic operation compare & swap
 * except that it is for local memory and a DAT_COUNT may
 * be only 32 bits, rather than 64.
 */

STATIC _INLINE_ DAT_COUNT
dapl_os_atomic_assign (
    INOUT DAPL_ATOMIC *v,
    IN	DAT_COUNT match_value,
    IN	DAT_COUNT new_value )
{
    DAT_COUNT	current_value;

    /*
     * Use the Pentium compare and exchange instruction
     */

#ifdef __ia64__
#if defined(REDHAT_EL5)
    current_value = __sync_val_compare_and_swap(v,match_value,new_value); 
#elif defined(REDHAT_EL4) 
    current_value = ia64_cmpxchg("acq",v,match_value,new_value,4);
#else
    current_value = ia64_cmpxchg(acq,v,match_value,new_value,4);
#endif /* __ia64__ */
#elif defined(__PPC__) || defined(__PPC64__)
        __asm__ __volatile__ (
"       lwsync\n\
1:      lwarx   %0,0,%2         # __cmpxchg_u32\n\
        cmpw    0,%0,%3\n\
        bne-    2f\n\
        stwcx.  %4,0,%2\n\
        bne-    1b\n\
        isync\n\
2:"
        : "=&r" (current_value), "=m" (*v)
        : "r" (v), "r" (match_value), "r" (new_value), "m" (*v)
        : "cc", "memory");
#else
    __asm__ __volatile__ (
        "lock; cmpxchgl %1, %2"
        : "=a" (current_value)
        : "q" (new_value), "m" (*v), "0" (match_value)
        : "memory");
#endif
    return current_value;
}

/*
 * Thread Functions
 */
typedef pthread_t		DAPL_OS_THREAD;

DAT_RETURN 
dapl_os_thread_create (
	IN  void			(*func)	(void *),
	IN  void			*data,
	OUT DAPL_OS_THREAD		*thread_id );

STATIC _INLINE_ void
dapl_os_sleep_usec(int usec)
{
	struct timespec sleep, remain;

	sleep.tv_sec = 0;
	sleep.tv_nsec = usec * 1000;
	nanosleep(&sleep, &remain);
}

/*
 * Lock Functions
 */

typedef pthread_mutex_t 	DAPL_OS_LOCK;

/* function prototypes */
STATIC _INLINE_ DAT_RETURN 
dapl_os_lock_init (
    IN	DAPL_OS_LOCK *m);

STATIC _INLINE_ DAT_RETURN 
dapl_os_lock (
    IN	DAPL_OS_LOCK *m);

STATIC _INLINE_ DAT_RETURN 
dapl_os_unlock (
    IN	DAPL_OS_LOCK *m);

STATIC _INLINE_ DAT_RETURN 
dapl_os_lock_destroy (
    IN	DAPL_OS_LOCK *m);

/* lock functions */
STATIC _INLINE_ DAT_RETURN 
dapl_os_lock_init (
    IN	DAPL_OS_LOCK *m)
{
    /* pthread_mutex_init always returns 0 */
    pthread_mutex_init (m, NULL);

    return DAT_SUCCESS;
}

STATIC _INLINE_ DAT_RETURN 
dapl_os_lock (
    IN	DAPL_OS_LOCK *m)
{
    if (0 == pthread_mutex_lock (m))
    {
	return DAT_SUCCESS;
    }
    else
    {
	return DAT_CLASS_ERROR | DAT_INTERNAL_ERROR;
    }
}

STATIC _INLINE_ DAT_RETURN 
dapl_os_unlock (
    IN	DAPL_OS_LOCK *m)
{
    if (0 == pthread_mutex_unlock (m))
    {
	return DAT_SUCCESS;
    }
    else
    {
	return DAT_CLASS_ERROR | DAT_INTERNAL_ERROR;
    }
}

STATIC _INLINE_ DAT_RETURN 
dapl_os_lock_destroy (
    IN	DAPL_OS_LOCK *m)
{
    if (0 == pthread_mutex_destroy (m))
    {
	return DAT_SUCCESS;
    }
    else
    {
	return DAT_CLASS_ERROR | DAT_INTERNAL_ERROR;
    }
}


/*
 * Wait Objects
 */

/*
 * The wait object invariant: Presuming a call to dapl_os_wait_object_wait
 * occurs at some point, there will be at least one wakeup after each call
 * to dapl_os_wait_object_signal.  I.e. Signals are not ignored, though
 * they may be coallesced.
 */

typedef struct
{
    DAT_BOOLEAN		signaled;
    pthread_cond_t	cv;
    pthread_mutex_t	lock;
} DAPL_OS_WAIT_OBJECT;

/* function prototypes */
DAT_RETURN 
dapl_os_wait_object_init (
    IN DAPL_OS_WAIT_OBJECT *wait_obj);

DAT_RETURN 
dapl_os_wait_object_wait (
    IN	DAPL_OS_WAIT_OBJECT *wait_obj, 
    IN  DAT_TIMEOUT timeout_val);

DAT_RETURN 
dapl_os_wait_object_wakeup (
    IN	DAPL_OS_WAIT_OBJECT *wait_obj);

DAT_RETURN 
dapl_os_wait_object_destroy (
    IN	DAPL_OS_WAIT_OBJECT *wait_obj);

/*
 * Memory Functions
 */

/* function prototypes */
STATIC _INLINE_ void *dapl_os_alloc (int size);

STATIC _INLINE_ void *dapl_os_realloc (void *ptr, int size);

STATIC _INLINE_ void dapl_os_free (void *ptr, int size);

STATIC _INLINE_ void * dapl_os_memzero (void *loc, int size);

STATIC _INLINE_ void * dapl_os_memcpy (void *dest, const void *src, int len);

STATIC _INLINE_ int dapl_os_memcmp (const void *mem1, const void *mem2, int len);

/* memory functions */


STATIC _INLINE_ void *dapl_os_alloc (int size)
{
    return malloc (size);
}

STATIC _INLINE_ void *dapl_os_realloc (void *ptr, int size)
{
    return realloc(ptr, size);
}

STATIC _INLINE_ void dapl_os_free (void *ptr, int size)
{
    free (ptr);
}

STATIC _INLINE_ void * dapl_os_memzero (void *loc, int size)
{
    return memset (loc, 0, size);
}

STATIC _INLINE_ void * dapl_os_memcpy (void *dest, const void *src, int len)
{
    return memcpy (dest, src, len);
}

STATIC _INLINE_ int dapl_os_memcmp (const void *mem1, const void *mem2, int len)
{
    return memcmp (mem1, mem2, len);
}

/*
 * Memory coherency functions
 * For i386 Linux, there are no coherency issues so we just return success.
 */
STATIC _INLINE_  DAT_RETURN
dapl_os_sync_rdma_read (
    IN      const DAT_LMR_TRIPLET	*local_segments,
    IN      DAT_VLEN			num_segments)
{
    return DAT_SUCCESS;
}

STATIC _INLINE_  DAT_RETURN
dapl_os_sync_rdma_write (
    IN      const DAT_LMR_TRIPLET	*local_segments,
    IN      DAT_VLEN			num_segments)
{
    return DAT_SUCCESS;
}


/*
 * String Functions
 */

STATIC _INLINE_ unsigned int dapl_os_strlen(const char *str)
{
    return strlen(str);
}

STATIC _INLINE_ char * dapl_os_strdup(const char *str)
{
    return strdup(str);
}


/*
 * Timer Functions
 */

typedef DAT_UINT64		   DAPL_OS_TIMEVAL;
typedef struct dapl_timer_entry	   DAPL_OS_TIMER;
typedef unsigned long long int	   DAPL_OS_TICKS;

/* function prototypes */
DAT_RETURN dapl_os_get_time (DAPL_OS_TIMEVAL *);


/*
 *
 * Name Service Helper functions
 *
 */
#if defined(IBHOSTS_NAMING) || defined(CM_BUSTED)
#define dapls_osd_getaddrinfo(name, addr_ptr) getaddrinfo(name,NULL,NULL,addr_ptr)
#define dapls_osd_freeaddrinfo(addr) freeaddrinfo (addr)

#endif /* IBHOSTS_NAMING */

/*
 * *printf format helpers. We use the C string constant concatenation
 * ability to define 64 bit formats, which unfortunatly are non standard
 * in the C compiler world. E.g. %llx for gcc, %I64x for Windows
 */
#include <inttypes.h>
#define F64d   "%"PRId64
#define F64u   "%"PRIu64
#define F64x   "%"PRIx64
#define F64X   "%"PRIX64


/*
 *  Conversion Functions
 */

STATIC _INLINE_ long int
dapl_os_strtol(const char *nptr, char **endptr, int base)
{
    return strtol(nptr, endptr, base);
}


/*
 *  Helper Functions
 */


#define dapl_os_assert(expression)	assert(expression)
#define dapl_os_printf(...) 		printf(__VA_ARGS__)
#define dapl_os_vprintf(fmt,args)	vprintf(fmt,args)
#define dapl_os_syslog(fmt,args)	vsyslog(LOG_USER|LOG_WARNING,fmt,args)

#define dapl_os_getpid (DAT_UINT32)getpid 
#define dapl_os_gettid (DAT_UINT32)pthread_self 

#endif /*  _DAPL_OSD_H_ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
