/* 
 * arch/mips/mm/remap.c
 * A copy of mm/memory.c with modifications to handle 64 bit
 * physical addresses.
 */

/*
 * maps a range of physical memory into the requested pages. the old
 * mappings are removed. any references to nonexistent pages results
 * in null mappings (currently treated as "copy-on-access")
 */

#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/smp_lock.h>
#include <linux/swapctl.h>
#include <linux/iobuf.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/module.h>

#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/tlb.h>

/*
 * Return indicates whether a page was freed so caller can adjust rss
 */
static inline void forget_pte(pte_t page)
{
	if (!pte_none(page)) {
		printk("forget_pte: old mapping existed!\n");
		BUG();
	}
}

static inline void remap_pte_range(pte_t * pte, unsigned long address, unsigned long size,
	phys_t phys_addr, pgprot_t prot)
{
	unsigned long end;

	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		struct page *page;
		pte_t oldpage;
		oldpage = ptep_get_and_clear(pte);

		page = virt_to_page(__va(phys_addr));
		if ((!VALID_PAGE(page)) || PageReserved(page))
 			set_pte(pte, mk_pte_phys(phys_addr, prot));
		forget_pte(oldpage);
		address += PAGE_SIZE;
		phys_addr += PAGE_SIZE;
		pte++;
	} while (address && (address < end));
}

static inline int remap_pmd_range(struct mm_struct *mm, pmd_t * pmd, unsigned long address, unsigned long size,
	phys_t phys_addr, pgprot_t prot)
{
	unsigned long end;

	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	phys_addr -= address;
	do {
		pte_t * pte = pte_alloc(mm, pmd, address);
		if (!pte)
			return -ENOMEM;
		remap_pte_range(pte, address, end - address, address + phys_addr, prot);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
	return 0;
}

extern phys_t (*fixup_bigphys_addr)(phys_t phys_addr, phys_t size);
/*  Note: this is only safe if the mm semaphore is held when called. */
int remap_page_range_high(unsigned long from, phys_t phys_addr, unsigned long size, pgprot_t prot)
{
	int error = 0;
	pgd_t * dir;
	unsigned long beg = from;
	unsigned long end = from + size;
	struct mm_struct *mm = current->mm;

	phys_addr = fixup_bigphys_addr(phys_addr, size);
	phys_addr -= from;
	dir = pgd_offset(mm, from);
	flush_cache_range(mm, beg, end);
	if (from >= end)
		BUG();

	spin_lock(&mm->page_table_lock);
	do {
		pmd_t *pmd = pmd_alloc(mm, dir, from);
		error = -ENOMEM;
		if (!pmd)
			break;
		error = remap_pmd_range(mm, pmd, from, end - from, phys_addr + from, prot);
		if (error)
			break;
		from = (from + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (from && (from < end));
	spin_unlock(&mm->page_table_lock);
	flush_tlb_range(mm, beg, end);
	return error;
}
