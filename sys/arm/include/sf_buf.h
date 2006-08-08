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


struct vm_page;

#ifdef ARM_USE_SMALL_ALLOC

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>

struct sf_buf;


static __inline vm_offset_t
sf_buf_kva(struct sf_buf *sf)
{
	return arm_ptovirt(VM_PAGE_TO_PHYS((vm_page_t)sf));
}

static __inline vm_page_t
sf_buf_page(struct sf_buf *sf)
{
	return ((vm_page_t)sf);
}

#else

#include <sys/queue.h>

struct sf_buf {
	LIST_ENTRY(sf_buf) list_entry;	/* list of buffers */
	TAILQ_ENTRY(sf_buf) free_entry;	/* list of buffers */
	struct		vm_page *m;	/* currently mapped page */
	vm_offset_t	kva;		/* va of mapping */
	int		ref_count;	/* usage of this mapping */
};

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

#endif
#endif /* !_MACHINE_SF_BUF_H_ */
