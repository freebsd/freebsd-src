/*
 *  linux/arch/arm/mm/small_page.c
 *
 *  Copyright (C) 1996  Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   26/01/1996	RMK	Cleaned up various areas to make little more generic
 *   07/02/1999	RMK	Support added for 16K and 32K page sizes
 *			containing 8K blocks
 */
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>

#include <asm/bitops.h>
#include <asm/pgtable.h>

#define PEDANTIC

/*
 * Requirement:
 *  We need to be able to allocate naturally aligned memory of finer
 *  granularity than the page size.  This is typically used for the
 *  second level page tables on 32-bit ARMs.
 *
 * Theory:
 *  We "misuse" the Linux memory management system.  We use alloc_page
 *  to allocate a page and then mark it as reserved.  The Linux memory
 *  management system will then ignore the "offset", "next_hash" and
 *  "pprev_hash" entries in the mem_map for this page.
 *
 *  We then use a bitstring in the "offset" field to mark which segments
 *  of the page are in use, and manipulate this as required during the
 *  allocation and freeing of these small pages.
 *
 *  We also maintain a queue of pages being used for this purpose using
 *  the "next_hash" and "pprev_hash" entries of mem_map;
 */

struct order {
	struct page *queue;
	unsigned int mask;		/* (1 << shift) - 1		*/
	unsigned int shift;		/* (1 << shift) size of page	*/
	unsigned int block_mask;	/* nr_blocks - 1		*/
	unsigned int all_used;		/* (1 << nr_blocks) - 1		*/
};


static struct order orders[] = {
#if PAGE_SIZE == 4096
	{ NULL, 2047, 11,  1, 0x00000003 }
#elif PAGE_SIZE == 32768
	{ NULL, 2047, 11, 15, 0x0000ffff },
	{ NULL, 8191, 13,  3, 0x0000000f }
#else
#error unsupported page size
#endif
};

#define USED_MAP(pg)			((pg)->index)
#define TEST_AND_CLEAR_USED(pg,off)	(test_and_clear_bit(off, &USED_MAP(pg)))
#define SET_USED(pg,off)		(set_bit(off, &USED_MAP(pg)))

static spinlock_t small_page_lock = SPIN_LOCK_UNLOCKED;

static void add_page_to_queue(struct page *page, struct page **p)
{
#ifdef PEDANTIC
	if (page->pprev_hash)
		PAGE_BUG(page);
#endif
	page->next_hash = *p;
	if (*p)
		(*p)->pprev_hash = &page->next_hash;
	*p = page;
	page->pprev_hash = p;
}

static void remove_page_from_queue(struct page *page)
{
	if (page->pprev_hash) {
		if (page->next_hash)
			page->next_hash->pprev_hash = page->pprev_hash;
		*page->pprev_hash = page->next_hash;
		page->pprev_hash = NULL;
	}
}

static unsigned long __get_small_page(int priority, struct order *order)
{
	unsigned long flags;
	struct page *page;
	int offset;

	if (!order->queue)
		goto need_new_page;

	spin_lock_irqsave(&small_page_lock, flags);
	page = order->queue;
again:
#ifdef PEDANTIC
	if (USED_MAP(page) & ~order->all_used)
		PAGE_BUG(page);
#endif
	offset = ffz(USED_MAP(page));
	SET_USED(page, offset);
	if (USED_MAP(page) == order->all_used)
		remove_page_from_queue(page);
	spin_unlock_irqrestore(&small_page_lock, flags);

	return (unsigned long) page_address(page) + (offset << order->shift);

need_new_page:
	page = alloc_page(priority);

	spin_lock_irqsave(&small_page_lock, flags);
	if (!order->queue) {
		if (!page)
			goto no_page;
		SetPageReserved(page);
		USED_MAP(page) = 0;
		cli();
		add_page_to_queue(page, &order->queue);
	} else {
		__free_page(page);
		cli();
		page = order->queue;
	}
	goto again;

no_page:
	spin_unlock_irqrestore(&small_page_lock, flags);
	return 0;
}

static void __free_small_page(unsigned long spage, struct order *order)
{
	unsigned long flags;
	struct page *page;

	if (virt_addr_valid(spage)) {
		page = virt_to_page(spage);

		/*
		 * The container-page must be marked Reserved
		 */
		if (!PageReserved(page) || spage & order->mask)
			goto non_small;

#ifdef PEDANTIC
		if (USED_MAP(page) & ~order->all_used)
			PAGE_BUG(page);
#endif

		spage = spage >> order->shift;
		spage &= order->block_mask;

		/*
		 * the following must be atomic wrt get_page
		 */
		spin_lock_irqsave(&small_page_lock, flags);

		if (USED_MAP(page) == order->all_used)
			add_page_to_queue(page, &order->queue);

		if (!TEST_AND_CLEAR_USED(page, spage))
			goto already_free;

		if (USED_MAP(page) == 0)
			goto free_page;

		spin_unlock_irqrestore(&small_page_lock, flags);
	}
	return;

free_page:
	/*
	 * unlink the page from the small page queue and free it
	 */
	remove_page_from_queue(page);
	spin_unlock_irqrestore(&small_page_lock, flags);
	ClearPageReserved(page);
	__free_page(page);
	return;

non_small:
	printk("Trying to free non-small page from %p\n", __builtin_return_address(0));
	return;
already_free:
	printk("Trying to free free small page from %p\n", __builtin_return_address(0));
}

unsigned long get_page_8k(int priority)
{
	return __get_small_page(priority, orders+1);
}

void free_page_8k(unsigned long spage)
{
	__free_small_page(spage, orders+1);
}
