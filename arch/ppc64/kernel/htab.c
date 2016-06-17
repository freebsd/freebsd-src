/*
 * PowerPC64 port by Mike Corrigan and Dave Engebretsen
 *   {mikejc|engebret}@us.ibm.com
 *
 *    Copyright (c) 2000 Mike Corrigan <mikejc@us.ibm.com>
 *
 * SMP scalability work:
 *    Copyright (C) 2001 Anton Blanchard <anton@au.ibm.com>, IBM
 * 
 *    Module name: htab.c
 *
 *    Description:
 *      PowerPC Hashed Page Table functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/sysctl.h>
#include <linux/ctype.h>
#include <linux/cache.h>

#include <asm/ppcdebug.h>
#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/page.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <asm/naca.h>
#include <asm/pmc.h>
#include <asm/machdep.h>
#include <asm/lmb.h>
#include <asm/abs_addr.h>
#include <asm/io.h>
#include <asm/eeh.h>
#include <asm/hvcall.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCallHpt.h>
#include <asm/cputable.h>

/*
 * Note:  pte   --> Linux PTE
 *        HPTE  --> PowerPC Hashed Page Table Entry
 *
 * Execution context:
 *   htab_initialize is called with the MMU off (of course), but
 *   the kernel has been copied down to zero so it can directly
 *   reference global data.  At this point it is very difficult
 *   to print debug info.
 *
 */

HTAB htab_data = {NULL, 0, 0, 0, 0};

extern unsigned long _SDR1;
extern unsigned long klimit;

void make_pte(HPTE *htab, unsigned long va, unsigned long pa,
	      int mode, unsigned long hash_mask, int large);
long plpar_pte_enter(unsigned long flags,
		     unsigned long ptex,
		     unsigned long new_pteh, unsigned long new_ptel,
		     unsigned long *old_pteh_ret, unsigned long *old_ptel_ret);
static long hpte_remove(unsigned long hpte_group);
static long rpa_lpar_hpte_remove(unsigned long hpte_group);
static long iSeries_hpte_remove(unsigned long hpte_group);
inline unsigned long get_lock_slot(unsigned long vpn);

static spinlock_t pSeries_tlbie_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t pSeries_lpar_tlbie_lock = SPIN_LOCK_UNLOCKED;

#define LOCK_SPLIT
#ifdef LOCK_SPLIT
hash_table_lock_t hash_table_lock[128] __cacheline_aligned_in_smp = { [0 ... 31] = {SPIN_LOCK_UNLOCKED}};
#else
hash_table_lock_t hash_table_lock[1] __cacheline_aligned_in_smp = { [0] = {SPIN_LOCK_UNLOCKED}};
#endif

#define KB (1024)
#define MB (1024*KB)

static inline void
loop_forever(void)
{
	volatile unsigned long x = 1;
	for(;x;x|=1)
		;
}

static inline void
create_pte_mapping(unsigned long start, unsigned long end,
		   unsigned long mode, unsigned long mask, int large)
{
	unsigned long addr;
	HPTE *htab = (HPTE *)__v2a(htab_data.htab);
	unsigned int step;

	if (large)
		step = 16*MB;
	else
		step = 4*KB;

	for (addr = start; addr < end; addr += step) {
		unsigned long vsid = get_kernel_vsid(addr);
		unsigned long va = (vsid << 28) | (addr & 0xfffffff);
		make_pte(htab, va, (unsigned long)__v2a(addr), 
			 mode, mask, large);
	}
}

void
htab_initialize(void)
{
	unsigned long table, htab_size_bytes;
	unsigned long pteg_count;
	unsigned long mode_rw, mask, lock_shift;

#if 0
	/* Can't really do the call below since it calls the normal RTAS
	 * entry point and we're still relocate off at the moment.
	 * Temporarily diabling until it can call through the relocate off
	 * RTAS entry point.  -Peter
	 */
	ppc64_boot_msg(0x05, "htab init");
#endif
	/*
	 * Calculate the required size of the htab.  We want the number of
	 * PTEGs to equal one half the number of real pages.
	 */ 
	htab_size_bytes = 1UL << naca->pftSize;
	pteg_count = htab_size_bytes >> 7;

	/* For debug, make the HTAB 1/8 as big as it normally would be. */
	ifppcdebug(PPCDBG_HTABSIZE) {
		pteg_count >>= 3;
		htab_size_bytes = pteg_count << 7;
	}

	htab_data.htab_num_ptegs = pteg_count;
	htab_data.htab_hash_mask = pteg_count - 1;

	/* 
	 * Calculate the number of bits to shift the pteg selector such that we
	 * use the high order 8 bits to select a page table lock.
	 */
	asm ("cntlzd %0,%1" : "=r" (lock_shift) : 
	     "r" (htab_data.htab_hash_mask));
	htab_data.htab_lock_shift = (64 - lock_shift) - 8;

	if(systemcfg->platform == PLATFORM_PSERIES) {
		/* Find storage for the HPT.  Must be contiguous in
		 * the absolute address space.
		 */
		table = lmb_alloc(htab_size_bytes, htab_size_bytes);
		if ( !table ) {
			ppc64_terminate_msg(0x20, "hpt space");
			loop_forever();
		}
		htab_data.htab = (HPTE *)__a2v(table);

		/* htab absolute addr + encoded htabsize */
		_SDR1 = table + __ilog2(pteg_count) - 11;

		/* Initialize the HPT with no entries */
		memset((void *)table, 0, htab_size_bytes);
	} else {
		/* Using a hypervisor which owns the htab */
		htab_data.htab = NULL;
		_SDR1 = 0; 
	}

	mode_rw = _PAGE_ACCESSED | _PAGE_COHERENT | PP_RWXX;
	mask = pteg_count-1;

	/* XXX we currently map kernel text rw, should fix this */
	if ((systemcfg->platform & PLATFORM_PSERIES) &&
	    (cur_cpu_spec->cpu_features & 
	     CPU_FTR_16M_PAGE) &&
	    (systemcfg->physicalMemorySize > 256*MB)) {
		create_pte_mapping((unsigned long)KERNELBASE, 
				   KERNELBASE + 256*MB, mode_rw, mask, 0);
		create_pte_mapping((unsigned long)KERNELBASE + 256*MB, 
				   KERNELBASE + (systemcfg->physicalMemorySize), 
				   mode_rw, mask, 1);
	} else {
		create_pte_mapping((unsigned long)KERNELBASE, 
				   KERNELBASE+(systemcfg->physicalMemorySize), 
				   mode_rw, mask, 0);
	}
#if 0
	/* Can't really do the call below since it calls the normal RTAS
	 * entry point and we're still relocate off at the moment.
	 * Temporarily diabling until it can call through the relocate off
	 * RTAS entry point.  -Peter
	 */
	ppc64_boot_msg(0x06, "htab done");
#endif
}
#undef KB
#undef MB

