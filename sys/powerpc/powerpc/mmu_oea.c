/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com> of Allegro Networks, Inc.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
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

/*
 * Manages physical address maps.
 *
 * In addition to hardware address maps, this module is called upon to
 * provide software-use-only maps which may or may not be stored in the
 * same form as hardware maps.  These pseudo-maps are used to store
 * intermediate results from copy operations to and from address spaces.
 *
 * Since the information managed by this module is also stored by the
 * logical address mapping module, this module may throw away valid virtual
 * to physical mappings at almost any time.  However, invalidations of
 * mappings must be done as requested.
 *
 * In order to cope with hardware architectures which make virtual to
 * physical map invalidates expensive, this module may delay invalidate
 * reduced protection operations until such time as they are actually
 * necessary.  This module is given full information as to which processors
 * are currently using which maps, and to when physical maps must be made
 * correct.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>

#include <machine/bat.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/sr.h>

#define	PMAP_DEBUG

#define TODO	panic("%s: not implemented", __func__);

#define	PMAP_LOCK(pm)
#define	PMAP_UNLOCK(pm)

#define	TLBIE(va)	__asm __volatile("tlbie %0" :: "r"(va))
#define	TLBSYNC()	__asm __volatile("tlbsync");
#define	SYNC()		__asm __volatile("sync");
#define	EIEIO()		__asm __volatile("eieio");

#define	VSID_MAKE(sr, hash)	((sr) | (((hash) & 0xfffff) << 4))
#define	VSID_TO_SR(vsid)	((vsid) & 0xf)
#define	VSID_TO_HASH(vsid)	(((vsid) >> 4) & 0xfffff)

#define	PVO_PTEGIDX_MASK	0x0007		/* which PTEG slot */
#define	PVO_PTEGIDX_VALID	0x0008		/* slot is valid */
#define	PVO_WIRED		0x0010		/* PVO entry is wired */
#define	PVO_MANAGED		0x0020		/* PVO entry is managed */
#define	PVO_EXECUTABLE		0x0040		/* PVO entry is executable */
#define	PVO_BOOTSTRAP		0x0004		/* PVO entry allocated during
						   bootstrap */
#define	PVO_VADDR(pvo)		((pvo)->pvo_vaddr & ~ADDR_POFF)
#define	PVO_ISEXECUTABLE(pvo)	((pvo)->pvo_vaddr & PVO_EXECUTABLE)
#define	PVO_PTEGIDX_GET(pvo)	((pvo)->pvo_vaddr & PVO_PTEGIDX_MASK)
#define	PVO_PTEGIDX_ISSET(pvo)	((pvo)->pvo_vaddr & PVO_PTEGIDX_VALID)
#define	PVO_PTEGIDX_CLR(pvo)	\
	((void)((pvo)->pvo_vaddr &= ~(PVO_PTEGIDX_VALID|PVO_PTEGIDX_MASK)))
#define	PVO_PTEGIDX_SET(pvo, i)	\
	((void)((pvo)->pvo_vaddr |= (i)|PVO_PTEGIDX_VALID))

#define	PMAP_PVO_CHECK(pvo)

struct mem_region {
	vm_offset_t	mr_start;
	vm_offset_t	mr_size;
};

struct ofw_map {
	vm_offset_t	om_va;
	vm_size_t	om_len;
	vm_offset_t	om_pa;
	u_int		om_mode;
};

int	pmap_bootstrapped = 0;

/*
 * Virtual and physical address of message buffer.
 */
struct		msgbuf *msgbufp;
vm_offset_t	msgbuf_phys;

/*
 * Physical addresses of first and last available physical page.
 */
vm_offset_t avail_start;
vm_offset_t avail_end;

/*
 * Map of physical memory regions.
 */
vm_offset_t	phys_avail[128];
u_int		phys_avail_count;
static struct	mem_region regions[128];
static struct	ofw_map translations[128];
static int	translations_size;

/*
 * First and last available kernel virtual addresses.
 */
vm_offset_t virtual_avail;
vm_offset_t virtual_end;
vm_offset_t kernel_vm_end;

/*
 * Kernel pmap.
 */
struct pmap kernel_pmap_store;
extern struct pmap ofw_pmap;

/*
 * PTEG data.
 */
static struct	pteg *pmap_pteg_table;
u_int		pmap_pteg_count;
u_int		pmap_pteg_mask;

/*
 * PVO data.
 */
struct	pvo_head *pmap_pvo_table;		/* pvo entries by pteg index */
struct	pvo_head pmap_pvo_kunmanaged =
    LIST_HEAD_INITIALIZER(pmap_pvo_kunmanaged);	/* list of unmanaged pages */
struct	pvo_head pmap_pvo_unmanaged =
    LIST_HEAD_INITIALIZER(pmap_pvo_unmanaged);	/* list of unmanaged pages */

vm_zone_t	pmap_upvo_zone;	/* zone for pvo entries for unmanaged pages */
vm_zone_t	pmap_mpvo_zone;	/* zone for pvo entries for managed pages */
struct		vm_object pmap_upvo_zone_obj;
struct		vm_object pmap_mpvo_zone_obj;
static vm_object_t	pmap_pvo_obj;
static u_int		pmap_pvo_count;

#define	PMAP_PVO_SIZE	1024
static struct	pvo_entry *pmap_bpvo_pool;
static int	pmap_bpvo_pool_index;
static int	pmap_bpvo_pool_count;

#define	VSID_NBPW	(sizeof(u_int32_t) * 8)
static u_int	pmap_vsid_bitmap[NPMAPS / VSID_NBPW];

static boolean_t pmap_initialized = FALSE;

/*
 * Statistics.
 */
u_int	pmap_pte_valid = 0;
u_int	pmap_pte_overflow = 0;
u_int	pmap_pte_replacements = 0;
u_int	pmap_pvo_entries = 0;
u_int	pmap_pvo_enter_calls = 0;
u_int	pmap_pvo_remove_calls = 0;
u_int	pmap_pte_spills = 0;
SYSCTL_INT(_machdep, OID_AUTO, pmap_pte_valid, CTLFLAG_RD, &pmap_pte_valid,
    0, "");
SYSCTL_INT(_machdep, OID_AUTO, pmap_pte_overflow, CTLFLAG_RD,
    &pmap_pte_overflow, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, pmap_pte_replacements, CTLFLAG_RD,
    &pmap_pte_replacements, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, pmap_pvo_entries, CTLFLAG_RD, &pmap_pvo_entries,
    0, "");
SYSCTL_INT(_machdep, OID_AUTO, pmap_pvo_enter_calls, CTLFLAG_RD,
    &pmap_pvo_enter_calls, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, pmap_pvo_remove_calls, CTLFLAG_RD,
    &pmap_pvo_remove_calls, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, pmap_pte_spills, CTLFLAG_RD,
    &pmap_pte_spills, 0, "");

struct	pvo_entry *pmap_pvo_zeropage;

vm_offset_t	pmap_rkva_start = VM_MIN_KERNEL_ADDRESS;
u_int		pmap_rkva_count = 4;

/*
 * Allocate physical memory for use in pmap_bootstrap.
 */
static vm_offset_t	pmap_bootstrap_alloc(vm_size_t, u_int);

/*
 * PTE calls.
 */
static int		pmap_pte_insert(u_int, struct pte *);

/*
 * PVO calls.
 */
static int	pmap_pvo_enter(pmap_t, vm_zone_t, struct pvo_head *,
		    vm_offset_t, vm_offset_t, u_int, int);
static void	pmap_pvo_remove(struct pvo_entry *, int);
static struct	pvo_entry *pmap_pvo_find_va(pmap_t, vm_offset_t, int *);
static struct	pte *pmap_pvo_to_pte(const struct pvo_entry *, int);

