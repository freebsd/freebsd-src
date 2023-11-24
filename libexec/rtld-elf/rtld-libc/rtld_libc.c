/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2019 Alex Richardson <arichardson@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
 */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rtld.h"
#include "rtld_printf.h"
#include "rtld_libc.h"

/*
 * Avoid dependencies from various libc calls on abort(). Since this is only
 * used for assertions in RTLD, we can just raise SIGABRT directly.
 */
void
abort(void)
{
	raise(SIGABRT);
	__builtin_trap();
}

static int rtld_errno;
int *__error(void);
int *
__error(void)
{

	return (&rtld_errno);
}

/* Avoid dependency on __libc_interposing, use the system call directly. */
#undef sigprocmask
int
sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{

	return (__sys_sigprocmask(how, set, oset));
}
__strong_reference(sigprocmask, __libc_sigprocmask);

#if defined DEBUG || !defined(NDEBUG)
/* Provide an implementation of __assert that does not pull in fprintf() */
void
__assert(const char *func, const char *file, int line, const char *failedexpr)
{

	if (func == NULL)
		(void)rtld_fdprintf(STDERR_FILENO,
		    "Assertion failed: (%s), file %s, line %d.\n", failedexpr,
		    file, line);
	else
		(void)rtld_fdprintf(STDERR_FILENO,
		    "Assertion failed: (%s), function %s, file %s, line %d.\n",
		    failedexpr, func, file, line);
	abort();
	/* NOTREACHED */
}
#endif

/*
 * Avoid pulling in all of pthreads from getpagesize().
 * It normally uses libc/gen/auxv.c which pulls in pthread_once().
 * This relies on init_pagesizes setting page_size so must not be called
 * before that.
 */
int
getpagesize(void)
{
	return (page_size);
}

extern int __sys___sysctl(const int *name, u_int namelen, void *oldp,
    size_t *oldlenp, const void *newp, size_t newlen);

int
sysctl(const int *name, u_int namelen, void *oldp, size_t *oldlenp,
    const void *newp, size_t newlen)
{
	int retval;

	assert(name[0] != CTL_USER && "Not supported inside rtld!");
	retval = __sys___sysctl(name, namelen, oldp, oldlenp, newp, newlen);
	return (retval);
}
