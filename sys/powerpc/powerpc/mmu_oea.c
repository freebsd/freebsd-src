/*
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
 * $NetBSD: pmap.c,v 1.28 2000/03/26 20:42:36 kleink Exp $
 */
/*
 * Copyright (C) 2001 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/vmmeter.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>

#include <sys/user.h>

#include <machine/bat.h>
#include <machine/pcb.h>
#include <machine/powerpc.h>
#include <machine/pte.h>

pte_t	*ptable;
int	ptab_cnt;
u_int	ptab_mask;
#define	HTABSIZE	(ptab_cnt * 64)

#define	MINPV		2048

struct pte_ovfl {
	LIST_ENTRY(pte_ovfl) po_list;	/* Linked list of overflow entries */
	struct pte	po_pte;		/* PTE for this mapping */
};

LIST_HEAD(pte_ovtab, pte_ovfl) *potable; /* Overflow entries for ptable */

static struct pmap	kernel_pmap_store;
pmap_t			kernel_pmap;

static int	npgs;
static u_int	nextavail;

#ifndef MSGBUFADDR
extern vm_offset_t	msgbuf_paddr;
#endif

static struct mem_region	*mem, *avail;

vm_offset_t	avail_start;
vm_offset_t	avail_end;
vm_offset_t	virtual_avail;
vm_offset_t	virtual_end;

vm_offset_t	kernel_vm_end;

static int	pmap_pagedaemon_waken = 0;

extern unsigned int	Maxmem;

#define	ATTRSHFT	4

struct pv_entry	*pv_table;

static vm_zone_t	pvzone;
static struct vm_zone	pvzone_store;
static struct vm_object	pvzone_obj;
static int		pv_entry_count=0, pv_entry_max=0, pv_entry_high_water=0;
static struct pv_entry	*pvinit;

#if !defined(PMAP_SHPGPERPROC)
#define	PMAP_SHPGPERPROC	200
#endif

struct pv_page;
struct pv_page_info {
	LIST_ENTRY(pv_page) pgi_list;
	struct pv_entry	*pgi_freelist;
	int		pgi_nfree;
};
#define	NPVPPG	((PAGE_SIZE - sizeof(struct pv_page_info)) / sizeof(struct pv_entry))
struct pv_page {
	struct pv_page_info	pvp_pgi;
	struct pv_entry		pvp_pv[NPVPPG];
};
LIST_HEAD(pv_page_list, pv_page) pv_page_freelist;
int	pv_nfree;
int	pv_pcnt;
static struct pv_entry	*pmap_alloc_pv(void);
static void		pmap_free_pv(struct pv_entry *);

struct po_page;
struct po_page_info {
	LIST_ENTRY(po_page) pgi_list;
	vm_page_t	pgi_page;
	LIST_HEAD(po_freelist, pte_ovfl) pgi_freelist;
	int		pgi_nfree;
};
#define	NPOPPG	((PAGE_SIZE - sizeof(struct po_page_info)) / sizeof(struct pte_ovfl))
struct po_page {
	struct po_page_info	pop_pgi;
	struct pte_ovfl		pop_po[NPOPPG];
};
LIST_HEAD(po_page_list, po_page) po_page_freelist;
int	po_nfree;
int	po_pcnt;
static struct pte_ovfl	*poalloc(void);
static void		pofree(struct pte_ovfl *, int);

static u_int	usedsr[NPMAPS / sizeof(u_int) / 8];

static int	pmap_initialized;

int	pte_spill(vm_offset_t);

/*
 * These small routines may have to be replaced,
 * if/when we support processors other that the 604.
 */
static __inline void
tlbie(vm_offset_t ea)
{

	__asm __volatile ("tlbie %0" :: "r"(ea));
}

static __inline void
tlbsync(void)
{

	__asm __volatile ("sync; tlbsync; sync");
}

static __inline void
tlbia(void)
{
	vm_offset_t	i;
	
	__asm __volatile ("sync");
	for (i = 0; i < (vm_offset_t)0x00040000; i += 0x00001000) {
		tlbie(i);
	}
	tlbsync();
}

static __inline int
ptesr(sr_t *sr, vm_offset_t addr)
{

	return sr[(u_int)addr >> ADDR_SR_SHFT];
}

static __inline int
pteidx(sr_t sr, vm_offset_t addr)
{
	int	hash;
	
	hash = (sr & SR_VSID) ^ (((u_int)addr & ADDR_PIDX) >> ADDR_PIDX_SHFT);
	return hash & ptab_mask;
}

static __inline int
ptematch(pte_t *ptp, sr_t sr, vm_offset_t va, int which)
{

	return ptp->pte_hi == (((sr & SR_VSID) << PTE_VSID_SHFT) |
	    (((u_int)va >> ADDR_API_SHFT) & PTE_API) | which);
}

static __inline struct pv_entry *
pa_to_pv(vm_offset_t pa)
{
#if 0 /* XXX */
	int	bank, pg;

	bank = vm_physseg_find(atop(pa), &pg);
	if (bank == -1)
		return NULL;
	return &vm_physmem[bank].pmseg.pvent[pg];
#endif
	return (NULL);
}

static __inline char *
pa_to_attr(vm_offset_t pa)
{
#if 0 /* XXX */
	int	bank, pg;

	bank = vm_physseg_find(atop(pa), &pg);
	if (bank == -1)
		return NULL;
	return &vm_physmem[bank].pmseg.attrs[pg];
#endif
	return (NULL);
}

/*
 * Try to insert page table entry *pt into the ptable at idx.
 *
 * Note: *pt mustn't have PTE_VALID set.
 * This is done here as required by Book III, 4.12.
 */
