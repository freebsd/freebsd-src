/* $Header: /home/ncvs/src/gnu/usr.bin/patch/INTERN.h,v 1.2 1995/05/30 05:02:27 rgrimes Exp $
 *
 * $Log: INTERN.h,v $
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
