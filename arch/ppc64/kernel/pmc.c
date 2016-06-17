/*
 * pmc.c
 * Copyright (C) 2001 Dave Engebretsen & Mike Corrigan IBM Corporation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Change Activity:
 * 2001/06/05 : engebret : Created.
 * 2002/04/11 : engebret : Add btmalloc code.
 * End Change Activity 
 */

#include <asm/proc_fs.h>
#include <asm/paca.h>
#include <asm/iSeries/ItLpPaca.h>
#include <asm/iSeries/ItLpQueue.h>
#include <asm/processor.h>

#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/pmc.h>
#include <asm/uaccess.h>
#include <asm/naca.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>
#include <asm/page.h>
#include <asm/machdep.h>
#include <asm/lmb.h>
#include <asm/abs_addr.h>
#include <asm/ppcdebug.h>

struct _pmc_sw pmc_sw_system = {
	0  
};

struct _pmc_sw pmc_sw_cpu[NR_CPUS] = {
	{0 }, 
};

/*
 * Provide enough storage for either system level counters or
 * one cpu's counters.
 */
struct _pmc_sw_text pmc_sw_text;
struct _pmc_hw_text pmc_hw_text;

extern pte_t *find_linux_pte( pgd_t * pgdir, unsigned long ea );
extern pgd_t *bolted_pgd;

static struct vm_struct *get_btm_area(unsigned long size, unsigned long flags);
static int local_free_bolted_pages(unsigned long ea, unsigned long num);

extern pgd_t bolted_dir[];
pgd_t *bolted_pgd  = (pgd_t *)&bolted_dir;

struct vm_struct *btmlist = NULL;
struct mm_struct btmalloc_mm = {pgd             : bolted_dir,
                                page_table_lock : SPIN_LOCK_UNLOCKED};

extern spinlock_t hash_table_lock[];

char *
ppc64_pmc_stab(int file)
{
	int  n;
	unsigned long stab_faults, stab_capacity_castouts, stab_invalidations; 
	unsigned long i;

	stab_faults = stab_capacity_castouts = stab_invalidations = n = 0;

	if (file == -1) {
		for (i = 0;  i < smp_num_cpus; i++) {
			stab_faults += pmc_sw_cpu[i].stab_faults;
			stab_capacity_castouts += pmc_sw_cpu[i].stab_capacity_castouts;
			stab_invalidations += pmc_sw_cpu[i].stab_invalidations;
		}
		n += sprintf(pmc_sw_text.buffer + n,    
			     "Faults         0x%lx\n", stab_faults); 
		n += sprintf(pmc_sw_text.buffer + n, 
			     "Castouts       0x%lx\n", stab_capacity_castouts); 
		n += sprintf(pmc_sw_text.buffer + n, 
			     "Invalidations  0x%lx\n", stab_invalidations); 
	} else {
		n += sprintf(pmc_sw_text.buffer + n,
			     "Faults         0x%lx\n", 
			     pmc_sw_cpu[file].stab_faults);
		
		n += sprintf(pmc_sw_text.buffer + n,   
			     "Castouts       0x%lx\n", 
			     pmc_sw_cpu[file].stab_capacity_castouts);
		
		n += sprintf(pmc_sw_text.buffer + n,   
			     "Invalidations  0x%lx\n", 
			     pmc_sw_cpu[file].stab_invalidations);

		for (i = 0; i < STAB_ENTRY_MAX; i++) {
			if (pmc_sw_cpu[file].stab_entry_use[i]) {
				n += sprintf(pmc_sw_text.buffer + n,   
					     "Entry %02ld       0x%lx\n", i, 
					     pmc_sw_cpu[file].stab_entry_use[i]);
			}
		}

	}

	return(pmc_sw_text.buffer); 
}

