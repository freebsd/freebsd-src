/*-
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
 * $NetBSD: pmap.c,v 1.28 2000/03/26 20:42:36 kleink Exp $
 */
/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <sys/kdb.h>

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
#include <vm/uma.h>

#include <machine/_inttypes.h>
#include <machine/cpu.h>
#include <machine/platform.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/bat.h>
#include <machine/hid.h>
#include <machine/pte.h>
#include <machine/sr.h>
#include <machine/trap.h>
#include <machine/mmuvar.h>

#include "mmu_oea64.h"
#include "mmu_if.h"
#include "moea64_if.h"

void moea64_release_vsid(uint64_t vsid);
uintptr_t moea64_get_unique_vsid(void); 

#define DISABLE_TRANS(msr)	msr = mfmsr(); mtmsr(msr & ~PSL_DR)
#define ENABLE_TRANS(msr)	mtmsr(msr)

#define	VSID_MAKE(sr, hash)	((sr) | (((hash) & 0xfffff) << 4))
#define	VSID_TO_HASH(vsid)	(((vsid) >> 4) & 0xfffff)
#define	VSID_HASH_MASK		0x0000007fffffffffULL

#define LOCK_TABLE() mtx_lock(&moea64_table_mutex)
#define UNLOCK_TABLE() mtx_unlock(&moea64_table_mutex);
#define ASSERT_TABLE_LOCK() mtx_assert(&moea64_table_mutex, MA_OWNED)

struct ofw_map {
	cell_t	om_va;
	cell_t	om_len;
	cell_t	om_pa_hi;
	cell_t	om_pa_lo;
	cell_t	om_mode;
};

/*
 * Map of physical memory regions.
 */
static struct	mem_region *regions;
static struct	mem_region *pregions;
static u_int	phys_avail_count;
static int	regions_sz, pregions_sz;

extern void bs_remap_earlyboot(void);

/*
 * Lock for the pteg and pvo tables.
 */
struct mtx	moea64_table_mutex;
struct mtx	moea64_slb_mutex;

/*
 * PTEG data.
 */
u_int		moea64_pteg_count;
u_int		moea64_pteg_mask;

/*
 * PVO data.
 */
struct	pvo_head *moea64_pvo_table;		/* pvo entries by pteg index */
struct	pvo_head moea64_pvo_kunmanaged =	/* list of unmanaged pages */
    LIST_HEAD_INITIALIZER(moea64_pvo_kunmanaged);

uma_zone_t	moea64_upvo_zone; /* zone for pvo entries for unmanaged pages */
uma_zone_t	moea64_mpvo_zone; /* zone for pvo entries for managed pages */

#define	BPVO_POOL_SIZE	327680
static struct	pvo_entry *moea64_bpvo_pool;
static int	moea64_bpvo_pool_index = 0;

#define	VSID_NBPW	(sizeof(u_int32_t) * 8)
#ifdef __powerpc64__
#define	NVSIDS		(NPMAPS * 16)
#define VSID_HASHMASK	0xffffffffUL
#else
#define NVSIDS		NPMAPS
#define VSID_HASHMASK	0xfffffUL
#endif
static u_int	moea64_vsid_bitmap[NVSIDS / VSID_NBPW];

static boolean_t moea64_initialized = FALSE;

/*
 * Statistics.
 */
u_int	moea64_pte_valid = 0;
u_int	moea64_pte_overflow = 0;
u_int	moea64_pvo_entries = 0;
u_int	moea64_pvo_enter_calls = 0;
u_int	moea64_pvo_remove_calls = 0;
SYSCTL_INT(_machdep, OID_AUTO, moea64_pte_valid, CTLFLAG_RD, 
    &moea64_pte_valid, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, moea64_pte_overflow, CTLFLAG_RD,
    &moea64_pte_overflow, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, moea64_pvo_entries, CTLFLAG_RD, 
    &moea64_pvo_entries, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, moea64_pvo_enter_calls, CTLFLAG_RD,
    &moea64_pvo_enter_calls, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, moea64_pvo_remove_calls, CTLFLAG_RD,
    &moea64_pvo_remove_calls, 0, "");

vm_offset_t	moea64_scratchpage_va[2];
struct pvo_entry *moea64_scratchpage_pvo[2];
uintptr_t	moea64_scratchpage_pte[2];
struct	mtx	moea64_scratchpage_mtx;

uint64_t 	moea64_large_page_mask = 0;
int		moea64_large_page_size = 0;
int		moea64_large_page_shift = 0;

/*
 * PVO calls.
 */
static int	moea64_pvo_enter(mmu_t, pmap_t, uma_zone_t, struct pvo_head *,
		    vm_offset_t, vm_offset_t, uint64_t, int);
static void	moea64_pvo_remove(mmu_t, struct pvo_entry *);
static struct	pvo_entry *moea64_pvo_find_va(pmap_t, vm_offset_t);

/*
 * Utility routines.
 */
static void		moea64_enter_locked(mmu_t, pmap_t, vm_offset_t,
			    vm_page_t, vm_prot_t, boolean_t);
static boolean_t	moea64_query_bit(mmu_t, vm_page_t, u_int64_t);
static u_int		moea64_clear_bit(mmu_t, vm_page_t, u_int64_t);
static void		moea64_kremove(mmu_t, vm_offset_t);
static void		moea64_syncicache(mmu_t, pmap_t pmap, vm_offset_t va, 
			    vm_offset_t pa, vm_size_t sz);

/*
 * Kernel MMU interface
 */
void moea64_change_wiring(mmu_t, pmap_t, vm_offset_t, boolean_t);
void moea64_clear_modify(mmu_t, vm_page_t);
void moea64_clear_reference(mmu_t, vm_page_t);
void moea64_copy_page(mmu_t, vm_page_t, vm_page_t);
void moea64_enter(mmu_t, pmap_t, vm_offset_t, vm_page_t, vm_prot_t, boolean_t);
void moea64_enter_object(mmu_t, pmap_t, vm_offset_t, vm_offset_t, vm_page_t,
    vm_prot_t);
void moea64_enter_quick(mmu_t, pmap_t, vm_offset_t, vm_page_t, vm_prot_t);
vm_paddr_t moea64_extract(mmu_t, pmap_t, vm_offset_t);
vm_page_t moea64_extract_and_hold(mmu_t, pmap_t, vm_offset_t, vm_prot_t);
void moea64_init(mmu_t);
boolean_t moea64_is_modified(mmu_t, vm_page_t);
boolean_t moea64_is_prefaultable(mmu_t, pmap_t, vm_offset_t);
boolean_t moea64_is_referenced(mmu_t, vm_page_t);
boolean_t moea64_ts_referenced(mmu_t, vm_page_t);
vm_offset_t moea64_map(mmu_t, vm_offset_t *, vm_offset_t, vm_offset_t, int);
boolean_t moea64_page_exists_quick(mmu_t, pmap_t, vm_page_t);
int moea64_page_wired_mappings(mmu_t, vm_page_t);
void moea64_pinit(mmu_t, pmap_t);
void moea64_pinit0(mmu_t, pmap_t);
void moea64_protect(mmu_t, pmap_t, vm_offset_t, vm_offset_t, vm_prot_t);
void moea64_qenter(mmu_t, vm_offset_t, vm_page_t *, int);
void moea64_qremove(mmu_t, vm_offset_t, int);
void moea64_release(mmu_t, pmap_t);
void moea64_remove(mmu_t, pmap_t, vm_offset_t, vm_offset_t);
void moea64_remove_all(mmu_t, vm_page_t);
void moea64_remove_write(mmu_t, vm_page_t);
void moea64_zero_page(mmu_t, vm_page_t);
void moea64_zero_page_area(mmu_t, vm_page_t, int, int);
void moea64_zero_page_idle(mmu_t, vm_page_t);
void moea64_activate(mmu_t, struct thread *);
void moea64_deactivate(mmu_t, struct thread *);
void *moea64_mapdev(mmu_t, vm_offset_t, vm_size_t);
void *moea64_mapdev_attr(mmu_t, vm_offset_t, vm_size_t, vm_memattr_t);
void moea64_unmapdev(mmu_t, vm_offset_t, vm_size_t);
vm_offset_t moea64_kextract(mmu_t, vm_offset_t);
void moea64_page_set_memattr(mmu_t, vm_page_t m, vm_memattr_t ma);
void moea64_kenter_attr(mmu_t, vm_offset_t, vm_offset_t, vm_memattr_t ma);
void moea64_kenter(mmu_t, vm_offset_t, vm_offset_t);
boolean_t moea64_dev_direct_mapped(mmu_t, vm_offset_t, vm_size_t);
static void moea64_sync_icache(mmu_t, pmap_t, vm_offset_t, vm_size_t);

