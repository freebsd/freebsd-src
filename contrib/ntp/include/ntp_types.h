/*
 *  ntp_types.h - defines how int32 and u_int32 are treated.
 *  For 64 bit systems like the DEC Alpha, they have to be defined
 *  as int and u_int.
 *  For 32 bit systems, define them as long and u_long
 */
#include "ntp_machine.h"

#ifndef _NTP_TYPES_
#define _NTP_TYPES_

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
 * Set up for prototyping
 */
#ifndef P
#if defined(__STDC__) || defined(HAVE_PROTOTYPES)
#define	P(x)	x
#else /* not __STDC__ and not HAVE_PROTOTYPES */
#define P(x)	()
#endif /* not __STDC__ and HAVE_PROTOTYPES */
#endif /* P */

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
# endif
# ifndef u_int32
#  define u_int32 unsigned int
# endif
#else /* not sizeof(int) == 4 */
# if (SIZEOF_LONG == 4)
# else /* not sizeof(long) == 4 */
#  ifndef int32
#   define int32 long
#  endif
#  ifndef u_int32
#   define u_int32 unsigned long
#  endif
# endif /* not sizeof(long) == 4 */
# include "Bletch: what's 32 bits on this machine?"
#endif /* not sizeof(int) == 4 */

#endif /* _NTP_TYPES_ */

