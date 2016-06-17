/*
 * PowerPC64 Segment Translation Support.
 *
 * Dave Engebretsen and Mike Corrigan {engebret|mikejc}@us.ibm.com
 *    Copyright (c) 2001 Dave Engebretsen
 * 
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/paca.h>
#include <asm/naca.h>
#include <asm/pmc.h>
#include <asm/cputable.h>

inline int make_ste(unsigned long stab, 
		    unsigned long esid, unsigned long vsid);
inline void make_slbe(unsigned long esid, unsigned long vsid,
		      int large);

/*
 * Build an entry for the base kernel segment and put it into
 * the segment table or SLB.  All other segment table or SLB
 * entries are faulted in.
 */
void stab_initialize(unsigned long stab)
{
	unsigned long esid, vsid; 

	esid = GET_ESID(KERNELBASE);
	vsid = get_kernel_vsid(esid << SID_SHIFT); 

	if (cur_cpu_spec->cpu_features & CPU_FTR_SLB) {
                /* Invalidate the entire SLB & all the ERATS */
                __asm__ __volatile__("isync" : : : "memory");
#ifndef CONFIG_PPC_ISERIES
                __asm__ __volatile__("slbmte  %0,%0"
                                      : : "r" (0) : "memory");
                __asm__ __volatile__("isync; slbia; isync":::"memory");
		make_slbe(esid, vsid, 0);
#else
                __asm__ __volatile__("isync; slbia; isync":::"memory");
#endif
	} else {
                __asm__ __volatile__("isync; slbia; isync":::"memory");
		make_ste(stab, esid, vsid);
	}
}

/*
 * Create a segment table entry for the given esid/vsid pair.
 */ 
inline int
make_ste(unsigned long stab, unsigned long esid, unsigned long vsid)
{
	unsigned long entry, group, old_esid, castout_entry, i;
	unsigned int global_entry;
	STE *ste, *castout_ste;
	unsigned char kp = 1;
	
#ifdef CONFIG_SHARED_MEMORY_ADDRESSING
	if(((esid >> SMALLOC_ESID_SHIFT) == 
	    (SMALLOC_START >> SMALLOC_EA_SHIFT)) && 
	   (current->thread.flags & PPC_FLAG_SHARED)) {
		kp = 0;
	}
#endif

	/* Search the primary group first. */
	global_entry = (esid & 0x1f) << 3;
	ste = (STE *)(stab | ((esid & 0x1f) << 7)); 

	/*
	 * Find an empty entry, if one exists.
	 */
	for(group = 0; group < 2; group++) {
		for(entry = 0; entry < 8; entry++, ste++) {
			if(!(ste->dw0.dw0.v)) {
				ste->dw1.dw1.vsid = vsid;
				/* Order VSID updte */
				__asm__ __volatile__ ("eieio" : : : "memory");
				ste->dw0.dw0.esid = esid;
				ste->dw0.dw0.v  = 1;
				ste->dw0.dw0.kp = kp;
				/* Order update     */
				__asm__ __volatile__ ("sync" : : : "memory"); 

				return(global_entry | entry);
			}
		}
		/* Now search the secondary group. */
		global_entry = ((~esid) & 0x1f) << 3;
		ste = (STE *)(stab | (((~esid) & 0x1f) << 7)); 
	}

	/*
	 * Could not find empty entry, pick one with a round robin selection.
	 * Search all entries in the two groups.  Note that the first time
	 * we get here, we start with entry 1 so the initializer
	 * can be common with the SLB castout code.
	 */

	/* This assumes we never castout when initializing the stab. */
	PMC_SW_PROCESSOR(stab_capacity_castouts); 

	castout_entry = get_paca()->xStab_data.next_round_robin;
	for(i = 0; i < 16; i++) {
		if(castout_entry < 8) {
			global_entry = (esid & 0x1f) << 3;
			ste = (STE *)(stab | ((esid & 0x1f) << 7)); 
			castout_ste = ste + castout_entry;
		} else {
			global_entry = ((~esid) & 0x1f) << 3;
			ste = (STE *)(stab | (((~esid) & 0x1f) << 7)); 
			castout_ste = ste + (castout_entry - 8);
		}

		if((((castout_ste->dw0.dw0.esid) >> 32) == 0) ||
		   (((castout_ste->dw0.dw0.esid) & 0xffffffff) > 0)) {
			/* Found an entry to castout.  It is either a user */
			/* region, or a secondary kernel segment.          */
			break;
		}

		castout_entry = (castout_entry + 1) & 0xf;
	}

	get_paca()->xStab_data.next_round_robin = (castout_entry + 1) & 0xf;

	/* Modify the old entry to the new value. */

	/* Force previous translations to complete. DRENG */
	__asm__ __volatile__ ("isync" : : : "memory" );

	castout_ste->dw0.dw0.v = 0;
	__asm__ __volatile__ ("sync" : : : "memory" );    /* Order update */
	castout_ste->dw1.dw1.vsid = vsid;
	__asm__ __volatile__ ("eieio" : : : "memory" );   /* Order update */
	old_esid = castout_ste->dw0.dw0.esid;
	castout_ste->dw0.dw0.esid = esid;
	castout_ste->dw0.dw0.v  = 1;
	castout_ste->dw0.dw0.kp = kp;
	__asm__ __volatile__ ("slbie  %0" : : "r" (old_esid << SID_SHIFT)); 
	/* Ensure completion of slbie */
	__asm__ __volatile__ ("sync" : : : "memory" );  

	return(global_entry | (castout_entry & 0x7));
}