char *
ppc64_pmc_htab(int file)
{
	int  n;
	unsigned long htab_primary_overflows, htab_capacity_castouts;
	unsigned long htab_read_to_write_faults; 

	htab_primary_overflows = htab_capacity_castouts = 0;
	htab_read_to_write_faults = n = 0;

	if (file == -1) {
		n += sprintf(pmc_sw_text.buffer + n,    
			     "Primary Overflows  0x%lx\n", 
			     pmc_sw_system.htab_primary_overflows); 
		n += sprintf(pmc_sw_text.buffer + n, 
			     "Castouts           0x%lx\n", 
			     pmc_sw_system.htab_capacity_castouts); 
	} else {
		n += sprintf(pmc_sw_text.buffer + n,
			     "Primary Overflows  N/A\n");

		n += sprintf(pmc_sw_text.buffer + n,   
			     "Castouts           N/A\n\n");

	}
	
	return(pmc_sw_text.buffer); 
}

char *
ppc64_pmc_hw(int file)
{
	int  n;

	n = 0;
	if (file == -1) {
		n += sprintf(pmc_hw_text.buffer + n, "Not Implemented\n");
	} else {
		n += sprintf(pmc_hw_text.buffer + n,    
			     "MMCR0  0x%lx\n", mfspr(MMCR0)); 
		n += sprintf(pmc_hw_text.buffer + n, 
			     "MMCR1  0x%lx\n", mfspr(MMCR1)); 
#if 0
		n += sprintf(pmc_hw_text.buffer + n,    
			     "MMCRA  0x%lx\n", mfspr(MMCRA)); 
#endif

		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC1   0x%lx\n", mfspr(PMC1)); 
		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC2   0x%lx\n", mfspr(PMC2)); 
		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC3   0x%lx\n", mfspr(PMC3)); 
		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC4   0x%lx\n", mfspr(PMC4)); 
		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC5   0x%lx\n", mfspr(PMC5)); 
		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC6   0x%lx\n", mfspr(PMC6)); 
		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC7   0x%lx\n", mfspr(PMC7)); 
		n += sprintf(pmc_hw_text.buffer + n,    
			     "PMC8   0x%lx\n", mfspr(PMC8)); 
	}

	return(pmc_hw_text.buffer); 
}

/*
 * Manage allocations of storage which is bolted in the HPT and low fault
 * overhead in the segment tables. Intended to be used for buffers used 
 * to collect performance data.  
 *
 * Remaining Issues:
 *   - Power4 is not tested at all, 0xB regions will always be castout of slb
 *   - On Power3, 0xB00000000 esid is left in the stab for all time,
 *     other 0xB segments are castout, but not explicitly removed.
 *   - Error path checking is weak at best, wrong at worst.
 *
 * btmalloc - Allocate a buffer which is bolted in the HPT and (eventually)
 *            the segment table.
 *
 * Input : unsigned long size: bytes of storage to allocate.
 * Return: void * : pointer to the kernel address of the buffer.
 */
void* btmalloc (unsigned long size) {
	pgd_t *pgdp;
	pmd_t *pmdp;
	pte_t *ptep, pte;
	unsigned long ea_base, ea, hpteflags, lock_slot;
	struct vm_struct *area;
	unsigned long pa, pg_count, page, vsid, slot, va, rpn, vpn;
  
	size = PAGE_ALIGN(size);
	if (!size || (size >> PAGE_SHIFT) > num_physpages) return NULL;

	spin_lock(&btmalloc_mm.page_table_lock);

	/* Get a virtual address region in the bolted space */
	area = get_btm_area(size, 0);
	if (!area) {
		spin_unlock(&btmalloc_mm.page_table_lock);
		return NULL;
	}

	ea_base = (unsigned long) area->addr;
	pg_count = (size >> PAGE_SHIFT);

	/* Create a Linux page table entry and an HPTE for each page */
	for(page = 0; page < pg_count; page++) {
		pa = get_free_page(GFP_KERNEL) - PAGE_OFFSET; 
		ea = ea_base + (page * PAGE_SIZE);
		vsid = get_kernel_vsid(ea);
		va = ( vsid << 28 ) | ( ea & 0xfffffff );
		vpn = va >> PAGE_SHIFT;
		lock_slot = get_lock_slot(vpn); 
		rpn = pa >> PAGE_SHIFT;

		spin_lock(&hash_table_lock[lock_slot]);
		/* Get a pointer to the linux page table entry for this page
		 * allocating pmd or pte pages along the way as needed.  Note
		 * that the pmd & pte pages are not themselfs bolted.
		 */
		pgdp = pgd_offset_b(ea);
		pmdp = pmd_alloc(&btmalloc_mm, pgdp, ea);
		ptep = pte_alloc(&btmalloc_mm, pmdp, ea);
		pte = *ptep;

		/* Clear any old hpte and set the new linux pte */
		set_pte(ptep, mk_pte_phys(pa & PAGE_MASK, PAGE_KERNEL));

		hpteflags = _PAGE_ACCESSED|_PAGE_COHERENT|PP_RWXX;

		pte_val(pte) &= ~_PAGE_HPTEFLAGS;
		pte_val(pte) |= _PAGE_HASHPTE;

		slot = ppc_md.hpte_insert(vpn, rpn, hpteflags, 1, 0);  

		pte_val(pte) |= ((slot<<12) & 
				 (_PAGE_GROUP_IX | _PAGE_SECONDARY));

		*ptep = pte;

		spin_unlock(&hash_table_lock[lock_slot]);
	}

	spin_unlock(&btmalloc_mm.page_table_lock);
	return (void*)ea_base;
}