/*
 * Create a pte. Used during initialization only.
 * We assume the PTE will fit in the primary PTEG.
 */
void make_pte(HPTE *htab, unsigned long va, unsigned long pa,
	      int mode, unsigned long hash_mask, int large)
{
	HPTE *hptep, local_hpte, rhpte;
	unsigned long hash, vpn, flags, lpar_rc;
	unsigned long i, dummy1, dummy2;
	long slot;

	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	hash = hpt_hash(vpn, large);

	local_hpte.dw1.dword1 = pa | mode;
	local_hpte.dw0.dword0 = 0;
	local_hpte.dw0.dw0.avpn = va >> 23;
	local_hpte.dw0.dw0.bolted = 1;		/* bolted */
	if (large) {
		local_hpte.dw0.dw0.l = 1;	/* large page */
		local_hpte.dw0.dw0.avpn &= ~0x1UL;
	}
	local_hpte.dw0.dw0.v = 1;

	if (systemcfg->platform == PLATFORM_PSERIES) {
		hptep  = htab + ((hash & hash_mask)*HPTES_PER_GROUP);

		for (i = 0; i < 8; ++i, ++hptep) {
			if (hptep->dw0.dw0.v == 0) {		/* !valid */
				*hptep = local_hpte;
				return;
			}
		}
	} else if (systemcfg->platform == PLATFORM_PSERIES_LPAR) {
		slot = ((hash & hash_mask)*HPTES_PER_GROUP);
		
		/* Set CEC cookie to 0                   */
		/* Zero page = 0                         */
		/* I-cache Invalidate = 0                */
		/* I-cache synchronize = 0               */
		/* Exact = 0 - modify any entry in group */
		flags = 0;
		
		lpar_rc =  plpar_pte_enter(flags, slot, local_hpte.dw0.dword0,
					   local_hpte.dw1.dword1, 
					   &dummy1, &dummy2);
		return;
	} else if (systemcfg->platform == PLATFORM_ISERIES_LPAR) {
		slot = HvCallHpt_findValid(&rhpte, vpn);
		if (slot < 0) {
			/* Must find space in primary group */
			panic("hash_page: hpte already exists\n");
		}
		HvCallHpt_addValidate(slot, 0, (HPTE *)&local_hpte );
		return;
	}

	/* We should _never_ get here and too early to call xmon. */
	ppc64_terminate_msg(0x22, "hpte platform");
	loop_forever();
}

/*
 * find_linux_pte returns the address of a linux pte for a given 
 * effective address and directory.  If not found, it returns zero.
 */
pte_t *find_linux_pte(pgd_t *pgdir, unsigned long ea)
{
	pgd_t *pg;
	pmd_t *pm;
	pte_t *pt = NULL;
	pte_t pte;

	pg = pgdir + pgd_index(ea);
	if (!pgd_none(*pg)) {
		pm = pmd_offset(pg, ea);
		if (!pmd_none(*pm)) { 
			pt = pte_offset(pm, ea);
			pte = *pt;
			if (!pte_present(pte))
				pt = NULL;
		}
	}

	return pt;
}

static inline unsigned long computeHptePP(unsigned long pte)
{
	return (pte & _PAGE_USER) |
		(((pte & _PAGE_USER) >> 1) &
		 ((~((pte >> 2) &	/* _PAGE_RW */
		     (pte >> 7))) &	/* _PAGE_DIRTY */
		  1));
}

/*
 * Handle a fault by adding an HPTE. If the address can't be determined
 * to be valid via Linux page tables, return 1. If handled return 0
 */
