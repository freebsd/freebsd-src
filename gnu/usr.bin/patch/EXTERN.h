/* $FreeBSD: src/gnu/usr.bin/patch/EXTERN.h,v 1.6.56.1.8.1 2012/03/03 06:15:13 kensmith Exp $
 *
 * $Log: EXTERN.h,v $
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