/*
 * Utility routines.
 */
static void *		pmap_pvo_allocf(uma_zone_t, int, u_int8_t *, int);
static struct		pvo_entry *pmap_rkva_alloc(void);
static void		pmap_pa_map(struct pvo_entry *, vm_offset_t,
			    struct pte *, int *);
static void		pmap_pa_unmap(struct pvo_entry *, struct pte *, int *);
static void		pmap_syncicache(vm_offset_t, vm_size_t);
static boolean_t	pmap_query_bit(vm_page_t, int);
static boolean_t	pmap_clear_bit(vm_page_t, int);
static void		tlbia(void);

static __inline int
va_to_sr(u_int *sr, vm_offset_t va)
{
	return (sr[(uintptr_t)va >> ADDR_SR_SHFT]);
}

static __inline u_int
va_to_pteg(u_int sr, vm_offset_t addr)
{
	u_int hash;

	hash = (sr & SR_VSID_MASK) ^ (((u_int)addr & ADDR_PIDX) >>
	    ADDR_PIDX_SHFT);
	return (hash & pmap_pteg_mask);
}

static __inline struct pvo_head *
pa_to_pvoh(vm_offset_t pa)
{
	struct	vm_page *pg;

	pg = PHYS_TO_VM_PAGE(pa);

	if (pg == NULL)
		return (&pmap_pvo_unmanaged);

	return (&pg->md.mdpg_pvoh);
}

static __inline struct pvo_head *
vm_page_to_pvoh(vm_page_t m)
{

	return (&m->md.mdpg_pvoh);
}

static __inline void
pmap_attr_clear(vm_page_t m, int ptebit)
{

	m->md.mdpg_attrs &= ~ptebit;
}

static __inline int
pmap_attr_fetch(vm_page_t m)
{

	return (m->md.mdpg_attrs);
}

static __inline void
pmap_attr_save(vm_page_t m, int ptebit)
{

	m->md.mdpg_attrs |= ptebit;
}

static __inline int
pmap_pte_compare(const struct pte *pt, const struct pte *pvo_pt)
{
	if (pt->pte_hi == pvo_pt->pte_hi)
		return (1);

	return (0);
}

static __inline int
pmap_pte_match(struct pte *pt, u_int sr, vm_offset_t va, int which)
{
	return (pt->pte_hi & ~PTE_VALID) ==
	    (((sr & SR_VSID_MASK) << PTE_VSID_SHFT) |
	    ((va >> ADDR_API_SHFT) & PTE_API) | which);
}

static __inline void
pmap_pte_create(struct pte *pt, u_int sr, vm_offset_t va, u_int pte_lo)
{
	/*
	 * Construct a PTE.  Default to IMB initially.  Valid bit only gets
	 * set when the real pte is set in memory.
	 *
	 * Note: Don't set the valid bit for correct operation of tlb update.
	 */
	pt->pte_hi = ((sr & SR_VSID_MASK) << PTE_VSID_SHFT) |
	    (((va & ADDR_PIDX) >> ADDR_API_SHFT) & PTE_API);
	pt->pte_lo = pte_lo;
}

static __inline void
pmap_pte_synch(struct pte *pt, struct pte *pvo_pt)
{

	pvo_pt->pte_lo |= pt->pte_lo & (PTE_REF | PTE_CHG);
}

static __inline void
pmap_pte_clear(struct pte *pt, vm_offset_t va, int ptebit)
{

	/*
	 * As shown in Section 7.6.3.2.3
	 */
	pt->pte_lo &= ~ptebit;
	TLBIE(va);
	EIEIO();
	TLBSYNC();
	SYNC();
}

static __inline void
pmap_pte_set(struct pte *pt, struct pte *pvo_pt)
{

	pvo_pt->pte_hi |= PTE_VALID;

	/*
	 * Update the PTE as defined in section 7.6.3.1.
	 * Note that the REF/CHG bits are from pvo_pt and thus should havce
	 * been saved so this routine can restore them (if desired).
	 */
	pt->pte_lo = pvo_pt->pte_lo;
	EIEIO();
	pt->pte_hi = pvo_pt->pte_hi;
	SYNC();
	pmap_pte_valid++;
}

static __inline void
pmap_pte_unset(struct pte *pt, struct pte *pvo_pt, vm_offset_t va)
{

	pvo_pt->pte_hi &= ~PTE_VALID;

	/*
	 * Force the reg & chg bits back into the PTEs.
	 */
	SYNC();

	/*
	 * Invalidate the pte.
	 */
	pt->pte_hi &= ~PTE_VALID;

	SYNC();
	TLBIE(va);
	EIEIO();
	TLBSYNC();
	SYNC();

	/*
	 * Save the reg & chg bits.
	 */
	pmap_pte_synch(pt, pvo_pt);
	pmap_pte_valid--;
}

static __inline void
pmap_pte_change(struct pte *pt, struct pte *pvo_pt, vm_offset_t va)
{

	/*
	 * Invalidate the PTE
	 */
	pmap_pte_unset(pt, pvo_pt, va);
	pmap_pte_set(pt, pvo_pt);
}

/*
 * Quick sort callout for comparing memory regions.
 */
static int	mr_cmp(const void *a, const void *b);
static int	om_cmp(const void *a, const void *b);

static int
mr_cmp(const void *a, const void *b)
{
	const struct	mem_region *regiona;
	const struct	mem_region *regionb;

	regiona = a;
	regionb = b;
	if (regiona->mr_start < regionb->mr_start)
		return (-1);
	else if (regiona->mr_start > regionb->mr_start)
		return (1);
	else
		return (0);
}

static int
om_cmp(const void *a, const void *b)
{
	const struct	ofw_map *mapa;
	const struct	ofw_map *mapb;

	mapa = a;
	mapb = b;
	if (mapa->om_pa < mapb->om_pa)
		return (-1);
	else if (mapa->om_pa > mapb->om_pa)
		return (1);
	else
		return (0);
}