static int
pte_insert(int idx, pte_t *pt)
{
	pte_t	*ptp;
	int	i;

	/*
	 * First try primary hash.
	 */
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++) {
		if (!(ptp->pte_hi & PTE_VALID)) {
			*ptp = *pt;
			ptp->pte_hi &= ~PTE_HID;
			__asm __volatile ("sync");
			ptp->pte_hi |= PTE_VALID;
			return 1;
		}
	}

	/*
	 * Then try secondary hash.
	 */

	idx ^= ptab_mask;

	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++) {
		if (!(ptp->pte_hi & PTE_VALID)) {
			*ptp = *pt;
			ptp->pte_hi |= PTE_HID;
			__asm __volatile ("sync");
			ptp->pte_hi |= PTE_VALID;
			return 1;
		} 
	}

	return 0;
}

/*
 * Spill handler.
 *
 * Tries to spill a page table entry from the overflow area.
 * Note that this routine runs in real mode on a separate stack,
 * with interrupts disabled.
 */
int
pte_spill(vm_offset_t addr)
{
	int		idx, i;
	sr_t		sr;
	struct pte_ovfl	*po;
	pte_t		ps;
	pte_t		*pt;

	__asm ("mfsrin %0,%1" : "=r"(sr) : "r"(addr));
	idx = pteidx(sr, addr);
	for (po = potable[idx].lh_first; po; po = po->po_list.le_next) {
		if (ptematch(&po->po_pte, sr, addr, 0)) {
			/*
			 * Now found an entry to be spilled into the real
			 * ptable.
			 */
			if (pte_insert(idx, &po->po_pte)) {
				LIST_REMOVE(po, po_list);
				pofree(po, 0);
				return 1;
			}
			/*
			 * Have to substitute some entry. Use the primary
			 * hash for this.
			 *
			 * Use low bits of timebase as random generator
			 */
			__asm ("mftb %0" : "=r"(i));
			pt = ptable + idx * 8 + (i & 7);
			pt->pte_hi &= ~PTE_VALID;
			ps = *pt;
			__asm __volatile ("sync");
			tlbie(addr);
			tlbsync();
			*pt = po->po_pte;
			__asm __volatile ("sync");
			pt->pte_hi |= PTE_VALID;
			po->po_pte = ps;
			if (ps.pte_hi & PTE_HID) {
				/*
				 * We took an entry that was on the alternate
				 * hash chain, so move it to it's original
				 * chain.
				 */
				po->po_pte.pte_hi &= ~PTE_HID;
				LIST_REMOVE(po, po_list);
				LIST_INSERT_HEAD(potable + (idx ^ ptab_mask),
						 po, po_list);
			}
			return 1;
		}
	}

	return 0;
}

/*
 * This is called during powerpc_init, before the system is really initialized.
 */
void
pmap_setavailmem(u_int kernelstart, u_int kernelend)
{
	struct mem_region	*mp, *mp1;
	int			cnt, i;
	u_int			s, e, sz;

	/*
	 * Get memory.
	 */
	mem_regions(&mem, &avail);
	for (mp = mem; mp->size; mp++)
		Maxmem += btoc(mp->size);

	/*
	 * Count the number of available entries.
	 */
	for (cnt = 0, mp = avail; mp->size; mp++) {
		cnt++; 
	}

	/*
	 * Page align all regions.
	 * Non-page aligned memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	kernelstart &= ~PAGE_MASK;
	kernelend = (kernelend + PAGE_MASK) & ~PAGE_MASK;
	for (mp = avail; mp->size; mp++) {
		s = mp->start;
		e = mp->start + mp->size;
		/*
		 * Check whether this region holds all of the kernel.
		 */
		if (s < kernelstart && e > kernelend) {
			avail[cnt].start = kernelend;
			avail[cnt++].size = e - kernelend;
			e = kernelstart;
		}
		/*
		 * Look whether this regions starts within the kernel.
		 */
		if (s >= kernelstart && s < kernelend) {
			if (e <= kernelend)
				goto empty;
			s = kernelend;
		}
		/*
		 * Now look whether this region ends within the kernel.
		 */
		if (e > kernelstart && e <= kernelend) {
			if (s >= kernelstart)
				goto empty;
			e = kernelstart;
		}
		/*
		 * Now page align the start and size of the region.
		 */
		s = round_page(s);
		e = trunc_page(e);
		if (e < s) {
			e = s;
		}
		sz = e - s;
		/*
		 * Check whether some memory is left here.
		 */
		if (sz == 0) {
		empty:
			bcopy(mp + 1, mp,
			      (cnt - (mp - avail)) * sizeof *mp);
			cnt--;
			mp--;
			continue;
		}

		/*
		 * Do an insertion sort.
		 */
		npgs += btoc(sz);

		for (mp1 = avail; mp1 < mp; mp1++) {
			if (s < mp1->start) {
				break;
			}
		}

		if (mp1 < mp) {
			bcopy(mp1, mp1 + 1, (char *)mp - (char *)mp1);
			mp1->start = s;
			mp1->size = sz;
		} else {
			mp->start = s;
			mp->size = sz;
		}
	}

#ifdef HTABENTS
	ptab_cnt = HTABENTS;
#else
	ptab_cnt = (Maxmem + 1) / 2;

	/* The minimum is 1024 PTEGs. */
	if (ptab_cnt < 1024) {
		ptab_cnt = 1024;
	}

	/* Round up to power of 2. */
	__asm ("cntlzw %0,%1" : "=r"(i) : "r"(ptab_cnt - 1));
	ptab_cnt = 1 << (32 - i);