int __hash_page(unsigned long ea, unsigned long access, 
		unsigned long vsid, pte_t *ptep)
{
	unsigned long va, vpn;
	unsigned long newpp, prpn;
	unsigned long hpteflags, lock_slot;
	long slot;
	pte_t old_pte, new_pte;

	/* Search the Linux page table for a match with va */
	va = (vsid << 28) | (ea & 0x0fffffff);
	vpn = va >> PAGE_SHIFT;
	lock_slot = get_lock_slot(vpn); 

	/* Acquire the hash table lock to guarantee that the linux
	 * pte we fetch will not change
	 */
	spin_lock(&hash_table_lock[lock_slot].lock);
	
	/* 
	 * Check the user's access rights to the page.  If access should be
	 * prevented then send the problem up to do_page_fault.
	 */
#ifdef CONFIG_SHARED_MEMORY_ADDRESSING
	access |= _PAGE_PRESENT;
	if (unlikely(access & ~(pte_val(*ptep)))) {
		if(!(((ea >> SMALLOC_EA_SHIFT) == 
		      (SMALLOC_START >> SMALLOC_EA_SHIFT)) &&
		     ((current->thread.flags) & PPC_FLAG_SHARED))) {
			spin_unlock(&hash_table_lock[lock_slot].lock);
			return 1;
		}
	}
#else
	access |= _PAGE_PRESENT;
	if (unlikely(access & ~(pte_val(*ptep)))) {
		spin_unlock(&hash_table_lock[lock_slot].lock);
		return 1;
	}
#endif

	/* 
	 * We have found a pte (which was present).
	 * The spinlocks prevent this status from changing
	 * The hash_table_lock prevents the _PAGE_HASHPTE status
	 * from changing (RPN, DIRTY and ACCESSED too)
	 * The page_table_lock prevents the pte from being 
	 * invalidated or modified
	 */

	/*
	 * At this point, we have a pte (old_pte) which can be used to build
	 * or update an HPTE. There are 2 cases:
	 *
	 * 1. There is a valid (present) pte with no associated HPTE (this is 
	 *	the most common case)
	 * 2. There is a valid (present) pte with an associated HPTE. The
	 *	current values of the pp bits in the HPTE prevent access
	 *	because we are doing software DIRTY bit management and the
	 *	page is currently not DIRTY. 
	 */

	old_pte = *ptep;
	new_pte = old_pte;

	/* If the attempted access was a store */
	if (access & _PAGE_RW)
		pte_val(new_pte) |= _PAGE_ACCESSED | _PAGE_DIRTY;
	else
		pte_val(new_pte) |= _PAGE_ACCESSED;

	newpp = computeHptePP(pte_val(new_pte));
	
	/* Check if pte already has an hpte (case 2) */
	if (unlikely(pte_val(old_pte) & _PAGE_HASHPTE)) {
		/* There MIGHT be an HPTE for this pte */
		unsigned long hash, slot, secondary;

		/* XXX fix large pte flag */
		hash = hpt_hash(vpn, 0);
		secondary = (pte_val(old_pte) & _PAGE_SECONDARY) >> 15;
		if (secondary)
			hash = ~hash;
		slot = (hash & htab_data.htab_hash_mask) * HPTES_PER_GROUP;
		slot += (pte_val(old_pte) & _PAGE_GROUP_IX) >> 12;

		/* XXX fix large pte flag */
		if (ppc_md.hpte_updatepp(slot, secondary, 
					 newpp, va, 0) == -1) {
			pte_val(old_pte) &= ~_PAGE_HPTEFLAGS;
		} else {
			if (!pte_same(old_pte, new_pte)) {
				*ptep = new_pte;
			}
		}
	}

	if (likely(!(pte_val(old_pte) & _PAGE_HASHPTE))) {
		/* Update the linux pte with the HPTE slot */
		pte_val(new_pte) &= ~_PAGE_HPTEFLAGS;
		pte_val(new_pte) |= _PAGE_HASHPTE;
		prpn = pte_val(old_pte) >> PTE_SHIFT;

		/* copy appropriate flags from linux pte */
		hpteflags = (pte_val(new_pte) & 0x1f8) | newpp;

		slot = ppc_md.hpte_insert(vpn, prpn, hpteflags, 0, 0);

		pte_val(new_pte) |= ((slot<<12) & 
				     (_PAGE_GROUP_IX | _PAGE_SECONDARY));

		*ptep = new_pte;
	}

	spin_unlock(&hash_table_lock[lock_slot].lock);

	return 0;
}

/*
 * Handle a fault by adding an HPTE. If the address can't be determined
 * to be valid via Linux page tables, return 1. If handled return 0
 */
int hash_page(unsigned long ea, unsigned long access)
{
	void *pgdir;
	unsigned long vsid;
	struct mm_struct *mm;
	pte_t *ptep;
	int ret;

	/* Check for invalid addresses. */
	if (!IS_VALID_EA(ea)) return 1;

 	switch (REGION_ID(ea)) {
	case USER_REGION_ID:
		mm = current->mm;
		if (mm == NULL) return 1;
		vsid = get_vsid(mm->context, ea);
		break;
	case IO_REGION_ID:
		mm = &ioremap_mm;
		vsid = get_kernel_vsid(ea);
		break;
	case VMALLOC_REGION_ID:
		mm = &init_mm;
		vsid = get_kernel_vsid(ea);
#ifdef CONFIG_SHARED_MEMORY_ADDRESSING
                /*
                 * Check if this is a user task with shared access to kernel
                 * data & we got a protection fault.  If it is, the kernel
                 * must have faulted in the segment and the protection flags
                 * on the segment are kernel access only.  Just flush the
                 * segment table & fault in the segment with the right flags.
                 */
                if(((current->thread.flags) & PPC_FLAG_SHARED) &&
                   (access & _PAGE_USER)) {
                        flush_stab();
                }
#endif
		break;
	case EEH_REGION_ID:
		/*
		 * Should only be hit if there is an access to MMIO space
		 * which is protected by EEH.
		 * Send the problem up to do_page_fault 
		 */
	case KERNEL_REGION_ID:
		/*
		 * Should never get here - entire 0xC0... region is bolted.
		 * Send the problem up to do_page_fault 
		 */
	default:
		/* Not a valid range
		 * Send the problem up to do_page_fault 
		 */
		return 1;
		break;
	}

	pgdir = mm->pgd;
	if (pgdir == NULL) return 1;

	/*
	 * Lock the Linux page table to prevent mmap and kswapd
	 * from modifying entries while we search and update
	 */
	spin_lock(&mm->page_table_lock);

	ptep = find_linux_pte(pgdir, ea);
	/*
	 * If no pte found or not present, send the problem up to
	 * do_page_fault
	 */
	if (ptep && pte_present(*ptep)) {
		ret = __hash_page(ea, access, vsid, ptep);
	} else {	
		/* If no pte, send the problem up to do_page_fault */
		ret = 1;
	}

	spin_unlock(&mm->page_table_lock);

	return ret;
}

