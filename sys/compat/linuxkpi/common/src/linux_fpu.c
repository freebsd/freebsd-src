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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <machine/fpu.h>

struct fpu_kern_ctx *__lkpi_fpu_ctx;
unsigned int __lkpi_fpu_ctx_level = 0;

static void
linux_fpu_init(void *arg __unused)
{
	__lkpi_fpu_ctx = fpu_kern_alloc_ctx(0);
}
SYSINIT(linux_fpu, SI_SUB_EVENTHANDLER, SI_ORDER_SECOND, linux_fpu_init, NULL);

static void
linux_fpu_uninit(void *arg __unused)
{
	fpu_kern_free_ctx(__lkpi_fpu_ctx);
}
SYSUNINIT(linux_fpu, SI_SUB_EVENTHANDLER, SI_ORDER_SECOND, linux_fpu_uninit, NULL);