/*
 * Free a range of bolted pages that were allocated with btmalloc
 */
void btfree(void *ea) {
	struct vm_struct **p, *tmp;
	unsigned long size = 0;

	if ((!ea) || ((PAGE_SIZE-1) & (unsigned long)ea)) {
		printk(KERN_ERR "Trying to btfree() bad address (%p)\n", ea);
		return;
	}

	spin_lock(&btmalloc_mm.page_table_lock);

	/* Scan the bolted memory list for an entry matching
	 * the address to be freed, get the size (in bytes)
	 * and free the entry.  The list lock is not dropped
	 * until the page table entries are removed.
	 */
	for(p = &btmlist; (tmp = *p); p = &tmp->next ) {
		if ( tmp->addr == ea ) {
			size = tmp->size;
			break;
		}
	}

	/* If no entry found, it is an error */
	if ( !size ) {
		printk(KERN_ERR "Trying to btfree() bad address (%p)\n", ea);
		spin_unlock(&btmalloc_mm.page_table_lock);
		return;
	}

	/* Free up the bolted pages and remove the page table entries */
	if(local_free_bolted_pages((unsigned long)ea, size >> PAGE_SHIFT)) {
		*p = tmp->next;
		kfree(tmp);
	}

	spin_unlock(&btmalloc_mm.page_table_lock);
}

static int local_free_bolted_pages(unsigned long ea, unsigned long num) {
	int i;
	pte_t pte;

	for(i=0; i<num; i++) {
		pte_t *ptep = find_linux_pte(bolted_pgd, ea);
		if(!ptep) {
			panic("free_bolted_pages - page being freed "
			      "(0x%lx) is not bolted", ea );
		}
		pte = *ptep;
		pte_clear(ptep);
		__free_pages(pte_page(pte), 0);
		flush_hash_page(0, ea, ptep); 
		ea += PAGE_SIZE;
	}
	return 1;
}

/*
 * get_btm_area
 *
 * Get a virtual region in the bolted space
 */
static struct vm_struct *get_btm_area(unsigned long size, 
				      unsigned long flags) {
	unsigned long addr;
	struct vm_struct **p, *tmp, *area;
  
	area = (struct vm_struct *) kmalloc(sizeof(*area), GFP_KERNEL);
	if (!area) return NULL;

	addr = BTMALLOC_START;
	for (p = &btmlist; (tmp = *p) ; p = &tmp->next) {
		if (size + addr < (unsigned long) tmp->addr)
			break;
		addr = tmp->size + (unsigned long) tmp->addr;
		if (addr + size > BTMALLOC_END) {
			kfree(area);
			return NULL;
		}
	}

	if (addr + size > BTMALLOC_END) {
		kfree(area);
		return NULL;
	}
	area->flags = flags;
	area->addr = (void *)addr;
	area->size = size;
	area->next = *p;
	*p = area;
	return area;
}
