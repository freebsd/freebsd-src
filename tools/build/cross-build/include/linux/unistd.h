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
#pragma once

#ifndef __USE_POSIX2
/* Ensure that unistd.h pulls in getopt */
#define __USE_POSIX2
#endif
/*
 * Before version 2.25, glibc's unistd.h would define the POSIX subset of
 * getopt.h by defining __need_getopt,  including getopt.h (which would
 * disable the header guard) and then undefining it so later including
 * getopt.h explicitly would define the extensions. However, we wrap getopt,
 * and so the wrapper's #pragma once breaks that. Thus getopt.h must be
 * included before the real unistd.h to ensure we get all the extensions.
 */
#include <getopt.h>
#include_next <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#ifndef required_argument
#error "something went wrong including getopt"
#endif

__BEGIN_DECLS

#ifdef __GLIBC__
static inline int
issetugid(void)
{
	return (0);
}
#endif

char	*fflagstostr(unsigned long flags);
int	strtofflags(char **stringp, u_long *setp, u_long *clrp);

/*
 * getentropy() was added in glibc 2.25. Declare it for !glibc and older
 * versions.
 */
#if defined(__GLIBC__) && !__GLIBC_PREREQ(2, 25)
static inline int
getentropy(void *buf, size_t buflen)
{
	return (syscall(__NR_getrandom, buf, buflen, 0));
}
#endif

void *setmode(const char *);
mode_t getmode(const void *, mode_t);

__END_DECLS