void
pmap_bootstrap(vm_offset_t kernelstart, vm_offset_t kernelend)
{
	ihandle_t	pmem, mmui;
	phandle_t	chosen, mmu;
	int		sz;
	int		i, j;
	vm_size_t	size, physsz;
	vm_offset_t	pa, va, off;
	u_int		batl, batu;

	/*
	 * Use an IBAT and a DBAT to map the bottom segment of memory
	 * where we are.
	 */
	batu = BATU(0x00000000, BAT_BL_256M, BAT_Vs);
	batl = BATL(0x00000000, BAT_M, BAT_PP_RW);
	__asm ("mtibatu 0,%0; mtibatl 0,%1; mtdbatu 0,%0; mtdbatl 0,%1"
	    :: "r"(batu), "r"(batl));
#if 0
	batu = BATU(0x80000000, BAT_BL_256M, BAT_Vs);
	batl = BATL(0x80000000, BAT_M, BAT_PP_RW);
	__asm ("mtibatu 1,%0; mtibatl 1,%1; mtdbatu 1,%0; mtdbatl 1,%1"
	    :: "r"(batu), "r"(batl));
#endif

	/*
	 * Set the start and end of kva.
	 */
	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	if ((pmem = OF_finddevice("/memory")) == -1)
		panic("pmap_bootstrap: can't locate memory device");
	if ((sz = OF_getproplen(pmem, "available")) == -1)
		panic("pmap_bootstrap: can't get length of available memory");
	if (sizeof(phys_avail) < sz)
		panic("pmap_bootstrap: phys_avail too small");
	if (sizeof(regions) < sz)
		panic("pmap_bootstrap: regions too small");
	bzero(regions, sz);
	if (OF_getprop(pmem, "available", regions, sz) == -1)
		panic("pmap_bootstrap: can't get available memory");
	sz /= sizeof(*regions);
	CTR0(KTR_PMAP, "pmap_bootstrap: physical memory");
	qsort(regions, sz, sizeof(*regions), mr_cmp);
	phys_avail_count = 0;
	physsz = 0;
	for (i = 0, j = 0; i < sz; i++, j += 2) {
		CTR3(KTR_PMAP, "region: %#x - %#x (%#x)", regions[i].mr_start,
		    regions[i].mr_start + regions[i].mr_size,
		    regions[i].mr_size);
		phys_avail[j] = regions[i].mr_start;
		phys_avail[j + 1] = regions[i].mr_start + regions[i].mr_size;
		phys_avail_count++;
		physsz += regions[i].mr_size;
	}
	physmem = btoc(physsz);

	/*
	 * Allocate PTEG table.
	 */
#ifdef PTEGCOUNT
	pmap_pteg_count = PTEGCOUNT;
#else
	pmap_pteg_count = 0x1000;

	while (pmap_pteg_count < physmem)
		pmap_pteg_count <<= 1;

	pmap_pteg_count >>= 1;
#endif /* PTEGCOUNT */

	size = pmap_pteg_count * sizeof(struct pteg);
	CTR2(KTR_PMAP, "pmap_bootstrap: %d PTEGs, %d bytes", pmap_pteg_count,
	    size);
	pmap_pteg_table = (struct pteg *)pmap_bootstrap_alloc(size, size);
	CTR1(KTR_PMAP, "pmap_bootstrap: PTEG table at %p", pmap_pteg_table);
	bzero((void *)pmap_pteg_table, pmap_pteg_count * sizeof(struct pteg));
	pmap_pteg_mask = pmap_pteg_count - 1;

	/*
	 * Allocate PTE overflow lists.
	 */
	size = sizeof(struct pvo_head) * pmap_pteg_count;
	pmap_pvo_table = (struct pvo_head *)pmap_bootstrap_alloc(size,
	    PAGE_SIZE);
	CTR1(KTR_PMAP, "pmap_bootstrap: PVO table at %p", pmap_pvo_table);
	for (i = 0; i < pmap_pteg_count; i++)
		LIST_INIT(&pmap_pvo_table[i]);

	/*
	 * Allocate the message buffer.
	 */
	msgbuf_phys = pmap_bootstrap_alloc(MSGBUF_SIZE, 0);

	/*
	 * Initialise the unmanaged pvo pool.
	 */
	pmap_bpvo_pool = (struct pvo_entry *)pmap_bootstrap_alloc(PAGE_SIZE, 0);
	pmap_bpvo_pool_index = 0;
	pmap_bpvo_pool_count = (int)PAGE_SIZE / sizeof(struct pvo_entry);

	/*
	 * Make sure kernel vsid is allocated as well as VSID 0.
	 */
	pmap_vsid_bitmap[(KERNEL_VSIDBITS & (NPMAPS - 1)) / VSID_NBPW]
		|= 1 << (KERNEL_VSIDBITS % VSID_NBPW);
	pmap_vsid_bitmap[0] |= 1;

	/*
	 * Set up the OpenFirmware pmap and add it's mappings.
	 */
	pmap_pinit(&ofw_pmap);
	ofw_pmap.pm_sr[KERNEL_SR] = KERNEL_SEGMENT;
	if ((chosen = OF_finddevice("/chosen")) == -1)
		panic("pmap_bootstrap: can't find /chosen");
	OF_getprop(chosen, "mmu", &mmui, 4);
	if ((mmu = OF_instance_to_package(mmui)) == -1)
		panic("pmap_bootstrap: can't get mmu package");
	if ((sz = OF_getproplen(mmu, "translations")) == -1)
		panic("pmap_bootstrap: can't get ofw translation count");
	if (sizeof(translations) < sz)
		panic("pmap_bootstrap: translations too small");
	bzero(translations, sz);
	if (OF_getprop(mmu, "translations", translations, sz) == -1)
		panic("pmap_bootstrap: can't get ofw translations");
	CTR0(KTR_PMAP, "pmap_bootstrap: translations");
	qsort(translations, sz, sizeof (*translations), om_cmp);
	for (i = 0; i < sz; i++) {
		CTR3(KTR_PMAP, "translation: pa=%#x va=%#x len=%#x",
		    translations[i].om_pa, translations[i].om_va,
		    translations[i].om_len);

		/* Drop stuff below something? */

		/* Enter the pages? */
		for (off = 0; off < translations[i].om_len; off += PAGE_SIZE) {
			struct	vm_page m;

			m.phys_addr = translations[i].om_pa + off;
			pmap_enter(&ofw_pmap, translations[i].om_va + off, &m,
			    VM_PROT_ALL, 1);
		}
	}
#ifdef SMP
	TLBSYNC();
#endif

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	for (i = 0; i < 16; i++) {
		kernel_pmap->pm_sr[i] = EMPTY_SEGMENT;
	}
	kernel_pmap->pm_sr[KERNEL_SR] = KERNEL_SEGMENT;
	kernel_pmap->pm_active = ~0;
	kernel_pmap->pm_count = 1;

	/*
	 * Allocate a kernel stack with a guard page for thread0 and map it
	 * into the kernel page map.
	 */
	pa = pmap_bootstrap_alloc(KSTACK_PAGES * PAGE_SIZE, 0);
	kstack0_phys = pa;
	kstack0 = virtual_avail + (KSTACK_GUARD_PAGES * PAGE_SIZE);
	CTR2(KTR_PMAP, "pmap_bootstrap: kstack0 at %#x (%#x)", kstack0_phys,
	    kstack0);
	virtual_avail += (KSTACK_PAGES + KSTACK_GUARD_PAGES) * PAGE_SIZE;
	for (i = 0; i < KSTACK_PAGES; i++) {
		pa = kstack0_phys + i * PAGE_SIZE;
		va = kstack0 + i * PAGE_SIZE;
		pmap_kenter(va, pa);
		TLBIE(va);
	}

	/*
	 * Calculate the first and last available physical addresses.
	 */
	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i + 2] != 0; i += 2)
		;
	avail_end = phys_avail[i + 1];
	Maxmem = powerpc_btop(avail_end);

	/*
	 * Allocate virtual address space for the message buffer.
	 */
	msgbufp = (struct msgbuf *)virtual_avail;
	virtual_avail += round_page(MSGBUF_SIZE);

	/*
	 * Initialize hardware.
	 */
	for (i = 0; i < 16; i++) {
		__asm __volatile("mtsrin %0,%1"
		    :: "r"(EMPTY_SEGMENT), "r"(i << ADDR_SR_SHFT));
	}
	__asm __volatile ("mtsr %0,%1"
	    :: "n"(KERNEL_SR), "r"(KERNEL_SEGMENT));
	__asm __volatile ("sync; mtsdr1 %0; isync"
	    :: "r"((u_int)pmap_pteg_table | (pmap_pteg_mask >> 10)));
	tlbia();

	pmap_bootstrapped++;
}

/*
 * Activate a user pmap.  The pmap must be activated before it's address
 * space can be accessed in any way.
 */
void
pmap_activate(struct thread *td)
{
	pmap_t	pm;
	int	i;

	/*
	 * Load all the data we need up front to encourasge the compiler to
	 * not issue any loads while we have interrupts disabled below.
	 */
	pm = &td->td_proc->p_vmspace->vm_pmap;

	KASSERT(pm->pm_active == 0, ("pmap_activate: pmap already active?"));

	pm->pm_active |= PCPU_GET(cpumask);

	/*
	 * XXX: Address this again later?
	 * NetBSD only change the segment registers on return to userland.
	 */
#if 0
	critical_enter();

	for (i = 0; i < 16; i++) {
		__asm __volatile("mtsr %0,%1" :: "r"(i), "r"(pm->pm_sr[i]));
	}
	__asm __volatile("sync; isync");

	critical_exit();
#endif
}

