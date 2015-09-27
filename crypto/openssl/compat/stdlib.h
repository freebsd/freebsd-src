/*
 * stdlib.h compatibility shim
 * Public domain
 */

#ifdef _MSC_VER
#include <../include/stdlib.h>
#else
#include_next <stdlib.h>
#endif

#ifndef LIBCRYPTOCOMPAT_STDLIB_H
#define LIBCRYPTOCOMPAT_STDLIB_H

#include <sys/types.h>
#include <stdint.h>

#ifndef HAVE_ARC4RANDOM_BUF
uint32_t arc4random(void);
void arc4random_buf(void *_buf, size_t n);
#endif

#ifndef HAVE_REALLOCARRAY
void *reallocarray(void *, size_t, size_t);
#endif

#ifndef HAVE_STRTONUM
long long strtonum(const char *nptr, long long minval,
		long long maxval, const char **errstr);
#endif

#endif
