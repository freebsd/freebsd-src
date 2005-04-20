/*-
 * Copyright (c) 2001 Doug Rabson
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include "libski.h"

/*
 * Ugh... Work around a bug in the Linux version of ski for SSC_GET_RTC. The
 * PSR.dt register is not preserved properly and causes further memory
 * references to be done without translation. All we need to do is preserve
 * PSR.dt across the SSC call. We do this by saving and restoring psr.l
 * completely.
 */
u_int64_t
ssc(u_int64_t in0, u_int64_t in1, u_int64_t in2, u_int64_t in3, int which)
{
	register u_int64_t psr;
	register u_int64_t ret0 __asm("r8");

	__asm __volatile("mov %0=psr;;" : "=r"(psr));
	__asm __volatile("mov r15=%1\n\t"
			 "break 0x80000;;"
			 : "=r"(ret0)
			 : "r"(which), "r"(in0), "r"(in1), "r"(in2), "r"(in3));
	__asm __volatile("mov psr.l=%0;; srlz.d" :: "r"(psr));
	return ret0;
}