static mmu_method_t moea64_methods[] = {
	MMUMETHOD(mmu_change_wiring,	moea64_change_wiring),
	MMUMETHOD(mmu_clear_modify,	moea64_clear_modify),
	MMUMETHOD(mmu_clear_reference,	moea64_clear_reference),
	MMUMETHOD(mmu_copy_page,	moea64_copy_page),
	MMUMETHOD(mmu_enter,		moea64_enter),
	MMUMETHOD(mmu_enter_object,	moea64_enter_object),
	MMUMETHOD(mmu_enter_quick,	moea64_enter_quick),
	MMUMETHOD(mmu_extract,		moea64_extract),
	MMUMETHOD(mmu_extract_and_hold,	moea64_extract_and_hold),
	MMUMETHOD(mmu_init,		moea64_init),
	MMUMETHOD(mmu_is_modified,	moea64_is_modified),
	MMUMETHOD(mmu_is_prefaultable,	moea64_is_prefaultable),
	MMUMETHOD(mmu_is_referenced,	moea64_is_referenced),
	MMUMETHOD(mmu_ts_referenced,	moea64_ts_referenced),
	MMUMETHOD(mmu_map,     		moea64_map),
	MMUMETHOD(mmu_page_exists_quick,moea64_page_exists_quick),
	MMUMETHOD(mmu_page_wired_mappings,moea64_page_wired_mappings),
	MMUMETHOD(mmu_pinit,		moea64_pinit),
	MMUMETHOD(mmu_pinit0,		moea64_pinit0),
	MMUMETHOD(mmu_protect,		moea64_protect),
	MMUMETHOD(mmu_qenter,		moea64_qenter),
	MMUMETHOD(mmu_qremove,		moea64_qremove),
	MMUMETHOD(mmu_release,		moea64_release),
	MMUMETHOD(mmu_remove,		moea64_remove),
	MMUMETHOD(mmu_remove_all,      	moea64_remove_all),
	MMUMETHOD(mmu_remove_write,	moea64_remove_write),
	MMUMETHOD(mmu_sync_icache,	moea64_sync_icache),
	MMUMETHOD(mmu_zero_page,       	moea64_zero_page),
	MMUMETHOD(mmu_zero_page_area,	moea64_zero_page_area),
	MMUMETHOD(mmu_zero_page_idle,	moea64_zero_page_idle),
	MMUMETHOD(mmu_activate,		moea64_activate),
	MMUMETHOD(mmu_deactivate,      	moea64_deactivate),
	MMUMETHOD(mmu_page_set_memattr,	moea64_page_set_memattr),

	/* Internal interfaces */
	MMUMETHOD(mmu_mapdev,		moea64_mapdev),
	MMUMETHOD(mmu_mapdev_attr,	moea64_mapdev_attr),
	MMUMETHOD(mmu_unmapdev,		moea64_unmapdev),
	MMUMETHOD(mmu_kextract,		moea64_kextract),
	MMUMETHOD(mmu_kenter,		moea64_kenter),
	MMUMETHOD(mmu_kenter_attr,	moea64_kenter_attr),
	MMUMETHOD(mmu_dev_direct_mapped,moea64_dev_direct_mapped),

	{ 0, 0 }
};

MMU_DEF(oea64_mmu, "mmu_oea64_base", moea64_methods, 0);

static __inline u_int
va_to_pteg(uint64_t vsid, vm_offset_t addr, int large)
{
	uint64_t hash;
	int shift;

	shift = large ? moea64_large_page_shift : ADDR_PIDX_SHFT;
	hash = (vsid & VSID_HASH_MASK) ^ (((uint64_t)addr & ADDR_PIDX) >>
	    shift);
	return (hash & moea64_pteg_mask);
}

static __inline struct pvo_head *
vm_page_to_pvoh(vm_page_t m)
{

	return (&m->md.mdpg_pvoh);
}

static __inline void
moea64_attr_clear(vm_page_t m, u_int64_t ptebit)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	m->md.mdpg_attrs &= ~ptebit;
}

static __inline u_int64_t
moea64_attr_fetch(vm_page_t m)
{

	return (m->md.mdpg_attrs);
}

static __inline void
moea64_attr_save(vm_page_t m, u_int64_t ptebit)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	m->md.mdpg_attrs |= ptebit;
}

static __inline void
moea64_pte_create(struct lpte *pt, uint64_t vsid, vm_offset_t va, 
    uint64_t pte_lo, int flags)
{

	ASSERT_TABLE_LOCK();

	/*
	 * Construct a PTE.  Default to IMB initially.  Valid bit only gets
	 * set when the real pte is set in memory.
	 *
	 * Note: Don't set the valid bit for correct operation of tlb update.
	 */
	pt->pte_hi = (vsid << LPTE_VSID_SHIFT) |
	    (((uint64_t)(va & ADDR_PIDX) >> ADDR_API_SHFT64) & LPTE_API);

	if (flags & PVO_LARGE)
		pt->pte_hi |= LPTE_BIG;

	pt->pte_lo = pte_lo;
}

static __inline uint64_t
moea64_calc_wimg(vm_offset_t pa, vm_memattr_t ma)
{
	uint64_t pte_lo;
	int i;

	if (ma != VM_MEMATTR_DEFAULT) {
		switch (ma) {
		case VM_MEMATTR_UNCACHEABLE:
			return (LPTE_I | LPTE_G);
		case VM_MEMATTR_WRITE_COMBINING:
		case VM_MEMATTR_WRITE_BACK:
		case VM_MEMATTR_PREFETCHABLE:
			return (LPTE_I);
		case VM_MEMATTR_WRITE_THROUGH:
			return (LPTE_W | LPTE_M);
		}
	}

	/*
	 * Assume the page is cache inhibited and access is guarded unless
	 * it's in our available memory array.
	 */
	pte_lo = LPTE_I | LPTE_G;
	for (i = 0; i < pregions_sz; i++) {
		if ((pa >= pregions[i].mr_start) &&
		    (pa < (pregions[i].mr_start + pregions[i].mr_size))) {
			pte_lo &= ~(LPTE_I | LPTE_G);
			pte_lo |= LPTE_M;
			break;
		}
	}

	return pte_lo;
}

/*
 * Quick sort callout for comparing memory regions.
 */
static int	om_cmp(const void *a, const void *b);

static int
om_cmp(const void *a, const void *b)
{
	const struct	ofw_map *mapa;
	const struct	ofw_map *mapb;

	mapa = a;
	mapb = b;
	if (mapa->om_pa_hi < mapb->om_pa_hi)
		return (-1);
	else if (mapa->om_pa_hi > mapb->om_pa_hi)
		return (1);
	else if (mapa->om_pa_lo < mapb->om_pa_lo)
		return (-1);
	else if (mapa->om_pa_lo > mapb->om_pa_lo)
		return (1);
	else
		return (0);
}

static void
moea64_add_ofw_mappings(mmu_t mmup, phandle_t mmu, size_t sz)
{
	struct ofw_map	translations[sz/sizeof(struct ofw_map)];
	register_t	msr;
	vm_offset_t	off;
	vm_paddr_t	pa_base;
	int		i;

	bzero(translations, sz);
	if (OF_getprop(mmu, "translations", translations, sz) == -1)
		panic("moea64_bootstrap: can't get ofw translations");

	CTR0(KTR_PMAP, "moea64_add_ofw_mappings: translations");
	sz /= sizeof(*translations);
	qsort(translations, sz, sizeof (*translations), om_cmp);

	for (i = 0; i < sz; i++) {
		CTR3(KTR_PMAP, "translation: pa=%#x va=%#x len=%#x",
		    (uint32_t)(translations[i].om_pa_lo), translations[i].om_va,
		    translations[i].om_len);

		if (translations[i].om_pa_lo % PAGE_SIZE)
			panic("OFW translation not page-aligned!");

		pa_base = translations[i].om_pa_lo;

	      #ifdef __powerpc64__
		pa_base += (vm_offset_t)translations[i].om_pa_hi << 32;
	      #else
		if (translations[i].om_pa_hi)
			panic("OFW translations above 32-bit boundary!");
	      #endif

		/* Now enter the pages for this mapping */

		DISABLE_TRANS(msr);
		for (off = 0; off < translations[i].om_len; off += PAGE_SIZE) {
			if (moea64_pvo_find_va(kernel_pmap,
			    translations[i].om_va + off) != NULL)
				continue;

			moea64_kenter(mmup, translations[i].om_va + off,
			    pa_base + off);
		}
		ENABLE_TRANS(msr);
	}
}

#ifdef __powerpc64__
static void
moea64_probe_large_page(void)
{
	uint16_t pvr = mfpvr() >> 16;

	switch (pvr) {
	case IBM970:
	case IBM970FX:
	case IBM970MP:
		powerpc_sync(); isync();
		mtspr(SPR_HID4, mfspr(SPR_HID4) & ~HID4_970_DISABLE_LG_PG);
		powerpc_sync(); isync();
		
		/* FALLTHROUGH */
	case IBMCELLBE:
		moea64_large_page_size = 0x1000000; /* 16 MB */
		moea64_large_page_shift = 24;
		break;
	default:
		moea64_large_page_size = 0;
	}

	moea64_large_page_mask = moea64_large_page_size - 1;
}

static void
moea64_bootstrap_slb_prefault(vm_offset_t va, int large)
{
	struct slb *cache;
	struct slb entry;
	uint64_t esid, slbe;
	uint64_t i;

	cache = PCPU_GET(slb);
	esid = va >> ADDR_SR_SHFT;
	slbe = (esid << SLBE_ESID_SHIFT) | SLBE_VALID;

	for (i = 0; i < 64; i++) {
		if (cache[i].slbe == (slbe | i))
			return;
	}

	entry.slbe = slbe;
	entry.slbv = KERNEL_VSID(esid) << SLBV_VSID_SHIFT;
	if (large)
		entry.slbv |= SLBV_L;

	slb_insert_kernel(entry.slbe, entry.slbv);
}
#endif

static void
moea64_setup_direct_map(mmu_t mmup, vm_offset_t kernelstart,
    vm_offset_t kernelend)
{
	register_t msr;
	vm_paddr_t pa;
	vm_offset_t size, off;
	uint64_t pte_lo;
	int i;

	if (moea64_large_page_size == 0) 
		hw_direct_map = 0;

	DISABLE_TRANS(msr);
	if (hw_direct_map) {
		PMAP_LOCK(kernel_pmap);
		for (i = 0; i < pregions_sz; i++) {
		  for (pa = pregions[i].mr_start; pa < pregions[i].mr_start +
		     pregions[i].mr_size; pa += moea64_large_page_size) {
			pte_lo = LPTE_M;

			/*
			 * Set memory access as guarded if prefetch within
			 * the page could exit the available physmem area.
			 */
			if (pa & moea64_large_page_mask) {
				pa &= moea64_large_page_mask;
				pte_lo |= LPTE_G;
			}
			if (pa + moea64_large_page_size >
			    pregions[i].mr_start + pregions[i].mr_size)
				pte_lo |= LPTE_G;

			moea64_pvo_enter(mmup, kernel_pmap, moea64_upvo_zone,
				    &moea64_pvo_kunmanaged, pa, pa,
				    pte_lo, PVO_WIRED | PVO_LARGE);
		  }
		}
		PMAP_UNLOCK(kernel_pmap);
	} else {
		size = sizeof(struct pvo_head) * moea64_pteg_count;
		off = (vm_offset_t)(moea64_pvo_table);
		for (pa = off; pa < off + size; pa += PAGE_SIZE) 
			moea64_kenter(mmup, pa, pa);
		size = BPVO_POOL_SIZE*sizeof(struct pvo_entry);
		off = (vm_offset_t)(moea64_bpvo_pool);
		for (pa = off; pa < off + size; pa += PAGE_SIZE) 
		moea64_kenter(mmup, pa, pa);

		/*
		 * Map certain important things, like ourselves.
		 *
		 * NOTE: We do not map the exception vector space. That code is
		 * used only in real mode, and leaving it unmapped allows us to
		 * catch NULL pointer deferences, instead of making NULL a valid
		 * address.
		 */

		for (pa = kernelstart & ~PAGE_MASK; pa < kernelend;
		    pa += PAGE_SIZE) 
			moea64_kenter(mmup, pa, pa);
	}
	ENABLE_TRANS(msr);
}

