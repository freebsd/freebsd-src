/* $FreeBSD$
 *
 * $Log: INTERN.h,v $
 * Revision 1.4  1998/01/21 14:37:12  ache
 * Resurrect patch 2.1 without FreeBSD Index: hack
 *
 * Revision 1.2  1995/05/30 05:02:27  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1993/06/19  14:21:52  paul
 * b-maked patch-2.10
 *
 * Revision 2.0  86/09/17  15:35:58  lwall
 * Baseline for netwide release.
 *
 */

#ifdef EXT
#undef EXT
#endif
#define EXT

#ifdef INIT
#undef INIT
#endif
#define INIT(x) = x

#define DOINIT
