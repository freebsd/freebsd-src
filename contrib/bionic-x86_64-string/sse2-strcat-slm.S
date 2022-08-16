/*
Copyright (c) 2014, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    * this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
    * this list of conditions and the following disclaimer in the documentation
    * and/or other materials provided with the distribution.

    * Neither the name of Intel Corporation nor the names of its contributors
    * may be used to endorse or promote products derived from this software
    * without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef STRCAT
# define STRCAT		strcat
#endif

#ifndef L
# define L(label)		.L##label
#endif

#ifndef cfi_startproc
# define cfi_startproc		 .cfi_startproc
#endif

#ifndef cfi_endproc
# define cfi_endproc		.cfi_endproc
#endif

#ifndef ENTRY
# define ENTRY(name)		\
	.type name,  @function;		\
	.globl name;		\
	.p2align 4;		\
name:		\
	cfi_startproc
#endif

#ifndef END
# define END(name)		\
	cfi_endproc;		\
	.size name, .-name
#endif

#define USE_AS_STRCAT

.text
ENTRY (STRCAT)
	mov	%rdi, %r9
#ifdef USE_AS_STRNCAT
	mov	%rdx, %r8
#endif

#define RETURN jmp L(Strcpy)
#include "sse2-strlen-slm.S"

#undef RETURN
#define RETURN ret

L(Strcpy):
	lea	(%r9, %rax), %rdi
	mov	%rsi, %rcx
	mov	%r9, %rax	/* save result */

#ifdef USE_AS_STRNCAT
	test	%r8, %r8
	jz	L(ExitZero)
# define USE_AS_STRNCPY
#endif
#include "sse2-strcpy-slm.S"
