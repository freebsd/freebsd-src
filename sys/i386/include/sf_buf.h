/*-
 * Copyright (c) 2003, 2005 Alan L. Cox <alc@cs.rice.edu>
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

#include <sys/_cpuset.h>
#include <sys/queue.h>

struct vm_page;

struct sf_buf {
	LIST_ENTRY(sf_buf) list_entry;	/* list of buffers */
	TAILQ_ENTRY(sf_buf) free_entry;	/* list of buffers */
	struct		vm_page *m;	/* currently mapped page */
	vm_offset_t	kva;		/* va of mapping */
	int		ref_count;	/* usage of this mapping */
#ifdef SMP
	cpuset_t	cpumask;	/* cpus on which mapping is valid */
#endif
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

boolean_t sf_buf_invalidate_cache(vm_page_t m);

#endif /* !_MACHINE_SF_BUF_H_ */
