/*-
 * Copyright (c) 2003 Alan L. Cox <alc@cs.rice.edu>
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

#ifndef _MACHINE_SF_BUF_H_
#define _MACHINE_SF_BUF_H_

#ifdef __mips_n64
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#else
#include <sys/queue.h>
#endif

#ifdef __mips_n64
/* In 64 bit the whole memory is directly mapped */
struct	sf_buf;

static inline struct sf_buf *
sf_buf_alloc(struct vm_page *m, int pri)
{

	return ((struct sf_buf *)m);
}

static inline void
sf_buf_free(struct sf_buf *sf)
{
}

static __inline vm_offset_t
sf_buf_kva(struct sf_buf *sf)
{
	vm_page_t	m;

	m = (vm_page_t)sf;
	return (MIPS_PHYS_TO_DIRECT(VM_PAGE_TO_PHYS(m)));
}

static __inline struct vm_page *
sf_buf_page(struct sf_buf *sf)
{

	return ((vm_page_t)sf);
}

#else /* ! __mips_n64 */
struct vm_page;

struct sf_buf {
	SLIST_ENTRY(sf_buf) free_list;	/* list of free buffer slots */
	struct		vm_page *m;	/* currently mapped page */
	vm_offset_t	kva;		/* va of mapping */
};

struct sf_buf * sf_buf_alloc(struct vm_page *m, int flags);
void sf_buf_free(struct sf_buf *sf);

static __inline vm_offset_t
sf_buf_kva(struct sf_buf *sf)
{

	return (sf->kva);
}

static __inline struct vm_page *
sf_buf_page(struct sf_buf *sf)
{

	return (sf->m);
}
#endif /* __mips_n64 */

#endif /* !_MACHINE_SF_BUF_H_ */
