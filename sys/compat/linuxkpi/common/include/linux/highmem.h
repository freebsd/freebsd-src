/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2016 Matthew Macy (mmacy@mattmacy.io)
 * Copyright (c) 2017 Mellanox Technologies, Ltd.
 * Copyright (c) 2021 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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
 */

#ifndef _LINUXKPI_LINUX_HIGHMEM_H_
#define _LINUXKPI_LINUX_HIGHMEM_H_

#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>

#include <linux/page.h>

#define	PageHighMem(p)		(0)

static inline vm_page_t
kmap_to_page(void *addr)
{

	return (virt_to_page(addr));
}

static inline void *
kmap(vm_page_t page)
{
	struct sf_buf *sf;

	if (PMAP_HAS_DMAP) {
		return ((void *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(page)));
	} else {
		sched_pin();
		sf = sf_buf_alloc(page, SFB_NOWAIT | SFB_CPUPRIVATE);
		if (sf == NULL) {
			sched_unpin();
			return (NULL);
		}
		return ((void *)sf_buf_kva(sf));
	}
}

static inline void *
kmap_atomic_prot(vm_page_t page, pgprot_t prot)
{
	vm_memattr_t attr = pgprot2cachemode(prot);

	if (attr != VM_MEMATTR_DEFAULT) {
		vm_page_lock(page);
		page->flags |= PG_FICTITIOUS;
		vm_page_unlock(page);
		pmap_page_set_memattr(page, attr);
	}
	return (kmap(page));
}

static inline void *
kmap_atomic(vm_page_t page)
{

	return (kmap_atomic_prot(page, VM_PROT_ALL));
}

static inline void *
kmap_local_page_prot(vm_page_t page, pgprot_t prot)
{

	return (kmap_atomic_prot(page, prot));
}

static inline void
kunmap(vm_page_t page)
{
	struct sf_buf *sf;

	if (!PMAP_HAS_DMAP) {
		/* lookup SF buffer in list */
		sf = sf_buf_alloc(page, SFB_NOWAIT | SFB_CPUPRIVATE);

		/* double-free */
		sf_buf_free(sf);
		sf_buf_free(sf);

		sched_unpin();
	}
}

static inline void
kunmap_atomic(void *vaddr)
{

	if (!PMAP_HAS_DMAP)
		kunmap(virt_to_page(vaddr));
}

static inline void
kunmap_local(void *addr)
{

	kunmap_atomic(addr);
}

#endif	/* _LINUXKPI_LINUX_HIGHMEM_H_ */