#endif

	/*
	 * Find suitably aligned memory for HTAB.
	 */
	for (mp = avail; mp->size; mp++) {
		s = roundup(mp->start, HTABSIZE) - mp->start;

		if (mp->size < s + HTABSIZE) {
			continue;
		}

		ptable = (pte_t *)(mp->start + s);

		if (mp->size == s + HTABSIZE) {
			if (s)
				mp->size = s;
			else {
				bcopy(mp + 1, mp,
				      (cnt - (mp - avail)) * sizeof *mp);
				mp = avail;
			}
			break;
		}

		if (s != 0) {
			bcopy(mp, mp + 1,
			      (cnt - (mp - avail)) * sizeof *mp);
			mp++->size = s;
			cnt++;
		}

		mp->start += s + HTABSIZE;
		mp->size -= s + HTABSIZE;
		break;
	}

	if (!mp->size) {
		panic("not enough memory?");
	}

	npgs -= btoc(HTABSIZE);
	bzero((void *)ptable, HTABSIZE);
	ptab_mask = ptab_cnt - 1;

	/*
	 * We cannot do pmap_steal_memory here,
	 * since we don't run with translation enabled yet.
	 */
	s = sizeof(struct pte_ovtab) * ptab_cnt;
	sz = round_page(s);

	for (mp = avail; mp->size; mp++) {
		if (mp->size >= sz) {
			break;
		}
	}

	if (!mp->size) {
		panic("not enough memory?");
	}

	npgs -= btoc(sz);
	potable = (struct pte_ovtab *)mp->start;
	mp->size -= sz;
	mp->start += sz;

	if (mp->size <= 0) {
		bcopy(mp + 1, mp, (cnt - (mp - avail)) * sizeof *mp);
	}

	for (i = 0; i < ptab_cnt; i++) {
		LIST_INIT(potable + i);
	}

#ifndef MSGBUFADDR
	/*
	 * allow for msgbuf
	 */
	sz = round_page(MSGBUFSIZE);
	mp = NULL;

	for (mp1 = avail; mp1->size; mp1++) {
		if (mp1->size >= sz) {
			mp = mp1;
		}
	}

	if (mp == NULL) {
		panic("not enough memory?");
	}

	npgs -= btoc(sz);
	msgbuf_paddr = mp->start + mp->size - sz;
	mp->size -= sz;

	if (mp->size <= 0) {
		bcopy(mp + 1, mp, (cnt - (mp - avail)) * sizeof *mp);
	}
#endif

	nextavail = avail->start;
	avail_start = avail->start;
	for (mp = avail, i = 0; mp->size; mp++) {
		avail_end = mp->start + mp->size;
		phys_avail[i++] = mp->start;
		phys_avail[i++] = mp->start + mp->size;
	}


}

void
pmap_bootstrap()
{
	int i;
	u_int32_t batl, batu;

	/*
	 * Initialize kernel pmap and hardware.
	 */
	kernel_pmap = &kernel_pmap_store;

	batu = BATU(0x80000000, BAT_BL_256M, BAT_Vs);
	batl = BATL(0x80000000, BAT_M, BAT_PP_RW);
	__asm ("mtdbatu 1,%0; mtdbatl 1,%1" :: "r" (batu), "r" (batl));

#if NPMAPS >= KERNEL_SEGMENT / 16
	usedsr[KERNEL_SEGMENT / 16 / (sizeof usedsr[0] * 8)]
		|= 1 << ((KERNEL_SEGMENT / 16) % (sizeof usedsr[0] * 8));
#endif

#if 0 /* XXX */
	for (i = 0; i < 16; i++) {
		kernel_pmap->pm_sr[i] = EMPTY_SEGMENT;
		__asm __volatile ("mtsrin %0,%1"
			      :: "r"(EMPTY_SEGMENT), "r"(i << ADDR_SR_SHFT));
	}
#endif

	for (i = 0; i < 16; i++) {
		int	j;

		__asm __volatile ("mfsrin %0,%1"
			: "=r" (j)
			: "r" (i << ADDR_SR_SHFT));

		kernel_pmap->pm_sr[i] = j;
	}

	kernel_pmap->pm_sr[KERNEL_SR] = KERNEL_SEGMENT;
	__asm __volatile ("mtsr %0,%1"
		      :: "n"(KERNEL_SR), "r"(KERNEL_SEGMENT));

	__asm __volatile ("sync; mtsdr1 %0; isync"
		      :: "r"((u_int)ptable | (ptab_mask >> 10)));

	tlbia();

	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;
}

/*
 * Initialize anything else for pmap handling.
 * Called during vm_init().
 */
void
pmap_init(vm_offset_t phys_start, vm_offset_t phys_end)
{
	int	initial_pvs;

	/*
	 * init the pv free list
	 */
	initial_pvs = vm_page_array_size;
	if (initial_pvs < MINPV) {
		initial_pvs = MINPV;
	}
	pvzone = &pvzone_store;
	pvinit = (struct pv_entry *) kmem_alloc(kernel_map,
	    initial_pvs * sizeof(struct pv_entry));
	zbootinit(pvzone, "PV ENTRY", sizeof(struct pv_entry), pvinit,
	    vm_page_array_size);

	pmap_initialized = TRUE;
}

/*
 * Initialize a preallocated and zeroed pmap structure.
 */
void
pmap_pinit(struct pmap *pm)
{
	int	i, j;

	/*
	 * Allocate some segment registers for this pmap.
	 */
	pm->pm_refs = 1;
	for (i = 0; i < sizeof usedsr / sizeof usedsr[0]; i++) {
		if (usedsr[i] != 0xffffffff) {
			j = ffs(~usedsr[i]) - 1;
			usedsr[i] |= 1 << j;
			pm->pm_sr[0] = (i * sizeof usedsr[0] * 8 + j) * 16;
			for (i = 1; i < 16; i++) {
				pm->pm_sr[i] = pm->pm_sr[i - 1] + 1;
			}
			return;
		}
	}
	panic("out of segments");
}

void
pmap_pinit2(pmap_t pmap)
{

	/*
	 * Nothing to be done.
	 */
	return;
}

/*
 * Add a reference to the given pmap.
 */
void
pmap_reference(struct pmap *pm)
{

	pm->pm_refs++;
}