/*
 * Create a segment buffer entry for the given esid/vsid pair.
 */ 
inline void make_slbe(unsigned long esid, unsigned long vsid, int large)
{
	unsigned long entry, castout_entry;
	union {
		unsigned long word0;
		slb_dword0    data;
	} esid_data;
	union {
		unsigned long word0;
		slb_dword1    data;
	} vsid_data;
	unsigned char kp = 1;
 	
#ifdef CONFIG_SHARED_MEMORY_ADDRESSING
	if(((esid >> SMALLOC_ESID_SHIFT) == 
	    (SMALLOC_START >> SMALLOC_EA_SHIFT)) && 
	   (current->thread.flags & PPC_FLAG_SHARED)) {
		kp = 0;
	}
#endif
	
	/*
	 * Find an empty entry, if one exists.
	 */
	for(entry = 0; entry < naca->slb_size; entry++) {
		__asm__ __volatile__("slbmfee  %0,%1" 
				     : "=r" (esid_data) : "r" (entry)); 
		if(!esid_data.data.v) {
			/* 
			 * Write the new SLB entry.
			 */
			vsid_data.word0 = 0;
			vsid_data.data.vsid = vsid;
			vsid_data.data.kp = kp;
			if (large)
				vsid_data.data.l = 1;

			esid_data.word0 = 0;
			esid_data.data.esid = esid;
			esid_data.data.v = 1;
			esid_data.data.index = entry;

			/* slbie not needed as no previous mapping existed. */
			/* Order update  */
			__asm__ __volatile__ ("isync" : : : "memory");
			__asm__ __volatile__ ("slbmte  %0,%1" 
					      : : "r" (vsid_data), 
					      "r" (esid_data)); 
			/* Order update  */
			__asm__ __volatile__ ("isync" : : : "memory");
			return;
		}
	}
	
	/*
	 * Could not find empty entry, pick one with a round robin selection.
	 */

	PMC_SW_PROCESSOR(stab_capacity_castouts); 

	/*
	 * Never cast out the segment for our own stack. Since we
	 * dont invalidate the ERAT we could have a valid translation
	 * for our stack during the first part of exception exit
	 * which gets invalidated due to a tlbie from another cpu at a
	 * non recoverable point (after setting srr0/1) - Anton
	 */

	castout_entry = get_paca()->xStab_data.next_round_robin;
	do {
		entry = castout_entry;
		castout_entry++;
		if (castout_entry >= naca->slb_size)
			castout_entry = 1;
		asm volatile("slbmfee  %0,%1" : "=r" (esid_data) : "r" (entry));
	} while (esid_data.data.esid == GET_ESID((unsigned long)_get_SP()));
	
	get_paca()->xStab_data.next_round_robin = castout_entry;

	/* We're executing this code on the interrupt stack, so the
	 * above code might pick the kernel stack segment as the victim.
	 *
	 * Because of this, we need to invalidate the old entry. We need
	 * to do this since it'll otherwise be in the ERAT and might come
	 * back and haunt us if it get's thrown out of there at the wrong
	 * time (i.e. similar to throwing out our own stack above).
	 */

	esid_data.data.v = 0;
	__asm__ __volatile__("slbie  %0" : : "r" (esid_data));

	/* 
	 * Write the new SLB entry.
	 */
	vsid_data.word0 = 0;
	vsid_data.data.vsid = vsid;
	vsid_data.data.kp = kp;
	if (large)
		vsid_data.data.l = 1;
	
	esid_data.word0 = 0;
	esid_data.data.esid = esid;
	esid_data.data.v = 1;
	esid_data.data.index = entry;
	
	__asm__ __volatile__ ("isync" : : : "memory");   /* Order update */
	__asm__ __volatile__ ("slbmte  %0,%1" 
			      : : "r" (vsid_data), "r" (esid_data)); 
	__asm__ __volatile__ ("isync" : : : "memory" );   /* Order update */
}