void
pmap_deactivate(struct thread *td)
{
	pmap_t	pm;

	pm = &td->td_proc->p_vmspace->vm_pmap;
	pm->pm_active &= ~(PCPU_GET(cpumask));
}

vm_offset_t
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{
	TODO;
	return (0);
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	TODO;
}

void
pmap_clear_modify(vm_page_t m)
{

	if (m->flags * PG_FICTITIOUS)
		return;
	pmap_clear_bit(m, PTE_CHG);
}

void
pmap_collect(void)
{
	TODO;
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	TODO;
}

void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{
	TODO;
}

/*
 * Zero a page of physical memory by temporarily mapping it into the tlb.
 */
void
pmap_zero_page(vm_offset_t pa)
{
	caddr_t	va;
	int	i;

	if (pa < SEGMENT_LENGTH) {
		va = (caddr_t) pa;
	} else if (pmap_initialized) {
		if (pmap_pvo_zeropage == NULL)
			pmap_pvo_zeropage = pmap_rkva_alloc();
		pmap_pa_map(pmap_pvo_zeropage, pa, NULL, NULL);
		va = (caddr_t)PVO_VADDR(pmap_pvo_zeropage);
	} else {
		panic("pmap_zero_page: can't zero pa %#x", pa);
	}

	bzero(va, PAGE_SIZE);

	for (i = PAGE_SIZE / CACHELINESIZE; i > 0; i--) {
		__asm __volatile("dcbz 0,%0" :: "r"(va));
		va += CACHELINESIZE;
	}

	if (pa >= SEGMENT_LENGTH)
		pmap_pa_unmap(pmap_pvo_zeropage, NULL, NULL);
}

void
pmap_zero_page_area(vm_offset_t pa, int off, int size)
{
	TODO;
}

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 */
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
	   boolean_t wired)
{
	struct		pvo_head *pvo_head;
	vm_zone_t	zone;
	u_int		pte_lo, pvo_flags;
	int		error;

	if (!pmap_initialized) {
		pvo_head = &pmap_pvo_kunmanaged;
		zone = pmap_upvo_zone;
		pvo_flags = 0;
	} else {
		pvo_head = pa_to_pvoh(m->phys_addr);
		zone = pmap_mpvo_zone;
		pvo_flags = PVO_MANAGED;
	}

	pte_lo = PTE_I | PTE_G;

	if (prot & VM_PROT_WRITE)
		pte_lo |= PTE_BW;
	else
		pte_lo |= PTE_BR;

	if (prot & VM_PROT_EXECUTE)
		pvo_flags |= PVO_EXECUTABLE;

	if (wired)
		pvo_flags |= PVO_WIRED;

	error = pmap_pvo_enter(pmap, zone, pvo_head, va, m->phys_addr, pte_lo,
	    pvo_flags);

	if (error == ENOENT) {
		/*
		 * Flush the real memory from the cache.
		 */
		if ((pvo_flags & PVO_EXECUTABLE) && (pte_lo & PTE_I) == 0) {
			pmap_syncicache(m->phys_addr, PAGE_SIZE);
		}
	}
}

vm_offset_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	TODO;
	return (0);
}

/*
 * Grow the number of kernel page table entries.  Unneeded.
 */
void
pmap_growkernel(vm_offset_t addr)
{
}

void
pmap_init(vm_offset_t phys_start, vm_offset_t phys_end)
{

	CTR(KTR_PMAP, "pmap_init");
}

void
pmap_init2(void)
{

	CTR(KTR_PMAP, "pmap_init2");

	pmap_pvo_obj = vm_object_allocate(OBJT_PHYS, 16);
	pmap_pvo_count = 0;
	pmap_upvo_zone = zinit("UPVO entry", sizeof (struct pvo_entry),
	    0, 0, 0);
	uma_zone_set_allocf(pmap_upvo_zone, pmap_pvo_allocf);
	pmap_mpvo_zone = zinit("MPVO entry", sizeof(struct pvo_entry),
	    PMAP_PVO_SIZE, ZONE_INTERRUPT, 1);
	uma_zone_set_allocf(pmap_mpvo_zone, pmap_pvo_allocf);
	pmap_initialized = TRUE;
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	TODO;
	return (0);
}

void
pmap_clear_reference(vm_page_t m)
{
	TODO;
}

/*
 *	pmap_ts_referenced:
 *
 *	Return a count of reference bits for a page, clearing those bits.
 *	It is not necessary for every reference bit to be cleared, but it
 *	is necessary that 0 only be returned when there are truly no
 *	reference bits set.
 *
 *	XXX: The exact number of bits to check and clear is a matter that
 *	should be tested and standardized at some point in the future for
 *	optimal aging of shared pages.
 */

int
pmap_ts_referenced(vm_page_t m)
{
	TODO;
	return (0);
}

/*
 * Map a wired page into kernel virtual address space.
 */
void
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	u_int		pte_lo;
	int		error;	
	int		i;

#if 0
	if (va < VM_MIN_KERNEL_ADDRESS)
		panic("pmap_kenter: attempt to enter non-kernel address %#x",
		    va);
#endif

	pte_lo = PTE_I | PTE_G | PTE_BW;
	for (i = 0; phys_avail[i + 2] != 0; i += 2) {
		if (pa >= phys_avail[i] && pa < phys_avail[i + 1]) {
			pte_lo &= ~(PTE_I | PTE_G);
			break;
		}
	}

	error = pmap_pvo_enter(kernel_pmap, pmap_upvo_zone,
	    &pmap_pvo_kunmanaged, va, pa, pte_lo, PVO_WIRED);

	if (error != 0 && error != ENOENT)
		panic("pmap_kenter: failed to enter va %#x pa %#x: %d", va,
		    pa, error);

	/*
	 * Flush the real memory from the instruction cache.
	 */
	if ((pte_lo & (PTE_I | PTE_G)) == 0) {
		pmap_syncicache(pa, PAGE_SIZE);
	}
}

vm_offset_t
pmap_kextract(vm_offset_t va)
{
	TODO;
	return (0);
}

/*
 * Remove a wired page from kernel virtual address space.
 */
void
pmap_kremove(vm_offset_t va)
{

	pmap_remove(kernel_pmap, va, roundup(va, PAGE_SIZE));
}

/*
 * Map a range of physical addresses into kernel virtual address space.
 *
 * The value passed in *virt is a suggested virtual address for the mapping.
 * Architectures which can support a direct-mapped physical to virtual region
 * can return the appropriate address within that region, leaving '*virt'
 * unchanged.  We cannot and therefore do not; *virt is updated with the
 * first usable address after the mapped region.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_offset_t pa_start, vm_offset_t pa_end, int prot)
{
	vm_offset_t	sva, va;

	sva = *virt;
	va = sva;
	for (; pa_start < pa_end; pa_start += PAGE_SIZE, va += PAGE_SIZE)
		pmap_kenter(va, pa_start);
	*virt = va;
	return (sva);
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{
	TODO;
	return (0);
}

/*              
 * Create the uarea for a new process.
 * This routine directly affects the fork perf for a process.
 */