/*
 * Retire the given pmap from service.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_destroy(struct pmap *pm)
{

	if (--pm->pm_refs == 0) {
		pmap_release(pm);
		free((caddr_t)pm, M_VMPGDATA);
	}
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 */
void
pmap_release(struct pmap *pm)
{
	int	i, j;
	
	if (!pm->pm_sr[0]) {
		panic("pmap_release");
	}
	i = pm->pm_sr[0] / 16;
	j = i % (sizeof usedsr[0] * 8);
	i /= sizeof usedsr[0] * 8;
	usedsr[i] &= ~(1 << j);
}

/*
 * Copy the range specified by src_addr/len
 * from the source map to the range dst_addr/len
 * in the destination map.
 *
 * This routine is only advisory and need not do anything.
 */
void
pmap_copy(struct pmap *dst_pmap, struct pmap *src_pmap, vm_offset_t dst_addr,
    vm_size_t len, vm_offset_t src_addr)
{

	return;
}

/*
 * Garbage collects the physical map system for
 * pages which are no longer used.
 * Success need not be guaranteed -- that is, there
 * may well be pages which are not referenced, but
 * others may be collected.
 * Called by the pageout daemon when pages are scarce.
 */
void
pmap_collect(void)
{

	return;
}

/*
 * Fill the given physical page with zeroes.
 */
void
pmap_zero_page(vm_offset_t pa)
{
#if 0
	bzero((caddr_t)pa, PAGE_SIZE);
#else
	int	i;

	for (i = PAGE_SIZE/CACHELINESIZE; i > 0; i--) {
		__asm __volatile ("dcbz 0,%0" :: "r"(pa));
		pa += CACHELINESIZE;
	}
#endif
}

void
pmap_zero_page_area(vm_offset_t pa, int off, int size)
{

	bzero((caddr_t)pa + off, size);
}

/*
 * Copy the given physical source page to its destination.
 */
void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{

	bcopy((caddr_t)src, (caddr_t)dst, PAGE_SIZE);
}

static struct pv_entry *
pmap_alloc_pv()
{
	pv_entry_count++;

	if (pv_entry_high_water &&
	    (pv_entry_count > pv_entry_high_water) &&
	    (pmap_pagedaemon_waken == 0)) {
		pmap_pagedaemon_waken = 1;
		wakeup(&vm_pages_needed);
	}

	return zalloc(pvzone);
}

static void
pmap_free_pv(struct pv_entry *pv)
{

	pv_entry_count--;
	zfree(pvzone, pv);
}

/*
 * We really hope that we don't need overflow entries
 * before the VM system is initialized!
 *
 * XXX: Should really be switched over to the zone allocator.
 */
static struct pte_ovfl *
poalloc()
{
	struct po_page	*pop;
	struct pte_ovfl	*po;
	vm_page_t	mem;
	int		i;
	
	if (!pmap_initialized) {
		panic("poalloc");
	}
	
	if (po_nfree == 0) {
		/*
		 * Since we cannot use maps for potable allocation,
		 * we have to steal some memory from the VM system.			XXX
		 */
		mem = vm_page_alloc(NULL, 0, VM_ALLOC_SYSTEM);
		po_pcnt++;
		pop = (struct po_page *)VM_PAGE_TO_PHYS(mem);
		pop->pop_pgi.pgi_page = mem;
		LIST_INIT(&pop->pop_pgi.pgi_freelist);
		for (i = NPOPPG - 1, po = pop->pop_po + 1; --i >= 0; po++) {
			LIST_INSERT_HEAD(&pop->pop_pgi.pgi_freelist, po,
			    po_list);
		}
		po_nfree += pop->pop_pgi.pgi_nfree = NPOPPG - 1;
		LIST_INSERT_HEAD(&po_page_freelist, pop, pop_pgi.pgi_list);
		po = pop->pop_po;
	} else {
		po_nfree--;
		pop = po_page_freelist.lh_first;
		if (--pop->pop_pgi.pgi_nfree <= 0) {
			LIST_REMOVE(pop, pop_pgi.pgi_list);
		}
		po = pop->pop_pgi.pgi_freelist.lh_first;
		LIST_REMOVE(po, po_list);
	}

	return po;
}

static void
pofree(struct pte_ovfl *po, int freepage)
{
	struct po_page	*pop;
	
	pop = (struct po_page *)trunc_page((vm_offset_t)po);
	switch (++pop->pop_pgi.pgi_nfree) {
	case NPOPPG:
		if (!freepage) {
			break;
		}
		po_nfree -= NPOPPG - 1;
		po_pcnt--;
		LIST_REMOVE(pop, pop_pgi.pgi_list);
		vm_page_free(pop->pop_pgi.pgi_page);
		return;
	case 1:
		LIST_INSERT_HEAD(&po_page_freelist, pop, pop_pgi.pgi_list);
	default:
		break;
	}
	LIST_INSERT_HEAD(&pop->pop_pgi.pgi_freelist, po, po_list);
	po_nfree++;
}

/*
 * This returns whether this is the first mapping of a page.
 */
static int
pmap_enter_pv(int pteidx, vm_offset_t va, vm_offset_t pa)
{
	struct pv_entry	*pv, *npv;
	int		s, first;
	
	if (!pmap_initialized) {
		return 0;
	}

	s = splimp();

	pv = pa_to_pv(pa);
	first = pv->pv_idx;
	if (pv->pv_idx == -1) {
		/*
		 * No entries yet, use header as the first entry.
		 */
		pv->pv_va = va;
		pv->pv_idx = pteidx;
		pv->pv_next = NULL;
	} else {
		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 */
		npv = pmap_alloc_pv();
		npv->pv_va = va;
		npv->pv_idx = pteidx;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
	}
	splx(s);
	return first;
}

