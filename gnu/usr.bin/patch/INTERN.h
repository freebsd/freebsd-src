/* $FreeBSD: src/gnu/usr.bin/patch/INTERN.h,v 1.6.56.1.4.1 2010/06/14 02:09:06 kensmith Exp $
 *
 * $Log: INTERN.h,v $
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
