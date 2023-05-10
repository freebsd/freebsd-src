/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Daniel Eischen <deischen@freebsd.org>.
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>.
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
 *
 * $FreeBSD$
 */

/*
 * Machine-dependent thread prototypes/definitions.
 */
#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#include <stddef.h>
#include <sys/types.h>
#include <machine/tls.h>

#define	CPU_SPINWAIT		__asm __volatile("pause")

/* For use in _Static_assert to check structs will fit in a page */
#define	THR_PAGE_SIZE_MIN	PAGE_SIZE

static __inline struct pthread *
_get_curthread(void)
{
	struct pthread *thr;

	__asm __volatile("movl %%gs:%1, %0" : "=r" (thr)
	    : "m" (*(volatile u_int *)offsetof(struct tcb, tcb_thread)));
	return (thr);
}

#define HAS__UMTX_OP_ERR	1

#endif