void flush_hash_page(unsigned long context, unsigned long ea, pte_t *ptep)
{
	unsigned long vsid, vpn, va, hash, secondary, slot, flags, lock_slot;
	unsigned long large = 0, local = 0;
	pte_t pte;

	if ((ea >= USER_START) && (ea <= USER_END))
		vsid = get_vsid(context, ea);
	else
		vsid = get_kernel_vsid(ea);

	va = (vsid << 28) | (ea & 0x0fffffff);
	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	lock_slot = get_lock_slot(vpn); 
	hash = hpt_hash(vpn, large);

	spin_lock_irqsave(&hash_table_lock[lock_slot].lock, flags);

	pte = __pte(pte_update(ptep, _PAGE_HPTEFLAGS, 0));
	secondary = (pte_val(pte) & _PAGE_SECONDARY) >> 15;
	if (secondary) hash = ~hash;
	slot = (hash & htab_data.htab_hash_mask) * HPTES_PER_GROUP;
	slot += (pte_val(pte) & _PAGE_GROUP_IX) >> 12;
	
	if (pte_val(pte) & _PAGE_HASHPTE) {
		ppc_md.hpte_invalidate(slot, secondary, va, large, local);
	}

	spin_unlock_irqrestore(&hash_table_lock[lock_slot].lock, flags);
}

long plpar_pte_enter(unsigned long flags,
		     unsigned long ptex,
		     unsigned long new_pteh, unsigned long new_ptel,
		     unsigned long *old_pteh_ret, unsigned long *old_ptel_ret)
{
	unsigned long dummy, ret;
	ret = plpar_hcall(H_ENTER, flags, ptex, new_pteh, new_ptel,
			   old_pteh_ret, old_ptel_ret, &dummy);
	return(ret);
}

long plpar_pte_remove(unsigned long flags,
		      unsigned long ptex,
		      unsigned long avpn,
		      unsigned long *old_pteh_ret, unsigned long *old_ptel_ret)
{
	unsigned long dummy;
	return plpar_hcall(H_REMOVE, flags, ptex, avpn, 0,
			   old_pteh_ret, old_ptel_ret, &dummy);
}

long plpar_pte_read(unsigned long flags,
		    unsigned long ptex,
		    unsigned long *old_pteh_ret, unsigned long *old_ptel_ret)
{
	unsigned long dummy;
	return plpar_hcall(H_READ, flags, ptex, 0, 0,
			   old_pteh_ret, old_ptel_ret, &dummy);
}

long plpar_pte_protect(unsigned long flags,
		       unsigned long ptex,
		       unsigned long avpn)
{
	return plpar_hcall_norets(H_PROTECT, flags, ptex, avpn);
}

static __inline__ void set_pp_bit(unsigned long pp, HPTE *addr)
{
	unsigned long old;
	unsigned long *p = &addr->dw1.dword1;

	__asm__ __volatile__(
        "1:	ldarx	%0,0,%3\n\
                rldimi  %0,%2,0,62\n\
                stdcx.	%0,0,%3\n\
            	bne	1b"
        : "=&r" (old), "=m" (*p)
        : "r" (pp), "r" (p), "m" (*p)
        : "cc");
}

/*
 * Calculate which hash_table_lock to use, based on the pteg being used.
 *
 * Given a VPN, use the high order 8 bits to select one of 2^7 locks.  The
 * highest order bit is used to indicate primary vs. secondary group.  If the
 * secondary is selected, complement the lock select bits.  This results in
 * both the primary and secondary groups being covered under the same lock.
 */
inline unsigned long get_lock_slot(unsigned long vpn)
{
	unsigned long lock_slot;
#ifdef LOCK_SPLIT
	lock_slot = (hpt_hash(vpn,0) >> htab_data.htab_lock_shift) & 0xff;
	if(lock_slot & 0x80) lock_slot = (~lock_slot) & 0x7f;
#else
	lock_slot = 0;
#endif
	return(lock_slot);
}

/*
 * Functions used to retrieve word 0 of a given page table entry.
 *
 * Input : slot : PTE index within the page table of the entry to retrieve 
 * Output: Contents of word 0 of the specified entry
 */
static unsigned long rpa_lpar_hpte_getword0(unsigned long slot)
{
	unsigned long dword0;
	unsigned long lpar_rc;
	unsigned long dummy_word1;
	unsigned long flags;

	/* Read 1 pte at a time                        */
	/* Do not need RPN to logical page translation */
	/* No cross CEC PFT access                     */
	flags = 0;
	
	lpar_rc = plpar_pte_read(flags, slot, &dword0, &dummy_word1);

	if (lpar_rc != H_Success)
		panic("Error on pte read in get_hpte0 rc = %lx\n", lpar_rc);

	return dword0;
}

unsigned long iSeries_hpte_getword0(unsigned long slot)
{
	unsigned long dword0;

	HPTE hpte;
	HvCallHpt_get(&hpte, slot);
	dword0 = hpte.dw0.dword0;

	return dword0;
}

/*
 * Functions used to find the PTE for a particular virtual address. 
 * Only used during boot when bolting pages.
 *
 * Input : vpn      : virtual page number
 * Output: PTE index within the page table of the entry
 *         -1 on failure
 */
static long hpte_find(unsigned long vpn)
{
	HPTE *hptep;
	unsigned long hash;
	unsigned long i, j;
	long slot;
	Hpte_dword0 dw0;

	hash = hpt_hash(vpn, 0);

	for (j = 0; j < 2; j++) {
		slot = (hash & htab_data.htab_hash_mask) * HPTES_PER_GROUP;
		for (i = 0; i < HPTES_PER_GROUP; i++) {
			hptep = htab_data.htab + slot;
			dw0 = hptep->dw0.dw0;

			if ((dw0.avpn == (vpn >> 11)) && dw0.v &&
			    (dw0.h == j)) {
				/* HPTE matches */
				if (j)
					slot = -slot;
				return slot;
			}
			++slot;
		}
		hash = ~hash;
	}

	return -1;
}