void
moea64_early_bootstrap(mmu_t mmup, vm_offset_t kernelstart, vm_offset_t kernelend)
{
	int		i, j;
	vm_size_t	physsz, hwphyssz;

#ifndef __powerpc64__
	/* We don't have a direct map since there is no BAT */
	hw_direct_map = 0;

	/* Make sure battable is zero, since we have no BAT */
	for (i = 0; i < 16; i++) {
		battable[i].batu = 0;
		battable[i].batl = 0;
	}
#else
	moea64_probe_large_page();

	/* Use a direct map if we have large page support */
	if (moea64_large_page_size > 0)
		hw_direct_map = 1;
	else
		hw_direct_map = 0;
#endif

	/* Get physical memory regions from firmware */
	mem_regions(&pregions, &pregions_sz, &regions, &regions_sz);
	CTR0(KTR_PMAP, "moea64_bootstrap: physical memory");

	if (sizeof(phys_avail)/sizeof(phys_avail[0]) < regions_sz)
		panic("moea64_bootstrap: phys_avail too small");

	phys_avail_count = 0;
	physsz = 0;
	hwphyssz = 0;
	TUNABLE_ULONG_FETCH("hw.physmem", (u_long *) &hwphyssz);
	for (i = 0, j = 0; i < regions_sz; i++, j += 2) {
		CTR3(KTR_PMAP, "region: %#x - %#x (%#x)", regions[i].mr_start,
		    regions[i].mr_start + regions[i].mr_size,
		    regions[i].mr_size);
		if (hwphyssz != 0 &&
		    (physsz + regions[i].mr_size) >= hwphyssz) {
			if (physsz < hwphyssz) {
				phys_avail[j] = regions[i].mr_start;
				phys_avail[j + 1] = regions[i].mr_start +
				    hwphyssz - physsz;
				physsz = hwphyssz;
				phys_avail_count++;
			}
			break;
		}
		phys_avail[j] = regions[i].mr_start;
		phys_avail[j + 1] = regions[i].mr_start + regions[i].mr_size;
		phys_avail_count++;
		physsz += regions[i].mr_size;
	}

	/* Check for overlap with the kernel and exception vectors */
	for (j = 0; j < 2*phys_avail_count; j+=2) {
		if (phys_avail[j] < EXC_LAST)
			phys_avail[j] += EXC_LAST;

		if (kernelstart >= phys_avail[j] &&
		    kernelstart < phys_avail[j+1]) {
			if (kernelend < phys_avail[j+1]) {
				phys_avail[2*phys_avail_count] =
				    (kernelend & ~PAGE_MASK) + PAGE_SIZE;
				phys_avail[2*phys_avail_count + 1] =
				    phys_avail[j+1];
				phys_avail_count++;
			}

			phys_avail[j+1] = kernelstart & ~PAGE_MASK;
		}

		if (kernelend >= phys_avail[j] &&
		    kernelend < phys_avail[j+1]) {
			if (kernelstart > phys_avail[j]) {
				phys_avail[2*phys_avail_count] = phys_avail[j];
				phys_avail[2*phys_avail_count + 1] =
				    kernelstart & ~PAGE_MASK;
				phys_avail_count++;
			}

			phys_avail[j] = (kernelend & ~PAGE_MASK) + PAGE_SIZE;
		}
	}

	physmem = btoc(physsz);

#ifdef PTEGCOUNT
	moea64_pteg_count = PTEGCOUNT;
#else
	moea64_pteg_count = 0x1000;

	while (moea64_pteg_count < physmem)
		moea64_pteg_count <<= 1;

	moea64_pteg_count >>= 1;
#endif /* PTEGCOUNT */
}

void
moea64_mid_bootstrap(mmu_t mmup, vm_offset_t kernelstart, vm_offset_t kernelend)
{
	vm_size_t	size;
	register_t	msr;
	int		i;

	/*
	 * Set PTEG mask
	 */
	moea64_pteg_mask = moea64_pteg_count - 1;

	/*
	 * Allocate pv/overflow lists.
	 */
	size = sizeof(struct pvo_head) * moea64_pteg_count;

	moea64_pvo_table = (struct pvo_head *)moea64_bootstrap_alloc(size,
	    PAGE_SIZE);
	CTR1(KTR_PMAP, "moea64_bootstrap: PVO table at %p", moea64_pvo_table);

	DISABLE_TRANS(msr);
	for (i = 0; i < moea64_pteg_count; i++)
		LIST_INIT(&moea64_pvo_table[i]);
	ENABLE_TRANS(msr);

	/*
	 * Initialize the lock that synchronizes access to the pteg and pvo
	 * tables.
	 */
	mtx_init(&moea64_table_mutex, "pmap table", NULL, MTX_DEF |
	    MTX_RECURSE);
	mtx_init(&moea64_slb_mutex, "SLB table", NULL, MTX_DEF);

	/*
	 * Initialise the unmanaged pvo pool.
	 */
	moea64_bpvo_pool = (struct pvo_entry *)moea64_bootstrap_alloc(
		BPVO_POOL_SIZE*sizeof(struct pvo_entry), 0);
	moea64_bpvo_pool_index = 0;

	/*
	 * Make sure kernel vsid is allocated as well as VSID 0.
	 */
	#ifndef __powerpc64__
	moea64_vsid_bitmap[(KERNEL_VSIDBITS & (NVSIDS - 1)) / VSID_NBPW]
		|= 1 << (KERNEL_VSIDBITS % VSID_NBPW);
	moea64_vsid_bitmap[0] |= 1;
	#endif

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	#ifdef __powerpc64__
	for (i = 0; i < 64; i++) {
		pcpup->pc_slb[i].slbv = 0;
		pcpup->pc_slb[i].slbe = 0;
	}
	#else
	for (i = 0; i < 16; i++) 
		kernel_pmap->pm_sr[i] = EMPTY_SEGMENT + i;
	#endif

	kernel_pmap->pmap_phys = kernel_pmap;
	CPU_FILL(&kernel_pmap->pm_active);

	PMAP_LOCK_INIT(kernel_pmap);

	/*
	 * Now map in all the other buffers we allocated earlier
	 */

	moea64_setup_direct_map(mmup, kernelstart, kernelend);
}

void
moea64_late_bootstrap(mmu_t mmup, vm_offset_t kernelstart, vm_offset_t kernelend)
{
	ihandle_t	mmui;
	phandle_t	chosen;
	phandle_t	mmu;
	size_t		sz;
	int		i;
	vm_offset_t	pa, va;
	void		*dpcpu;

	/*
	 * Set up the Open Firmware pmap and add its mappings if not in real
	 * mode.
	 */

	chosen = OF_finddevice("/chosen");
	if (chosen != -1 && OF_getprop(chosen, "mmu", &mmui, 4) != -1) {
	    mmu = OF_instance_to_package(mmui);
	    if (mmu == -1 || (sz = OF_getproplen(mmu, "translations")) == -1)
		sz = 0;
	    if (sz > 6144 /* tmpstksz - 2 KB headroom */)
		panic("moea64_bootstrap: too many ofw translations");

	    if (sz > 0)
		moea64_add_ofw_mappings(mmup, mmu, sz);
	}

	/*
	 * Calculate the last available physical address.
	 */
	for (i = 0; phys_avail[i + 2] != 0; i += 2)
		;
	Maxmem = powerpc_btop(phys_avail[i + 1]);

	/*
	 * Initialize MMU and remap early physical mappings
	 */
	MMU_CPU_BOOTSTRAP(mmup,0);
	mtmsr(mfmsr() | PSL_DR | PSL_IR);
	pmap_bootstrapped++;
	bs_remap_earlyboot();

	/*
	 * Set the start and end of kva.
	 */
	virtual_avail = VM_MIN_KERNEL_ADDRESS;
	virtual_end = VM_MAX_SAFE_KERNEL_ADDRESS; 

	/*
	 * Map the entire KVA range into the SLB. We must not fault there.
	 */
	#ifdef __powerpc64__
	for (va = virtual_avail; va < virtual_end; va += SEGMENT_LENGTH)
		moea64_bootstrap_slb_prefault(va, 0);
	#endif

	/*
	 * Figure out how far we can extend virtual_end into segment 16
	 * without running into existing mappings. Segment 16 is guaranteed
	 * to contain neither RAM nor devices (at least on Apple hardware),
	 * but will generally contain some OFW mappings we should not
	 * step on.
	 */

	#ifndef __powerpc64__	/* KVA is in high memory on PPC64 */
	PMAP_LOCK(kernel_pmap);
	while (virtual_end < VM_MAX_KERNEL_ADDRESS &&
	    moea64_pvo_find_va(kernel_pmap, virtual_end+1) == NULL)
		virtual_end += PAGE_SIZE;
	PMAP_UNLOCK(kernel_pmap);
	#endif

	/*
	 * Allocate a kernel stack with a guard page for thread0 and map it
	 * into the kernel page map.
	 */
	pa = moea64_bootstrap_alloc(KSTACK_PAGES * PAGE_SIZE, PAGE_SIZE);
	va = virtual_avail + KSTACK_GUARD_PAGES * PAGE_SIZE;
	virtual_avail = va + KSTACK_PAGES * PAGE_SIZE;
	CTR2(KTR_PMAP, "moea64_bootstrap: kstack0 at %#x (%#x)", pa, va);
	thread0.td_kstack = va;
	thread0.td_kstack_pages = KSTACK_PAGES;
	for (i = 0; i < KSTACK_PAGES; i++) {
		moea64_kenter(mmup, va, pa);
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
	}

	/*
	 * Allocate virtual address space for the message buffer.
	 */
	pa = msgbuf_phys = moea64_bootstrap_alloc(msgbufsize, PAGE_SIZE);
	msgbufp = (struct msgbuf *)virtual_avail;
	va = virtual_avail;
	virtual_avail += round_page(msgbufsize);
	while (va < virtual_avail) {
		moea64_kenter(mmup, va, pa);
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
	}

	/*
	 * Allocate virtual address space for the dynamic percpu area.
	 */
	pa = moea64_bootstrap_alloc(DPCPU_SIZE, PAGE_SIZE);
	dpcpu = (void *)virtual_avail;
	va = virtual_avail;
	virtual_avail += DPCPU_SIZE;
	while (va < virtual_avail) {
		moea64_kenter(mmup, va, pa);
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
	}
	dpcpu_init(dpcpu, 0);

	/*
	 * Allocate some things for page zeroing. We put this directly
	 * in the page table, marked with LPTE_LOCKED, to avoid any
	 * of the PVO book-keeping or other parts of the VM system
	 * from even knowing that this hack exists.
	 */

	if (!hw_direct_map) {
		mtx_init(&moea64_scratchpage_mtx, "pvo zero page", NULL,
		    MTX_DEF);
		for (i = 0; i < 2; i++) {
			moea64_scratchpage_va[i] = (virtual_end+1) - PAGE_SIZE;
			virtual_end -= PAGE_SIZE;

			moea64_kenter(mmup, moea64_scratchpage_va[i], 0);

			moea64_scratchpage_pvo[i] = moea64_pvo_find_va(
			    kernel_pmap, (vm_offset_t)moea64_scratchpage_va[i]);
			LOCK_TABLE();
			moea64_scratchpage_pte[i] = MOEA64_PVO_TO_PTE(
			    mmup, moea64_scratchpage_pvo[i]);
			moea64_scratchpage_pvo[i]->pvo_pte.lpte.pte_hi
			    |= LPTE_LOCKED;
			MOEA64_PTE_CHANGE(mmup, moea64_scratchpage_pte[i],
			    &moea64_scratchpage_pvo[i]->pvo_pte.lpte,
			    moea64_scratchpage_pvo[i]->pvo_vpn);
			UNLOCK_TABLE();
		}
	}
}

