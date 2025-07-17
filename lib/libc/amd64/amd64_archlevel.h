/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Robert Clausecker <fuz@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
 */

/* must be macros so they can be accessed from assembly */
#define X86_64_SCALAR    0 /* disable SIMD optimisations */
#define	X86_64_BASELINE  1 /* CMOV, CX8, FPU, FXSR, MMX, OSFXSR, SSE, SSE2 */
#define	X86_64_V2        2 /* CMPXCHG16B, LAHF-SAHF, POPCNT, SSE3, SSSE3, SSE4_1, SSE4_2 */
#define	X86_64_V3        3 /* AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT, MOVBE, OSXSAVE */
#define	X86_64_V4        4 /* AVX512F, AVX512BW, AVX512CD, AVX512DQ, AVX512VL */

#define	X86_64_MAX       X86_64_V4 /* highest supported architecture level */
#define	X86_64_UNDEFINED -1 /* architecture level not set yet */

#ifndef __ASSEMBLER__
#include <dlfcn.h>

dlfunc_t	__archlevel_resolve(u_int, u_int, u_int, u_int,
		    int32_t[X86_64_MAX + 1]) __hidden;
#else
#include <machine/asm.h>

#define ARCHRESOLVE(func) \
	.globl CNAME(func); \
	.type CNAME(func), @gnu_indirect_function; \
	.set CNAME(func), __CONCAT(func,_resolver); \
	ARCHENTRY(func, resolver); \
	lea __CONCAT(func,_funcs)(%rip), %r8; \
	jmp CNAME(__archlevel_resolve); \
	ARCHEND(func, resolver)

/*
 * The func_funcs array stores the location of the implementations
 * as the distance from the func_funcs array to the function.  Due
 * to compiling for the medium code model, a 32 bit integer suffices
 * to hold the distance.
 *
 * Doing it this way both saves storage and avoids giving rtld
 * relocations to process at load time.
 */
#define ARCHFUNCS(func) \
	ARCHRESOLVE(func); \
	.section .rodata; \
	.align 4; \
	__CONCAT(func,_funcs):

#define NOARCHFUNC \
	.4byte 0

#define ARCHFUNC(func, level) \
	.4byte __CONCAT(__CONCAT(func,_),level) - __CONCAT(func,_funcs)

#define ENDARCHFUNCS(func) \
	.zero 4*(X86_64_MAX+1)-(.-__CONCAT(func,_funcs)); \
	.size __CONCAT(func,_funcs), .-__CONCAT(func,_funcs)

#define ARCHENTRY(func, level) \
	_START_ENTRY; \
	.type __CONCAT(__CONCAT(func,_),level), @function; \
	__CONCAT(__CONCAT(func,_),level):; \
	.cfi_startproc

#define ARCHEND(func, level) \
	END(__CONCAT(__CONCAT(func,_),level))

#endif  /* __ASSEMBLER__ */