static long rpa_lpar_hpte_find(unsigned long vpn)
{
	unsigned long hash;
	unsigned long i, j;
	long slot;
	union {
		unsigned long dword0;
		Hpte_dword0 dw0;
	} hpte_dw0;
	Hpte_dword0 dw0;

	hash = hpt_hash(vpn, 0);

	for (j = 0; j < 2; j++) {
		slot = (hash & htab_data.htab_hash_mask) * HPTES_PER_GROUP;
		for (i = 0; i < HPTES_PER_GROUP; i++) {
			hpte_dw0.dword0 = rpa_lpar_hpte_getword0(slot);
			dw0 = hpte_dw0.dw0;

			if ((dw0.avpn == (vpn >> 11)) && dw0.v &&
			    (dw0.h == j)) {
				/* HPTE matches */
				if (j)
					slot = -slot;
				return slot;
			}
			++slot;
		}
		hash = ~hash;
	}

	return -1;
} 

static long iSeries_hpte_find(unsigned long vpn)
{
	HPTE hpte;
	long slot;

	/*
	 * The HvCallHpt_findValid interface is as follows:
	 * 0xffffffffffffffff : No entry found.
	 * 0x00000000xxxxxxxx : Entry found in primary group, slot x
	 * 0x80000000xxxxxxxx : Entry found in secondary group, slot x
	 */
	slot = HvCallHpt_findValid(&hpte, vpn); 
	if (hpte.dw0.dw0.v) {
		if (slot < 0) {
			slot &= 0x7fffffffffffffff;
			slot = -slot;
		}
	} else {
		slot = -1;
	}

	return slot;
}

/*
 * Functions used to invalidate a page table entry from the page table
 * and tlb.
 *
 * Input : slot  : PTE index within the page table of the entry to invalidated
 *         va    : Virtual address of the entry being invalidated
 *         large : 1 = large page (16M)
 *         local : 1 = Use tlbiel to only invalidate the local tlb 
 */
static void hpte_invalidate(unsigned long slot, 
			    unsigned long secondary,
			    unsigned long va,
			    int large, int local)
{
	HPTE *hptep = htab_data.htab + slot;
	Hpte_dword0 dw0;
	unsigned long vpn, avpn;
	unsigned long flags;

	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	avpn = vpn >> 11;

	dw0 = hptep->dw0.dw0;

	/*
	 * Do not remove bolted entries.  Alternatively, we could check
	 * the AVPN, hash group, and valid bits.  By doing it this way,
	 * it is common with the pSeries LPAR optimal path.
	 */
	if (dw0.bolted) return;

	/* Invalidate the hpte. */
	hptep->dw0.dword0 = 0;

	/* Invalidate the tlb */
	spin_lock_irqsave(&pSeries_tlbie_lock, flags);
	_tlbie(va, large);
	spin_unlock_irqrestore(&pSeries_tlbie_lock, flags);
}

static void rpa_lpar_hpte_invalidate(unsigned long slot, 
				     unsigned long secondary,
				     unsigned long va,
				     int large, int local)
{
	unsigned long lpar_rc;
	unsigned long dummy1, dummy2;

	/* 
	 * Don't remove a bolted entry.  This case can occur when we bolt
	 * pages dynamically after initial boot.
	 */
	lpar_rc = plpar_pte_remove(H_ANDCOND, slot, (0x1UL << 4), 
				   &dummy1, &dummy2);

	if (lpar_rc != H_Success)
		panic("Bad return code from invalidate rc = %lx\n", lpar_rc);
}

static void iSeries_hpte_invalidate(unsigned long slot, 
				    unsigned long secondary,
				    unsigned long va,
				    int large, int local)
{
	HPTE lhpte;
	unsigned long vpn, avpn;

	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	avpn = vpn >> 11;

	lhpte.dw0.dword0 = iSeries_hpte_getword0(slot);
	
	if ((lhpte.dw0.dw0.avpn == avpn) && 
	    (lhpte.dw0.dw0.v) &&
	    (lhpte.dw0.dw0.h == secondary)) {
		HvCallHpt_invalidateSetSwBitsGet(slot, 0, 0);
	}
}

/*
 * Functions used to update page protection bits.
 *
 * Input : slot  : PTE index within the page table of the entry to update
 *         newpp : new page protection bits
 *         va    : Virtual address of the entry being updated
 *         large : 1 = large page (16M)
 * Output: 0 on success, -1 on failure
 */
static long hpte_updatepp(unsigned long slot, 
			  unsigned long secondary,
			  unsigned long newpp,
			  unsigned long va, int large)
{
	HPTE *hptep = htab_data.htab + slot;
	Hpte_dword0 dw0;
	Hpte_dword1 dw1;
	unsigned long vpn, avpn;
	unsigned long flags;

	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	avpn = vpn >> 11;

	dw0 = hptep->dw0.dw0;
	if ((dw0.avpn == avpn) && 
	    (dw0.v) && (dw0.h == secondary)) {
		/* Turn off valid bit in HPTE */
		dw0.v = 0;
		hptep->dw0.dw0 = dw0;
		
		/* Ensure it is out of the tlb too */
		spin_lock_irqsave(&pSeries_tlbie_lock, flags);
		_tlbie(va, large);
		spin_unlock_irqrestore(&pSeries_tlbie_lock, flags);
		
		/* Insert the new pp bits into the HPTE */
		dw1 = hptep->dw1.dw1;
		dw1.pp = newpp;
		hptep->dw1.dw1 = dw1;
		
		/* Ensure it is visible before validating */
		__asm__ __volatile__ ("eieio" : : : "memory");
		
		/* Turn the valid bit back on in HPTE */
		dw0.v = 1;
		hptep->dw0.dw0 = dw0;
		
		__asm__ __volatile__ ("ptesync" : : : "memory");
		
		return 0;
	}

	return -1;
}