/*
 * Activate a user pmap.  The pmap must be activated before its address
 * space can be accessed in any way.
 */
void
moea64_activate(mmu_t mmu, struct thread *td)
{
	pmap_t	pm;

	pm = &td->td_proc->p_vmspace->vm_pmap;
	CPU_SET(PCPU_GET(cpuid), &pm->pm_active);

	#ifdef __powerpc64__
	PCPU_SET(userslb, pm->pm_slb);
	#else
	PCPU_SET(curpmap, pm->pmap_phys);
	#endif
}

void
moea64_deactivate(mmu_t mmu, struct thread *td)
{
	pmap_t	pm;

	pm = &td->td_proc->p_vmspace->vm_pmap;
	CPU_CLR(PCPU_GET(cpuid), &pm->pm_active);
	#ifdef __powerpc64__
	PCPU_SET(userslb, NULL);
	#else
	PCPU_SET(curpmap, NULL);
	#endif
}

void
moea64_change_wiring(mmu_t mmu, pmap_t pm, vm_offset_t va, boolean_t wired)
{
	struct	pvo_entry *pvo;
	uintptr_t pt;
	uint64_t vsid;
	int	i, ptegidx;

	PMAP_LOCK(pm);
	pvo = moea64_pvo_find_va(pm, va & ~ADDR_POFF);

	if (pvo != NULL) {
		LOCK_TABLE();
		pt = MOEA64_PVO_TO_PTE(mmu, pvo);

		if (wired) {
			if ((pvo->pvo_vaddr & PVO_WIRED) == 0)
				pm->pm_stats.wired_count++;
			pvo->pvo_vaddr |= PVO_WIRED;
			pvo->pvo_pte.lpte.pte_hi |= LPTE_WIRED;
		} else {
			if ((pvo->pvo_vaddr & PVO_WIRED) != 0)
				pm->pm_stats.wired_count--;
			pvo->pvo_vaddr &= ~PVO_WIRED;
			pvo->pvo_pte.lpte.pte_hi &= ~LPTE_WIRED;
		}

		if (pt != -1) {
			/* Update wiring flag in page table. */
			MOEA64_PTE_CHANGE(mmu, pt, &pvo->pvo_pte.lpte,
			    pvo->pvo_vpn);
		} else if (wired) {
			/*
			 * If we are wiring the page, and it wasn't in the
			 * page table before, add it.
			 */
			vsid = PVO_VSID(pvo);
			ptegidx = va_to_pteg(vsid, PVO_VADDR(pvo),
			    pvo->pvo_vaddr & PVO_LARGE);

			i = MOEA64_PTE_INSERT(mmu, ptegidx, &pvo->pvo_pte.lpte);
			
			if (i >= 0) {
				PVO_PTEGIDX_CLR(pvo);
				PVO_PTEGIDX_SET(pvo, i);
			}
		}
			
		UNLOCK_TABLE();
	}
	PMAP_UNLOCK(pm);
}

/*
 * This goes through and sets the physical address of our
 * special scratch PTE to the PA we want to zero or copy. Because
 * of locking issues (this can get called in pvo_enter() by
 * the UMA allocator), we can't use most other utility functions here
 */

static __inline
void moea64_set_scratchpage_pa(mmu_t mmup, int which, vm_offset_t pa) {

	KASSERT(!hw_direct_map, ("Using OEA64 scratchpage with a direct map!"));
	mtx_assert(&moea64_scratchpage_mtx, MA_OWNED);

	moea64_scratchpage_pvo[which]->pvo_pte.lpte.pte_lo &=
	    ~(LPTE_WIMG | LPTE_RPGN);
	moea64_scratchpage_pvo[which]->pvo_pte.lpte.pte_lo |=
	    moea64_calc_wimg(pa, VM_MEMATTR_DEFAULT) | (uint64_t)pa;
	MOEA64_PTE_CHANGE(mmup, moea64_scratchpage_pte[which],
	    &moea64_scratchpage_pvo[which]->pvo_pte.lpte,
	    moea64_scratchpage_pvo[which]->pvo_vpn);
	isync();
}

void
moea64_copy_page(mmu_t mmu, vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t	dst;
	vm_offset_t	src;

	dst = VM_PAGE_TO_PHYS(mdst);
	src = VM_PAGE_TO_PHYS(msrc);

	if (hw_direct_map) {
		kcopy((void *)src, (void *)dst, PAGE_SIZE);
	} else {
		mtx_lock(&moea64_scratchpage_mtx);

		moea64_set_scratchpage_pa(mmu, 0, src);
		moea64_set_scratchpage_pa(mmu, 1, dst);

		kcopy((void *)moea64_scratchpage_va[0], 
		    (void *)moea64_scratchpage_va[1], PAGE_SIZE);

		mtx_unlock(&moea64_scratchpage_mtx);
	}
}

void
moea64_zero_page_area(mmu_t mmu, vm_page_t m, int off, int size)
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(m);

	if (size + off > PAGE_SIZE)
		panic("moea64_zero_page: size + off > PAGE_SIZE");

	if (hw_direct_map) {
		bzero((caddr_t)pa + off, size);
	} else {
		mtx_lock(&moea64_scratchpage_mtx);
		moea64_set_scratchpage_pa(mmu, 0, pa);
		bzero((caddr_t)moea64_scratchpage_va[0] + off, size);
		mtx_unlock(&moea64_scratchpage_mtx);
	}
}

/*
 * Zero a page of physical memory by temporarily mapping it
 */
void
moea64_zero_page(mmu_t mmu, vm_page_t m)
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(m);
	vm_offset_t va, off;

	if (!hw_direct_map) {
		mtx_lock(&moea64_scratchpage_mtx);

		moea64_set_scratchpage_pa(mmu, 0, pa);
		va = moea64_scratchpage_va[0];
	} else {
		va = pa;
	}

	for (off = 0; off < PAGE_SIZE; off += cacheline_size)
		__asm __volatile("dcbz 0,%0" :: "r"(va + off));

	if (!hw_direct_map)
		mtx_unlock(&moea64_scratchpage_mtx);
}

void
moea64_zero_page_idle(mmu_t mmu, vm_page_t m)
{

	moea64_zero_page(mmu, m);
}

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 */
void
moea64_enter(mmu_t mmu, pmap_t pmap, vm_offset_t va, vm_page_t m, 
    vm_prot_t prot, boolean_t wired)
{

	vm_page_lock_queues();
	PMAP_LOCK(pmap);
	moea64_enter_locked(mmu, pmap, va, m, prot, wired);
	vm_page_unlock_queues();
	PMAP_UNLOCK(pmap);
}

/*
 * Map the given physical page at the specified virtual address in the
 * target pmap with the protection requested.  If specified the page
 * will be wired down.
 *
 * The page queues and pmap must be locked.
 */

static void
moea64_enter_locked(mmu_t mmu, pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, boolean_t wired)
{
	struct		pvo_head *pvo_head;
	uma_zone_t	zone;
	vm_page_t	pg;
	uint64_t	pte_lo;
	u_int		pvo_flags;
	int		error;

	if (!moea64_initialized) {
		pvo_head = &moea64_pvo_kunmanaged;
		pg = NULL;
		zone = moea64_upvo_zone;
		pvo_flags = 0;
	} else {
		pvo_head = vm_page_to_pvoh(m);
		pg = m;
		zone = moea64_mpvo_zone;
		pvo_flags = PVO_MANAGED;
	}

	if (pmap_bootstrapped)
		mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((m->oflags & (VPO_UNMANAGED | VPO_BUSY)) != 0 ||
	    VM_OBJECT_LOCKED(m->object),
	    ("moea64_enter_locked: page %p is not busy", m));

	/* XXX change the pvo head for fake pages */
	if ((m->oflags & VPO_UNMANAGED) != 0) {
		pvo_flags &= ~PVO_MANAGED;
		pvo_head = &moea64_pvo_kunmanaged;
		zone = moea64_upvo_zone;
	}

	pte_lo = moea64_calc_wimg(VM_PAGE_TO_PHYS(m), pmap_page_get_memattr(m));

	if (prot & VM_PROT_WRITE) {
		pte_lo |= LPTE_BW;
		if (pmap_bootstrapped &&
		    (m->oflags & VPO_UNMANAGED) == 0)
			vm_page_aflag_set(m, PGA_WRITEABLE);
	} else
		pte_lo |= LPTE_BR;

	if ((prot & VM_PROT_EXECUTE) == 0)
		pte_lo |= LPTE_NOEXEC;

	if (wired)
		pvo_flags |= PVO_WIRED;

	error = moea64_pvo_enter(mmu, pmap, zone, pvo_head, va,
	    VM_PAGE_TO_PHYS(m), pte_lo, pvo_flags);

	/*
	 * Flush the page from the instruction cache if this page is
	 * mapped executable and cacheable.
	 */
	if ((pte_lo & (LPTE_I | LPTE_G | LPTE_NOEXEC)) == 0)
		moea64_syncicache(mmu, pmap, va, VM_PAGE_TO_PHYS(m), PAGE_SIZE);
}

