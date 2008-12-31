/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: hp300: @(#)pmap.h 7.2 (Berkeley) 12/16/90
 *	from: @(#)pmap.h        7.4 (Berkeley) 5/12/91
 *	from: FreeBSD: src/sys/i386/include/pmap.h,v 1.70 2000/11/30
 * $FreeBSD: src/sys/sun4v/include/pmap.h,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <machine/hv_api.h>

#define TSB_INIT_SHIFT          3
#define	PMAP_CONTEXT_MAX	8192
/* 
 *  We don't want TSBs getting above 1MB - which is enough 
 *  for a working set of 512MB - revisit in the future 
 */
#define TSB_MAX_RESIZE          (20 - TSB_INIT_SHIFT - PAGE_SHIFT)

typedef	struct pmap *pmap_t;
typedef uint32_t pmap_cpumask_t;

struct pv_entry;
struct tte_hash;

struct md_page {
	int pv_list_count;
	TAILQ_HEAD(, pv_entry) pv_list;
};


struct pmap {
	uint64_t                pm_context;
	uint64_t                pm_hashscratch;
	uint64_t                pm_tsbscratch;
	vm_paddr_t              pm_tsb_ra;

	struct	mtx             pm_mtx;
	struct tte_hash        *pm_hash;
	TAILQ_HEAD(,pv_entry)	pm_pvlist;	/* list of mappings in pmap */
	struct hv_tsb_info      pm_tsb;
	pmap_cpumask_t          pm_active;      /* mask of cpus currently using pmap */
	pmap_cpumask_t          pm_tlbactive;   /* mask of cpus that have used this pmap */
	struct	pmap_statistics pm_stats;
	uint32_t                pm_tsb_miss_count;
	uint32_t                pm_tsb_cap_miss_count;
	vm_paddr_t              pm_old_tsb_ra[TSB_MAX_RESIZE];
};

#define	PMAP_LOCK(pmap)		mtx_lock(&(pmap)->pm_mtx)
#define	PMAP_LOCK_ASSERT(pmap, type) \
				mtx_assert(&(pmap)->pm_mtx, (type))
#define	PMAP_LOCK_DESTROY(pmap)	mtx_destroy(&(pmap)->pm_mtx)
#define	PMAP_LOCK_INIT(pmap)	mtx_init(&(pmap)->pm_mtx, "pmap", \
				    NULL, MTX_DEF | MTX_DUPOK)
#define	PMAP_LOCKED(pmap)	mtx_owned(&(pmap)->pm_mtx)
#define	PMAP_MTX(pmap)		(&(pmap)->pm_mtx)
#define	PMAP_TRYLOCK(pmap)	mtx_trylock(&(pmap)->pm_mtx)
#define	PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->pm_mtx)

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_list.
 */

typedef struct pv_entry {
	pmap_t  pv_pmap;
	vm_offset_t	pv_va;
	TAILQ_ENTRY(pv_entry) pv_list;
	TAILQ_ENTRY(pv_entry) pv_plist;
} *pv_entry_t;

#define pmap_page_is_mapped(m)  (!TAILQ_EMPTY(&(m)->md.pv_list))

void	pmap_bootstrap(vm_offset_t ekva);
vm_paddr_t pmap_kextract(vm_offset_t va);

void    pmap_invalidate_page(pmap_t pmap, vm_offset_t va, int cleartsb);
void    pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int cleartsb);
void    pmap_invalidate_all(pmap_t pmap);
void	pmap_scrub_pages(vm_paddr_t pa, int64_t size);
void    pmap_free_contig_pages(void *ptr, int npages);
void    *pmap_alloc_zeroed_contig_pages(int npages, uint64_t alignment);

#define	vtophys(va)	pmap_kextract((vm_offset_t)(va))

extern	struct pmap kernel_pmap_store;
#define	kernel_pmap	(&kernel_pmap_store)
extern	vm_paddr_t phys_avail[];
extern	vm_offset_t virtual_avail;
extern	vm_offset_t virtual_end;
extern	vm_paddr_t msgbuf_phys;

#endif /* !_MACHINE_PMAP_H_ */
