/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2017 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUXKPI_LINUX_COMPAT_H_
#define	_LINUXKPI_LINUX_COMPAT_H_

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/malloc.h>

struct domainset;
struct thread;
struct task_struct;

extern int linux_alloc_current(struct thread *, int flags);
extern void linux_free_current(struct task_struct *);
extern struct domainset *linux_get_vm_domain_set(int node);

#define	__current_unallocated(td)	\
	__predict_false((td)->td_lkpi_task == NULL)

static inline void
linux_set_current(struct thread *td)
{
	if (__current_unallocated(td))
		lkpi_alloc_current(td, M_WAITOK);
}

static inline int
linux_set_current_flags(struct thread *td, int flags)
{
	if (__current_unallocated(td))
		return (lkpi_alloc_current(td, flags));
	return (0);
}

#define	compat_ptr(x)		((void *)(uintptr_t)x)
#define	ptr_to_compat(x)	((uintptr_t)x)

typedef void fpu_safe_exec_cb_t(void *ctx);
void lkpi_fpu_safe_exec(fpu_safe_exec_cb_t func, void *ctx);

#endif	/* _LINUXKPI_LINUX_COMPAT_H_ */