void
pmap_new_proc(struct proc *p)
{
	vm_object_t	upobj;
	vm_offset_t	up;
	vm_page_t	m;
	u_int		i;

	/*
	 * Allocate the object for the upages.
	 */
	upobj = p->p_upages_obj;
	if (upobj == NULL) {
		upobj = vm_object_allocate(OBJT_DEFAULT, UAREA_PAGES);
		p->p_upages_obj = upobj;
	}

	/*
	 * Get a kernel virtual address for the uarea for this process.
	 */
	up = (vm_offset_t)p->p_uarea;
	if (up == 0) {
		up = kmem_alloc_nofault(kernel_map, UAREA_PAGES * PAGE_SIZE);
		if (up == 0)
			panic("pmap_new_proc: upage allocation failed");
		p->p_uarea = (struct user *)up;
	}

	for (i = 0; i < UAREA_PAGES; i++) {
		/*
		 * Get a uarea page.
		 */
		m = vm_page_grab(upobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		/*
		 * Wire the page.
		 */
		m->wire_count++;

		/*
		 * Enter the page into the kernel address space.
		 */
		pmap_kenter(up + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));

		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
		    vm_pindex_t pindex, vm_size_t size, int limit)
{
	TODO;
}

/*
 * Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	struct	pvo_head *pvo_head;
	struct	pvo_entry *pvo, *next_pvo;
	struct	pte *pt;

	/*
	 * Since the routine only downgrades protection, if the
	 * maximal protection is desired, there isn't any change
	 * to be made.
	 */
	if ((prot & (VM_PROT_READ|VM_PROT_WRITE)) ==
	    (VM_PROT_READ|VM_PROT_WRITE))
		return;

	pvo_head = vm_page_to_pvoh(m);
	for (pvo = LIST_FIRST(pvo_head); pvo != NULL; pvo = next_pvo) {
		next_pvo = LIST_NEXT(pvo, pvo_vlink);
		PMAP_PVO_CHECK(pvo);	/* sanity check */

		/*
		 * Downgrading to no mapping at all, we just remove the entry.
		 */
		if ((prot & VM_PROT_READ) == 0) {
			pmap_pvo_remove(pvo, -1);
			continue;
		}

		/*
		 * If EXEC permission is being revoked, just clear the flag
		 * in the PVO.
		 */
		if ((prot & VM_PROT_EXECUTE) == 0)
			pvo->pvo_vaddr &= ~PVO_EXECUTABLE;

		/*
		 * If this entry is already RO, don't diddle with the page
		 * table.
		 */
		if ((pvo->pvo_pte.pte_lo & PTE_PP) == PTE_BR) {
			PMAP_PVO_CHECK(pvo);
			continue;
		}

		/*
		 * Grab the PTE before we diddle the bits so pvo_to_pte can
		 * verify the pte contents are as expected.
		 */
		pt = pmap_pvo_to_pte(pvo, -1);
		pvo->pvo_pte.pte_lo &= ~PTE_PP;
		pvo->pvo_pte.pte_lo |= PTE_BR;
		if (pt != NULL)
			pmap_pte_change(pt, &pvo->pvo_pte, pvo->pvo_vaddr);
		PMAP_PVO_CHECK(pvo);	/* sanity check */
	}
}

/*
 * Make the specified page pageable (or not).  Unneeded.
 */
void
pmap_pageable(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
	      boolean_t pageable)
{
}

/*
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	TODO;
	return (0);
}

static u_int	pmap_vsidcontext;

void
pmap_pinit(pmap_t pmap)
{
	int	i, mask;
	u_int	entropy;

	entropy = 0;
	__asm __volatile("mftb %0" : "=r"(entropy));

	/*
	 * Allocate some segment registers for this pmap.
	 */
	pmap->pm_count = 1;
	for (i = 0; i < NPMAPS; i += VSID_NBPW) {
		u_int	hash, n;

		/*
		 * Create a new value by mutiplying by a prime and adding in
		 * entropy from the timebase register.  This is to make the
		 * VSID more random so that the PT hash function collides
		 * less often.  (Note that the prime casues gcc to do shifts
		 * instead of a multiply.)
		 */
		pmap_vsidcontext = (pmap_vsidcontext * 0x1105) + entropy;
		hash = pmap_vsidcontext & (NPMAPS - 1);
		if (hash == 0)		/* 0 is special, avoid it */
			continue;
		n = hash >> 5;
		mask = 1 << (hash & (VSID_NBPW - 1));
		hash = (pmap_vsidcontext & 0xfffff);
		if (pmap_vsid_bitmap[n] & mask) {	/* collision? */
			/* anything free in this bucket? */
			if (pmap_vsid_bitmap[n] == 0xffffffff) {
				entropy = (pmap_vsidcontext >> 20);
				continue;
			}
			i = ffs(~pmap_vsid_bitmap[i]) - 1;
			mask = 1 << i;
			hash &= 0xfffff & ~(VSID_NBPW - 1);
			hash |= i;
		}
		pmap_vsid_bitmap[n] |= mask;
		for (i = 0; i < 16; i++)
			pmap->pm_sr[i] = VSID_MAKE(i, hash);
		return;
	}

	panic("pmap_pinit: out of segments");
}

/*
 * Initialize the pmap associated with process 0.
 */
void
pmap_pinit0(pmap_t pm)
{

	pmap_pinit(pm);
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

void
pmap_pinit2(pmap_t pmap)
{
	/* XXX: Remove this stub when no longer called */
}

void
pmap_prefault(pmap_t pm, vm_offset_t va, vm_map_entry_t entry)
{
	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("pmap_prefault: non current pmap"));
	/* XXX */
}

void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	TODO;
}

vm_offset_t
pmap_phys_address(int ppn)
{
	TODO;
	return (0);
}

/*
 * Map a list of wired pages into kernel virtual address space.  This is
 * intended for temporary mappings which do not need page modification or
 * references recorded.  Existing mappings in the region are overwritten.
 */
void
pmap_qenter(vm_offset_t va, vm_page_t *m, int count)
{
	int	i;

	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kenter(va, VM_PAGE_TO_PHYS(m[i]));
}

/*
 * Remove page mappings from kernel virtual address space.  Intended for
 * temporary mappings entered by pmap_qenter.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	int	i;

	for (i = 0; i < count; i++, va += PAGE_SIZE)
		pmap_kremove(va);
}

/*
 * Add a reference to the specified pmap.
 */
void
pmap_reference(pmap_t pm)
{

	if (pm != NULL)
		pm->pm_count++;
}

void
pmap_release(pmap_t pmap)
{
	TODO;
}

/*
 * Remove the given range of addresses from the specified map.
 */
void
pmap_remove(pmap_t pm, vm_offset_t sva, vm_offset_t eva)
{
	struct	pvo_entry *pvo;
	int	pteidx;

	for (; sva < eva; sva += PAGE_SIZE) {
		pvo = pmap_pvo_find_va(pm, sva, &pteidx);
		if (pvo != NULL) {
			pmap_pvo_remove(pvo, pteidx);
		}
	}
}

void
pmap_remove_pages(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	TODO;
}

void
pmap_swapin_proc(struct proc *p)
{
	TODO;
}

void
pmap_swapout_proc(struct proc *p)
{
	TODO;
}

/*
 * Create the kernel stack and pcb for a new thread.
 * This routine directly affects the fork perf for a process and
 * create performance for a thread.
 */
