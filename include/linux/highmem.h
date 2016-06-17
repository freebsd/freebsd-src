#ifndef _LINUX_HIGHMEM_H
#define _LINUX_HIGHMEM_H

#include <linux/config.h>
#include <asm/pgalloc.h>

#ifdef CONFIG_HIGHMEM

extern struct page *highmem_start_page;

#include <asm/highmem.h>

/* declarations for linux/mm/highmem.c */
unsigned int nr_free_highpages(void);

extern struct buffer_head *create_bounce(int rw, struct buffer_head * bh_orig);

static inline char *bh_kmap(struct buffer_head *bh)
{
	return kmap(bh->b_page) + bh_offset(bh);
}

static inline void bh_kunmap(struct buffer_head *bh)
{
	kunmap(bh->b_page);
}

/*
 * remember to add offset! and never ever reenable interrupts between a
 * bh_kmap_irq and bh_kunmap_irq!!
 */
static inline char *bh_kmap_irq(struct buffer_head *bh, unsigned long *flags)
{
	unsigned long addr;

	__save_flags(*flags);

	/*
	 * could be low
	 */
	if (!PageHighMem(bh->b_page))
		return bh->b_data;

	/*
	 * it's a highmem page
	 */
	__cli();
	addr = (unsigned long) kmap_atomic(bh->b_page, KM_BH_IRQ);

	if (addr & ~PAGE_MASK)
		BUG();

	return (char *) addr + bh_offset(bh);
}

static inline void bh_kunmap_irq(char *buffer, unsigned long *flags)
{
	unsigned long ptr = (unsigned long) buffer & PAGE_MASK;

	kunmap_atomic((void *) ptr, KM_BH_IRQ);
	__restore_flags(*flags);
}

#else /* CONFIG_HIGHMEM */

static inline unsigned int nr_free_highpages(void) { return 0; }

static inline void *kmap(struct page *page) { return page_address(page); }

#define kunmap(page) do { } while (0)

#define kmap_atomic(page,idx)		kmap(page)
#define kunmap_atomic(page,idx)		kunmap(page)

#define bh_kmap(bh)			((bh)->b_data)
#define bh_kunmap(bh)			do { } while (0)
#define kmap_nonblock(page)            kmap(page)
#define bh_kmap_irq(bh, flags)		((bh)->b_data)
#define bh_kunmap_irq(bh, flags)	do { *(flags) = 0; } while (0)

#endif /* CONFIG_HIGHMEM */

/* when CONFIG_HIGHMEM is not set these will be plain clear/copy_page */
static inline void clear_user_highpage(struct page *page, unsigned long vaddr)
{
	void *addr = kmap_atomic(page, KM_USER0);
	clear_user_page(addr, vaddr);
	kunmap_atomic(addr, KM_USER0);
}

static inline void clear_highpage(struct page *page)
{
	clear_page(kmap(page));
	kunmap(page);
}

/*
 * Same but also flushes aliased cache contents to RAM.
 */
static inline void memclear_highpage_flush(struct page *page, unsigned int offset, unsigned int size)
{
	char *kaddr;

	if (offset + size > PAGE_SIZE)
		out_of_line_bug();
	kaddr = kmap(page);
	memset(kaddr + offset, 0, size);
	flush_dcache_page(page);
	flush_page_to_ram(page);
	kunmap(page);
}

static inline void copy_user_highpage(struct page *to, struct page *from, unsigned long vaddr)
{
	char *vfrom, *vto;

	vfrom = kmap_atomic(from, KM_USER0);
	vto = kmap_atomic(to, KM_USER1);
	copy_user_page(vto, vfrom, vaddr);
	kunmap_atomic(vfrom, KM_USER0);
	kunmap_atomic(vto, KM_USER1);
}

static inline void copy_highpage(struct page *to, struct page *from)
{
	char *vfrom, *vto;

	vfrom = kmap_atomic(from, KM_USER0);
	vto = kmap_atomic(to, KM_USER1);
	copy_page(vto, vfrom);
	kunmap_atomic(vfrom, KM_USER0);
	kunmap_atomic(vto, KM_USER1);
}

#endif /* _LINUX_HIGHMEM_H */