static long rpa_lpar_hpte_updatepp(unsigned long slot, 
				   unsigned long secondary,
				   unsigned long newpp,
				   unsigned long va, int large)
{
	unsigned long lpar_rc;
	unsigned long flags = (newpp & 7);
	unsigned long avpn = va >> 23;
	HPTE hpte;

	lpar_rc = plpar_pte_read(0, slot, &hpte.dw0.dword0, &hpte.dw1.dword1);

	if ((hpte.dw0.dw0.avpn == avpn) &&
	    (hpte.dw0.dw0.v) && 
	    (hpte.dw0.dw0.h == secondary)) {
		lpar_rc = plpar_pte_protect(flags, slot, 0);
		if (lpar_rc != H_Success)
			panic("bad return code from pte protect rc = %lx\n", 
			      lpar_rc);
		return 0;
	}

	return -1;
}

static long iSeries_hpte_updatepp(unsigned long slot, 
				  unsigned long secondary,
				  unsigned long newpp, 
				  unsigned long va, int large)
{
	unsigned long vpn, avpn;
	HPTE hpte;

	if (large)
		vpn = va >> LARGE_PAGE_SHIFT;
	else
		vpn = va >> PAGE_SHIFT;

	avpn = vpn >> 11;

	HvCallHpt_get(&hpte, slot);
	if ((hpte.dw0.dw0.avpn == avpn) && 
	    (hpte.dw0.dw0.v) &&
	    (hpte.dw0.dw0.h == secondary)) {
		HvCallHpt_setPp(slot, newpp);
		return 0;
	}
	return -1;
}

/*
 * Functions used to update the page protection bits. Intended to be used 
 * to create guard pages for kernel data structures on pages which are bolted
 * in the HPT. Assumes pages being operated on will not be stolen.
 * Does not work on large pages. No need to lock here because we are the 
 * only user.
 * 
 * Input : newpp : page protection flags
 *         ea    : effective kernel address to bolt.
 */
static void hpte_updateboltedpp(unsigned long newpp, unsigned long ea)
{
	unsigned long vsid, va, vpn, flags;
	long slot;
	HPTE *hptep;

	vsid = get_kernel_vsid(ea);
	va = (vsid << 28) | (ea & 0x0fffffff);
	vpn = va >> PAGE_SHIFT;

	slot = hpte_find(vpn);
	if (slot == -1)
		panic("could not find page to bolt\n");
	hptep = htab_data.htab + slot;

	set_pp_bit(newpp, hptep);

	/* Ensure it is out of the tlb too */
	spin_lock_irqsave(&pSeries_tlbie_lock, flags);
	_tlbie(va, 0);
	spin_unlock_irqrestore(&pSeries_tlbie_lock, flags);
}

static void rpa_lpar_hpte_updateboltedpp(unsigned long newpp, unsigned long ea)
{
	unsigned long lpar_rc;
	unsigned long vsid, va, vpn, flags;
	long slot;

	vsid = get_kernel_vsid(ea);
	va = (vsid << 28) | (ea & 0x0fffffff);
	vpn = va >> PAGE_SHIFT;

	slot = rpa_lpar_hpte_find(vpn);
	if (slot == -1)
		panic("updateboltedpp: Could not find page to bolt\n");

	flags = newpp & 3;
	lpar_rc = plpar_pte_protect(flags, slot, 0);

	if (lpar_rc != H_Success)
		panic("Bad return code from pte bolted protect rc = %lx\n",
		      lpar_rc); 
}

void iSeries_hpte_updateboltedpp(unsigned long newpp, unsigned long ea)
{
	unsigned long vsid,va,vpn;
	long slot;

	vsid = get_kernel_vsid( ea );
	va = ( vsid << 28 ) | ( ea & 0x0fffffff );
	vpn = va >> PAGE_SHIFT;

	slot = iSeries_hpte_find(vpn); 
	if (slot == -1)
		panic("updateboltedpp: Could not find page to bolt\n");

	HvCallHpt_setPp(slot, newpp);
}

/*
 * Functions used to insert new hardware page table entries.
 * Will castout non-bolted entries as necessary using a random
 * algorithm.
 *
 * Input : vpn      : virtual page number
 *         prpn     : real page number in absolute space
 *         hpteflags: page protection flags
 *         bolted   : 1 = bolt the page
 *         large    : 1 = large page (16M)
 * Output: hsss, where h = hash group, sss = slot within that group
 */
static long hpte_insert(unsigned long vpn, unsigned long prpn,
			unsigned long hpteflags, int bolted, int large)
{
	HPTE *hptep;
	Hpte_dword0 dw0;
	HPTE lhpte;
	int i, secondary;
	unsigned long hash = hpt_hash(vpn, 0);
	unsigned long avpn = vpn >> 11;
	unsigned long arpn = physRpn_to_absRpn(prpn);
	unsigned long hpte_group;

repeat:
	secondary = 0;
	hpte_group = ((hash & htab_data.htab_hash_mask) *
		      HPTES_PER_GROUP) & ~0x7UL;
	hptep = htab_data.htab + hpte_group;

	for (i = 0; i < HPTES_PER_GROUP; i++) {
		dw0 = hptep->dw0.dw0;
		if (!dw0.v) {
			/* retry with lock held */
			dw0 = hptep->dw0.dw0;
			if (!dw0.v)
				break;
		}
		hptep++;
	}

	if (i == HPTES_PER_GROUP) {
		secondary = 1;
		hpte_group = ((~hash & htab_data.htab_hash_mask) *
			      HPTES_PER_GROUP) & ~0x7UL;
		hptep = htab_data.htab + hpte_group;

		for (i = 0; i < HPTES_PER_GROUP; i++) {
			dw0 = hptep->dw0.dw0;
			if (!dw0.v) {
				/* retry with lock held */
				dw0 = hptep->dw0.dw0;
				if (!dw0.v)
					break;
			}
			hptep++;
		}
		if (i == HPTES_PER_GROUP) {
			if (mftb() & 0x1)
				hpte_group=((hash & htab_data.htab_hash_mask)* 
					    HPTES_PER_GROUP) & ~0x7UL;
			
			hpte_remove(hpte_group);
			goto repeat;
		}
	}

	lhpte.dw1.dword1      = 0;
	lhpte.dw1.dw1.rpn     = arpn;
	lhpte.dw1.flags.flags = hpteflags;

	lhpte.dw0.dword0      = 0;
	lhpte.dw0.dw0.avpn    = avpn;
	lhpte.dw0.dw0.h       = secondary;
	lhpte.dw0.dw0.bolted  = bolted;
	lhpte.dw0.dw0.v       = 1;

	if (large) lhpte.dw0.dw0.l = 1;

	hptep->dw1.dword1 = lhpte.dw1.dword1;

	/* Guarantee the second dword is visible before the valid bit */
	__asm__ __volatile__ ("eieio" : : : "memory");

	/*
	 * Now set the first dword including the valid bit
	 * NOTE: this also unlocks the hpte
	 */
	hptep->dw0.dword0 = lhpte.dw0.dword0;

	__asm__ __volatile__ ("ptesync" : : : "memory");

	return ((secondary << 3) | i);
}

