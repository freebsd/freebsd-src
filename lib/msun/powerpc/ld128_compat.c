/*
 * On powerpc64le, long double is IEEE binary128, which the compiler treats as
 * _Float128.  clang lowers long double libm calls to the f128-suffixed entry
 * points (sinf128, powf128, ...) rather than the *l names.  Provide those entry
 * points as thin forwarders to the existing long double implementations.
 *
 * This file is compiled with -fno-builtin so the calls below are not themselves
 * rewritten back to the f128 names, which would recurse infinitely.
 */
#include <math.h>

long double acosf128(long double x) { return acosl(x); }
long double asinf128(long double x) { return asinl(x); }
long double atanf128(long double x) { return atanl(x); }
long double atan2f128(long double y, long double x) { return atan2l(y, x); }
long double ceilf128(long double x) { return ceill(x); }
long double cosf128(long double x) { return cosl(x); }
long double coshf128(long double x) { return coshl(x); }
long double expf128(long double x) { return expl(x); }
long double exp2f128(long double x) { return exp2l(x); }
long double floorf128(long double x) { return floorl(x); }
long double fmaf128(long double x, long double y, long double z) { return fmal(x, y, z); }
long double fmaxf128(long double x, long double y) { return fmaxl(x, y); }
long double fminf128(long double x, long double y) { return fminl(x, y); }
long double fmodf128(long double x, long double y) { return fmodl(x, y); }
long double ldexpf128(long double x, int n) { return ldexpl(x, n); }
long long llroundf128(long double x) { return llroundl(x); }
long double logf128(long double x) { return logl(x); }
long double log10f128(long double x) { return log10l(x); }
long double log2f128(long double x) { return log2l(x); }
long lroundf128(long double x) { return lroundl(x); }
long double modff128(long double x, long double *iptr) { return modfl(x, iptr); }
long double nearbyintf128(long double x) { return nearbyintl(x); }
long double powf128(long double x, long double y) { return powl(x, y); }
long double rintf128(long double x) { return rintl(x); }
long double roundf128(long double x) { return roundl(x); }
void sincosf128(long double x, long double *s, long double *c) { sincosl(x, s, c); }
long double sinf128(long double x) { return sinl(x); }
long double sinhf128(long double x) { return sinhl(x); }
long double sqrtf128(long double x) { return sqrtl(x); }
long double tanf128(long double x) { return tanl(x); }
long double tanhf128(long double x) { return tanhl(x); }
long double truncf128(long double x) { return truncl(x); }
