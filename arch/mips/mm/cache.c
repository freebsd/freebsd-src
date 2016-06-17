/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 2003 by Ralf Baechle
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/cacheflush.h>

asmlinkage int sys_cacheflush(void *addr, int bytes, int cache)
{
	/* This should flush more selectivly ...  */
	__flush_cache_all();

	return 0;
}

void flush_dcache_page(struct page *page)
{
	unsigned long addr;

	if (!cpu_has_dc_aliases)
		return;

	if (page->mapping && page->mapping->i_mmap == NULL &&
	    page->mapping->i_mmap_shared == NULL) {
		SetPageDcacheDirty(page);

		return;
	}

	/*
	 * We could delay the flush for the !page->mapping case too.  But that
	 * case is for exec env/arg pages and those are 99% certainly going to
	 * get faulted into the tlb (and thus flushed) anyways.
	 */
	addr = (unsigned long) page_address(page);
	flush_data_cache_page(addr);
}

void __update_cache(struct vm_area_struct *vma, unsigned long address,
        pte_t pte)
{
	unsigned long addr;
	struct page *page;

	if (!cpu_has_dc_aliases)
		return;

	page = pte_page(pte);
	if (VALID_PAGE(page) && page->mapping &&
	    (page->flags & (1UL << PG_dcache_dirty))) {
		if (pages_do_alias((unsigned long) page_address(page), address & PAGE_MASK)) {
			addr = (unsigned long) page_address(page);
			flush_data_cache_page(addr);
		}

		ClearPageDcacheDirty(page);
	}
}

EXPORT_SYMBOL(flush_dcache_page);