/*
 * Allocate a segment table entry for the given ea.
 */
int ste_allocate ( unsigned long ea, 
		   unsigned long trap) 
{
	unsigned long vsid, esid;
	int kernel_segment = 0;

	PMC_SW_PROCESSOR(stab_faults); 

	/* Check for invalid effective addresses. */
	if (!IS_VALID_EA(ea)) {
		return 1;
	}
	
	/* Kernel or user address? */
	if (REGION_ID(ea)) {
		kernel_segment = 1;
		vsid = get_kernel_vsid( ea );
	} else {
		struct mm_struct *mm = current->mm;
		if ( mm ) {
			vsid = get_vsid(mm->context, ea );
		} else {
			return 1;
		}
	}

#ifdef CONFIG_SHARED_MEMORY_ADDRESSING
	/* Shared segments might be mapped into a user task space,
	 * so we need to add them to the list of entries to flush
	 */
	if ((ea >> SMALLOC_EA_SHIFT) == (SMALLOC_START >> SMALLOC_EA_SHIFT)) {
		kernel_segment = 0;
	}
#endif

	esid = GET_ESID(ea);
	if (trap == 0x380 || trap == 0x480) {
#ifndef CONFIG_PPC_ISERIES
		if (REGION_ID(ea) == KERNEL_REGION_ID)
			make_slbe(esid, vsid, 1); 
		else
#endif
			make_slbe(esid, vsid, 0); 
	} else {
		unsigned char top_entry, stab_entry, *segments; 

		stab_entry = make_ste(get_paca()->xStab_data.virt, esid, vsid);
		PMC_SW_PROCESSOR_A(stab_entry_use, stab_entry & 0xf); 

		segments = get_paca()->xSegments;		
		top_entry = segments[0];
		if(!kernel_segment && top_entry < (STAB_CACHE_SIZE - 1)) {
			top_entry++;
			segments[top_entry] = stab_entry;
			if(top_entry == STAB_CACHE_SIZE - 1) top_entry = 0xff;
			segments[0] = top_entry;
		}
	}
	
	return(0); 
}
 
/* 
 * Flush all entries from the segment table of the current processor.
 * Kernel and Bolted entries are not removed as we cannot tolerate 
 * faults on those addresses.
 */

#define STAB_PRESSURE 0

void flush_stab(void)
{
	STE *stab = (STE *) get_paca()->xStab_data.virt;
	unsigned char *segments = get_paca()->xSegments;
	unsigned long flags, i;

	if (cur_cpu_spec->cpu_features & CPU_FTR_SLB) {
		unsigned long flags;

		PMC_SW_PROCESSOR(stab_invalidations); 

		__save_and_cli(flags);
		__asm__ __volatile__("isync; slbia; isync":::"memory");
		__restore_flags(flags);
	} else {
		unsigned long entry;
		STE *ste;

		/* Force previous translations to complete. DRENG */
		__asm__ __volatile__ ("isync" : : : "memory");

		__save_and_cli(flags);
		if(segments[0] != 0xff && !STAB_PRESSURE) {
			for(i = 1; i <= segments[0]; i++) {
				ste = stab + segments[i]; 
				ste->dw0.dw0.v = 0;
				PMC_SW_PROCESSOR(stab_invalidations); 
			}
		} else {
			/* Invalidate all entries. */
                        ste = stab;

		        /* Never flush the first entry. */ 
		        ste += 1;
			for(entry = 1;
			    entry < (PAGE_SIZE / sizeof(STE)); 
			    entry++, ste++) {
				unsigned long ea;
				ea = ste->dw0.dw0.esid << SID_SHIFT;
				if (STAB_PRESSURE || (!REGION_ID(ea)) ||
				    (REGION_ID(ea) == VMALLOC_REGION_ID)) {
					ste->dw0.dw0.v = 0;
					PMC_SW_PROCESSOR(stab_invalidations); 
				}
			}
		}

		*((unsigned long *)segments) = 0;
		__restore_flags(flags);

		/* Invalidate the SLB. */
		/* Force invals to complete. */
		__asm__ __volatile__ ("sync" : : : "memory");  
		/* Flush the SLB.            */
		__asm__ __volatile__ ("slbia" : : : "memory"); 
		/* Force flush to complete.  */
		__asm__ __volatile__ ("sync" : : : "memory");  
	}
}
