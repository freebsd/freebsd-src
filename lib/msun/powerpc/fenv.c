/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
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

#define	__fenv_static
#include "fenv.h"
#ifdef __SPE__
#include <sys/types.h>
#include <machine/spr.h>
#endif

#ifdef __GNUC_GNU_INLINE__
#error "This file must be compiled with C99 'inline' semantics"
#endif

#ifdef __SPE__
const fenv_t __fe_dfl_env = SPEFSCR_DFLT;
#else
const fenv_t __fe_dfl_env = 0x00000000;
#endif

extern inline int feclearexcept(int __excepts);
extern inline int fegetexceptflag(fexcept_t *__flagp, int __excepts);
extern inline int fesetexceptflag(const fexcept_t *__flagp, int __excepts);
#ifndef __SPE__
extern inline int feraiseexcept(int __excepts);
#endif
extern inline int fetestexcept(int __excepts);
extern inline int fegetround(void);
extern inline int fesetround(int __round);
extern inline int fegetenv(fenv_t *__envp);
extern inline int feholdexcept(fenv_t *__envp);
extern inline int fesetenv(const fenv_t *__envp);
extern inline int feupdateenv(const fenv_t *__envp);
extern inline int feenableexcept(int __mask);
extern inline int fedisableexcept(int __mask);

#ifdef __SPE__
#define	PMAX	0x7f7fffff
#define	PMIN	0x00800000
int	feraiseexcept(int __excepts)
{
	uint32_t spefscr;

	spefscr = mfspr(SPR_SPEFSCR);
	mtspr(SPR_SPEFSCR, spefscr | (__excepts & FE_ALL_EXCEPT));

	if (__excepts & FE_INVALID)
		__asm __volatile ("efsdiv %0, %0, %1" :: "r"(0), "r"(0));
	if (__excepts & FE_DIVBYZERO)
		__asm __volatile ("efsdiv %0, %0, %1" :: "r"(1.0f), "r"(0));
	if (__excepts & FE_UNDERFLOW)
		__asm __volatile ("efsmul %0, %0, %0" :: "r"(PMIN));
	if (__excepts & FE_OVERFLOW)
		__asm __volatile ("efsadd %0, %0, %0" :: "r"(PMAX));
	if (__excepts & FE_INEXACT)
		__asm __volatile ("efssub %0, %0, %1" :: "r"(PMIN), "r"(1.0f));
	return (0);
}
#endif