void
pmap_new_thread(struct thread *td)
{
	vm_object_t	ksobj;
	vm_offset_t	ks;
	vm_page_t	m;
	u_int		i;

	/*
	 * Allocate object for the kstack.
	 */
	ksobj = td->td_kstack_obj;
	if (ksobj == NULL) {
		ksobj = vm_object_allocate(OBJT_DEFAULT, KSTACK_PAGES);
		td->td_kstack_obj = ksobj;
	}

	/*
	 * Get a kernel virtual address for the kstack for this thread.
	 */
	ks = td->td_kstack;
	if (ks == 0) {
		ks = kmem_alloc_nofault(kernel_map,
		    (KSTACK_PAGES + KSTACK_GUARD_PAGES) * PAGE_SIZE);
		if (ks == 0)
			panic("pmap_new_thread: kstack allocation failed");
		TLBIE(ks);
		ks += KSTACK_GUARD_PAGES * PAGE_SIZE;
		td->td_kstack = ks;
	}

	for (i = 0; i < KSTACK_PAGES; i++) {
		/*
		 * Get a kernel stack page.
		 */
		m = vm_page_grab(ksobj, i, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

		/*
		 * Wire the page.
		 */
		m->wire_count++;

		/*
		 * Enter the page into the kernel address space.
		 */
		pmap_kenter(ks + i * PAGE_SIZE, VM_PAGE_TO_PHYS(m));

		vm_page_wakeup(m);
		vm_page_flag_clear(m, PG_ZERO);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE);
		m->valid = VM_PAGE_BITS_ALL;
	}
}

void
pmap_dispose_proc(struct proc *p)
{
	TODO;
}

void
pmap_dispose_thread(struct thread *td)
{
	TODO;
}

void
pmap_swapin_thread(struct thread *td)
{
	TODO;
}

void
pmap_swapout_thread(struct thread *td)
{
	TODO;
}

/*
 * Allocate a physical page of memory directly from the phys_avail map.
 * Can only be called from pmap_bootstrap before avail start and end are
 * calculated.
 */
static vm_offset_t
pmap_bootstrap_alloc(vm_size_t size, u_int align)
{
	vm_offset_t	s, e;
	int		i, j;

	size = round_page(size);
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		if (align != 0)
			s = (phys_avail[i] + align - 1) & ~(align - 1);
		else
			s = phys_avail[i];
		e = s + size;

		if (s < phys_avail[i] || e > phys_avail[i + 1])
			continue;

		if (s == phys_avail[i]) {
			phys_avail[i] += size;
		} else if (e == phys_avail[i + 1]) {
			phys_avail[i + 1] -= size;
		} else {
			for (j = phys_avail_count * 2; j > i; j -= 2) {
				phys_avail[j] = phys_avail[j - 2];
				phys_avail[j + 1] = phys_avail[j - 1];
			}

			phys_avail[i + 3] = phys_avail[i + 1];
			phys_avail[i + 1] = s;
			phys_avail[i + 2] = e;
			phys_avail_count++;
		}

		return (s);
	}
	panic("pmap_bootstrap_alloc: could not allocate memory");
}

/*
 * Return an unmapped pvo for a kernel virtual address.
 * Used by pmap functions that operate on physical pages.
 */
static struct pvo_entry *
pmap_rkva_alloc(void)
{
	struct		pvo_entry *pvo;
	struct		pte *pt;
	vm_offset_t	kva;
	int		pteidx;

	if (pmap_rkva_count == 0)
		panic("pmap_rkva_alloc: no more reserved KVAs");

	kva = pmap_rkva_start + (PAGE_SIZE * --pmap_rkva_count);
	pmap_kenter(kva, 0);

	pvo = pmap_pvo_find_va(kernel_pmap, kva, &pteidx);

	if (pvo == NULL)
		panic("pmap_kva_alloc: pmap_pvo_find_va failed");

	pt = pmap_pvo_to_pte(pvo, pteidx);

	if (pt == NULL)
		panic("pmap_kva_alloc: pmap_pvo_to_pte failed");

	pmap_pte_unset(pt, &pvo->pvo_pte, pvo->pvo_vaddr);
	PVO_PTEGIDX_CLR(pvo);

	pmap_pte_overflow++;

	return (pvo);
}

static void
pmap_pa_map(struct pvo_entry *pvo, vm_offset_t pa, struct pte *saved_pt,
    int *depth_p)
{
	struct	pte *pt;

	/*
	 * If this pvo already has a valid pte, we need to save it so it can
	 * be restored later.  We then just reload the new PTE over the old
	 * slot.
	 */
	if (saved_pt != NULL) {
		pt = pmap_pvo_to_pte(pvo, -1);

		if (pt != NULL) {
			pmap_pte_unset(pt, &pvo->pvo_pte, pvo->pvo_vaddr);
			PVO_PTEGIDX_CLR(pvo);
			pmap_pte_overflow++;
		}

		*saved_pt = pvo->pvo_pte;

		pvo->pvo_pte.pte_lo &= ~PTE_RPGN;
	}

	pvo->pvo_pte.pte_lo |= pa;

	if (!pmap_pte_spill(pvo->pvo_vaddr))
		panic("pmap_pa_map: could not spill pvo %p", pvo);

	if (depth_p != NULL)
		(*depth_p)++;
}

static void
pmap_pa_unmap(struct pvo_entry *pvo, struct pte *saved_pt, int *depth_p)
{
	struct	pte *pt;

	pt = pmap_pvo_to_pte(pvo, -1);

	if (pt != NULL) {
		pmap_pte_unset(pt, &pvo->pvo_pte, pvo->pvo_vaddr);
		PVO_PTEGIDX_CLR(pvo);
		pmap_pte_overflow++;
	}

	pvo->pvo_pte.pte_lo &= ~PTE_RPGN;

	/*
	 * If there is a saved PTE and it's valid, restore it and return.
	 */
	if (saved_pt != NULL && (saved_pt->pte_lo & PTE_RPGN) != 0) {
		if (depth_p != NULL && --(*depth_p) == 0)
			panic("pmap_pa_unmap: restoring but depth == 0");

		pvo->pvo_pte = *saved_pt;

		if (!pmap_pte_spill(pvo->pvo_vaddr))
			panic("pmap_pa_unmap: could not spill pvo %p", pvo);
	}
}

static void
pmap_syncicache(vm_offset_t pa, vm_size_t len)
{
	__syncicache((void *)pa, len);
}

static void
tlbia(void)
{
	caddr_t	i;

	SYNC();
	for (i = 0; i < (caddr_t)0x00040000; i += 0x00001000) {
		TLBIE(i);
		EIEIO();
	}
	TLBSYNC();
	SYNC();
}