static void
moea64_syncicache(mmu_t mmu, pmap_t pmap, vm_offset_t va, vm_offset_t pa,
    vm_size_t sz)
{

	/*
	 * This is much trickier than on older systems because
	 * we can't sync the icache on physical addresses directly
	 * without a direct map. Instead we check a couple of cases
	 * where the memory is already mapped in and, failing that,
	 * use the same trick we use for page zeroing to create
	 * a temporary mapping for this physical address.
	 */

	if (!pmap_bootstrapped) {
		/*
		 * If PMAP is not bootstrapped, we are likely to be
		 * in real mode.
		 */
		__syncicache((void *)pa, sz);
	} else if (pmap == kernel_pmap) {
		__syncicache((void *)va, sz);
	} else if (hw_direct_map) {
		__syncicache((void *)pa, sz);
	} else {
		/* Use the scratch page to set up a temp mapping */

		mtx_lock(&moea64_scratchpage_mtx);

		moea64_set_scratchpage_pa(mmu, 1, pa & ~ADDR_POFF);
		__syncicache((void *)(moea64_scratchpage_va[1] + 
		    (va & ADDR_POFF)), sz);

		mtx_unlock(&moea64_scratchpage_mtx);
	}
}

/*
 * Maps a sequence of resident pages belonging to the same object.
 * The sequence begins with the given page m_start.  This page is
 * mapped at the given virtual address start.  Each subsequent page is
 * mapped at a virtual address that is offset from start by the same
 * amount as the page is offset from m_start within the object.  The
 * last page in the sequence is the page with the largest offset from
 * m_start that can be mapped at a virtual address less than the given
 * virtual address end.  Not every virtual page between start and end
 * is mapped; only those for which a resident page exists with the
 * corresponding offset from m_start are mapped.
 */
void
moea64_enter_object(mmu_t mmu, pmap_t pm, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{
	vm_page_t m;
	vm_pindex_t diff, psize;

	psize = atop(end - start);
	m = m_start;
	vm_page_lock_queues();
	PMAP_LOCK(pm);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		moea64_enter_locked(mmu, pm, start + ptoa(diff), m, prot &
		    (VM_PROT_READ | VM_PROT_EXECUTE), FALSE);
		m = TAILQ_NEXT(m, listq);
	}
	vm_page_unlock_queues();
	PMAP_UNLOCK(pm);
}

void
moea64_enter_quick(mmu_t mmu, pmap_t pm, vm_offset_t va, vm_page_t m,
    vm_prot_t prot)
{

	vm_page_lock_queues();
	PMAP_LOCK(pm);
	moea64_enter_locked(mmu, pm, va, m,
	    prot & (VM_PROT_READ | VM_PROT_EXECUTE), FALSE);
	vm_page_unlock_queues();
	PMAP_UNLOCK(pm);
}

vm_paddr_t
moea64_extract(mmu_t mmu, pmap_t pm, vm_offset_t va)
{
	struct	pvo_entry *pvo;
	vm_paddr_t pa;

	PMAP_LOCK(pm);
	pvo = moea64_pvo_find_va(pm, va);
	if (pvo == NULL)
		pa = 0;
	else
		pa = (pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN) |
		    (va - PVO_VADDR(pvo));
	PMAP_UNLOCK(pm);
	return (pa);
}

/*
 * Atomically extract and hold the physical page with the given
 * pmap and virtual address pair if that mapping permits the given
 * protection.
 */
vm_page_t
moea64_extract_and_hold(mmu_t mmu, pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	struct	pvo_entry *pvo;
	vm_page_t m;
        vm_paddr_t pa;
        
	m = NULL;
	pa = 0;
	PMAP_LOCK(pmap);
retry:
	pvo = moea64_pvo_find_va(pmap, va & ~ADDR_POFF);
	if (pvo != NULL && (pvo->pvo_pte.lpte.pte_hi & LPTE_VALID) &&
	    ((pvo->pvo_pte.lpte.pte_lo & LPTE_PP) == LPTE_RW ||
	     (prot & VM_PROT_WRITE) == 0)) {
		if (vm_page_pa_tryrelock(pmap,
			pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN, &pa))
			goto retry;
		m = PHYS_TO_VM_PAGE(pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN);
		vm_page_hold(m);
	}
	PA_UNLOCK_COND(pa);
	PMAP_UNLOCK(pmap);
	return (m);
}

static mmu_t installed_mmu;

static void *
moea64_uma_page_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait) 
{
	/*
	 * This entire routine is a horrible hack to avoid bothering kmem
	 * for new KVA addresses. Because this can get called from inside
	 * kmem allocation routines, calling kmem for a new address here
	 * can lead to multiply locking non-recursive mutexes.
	 */
	static vm_pindex_t color;
        vm_offset_t va;

        vm_page_t m;
        int pflags, needed_lock;

	*flags = UMA_SLAB_PRIV;
	needed_lock = !PMAP_LOCKED(kernel_pmap);

	if (needed_lock)
		PMAP_LOCK(kernel_pmap);

        if ((wait & (M_NOWAIT|M_USE_RESERVE)) == M_NOWAIT)
                pflags = VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED;
        else
                pflags = VM_ALLOC_SYSTEM | VM_ALLOC_WIRED;
        if (wait & M_ZERO)
                pflags |= VM_ALLOC_ZERO;

        for (;;) {
                m = vm_page_alloc(NULL, color++, pflags | VM_ALLOC_NOOBJ);
                if (m == NULL) {
                        if (wait & M_NOWAIT)
                                return (NULL);
                        VM_WAIT;
                } else
                        break;
        }

	va = VM_PAGE_TO_PHYS(m);

	moea64_pvo_enter(installed_mmu, kernel_pmap, moea64_upvo_zone,
	    &moea64_pvo_kunmanaged, va, VM_PAGE_TO_PHYS(m), LPTE_M,
	    PVO_WIRED | PVO_BOOTSTRAP);

	if (needed_lock)
		PMAP_UNLOCK(kernel_pmap);
	
	if ((wait & M_ZERO) && (m->flags & PG_ZERO) == 0)
                bzero((void *)va, PAGE_SIZE);

	return (void *)va;
}

void
moea64_init(mmu_t mmu)
{

	CTR0(KTR_PMAP, "moea64_init");

	moea64_upvo_zone = uma_zcreate("UPVO entry", sizeof (struct pvo_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
	    UMA_ZONE_VM | UMA_ZONE_NOFREE);
	moea64_mpvo_zone = uma_zcreate("MPVO entry", sizeof(struct pvo_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
	    UMA_ZONE_VM | UMA_ZONE_NOFREE);

	if (!hw_direct_map) {
		installed_mmu = mmu;
		uma_zone_set_allocf(moea64_upvo_zone,moea64_uma_page_alloc);
		uma_zone_set_allocf(moea64_mpvo_zone,moea64_uma_page_alloc);
	}

	moea64_initialized = TRUE;
}

boolean_t
moea64_is_referenced(mmu_t mmu, vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("moea64_is_referenced: page %p is not managed", m));
	return (moea64_query_bit(mmu, m, PTE_REF));
}

boolean_t
moea64_is_modified(mmu_t mmu, vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("moea64_is_modified: page %p is not managed", m));

	/*
	 * If the page is not VPO_BUSY, then PGA_WRITEABLE cannot be
	 * concurrently set while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no PTEs can have LPTE_CHG set.
	 */
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((m->oflags & VPO_BUSY) == 0 &&
	    (m->aflags & PGA_WRITEABLE) == 0)
		return (FALSE);
	return (moea64_query_bit(mmu, m, LPTE_CHG));
}

boolean_t
moea64_is_prefaultable(mmu_t mmu, pmap_t pmap, vm_offset_t va)
{
	struct pvo_entry *pvo;
	boolean_t rv;

	PMAP_LOCK(pmap);
	pvo = moea64_pvo_find_va(pmap, va & ~ADDR_POFF);
	rv = pvo == NULL || (pvo->pvo_pte.lpte.pte_hi & LPTE_VALID) == 0;
	PMAP_UNLOCK(pmap);
	return (rv);
}

void
moea64_clear_reference(mmu_t mmu, vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("moea64_clear_reference: page %p is not managed", m));
	moea64_clear_bit(mmu, m, LPTE_REF);
}

