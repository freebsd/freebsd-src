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

#ifdef LKPI_HAVE_FPU_CTX

#include <machine/fpu.h>

/*
 * Technically the Linux API isn't supposed to allow nesting sections
 * either, but currently used versions of GPU drivers rely on nesting
 * working, so we only enter the section on the outermost level.
 */

void
lkpi_kernel_fpu_begin(void)
{
	int err;

	if ((current->fpu_ctx_level)++ == 0) {
		err = linux_set_fpu_ctx(current);
		fpu_kern_enter(curthread, current->fpu_ctx,
		    err == 0 ? FPU_KERN_KTHR : FPU_KERN_NOCTX);
	}
}

void
lkpi_kernel_fpu_end(void)
{
	if (--(current->fpu_ctx_level) == 0)
		fpu_kern_leave(curthread, current->fpu_ctx);
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

#endif
