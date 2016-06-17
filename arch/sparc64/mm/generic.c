/* $Id: generic.c,v 1.17 2001/04/09 04:08:06 davem Exp $
 * generic.c: Generic Sparc mm routines that are not dependent upon
 *            MMU type but are Sparc specific.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/pagemap.h>

#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/page.h>

static inline void forget_pte(pte_t page)
{
	if (pte_none(page))
		return;
	if (pte_present(page)) {
		struct page *ptpage = pte_page(page);
		if ((!VALID_PAGE(ptpage)) || PageReserved(ptpage))
			return;
		page_cache_release(ptpage);
		return;
	}
	swap_free(pte_to_swp_entry(page));
}

/* Remap IO memory, the same way as remap_page_range(), but use
 * the obio memory space.
 *
 * They use a pgprot that sets PAGE_IO and does not check the
 * mem_map table as this is independent of normal memory.
 *
 * As a special hack if the lowest bit of offset is set the
 * side-effect bit will be turned off.  This is used as a
 * performance improvement on FFB/AFB. -DaveM
 */
static inline void io_remap_pte_range(pte_t * pte, unsigned long address, unsigned long size,
	unsigned long offset, pgprot_t prot, int space)
{
	unsigned long end;

	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		pte_t oldpage;
		pte_t entry;
		unsigned long curend = address + PAGE_SIZE;
		
		entry = mk_pte_io((offset & ~(0x1UL)), prot, space);
		if (!(address & 0xffff)) {
			if (!(address & 0x3fffff) && !(offset & 0x3ffffe) && end >= address + 0x400000) {
				entry = mk_pte_io((offset & ~(0x1UL)),
						  __pgprot(pgprot_val (prot) | _PAGE_SZ4MB),
						  space);
				curend = address + 0x400000;
				offset += 0x400000;
			} else if (!(address & 0x7ffff) && !(offset & 0x7fffe) && end >= address + 0x80000) {
				entry = mk_pte_io((offset & ~(0x1UL)),
						  __pgprot(pgprot_val (prot) | _PAGE_SZ512K),
						  space);
				curend = address + 0x80000;
				offset += 0x80000;
			} else if (!(offset & 0xfffe) && end >= address + 0x10000) {
				entry = mk_pte_io((offset & ~(0x1UL)),
						  __pgprot(pgprot_val (prot) | _PAGE_SZ64K),
						  space);
				curend = address + 0x10000;
				offset += 0x10000;
			} else
				offset += PAGE_SIZE;
		} else
			offset += PAGE_SIZE;

		if (offset & 0x1UL)
			pte_val(entry) &= ~(_PAGE_E);
		do {
			oldpage = *pte;
			pte_clear(pte);
			set_pte(pte, entry);
			forget_pte(oldpage);
			address += PAGE_SIZE;
			pte++;
		} while (address < curend);
	} while (address < end);
}

static inline int io_remap_pmd_range(pmd_t * pmd, unsigned long address, unsigned long size,
	unsigned long offset, pgprot_t prot, int space)
{
	unsigned long end;

	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	offset -= address;
	do {
		pte_t * pte = pte_alloc(current->mm, pmd, address);
		if (!pte)
			return -ENOMEM;
		io_remap_pte_range(pte, address, end - address, address + offset, prot, space);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);
	return 0;
}

int io_remap_page_range(unsigned long from, unsigned long offset, unsigned long size, pgprot_t prot, int space)
{
	int error = 0;
	pgd_t * dir;
	unsigned long beg = from;
	unsigned long end = from + size;
	struct mm_struct *mm = current->mm;

	prot = __pgprot(pg_iobits);
	offset -= from;
	dir = pgd_offset(mm, from);
	flush_cache_range(mm, beg, end);

	spin_lock(&mm->page_table_lock);
	while (from < end) {
		pmd_t *pmd = pmd_alloc(current->mm, dir, from);
		error = -ENOMEM;
		if (!pmd)
			break;
		error = io_remap_pmd_range(pmd, from, end - from, offset + from, prot, space);
		if (error)
			break;
		from = (from + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	}
	spin_unlock(&mm->page_table_lock);

	flush_tlb_range(current->mm, beg, end);
	return error;
}
