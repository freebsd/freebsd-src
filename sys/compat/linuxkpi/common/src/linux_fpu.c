/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * 
 * Copyright (c) 2020 Val Packett <val@packett.cool>
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <linux/compat.h>
#include <linux/sched.h>

#include <asm/fpu/api.h>

#if defined(__aarch64__) || defined(__arm__) || defined(__amd64__) ||	\
    defined(__i386__) || defined(__powerpc64__)

#include <machine/fpu.h>

/*
 * Technically the Linux API isn't supposed to allow nesting sections
 * either, but currently used versions of GPU drivers rely on nesting
 * working, so we only enter the section on the outermost level.
 */

void
lkpi_kernel_fpu_begin(void)
{
	if ((current->fpu_ctx_level)++ == 0)
		fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX);
}

void
lkpi_kernel_fpu_end(void)
{
	if (--(current->fpu_ctx_level) == 0)
		fpu_kern_leave(curthread, NULL);
}

void
lkpi_fpu_safe_exec(fpu_safe_exec_cb_t func, void *ctx)
{
	unsigned int save_fpu_level;

	save_fpu_level =
	    __current_unallocated(curthread) ? 0 : current->fpu_ctx_level;
	if (__predict_false(save_fpu_level != 0)) {
		current->fpu_ctx_level = 1;
		kernel_fpu_end();
	}
	func(ctx);
	if (__predict_false(save_fpu_level != 0)) {
		kernel_fpu_begin();
		current->fpu_ctx_level = save_fpu_level;
	}
}

#else

void
lkpi_kernel_fpu_begin(void)
{
}

void
lkpi_kernel_fpu_end(void)
{
}

void
lkpi_fpu_safe_exec(fpu_safe_exec_cb_t func, void *ctx)
{
	func(ctx);
}

#endif