static void
pmap_remove_pv(int pteidx, vm_offset_t va, vm_offset_t pa, struct pte *pte)
{
	struct pv_entry	*pv, *npv;
	char		*attr;

	/*
	 * First transfer reference/change bits to cache.
	 */
	attr = pa_to_attr(pa);
	if (attr == NULL) {
		return;
	}
	*attr |= (pte->pte_lo & (PTE_REF | PTE_CHG)) >> ATTRSHFT;
	
	/*
	 * Remove from the PV table.
	 */
	pv = pa_to_pv(pa);
	
	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */
	if (pteidx == pv->pv_idx && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			pmap_free_pv(npv);
		} else {
			pv->pv_idx = -1;
		}
	} else {
		for (; (npv = pv->pv_next); pv = npv) {
			if (pteidx == npv->pv_idx && va == npv->pv_va) {
				break;
			}
		}
		if (npv) {
			pv->pv_next = npv->pv_next;
			pmap_free_pv(npv);
		}
#ifdef	DIAGNOSTIC
		else {
			panic("pmap_remove_pv: not on list\n");
		}
#endif
	}
}

/*
 * Insert physical page at pa into the given pmap at virtual address va.
 */
void
pmap_enter(pmap_t pm, vm_offset_t va, vm_page_t pg, vm_prot_t prot,
    boolean_t wired)
{
	sr_t			sr;
	int			idx, s;
	pte_t			pte;
	struct pte_ovfl		*po;
	struct mem_region	*mp;
	vm_offset_t		pa;

	pa = VM_PAGE_TO_PHYS(pg) & ~PAGE_MASK;

	/*
	 * Have to remove any existing mapping first.
	 */
	pmap_remove(pm, va, va + PAGE_SIZE);

	/*
	 * Compute the HTAB index.
	 */
	idx = pteidx(sr = ptesr(pm->pm_sr, va), va);
	/*
	 * Construct the PTE.
	 *
	 * Note: Don't set the valid bit for correct operation of tlb update.
	 */
	pte.pte_hi = ((sr & SR_VSID) << PTE_VSID_SHFT)
		| ((va & ADDR_PIDX) >> ADDR_API_SHFT);
	pte.pte_lo = (pa & PTE_RPGN) | PTE_M | PTE_I | PTE_G;

	for (mp = mem; mp->size; mp++) {
		if (pa >= mp->start && pa < mp->start + mp->size) {
			pte.pte_lo &= ~(PTE_I | PTE_G);
			break;
		}
	}
	if (prot & VM_PROT_WRITE) {
		pte.pte_lo |= PTE_RW;
	} else {
		pte.pte_lo |= PTE_RO;
	}

	/*
	 * Now record mapping for later back-translation.
	 */
	if (pmap_initialized && (pg->flags & PG_FICTITIOUS) == 0) {
		if (pmap_enter_pv(idx, va, pa)) {
			/* 
			 * Flush the real memory from the cache.
			 */
			__syncicache((void *)pa, PAGE_SIZE);
		}
	}

	s = splimp();
	pm->pm_stats.resident_count++;
	/*
	 * Try to insert directly into HTAB.
	 */
	if (pte_insert(idx, &pte)) {
		splx(s);
		return;
	}

	/*
	 * Have to allocate overflow entry.
	 *
	 * Note, that we must use real addresses for these.
	 */
	po = poalloc();
	po->po_pte = pte;
	LIST_INSERT_HEAD(potable + idx, po, po_list);
	splx(s);
}

void
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	struct vm_page	pg;

	pg.phys_addr = pa;
	pmap_enter(kernel_pmap, va, &pg, VM_PROT_READ|VM_PROT_WRITE, TRUE);
}

void
pmap_kremove(vm_offset_t va)
{
	pmap_remove(kernel_pmap, va, va + PAGE_SIZE);
}

/*
 * Remove the given range of mapping entries.
 */
void
pmap_remove(struct pmap *pm, vm_offset_t va, vm_offset_t endva)
{
	int		idx, i, s;
	sr_t		sr;
	pte_t		*ptp;
	struct pte_ovfl	*po, *npo;

	s = splimp();
	while (va < endva) {
		idx = pteidx(sr = ptesr(pm->pm_sr, va), va);
		for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++) {
			if (ptematch(ptp, sr, va, PTE_VALID)) {
				pmap_remove_pv(idx, va, ptp->pte_lo, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				__asm __volatile ("sync");
				tlbie(va);
				tlbsync();
				pm->pm_stats.resident_count--;
			}
		}
		for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0;
		    ptp++) {
			if (ptematch(ptp, sr, va, PTE_VALID | PTE_HID)) {
				pmap_remove_pv(idx, va, ptp->pte_lo, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				__asm __volatile ("sync");
				tlbie(va);
				tlbsync();
				pm->pm_stats.resident_count--;
			}
		}
		for (po = potable[idx].lh_first; po; po = npo) {
			npo = po->po_list.le_next;
			if (ptematch(&po->po_pte, sr, va, 0)) {
				pmap_remove_pv(idx, va, po->po_pte.pte_lo,
					       &po->po_pte);
				LIST_REMOVE(po, po_list);
				pofree(po, 1);
				pm->pm_stats.resident_count--;
			}
		}
		va += PAGE_SIZE;
	}
	splx(s);
}

static pte_t *
pte_find(struct pmap *pm, vm_offset_t va)
{
	int		idx, i;
	sr_t		sr;
	pte_t		*ptp;
	struct pte_ovfl	*po;

	idx = pteidx(sr = ptesr(pm->pm_sr, va), va);
	for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++) {
		if (ptematch(ptp, sr, va, PTE_VALID)) {
			return ptp;
		}
	}
	for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0; ptp++) {
		if (ptematch(ptp, sr, va, PTE_VALID | PTE_HID)) {
			return ptp;
		}
	}
	for (po = potable[idx].lh_first; po; po = po->po_list.le_next) {
		if (ptematch(&po->po_pte, sr, va, 0)) {
			return &po->po_pte;
		}
	}
	return 0;
}

