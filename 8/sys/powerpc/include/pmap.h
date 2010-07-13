/*-
 * Copyright (C) 2006 Semihalf, Marian Balakowicz <m8@semihalf.com>
 * All rights reserved.
 *
 * Adapted for Freescale's e500 core CPUs.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: $NetBSD: pmap.h,v 1.17 2000/03/30 16:18:24 jdolecek Exp $
 */

#ifndef	_MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <machine/sr.h>
#include <machine/pte.h>
#include <machine/tlb.h>

struct pmap_md {
	u_int		md_index;
	vm_paddr_t      md_paddr;
	vm_offset_t     md_vaddr;
	vm_size_t       md_size;
};

#if defined(AIM)

#if !defined(NPMAPS)
#define	NPMAPS		32768
#endif /* !defined(NPMAPS) */

struct	pmap {
	struct	mtx	pm_mtx;
	u_int		pm_sr[16];
	u_int		pm_active;
	u_int		pm_context;

	struct pmap	*pmap_phys;
	struct		pmap_statistics	pm_stats;
};

typedef	struct pmap *pmap_t;

struct pvo_entry {
	LIST_ENTRY(pvo_entry) pvo_vlink;	/* Link to common virt page */
	LIST_ENTRY(pvo_entry) pvo_olink;	/* Link to overflow entry */
	union {
		struct	pte pte;		/* 32 bit PTE */
		struct	lpte lpte;		/* 64 bit PTE */
	} pvo_pte;
	pmap_t		pvo_pmap;		/* Owning pmap */
	vm_offset_t	pvo_vaddr;		/* VA of entry */
};
LIST_HEAD(pvo_head, pvo_entry);

struct	md_page {
	u_int64_t mdpg_attrs;
	struct	pvo_head mdpg_pvoh;
};

#define	pmap_page_get_memattr(m)	VM_MEMATTR_DEFAULT
#define	pmap_page_is_mapped(m)	(!LIST_EMPTY(&(m)->md.mdpg_pvoh))
#define	pmap_page_set_memattr(m, ma)	(void)0

#else

struct pmap {
	struct mtx		pm_mtx;		/* pmap mutex */
	tlbtid_t		pm_tid[MAXCPU];	/* TID to identify this pmap entries in TLB */
	u_int			pm_active;	/* active on cpus */
	int			pm_refs;	/* ref count */
	struct pmap_statistics	pm_stats;	/* pmap statistics */

	/* Page table directory, array of pointers to page tables. */
	pte_t			*pm_pdir[PDIR_NENTRIES];

	/* List of allocated ptbl bufs (ptbl kva regions). */
	TAILQ_HEAD(, ptbl_buf)	pm_ptbl_list;
};
typedef	struct pmap *pmap_t;

struct pv_entry {
	pmap_t pv_pmap;
	vm_offset_t pv_va;
	TAILQ_ENTRY(pv_entry) pv_link;
};
typedef struct pv_entry *pv_entry_t;

struct md_page {
	TAILQ_HEAD(, pv_entry) pv_list;
};

#define	pmap_page_get_memattr(m)	VM_MEMATTR_DEFAULT
#define	pmap_page_is_mapped(m)	(!TAILQ_EMPTY(&(m)->md.pv_list))
#define	pmap_page_set_memattr(m, ma)	(void)0

#endif /* AIM */

extern	struct pmap kernel_pmap_store;
#define	kernel_pmap	(&kernel_pmap_store)

#ifdef _KERNEL

#define	PMAP_LOCK(pmap)		mtx_lock(&(pmap)->pm_mtx)
#define	PMAP_LOCK_ASSERT(pmap, type) \
				mtx_assert(&(pmap)->pm_mtx, (type))
#define	PMAP_LOCK_DESTROY(pmap)	mtx_destroy(&(pmap)->pm_mtx)
#define	PMAP_LOCK_INIT(pmap)	mtx_init(&(pmap)->pm_mtx, "pmap", \
				    NULL, MTX_DEF)
#define	PMAP_LOCKED(pmap)	mtx_owned(&(pmap)->pm_mtx)
#define	PMAP_MTX(pmap)		(&(pmap)->pm_mtx)
#define	PMAP_TRYLOCK(pmap)	mtx_trylock(&(pmap)->pm_mtx)
#define	PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->pm_mtx)

void		pmap_bootstrap(vm_offset_t, vm_offset_t);
void		pmap_kenter(vm_offset_t va, vm_offset_t pa);
void		pmap_kremove(vm_offset_t);
void		*pmap_mapdev(vm_offset_t, vm_size_t);
void		pmap_unmapdev(vm_offset_t, vm_size_t);
void		pmap_deactivate(struct thread *);
vm_offset_t	pmap_kextract(vm_offset_t);
int		pmap_dev_direct_mapped(vm_offset_t, vm_size_t);
boolean_t	pmap_mmu_install(char *name, int prio);

#define	vtophys(va)	pmap_kextract((vm_offset_t)(va))

#define PHYS_AVAIL_SZ	128
extern	vm_offset_t phys_avail[PHYS_AVAIL_SZ];
extern	vm_offset_t virtual_avail;
extern	vm_offset_t virtual_end;

extern	vm_offset_t msgbuf_phys;

extern	int pmap_bootstrapped;

extern vm_offset_t pmap_dumpsys_map(struct pmap_md *, vm_size_t, vm_size_t *);
extern void pmap_dumpsys_unmap(struct pmap_md *, vm_size_t, vm_offset_t);

extern struct pmap_md *pmap_scan_md(struct pmap_md *);

#endif

#endif /* !_MACHINE_PMAP_H_ */
