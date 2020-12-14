/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2018-2020 Alex Richardson <arichardson@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#if __has_include_next(<limits.h>)
#include_next <limits.h>
#endif

#if __has_include(<linux/limits.h>)
#include <linux/limits.h>
#endif

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#if !defined(_GNU_SOURCE)
#warning "Attempting to use limits.h with -std=c89/without _GNU_SOURCE, many macros will be missing"
#endif

#else /* Not C89 */
/* Not C89 -> check that all macros that we expect are defined */
#ifndef IOV_MAX
#error IOV_MAX should be defined
#endif
#endif /* C89 */

#ifndef MAXBSIZE
#define MAXBSIZE 65536 /* must be power of 2 */
#endif

#ifndef OFF_MAX
#define OFF_MAX UINT64_MAX
#endif

#ifndef QUAD_MAX
#define QUAD_MAX INT64_MAX
#endif

#ifndef GID_MAX
#define GID_MAX ((gid_t)-1)
#endif

#ifndef UID_MAX
#define UID_MAX ((uid_t)-1)
#endif

#ifdef __GLIBC__
#ifndef _LIBC_LIMITS_H_
#error "DIDN't include correct limits?"
#endif

#ifndef __USE_XOPEN
#warning __USE_XOPEN should be defined (did you forget to set _GNU_SOURCE?)
#endif

#include <sys/types.h>
#include <sys/uio.h> /* For IOV_MAX */

/* Sanity checks for glibc */
#ifndef _GNU_SOURCE
#error _GNU_SOURCE not defined
#endif

#ifndef __USE_POSIX
#warning __USE_POSIX not defined
#endif

#if defined __GNUC__ && !defined _GCC_LIMITS_H_
#error "GCC limits not included"
#endif

#ifndef __OFF_T_MATCHES_OFF64_T
#error "Expected 64-bit off_t"
#endif

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX PATH_MAX
#endif
#endif