static int
pmap_pvo_enter(pmap_t pm, vm_zone_t zone, struct pvo_head *pvo_head,
    vm_offset_t va, vm_offset_t pa, u_int pte_lo, int flags)
{
	struct	pvo_entry *pvo;
	u_int	sr;
	int	first;
	u_int	ptegidx;
	int	i;

	pmap_pvo_enter_calls++;

	/*
	 * Compute the PTE Group index.
	 */
	va &= ~ADDR_POFF;
	sr = va_to_sr(pm->pm_sr, va);
	ptegidx = va_to_pteg(sr, va);

	/*
	 * Remove any existing mapping for this page.  Reuse the pvo entry if
	 * there is a mapping.
	 */
	LIST_FOREACH(pvo, &pmap_pvo_table[ptegidx], pvo_olink) {
		if (pvo->pvo_pmap == pm && PVO_VADDR(pvo) == va) {
			if ((pvo->pvo_pte.pte_lo & PTE_RPGN) == pa)
				return (0);
			pmap_pvo_remove(pvo, -1);
			break;
		}
	}

	/*
	 * If we aren't overwriting a mapping, try to allocate.
	 */
	if (pmap_initialized) {
		pvo = zalloc(zone);
	} else {
		if (pmap_bpvo_pool_index >= pmap_bpvo_pool_count) {
			pmap_bpvo_pool = (struct pvo_entry *)
			    pmap_bootstrap_alloc(PAGE_SIZE, 0);
			pmap_bpvo_pool_index = 0;
		}
		pvo = &pmap_bpvo_pool[pmap_bpvo_pool_index];
		pmap_bpvo_pool_index++;
		pvo->pvo_vaddr |= PVO_BOOTSTRAP;
	}

	if (pvo == NULL) {
		return (ENOMEM);
	}

	pmap_pvo_entries++;
	pvo->pvo_vaddr = va;
	pvo->pvo_pmap = pm;
	LIST_INSERT_HEAD(&pmap_pvo_table[ptegidx], pvo, pvo_olink);
	pvo->pvo_vaddr &= ~ADDR_POFF;
	if (flags & VM_PROT_EXECUTE)
		pvo->pvo_vaddr |= PVO_EXECUTABLE;
	if (flags & PVO_WIRED)
		pvo->pvo_vaddr |= PVO_WIRED;
	if (pvo_head != &pmap_pvo_kunmanaged)
		pvo->pvo_vaddr |= PVO_MANAGED;
	pmap_pte_create(&pvo->pvo_pte, sr, va, pa | pte_lo);

	/*
	 * Remember if the list was empty and therefore will be the first
	 * item.
	 */
	first = LIST_FIRST(pvo_head) == NULL;

	LIST_INSERT_HEAD(pvo_head, pvo, pvo_vlink);
	if (pvo->pvo_pte.pte_lo & PVO_WIRED)
		pvo->pvo_pmap->pm_stats.wired_count++;
	pvo->pvo_pmap->pm_stats.resident_count++;

	/*
	 * We hope this succeeds but it isn't required.
	 */
	i = pmap_pte_insert(ptegidx, &pvo->pvo_pte);
	if (i >= 0) {
		PVO_PTEGIDX_SET(pvo, i);
	} else {
		panic("pmap_pvo_enter: overflow");
		pmap_pte_overflow++;
	}

	return (first ? ENOENT : 0);
}

static void
pmap_pvo_remove(struct pvo_entry *pvo, int pteidx)
{
	struct	pte *pt;

	/*
	 * If there is an active pte entry, we need to deactivate it (and
	 * save the ref & cfg bits).
	 */
	pt = pmap_pvo_to_pte(pvo, pteidx);
	if (pt != NULL) {
		pmap_pte_unset(pt, &pvo->pvo_pte, pvo->pvo_vaddr);
		PVO_PTEGIDX_CLR(pvo);
	} else {
		pmap_pte_overflow--;
	}

	/*
	 * Update our statistics.
	 */
	pvo->pvo_pmap->pm_stats.resident_count--;
	if (pvo->pvo_pte.pte_lo & PVO_WIRED)
		pvo->pvo_pmap->pm_stats.wired_count--;

	/*
	 * Save the REF/CHG bits into their cache if the page is managed.
	 */
	if (pvo->pvo_vaddr & PVO_MANAGED) {
		struct	vm_page *pg;

		pg = PHYS_TO_VM_PAGE(pvo->pvo_pte.pte_lo & PTE_RPGN);
		if (pg != NULL) {
			pmap_attr_save(pg, pvo->pvo_pte.pte_lo &
			    (PTE_REF | PTE_CHG));
		}
	}

	/*
	 * Remove this PVO from the PV list.
	 */
	LIST_REMOVE(pvo, pvo_vlink);

	/*
	 * Remove this from the overflow list and return it to the pool
	 * if we aren't going to reuse it.
	 */
	LIST_REMOVE(pvo, pvo_olink);
	if (!(pvo->pvo_vaddr & PVO_BOOTSTRAP))
		zfree(pvo->pvo_vaddr & PVO_MANAGED ? pmap_mpvo_zone :
		    pmap_upvo_zone, pvo);
	pmap_pvo_entries--;
	pmap_pvo_remove_calls++;
}

static __inline int
pmap_pvo_pte_index(const struct pvo_entry *pvo, int ptegidx)
{
	int	pteidx;

	/*
	 * We can find the actual pte entry without searching by grabbing
	 * the PTEG index from 3 unused bits in pte_lo[11:9] and by
	 * noticing the HID bit.
	 */
	pteidx = ptegidx * 8 + PVO_PTEGIDX_GET(pvo);
	if (pvo->pvo_pte.pte_hi & PTE_HID)
		pteidx ^= pmap_pteg_mask * 8;

	return (pteidx);
}

static struct pvo_entry *
pmap_pvo_find_va(pmap_t pm, vm_offset_t va, int *pteidx_p)
{
	struct	pvo_entry *pvo;
	int	ptegidx;
	u_int	sr;

	va &= ~ADDR_POFF;
	sr = va_to_sr(pm->pm_sr, va);
	ptegidx = va_to_pteg(sr, va);

	LIST_FOREACH(pvo, &pmap_pvo_table[ptegidx], pvo_olink) {
		if (pvo->pvo_pmap == pm && PVO_VADDR(pvo) == va) {
			if (pteidx_p)
				*pteidx_p = pmap_pvo_pte_index(pvo, ptegidx);
			return (pvo);
		}
	}

	return (NULL);
}

static struct pte *
pmap_pvo_to_pte(const struct pvo_entry *pvo, int pteidx)
{
	struct	pte *pt;

	/*
	 * If we haven't been supplied the ptegidx, calculate it.
	 */
	if (pteidx == -1) {
		int	ptegidx;
		u_int	sr;

		sr = va_to_sr(pvo->pvo_pmap->pm_sr, pvo->pvo_vaddr);
		ptegidx = va_to_pteg(sr, pvo->pvo_vaddr);
		pteidx = pmap_pvo_pte_index(pvo, ptegidx);
	}

	pt = &pmap_pteg_table[pteidx >> 3].pt[pteidx & 7];

	if ((pvo->pvo_pte.pte_hi & PTE_VALID) && !PVO_PTEGIDX_ISSET(pvo)) {
		panic("pmap_pvo_to_pte: pvo %p has valid pte in pvo but no "
		    "valid pte index", pvo);
	}

	if ((pvo->pvo_pte.pte_hi & PTE_VALID) == 0 && PVO_PTEGIDX_ISSET(pvo)) {
		panic("pmap_pvo_to_pte: pvo %p has valid pte index in pvo "
		    "pvo but no valid pte", pvo);
	}

	if ((pt->pte_hi ^ (pvo->pvo_pte.pte_hi & ~PTE_VALID)) == PTE_VALID) {
		if ((pvo->pvo_pte.pte_hi & PTE_VALID) == 0) {
			panic("pmap_pvo_to_pte: pvo %p has valid pte in "
			    "pmap_pteg_table %p but invalid in pvo", pvo, pt);
		}

		if (((pt->pte_lo ^ pvo->pvo_pte.pte_lo) & ~(PTE_CHG|PTE_REF))
		    != 0) {
			panic("pmap_pvo_to_pte: pvo %p pte does not match "
			    "pte %p in pmap_pteg_table", pvo, pt);
		}

		return (pt);
	}

	if (pvo->pvo_pte.pte_hi & PTE_VALID) {
		panic("pmap_pvo_to_pte: pvo %p has invalid pte %p in "
		    "pmap_pteg_table but valid in pvo", pvo, pt);
	}

	return (NULL);
}

static void *
pmap_pvo_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	vm_page_t	m;

	if (bytes != PAGE_SIZE)
		panic("pmap_pvo_allocf: benno was shortsighted.  hit him.");

	*flags = UMA_SLAB_PRIV;
	m = vm_page_alloc(pmap_pvo_obj, pmap_pvo_count, VM_ALLOC_SYSTEM);
	if (m == NULL)
		return (NULL);
	pmap_pvo_count++;
	return ((void *)VM_PAGE_TO_PHYS(m));
}

/*
 * XXX: THIS STUFF SHOULD BE IN pte.c?
 */
