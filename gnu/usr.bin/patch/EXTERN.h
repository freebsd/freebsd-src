/* $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/patch/EXTERN.h,v 1.2 1995/05/30 05:02:26 rgrimes Exp $
 *
 * $Log: EXTERN.h,v $
 * Revision 1.2  1995/05/30  05:02:26  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1993/06/19  14:21:52  paul
 * b-maked patch-2.10
 *
 * Revision 2.0  86/09/17  15:35:37  lwall
 * Baseline for netwide release.
 *
 */

#ifdef EXT
#undef EXT
#endif
#define EXT extern

#ifdef INIT
#undef INIT
#endif
#define INIT(x)

#ifdef DOINIT
#undef DOINIT
#endif
