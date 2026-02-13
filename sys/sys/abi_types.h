/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 */

#ifndef _ABI_TYPES_H_
#define	_ABI_TYPES_H_

#include <sys/_types.h>

/*
 * i386 is the only arch with a 32-bit time_t.
 * Also it is the only arch with (u)int64_t having 4-bytes alignment.
 */
typedef struct {
#ifdef __amd64__
	__uint32_t val[2];
#else
	__uint64_t val;
#endif
} freebsd32_uint64_t;

#if defined(__amd64__) || defined(__i386__)
typedef	__int32_t	time32_t;
#else
typedef	__int64_t	time32_t;
#endif
#define	__HAVE_TIME32_T

#endif