static long rpa_lpar_hpte_insert(unsigned long vpn, unsigned long prpn,
				 unsigned long hpteflags,
				 int bolted, int large)
{
	/* XXX fix for large page */
	unsigned long lpar_rc;
	unsigned long flags;
	unsigned long slot;
	HPTE lhpte;
	int secondary;
	unsigned long hash = hpt_hash(vpn, 0);
	unsigned long avpn = vpn >> 11;
	unsigned long arpn = physRpn_to_absRpn(prpn);
	unsigned long hpte_group;

	/* Fill in the local HPTE with absolute rpn, avpn and flags */
	lhpte.dw1.dword1      = 0;
	lhpte.dw1.dw1.rpn     = arpn;
	lhpte.dw1.flags.flags = hpteflags;

	lhpte.dw0.dword0      = 0;
	lhpte.dw0.dw0.avpn    = avpn;
	lhpte.dw0.dw0.bolted  = bolted;
	lhpte.dw0.dw0.v       = 1;

	if (large) lhpte.dw0.dw0.l = 1;

	/* Now fill in the actual HPTE */
	/* Set CEC cookie to 0         */
	/* Large page = 0              */
	/* Zero page = 0               */
	/* I-cache Invalidate = 0      */
	/* I-cache synchronize = 0     */
	/* Exact = 0                   */
	flags = 0;

	/* XXX why is this here? - Anton */
	/*   -- Because at one point we hit a case where non cachable
	 *      pages where marked coherent & this is rejected by the HV.
	 *      Perhaps it is no longer an issue ... DRENG.
	 */ 
	if (hpteflags & (_PAGE_GUARDED|_PAGE_NO_CACHE))
		lhpte.dw1.flags.flags &= ~_PAGE_COHERENT;

repeat:
	secondary = 0;
	lhpte.dw0.dw0.h = secondary;
	hpte_group = ((hash & htab_data.htab_hash_mask) *
		      HPTES_PER_GROUP) & ~0x7UL;

	__asm__ __volatile__ (
		H_ENTER_r3
		"mr    4, %2\n"
                "mr    5, %3\n"
                "mr    6, %4\n"
                "mr    7, %5\n"
		HVSC
                "mr    %0, 3\n"
                "mr    %1, 4\n"
		: "=r" (lpar_rc), "=r" (slot)
		: "r" (flags), "r" (hpte_group), "r" (lhpte.dw0.dword0),
		"r" (lhpte.dw1.dword1)
		: "r0", "r3", "r4", "r5", "r6", "r7", 
		  "r8", "r9", "r10", "r11", "r12", "cc");

	if (lpar_rc == H_PTEG_Full) {
		secondary = 1;
		lhpte.dw0.dw0.h = secondary;
		hpte_group = ((~hash & htab_data.htab_hash_mask) *
			      HPTES_PER_GROUP) & ~0x7UL;

		__asm__ __volatile__ (
			      H_ENTER_r3
			      "mr    4, %2\n"
			      "mr    5, %3\n"
			      "mr    6, %4\n"
			      "mr    7, %5\n"
			      HVSC
			      "mr    %0, 3\n"
			      "mr    %1, 4\n"
			      : "=r" (lpar_rc), "=r" (slot)
			      : "r" (flags), "r" (hpte_group), "r" (lhpte.dw0.dword0),
			      "r" (lhpte.dw1.dword1)
			      : "r0", "r3", "r4", "r5", "r6", "r7",
			        "r8", "r9", "r10", "r11", "r12", "cc");
		if (lpar_rc == H_PTEG_Full) {
			if (mftb() & 0x1)
				hpte_group=((hash & htab_data.htab_hash_mask)* 
					    HPTES_PER_GROUP) & ~0x7UL;
			
			rpa_lpar_hpte_remove(hpte_group);
			goto repeat;
		}
	}

	if (lpar_rc != H_Success)
		panic("Bad return code from pte enter rc = %lx\n", lpar_rc);

	return ((secondary << 3) | (slot & 0x7));
}

