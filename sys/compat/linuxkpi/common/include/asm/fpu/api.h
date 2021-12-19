/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 * 
 * Copyright (c) 2020 Greg V <greg@unrelenting.technology>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_LINUXKPI_ASM_FPU_API_H_
#define	_LINUXKPI_ASM_FPU_API_H_

#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)

#include <machine/fpu.h>

extern struct fpu_kern_ctx *__lkpi_fpu_ctx;
extern unsigned int __lkpi_fpu_ctx_level;

static inline void
kernel_fpu_begin()
{
	if (__lkpi_fpu_ctx_level++ == 0) {
		fpu_kern_enter(curthread, __lkpi_fpu_ctx, FPU_KERN_NORMAL);
	}
}

static inline void
kernel_fpu_end()
{
	if (--__lkpi_fpu_ctx_level == 0) {
		fpu_kern_leave(curthread, __lkpi_fpu_ctx);
	}
}

#else

static inline void
kernel_fpu_begin()
{
}

static inline void
kernel_fpu_end()
{
}

#endif

#endif /* _LINUXKPI_ASM_FPU_API_H_ */
