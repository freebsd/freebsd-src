/* $FreeBSD: src/gnu/usr.bin/patch/EXTERN.h,v 1.6.50.1 2008/10/02 02:57:24 kensmith Exp $
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
