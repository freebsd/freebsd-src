/*	$NetBSD: ieeefp.h,v 1.4 1998/01/09 08:03:43 perry Exp $	*/

/* 
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#ifndef _IEEEFP_H_
#define _IEEEFP_H_

#include <sys/cdefs.h>
#include <machine/ieeefp.h>

#ifdef __i386__
#include <machine/floatingpoint.h>
#else /* !__i386__ */
__BEGIN_DECLS
extern fp_rnd    fpgetround __P((void));
extern fp_rnd    fpsetround __P((fp_rnd));
extern fp_except fpgetmask __P((void));
extern fp_except fpsetmask __P((fp_except));
extern fp_except fpgetsticky __P((void));
extern fp_except fpsetsticky __P((fp_except));
__END_DECLS
#endif /* __i386__ */

#endif /* _IEEEFP_H_ */
