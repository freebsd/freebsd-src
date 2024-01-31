/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 2003 Peter Wemm
 * All rights reserved.
 * Copyright (c) 2005-2010 Alan L. Cox <alc@cs.rice.edu>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 */
/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * Copyright (c) 2014-2020 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Jake Burkholder,
 * Safeport Network Services, and Network Associates Laboratories, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Portions of this software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
 * the FreeBSD Foundation.
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
 */

#define	AMD64_NPT_AWARE

#include <sys/cdefs.h>
/*
 *	Manages physical address maps.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include "opt_ddb.h"
#include "opt_pmap.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/asan.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rangeset.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/smr.h>
#include <sys/sx.h>
#include <sys/turnstile.h>
#include <sys/vmem.h>
#include <sys/vmmeter.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/smp.h>
#ifdef DDB
#include <sys/kdb.h>
#include <ddb/ddb.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/vm_dumpset.h>
#include <vm/uma.h>

#include <machine/asan.h>
#include <machine/intr_machdep.h>
#include <x86/apicvar.h>
#include <x86/ifunc.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/msan.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <machine/sysarch.h>
#include <machine/tss.h>

#ifdef NUMA
#define	PMAP_MEMDOM	MAXMEMDOM
#else
#define	PMAP_MEMDOM	1
#endif

static __inline bool
pmap_type_guest(pmap_t pmap)
{

	return ((pmap->pm_type == PT_EPT) || (pmap->pm_type == PT_RVI));
}

static __inline bool
pmap_emulate_ad_bits(pmap_t pmap)
{

	return ((pmap->pm_flags & PMAP_EMULATE_AD_BITS) != 0);
}

static __inline pt_entry_t
pmap_valid_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		mask = X86_PG_V;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_EMUL_V;
		else
			mask = EPT_PG_READ;
		break;
	default:
		panic("pmap_valid_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_rw_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		mask = X86_PG_RW;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_EMUL_RW;
		else
			mask = EPT_PG_WRITE;
		break;
	default:
		panic("pmap_rw_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static pt_entry_t pg_g;

static __inline pt_entry_t
pmap_global_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
		mask = pg_g;
		break;
	case PT_RVI:
	case PT_EPT:
		mask = 0;
		break;
	default:
		panic("pmap_global_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_accessed_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		mask = X86_PG_A;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_READ;
		else
			mask = EPT_PG_A;
		break;
	default:
		panic("pmap_accessed_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_modified_bit(pmap_t pmap)
{
	pt_entry_t mask;

	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		mask = X86_PG_M;
		break;
	case PT_EPT:
		if (pmap_emulate_ad_bits(pmap))
			mask = EPT_PG_WRITE;
		else
			mask = EPT_PG_M;
		break;
	default:
		panic("pmap_modified_bit: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static __inline pt_entry_t
pmap_pku_mask_bit(pmap_t pmap)
{

	return (pmap->pm_type == PT_X86 ? X86_PG_PKU_MASK : 0);
}

static __inline bool
safe_to_clear_referenced(pmap_t pmap, pt_entry_t pte)
{

	if (!pmap_emulate_ad_bits(pmap))
		return (true);

	KASSERT(pmap->pm_type == PT_EPT, ("invalid pm_type %d", pmap->pm_type));

	/*
	 * XWR = 010 or 110 will cause an unconditional EPT misconfiguration
	 * so we don't let the referenced (aka EPT_PG_READ) bit to be cleared
	 * if the EPT_PG_WRITE bit is set.
	 */
	if ((pte & EPT_PG_WRITE) != 0)
		return (false);

	/*
	 * XWR = 100 is allowed only if the PMAP_SUPPORTS_EXEC_ONLY is set.
	 */
	if ((pte & EPT_PG_EXECUTE) == 0 ||
	    ((pmap->pm_flags & PMAP_SUPPORTS_EXEC_ONLY) != 0))
		return (true);
	else
		return (false);
}

#ifdef PV_STATS
#define PV_STAT(x)	do { x ; } while (0)
#else
#define PV_STAT(x)	do { } while (0)
#endif

#undef pa_index
#ifdef NUMA
#define	pa_index(pa)	({					\
	KASSERT((pa) <= vm_phys_segs[vm_phys_nsegs - 1].end,	\
	    ("address %lx beyond the last segment", (pa)));	\
	(pa) >> PDRSHIFT;					\
})
#define	pa_to_pmdp(pa)	(&pv_table[pa_index(pa)])
#define	pa_to_pvh(pa)	(&(pa_to_pmdp(pa)->pv_page))
#define	PHYS_TO_PV_LIST_LOCK(pa)	({			\
	struct rwlock *_lock;					\
	if (__predict_false((pa) > pmap_last_pa))		\
		_lock = &pv_dummy_large.pv_lock;		\
	else							\
		_lock = &(pa_to_pmdp(pa)->pv_lock);		\
	_lock;							\
})
#else
#define	pa_index(pa)	((pa) >> PDRSHIFT)
#define	pa_to_pvh(pa)	(&pv_table[pa_index(pa)])

#define	NPV_LIST_LOCKS	MAXCPU

#define	PHYS_TO_PV_LIST_LOCK(pa)	\
			(&pv_list_locks[pa_index(pa) % NPV_LIST_LOCKS])
#endif

#define	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa)	do {	\
	struct rwlock **_lockp = (lockp);		\
	struct rwlock *_new_lock;			\
							\
	_new_lock = PHYS_TO_PV_LIST_LOCK(pa);		\
	if (_new_lock != *_lockp) {			\
		if (*_lockp != NULL)			\
			rw_wunlock(*_lockp);		\
		*_lockp = _new_lock;			\
		rw_wlock(*_lockp);			\
	}						\
} while (0)

#define	CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m)	\
			CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, VM_PAGE_TO_PHYS(m))

#define	RELEASE_PV_LIST_LOCK(lockp)		do {	\
	struct rwlock **_lockp = (lockp);		\
							\
	if (*_lockp != NULL) {				\
		rw_wunlock(*_lockp);			\
		*_lockp = NULL;				\
	}						\
} while (0)

#define	VM_PAGE_TO_PV_LIST_LOCK(m)	\
			PHYS_TO_PV_LIST_LOCK(VM_PAGE_TO_PHYS(m))

/*
 * Statically allocate kernel pmap memory.  However, memory for
 * pm_pcids is obtained after the dynamic allocator is operational.
 * Initialize it with a non-canonical pointer to catch early accesses
 * regardless of the active mapping.
 */
struct pmap kernel_pmap_store = {
	.pm_pcidp = (void *)0xdeadbeefdeadbeef,
};

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */

int nkpt;
SYSCTL_INT(_machdep, OID_AUTO, nkpt, CTLFLAG_RD, &nkpt, 0,
    "Number of kernel page table pages allocated on bootup");

static int ndmpdp;
vm_paddr_t dmaplimit;
vm_offset_t kernel_vm_end = VM_MIN_KERNEL_ADDRESS;
pt_entry_t pg_nx;

static SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "VM/pmap parameters");

static int __read_frequently pg_ps_enabled = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, pg_ps_enabled, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &pg_ps_enabled, 0, "Are large page mappings enabled?");

int __read_frequently la57 = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, la57, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &la57, 0,
    "5-level paging for host is enabled");

static bool
pmap_is_la57(pmap_t pmap)
{
	if (pmap->pm_type == PT_X86)
		return (la57);
	return (false);		/* XXXKIB handle EPT */
}

#define	PAT_INDEX_SIZE	8
static int pat_index[PAT_INDEX_SIZE];	/* cache mode to PAT index conversion */

static u_int64_t	KPTphys;	/* phys addr of kernel level 1 */
static u_int64_t	KPDphys;	/* phys addr of kernel level 2 */
static u_int64_t	KPDPphys;	/* phys addr of kernel level 3 */
u_int64_t		KPML4phys;	/* phys addr of kernel level 4 */
u_int64_t		KPML5phys;	/* phys addr of kernel level 5,
					   if supported */

#ifdef KASAN
static uint64_t		KASANPDPphys;
#endif
#ifdef KMSAN
static uint64_t		KMSANSHADPDPphys;
static uint64_t		KMSANORIGPDPphys;

/*
 * To support systems with large amounts of memory, it is necessary to extend
 * the maximum size of the direct map.  This could eat into the space reserved
 * for the shadow map.
 */
_Static_assert(DMPML4I + NDMPML4E <= KMSANSHADPML4I, "direct map overflow");
#endif

static pml4_entry_t	*kernel_pml4;
static u_int64_t	DMPDphys;	/* phys addr of direct mapped level 2 */
static u_int64_t	DMPDPphys;	/* phys addr of direct mapped level 3 */
static int		ndmpdpphys;	/* number of DMPDPphys pages */

vm_paddr_t		kernphys;	/* phys addr of start of bootstrap data */
vm_paddr_t		KERNend;	/* and the end */

/*
 * pmap_mapdev support pre initialization (i.e. console)
 */
#define	PMAP_PREINIT_MAPPING_COUNT	8
static struct pmap_preinit_mapping {
	vm_paddr_t	pa;
	vm_offset_t	va;
	vm_size_t	sz;
	int		mode;
} pmap_preinit_mapping[PMAP_PREINIT_MAPPING_COUNT];
static int pmap_initialized;

/*
 * Data for the pv entry allocation mechanism.
 * Updates to pv_invl_gen are protected by the pv list lock but reads are not.
 */
#ifdef NUMA
static __inline int
pc_to_domain(struct pv_chunk *pc)
{

	return (vm_phys_domain(DMAP_TO_PHYS((vm_offset_t)pc)));
}
#else
static __inline int
pc_to_domain(struct pv_chunk *pc __unused)
{

	return (0);
}
#endif

struct pv_chunks_list {
	struct mtx pvc_lock;
	TAILQ_HEAD(pch, pv_chunk) pvc_list;
	int active_reclaims;
} __aligned(CACHE_LINE_SIZE);

struct pv_chunks_list __exclusive_cache_line pv_chunks[PMAP_MEMDOM];

#ifdef	NUMA
struct pmap_large_md_page {
	struct rwlock   pv_lock;
	struct md_page  pv_page;
	u_long pv_invl_gen;
};
__exclusive_cache_line static struct pmap_large_md_page pv_dummy_large;
#define pv_dummy pv_dummy_large.pv_page
__read_mostly static struct pmap_large_md_page *pv_table;
__read_mostly vm_paddr_t pmap_last_pa;
#else
static struct rwlock __exclusive_cache_line pv_list_locks[NPV_LIST_LOCKS];
static u_long pv_invl_gen[NPV_LIST_LOCKS];
static struct md_page *pv_table;
static struct md_page pv_dummy;
#endif

/*
 * All those kernel PT submaps that BSD is so fond of
 */
pt_entry_t *CMAP1 = NULL;
caddr_t CADDR1 = 0;
static vm_offset_t qframe = 0;
static struct mtx qframe_mtx;

static int pmap_flags = PMAP_PDE_SUPERPAGE;	/* flags for x86 pmaps */

static vmem_t *large_vmem;
static u_int lm_ents;
#define	PMAP_ADDRESS_IN_LARGEMAP(va)	((va) >= LARGEMAP_MIN_ADDRESS && \
	(va) < LARGEMAP_MIN_ADDRESS + NBPML4 * (u_long)lm_ents)

int pmap_pcid_enabled = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, pcid_enabled, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &pmap_pcid_enabled, 0, "Is TLB Context ID enabled ?");
int invpcid_works = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, invpcid_works, CTLFLAG_RD, &invpcid_works, 0,
    "Is the invpcid instruction available ?");
int pmap_pcid_invlpg_workaround = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, pcid_invlpg_workaround,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &pmap_pcid_invlpg_workaround, 0,
    "Enable small core PCID/INVLPG workaround");
int pmap_pcid_invlpg_workaround_uena = 1;

int __read_frequently pti = 0;
SYSCTL_INT(_vm_pmap, OID_AUTO, pti, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &pti, 0,
    "Page Table Isolation enabled");
static vm_object_t pti_obj;
static pml4_entry_t *pti_pml4;
static vm_pindex_t pti_pg_idx;
static bool pti_finalized;

struct pmap_pkru_range {
	struct rs_el	pkru_rs_el;
	u_int		pkru_keyidx;
	int		pkru_flags;
};

static uma_zone_t pmap_pkru_ranges_zone;
static bool pmap_pkru_same(pmap_t pmap, vm_offset_t sva, vm_offset_t eva);
static pt_entry_t pmap_pkru_get(pmap_t pmap, vm_offset_t va);
static void pmap_pkru_on_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva);
static void *pkru_dup_range(void *ctx, void *data);
static void pkru_free_range(void *ctx, void *node);
static int pmap_pkru_copy(pmap_t dst_pmap, pmap_t src_pmap);
static int pmap_pkru_deassign(pmap_t pmap, vm_offset_t sva, vm_offset_t eva);
static void pmap_pkru_deassign_all(pmap_t pmap);

static COUNTER_U64_DEFINE_EARLY(pcid_save_cnt);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pcid_save_cnt, CTLFLAG_RD,
    &pcid_save_cnt, "Count of saved TLB context on switch");

static LIST_HEAD(, pmap_invl_gen) pmap_invl_gen_tracker =
    LIST_HEAD_INITIALIZER(&pmap_invl_gen_tracker);
static struct mtx invl_gen_mtx;
/* Fake lock object to satisfy turnstiles interface. */
static struct lock_object invl_gen_ts = {
	.lo_name = "invlts",
};
static struct pmap_invl_gen pmap_invl_gen_head = {
	.gen = 1,
	.next = NULL,
};
static u_long pmap_invl_gen = 1;
static int pmap_invl_waiters;
static struct callout pmap_invl_callout;
static bool pmap_invl_callout_inited;

#define	PMAP_ASSERT_NOT_IN_DI() \
    KASSERT(pmap_not_in_di(), ("DI already started"))

static bool
pmap_di_locked(void)
{
	int tun;

	if ((cpu_feature2 & CPUID2_CX16) == 0)
		return (true);
	tun = 0;
	TUNABLE_INT_FETCH("vm.pmap.di_locked", &tun);
	return (tun != 0);
}

static int
sysctl_pmap_di_locked(SYSCTL_HANDLER_ARGS)
{
	int locked;

	locked = pmap_di_locked();
	return (sysctl_handle_int(oidp, &locked, 0, req));
}
SYSCTL_PROC(_vm_pmap, OID_AUTO, di_locked, CTLTYPE_INT | CTLFLAG_RDTUN |
    CTLFLAG_MPSAFE, 0, 0, sysctl_pmap_di_locked, "",
    "Locked delayed invalidation");

static bool pmap_not_in_di_l(void);
static bool pmap_not_in_di_u(void);
DEFINE_IFUNC(, bool, pmap_not_in_di, (void))
{

	return (pmap_di_locked() ? pmap_not_in_di_l : pmap_not_in_di_u);
}

static bool
pmap_not_in_di_l(void)
{
	struct pmap_invl_gen *invl_gen;

	invl_gen = &curthread->td_md.md_invl_gen;
	return (invl_gen->gen == 0);
}

static void
pmap_thread_init_invl_gen_l(struct thread *td)
{
	struct pmap_invl_gen *invl_gen;

	invl_gen = &td->td_md.md_invl_gen;
	invl_gen->gen = 0;
}

static void
pmap_delayed_invl_wait_block(u_long *m_gen, u_long *invl_gen)
{
	struct turnstile *ts;

	ts = turnstile_trywait(&invl_gen_ts);
	if (*m_gen > atomic_load_long(invl_gen))
		turnstile_wait(ts, NULL, TS_SHARED_QUEUE);
	else
		turnstile_cancel(ts);
}

static void
pmap_delayed_invl_finish_unblock(u_long new_gen)
{
	struct turnstile *ts;

	turnstile_chain_lock(&invl_gen_ts);
	ts = turnstile_lookup(&invl_gen_ts);
	if (new_gen != 0)
		pmap_invl_gen = new_gen;
	if (ts != NULL) {
		turnstile_broadcast(ts, TS_SHARED_QUEUE);
		turnstile_unpend(ts);
	}
	turnstile_chain_unlock(&invl_gen_ts);
}

/*
 * Start a new Delayed Invalidation (DI) block of code, executed by
 * the current thread.  Within a DI block, the current thread may
 * destroy both the page table and PV list entries for a mapping and
 * then release the corresponding PV list lock before ensuring that
 * the mapping is flushed from the TLBs of any processors with the
 * pmap active.
 */
static void
pmap_delayed_invl_start_l(void)
{
	struct pmap_invl_gen *invl_gen;
	u_long currgen;

	invl_gen = &curthread->td_md.md_invl_gen;
	PMAP_ASSERT_NOT_IN_DI();
	mtx_lock(&invl_gen_mtx);
	if (LIST_EMPTY(&pmap_invl_gen_tracker))
		currgen = pmap_invl_gen;
	else
		currgen = LIST_FIRST(&pmap_invl_gen_tracker)->gen;
	invl_gen->gen = currgen + 1;
	LIST_INSERT_HEAD(&pmap_invl_gen_tracker, invl_gen, link);
	mtx_unlock(&invl_gen_mtx);
}

/*
 * Finish the DI block, previously started by the current thread.  All
 * required TLB flushes for the pages marked by
 * pmap_delayed_invl_page() must be finished before this function is
 * called.
 *
 * This function works by bumping the global DI generation number to
 * the generation number of the current thread's DI, unless there is a
 * pending DI that started earlier.  In the latter case, bumping the
 * global DI generation number would incorrectly signal that the
 * earlier DI had finished.  Instead, this function bumps the earlier
 * DI's generation number to match the generation number of the
 * current thread's DI.
 */
static void
pmap_delayed_invl_finish_l(void)
{
	struct pmap_invl_gen *invl_gen, *next;

	invl_gen = &curthread->td_md.md_invl_gen;
	KASSERT(invl_gen->gen != 0, ("missed invl_start"));
	mtx_lock(&invl_gen_mtx);
	next = LIST_NEXT(invl_gen, link);
	if (next == NULL)
		pmap_delayed_invl_finish_unblock(invl_gen->gen);
	else
		next->gen = invl_gen->gen;
	LIST_REMOVE(invl_gen, link);
	mtx_unlock(&invl_gen_mtx);
	invl_gen->gen = 0;
}

static bool
pmap_not_in_di_u(void)
{
	struct pmap_invl_gen *invl_gen;

	invl_gen = &curthread->td_md.md_invl_gen;
	return (((uintptr_t)invl_gen->next & PMAP_INVL_GEN_NEXT_INVALID) != 0);
}

static void
pmap_thread_init_invl_gen_u(struct thread *td)
{
	struct pmap_invl_gen *invl_gen;

	invl_gen = &td->td_md.md_invl_gen;
	invl_gen->gen = 0;
	invl_gen->next = (void *)PMAP_INVL_GEN_NEXT_INVALID;
}

static bool
pmap_di_load_invl(struct pmap_invl_gen *ptr, struct pmap_invl_gen *out)
{
	uint64_t new_high, new_low, old_high, old_low;
	char res;

	old_low = new_low = 0;
	old_high = new_high = (uintptr_t)0;

	__asm volatile("lock;cmpxchg16b\t%1"
	    : "=@cce" (res), "+m" (*ptr), "+a" (old_low), "+d" (old_high)
	    : "b"(new_low), "c" (new_high)
	    : "memory", "cc");
	if (res == 0) {
		if ((old_high & PMAP_INVL_GEN_NEXT_INVALID) != 0)
			return (false);
		out->gen = old_low;
		out->next = (void *)old_high;
	} else {
		out->gen = new_low;
		out->next = (void *)new_high;
	}
	return (true);
}

static bool
pmap_di_store_invl(struct pmap_invl_gen *ptr, struct pmap_invl_gen *old_val,
    struct pmap_invl_gen *new_val)
{
	uint64_t new_high, new_low, old_high, old_low;
	char res;

	new_low = new_val->gen;
	new_high = (uintptr_t)new_val->next;
	old_low = old_val->gen;
	old_high = (uintptr_t)old_val->next;

	__asm volatile("lock;cmpxchg16b\t%1"
	    : "=@cce" (res), "+m" (*ptr), "+a" (old_low), "+d" (old_high)
	    : "b"(new_low), "c" (new_high)
	    : "memory", "cc");
	return (res);
}

static COUNTER_U64_DEFINE_EARLY(pv_page_count);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pv_page_count, CTLFLAG_RD,
    &pv_page_count, "Current number of allocated pv pages");

static COUNTER_U64_DEFINE_EARLY(user_pt_page_count);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, user_pt_page_count, CTLFLAG_RD,
    &user_pt_page_count,
    "Current number of allocated page table pages for userspace");

static COUNTER_U64_DEFINE_EARLY(kernel_pt_page_count);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, kernel_pt_page_count, CTLFLAG_RD,
    &kernel_pt_page_count,
    "Current number of allocated page table pages for the kernel");

#ifdef PV_STATS

static COUNTER_U64_DEFINE_EARLY(invl_start_restart);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, invl_start_restart,
    CTLFLAG_RD, &invl_start_restart,
    "Number of delayed TLB invalidation request restarts");

static COUNTER_U64_DEFINE_EARLY(invl_finish_restart);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, invl_finish_restart, CTLFLAG_RD,
    &invl_finish_restart,
    "Number of delayed TLB invalidation completion restarts");

static int invl_max_qlen;
SYSCTL_INT(_vm_pmap, OID_AUTO, invl_max_qlen, CTLFLAG_RD,
    &invl_max_qlen, 0,
    "Maximum delayed TLB invalidation request queue length");
#endif

#define di_delay	locks_delay

static void
pmap_delayed_invl_start_u(void)
{
	struct pmap_invl_gen *invl_gen, *p, prev, new_prev;
	struct thread *td;
	struct lock_delay_arg lda;
	uintptr_t prevl;
	u_char pri;
#ifdef PV_STATS
	int i, ii;
#endif

	td = curthread;
	invl_gen = &td->td_md.md_invl_gen;
	PMAP_ASSERT_NOT_IN_DI();
	lock_delay_arg_init(&lda, &di_delay);
	invl_gen->saved_pri = 0;
	pri = td->td_base_pri;
	if (pri > PVM) {
		thread_lock(td);
		pri = td->td_base_pri;
		if (pri > PVM) {
			invl_gen->saved_pri = pri;
			sched_prio(td, PVM);
		}
		thread_unlock(td);
	}
again:
	PV_STAT(i = 0);
	for (p = &pmap_invl_gen_head;; p = prev.next) {
		PV_STAT(i++);
		prevl = (uintptr_t)atomic_load_ptr(&p->next);
		if ((prevl & PMAP_INVL_GEN_NEXT_INVALID) != 0) {
			PV_STAT(counter_u64_add(invl_start_restart, 1));
			lock_delay(&lda);
			goto again;
		}
		if (prevl == 0)
			break;
		prev.next = (void *)prevl;
	}
#ifdef PV_STATS
	if ((ii = invl_max_qlen) < i)
		atomic_cmpset_int(&invl_max_qlen, ii, i);
#endif

	if (!pmap_di_load_invl(p, &prev) || prev.next != NULL) {
		PV_STAT(counter_u64_add(invl_start_restart, 1));
		lock_delay(&lda);
		goto again;
	}

	new_prev.gen = prev.gen;
	new_prev.next = invl_gen;
	invl_gen->gen = prev.gen + 1;

	/* Formal fence between store to invl->gen and updating *p. */
	atomic_thread_fence_rel();

	/*
	 * After inserting an invl_gen element with invalid bit set,
	 * this thread blocks any other thread trying to enter the
	 * delayed invalidation block.  Do not allow to remove us from
	 * the CPU, because it causes starvation for other threads.
	 */
	critical_enter();

	/*
	 * ABA for *p is not possible there, since p->gen can only
	 * increase.  So if the *p thread finished its di, then
	 * started a new one and got inserted into the list at the
	 * same place, its gen will appear greater than the previously
	 * read gen.
	 */
	if (!pmap_di_store_invl(p, &prev, &new_prev)) {
		critical_exit();
		PV_STAT(counter_u64_add(invl_start_restart, 1));
		lock_delay(&lda);
		goto again;
	}

	/*
	 * There we clear PMAP_INVL_GEN_NEXT_INVALID in
	 * invl_gen->next, allowing other threads to iterate past us.
	 * pmap_di_store_invl() provides fence between the generation
	 * write and the update of next.
	 */
	invl_gen->next = NULL;
	critical_exit();
}

static bool
pmap_delayed_invl_finish_u_crit(struct pmap_invl_gen *invl_gen,
    struct pmap_invl_gen *p)
{
	struct pmap_invl_gen prev, new_prev;
	u_long mygen;

	/*
	 * Load invl_gen->gen after setting invl_gen->next
	 * PMAP_INVL_GEN_NEXT_INVALID.  This prevents larger
	 * generations to propagate to our invl_gen->gen.  Lock prefix
	 * in atomic_set_ptr() worked as seq_cst fence.
	 */
	mygen = atomic_load_long(&invl_gen->gen);

	if (!pmap_di_load_invl(p, &prev) || prev.next != invl_gen)
		return (false);

	KASSERT(prev.gen < mygen,
	    ("invalid di gen sequence %lu %lu", prev.gen, mygen));
	new_prev.gen = mygen;
	new_prev.next = (void *)((uintptr_t)invl_gen->next &
	    ~PMAP_INVL_GEN_NEXT_INVALID);

	/* Formal fence between load of prev and storing update to it. */
	atomic_thread_fence_rel();

	return (pmap_di_store_invl(p, &prev, &new_prev));
}

static void
pmap_delayed_invl_finish_u(void)
{
	struct pmap_invl_gen *invl_gen, *p;
	struct thread *td;
	struct lock_delay_arg lda;
	uintptr_t prevl;

	td = curthread;
	invl_gen = &td->td_md.md_invl_gen;
	KASSERT(invl_gen->gen != 0, ("missed invl_start: gen 0"));
	KASSERT(((uintptr_t)invl_gen->next & PMAP_INVL_GEN_NEXT_INVALID) == 0,
	    ("missed invl_start: INVALID"));
	lock_delay_arg_init(&lda, &di_delay);

again:
	for (p = &pmap_invl_gen_head; p != NULL; p = (void *)prevl) {
		prevl = (uintptr_t)atomic_load_ptr(&p->next);
		if ((prevl & PMAP_INVL_GEN_NEXT_INVALID) != 0) {
			PV_STAT(counter_u64_add(invl_finish_restart, 1));
			lock_delay(&lda);
			goto again;
		}
		if ((void *)prevl == invl_gen)
			break;
	}

	/*
	 * It is legitimate to not find ourself on the list if a
	 * thread before us finished its DI and started it again.
	 */
	if (__predict_false(p == NULL)) {
		PV_STAT(counter_u64_add(invl_finish_restart, 1));
		lock_delay(&lda);
		goto again;
	}

	critical_enter();
	atomic_set_ptr((uintptr_t *)&invl_gen->next,
	    PMAP_INVL_GEN_NEXT_INVALID);
	if (!pmap_delayed_invl_finish_u_crit(invl_gen, p)) {
		atomic_clear_ptr((uintptr_t *)&invl_gen->next,
		    PMAP_INVL_GEN_NEXT_INVALID);
		critical_exit();
		PV_STAT(counter_u64_add(invl_finish_restart, 1));
		lock_delay(&lda);
		goto again;
	}
	critical_exit();
	if (atomic_load_int(&pmap_invl_waiters) > 0)
		pmap_delayed_invl_finish_unblock(0);
	if (invl_gen->saved_pri != 0) {
		thread_lock(td);
		sched_prio(td, invl_gen->saved_pri);
		thread_unlock(td);
	}
}

#ifdef DDB
DB_SHOW_COMMAND(di_queue, pmap_di_queue)
{
	struct pmap_invl_gen *p, *pn;
	struct thread *td;
	uintptr_t nextl;
	bool first;

	for (p = &pmap_invl_gen_head, first = true; p != NULL; p = pn,
	    first = false) {
		nextl = (uintptr_t)atomic_load_ptr(&p->next);
		pn = (void *)(nextl & ~PMAP_INVL_GEN_NEXT_INVALID);
		td = first ? NULL : __containerof(p, struct thread,
		    td_md.md_invl_gen);
		db_printf("gen %lu inv %d td %p tid %d\n", p->gen,
		    (nextl & PMAP_INVL_GEN_NEXT_INVALID) != 0, td,
		    td != NULL ? td->td_tid : -1);
	}
}
#endif

#ifdef PV_STATS
static COUNTER_U64_DEFINE_EARLY(invl_wait);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, invl_wait,
    CTLFLAG_RD, &invl_wait,
    "Number of times DI invalidation blocked pmap_remove_all/write");

static COUNTER_U64_DEFINE_EARLY(invl_wait_slow);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, invl_wait_slow, CTLFLAG_RD,
     &invl_wait_slow, "Number of slow invalidation waits for lockless DI");

#endif

#ifdef NUMA
static u_long *
pmap_delayed_invl_genp(vm_page_t m)
{
	vm_paddr_t pa;
	u_long *gen;

	pa = VM_PAGE_TO_PHYS(m);
	if (__predict_false((pa) > pmap_last_pa))
		gen = &pv_dummy_large.pv_invl_gen;
	else
		gen = &(pa_to_pmdp(pa)->pv_invl_gen);

	return (gen);
}
#else
static u_long *
pmap_delayed_invl_genp(vm_page_t m)
{

	return (&pv_invl_gen[pa_index(VM_PAGE_TO_PHYS(m)) % NPV_LIST_LOCKS]);
}
#endif

static void
pmap_delayed_invl_callout_func(void *arg __unused)
{

	if (atomic_load_int(&pmap_invl_waiters) == 0)
		return;
	pmap_delayed_invl_finish_unblock(0);
}

static void
pmap_delayed_invl_callout_init(void *arg __unused)
{

	if (pmap_di_locked())
		return;
	callout_init(&pmap_invl_callout, 1);
	pmap_invl_callout_inited = true;
}
SYSINIT(pmap_di_callout, SI_SUB_CPU + 1, SI_ORDER_ANY,
    pmap_delayed_invl_callout_init, NULL);

/*
 * Ensure that all currently executing DI blocks, that need to flush
 * TLB for the given page m, actually flushed the TLB at the time the
 * function returned.  If the page m has an empty PV list and we call
 * pmap_delayed_invl_wait(), upon its return we know that no CPU has a
 * valid mapping for the page m in either its page table or TLB.
 *
 * This function works by blocking until the global DI generation
 * number catches up with the generation number associated with the
 * given page m and its PV list.  Since this function's callers
 * typically own an object lock and sometimes own a page lock, it
 * cannot sleep.  Instead, it blocks on a turnstile to relinquish the
 * processor.
 */
static void
pmap_delayed_invl_wait_l(vm_page_t m)
{
	u_long *m_gen;
#ifdef PV_STATS
	bool accounted = false;
#endif

	m_gen = pmap_delayed_invl_genp(m);
	while (*m_gen > pmap_invl_gen) {
#ifdef PV_STATS
		if (!accounted) {
			counter_u64_add(invl_wait, 1);
			accounted = true;
		}
#endif
		pmap_delayed_invl_wait_block(m_gen, &pmap_invl_gen);
	}
}

static void
pmap_delayed_invl_wait_u(vm_page_t m)
{
	u_long *m_gen;
	struct lock_delay_arg lda;
	bool fast;

	fast = true;
	m_gen = pmap_delayed_invl_genp(m);
	lock_delay_arg_init(&lda, &di_delay);
	while (*m_gen > atomic_load_long(&pmap_invl_gen_head.gen)) {
		if (fast || !pmap_invl_callout_inited) {
			PV_STAT(counter_u64_add(invl_wait, 1));
			lock_delay(&lda);
			fast = false;
		} else {
			/*
			 * The page's invalidation generation number
			 * is still below the current thread's number.
			 * Prepare to block so that we do not waste
			 * CPU cycles or worse, suffer livelock.
			 *
			 * Since it is impossible to block without
			 * racing with pmap_delayed_invl_finish_u(),
			 * prepare for the race by incrementing
			 * pmap_invl_waiters and arming a 1-tick
			 * callout which will unblock us if we lose
			 * the race.
			 */
			atomic_add_int(&pmap_invl_waiters, 1);

			/*
			 * Re-check the current thread's invalidation
			 * generation after incrementing
			 * pmap_invl_waiters, so that there is no race
			 * with pmap_delayed_invl_finish_u() setting
			 * the page generation and checking
			 * pmap_invl_waiters.  The only race allowed
			 * is for a missed unblock, which is handled
			 * by the callout.
			 */
			if (*m_gen >
			    atomic_load_long(&pmap_invl_gen_head.gen)) {
				callout_reset(&pmap_invl_callout, 1,
				    pmap_delayed_invl_callout_func, NULL);
				PV_STAT(counter_u64_add(invl_wait_slow, 1));
				pmap_delayed_invl_wait_block(m_gen,
				    &pmap_invl_gen_head.gen);
			}
			atomic_add_int(&pmap_invl_waiters, -1);
		}
	}
}

DEFINE_IFUNC(, void, pmap_thread_init_invl_gen, (struct thread *))
{

	return (pmap_di_locked() ? pmap_thread_init_invl_gen_l :
	    pmap_thread_init_invl_gen_u);
}

DEFINE_IFUNC(static, void, pmap_delayed_invl_start, (void))
{

	return (pmap_di_locked() ? pmap_delayed_invl_start_l :
	    pmap_delayed_invl_start_u);
}

DEFINE_IFUNC(static, void, pmap_delayed_invl_finish, (void))
{

	return (pmap_di_locked() ? pmap_delayed_invl_finish_l :
	    pmap_delayed_invl_finish_u);
}

DEFINE_IFUNC(static, void, pmap_delayed_invl_wait, (vm_page_t))
{

	return (pmap_di_locked() ? pmap_delayed_invl_wait_l :
	    pmap_delayed_invl_wait_u);
}

/*
 * Mark the page m's PV list as participating in the current thread's
 * DI block.  Any threads concurrently using m's PV list to remove or
 * restrict all mappings to m will wait for the current thread's DI
 * block to complete before proceeding.
 *
 * The function works by setting the DI generation number for m's PV
 * list to at least the DI generation number of the current thread.
 * This forces a caller of pmap_delayed_invl_wait() to block until
 * current thread calls pmap_delayed_invl_finish().
 */
static void
pmap_delayed_invl_page(vm_page_t m)
{
	u_long gen, *m_gen;

	rw_assert(VM_PAGE_TO_PV_LIST_LOCK(m), RA_WLOCKED);
	gen = curthread->td_md.md_invl_gen.gen;
	if (gen == 0)
		return;
	m_gen = pmap_delayed_invl_genp(m);
	if (*m_gen < gen)
		*m_gen = gen;
}

/*
 * Crashdump maps.
 */
static caddr_t crashdumpmap;

/*
 * Internal flags for pmap_enter()'s helper functions.
 */
#define	PMAP_ENTER_NORECLAIM	0x1000000	/* Don't reclaim PV entries. */
#define	PMAP_ENTER_NOREPLACE	0x2000000	/* Don't replace mappings. */

/*
 * Internal flags for pmap_mapdev_internal() and
 * pmap_change_props_locked().
 */
#define	MAPDEV_FLUSHCACHE	0x00000001	/* Flush cache after mapping. */
#define	MAPDEV_SETATTR		0x00000002	/* Modify existing attrs. */
#define	MAPDEV_ASSERTVALID	0x00000004	/* Assert mapping validity. */

TAILQ_HEAD(pv_chunklist, pv_chunk);

static void	free_pv_chunk(struct pv_chunk *pc);
static void	free_pv_chunk_batch(struct pv_chunklist *batch);
static void	free_pv_entry(pmap_t pmap, pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t pmap, struct rwlock **lockp);
static int	popcnt_pc_map_pq(uint64_t *map);
static vm_page_t reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp);
static void	reserve_pv_entries(pmap_t pmap, int needed,
		    struct rwlock **lockp);
static void	pmap_pv_demote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
		    struct rwlock **lockp);
static bool	pmap_pv_insert_pde(pmap_t pmap, vm_offset_t va, pd_entry_t pde,
		    u_int flags, struct rwlock **lockp);
#if VM_NRESERVLEVEL > 0
static void	pmap_pv_promote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
		    struct rwlock **lockp);
#endif
static void	pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static pv_entry_t pmap_pvh_remove(struct md_page *pvh, pmap_t pmap,
		    vm_offset_t va);

static void	pmap_abort_ptp(pmap_t pmap, vm_offset_t va, vm_page_t mpte);
static int pmap_change_props_locked(vm_offset_t va, vm_size_t size,
    vm_prot_t prot, int mode, int flags);
static bool	pmap_demote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va);
static bool	pmap_demote_pde_locked(pmap_t pmap, pd_entry_t *pde,
    vm_offset_t va, struct rwlock **lockp);
static bool	pmap_demote_pdpe(pmap_t pmap, pdp_entry_t *pdpe,
    vm_offset_t va);
static int	pmap_enter_2mpage(pmap_t pmap, vm_offset_t va, vm_page_t m,
		    vm_prot_t prot, struct rwlock **lockp);
static int	pmap_enter_pde(pmap_t pmap, vm_offset_t va, pd_entry_t newpde,
		    u_int flags, vm_page_t m, struct rwlock **lockp);
static vm_page_t pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va,
    vm_page_t m, vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp);
static void pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte);
static int pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte, bool promoted,
    bool allpte_PG_A_set);
static void pmap_invalidate_cache_range_selfsnoop(vm_offset_t sva,
    vm_offset_t eva);
static void pmap_invalidate_cache_range_all(vm_offset_t sva,
    vm_offset_t eva);
static void pmap_invalidate_pde_page(pmap_t pmap, vm_offset_t va,
		    pd_entry_t pde);
static void pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode);
static vm_page_t pmap_large_map_getptp_unlocked(void);
static vm_paddr_t pmap_large_map_kextract(vm_offset_t va);
#if VM_NRESERVLEVEL > 0
static bool pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va,
    vm_page_t mpte, struct rwlock **lockp);
#endif
static bool pmap_protect_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t sva,
    vm_prot_t prot);
static void pmap_pte_props(pt_entry_t *pte, u_long bits, u_long mask);
static void pmap_pti_add_kva_locked(vm_offset_t sva, vm_offset_t eva,
    bool exec);
static pdp_entry_t *pmap_pti_pdpe(vm_offset_t va);
static pd_entry_t *pmap_pti_pde(vm_offset_t va);
static void pmap_pti_wire_pte(void *pte);
static int pmap_remove_pde(pmap_t pmap, pd_entry_t *pdq, vm_offset_t sva,
    struct spglist *free, struct rwlock **lockp);
static int pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t sva,
    pd_entry_t ptepde, struct spglist *free, struct rwlock **lockp);
static vm_page_t pmap_remove_pt_page(pmap_t pmap, vm_offset_t va);
static void pmap_remove_page(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    struct spglist *free);
static bool pmap_remove_ptes(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
		    pd_entry_t *pde, struct spglist *free,
		    struct rwlock **lockp);
static bool pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va,
    vm_page_t m, struct rwlock **lockp);
static void pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    pd_entry_t newpde);
static void pmap_update_pde_invalidate(pmap_t, vm_offset_t va, pd_entry_t pde);

static pd_entry_t *pmap_alloc_pde(pmap_t pmap, vm_offset_t va, vm_page_t *pdpgp,
		struct rwlock **lockp);
static vm_page_t pmap_allocpte_alloc(pmap_t pmap, vm_pindex_t ptepindex,
		struct rwlock **lockp, vm_offset_t va);
static vm_page_t pmap_allocpte_nosleep(pmap_t pmap, vm_pindex_t ptepindex,
		struct rwlock **lockp, vm_offset_t va);
static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va,
		struct rwlock **lockp);

static void _pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct spglist *free);
static int pmap_unuse_pt(pmap_t, vm_offset_t, pd_entry_t, struct spglist *);

static vm_page_t pmap_alloc_pt_page(pmap_t, vm_pindex_t, int);
static void pmap_free_pt_page(pmap_t, vm_page_t, bool);

/********************/
/* Inline functions */
/********************/

/*
 * Return a non-clipped indexes for a given VA, which are page table
 * pages indexes at the corresponding level.
 */
static __inline vm_pindex_t
pmap_pde_pindex(vm_offset_t va)
{
	return (va >> PDRSHIFT);
}

static __inline vm_pindex_t
pmap_pdpe_pindex(vm_offset_t va)
{
	return (NUPDE + (va >> PDPSHIFT));
}

static __inline vm_pindex_t
pmap_pml4e_pindex(vm_offset_t va)
{
	return (NUPDE + NUPDPE + (va >> PML4SHIFT));
}

static __inline vm_pindex_t
pmap_pml5e_pindex(vm_offset_t va)
{
	return (NUPDE + NUPDPE + NUPML4E + (va >> PML5SHIFT));
}

static __inline pml4_entry_t *
pmap_pml5e(pmap_t pmap, vm_offset_t va)
{

	MPASS(pmap_is_la57(pmap));
	return (&pmap->pm_pmltop[pmap_pml5e_index(va)]);
}

static __inline pml4_entry_t *
pmap_pml5e_u(pmap_t pmap, vm_offset_t va)
{

	MPASS(pmap_is_la57(pmap));
	return (&pmap->pm_pmltopu[pmap_pml5e_index(va)]);
}

static __inline pml4_entry_t *
pmap_pml5e_to_pml4e(pml5_entry_t *pml5e, vm_offset_t va)
{
	pml4_entry_t *pml4e;

	/* XXX MPASS(pmap_is_la57(pmap); */
	pml4e = (pml4_entry_t *)PHYS_TO_DMAP(*pml5e & PG_FRAME);
	return (&pml4e[pmap_pml4e_index(va)]);
}

/* Return a pointer to the PML4 slot that corresponds to a VA */
static __inline pml4_entry_t *
pmap_pml4e(pmap_t pmap, vm_offset_t va)
{
	pml5_entry_t *pml5e;
	pml4_entry_t *pml4e;
	pt_entry_t PG_V;

	if (pmap_is_la57(pmap)) {
		pml5e = pmap_pml5e(pmap, va);
		PG_V = pmap_valid_bit(pmap);
		if ((*pml5e & PG_V) == 0)
			return (NULL);
		pml4e = (pml4_entry_t *)PHYS_TO_DMAP(*pml5e & PG_FRAME);
	} else {
		pml4e = pmap->pm_pmltop;
	}
	return (&pml4e[pmap_pml4e_index(va)]);
}

static __inline pml4_entry_t *
pmap_pml4e_u(pmap_t pmap, vm_offset_t va)
{
	MPASS(!pmap_is_la57(pmap));
	return (&pmap->pm_pmltopu[pmap_pml4e_index(va)]);
}

/* Return a pointer to the PDP slot that corresponds to a VA */
static __inline pdp_entry_t *
pmap_pml4e_to_pdpe(pml4_entry_t *pml4e, vm_offset_t va)
{
	pdp_entry_t *pdpe;

	pdpe = (pdp_entry_t *)PHYS_TO_DMAP(*pml4e & PG_FRAME);
	return (&pdpe[pmap_pdpe_index(va)]);
}

/* Return a pointer to the PDP slot that corresponds to a VA */
static __inline pdp_entry_t *
pmap_pdpe(pmap_t pmap, vm_offset_t va)
{
	pml4_entry_t *pml4e;
	pt_entry_t PG_V;

	PG_V = pmap_valid_bit(pmap);
	pml4e = pmap_pml4e(pmap, va);
	if (pml4e == NULL || (*pml4e & PG_V) == 0)
		return (NULL);
	return (pmap_pml4e_to_pdpe(pml4e, va));
}

/* Return a pointer to the PD slot that corresponds to a VA */
static __inline pd_entry_t *
pmap_pdpe_to_pde(pdp_entry_t *pdpe, vm_offset_t va)
{
	pd_entry_t *pde;

	KASSERT((*pdpe & PG_PS) == 0,
	    ("%s: pdpe %#lx is a leaf", __func__, *pdpe));
	pde = (pd_entry_t *)PHYS_TO_DMAP(*pdpe & PG_FRAME);
	return (&pde[pmap_pde_index(va)]);
}

/* Return a pointer to the PD slot that corresponds to a VA */
static __inline pd_entry_t *
pmap_pde(pmap_t pmap, vm_offset_t va)
{
	pdp_entry_t *pdpe;
	pt_entry_t PG_V;

	PG_V = pmap_valid_bit(pmap);
	pdpe = pmap_pdpe(pmap, va);
	if (pdpe == NULL || (*pdpe & PG_V) == 0)
		return (NULL);
	KASSERT((*pdpe & PG_PS) == 0,
	    ("pmap_pde for 1G page, pmap %p va %#lx", pmap, va));
	return (pmap_pdpe_to_pde(pdpe, va));
}

/* Return a pointer to the PT slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_pde_to_pte(pd_entry_t *pde, vm_offset_t va)
{
	pt_entry_t *pte;

	KASSERT((*pde & PG_PS) == 0,
	    ("%s: pde %#lx is a leaf", __func__, *pde));
	pte = (pt_entry_t *)PHYS_TO_DMAP(*pde & PG_FRAME);
	return (&pte[pmap_pte_index(va)]);
}

/* Return a pointer to the PT slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t *pde;
	pt_entry_t PG_V;

	PG_V = pmap_valid_bit(pmap);
	pde = pmap_pde(pmap, va);
	if (pde == NULL || (*pde & PG_V) == 0)
		return (NULL);
	if ((*pde & PG_PS) != 0)	/* compat with i386 pmap_pte() */
		return ((pt_entry_t *)pde);
	return (pmap_pde_to_pte(pde, va));
}

static __inline void
pmap_resident_count_adj(pmap_t pmap, int count)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(pmap->pm_stats.resident_count + count >= 0,
	    ("pmap %p resident count underflow %ld %d", pmap,
	    pmap->pm_stats.resident_count, count));
	pmap->pm_stats.resident_count += count;
}

static __inline void
pmap_pt_page_count_pinit(pmap_t pmap, int count)
{
	KASSERT(pmap->pm_stats.resident_count + count >= 0,
	    ("pmap %p resident count underflow %ld %d", pmap,
	    pmap->pm_stats.resident_count, count));
	pmap->pm_stats.resident_count += count;
}

static __inline void
pmap_pt_page_count_adj(pmap_t pmap, int count)
{
	if (pmap == kernel_pmap)
		counter_u64_add(kernel_pt_page_count, count);
	else {
		if (pmap != NULL)
			pmap_resident_count_adj(pmap, count);
		counter_u64_add(user_pt_page_count, count);
	}
}

pt_entry_t vtoptem __read_mostly = ((1ul << (NPTEPGSHIFT + NPDEPGSHIFT +
    NPDPEPGSHIFT + NPML4EPGSHIFT)) - 1) << 3;
vm_offset_t PTmap __read_mostly = (vm_offset_t)P4Tmap;

pt_entry_t *
vtopte(vm_offset_t va)
{
	KASSERT(va >= VM_MAXUSER_ADDRESS, ("vtopte on a uva/gpa 0x%0lx", va));

	return ((pt_entry_t *)(PTmap + ((va >> (PAGE_SHIFT - 3)) & vtoptem)));
}

pd_entry_t vtopdem __read_mostly = ((1ul << (NPDEPGSHIFT + NPDPEPGSHIFT +
    NPML4EPGSHIFT)) - 1) << 3;
vm_offset_t PDmap __read_mostly = (vm_offset_t)P4Dmap;

static __inline pd_entry_t *
vtopde(vm_offset_t va)
{
	KASSERT(va >= VM_MAXUSER_ADDRESS, ("vtopde on a uva/gpa 0x%0lx", va));

	return ((pt_entry_t *)(PDmap + ((va >> (PDRSHIFT - 3)) & vtopdem)));
}

static u_int64_t
allocpages(vm_paddr_t *firstaddr, int n)
{
	u_int64_t ret;

	ret = *firstaddr;
	bzero((void *)ret, n * PAGE_SIZE);
	*firstaddr += n * PAGE_SIZE;
	return (ret);
}

CTASSERT(powerof2(NDMPML4E));

/* number of kernel PDP slots */
#define	NKPDPE(ptpgs)		howmany(ptpgs, NPDEPG)

static void
nkpt_init(vm_paddr_t addr)
{
	int pt_pages;

#ifdef NKPT
	pt_pages = NKPT;
#else
	pt_pages = howmany(addr - kernphys, NBPDR) + 1; /* +1 for 2M hole @0 */
	pt_pages += NKPDPE(pt_pages);

	/*
	 * Add some slop beyond the bare minimum required for bootstrapping
	 * the kernel.
	 *
	 * This is quite important when allocating KVA for kernel modules.
	 * The modules are required to be linked in the negative 2GB of
	 * the address space.  If we run out of KVA in this region then
	 * pmap_growkernel() will need to allocate page table pages to map
	 * the entire 512GB of KVA space which is an unnecessary tax on
	 * physical memory.
	 *
	 * Secondly, device memory mapped as part of setting up the low-
	 * level console(s) is taken from KVA, starting at virtual_avail.
	 * This is because cninit() is called after pmap_bootstrap() but
	 * before vm_init() and pmap_init(). 20MB for a frame buffer is
	 * not uncommon.
	 */
	pt_pages += 32;		/* 64MB additional slop. */
#endif
	nkpt = pt_pages;
}

/*
 * Returns the proper write/execute permission for a physical page that is
 * part of the initial boot allocations.
 *
 * If the page has kernel text, it is marked as read-only. If the page has
 * kernel read-only data, it is marked as read-only/not-executable. If the
 * page has only read-write data, it is marked as read-write/not-executable.
 * If the page is below/above the kernel range, it is marked as read-write.
 *
 * This function operates on 2M pages, since we map the kernel space that
 * way.
 */
static inline pt_entry_t
bootaddr_rwx(vm_paddr_t pa)
{
	/*
	 * The kernel is loaded at a 2MB-aligned address, and memory below that
	 * need not be executable.  The .bss section is padded to a 2MB
	 * boundary, so memory following the kernel need not be executable
	 * either.  Preloaded kernel modules have their mapping permissions
	 * fixed up by the linker.
	 */
	if (pa < trunc_2mpage(kernphys + btext - KERNSTART) ||
	    pa >= trunc_2mpage(kernphys + _end - KERNSTART))
		return (X86_PG_RW | pg_nx);

	/*
	 * The linker should ensure that the read-only and read-write
	 * portions don't share the same 2M page, so this shouldn't
	 * impact read-only data. However, in any case, any page with
	 * read-write data needs to be read-write.
	 */
	if (pa >= trunc_2mpage(kernphys + brwsection - KERNSTART))
		return (X86_PG_RW | pg_nx);

	/*
	 * Mark any 2M page containing kernel text as read-only. Mark
	 * other pages with read-only data as read-only and not executable.
	 * (It is likely a small portion of the read-only data section will
	 * be marked as read-only, but executable. This should be acceptable
	 * since the read-only protection will keep the data from changing.)
	 * Note that fixups to the .text section will still work until we
	 * set CR0.WP.
	 */
	if (pa < round_2mpage(kernphys + etext - KERNSTART))
		return (0);
	return (pg_nx);
}

static void
create_pagetables(vm_paddr_t *firstaddr)
{
	pd_entry_t *pd_p;
	pdp_entry_t *pdp_p;
	pml4_entry_t *p4_p;
	uint64_t DMPDkernphys;
	vm_paddr_t pax;
#ifdef KASAN
	pt_entry_t *pt_p;
	uint64_t KASANPDphys, KASANPTphys, KASANphys;
	vm_offset_t kasankernbase;
	int kasankpdpi, kasankpdi, nkasanpte;
#endif
	int i, j, ndm1g, nkpdpe, nkdmpde;

	TSENTER();
	/* Allocate page table pages for the direct map */
	ndmpdp = howmany(ptoa(Maxmem), NBPDP);
	if (ndmpdp < 4)		/* Minimum 4GB of dirmap */
		ndmpdp = 4;
	ndmpdpphys = howmany(ndmpdp, NPDPEPG);
	if (ndmpdpphys > NDMPML4E) {
		/*
		 * Each NDMPML4E allows 512 GB, so limit to that,
		 * and then readjust ndmpdp and ndmpdpphys.
		 */
		printf("NDMPML4E limits system to %d GB\n", NDMPML4E * 512);
		Maxmem = atop(NDMPML4E * NBPML4);
		ndmpdpphys = NDMPML4E;
		ndmpdp = NDMPML4E * NPDEPG;
	}
	DMPDPphys = allocpages(firstaddr, ndmpdpphys);
	ndm1g = 0;
	if ((amd_feature & AMDID_PAGE1GB) != 0) {
		/*
		 * Calculate the number of 1G pages that will fully fit in
		 * Maxmem.
		 */
		ndm1g = ptoa(Maxmem) >> PDPSHIFT;

		/*
		 * Allocate 2M pages for the kernel. These will be used in
		 * place of the one or more 1G pages from ndm1g that maps
		 * kernel memory into DMAP.
		 */
		nkdmpde = howmany((vm_offset_t)brwsection - KERNSTART +
		    kernphys - rounddown2(kernphys, NBPDP), NBPDP);
		DMPDkernphys = allocpages(firstaddr, nkdmpde);
	}
	if (ndm1g < ndmpdp)
		DMPDphys = allocpages(firstaddr, ndmpdp - ndm1g);
	dmaplimit = (vm_paddr_t)ndmpdp << PDPSHIFT;

	/* Allocate pages. */
	KPML4phys = allocpages(firstaddr, 1);
	KPDPphys = allocpages(firstaddr, NKPML4E);
#ifdef KASAN
	KASANPDPphys = allocpages(firstaddr, NKASANPML4E);
	KASANPDphys = allocpages(firstaddr, 1);
#endif
#ifdef KMSAN
	/*
	 * The KMSAN shadow maps are initially left unpopulated, since there is
	 * no need to shadow memory above KERNBASE.
	 */
	KMSANSHADPDPphys = allocpages(firstaddr, NKMSANSHADPML4E);
	KMSANORIGPDPphys = allocpages(firstaddr, NKMSANORIGPML4E);
#endif

	/*
	 * Allocate the initial number of kernel page table pages required to
	 * bootstrap.  We defer this until after all memory-size dependent
	 * allocations are done (e.g. direct map), so that we don't have to
	 * build in too much slop in our estimate.
	 *
	 * Note that when NKPML4E > 1, we have an empty page underneath
	 * all but the KPML4I'th one, so we need NKPML4E-1 extra (zeroed)
	 * pages.  (pmap_enter requires a PD page to exist for each KPML4E.)
	 */
	nkpt_init(*firstaddr);
	nkpdpe = NKPDPE(nkpt);

	KPTphys = allocpages(firstaddr, nkpt);
	KPDphys = allocpages(firstaddr, nkpdpe);

#ifdef KASAN
	nkasanpte = howmany(nkpt, KASAN_SHADOW_SCALE);
	KASANPTphys = allocpages(firstaddr, nkasanpte);
	KASANphys = allocpages(firstaddr, nkasanpte * NPTEPG);
#endif

	/*
	 * Connect the zero-filled PT pages to their PD entries.  This
	 * implicitly maps the PT pages at their correct locations within
	 * the PTmap.
	 */
	pd_p = (pd_entry_t *)KPDphys;
	for (i = 0; i < nkpt; i++)
		pd_p[i] = (KPTphys + ptoa(i)) | X86_PG_RW | X86_PG_V;

	/*
	 * Map from start of the kernel in physical memory (staging
	 * area) to the end of loader preallocated memory using 2MB
	 * pages.  This replaces some of the PD entries created above.
	 * For compatibility, identity map 2M at the start.
	 */
	pd_p[0] = X86_PG_V | PG_PS | pg_g | X86_PG_M | X86_PG_A |
	    X86_PG_RW | pg_nx;
	for (i = 1, pax = kernphys; pax < KERNend; i++, pax += NBPDR) {
		/* Preset PG_M and PG_A because demotion expects it. */
		pd_p[i] = pax | X86_PG_V | PG_PS | pg_g | X86_PG_M |
		    X86_PG_A | bootaddr_rwx(pax);
	}

	/*
	 * Because we map the physical blocks in 2M pages, adjust firstaddr
	 * to record the physical blocks we've actually mapped into kernel
	 * virtual address space.
	 */
	if (*firstaddr < round_2mpage(KERNend))
		*firstaddr = round_2mpage(KERNend);

	/* And connect up the PD to the PDP (leaving room for L4 pages) */
	pdp_p = (pdp_entry_t *)(KPDPphys + ptoa(KPML4I - KPML4BASE));
	for (i = 0; i < nkpdpe; i++)
		pdp_p[i + KPDPI] = (KPDphys + ptoa(i)) | X86_PG_RW | X86_PG_V;

#ifdef KASAN
	kasankernbase = kasan_md_addr_to_shad(KERNBASE);
	kasankpdpi = pmap_pdpe_index(kasankernbase);
	kasankpdi = pmap_pde_index(kasankernbase);

	pdp_p = (pdp_entry_t *)KASANPDPphys;
	pdp_p[kasankpdpi] = (KASANPDphys | X86_PG_RW | X86_PG_V | pg_nx);

	pd_p = (pd_entry_t *)KASANPDphys;
	for (i = 0; i < nkasanpte; i++)
		pd_p[i + kasankpdi] = (KASANPTphys + ptoa(i)) | X86_PG_RW |
		    X86_PG_V | pg_nx;

	pt_p = (pt_entry_t *)KASANPTphys;
	for (i = 0; i < nkasanpte * NPTEPG; i++)
		pt_p[i] = (KASANphys + ptoa(i)) | X86_PG_RW | X86_PG_V |
		    X86_PG_M | X86_PG_A | pg_nx;
#endif

	/*
	 * Now, set up the direct map region using 2MB and/or 1GB pages.  If
	 * the end of physical memory is not aligned to a 1GB page boundary,
	 * then the residual physical memory is mapped with 2MB pages.  Later,
	 * if pmap_mapdev{_attr}() uses the direct map for non-write-back
	 * memory, pmap_change_attr() will demote any 2MB or 1GB page mappings
	 * that are partially used. 
	 */
	pd_p = (pd_entry_t *)DMPDphys;
	for (i = NPDEPG * ndm1g, j = 0; i < NPDEPG * ndmpdp; i++, j++) {
		pd_p[j] = (vm_paddr_t)i << PDRSHIFT;
		/* Preset PG_M and PG_A because demotion expects it. */
		pd_p[j] |= X86_PG_RW | X86_PG_V | PG_PS | pg_g |
		    X86_PG_M | X86_PG_A | pg_nx;
	}
	pdp_p = (pdp_entry_t *)DMPDPphys;
	for (i = 0; i < ndm1g; i++) {
		pdp_p[i] = (vm_paddr_t)i << PDPSHIFT;
		/* Preset PG_M and PG_A because demotion expects it. */
		pdp_p[i] |= X86_PG_RW | X86_PG_V | PG_PS | pg_g |
		    X86_PG_M | X86_PG_A | pg_nx;
	}
	for (j = 0; i < ndmpdp; i++, j++) {
		pdp_p[i] = DMPDphys + ptoa(j);
		pdp_p[i] |= X86_PG_RW | X86_PG_V | pg_nx;
	}

	/*
	 * Instead of using a 1G page for the memory containing the kernel,
	 * use 2M pages with read-only and no-execute permissions.  (If using 1G
	 * pages, this will partially overwrite the PDPEs above.)
	 */
	if (ndm1g > 0) {
		pd_p = (pd_entry_t *)DMPDkernphys;
		for (i = 0, pax = rounddown2(kernphys, NBPDP);
		    i < NPDEPG * nkdmpde; i++, pax += NBPDR) {
			pd_p[i] = pax | X86_PG_V | PG_PS | pg_g | X86_PG_M |
			    X86_PG_A | pg_nx | bootaddr_rwx(pax);
		}
		j = rounddown2(kernphys, NBPDP) >> PDPSHIFT;
		for (i = 0; i < nkdmpde; i++) {
			pdp_p[i + j] = (DMPDkernphys + ptoa(i)) |
			    X86_PG_RW | X86_PG_V | pg_nx;
		}
	}

	/* And recursively map PML4 to itself in order to get PTmap */
	p4_p = (pml4_entry_t *)KPML4phys;
	p4_p[PML4PML4I] = KPML4phys;
	p4_p[PML4PML4I] |= X86_PG_RW | X86_PG_V | pg_nx;

#ifdef KASAN
	/* Connect the KASAN shadow map slots up to the PML4. */
	for (i = 0; i < NKASANPML4E; i++) {
		p4_p[KASANPML4I + i] = KASANPDPphys + ptoa(i);
		p4_p[KASANPML4I + i] |= X86_PG_RW | X86_PG_V | pg_nx;
	}
#endif

#ifdef KMSAN
	/* Connect the KMSAN shadow map slots up to the PML4. */
	for (i = 0; i < NKMSANSHADPML4E; i++) {
		p4_p[KMSANSHADPML4I + i] = KMSANSHADPDPphys + ptoa(i);
		p4_p[KMSANSHADPML4I + i] |= X86_PG_RW | X86_PG_V | pg_nx;
	}

	/* Connect the KMSAN origin map slots up to the PML4. */
	for (i = 0; i < NKMSANORIGPML4E; i++) {
		p4_p[KMSANORIGPML4I + i] = KMSANORIGPDPphys + ptoa(i);
		p4_p[KMSANORIGPML4I + i] |= X86_PG_RW | X86_PG_V | pg_nx;
	}
#endif

	/* Connect the Direct Map slots up to the PML4. */
	for (i = 0; i < ndmpdpphys; i++) {
		p4_p[DMPML4I + i] = DMPDPphys + ptoa(i);
		p4_p[DMPML4I + i] |= X86_PG_RW | X86_PG_V | pg_nx;
	}

	/* Connect the KVA slots up to the PML4 */
	for (i = 0; i < NKPML4E; i++) {
		p4_p[KPML4BASE + i] = KPDPphys + ptoa(i);
		p4_p[KPML4BASE + i] |= X86_PG_RW | X86_PG_V;
	}

	kernel_pml4 = (pml4_entry_t *)PHYS_TO_DMAP(KPML4phys);
	TSEXIT();
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *
 *	On amd64 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address "KERNBASE" to the actual
 *	(physical) address starting relative to 0]
 */
void
pmap_bootstrap(vm_paddr_t *firstaddr)
{
	vm_offset_t va;
	pt_entry_t *pte, *pcpu_pte;
	struct region_descriptor r_gdt;
	uint64_t cr4, pcpu0_phys;
	u_long res;
	int i;

	TSENTER();
	KERNend = *firstaddr;
	res = atop(KERNend - (vm_paddr_t)kernphys);

	if (!pti)
		pg_g = X86_PG_G;

	/*
	 * Create an initial set of page tables to run the kernel in.
	 */
	create_pagetables(firstaddr);

	pcpu0_phys = allocpages(firstaddr, 1);

	/*
	 * Add a physical memory segment (vm_phys_seg) corresponding to the
	 * preallocated kernel page table pages so that vm_page structures
	 * representing these pages will be created.  The vm_page structures
	 * are required for promotion of the corresponding kernel virtual
	 * addresses to superpage mappings.
	 */
	vm_phys_early_add_seg(KPTphys, KPTphys + ptoa(nkpt));

	/*
	 * Account for the virtual addresses mapped by create_pagetables().
	 */
	virtual_avail = (vm_offset_t)KERNSTART + round_2mpage(KERNend -
	    (vm_paddr_t)kernphys);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Enable PG_G global pages, then switch to the kernel page
	 * table from the bootstrap page table.  After the switch, it
	 * is possible to enable SMEP and SMAP since PG_U bits are
	 * correct now.
	 */
	cr4 = rcr4();
	cr4 |= CR4_PGE;
	load_cr4(cr4);
	load_cr3(KPML4phys);
	if (cpu_stdext_feature & CPUID_STDEXT_SMEP)
		cr4 |= CR4_SMEP;
	if (cpu_stdext_feature & CPUID_STDEXT_SMAP)
		cr4 |= CR4_SMAP;
	load_cr4(cr4);

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 * Count bootstrap data as being resident in case any of this data is
	 * later unmapped (using pmap_remove()) and freed.
	 */
	PMAP_LOCK_INIT(kernel_pmap);
	kernel_pmap->pm_pmltop = kernel_pml4;
	kernel_pmap->pm_cr3 = KPML4phys;
	kernel_pmap->pm_ucr3 = PMAP_NO_CR3;
	TAILQ_INIT(&kernel_pmap->pm_pvchunk);
	kernel_pmap->pm_stats.resident_count = res;
	vm_radix_init(&kernel_pmap->pm_root);
	kernel_pmap->pm_flags = pmap_flags;
	if ((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0) {
		rangeset_init(&kernel_pmap->pm_pkru, pkru_dup_range,
		    pkru_free_range, kernel_pmap, M_NOWAIT);
	}

	/*
	 * The kernel pmap is always active on all CPUs.  Once CPUs are
	 * enumerated, the mask will be set equal to all_cpus.
	 */
	CPU_FILL(&kernel_pmap->pm_active);

 	/*
	 * Initialize the TLB invalidations generation number lock.
	 */
	mtx_init(&invl_gen_mtx, "invlgn", NULL, MTX_DEF);

	/*
	 * Reserve some special page table entries/VA space for temporary
	 * mapping of pages.
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*PAGE_SIZE); p = pte; pte += (n);

	va = virtual_avail;
	pte = vtopte(va);

	/*
	 * Crashdump maps.  The first page is reused as CMAP1 for the
	 * memory test.
	 */
	SYSMAP(caddr_t, CMAP1, crashdumpmap, MAXDUMPPGS)
	CADDR1 = crashdumpmap;

	SYSMAP(struct pcpu *, pcpu_pte, __pcpu, MAXCPU);
	virtual_avail = va;

	/*
	 * Map the BSP PCPU now, the rest of the PCPUs are mapped by
	 * amd64_mp_alloc_pcpu()/start_all_aps() when we know the
	 * number of CPUs and NUMA affinity.
	 */
	pcpu_pte[0] = pcpu0_phys | X86_PG_V | X86_PG_RW | pg_g | pg_nx |
	    X86_PG_M | X86_PG_A;
	for (i = 1; i < MAXCPU; i++)
		pcpu_pte[i] = 0;

	/*
	 * Re-initialize PCPU area for BSP after switching.
	 * Make hardware use gdt and common_tss from the new PCPU.
	 */
	STAILQ_INIT(&cpuhead);
	wrmsr(MSR_GSBASE, (uint64_t)&__pcpu[0]);
	pcpu_init(&__pcpu[0], 0, sizeof(struct pcpu));
	amd64_bsp_pcpu_init1(&__pcpu[0]);
	amd64_bsp_ist_init(&__pcpu[0]);
	__pcpu[0].pc_common_tss.tss_iobase = sizeof(struct amd64tss) +
	    IOPERM_BITMAP_SIZE;
	memcpy(__pcpu[0].pc_gdt, temp_bsp_pcpu.pc_gdt, NGDT *
	    sizeof(struct user_segment_descriptor));
	gdt_segs[GPROC0_SEL].ssd_base = (uintptr_t)&__pcpu[0].pc_common_tss;
	ssdtosyssd(&gdt_segs[GPROC0_SEL],
	    (struct system_segment_descriptor *)&__pcpu[0].pc_gdt[GPROC0_SEL]);
	r_gdt.rd_limit = NGDT * sizeof(struct user_segment_descriptor) - 1;
	r_gdt.rd_base = (long)__pcpu[0].pc_gdt;
	lgdt(&r_gdt);
	wrmsr(MSR_GSBASE, (uint64_t)&__pcpu[0]);
	ltr(GSEL(GPROC0_SEL, SEL_KPL));
	__pcpu[0].pc_dynamic = temp_bsp_pcpu.pc_dynamic;
	__pcpu[0].pc_acpi_id = temp_bsp_pcpu.pc_acpi_id;

	/*
	 * Initialize the PAT MSR.
	 * pmap_init_pat() clears and sets CR4_PGE, which, as a
	 * side-effect, invalidates stale PG_G TLB entries that might
	 * have been created in our pre-boot environment.
	 */
	pmap_init_pat();

	/* Initialize TLB Context Id. */
	if (pmap_pcid_enabled) {
		kernel_pmap->pm_pcidp = (void *)(uintptr_t)
		    offsetof(struct pcpu, pc_kpmap_store);

		PCPU_SET(kpmap_store.pm_pcid, PMAP_PCID_KERN);
		PCPU_SET(kpmap_store.pm_gen, 1);

		/*
		 * PMAP_PCID_KERN + 1 is used for initialization of
		 * proc0 pmap.  The pmap' pcid state might be used by
		 * EFIRT entry before first context switch, so it
		 * needs to be valid.
		 */
		PCPU_SET(pcid_next, PMAP_PCID_KERN + 2);
		PCPU_SET(pcid_gen, 1);

		/*
		 * pcpu area for APs is zeroed during AP startup.
		 * pc_pcid_next and pc_pcid_gen are initialized by AP
		 * during pcpu setup.
		 */
		load_cr4(rcr4() | CR4_PCIDE);
	}
	TSEXIT();
}

/*
 * Setup the PAT MSR.
 */
void
pmap_init_pat(void)
{
	uint64_t pat_msr;
	u_long cr0, cr4;
	int i;

	/* Bail if this CPU doesn't implement PAT. */
	if ((cpu_feature & CPUID_PAT) == 0)
		panic("no PAT??");

	/* Set default PAT index table. */
	for (i = 0; i < PAT_INDEX_SIZE; i++)
		pat_index[i] = -1;
	pat_index[PAT_WRITE_BACK] = 0;
	pat_index[PAT_WRITE_THROUGH] = 1;
	pat_index[PAT_UNCACHEABLE] = 3;
	pat_index[PAT_WRITE_COMBINING] = 6;
	pat_index[PAT_WRITE_PROTECTED] = 5;
	pat_index[PAT_UNCACHED] = 2;

	/*
	 * Initialize default PAT entries.
	 * Leave the indices 0-3 at the default of WB, WT, UC-, and UC.
	 * Program 5 and 6 as WP and WC.
	 *
	 * Leave 4 and 7 as WB and UC.  Note that a recursive page table
	 * mapping for a 2M page uses a PAT value with the bit 3 set due
	 * to its overload with PG_PS.
	 */
	pat_msr = PAT_VALUE(0, PAT_WRITE_BACK) |
	    PAT_VALUE(1, PAT_WRITE_THROUGH) |
	    PAT_VALUE(2, PAT_UNCACHED) |
	    PAT_VALUE(3, PAT_UNCACHEABLE) |
	    PAT_VALUE(4, PAT_WRITE_BACK) |
	    PAT_VALUE(5, PAT_WRITE_PROTECTED) |
	    PAT_VALUE(6, PAT_WRITE_COMBINING) |
	    PAT_VALUE(7, PAT_UNCACHEABLE);

	/* Disable PGE. */
	cr4 = rcr4();
	load_cr4(cr4 & ~CR4_PGE);

	/* Disable caches (CD = 1, NW = 0). */
	cr0 = rcr0();
	load_cr0((cr0 & ~CR0_NW) | CR0_CD);

	/* Flushes caches and TLBs. */
	wbinvd();
	invltlb();

	/* Update PAT and index table. */
	wrmsr(MSR_PAT, pat_msr);

	/* Flush caches and TLBs again. */
	wbinvd();
	invltlb();

	/* Restore caches and PGE. */
	load_cr0(cr0);
	load_cr4(cr4);
}

vm_page_t
pmap_page_alloc_below_4g(bool zeroed)
{
	return (vm_page_alloc_noobj_contig((zeroed ? VM_ALLOC_ZERO : 0),
	    1, 0, (1ULL << 32), PAGE_SIZE, 0, VM_MEMATTR_DEFAULT));
}

extern const char la57_trampoline[], la57_trampoline_gdt_desc[],
    la57_trampoline_gdt[], la57_trampoline_end[];

static void
pmap_bootstrap_la57(void *arg __unused)
{
	char *v_code;
	pml5_entry_t *v_pml5;
	pml4_entry_t *v_pml4;
	pdp_entry_t *v_pdp;
	pd_entry_t *v_pd;
	pt_entry_t *v_pt;
	vm_page_t m_code, m_pml4, m_pdp, m_pd, m_pt, m_pml5;
	void (*la57_tramp)(uint64_t pml5);
	struct region_descriptor r_gdt;

	if ((cpu_stdext_feature2 & CPUID_STDEXT2_LA57) == 0)
		return;
	TUNABLE_INT_FETCH("vm.pmap.la57", &la57);
	if (!la57)
		return;

	r_gdt.rd_limit = NGDT * sizeof(struct user_segment_descriptor) - 1;
	r_gdt.rd_base = (long)__pcpu[0].pc_gdt;

	m_code = pmap_page_alloc_below_4g(true);
	v_code = (char *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_code));
	m_pml5 = pmap_page_alloc_below_4g(true);
	KPML5phys = VM_PAGE_TO_PHYS(m_pml5);
	v_pml5 = (pml5_entry_t *)PHYS_TO_DMAP(KPML5phys);
	m_pml4 = pmap_page_alloc_below_4g(true);
	v_pml4 = (pdp_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pml4));
	m_pdp = pmap_page_alloc_below_4g(true);
	v_pdp = (pdp_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pdp));
	m_pd = pmap_page_alloc_below_4g(true);
	v_pd = (pdp_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pd));
	m_pt = pmap_page_alloc_below_4g(true);
	v_pt = (pt_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pt));

	/*
	 * Map m_code 1:1, it appears below 4G in KVA due to physical
	 * address being below 4G.  Since kernel KVA is in upper half,
	 * the pml4e should be zero and free for temporary use.
	 */
	kernel_pmap->pm_pmltop[pmap_pml4e_index(VM_PAGE_TO_PHYS(m_code))] =
	    VM_PAGE_TO_PHYS(m_pdp) | X86_PG_V | X86_PG_RW | X86_PG_A |
	    X86_PG_M;
	v_pdp[pmap_pdpe_index(VM_PAGE_TO_PHYS(m_code))] =
	    VM_PAGE_TO_PHYS(m_pd) | X86_PG_V | X86_PG_RW | X86_PG_A |
	    X86_PG_M;
	v_pd[pmap_pde_index(VM_PAGE_TO_PHYS(m_code))] =
	    VM_PAGE_TO_PHYS(m_pt) | X86_PG_V | X86_PG_RW | X86_PG_A |
	    X86_PG_M;
	v_pt[pmap_pte_index(VM_PAGE_TO_PHYS(m_code))] =
	    VM_PAGE_TO_PHYS(m_code) | X86_PG_V | X86_PG_RW | X86_PG_A |
	    X86_PG_M;

	/*
	 * Add pml5 entry at top of KVA pointing to existing pml4 table,
	 * entering all existing kernel mappings into level 5 table.
	 */
	v_pml5[pmap_pml5e_index(UPT_MAX_ADDRESS)] = KPML4phys | X86_PG_V |
	    X86_PG_RW | X86_PG_A | X86_PG_M | pg_g;

	/*
	 * Add pml5 entry for 1:1 trampoline mapping after LA57 is turned on.
	 */
	v_pml5[pmap_pml5e_index(VM_PAGE_TO_PHYS(m_code))] =
	    VM_PAGE_TO_PHYS(m_pml4) | X86_PG_V | X86_PG_RW | X86_PG_A |
	    X86_PG_M;
	v_pml4[pmap_pml4e_index(VM_PAGE_TO_PHYS(m_code))] =
	    VM_PAGE_TO_PHYS(m_pdp) | X86_PG_V | X86_PG_RW | X86_PG_A |
	    X86_PG_M;

	/*
	 * Copy and call the 48->57 trampoline, hope we return there, alive.
	 */
	bcopy(la57_trampoline, v_code, la57_trampoline_end - la57_trampoline);
	*(u_long *)(v_code + 2 + (la57_trampoline_gdt_desc - la57_trampoline)) =
	    la57_trampoline_gdt - la57_trampoline + VM_PAGE_TO_PHYS(m_code);
	la57_tramp = (void (*)(uint64_t))VM_PAGE_TO_PHYS(m_code);
	invlpg((vm_offset_t)la57_tramp);
	la57_tramp(KPML5phys);

	/*
	 * gdt was necessary reset, switch back to our gdt.
	 */
	lgdt(&r_gdt);
	wrmsr(MSR_GSBASE, (uint64_t)&__pcpu[0]);
	load_ds(_udatasel);
	load_es(_udatasel);
	load_fs(_ufssel);
	ssdtosyssd(&gdt_segs[GPROC0_SEL],
	    (struct system_segment_descriptor *)&__pcpu[0].pc_gdt[GPROC0_SEL]);
	ltr(GSEL(GPROC0_SEL, SEL_KPL));

	/*
	 * Now unmap the trampoline, and free the pages.
	 * Clear pml5 entry used for 1:1 trampoline mapping.
	 */
	pte_clear(&v_pml5[pmap_pml5e_index(VM_PAGE_TO_PHYS(m_code))]);
	invlpg((vm_offset_t)v_code);
	vm_page_free(m_code);
	vm_page_free(m_pdp);
	vm_page_free(m_pd);
	vm_page_free(m_pt);

	/* 
	 * Recursively map PML5 to itself in order to get PTmap and
	 * PDmap.
	 */
	v_pml5[PML5PML5I] = KPML5phys | X86_PG_RW | X86_PG_V | pg_nx;

	vtoptem = ((1ul << (NPTEPGSHIFT + NPDEPGSHIFT + NPDPEPGSHIFT +
	    NPML4EPGSHIFT + NPML5EPGSHIFT)) - 1) << 3;
	PTmap = (vm_offset_t)P5Tmap;
	vtopdem = ((1ul << (NPDEPGSHIFT + NPDPEPGSHIFT +
	    NPML4EPGSHIFT + NPML5EPGSHIFT)) - 1) << 3;
	PDmap = (vm_offset_t)P5Dmap;

	kernel_pmap->pm_cr3 = KPML5phys;
	kernel_pmap->pm_pmltop = v_pml5;
	pmap_pt_page_count_adj(kernel_pmap, 1);
}
SYSINIT(la57, SI_SUB_KMEM, SI_ORDER_ANY, pmap_bootstrap_la57, NULL);

/*
 *	Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pat_mode = PAT_WRITE_BACK;
}

static int pmap_allow_2m_x_ept;
SYSCTL_INT(_vm_pmap, OID_AUTO, allow_2m_x_ept, CTLFLAG_RWTUN | CTLFLAG_NOFETCH,
    &pmap_allow_2m_x_ept, 0,
    "Allow executable superpage mappings in EPT");

void
pmap_allow_2m_x_ept_recalculate(void)
{
	/*
	 * SKL002, SKL012S.  Since the EPT format is only used by
	 * Intel CPUs, the vendor check is merely a formality.
	 */
	if (!(cpu_vendor_id != CPU_VENDOR_INTEL ||
	    (cpu_ia32_arch_caps & IA32_ARCH_CAP_IF_PSCHANGE_MC_NO) != 0 ||
	    (CPUID_TO_FAMILY(cpu_id) == 0x6 &&
	    (CPUID_TO_MODEL(cpu_id) == 0x26 ||	/* Atoms */
	    CPUID_TO_MODEL(cpu_id) == 0x27 ||
	    CPUID_TO_MODEL(cpu_id) == 0x35 ||
	    CPUID_TO_MODEL(cpu_id) == 0x36 ||
	    CPUID_TO_MODEL(cpu_id) == 0x37 ||
	    CPUID_TO_MODEL(cpu_id) == 0x86 ||
	    CPUID_TO_MODEL(cpu_id) == 0x1c ||
	    CPUID_TO_MODEL(cpu_id) == 0x4a ||
	    CPUID_TO_MODEL(cpu_id) == 0x4c ||
	    CPUID_TO_MODEL(cpu_id) == 0x4d ||
	    CPUID_TO_MODEL(cpu_id) == 0x5a ||
	    CPUID_TO_MODEL(cpu_id) == 0x5c ||
	    CPUID_TO_MODEL(cpu_id) == 0x5d ||
	    CPUID_TO_MODEL(cpu_id) == 0x5f ||
	    CPUID_TO_MODEL(cpu_id) == 0x6e ||
	    CPUID_TO_MODEL(cpu_id) == 0x7a ||
	    CPUID_TO_MODEL(cpu_id) == 0x57 ||	/* Knights */
	    CPUID_TO_MODEL(cpu_id) == 0x85))))
		pmap_allow_2m_x_ept = 1;
#ifndef BURN_BRIDGES
	TUNABLE_INT_FETCH("hw.allow_2m_x_ept", &pmap_allow_2m_x_ept);
#endif
	TUNABLE_INT_FETCH("vm.pmap.allow_2m_x_ept", &pmap_allow_2m_x_ept);
}

static bool
pmap_allow_2m_x_page(pmap_t pmap, bool executable)
{

	return (pmap->pm_type != PT_EPT || !executable ||
	    !pmap_allow_2m_x_ept);
}

#ifdef NUMA
static void
pmap_init_pv_table(void)
{
	struct pmap_large_md_page *pvd;
	vm_size_t s;
	long start, end, highest, pv_npg;
	int domain, i, j, pages;

	/*
	 * For correctness we depend on the size being evenly divisible into a
	 * page. As a tradeoff between performance and total memory use, the
	 * entry is 64 bytes (aka one cacheline) in size. Not being smaller
	 * avoids false-sharing, but not being 128 bytes potentially allows for
	 * avoidable traffic due to adjacent cacheline prefetcher.
	 *
	 * Assert the size so that accidental changes fail to compile.
	 */
	CTASSERT((sizeof(*pvd) == 64));

	/*
	 * Calculate the size of the array.
	 */
	pmap_last_pa = vm_phys_segs[vm_phys_nsegs - 1].end;
	pv_npg = howmany(pmap_last_pa, NBPDR);
	s = (vm_size_t)pv_npg * sizeof(struct pmap_large_md_page);
	s = round_page(s);
	pv_table = (struct pmap_large_md_page *)kva_alloc(s);
	if (pv_table == NULL)
		panic("%s: kva_alloc failed\n", __func__);

	/*
	 * Iterate physical segments to allocate space for respective pages.
	 */
	highest = -1;
	s = 0;
	for (i = 0; i < vm_phys_nsegs; i++) {
		end = vm_phys_segs[i].end / NBPDR;
		domain = vm_phys_segs[i].domain;

		if (highest >= end)
			continue;

		start = highest + 1;
		pvd = &pv_table[start];

		pages = end - start + 1;
		s = round_page(pages * sizeof(*pvd));
		highest = start + (s / sizeof(*pvd)) - 1;

		for (j = 0; j < s; j += PAGE_SIZE) {
			vm_page_t m = vm_page_alloc_noobj_domain(domain, 0);
			if (m == NULL)
				panic("failed to allocate PV table page");
			pmap_qenter((vm_offset_t)pvd + j, &m, 1);
		}

		for (j = 0; j < s / sizeof(*pvd); j++) {
			rw_init_flags(&pvd->pv_lock, "pmap pv list", RW_NEW);
			TAILQ_INIT(&pvd->pv_page.pv_list);
			pvd->pv_page.pv_gen = 0;
			pvd->pv_page.pat_mode = 0;
			pvd->pv_invl_gen = 0;
			pvd++;
		}
	}
	pvd = &pv_dummy_large;
	rw_init_flags(&pvd->pv_lock, "pmap pv list dummy", RW_NEW);
	TAILQ_INIT(&pvd->pv_page.pv_list);
	pvd->pv_page.pv_gen = 0;
	pvd->pv_page.pat_mode = 0;
	pvd->pv_invl_gen = 0;
}
#else
static void
pmap_init_pv_table(void)
{
	vm_size_t s;
	long i, pv_npg;

	/*
	 * Initialize the pool of pv list locks.
	 */
	for (i = 0; i < NPV_LIST_LOCKS; i++)
		rw_init(&pv_list_locks[i], "pmap pv list");

	/*
	 * Calculate the size of the pv head table for superpages.
	 */
	pv_npg = howmany(vm_phys_segs[vm_phys_nsegs - 1].end, NBPDR);

	/*
	 * Allocate memory for the pv head table for superpages.
	 */
	s = (vm_size_t)pv_npg * sizeof(struct md_page);
	s = round_page(s);
	pv_table = kmem_malloc(s, M_WAITOK | M_ZERO);
	for (i = 0; i < pv_npg; i++)
		TAILQ_INIT(&pv_table[i].pv_list);
	TAILQ_INIT(&pv_dummy.pv_list);
}
#endif

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
	struct pmap_preinit_mapping *ppim;
	vm_page_t m, mpte;
	int error, i, ret, skz63;

	/* L1TF, reserve page @0 unconditionally */
	vm_page_blacklist_add(0, bootverbose);

	/* Detect bare-metal Skylake Server and Skylake-X. */
	if (vm_guest == VM_GUEST_NO && cpu_vendor_id == CPU_VENDOR_INTEL &&
	    CPUID_TO_FAMILY(cpu_id) == 0x6 && CPUID_TO_MODEL(cpu_id) == 0x55) {
		/*
		 * Skylake-X errata SKZ63. Processor May Hang When
		 * Executing Code In an HLE Transaction Region between
		 * 40000000H and 403FFFFFH.
		 *
		 * Mark the pages in the range as preallocated.  It
		 * seems to be impossible to distinguish between
		 * Skylake Server and Skylake X.
		 */
		skz63 = 1;
		TUNABLE_INT_FETCH("hw.skz63_enable", &skz63);
		if (skz63 != 0) {
			if (bootverbose)
				printf("SKZ63: skipping 4M RAM starting "
				    "at physical 1G\n");
			for (i = 0; i < atop(0x400000); i++) {
				ret = vm_page_blacklist_add(0x40000000 +
				    ptoa(i), false);
				if (!ret && bootverbose)
					printf("page at %#lx already used\n",
					    0x40000000 + ptoa(i));
			}
		}
	}

	/* IFU */
	pmap_allow_2m_x_ept_recalculate();

	/*
	 * Initialize the vm page array entries for the kernel pmap's
	 * page table pages.
	 */ 
	PMAP_LOCK(kernel_pmap);
	for (i = 0; i < nkpt; i++) {
		mpte = PHYS_TO_VM_PAGE(KPTphys + (i << PAGE_SHIFT));
		KASSERT(mpte >= vm_page_array &&
		    mpte < &vm_page_array[vm_page_array_size],
		    ("pmap_init: page table page is out of range"));
		mpte->pindex = pmap_pde_pindex(KERNBASE) + i;
		mpte->phys_addr = KPTphys + (i << PAGE_SHIFT);
		mpte->ref_count = 1;

		/*
		 * Collect the page table pages that were replaced by a 2MB
		 * page in create_pagetables().  They are zero filled.
		 */
		if ((i == 0 ||
		    kernphys + ((vm_paddr_t)(i - 1) << PDRSHIFT) < KERNend) &&
		    pmap_insert_pt_page(kernel_pmap, mpte, false, false))
			panic("pmap_init: pmap_insert_pt_page failed");
	}
	PMAP_UNLOCK(kernel_pmap);
	vm_wire_add(nkpt);

	/*
	 * If the kernel is running on a virtual machine, then it must assume
	 * that MCA is enabled by the hypervisor.  Moreover, the kernel must
	 * be prepared for the hypervisor changing the vendor and family that
	 * are reported by CPUID.  Consequently, the workaround for AMD Family
	 * 10h Erratum 383 is enabled if the processor's feature set does not
	 * include at least one feature that is only supported by older Intel
	 * or newer AMD processors.
	 */
	if (vm_guest != VM_GUEST_NO && (cpu_feature & CPUID_SS) == 0 &&
	    (cpu_feature2 & (CPUID2_SSSE3 | CPUID2_SSE41 | CPUID2_AESNI |
	    CPUID2_AVX | CPUID2_XSAVE)) == 0 && (amd_feature2 & (AMDID2_XOP |
	    AMDID2_FMA4)) == 0)
		workaround_erratum383 = 1;

	/*
	 * Are large page mappings enabled?
	 */
	TUNABLE_INT_FETCH("vm.pmap.pg_ps_enabled", &pg_ps_enabled);
	if (pg_ps_enabled) {
		KASSERT(MAXPAGESIZES > 1 && pagesizes[1] == 0,
		    ("pmap_init: can't assign to pagesizes[1]"));
		pagesizes[1] = NBPDR;
		if ((amd_feature & AMDID_PAGE1GB) != 0) {
			KASSERT(MAXPAGESIZES > 2 && pagesizes[2] == 0,
			    ("pmap_init: can't assign to pagesizes[2]"));
			pagesizes[2] = NBPDP;
		}
	}

	/*
	 * Initialize pv chunk lists.
	 */
	for (i = 0; i < PMAP_MEMDOM; i++) {
		mtx_init(&pv_chunks[i].pvc_lock, "pmap pv chunk list", NULL, MTX_DEF);
		TAILQ_INIT(&pv_chunks[i].pvc_list);
	}
	pmap_init_pv_table();

	pmap_initialized = 1;
	for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
		ppim = pmap_preinit_mapping + i;
		if (ppim->va == 0)
			continue;
		/* Make the direct map consistent */
		if (ppim->pa < dmaplimit && ppim->pa + ppim->sz <= dmaplimit) {
			(void)pmap_change_attr(PHYS_TO_DMAP(ppim->pa),
			    ppim->sz, ppim->mode);
		}
		if (!bootverbose)
			continue;
		printf("PPIM %u: PA=%#lx, VA=%#lx, size=%#lx, mode=%#x\n", i,
		    ppim->pa, ppim->va, ppim->sz, ppim->mode);
	}

	mtx_init(&qframe_mtx, "qfrmlk", NULL, MTX_SPIN);
	error = vmem_alloc(kernel_arena, PAGE_SIZE, M_BESTFIT | M_WAITOK,
	    (vmem_addr_t *)&qframe);
	if (error != 0)
		panic("qframe allocation failed");

	lm_ents = 8;
	TUNABLE_INT_FETCH("vm.pmap.large_map_pml4_entries", &lm_ents);
	if (lm_ents > LMEPML4I - LMSPML4I + 1)
		lm_ents = LMEPML4I - LMSPML4I + 1;
#ifdef KMSAN
	if (lm_ents > KMSANORIGPML4I - LMSPML4I) {
		printf(
	    "pmap: shrinking large map for KMSAN (%d slots to %ld slots)\n",
		    lm_ents, KMSANORIGPML4I - LMSPML4I);
		lm_ents = KMSANORIGPML4I - LMSPML4I;
	}
#endif
	if (bootverbose)
		printf("pmap: large map %u PML4 slots (%lu GB)\n",
		    lm_ents, (u_long)lm_ents * (NBPML4 / 1024 / 1024 / 1024));
	if (lm_ents != 0) {
		large_vmem = vmem_create("large", LARGEMAP_MIN_ADDRESS,
		    (vmem_size_t)lm_ents * NBPML4, PAGE_SIZE, 0, M_WAITOK);
		if (large_vmem == NULL) {
			printf("pmap: cannot create large map\n");
			lm_ents = 0;
		}
		for (i = 0; i < lm_ents; i++) {
			m = pmap_large_map_getptp_unlocked();
			/* XXXKIB la57 */
			kernel_pml4[LMSPML4I + i] = X86_PG_V |
			    X86_PG_RW | X86_PG_A | X86_PG_M | pg_nx |
			    VM_PAGE_TO_PHYS(m);
		}
	}
}

SYSCTL_UINT(_vm_pmap, OID_AUTO, large_map_pml4_entries,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &lm_ents, 0,
    "Maximum number of PML4 entries for use by large map (tunable).  "
    "Each entry corresponds to 512GB of address space.");

static SYSCTL_NODE(_vm_pmap, OID_AUTO, pde, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "2MB page mapping counters");

static COUNTER_U64_DEFINE_EARLY(pmap_pde_demotions);
SYSCTL_COUNTER_U64(_vm_pmap_pde, OID_AUTO, demotions,
    CTLFLAG_RD, &pmap_pde_demotions, "2MB page demotions");

static COUNTER_U64_DEFINE_EARLY(pmap_pde_mappings);
SYSCTL_COUNTER_U64(_vm_pmap_pde, OID_AUTO, mappings, CTLFLAG_RD,
    &pmap_pde_mappings, "2MB page mappings");

static COUNTER_U64_DEFINE_EARLY(pmap_pde_p_failures);
SYSCTL_COUNTER_U64(_vm_pmap_pde, OID_AUTO, p_failures, CTLFLAG_RD,
    &pmap_pde_p_failures, "2MB page promotion failures");

static COUNTER_U64_DEFINE_EARLY(pmap_pde_promotions);
SYSCTL_COUNTER_U64(_vm_pmap_pde, OID_AUTO, promotions, CTLFLAG_RD,
    &pmap_pde_promotions, "2MB page promotions");

static SYSCTL_NODE(_vm_pmap, OID_AUTO, pdpe, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "1GB page mapping counters");

static COUNTER_U64_DEFINE_EARLY(pmap_pdpe_demotions);
SYSCTL_COUNTER_U64(_vm_pmap_pdpe, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_pdpe_demotions, "1GB page demotions");

/***************************************************
 * Low level helper routines.....
 ***************************************************/

static pt_entry_t
pmap_swap_pat(pmap_t pmap, pt_entry_t entry)
{
	int x86_pat_bits = X86_PG_PTE_PAT | X86_PG_PDE_PAT;

	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		/* Verify that both PAT bits are not set at the same time */
		KASSERT((entry & x86_pat_bits) != x86_pat_bits,
		    ("Invalid PAT bits in entry %#lx", entry));

		/* Swap the PAT bits if one of them is set */
		if ((entry & x86_pat_bits) != 0)
			entry ^= x86_pat_bits;
		break;
	case PT_EPT:
		/*
		 * Nothing to do - the memory attributes are represented
		 * the same way for regular pages and superpages.
		 */
		break;
	default:
		panic("pmap_switch_pat_bits: bad pm_type %d", pmap->pm_type);
	}

	return (entry);
}

bool
pmap_is_valid_memattr(pmap_t pmap __unused, vm_memattr_t mode)
{

	return (mode >= 0 && mode < PAT_INDEX_SIZE &&
	    pat_index[(int)mode] >= 0);
}

/*
 * Determine the appropriate bits to set in a PTE or PDE for a specified
 * caching mode.
 */
int
pmap_cache_bits(pmap_t pmap, int mode, bool is_pde)
{
	int cache_bits, pat_flag, pat_idx;

	if (!pmap_is_valid_memattr(pmap, mode))
		panic("Unknown caching mode %d\n", mode);

	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		/* The PAT bit is different for PTE's and PDE's. */
		pat_flag = is_pde ? X86_PG_PDE_PAT : X86_PG_PTE_PAT;

		/* Map the caching mode to a PAT index. */
		pat_idx = pat_index[mode];

		/* Map the 3-bit index value into the PAT, PCD, and PWT bits. */
		cache_bits = 0;
		if (pat_idx & 0x4)
			cache_bits |= pat_flag;
		if (pat_idx & 0x2)
			cache_bits |= PG_NC_PCD;
		if (pat_idx & 0x1)
			cache_bits |= PG_NC_PWT;
		break;

	case PT_EPT:
		cache_bits = EPT_PG_IGNORE_PAT | EPT_PG_MEMORY_TYPE(mode);
		break;

	default:
		panic("unsupported pmap type %d", pmap->pm_type);
	}

	return (cache_bits);
}

static int
pmap_cache_mask(pmap_t pmap, bool is_pde)
{
	int mask;

	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		mask = is_pde ? X86_PG_PDE_CACHE : X86_PG_PTE_CACHE;
		break;
	case PT_EPT:
		mask = EPT_PG_IGNORE_PAT | EPT_PG_MEMORY_TYPE(0x7);
		break;
	default:
		panic("pmap_cache_mask: invalid pm_type %d", pmap->pm_type);
	}

	return (mask);
}

static int
pmap_pat_index(pmap_t pmap, pt_entry_t pte, bool is_pde)
{
	int pat_flag, pat_idx;

	pat_idx = 0;
	switch (pmap->pm_type) {
	case PT_X86:
	case PT_RVI:
		/* The PAT bit is different for PTE's and PDE's. */
		pat_flag = is_pde ? X86_PG_PDE_PAT : X86_PG_PTE_PAT;

		if ((pte & pat_flag) != 0)
			pat_idx |= 0x4;
		if ((pte & PG_NC_PCD) != 0)
			pat_idx |= 0x2;
		if ((pte & PG_NC_PWT) != 0)
			pat_idx |= 0x1;
		break;
	case PT_EPT:
		if ((pte & EPT_PG_IGNORE_PAT) != 0)
			panic("EPT PTE %#lx has no PAT memory type", pte);
		pat_idx = (pte & EPT_PG_MEMORY_TYPE(0x7)) >> 3;
		break;
	}

	/* See pmap_init_pat(). */
	if (pat_idx == 4)
		pat_idx = 0;
	if (pat_idx == 7)
		pat_idx = 3;

	return (pat_idx);
}

bool
pmap_ps_enabled(pmap_t pmap)
{

	return (pg_ps_enabled && (pmap->pm_flags & PMAP_PDE_SUPERPAGE) != 0);
}

static void
pmap_update_pde_store(pmap_t pmap, pd_entry_t *pde, pd_entry_t newpde)
{

	switch (pmap->pm_type) {
	case PT_X86:
		break;
	case PT_RVI:
	case PT_EPT:
		/*
		 * XXX
		 * This is a little bogus since the generation number is
		 * supposed to be bumped up when a region of the address
		 * space is invalidated in the page tables.
		 *
		 * In this case the old PDE entry is valid but yet we want
		 * to make sure that any mappings using the old entry are
		 * invalidated in the TLB.
		 *
		 * The reason this works as expected is because we rendezvous
		 * "all" host cpus and force any vcpu context to exit as a
		 * side-effect.
		 */
		atomic_add_long(&pmap->pm_eptgen, 1);
		break;
	default:
		panic("pmap_update_pde_store: bad pm_type %d", pmap->pm_type);
	}
	pde_store(pde, newpde);
}

/*
 * After changing the page size for the specified virtual address in the page
 * table, flush the corresponding entries from the processor's TLB.  Only the
 * calling processor's TLB is affected.
 *
 * The calling thread must be pinned to a processor.
 */
static void
pmap_update_pde_invalidate(pmap_t pmap, vm_offset_t va, pd_entry_t newpde)
{
	pt_entry_t PG_G;

	if (pmap_type_guest(pmap))
		return;

	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_update_pde_invalidate: invalid type %d", pmap->pm_type));

	PG_G = pmap_global_bit(pmap);

	if ((newpde & PG_PS) == 0)
		/* Demotion: flush a specific 2MB page mapping. */
		pmap_invlpg(pmap, va);
	else if ((newpde & PG_G) == 0)
		/*
		 * Promotion: flush every 4KB page mapping from the TLB
		 * because there are too many to flush individually.
		 */
		invltlb();
	else {
		/*
		 * Promotion: flush every 4KB page mapping from the TLB,
		 * including any global (PG_G) mappings.
		 */
		invltlb_glob();
	}
}

/*
 * The amd64 pmap uses different approaches to TLB invalidation
 * depending on the kernel configuration, available hardware features,
 * and known hardware errata.  The kernel configuration option that
 * has the greatest operational impact on TLB invalidation is PTI,
 * which is enabled automatically on affected Intel CPUs.  The most
 * impactful hardware features are first PCID, and then INVPCID
 * instruction presence.  PCID usage is quite different for PTI
 * vs. non-PTI.
 *
 * * Kernel Page Table Isolation (PTI or KPTI) is used to mitigate
 *   the Meltdown bug in some Intel CPUs.  Under PTI, each user address
 *   space is served by two page tables, user and kernel.  The user
 *   page table only maps user space and a kernel trampoline.  The
 *   kernel trampoline includes the entirety of the kernel text but
 *   only the kernel data that is needed to switch from user to kernel
 *   mode.  The kernel page table maps the user and kernel address
 *   spaces in their entirety.  It is identical to the per-process
 *   page table used in non-PTI mode.
 *
 *   User page tables are only used when the CPU is in user mode.
 *   Consequently, some TLB invalidations can be postponed until the
 *   switch from kernel to user mode.  In contrast, the user
 *   space part of the kernel page table is used for copyout(9), so
 *   TLB invalidations on this page table cannot be similarly postponed.
 *
 *   The existence of a user mode page table for the given pmap is
 *   indicated by a pm_ucr3 value that differs from PMAP_NO_CR3, in
 *   which case pm_ucr3 contains the %cr3 register value for the user
 *   mode page table's root.
 *
 * * The pm_active bitmask indicates which CPUs currently have the
 *   pmap active.  A CPU's bit is set on context switch to the pmap, and
 *   cleared on switching off this CPU.  For the kernel page table,
 *   the pm_active field is immutable and contains all CPUs.  The
 *   kernel page table is always logically active on every processor,
 *   but not necessarily in use by the hardware, e.g., in PTI mode.
 *
 *   When requesting invalidation of virtual addresses with
 *   pmap_invalidate_XXX() functions, the pmap sends shootdown IPIs to
 *   all CPUs recorded as active in pm_active.  Updates to and reads
 *   from pm_active are not synchronized, and so they may race with
 *   each other.  Shootdown handlers are prepared to handle the race.
 *
 * * PCID is an optional feature of the long mode x86 MMU where TLB
 *   entries are tagged with the 'Process ID' of the address space
 *   they belong to.  This feature provides a limited namespace for
 *   process identifiers, 12 bits, supporting 4095 simultaneous IDs
 *   total.
 *
 *   Allocation of a PCID to a pmap is done by an algorithm described
 *   in section 15.12, "Other TLB Consistency Algorithms", of
 *   Vahalia's book "Unix Internals".  A PCID cannot be allocated for
 *   the whole lifetime of a pmap in pmap_pinit() due to the limited
 *   namespace.  Instead, a per-CPU, per-pmap PCID is assigned when
 *   the CPU is about to start caching TLB entries from a pmap,
 *   i.e., on the context switch that activates the pmap on the CPU.
 *
 *   The PCID allocator maintains a per-CPU, per-pmap generation
 *   count, pm_gen, which is incremented each time a new PCID is
 *   allocated.  On TLB invalidation, the generation counters for the
 *   pmap are zeroed, which signals the context switch code that the
 *   previously allocated PCID is no longer valid.  Effectively,
 *   zeroing any of these counters triggers a TLB shootdown for the
 *   given CPU/address space, due to the allocation of a new PCID.
 *
 *   Zeroing can be performed remotely.  Consequently, if a pmap is
 *   inactive on a CPU, then a TLB shootdown for that pmap and CPU can
 *   be initiated by an ordinary memory access to reset the target
 *   CPU's generation count within the pmap.  The CPU initiating the
 *   TLB shootdown does not need to send an IPI to the target CPU.
 *
 * * PTI + PCID.  The available PCIDs are divided into two sets: PCIDs
 *   for complete (kernel) page tables, and PCIDs for user mode page
 *   tables.  A user PCID value is obtained from the kernel PCID value
 *   by setting the highest bit, 11, to 1 (0x800 == PMAP_PCID_USER_PT).
 *
 *   User space page tables are activated on return to user mode, by
 *   loading pm_ucr3 into %cr3.  If the PCPU(ucr3_load_mask) requests
 *   clearing bit 63 of the loaded ucr3, this effectively causes
 *   complete invalidation of the user mode TLB entries for the
 *   current pmap.  In which case, local invalidations of individual
 *   pages in the user page table are skipped.
 *
 * * Local invalidation, all modes.  If the requested invalidation is
 *   for a specific address or the total invalidation of a currently
 *   active pmap, then the TLB is flushed using INVLPG for a kernel
 *   page table, and INVPCID(INVPCID_CTXGLOB)/invltlb_glob() for a
 *   user space page table(s).
 *
 *   If the INVPCID instruction is available, it is used to flush user
 *   entries from the kernel page table.
 *
 *   When PCID is enabled, the INVLPG instruction invalidates all TLB
 *   entries for the given page that either match the current PCID or
 *   are global. Since TLB entries for the same page under different
 *   PCIDs are unaffected, kernel pages which reside in all address
 *   spaces could be problematic.  We avoid the problem by creating
 *   all kernel PTEs with the global flag (PG_G) set, when PTI is
 *   disabled.
 *
 * * mode: PTI disabled, PCID present.  The kernel reserves PCID 0 for its
 *   address space, all other 4095 PCIDs are used for user mode spaces
 *   as described above.  A context switch allocates a new PCID if
 *   the recorded PCID is zero or the recorded generation does not match
 *   the CPU's generation, effectively flushing the TLB for this address space.
 *   Total remote invalidation is performed by zeroing pm_gen for all CPUs.
 *	local user page: INVLPG
 *	local kernel page: INVLPG
 *	local user total: INVPCID(CTX)
 *	local kernel total: INVPCID(CTXGLOB) or invltlb_glob()
 *	remote user page, inactive pmap: zero pm_gen
 *	remote user page, active pmap: zero pm_gen + IPI:INVLPG
 *	(Both actions are required to handle the aforementioned pm_active races.)
 *	remote kernel page: IPI:INVLPG
 *	remote user total, inactive pmap: zero pm_gen
 *	remote user total, active pmap: zero pm_gen + IPI:(INVPCID(CTX) or
 *          reload %cr3)
 *	(See note above about pm_active races.)
 *	remote kernel total: IPI:(INVPCID(CTXGLOB) or invltlb_glob())
 *
 * PTI enabled, PCID present.
 *	local user page: INVLPG for kpt, INVPCID(ADDR) or (INVLPG for ucr3)
 *          for upt
 *	local kernel page: INVLPG
 *	local user total: INVPCID(CTX) or reload %cr3 for kpt, clear PCID_SAVE
 *          on loading UCR3 into %cr3 for upt
 *	local kernel total: INVPCID(CTXGLOB) or invltlb_glob()
 *	remote user page, inactive pmap: zero pm_gen
 *	remote user page, active pmap: zero pm_gen + IPI:(INVLPG for kpt,
 *          INVPCID(ADDR) for upt)
 *	remote kernel page: IPI:INVLPG
 *	remote user total, inactive pmap: zero pm_gen
 *	remote user total, active pmap: zero pm_gen + IPI:(INVPCID(CTX) for kpt,
 *          clear PCID_SAVE on loading UCR3 into $cr3 for upt)
 *	remote kernel total: IPI:(INVPCID(CTXGLOB) or invltlb_glob())
 *
 *  No PCID.
 *	local user page: INVLPG
 *	local kernel page: INVLPG
 *	local user total: reload %cr3
 *	local kernel total: invltlb_glob()
 *	remote user page, inactive pmap: -
 *	remote user page, active pmap: IPI:INVLPG
 *	remote kernel page: IPI:INVLPG
 *	remote user total, inactive pmap: -
 *	remote user total, active pmap: IPI:(reload %cr3)
 *	remote kernel total: IPI:invltlb_glob()
 *  Since on return to user mode, the reload of %cr3 with ucr3 causes
 *  TLB invalidation, no specific action is required for user page table.
 *
 * EPT.  EPT pmaps do not map KVA, all mappings are userspace.
 * XXX TODO
 */

#ifdef SMP
/*
 * Interrupt the cpus that are executing in the guest context.
 * This will force the vcpu to exit and the cached EPT mappings
 * will be invalidated by the host before the next vmresume.
 */
static __inline void
pmap_invalidate_ept(pmap_t pmap)
{
	smr_seq_t goal;
	int ipinum;

	sched_pin();
	KASSERT(!CPU_ISSET(curcpu, &pmap->pm_active),
	    ("pmap_invalidate_ept: absurd pm_active"));

	/*
	 * The TLB mappings associated with a vcpu context are not
	 * flushed each time a different vcpu is chosen to execute.
	 *
	 * This is in contrast with a process's vtop mappings that
	 * are flushed from the TLB on each context switch.
	 *
	 * Therefore we need to do more than just a TLB shootdown on
	 * the active cpus in 'pmap->pm_active'. To do this we keep
	 * track of the number of invalidations performed on this pmap.
	 *
	 * Each vcpu keeps a cache of this counter and compares it
	 * just before a vmresume. If the counter is out-of-date an
	 * invept will be done to flush stale mappings from the TLB.
	 *
	 * To ensure that all vCPU threads have observed the new counter
	 * value before returning, we use SMR.  Ordering is important here:
	 * the VMM enters an SMR read section before loading the counter
	 * and after updating the pm_active bit set.  Thus, pm_active is
	 * a superset of active readers, and any reader that has observed
	 * the goal has observed the new counter value.
	 */
	atomic_add_long(&pmap->pm_eptgen, 1);

	goal = smr_advance(pmap->pm_eptsmr);

	/*
	 * Force the vcpu to exit and trap back into the hypervisor.
	 */
	ipinum = pmap->pm_flags & PMAP_NESTED_IPIMASK;
	ipi_selected(pmap->pm_active, ipinum);
	sched_unpin();

	/*
	 * Ensure that all active vCPUs will observe the new generation counter
	 * value before executing any more guest instructions.
	 */
	smr_wait(pmap->pm_eptsmr, goal);
}

static inline void
pmap_invalidate_preipi_pcid(pmap_t pmap)
{
	struct pmap_pcid *pcidp;
	u_int cpuid, i;

	sched_pin();

	cpuid = PCPU_GET(cpuid);
	if (pmap != PCPU_GET(curpmap))
		cpuid = 0xffffffff;	/* An impossible value */

	CPU_FOREACH(i) {
		if (cpuid != i) {
			pcidp = zpcpu_get_cpu(pmap->pm_pcidp, i);
			pcidp->pm_gen = 0;
		}
	}

	/*
	 * The fence is between stores to pm_gen and the read of the
	 * pm_active mask.  We need to ensure that it is impossible
	 * for us to miss the bit update in pm_active and
	 * simultaneously observe a non-zero pm_gen in
	 * pmap_activate_sw(), otherwise TLB update is missed.
	 * Without the fence, IA32 allows such an outcome.  Note that
	 * pm_active is updated by a locked operation, which provides
	 * the reciprocal fence.
	 */
	atomic_thread_fence_seq_cst();
}

static void
pmap_invalidate_preipi_nopcid(pmap_t pmap __unused)
{
	sched_pin();
}

DEFINE_IFUNC(static, void, pmap_invalidate_preipi, (pmap_t))
{
	return (pmap_pcid_enabled ? pmap_invalidate_preipi_pcid :
	    pmap_invalidate_preipi_nopcid);
}

static inline void
pmap_invalidate_page_pcid_cb(pmap_t pmap, vm_offset_t va,
    const bool invpcid_works1)
{
	struct invpcid_descr d;
	uint64_t kcr3, ucr3;
	uint32_t pcid;

	/*
	 * Because pm_pcid is recalculated on a context switch, we
	 * must ensure there is no preemption, not just pinning.
	 * Otherwise, we might use a stale value below.
	 */
	CRITICAL_ASSERT(curthread);

	/*
	 * No need to do anything with user page tables invalidation
	 * if there is no user page table, or invalidation is deferred
	 * until the return to userspace.  ucr3_load_mask is stable
	 * because we have preemption disabled.
	 */
	if (pmap->pm_ucr3 == PMAP_NO_CR3 ||
	    PCPU_GET(ucr3_load_mask) != PMAP_UCR3_NOMASK)
		return;

	pcid = pmap_get_pcid(pmap);
	if (invpcid_works1) {
		d.pcid = pcid | PMAP_PCID_USER_PT;
		d.pad = 0;
		d.addr = va;
		invpcid(&d, INVPCID_ADDR);
	} else {
		kcr3 = pmap->pm_cr3 | pcid | CR3_PCID_SAVE;
		ucr3 = pmap->pm_ucr3 | pcid | PMAP_PCID_USER_PT | CR3_PCID_SAVE;
		pmap_pti_pcid_invlpg(ucr3, kcr3, va);
	}
}

static void
pmap_invalidate_page_pcid_invpcid_cb(pmap_t pmap, vm_offset_t va)
{
	pmap_invalidate_page_pcid_cb(pmap, va, true);
}

static void
pmap_invalidate_page_pcid_noinvpcid_cb(pmap_t pmap, vm_offset_t va)
{
	pmap_invalidate_page_pcid_cb(pmap, va, false);
}

static void
pmap_invalidate_page_nopcid_cb(pmap_t pmap __unused, vm_offset_t va __unused)
{
}

DEFINE_IFUNC(static, void, pmap_invalidate_page_cb, (pmap_t, vm_offset_t))
{
	if (pmap_pcid_enabled)
		return (invpcid_works ? pmap_invalidate_page_pcid_invpcid_cb :
		    pmap_invalidate_page_pcid_noinvpcid_cb);
	return (pmap_invalidate_page_nopcid_cb);
}

static void
pmap_invalidate_page_curcpu_cb(pmap_t pmap, vm_offset_t va,
    vm_offset_t addr2 __unused)
{
	if (pmap == kernel_pmap) {
		pmap_invlpg(kernel_pmap, va);
	} else if (pmap == PCPU_GET(curpmap)) {
		invlpg(va);
		pmap_invalidate_page_cb(pmap, va);
	}
}

void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	if (pmap_type_guest(pmap)) {
		pmap_invalidate_ept(pmap);
		return;
	}

	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_invalidate_page: invalid type %d", pmap->pm_type));

	pmap_invalidate_preipi(pmap);
	smp_masked_invlpg(va, pmap, pmap_invalidate_page_curcpu_cb);
}

/* 4k PTEs -- Chosen to exceed the total size of Broadwell L2 TLB */
#define	PMAP_INVLPG_THRESHOLD	(4 * 1024 * PAGE_SIZE)

static void
pmap_invalidate_range_pcid_cb(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
    const bool invpcid_works1)
{
	struct invpcid_descr d;
	uint64_t kcr3, ucr3;
	uint32_t pcid;

	CRITICAL_ASSERT(curthread);

	if (pmap != PCPU_GET(curpmap) ||
	    pmap->pm_ucr3 == PMAP_NO_CR3 ||
	    PCPU_GET(ucr3_load_mask) != PMAP_UCR3_NOMASK)
		return;

	pcid = pmap_get_pcid(pmap);
	if (invpcid_works1) {
		d.pcid = pcid | PMAP_PCID_USER_PT;
		d.pad = 0;
		for (d.addr = sva; d.addr < eva; d.addr += PAGE_SIZE)
			invpcid(&d, INVPCID_ADDR);
	} else {
		kcr3 = pmap->pm_cr3 | pcid | CR3_PCID_SAVE;
		ucr3 = pmap->pm_ucr3 | pcid | PMAP_PCID_USER_PT | CR3_PCID_SAVE;
		pmap_pti_pcid_invlrng(ucr3, kcr3, sva, eva);
	}
}

static void
pmap_invalidate_range_pcid_invpcid_cb(pmap_t pmap, vm_offset_t sva,
    vm_offset_t eva)
{
	pmap_invalidate_range_pcid_cb(pmap, sva, eva, true);
}

static void
pmap_invalidate_range_pcid_noinvpcid_cb(pmap_t pmap, vm_offset_t sva,
    vm_offset_t eva)
{
	pmap_invalidate_range_pcid_cb(pmap, sva, eva, false);
}

static void
pmap_invalidate_range_nopcid_cb(pmap_t pmap __unused, vm_offset_t sva __unused,
    vm_offset_t eva __unused)
{
}

DEFINE_IFUNC(static, void, pmap_invalidate_range_cb, (pmap_t, vm_offset_t,
    vm_offset_t))
{
	if (pmap_pcid_enabled)
		return (invpcid_works ? pmap_invalidate_range_pcid_invpcid_cb :
		    pmap_invalidate_range_pcid_noinvpcid_cb);
	return (pmap_invalidate_range_nopcid_cb);
}

static void
pmap_invalidate_range_curcpu_cb(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t addr;

	if (pmap == kernel_pmap) {
		if (PCPU_GET(pcid_invlpg_workaround)) {
			struct invpcid_descr d = { 0 };

			invpcid(&d, INVPCID_CTXGLOB);
		} else {
			for (addr = sva; addr < eva; addr += PAGE_SIZE)
				invlpg(addr);
		}
	} else if (pmap == PCPU_GET(curpmap)) {
		for (addr = sva; addr < eva; addr += PAGE_SIZE)
			invlpg(addr);
		pmap_invalidate_range_cb(pmap, sva, eva);
	}
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	if (eva - sva >= PMAP_INVLPG_THRESHOLD) {
		pmap_invalidate_all(pmap);
		return;
	}

	if (pmap_type_guest(pmap)) {
		pmap_invalidate_ept(pmap);
		return;
	}

	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_invalidate_range: invalid type %d", pmap->pm_type));

	pmap_invalidate_preipi(pmap);
	smp_masked_invlpg_range(sva, eva, pmap,
	    pmap_invalidate_range_curcpu_cb);
}

static inline void
pmap_invalidate_all_pcid_cb(pmap_t pmap, bool invpcid_works1)
{
	struct invpcid_descr d;
	uint64_t kcr3;
	uint32_t pcid;

	if (pmap == kernel_pmap) {
		if (invpcid_works1) {
			bzero(&d, sizeof(d));
			invpcid(&d, INVPCID_CTXGLOB);
		} else {
			invltlb_glob();
		}
	} else if (pmap == PCPU_GET(curpmap)) {
		CRITICAL_ASSERT(curthread);

		pcid = pmap_get_pcid(pmap);
		if (invpcid_works1) {
			d.pcid = pcid;
			d.pad = 0;
			d.addr = 0;
			invpcid(&d, INVPCID_CTX);
		} else {
			kcr3 = pmap->pm_cr3 | pcid;
			load_cr3(kcr3);
		}
		if (pmap->pm_ucr3 != PMAP_NO_CR3)
			PCPU_SET(ucr3_load_mask, ~CR3_PCID_SAVE);
	}
}

static void
pmap_invalidate_all_pcid_invpcid_cb(pmap_t pmap)
{
	pmap_invalidate_all_pcid_cb(pmap, true);
}

static void
pmap_invalidate_all_pcid_noinvpcid_cb(pmap_t pmap)
{
	pmap_invalidate_all_pcid_cb(pmap, false);
}

static void
pmap_invalidate_all_nopcid_cb(pmap_t pmap)
{
	if (pmap == kernel_pmap)
		invltlb_glob();
	else if (pmap == PCPU_GET(curpmap))
		invltlb();
}

DEFINE_IFUNC(static, void, pmap_invalidate_all_cb, (pmap_t))
{
	if (pmap_pcid_enabled)
		return (invpcid_works ? pmap_invalidate_all_pcid_invpcid_cb :
		    pmap_invalidate_all_pcid_noinvpcid_cb);
	return (pmap_invalidate_all_nopcid_cb);
}

static void
pmap_invalidate_all_curcpu_cb(pmap_t pmap, vm_offset_t addr1 __unused,
    vm_offset_t addr2 __unused)
{
	pmap_invalidate_all_cb(pmap);
}

void
pmap_invalidate_all(pmap_t pmap)
{
	if (pmap_type_guest(pmap)) {
		pmap_invalidate_ept(pmap);
		return;
	}

	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_invalidate_all: invalid type %d", pmap->pm_type));

	pmap_invalidate_preipi(pmap);
	smp_masked_invltlb(pmap, pmap_invalidate_all_curcpu_cb);
}

static void
pmap_invalidate_cache_curcpu_cb(pmap_t pmap __unused, vm_offset_t va __unused,
    vm_offset_t addr2 __unused)
{
	wbinvd();
}

void
pmap_invalidate_cache(void)
{
	sched_pin();
	smp_cache_flush(pmap_invalidate_cache_curcpu_cb);
}

struct pde_action {
	cpuset_t invalidate;	/* processors that invalidate their TLB */
	pmap_t pmap;
	vm_offset_t va;
	pd_entry_t *pde;
	pd_entry_t newpde;
	u_int store;		/* processor that updates the PDE */
};

static void
pmap_update_pde_action(void *arg)
{
	struct pde_action *act = arg;

	if (act->store == PCPU_GET(cpuid))
		pmap_update_pde_store(act->pmap, act->pde, act->newpde);
}

static void
pmap_update_pde_teardown(void *arg)
{
	struct pde_action *act = arg;

	if (CPU_ISSET(PCPU_GET(cpuid), &act->invalidate))
		pmap_update_pde_invalidate(act->pmap, act->va, act->newpde);
}

/*
 * Change the page size for the specified virtual address in a way that
 * prevents any possibility of the TLB ever having two entries that map the
 * same virtual address using different page sizes.  This is the recommended
 * workaround for Erratum 383 on AMD Family 10h processors.  It prevents a
 * machine check exception for a TLB state that is improperly diagnosed as a
 * hardware error.
 */
static void
pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde, pd_entry_t newpde)
{
	struct pde_action act;
	cpuset_t active, other_cpus;
	u_int cpuid;

	sched_pin();
	cpuid = PCPU_GET(cpuid);
	other_cpus = all_cpus;
	CPU_CLR(cpuid, &other_cpus);
	if (pmap == kernel_pmap || pmap_type_guest(pmap)) 
		active = all_cpus;
	else {
		active = pmap->pm_active;
	}
	if (CPU_OVERLAP(&active, &other_cpus)) { 
		act.store = cpuid;
		act.invalidate = active;
		act.va = va;
		act.pmap = pmap;
		act.pde = pde;
		act.newpde = newpde;
		CPU_SET(cpuid, &active);
		smp_rendezvous_cpus(active,
		    smp_no_rendezvous_barrier, pmap_update_pde_action,
		    pmap_update_pde_teardown, &act);
	} else {
		pmap_update_pde_store(pmap, pde, newpde);
		if (CPU_ISSET(cpuid, &active))
			pmap_update_pde_invalidate(pmap, va, newpde);
	}
	sched_unpin();
}
#else /* !SMP */
/*
 * Normal, non-SMP, invalidation functions.
 */
void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	struct invpcid_descr d;
	struct pmap_pcid *pcidp;
	uint64_t kcr3, ucr3;
	uint32_t pcid;

	if (pmap->pm_type == PT_RVI || pmap->pm_type == PT_EPT) {
		pmap->pm_eptgen++;
		return;
	}
	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_invalidate_range: unknown type %d", pmap->pm_type));

	if (pmap == kernel_pmap || pmap == PCPU_GET(curpmap)) {
		invlpg(va);
		if (pmap == PCPU_GET(curpmap) && pmap_pcid_enabled &&
		    pmap->pm_ucr3 != PMAP_NO_CR3) {
			critical_enter();
			pcid = pmap_get_pcid(pmap);
			if (invpcid_works) {
				d.pcid = pcid | PMAP_PCID_USER_PT;
				d.pad = 0;
				d.addr = va;
				invpcid(&d, INVPCID_ADDR);
			} else {
				kcr3 = pmap->pm_cr3 | pcid | CR3_PCID_SAVE;
				ucr3 = pmap->pm_ucr3 | pcid |
				    PMAP_PCID_USER_PT | CR3_PCID_SAVE;
				pmap_pti_pcid_invlpg(ucr3, kcr3, va);
			}
			critical_exit();
		}
	} else if (pmap_pcid_enabled) {
		pcidp = zpcpu_get(pmap->pm_pcidp);
		pcidp->pm_gen = 0;
	}
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	struct invpcid_descr d;
	struct pmap_pcid *pcidp;
	vm_offset_t addr;
	uint64_t kcr3, ucr3;
	uint32_t pcid;

	if (pmap->pm_type == PT_RVI || pmap->pm_type == PT_EPT) {
		pmap->pm_eptgen++;
		return;
	}
	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_invalidate_range: unknown type %d", pmap->pm_type));

	if (pmap == kernel_pmap || pmap == PCPU_GET(curpmap)) {
		for (addr = sva; addr < eva; addr += PAGE_SIZE)
			invlpg(addr);
		if (pmap == PCPU_GET(curpmap) && pmap_pcid_enabled &&
		    pmap->pm_ucr3 != PMAP_NO_CR3) {
			critical_enter();
			pcid = pmap_get_pcid(pmap);
			if (invpcid_works) {
				d.pcid = pcid | PMAP_PCID_USER_PT;
				d.pad = 0;
				d.addr = sva;
				for (; d.addr < eva; d.addr += PAGE_SIZE)
					invpcid(&d, INVPCID_ADDR);
			} else {
				kcr3 = pmap->pm_cr3 | pcid | CR3_PCID_SAVE;
				ucr3 = pmap->pm_ucr3 | pcid |
				    PMAP_PCID_USER_PT | CR3_PCID_SAVE;
				pmap_pti_pcid_invlrng(ucr3, kcr3, sva, eva);
			}
			critical_exit();
		}
	} else if (pmap_pcid_enabled) {
		pcidp = zpcpu_get(pmap->pm_pcidp);
		pcidp->pm_gen = 0;
	}
}

void
pmap_invalidate_all(pmap_t pmap)
{
	struct invpcid_descr d;
	struct pmap_pcid *pcidp;
	uint64_t kcr3, ucr3;
	uint32_t pcid;

	if (pmap->pm_type == PT_RVI || pmap->pm_type == PT_EPT) {
		pmap->pm_eptgen++;
		return;
	}
	KASSERT(pmap->pm_type == PT_X86,
	    ("pmap_invalidate_all: unknown type %d", pmap->pm_type));

	if (pmap == kernel_pmap) {
		if (pmap_pcid_enabled && invpcid_works) {
			bzero(&d, sizeof(d));
			invpcid(&d, INVPCID_CTXGLOB);
		} else {
			invltlb_glob();
		}
	} else if (pmap == PCPU_GET(curpmap)) {
		if (pmap_pcid_enabled) {
			critical_enter();
			pcid = pmap_get_pcid(pmap);
			if (invpcid_works) {
				d.pcid = pcid;
				d.pad = 0;
				d.addr = 0;
				invpcid(&d, INVPCID_CTX);
				if (pmap->pm_ucr3 != PMAP_NO_CR3) {
					d.pcid |= PMAP_PCID_USER_PT;
					invpcid(&d, INVPCID_CTX);
				}
			} else {
				kcr3 = pmap->pm_cr3 | pcid;
				if (pmap->pm_ucr3 != PMAP_NO_CR3) {
					ucr3 = pmap->pm_ucr3 | pcid |
					    PMAP_PCID_USER_PT;
					pmap_pti_pcid_invalidate(ucr3, kcr3);
				} else
					load_cr3(kcr3);
			}
			critical_exit();
		} else {
			invltlb();
		}
	} else if (pmap_pcid_enabled) {
		pcidp = zpcpu_get(pmap->pm_pcidp);
		pcidp->pm_gen = 0;
	}
}

void
pmap_invalidate_cache(void)
{

	wbinvd();
}

static void
pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde, pd_entry_t newpde)
{
	struct pmap_pcid *pcidp;

	pmap_update_pde_store(pmap, pde, newpde);
	if (pmap == kernel_pmap || pmap == PCPU_GET(curpmap))
		pmap_update_pde_invalidate(pmap, va, newpde);
	else {
		pcidp = zpcpu_get(pmap->pm_pcidp);
		pcidp->pm_gen = 0;
	}
}
#endif /* !SMP */

static void
pmap_invalidate_pde_page(pmap_t pmap, vm_offset_t va, pd_entry_t pde)
{

	/*
	 * When the PDE has PG_PROMOTED set, the 2MB page mapping was created
	 * by a promotion that did not invalidate the 512 4KB page mappings
	 * that might exist in the TLB.  Consequently, at this point, the TLB
	 * may hold both 4KB and 2MB page mappings for the address range [va,
	 * va + NBPDR).  Therefore, the entire range must be invalidated here.
	 * In contrast, when PG_PROMOTED is clear, the TLB will not hold any
	 * 4KB page mappings for the address range [va, va + NBPDR), and so a
	 * single INVLPG suffices to invalidate the 2MB page mapping from the
	 * TLB.
	 */
	if ((pde & PG_PROMOTED) != 0)
		pmap_invalidate_range(pmap, va, va + NBPDR - 1);
	else
		pmap_invalidate_page(pmap, va);
}

DEFINE_IFUNC(, void, pmap_invalidate_cache_range,
    (vm_offset_t sva, vm_offset_t eva))
{

	if ((cpu_feature & CPUID_SS) != 0)
		return (pmap_invalidate_cache_range_selfsnoop);
	if ((cpu_feature & CPUID_CLFSH) != 0)
		return (pmap_force_invalidate_cache_range);
	return (pmap_invalidate_cache_range_all);
}

#define PMAP_CLFLUSH_THRESHOLD   (2 * 1024 * 1024)

static void
pmap_invalidate_cache_range_check_align(vm_offset_t sva, vm_offset_t eva)
{

	KASSERT((sva & PAGE_MASK) == 0,
	    ("pmap_invalidate_cache_range: sva not page-aligned"));
	KASSERT((eva & PAGE_MASK) == 0,
	    ("pmap_invalidate_cache_range: eva not page-aligned"));
}

static void
pmap_invalidate_cache_range_selfsnoop(vm_offset_t sva, vm_offset_t eva)
{

	pmap_invalidate_cache_range_check_align(sva, eva);
}

void
pmap_force_invalidate_cache_range(vm_offset_t sva, vm_offset_t eva)
{

	sva &= ~(vm_offset_t)(cpu_clflush_line_size - 1);

	/*
	 * XXX: Some CPUs fault, hang, or trash the local APIC
	 * registers if we use CLFLUSH on the local APIC range.  The
	 * local APIC is always uncached, so we don't need to flush
	 * for that range anyway.
	 */
	if (pmap_kextract(sva) == lapic_paddr)
		return;

	if ((cpu_stdext_feature & CPUID_STDEXT_CLFLUSHOPT) != 0) {
		/*
		 * Do per-cache line flush.  Use a locked
		 * instruction to insure that previous stores are
		 * included in the write-back.  The processor
		 * propagates flush to other processors in the cache
		 * coherence domain.
		 */
		atomic_thread_fence_seq_cst();
		for (; sva < eva; sva += cpu_clflush_line_size)
			clflushopt(sva);
		atomic_thread_fence_seq_cst();
	} else {
		/*
		 * Writes are ordered by CLFLUSH on Intel CPUs.
		 */
		if (cpu_vendor_id != CPU_VENDOR_INTEL)
			mfence();
		for (; sva < eva; sva += cpu_clflush_line_size)
			clflush(sva);
		if (cpu_vendor_id != CPU_VENDOR_INTEL)
			mfence();
	}
}

static void
pmap_invalidate_cache_range_all(vm_offset_t sva, vm_offset_t eva)
{

	pmap_invalidate_cache_range_check_align(sva, eva);
	pmap_invalidate_cache();
}

/*
 * Remove the specified set of pages from the data and instruction caches.
 *
 * In contrast to pmap_invalidate_cache_range(), this function does not
 * rely on the CPU's self-snoop feature, because it is intended for use
 * when moving pages into a different cache domain.
 */
void
pmap_invalidate_cache_pages(vm_page_t *pages, int count)
{
	vm_offset_t daddr, eva;
	int i;
	bool useclflushopt;

	useclflushopt = (cpu_stdext_feature & CPUID_STDEXT_CLFLUSHOPT) != 0;
	if (count >= PMAP_CLFLUSH_THRESHOLD / PAGE_SIZE ||
	    ((cpu_feature & CPUID_CLFSH) == 0 && !useclflushopt))
		pmap_invalidate_cache();
	else {
		if (useclflushopt)
			atomic_thread_fence_seq_cst();
		else if (cpu_vendor_id != CPU_VENDOR_INTEL)
			mfence();
		for (i = 0; i < count; i++) {
			daddr = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pages[i]));
			eva = daddr + PAGE_SIZE;
			for (; daddr < eva; daddr += cpu_clflush_line_size) {
				if (useclflushopt)
					clflushopt(daddr);
				else
					clflush(daddr);
			}
		}
		if (useclflushopt)
			atomic_thread_fence_seq_cst();
		else if (cpu_vendor_id != CPU_VENDOR_INTEL)
			mfence();
	}
}

void
pmap_flush_cache_range(vm_offset_t sva, vm_offset_t eva)
{

	pmap_invalidate_cache_range_check_align(sva, eva);

	if ((cpu_stdext_feature & CPUID_STDEXT_CLWB) == 0) {
		pmap_force_invalidate_cache_range(sva, eva);
		return;
	}

	/* See comment in pmap_force_invalidate_cache_range(). */
	if (pmap_kextract(sva) == lapic_paddr)
		return;

	atomic_thread_fence_seq_cst();
	for (; sva < eva; sva += cpu_clflush_line_size)
		clwb(sva);
	atomic_thread_fence_seq_cst();
}

void
pmap_flush_cache_phys_range(vm_paddr_t spa, vm_paddr_t epa, vm_memattr_t mattr)
{
	pt_entry_t *pte;
	vm_offset_t vaddr;
	int error __diagused;
	int pte_bits;

	KASSERT((spa & PAGE_MASK) == 0,
	    ("pmap_flush_cache_phys_range: spa not page-aligned"));
	KASSERT((epa & PAGE_MASK) == 0,
	    ("pmap_flush_cache_phys_range: epa not page-aligned"));

	if (spa < dmaplimit) {
		pmap_flush_cache_range(PHYS_TO_DMAP(spa), PHYS_TO_DMAP(MIN(
		    dmaplimit, epa)));
		if (dmaplimit >= epa)
			return;
		spa = dmaplimit;
	}

	pte_bits = pmap_cache_bits(kernel_pmap, mattr, false) | X86_PG_RW |
	    X86_PG_V;
	error = vmem_alloc(kernel_arena, PAGE_SIZE, M_BESTFIT | M_WAITOK,
	    &vaddr);
	KASSERT(error == 0, ("vmem_alloc failed: %d", error));
	pte = vtopte(vaddr);
	for (; spa < epa; spa += PAGE_SIZE) {
		sched_pin();
		pte_store(pte, spa | pte_bits);
		pmap_invlpg(kernel_pmap, vaddr);
		/* XXXKIB atomic inside flush_cache_range are excessive */
		pmap_flush_cache_range(vaddr, vaddr + PAGE_SIZE);
		sched_unpin();
	}
	vmem_free(kernel_arena, vaddr, PAGE_SIZE);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_paddr_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	vm_paddr_t pa;

	pa = 0;
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK(pmap);
	pdpe = pmap_pdpe(pmap, va);
	if (pdpe != NULL && (*pdpe & PG_V) != 0) {
		if ((*pdpe & PG_PS) != 0)
			pa = (*pdpe & PG_PS_FRAME) | (va & PDPMASK);
		else {
			pde = pmap_pdpe_to_pde(pdpe, va);
			if ((*pde & PG_V) != 0) {
				if ((*pde & PG_PS) != 0) {
					pa = (*pde & PG_PS_FRAME) |
					    (va & PDRMASK);
				} else {
					pte = pmap_pde_to_pte(pde, va);
					pa = (*pte & PG_FRAME) |
					    (va & PAGE_MASK);
				}
			}
		}
	}
	PMAP_UNLOCK(pmap);
	return (pa);
}

/*
 *	Routine:	pmap_extract_and_hold
 *	Function:
 *		Atomically extract and hold the physical page
 *		with the given pmap and virtual address pair
 *		if that mapping permits the given protection.
 */
vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	pdp_entry_t pdpe, *pdpep;
	pd_entry_t pde, *pdep;
	pt_entry_t pte, PG_RW, PG_V;
	vm_page_t m;

	m = NULL;
	PG_RW = pmap_rw_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK(pmap);

	pdpep = pmap_pdpe(pmap, va);
	if (pdpep == NULL || ((pdpe = *pdpep) & PG_V) == 0)
		goto out;
	if ((pdpe & PG_PS) != 0) {
		if ((pdpe & PG_RW) == 0 && (prot & VM_PROT_WRITE) != 0)
			goto out;
		m = PHYS_TO_VM_PAGE((pdpe & PG_PS_FRAME) | (va & PDPMASK));
		goto check_page;
	}

	pdep = pmap_pdpe_to_pde(pdpep, va);
	if (pdep == NULL || ((pde = *pdep) & PG_V) == 0)
		goto out;
	if ((pde & PG_PS) != 0) {
		if ((pde & PG_RW) == 0 && (prot & VM_PROT_WRITE) != 0)
			goto out;
		m = PHYS_TO_VM_PAGE((pde & PG_PS_FRAME) | (va & PDRMASK));
		goto check_page;
	}

	pte = *pmap_pde_to_pte(pdep, va);
	if ((pte & PG_V) == 0 ||
	    ((pte & PG_RW) == 0 && (prot & VM_PROT_WRITE) != 0))
		goto out;
	m = PHYS_TO_VM_PAGE(pte & PG_FRAME);

check_page:
	if (m != NULL && !vm_page_wire_mapped(m))
		m = NULL;
out:
	PMAP_UNLOCK(pmap);
	return (m);
}

/*
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated with the given kernel
 *		virtual address.
 */
vm_paddr_t
pmap_kextract(vm_offset_t va)
{
	pd_entry_t pde;
	vm_paddr_t pa;

	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS) {
		pa = DMAP_TO_PHYS(va);
	} else if (PMAP_ADDRESS_IN_LARGEMAP(va)) {
		pa = pmap_large_map_kextract(va);
	} else {
		pde = *vtopde(va);
		if (pde & PG_PS) {
			pa = (pde & PG_PS_FRAME) | (va & PDRMASK);
		} else {
			/*
			 * Beware of a concurrent promotion that changes the
			 * PDE at this point!  For example, vtopte() must not
			 * be used to access the PTE because it would use the
			 * new PDE.  It is, however, safe to use the old PDE
			 * because the page table page is preserved by the
			 * promotion.
			 */
			pa = *pmap_pde_to_pte(&pde, va);
			pa = (pa & PG_FRAME) | (va & PAGE_MASK);
		}
	}
	return (pa);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * Add a wired page to the kva.
 * Note: not SMP coherent.
 */
void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_store(pte, pa | pg_g | pg_nx | X86_PG_A | X86_PG_M |
	    X86_PG_RW | X86_PG_V);
}

static __inline void
pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode)
{
	pt_entry_t *pte;
	int cache_bits;

	pte = vtopte(va);
	cache_bits = pmap_cache_bits(kernel_pmap, mode, false);
	pte_store(pte, pa | pg_g | pg_nx | X86_PG_A | X86_PG_M |
	    X86_PG_RW | X86_PG_V | cache_bits);
}

/*
 * Remove a page from the kernel pagetables.
 * Note: not SMP coherent.
 */
void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_clear(pte);
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	The value passed in '*virt' is a suggested virtual address for
 *	the mapping. Architectures which can support a direct-mapped
 *	physical to virtual region can return the appropriate address
 *	within that region, leaving '*virt' unchanged. Other
 *	architectures should map the pages starting at '*virt' and
 *	update '*virt' with the first usable address after the mapped
 *	region.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{
	return PHYS_TO_DMAP(start);
}

/*
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */
void
pmap_qenter(vm_offset_t sva, vm_page_t *ma, int count)
{
	pt_entry_t *endpte, oldpte, pa, *pte;
	vm_page_t m;
	int cache_bits;

	oldpte = 0;
	pte = vtopte(sva);
	endpte = pte + count;
	while (pte < endpte) {
		m = *ma++;
		cache_bits = pmap_cache_bits(kernel_pmap, m->md.pat_mode, false);
		pa = VM_PAGE_TO_PHYS(m) | cache_bits;
		if ((*pte & (PG_FRAME | X86_PG_PTE_CACHE)) != pa) {
			oldpte |= *pte;
			pte_store(pte, pa | pg_g | pg_nx | X86_PG_A |
			    X86_PG_M | X86_PG_RW | X86_PG_V);
		}
		pte++;
	}
	if (__predict_false((oldpte & X86_PG_V) != 0))
		pmap_invalidate_range(kernel_pmap, sva, sva + count *
		    PAGE_SIZE);
}

/*
 * This routine tears out page mappings from the
 * kernel -- it is meant only for temporary mappings.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	vm_offset_t va;

	va = sva;
	while (count-- > 0) {
		KASSERT(va >= VM_MIN_KERNEL_ADDRESS, ("usermode va %lx", va));
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/
/*
 * Schedule the specified unused page table page to be freed.  Specifically,
 * add the page to the specified list of pages that will be released to the
 * physical memory manager after the TLB has been updated.
 */
static __inline void
pmap_add_delayed_free_list(vm_page_t m, struct spglist *free, bool set_PG_ZERO)
{

	if (set_PG_ZERO)
		m->flags |= PG_ZERO;
	else
		m->flags &= ~PG_ZERO;
	SLIST_INSERT_HEAD(free, m, plinks.s.ss);
}

/*
 * Inserts the specified page table page into the specified pmap's collection
 * of idle page table pages.  Each of a pmap's page table pages is responsible
 * for mapping a distinct range of virtual addresses.  The pmap's collection is
 * ordered by this virtual address range.
 *
 * If "promoted" is false, then the page table page "mpte" must be zero filled;
 * "mpte"'s valid field will be set to 0.
 *
 * If "promoted" is true and "allpte_PG_A_set" is false, then "mpte" must
 * contain valid mappings with identical attributes except for PG_A; "mpte"'s
 * valid field will be set to 1.
 *
 * If "promoted" and "allpte_PG_A_set" are both true, then "mpte" must contain
 * valid mappings with identical attributes including PG_A; "mpte"'s valid
 * field will be set to VM_PAGE_BITS_ALL.
 */
static __inline int
pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte, bool promoted,
    bool allpte_PG_A_set)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(promoted || !allpte_PG_A_set,
	    ("a zero-filled PTP can't have PG_A set in every PTE"));
	mpte->valid = promoted ? (allpte_PG_A_set ? VM_PAGE_BITS_ALL : 1) : 0;
	return (vm_radix_insert(&pmap->pm_root, mpte));
}

/*
 * Removes the page table page mapping the specified virtual address from the
 * specified pmap's collection of idle page table pages, and returns it.
 * Otherwise, returns NULL if there is no page table page corresponding to the
 * specified virtual address.
 */
static __inline vm_page_t
pmap_remove_pt_page(pmap_t pmap, vm_offset_t va)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	return (vm_radix_remove(&pmap->pm_root, pmap_pde_pindex(va)));
}

/*
 * Decrements a page table page's reference count, which is used to record the
 * number of valid page table entries within the page.  If the reference count
 * drops to zero, then the page table page is unmapped.  Returns true if the
 * page table page was unmapped and false otherwise.
 */
static inline bool
pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{

	--m->ref_count;
	if (m->ref_count == 0) {
		_pmap_unwire_ptp(pmap, va, m, free);
		return (true);
	} else
		return (false);
}

static void
_pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{
	pml5_entry_t *pml5;
	pml4_entry_t *pml4;
	pdp_entry_t *pdp;
	pd_entry_t *pd;
	vm_page_t pdpg, pdppg, pml4pg;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * unmap the page table page
	 */
	if (m->pindex >= NUPDE + NUPDPE + NUPML4E) {
		/* PML4 page */
		MPASS(pmap_is_la57(pmap));
		pml5 = pmap_pml5e(pmap, va);
		*pml5 = 0;
		if (pmap->pm_pmltopu != NULL && va <= VM_MAXUSER_ADDRESS) {
			pml5 = pmap_pml5e_u(pmap, va);
			*pml5 = 0;
		}
	} else if (m->pindex >= NUPDE + NUPDPE) {
		/* PDP page */
		pml4 = pmap_pml4e(pmap, va);
		*pml4 = 0;
		if (!pmap_is_la57(pmap) && pmap->pm_pmltopu != NULL &&
		    va <= VM_MAXUSER_ADDRESS) {
			pml4 = pmap_pml4e_u(pmap, va);
			*pml4 = 0;
		}
	} else if (m->pindex >= NUPDE) {
		/* PD page */
		pdp = pmap_pdpe(pmap, va);
		*pdp = 0;
	} else {
		/* PTE page */
		pd = pmap_pde(pmap, va);
		*pd = 0;
	}
	if (m->pindex < NUPDE) {
		/* We just released a PT, unhold the matching PD */
		pdpg = PHYS_TO_VM_PAGE(*pmap_pdpe(pmap, va) & PG_FRAME);
		pmap_unwire_ptp(pmap, va, pdpg, free);
	} else if (m->pindex < NUPDE + NUPDPE) {
		/* We just released a PD, unhold the matching PDP */
		pdppg = PHYS_TO_VM_PAGE(*pmap_pml4e(pmap, va) & PG_FRAME);
		pmap_unwire_ptp(pmap, va, pdppg, free);
	} else if (m->pindex < NUPDE + NUPDPE + NUPML4E && pmap_is_la57(pmap)) {
		/* We just released a PDP, unhold the matching PML4 */
		pml4pg = PHYS_TO_VM_PAGE(*pmap_pml5e(pmap, va) & PG_FRAME);
		pmap_unwire_ptp(pmap, va, pml4pg, free);
	}

	pmap_pt_page_count_adj(pmap, -1);

	/* 
	 * Put page on a list so that it is released after
	 * *ALL* TLB shootdown is done
	 */
	pmap_add_delayed_free_list(m, free, true);
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the reference count.
 */
static int
pmap_unuse_pt(pmap_t pmap, vm_offset_t va, pd_entry_t ptepde,
    struct spglist *free)
{
	vm_page_t mpte;

	if (va >= VM_MAXUSER_ADDRESS)
		return (0);
	KASSERT(ptepde != 0, ("pmap_unuse_pt: ptepde != 0"));
	mpte = PHYS_TO_VM_PAGE(ptepde & PG_FRAME);
	return (pmap_unwire_ptp(pmap, va, mpte, free));
}

/*
 * Release a page table page reference after a failed attempt to create a
 * mapping.
 */
static void
pmap_abort_ptp(pmap_t pmap, vm_offset_t va, vm_page_t mpte)
{
	struct spglist free;

	SLIST_INIT(&free);
	if (pmap_unwire_ptp(pmap, va, mpte, &free)) {
		/*
		 * Although "va" was never mapped, paging-structure caches
		 * could nonetheless have entries that refer to the freed
		 * page table pages.  Invalidate those entries.
		 */
		pmap_invalidate_page(pmap, va);
		vm_page_free_pages_toq(&free, true);
	}
}

static void
pmap_pinit_pcids(pmap_t pmap, uint32_t pcid, int gen)
{
	struct pmap_pcid *pcidp;
	int i;

	CPU_FOREACH(i) {
		pcidp = zpcpu_get_cpu(pmap->pm_pcidp, i);
		pcidp->pm_pcid = pcid;
		pcidp->pm_gen = gen;
	}
}

void
pmap_pinit0(pmap_t pmap)
{
	struct proc *p;
	struct thread *td;

	PMAP_LOCK_INIT(pmap);
	pmap->pm_pmltop = kernel_pmap->pm_pmltop;
	pmap->pm_pmltopu = NULL;
	pmap->pm_cr3 = kernel_pmap->pm_cr3;
	/* hack to keep pmap_pti_pcid_invalidate() alive */
	pmap->pm_ucr3 = PMAP_NO_CR3;
	vm_radix_init(&pmap->pm_root);
	CPU_ZERO(&pmap->pm_active);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	pmap->pm_flags = pmap_flags;
	pmap->pm_pcidp = uma_zalloc_pcpu(pcpu_zone_8, M_WAITOK);
	pmap_pinit_pcids(pmap, PMAP_PCID_KERN + 1, 1);
	pmap_activate_boot(pmap);
	td = curthread;
	if (pti) {
		p = td->td_proc;
		PROC_LOCK(p);
		p->p_md.md_flags |= P_MD_KPTI;
		PROC_UNLOCK(p);
	}
	pmap_thread_init_invl_gen(td);

	if ((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0) {
		pmap_pkru_ranges_zone = uma_zcreate("pkru ranges",
		    sizeof(struct pmap_pkru_range), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
	}
}

void
pmap_pinit_pml4(vm_page_t pml4pg)
{
	pml4_entry_t *pm_pml4;
	int i;

	pm_pml4 = (pml4_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pml4pg));

	/* Wire in kernel global address entries. */
	for (i = 0; i < NKPML4E; i++) {
		pm_pml4[KPML4BASE + i] = (KPDPphys + ptoa(i)) | X86_PG_RW |
		    X86_PG_V;
	}
#ifdef KASAN
	for (i = 0; i < NKASANPML4E; i++) {
		pm_pml4[KASANPML4I + i] = (KASANPDPphys + ptoa(i)) | X86_PG_RW |
		    X86_PG_V | pg_nx;
	}
#endif
#ifdef KMSAN
	for (i = 0; i < NKMSANSHADPML4E; i++) {
		pm_pml4[KMSANSHADPML4I + i] = (KMSANSHADPDPphys + ptoa(i)) |
		    X86_PG_RW | X86_PG_V | pg_nx;
	}
	for (i = 0; i < NKMSANORIGPML4E; i++) {
		pm_pml4[KMSANORIGPML4I + i] = (KMSANORIGPDPphys + ptoa(i)) |
		    X86_PG_RW | X86_PG_V | pg_nx;
	}
#endif
	for (i = 0; i < ndmpdpphys; i++) {
		pm_pml4[DMPML4I + i] = (DMPDPphys + ptoa(i)) | X86_PG_RW |
		    X86_PG_V;
	}

	/* install self-referential address mapping entry(s) */
	pm_pml4[PML4PML4I] = VM_PAGE_TO_PHYS(pml4pg) | X86_PG_V | X86_PG_RW |
	    X86_PG_A | X86_PG_M;

	/* install large map entries if configured */
	for (i = 0; i < lm_ents; i++)
		pm_pml4[LMSPML4I + i] = kernel_pmap->pm_pmltop[LMSPML4I + i];
}

void
pmap_pinit_pml5(vm_page_t pml5pg)
{
	pml5_entry_t *pm_pml5;

	pm_pml5 = (pml5_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pml5pg));

	/*
	 * Add pml5 entry at top of KVA pointing to existing pml4 table,
	 * entering all existing kernel mappings into level 5 table.
	 */
	pm_pml5[pmap_pml5e_index(UPT_MAX_ADDRESS)] = KPML4phys | X86_PG_V |
	    X86_PG_RW | X86_PG_A | X86_PG_M | pg_g |
	    pmap_cache_bits(kernel_pmap, VM_MEMATTR_DEFAULT, false);

	/* 
	 * Install self-referential address mapping entry.
	 */
	pm_pml5[PML5PML5I] = VM_PAGE_TO_PHYS(pml5pg) |
	    X86_PG_RW | X86_PG_V | X86_PG_M | X86_PG_A |
	    pmap_cache_bits(kernel_pmap, VM_MEMATTR_DEFAULT, false);
}

static void
pmap_pinit_pml4_pti(vm_page_t pml4pgu)
{
	pml4_entry_t *pm_pml4u;
	int i;

	pm_pml4u = (pml4_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pml4pgu));
	for (i = 0; i < NPML4EPG; i++)
		pm_pml4u[i] = pti_pml4[i];
}

static void
pmap_pinit_pml5_pti(vm_page_t pml5pgu)
{
	pml5_entry_t *pm_pml5u;

	pm_pml5u = (pml5_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pml5pgu));
	pagezero(pm_pml5u);

	/*
	 * Add pml5 entry at top of KVA pointing to existing pml4 pti
	 * table, entering all kernel mappings needed for usermode
	 * into level 5 table.
	 */
	pm_pml5u[pmap_pml5e_index(UPT_MAX_ADDRESS)] =
	    pmap_kextract((vm_offset_t)pti_pml4) |
	    X86_PG_V | X86_PG_RW | X86_PG_A | X86_PG_M | pg_g |
	    pmap_cache_bits(kernel_pmap, VM_MEMATTR_DEFAULT, false);
}

/* Allocate a page table page and do related bookkeeping */
static vm_page_t
pmap_alloc_pt_page(pmap_t pmap, vm_pindex_t pindex, int flags)
{
	vm_page_t m;

	m = vm_page_alloc_noobj(flags);
	if (__predict_false(m == NULL))
		return (NULL);
	m->pindex = pindex;
	pmap_pt_page_count_adj(pmap, 1);
	return (m);
}

static void
pmap_free_pt_page(pmap_t pmap, vm_page_t m, bool zerofilled)
{
	/*
	 * This function assumes the page will need to be unwired,
	 * even though the counterpart allocation in pmap_alloc_pt_page()
	 * doesn't enforce VM_ALLOC_WIRED.  However, all current uses
	 * of pmap_free_pt_page() require unwiring.  The case in which
	 * a PT page doesn't require unwiring because its ref_count has
	 * naturally reached 0 is handled through _pmap_unwire_ptp().
	 */
	vm_page_unwire_noq(m);
	if (zerofilled)
		vm_page_free_zero(m);
	else
		vm_page_free(m);

	pmap_pt_page_count_adj(pmap, -1);
}

_Static_assert(sizeof(struct pmap_pcid) == 8, "Fix pcpu zone for pm_pcidp");

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
int
pmap_pinit_type(pmap_t pmap, enum pmap_type pm_type, int flags)
{
	vm_page_t pmltop_pg, pmltop_pgu;
	vm_paddr_t pmltop_phys;

	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);

	/*
	 * Allocate the page directory page.  Pass NULL instead of a
	 * pointer to the pmap here to avoid calling
	 * pmap_resident_count_adj() through pmap_pt_page_count_adj(),
	 * since that requires pmap lock.  Instead do the accounting
	 * manually.
	 *
	 * Note that final call to pmap_remove() optimization that
	 * checks for zero resident_count is basically disabled by
	 * accounting for top-level page.  But the optimization was
	 * not effective since we started using non-managed mapping of
	 * the shared page.
	 */
	pmltop_pg = pmap_alloc_pt_page(NULL, 0, VM_ALLOC_WIRED | VM_ALLOC_ZERO |
	    VM_ALLOC_WAITOK);
	pmap_pt_page_count_pinit(pmap, 1);

	pmltop_phys = VM_PAGE_TO_PHYS(pmltop_pg);
	pmap->pm_pmltop = (pml5_entry_t *)PHYS_TO_DMAP(pmltop_phys);

	if (pmap_pcid_enabled) {
		if (pmap->pm_pcidp == NULL)
			pmap->pm_pcidp = uma_zalloc_pcpu(pcpu_zone_8,
			    M_WAITOK);
		pmap_pinit_pcids(pmap, PMAP_PCID_NONE, 0);
	}
	pmap->pm_cr3 = PMAP_NO_CR3;	/* initialize to an invalid value */
	pmap->pm_ucr3 = PMAP_NO_CR3;
	pmap->pm_pmltopu = NULL;

	pmap->pm_type = pm_type;

	/*
	 * Do not install the host kernel mappings in the nested page
	 * tables. These mappings are meaningless in the guest physical
	 * address space.
	 * Install minimal kernel mappings in PTI case.
	 */
	switch (pm_type) {
	case PT_X86:
		pmap->pm_cr3 = pmltop_phys;
		if (pmap_is_la57(pmap))
			pmap_pinit_pml5(pmltop_pg);
		else
			pmap_pinit_pml4(pmltop_pg);
		if ((curproc->p_md.md_flags & P_MD_KPTI) != 0) {
			/*
			 * As with pmltop_pg, pass NULL instead of a
			 * pointer to the pmap to ensure that the PTI
			 * page counted explicitly.
			 */
			pmltop_pgu = pmap_alloc_pt_page(NULL, 0,
			    VM_ALLOC_WIRED | VM_ALLOC_WAITOK);
			pmap_pt_page_count_pinit(pmap, 1);
			pmap->pm_pmltopu = (pml4_entry_t *)PHYS_TO_DMAP(
			    VM_PAGE_TO_PHYS(pmltop_pgu));
			if (pmap_is_la57(pmap))
				pmap_pinit_pml5_pti(pmltop_pgu);
			else
				pmap_pinit_pml4_pti(pmltop_pgu);
			pmap->pm_ucr3 = VM_PAGE_TO_PHYS(pmltop_pgu);
		}
		if ((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0) {
			rangeset_init(&pmap->pm_pkru, pkru_dup_range,
			    pkru_free_range, pmap, M_NOWAIT);
		}
		break;
	case PT_EPT:
	case PT_RVI:
		pmap->pm_eptsmr = smr_create("pmap", 0, 0);
		break;
	}

	vm_radix_init(&pmap->pm_root);
	CPU_ZERO(&pmap->pm_active);
	TAILQ_INIT(&pmap->pm_pvchunk);
	pmap->pm_flags = flags;
	pmap->pm_eptgen = 0;

	return (1);
}

int
pmap_pinit(pmap_t pmap)
{

	return (pmap_pinit_type(pmap, PT_X86, pmap_flags));
}

static void
pmap_allocpte_free_unref(pmap_t pmap, vm_offset_t va, pt_entry_t *pte)
{
	vm_page_t mpg;
	struct spglist free;

	mpg = PHYS_TO_VM_PAGE(*pte & PG_FRAME);
	if (mpg->ref_count != 0)
		return;
	SLIST_INIT(&free);
	_pmap_unwire_ptp(pmap, va, mpg, &free);
	pmap_invalidate_page(pmap, va);
	vm_page_free_pages_toq(&free, true);
}

static pml4_entry_t *
pmap_allocpte_getpml4(pmap_t pmap, struct rwlock **lockp, vm_offset_t va,
    bool addref)
{
	vm_pindex_t pml5index;
	pml5_entry_t *pml5;
	pml4_entry_t *pml4;
	vm_page_t pml4pg;
	pt_entry_t PG_V;
	bool allocated;

	if (!pmap_is_la57(pmap))
		return (&pmap->pm_pmltop[pmap_pml4e_index(va)]);

	PG_V = pmap_valid_bit(pmap);
	pml5index = pmap_pml5e_index(va);
	pml5 = &pmap->pm_pmltop[pml5index];
	if ((*pml5 & PG_V) == 0) {
		if (pmap_allocpte_nosleep(pmap, pmap_pml5e_pindex(va), lockp,
		    va) == NULL)
			return (NULL);
		allocated = true;
	} else {
		allocated = false;
	}
	pml4 = (pml4_entry_t *)PHYS_TO_DMAP(*pml5 & PG_FRAME);
	pml4 = &pml4[pmap_pml4e_index(va)];
	if ((*pml4 & PG_V) == 0) {
		pml4pg = PHYS_TO_VM_PAGE(*pml5 & PG_FRAME);
		if (allocated && !addref)
			pml4pg->ref_count--;
		else if (!allocated && addref)
			pml4pg->ref_count++;
	}
	return (pml4);
}

static pdp_entry_t *
pmap_allocpte_getpdp(pmap_t pmap, struct rwlock **lockp, vm_offset_t va,
    bool addref)
{
	vm_page_t pdppg;
	pml4_entry_t *pml4;
	pdp_entry_t *pdp;
	pt_entry_t PG_V;
	bool allocated;

	PG_V = pmap_valid_bit(pmap);

	pml4 = pmap_allocpte_getpml4(pmap, lockp, va, false);
	if (pml4 == NULL)
		return (NULL);

	if ((*pml4 & PG_V) == 0) {
		/* Have to allocate a new pdp, recurse */
		if (pmap_allocpte_nosleep(pmap, pmap_pml4e_pindex(va), lockp,
		    va) == NULL) {
			if (pmap_is_la57(pmap))
				pmap_allocpte_free_unref(pmap, va,
				    pmap_pml5e(pmap, va));
			return (NULL);
		}
		allocated = true;
	} else {
		allocated = false;
	}
	pdp = (pdp_entry_t *)PHYS_TO_DMAP(*pml4 & PG_FRAME);
	pdp = &pdp[pmap_pdpe_index(va)];
	if ((*pdp & PG_V) == 0) {
		pdppg = PHYS_TO_VM_PAGE(*pml4 & PG_FRAME);
		if (allocated && !addref)
			pdppg->ref_count--;
		else if (!allocated && addref)
			pdppg->ref_count++;
	}
	return (pdp);
}

/*
 * The ptepindexes, i.e. page indices, of the page table pages encountered
 * while translating virtual address va are defined as follows:
 * - for the page table page (last level),
 *      ptepindex = pmap_pde_pindex(va) = va >> PDRSHIFT,
 *   in other words, it is just the index of the PDE that maps the page
 *   table page.
 * - for the page directory page,
 *      ptepindex = NUPDE (number of userland PD entries) +
 *          (pmap_pde_index(va) >> NPDEPGSHIFT)
 *   i.e. index of PDPE is put after the last index of PDE,
 * - for the page directory pointer page,
 *      ptepindex = NUPDE + NUPDPE + (pmap_pde_index(va) >> (NPDEPGSHIFT +
 *          NPML4EPGSHIFT),
 *   i.e. index of pml4e is put after the last index of PDPE,
 * - for the PML4 page (if LA57 mode is enabled),
 *      ptepindex = NUPDE + NUPDPE + NUPML4E + (pmap_pde_index(va) >>
 *          (NPDEPGSHIFT + NPML4EPGSHIFT + NPML5EPGSHIFT),
 *   i.e. index of pml5e is put after the last index of PML4E.
 *
 * Define an order on the paging entries, where all entries of the
 * same height are put together, then heights are put from deepest to
 * root.  Then ptexpindex is the sequential number of the
 * corresponding paging entry in this order.
 *
 * The values of NUPDE, NUPDPE, and NUPML4E are determined by the size of
 * LA57 paging structures even in LA48 paging mode. Moreover, the
 * ptepindexes are calculated as if the paging structures were 5-level
 * regardless of the actual mode of operation.
 *
 * The root page at PML4/PML5 does not participate in this indexing scheme,
 * since it is statically allocated by pmap_pinit() and not by pmap_allocpte().
 */
static vm_page_t
pmap_allocpte_nosleep(pmap_t pmap, vm_pindex_t ptepindex, struct rwlock **lockp,
    vm_offset_t va)
{
	vm_pindex_t pml5index, pml4index;
	pml5_entry_t *pml5, *pml5u;
	pml4_entry_t *pml4, *pml4u;
	pdp_entry_t *pdp;
	pd_entry_t *pd;
	vm_page_t m, pdpg;
	pt_entry_t PG_A, PG_M, PG_RW, PG_V;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	/*
	 * Allocate a page table page.
	 */
	m = pmap_alloc_pt_page(pmap, ptepindex,
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO);
	if (m == NULL)
		return (NULL);

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */
	if (ptepindex >= NUPDE + NUPDPE + NUPML4E) {
		MPASS(pmap_is_la57(pmap));

		pml5index = pmap_pml5e_index(va);
		pml5 = &pmap->pm_pmltop[pml5index];
		KASSERT((*pml5 & PG_V) == 0,
		    ("pmap %p va %#lx pml5 %#lx", pmap, va, *pml5));
		*pml5 = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V | PG_A | PG_M;

		if (pmap->pm_pmltopu != NULL && pml5index < NUPML5E) {
			if (pmap->pm_ucr3 != PMAP_NO_CR3)
				*pml5 |= pg_nx;

			pml5u = &pmap->pm_pmltopu[pml5index];
			*pml5u = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V |
			    PG_A | PG_M;
		}
	} else if (ptepindex >= NUPDE + NUPDPE) {
		pml4index = pmap_pml4e_index(va);
		/* Wire up a new PDPE page */
		pml4 = pmap_allocpte_getpml4(pmap, lockp, va, true);
		if (pml4 == NULL) {
			pmap_free_pt_page(pmap, m, true);
			return (NULL);
		}
		KASSERT((*pml4 & PG_V) == 0,
		    ("pmap %p va %#lx pml4 %#lx", pmap, va, *pml4));
		*pml4 = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V | PG_A | PG_M;

		if (!pmap_is_la57(pmap) && pmap->pm_pmltopu != NULL &&
		    pml4index < NUPML4E) {
			/*
			 * PTI: Make all user-space mappings in the
			 * kernel-mode page table no-execute so that
			 * we detect any programming errors that leave
			 * the kernel-mode page table active on return
			 * to user space.
			 */
			if (pmap->pm_ucr3 != PMAP_NO_CR3)
				*pml4 |= pg_nx;

			pml4u = &pmap->pm_pmltopu[pml4index];
			*pml4u = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V |
			    PG_A | PG_M;
		}
	} else if (ptepindex >= NUPDE) {
		/* Wire up a new PDE page */
		pdp = pmap_allocpte_getpdp(pmap, lockp, va, true);
		if (pdp == NULL) {
			pmap_free_pt_page(pmap, m, true);
			return (NULL);
		}
		KASSERT((*pdp & PG_V) == 0,
		    ("pmap %p va %#lx pdp %#lx", pmap, va, *pdp));
		*pdp = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V | PG_A | PG_M;
	} else {
		/* Wire up a new PTE page */
		pdp = pmap_allocpte_getpdp(pmap, lockp, va, false);
		if (pdp == NULL) {
			pmap_free_pt_page(pmap, m, true);
			return (NULL);
		}
		if ((*pdp & PG_V) == 0) {
			/* Have to allocate a new pd, recurse */
		  if (pmap_allocpte_nosleep(pmap, pmap_pdpe_pindex(va),
		      lockp, va) == NULL) {
				pmap_allocpte_free_unref(pmap, va,
				    pmap_pml4e(pmap, va));
				pmap_free_pt_page(pmap, m, true);
				return (NULL);
			}
		} else {
			/* Add reference to the pd page */
			pdpg = PHYS_TO_VM_PAGE(*pdp & PG_FRAME);
			pdpg->ref_count++;
		}
		pd = (pd_entry_t *)PHYS_TO_DMAP(*pdp & PG_FRAME);

		/* Now we know where the page directory page is */
		pd = &pd[pmap_pde_index(va)];
		KASSERT((*pd & PG_V) == 0,
		    ("pmap %p va %#lx pd %#lx", pmap, va, *pd));
		*pd = VM_PAGE_TO_PHYS(m) | PG_U | PG_RW | PG_V | PG_A | PG_M;
	}

	return (m);
}

/*
 * This routine is called if the desired page table page does not exist.
 *
 * If page table page allocation fails, this routine may sleep before
 * returning NULL.  It sleeps only if a lock pointer was given.  Sleep
 * occurs right before returning to the caller. This way, we never
 * drop pmap lock to sleep while a page table page has ref_count == 0,
 * which prevents the page from being freed under us.
 */
static vm_page_t
pmap_allocpte_alloc(pmap_t pmap, vm_pindex_t ptepindex, struct rwlock **lockp,
    vm_offset_t va)
{
	vm_page_t m;

	m = pmap_allocpte_nosleep(pmap, ptepindex, lockp, va);
	if (m == NULL && lockp != NULL) {
		RELEASE_PV_LIST_LOCK(lockp);
		PMAP_UNLOCK(pmap);
		PMAP_ASSERT_NOT_IN_DI();
		vm_wait(NULL);
		PMAP_LOCK(pmap);
	}
	return (m);
}

static pd_entry_t *
pmap_alloc_pde(pmap_t pmap, vm_offset_t va, vm_page_t *pdpgp,
    struct rwlock **lockp)
{
	pdp_entry_t *pdpe, PG_V;
	pd_entry_t *pde;
	vm_page_t pdpg;
	vm_pindex_t pdpindex;

	PG_V = pmap_valid_bit(pmap);

retry:
	pdpe = pmap_pdpe(pmap, va);
	if (pdpe != NULL && (*pdpe & PG_V) != 0) {
		pde = pmap_pdpe_to_pde(pdpe, va);
		if (va < VM_MAXUSER_ADDRESS) {
			/* Add a reference to the pd page. */
			pdpg = PHYS_TO_VM_PAGE(*pdpe & PG_FRAME);
			pdpg->ref_count++;
		} else
			pdpg = NULL;
	} else if (va < VM_MAXUSER_ADDRESS) {
		/* Allocate a pd page. */
		pdpindex = pmap_pde_pindex(va) >> NPDPEPGSHIFT;
		pdpg = pmap_allocpte_alloc(pmap, NUPDE + pdpindex, lockp, va);
		if (pdpg == NULL) {
			if (lockp != NULL)
				goto retry;
			else
				return (NULL);
		}
		pde = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pdpg));
		pde = &pde[pmap_pde_index(va)];
	} else
		panic("pmap_alloc_pde: missing page table page for va %#lx",
		    va);
	*pdpgp = pdpg;
	return (pde);
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	vm_pindex_t ptepindex;
	pd_entry_t *pd, PG_V;
	vm_page_t m;

	PG_V = pmap_valid_bit(pmap);

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = pmap_pde_pindex(va);
retry:
	/*
	 * Get the page directory entry
	 */
	pd = pmap_pde(pmap, va);

	/*
	 * This supports switching from a 2MB page to a
	 * normal 4K page.
	 */
	if (pd != NULL && (*pd & (PG_PS | PG_V)) == (PG_PS | PG_V)) {
		if (!pmap_demote_pde_locked(pmap, pd, va, lockp)) {
			/*
			 * Invalidation of the 2MB page mapping may have caused
			 * the deallocation of the underlying PD page.
			 */
			pd = NULL;
		}
	}

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (pd != NULL && (*pd & PG_V) != 0) {
		m = PHYS_TO_VM_PAGE(*pd & PG_FRAME);
		m->ref_count++;
	} else {
		/*
		 * Here if the pte page isn't mapped, or if it has been
		 * deallocated.
		 */
		m = pmap_allocpte_alloc(pmap, ptepindex, lockp, va);
		if (m == NULL && lockp != NULL)
			goto retry;
	}
	return (m);
}

/***************************************************
 * Pmap allocation/deallocation routines.
 ***************************************************/

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pmap)
{
	vm_page_t m;
	int i;

	KASSERT(vm_radix_is_empty(&pmap->pm_root),
	    ("pmap_release: pmap %p has reserved page table page(s)",
	    pmap));
	KASSERT(CPU_EMPTY(&pmap->pm_active),
	    ("releasing active pmap %p", pmap));

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pmap->pm_pmltop));

	if (pmap_is_la57(pmap)) {
		pmap->pm_pmltop[pmap_pml5e_index(UPT_MAX_ADDRESS)] = 0;
		pmap->pm_pmltop[PML5PML5I] = 0;
	} else {
		for (i = 0; i < NKPML4E; i++)	/* KVA */
			pmap->pm_pmltop[KPML4BASE + i] = 0;
#ifdef KASAN
		for (i = 0; i < NKASANPML4E; i++) /* KASAN shadow map */
			pmap->pm_pmltop[KASANPML4I + i] = 0;
#endif
#ifdef KMSAN
		for (i = 0; i < NKMSANSHADPML4E; i++) /* KMSAN shadow map */
			pmap->pm_pmltop[KMSANSHADPML4I + i] = 0;
		for (i = 0; i < NKMSANORIGPML4E; i++) /* KMSAN shadow map */
			pmap->pm_pmltop[KMSANORIGPML4I + i] = 0;
#endif
		for (i = 0; i < ndmpdpphys; i++)/* Direct Map */
			pmap->pm_pmltop[DMPML4I + i] = 0;
		pmap->pm_pmltop[PML4PML4I] = 0;	/* Recursive Mapping */
		for (i = 0; i < lm_ents; i++)	/* Large Map */
			pmap->pm_pmltop[LMSPML4I + i] = 0;
	}

	pmap_free_pt_page(NULL, m, true);
	pmap_pt_page_count_pinit(pmap, -1);

	if (pmap->pm_pmltopu != NULL) {
		m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pmap->
		    pm_pmltopu));
		pmap_free_pt_page(NULL, m, false);
		pmap_pt_page_count_pinit(pmap, -1);
	}
	if (pmap->pm_type == PT_X86 &&
	    (cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0)
		rangeset_fini(&pmap->pm_pkru);

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap %p resident count %ld != 0",
	    pmap, pmap->pm_stats.resident_count));
}

static int
kvm_size(SYSCTL_HANDLER_ARGS)
{
	unsigned long ksize = VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS;

	return sysctl_handle_long(oidp, &ksize, 0, req);
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_size, CTLTYPE_LONG | CTLFLAG_RD | CTLFLAG_MPSAFE,
    0, 0, kvm_size, "LU",
    "Size of KVM");

static int
kvm_free(SYSCTL_HANDLER_ARGS)
{
	unsigned long kfree = VM_MAX_KERNEL_ADDRESS - kernel_vm_end;

	return sysctl_handle_long(oidp, &kfree, 0, req);
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_free, CTLTYPE_LONG | CTLFLAG_RD | CTLFLAG_MPSAFE,
    0, 0, kvm_free, "LU",
    "Amount of KVM free");

#ifdef KMSAN
static void
pmap_kmsan_shadow_map_page_array(vm_paddr_t pdppa, vm_size_t size)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_paddr_t dummypa, dummypd, dummypt;
	int i, npde, npdpg;

	npdpg = howmany(size, NBPDP);
	npde = size / NBPDR;

	dummypa = vm_phys_early_alloc(-1, PAGE_SIZE);
	pagezero((void *)PHYS_TO_DMAP(dummypa));

	dummypt = vm_phys_early_alloc(-1, PAGE_SIZE);
	pagezero((void *)PHYS_TO_DMAP(dummypt));
	dummypd = vm_phys_early_alloc(-1, PAGE_SIZE * npdpg);
	for (i = 0; i < npdpg; i++)
		pagezero((void *)PHYS_TO_DMAP(dummypd + ptoa(i)));

	pte = (pt_entry_t *)PHYS_TO_DMAP(dummypt);
	for (i = 0; i < NPTEPG; i++)
		pte[i] = (pt_entry_t)(dummypa | X86_PG_V | X86_PG_RW |
		    X86_PG_A | X86_PG_M | pg_nx);

	pde = (pd_entry_t *)PHYS_TO_DMAP(dummypd);
	for (i = 0; i < npde; i++)
		pde[i] = (pd_entry_t)(dummypt | X86_PG_V | X86_PG_RW | pg_nx);

	pdpe = (pdp_entry_t *)PHYS_TO_DMAP(pdppa);
	for (i = 0; i < npdpg; i++)
		pdpe[i] = (pdp_entry_t)(dummypd + ptoa(i) | X86_PG_V |
		    X86_PG_RW | pg_nx);
}

static void
pmap_kmsan_page_array_startup(vm_offset_t start, vm_offset_t end)
{
	vm_size_t size;

	KASSERT(start % NBPDP == 0, ("unaligned page array start address"));

	/*
	 * The end of the page array's KVA region is 2MB aligned, see
	 * kmem_init().
	 */
	size = round_2mpage(end) - start;
	pmap_kmsan_shadow_map_page_array(KMSANSHADPDPphys, size);
	pmap_kmsan_shadow_map_page_array(KMSANORIGPDPphys, size);
}
#endif

/*
 * Allocate physical memory for the vm_page array and map it into KVA,
 * attempting to back the vm_pages with domain-local memory.
 */
void
pmap_page_array_startup(long pages)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde, newpdir;
	vm_offset_t va, start, end;
	vm_paddr_t pa;
	long pfn;
	int domain, i;

	vm_page_array_size = pages;

	start = VM_MIN_KERNEL_ADDRESS;
	end = start + pages * sizeof(struct vm_page);
	for (va = start; va < end; va += NBPDR) {
		pfn = first_page + (va - start) / sizeof(struct vm_page);
		domain = vm_phys_domain(ptoa(pfn));
		pdpe = pmap_pdpe(kernel_pmap, va);
		if ((*pdpe & X86_PG_V) == 0) {
			pa = vm_phys_early_alloc(domain, PAGE_SIZE);
			dump_add_page(pa);
			pagezero((void *)PHYS_TO_DMAP(pa));
			*pdpe = (pdp_entry_t)(pa | X86_PG_V | X86_PG_RW |
			    X86_PG_A | X86_PG_M);
		}
		pde = pmap_pdpe_to_pde(pdpe, va);
		if ((*pde & X86_PG_V) != 0)
			panic("Unexpected pde");
		pa = vm_phys_early_alloc(domain, NBPDR);
		for (i = 0; i < NPDEPG; i++)
			dump_add_page(pa + i * PAGE_SIZE);
		newpdir = (pd_entry_t)(pa | X86_PG_V | X86_PG_RW | X86_PG_A |
		    X86_PG_M | PG_PS | pg_g | pg_nx);
		pde_store(pde, newpdir);
	}
	vm_page_array = (vm_page_t)start;

#ifdef KMSAN
	pmap_kmsan_page_array_startup(start, end);
#endif
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	vm_paddr_t paddr;
	vm_page_t nkpg;
	pd_entry_t *pde, newpdir;
	pdp_entry_t *pdpe;
	vm_offset_t end;

	TSENTER();
	mtx_assert(&kernel_map->system_mtx, MA_OWNED);

	/*
	 * The kernel map covers two distinct regions of KVA: that used
	 * for dynamic kernel memory allocations, and the uppermost 2GB
	 * of the virtual address space.  The latter is used to map the
	 * kernel and loadable kernel modules.  This scheme enables the
	 * use of a special code generation model for kernel code which
	 * takes advantage of compact addressing modes in machine code.
	 *
	 * Both regions grow upwards; to avoid wasting memory, the gap
	 * in between is unmapped.  If "addr" is above "KERNBASE", the
	 * kernel's region is grown, otherwise the kmem region is grown.
	 *
	 * The correctness of this action is based on the following
	 * argument: vm_map_insert() allocates contiguous ranges of the
	 * kernel virtual address space.  It calls this function if a range
	 * ends after "kernel_vm_end".  If the kernel is mapped between
	 * "kernel_vm_end" and "addr", then the range cannot begin at
	 * "kernel_vm_end".  In fact, its beginning address cannot be less
	 * than the kernel.  Thus, there is no immediate need to allocate
	 * any new kernel page table pages between "kernel_vm_end" and
	 * "KERNBASE".
	 */
	if (KERNBASE < addr) {
		end = KERNBASE + nkpt * NBPDR;
		if (end == 0) {
			TSEXIT();
			return;
		}
	} else {
		end = kernel_vm_end;
	}

	addr = roundup2(addr, NBPDR);
	if (addr - 1 >= vm_map_max(kernel_map))
		addr = vm_map_max(kernel_map);
	if (addr <= end) {
		/*
		 * The grown region is already mapped, so there is
		 * nothing to do.
		 */
		TSEXIT();
		return;
	}

	kasan_shadow_map(end, addr - end);
	kmsan_shadow_map(end, addr - end);
	while (end < addr) {
		pdpe = pmap_pdpe(kernel_pmap, end);
		if ((*pdpe & X86_PG_V) == 0) {
			nkpg = pmap_alloc_pt_page(kernel_pmap,
			    pmap_pdpe_pindex(end), VM_ALLOC_WIRED |
			    VM_ALLOC_INTERRUPT | VM_ALLOC_ZERO);
			if (nkpg == NULL)
				panic("pmap_growkernel: no memory to grow kernel");
			paddr = VM_PAGE_TO_PHYS(nkpg);
			*pdpe = (pdp_entry_t)(paddr | X86_PG_V | X86_PG_RW |
			    X86_PG_A | X86_PG_M);
			continue; /* try again */
		}
		pde = pmap_pdpe_to_pde(pdpe, end);
		if ((*pde & X86_PG_V) != 0) {
			end = (end + NBPDR) & ~PDRMASK;
			if (end - 1 >= vm_map_max(kernel_map)) {
				end = vm_map_max(kernel_map);
				break;                       
			}
			continue;
		}

		nkpg = pmap_alloc_pt_page(kernel_pmap, pmap_pde_pindex(end),
		    VM_ALLOC_WIRED | VM_ALLOC_INTERRUPT | VM_ALLOC_ZERO);
		if (nkpg == NULL)
			panic("pmap_growkernel: no memory to grow kernel");
		paddr = VM_PAGE_TO_PHYS(nkpg);
		newpdir = paddr | X86_PG_V | X86_PG_RW | X86_PG_A | X86_PG_M;
		pde_store(pde, newpdir);

		end = (end + NBPDR) & ~PDRMASK;
		if (end - 1 >= vm_map_max(kernel_map)) {
			end = vm_map_max(kernel_map);
			break;                       
		}
	}

	if (end <= KERNBASE)
		kernel_vm_end = end;
	else
		nkpt = howmany(end - KERNBASE, NBPDR);
	TSEXIT();
}

/***************************************************
 * page management routines.
 ***************************************************/

static const uint64_t pc_freemask[_NPCM] = {
	[0 ... _NPCM - 2] = PC_FREEN,
	[_NPCM - 1] = PC_FREEL
};

#ifdef PV_STATS

static COUNTER_U64_DEFINE_EARLY(pc_chunk_count);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pc_chunk_count, CTLFLAG_RD,
    &pc_chunk_count, "Current number of pv entry cnunks");

static COUNTER_U64_DEFINE_EARLY(pc_chunk_allocs);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pc_chunk_allocs, CTLFLAG_RD,
    &pc_chunk_allocs, "Total number of pv entry chunks allocated");

static COUNTER_U64_DEFINE_EARLY(pc_chunk_frees);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pc_chunk_frees, CTLFLAG_RD,
    &pc_chunk_frees, "Total number of pv entry chunks freed");

static COUNTER_U64_DEFINE_EARLY(pc_chunk_tryfail);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pc_chunk_tryfail, CTLFLAG_RD,
    &pc_chunk_tryfail,
    "Number of failed attempts to get a pv entry chunk page");

static COUNTER_U64_DEFINE_EARLY(pv_entry_frees);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pv_entry_frees, CTLFLAG_RD,
    &pv_entry_frees, "Total number of pv entries freed");

static COUNTER_U64_DEFINE_EARLY(pv_entry_allocs);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pv_entry_allocs, CTLFLAG_RD,
    &pv_entry_allocs, "Total number of pv entries allocated");

static COUNTER_U64_DEFINE_EARLY(pv_entry_count);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pv_entry_count, CTLFLAG_RD,
    &pv_entry_count, "Current number of pv entries");

static COUNTER_U64_DEFINE_EARLY(pv_entry_spare);
SYSCTL_COUNTER_U64(_vm_pmap, OID_AUTO, pv_entry_spare, CTLFLAG_RD,
    &pv_entry_spare, "Current number of spare pv entries");
#endif

static void
reclaim_pv_chunk_leave_pmap(pmap_t pmap, pmap_t locked_pmap, bool start_di)
{

	if (pmap == NULL)
		return;
	pmap_invalidate_all(pmap);
	if (pmap != locked_pmap)
		PMAP_UNLOCK(pmap);
	if (start_di)
		pmap_delayed_invl_finish();
}

/*
 * We are in a serious low memory condition.  Resort to
 * drastic measures to free some pages so we can allocate
 * another pv entry chunk.
 *
 * Returns NULL if PV entries were reclaimed from the specified pmap.
 *
 * We do not, however, unmap 2mpages because subsequent accesses will
 * allocate per-page pv entries until repromotion occurs, thereby
 * exacerbating the shortage of free pv entries.
 */
static vm_page_t
reclaim_pv_chunk_domain(pmap_t locked_pmap, struct rwlock **lockp, int domain)
{
	struct pv_chunks_list *pvc;
	struct pv_chunk *pc, *pc_marker, *pc_marker_end;
	struct pv_chunk_header pc_marker_b, pc_marker_end_b;
	struct md_page *pvh;
	pd_entry_t *pde;
	pmap_t next_pmap, pmap;
	pt_entry_t *pte, tpte;
	pt_entry_t PG_G, PG_A, PG_M, PG_RW;
	pv_entry_t pv;
	vm_offset_t va;
	vm_page_t m, m_pc;
	struct spglist free;
	uint64_t inuse;
	int bit, field, freed;
	bool start_di, restart;

	PMAP_LOCK_ASSERT(locked_pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("reclaim_pv_chunk: lockp is NULL"));
	pmap = NULL;
	m_pc = NULL;
	PG_G = PG_A = PG_M = PG_RW = 0;
	SLIST_INIT(&free);
	bzero(&pc_marker_b, sizeof(pc_marker_b));
	bzero(&pc_marker_end_b, sizeof(pc_marker_end_b));
	pc_marker = (struct pv_chunk *)&pc_marker_b;
	pc_marker_end = (struct pv_chunk *)&pc_marker_end_b;

	/*
	 * A delayed invalidation block should already be active if
	 * pmap_advise() or pmap_remove() called this function by way
	 * of pmap_demote_pde_locked().
	 */
	start_di = pmap_not_in_di();

	pvc = &pv_chunks[domain];
	mtx_lock(&pvc->pvc_lock);
	pvc->active_reclaims++;
	TAILQ_INSERT_HEAD(&pvc->pvc_list, pc_marker, pc_lru);
	TAILQ_INSERT_TAIL(&pvc->pvc_list, pc_marker_end, pc_lru);
	while ((pc = TAILQ_NEXT(pc_marker, pc_lru)) != pc_marker_end &&
	    SLIST_EMPTY(&free)) {
		next_pmap = pc->pc_pmap;
		if (next_pmap == NULL) {
			/*
			 * The next chunk is a marker.  However, it is
			 * not our marker, so active_reclaims must be
			 * > 1.  Consequently, the next_chunk code
			 * will not rotate the pv_chunks list.
			 */
			goto next_chunk;
		}
		mtx_unlock(&pvc->pvc_lock);

		/*
		 * A pv_chunk can only be removed from the pc_lru list
		 * when both pc_chunks_mutex is owned and the
		 * corresponding pmap is locked.
		 */
		if (pmap != next_pmap) {
			restart = false;
			reclaim_pv_chunk_leave_pmap(pmap, locked_pmap,
			    start_di);
			pmap = next_pmap;
			/* Avoid deadlock and lock recursion. */
			if (pmap > locked_pmap) {
				RELEASE_PV_LIST_LOCK(lockp);
				PMAP_LOCK(pmap);
				if (start_di)
					pmap_delayed_invl_start();
				mtx_lock(&pvc->pvc_lock);
				restart = true;
			} else if (pmap != locked_pmap) {
				if (PMAP_TRYLOCK(pmap)) {
					if (start_di)
						pmap_delayed_invl_start();
					mtx_lock(&pvc->pvc_lock);
					restart = true;
				} else {
					pmap = NULL; /* pmap is not locked */
					mtx_lock(&pvc->pvc_lock);
					pc = TAILQ_NEXT(pc_marker, pc_lru);
					if (pc == NULL ||
					    pc->pc_pmap != next_pmap)
						continue;
					goto next_chunk;
				}
			} else if (start_di)
				pmap_delayed_invl_start();
			PG_G = pmap_global_bit(pmap);
			PG_A = pmap_accessed_bit(pmap);
			PG_M = pmap_modified_bit(pmap);
			PG_RW = pmap_rw_bit(pmap);
			if (restart)
				continue;
		}

		/*
		 * Destroy every non-wired, 4 KB page mapping in the chunk.
		 */
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			for (inuse = ~pc->pc_map[field] & pc_freemask[field];
			    inuse != 0; inuse &= ~(1UL << bit)) {
				bit = bsfq(inuse);
				pv = &pc->pc_pventry[field * 64 + bit];
				va = pv->pv_va;
				pde = pmap_pde(pmap, va);
				if ((*pde & PG_PS) != 0)
					continue;
				pte = pmap_pde_to_pte(pde, va);
				if ((*pte & PG_W) != 0)
					continue;
				tpte = pte_load_clear(pte);
				if ((tpte & PG_G) != 0)
					pmap_invalidate_page(pmap, va);
				m = PHYS_TO_VM_PAGE(tpte & PG_FRAME);
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
					vm_page_dirty(m);
				if ((tpte & PG_A) != 0)
					vm_page_aflag_set(m, PGA_REFERENCED);
				CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
				TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
				m->md.pv_gen++;
				if (TAILQ_EMPTY(&m->md.pv_list) &&
				    (m->flags & PG_FICTITIOUS) == 0) {
					pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						vm_page_aflag_clear(m,
						    PGA_WRITEABLE);
					}
				}
				pmap_delayed_invl_page(m);
				pc->pc_map[field] |= 1UL << bit;
				pmap_unuse_pt(pmap, va, *pde, &free);
				freed++;
			}
		}
		if (freed == 0) {
			mtx_lock(&pvc->pvc_lock);
			goto next_chunk;
		}
		/* Every freed mapping is for a 4 KB page. */
		pmap_resident_count_adj(pmap, -freed);
		PV_STAT(counter_u64_add(pv_entry_frees, freed));
		PV_STAT(counter_u64_add(pv_entry_spare, freed));
		PV_STAT(counter_u64_add(pv_entry_count, -freed));
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		if (pc_is_free(pc)) {
			PV_STAT(counter_u64_add(pv_entry_spare, -_NPCPV));
			PV_STAT(counter_u64_add(pc_chunk_count, -1));
			PV_STAT(counter_u64_add(pc_chunk_frees, 1));
			/* Entire chunk is free; return it. */
			m_pc = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pc));
			dump_drop_page(m_pc->phys_addr);
			mtx_lock(&pvc->pvc_lock);
			TAILQ_REMOVE(&pvc->pvc_list, pc, pc_lru);
			break;
		}
		TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		mtx_lock(&pvc->pvc_lock);
		/* One freed pv entry in locked_pmap is sufficient. */
		if (pmap == locked_pmap)
			break;
next_chunk:
		TAILQ_REMOVE(&pvc->pvc_list, pc_marker, pc_lru);
		TAILQ_INSERT_AFTER(&pvc->pvc_list, pc, pc_marker, pc_lru);
		if (pvc->active_reclaims == 1 && pmap != NULL) {
			/*
			 * Rotate the pv chunks list so that we do not
			 * scan the same pv chunks that could not be
			 * freed (because they contained a wired
			 * and/or superpage mapping) on every
			 * invocation of reclaim_pv_chunk().
			 */
			while ((pc = TAILQ_FIRST(&pvc->pvc_list)) != pc_marker) {
				MPASS(pc->pc_pmap != NULL);
				TAILQ_REMOVE(&pvc->pvc_list, pc, pc_lru);
				TAILQ_INSERT_TAIL(&pvc->pvc_list, pc, pc_lru);
			}
		}
	}
	TAILQ_REMOVE(&pvc->pvc_list, pc_marker, pc_lru);
	TAILQ_REMOVE(&pvc->pvc_list, pc_marker_end, pc_lru);
	pvc->active_reclaims--;
	mtx_unlock(&pvc->pvc_lock);
	reclaim_pv_chunk_leave_pmap(pmap, locked_pmap, start_di);
	if (m_pc == NULL && !SLIST_EMPTY(&free)) {
		m_pc = SLIST_FIRST(&free);
		SLIST_REMOVE_HEAD(&free, plinks.s.ss);
		/* Recycle a freed page table page. */
		m_pc->ref_count = 1;
	}
	vm_page_free_pages_toq(&free, true);
	return (m_pc);
}

static vm_page_t
reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp)
{
	vm_page_t m;
	int i, domain;

	domain = PCPU_GET(domain);
	for (i = 0; i < vm_ndomains; i++) {
		m = reclaim_pv_chunk_domain(locked_pmap, lockp, domain);
		if (m != NULL)
			break;
		domain = (domain + 1) % vm_ndomains;
	}

	return (m);
}

/*
 * free the pv_entry back to the free list
 */
static void
free_pv_entry(pmap_t pmap, pv_entry_t pv)
{
	struct pv_chunk *pc;
	int idx, field, bit;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(counter_u64_add(pv_entry_frees, 1));
	PV_STAT(counter_u64_add(pv_entry_spare, 1));
	PV_STAT(counter_u64_add(pv_entry_count, -1));
	pc = pv_to_chunk(pv);
	idx = pv - &pc->pc_pventry[0];
	field = idx / 64;
	bit = idx % 64;
	pc->pc_map[field] |= 1ul << bit;
	if (!pc_is_free(pc)) {
		/* 98% of the time, pc is already at the head of the list. */
		if (__predict_false(pc != TAILQ_FIRST(&pmap->pm_pvchunk))) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		}
		return;
	}
	TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
	free_pv_chunk(pc);
}

static void
free_pv_chunk_dequeued(struct pv_chunk *pc)
{
	vm_page_t m;

	PV_STAT(counter_u64_add(pv_entry_spare, -_NPCPV));
	PV_STAT(counter_u64_add(pc_chunk_count, -1));
	PV_STAT(counter_u64_add(pc_chunk_frees, 1));
	counter_u64_add(pv_page_count, -1);
	/* entire chunk is free, return it */
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pc));
	dump_drop_page(m->phys_addr);
	vm_page_unwire_noq(m);
	vm_page_free(m);
}

static void
free_pv_chunk(struct pv_chunk *pc)
{
	struct pv_chunks_list *pvc;

	pvc = &pv_chunks[pc_to_domain(pc)];
	mtx_lock(&pvc->pvc_lock);
	TAILQ_REMOVE(&pvc->pvc_list, pc, pc_lru);
	mtx_unlock(&pvc->pvc_lock);
	free_pv_chunk_dequeued(pc);
}

static void
free_pv_chunk_batch(struct pv_chunklist *batch)
{
	struct pv_chunks_list *pvc;
	struct pv_chunk *pc, *npc;
	int i;

	for (i = 0; i < vm_ndomains; i++) {
		if (TAILQ_EMPTY(&batch[i]))
			continue;
		pvc = &pv_chunks[i];
		mtx_lock(&pvc->pvc_lock);
		TAILQ_FOREACH(pc, &batch[i], pc_list) {
			TAILQ_REMOVE(&pvc->pvc_list, pc, pc_lru);
		}
		mtx_unlock(&pvc->pvc_lock);
	}

	for (i = 0; i < vm_ndomains; i++) {
		TAILQ_FOREACH_SAFE(pc, &batch[i], pc_list, npc) {
			free_pv_chunk_dequeued(pc);
		}
	}
}

/*
 * Returns a new PV entry, allocating a new PV chunk from the system when
 * needed.  If this PV chunk allocation fails and a PV list lock pointer was
 * given, a PV chunk is reclaimed from an arbitrary pmap.  Otherwise, NULL is
 * returned.
 *
 * The given PV list lock may be released.
 */
static pv_entry_t
get_pv_entry(pmap_t pmap, struct rwlock **lockp)
{
	struct pv_chunks_list *pvc;
	int bit, field;
	pv_entry_t pv;
	struct pv_chunk *pc;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(counter_u64_add(pv_entry_allocs, 1));
retry:
	pc = TAILQ_FIRST(&pmap->pm_pvchunk);
	if (pc != NULL) {
		for (field = 0; field < _NPCM; field++) {
			if (pc->pc_map[field]) {
				bit = bsfq(pc->pc_map[field]);
				break;
			}
		}
		if (field < _NPCM) {
			pv = &pc->pc_pventry[field * 64 + bit];
			pc->pc_map[field] &= ~(1ul << bit);
			/* If this was the last item, move it to tail */
			if (pc->pc_map[0] == 0 && pc->pc_map[1] == 0 &&
			    pc->pc_map[2] == 0) {
				TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
				TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc,
				    pc_list);
			}
			PV_STAT(counter_u64_add(pv_entry_count, 1));
			PV_STAT(counter_u64_add(pv_entry_spare, -1));
			return (pv);
		}
	}
	/* No free items, allocate another chunk */
	m = vm_page_alloc_noobj(VM_ALLOC_WIRED);
	if (m == NULL) {
		if (lockp == NULL) {
			PV_STAT(counter_u64_add(pc_chunk_tryfail, 1));
			return (NULL);
		}
		m = reclaim_pv_chunk(pmap, lockp);
		if (m == NULL)
			goto retry;
	} else
		counter_u64_add(pv_page_count, 1);
	PV_STAT(counter_u64_add(pc_chunk_count, 1));
	PV_STAT(counter_u64_add(pc_chunk_allocs, 1));
	dump_add_page(m->phys_addr);
	pc = (void *)PHYS_TO_DMAP(m->phys_addr);
	pc->pc_pmap = pmap;
	pc->pc_map[0] = PC_FREEN & ~1ul;	/* preallocated bit 0 */
	pc->pc_map[1] = PC_FREEN;
	pc->pc_map[2] = PC_FREEL;
	pvc = &pv_chunks[vm_page_domain(m)];
	mtx_lock(&pvc->pvc_lock);
	TAILQ_INSERT_TAIL(&pvc->pvc_list, pc, pc_lru);
	mtx_unlock(&pvc->pvc_lock);
	pv = &pc->pc_pventry[0];
	TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
	PV_STAT(counter_u64_add(pv_entry_count, 1));
	PV_STAT(counter_u64_add(pv_entry_spare, _NPCPV - 1));
	return (pv);
}

/*
 * Returns the number of one bits within the given PV chunk map.
 *
 * The erratas for Intel processors state that "POPCNT Instruction May
 * Take Longer to Execute Than Expected".  It is believed that the
 * issue is the spurious dependency on the destination register.
 * Provide a hint to the register rename logic that the destination
 * value is overwritten, by clearing it, as suggested in the
 * optimization manual.  It should be cheap for unaffected processors
 * as well.
 *
 * Reference numbers for erratas are
 * 4th Gen Core: HSD146
 * 5th Gen Core: BDM85
 * 6th Gen Core: SKL029
 */
static int
popcnt_pc_map_pq(uint64_t *map)
{
	u_long result, tmp;

	__asm __volatile("xorl %k0,%k0;popcntq %2,%0;"
	    "xorl %k1,%k1;popcntq %3,%1;addl %k1,%k0;"
	    "xorl %k1,%k1;popcntq %4,%1;addl %k1,%k0"
	    : "=&r" (result), "=&r" (tmp)
	    : "m" (map[0]), "m" (map[1]), "m" (map[2]));
	return (result);
}

/*
 * Ensure that the number of spare PV entries in the specified pmap meets or
 * exceeds the given count, "needed".
 *
 * The given PV list lock may be released.
 */
static void
reserve_pv_entries(pmap_t pmap, int needed, struct rwlock **lockp)
{
	struct pv_chunks_list *pvc;
	struct pch new_tail[PMAP_MEMDOM];
	struct pv_chunk *pc;
	vm_page_t m;
	int avail, free, i;
	bool reclaimed;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("reserve_pv_entries: lockp is NULL"));

	/*
	 * Newly allocated PV chunks must be stored in a private list until
	 * the required number of PV chunks have been allocated.  Otherwise,
	 * reclaim_pv_chunk() could recycle one of these chunks.  In
	 * contrast, these chunks must be added to the pmap upon allocation.
	 */
	for (i = 0; i < PMAP_MEMDOM; i++)
		TAILQ_INIT(&new_tail[i]);
retry:
	avail = 0;
	TAILQ_FOREACH(pc, &pmap->pm_pvchunk, pc_list) {
#ifndef __POPCNT__
		if ((cpu_feature2 & CPUID2_POPCNT) == 0)
			bit_count((bitstr_t *)pc->pc_map, 0,
			    sizeof(pc->pc_map) * NBBY, &free);
		else
#endif
		free = popcnt_pc_map_pq(pc->pc_map);
		if (free == 0)
			break;
		avail += free;
		if (avail >= needed)
			break;
	}
	for (reclaimed = false; avail < needed; avail += _NPCPV) {
		m = vm_page_alloc_noobj(VM_ALLOC_WIRED);
		if (m == NULL) {
			m = reclaim_pv_chunk(pmap, lockp);
			if (m == NULL)
				goto retry;
			reclaimed = true;
		} else
			counter_u64_add(pv_page_count, 1);
		PV_STAT(counter_u64_add(pc_chunk_count, 1));
		PV_STAT(counter_u64_add(pc_chunk_allocs, 1));
		dump_add_page(m->phys_addr);
		pc = (void *)PHYS_TO_DMAP(m->phys_addr);
		pc->pc_pmap = pmap;
		pc->pc_map[0] = PC_FREEN;
		pc->pc_map[1] = PC_FREEN;
		pc->pc_map[2] = PC_FREEL;
		TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&new_tail[vm_page_domain(m)], pc, pc_lru);
		PV_STAT(counter_u64_add(pv_entry_spare, _NPCPV));

		/*
		 * The reclaim might have freed a chunk from the current pmap.
		 * If that chunk contained available entries, we need to
		 * re-count the number of available entries.
		 */
		if (reclaimed)
			goto retry;
	}
	for (i = 0; i < vm_ndomains; i++) {
		if (TAILQ_EMPTY(&new_tail[i]))
			continue;
		pvc = &pv_chunks[i];
		mtx_lock(&pvc->pvc_lock);
		TAILQ_CONCAT(&pvc->pvc_list, &new_tail[i], pc_lru);
		mtx_unlock(&pvc->pvc_lock);
	}
}

/*
 * First find and then remove the pv entry for the specified pmap and virtual
 * address from the specified pv list.  Returns the pv entry if found and NULL
 * otherwise.  This operation can be performed on pv lists for either 4KB or
 * 2MB page mappings.
 */
static __inline pv_entry_t
pmap_pvh_remove(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
		if (pmap == PV_PMAP(pv) && va == pv->pv_va) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
			pvh->pv_gen++;
			break;
		}
	}
	return (pv);
}

/*
 * After demotion from a 2MB page mapping to 512 4KB page mappings,
 * destroy the pv entry for the 2MB page mapping and reinstantiate the pv
 * entries for each of the 4KB page mappings.
 */
static void
pmap_pv_demote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	struct pv_chunk *pc;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;
	int bit, field;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((pa & PDRMASK) == 0,
	    ("pmap_pv_demote_pde: pa is not 2mpage aligned"));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the 2mpage's pv entry for this mapping to the first
	 * page's pv list.  Once this transfer begins, the pv list lock
	 * must not be released until the last pv entry is reinstantiated.
	 */
	pvh = pa_to_pvh(pa);
	va = trunc_2mpage(va);
	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_demote_pde: pv not found"));
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
	m->md.pv_gen++;
	/* Instantiate the remaining NPTEPG - 1 pv entries. */
	PV_STAT(counter_u64_add(pv_entry_allocs, NPTEPG - 1));
	va_last = va + NBPDR - PAGE_SIZE;
	for (;;) {
		pc = TAILQ_FIRST(&pmap->pm_pvchunk);
		KASSERT(pc->pc_map[0] != 0 || pc->pc_map[1] != 0 ||
		    pc->pc_map[2] != 0, ("pmap_pv_demote_pde: missing spare"));
		for (field = 0; field < _NPCM; field++) {
			while (pc->pc_map[field]) {
				bit = bsfq(pc->pc_map[field]);
				pc->pc_map[field] &= ~(1ul << bit);
				pv = &pc->pc_pventry[field * 64 + bit];
				va += PAGE_SIZE;
				pv->pv_va = va;
				m++;
				KASSERT((m->oflags & VPO_UNMANAGED) == 0,
			    ("pmap_pv_demote_pde: page %p is not managed", m));
				TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
				m->md.pv_gen++;
				if (va == va_last)
					goto out;
			}
		}
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
	}
out:
	if (pc->pc_map[0] == 0 && pc->pc_map[1] == 0 && pc->pc_map[2] == 0) {
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
	}
	PV_STAT(counter_u64_add(pv_entry_count, NPTEPG - 1));
	PV_STAT(counter_u64_add(pv_entry_spare, -(NPTEPG - 1)));
}

#if VM_NRESERVLEVEL > 0
/*
 * After promotion from 512 4KB page mappings to a single 2MB page mapping,
 * replace the many pv entries for the 4KB page mappings by a single pv entry
 * for the 2MB page mapping.
 */
static void
pmap_pv_promote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;

	KASSERT((pa & PDRMASK) == 0,
	    ("pmap_pv_promote_pde: pa is not 2mpage aligned"));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the first page's pv entry for this mapping to the 2mpage's
	 * pv list.  Aside from avoiding the cost of a call to get_pv_entry(),
	 * a transfer avoids the possibility that get_pv_entry() calls
	 * reclaim_pv_chunk() and that reclaim_pv_chunk() removes one of the
	 * mappings that is being promoted.
	 */
	m = PHYS_TO_VM_PAGE(pa);
	va = trunc_2mpage(va);
	pv = pmap_pvh_remove(&m->md, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_promote_pde: pv not found"));
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
	pvh->pv_gen++;
	/* Free the remaining NPTEPG - 1 pv entries. */
	va_last = va + NBPDR - PAGE_SIZE;
	do {
		m++;
		va += PAGE_SIZE;
		pmap_pvh_free(&m->md, pmap, va);
	} while (va < va_last);
}
#endif /* VM_NRESERVLEVEL > 0 */

/*
 * First find and then destroy the pv entry for the specified pmap and virtual
 * address.  This operation can be performed on pv lists for either 4KB or 2MB
 * page mappings.
 */
static void
pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pvh_free: pv not found"));
	free_pv_entry(pmap, pv);
}

/*
 * Conditionally create the PV entry for a 4KB page mapping if the required
 * memory can be allocated without resorting to reclamation.
 */
static bool
pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct rwlock **lockp)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/* Pass NULL instead of the lock pointer to disable reclamation. */
	if ((pv = get_pv_entry(pmap, NULL)) != NULL) {
		pv->pv_va = va;
		CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		return (true);
	} else
		return (false);
}

/*
 * Create the PV entry for a 2MB page mapping.  Always returns true unless the
 * flag PMAP_ENTER_NORECLAIM is specified.  If that flag is specified, returns
 * false if the PV entry cannot be allocated without resorting to reclamation.
 */
static bool
pmap_pv_insert_pde(pmap_t pmap, vm_offset_t va, pd_entry_t pde, u_int flags,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_paddr_t pa;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/* Pass NULL instead of the lock pointer to disable reclamation. */
	if ((pv = get_pv_entry(pmap, (flags & PMAP_ENTER_NORECLAIM) != 0 ?
	    NULL : lockp)) == NULL)
		return (false);
	pv->pv_va = va;
	pa = pde & PG_PS_FRAME;
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
	pvh->pv_gen++;
	return (true);
}

/*
 * Fills a page table page with mappings to consecutive physical pages.
 */
static void
pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte)
{
	pt_entry_t *pte;

	for (pte = firstpte; pte < firstpte + NPTEPG; pte++) {
		*pte = newpte;
		newpte += PAGE_SIZE;
	}
}

/*
 * Tries to demote a 2MB page mapping.  If demotion fails, the 2MB page
 * mapping is invalidated.
 */
static bool
pmap_demote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	struct rwlock *lock;
	bool rv;

	lock = NULL;
	rv = pmap_demote_pde_locked(pmap, pde, va, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	return (rv);
}

static void
pmap_demote_pde_check(pt_entry_t *firstpte __unused, pt_entry_t newpte __unused)
{
#ifdef INVARIANTS
#ifdef DIAGNOSTIC
	pt_entry_t *xpte, *ypte;

	for (xpte = firstpte; xpte < firstpte + NPTEPG;
	    xpte++, newpte += PAGE_SIZE) {
		if ((*xpte & PG_FRAME) != (newpte & PG_FRAME)) {
			printf("pmap_demote_pde: xpte %zd and newpte map "
			    "different pages: found %#lx, expected %#lx\n",
			    xpte - firstpte, *xpte, newpte);
			printf("page table dump\n");
			for (ypte = firstpte; ypte < firstpte + NPTEPG; ypte++)
				printf("%zd %#lx\n", ypte - firstpte, *ypte);
			panic("firstpte");
		}
	}
#else
	KASSERT((*firstpte & PG_FRAME) == (newpte & PG_FRAME),
	    ("pmap_demote_pde: firstpte and newpte map different physical"
	    " addresses"));
#endif
#endif
}

static void
pmap_demote_pde_abort(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    pd_entry_t oldpde, struct rwlock **lockp)
{
	struct spglist free;
	vm_offset_t sva;

	SLIST_INIT(&free);
	sva = trunc_2mpage(va);
	pmap_remove_pde(pmap, pde, sva, &free, lockp);
	if ((oldpde & pmap_global_bit(pmap)) == 0)
		pmap_invalidate_pde_page(pmap, sva, oldpde);
	vm_page_free_pages_toq(&free, true);
	CTR2(KTR_PMAP, "pmap_demote_pde: failure for va %#lx in pmap %p",
	    va, pmap);
}

static bool
pmap_demote_pde_locked(pmap_t pmap, pd_entry_t *pde, vm_offset_t va,
    struct rwlock **lockp)
{
	pd_entry_t newpde, oldpde;
	pt_entry_t *firstpte, newpte;
	pt_entry_t PG_A, PG_G, PG_M, PG_PKU_MASK, PG_RW, PG_V;
	vm_paddr_t mptepa;
	vm_page_t mpte;
	int PG_PTE_CACHE;
	bool in_kernel;

	PG_A = pmap_accessed_bit(pmap);
	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_PTE_CACHE = pmap_cache_mask(pmap, false);
	PG_PKU_MASK = pmap_pku_mask_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	in_kernel = va >= VM_MAXUSER_ADDRESS;
	oldpde = *pde;
	KASSERT((oldpde & (PG_PS | PG_V)) == (PG_PS | PG_V),
	    ("pmap_demote_pde: oldpde is missing PG_PS and/or PG_V"));

	/*
	 * Invalidate the 2MB page mapping and return "failure" if the
	 * mapping was never accessed.
	 */
	if ((oldpde & PG_A) == 0) {
		KASSERT((oldpde & PG_W) == 0,
		    ("pmap_demote_pde: a wired mapping is missing PG_A"));
		pmap_demote_pde_abort(pmap, va, pde, oldpde, lockp);
		return (false);
	}

	mpte = pmap_remove_pt_page(pmap, va);
	if (mpte == NULL) {
		KASSERT((oldpde & PG_W) == 0,
		    ("pmap_demote_pde: page table page for a wired mapping"
		    " is missing"));

		/*
		 * If the page table page is missing and the mapping
		 * is for a kernel address, the mapping must belong to
		 * the direct map.  Page table pages are preallocated
		 * for every other part of the kernel address space,
		 * so the direct map region is the only part of the
		 * kernel address space that must be handled here.
		 */
		KASSERT(!in_kernel || (va >= DMAP_MIN_ADDRESS &&
		    va < DMAP_MAX_ADDRESS),
		    ("pmap_demote_pde: No saved mpte for va %#lx", va));

		/*
		 * If the 2MB page mapping belongs to the direct map
		 * region of the kernel's address space, then the page
		 * allocation request specifies the highest possible
		 * priority (VM_ALLOC_INTERRUPT).  Otherwise, the
		 * priority is normal.
		 */
		mpte = pmap_alloc_pt_page(pmap, pmap_pde_pindex(va),
		    (in_kernel ? VM_ALLOC_INTERRUPT : 0) | VM_ALLOC_WIRED);

		/*
		 * If the allocation of the new page table page fails,
		 * invalidate the 2MB page mapping and return "failure".
		 */
		if (mpte == NULL) {
			pmap_demote_pde_abort(pmap, va, pde, oldpde, lockp);
			return (false);
		}

		if (!in_kernel)
			mpte->ref_count = NPTEPG;
	}
	mptepa = VM_PAGE_TO_PHYS(mpte);
	firstpte = (pt_entry_t *)PHYS_TO_DMAP(mptepa);
	newpde = mptepa | PG_M | PG_A | (oldpde & PG_U) | PG_RW | PG_V;
	KASSERT((oldpde & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_demote_pde: oldpde is missing PG_M"));
	newpte = oldpde & ~PG_PS;
	newpte = pmap_swap_pat(pmap, newpte);

	/*
	 * If the PTP is not leftover from an earlier promotion or it does not
	 * have PG_A set in every PTE, then fill it.  The new PTEs will all
	 * have PG_A set.
	 */
	if (!vm_page_all_valid(mpte))
		pmap_fill_ptp(firstpte, newpte);

	pmap_demote_pde_check(firstpte, newpte);

	/*
	 * If the mapping has changed attributes, update the PTEs.
	 */
	if ((*firstpte & PG_PTE_PROMOTE) != (newpte & PG_PTE_PROMOTE))
		pmap_fill_ptp(firstpte, newpte);

	/*
	 * The spare PV entries must be reserved prior to demoting the
	 * mapping, that is, prior to changing the PDE.  Otherwise, the state
	 * of the PDE and the PV lists will be inconsistent, which can result
	 * in reclaim_pv_chunk() attempting to remove a PV entry from the
	 * wrong PV list and pmap_pv_demote_pde() failing to find the expected
	 * PV entry for the 2MB page mapping that is being demoted.
	 */
	if ((oldpde & PG_MANAGED) != 0)
		reserve_pv_entries(pmap, NPTEPG - 1, lockp);

	/*
	 * Demote the mapping.  This pmap is locked.  The old PDE has
	 * PG_A set.  If the old PDE has PG_RW set, it also has PG_M
	 * set.  Thus, there is no danger of a race with another
	 * processor changing the setting of PG_A and/or PG_M between
	 * the read above and the store below. 
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, newpde);
	else
		pde_store(pde, newpde);

	/*
	 * Invalidate a stale recursive mapping of the page table page.
	 */
	if (in_kernel)
		pmap_invalidate_page(pmap, (vm_offset_t)vtopte(va));

	/*
	 * Demote the PV entry.
	 */
	if ((oldpde & PG_MANAGED) != 0)
		pmap_pv_demote_pde(pmap, va, oldpde & PG_PS_FRAME, lockp);

	counter_u64_add(pmap_pde_demotions, 1);
	CTR2(KTR_PMAP, "pmap_demote_pde: success for va %#lx in pmap %p",
	    va, pmap);
	return (true);
}

/*
 * pmap_remove_kernel_pde: Remove a kernel superpage mapping.
 */
static void
pmap_remove_kernel_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	pd_entry_t newpde;
	vm_paddr_t mptepa;
	vm_page_t mpte;

	KASSERT(pmap == kernel_pmap, ("pmap %p is not kernel_pmap", pmap));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mpte = pmap_remove_pt_page(pmap, va);
	if (mpte == NULL)
		panic("pmap_remove_kernel_pde: Missing pt page.");

	mptepa = VM_PAGE_TO_PHYS(mpte);
	newpde = mptepa | X86_PG_M | X86_PG_A | X86_PG_RW | X86_PG_V;

	/*
	 * If this page table page was unmapped by a promotion, then it
	 * contains valid mappings.  Zero it to invalidate those mappings.
	 */
	if (vm_page_any_valid(mpte))
		pagezero((void *)PHYS_TO_DMAP(mptepa));

	/*
	 * Demote the mapping.
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, newpde);
	else
		pde_store(pde, newpde);

	/*
	 * Invalidate a stale recursive mapping of the page table page.
	 */
	pmap_invalidate_page(pmap, (vm_offset_t)vtopte(va));
}

/*
 * pmap_remove_pde: do the things to unmap a superpage in a process
 */
static int
pmap_remove_pde(pmap_t pmap, pd_entry_t *pdq, vm_offset_t sva,
    struct spglist *free, struct rwlock **lockp)
{
	struct md_page *pvh;
	pd_entry_t oldpde;
	vm_offset_t eva, va;
	vm_page_t m, mpte;
	pt_entry_t PG_G, PG_A, PG_M, PG_RW;

	PG_G = pmap_global_bit(pmap);
	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("pmap_remove_pde: sva is not 2mpage aligned"));
	oldpde = pte_load_clear(pdq);
	if (oldpde & PG_W)
		pmap->pm_stats.wired_count -= NBPDR / PAGE_SIZE;
	if ((oldpde & PG_G) != 0)
		pmap_invalidate_pde_page(kernel_pmap, sva, oldpde);
	pmap_resident_count_adj(pmap, -NBPDR / PAGE_SIZE);
	if (oldpde & PG_MANAGED) {
		CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, oldpde & PG_PS_FRAME);
		pvh = pa_to_pvh(oldpde & PG_PS_FRAME);
		pmap_pvh_free(pvh, pmap, sva);
		eva = sva + NBPDR;
		for (va = sva, m = PHYS_TO_VM_PAGE(oldpde & PG_PS_FRAME);
		    va < eva; va += PAGE_SIZE, m++) {
			if ((oldpde & (PG_M | PG_RW)) == (PG_M | PG_RW))
				vm_page_dirty(m);
			if (oldpde & PG_A)
				vm_page_aflag_set(m, PGA_REFERENCED);
			if (TAILQ_EMPTY(&m->md.pv_list) &&
			    TAILQ_EMPTY(&pvh->pv_list))
				vm_page_aflag_clear(m, PGA_WRITEABLE);
			pmap_delayed_invl_page(m);
		}
	}
	if (pmap == kernel_pmap) {
		pmap_remove_kernel_pde(pmap, pdq, sva);
	} else {
		mpte = pmap_remove_pt_page(pmap, sva);
		if (mpte != NULL) {
			KASSERT(vm_page_any_valid(mpte),
			    ("pmap_remove_pde: pte page not promoted"));
			pmap_pt_page_count_adj(pmap, -1);
			KASSERT(mpte->ref_count == NPTEPG,
			    ("pmap_remove_pde: pte page ref count error"));
			mpte->ref_count = 0;
			pmap_add_delayed_free_list(mpte, free, false);
		}
	}
	return (pmap_unuse_pt(pmap, sva, *pmap_pdpe(pmap, sva), free));
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t va, 
    pd_entry_t ptepde, struct spglist *free, struct rwlock **lockp)
{
	struct md_page *pvh;
	pt_entry_t oldpte, PG_A, PG_M, PG_RW;
	vm_page_t m;

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpte = pte_load_clear(ptq);
	if (oldpte & PG_W)
		pmap->pm_stats.wired_count -= 1;
	pmap_resident_count_adj(pmap, -1);
	if (oldpte & PG_MANAGED) {
		m = PHYS_TO_VM_PAGE(oldpte & PG_FRAME);
		if ((oldpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		if (oldpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);
		CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
		pmap_pvh_free(&m->md, pmap, va);
		if (TAILQ_EMPTY(&m->md.pv_list) &&
		    (m->flags & PG_FICTITIOUS) == 0) {
			pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
			if (TAILQ_EMPTY(&pvh->pv_list))
				vm_page_aflag_clear(m, PGA_WRITEABLE);
		}
		pmap_delayed_invl_page(m);
	}
	return (pmap_unuse_pt(pmap, va, ptepde, free));
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    struct spglist *free)
{
	struct rwlock *lock;
	pt_entry_t *pte, PG_V;

	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if ((*pde & PG_V) == 0)
		return;
	pte = pmap_pde_to_pte(pde, va);
	if ((*pte & PG_V) == 0)
		return;
	lock = NULL;
	pmap_remove_pte(pmap, pte, va, *pde, free, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	pmap_invalidate_page(pmap, va);
}

/*
 * Removes the specified range of addresses from the page table page.
 */
static bool
pmap_remove_ptes(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
    pd_entry_t *pde, struct spglist *free, struct rwlock **lockp)
{
	pt_entry_t PG_G, *pte;
	vm_offset_t va;
	bool anyvalid;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PG_G = pmap_global_bit(pmap);
	anyvalid = false;
	va = eva;
	for (pte = pmap_pde_to_pte(pde, sva); sva != eva; pte++,
	    sva += PAGE_SIZE) {
		if (*pte == 0) {
			if (va != eva) {
				pmap_invalidate_range(pmap, va, sva);
				va = eva;
			}
			continue;
		}
		if ((*pte & PG_G) == 0)
			anyvalid = true;
		else if (va == eva)
			va = sva;
		if (pmap_remove_pte(pmap, pte, sva, *pde, free, lockp)) {
			sva += PAGE_SIZE;
			break;
		}
	}
	if (va != eva)
		pmap_invalidate_range(pmap, va, sva);
	return (anyvalid);
}

static void
pmap_remove1(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, bool map_delete)
{
	struct rwlock *lock;
	vm_page_t mt;
	vm_offset_t va_next;
	pml5_entry_t *pml5e;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t ptpaddr, *pde;
	pt_entry_t PG_G, PG_V;
	struct spglist free;
	int anyvalid;

	PG_G = pmap_global_bit(pmap);
	PG_V = pmap_valid_bit(pmap);

	/*
	 * If there are no resident pages besides the top level page
	 * table page(s), there is nothing to do.  Kernel pmap always
	 * accounts whole preloaded area as resident, which makes its
	 * resident count > 2.
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count <= 1 + (pmap->pm_pmltopu != NULL ?
	    1 : 0))
		return;

	anyvalid = 0;
	SLIST_INIT(&free);

	pmap_delayed_invl_start();
	PMAP_LOCK(pmap);
	if (map_delete)
		pmap_pkru_on_remove(pmap, sva, eva);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if (sva + PAGE_SIZE == eva) {
		pde = pmap_pde(pmap, sva);
		if (pde && (*pde & PG_PS) == 0) {
			pmap_remove_page(pmap, sva, pde, &free);
			goto out;
		}
	}

	lock = NULL;
	for (; sva < eva; sva = va_next) {
		if (pmap->pm_stats.resident_count == 0)
			break;

		if (pmap_is_la57(pmap)) {
			pml5e = pmap_pml5e(pmap, sva);
			if ((*pml5e & PG_V) == 0) {
				va_next = (sva + NBPML5) & ~PML5MASK;
				if (va_next < sva)
					va_next = eva;
				continue;
			}
			pml4e = pmap_pml5e_to_pml4e(pml5e, sva);
		} else {
			pml4e = pmap_pml4e(pmap, sva);
		}
		if ((*pml4e & PG_V) == 0) {
			va_next = (sva + NBPML4) & ~PML4MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + NBPDP) & ~PDPMASK;
		if (va_next < sva)
			va_next = eva;
		pdpe = pmap_pml4e_to_pdpe(pml4e, sva);
		if ((*pdpe & PG_V) == 0)
			continue;
		if ((*pdpe & PG_PS) != 0) {
			KASSERT(va_next <= eva,
			    ("partial update of non-transparent 1G mapping "
			    "pdpe %#lx sva %#lx eva %#lx va_next %#lx",
			    *pdpe, sva, eva, va_next));
			MPASS(pmap != kernel_pmap); /* XXXKIB */
			MPASS((*pdpe & (PG_MANAGED | PG_G)) == 0);
			anyvalid = 1;
			*pdpe = 0;
			pmap_resident_count_adj(pmap, -NBPDP / PAGE_SIZE);
			mt = PHYS_TO_VM_PAGE(*pmap_pml4e(pmap, sva) & PG_FRAME);
			pmap_unwire_ptp(pmap, sva, mt, &free);
			continue;
		}

		/*
		 * Calculate index for next page table.
		 */
		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);
		ptpaddr = *pde;

		/*
		 * Weed out invalid mappings.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & PG_PS) != 0) {
			/*
			 * Are we removing the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == va_next && eva >= va_next) {
				/*
				 * The TLB entry for a PG_G mapping is
				 * invalidated by pmap_remove_pde().
				 */
				if ((ptpaddr & PG_G) == 0)
					anyvalid = 1;
				pmap_remove_pde(pmap, pde, sva, &free, &lock);
				continue;
			} else if (!pmap_demote_pde_locked(pmap, pde, sva,
			    &lock)) {
				/* The large page mapping was destroyed. */
				continue;
			} else
				ptpaddr = *pde;
		}

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (va_next > eva)
			va_next = eva;

		if (pmap_remove_ptes(pmap, sva, va_next, pde, &free, &lock))
			anyvalid = 1;
	}
	if (lock != NULL)
		rw_wunlock(lock);
out:
	if (anyvalid)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
	pmap_delayed_invl_finish();
	vm_page_free_pages_toq(&free, true);
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	pmap_remove1(pmap, sva, eva, false);
}

/*
 *	Remove the given range of addresses as part of a logical unmap
 *	operation. This has the effect of calling pmap_remove(), but
 *	also clears any metadata that should persist for the lifetime
 *	of a logical mapping.
 */
void
pmap_map_delete(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	pmap_remove1(pmap, sva, eva, true);
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
 *
 *	Notes:
 *		Original versions of this routine were very
 *		inefficient because they iteratively called
 *		pmap_remove (slow...)
 */

void
pmap_remove_all(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv;
	pmap_t pmap;
	struct rwlock *lock;
	pt_entry_t *pte, tpte, PG_A, PG_M, PG_RW;
	pd_entry_t *pde;
	vm_offset_t va;
	struct spglist free;
	int pvh_gen, md_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	SLIST_INIT(&free);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
	rw_wlock(lock);
retry:
	while ((pv = TAILQ_FIRST(&pvh->pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		(void)pmap_demote_pde_locked(pmap, pde, va, &lock);
		PMAP_UNLOCK(pmap);
	}
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		PG_A = pmap_accessed_bit(pmap);
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		pmap_resident_count_adj(pmap, -1);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0, ("pmap_remove_all: found"
		    " a 2mpage in page %p's pv list", m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		tpte = pte_load_clear(pte);
		if (tpte & PG_W)
			pmap->pm_stats.wired_count--;
		if (tpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		pmap_unuse_pt(pmap, pv->pv_va, *pde, &free);
		pmap_invalidate_page(pmap, pv->pv_va);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		free_pv_entry(pmap, pv);
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(lock);
	pmap_delayed_invl_wait(m);
	vm_page_free_pages_toq(&free, true);
}

/*
 * pmap_protect_pde: do the things to protect a 2mpage in a process
 */
static bool
pmap_protect_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t sva, vm_prot_t prot)
{
	pd_entry_t newpde, oldpde;
	vm_page_t m, mt;
	bool anychanged;
	pt_entry_t PG_G, PG_M, PG_RW;

	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("pmap_protect_pde: sva is not 2mpage aligned"));
	anychanged = false;
retry:
	oldpde = newpde = *pde;
	if ((prot & VM_PROT_WRITE) == 0) {
		if ((oldpde & (PG_MANAGED | PG_M | PG_RW)) ==
		    (PG_MANAGED | PG_M | PG_RW)) {
			m = PHYS_TO_VM_PAGE(oldpde & PG_PS_FRAME);
			for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
				vm_page_dirty(mt);
		}
		newpde &= ~(PG_RW | PG_M);
	}
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpde |= pg_nx;
	if (newpde != oldpde) {
		/*
		 * As an optimization to future operations on this PDE, clear
		 * PG_PROMOTED.  The impending invalidation will remove any
		 * lingering 4KB page mappings from the TLB.
		 */
		if (!atomic_cmpset_long(pde, oldpde, newpde & ~PG_PROMOTED))
			goto retry;
		if ((oldpde & PG_G) != 0)
			pmap_invalidate_pde_page(kernel_pmap, sva, oldpde);
		else
			anychanged = true;
	}
	return (anychanged);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_page_t m;
	vm_offset_t va_next;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t ptpaddr, *pde;
	pt_entry_t *pte, PG_G, PG_M, PG_RW, PG_V;
	pt_entry_t obits, pbits;
	bool anychanged;

	KASSERT((prot & ~VM_PROT_ALL) == 0, ("invalid prot %x", prot));
	if (prot == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) ==
	    (VM_PROT_WRITE|VM_PROT_EXECUTE))
		return;

	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);
	anychanged = false;

	/*
	 * Although this function delays and batches the invalidation
	 * of stale TLB entries, it does not need to call
	 * pmap_delayed_invl_start() and
	 * pmap_delayed_invl_finish(), because it does not
	 * ordinarily destroy mappings.  Stale TLB entries from
	 * protection-only changes need only be invalidated before the
	 * pmap lock is released, because protection-only changes do
	 * not destroy PV entries.  Even operations that iterate over
	 * a physical page's PV list of mappings, like
	 * pmap_remove_write(), acquire the pmap lock for each
	 * mapping.  Consequently, for protection-only changes, the
	 * pmap lock suffices to synchronize both page table and TLB
	 * updates.
	 *
	 * This function only destroys a mapping if pmap_demote_pde()
	 * fails.  In that case, stale TLB entries are immediately
	 * invalidated.
	 */

	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pml4e = pmap_pml4e(pmap, sva);
		if (pml4e == NULL || (*pml4e & PG_V) == 0) {
			va_next = (sva + NBPML4) & ~PML4MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + NBPDP) & ~PDPMASK;
		if (va_next < sva)
			va_next = eva;
		pdpe = pmap_pml4e_to_pdpe(pml4e, sva);
		if ((*pdpe & PG_V) == 0)
			continue;
		if ((*pdpe & PG_PS) != 0) {
			KASSERT(va_next <= eva,
			    ("partial update of non-transparent 1G mapping "
			    "pdpe %#lx sva %#lx eva %#lx va_next %#lx",
			    *pdpe, sva, eva, va_next));
retry_pdpe:
			obits = pbits = *pdpe;
			MPASS((pbits & (PG_MANAGED | PG_G)) == 0);
			MPASS(pmap != kernel_pmap); /* XXXKIB */
			if ((prot & VM_PROT_WRITE) == 0)
				pbits &= ~(PG_RW | PG_M);
			if ((prot & VM_PROT_EXECUTE) == 0)
				pbits |= pg_nx;

			if (pbits != obits) {
				if (!atomic_cmpset_long(pdpe, obits, pbits))
					/* PG_PS cannot be cleared under us, */
					goto retry_pdpe;
				anychanged = true;
			}
			continue;
		}

		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, sva);
		ptpaddr = *pde;

		/*
		 * Weed out invalid mappings.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & PG_PS) != 0) {
			/*
			 * Are we protecting the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == va_next && eva >= va_next) {
				/*
				 * The TLB entry for a PG_G mapping is
				 * invalidated by pmap_protect_pde().
				 */
				if (pmap_protect_pde(pmap, pde, sva, prot))
					anychanged = true;
				continue;
			} else if (!pmap_demote_pde(pmap, pde, sva)) {
				/*
				 * The large page mapping was destroyed.
				 */
				continue;
			}
		}

		if (va_next > eva)
			va_next = eva;

		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
retry:
			obits = pbits = *pte;
			if ((pbits & PG_V) == 0)
				continue;

			if ((prot & VM_PROT_WRITE) == 0) {
				if ((pbits & (PG_MANAGED | PG_M | PG_RW)) ==
				    (PG_MANAGED | PG_M | PG_RW)) {
					m = PHYS_TO_VM_PAGE(pbits & PG_FRAME);
					vm_page_dirty(m);
				}
				pbits &= ~(PG_RW | PG_M);
			}
			if ((prot & VM_PROT_EXECUTE) == 0)
				pbits |= pg_nx;

			if (pbits != obits) {
				if (!atomic_cmpset_long(pte, obits, pbits))
					goto retry;
				if (obits & PG_G)
					pmap_invalidate_page(pmap, sva);
				else
					anychanged = true;
			}
		}
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
}

static bool
pmap_pde_ept_executable(pmap_t pmap, pd_entry_t pde)
{

	if (pmap->pm_type != PT_EPT)
		return (false);
	return ((pde & EPT_PG_EXECUTE) != 0);
}

#if VM_NRESERVLEVEL > 0
/*
 * Tries to promote the 512, contiguous 4KB page mappings that are within a
 * single page table page (PTP) to a single 2MB page mapping.  For promotion
 * to occur, two conditions must be met: (1) the 4KB page mappings must map
 * aligned, contiguous physical memory and (2) the 4KB page mappings must have
 * identical characteristics. 
 */
static bool
pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va, vm_page_t mpte,
    struct rwlock **lockp)
{
	pd_entry_t newpde;
	pt_entry_t *firstpte, oldpte, pa, *pte;
	pt_entry_t allpte_PG_A, PG_A, PG_G, PG_M, PG_PKU_MASK, PG_RW, PG_V;
	int PG_PTE_CACHE;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (!pmap_ps_enabled(pmap))
		return (false);

	PG_A = pmap_accessed_bit(pmap);
	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);
	PG_PKU_MASK = pmap_pku_mask_bit(pmap);
	PG_PTE_CACHE = pmap_cache_mask(pmap, false);

	/*
	 * Examine the first PTE in the specified PTP.  Abort if this PTE is
	 * ineligible for promotion due to hardware errata, invalid, or does
	 * not map the first 4KB physical page within a 2MB page.
	 */
	firstpte = (pt_entry_t *)PHYS_TO_DMAP(*pde & PG_FRAME);
	newpde = *firstpte;
	if (!pmap_allow_2m_x_page(pmap, pmap_pde_ept_executable(pmap, newpde)))
		return (false);
	if ((newpde & ((PG_FRAME & PDRMASK) | PG_V)) != PG_V) {
		counter_u64_add(pmap_pde_p_failures, 1);
		CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (false);
	}

	/*
	 * Both here and in the below "for" loop, to allow for repromotion
	 * after MADV_FREE, conditionally write protect a clean PTE before
	 * possibly aborting the promotion due to other PTE attributes.  Why?
	 * Suppose that MADV_FREE is applied to a part of a superpage, the
	 * address range [S, E).  pmap_advise() will demote the superpage
	 * mapping, destroy the 4KB page mapping at the end of [S, E), and
	 * clear PG_M and PG_A in the PTEs for the rest of [S, E).  Later,
	 * imagine that the memory in [S, E) is recycled, but the last 4KB
	 * page in [S, E) is not the last to be rewritten, or simply accessed.
	 * In other words, there is still a 4KB page in [S, E), call it P,
	 * that is writeable but PG_M and PG_A are clear in P's PTE.  Unless
	 * we write protect P before aborting the promotion, if and when P is
	 * finally rewritten, there won't be a page fault to trigger
	 * repromotion.
	 */
setpde:
	if ((newpde & (PG_M | PG_RW)) == PG_RW) {
		/*
		 * When PG_M is already clear, PG_RW can be cleared without
		 * a TLB invalidation.
		 */
		if (!atomic_fcmpset_long(firstpte, &newpde, newpde & ~PG_RW))
			goto setpde;
		newpde &= ~PG_RW;
		CTR2(KTR_PMAP, "pmap_promote_pde: protect for va %#lx"
		    " in pmap %p", va & ~PDRMASK, pmap);
	}

	/*
	 * Examine each of the other PTEs in the specified PTP.  Abort if this
	 * PTE maps an unexpected 4KB physical page or does not have identical
	 * characteristics to the first PTE.
	 */
	allpte_PG_A = newpde & PG_A;
	pa = (newpde & (PG_PS_FRAME | PG_V)) + NBPDR - PAGE_SIZE;
	for (pte = firstpte + NPTEPG - 1; pte > firstpte; pte--) {
		oldpte = *pte;
		if ((oldpte & (PG_FRAME | PG_V)) != pa) {
			counter_u64_add(pmap_pde_p_failures, 1);
			CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (false);
		}
setpte:
		if ((oldpte & (PG_M | PG_RW)) == PG_RW) {
			/*
			 * When PG_M is already clear, PG_RW can be cleared
			 * without a TLB invalidation.
			 */
			if (!atomic_fcmpset_long(pte, &oldpte, oldpte & ~PG_RW))
				goto setpte;
			oldpte &= ~PG_RW;
			CTR2(KTR_PMAP, "pmap_promote_pde: protect for va %#lx"
			    " in pmap %p", (oldpte & PG_FRAME & PDRMASK) |
			    (va & ~PDRMASK), pmap);
		}
		if ((oldpte & PG_PTE_PROMOTE) != (newpde & PG_PTE_PROMOTE)) {
			counter_u64_add(pmap_pde_p_failures, 1);
			CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (false);
		}
		allpte_PG_A &= oldpte;
		pa -= PAGE_SIZE;
	}

	/*
	 * Unless all PTEs have PG_A set, clear it from the superpage mapping,
	 * so that promotions triggered by speculative mappings, such as
	 * pmap_enter_quick(), don't automatically mark the underlying pages
	 * as referenced.
	 */
	newpde &= ~PG_A | allpte_PG_A;

	/*
	 * EPT PTEs with PG_M set and PG_A clear are not supported by early
	 * MMUs supporting EPT.
	 */
	KASSERT((newpde & PG_A) != 0 || safe_to_clear_referenced(pmap, newpde),
	    ("unsupported EPT PTE"));

	/*
	 * Save the PTP in its current state until the PDE mapping the
	 * superpage is demoted by pmap_demote_pde() or destroyed by
	 * pmap_remove_pde().  If PG_A is not set in every PTE, then request
	 * that the PTP be refilled on demotion.
	 */
	if (mpte == NULL)
		mpte = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
	KASSERT(mpte >= vm_page_array &&
	    mpte < &vm_page_array[vm_page_array_size],
	    ("pmap_promote_pde: page table page is out of range"));
	KASSERT(mpte->pindex == pmap_pde_pindex(va),
	    ("pmap_promote_pde: page table page's pindex is wrong "
	    "mpte %p pidx %#lx va %#lx va pde pidx %#lx",
	    mpte, mpte->pindex, va, pmap_pde_pindex(va)));
	if (pmap_insert_pt_page(pmap, mpte, true, allpte_PG_A != 0)) {
		counter_u64_add(pmap_pde_p_failures, 1);
		CTR2(KTR_PMAP,
		    "pmap_promote_pde: failure for va %#lx in pmap %p", va,
		    pmap);
		return (false);
	}

	/*
	 * Promote the pv entries.
	 */
	if ((newpde & PG_MANAGED) != 0)
		pmap_pv_promote_pde(pmap, va, newpde & PG_PS_FRAME, lockp);

	/*
	 * Propagate the PAT index to its proper position.
	 */
	newpde = pmap_swap_pat(pmap, newpde);

	/*
	 * Map the superpage.
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, PG_PS | newpde);
	else
		pde_store(pde, PG_PROMOTED | PG_PS | newpde);

	counter_u64_add(pmap_pde_promotions, 1);
	CTR2(KTR_PMAP, "pmap_promote_pde: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (true);
}
#endif /* VM_NRESERVLEVEL > 0 */

static int
pmap_enter_largepage(pmap_t pmap, vm_offset_t va, pt_entry_t newpte, int flags,
    int psind)
{
	vm_page_t mp;
	pt_entry_t origpte, *pml4e, *pdpe, *pde, pten, PG_V;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(psind > 0 && psind < MAXPAGESIZES && pagesizes[psind] != 0,
	    ("psind %d unexpected", psind));
	KASSERT(((newpte & PG_FRAME) & (pagesizes[psind] - 1)) == 0,
	    ("unaligned phys address %#lx newpte %#lx psind %d",
	    newpte & PG_FRAME, newpte, psind));
	KASSERT((va & (pagesizes[psind] - 1)) == 0,
	    ("unaligned va %#lx psind %d", va, psind));
	KASSERT(va < VM_MAXUSER_ADDRESS,
	    ("kernel mode non-transparent superpage")); /* XXXKIB */
	KASSERT(va + pagesizes[psind] < VM_MAXUSER_ADDRESS,
	    ("overflowing user map va %#lx psind %d", va, psind)); /* XXXKIB */

	PG_V = pmap_valid_bit(pmap);

restart:
	if (!pmap_pkru_same(pmap, va, va + pagesizes[psind]))
		return (KERN_PROTECTION_FAILURE);
	pten = newpte;
	if (va < VM_MAXUSER_ADDRESS && pmap->pm_type == PT_X86)
		pten |= pmap_pkru_get(pmap, va);

	if (psind == 2) {	/* 1G */
		pml4e = pmap_pml4e(pmap, va);
		if (pml4e == NULL || (*pml4e & PG_V) == 0) {
			mp = pmap_allocpte_alloc(pmap, pmap_pml4e_pindex(va),
			    NULL, va);
			if (mp == NULL)
				goto allocf;
			pdpe = (pdp_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mp));
			pdpe = &pdpe[pmap_pdpe_index(va)];
			origpte = *pdpe;
			MPASS(origpte == 0);
		} else {
			pdpe = pmap_pml4e_to_pdpe(pml4e, va);
			KASSERT(pdpe != NULL, ("va %#lx lost pdpe", va));
			origpte = *pdpe;
			if ((origpte & PG_V) == 0) {
				mp = PHYS_TO_VM_PAGE(*pml4e & PG_FRAME);
				mp->ref_count++;
			}
		}
		*pdpe = pten;
	} else /* (psind == 1) */ {	/* 2M */
		pde = pmap_pde(pmap, va);
		if (pde == NULL) {
			mp = pmap_allocpte_alloc(pmap, pmap_pdpe_pindex(va),
			    NULL, va);
			if (mp == NULL)
				goto allocf;
			pde = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mp));
			pde = &pde[pmap_pde_index(va)];
			origpte = *pde;
			MPASS(origpte == 0);
		} else {
			origpte = *pde;
			if ((origpte & PG_V) == 0) {
				pdpe = pmap_pdpe(pmap, va);
				MPASS(pdpe != NULL && (*pdpe & PG_V) != 0);
				mp = PHYS_TO_VM_PAGE(*pdpe & PG_FRAME);
				mp->ref_count++;
			}
		}
		*pde = pten;
	}
	KASSERT((origpte & PG_V) == 0 || ((origpte & PG_PS) != 0 &&
	    (origpte & PG_PS_FRAME) == (pten & PG_PS_FRAME)),
	    ("va %#lx changing %s phys page origpte %#lx pten %#lx",
	    va, psind == 2 ? "1G" : "2M", origpte, pten));
	if ((pten & PG_W) != 0 && (origpte & PG_W) == 0)
		pmap->pm_stats.wired_count += pagesizes[psind] / PAGE_SIZE;
	else if ((pten & PG_W) == 0 && (origpte & PG_W) != 0)
		pmap->pm_stats.wired_count -= pagesizes[psind] / PAGE_SIZE;
	if ((origpte & PG_V) == 0)
		pmap_resident_count_adj(pmap, pagesizes[psind] / PAGE_SIZE);

	return (KERN_SUCCESS);

allocf:
	if ((flags & PMAP_ENTER_NOSLEEP) != 0)
		return (KERN_RESOURCE_SHORTAGE);
	PMAP_UNLOCK(pmap);
	vm_wait(NULL);
	PMAP_LOCK(pmap);
	goto restart;
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 *
 *	When destroying both a page table and PV entry, this function
 *	performs the TLB invalidation before releasing the PV list
 *	lock, so we do not need pmap_delayed_invl_page() calls here.
 */
int
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    u_int flags, int8_t psind)
{
	struct rwlock *lock;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_G, PG_A, PG_M, PG_RW, PG_V;
	pt_entry_t newpte, origpte;
	pv_entry_t pv;
	vm_paddr_t opa, pa;
	vm_page_t mpte, om;
	int rv;
	bool nosleep;

	PG_A = pmap_accessed_bit(pmap);
	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	va = trunc_page(va);
	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
	    ("pmap_enter: invalid to pmap_enter page table pages (va: 0x%lx)",
	    va));
	KASSERT((m->oflags & VPO_UNMANAGED) != 0 || !VA_IS_CLEANMAP(va),
	    ("pmap_enter: managed mapping within the clean submap"));
	if ((m->oflags & VPO_UNMANAGED) == 0)
		VM_PAGE_OBJECT_BUSY_ASSERT(m);
	KASSERT((flags & PMAP_ENTER_RESERVED) == 0,
	    ("pmap_enter: flags %u has reserved bits set", flags));
	pa = VM_PAGE_TO_PHYS(m);
	newpte = (pt_entry_t)(pa | PG_A | PG_V);
	if ((flags & VM_PROT_WRITE) != 0)
		newpte |= PG_M;
	if ((prot & VM_PROT_WRITE) != 0)
		newpte |= PG_RW;
	KASSERT((newpte & (PG_M | PG_RW)) != PG_M,
	    ("pmap_enter: flags includes VM_PROT_WRITE but prot doesn't"));
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpte |= pg_nx;
	if ((flags & PMAP_ENTER_WIRED) != 0)
		newpte |= PG_W;
	if (va < VM_MAXUSER_ADDRESS)
		newpte |= PG_U;
	if (pmap == kernel_pmap)
		newpte |= PG_G;
	newpte |= pmap_cache_bits(pmap, m->md.pat_mode, psind > 0);

	/*
	 * Set modified bit gratuitously for writeable mappings if
	 * the page is unmanaged. We do not want to take a fault
	 * to do the dirty bit accounting for these mappings.
	 */
	if ((m->oflags & VPO_UNMANAGED) != 0) {
		if ((newpte & PG_RW) != 0)
			newpte |= PG_M;
	} else
		newpte |= PG_MANAGED;

	lock = NULL;
	PMAP_LOCK(pmap);
	if ((flags & PMAP_ENTER_LARGEPAGE) != 0) {
		KASSERT((m->oflags & VPO_UNMANAGED) != 0,
		    ("managed largepage va %#lx flags %#x", va, flags));
		rv = pmap_enter_largepage(pmap, va, newpte | PG_PS, flags,
		    psind);
		goto out;
	}
	if (psind == 1) {
		/* Assert the required virtual and physical alignment. */ 
		KASSERT((va & PDRMASK) == 0, ("pmap_enter: va unaligned"));
		KASSERT(m->psind > 0, ("pmap_enter: m->psind < psind"));
		rv = pmap_enter_pde(pmap, va, newpte | PG_PS, flags, m, &lock);
		goto out;
	}
	mpte = NULL;

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
retry:
	pde = pmap_pde(pmap, va);
	if (pde != NULL && (*pde & PG_V) != 0 && ((*pde & PG_PS) == 0 ||
	    pmap_demote_pde_locked(pmap, pde, va, &lock))) {
		pte = pmap_pde_to_pte(pde, va);
		if (va < VM_MAXUSER_ADDRESS && mpte == NULL) {
			mpte = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
			mpte->ref_count++;
		}
	} else if (va < VM_MAXUSER_ADDRESS) {
		/*
		 * Here if the pte page isn't mapped, or if it has been
		 * deallocated.
		 */
		nosleep = (flags & PMAP_ENTER_NOSLEEP) != 0;
		mpte = pmap_allocpte_alloc(pmap, pmap_pde_pindex(va),
		    nosleep ? NULL : &lock, va);
		if (mpte == NULL && nosleep) {
			rv = KERN_RESOURCE_SHORTAGE;
			goto out;
		}
		goto retry;
	} else
		panic("pmap_enter: invalid page directory va=%#lx", va);

	origpte = *pte;
	pv = NULL;
	if (va < VM_MAXUSER_ADDRESS && pmap->pm_type == PT_X86)
		newpte |= pmap_pkru_get(pmap, va);

	/*
	 * Is the specified virtual address already mapped?
	 */
	if ((origpte & PG_V) != 0) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if ((newpte & PG_W) != 0 && (origpte & PG_W) == 0)
			pmap->pm_stats.wired_count++;
		else if ((newpte & PG_W) == 0 && (origpte & PG_W) != 0)
			pmap->pm_stats.wired_count--;

		/*
		 * Remove the extra PT page reference.
		 */
		if (mpte != NULL) {
			mpte->ref_count--;
			KASSERT(mpte->ref_count > 0,
			    ("pmap_enter: missing reference to page table page,"
			     " va: 0x%lx", va));
		}

		/*
		 * Has the physical page changed?
		 */
		opa = origpte & PG_FRAME;
		if (opa == pa) {
			/*
			 * No, might be a protection or wiring change.
			 */
			if ((origpte & PG_MANAGED) != 0 &&
			    (newpte & PG_RW) != 0)
				vm_page_aflag_set(m, PGA_WRITEABLE);
			if (((origpte ^ newpte) & ~(PG_M | PG_A)) == 0)
				goto unchanged;
			goto validate;
		}

		/*
		 * The physical page has changed.  Temporarily invalidate
		 * the mapping.  This ensures that all threads sharing the
		 * pmap keep a consistent view of the mapping, which is
		 * necessary for the correct handling of COW faults.  It
		 * also permits reuse of the old mapping's PV entry,
		 * avoiding an allocation.
		 *
		 * For consistency, handle unmanaged mappings the same way.
		 */
		origpte = pte_load_clear(pte);
		KASSERT((origpte & PG_FRAME) == opa,
		    ("pmap_enter: unexpected pa update for %#lx", va));
		if ((origpte & PG_MANAGED) != 0) {
			om = PHYS_TO_VM_PAGE(opa);

			/*
			 * The pmap lock is sufficient to synchronize with
			 * concurrent calls to pmap_page_test_mappings() and
			 * pmap_ts_referenced().
			 */
			if ((origpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
				vm_page_dirty(om);
			if ((origpte & PG_A) != 0) {
				pmap_invalidate_page(pmap, va);
				vm_page_aflag_set(om, PGA_REFERENCED);
			}
			CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, opa);
			pv = pmap_pvh_remove(&om->md, pmap, va);
			KASSERT(pv != NULL,
			    ("pmap_enter: no PV entry for %#lx", va));
			if ((newpte & PG_MANAGED) == 0)
				free_pv_entry(pmap, pv);
			if ((om->a.flags & PGA_WRITEABLE) != 0 &&
			    TAILQ_EMPTY(&om->md.pv_list) &&
			    ((om->flags & PG_FICTITIOUS) != 0 ||
			    TAILQ_EMPTY(&pa_to_pvh(opa)->pv_list)))
				vm_page_aflag_clear(om, PGA_WRITEABLE);
		} else {
			/*
			 * Since this mapping is unmanaged, assume that PG_A
			 * is set.
			 */
			pmap_invalidate_page(pmap, va);
		}
		origpte = 0;
	} else {
		/*
		 * Increment the counters.
		 */
		if ((newpte & PG_W) != 0)
			pmap->pm_stats.wired_count++;
		pmap_resident_count_adj(pmap, 1);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((newpte & PG_MANAGED) != 0) {
		if (pv == NULL) {
			pv = get_pv_entry(pmap, &lock);
			pv->pv_va = va;
		}
		CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, pa);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		m->md.pv_gen++;
		if ((newpte & PG_RW) != 0)
			vm_page_aflag_set(m, PGA_WRITEABLE);
	}

	/*
	 * Update the PTE.
	 */
	if ((origpte & PG_V) != 0) {
validate:
		origpte = pte_load_store(pte, newpte);
		KASSERT((origpte & PG_FRAME) == pa,
		    ("pmap_enter: unexpected pa update for %#lx", va));
		if ((newpte & PG_M) == 0 && (origpte & (PG_M | PG_RW)) ==
		    (PG_M | PG_RW)) {
			if ((origpte & PG_MANAGED) != 0)
				vm_page_dirty(m);

			/*
			 * Although the PTE may still have PG_RW set, TLB
			 * invalidation may nonetheless be required because
			 * the PTE no longer has PG_M set.
			 */
		} else if ((origpte & PG_NX) != 0 || (newpte & PG_NX) == 0) {
			/*
			 * This PTE change does not require TLB invalidation.
			 */
			goto unchanged;
		}
		if ((origpte & PG_A) != 0)
			pmap_invalidate_page(pmap, va);
	} else
		pte_store(pte, newpte);

unchanged:

#if VM_NRESERVLEVEL > 0
	/*
	 * If both the page table page and the reservation are fully
	 * populated, then attempt promotion.
	 */
	if ((mpte == NULL || mpte->ref_count == NPTEPG) &&
	    (m->flags & PG_FICTITIOUS) == 0 &&
	    vm_reserv_level_iffullpop(m) == 0)
		(void)pmap_promote_pde(pmap, pde, va, mpte, &lock);
#endif

	rv = KERN_SUCCESS;
out:
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
	return (rv);
}

/*
 * Tries to create a read- and/or execute-only 2MB page mapping.  Returns
 * KERN_SUCCESS if the mapping was created.  Otherwise, returns an error
 * value.  See pmap_enter_pde() for the possible error values when "no sleep",
 * "no replace", and "no reclaim" are specified.
 */
static int
pmap_enter_2mpage(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    struct rwlock **lockp)
{
	pd_entry_t newpde;
	pt_entry_t PG_V;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PG_V = pmap_valid_bit(pmap);
	newpde = VM_PAGE_TO_PHYS(m) |
	    pmap_cache_bits(pmap, m->md.pat_mode, true) | PG_PS | PG_V;
	if ((m->oflags & VPO_UNMANAGED) == 0)
		newpde |= PG_MANAGED;
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpde |= pg_nx;
	if (va < VM_MAXUSER_ADDRESS)
		newpde |= PG_U;
	return (pmap_enter_pde(pmap, va, newpde, PMAP_ENTER_NOSLEEP |
	    PMAP_ENTER_NOREPLACE | PMAP_ENTER_NORECLAIM, NULL, lockp));
}

/*
 * Returns true if every page table entry in the specified page table page is
 * zero.
 */
static bool
pmap_every_pte_zero(vm_paddr_t pa)
{
	pt_entry_t *pt_end, *pte;

	KASSERT((pa & PAGE_MASK) == 0, ("pa is misaligned"));
	pte = (pt_entry_t *)PHYS_TO_DMAP(pa);
	for (pt_end = pte + NPTEPG; pte < pt_end; pte++) {
		if (*pte != 0)
			return (false);
	}
	return (true);
}

/*
 * Tries to create the specified 2MB page mapping.  Returns KERN_SUCCESS if
 * the mapping was created, and one of KERN_FAILURE, KERN_NO_SPACE,
 * KERN_PROTECTION_FAILURE, or KERN_RESOURCE_SHORTAGE otherwise.  Returns
 * KERN_FAILURE if either (1) PMAP_ENTER_NOREPLACE was specified and a 4KB
 * page mapping already exists within the 2MB virtual address range starting
 * at the specified virtual address or (2) the requested 2MB page mapping is
 * not supported due to hardware errata.  Returns KERN_NO_SPACE if
 * PMAP_ENTER_NOREPLACE was specified and a 2MB page mapping already exists at
 * the specified virtual address.  Returns KERN_PROTECTION_FAILURE if the PKRU
 * settings are not the same across the 2MB virtual address range starting at
 * the specified virtual address.  Returns KERN_RESOURCE_SHORTAGE if either
 * (1) PMAP_ENTER_NOSLEEP was specified and a page table page allocation
 * failed or (2) PMAP_ENTER_NORECLAIM was specified and a PV entry allocation
 * failed.
 *
 * The parameter "m" is only used when creating a managed, writeable mapping.
 */
static int
pmap_enter_pde(pmap_t pmap, vm_offset_t va, pd_entry_t newpde, u_int flags,
    vm_page_t m, struct rwlock **lockp)
{
	struct spglist free;
	pd_entry_t oldpde, *pde;
	pt_entry_t PG_G, PG_RW, PG_V;
	vm_page_t mt, pdpg;
	vm_page_t uwptpg;

	PG_G = pmap_global_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);
	KASSERT((newpde & (pmap_modified_bit(pmap) | PG_RW)) != PG_RW,
	    ("pmap_enter_pde: newpde is missing PG_M"));
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	if (!pmap_allow_2m_x_page(pmap, pmap_pde_ept_executable(pmap,
	    newpde))) {
		CTR2(KTR_PMAP, "pmap_enter_pde: 2m x blocked for va %#lx"
		    " in pmap %p", va, pmap);
		return (KERN_FAILURE);
	}
	if ((pde = pmap_alloc_pde(pmap, va, &pdpg, (flags &
	    PMAP_ENTER_NOSLEEP) != 0 ? NULL : lockp)) == NULL) {
		CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (KERN_RESOURCE_SHORTAGE);
	}

	/*
	 * If pkru is not same for the whole pde range, return failure
	 * and let vm_fault() cope.  Check after pde allocation, since
	 * it could sleep.
	 */
	if (!pmap_pkru_same(pmap, va, va + NBPDR)) {
		pmap_abort_ptp(pmap, va, pdpg);
		return (KERN_PROTECTION_FAILURE);
	}
	if (va < VM_MAXUSER_ADDRESS && pmap->pm_type == PT_X86) {
		newpde &= ~X86_PG_PKU_MASK;
		newpde |= pmap_pkru_get(pmap, va);
	}

	/*
	 * If there are existing mappings, either abort or remove them.
	 */
	oldpde = *pde;
	if ((oldpde & PG_V) != 0) {
		KASSERT(pdpg == NULL || pdpg->ref_count > 1,
		    ("pmap_enter_pde: pdpg's reference count is too low"));
		if ((flags & PMAP_ENTER_NOREPLACE) != 0) {
			if ((oldpde & PG_PS) != 0) {
				if (pdpg != NULL)
					pdpg->ref_count--;
				CTR2(KTR_PMAP,
				    "pmap_enter_pde: no space for va %#lx"
				    " in pmap %p", va, pmap);
				return (KERN_NO_SPACE);
			} else if (va < VM_MAXUSER_ADDRESS ||
			    !pmap_every_pte_zero(oldpde & PG_FRAME)) {
				if (pdpg != NULL)
					pdpg->ref_count--;
				CTR2(KTR_PMAP,
				    "pmap_enter_pde: failure for va %#lx"
				    " in pmap %p", va, pmap);
				return (KERN_FAILURE);
			}
		}
		/* Break the existing mapping(s). */
		SLIST_INIT(&free);
		if ((oldpde & PG_PS) != 0) {
			/*
			 * The reference to the PD page that was acquired by
			 * pmap_alloc_pde() ensures that it won't be freed.
			 * However, if the PDE resulted from a promotion, then
			 * a reserved PT page could be freed.
			 */
			(void)pmap_remove_pde(pmap, pde, va, &free, lockp);
			if ((oldpde & PG_G) == 0)
				pmap_invalidate_pde_page(pmap, va, oldpde);
		} else {
			pmap_delayed_invl_start();
			if (pmap_remove_ptes(pmap, va, va + NBPDR, pde, &free,
			    lockp))
		               pmap_invalidate_all(pmap);
			pmap_delayed_invl_finish();
		}
		if (va < VM_MAXUSER_ADDRESS) {
			vm_page_free_pages_toq(&free, true);
			KASSERT(*pde == 0, ("pmap_enter_pde: non-zero pde %p",
			    pde));
		} else {
			KASSERT(SLIST_EMPTY(&free),
			    ("pmap_enter_pde: freed kernel page table page"));

			/*
			 * Both pmap_remove_pde() and pmap_remove_ptes() will
			 * leave the kernel page table page zero filled.
			 */
			mt = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
			if (pmap_insert_pt_page(pmap, mt, false, false))
				panic("pmap_enter_pde: trie insert failed");
		}
	}

	/*
	 * Allocate leaf ptpage for wired userspace pages.
	 */
	uwptpg = NULL;
	if ((newpde & PG_W) != 0 && pmap != kernel_pmap) {
		uwptpg = pmap_alloc_pt_page(pmap, pmap_pde_pindex(va),
		    VM_ALLOC_WIRED);
		if (uwptpg == NULL)
			return (KERN_RESOURCE_SHORTAGE);
		if (pmap_insert_pt_page(pmap, uwptpg, true, false)) {
			pmap_free_pt_page(pmap, uwptpg, false);
			return (KERN_RESOURCE_SHORTAGE);
		}

		uwptpg->ref_count = NPTEPG;
	}
	if ((newpde & PG_MANAGED) != 0) {
		/*
		 * Abort this mapping if its PV entry could not be created.
		 */
		if (!pmap_pv_insert_pde(pmap, va, newpde, flags, lockp)) {
			if (pdpg != NULL)
				pmap_abort_ptp(pmap, va, pdpg);
			if (uwptpg != NULL) {
				mt = pmap_remove_pt_page(pmap, va);
				KASSERT(mt == uwptpg,
				    ("removed pt page %p, expected %p", mt,
				    uwptpg));
				uwptpg->ref_count = 1;
				pmap_free_pt_page(pmap, uwptpg, false);
			}
			CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (KERN_RESOURCE_SHORTAGE);
		}
		if ((newpde & PG_RW) != 0) {
			for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
				vm_page_aflag_set(mt, PGA_WRITEABLE);
		}
	}

	/*
	 * Increment counters.
	 */
	if ((newpde & PG_W) != 0)
		pmap->pm_stats.wired_count += NBPDR / PAGE_SIZE;
	pmap_resident_count_adj(pmap, NBPDR / PAGE_SIZE);

	/*
	 * Map the superpage.  (This is not a promoted mapping; there will not
	 * be any lingering 4KB page mappings in the TLB.)
	 */
	pde_store(pde, newpde);

	counter_u64_add(pmap_pde_mappings, 1);
	CTR2(KTR_PMAP, "pmap_enter_pde: success for va %#lx in pmap %p",
	    va, pmap);
	return (KERN_SUCCESS);
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
pmap_enter_object(pmap_t pmap, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{
	struct rwlock *lock;
	vm_offset_t va;
	vm_page_t m, mpte;
	vm_pindex_t diff, psize;
	int rv;

	VM_OBJECT_ASSERT_LOCKED(m_start->object);

	psize = atop(end - start);
	mpte = NULL;
	m = m_start;
	lock = NULL;
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		va = start + ptoa(diff);
		if ((va & PDRMASK) == 0 && va + NBPDR <= end &&
		    m->psind == 1 && pmap_ps_enabled(pmap) &&
		    ((rv = pmap_enter_2mpage(pmap, va, m, prot, &lock)) ==
		    KERN_SUCCESS || rv == KERN_NO_SPACE))
			m = &m[NBPDR / PAGE_SIZE - 1];
		else
			mpte = pmap_enter_quick_locked(pmap, va, m, prot,
			    mpte, &lock);
		m = TAILQ_NEXT(m, listq);
	}
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
}

/*
 * this code makes some *MAJOR* assumptions:
 * 1. Current pmap & pmap exists.
 * 2. Not wired.
 * 3. Read access.
 * 4. No page table pages.
 * but is *MUCH* faster than pmap_enter...
 */

void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	struct rwlock *lock;

	lock = NULL;
	PMAP_LOCK(pmap);
	(void)pmap_enter_quick_locked(pmap, va, m, prot, NULL, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
}

static vm_page_t
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp)
{
	pd_entry_t *pde;
	pt_entry_t newpte, *pte, PG_V;

	KASSERT(!VA_IS_CLEANMAP(va) ||
	    (m->oflags & VPO_UNMANAGED) != 0,
	    ("pmap_enter_quick_locked: managed mapping within the clean submap"));
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pde = NULL;

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		pdp_entry_t *pdpe;
		vm_pindex_t ptepindex;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = pmap_pde_pindex(va);
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->ref_count++;
		} else {
			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.  Otherwise, we
			 * attempt to allocate a page table page, passing NULL
			 * instead of the PV list lock pointer because we don't
			 * intend to sleep.  If this attempt fails, we don't
			 * retry.  Instead, we give up.
			 */
			pdpe = pmap_pdpe(pmap, va);
			if (pdpe != NULL && (*pdpe & PG_V) != 0) {
				if ((*pdpe & PG_PS) != 0)
					return (NULL);
				pde = pmap_pdpe_to_pde(pdpe, va);
				if ((*pde & PG_V) != 0) {
					if ((*pde & PG_PS) != 0)
						return (NULL);
					mpte = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
					mpte->ref_count++;
				} else {
					mpte = pmap_allocpte_alloc(pmap,
					    ptepindex, NULL, va);
					if (mpte == NULL)
						return (NULL);
				}
			} else {
				mpte = pmap_allocpte_alloc(pmap, ptepindex,
				    NULL, va);
				if (mpte == NULL)
					return (NULL);
			}
		}
		pte = (pt_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mpte));
		pte = &pte[pmap_pte_index(va)];
	} else {
		mpte = NULL;
		pte = vtopte(va);
	}
	if (*pte) {
		if (mpte != NULL)
			mpte->ref_count--;
		return (NULL);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0 &&
	    !pmap_try_insert_pv_entry(pmap, va, m, lockp)) {
		if (mpte != NULL)
			pmap_abort_ptp(pmap, va, mpte);
		return (NULL);
	}

	/*
	 * Increment counters
	 */
	pmap_resident_count_adj(pmap, 1);

	newpte = VM_PAGE_TO_PHYS(m) | PG_V |
	    pmap_cache_bits(pmap, m->md.pat_mode, false);
	if ((m->oflags & VPO_UNMANAGED) == 0)
		newpte |= PG_MANAGED;
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpte |= pg_nx;
	if (va < VM_MAXUSER_ADDRESS)
		newpte |= PG_U | pmap_pkru_get(pmap, va);
	pte_store(pte, newpte);

#if VM_NRESERVLEVEL > 0
	/*
	 * If both the PTP and the reservation are fully populated, then
	 * attempt promotion.
	 */
	if ((mpte == NULL || mpte->ref_count == NPTEPG) &&
	    (m->flags & PG_FICTITIOUS) == 0 &&
	    vm_reserv_level_iffullpop(m) == 0) {
		if (pde == NULL)
			pde = pmap_pde(pmap, va);

		/*
		 * If promotion succeeds, then the next call to this function
		 * should not be given the unmapped PTP as a hint.
		 */
		if (pmap_promote_pde(pmap, pde, va, mpte, lockp))
			mpte = NULL;
	}
#endif

	return (mpte);
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 */
void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	vm_offset_t va;

	va = (vm_offset_t)crashdumpmap + (i * PAGE_SIZE);
	pmap_kenter(va, pa);
	pmap_invlpg(kernel_pmap, va);
	return ((void *)crashdumpmap);
}

/*
 * This code maps large physical mmap regions into the
 * processor address space.  Note that some shortcuts
 * are taken, but the code works.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{
	pd_entry_t *pde;
	pt_entry_t PG_A, PG_M, PG_RW, PG_V;
	vm_paddr_t pa, ptepa;
	vm_page_t p, pdpg;
	int pat_mode;

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object->type == OBJT_DEVICE || object->type == OBJT_SG,
	    ("pmap_object_init_pt: non-device object"));
	if ((addr & (NBPDR - 1)) == 0 && (size & (NBPDR - 1)) == 0) {
		if (!pmap_ps_enabled(pmap))
			return;
		if (!vm_object_populate(object, pindex, pindex + atop(size)))
			return;
		p = vm_page_lookup(object, pindex);
		KASSERT(vm_page_all_valid(p),
		    ("pmap_object_init_pt: invalid page %p", p));
		pat_mode = p->md.pat_mode;

		/*
		 * Abort the mapping if the first page is not physically
		 * aligned to a 2MB page boundary.
		 */
		ptepa = VM_PAGE_TO_PHYS(p);
		if (ptepa & (NBPDR - 1))
			return;

		/*
		 * Skip the first page.  Abort the mapping if the rest of
		 * the pages are not physically contiguous or have differing
		 * memory attributes.
		 */
		p = TAILQ_NEXT(p, listq);
		for (pa = ptepa + PAGE_SIZE; pa < ptepa + size;
		    pa += PAGE_SIZE) {
			KASSERT(vm_page_all_valid(p),
			    ("pmap_object_init_pt: invalid page %p", p));
			if (pa != VM_PAGE_TO_PHYS(p) ||
			    pat_mode != p->md.pat_mode)
				return;
			p = TAILQ_NEXT(p, listq);
		}

		/*
		 * Map using 2MB pages.  Since "ptepa" is 2M aligned and
		 * "size" is a multiple of 2M, adding the PAT setting to "pa"
		 * will not affect the termination of this loop.
		 */ 
		PMAP_LOCK(pmap);
		for (pa = ptepa | pmap_cache_bits(pmap, pat_mode, true);
		    pa < ptepa + size; pa += NBPDR) {
			pde = pmap_alloc_pde(pmap, addr, &pdpg, NULL);
			if (pde == NULL) {
				/*
				 * The creation of mappings below is only an
				 * optimization.  If a page directory page
				 * cannot be allocated without blocking,
				 * continue on to the next mapping rather than
				 * blocking.
				 */
				addr += NBPDR;
				continue;
			}
			if ((*pde & PG_V) == 0) {
				pde_store(pde, pa | PG_PS | PG_M | PG_A |
				    PG_U | PG_RW | PG_V);
				pmap_resident_count_adj(pmap, NBPDR / PAGE_SIZE);
				counter_u64_add(pmap_pde_mappings, 1);
			} else {
				/* Continue on if the PDE is already valid. */
				pdpg->ref_count--;
				KASSERT(pdpg->ref_count > 0,
				    ("pmap_object_init_pt: missing reference "
				    "to page directory page, va: 0x%lx", addr));
			}
			addr += NBPDR;
		}
		PMAP_UNLOCK(pmap);
	}
}

/*
 *	Clear the wired attribute from the mappings for the specified range of
 *	addresses in the given pmap.  Every valid mapping within that range
 *	must have the wired attribute set.  In contrast, invalid mappings
 *	cannot have the wired attribute set, so they are ignored.
 *
 *	The wired attribute of the page table entry is not a hardware
 *	feature, so there is no need to invalidate any TLB entries.
 *	Since pmap_demote_pde() for the wired entry must never fail,
 *	pmap_delayed_invl_start()/finish() calls around the
 *	function are not needed.
 */
void
pmap_unwire(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t va_next;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V, PG_G __diagused;

	PG_V = pmap_valid_bit(pmap);
	PG_G = pmap_global_bit(pmap);
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pml4e = pmap_pml4e(pmap, sva);
		if (pml4e == NULL || (*pml4e & PG_V) == 0) {
			va_next = (sva + NBPML4) & ~PML4MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + NBPDP) & ~PDPMASK;
		if (va_next < sva)
			va_next = eva;
		pdpe = pmap_pml4e_to_pdpe(pml4e, sva);
		if ((*pdpe & PG_V) == 0)
			continue;
		if ((*pdpe & PG_PS) != 0) {
			KASSERT(va_next <= eva,
			    ("partial update of non-transparent 1G mapping "
			    "pdpe %#lx sva %#lx eva %#lx va_next %#lx",
			    *pdpe, sva, eva, va_next));
			MPASS(pmap != kernel_pmap); /* XXXKIB */
			MPASS((*pdpe & (PG_MANAGED | PG_G)) == 0);
			atomic_clear_long(pdpe, PG_W);
			pmap->pm_stats.wired_count -= NBPDP / PAGE_SIZE;
			continue;
		}

		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;
		pde = pmap_pdpe_to_pde(pdpe, sva);
		if ((*pde & PG_V) == 0)
			continue;
		if ((*pde & PG_PS) != 0) {
			if ((*pde & PG_W) == 0)
				panic("pmap_unwire: pde %#jx is missing PG_W",
				    (uintmax_t)*pde);

			/*
			 * Are we unwiring the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == va_next && eva >= va_next) {
				atomic_clear_long(pde, PG_W);
				pmap->pm_stats.wired_count -= NBPDR /
				    PAGE_SIZE;
				continue;
			} else if (!pmap_demote_pde(pmap, pde, sva))
				panic("pmap_unwire: demotion failed");
		}
		if (va_next > eva)
			va_next = eva;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			if ((*pte & PG_V) == 0)
				continue;
			if ((*pte & PG_W) == 0)
				panic("pmap_unwire: pte %#jx is missing PG_W",
				    (uintmax_t)*pte);

			/*
			 * PG_W must be cleared atomically.  Although the pmap
			 * lock synchronizes access to PG_W, another processor
			 * could be setting PG_M and/or PG_A concurrently.
			 */
			atomic_clear_long(pte, PG_W);
			pmap->pm_stats.wired_count--;
		}
	}
	PMAP_UNLOCK(pmap);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr, vm_size_t len,
    vm_offset_t src_addr)
{
	struct rwlock *lock;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde, srcptepaddr;
	pt_entry_t *dst_pte, PG_A, PG_M, PG_V, ptetemp, *src_pte;
	vm_offset_t addr, end_addr, va_next;
	vm_page_t dst_pdpg, dstmpte, srcmpte;

	if (dst_addr != src_addr)
		return;

	if (dst_pmap->pm_type != src_pmap->pm_type)
		return;

	/*
	 * EPT page table entries that require emulation of A/D bits are
	 * sensitive to clearing the PG_A bit (aka EPT_PG_READ). Although
	 * we clear PG_M (aka EPT_PG_WRITE) concomitantly, the PG_U bit
	 * (aka EPT_PG_EXECUTE) could still be set. Since some EPT
	 * implementations flag an EPT misconfiguration for exec-only
	 * mappings we skip this function entirely for emulated pmaps.
	 */
	if (pmap_emulate_ad_bits(dst_pmap))
		return;

	end_addr = src_addr + len;
	lock = NULL;
	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}

	PG_A = pmap_accessed_bit(dst_pmap);
	PG_M = pmap_modified_bit(dst_pmap);
	PG_V = pmap_valid_bit(dst_pmap);

	for (addr = src_addr; addr < end_addr; addr = va_next) {
		KASSERT(addr < UPT_MIN_ADDRESS,
		    ("pmap_copy: invalid to pmap_copy page tables"));

		pml4e = pmap_pml4e(src_pmap, addr);
		if (pml4e == NULL || (*pml4e & PG_V) == 0) {
			va_next = (addr + NBPML4) & ~PML4MASK;
			if (va_next < addr)
				va_next = end_addr;
			continue;
		}

		va_next = (addr + NBPDP) & ~PDPMASK;
		if (va_next < addr)
			va_next = end_addr;
		pdpe = pmap_pml4e_to_pdpe(pml4e, addr);
		if ((*pdpe & PG_V) == 0)
			continue;
		if ((*pdpe & PG_PS) != 0) {
			KASSERT(va_next <= end_addr,
			    ("partial update of non-transparent 1G mapping "
			    "pdpe %#lx sva %#lx eva %#lx va_next %#lx",
			    *pdpe, addr, end_addr, va_next));
			MPASS((addr & PDPMASK) == 0);
			MPASS((*pdpe & PG_MANAGED) == 0);
			srcptepaddr = *pdpe;
			pdpe = pmap_pdpe(dst_pmap, addr);
			if (pdpe == NULL) {
				if (pmap_allocpte_alloc(dst_pmap,
				    pmap_pml4e_pindex(addr), NULL, addr) ==
				    NULL)
					break;
				pdpe = pmap_pdpe(dst_pmap, addr);
			} else {
				pml4e = pmap_pml4e(dst_pmap, addr);
				dst_pdpg = PHYS_TO_VM_PAGE(*pml4e & PG_FRAME);
				dst_pdpg->ref_count++;
			}
			KASSERT(*pdpe == 0,
			    ("1G mapping present in dst pmap "
			    "pdpe %#lx sva %#lx eva %#lx va_next %#lx",
			    *pdpe, addr, end_addr, va_next));
			*pdpe = srcptepaddr & ~PG_W;
			pmap_resident_count_adj(dst_pmap, NBPDP / PAGE_SIZE);
			continue;
		}

		va_next = (addr + NBPDR) & ~PDRMASK;
		if (va_next < addr)
			va_next = end_addr;

		pde = pmap_pdpe_to_pde(pdpe, addr);
		srcptepaddr = *pde;
		if (srcptepaddr == 0)
			continue;
			
		if (srcptepaddr & PG_PS) {
			/*
			 * We can only virtual copy whole superpages.
			 */
			if ((addr & PDRMASK) != 0 || addr + NBPDR > end_addr)
				continue;
			pde = pmap_alloc_pde(dst_pmap, addr, &dst_pdpg, NULL);
			if (pde == NULL)
				break;
			if (*pde == 0 && ((srcptepaddr & PG_MANAGED) == 0 ||
			    pmap_pv_insert_pde(dst_pmap, addr, srcptepaddr,
			    PMAP_ENTER_NORECLAIM, &lock))) {
				/*
				 * We leave the dirty bit unchanged because
				 * managed read/write superpage mappings are
				 * required to be dirty.  However, managed
				 * superpage mappings are not required to
				 * have their accessed bit set, so we clear
				 * it because we don't know if this mapping
				 * will be used.
				 */
				srcptepaddr &= ~PG_W;
				if ((srcptepaddr & PG_MANAGED) != 0)
					srcptepaddr &= ~PG_A;
				*pde = srcptepaddr;
				pmap_resident_count_adj(dst_pmap, NBPDR /
				    PAGE_SIZE);
				counter_u64_add(pmap_pde_mappings, 1);
			} else
				pmap_abort_ptp(dst_pmap, addr, dst_pdpg);
			continue;
		}

		srcptepaddr &= PG_FRAME;
		srcmpte = PHYS_TO_VM_PAGE(srcptepaddr);
		KASSERT(srcmpte->ref_count > 0,
		    ("pmap_copy: source page table page is unused"));

		if (va_next > end_addr)
			va_next = end_addr;

		src_pte = (pt_entry_t *)PHYS_TO_DMAP(srcptepaddr);
		src_pte = &src_pte[pmap_pte_index(addr)];
		dstmpte = NULL;
		for (; addr < va_next; addr += PAGE_SIZE, src_pte++) {
			ptetemp = *src_pte;

			/*
			 * We only virtual copy managed pages.
			 */
			if ((ptetemp & PG_MANAGED) == 0)
				continue;

			if (dstmpte != NULL) {
				KASSERT(dstmpte->pindex ==
				    pmap_pde_pindex(addr),
				    ("dstmpte pindex/addr mismatch"));
				dstmpte->ref_count++;
			} else if ((dstmpte = pmap_allocpte(dst_pmap, addr,
			    NULL)) == NULL)
				goto out;
			dst_pte = (pt_entry_t *)
			    PHYS_TO_DMAP(VM_PAGE_TO_PHYS(dstmpte));
			dst_pte = &dst_pte[pmap_pte_index(addr)];
			if (*dst_pte == 0 &&
			    pmap_try_insert_pv_entry(dst_pmap, addr,
			    PHYS_TO_VM_PAGE(ptetemp & PG_FRAME), &lock)) {
				/*
				 * Clear the wired, modified, and accessed
				 * (referenced) bits during the copy.
				 */
				*dst_pte = ptetemp & ~(PG_W | PG_M | PG_A);
				pmap_resident_count_adj(dst_pmap, 1);
			} else {
				pmap_abort_ptp(dst_pmap, addr, dstmpte);
				goto out;
			}
			/* Have we copied all of the valid mappings? */ 
			if (dstmpte->ref_count >= srcmpte->ref_count)
				break;
		}
	}
out:
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(src_pmap);
	PMAP_UNLOCK(dst_pmap);
}

int
pmap_vmspace_copy(pmap_t dst_pmap, pmap_t src_pmap)
{
	int error;

	if (dst_pmap->pm_type != src_pmap->pm_type ||
	    dst_pmap->pm_type != PT_X86 ||
	    (cpu_stdext_feature2 & CPUID_STDEXT2_PKU) == 0)
		return (0);
	for (;;) {
		if (dst_pmap < src_pmap) {
			PMAP_LOCK(dst_pmap);
			PMAP_LOCK(src_pmap);
		} else {
			PMAP_LOCK(src_pmap);
			PMAP_LOCK(dst_pmap);
		}
		error = pmap_pkru_copy(dst_pmap, src_pmap);
		/* Clean up partial copy on failure due to no memory. */
		if (error == ENOMEM)
			pmap_pkru_deassign_all(dst_pmap);
		PMAP_UNLOCK(src_pmap);
		PMAP_UNLOCK(dst_pmap);
		if (error != ENOMEM)
			break;
		vm_wait(NULL);
	}
	return (error);
}

/*
 * Zero the specified hardware page.
 */
void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va;

#ifdef TSLOG_PAGEZERO
	TSENTER();
#endif
	va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	pagezero((void *)va);
#ifdef TSLOG_PAGEZERO
	TSEXIT();
#endif
}

/*
 * Zero an area within a single hardware page.  off and size must not
 * cover an area beyond a single hardware page.
 */
void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	vm_offset_t va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));

	if (off == 0 && size == PAGE_SIZE)
		pagezero((void *)va);
	else
		bzero((char *)va + off, size);
}

/*
 * Copy 1 specified hardware page to another.
 */
void
pmap_copy_page(vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t src = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(msrc));
	vm_offset_t dst = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mdst));

	pagecopy((void *)src, (void *)dst);
}

int unmapped_buf_allowed = 1;

void
pmap_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{
	void *a_cp, *b_cp;
	vm_page_t pages[2];
	vm_offset_t vaddr[2], a_pg_offset, b_pg_offset;
	int cnt;
	bool mapped;

	while (xfersize > 0) {
		a_pg_offset = a_offset & PAGE_MASK;
		pages[0] = ma[a_offset >> PAGE_SHIFT];
		b_pg_offset = b_offset & PAGE_MASK;
		pages[1] = mb[b_offset >> PAGE_SHIFT];
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		mapped = pmap_map_io_transient(pages, vaddr, 2, false);
		a_cp = (char *)vaddr[0] + a_pg_offset;
		b_cp = (char *)vaddr[1] + b_pg_offset;
		bcopy(a_cp, b_cp, cnt);
		if (__predict_false(mapped))
			pmap_unmap_io_transient(pages, vaddr, 2, false);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
}

/*
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
bool
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	struct md_page *pvh;
	struct rwlock *lock;
	pv_entry_t pv;
	int loops = 0;
	bool rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_page_exists_quick: page %p is not managed", m));
	rv = false;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		if (PV_PMAP(pv) == pmap) {
			rv = true;
			break;
		}
		loops++;
		if (loops >= 16)
			break;
	}
	if (!rv && loops < 16 && (m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			if (PV_PMAP(pv) == pmap) {
				rv = true;
				break;
			}
			loops++;
			if (loops >= 16)
				break;
		}
	}
	rw_runlock(lock);
	return (rv);
}

/*
 *	pmap_page_wired_mappings:
 *
 *	Return the number of managed mappings to the given physical page
 *	that are wired.
 */
int
pmap_page_wired_mappings(vm_page_t m)
{
	struct rwlock *lock;
	struct md_page *pvh;
	pmap_t pmap;
	pt_entry_t *pte;
	pv_entry_t pv;
	int count, md_gen, pvh_gen;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (0);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
restart:
	count = 0;
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			rw_runlock(lock);
			PMAP_LOCK(pmap);
			rw_rlock(lock);
			if (md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		pte = pmap_pte(pmap, pv->pv_va);
		if ((*pte & PG_W) != 0)
			count++;
		PMAP_UNLOCK(pmap);
	}
	if ((m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			pmap = PV_PMAP(pv);
			if (!PMAP_TRYLOCK(pmap)) {
				md_gen = m->md.pv_gen;
				pvh_gen = pvh->pv_gen;
				rw_runlock(lock);
				PMAP_LOCK(pmap);
				rw_rlock(lock);
				if (md_gen != m->md.pv_gen ||
				    pvh_gen != pvh->pv_gen) {
					PMAP_UNLOCK(pmap);
					goto restart;
				}
			}
			pte = pmap_pde(pmap, pv->pv_va);
			if ((*pte & PG_W) != 0)
				count++;
			PMAP_UNLOCK(pmap);
		}
	}
	rw_runlock(lock);
	return (count);
}

/*
 * Returns true if the given page is mapped individually or as part of
 * a 2mpage.  Otherwise, returns false.
 */
bool
pmap_page_is_mapped(vm_page_t m)
{
	struct rwlock *lock;
	bool rv;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (false);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
	rv = !TAILQ_EMPTY(&m->md.pv_list) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    !TAILQ_EMPTY(&pa_to_pvh(VM_PAGE_TO_PHYS(m))->pv_list));
	rw_runlock(lock);
	return (rv);
}

/*
 * Destroy all managed, non-wired mappings in the given user-space
 * pmap.  This pmap cannot be active on any processor besides the
 * caller.
 *
 * This function cannot be applied to the kernel pmap.  Moreover, it
 * is not intended for general use.  It is only to be used during
 * process termination.  Consequently, it can be implemented in ways
 * that make it faster than pmap_remove().  First, it can more quickly
 * destroy mappings by iterating over the pmap's collection of PV
 * entries, rather than searching the page table.  Second, it doesn't
 * have to test and clear the page table entries atomically, because
 * no processor is currently accessing the user address space.  In
 * particular, a page table entry's dirty bit won't change state once
 * this function starts.
 *
 * Although this function destroys all of the pmap's managed,
 * non-wired mappings, it can delay and batch the invalidation of TLB
 * entries without calling pmap_delayed_invl_start() and
 * pmap_delayed_invl_finish().  Because the pmap is not active on
 * any other processor, none of these TLB entries will ever be used
 * before their eventual invalidation.  Consequently, there is no need
 * for either pmap_remove_all() or pmap_remove_write() to wait for
 * that eventual TLB invalidation.
 */
void
pmap_remove_pages(pmap_t pmap)
{
	pd_entry_t ptepde;
	pt_entry_t *pte, tpte;
	pt_entry_t PG_M, PG_RW, PG_V;
	struct spglist free;
	struct pv_chunklist free_chunks[PMAP_MEMDOM];
	vm_page_t m, mpte, mt;
	pv_entry_t pv;
	struct md_page *pvh;
	struct pv_chunk *pc, *npc;
	struct rwlock *lock;
	int64_t bit;
	uint64_t inuse, bitmask;
	int allfree, field, i, idx;
#ifdef PV_STATS
	int freed;
#endif
	bool superpage;
	vm_paddr_t pa;

	/*
	 * Assert that the given pmap is only active on the current
	 * CPU.  Unfortunately, we cannot block another CPU from
	 * activating the pmap while this function is executing.
	 */
	KASSERT(pmap == PCPU_GET(curpmap), ("non-current pmap %p", pmap));
#ifdef INVARIANTS
	{
		cpuset_t other_cpus;

		other_cpus = all_cpus;
		critical_enter();
		CPU_CLR(PCPU_GET(cpuid), &other_cpus);
		CPU_AND(&other_cpus, &other_cpus, &pmap->pm_active);
		critical_exit();
		KASSERT(CPU_EMPTY(&other_cpus), ("pmap active %p", pmap));
	}
#endif

	lock = NULL;
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	for (i = 0; i < PMAP_MEMDOM; i++)
		TAILQ_INIT(&free_chunks[i]);
	SLIST_INIT(&free);
	PMAP_LOCK(pmap);
	TAILQ_FOREACH_SAFE(pc, &pmap->pm_pvchunk, pc_list, npc) {
		allfree = 1;
#ifdef PV_STATS
		freed = 0;
#endif
		for (field = 0; field < _NPCM; field++) {
			inuse = ~pc->pc_map[field] & pc_freemask[field];
			while (inuse != 0) {
				bit = bsfq(inuse);
				bitmask = 1UL << bit;
				idx = field * 64 + bit;
				pv = &pc->pc_pventry[idx];
				inuse &= ~bitmask;

				pte = pmap_pdpe(pmap, pv->pv_va);
				ptepde = *pte;
				pte = pmap_pdpe_to_pde(pte, pv->pv_va);
				tpte = *pte;
				if ((tpte & (PG_PS | PG_V)) == PG_V) {
					superpage = false;
					ptepde = tpte;
					pte = (pt_entry_t *)PHYS_TO_DMAP(tpte &
					    PG_FRAME);
					pte = &pte[pmap_pte_index(pv->pv_va)];
					tpte = *pte;
				} else {
					/*
					 * Keep track whether 'tpte' is a
					 * superpage explicitly instead of
					 * relying on PG_PS being set.
					 *
					 * This is because PG_PS is numerically
					 * identical to PG_PTE_PAT and thus a
					 * regular page could be mistaken for
					 * a superpage.
					 */
					superpage = true;
				}

				if ((tpte & PG_V) == 0) {
					panic("bad pte va %lx pte %lx",
					    pv->pv_va, tpte);
				}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
				if (tpte & PG_W) {
					allfree = 0;
					continue;
				}

				/* Mark free */
				pc->pc_map[field] |= bitmask;

				/*
				 * Because this pmap is not active on other
				 * processors, the dirty bit cannot have
				 * changed state since we last loaded pte.
				 */
				pte_clear(pte);

				if (superpage)
					pa = tpte & PG_PS_FRAME;
				else
					pa = tpte & PG_FRAME;

				m = PHYS_TO_VM_PAGE(pa);
				KASSERT(m->phys_addr == pa,
				    ("vm_page_t %p phys_addr mismatch %016jx %016jx",
				    m, (uintmax_t)m->phys_addr,
				    (uintmax_t)tpte));

				KASSERT((m->flags & PG_FICTITIOUS) != 0 ||
				    m < &vm_page_array[vm_page_array_size],
				    ("pmap_remove_pages: bad tpte %#jx",
				    (uintmax_t)tpte));

				/*
				 * Update the vm_page_t clean/reference bits.
				 */
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
					if (superpage) {
						for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
							vm_page_dirty(mt);
					} else
						vm_page_dirty(m);
				}

				CHANGE_PV_LIST_LOCK_TO_VM_PAGE(&lock, m);

				if (superpage) {
					pmap_resident_count_adj(pmap, -NBPDR / PAGE_SIZE);
					pvh = pa_to_pvh(tpte & PG_PS_FRAME);
					TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
					pvh->pv_gen++;
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
							if ((mt->a.flags & PGA_WRITEABLE) != 0 &&
							    TAILQ_EMPTY(&mt->md.pv_list))
								vm_page_aflag_clear(mt, PGA_WRITEABLE);
					}
					mpte = pmap_remove_pt_page(pmap, pv->pv_va);
					if (mpte != NULL) {
						KASSERT(vm_page_any_valid(mpte),
						    ("pmap_remove_pages: pte page not promoted"));
						pmap_pt_page_count_adj(pmap, -1);
						KASSERT(mpte->ref_count == NPTEPG,
						    ("pmap_remove_pages: pte page reference count error"));
						mpte->ref_count = 0;
						pmap_add_delayed_free_list(mpte, &free, false);
					}
				} else {
					pmap_resident_count_adj(pmap, -1);
					TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
					m->md.pv_gen++;
					if ((m->a.flags & PGA_WRITEABLE) != 0 &&
					    TAILQ_EMPTY(&m->md.pv_list) &&
					    (m->flags & PG_FICTITIOUS) == 0) {
						pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
						if (TAILQ_EMPTY(&pvh->pv_list))
							vm_page_aflag_clear(m, PGA_WRITEABLE);
					}
				}
				pmap_unuse_pt(pmap, pv->pv_va, ptepde, &free);
#ifdef PV_STATS
				freed++;
#endif
			}
		}
		PV_STAT(counter_u64_add(pv_entry_frees, freed));
		PV_STAT(counter_u64_add(pv_entry_spare, freed));
		PV_STAT(counter_u64_add(pv_entry_count, -freed));
		if (allfree) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			TAILQ_INSERT_TAIL(&free_chunks[pc_to_domain(pc)], pc, pc_list);
		}
	}
	if (lock != NULL)
		rw_wunlock(lock);
	pmap_invalidate_all(pmap);
	pmap_pkru_deassign_all(pmap);
	free_pv_chunk_batch((struct pv_chunklist *)&free_chunks);
	PMAP_UNLOCK(pmap);
	vm_page_free_pages_toq(&free, true);
}

static bool
pmap_page_test_mappings(vm_page_t m, bool accessed, bool modified)
{
	struct rwlock *lock;
	pv_entry_t pv;
	struct md_page *pvh;
	pt_entry_t *pte, mask;
	pt_entry_t PG_A, PG_M, PG_RW, PG_V;
	pmap_t pmap;
	int md_gen, pvh_gen;
	bool rv;

	rv = false;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
restart:
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			rw_runlock(lock);
			PMAP_LOCK(pmap);
			rw_rlock(lock);
			if (md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		pte = pmap_pte(pmap, pv->pv_va);
		mask = 0;
		if (modified) {
			PG_M = pmap_modified_bit(pmap);
			PG_RW = pmap_rw_bit(pmap);
			mask |= PG_RW | PG_M;
		}
		if (accessed) {
			PG_A = pmap_accessed_bit(pmap);
			PG_V = pmap_valid_bit(pmap);
			mask |= PG_V | PG_A;
		}
		rv = (*pte & mask) == mask;
		PMAP_UNLOCK(pmap);
		if (rv)
			goto out;
	}
	if ((m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			pmap = PV_PMAP(pv);
			if (!PMAP_TRYLOCK(pmap)) {
				md_gen = m->md.pv_gen;
				pvh_gen = pvh->pv_gen;
				rw_runlock(lock);
				PMAP_LOCK(pmap);
				rw_rlock(lock);
				if (md_gen != m->md.pv_gen ||
				    pvh_gen != pvh->pv_gen) {
					PMAP_UNLOCK(pmap);
					goto restart;
				}
			}
			pte = pmap_pde(pmap, pv->pv_va);
			mask = 0;
			if (modified) {
				PG_M = pmap_modified_bit(pmap);
				PG_RW = pmap_rw_bit(pmap);
				mask |= PG_RW | PG_M;
			}
			if (accessed) {
				PG_A = pmap_accessed_bit(pmap);
				PG_V = pmap_valid_bit(pmap);
				mask |= PG_V | PG_A;
			}
			rv = (*pte & mask) == mask;
			PMAP_UNLOCK(pmap);
			if (rv)
				goto out;
		}
	}
out:
	rw_runlock(lock);
	return (rv);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
bool
pmap_is_modified(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_modified: page %p is not managed", m));

	/*
	 * If the page is not busied then this check is racy.
	 */
	if (!pmap_page_is_write_mapped(m))
		return (false);
	return (pmap_page_test_mappings(m, false, true));
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is eligible
 *	for prefault.
 */
bool
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	bool rv;

	PG_V = pmap_valid_bit(pmap);

	/*
	 * Return true if and only if the PTE for the specified virtual
	 * address is allocated but invalid.
	 */
	rv = false;
	PMAP_LOCK(pmap);
	pde = pmap_pde(pmap, addr);
	if (pde != NULL && (*pde & (PG_PS | PG_V)) == PG_V) {
		pte = pmap_pde_to_pte(pde, addr);
		rv = (*pte & PG_V) == 0;
	}
	PMAP_UNLOCK(pmap);
	return (rv);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page was referenced
 *	in any physical maps.
 */
bool
pmap_is_referenced(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	return (pmap_page_test_mappings(m, true, false));
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	struct md_page *pvh;
	pmap_t pmap;
	struct rwlock *lock;
	pv_entry_t next_pv, pv;
	pd_entry_t *pde;
	pt_entry_t oldpte, *pte, PG_M, PG_RW;
	vm_offset_t va;
	int pvh_gen, md_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_write: page %p is not managed", m));

	vm_page_assert_busied(m);
	if (!pmap_page_is_write_mapped(m))
		return;

	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
	rw_wlock(lock);
retry:
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_next, next_pv) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		PG_RW = pmap_rw_bit(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		if ((*pde & PG_RW) != 0)
			(void)pmap_demote_pde_locked(pmap, pde, va, &lock);
		KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
		    ("inconsistent pv lock %p %p for page %p",
		    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
		PMAP_UNLOCK(pmap);
	}
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen ||
			    md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0,
		    ("pmap_remove_write: found a 2mpage in page %p's pv list",
		    m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		oldpte = *pte;
		if (oldpte & PG_RW) {
			while (!atomic_fcmpset_long(pte, &oldpte, oldpte &
			    ~(PG_RW | PG_M)))
				cpu_spinwait();
			if ((oldpte & PG_M) != 0)
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	pmap_delayed_invl_wait(m);
}

/*
 *	pmap_ts_referenced:
 *
 *	Return a count of reference bits for a page, clearing those bits.
 *	It is not necessary for every reference bit to be cleared, but it
 *	is necessary that 0 only be returned when there are truly no
 *	reference bits set.
 *
 *	As an optimization, update the page's dirty field if a modified bit is
 *	found while counting reference bits.  This opportunistic update can be
 *	performed at low cost and can eliminate the need for some future calls
 *	to pmap_is_modified().  However, since this function stops after
 *	finding PMAP_TS_REFERENCED_MAX reference bits, it may not detect some
 *	dirty pages.  Those dirty pages will only be detected by a future call
 *	to pmap_is_modified().
 *
 *	A DI block is not needed within this function, because
 *	invalidations are performed before the PV list lock is
 *	released.
 */
int
pmap_ts_referenced(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv, pvf;
	pmap_t pmap;
	struct rwlock *lock;
	pd_entry_t oldpde, *pde;
	pt_entry_t *pte, PG_A, PG_M, PG_RW;
	vm_offset_t va;
	vm_paddr_t pa;
	int cleared, md_gen, not_cleared, pvh_gen;
	struct spglist free;
	bool demoted;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	SLIST_INIT(&free);
	cleared = 0;
	pa = VM_PAGE_TO_PHYS(m);
	lock = PHYS_TO_PV_LIST_LOCK(pa);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy : pa_to_pvh(pa);
	rw_wlock(lock);
retry:
	not_cleared = 0;
	if ((pvf = TAILQ_FIRST(&pvh->pv_list)) == NULL)
		goto small_mappings;
	pv = pvf;
	do {
		if (pvf == NULL)
			pvf = pv;
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		PG_A = pmap_accessed_bit(pmap);
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, pv->pv_va);
		oldpde = *pde;
		if ((oldpde & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
			/*
			 * Although "oldpde" is mapping a 2MB page, because
			 * this function is called at a 4KB page granularity,
			 * we only update the 4KB page under test.
			 */
			vm_page_dirty(m);
		}
		if ((oldpde & PG_A) != 0) {
			/*
			 * Since this reference bit is shared by 512 4KB
			 * pages, it should not be cleared every time it is
			 * tested.  Apply a simple "hash" function on the
			 * physical page number, the virtual superpage number,
			 * and the pmap address to select one 4KB page out of
			 * the 512 on which testing the reference bit will
			 * result in clearing that reference bit.  This
			 * function is designed to avoid the selection of the
			 * same 4KB page for every 2MB page mapping.
			 *
			 * On demotion, a mapping that hasn't been referenced
			 * is simply destroyed.  To avoid the possibility of a
			 * subsequent page fault on a demoted wired mapping,
			 * always leave its reference bit set.  Moreover,
			 * since the superpage is wired, the current state of
			 * its reference bit won't affect page replacement.
			 */
			if ((((pa >> PAGE_SHIFT) ^ (pv->pv_va >> PDRSHIFT) ^
			    (uintptr_t)pmap) & (NPTEPG - 1)) == 0 &&
			    (oldpde & PG_W) == 0) {
				if (safe_to_clear_referenced(pmap, oldpde)) {
					atomic_clear_long(pde, PG_A);
					pmap_invalidate_page(pmap, pv->pv_va);
					demoted = false;
				} else if (pmap_demote_pde_locked(pmap, pde,
				    pv->pv_va, &lock)) {
					/*
					 * Remove the mapping to a single page
					 * so that a subsequent access may
					 * repromote.  Since the underlying
					 * page table page is fully populated,
					 * this removal never frees a page
					 * table page.
					 */
					demoted = true;
					va += VM_PAGE_TO_PHYS(m) - (oldpde &
					    PG_PS_FRAME);
					pte = pmap_pde_to_pte(pde, va);
					pmap_remove_pte(pmap, pte, va, *pde,
					    NULL, &lock);
					pmap_invalidate_page(pmap, va);
				} else
					demoted = true;

				if (demoted) {
					/*
					 * The superpage mapping was removed
					 * entirely and therefore 'pv' is no
					 * longer valid.
					 */
					if (pvf == pv)
						pvf = NULL;
					pv = NULL;
				}
				cleared++;
				KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
				    ("inconsistent pv lock %p %p for page %p",
				    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
			} else
				not_cleared++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (pv != NULL && TAILQ_NEXT(pv, pv_next) != NULL) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
			pvh->pv_gen++;
		}
		if (cleared + not_cleared >= PMAP_TS_REFERENCED_MAX)
			goto out;
	} while ((pv = TAILQ_FIRST(&pvh->pv_list)) != pvf);
small_mappings:
	if ((pvf = TAILQ_FIRST(&m->md.pv_list)) == NULL)
		goto out;
	pv = pvf;
	do {
		if (pvf == NULL)
			pvf = pv;
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		PG_A = pmap_accessed_bit(pmap);
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0,
		    ("pmap_ts_referenced: found a 2mpage in page %p's pv list",
		    m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		if ((*pte & PG_A) != 0) {
			if (safe_to_clear_referenced(pmap, *pte)) {
				atomic_clear_long(pte, PG_A);
				pmap_invalidate_page(pmap, pv->pv_va);
				cleared++;
			} else if ((*pte & PG_W) == 0) {
				/*
				 * Wired pages cannot be paged out so
				 * doing accessed bit emulation for
				 * them is wasted effort. We do the
				 * hard work for unwired pages only.
				 */
				pmap_remove_pte(pmap, pte, pv->pv_va,
				    *pde, &free, &lock);
				pmap_invalidate_page(pmap, pv->pv_va);
				cleared++;
				if (pvf == pv)
					pvf = NULL;
				pv = NULL;
				KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
				    ("inconsistent pv lock %p %p for page %p",
				    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
			} else
				not_cleared++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (pv != NULL && TAILQ_NEXT(pv, pv_next) != NULL) {
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
			m->md.pv_gen++;
		}
	} while ((pv = TAILQ_FIRST(&m->md.pv_list)) != pvf && cleared +
	    not_cleared < PMAP_TS_REFERENCED_MAX);
out:
	rw_wunlock(lock);
	vm_page_free_pages_toq(&free, true);
	return (cleared + not_cleared);
}

/*
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping and set the mapped page's dirty field.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{
	struct rwlock *lock;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t oldpde, *pde;
	pt_entry_t *pte, PG_A, PG_G, PG_M, PG_RW, PG_V;
	vm_offset_t va, va_next;
	vm_page_t m;
	bool anychanged;

	if (advice != MADV_DONTNEED && advice != MADV_FREE)
		return;

	/*
	 * A/D bit emulation requires an alternate code path when clearing
	 * the modified and accessed bits below. Since this function is
	 * advisory in nature we skip it entirely for pmaps that require
	 * A/D bit emulation.
	 */
	if (pmap_emulate_ad_bits(pmap))
		return;

	PG_A = pmap_accessed_bit(pmap);
	PG_G = pmap_global_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);
	anychanged = false;
	pmap_delayed_invl_start();
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		pml4e = pmap_pml4e(pmap, sva);
		if (pml4e == NULL || (*pml4e & PG_V) == 0) {
			va_next = (sva + NBPML4) & ~PML4MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + NBPDP) & ~PDPMASK;
		if (va_next < sva)
			va_next = eva;
		pdpe = pmap_pml4e_to_pdpe(pml4e, sva);
		if ((*pdpe & PG_V) == 0)
			continue;
		if ((*pdpe & PG_PS) != 0)
			continue;

		va_next = (sva + NBPDR) & ~PDRMASK;
		if (va_next < sva)
			va_next = eva;
		pde = pmap_pdpe_to_pde(pdpe, sva);
		oldpde = *pde;
		if ((oldpde & PG_V) == 0)
			continue;
		else if ((oldpde & PG_PS) != 0) {
			if ((oldpde & PG_MANAGED) == 0)
				continue;
			lock = NULL;
			if (!pmap_demote_pde_locked(pmap, pde, sva, &lock)) {
				if (lock != NULL)
					rw_wunlock(lock);

				/*
				 * The large page mapping was destroyed.
				 */
				continue;
			}

			/*
			 * Unless the page mappings are wired, remove the
			 * mapping to a single page so that a subsequent
			 * access may repromote.  Choosing the last page
			 * within the address range [sva, min(va_next, eva))
			 * generally results in more repromotions.  Since the
			 * underlying page table page is fully populated, this
			 * removal never frees a page table page.
			 */
			if ((oldpde & PG_W) == 0) {
				va = eva;
				if (va > va_next)
					va = va_next;
				va -= PAGE_SIZE;
				KASSERT(va >= sva,
				    ("pmap_advise: no address gap"));
				pte = pmap_pde_to_pte(pde, va);
				KASSERT((*pte & PG_V) != 0,
				    ("pmap_advise: invalid PTE"));
				pmap_remove_pte(pmap, pte, va, *pde, NULL,
				    &lock);
				anychanged = true;
			}
			if (lock != NULL)
				rw_wunlock(lock);
		}
		if (va_next > eva)
			va_next = eva;
		va = va_next;
		for (pte = pmap_pde_to_pte(pde, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			if ((*pte & (PG_MANAGED | PG_V)) != (PG_MANAGED | PG_V))
				goto maybe_invlrng;
			else if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
				if (advice == MADV_DONTNEED) {
					/*
					 * Future calls to pmap_is_modified()
					 * can be avoided by making the page
					 * dirty now.
					 */
					m = PHYS_TO_VM_PAGE(*pte & PG_FRAME);
					vm_page_dirty(m);
				}
				atomic_clear_long(pte, PG_M | PG_A);
			} else if ((*pte & PG_A) != 0)
				atomic_clear_long(pte, PG_A);
			else
				goto maybe_invlrng;

			if ((*pte & PG_G) != 0) {
				if (va == va_next)
					va = sva;
			} else
				anychanged = true;
			continue;
maybe_invlrng:
			if (va != va_next) {
				pmap_invalidate_range(pmap, va, sva);
				va = va_next;
			}
		}
		if (va != va_next)
			pmap_invalidate_range(pmap, va, sva);
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
	pmap_delayed_invl_finish();
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	struct md_page *pvh;
	pmap_t pmap;
	pv_entry_t next_pv, pv;
	pd_entry_t oldpde, *pde;
	pt_entry_t *pte, PG_M, PG_RW;
	struct rwlock *lock;
	vm_offset_t va;
	int md_gen, pvh_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	vm_page_assert_busied(m);

	if (!pmap_page_is_write_mapped(m))
		return;
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_wlock(lock);
restart:
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_next, next_pv) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		va = pv->pv_va;
		pde = pmap_pde(pmap, va);
		oldpde = *pde;
		/* If oldpde has PG_RW set, then it also has PG_M set. */
		if ((oldpde & PG_RW) != 0 &&
		    pmap_demote_pde_locked(pmap, pde, va, &lock) &&
		    (oldpde & PG_W) == 0) {
			/*
			 * Write protect the mapping to a single page so that
			 * a subsequent write access may repromote.
			 */
			va += VM_PAGE_TO_PHYS(m) - (oldpde & PG_PS_FRAME);
			pte = pmap_pde_to_pte(pde, va);
			atomic_clear_long(pte, PG_M | PG_RW);
			vm_page_dirty(m);
			pmap_invalidate_page(pmap, va);
		}
		PMAP_UNLOCK(pmap);
	}
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		PG_M = pmap_modified_bit(pmap);
		PG_RW = pmap_rw_bit(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0, ("pmap_clear_modify: found"
		    " a 2mpage in page %p's pv list", m));
		pte = pmap_pde_to_pte(pde, pv->pv_va);
		if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
			atomic_clear_long(pte, PG_M);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
}

/*
 * Miscellaneous support routines follow
 */

/* Adjust the properties for a leaf page table entry. */
static __inline void
pmap_pte_props(pt_entry_t *pte, u_long bits, u_long mask)
{
	u_long opte, npte;

	opte = *(u_long *)pte;
	do {
		npte = opte & ~mask;
		npte |= bits;
	} while (npte != opte && !atomic_fcmpset_long((u_long *)pte, &opte,
	    npte));
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
static void *
pmap_mapdev_internal(vm_paddr_t pa, vm_size_t size, int mode, int flags)
{
	struct pmap_preinit_mapping *ppim;
	vm_offset_t va, offset;
	vm_size_t tmpsize;
	int i;

	offset = pa & PAGE_MASK;
	size = round_page(offset + size);
	pa = trunc_page(pa);

	if (!pmap_initialized) {
		va = 0;
		for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
			ppim = pmap_preinit_mapping + i;
			if (ppim->va == 0) {
				ppim->pa = pa;
				ppim->sz = size;
				ppim->mode = mode;
				ppim->va = virtual_avail;
				virtual_avail += size;
				va = ppim->va;
				break;
			}
		}
		if (va == 0)
			panic("%s: too many preinit mappings", __func__);
	} else {
		/*
		 * If we have a preinit mapping, re-use it.
		 */
		for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
			ppim = pmap_preinit_mapping + i;
			if (ppim->pa == pa && ppim->sz == size &&
			    (ppim->mode == mode ||
			    (flags & MAPDEV_SETATTR) == 0))
				return ((void *)(ppim->va + offset));
		}
		/*
		 * If the specified range of physical addresses fits within
		 * the direct map window, use the direct map.
		 */
		if (pa < dmaplimit && pa + size <= dmaplimit) {
			va = PHYS_TO_DMAP(pa);
			if ((flags & MAPDEV_SETATTR) != 0) {
				PMAP_LOCK(kernel_pmap);
				i = pmap_change_props_locked(va, size,
				    PROT_NONE, mode, flags);
				PMAP_UNLOCK(kernel_pmap);
			} else
				i = 0;
			if (!i)
				return ((void *)(va + offset));
		}
		va = kva_alloc(size);
		if (va == 0)
			panic("%s: Couldn't allocate KVA", __func__);
	}
	for (tmpsize = 0; tmpsize < size; tmpsize += PAGE_SIZE)
		pmap_kenter_attr(va + tmpsize, pa + tmpsize, mode);
	pmap_invalidate_range(kernel_pmap, va, va + tmpsize);
	if ((flags & MAPDEV_FLUSHCACHE) != 0)
		pmap_invalidate_cache_range(va, va + tmpsize);
	return ((void *)(va + offset));
}

void *
pmap_mapdev_attr(vm_paddr_t pa, vm_size_t size, int mode)
{

	return (pmap_mapdev_internal(pa, size, mode, MAPDEV_FLUSHCACHE |
	    MAPDEV_SETATTR));
}

void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{

	return (pmap_mapdev_attr(pa, size, PAT_UNCACHEABLE));
}

void *
pmap_mapdev_pciecfg(vm_paddr_t pa, vm_size_t size)
{

	return (pmap_mapdev_internal(pa, size, PAT_UNCACHEABLE,
	    MAPDEV_SETATTR));
}

void *
pmap_mapbios(vm_paddr_t pa, vm_size_t size)
{

	return (pmap_mapdev_internal(pa, size, PAT_WRITE_BACK,
	    MAPDEV_FLUSHCACHE));
}

void
pmap_unmapdev(void *p, vm_size_t size)
{
	struct pmap_preinit_mapping *ppim;
	vm_offset_t offset, va;
	int i;

	va = (vm_offset_t)p;

	/* If we gave a direct map region in pmap_mapdev, do nothing */
	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS)
		return;
	offset = va & PAGE_MASK;
	size = round_page(offset + size);
	va = trunc_page(va);
	for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
		ppim = pmap_preinit_mapping + i;
		if (ppim->va == va && ppim->sz == size) {
			if (pmap_initialized)
				return;
			ppim->pa = 0;
			ppim->va = 0;
			ppim->sz = 0;
			ppim->mode = 0;
			if (va + size == virtual_avail)
				virtual_avail = va;
			return;
		}
	}
	if (pmap_initialized) {
		pmap_qremove(va, atop(size));
		kva_free(va, size);
	}
}

/*
 * Tries to demote a 1GB page mapping.
 */
static bool
pmap_demote_pdpe(pmap_t pmap, pdp_entry_t *pdpe, vm_offset_t va)
{
	pdp_entry_t newpdpe, oldpdpe;
	pd_entry_t *firstpde, newpde, *pde;
	pt_entry_t PG_A, PG_M, PG_RW, PG_V;
	vm_paddr_t pdpgpa;
	vm_page_t pdpg;

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpdpe = *pdpe;
	KASSERT((oldpdpe & (PG_PS | PG_V)) == (PG_PS | PG_V),
	    ("pmap_demote_pdpe: oldpdpe is missing PG_PS and/or PG_V"));
	pdpg = pmap_alloc_pt_page(pmap, va >> PDPSHIFT,
	    VM_ALLOC_WIRED | VM_ALLOC_INTERRUPT);
	if (pdpg  == NULL) {
		CTR2(KTR_PMAP, "pmap_demote_pdpe: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (false);
	}
	pdpgpa = VM_PAGE_TO_PHYS(pdpg);
	firstpde = (pd_entry_t *)PHYS_TO_DMAP(pdpgpa);
	newpdpe = pdpgpa | PG_M | PG_A | (oldpdpe & PG_U) | PG_RW | PG_V;
	KASSERT((oldpdpe & PG_A) != 0,
	    ("pmap_demote_pdpe: oldpdpe is missing PG_A"));
	KASSERT((oldpdpe & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_demote_pdpe: oldpdpe is missing PG_M"));
	newpde = oldpdpe;

	/*
	 * Initialize the page directory page.
	 */
	for (pde = firstpde; pde < firstpde + NPDEPG; pde++) {
		*pde = newpde;
		newpde += NBPDR;
	}

	/*
	 * Demote the mapping.
	 */
	*pdpe = newpdpe;

	/*
	 * Invalidate a stale recursive mapping of the page directory page.
	 */
	pmap_invalidate_page(pmap, (vm_offset_t)vtopde(va));

	counter_u64_add(pmap_pdpe_demotions, 1);
	CTR2(KTR_PMAP, "pmap_demote_pdpe: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (true);
}

/*
 * Sets the memory attribute for the specified page.
 */
void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{

	m->md.pat_mode = ma;

	/*
	 * If "m" is a normal page, update its direct mapping.  This update
	 * can be relied upon to perform any cache operations that are
	 * required for data coherence.
	 */
	if ((m->flags & PG_FICTITIOUS) == 0 &&
	    pmap_change_attr(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)), PAGE_SIZE,
	    m->md.pat_mode))
		panic("memory attribute change on the direct map failed");
}

void
pmap_page_set_memattr_noflush(vm_page_t m, vm_memattr_t ma)
{
	int error;

	m->md.pat_mode = ma;

	if ((m->flags & PG_FICTITIOUS) != 0)
		return;
	PMAP_LOCK(kernel_pmap);
	error = pmap_change_props_locked(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)),
	    PAGE_SIZE, PROT_NONE, m->md.pat_mode, 0);
	PMAP_UNLOCK(kernel_pmap);
	if (error != 0)
		panic("memory attribute change on the direct map failed");
}

/*
 * Changes the specified virtual address range's memory type to that given by
 * the parameter "mode".  The specified virtual address range must be
 * completely contained within either the direct map or the kernel map.  If
 * the virtual address range is contained within the kernel map, then the
 * memory type for each of the corresponding ranges of the direct map is also
 * changed.  (The corresponding ranges of the direct map are those ranges that
 * map the same physical pages as the specified virtual address range.)  These
 * changes to the direct map are necessary because Intel describes the
 * behavior of their processors as "undefined" if two or more mappings to the
 * same physical page have different memory types.
 *
 * Returns zero if the change completed successfully, and either EINVAL or
 * ENOMEM if the change failed.  Specifically, EINVAL is returned if some part
 * of the virtual address range was not mapped, and ENOMEM is returned if
 * there was insufficient memory available to complete the change.  In the
 * latter case, the memory type may have been changed on some part of the
 * virtual address range or the direct map.
 */
int
pmap_change_attr(vm_offset_t va, vm_size_t size, int mode)
{
	int error;

	PMAP_LOCK(kernel_pmap);
	error = pmap_change_props_locked(va, size, PROT_NONE, mode,
	    MAPDEV_FLUSHCACHE);
	PMAP_UNLOCK(kernel_pmap);
	return (error);
}

/*
 * Changes the specified virtual address range's protections to those
 * specified by "prot".  Like pmap_change_attr(), protections for aliases
 * in the direct map are updated as well.  Protections on aliasing mappings may
 * be a subset of the requested protections; for example, mappings in the direct
 * map are never executable.
 */
int
pmap_change_prot(vm_offset_t va, vm_size_t size, vm_prot_t prot)
{
	int error;

	/* Only supported within the kernel map. */
	if (va < VM_MIN_KERNEL_ADDRESS)
		return (EINVAL);

	PMAP_LOCK(kernel_pmap);
	error = pmap_change_props_locked(va, size, prot, -1,
	    MAPDEV_ASSERTVALID);
	PMAP_UNLOCK(kernel_pmap);
	return (error);
}

static int
pmap_change_props_locked(vm_offset_t va, vm_size_t size, vm_prot_t prot,
    int mode, int flags)
{
	vm_offset_t base, offset, tmpva;
	vm_paddr_t pa_start, pa_end, pa_end1;
	pdp_entry_t *pdpe;
	pd_entry_t *pde, pde_bits, pde_mask;
	pt_entry_t *pte, pte_bits, pte_mask;
	int error;
	bool changed;

	PMAP_LOCK_ASSERT(kernel_pmap, MA_OWNED);
	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);

	/*
	 * Only supported on kernel virtual addresses, including the direct
	 * map but excluding the recursive map.
	 */
	if (base < DMAP_MIN_ADDRESS)
		return (EINVAL);

	/*
	 * Construct our flag sets and masks.  "bits" is the subset of
	 * "mask" that will be set in each modified PTE.
	 *
	 * Mappings in the direct map are never allowed to be executable.
	 */
	pde_bits = pte_bits = 0;
	pde_mask = pte_mask = 0;
	if (mode != -1) {
		pde_bits |= pmap_cache_bits(kernel_pmap, mode, true);
		pde_mask |= X86_PG_PDE_CACHE;
		pte_bits |= pmap_cache_bits(kernel_pmap, mode, false);
		pte_mask |= X86_PG_PTE_CACHE;
	}
	if (prot != VM_PROT_NONE) {
		if ((prot & VM_PROT_WRITE) != 0) {
			pde_bits |= X86_PG_RW;
			pte_bits |= X86_PG_RW;
		}
		if ((prot & VM_PROT_EXECUTE) == 0 ||
		    va < VM_MIN_KERNEL_ADDRESS) {
			pde_bits |= pg_nx;
			pte_bits |= pg_nx;
		}
		pde_mask |= X86_PG_RW | pg_nx;
		pte_mask |= X86_PG_RW | pg_nx;
	}

	/*
	 * Pages that aren't mapped aren't supported.  Also break down 2MB pages
	 * into 4KB pages if required.
	 */
	for (tmpva = base; tmpva < base + size; ) {
		pdpe = pmap_pdpe(kernel_pmap, tmpva);
		if (pdpe == NULL || *pdpe == 0) {
			KASSERT((flags & MAPDEV_ASSERTVALID) == 0,
			    ("%s: addr %#lx is not mapped", __func__, tmpva));
			return (EINVAL);
		}
		if (*pdpe & PG_PS) {
			/*
			 * If the current 1GB page already has the required
			 * properties, then we need not demote this page.  Just
			 * increment tmpva to the next 1GB page frame.
			 */
			if ((*pdpe & pde_mask) == pde_bits) {
				tmpva = trunc_1gpage(tmpva) + NBPDP;
				continue;
			}

			/*
			 * If the current offset aligns with a 1GB page frame
			 * and there is at least 1GB left within the range, then
			 * we need not break down this page into 2MB pages.
			 */
			if ((tmpva & PDPMASK) == 0 &&
			    tmpva + PDPMASK < base + size) {
				tmpva += NBPDP;
				continue;
			}
			if (!pmap_demote_pdpe(kernel_pmap, pdpe, tmpva))
				return (ENOMEM);
		}
		pde = pmap_pdpe_to_pde(pdpe, tmpva);
		if (*pde == 0) {
			KASSERT((flags & MAPDEV_ASSERTVALID) == 0,
			    ("%s: addr %#lx is not mapped", __func__, tmpva));
			return (EINVAL);
		}
		if (*pde & PG_PS) {
			/*
			 * If the current 2MB page already has the required
			 * properties, then we need not demote this page.  Just
			 * increment tmpva to the next 2MB page frame.
			 */
			if ((*pde & pde_mask) == pde_bits) {
				tmpva = trunc_2mpage(tmpva) + NBPDR;
				continue;
			}

			/*
			 * If the current offset aligns with a 2MB page frame
			 * and there is at least 2MB left within the range, then
			 * we need not break down this page into 4KB pages.
			 */
			if ((tmpva & PDRMASK) == 0 &&
			    tmpva + PDRMASK < base + size) {
				tmpva += NBPDR;
				continue;
			}
			if (!pmap_demote_pde(kernel_pmap, pde, tmpva))
				return (ENOMEM);
		}
		pte = pmap_pde_to_pte(pde, tmpva);
		if (*pte == 0) {
			KASSERT((flags & MAPDEV_ASSERTVALID) == 0,
			    ("%s: addr %#lx is not mapped", __func__, tmpva));
			return (EINVAL);
		}
		tmpva += PAGE_SIZE;
	}
	error = 0;

	/*
	 * Ok, all the pages exist, so run through them updating their
	 * properties if required.
	 */
	changed = false;
	pa_start = pa_end = 0;
	for (tmpva = base; tmpva < base + size; ) {
		pdpe = pmap_pdpe(kernel_pmap, tmpva);
		if (*pdpe & PG_PS) {
			if ((*pdpe & pde_mask) != pde_bits) {
				pmap_pte_props(pdpe, pde_bits, pde_mask);
				changed = true;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS &&
			    (*pdpe & PG_PS_FRAME) < dmaplimit) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = *pdpe & PG_PS_FRAME;
					pa_end = pa_start + NBPDP;
				} else if (pa_end == (*pdpe & PG_PS_FRAME))
					pa_end += NBPDP;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_props_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, prot, mode,
					    flags);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = *pdpe & PG_PS_FRAME;
					pa_end = pa_start + NBPDP;
				}
			}
			tmpva = trunc_1gpage(tmpva) + NBPDP;
			continue;
		}
		pde = pmap_pdpe_to_pde(pdpe, tmpva);
		if (*pde & PG_PS) {
			if ((*pde & pde_mask) != pde_bits) {
				pmap_pte_props(pde, pde_bits, pde_mask);
				changed = true;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS &&
			    (*pde & PG_PS_FRAME) < dmaplimit) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = *pde & PG_PS_FRAME;
					pa_end = pa_start + NBPDR;
				} else if (pa_end == (*pde & PG_PS_FRAME))
					pa_end += NBPDR;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_props_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, prot, mode,
					    flags);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = *pde & PG_PS_FRAME;
					pa_end = pa_start + NBPDR;
				}
			}
			tmpva = trunc_2mpage(tmpva) + NBPDR;
		} else {
			pte = pmap_pde_to_pte(pde, tmpva);
			if ((*pte & pte_mask) != pte_bits) {
				pmap_pte_props(pte, pte_bits, pte_mask);
				changed = true;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS &&
			    (*pte & PG_FRAME) < dmaplimit) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = *pte & PG_FRAME;
					pa_end = pa_start + PAGE_SIZE;
				} else if (pa_end == (*pte & PG_FRAME))
					pa_end += PAGE_SIZE;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_props_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, prot, mode,
					    flags);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = *pte & PG_FRAME;
					pa_end = pa_start + PAGE_SIZE;
				}
			}
			tmpva += PAGE_SIZE;
		}
	}
	if (error == 0 && pa_start != pa_end && pa_start < dmaplimit) {
		pa_end1 = MIN(pa_end, dmaplimit);
		if (pa_start != pa_end1)
			error = pmap_change_props_locked(PHYS_TO_DMAP(pa_start),
			    pa_end1 - pa_start, prot, mode, flags);
	}

	/*
	 * Flush CPU caches if required to make sure any data isn't cached that
	 * shouldn't be, etc.
	 */
	if (changed) {
		pmap_invalidate_range(kernel_pmap, base, tmpva);
		if ((flags & MAPDEV_FLUSHCACHE) != 0)
			pmap_invalidate_cache_range(base, tmpva);
	}
	return (error);
}

/*
 * Demotes any mapping within the direct map region that covers more than the
 * specified range of physical addresses.  This range's size must be a power
 * of two and its starting address must be a multiple of its size.  Since the
 * demotion does not change any attributes of the mapping, a TLB invalidation
 * is not mandatory.  The caller may, however, request a TLB invalidation.
 */
void
pmap_demote_DMAP(vm_paddr_t base, vm_size_t len, bool invalidate)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	vm_offset_t va;
	bool changed;

	if (len == 0)
		return;
	KASSERT(powerof2(len), ("pmap_demote_DMAP: len is not a power of 2"));
	KASSERT((base & (len - 1)) == 0,
	    ("pmap_demote_DMAP: base is not a multiple of len"));
	if (len < NBPDP && base < dmaplimit) {
		va = PHYS_TO_DMAP(base);
		changed = false;
		PMAP_LOCK(kernel_pmap);
		pdpe = pmap_pdpe(kernel_pmap, va);
		if ((*pdpe & X86_PG_V) == 0)
			panic("pmap_demote_DMAP: invalid PDPE");
		if ((*pdpe & PG_PS) != 0) {
			if (!pmap_demote_pdpe(kernel_pmap, pdpe, va))
				panic("pmap_demote_DMAP: PDPE failed");
			changed = true;
		}
		if (len < NBPDR) {
			pde = pmap_pdpe_to_pde(pdpe, va);
			if ((*pde & X86_PG_V) == 0)
				panic("pmap_demote_DMAP: invalid PDE");
			if ((*pde & PG_PS) != 0) {
				if (!pmap_demote_pde(kernel_pmap, pde, va))
					panic("pmap_demote_DMAP: PDE failed");
				changed = true;
			}
		}
		if (changed && invalidate)
			pmap_invalidate_page(kernel_pmap, va);
		PMAP_UNLOCK(kernel_pmap);
	}
}

/*
 * Perform the pmap work for mincore(2).  If the page is not both referenced and
 * modified by this pmap, returns its physical address so that the caller can
 * find other mappings.
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *pap)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pdep;
	pt_entry_t pte, PG_A, PG_M, PG_RW, PG_V;
	vm_paddr_t pa;
	int val;

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	PMAP_LOCK(pmap);
	pte = 0;
	pa = 0;
	val = 0;
	pdpe = pmap_pdpe(pmap, addr);
	if (pdpe == NULL)
		goto out;
	if ((*pdpe & PG_V) != 0) {
		if ((*pdpe & PG_PS) != 0) {
			pte = *pdpe;
			pa = ((pte & PG_PS_PDP_FRAME) | (addr & PDPMASK)) &
			    PG_FRAME;
			val = MINCORE_PSIND(2);
		} else {
			pdep = pmap_pde(pmap, addr);
			if (pdep != NULL && (*pdep & PG_V) != 0) {
				if ((*pdep & PG_PS) != 0) {
					pte = *pdep;
			/* Compute the physical address of the 4KB page. */
					pa = ((pte & PG_PS_FRAME) | (addr &
					    PDRMASK)) & PG_FRAME;
					val = MINCORE_PSIND(1);
				} else {
					pte = *pmap_pde_to_pte(pdep, addr);
					pa = pte & PG_FRAME;
					val = 0;
				}
			}
		}
	}
	if ((pte & PG_V) != 0) {
		val |= MINCORE_INCORE;
		if ((pte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		if ((pte & PG_A) != 0)
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
	}
	if ((val & (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER)) !=
	    (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER) &&
	    (pte & (PG_MANAGED | PG_V)) == (PG_MANAGED | PG_V)) {
		*pap = pa;
	}
out:
	PMAP_UNLOCK(pmap);
	return (val);
}

static uint64_t
pmap_pcid_alloc(pmap_t pmap, struct pmap_pcid *pcidp)
{
	uint32_t gen, new_gen, pcid_next;

	CRITICAL_ASSERT(curthread);
	gen = PCPU_GET(pcid_gen);
	if (pcidp->pm_pcid == PMAP_PCID_KERN)
		return (pti ? 0 : CR3_PCID_SAVE);
	if (pcidp->pm_gen == gen)
		return (CR3_PCID_SAVE);
	pcid_next = PCPU_GET(pcid_next);
	KASSERT((!pti && pcid_next <= PMAP_PCID_OVERMAX) ||
	    (pti && pcid_next <= PMAP_PCID_OVERMAX_KERN),
	    ("cpu %d pcid_next %#x", PCPU_GET(cpuid), pcid_next));
	if ((!pti && pcid_next == PMAP_PCID_OVERMAX) ||
	    (pti && pcid_next == PMAP_PCID_OVERMAX_KERN)) {
		new_gen = gen + 1;
		if (new_gen == 0)
			new_gen = 1;
		PCPU_SET(pcid_gen, new_gen);
		pcid_next = PMAP_PCID_KERN + 1;
	} else {
		new_gen = gen;
	}
	pcidp->pm_pcid = pcid_next;
	pcidp->pm_gen = new_gen;
	PCPU_SET(pcid_next, pcid_next + 1);
	return (0);
}

static uint64_t
pmap_pcid_alloc_checked(pmap_t pmap, struct pmap_pcid *pcidp)
{
	uint64_t cached;

	cached = pmap_pcid_alloc(pmap, pcidp);
	KASSERT(pcidp->pm_pcid < PMAP_PCID_OVERMAX,
	    ("pmap %p cpu %d pcid %#x", pmap, PCPU_GET(cpuid), pcidp->pm_pcid));
	KASSERT(pcidp->pm_pcid != PMAP_PCID_KERN || pmap == kernel_pmap,
	    ("non-kernel pmap pmap %p cpu %d pcid %#x",
	    pmap, PCPU_GET(cpuid), pcidp->pm_pcid));
	return (cached);
}

static void
pmap_activate_sw_pti_post(struct thread *td, pmap_t pmap)
{

	PCPU_GET(tssp)->tss_rsp0 = pmap->pm_ucr3 != PMAP_NO_CR3 ?
	    PCPU_GET(pti_rsp0) : (uintptr_t)td->td_md.md_stack_base;
}

static void
pmap_activate_sw_pcid_pti(struct thread *td, pmap_t pmap, u_int cpuid)
{
	pmap_t old_pmap;
	struct pmap_pcid *pcidp, *old_pcidp;
	uint64_t cached, cr3, kcr3, ucr3;

	KASSERT((read_rflags() & PSL_I) == 0,
	    ("PCID needs interrupts disabled in pmap_activate_sw()"));

	/* See the comment in pmap_invalidate_page_pcid(). */
	if (PCPU_GET(ucr3_load_mask) != PMAP_UCR3_NOMASK) {
		PCPU_SET(ucr3_load_mask, PMAP_UCR3_NOMASK);
		old_pmap = PCPU_GET(curpmap);
		MPASS(old_pmap->pm_ucr3 != PMAP_NO_CR3);
		old_pcidp = zpcpu_get_cpu(old_pmap->pm_pcidp, cpuid);
		old_pcidp->pm_gen = 0;
	}

	pcidp = zpcpu_get_cpu(pmap->pm_pcidp, cpuid);
	cached = pmap_pcid_alloc_checked(pmap, pcidp);
	cr3 = rcr3();
	if ((cr3 & ~CR3_PCID_MASK) != pmap->pm_cr3)
		load_cr3(pmap->pm_cr3 | pcidp->pm_pcid);
	PCPU_SET(curpmap, pmap);
	kcr3 = pmap->pm_cr3 | pcidp->pm_pcid;
	ucr3 = pmap->pm_ucr3 | pcidp->pm_pcid | PMAP_PCID_USER_PT;

	if (!cached && pmap->pm_ucr3 != PMAP_NO_CR3)
		PCPU_SET(ucr3_load_mask, ~CR3_PCID_SAVE);

	PCPU_SET(kcr3, kcr3 | CR3_PCID_SAVE);
	PCPU_SET(ucr3, ucr3 | CR3_PCID_SAVE);
	if (cached)
		counter_u64_add(pcid_save_cnt, 1);

	pmap_activate_sw_pti_post(td, pmap);
}

static void
pmap_activate_sw_pcid_nopti(struct thread *td __unused, pmap_t pmap,
    u_int cpuid)
{
	struct pmap_pcid *pcidp;
	uint64_t cached, cr3;

	KASSERT((read_rflags() & PSL_I) == 0,
	    ("PCID needs interrupts disabled in pmap_activate_sw()"));

	pcidp = zpcpu_get_cpu(pmap->pm_pcidp, cpuid);
	cached = pmap_pcid_alloc_checked(pmap, pcidp);
	cr3 = rcr3();
	if (!cached || (cr3 & ~CR3_PCID_MASK) != pmap->pm_cr3)
		load_cr3(pmap->pm_cr3 | pcidp->pm_pcid | cached);
	PCPU_SET(curpmap, pmap);
	if (cached)
		counter_u64_add(pcid_save_cnt, 1);
}

static void
pmap_activate_sw_nopcid_nopti(struct thread *td __unused, pmap_t pmap,
    u_int cpuid __unused)
{

	load_cr3(pmap->pm_cr3);
	PCPU_SET(curpmap, pmap);
}

static void
pmap_activate_sw_nopcid_pti(struct thread *td, pmap_t pmap,
    u_int cpuid __unused)
{

	pmap_activate_sw_nopcid_nopti(td, pmap, cpuid);
	PCPU_SET(kcr3, pmap->pm_cr3);
	PCPU_SET(ucr3, pmap->pm_ucr3);
	pmap_activate_sw_pti_post(td, pmap);
}

DEFINE_IFUNC(static, void, pmap_activate_sw_mode, (struct thread *, pmap_t,
    u_int))
{

	if (pmap_pcid_enabled && pti)
		return (pmap_activate_sw_pcid_pti);
	else if (pmap_pcid_enabled && !pti)
		return (pmap_activate_sw_pcid_nopti);
	else if (!pmap_pcid_enabled && pti)
		return (pmap_activate_sw_nopcid_pti);
	else /* if (!pmap_pcid_enabled && !pti) */
		return (pmap_activate_sw_nopcid_nopti);
}

void
pmap_activate_sw(struct thread *td)
{
	pmap_t oldpmap, pmap;
	u_int cpuid;

	oldpmap = PCPU_GET(curpmap);
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	if (oldpmap == pmap) {
		if (cpu_vendor_id != CPU_VENDOR_INTEL)
			mfence();
		return;
	}
	cpuid = PCPU_GET(cpuid);
#ifdef SMP
	CPU_SET_ATOMIC(cpuid, &pmap->pm_active);
#else
	CPU_SET(cpuid, &pmap->pm_active);
#endif
	pmap_activate_sw_mode(td, pmap, cpuid);
#ifdef SMP
	CPU_CLR_ATOMIC(cpuid, &oldpmap->pm_active);
#else
	CPU_CLR(cpuid, &oldpmap->pm_active);
#endif
}

void
pmap_activate(struct thread *td)
{
	/*
	 * invltlb_{invpcid,}_pcid_handler() is used to handle an
	 * invalidate_all IPI, which checks for curpmap ==
	 * smp_tlb_pmap.  The below sequence of operations has a
	 * window where %CR3 is loaded with the new pmap's PML4
	 * address, but the curpmap value has not yet been updated.
	 * This causes the invltlb IPI handler, which is called
	 * between the updates, to execute as a NOP, which leaves
	 * stale TLB entries.
	 *
	 * Note that the most common use of pmap_activate_sw(), from
	 * a context switch, is immune to this race, because
	 * interrupts are disabled (while the thread lock is owned),
	 * so the IPI is delayed until after curpmap is updated.  Protect
	 * other callers in a similar way, by disabling interrupts
	 * around the %cr3 register reload and curpmap assignment.
	 */
	spinlock_enter();
	pmap_activate_sw(td);
	spinlock_exit();
}

void
pmap_activate_boot(pmap_t pmap)
{
	uint64_t kcr3;
	u_int cpuid;

	/*
	 * kernel_pmap must be never deactivated, and we ensure that
	 * by never activating it at all.
	 */
	MPASS(pmap != kernel_pmap);

	cpuid = PCPU_GET(cpuid);
#ifdef SMP
	CPU_SET_ATOMIC(cpuid, &pmap->pm_active);
#else
	CPU_SET(cpuid, &pmap->pm_active);
#endif
	PCPU_SET(curpmap, pmap);
	if (pti) {
		kcr3 = pmap->pm_cr3;
		if (pmap_pcid_enabled)
			kcr3 |= pmap_get_pcid(pmap) | CR3_PCID_SAVE;
	} else {
		kcr3 = PMAP_NO_CR3;
	}
	PCPU_SET(kcr3, kcr3);
	PCPU_SET(ucr3, PMAP_NO_CR3);
}

void
pmap_active_cpus(pmap_t pmap, cpuset_t *res)
{
	*res = pmap->pm_active;
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
}

/*
 *	Increase the starting virtual address of the given mapping if a
 *	different alignment might result in more superpage mappings.
 */
void
pmap_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{
	vm_offset_t superpage_offset;

	if (size < NBPDR)
		return;
	if (object != NULL && (object->flags & OBJ_COLORED) != 0)
		offset += ptoa(object->pg_color);
	superpage_offset = offset & PDRMASK;
	if (size - ((NBPDR - superpage_offset) & PDRMASK) < NBPDR ||
	    (*addr & PDRMASK) == superpage_offset)
		return;
	if ((*addr & PDRMASK) < superpage_offset)
		*addr = (*addr & ~PDRMASK) + superpage_offset;
	else
		*addr = ((*addr + PDRMASK) & ~PDRMASK) + superpage_offset;
}

#ifdef INVARIANTS
static unsigned long num_dirty_emulations;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, num_dirty_emulations, CTLFLAG_RW,
	     &num_dirty_emulations, 0, NULL);

static unsigned long num_accessed_emulations;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, num_accessed_emulations, CTLFLAG_RW,
	     &num_accessed_emulations, 0, NULL);

static unsigned long num_superpage_accessed_emulations;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, num_superpage_accessed_emulations, CTLFLAG_RW,
	     &num_superpage_accessed_emulations, 0, NULL);

static unsigned long ad_emulation_superpage_promotions;
SYSCTL_ULONG(_vm_pmap, OID_AUTO, ad_emulation_superpage_promotions, CTLFLAG_RW,
	     &ad_emulation_superpage_promotions, 0, NULL);
#endif	/* INVARIANTS */

int
pmap_emulate_accessed_dirty(pmap_t pmap, vm_offset_t va, int ftype)
{
	int rv;
	struct rwlock *lock;
#if VM_NRESERVLEVEL > 0
	vm_page_t m, mpte;
#endif
	pd_entry_t *pde;
	pt_entry_t *pte, PG_A, PG_M, PG_RW, PG_V;

	KASSERT(ftype == VM_PROT_READ || ftype == VM_PROT_WRITE,
	    ("pmap_emulate_accessed_dirty: invalid fault type %d", ftype));

	if (!pmap_emulate_ad_bits(pmap))
		return (-1);

	PG_A = pmap_accessed_bit(pmap);
	PG_M = pmap_modified_bit(pmap);
	PG_V = pmap_valid_bit(pmap);
	PG_RW = pmap_rw_bit(pmap);

	rv = -1;
	lock = NULL;
	PMAP_LOCK(pmap);

	pde = pmap_pde(pmap, va);
	if (pde == NULL || (*pde & PG_V) == 0)
		goto done;

	if ((*pde & PG_PS) != 0) {
		if (ftype == VM_PROT_READ) {
#ifdef INVARIANTS
			atomic_add_long(&num_superpage_accessed_emulations, 1);
#endif
			*pde |= PG_A;
			rv = 0;
		}
		goto done;
	}

	pte = pmap_pde_to_pte(pde, va);
	if ((*pte & PG_V) == 0)
		goto done;

	if (ftype == VM_PROT_WRITE) {
		if ((*pte & PG_RW) == 0)
			goto done;
		/*
		 * Set the modified and accessed bits simultaneously.
		 *
		 * Intel EPT PTEs that do software emulation of A/D bits map
		 * PG_A and PG_M to EPT_PG_READ and EPT_PG_WRITE respectively.
		 * An EPT misconfiguration is triggered if the PTE is writable
		 * but not readable (WR=10). This is avoided by setting PG_A
		 * and PG_M simultaneously.
		 */
		*pte |= PG_M | PG_A;
	} else {
		*pte |= PG_A;
	}

#if VM_NRESERVLEVEL > 0
	/* try to promote the mapping */
	if (va < VM_MAXUSER_ADDRESS)
		mpte = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
	else
		mpte = NULL;

	m = PHYS_TO_VM_PAGE(*pte & PG_FRAME);

	if ((mpte == NULL || mpte->ref_count == NPTEPG) &&
	    (m->flags & PG_FICTITIOUS) == 0 &&
	    vm_reserv_level_iffullpop(m) == 0 &&
	    pmap_promote_pde(pmap, pde, va, mpte, &lock)) {
#ifdef INVARIANTS
		atomic_add_long(&ad_emulation_superpage_promotions, 1);
#endif
	}
#endif

#ifdef INVARIANTS
	if (ftype == VM_PROT_WRITE)
		atomic_add_long(&num_dirty_emulations, 1);
	else
		atomic_add_long(&num_accessed_emulations, 1);
#endif
	rv = 0;		/* success */
done:
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);
	return (rv);
}

void
pmap_get_mapping(pmap_t pmap, vm_offset_t va, uint64_t *ptr, int *num)
{
	pml4_entry_t *pml4;
	pdp_entry_t *pdp;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	int idx;

	idx = 0;
	PG_V = pmap_valid_bit(pmap);
	PMAP_LOCK(pmap);

	pml4 = pmap_pml4e(pmap, va);
	if (pml4 == NULL)
		goto done;
	ptr[idx++] = *pml4;
	if ((*pml4 & PG_V) == 0)
		goto done;

	pdp = pmap_pml4e_to_pdpe(pml4, va);
	ptr[idx++] = *pdp;
	if ((*pdp & PG_V) == 0 || (*pdp & PG_PS) != 0)
		goto done;

	pde = pmap_pdpe_to_pde(pdp, va);
	ptr[idx++] = *pde;
	if ((*pde & PG_V) == 0 || (*pde & PG_PS) != 0)
		goto done;

	pte = pmap_pde_to_pte(pde, va);
	ptr[idx++] = *pte;

done:
	PMAP_UNLOCK(pmap);
	*num = idx;
}

/**
 * Get the kernel virtual address of a set of physical pages. If there are
 * physical addresses not covered by the DMAP perform a transient mapping
 * that will be removed when calling pmap_unmap_io_transient.
 *
 * \param page        The pages the caller wishes to obtain the virtual
 *                    address on the kernel memory map.
 * \param vaddr       On return contains the kernel virtual memory address
 *                    of the pages passed in the page parameter.
 * \param count       Number of pages passed in.
 * \param can_fault   true if the thread using the mapped pages can take
 *                    page faults, false otherwise.
 *
 * \returns true if the caller must call pmap_unmap_io_transient when
 *          finished or false otherwise.
 *
 */
bool
pmap_map_io_transient(vm_page_t page[], vm_offset_t vaddr[], int count,
    bool can_fault)
{
	vm_paddr_t paddr;
	bool needs_mapping;
	pt_entry_t *pte;
	int cache_bits, error __unused, i;

	/*
	 * Allocate any KVA space that we need, this is done in a separate
	 * loop to prevent calling vmem_alloc while pinned.
	 */
	needs_mapping = false;
	for (i = 0; i < count; i++) {
		paddr = VM_PAGE_TO_PHYS(page[i]);
		if (__predict_false(paddr >= dmaplimit)) {
			error = vmem_alloc(kernel_arena, PAGE_SIZE,
			    M_BESTFIT | M_WAITOK, &vaddr[i]);
			KASSERT(error == 0, ("vmem_alloc failed: %d", error));
			needs_mapping = true;
		} else {
			vaddr[i] = PHYS_TO_DMAP(paddr);
		}
	}

	/* Exit early if everything is covered by the DMAP */
	if (!needs_mapping)
		return (false);

	/*
	 * NB:  The sequence of updating a page table followed by accesses
	 * to the corresponding pages used in the !DMAP case is subject to
	 * the situation described in the "AMD64 Architecture Programmer's
	 * Manual Volume 2: System Programming" rev. 3.23, "7.3.1 Special
	 * Coherency Considerations".  Therefore, issuing the INVLPG right
	 * after modifying the PTE bits is crucial.
	 */
	if (!can_fault)
		sched_pin();
	for (i = 0; i < count; i++) {
		paddr = VM_PAGE_TO_PHYS(page[i]);
		if (paddr >= dmaplimit) {
			if (can_fault) {
				/*
				 * Slow path, since we can get page faults
				 * while mappings are active don't pin the
				 * thread to the CPU and instead add a global
				 * mapping visible to all CPUs.
				 */
				pmap_qenter(vaddr[i], &page[i], 1);
			} else {
				pte = vtopte(vaddr[i]);
				cache_bits = pmap_cache_bits(kernel_pmap,
				    page[i]->md.pat_mode, false);
				pte_store(pte, paddr | X86_PG_RW | X86_PG_V |
				    cache_bits);
				pmap_invlpg(kernel_pmap, vaddr[i]);
			}
		}
	}

	return (needs_mapping);
}

void
pmap_unmap_io_transient(vm_page_t page[], vm_offset_t vaddr[], int count,
    bool can_fault)
{
	vm_paddr_t paddr;
	int i;

	if (!can_fault)
		sched_unpin();
	for (i = 0; i < count; i++) {
		paddr = VM_PAGE_TO_PHYS(page[i]);
		if (paddr >= dmaplimit) {
			if (can_fault)
				pmap_qremove(vaddr[i], 1);
			vmem_free(kernel_arena, vaddr[i], PAGE_SIZE);
		}
	}
}

vm_offset_t
pmap_quick_enter_page(vm_page_t m)
{
	vm_paddr_t paddr;

	paddr = VM_PAGE_TO_PHYS(m);
	if (paddr < dmaplimit)
		return (PHYS_TO_DMAP(paddr));
	mtx_lock_spin(&qframe_mtx);
	KASSERT(*vtopte(qframe) == 0, ("qframe busy"));

	/*
	 * Since qframe is exclusively mapped by us, and we do not set
	 * PG_G, we can use INVLPG here.
	 */
	invlpg(qframe);

	pte_store(vtopte(qframe), paddr | X86_PG_RW | X86_PG_V | X86_PG_A |
	    X86_PG_M | pmap_cache_bits(kernel_pmap, m->md.pat_mode, false));
	return (qframe);
}

void
pmap_quick_remove_page(vm_offset_t addr)
{

	if (addr != qframe)
		return;
	pte_store(vtopte(qframe), 0);
	mtx_unlock_spin(&qframe_mtx);
}

/*
 * Pdp pages from the large map are managed differently from either
 * kernel or user page table pages.  They are permanently allocated at
 * initialization time, and their reference count is permanently set to
 * zero.  The pml4 entries pointing to those pages are copied into
 * each allocated pmap.
 *
 * In contrast, pd and pt pages are managed like user page table
 * pages.  They are dynamically allocated, and their reference count
 * represents the number of valid entries within the page.
 */
static vm_page_t
pmap_large_map_getptp_unlocked(void)
{
	return (pmap_alloc_pt_page(kernel_pmap, 0, VM_ALLOC_ZERO));
}

static vm_page_t
pmap_large_map_getptp(void)
{
	vm_page_t m;

	PMAP_LOCK_ASSERT(kernel_pmap, MA_OWNED);
	m = pmap_large_map_getptp_unlocked();
	if (m == NULL) {
		PMAP_UNLOCK(kernel_pmap);
		vm_wait(NULL);
		PMAP_LOCK(kernel_pmap);
		/* Callers retry. */
	}
	return (m);
}

static pdp_entry_t *
pmap_large_map_pdpe(vm_offset_t va)
{
	vm_pindex_t pml4_idx;
	vm_paddr_t mphys;

	pml4_idx = pmap_pml4e_index(va);
	KASSERT(LMSPML4I <= pml4_idx && pml4_idx < LMSPML4I + lm_ents,
	    ("pmap_large_map_pdpe: va %#jx out of range idx %#jx LMSPML4I "
	    "%#jx lm_ents %d",
	    (uintmax_t)va, (uintmax_t)pml4_idx, LMSPML4I, lm_ents));
	KASSERT((kernel_pml4[pml4_idx] & X86_PG_V) != 0,
	    ("pmap_large_map_pdpe: invalid pml4 for va %#jx idx %#jx "
	    "LMSPML4I %#jx lm_ents %d",
	    (uintmax_t)va, (uintmax_t)pml4_idx, LMSPML4I, lm_ents));
	mphys = kernel_pml4[pml4_idx] & PG_FRAME;
	return ((pdp_entry_t *)PHYS_TO_DMAP(mphys) + pmap_pdpe_index(va));
}

static pd_entry_t *
pmap_large_map_pde(vm_offset_t va)
{
	pdp_entry_t *pdpe;
	vm_page_t m;
	vm_paddr_t mphys;

retry:
	pdpe = pmap_large_map_pdpe(va);
	if (*pdpe == 0) {
		m = pmap_large_map_getptp();
		if (m == NULL)
			goto retry;
		mphys = VM_PAGE_TO_PHYS(m);
		*pdpe = mphys | X86_PG_A | X86_PG_RW | X86_PG_V | pg_nx;
	} else {
		MPASS((*pdpe & X86_PG_PS) == 0);
		mphys = *pdpe & PG_FRAME;
	}
	return ((pd_entry_t *)PHYS_TO_DMAP(mphys) + pmap_pde_index(va));
}

static pt_entry_t *
pmap_large_map_pte(vm_offset_t va)
{
	pd_entry_t *pde;
	vm_page_t m;
	vm_paddr_t mphys;

retry:
	pde = pmap_large_map_pde(va);
	if (*pde == 0) {
		m = pmap_large_map_getptp();
		if (m == NULL)
			goto retry;
		mphys = VM_PAGE_TO_PHYS(m);
		*pde = mphys | X86_PG_A | X86_PG_RW | X86_PG_V | pg_nx;
		PHYS_TO_VM_PAGE(DMAP_TO_PHYS((uintptr_t)pde))->ref_count++;
	} else {
		MPASS((*pde & X86_PG_PS) == 0);
		mphys = *pde & PG_FRAME;
	}
	return ((pt_entry_t *)PHYS_TO_DMAP(mphys) + pmap_pte_index(va));
}

static vm_paddr_t
pmap_large_map_kextract(vm_offset_t va)
{
	pdp_entry_t *pdpe, pdp;
	pd_entry_t *pde, pd;
	pt_entry_t *pte, pt;

	KASSERT(PMAP_ADDRESS_IN_LARGEMAP(va),
	    ("not largemap range %#lx", (u_long)va));
	pdpe = pmap_large_map_pdpe(va);
	pdp = *pdpe;
	KASSERT((pdp & X86_PG_V) != 0,
	    ("invalid pdp va %#lx pdpe %#lx pdp %#lx", va,
	    (u_long)pdpe, pdp));
	if ((pdp & X86_PG_PS) != 0) {
		KASSERT((amd_feature & AMDID_PAGE1GB) != 0,
		    ("no 1G pages, va %#lx pdpe %#lx pdp %#lx", va,
		    (u_long)pdpe, pdp));
		return ((pdp & PG_PS_PDP_FRAME) | (va & PDPMASK));
	}
	pde = pmap_pdpe_to_pde(pdpe, va);
	pd = *pde;
	KASSERT((pd & X86_PG_V) != 0,
	    ("invalid pd va %#lx pde %#lx pd %#lx", va, (u_long)pde, pd));
	if ((pd & X86_PG_PS) != 0)
		return ((pd & PG_PS_FRAME) | (va & PDRMASK));
	pte = pmap_pde_to_pte(pde, va);
	pt = *pte;
	KASSERT((pt & X86_PG_V) != 0,
	    ("invalid pte va %#lx pte %#lx pt %#lx", va, (u_long)pte, pt));
	return ((pt & PG_FRAME) | (va & PAGE_MASK));
}

static int
pmap_large_map_getva(vm_size_t len, vm_offset_t align, vm_offset_t phase,
    vmem_addr_t *vmem_res)
{

	/*
	 * Large mappings are all but static.  Consequently, there
	 * is no point in waiting for an earlier allocation to be
	 * freed.
	 */
	return (vmem_xalloc(large_vmem, len, align, phase, 0, VMEM_ADDR_MIN,
	    VMEM_ADDR_MAX, M_NOWAIT | M_BESTFIT, vmem_res));
}

int
pmap_large_map(vm_paddr_t spa, vm_size_t len, void **addr,
    vm_memattr_t mattr)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_offset_t va, inc;
	vmem_addr_t vmem_res;
	vm_paddr_t pa;
	int error;

	if (len == 0 || spa + len < spa)
		return (EINVAL);

	/* See if DMAP can serve. */
	if (spa + len <= dmaplimit) {
		va = PHYS_TO_DMAP(spa);
		*addr = (void *)va;
		return (pmap_change_attr(va, len, mattr));
	}

	/*
	 * No, allocate KVA.  Fit the address with best possible
	 * alignment for superpages.  Fall back to worse align if
	 * failed.
	 */
	error = ENOMEM;
	if ((amd_feature & AMDID_PAGE1GB) != 0 && rounddown2(spa + len,
	    NBPDP) >= roundup2(spa, NBPDP) + NBPDP)
		error = pmap_large_map_getva(len, NBPDP, spa & PDPMASK,
		    &vmem_res);
	if (error != 0 && rounddown2(spa + len, NBPDR) >= roundup2(spa,
	    NBPDR) + NBPDR)
		error = pmap_large_map_getva(len, NBPDR, spa & PDRMASK,
		    &vmem_res);
	if (error != 0)
		error = pmap_large_map_getva(len, PAGE_SIZE, 0, &vmem_res);
	if (error != 0)
		return (error);

	/*
	 * Fill pagetable.  PG_M is not pre-set, we scan modified bits
	 * in the pagetable to minimize flushing.  No need to
	 * invalidate TLB, since we only update invalid entries.
	 */
	PMAP_LOCK(kernel_pmap);
	for (pa = spa, va = vmem_res; len > 0; pa += inc, va += inc,
	    len -= inc) {
		if ((amd_feature & AMDID_PAGE1GB) != 0 && len >= NBPDP &&
		    (pa & PDPMASK) == 0 && (va & PDPMASK) == 0) {
			pdpe = pmap_large_map_pdpe(va);
			MPASS(*pdpe == 0);
			*pdpe = pa | pg_g | X86_PG_PS | X86_PG_RW |
			    X86_PG_V | X86_PG_A | pg_nx |
			    pmap_cache_bits(kernel_pmap, mattr, true);
			inc = NBPDP;
		} else if (len >= NBPDR && (pa & PDRMASK) == 0 &&
		    (va & PDRMASK) == 0) {
			pde = pmap_large_map_pde(va);
			MPASS(*pde == 0);
			*pde = pa | pg_g | X86_PG_PS | X86_PG_RW |
			    X86_PG_V | X86_PG_A | pg_nx |
			    pmap_cache_bits(kernel_pmap, mattr, true);
			PHYS_TO_VM_PAGE(DMAP_TO_PHYS((uintptr_t)pde))->
			    ref_count++;
			inc = NBPDR;
		} else {
			pte = pmap_large_map_pte(va);
			MPASS(*pte == 0);
			*pte = pa | pg_g | X86_PG_RW | X86_PG_V |
			    X86_PG_A | pg_nx | pmap_cache_bits(kernel_pmap,
			    mattr, false);
			PHYS_TO_VM_PAGE(DMAP_TO_PHYS((uintptr_t)pte))->
			    ref_count++;
			inc = PAGE_SIZE;
		}
	}
	PMAP_UNLOCK(kernel_pmap);
	MPASS(len == 0);

	*addr = (void *)vmem_res;
	return (0);
}

void
pmap_large_unmap(void *svaa, vm_size_t len)
{
	vm_offset_t sva, va;
	vm_size_t inc;
	pdp_entry_t *pdpe, pdp;
	pd_entry_t *pde, pd;
	pt_entry_t *pte;
	vm_page_t m;
	struct spglist spgf;

	sva = (vm_offset_t)svaa;
	if (len == 0 || sva + len < sva || (sva >= DMAP_MIN_ADDRESS &&
	    sva + len <= DMAP_MIN_ADDRESS + dmaplimit))
		return;

	SLIST_INIT(&spgf);
	KASSERT(PMAP_ADDRESS_IN_LARGEMAP(sva) &&
	    PMAP_ADDRESS_IN_LARGEMAP(sva + len - 1),
	    ("not largemap range %#lx %#lx", (u_long)svaa, (u_long)svaa + len));
	PMAP_LOCK(kernel_pmap);
	for (va = sva; va < sva + len; va += inc) {
		pdpe = pmap_large_map_pdpe(va);
		pdp = *pdpe;
		KASSERT((pdp & X86_PG_V) != 0,
		    ("invalid pdp va %#lx pdpe %#lx pdp %#lx", va,
		    (u_long)pdpe, pdp));
		if ((pdp & X86_PG_PS) != 0) {
			KASSERT((amd_feature & AMDID_PAGE1GB) != 0,
			    ("no 1G pages, va %#lx pdpe %#lx pdp %#lx", va,
			    (u_long)pdpe, pdp));
			KASSERT((va & PDPMASK) == 0,
			    ("PDPMASK bit set, va %#lx pdpe %#lx pdp %#lx", va,
			    (u_long)pdpe, pdp));
			KASSERT(va + NBPDP <= sva + len,
			    ("unmap covers partial 1GB page, sva %#lx va %#lx "
			    "pdpe %#lx pdp %#lx len %#lx", sva, va,
			    (u_long)pdpe, pdp, len));
			*pdpe = 0;
			inc = NBPDP;
			continue;
		}
		pde = pmap_pdpe_to_pde(pdpe, va);
		pd = *pde;
		KASSERT((pd & X86_PG_V) != 0,
		    ("invalid pd va %#lx pde %#lx pd %#lx", va,
		    (u_long)pde, pd));
		if ((pd & X86_PG_PS) != 0) {
			KASSERT((va & PDRMASK) == 0,
			    ("PDRMASK bit set, va %#lx pde %#lx pd %#lx", va,
			    (u_long)pde, pd));
			KASSERT(va + NBPDR <= sva + len,
			    ("unmap covers partial 2MB page, sva %#lx va %#lx "
			    "pde %#lx pd %#lx len %#lx", sva, va, (u_long)pde,
			    pd, len));
			pde_store(pde, 0);
			inc = NBPDR;
			m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pde));
			m->ref_count--;
			if (m->ref_count == 0) {
				*pdpe = 0;
				SLIST_INSERT_HEAD(&spgf, m, plinks.s.ss);
			}
			continue;
		}
		pte = pmap_pde_to_pte(pde, va);
		KASSERT((*pte & X86_PG_V) != 0,
		    ("invalid pte va %#lx pte %#lx pt %#lx", va,
		    (u_long)pte, *pte));
		pte_clear(pte);
		inc = PAGE_SIZE;
		m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pte));
		m->ref_count--;
		if (m->ref_count == 0) {
			*pde = 0;
			SLIST_INSERT_HEAD(&spgf, m, plinks.s.ss);
			m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pde));
			m->ref_count--;
			if (m->ref_count == 0) {
				*pdpe = 0;
				SLIST_INSERT_HEAD(&spgf, m, plinks.s.ss);
			}
		}
	}
	pmap_invalidate_range(kernel_pmap, sva, sva + len);
	PMAP_UNLOCK(kernel_pmap);
	vm_page_free_pages_toq(&spgf, false);
	vmem_free(large_vmem, sva, len);
}

static void
pmap_large_map_wb_fence_mfence(void)
{

	mfence();
}

static void
pmap_large_map_wb_fence_atomic(void)
{

	atomic_thread_fence_seq_cst();
}

static void
pmap_large_map_wb_fence_nop(void)
{
}

DEFINE_IFUNC(static, void, pmap_large_map_wb_fence, (void))
{

	if (cpu_vendor_id != CPU_VENDOR_INTEL)
		return (pmap_large_map_wb_fence_mfence);
	else if ((cpu_stdext_feature & (CPUID_STDEXT_CLWB |
	    CPUID_STDEXT_CLFLUSHOPT)) == 0)
		return (pmap_large_map_wb_fence_atomic);
	else
		/* clflush is strongly enough ordered */
		return (pmap_large_map_wb_fence_nop);
}

static void
pmap_large_map_flush_range_clwb(vm_offset_t va, vm_size_t len)
{

	for (; len > 0; len -= cpu_clflush_line_size,
	    va += cpu_clflush_line_size)
		clwb(va);
}

static void
pmap_large_map_flush_range_clflushopt(vm_offset_t va, vm_size_t len)
{

	for (; len > 0; len -= cpu_clflush_line_size,
	    va += cpu_clflush_line_size)
		clflushopt(va);
}

static void
pmap_large_map_flush_range_clflush(vm_offset_t va, vm_size_t len)
{

	for (; len > 0; len -= cpu_clflush_line_size,
	    va += cpu_clflush_line_size)
		clflush(va);
}

static void
pmap_large_map_flush_range_nop(vm_offset_t sva __unused, vm_size_t len __unused)
{
}

DEFINE_IFUNC(static, void, pmap_large_map_flush_range, (vm_offset_t, vm_size_t))
{

	if ((cpu_stdext_feature & CPUID_STDEXT_CLWB) != 0)
		return (pmap_large_map_flush_range_clwb);
	else if ((cpu_stdext_feature & CPUID_STDEXT_CLFLUSHOPT) != 0)
		return (pmap_large_map_flush_range_clflushopt);
	else if ((cpu_feature & CPUID_CLFSH) != 0)
		return (pmap_large_map_flush_range_clflush);
	else
		return (pmap_large_map_flush_range_nop);
}

static void
pmap_large_map_wb_large(vm_offset_t sva, vm_offset_t eva)
{
	volatile u_long *pe;
	u_long p;
	vm_offset_t va;
	vm_size_t inc;
	bool seen_other;

	for (va = sva; va < eva; va += inc) {
		inc = 0;
		if ((amd_feature & AMDID_PAGE1GB) != 0) {
			pe = (volatile u_long *)pmap_large_map_pdpe(va);
			p = *pe;
			if ((p & X86_PG_PS) != 0)
				inc = NBPDP;
		}
		if (inc == 0) {
			pe = (volatile u_long *)pmap_large_map_pde(va);
			p = *pe;
			if ((p & X86_PG_PS) != 0)
				inc = NBPDR;
		}
		if (inc == 0) {
			pe = (volatile u_long *)pmap_large_map_pte(va);
			p = *pe;
			inc = PAGE_SIZE;
		}
		seen_other = false;
		for (;;) {
			if ((p & X86_PG_AVAIL1) != 0) {
				/*
				 * Spin-wait for the end of a parallel
				 * write-back.
				 */
				cpu_spinwait();
				p = *pe;

				/*
				 * If we saw other write-back
				 * occuring, we cannot rely on PG_M to
				 * indicate state of the cache.  The
				 * PG_M bit is cleared before the
				 * flush to avoid ignoring new writes,
				 * and writes which are relevant for
				 * us might happen after.
				 */
				seen_other = true;
				continue;
			}

			if ((p & X86_PG_M) != 0 || seen_other) {
				if (!atomic_fcmpset_long(pe, &p,
				    (p & ~X86_PG_M) | X86_PG_AVAIL1))
					/*
					 * If we saw PG_M without
					 * PG_AVAIL1, and then on the
					 * next attempt we do not
					 * observe either PG_M or
					 * PG_AVAIL1, the other
					 * write-back started after us
					 * and finished before us.  We
					 * can rely on it doing our
					 * work.
					 */
					continue;
				pmap_large_map_flush_range(va, inc);
				atomic_clear_long(pe, X86_PG_AVAIL1);
			}
			break;
		}
		maybe_yield();
	}
}

/*
 * Write-back cache lines for the given address range.
 *
 * Must be called only on the range or sub-range returned from
 * pmap_large_map().  Must not be called on the coalesced ranges.
 *
 * Does nothing on CPUs without CLWB, CLFLUSHOPT, or CLFLUSH
 * instructions support.
 */
void
pmap_large_map_wb(void *svap, vm_size_t len)
{
	vm_offset_t eva, sva;

	sva = (vm_offset_t)svap;
	eva = sva + len;
	pmap_large_map_wb_fence();
	if (sva >= DMAP_MIN_ADDRESS && eva <= DMAP_MIN_ADDRESS + dmaplimit) {
		pmap_large_map_flush_range(sva, len);
	} else {
		KASSERT(sva >= LARGEMAP_MIN_ADDRESS &&
		    eva <= LARGEMAP_MIN_ADDRESS + lm_ents * NBPML4,
		    ("pmap_large_map_wb: not largemap %#lx %#lx", sva, len));
		pmap_large_map_wb_large(sva, eva);
	}
	pmap_large_map_wb_fence();
}

static vm_page_t
pmap_pti_alloc_page(void)
{
	vm_page_t m;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);
	m = vm_page_grab(pti_obj, pti_pg_idx++, VM_ALLOC_WIRED | VM_ALLOC_ZERO);
	return (m);
}

static bool
pmap_pti_free_page(vm_page_t m)
{
	if (!vm_page_unwire_noq(m))
		return (false);
	vm_page_xbusy_claim(m);
	vm_page_free_zero(m);
	return (true);
}

static void
pmap_pti_init(void)
{
	vm_page_t pml4_pg;
	pdp_entry_t *pdpe;
	vm_offset_t va;
	int i;

	if (!pti)
		return;
	pti_obj = vm_pager_allocate(OBJT_PHYS, NULL, 0, VM_PROT_ALL, 0, NULL);
	VM_OBJECT_WLOCK(pti_obj);
	pml4_pg = pmap_pti_alloc_page();
	pti_pml4 = (pml4_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pml4_pg));
	for (va = VM_MIN_KERNEL_ADDRESS; va <= VM_MAX_KERNEL_ADDRESS &&
	    va >= VM_MIN_KERNEL_ADDRESS && va > NBPML4; va += NBPML4) {
		pdpe = pmap_pti_pdpe(va);
		pmap_pti_wire_pte(pdpe);
	}
	pmap_pti_add_kva_locked((vm_offset_t)&__pcpu[0],
	    (vm_offset_t)&__pcpu[0] + sizeof(__pcpu[0]) * MAXCPU, false);
	pmap_pti_add_kva_locked((vm_offset_t)idt, (vm_offset_t)idt +
	    sizeof(struct gate_descriptor) * NIDT, false);
	CPU_FOREACH(i) {
		/* Doublefault stack IST 1 */
		va = __pcpu[i].pc_common_tss.tss_ist1 + sizeof(struct nmi_pcpu);
		pmap_pti_add_kva_locked(va - DBLFAULT_STACK_SIZE, va, false);
		/* NMI stack IST 2 */
		va = __pcpu[i].pc_common_tss.tss_ist2 + sizeof(struct nmi_pcpu);
		pmap_pti_add_kva_locked(va - NMI_STACK_SIZE, va, false);
		/* MC# stack IST 3 */
		va = __pcpu[i].pc_common_tss.tss_ist3 +
		    sizeof(struct nmi_pcpu);
		pmap_pti_add_kva_locked(va - MCE_STACK_SIZE, va, false);
		/* DB# stack IST 4 */
		va = __pcpu[i].pc_common_tss.tss_ist4 + sizeof(struct nmi_pcpu);
		pmap_pti_add_kva_locked(va - DBG_STACK_SIZE, va, false);
	}
	pmap_pti_add_kva_locked((vm_offset_t)KERNSTART, (vm_offset_t)etext,
	    true);
	pti_finalized = true;
	VM_OBJECT_WUNLOCK(pti_obj);
}

static void
pmap_cpu_init(void *arg __unused)
{
	CPU_COPY(&all_cpus, &kernel_pmap->pm_active);
	pmap_pti_init();
}
SYSINIT(pmap_cpu, SI_SUB_CPU + 1, SI_ORDER_ANY, pmap_cpu_init, NULL);

static pdp_entry_t *
pmap_pti_pdpe(vm_offset_t va)
{
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	vm_page_t m;
	vm_pindex_t pml4_idx;
	vm_paddr_t mphys;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);

	pml4_idx = pmap_pml4e_index(va);
	pml4e = &pti_pml4[pml4_idx];
	m = NULL;
	if (*pml4e == 0) {
		if (pti_finalized)
			panic("pml4 alloc after finalization\n");
		m = pmap_pti_alloc_page();
		if (*pml4e != 0) {
			pmap_pti_free_page(m);
			mphys = *pml4e & ~PAGE_MASK;
		} else {
			mphys = VM_PAGE_TO_PHYS(m);
			*pml4e = mphys | X86_PG_RW | X86_PG_V;
		}
	} else {
		mphys = *pml4e & ~PAGE_MASK;
	}
	pdpe = (pdp_entry_t *)PHYS_TO_DMAP(mphys) + pmap_pdpe_index(va);
	return (pdpe);
}

static void
pmap_pti_wire_pte(void *pte)
{
	vm_page_t m;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((uintptr_t)pte));
	m->ref_count++;
}

static void
pmap_pti_unwire_pde(void *pde, bool only_ref)
{
	vm_page_t m;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((uintptr_t)pde));
	MPASS(only_ref || m->ref_count > 1);
	pmap_pti_free_page(m);
}

static void
pmap_pti_unwire_pte(void *pte, vm_offset_t va)
{
	vm_page_t m;
	pd_entry_t *pde;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((uintptr_t)pte));
	if (pmap_pti_free_page(m)) {
		pde = pmap_pti_pde(va);
		MPASS((*pde & (X86_PG_PS | X86_PG_V)) == X86_PG_V);
		*pde = 0;
		pmap_pti_unwire_pde(pde, false);
	}
}

static pd_entry_t *
pmap_pti_pde(vm_offset_t va)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	vm_page_t m;
	vm_pindex_t pd_idx;
	vm_paddr_t mphys;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);

	pdpe = pmap_pti_pdpe(va);
	if (*pdpe == 0) {
		m = pmap_pti_alloc_page();
		if (*pdpe != 0) {
			pmap_pti_free_page(m);
			MPASS((*pdpe & X86_PG_PS) == 0);
			mphys = *pdpe & ~PAGE_MASK;
		} else {
			mphys =  VM_PAGE_TO_PHYS(m);
			*pdpe = mphys | X86_PG_RW | X86_PG_V;
		}
	} else {
		MPASS((*pdpe & X86_PG_PS) == 0);
		mphys = *pdpe & ~PAGE_MASK;
	}

	pde = (pd_entry_t *)PHYS_TO_DMAP(mphys);
	pd_idx = pmap_pde_index(va);
	pde += pd_idx;
	return (pde);
}

static pt_entry_t *
pmap_pti_pte(vm_offset_t va, bool *unwire_pde)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_page_t m;
	vm_paddr_t mphys;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);

	pde = pmap_pti_pde(va);
	if (unwire_pde != NULL) {
		*unwire_pde = true;
		pmap_pti_wire_pte(pde);
	}
	if (*pde == 0) {
		m = pmap_pti_alloc_page();
		if (*pde != 0) {
			pmap_pti_free_page(m);
			MPASS((*pde & X86_PG_PS) == 0);
			mphys = *pde & ~(PAGE_MASK | pg_nx);
		} else {
			mphys = VM_PAGE_TO_PHYS(m);
			*pde = mphys | X86_PG_RW | X86_PG_V;
			if (unwire_pde != NULL)
				*unwire_pde = false;
		}
	} else {
		MPASS((*pde & X86_PG_PS) == 0);
		mphys = *pde & ~(PAGE_MASK | pg_nx);
	}

	pte = (pt_entry_t *)PHYS_TO_DMAP(mphys);
	pte += pmap_pte_index(va);

	return (pte);
}

static void
pmap_pti_add_kva_locked(vm_offset_t sva, vm_offset_t eva, bool exec)
{
	vm_paddr_t pa;
	pd_entry_t *pde;
	pt_entry_t *pte, ptev;
	bool unwire_pde;

	VM_OBJECT_ASSERT_WLOCKED(pti_obj);

	sva = trunc_page(sva);
	MPASS(sva > VM_MAXUSER_ADDRESS);
	eva = round_page(eva);
	MPASS(sva < eva);
	for (; sva < eva; sva += PAGE_SIZE) {
		pte = pmap_pti_pte(sva, &unwire_pde);
		pa = pmap_kextract(sva);
		ptev = pa | X86_PG_RW | X86_PG_V | X86_PG_A | X86_PG_G |
		    (exec ? 0 : pg_nx) | pmap_cache_bits(kernel_pmap,
		    VM_MEMATTR_DEFAULT, false);
		if (*pte == 0) {
			pte_store(pte, ptev);
			pmap_pti_wire_pte(pte);
		} else {
			KASSERT(!pti_finalized,
			    ("pti overlap after fin %#lx %#lx %#lx",
			    sva, *pte, ptev));
			KASSERT(*pte == ptev,
			    ("pti non-identical pte after fin %#lx %#lx %#lx",
			    sva, *pte, ptev));
		}
		if (unwire_pde) {
			pde = pmap_pti_pde(sva);
			pmap_pti_unwire_pde(pde, true);
		}
	}
}

void
pmap_pti_add_kva(vm_offset_t sva, vm_offset_t eva, bool exec)
{

	if (!pti)
		return;
	VM_OBJECT_WLOCK(pti_obj);
	pmap_pti_add_kva_locked(sva, eva, exec);
	VM_OBJECT_WUNLOCK(pti_obj);
}

void
pmap_pti_remove_kva(vm_offset_t sva, vm_offset_t eva)
{
	pt_entry_t *pte;
	vm_offset_t va;

	if (!pti)
		return;
	sva = rounddown2(sva, PAGE_SIZE);
	MPASS(sva > VM_MAXUSER_ADDRESS);
	eva = roundup2(eva, PAGE_SIZE);
	MPASS(sva < eva);
	VM_OBJECT_WLOCK(pti_obj);
	for (va = sva; va < eva; va += PAGE_SIZE) {
		pte = pmap_pti_pte(va, NULL);
		KASSERT((*pte & X86_PG_V) != 0,
		    ("invalid pte va %#lx pte %#lx pt %#lx", va,
		    (u_long)pte, *pte));
		pte_clear(pte);
		pmap_pti_unwire_pte(pte, va);
	}
	pmap_invalidate_range(kernel_pmap, sva, eva);
	VM_OBJECT_WUNLOCK(pti_obj);
}

static void *
pkru_dup_range(void *ctx __unused, void *data)
{
	struct pmap_pkru_range *node, *new_node;

	new_node = uma_zalloc(pmap_pkru_ranges_zone, M_NOWAIT);
	if (new_node == NULL)
		return (NULL);
	node = data;
	memcpy(new_node, node, sizeof(*node));
	return (new_node);
}

static void
pkru_free_range(void *ctx __unused, void *node)
{

	uma_zfree(pmap_pkru_ranges_zone, node);
}

static int
pmap_pkru_assign(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, u_int keyidx,
    int flags)
{
	struct pmap_pkru_range *ppr;
	int error;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	MPASS(pmap->pm_type == PT_X86);
	MPASS((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0);
	if ((flags & AMD64_PKRU_EXCL) != 0 &&
	    !rangeset_check_empty(&pmap->pm_pkru, sva, eva))
		return (EBUSY);
	ppr = uma_zalloc(pmap_pkru_ranges_zone, M_NOWAIT);
	if (ppr == NULL)
		return (ENOMEM);
	ppr->pkru_keyidx = keyidx;
	ppr->pkru_flags = flags & AMD64_PKRU_PERSIST;
	error = rangeset_insert(&pmap->pm_pkru, sva, eva, ppr);
	if (error != 0)
		uma_zfree(pmap_pkru_ranges_zone, ppr);
	return (error);
}

static int
pmap_pkru_deassign(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	MPASS(pmap->pm_type == PT_X86);
	MPASS((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0);
	return (rangeset_remove(&pmap->pm_pkru, sva, eva));
}

static void
pmap_pkru_deassign_all(pmap_t pmap)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (pmap->pm_type == PT_X86 &&
	    (cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0)
		rangeset_remove_all(&pmap->pm_pkru);
}

static bool
pmap_pkru_same(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	struct pmap_pkru_range *ppr, *prev_ppr;
	vm_offset_t va;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (pmap->pm_type != PT_X86 ||
	    (cpu_stdext_feature2 & CPUID_STDEXT2_PKU) == 0 ||
	    sva >= VM_MAXUSER_ADDRESS)
		return (true);
	MPASS(eva <= VM_MAXUSER_ADDRESS);
	for (va = sva; va < eva; prev_ppr = ppr) {
		ppr = rangeset_lookup(&pmap->pm_pkru, va);
		if (va == sva)
			prev_ppr = ppr;
		else if ((ppr == NULL) ^ (prev_ppr == NULL))
			return (false);
		if (ppr == NULL) {
			va += PAGE_SIZE;
			continue;
		}
		if (prev_ppr->pkru_keyidx != ppr->pkru_keyidx)
			return (false);
		va = ppr->pkru_rs_el.re_end;
	}
	return (true);
}

static pt_entry_t
pmap_pkru_get(pmap_t pmap, vm_offset_t va)
{
	struct pmap_pkru_range *ppr;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (pmap->pm_type != PT_X86 ||
	    (cpu_stdext_feature2 & CPUID_STDEXT2_PKU) == 0 ||
	    va >= VM_MAXUSER_ADDRESS)
		return (0);
	ppr = rangeset_lookup(&pmap->pm_pkru, va);
	if (ppr != NULL)
		return (X86_PG_PKU(ppr->pkru_keyidx));
	return (0);
}

static bool
pred_pkru_on_remove(void *ctx __unused, void *r)
{
	struct pmap_pkru_range *ppr;

	ppr = r;
	return ((ppr->pkru_flags & AMD64_PKRU_PERSIST) == 0);
}

static void
pmap_pkru_on_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (pmap->pm_type == PT_X86 &&
	    (cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0) {
		rangeset_remove_pred(&pmap->pm_pkru, sva, eva,
		    pred_pkru_on_remove);
	}
}

static int
pmap_pkru_copy(pmap_t dst_pmap, pmap_t src_pmap)
{

	PMAP_LOCK_ASSERT(dst_pmap, MA_OWNED);
	PMAP_LOCK_ASSERT(src_pmap, MA_OWNED);
	MPASS(dst_pmap->pm_type == PT_X86);
	MPASS(src_pmap->pm_type == PT_X86);
	MPASS((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) != 0);
	if (src_pmap->pm_pkru.rs_data_ctx == NULL)
		return (0);
	return (rangeset_copy(&dst_pmap->pm_pkru, &src_pmap->pm_pkru));
}

static void
pmap_pkru_update_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
    u_int keyidx)
{
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t newpde, ptpaddr, *pde;
	pt_entry_t newpte, *ptep, pte;
	vm_offset_t va, va_next;
	bool changed;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	MPASS(pmap->pm_type == PT_X86);
	MPASS(keyidx <= PMAP_MAX_PKRU_IDX);

	for (changed = false, va = sva; va < eva; va = va_next) {
		pml4e = pmap_pml4e(pmap, va);
		if (pml4e == NULL || (*pml4e & X86_PG_V) == 0) {
			va_next = (va + NBPML4) & ~PML4MASK;
			if (va_next < va)
				va_next = eva;
			continue;
		}

		pdpe = pmap_pml4e_to_pdpe(pml4e, va);
		if ((*pdpe & X86_PG_V) == 0) {
			va_next = (va + NBPDP) & ~PDPMASK;
			if (va_next < va)
				va_next = eva;
			continue;
		}

		va_next = (va + NBPDR) & ~PDRMASK;
		if (va_next < va)
			va_next = eva;

		pde = pmap_pdpe_to_pde(pdpe, va);
		ptpaddr = *pde;
		if (ptpaddr == 0)
			continue;

		MPASS((ptpaddr & X86_PG_V) != 0);
		if ((ptpaddr & PG_PS) != 0) {
			if (va + NBPDR == va_next && eva >= va_next) {
				newpde = (ptpaddr & ~X86_PG_PKU_MASK) |
				    X86_PG_PKU(keyidx);
				if (newpde != ptpaddr) {
					*pde = newpde;
					changed = true;
				}
				continue;
			} else if (!pmap_demote_pde(pmap, pde, va)) {
				continue;
			}
		}

		if (va_next > eva)
			va_next = eva;

		for (ptep = pmap_pde_to_pte(pde, va); va != va_next;
		    ptep++, va += PAGE_SIZE) {
			pte = *ptep;
			if ((pte & X86_PG_V) == 0)
				continue;
			newpte = (pte & ~X86_PG_PKU_MASK) | X86_PG_PKU(keyidx);
			if (newpte != pte) {
				*ptep = newpte;
				changed = true;
			}
		}
	}
	if (changed)
		pmap_invalidate_range(pmap, sva, eva);
}

static int
pmap_pkru_check_uargs(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
    u_int keyidx, int flags)
{

	if (pmap->pm_type != PT_X86 || keyidx > PMAP_MAX_PKRU_IDX ||
	    (flags & ~(AMD64_PKRU_PERSIST | AMD64_PKRU_EXCL)) != 0)
		return (EINVAL);
	if (eva <= sva || eva > VM_MAXUSER_ADDRESS)
		return (EFAULT);
	if ((cpu_stdext_feature2 & CPUID_STDEXT2_PKU) == 0)
		return (ENOTSUP);
	return (0);
}

int
pmap_pkru_set(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, u_int keyidx,
    int flags)
{
	int error;

	sva = trunc_page(sva);
	eva = round_page(eva);
	error = pmap_pkru_check_uargs(pmap, sva, eva, keyidx, flags);
	if (error != 0)
		return (error);
	for (;;) {
		PMAP_LOCK(pmap);
		error = pmap_pkru_assign(pmap, sva, eva, keyidx, flags);
		if (error == 0)
			pmap_pkru_update_range(pmap, sva, eva, keyidx);
		PMAP_UNLOCK(pmap);
		if (error != ENOMEM)
			break;
		vm_wait(NULL);
	}
	return (error);
}

int
pmap_pkru_clear(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	int error;

	sva = trunc_page(sva);
	eva = round_page(eva);
	error = pmap_pkru_check_uargs(pmap, sva, eva, 0, 0);
	if (error != 0)
		return (error);
	for (;;) {
		PMAP_LOCK(pmap);
		error = pmap_pkru_deassign(pmap, sva, eva);
		if (error == 0)
			pmap_pkru_update_range(pmap, sva, eva, 0);
		PMAP_UNLOCK(pmap);
		if (error != ENOMEM)
			break;
		vm_wait(NULL);
	}
	return (error);
}

#if defined(KASAN) || defined(KMSAN)

/*
 * Reserve enough memory to:
 * 1) allocate PDP pages for the shadow map(s),
 * 2) shadow the boot stack of KSTACK_PAGES pages,
 * so we need one PD page, one or two PT pages, and KSTACK_PAGES shadow pages
 * per shadow map.
 */
#ifdef KASAN
#define	SAN_EARLY_PAGES	\
	(NKASANPML4E + 1 + 2 + howmany(KSTACK_PAGES, KASAN_SHADOW_SCALE))
#else
#define	SAN_EARLY_PAGES	\
	(NKMSANSHADPML4E + NKMSANORIGPML4E + 2 * (1 + 2 + KSTACK_PAGES))
#endif

static uint64_t __nosanitizeaddress __nosanitizememory
pmap_san_enter_early_alloc_4k(uint64_t pabase)
{
	static uint8_t data[PAGE_SIZE * SAN_EARLY_PAGES] __aligned(PAGE_SIZE);
	static size_t offset = 0;
	uint64_t pa;

	if (offset == sizeof(data)) {
		panic("%s: ran out of memory for the bootstrap shadow map",
		    __func__);
	}

	pa = pabase + ((vm_offset_t)&data[offset] - KERNSTART);
	offset += PAGE_SIZE;
	return (pa);
}

/*
 * Map a shadow page, before the kernel has bootstrapped its page tables.  This
 * is currently only used to shadow the temporary boot stack set up by locore.
 */
static void __nosanitizeaddress __nosanitizememory
pmap_san_enter_early(vm_offset_t va)
{
	static bool first = true;
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	uint64_t cr3, pa, base;
	int i;

	base = amd64_loadaddr();
	cr3 = rcr3();

	if (first) {
		/*
		 * If this the first call, we need to allocate new PML4Es for
		 * the bootstrap shadow map(s).  We don't know how the PML4 page
		 * was initialized by the boot loader, so we can't simply test
		 * whether the shadow map's PML4Es are zero.
		 */
		first = false;
#ifdef KASAN
		for (i = 0; i < NKASANPML4E; i++) {
			pa = pmap_san_enter_early_alloc_4k(base);

			pml4e = (pml4_entry_t *)cr3 +
			    pmap_pml4e_index(KASAN_MIN_ADDRESS + i * NBPML4);
			*pml4e = (pml4_entry_t)(pa | X86_PG_RW | X86_PG_V);
		}
#else
		for (i = 0; i < NKMSANORIGPML4E; i++) {
			pa = pmap_san_enter_early_alloc_4k(base);

			pml4e = (pml4_entry_t *)cr3 +
			    pmap_pml4e_index(KMSAN_ORIG_MIN_ADDRESS +
			    i * NBPML4);
			*pml4e = (pml4_entry_t)(pa | X86_PG_RW | X86_PG_V);
		}
		for (i = 0; i < NKMSANSHADPML4E; i++) {
			pa = pmap_san_enter_early_alloc_4k(base);

			pml4e = (pml4_entry_t *)cr3 +
			    pmap_pml4e_index(KMSAN_SHAD_MIN_ADDRESS +
			    i * NBPML4);
			*pml4e = (pml4_entry_t)(pa | X86_PG_RW | X86_PG_V);
		}
#endif
	}
	pml4e = (pml4_entry_t *)cr3 + pmap_pml4e_index(va);
	pdpe = (pdp_entry_t *)(*pml4e & PG_FRAME) + pmap_pdpe_index(va);
	if (*pdpe == 0) {
		pa = pmap_san_enter_early_alloc_4k(base);
		*pdpe = (pdp_entry_t)(pa | X86_PG_RW | X86_PG_V);
	}
	pde = (pd_entry_t *)(*pdpe & PG_FRAME) + pmap_pde_index(va);
	if (*pde == 0) {
		pa = pmap_san_enter_early_alloc_4k(base);
		*pde = (pd_entry_t)(pa | X86_PG_RW | X86_PG_V);
	}
	pte = (pt_entry_t *)(*pde & PG_FRAME) + pmap_pte_index(va);
	if (*pte != 0)
		panic("%s: PTE for %#lx is already initialized", __func__, va);
	pa = pmap_san_enter_early_alloc_4k(base);
	*pte = (pt_entry_t)(pa | X86_PG_A | X86_PG_M | X86_PG_RW | X86_PG_V);
}

static vm_page_t
pmap_san_enter_alloc_4k(void)
{
	vm_page_t m;

	m = vm_page_alloc_noobj(VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED |
	    VM_ALLOC_ZERO);
	if (m == NULL)
		panic("%s: no memory to grow shadow map", __func__);
	return (m);
}

static vm_page_t
pmap_san_enter_alloc_2m(void)
{
	return (vm_page_alloc_noobj_contig(VM_ALLOC_WIRED | VM_ALLOC_ZERO,
	    NPTEPG, 0, ~0ul, NBPDR, 0, VM_MEMATTR_DEFAULT));
}

/*
 * Grow a shadow map by at least one 4KB page at the specified address.  Use 2MB
 * pages when possible.
 */
void __nosanitizeaddress __nosanitizememory
pmap_san_enter(vm_offset_t va)
{
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_page_t m;

	if (kernphys == 0) {
		/*
		 * We're creating a temporary shadow map for the boot stack.
		 */
		pmap_san_enter_early(va);
		return;
	}

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);

	pdpe = pmap_pdpe(kernel_pmap, va);
	if ((*pdpe & X86_PG_V) == 0) {
		m = pmap_san_enter_alloc_4k();
		*pdpe = (pdp_entry_t)(VM_PAGE_TO_PHYS(m) | X86_PG_RW |
		    X86_PG_V | pg_nx);
	}
	pde = pmap_pdpe_to_pde(pdpe, va);
	if ((*pde & X86_PG_V) == 0) {
		m = pmap_san_enter_alloc_2m();
		if (m != NULL) {
			*pde = (pd_entry_t)(VM_PAGE_TO_PHYS(m) | X86_PG_RW |
			    X86_PG_PS | X86_PG_V | X86_PG_A | X86_PG_M | pg_nx);
		} else {
			m = pmap_san_enter_alloc_4k();
			*pde = (pd_entry_t)(VM_PAGE_TO_PHYS(m) | X86_PG_RW |
			    X86_PG_V | pg_nx);
		}
	}
	if ((*pde & X86_PG_PS) != 0)
		return;
	pte = pmap_pde_to_pte(pde, va);
	if ((*pte & X86_PG_V) != 0)
		return;
	m = pmap_san_enter_alloc_4k();
	*pte = (pt_entry_t)(VM_PAGE_TO_PHYS(m) | X86_PG_RW | X86_PG_V |
	    X86_PG_M | X86_PG_A | pg_nx);
}
#endif

/*
 * Track a range of the kernel's virtual address space that is contiguous
 * in various mapping attributes.
 */
struct pmap_kernel_map_range {
	vm_offset_t sva;
	pt_entry_t attrs;
	int ptes;
	int pdes;
	int pdpes;
};

static void
sysctl_kmaps_dump(struct sbuf *sb, struct pmap_kernel_map_range *range,
    vm_offset_t eva)
{
	const char *mode;
	int i, pat_idx;

	if (eva <= range->sva)
		return;

	pat_idx = pmap_pat_index(kernel_pmap, range->attrs, true);
	for (i = 0; i < PAT_INDEX_SIZE; i++)
		if (pat_index[i] == pat_idx)
			break;

	switch (i) {
	case PAT_WRITE_BACK:
		mode = "WB";
		break;
	case PAT_WRITE_THROUGH:
		mode = "WT";
		break;
	case PAT_UNCACHEABLE:
		mode = "UC";
		break;
	case PAT_UNCACHED:
		mode = "U-";
		break;
	case PAT_WRITE_PROTECTED:
		mode = "WP";
		break;
	case PAT_WRITE_COMBINING:
		mode = "WC";
		break;
	default:
		printf("%s: unknown PAT mode %#x for range 0x%016lx-0x%016lx\n",
		    __func__, pat_idx, range->sva, eva);
		mode = "??";
		break;
	}

	sbuf_printf(sb, "0x%016lx-0x%016lx r%c%c%c%c %s %d %d %d\n",
	    range->sva, eva,
	    (range->attrs & X86_PG_RW) != 0 ? 'w' : '-',
	    (range->attrs & pg_nx) != 0 ? '-' : 'x',
	    (range->attrs & X86_PG_U) != 0 ? 'u' : 's',
	    (range->attrs & X86_PG_G) != 0 ? 'g' : '-',
	    mode, range->pdpes, range->pdes, range->ptes);

	/* Reset to sentinel value. */
	range->sva = la57 ? KV5ADDR(NPML5EPG - 1, NPML4EPG - 1, NPDPEPG - 1,
	    NPDEPG - 1, NPTEPG - 1) : KV4ADDR(NPML4EPG - 1, NPDPEPG - 1,
	    NPDEPG - 1, NPTEPG - 1);
}

/*
 * Determine whether the attributes specified by a page table entry match those
 * being tracked by the current range.  This is not quite as simple as a direct
 * flag comparison since some PAT modes have multiple representations.
 */
static bool
sysctl_kmaps_match(struct pmap_kernel_map_range *range, pt_entry_t attrs)
{
	pt_entry_t diff, mask;

	mask = X86_PG_G | X86_PG_RW | X86_PG_U | X86_PG_PDE_CACHE | pg_nx;
	diff = (range->attrs ^ attrs) & mask;
	if (diff == 0)
		return (true);
	if ((diff & ~X86_PG_PDE_PAT) == 0 &&
	    pmap_pat_index(kernel_pmap, range->attrs, true) ==
	    pmap_pat_index(kernel_pmap, attrs, true))
		return (true);
	return (false);
}

static void
sysctl_kmaps_reinit(struct pmap_kernel_map_range *range, vm_offset_t va,
    pt_entry_t attrs)
{

	memset(range, 0, sizeof(*range));
	range->sva = va;
	range->attrs = attrs;
}

/*
 * Given a leaf PTE, derive the mapping's attributes.  If they do not match
 * those of the current run, dump the address range and its attributes, and
 * begin a new run.
 */
static void
sysctl_kmaps_check(struct sbuf *sb, struct pmap_kernel_map_range *range,
    vm_offset_t va, pml4_entry_t pml4e, pdp_entry_t pdpe, pd_entry_t pde,
    pt_entry_t pte)
{
	pt_entry_t attrs;

	attrs = pml4e & (X86_PG_RW | X86_PG_U | pg_nx);

	attrs |= pdpe & pg_nx;
	attrs &= pg_nx | (pdpe & (X86_PG_RW | X86_PG_U));
	if ((pdpe & PG_PS) != 0) {
		attrs |= pdpe & (X86_PG_G | X86_PG_PDE_CACHE);
	} else if (pde != 0) {
		attrs |= pde & pg_nx;
		attrs &= pg_nx | (pde & (X86_PG_RW | X86_PG_U));
	}
	if ((pde & PG_PS) != 0) {
		attrs |= pde & (X86_PG_G | X86_PG_PDE_CACHE);
	} else if (pte != 0) {
		attrs |= pte & pg_nx;
		attrs &= pg_nx | (pte & (X86_PG_RW | X86_PG_U));
		attrs |= pte & (X86_PG_G | X86_PG_PTE_CACHE);

		/* Canonicalize by always using the PDE PAT bit. */
		if ((attrs & X86_PG_PTE_PAT) != 0)
			attrs ^= X86_PG_PDE_PAT | X86_PG_PTE_PAT;
	}

	if (range->sva > va || !sysctl_kmaps_match(range, attrs)) {
		sysctl_kmaps_dump(sb, range, va);
		sysctl_kmaps_reinit(range, va, attrs);
	}
}

static int
sysctl_kmaps(SYSCTL_HANDLER_ARGS)
{
	struct pmap_kernel_map_range range;
	struct sbuf sbuf, *sb;
	pml4_entry_t pml4e;
	pdp_entry_t *pdp, pdpe;
	pd_entry_t *pd, pde;
	pt_entry_t *pt, pte;
	vm_offset_t sva;
	vm_paddr_t pa;
	int error, i, j, k, l;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sb = &sbuf;
	sbuf_new_for_sysctl(sb, NULL, PAGE_SIZE, req);

	/* Sentinel value. */
	range.sva = la57 ? KV5ADDR(NPML5EPG - 1, NPML4EPG - 1, NPDPEPG - 1,
	    NPDEPG - 1, NPTEPG - 1) : KV4ADDR(NPML4EPG - 1, NPDPEPG - 1,
	    NPDEPG - 1, NPTEPG - 1);

	/*
	 * Iterate over the kernel page tables without holding the kernel pmap
	 * lock.  Outside of the large map, kernel page table pages are never
	 * freed, so at worst we will observe inconsistencies in the output.
	 * Within the large map, ensure that PDP and PD page addresses are
	 * valid before descending.
	 */
	for (sva = 0, i = pmap_pml4e_index(sva); i < NPML4EPG; i++) {
		switch (i) {
		case PML4PML4I:
			sbuf_printf(sb, "\nRecursive map:\n");
			break;
		case DMPML4I:
			sbuf_printf(sb, "\nDirect map:\n");
			break;
#ifdef KASAN
		case KASANPML4I:
			sbuf_printf(sb, "\nKASAN shadow map:\n");
			break;
#endif
#ifdef KMSAN
		case KMSANSHADPML4I:
			sbuf_printf(sb, "\nKMSAN shadow map:\n");
			break;
		case KMSANORIGPML4I:
			sbuf_printf(sb, "\nKMSAN origin map:\n");
			break;
#endif
		case KPML4BASE:
			sbuf_printf(sb, "\nKernel map:\n");
			break;
		case LMSPML4I:
			sbuf_printf(sb, "\nLarge map:\n");
			break;
		}

		/* Convert to canonical form. */
		if (sva == 1ul << 47)
			sva |= -1ul << 48;

restart:
		pml4e = kernel_pml4[i];
		if ((pml4e & X86_PG_V) == 0) {
			sva = rounddown2(sva, NBPML4);
			sysctl_kmaps_dump(sb, &range, sva);
			sva += NBPML4;
			continue;
		}
		pa = pml4e & PG_FRAME;
		pdp = (pdp_entry_t *)PHYS_TO_DMAP(pa);

		for (j = pmap_pdpe_index(sva); j < NPDPEPG; j++) {
			pdpe = pdp[j];
			if ((pdpe & X86_PG_V) == 0) {
				sva = rounddown2(sva, NBPDP);
				sysctl_kmaps_dump(sb, &range, sva);
				sva += NBPDP;
				continue;
			}
			pa = pdpe & PG_FRAME;
			if ((pdpe & PG_PS) != 0) {
				sva = rounddown2(sva, NBPDP);
				sysctl_kmaps_check(sb, &range, sva, pml4e, pdpe,
				    0, 0);
				range.pdpes++;
				sva += NBPDP;
				continue;
			}
			if (PMAP_ADDRESS_IN_LARGEMAP(sva) &&
			    vm_phys_paddr_to_vm_page(pa) == NULL) {
				/*
				 * Page table pages for the large map may be
				 * freed.  Validate the next-level address
				 * before descending.
				 */
				goto restart;
			}
			pd = (pd_entry_t *)PHYS_TO_DMAP(pa);

			for (k = pmap_pde_index(sva); k < NPDEPG; k++) {
				pde = pd[k];
				if ((pde & X86_PG_V) == 0) {
					sva = rounddown2(sva, NBPDR);
					sysctl_kmaps_dump(sb, &range, sva);
					sva += NBPDR;
					continue;
				}
				pa = pde & PG_FRAME;
				if ((pde & PG_PS) != 0) {
					sva = rounddown2(sva, NBPDR);
					sysctl_kmaps_check(sb, &range, sva,
					    pml4e, pdpe, pde, 0);
					range.pdes++;
					sva += NBPDR;
					continue;
				}
				if (PMAP_ADDRESS_IN_LARGEMAP(sva) &&
				    vm_phys_paddr_to_vm_page(pa) == NULL) {
					/*
					 * Page table pages for the large map
					 * may be freed.  Validate the
					 * next-level address before descending.
					 */
					goto restart;
				}
				pt = (pt_entry_t *)PHYS_TO_DMAP(pa);

				for (l = pmap_pte_index(sva); l < NPTEPG; l++,
				    sva += PAGE_SIZE) {
					pte = pt[l];
					if ((pte & X86_PG_V) == 0) {
						sysctl_kmaps_dump(sb, &range,
						    sva);
						continue;
					}
					sysctl_kmaps_check(sb, &range, sva,
					    pml4e, pdpe, pde, pte);
					range.ptes++;
				}
			}
		}
	}

	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}
SYSCTL_OID(_vm_pmap, OID_AUTO, kernel_maps,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE | CTLFLAG_SKIP,
    NULL, 0, sysctl_kmaps, "A",
    "Dump kernel address layout");

#ifdef DDB
DB_SHOW_COMMAND(pte, pmap_print_pte)
{
	pmap_t pmap;
	pml5_entry_t *pml5;
	pml4_entry_t *pml4;
	pdp_entry_t *pdp;
	pd_entry_t *pde;
	pt_entry_t *pte, PG_V;
	vm_offset_t va;

	if (!have_addr) {
		db_printf("show pte addr\n");
		return;
	}
	va = (vm_offset_t)addr;

	if (kdb_thread != NULL)
		pmap = vmspace_pmap(kdb_thread->td_proc->p_vmspace);
	else
		pmap = PCPU_GET(curpmap);

	PG_V = pmap_valid_bit(pmap);
	db_printf("VA 0x%016lx", va);

	if (pmap_is_la57(pmap)) {
		pml5 = pmap_pml5e(pmap, va);
		db_printf(" pml5e 0x%016lx", *pml5);
		if ((*pml5 & PG_V) == 0) {
			db_printf("\n");
			return;
		}
		pml4 = pmap_pml5e_to_pml4e(pml5, va);
	} else {
		pml4 = pmap_pml4e(pmap, va);
	}
	db_printf(" pml4e 0x%016lx", *pml4);
	if ((*pml4 & PG_V) == 0) {
		db_printf("\n");
		return;
	}
	pdp = pmap_pml4e_to_pdpe(pml4, va);
	db_printf(" pdpe 0x%016lx", *pdp);
	if ((*pdp & PG_V) == 0 || (*pdp & PG_PS) != 0) {
		db_printf("\n");
		return;
	}
	pde = pmap_pdpe_to_pde(pdp, va);
	db_printf(" pde 0x%016lx", *pde);
	if ((*pde & PG_V) == 0 || (*pde & PG_PS) != 0) {
		db_printf("\n");
		return;
	}
	pte = pmap_pde_to_pte(pde, va);
	db_printf(" pte 0x%016lx\n", *pte);
}

DB_SHOW_COMMAND(phys2dmap, pmap_phys2dmap)
{
	vm_paddr_t a;

	if (have_addr) {
		a = (vm_paddr_t)addr;
		db_printf("0x%jx\n", (uintmax_t)PHYS_TO_DMAP(a));
	} else {
		db_printf("show phys2dmap addr\n");
	}
}

static void
ptpages_show_page(int level, int idx, vm_page_t pg)
{
	db_printf("l %d i %d pg %p phys %#lx ref %x\n",
	    level, idx, pg, VM_PAGE_TO_PHYS(pg), pg->ref_count);
}

static void
ptpages_show_complain(int level, int idx, uint64_t pte)
{
	db_printf("l %d i %d pte %#lx\n", level, idx, pte);
}

static void
ptpages_show_pml4(vm_page_t pg4, int num_entries, uint64_t PG_V)
{
	vm_page_t pg3, pg2, pg1;
	pml4_entry_t *pml4;
	pdp_entry_t *pdp;
	pd_entry_t *pd;
	int i4, i3, i2;

	pml4 = (pml4_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pg4));
	for (i4 = 0; i4 < num_entries; i4++) {
		if ((pml4[i4] & PG_V) == 0)
			continue;
		pg3 = PHYS_TO_VM_PAGE(pml4[i4] & PG_FRAME);
		if (pg3 == NULL) {
			ptpages_show_complain(3, i4, pml4[i4]);
			continue;
		}
		ptpages_show_page(3, i4, pg3);
		pdp = (pdp_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pg3));
		for (i3 = 0; i3 < NPDPEPG; i3++) {
			if ((pdp[i3] & PG_V) == 0)
				continue;
			pg2 = PHYS_TO_VM_PAGE(pdp[i3] & PG_FRAME);
			if (pg3 == NULL) {
				ptpages_show_complain(2, i3, pdp[i3]);
				continue;
			}
			ptpages_show_page(2, i3, pg2);
			pd = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pg2));
			for (i2 = 0; i2 < NPDEPG; i2++) {
				if ((pd[i2] & PG_V) == 0)
					continue;
				pg1 = PHYS_TO_VM_PAGE(pd[i2] & PG_FRAME);
				if (pg1 == NULL) {
					ptpages_show_complain(1, i2, pd[i2]);
					continue;
				}
				ptpages_show_page(1, i2, pg1);
			}
		}
	}
}

DB_SHOW_COMMAND(ptpages, pmap_ptpages)
{
	pmap_t pmap;
	vm_page_t pg;
	pml5_entry_t *pml5;
	uint64_t PG_V;
	int i5;

	if (have_addr)
		pmap = (pmap_t)addr;
	else
		pmap = PCPU_GET(curpmap);

	PG_V = pmap_valid_bit(pmap);

	if (pmap_is_la57(pmap)) {
		pml5 = pmap->pm_pmltop;
		for (i5 = 0; i5 < NUPML5E; i5++) {
			if ((pml5[i5] & PG_V) == 0)
				continue;
			pg = PHYS_TO_VM_PAGE(pml5[i5] & PG_FRAME);
			if (pg == NULL) {
				ptpages_show_complain(4, i5, pml5[i5]);
				continue;
			}
			ptpages_show_page(4, i5, pg);
			ptpages_show_pml4(pg, NPML4EPG, PG_V);
		}
	} else {
		ptpages_show_pml4(PHYS_TO_VM_PAGE(DMAP_TO_PHYS(
		    (vm_offset_t)pmap->pm_pmltop)), NUP4ML4E, PG_V);
	}
}
#endif