int
pmap_pte_spill(vm_offset_t addr)
{
	struct	pvo_entry *source_pvo, *victim_pvo;
	struct	pvo_entry *pvo;
	int	ptegidx, i, j;
	u_int	sr;
	struct	pteg *pteg;
	struct	pte *pt;

	pmap_pte_spills++;

	__asm __volatile("mfsrin %0,%1" : "=r"(sr) : "r"(addr));
	ptegidx = va_to_pteg(sr, addr);

	/*
	 * Have to substitute some entry.  Use the primary hash for this.
	 * Use low bits of timebase as random generator.
	 */
	pteg = &pmap_pteg_table[ptegidx];
	__asm __volatile("mftb %0" : "=r"(i));
	i &= 7;
	pt = &pteg->pt[i];

	source_pvo = NULL;
	victim_pvo = NULL;
	LIST_FOREACH(pvo, &pmap_pvo_table[ptegidx], pvo_olink) {
		/*
		 * We need to find a pvo entry for this address.
		 */
		PMAP_PVO_CHECK(pvo);
		if (source_pvo == NULL &&
		    pmap_pte_match(&pvo->pvo_pte, sr, addr,
		    pvo->pvo_pte.pte_hi & PTE_HID)) {
			/*
			 * Now found an entry to be spilled into the pteg.
			 * The PTE is now valid, so we know it's active.
			 */
			j = pmap_pte_insert(ptegidx, &pvo->pvo_pte);

			if (j >= 0) {
				PVO_PTEGIDX_SET(pvo, j);
				pmap_pte_overflow--;
				PMAP_PVO_CHECK(pvo);
				return (1);
			}

			source_pvo = pvo;

			if (victim_pvo != NULL)
				break;
		}

		/*
		 * We also need the pvo entry of the victim we are replacing
		 * so save the R & C bits of the PTE.
		 */
		if ((pt->pte_hi & PTE_HID) == 0 && victim_pvo == NULL &&
		    pmap_pte_compare(pt, &pvo->pvo_pte)) {
			victim_pvo = pvo;
			if (source_pvo != NULL)
				break;
		}
	}

	if (source_pvo == NULL)
		return (0);

	if (victim_pvo == NULL) {
		if ((pt->pte_hi & PTE_HID) == 0)
			panic("pmap_pte_spill: victim p-pte (%p) has no pvo"
			    "entry", pt);

		/*
		 * If this is a secondary PTE, we need to search it's primary
		 * pvo bucket for the matching PVO.
		 */
		LIST_FOREACH(pvo, &pmap_pvo_table[ptegidx ^ pmap_pteg_mask],
		    pvo_olink) {
			PMAP_PVO_CHECK(pvo);
			/*
			 * We also need the pvo entry of the victim we are
			 * replacing so save the R & C bits of the PTE.
			 */
			if (pmap_pte_compare(pt, &pvo->pvo_pte)) {
				victim_pvo = pvo;
				break;
			}
		}

		if (victim_pvo == NULL)
			panic("pmap_pte_spill: victim s-pte (%p) has no pvo"
			    "entry", pt);
	}

	/*
	 * We are invalidating the TLB entry for the EA we are replacing even
	 * though it's valid.  If we don't, we lose any ref/chg bit changes
	 * contained in the TLB entry.
	 */
	source_pvo->pvo_pte.pte_hi &= ~PTE_HID;

	pmap_pte_unset(pt, &victim_pvo->pvo_pte, victim_pvo->pvo_vaddr);
	pmap_pte_set(pt, &source_pvo->pvo_pte);

	PVO_PTEGIDX_CLR(victim_pvo);
	PVO_PTEGIDX_SET(source_pvo, i);
	pmap_pte_replacements++;

	PMAP_PVO_CHECK(victim_pvo);
	PMAP_PVO_CHECK(source_pvo);

	return (1);
}

static int
pmap_pte_insert(u_int ptegidx, struct pte *pvo_pt)
{
	struct	pte *pt;
	int	i;

	/*
	 * First try primary hash.
	 */
	for (pt = pmap_pteg_table[ptegidx].pt, i = 0; i < 8; i++, pt++) {
		if ((pt->pte_hi & PTE_VALID) == 0) {
			pvo_pt->pte_hi &= ~PTE_HID;
			pmap_pte_set(pt, pvo_pt);
			return (i);
		}
	}

	/*
	 * Now try secondary hash.
	 */
	ptegidx ^= pmap_pteg_mask;
	ptegidx++;
	for (pt = pmap_pteg_table[ptegidx].pt, i = 0; i < 8; i++, pt++) {
		if ((pt->pte_hi & PTE_VALID) == 0) {
			pvo_pt->pte_hi |= PTE_HID;
			pmap_pte_set(pt, pvo_pt);
			return (i);
		}
	}

	panic("pmap_pte_insert: overflow");
	return (-1);
}

static boolean_t
pmap_query_bit(vm_page_t m, int ptebit)
{
	struct	pvo_entry *pvo;
	struct	pte *pt;

	if (pmap_attr_fetch(m) & ptebit)
		return (TRUE);

	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {
		PMAP_PVO_CHECK(pvo);	/* sanity check */

		/*
		 * See if we saved the bit off.  If so, cache it and return
		 * success.
		 */
		if (pvo->pvo_pte.pte_lo & ptebit) {
			pmap_attr_save(m, ptebit);
			PMAP_PVO_CHECK(pvo);	/* sanity check */
			return (TRUE);
		}
	}

	/*
	 * No luck, now go through the hard part of looking at the PTEs
	 * themselves.  Sync so that any pending REF/CHG bits are flushed to
	 * the PTEs.
	 */
	SYNC();
	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {
		PMAP_PVO_CHECK(pvo);	/* sanity check */

		/*
		 * See if this pvo has a valid PTE.  if so, fetch the
		 * REF/CHG bits from the valid PTE.  If the appropriate
		 * ptebit is set, cache it and return success.
		 */
		pt = pmap_pvo_to_pte(pvo, -1);
		if (pt != NULL) {
			pmap_pte_synch(pt, &pvo->pvo_pte);
			if (pvo->pvo_pte.pte_lo & ptebit) {
				pmap_attr_save(m, ptebit);
				PMAP_PVO_CHECK(pvo);	/* sanity check */
				return (TRUE);
			}
		}
	}

	return (TRUE);
}

static boolean_t
pmap_clear_bit(vm_page_t m, int ptebit)
{
	struct	pvo_entry *pvo;
	struct	pte *pt;
	int	rv;

	/*
	 * Clear the cached value.
	 */
	rv = pmap_attr_fetch(m);
	pmap_attr_clear(m, ptebit);

	/*
	 * Sync so that any pending REF/CHG bits are flushed to the PTEs (so
	 * we can reset the right ones).  note that since the pvo entries and
	 * list heads are accessed via BAT0 and are never placed in the page
	 * table, we don't have to worry about further accesses setting the
	 * REF/CHG bits.
	 */
	SYNC();

	/*
	 * For each pvo entry, clear the pvo's ptebit.  If this pvo has a
	 * valid pte clear the ptebit from the valid pte.
	 */
	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {
		PMAP_PVO_CHECK(pvo);	/* sanity check */
		pt = pmap_pvo_to_pte(pvo, -1);
		if (pt != NULL) {
			pmap_pte_synch(pt, &pvo->pvo_pte);
			if (pvo->pvo_pte.pte_lo & ptebit)
				pmap_pte_clear(pt, PVO_VADDR(pvo), ptebit);
		}
		rv |= pvo->pvo_pte.pte_lo;
		pvo->pvo_pte.pte_lo &= ~ptebit;
		PMAP_PVO_CHECK(pvo);	/* sanity check */
	}

	return ((rv & ptebit) != 0);
}