/*
 * Get the physical page address for the given pmap/virtual address.
 */
vm_offset_t
pmap_extract(pmap_t pm, vm_offset_t va)
{
	pte_t	*ptp;
	int	s;

	s = splimp();
	
	if (!(ptp = pte_find(pm, va))) {
		splx(s);
		return (0);
	}
	splx(s);
	return ((ptp->pte_lo & PTE_RPGN) | (va & ADDR_POFF));
}

/*
 * Lower the protection on the specified range of this pmap.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_protect(struct pmap *pm, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	pte_t	*ptp;
	int	valid, s;
	
	if (prot & VM_PROT_READ) {
		s = splimp();
		while (sva < eva) {
			ptp = pte_find(pm, sva);
			if (ptp) {
				valid = ptp->pte_hi & PTE_VALID;
				ptp->pte_hi &= ~PTE_VALID;
				__asm __volatile ("sync");
				tlbie(sva);
				tlbsync();
				ptp->pte_lo &= ~PTE_PP;
				ptp->pte_lo |= PTE_RO;
				__asm __volatile ("sync");
				ptp->pte_hi |= valid;
			}
			sva += PAGE_SIZE;
		}
		splx(s);
		return;
	}
	pmap_remove(pm, sva, eva);
}

boolean_t
ptemodify(vm_page_t pg, u_int mask, u_int val)
{
	vm_offset_t	pa;
	struct pv_entry	*pv;
	pte_t		*ptp;
	struct pte_ovfl	*po;
	int		i, s;
	char		*attr;
	int		rv;

	pa = VM_PAGE_TO_PHYS(pg);

	/*
	 * First modify bits in cache.
	 */
	attr = pa_to_attr(pa);
	if (attr == NULL) {
		return FALSE;
	}

	*attr &= ~mask >> ATTRSHFT;
	*attr |= val >> ATTRSHFT;
	
	pv = pa_to_pv(pa);
	if (pv->pv_idx < 0) {
		return FALSE;
	}

	rv = FALSE;
	s = splimp();
	for (; pv; pv = pv->pv_next) {
		for (ptp = ptable + pv->pv_idx * 8, i = 8; --i >= 0; ptp++) {
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				ptp->pte_hi &= ~PTE_VALID;
				__asm __volatile ("sync");
				tlbie(pv->pv_va);
				tlbsync();
				rv |= ptp->pte_lo & mask; 
				ptp->pte_lo &= ~mask;
				ptp->pte_lo |= val;
				__asm __volatile ("sync");
				ptp->pte_hi |= PTE_VALID;
			}
		}
		for (ptp = ptable + (pv->pv_idx ^ ptab_mask) * 8, i = 8;
		    --i >= 0; ptp++) {
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				ptp->pte_hi &= ~PTE_VALID;
				__asm __volatile ("sync");
				tlbie(pv->pv_va);
				tlbsync();
				rv |= ptp->pte_lo & mask; 
				ptp->pte_lo &= ~mask;
				ptp->pte_lo |= val;
				__asm __volatile ("sync");
				ptp->pte_hi |= PTE_VALID;
			}
		}
		for (po = potable[pv->pv_idx].lh_first; po;
		    po = po->po_list.le_next) {
			if ((po->po_pte.pte_lo & PTE_RPGN) == pa) {
				rv |= ptp->pte_lo & mask; 
				po->po_pte.pte_lo &= ~mask;
				po->po_pte.pte_lo |= val;
			}
		}
	}
	splx(s);
	return rv != 0;
}

int
ptebits(vm_page_t pg, int bit)
{
	struct pv_entry	*pv;
	pte_t		*ptp;
	struct pte_ovfl	*po;
	int		i, s, bits;
	char		*attr;
	vm_offset_t	pa;

	bits = 0;
	pa = VM_PAGE_TO_PHYS(pg);

	/*
	 * First try the cache.
	 */
	attr = pa_to_attr(pa);
	if (attr == NULL) {
		return 0;
	}
	bits |= (*attr << ATTRSHFT) & bit;
	if (bits == bit) {
		return bits;
	}

	pv = pa_to_pv(pa);
	if (pv->pv_idx < 0) {
		return 0;
	}
	
	s = splimp();
	for (; pv; pv = pv->pv_next) {
		for (ptp = ptable + pv->pv_idx * 8, i = 8; --i >= 0; ptp++) {
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				bits |= ptp->pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
		}
		for (ptp = ptable + (pv->pv_idx ^ ptab_mask) * 8, i = 8;
		    --i >= 0; ptp++) {
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				bits |= ptp->pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
		}
		for (po = potable[pv->pv_idx].lh_first; po;
		    po = po->po_list.le_next) {
			if ((po->po_pte.pte_lo & PTE_RPGN) == pa) {
				bits |= po->po_pte.pte_lo & bit;
				if (bits == bit) {
					splx(s);
					return bits;
				}
			}
		}
	}
	splx(s);
	return bits;
}

