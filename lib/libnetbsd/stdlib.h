/* $FreeBSD$ */

#ifndef _LIBNETBSD_STDLIB_H_
#define _LIBNETBSD_STDLIB_H_

#include_next <stdlib.h>

long long strsuftoll(const char *, const char *, long long, long long);
long long strsuftollx(const char *, const char *,
    long long, long long, char *, size_t);

#endif /* _LIBNETBSD_STDLIB_H_ */