void
moea64_clear_modify(mmu_t mmu, vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("moea64_clear_modify: page %p is not managed", m));
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	KASSERT((m->oflags & VPO_BUSY) == 0,
	    ("moea64_clear_modify: page %p is busy", m));

	/*
	 * If the page is not PGA_WRITEABLE, then no PTEs can have LPTE_CHG
	 * set.  If the object containing the page is locked and the page is
	 * not VPO_BUSY, then PGA_WRITEABLE cannot be concurrently set.
	 */
	if ((m->aflags & PGA_WRITEABLE) == 0)
		return;
	moea64_clear_bit(mmu, m, LPTE_CHG);
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
moea64_remove_write(mmu_t mmu, vm_page_t m)
{
	struct	pvo_entry *pvo;
	uintptr_t pt;
	pmap_t	pmap;
	uint64_t lo;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("moea64_remove_write: page %p is not managed", m));

	/*
	 * If the page is not VPO_BUSY, then PGA_WRITEABLE cannot be set by
	 * another thread while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no page table entries need updating.
	 */
	VM_OBJECT_LOCK_ASSERT(m->object, MA_OWNED);
	if ((m->oflags & VPO_BUSY) == 0 &&
	    (m->aflags & PGA_WRITEABLE) == 0)
		return;
	vm_page_lock_queues();
	lo = moea64_attr_fetch(m);
	powerpc_sync();
	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {
		pmap = pvo->pvo_pmap;
		PMAP_LOCK(pmap);
		LOCK_TABLE();
		if ((pvo->pvo_pte.lpte.pte_lo & LPTE_PP) != LPTE_BR) {
			pt = MOEA64_PVO_TO_PTE(mmu, pvo);
			pvo->pvo_pte.lpte.pte_lo &= ~LPTE_PP;
			pvo->pvo_pte.lpte.pte_lo |= LPTE_BR;
			if (pt != -1) {
				MOEA64_PTE_SYNCH(mmu, pt, &pvo->pvo_pte.lpte);
				lo |= pvo->pvo_pte.lpte.pte_lo;
				pvo->pvo_pte.lpte.pte_lo &= ~LPTE_CHG;
				MOEA64_PTE_CHANGE(mmu, pt,
				    &pvo->pvo_pte.lpte, pvo->pvo_vpn);
				if (pvo->pvo_pmap == kernel_pmap)
					isync();
			}
		}
		UNLOCK_TABLE();
		PMAP_UNLOCK(pmap);
	}
	if ((lo & LPTE_CHG) != 0) {
		moea64_attr_clear(m, LPTE_CHG);
		vm_page_dirty(m);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	vm_page_unlock_queues();
}

/*
 *	moea64_ts_referenced:
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
boolean_t
moea64_ts_referenced(mmu_t mmu, vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("moea64_ts_referenced: page %p is not managed", m));
	return (moea64_clear_bit(mmu, m, LPTE_REF));
}

/*
 * Modify the WIMG settings of all mappings for a page.
 */
void
moea64_page_set_memattr(mmu_t mmu, vm_page_t m, vm_memattr_t ma)
{
	struct	pvo_entry *pvo;
	struct  pvo_head *pvo_head;
	uintptr_t pt;
	pmap_t	pmap;
	uint64_t lo;

	if ((m->oflags & VPO_UNMANAGED) != 0) {
		m->md.mdpg_cache_attrs = ma;
		return;
	}

	vm_page_lock_queues();
	pvo_head = vm_page_to_pvoh(m);
	lo = moea64_calc_wimg(VM_PAGE_TO_PHYS(m), ma);
	LIST_FOREACH(pvo, pvo_head, pvo_vlink) {
		pmap = pvo->pvo_pmap;
		PMAP_LOCK(pmap);
		LOCK_TABLE();
		pt = MOEA64_PVO_TO_PTE(mmu, pvo);
		pvo->pvo_pte.lpte.pte_lo &= ~LPTE_WIMG;
		pvo->pvo_pte.lpte.pte_lo |= lo;
		if (pt != -1) {
			MOEA64_PTE_CHANGE(mmu, pt, &pvo->pvo_pte.lpte,
			    pvo->pvo_vpn);
			if (pvo->pvo_pmap == kernel_pmap)
				isync();
		}
		UNLOCK_TABLE();
		PMAP_UNLOCK(pmap);
	}
	m->md.mdpg_cache_attrs = ma;
	vm_page_unlock_queues();
}

/*
 * Map a wired page into kernel virtual address space.
 */
void
moea64_kenter_attr(mmu_t mmu, vm_offset_t va, vm_offset_t pa, vm_memattr_t ma)
{
	uint64_t	pte_lo;
	int		error;	

	pte_lo = moea64_calc_wimg(pa, ma);

	PMAP_LOCK(kernel_pmap);
	error = moea64_pvo_enter(mmu, kernel_pmap, moea64_upvo_zone,
	    &moea64_pvo_kunmanaged, va, pa, pte_lo, PVO_WIRED);

	if (error != 0 && error != ENOENT)
		panic("moea64_kenter: failed to enter va %#zx pa %#zx: %d", va,
		    pa, error);

	/*
	 * Flush the memory from the instruction cache.
	 */
	if ((pte_lo & (LPTE_I | LPTE_G)) == 0)
		__syncicache((void *)va, PAGE_SIZE);
	PMAP_UNLOCK(kernel_pmap);
}

void
moea64_kenter(mmu_t mmu, vm_offset_t va, vm_offset_t pa)
{

	moea64_kenter_attr(mmu, va, pa, VM_MEMATTR_DEFAULT);
}

/*
 * Extract the physical page address associated with the given kernel virtual
 * address.
 */
vm_offset_t
moea64_kextract(mmu_t mmu, vm_offset_t va)
{
	struct		pvo_entry *pvo;
	vm_paddr_t pa;

	/*
	 * Shortcut the direct-mapped case when applicable.  We never put
	 * anything but 1:1 mappings below VM_MIN_KERNEL_ADDRESS.
	 */
	if (va < VM_MIN_KERNEL_ADDRESS)
		return (va);

	PMAP_LOCK(kernel_pmap);
	pvo = moea64_pvo_find_va(kernel_pmap, va);
	KASSERT(pvo != NULL, ("moea64_kextract: no addr found for %#" PRIxPTR,
	    va));
	pa = (pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN) | (va - PVO_VADDR(pvo));
	PMAP_UNLOCK(kernel_pmap);
	return (pa);
}

/*
 * Remove a wired page from kernel virtual address space.
 */
void
moea64_kremove(mmu_t mmu, vm_offset_t va)
{
	moea64_remove(mmu, kernel_pmap, va, va + PAGE_SIZE);
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
moea64_map(mmu_t mmu, vm_offset_t *virt, vm_offset_t pa_start,
    vm_offset_t pa_end, int prot)
{
	vm_offset_t	sva, va;

	sva = *virt;
	va = sva;
	for (; pa_start < pa_end; pa_start += PAGE_SIZE, va += PAGE_SIZE)
		moea64_kenter(mmu, va, pa_start);
	*virt = va;

	return (sva);
}

/*
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
moea64_page_exists_quick(mmu_t mmu, pmap_t pmap, vm_page_t m)
{
        int loops;
	struct pvo_entry *pvo;
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("moea64_page_exists_quick: page %p is not managed", m));
	loops = 0;
	rv = FALSE;
	vm_page_lock_queues();
	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {
		if (pvo->pvo_pmap == pmap) {
			rv = TRUE;
			break;
		}
		if (++loops >= 16)
			break;
	}
	vm_page_unlock_queues();
	return (rv);
}

/*
 * Return the number of managed mappings to the given physical page
 * that are wired.
 */
int
moea64_page_wired_mappings(mmu_t mmu, vm_page_t m)
{
	struct pvo_entry *pvo;
	int count;

	count = 0;
	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (count);
	vm_page_lock_queues();
	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink)
		if ((pvo->pvo_vaddr & PVO_WIRED) != 0)
			count++;
	vm_page_unlock_queues();
	return (count);
}

static uintptr_t	moea64_vsidcontext;

uintptr_t
moea64_get_unique_vsid(void) {
	u_int entropy;
	register_t hash;
	uint32_t mask;
	int i;

	entropy = 0;
	__asm __volatile("mftb %0" : "=r"(entropy));

	mtx_lock(&moea64_slb_mutex);
	for (i = 0; i < NVSIDS; i += VSID_NBPW) {
		u_int	n;

		/*
		 * Create a new value by mutiplying by a prime and adding in
		 * entropy from the timebase register.  This is to make the
		 * VSID more random so that the PT hash function collides
		 * less often.  (Note that the prime casues gcc to do shifts
		 * instead of a multiply.)
		 */
		moea64_vsidcontext = (moea64_vsidcontext * 0x1105) + entropy;
		hash = moea64_vsidcontext & (NVSIDS - 1);
		if (hash == 0)		/* 0 is special, avoid it */
			continue;
		n = hash >> 5;
		mask = 1 << (hash & (VSID_NBPW - 1));
		hash = (moea64_vsidcontext & VSID_HASHMASK);
		if (moea64_vsid_bitmap[n] & mask) {	/* collision? */
			/* anything free in this bucket? */
			if (moea64_vsid_bitmap[n] == 0xffffffff) {
				entropy = (moea64_vsidcontext >> 20);
				continue;
			}
			i = ffs(~moea64_vsid_bitmap[n]) - 1;
			mask = 1 << i;
			hash &= VSID_HASHMASK & ~(VSID_NBPW - 1);
			hash |= i;
		}
		KASSERT(!(moea64_vsid_bitmap[n] & mask),
		    ("Allocating in-use VSID %#zx\n", hash));
		moea64_vsid_bitmap[n] |= mask;
		mtx_unlock(&moea64_slb_mutex);
		return (hash);
	}

	mtx_unlock(&moea64_slb_mutex);
	panic("%s: out of segments",__func__);
}

#ifdef __powerpc64__
void
moea64_pinit(mmu_t mmu, pmap_t pmap)
{
	PMAP_LOCK_INIT(pmap);

	pmap->pm_slb_tree_root = slb_alloc_tree();
	pmap->pm_slb = slb_alloc_user_cache();
	pmap->pm_slb_len = 0;
}
#else
void
moea64_pinit(mmu_t mmu, pmap_t pmap)
{
	int	i;
	uint32_t hash;

	PMAP_LOCK_INIT(pmap);

	if (pmap_bootstrapped)
		pmap->pmap_phys = (pmap_t)moea64_kextract(mmu,
		    (vm_offset_t)pmap);
	else
		pmap->pmap_phys = pmap;

	/*
	 * Allocate some segment registers for this pmap.
	 */
	hash = moea64_get_unique_vsid();

	for (i = 0; i < 16; i++) 
		pmap->pm_sr[i] = VSID_MAKE(i, hash);

	KASSERT(pmap->pm_sr[0] != 0, ("moea64_pinit: pm_sr[0] = 0"));
}
#endif

/*
 * Initialize the pmap associated with process 0.
 */