/*
 * Lower the protection on the specified physical page.
 *
 * There are only two cases: either the protection is going to 0,
 * or it is going to read-only.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	vm_offset_t	pa;
	vm_offset_t	va;
	pte_t		*ptp;
	struct pte_ovfl	*po, *npo;
	int		i, s, idx;
	struct pv_entry	*pv;

	pa = VM_PAGE_TO_PHYS(m);

	pa &= ~ADDR_POFF;
	if (prot & VM_PROT_READ) {
		ptemodify(m, PTE_PP, PTE_RO);
		return;
	}

	pv = pa_to_pv(pa);
	if (pv == NULL) {
		return;
	}

	s = splimp();
	while (pv->pv_idx >= 0) {
		idx = pv->pv_idx;
		va = pv->pv_va;
		for (ptp = ptable + idx * 8, i = 8; --i >= 0; ptp++) {
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				pmap_remove_pv(idx, va, pa, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				__asm __volatile ("sync");
				tlbie(va);
				tlbsync();
				goto next;
			}
		}
		for (ptp = ptable + (idx ^ ptab_mask) * 8, i = 8; --i >= 0;
		    ptp++) {
			if ((ptp->pte_hi & PTE_VALID)
			    && (ptp->pte_lo & PTE_RPGN) == pa) {
				pmap_remove_pv(idx, va, pa, ptp);
				ptp->pte_hi &= ~PTE_VALID;
				__asm __volatile ("sync");
				tlbie(va);
				tlbsync();
				goto next;
			}
		}
		for (po = potable[idx].lh_first; po; po = npo) {
			npo = po->po_list.le_next;
			if ((po->po_pte.pte_lo & PTE_RPGN) == pa) {
				pmap_remove_pv(idx, va, pa, &po->po_pte);
				LIST_REMOVE(po, po_list);
				pofree(po, 1);
				goto next;
			}
		}
next:
	}
	splx(s);
}

/*
 * Activate the address space for the specified process.  If the process
 * is the current process, load the new MMU context.
 */
void
pmap_activate(struct thread *td)
{
	struct pcb	*pcb;
	pmap_t		pmap;
	pmap_t		rpm;
	int		psl, i, ksr, seg;

	pcb = td->td_pcb;
	pmap = vmspace_pmap(td->td_proc->p_vmspace);

	/*
	 * XXX Normally performed in cpu_fork().
	 */
	if (pcb->pcb_pm != pmap) {
		pcb->pcb_pm = pmap;
		(vm_offset_t) pcb->pcb_pmreal = pmap_extract(kernel_pmap,
		    (vm_offset_t)pcb->pcb_pm);
	}

	if (td == curthread) {
		/* Disable interrupts while switching. */
		psl = mfmsr();
		mtmsr(psl & ~PSL_EE);

#if 0 /* XXX */
		/* Store pointer to new current pmap. */
		curpm = pcb->pcb_pmreal;
#endif

		/* Save kernel SR. */
		__asm __volatile("mfsr %0,14" : "=r"(ksr) :);

		/*
		 * Set new segment registers.  We use the pmap's real
		 * address to avoid accessibility problems.
		 */
		rpm = pcb->pcb_pmreal;
		for (i = 0; i < 16; i++) {
			seg = rpm->pm_sr[i];
			__asm __volatile("mtsrin %0,%1"
			    :: "r"(seg), "r"(i << ADDR_SR_SHFT));
		}

		/* Restore kernel SR. */
		__asm __volatile("mtsr 14,%0" :: "r"(ksr));

		/* Interrupts are OK again. */
		mtmsr(psl);
	}
}

/*
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 */
void
pmap_qenter(vm_offset_t va, vm_page_t *m, int count)
{
	int	i;

	for (i = 0; i < count; i++) {
		vm_offset_t tva = va + i * PAGE_SIZE;
		pmap_kenter(tva, VM_PAGE_TO_PHYS(m[i]));
	}
}

/*
 * this routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	vm_offset_t	end_va;

	end_va = va + count*PAGE_SIZE;

	while (va < end_va) {
		unsigned *pte;

		pte = (unsigned *)vtopte(va);
		*pte = 0;
		tlbie(va);
		va += PAGE_SIZE;
	}
}

/*
 * 	pmap_ts_referenced:
 *
 *	Return the count of reference bits for a page, clearing all of them.
 */
int
pmap_ts_referenced(vm_page_t m)
{

	/* XXX: coming soon... */
	return (0);
}

/*
 * this routine returns true if a physical page resides
 * in the given pmap.
 */
boolean_t
pmap_page_exists(pmap_t pmap, vm_page_t m)
{
#if 0 /* XXX: This must go! */
	register pv_entry_t pv;
	int s;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	s = splvm();

	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	for (pv = pv_table; pv; pv = pv->pv_next) {
		if (pv->pv_pmap == pmap) {
			splx(s);
			return TRUE;
		}
	}
	splx(s);
#endif
	return (FALSE);
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_offset_t start, vm_offset_t end, int prot)
{
	vm_offset_t	sva, va;

	sva = *virt;
	va = sva;

	while (start < end) {
		pmap_kenter(va, start);
		va += PAGE_SIZE;
		start += PAGE_SIZE;
	}

	*virt = va;
	return (sva);
}

vm_offset_t
pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size)
{

	return (addr);
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{

	/* XXX: coming soon... */
	return (0);
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size, int limit)
{

	/* XXX: coming soon... */
	return;
}

void
pmap_growkernel(vm_offset_t addr)
{

	/* XXX: coming soon... */
	return;
}

/*
 * Initialize the address space (zone) for the pv_entries.  Set a
 * high water mark so that the system can recover from excessive
 * numbers of pv entries.
 */
void
pmap_init2()
{
	int shpgperproc = PMAP_SHPGPERPROC;

	TUNABLE_INT_FETCH("vm.pmap.shpgperproc", &shpgperproc);
	pv_entry_max = shpgperproc * maxproc + vm_page_array_size;
	pv_entry_high_water = 9 * (pv_entry_max / 10);
	zinitna(pvzone, &pvzone_obj, NULL, 0, pv_entry_max, ZONE_INTERRUPT, 1);
}

void
pmap_swapin_proc(struct proc *p)
{

	/* XXX: coming soon... */
	return;
}

void
pmap_swapout_proc(struct proc *p)
{

	/* XXX: coming soon... */
	return;
}


/*
 * Create the kernel stack (including pcb for i386) for a new thread.
 * This routine directly affects the fork perf for a process and
 * create performance for a thread.
 */
void
pmap_new_thread(td)
	struct thread *td;
{
	/* XXX: coming soon... */
	return;
}

/*
 * Dispose the kernel stack for a thread that has exited.
 * This routine directly impacts the exit perf of a process and thread.
 */
void
pmap_dispose_thread(td)
	struct thread *td;
{
	/* XXX: coming soon... */
	return;
}

/*
 * Allow the Kernel stack for a thread to be prejudicially paged out.
 */
void
pmap_swapout_thread(td)
	struct thread *td;
{
	int i;
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_lookup(ksobj, i);
		if (m == NULL)
			panic("pmap_swapout_thread: kstack already missing?");
		vm_page_dirty(m);
		vm_page_unwire(m, 0);
		pmap_kremove(ks + i * PAGE_SIZE);
	}
}

