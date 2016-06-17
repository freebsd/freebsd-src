/*
 *  linux/arch/arm/mm/ioremap.c
 *
 * Re-map IO memory to kernel address space so that we can access it.
 *
 * (C) Copyright 1995 1996 Linus Torvalds
 *
 * Hacked for ARM by Phil Blundell <philb@gnu.org>
 * Hacked to allow all architectures to build, and various cleanups
 * by Russell King
 *
 * This allows a driver to remap an arbitrary region of bus memory into
 * virtual space.  One should *only* use readl, writel, memcpy_toio and
 * so on with such remapped areas.
 *
 * ioremap support tweaked to allow support for large page mappings.  We
 * have several issues that needs to be resolved first however:
 *
 *  1. We need set_pte, or something like set_pte to understand large
 *     page mappings.
 *
 *  2. we need the unmap_* functions to likewise understand large page
 *     mappings.
 */
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/io.h>

static inline void
remap_area_pte(pte_t * pte, unsigned long address, unsigned long size,
	       unsigned long pfn, pgprot_t pgprot)
{
	unsigned long end;

	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	BUG_ON(address >= end);
	do {
		if (!pte_none(*pte))
			goto bad;

		set_pte(pte, pfn_pte(pfn, pgprot));
		address += PAGE_SIZE;
		pfn++;
		pte++;
	} while (address && (address < end));
	return;

 bad:
	printk("remap_area_pte: page already exists\n");
	BUG();
}

static inline int
remap_area_pmd(pmd_t * pmd, unsigned long address, unsigned long size,
	       unsigned long pfn, unsigned long flags)
{
	unsigned long end;
	pgprot_t pgprot;

	address &= ~PGDIR_MASK;
	end = address + size;

	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;

	pfn -= address >> PAGE_SHIFT;
	BUG_ON(address >= end);

	pgprot = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY | L_PTE_WRITE | flags);
	do {
		pte_t * pte = pte_alloc(&init_mm, pmd, address);
		if (!pte)
			return -ENOMEM;
		remap_area_pte(pte, address, end - address, pfn + (address >> PAGE_SHIFT), pgprot);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
	return 0;
}

static int
remap_area_pages(unsigned long address, unsigned long pfn,
		 unsigned long size, unsigned long flags)
{
	int error;
	pgd_t * dir;
	unsigned long end = address + size;

	pfn -= address >> PAGE_SHIFT;
	dir = pgd_offset(&init_mm, address);
	flush_cache_all();
	BUG_ON(address >= end);
	spin_lock(&init_mm.page_table_lock);
	do {
		pmd_t *pmd;
		pmd = pmd_alloc(&init_mm, dir, address);
		error = -ENOMEM;
		if (!pmd)
			break;
		if (remap_area_pmd(pmd, address, end - address,
					 pfn + (address >> PAGE_SHIFT), flags))
			break;
		error = 0;
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address && (address < end));
	spin_unlock(&init_mm.page_table_lock);
	flush_tlb_all();
	return error;
}

/*
 * Remap an arbitrary physical address space into the kernel virtual
 * address space. Needed when the kernel wants to access high addresses
 * directly.
 *
 * NOTE! We need to allow non-page-aligned mappings too: we will obviously
 * have to convert them into an offset in a page-aligned mapping, but the
 * caller shouldn't need to know that small detail.
 *
 * 'flags' are the extra L_PTE_ flags that you want to specify for this
 * mapping.  See include/asm-arm/proc-armv/pgtable.h for more information.
 */
void * __ioremap(unsigned long phys_addr, size_t size, unsigned long flags)
{
	void * addr;
	struct vm_struct * area;
	unsigned long offset, last_addr;

	/* Don't allow wraparound or zero size */
	last_addr = phys_addr + size - 1;
	if (!size || last_addr < phys_addr)
		return NULL;

	/*
	 * Mappings have to be page-aligned
	 */
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr) - phys_addr;

	/*
	 * Ok, go for it..
	 */
	area = get_vm_area(size, VM_IOREMAP);
	if (!area)
		return NULL;
	addr = area->addr;
	if (remap_area_pages(VMALLOC_VMADDR(addr), phys_addr >> PAGE_SHIFT, size, flags)) {
		vfree(addr);
		return NULL;
	}
	return (void *) (offset + (char *)addr);
}

void __iounmap(void *addr)
{
	vfree((void *) (PAGE_MASK & (unsigned long) addr));
}
