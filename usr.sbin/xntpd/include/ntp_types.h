/* ntp_types.h,v 3.1 1993/07/06 01:07:00 jbj Exp
 *  ntp_types.h - defines how LONG and U_LONG are treated.  For 64 bit systems
 *  like the DEC Alpha, they has to be defined as int and u_int.  for 32 bit
 *  systems, define them as long and u_long
 */
#include "ntp_machine.h"

#ifndef _NTP_TYPES_
#define _NTP_TYPES_

/*
 * This is another naming conflict.
 * On NetBSD for MAC the macro "mac" is defined as 1
 * this is fun for a as a paket structure contains an
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
#if defined(__STDC__) || defined(USE_PROTOTYPES)
#define	P(x)	x
#else /* __STDC__ USE_PROTOTYPES */
#define P(x)	()
#if	!defined(const)
#define	const
#endif /* const */
#endif /* __STDC__ USE_PROTOTYPES */
#endif /* P */

/*
 * DEC Alpha systems need LONG and U_LONG defined as int and u_int
 */
#ifdef __alpha
#ifndef LONG
#define LONG int
#endif /* LONG */
#ifndef U_LONG
#define U_LONG u_int
#endif /* U_LONG */
/*
 *  All other systems fall into this part
 */
#else /* __alpha */
#ifndef LONG
#define LONG long
#endif /* LONG */
#ifndef U_LONG
#define U_LONG u_long
#endif /* U_LONG */
#endif /* __ alplha */
    
#endif /* _NTP_TYPES_ */