void
moea64_pinit0(mmu_t mmu, pmap_t pm)
{
	moea64_pinit(mmu, pm);
	bzero(&pm->pm_stats, sizeof(pm->pm_stats));
}

/*
 * Set the physical protection on the specified range of this map as requested.
 */
void
moea64_protect(mmu_t mmu, pmap_t pm, vm_offset_t sva, vm_offset_t eva,
    vm_prot_t prot)
{
	struct	pvo_entry *pvo;
	uintptr_t pt;

	CTR4(KTR_PMAP, "moea64_protect: pm=%p sva=%#x eva=%#x prot=%#x", pm, sva,
	    eva, prot);


	KASSERT(pm == &curproc->p_vmspace->vm_pmap || pm == kernel_pmap,
	    ("moea64_protect: non current pmap"));

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		moea64_remove(mmu, pm, sva, eva);
		return;
	}

	vm_page_lock_queues();
	PMAP_LOCK(pm);
	for (; sva < eva; sva += PAGE_SIZE) {
		pvo = moea64_pvo_find_va(pm, sva);
		if (pvo == NULL)
			continue;

		/*
		 * Grab the PTE pointer before we diddle with the cached PTE
		 * copy.
		 */
		LOCK_TABLE();
		pt = MOEA64_PVO_TO_PTE(mmu, pvo);

		/*
		 * Change the protection of the page.
		 */
		pvo->pvo_pte.lpte.pte_lo &= ~LPTE_PP;
		pvo->pvo_pte.lpte.pte_lo |= LPTE_BR;
		pvo->pvo_pte.lpte.pte_lo &= ~LPTE_NOEXEC;
		if ((prot & VM_PROT_EXECUTE) == 0) 
			pvo->pvo_pte.lpte.pte_lo |= LPTE_NOEXEC;

		/*
		 * If the PVO is in the page table, update that pte as well.
		 */
		if (pt != -1) {
			MOEA64_PTE_CHANGE(mmu, pt, &pvo->pvo_pte.lpte,
			    pvo->pvo_vpn);
			if ((pvo->pvo_pte.lpte.pte_lo & 
			    (LPTE_I | LPTE_G | LPTE_NOEXEC)) == 0) {
				moea64_syncicache(mmu, pm, sva,
				    pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN,
				    PAGE_SIZE);
			}
		}
		UNLOCK_TABLE();
	}
	vm_page_unlock_queues();
	PMAP_UNLOCK(pm);
}

/*
 * Map a list of wired pages into kernel virtual address space.  This is
 * intended for temporary mappings which do not need page modification or
 * references recorded.  Existing mappings in the region are overwritten.
 */
void
moea64_qenter(mmu_t mmu, vm_offset_t va, vm_page_t *m, int count)
{
	while (count-- > 0) {
		moea64_kenter(mmu, va, VM_PAGE_TO_PHYS(*m));
		va += PAGE_SIZE;
		m++;
	}
}

/*
 * Remove page mappings from kernel virtual address space.  Intended for
 * temporary mappings entered by moea64_qenter.
 */
void
moea64_qremove(mmu_t mmu, vm_offset_t va, int count)
{
	while (count-- > 0) {
		moea64_kremove(mmu, va);
		va += PAGE_SIZE;
	}
}

void
moea64_release_vsid(uint64_t vsid)
{
	int idx, mask;

	mtx_lock(&moea64_slb_mutex);
	idx = vsid & (NVSIDS-1);
	mask = 1 << (idx % VSID_NBPW);
	idx /= VSID_NBPW;
	KASSERT(moea64_vsid_bitmap[idx] & mask,
	    ("Freeing unallocated VSID %#jx", vsid));
	moea64_vsid_bitmap[idx] &= ~mask;
	mtx_unlock(&moea64_slb_mutex);
}
	

void
moea64_release(mmu_t mmu, pmap_t pmap)
{
        
	/*
	 * Free segment registers' VSIDs
	 */
    #ifdef __powerpc64__
	slb_free_tree(pmap);
	slb_free_user_cache(pmap->pm_slb);
    #else
	KASSERT(pmap->pm_sr[0] != 0, ("moea64_release: pm_sr[0] = 0"));

	moea64_release_vsid(VSID_TO_HASH(pmap->pm_sr[0]));
    #endif

	PMAP_LOCK_DESTROY(pmap);
}

/*
 * Remove the given range of addresses from the specified map.
 */
void
moea64_remove(mmu_t mmu, pmap_t pm, vm_offset_t sva, vm_offset_t eva)
{
	struct	pvo_entry *pvo;

	vm_page_lock_queues();
	PMAP_LOCK(pm);
	for (; sva < eva; sva += PAGE_SIZE) {
		pvo = moea64_pvo_find_va(pm, sva);
		if (pvo != NULL)
			moea64_pvo_remove(mmu, pvo);
	}
	vm_page_unlock_queues();
	PMAP_UNLOCK(pm);
}

/*
 * Remove physical page from all pmaps in which it resides. moea64_pvo_remove()
 * will reflect changes in pte's back to the vm_page.
 */
void
moea64_remove_all(mmu_t mmu, vm_page_t m)
{
	struct  pvo_head *pvo_head;
	struct	pvo_entry *pvo, *next_pvo;
	pmap_t	pmap;

	vm_page_lock_queues();
	pvo_head = vm_page_to_pvoh(m);
	for (pvo = LIST_FIRST(pvo_head); pvo != NULL; pvo = next_pvo) {
		next_pvo = LIST_NEXT(pvo, pvo_vlink);

		pmap = pvo->pvo_pmap;
		PMAP_LOCK(pmap);
		moea64_pvo_remove(mmu, pvo);
		PMAP_UNLOCK(pmap);
	}
	if ((m->aflags & PGA_WRITEABLE) && moea64_is_modified(mmu, m)) {
		moea64_attr_clear(m, LPTE_CHG);
		vm_page_dirty(m);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	vm_page_unlock_queues();
}

/*
 * Allocate a physical page of memory directly from the phys_avail map.
 * Can only be called from moea64_bootstrap before avail start and end are
 * calculated.
 */
vm_offset_t
moea64_bootstrap_alloc(vm_size_t size, u_int align)
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

		if (s + size > platform_real_maxaddr())
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
	panic("moea64_bootstrap_alloc: could not allocate memory");
}

static int
moea64_pvo_enter(mmu_t mmu, pmap_t pm, uma_zone_t zone,
    struct pvo_head *pvo_head, vm_offset_t va, vm_offset_t pa,
    uint64_t pte_lo, int flags)
{
	struct	 pvo_entry *pvo;
	uint64_t vsid;
	int	 first;
	u_int	 ptegidx;
	int	 i;
	int      bootstrap;

	/*
	 * One nasty thing that can happen here is that the UMA calls to
	 * allocate new PVOs need to map more memory, which calls pvo_enter(),
	 * which calls UMA...
	 *
	 * We break the loop by detecting recursion and allocating out of
	 * the bootstrap pool.
	 */

	first = 0;
	bootstrap = (flags & PVO_BOOTSTRAP);

	if (!moea64_initialized)
		bootstrap = 1;

	/*
	 * Compute the PTE Group index.
	 */
	va &= ~ADDR_POFF;
	vsid = va_to_vsid(pm, va);
	ptegidx = va_to_pteg(vsid, va, flags & PVO_LARGE);

	/*
	 * Remove any existing mapping for this page.  Reuse the pvo entry if
	 * there is a mapping.
	 */
	LOCK_TABLE();

	moea64_pvo_enter_calls++;

	LIST_FOREACH(pvo, &moea64_pvo_table[ptegidx], pvo_olink) {
		if (pvo->pvo_pmap == pm && PVO_VADDR(pvo) == va) {
			if ((pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN) == pa &&
			    (pvo->pvo_pte.lpte.pte_lo & (LPTE_NOEXEC | LPTE_PP))
			    == (pte_lo & (LPTE_NOEXEC | LPTE_PP))) {
			    	if (!(pvo->pvo_pte.lpte.pte_hi & LPTE_VALID)) {
					/* Re-insert if spilled */
					i = MOEA64_PTE_INSERT(mmu, ptegidx,
					    &pvo->pvo_pte.lpte);
					if (i >= 0)
						PVO_PTEGIDX_SET(pvo, i);
					moea64_pte_overflow--;
				}
				UNLOCK_TABLE();
				return (0);
			}
			moea64_pvo_remove(mmu, pvo);
			break;
		}
	}

	/*
	 * If we aren't overwriting a mapping, try to allocate.
	 */
	if (bootstrap) {
		if (moea64_bpvo_pool_index >= BPVO_POOL_SIZE) {
			panic("moea64_enter: bpvo pool exhausted, %d, %d, %zd",
			      moea64_bpvo_pool_index, BPVO_POOL_SIZE, 
			      BPVO_POOL_SIZE * sizeof(struct pvo_entry));
		}
		pvo = &moea64_bpvo_pool[moea64_bpvo_pool_index];
		moea64_bpvo_pool_index++;
		bootstrap = 1;
	} else {
		/*
		 * Note: drop the table lock around the UMA allocation in
		 * case the UMA allocator needs to manipulate the page
		 * table. The mapping we are working with is already
		 * protected by the PMAP lock.
		 */
		UNLOCK_TABLE();
		pvo = uma_zalloc(zone, M_NOWAIT);
		LOCK_TABLE();
	}

	if (pvo == NULL) {
		UNLOCK_TABLE();
		return (ENOMEM);
	}

	moea64_pvo_entries++;
	pvo->pvo_vaddr = va;
	pvo->pvo_vpn = (uint64_t)((va & ADDR_PIDX) >> ADDR_PIDX_SHFT)
	    | (vsid << 16);
	pvo->pvo_pmap = pm;
	LIST_INSERT_HEAD(&moea64_pvo_table[ptegidx], pvo, pvo_olink);
	pvo->pvo_vaddr &= ~ADDR_POFF;

	if (flags & PVO_WIRED)
		pvo->pvo_vaddr |= PVO_WIRED;
	if (pvo_head != &moea64_pvo_kunmanaged)
		pvo->pvo_vaddr |= PVO_MANAGED;
	if (bootstrap)
		pvo->pvo_vaddr |= PVO_BOOTSTRAP;
	if (flags & PVO_LARGE)
		pvo->pvo_vaddr |= PVO_LARGE;

	moea64_pte_create(&pvo->pvo_pte.lpte, vsid, va, 
	    (uint64_t)(pa) | pte_lo, flags);

	/*
	 * Remember if the list was empty and therefore will be the first
	 * item.
	 */
	if (LIST_FIRST(pvo_head) == NULL)
		first = 1;
	LIST_INSERT_HEAD(pvo_head, pvo, pvo_vlink);

	if (pvo->pvo_vaddr & PVO_WIRED) {
		pvo->pvo_pte.lpte.pte_hi |= LPTE_WIRED;
		pm->pm_stats.wired_count++;
	}
	pm->pm_stats.resident_count++;

	/*
	 * We hope this succeeds but it isn't required.
	 */
	i = MOEA64_PTE_INSERT(mmu, ptegidx, &pvo->pvo_pte.lpte);
	if (i >= 0) {
		PVO_PTEGIDX_SET(pvo, i);
	} else {
		panic("moea64_pvo_enter: overflow");
		moea64_pte_overflow++;
	}

	if (pm == kernel_pmap)
		isync();

	UNLOCK_TABLE();

#ifdef __powerpc64__
	/*
	 * Make sure all our bootstrap mappings are in the SLB as soon
	 * as virtual memory is switched on.
	 */
	if (!pmap_bootstrapped)
		moea64_bootstrap_slb_prefault(va, flags & PVO_LARGE);
#endif

	return (first ? ENOENT : 0);
}

