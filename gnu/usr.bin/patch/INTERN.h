/* $FreeBSD$
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
