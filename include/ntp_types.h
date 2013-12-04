/*
 *  ntp_types.h - defines how int32 and u_int32 are treated.
 *  For 64 bit systems like the DEC Alpha, they have to be defined
 *  as int and u_int.
 *  For 32 bit systems, define them as long and u_long
 */
#ifndef NTP_TYPES_H
#define NTP_TYPES_H

#include <sys/types.h>
#include "ntp_machine.h"

#ifndef TRUE
# define	TRUE	1
#endif
#ifndef FALSE
# define	FALSE	0
#endif

/*
 * This is another naming conflict.
 * On NetBSD for MAC the macro "mac" is defined as 1
 * this is fun for us as a packet structure contains an
 * optional "mac" member - severe confusion results 8-)
 * As we hopefully do not have to rely on that macro we
 * just undefine that.
 */
#ifdef mac
#undef mac
#endif

/*
 * used to quiet compiler warnings
 */
#ifndef UNUSED_ARG
#define UNUSED_ARG(arg)	((void)(arg))
#endif

/*
 * COUNTOF(array) - size of array in elements
 */
#define COUNTOF(arr)	(sizeof(arr) / sizeof((arr)[0]))

/*
 * VMS DECC (v4.1), {u_char,u_short,u_long} are only in SOCKET.H,
 *			and u_int isn't defined anywhere
 */
#if defined(VMS)
#include <socket.h>
typedef unsigned int u_int;
/*
 * Note: VMS DECC has  long == int  (even on __alpha),
 *	 so the distinction below doesn't matter
 */
#endif /* VMS */

#if (SIZEOF_INT == 4)
# ifndef int32
#  define int32 int
#  ifndef INT32_MIN
#   define INT32_MIN INT_MIN
#  endif
#  ifndef INT32_MAX
#   define INT32_MAX INT_MAX
#  endif
# endif
# ifndef u_int32
#  define u_int32 unsigned int
#  ifndef U_INT32_MAX
#   define U_INT32_MAX UINT_MAX
#  endif
# endif
#else /* not sizeof(int) == 4 */
# if (SIZEOF_LONG == 4)
#  ifndef int32
#   define int32 long
#   ifndef INT32_MIN
#    define INT32_MIN LONG_MIN
#   endif
#   ifndef INT32_MAX
#    define INT32_MAX LONG_MAX
#   endif
#  endif
#  ifndef u_int32
#   define u_int32 unsigned long
#   ifndef U_INT32_MAX
#    define U_INT32_MAX ULONG_MAX
#   endif
#  endif
# else /* not sizeof(long) == 4 */
#  include "Bletch: what's 32 bits on this machine?"
# endif /* not sizeof(long) == 4 */
#endif /* not sizeof(int) == 4 */

typedef u_char		ntp_u_int8_t;
typedef u_short		ntp_u_int16_t;
typedef u_int32		ntp_u_int32_t;

typedef struct ntp_uint64_t { u_int32 val[2]; } ntp_uint64_t;

typedef unsigned short associd_t; /* association ID */
typedef u_int32 keyid_t;	/* cryptographic key ID */
typedef u_int32 tstamp_t;	/* NTP seconds timestamp */

/*
 * On Unix struct sock_timeval is equivalent to struct timeval.
 * On Windows built with 64-bit time_t, sock_timeval.tv_sec is a long
 * as required by Windows' socket() interface timeout argument, while
 * timeval.tv_sec is time_t for the more common use as a UTC time 
 * within NTP.
 */
#ifndef SYS_WINNT
#define	sock_timeval	timeval
#endif

/*
 * On Unix open() works for tty (serial) devices just fine, while on
 * Windows refclock serial devices are opened using CreateFile, a lower
 * level than the CRT-provided descriptors, because the C runtime lacks
 * tty APIs.  For refclocks which wish to use open() as well as or 
 * instead of refclock_open(), tty_open() is equivalent to open() on
 * Unix and  implemented in the Windows port similarly to
 * refclock_open().
 */
#ifndef SYS_WINNT
#define tty_open(f, a, m)	open(f, a, m)
#endif


#endif	/* NTP_TYPES_H */