static void
moea64_pvo_remove(mmu_t mmu, struct pvo_entry *pvo)
{
	uintptr_t pt;

	/*
	 * If there is an active pte entry, we need to deactivate it (and
	 * save the ref & cfg bits).
	 */
	LOCK_TABLE();
	pt = MOEA64_PVO_TO_PTE(mmu, pvo);
	if (pt != -1) {
		MOEA64_PTE_UNSET(mmu, pt, &pvo->pvo_pte.lpte, pvo->pvo_vpn);
		PVO_PTEGIDX_CLR(pvo);
	} else {
		moea64_pte_overflow--;
	}

	/*
	 * Update our statistics.
	 */
	pvo->pvo_pmap->pm_stats.resident_count--;
	if (pvo->pvo_vaddr & PVO_WIRED)
		pvo->pvo_pmap->pm_stats.wired_count--;

	/*
	 * Save the REF/CHG bits into their cache if the page is managed.
	 */
	if ((pvo->pvo_vaddr & PVO_MANAGED) == PVO_MANAGED) {
		struct	vm_page *pg;

		pg = PHYS_TO_VM_PAGE(pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN);
		if (pg != NULL) {
			moea64_attr_save(pg, pvo->pvo_pte.lpte.pte_lo &
			    (LPTE_REF | LPTE_CHG));
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

	moea64_pvo_entries--;
	moea64_pvo_remove_calls++;

	UNLOCK_TABLE();

	if (!(pvo->pvo_vaddr & PVO_BOOTSTRAP))
		uma_zfree((pvo->pvo_vaddr & PVO_MANAGED) ? moea64_mpvo_zone :
		    moea64_upvo_zone, pvo);
}

static struct pvo_entry *
moea64_pvo_find_va(pmap_t pm, vm_offset_t va)
{
	struct		pvo_entry *pvo;
	int		ptegidx;
	uint64_t	vsid;
	#ifdef __powerpc64__
	uint64_t	slbv;

	if (pm == kernel_pmap) {
		slbv = kernel_va_to_slbv(va);
	} else {
		struct slb *slb;
		slb = user_va_to_slb_entry(pm, va);
		/* The page is not mapped if the segment isn't */
		if (slb == NULL)
			return NULL;
		slbv = slb->slbv;
	}

	vsid = (slbv & SLBV_VSID_MASK) >> SLBV_VSID_SHIFT;
	if (slbv & SLBV_L)
		va &= ~moea64_large_page_mask;
	else
		va &= ~ADDR_POFF;
	ptegidx = va_to_pteg(vsid, va, slbv & SLBV_L);
	#else
	va &= ~ADDR_POFF;
	vsid = va_to_vsid(pm, va);
	ptegidx = va_to_pteg(vsid, va, 0);
	#endif

	LOCK_TABLE();
	LIST_FOREACH(pvo, &moea64_pvo_table[ptegidx], pvo_olink) {
		if (pvo->pvo_pmap == pm && PVO_VADDR(pvo) == va)
			break;
	}
	UNLOCK_TABLE();

	return (pvo);
}

static boolean_t
moea64_query_bit(mmu_t mmu, vm_page_t m, u_int64_t ptebit)
{
	struct	pvo_entry *pvo;
	uintptr_t pt;

	if (moea64_attr_fetch(m) & ptebit)
		return (TRUE);

	vm_page_lock_queues();

	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {

		/*
		 * See if we saved the bit off.  If so, cache it and return
		 * success.
		 */
		if (pvo->pvo_pte.lpte.pte_lo & ptebit) {
			moea64_attr_save(m, ptebit);
			vm_page_unlock_queues();
			return (TRUE);
		}
	}

	/*
	 * No luck, now go through the hard part of looking at the PTEs
	 * themselves.  Sync so that any pending REF/CHG bits are flushed to
	 * the PTEs.
	 */
	powerpc_sync();
	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {

		/*
		 * See if this pvo has a valid PTE.  if so, fetch the
		 * REF/CHG bits from the valid PTE.  If the appropriate
		 * ptebit is set, cache it and return success.
		 */
		LOCK_TABLE();
		pt = MOEA64_PVO_TO_PTE(mmu, pvo);
		if (pt != -1) {
			MOEA64_PTE_SYNCH(mmu, pt, &pvo->pvo_pte.lpte);
			if (pvo->pvo_pte.lpte.pte_lo & ptebit) {
				UNLOCK_TABLE();

				moea64_attr_save(m, ptebit);
				vm_page_unlock_queues();
				return (TRUE);
			}
		}
		UNLOCK_TABLE();
	}

	vm_page_unlock_queues();
	return (FALSE);
}

static u_int
moea64_clear_bit(mmu_t mmu, vm_page_t m, u_int64_t ptebit)
{
	u_int	count;
	struct	pvo_entry *pvo;
	uintptr_t pt;

	vm_page_lock_queues();

	/*
	 * Clear the cached value.
	 */
	moea64_attr_clear(m, ptebit);

	/*
	 * Sync so that any pending REF/CHG bits are flushed to the PTEs (so
	 * we can reset the right ones).  note that since the pvo entries and
	 * list heads are accessed via BAT0 and are never placed in the page
	 * table, we don't have to worry about further accesses setting the
	 * REF/CHG bits.
	 */
	powerpc_sync();

	/*
	 * For each pvo entry, clear the pvo's ptebit.  If this pvo has a
	 * valid pte clear the ptebit from the valid pte.
	 */
	count = 0;
	LIST_FOREACH(pvo, vm_page_to_pvoh(m), pvo_vlink) {

		LOCK_TABLE();
		pt = MOEA64_PVO_TO_PTE(mmu, pvo);
		if (pt != -1) {
			MOEA64_PTE_SYNCH(mmu, pt, &pvo->pvo_pte.lpte);
			if (pvo->pvo_pte.lpte.pte_lo & ptebit) {
				count++;
				MOEA64_PTE_CLEAR(mmu, pt, &pvo->pvo_pte.lpte,
				    pvo->pvo_vpn, ptebit);
			}
		}
		pvo->pvo_pte.lpte.pte_lo &= ~ptebit;
		UNLOCK_TABLE();
	}

	vm_page_unlock_queues();
	return (count);
}

boolean_t
moea64_dev_direct_mapped(mmu_t mmu, vm_offset_t pa, vm_size_t size)
{
	struct pvo_entry *pvo;
	vm_offset_t ppa;
	int error = 0;

	PMAP_LOCK(kernel_pmap);
	for (ppa = pa & ~ADDR_POFF; ppa < pa + size; ppa += PAGE_SIZE) {
		pvo = moea64_pvo_find_va(kernel_pmap, ppa);
		if (pvo == NULL ||
		    (pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN) != ppa) {
			error = EFAULT;
			break;
		}
	}
	PMAP_UNLOCK(kernel_pmap);

	return (error);
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
moea64_mapdev_attr(mmu_t mmu, vm_offset_t pa, vm_size_t size, vm_memattr_t ma)
{
	vm_offset_t va, tmpva, ppa, offset;

	ppa = trunc_page(pa);
	offset = pa & PAGE_MASK;
	size = roundup(offset + size, PAGE_SIZE);

	va = kmem_alloc_nofault(kernel_map, size);

	if (!va)
		panic("moea64_mapdev: Couldn't alloc kernel virtual memory");

	for (tmpva = va; size > 0;) {
		moea64_kenter_attr(mmu, tmpva, ppa, ma);
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		ppa += PAGE_SIZE;
	}

	return ((void *)(va + offset));
}

void *
moea64_mapdev(mmu_t mmu, vm_offset_t pa, vm_size_t size)
{

	return moea64_mapdev_attr(mmu, pa, size, VM_MEMATTR_DEFAULT);
}

void
moea64_unmapdev(mmu_t mmu, vm_offset_t va, vm_size_t size)
{
	vm_offset_t base, offset;

	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = roundup(offset + size, PAGE_SIZE);

	kmem_free(kernel_map, base, size);
}

void
moea64_sync_icache(mmu_t mmu, pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	struct pvo_entry *pvo;
	vm_offset_t lim;
	vm_paddr_t pa;
	vm_size_t len;

	PMAP_LOCK(pm);
	while (sz > 0) {
		lim = round_page(va);
		len = MIN(lim - va, sz);
		pvo = moea64_pvo_find_va(pm, va & ~ADDR_POFF);
		if (pvo != NULL && !(pvo->pvo_pte.lpte.pte_lo & LPTE_I)) {
			pa = (pvo->pvo_pte.lpte.pte_lo & LPTE_RPGN) |
			    (va & ADDR_POFF);
			moea64_syncicache(mmu, pm, va, pa, len);
		}
		va += len;
		sz -= len;
	}
	PMAP_UNLOCK(pm);
}