/*
 * Bring the kernel stack for a specified thread back in.
 */
void
pmap_swapin_thread(td)
	struct thread *td;
{
	int i, rv;
	vm_object_t ksobj;
	vm_offset_t ks;
	vm_page_t m;

	ksobj = td->td_kstack_obj;
	ks = td->td_kstack;
	for (i = 0; i < KSTACK_PAGES; i++) {
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		pmap_kenter(ks + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));
		if (m->valid != VM_PAGE_BITS_ALL) {
			rv = vm_pager_get_pages(ksobj, &m, 1, 0);
			if (rv != VM_PAGER_OK)
				panic("pmap_swapin_thread: cannot get kstack for proc: %d\n", td->td_proc->p_pid);
			m = vm_page_lookup(ksobj, i);
			m->valid = VM_PAGE_BITS_ALL;
		}
		vm_page_wire(m);
		vm_page_wakeup(m);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
	}
}

void
pmap_pageable(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, boolean_t pageable)
{

	return;
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{

	/* XXX: coming soon... */
	return;
}

void
pmap_prefault(pmap_t pmap, vm_offset_t addra, vm_map_entry_t entry)
{

	/* XXX: coming soon... */
	return;
}

void
pmap_remove_pages(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{

	/* XXX: coming soon... */
	return;
}

void
pmap_pinit0(pmap_t pmap)
{

	/* XXX: coming soon... */
	return;
}

void
pmap_dispose_proc(struct proc *p)
{

	/* XXX: coming soon... */
	return;
}

vm_offset_t
pmap_steal_memory(vm_size_t size)
{
	vm_size_t bank_size;
	vm_offset_t pa;

	size = round_page(size);

	bank_size = phys_avail[1] - phys_avail[0];
	while (size > bank_size) {
		int i;
		for (i = 0; phys_avail[i+2]; i+= 2) {
			phys_avail[i] = phys_avail[i+2];
			phys_avail[i+1] = phys_avail[i+3];
		}
		phys_avail[i] = 0;
		phys_avail[i+1] = 0;
		if (!phys_avail[0])
			panic("pmap_steal_memory: out of memory");
		bank_size = phys_avail[1] - phys_avail[0];
	}

	pa = phys_avail[0];
	phys_avail[0] += size;

	bzero((caddr_t) pa, size);
	return pa;
}

/*
 * Create the UAREA_PAGES for a new process.
 * This routine directly affects the fork perf for a process.
 */
void
pmap_new_proc(struct proc *p)
{
	int		i;
	vm_object_t	upobj;
	vm_offset_t	up;
	vm_page_t	m;
	pte_t		pte;
	sr_t		sr;
	int		idx;
	vm_offset_t	va;

	/*
	 * allocate object for the upages
	 */
	upobj = p->p_upages_obj;
	if (upobj == NULL) {
		upobj = vm_object_allocate(OBJT_DEFAULT, UAREA_PAGES);
		p->p_upages_obj = upobj;
	}

	/* get a kernel virtual address for the UAREA_PAGES for this proc */
	up = (vm_offset_t)p->p_uarea;
	if (up == 0) {
		up = kmem_alloc_nofault(kernel_map, UAREA_PAGES * PAGE_SIZE);
		if (up == 0)
			panic("pmap_new_proc: upage allocation failed");
		p->p_uarea = (struct user *)up;
	}

	for (i = 0; i < UAREA_PAGES; i++) {
		/*
		 * Get a kernel stack page
		 */
		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		/*
		 * Wire the page
		 */
		m->wire_count++;
		cnt.v_wire_count++;

		/*
		 * Enter the page into the kernel address space.
		 */
		va = up + i * PAGE_SIZE;
		idx = pteidx(sr = ptesr(kernel_pmap->pm_sr, va), va);

		pte.pte_hi = ((sr & SR_VSID) << PTE_VSID_SHFT) |
		    ((va & ADDR_PIDX) >> ADDR_API_SHFT);
		pte.pte_lo = (VM_PAGE_TO_PHYS(m) & PTE_RPGN) | PTE_M | PTE_I |
		    PTE_G | PTE_RW;

		if (!pte_insert(idx, &pte)) {
			struct pte_ovfl	*po;

			po = poalloc();
			po->po_pte = pte;
			LIST_INSERT_HEAD(potable + idx, po, po_list);
		}

		tlbie(va);

		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

void *
pmap_mapdev(vm_offset_t pa, vm_size_t len)
{
	vm_offset_t     faddr;
	vm_offset_t     taddr, va;
	int             off;

	faddr = trunc_page(pa);
	off = pa - faddr;
	len = round_page(off + len);

	GIANT_REQUIRED;

	va = taddr = kmem_alloc_pageable(kernel_map, len);

	if (va == 0)
		return NULL;

	for (; len > 0; len -= PAGE_SIZE) {
		pmap_kenter(taddr, faddr);
		faddr += PAGE_SIZE;
		taddr += PAGE_SIZE;
	}

	return (void *)(va + off);
}
