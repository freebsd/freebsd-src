/*
 * highmem.h: virtual kernel memory mappings for high memory
 *
 * PowerPC version, stolen from the i386 version.
 *
 * Used in CONFIG_HIGHMEM systems for memory pages which
 * are not addressable by direct kernel virtual addresses.
 *
 * Copyright (C) 1999 Gerhard Wichert, Siemens AG
 *		      Gerhard.Wichert@pdb.siemens.de
 *
 *
 * Redesigned the x86 32-bit VM architecture to deal with
 * up to 16 Terrabyte physical memory. With current x86 CPUs
 * we now support up to 64 Gigabytes physical RAM.
 *
 * Copyright (C) 1999 Ingo Molnar <mingo@redhat.com>
 */

#ifndef _ASM_HIGHMEM_H
#define _ASM_HIGHMEM_H

#ifdef __KERNEL__

#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/kmap_types.h>
#include <asm/pgtable.h>

/* undef for production */
#define HIGHMEM_DEBUG 1

extern pte_t *kmap_pte;
extern pgprot_t kmap_prot;
extern pte_t *pkmap_page_table;

extern void kmap_init(void) __init;

/*
 * Right now we initialize only a single pte table. It can be extended
 * easily, subsequent pte tables have to be allocated in one physical
 * chunk of RAM.
 */
#define PKMAP_BASE	CONFIG_HIGHMEM_START
#define LAST_PKMAP	PTRS_PER_PTE
#define LAST_PKMAP_MASK (LAST_PKMAP-1)
#define PKMAP_NR(virt)  ((virt-PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)  (PKMAP_BASE + ((nr) << PAGE_SHIFT))

#define KMAP_FIX_BEGIN	(PKMAP_BASE + 0x00400000UL)

extern void *kmap_high(struct page *page, int nonblock);
extern void kunmap_high(struct page *page);

#define kmap(page)		__kmap(page, 0)
#define kmap_nonblock(page)	__kmap(page, 1)

static inline void *__kmap(struct page *page, int nonblock)
{
	if (in_interrupt())
		BUG();
	if (page < highmem_start_page)
		return page_address(page);
	return kmap_high(page, nonblock);
}

static inline void kunmap(struct page *page)
{
	if (in_interrupt())
		BUG();
	if (page < highmem_start_page)
		return;
	kunmap_high(page);
}

/*
 * The use of kmap_atomic/kunmap_atomic is discouraged - kmap/kunmap
 * gives a more generic (and caching) interface. But kmap_atomic can
 * be used in IRQ contexts, so in some (very limited) cases we need
 * it.
 */
static inline void *kmap_atomic(struct page *page, enum km_type type)
{
	unsigned int idx;
	unsigned long vaddr;

	if (page < highmem_start_page)
		return page_address(page);

	idx = type + KM_TYPE_NR*smp_processor_id();
	vaddr = KMAP_FIX_BEGIN + idx * PAGE_SIZE;
#if HIGHMEM_DEBUG
	if (!pte_none(*(kmap_pte+idx)))
		BUG();
#endif
	set_pte(kmap_pte+idx, mk_pte(page, kmap_prot));
	flush_tlb_page(0, vaddr);

	return (void*) vaddr;
}

static inline void kunmap_atomic(void *kvaddr, enum km_type type)
{
#if HIGHMEM_DEBUG
	unsigned long vaddr = (unsigned long) kvaddr & PAGE_MASK;
	unsigned int idx = type + KM_TYPE_NR*smp_processor_id();

	if (vaddr < KMAP_FIX_BEGIN) // FIXME
		return;

	if (vaddr != KMAP_FIX_BEGIN + idx * PAGE_SIZE)
		BUG();

	/*
	 * force other mappings to Oops if they'll try to access
	 * this pte without first remap it
	 */
	pte_clear(kmap_pte+idx);
	flush_tlb_page(0, vaddr);
#endif
}

#endif /* __KERNEL__ */

#endif /* _ASM_HIGHMEM_H */