static long iSeries_hpte_insert(unsigned long vpn, unsigned long prpn,
				unsigned long hpteflags,
				int bolted, int large)
{
	HPTE lhpte;
	unsigned long hash, hpte_group;
	unsigned long avpn = vpn >> 11;
	unsigned long arpn = physRpn_to_absRpn( prpn );
	int secondary = 0;
	long slot;

	hash = hpt_hash(vpn, 0);

repeat:
	slot = HvCallHpt_findValid(&lhpte, vpn);
	if (lhpte.dw0.dw0.v) {
		panic("select_hpte_slot found entry already valid\n");
	}

	if (slot == -1) { /* No available entry found in either group */
		if (mftb() & 0x1) {
			hpte_group=((hash & htab_data.htab_hash_mask)* 
				    HPTES_PER_GROUP) & ~0x7UL;
		} else {
			hpte_group=((~hash & htab_data.htab_hash_mask)* 
				    HPTES_PER_GROUP) & ~0x7UL;
		}

		hash = hpt_hash(vpn, 0);
		iSeries_hpte_remove(hpte_group);
		goto repeat;
	} else if (slot < 0) {
		slot &= 0x7fffffffffffffff;
		secondary = 1;
	}

	/* Create the HPTE */
	lhpte.dw1.dword1      = 0;
	lhpte.dw1.dw1.rpn     = arpn;
	lhpte.dw1.flags.flags = hpteflags;

	lhpte.dw0.dword0     = 0;
	lhpte.dw0.dw0.avpn   = avpn;
	lhpte.dw0.dw0.h      = secondary;
	lhpte.dw0.dw0.bolted = bolted;
	lhpte.dw0.dw0.v      = 1;

	/* Now fill in the actual HPTE */
	HvCallHpt_addValidate(slot, secondary, (HPTE *)&lhpte);
	return ((secondary << 3) | (slot & 0x7));
}

/*
 * Functions used to remove hardware page table entries.
 *
 * Input : hpte_group: PTE index of the first entry in a group
 * Output: offset within the group of the entry removed or
 *         -1 on failure
 */
static long hpte_remove(unsigned long hpte_group)
{
	HPTE *hptep;
	Hpte_dword0 dw0;
	int i;
	int slot_offset;
	unsigned long vsid, group, pi, pi_high;
	unsigned long slot;
	unsigned long flags;
	int large;
	unsigned long va;

	/* pick a random slot to start at */
	slot_offset = mftb() & 0x7;

	for (i = 0; i < HPTES_PER_GROUP; i++) {
		hptep = htab_data.htab + hpte_group + slot_offset;
		dw0 = hptep->dw0.dw0;

		if (dw0.v && !dw0.bolted) {
			/* retry with lock held */
			dw0 = hptep->dw0.dw0;
			if (dw0.v && !dw0.bolted)
				break;
		}

		slot_offset++;
		slot_offset &= 0x7;
	}

	if (i == HPTES_PER_GROUP)
		return -1;

	large = dw0.l;

	/* Invalidate the hpte. NOTE: this also unlocks it */
	hptep->dw0.dword0 = 0;

	/* Invalidate the tlb */
	vsid = dw0.avpn >> 5;
	slot = hptep - htab_data.htab;
	group = slot >> 3;
	if (dw0.h)
		group = ~group;
	pi = (vsid ^ group) & 0x7ff;
	pi_high = (dw0.avpn & 0x1f) << 11;
	pi |= pi_high;

	if (large)
		va = pi << LARGE_PAGE_SHIFT;
	else
		va = pi << PAGE_SHIFT;

	spin_lock_irqsave(&pSeries_tlbie_lock, flags);
	_tlbie(va, large);
	spin_unlock_irqrestore(&pSeries_tlbie_lock, flags);

	return i;
}

static long rpa_lpar_hpte_remove(unsigned long hpte_group)
{
	unsigned long slot_offset;
	unsigned long lpar_rc;
	int i;
	unsigned long dummy1, dummy2;

	/* pick a random slot to start at */
	slot_offset = mftb() & 0x7;

	for (i = 0; i < HPTES_PER_GROUP; i++) {

		/* Don't remove a bolted entry */
		lpar_rc = plpar_pte_remove(H_ANDCOND, hpte_group + slot_offset,
					   (0x1UL << 4), &dummy1, &dummy2);

		if (lpar_rc == H_Success)
			return i;

		if (lpar_rc != H_Not_Found)
			panic("Bad return code from pte remove rc = %lx\n",
			      lpar_rc);

		slot_offset++;
		slot_offset &= 0x7;
	}

	return -1;
}

static long iSeries_hpte_remove(unsigned long hpte_group)
{
	unsigned long slot_offset;
	int i;
	HPTE lhpte;

	/* Pick a random slot to start at */
	slot_offset = mftb() & 0x7;

	for (i = 0; i < HPTES_PER_GROUP; i++) {
		lhpte.dw0.dword0 = 
			iSeries_hpte_getword0(hpte_group + slot_offset);

		if (!lhpte.dw0.dw0.bolted) {
			HvCallHpt_invalidateSetSwBitsGet(hpte_group + 
							 slot_offset, 0, 0);
			return i;
		}

		slot_offset++;
		slot_offset &= 0x7;
	}

	return -1;
}

void hpte_init_pSeries(void)
{
	ppc_md.hpte_invalidate     = hpte_invalidate;
	ppc_md.hpte_updatepp       = hpte_updatepp;
	ppc_md.hpte_updateboltedpp = hpte_updateboltedpp;
	ppc_md.hpte_insert	   = hpte_insert;
	ppc_md.hpte_remove	   = hpte_remove;
}

void pSeries_lpar_mm_init(void)
{
	ppc_md.hpte_invalidate     = rpa_lpar_hpte_invalidate;
	ppc_md.hpte_updatepp       = rpa_lpar_hpte_updatepp;
	ppc_md.hpte_updateboltedpp = rpa_lpar_hpte_updateboltedpp;
	ppc_md.hpte_insert         = rpa_lpar_hpte_insert;
	ppc_md.hpte_remove         = rpa_lpar_hpte_remove;
}

void hpte_init_iSeries(void)
{
	ppc_md.hpte_invalidate     = iSeries_hpte_invalidate;
	ppc_md.hpte_updatepp       = iSeries_hpte_updatepp;
	ppc_md.hpte_updateboltedpp = iSeries_hpte_updateboltedpp;
	ppc_md.hpte_insert         = iSeries_hpte_insert;
	ppc_md.hpte_remove         = iSeries_hpte_remove;
}

