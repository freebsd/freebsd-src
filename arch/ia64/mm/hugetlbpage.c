/*
 * IA-64 Huge TLB Page Support for Kernel.
 *
 * Copyright (C) 2002, Rohit Seth <rohit.seth@intel.com>
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <asm/mman.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>


#define TASK_HPAGE_BASE (REGION_HPAGE << REGION_SHIFT)

static long    htlbpagemem;
int     htlbpage_max;
static long    htlbzone_pages;

struct vm_operations_struct hugetlb_vm_ops;
static LIST_HEAD(htlbpage_freelist);
static spinlock_t htlbpage_lock = SPIN_LOCK_UNLOCKED;

static struct page *alloc_hugetlb_page(void)
{
	int i;
	struct page *page;

	spin_lock(&htlbpage_lock);
	if (list_empty(&htlbpage_freelist)) {
		spin_unlock(&htlbpage_lock);
		return NULL;
	}

	page = list_entry(htlbpage_freelist.next, struct page, list);
	list_del(&page->list);
	htlbpagemem--;
	spin_unlock(&htlbpage_lock);
	set_page_count(page, 1);
	for (i = 0; i < (HPAGE_SIZE/PAGE_SIZE); ++i)
		clear_highpage(&page[i]);
	return page;
}

static pte_t *
huge_pte_alloc (struct mm_struct *mm, unsigned long addr)
{
	unsigned long taddr = htlbpage_to_page(addr);
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte = NULL;

	pgd = pgd_offset(mm, taddr);
	pmd = pmd_alloc(mm, pgd, taddr);
	if (pmd)
		pte = pte_alloc(mm, pmd, taddr);
	return pte;
}

static pte_t *
huge_pte_offset (struct mm_struct *mm, unsigned long addr)
{
	unsigned long taddr = htlbpage_to_page(addr);
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte = NULL;

	pgd = pgd_offset(mm, taddr);
	pmd = pmd_offset(pgd, taddr);
	pte = pte_offset(pmd, taddr);
	return pte;
}

#define mk_pte_huge(entry) { pte_val(entry) |= _PAGE_P; }

static void
set_huge_pte (struct mm_struct *mm, struct vm_area_struct *vma,
	      struct page *page, pte_t * page_table, int write_access)
{
	pte_t entry;

	mm->rss += (HPAGE_SIZE / PAGE_SIZE);
	if (write_access) {
		entry =
		    pte_mkwrite(pte_mkdirty(mk_pte(page, vma->vm_page_prot)));
	} else
		entry = pte_wrprotect(mk_pte(page, vma->vm_page_prot));
	entry = pte_mkyoung(entry);
	mk_pte_huge(entry);
	set_pte(page_table, entry);
	return;
}
/*
 * This function checks for proper alignment of input addr and len parameters.
 */
int is_aligned_hugepage_range(unsigned long addr, unsigned long len)
{
	if (len & ~HPAGE_MASK)
		return -EINVAL;
	if (addr & ~HPAGE_MASK)
		return -EINVAL;
	if (REGION_NUMBER(addr) != REGION_HPAGE)
		return -EINVAL;

	return 0;
}
/* This function checks if the address and address+len falls out of HugeTLB region.  It
 * return -EINVAL if any part of address range falls in HugeTLB region.
 */
int  is_invalid_hugepage_range(unsigned long addr, unsigned long len)
{
	if (REGION_NUMBER(addr) == REGION_HPAGE)
		return -EINVAL;
	if (REGION_NUMBER(addr+len) == REGION_HPAGE)
		return -EINVAL;
	return 0;
}

/*
 * Same as generic free_pgtables(), except constant PGDIR_* and pgd_offset
 * are hugetlb region specific.
 */
void hugetlb_free_pgtables(struct mm_struct * mm, struct vm_area_struct *prev,
	unsigned long start, unsigned long end)
{
	unsigned long first = start & HUGETLB_PGDIR_MASK;
	unsigned long last = end + HUGETLB_PGDIR_SIZE - 1;
	unsigned long start_index, end_index;

	if (!prev) {
		prev = mm->mmap;
		if (!prev)
			goto no_mmaps;
		if (prev->vm_end > start) {
			if (last > prev->vm_start)
				last = prev->vm_start;
			goto no_mmaps;
		}
	}
	for (;;) {
		struct vm_area_struct *next = prev->vm_next;

		if (next) {
			if (next->vm_start < start) {
				prev = next;
				continue;
			}
			if (last > next->vm_start)
				last = next->vm_start;
		}
		if (prev->vm_end > first)
			first = prev->vm_end + HUGETLB_PGDIR_SIZE - 1;
		break;
	}
no_mmaps:
	if (last < first)
		return;
	/*
	 * If the PGD bits are not consecutive in the virtual address, the
	 * old method of shifting the VA >> by PGDIR_SHIFT doesn't work.
	 */
	start_index = pgd_index(htlbpage_to_page(first));
	end_index = pgd_index(htlbpage_to_page(last));
	if (end_index > start_index) {
		clear_page_tables(mm, start_index, end_index - start_index);
		flush_tlb_pgtables(mm, first & HUGETLB_PGDIR_MASK,
				   last & HUGETLB_PGDIR_MASK);
	}
}

int copy_hugetlb_page_range(struct mm_struct *dst, struct mm_struct *src,
			struct vm_area_struct *vma)
{
	pte_t *src_pte, *dst_pte, entry;
	struct page *ptepage;
	unsigned long addr = vma->vm_start;
	unsigned long end = vma->vm_end;

	while (addr < end) {
		dst_pte = huge_pte_alloc(dst, addr);
		if (!dst_pte)
			goto nomem;
		src_pte = huge_pte_offset(src, addr);
		entry = *src_pte;
		ptepage = pte_page(entry);
		get_page(ptepage);
		set_pte(dst_pte, entry);
		dst->rss += (HPAGE_SIZE / PAGE_SIZE);
		addr += HPAGE_SIZE;
	}
	return 0;
nomem:
	return -ENOMEM;
}

int
follow_hugetlb_page(struct mm_struct *mm, struct vm_area_struct *vma,
		    struct page **pages, struct vm_area_struct **vmas,
		    unsigned long *st, int *length, int i)
{
	pte_t *ptep, pte;
	unsigned long start = *st;
	unsigned long pstart;
	int len = *length;
	struct page *page;

	do {
		pstart = start;
		ptep = huge_pte_offset(mm, start);
		pte = *ptep;

back1:
		page = pte_page(pte);
		if (pages) {
			page += ((start & ~HPAGE_MASK) >> PAGE_SHIFT);
			pages[i] = page;
		}
		if (vmas)
			vmas[i] = vma;
		i++;
		len--;
		start += PAGE_SIZE;
		if (((start & HPAGE_MASK) == pstart) && len &&
				(start < vma->vm_end))
			goto back1;
	} while (len && start < vma->vm_end);
	*length = len;
	*st = start;
	return i;
}

void free_huge_page(struct page *page)
{
	BUG_ON(page_count(page));
	BUG_ON(page->mapping);

	INIT_LIST_HEAD(&page->list);

	spin_lock(&htlbpage_lock);
	list_add(&page->list, &htlbpage_freelist);
	htlbpagemem++;
	spin_unlock(&htlbpage_lock);
}

void huge_page_release(struct page *page)
{
	if (!put_page_testzero(page))
		return;

	free_huge_page(page);
}

void unmap_hugepage_range(struct vm_area_struct *vma, unsigned long start, unsigned long end)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long address;
	pte_t *pte;
	struct page *page;

	BUG_ON(start & (HPAGE_SIZE - 1));
	BUG_ON(end & (HPAGE_SIZE - 1));

	for (address = start; address < end; address += HPAGE_SIZE) {
		pte = huge_pte_offset(mm, address);
		if (pte_none(*pte))
			continue;
		page = pte_page(*pte);
		huge_page_release(page);
		pte_clear(pte);
	}
	mm->rss -= (end - start) >> PAGE_SHIFT;
	flush_tlb_range(mm, start, end);
}

void zap_hugepage_range(struct vm_area_struct *vma, unsigned long start, unsigned long length)
{
	struct mm_struct *mm = vma->vm_mm;
	spin_lock(&mm->page_table_lock);
	unmap_hugepage_range(vma, start, start + length);
	spin_unlock(&mm->page_table_lock);
}

int hugetlb_prefault(struct address_space *mapping, struct vm_area_struct *vma)
{
	struct mm_struct *mm = current->mm;
	struct inode *inode = mapping->host;
	unsigned long addr;
	int ret = 0;

	BUG_ON(vma->vm_start & ~HPAGE_MASK);
	BUG_ON(vma->vm_end & ~HPAGE_MASK);

	spin_lock(&mm->page_table_lock);
	for (addr = vma->vm_start; addr < vma->vm_end; addr += HPAGE_SIZE) {
		unsigned long idx;
		pte_t *pte = huge_pte_alloc(mm, addr);
		struct page *page;

		if (!pte) {
			ret = -ENOMEM;
			goto out;
		}
		if (!pte_none(*pte))
			continue;

		idx = ((addr - vma->vm_start) >> HPAGE_SHIFT)
			+ (vma->vm_pgoff >> (HPAGE_SHIFT - PAGE_SHIFT));
		page = find_get_page(mapping, idx);
		if (!page) {
			/* charge the fs quota first */
			if (hugetlb_get_quota(mapping)) {
				ret = -ENOMEM;
				goto out;
			}
			page = alloc_hugetlb_page();
			if (!page) {
				hugetlb_put_quota(mapping);
				ret = -ENOMEM;
				goto out;
			}
			add_to_page_cache(page, mapping, idx);
			unlock_page(page);
		}
		set_huge_pte(mm, vma, page, pte, vma->vm_flags & VM_WRITE);
	}
out:
	spin_unlock(&mm->page_table_lock);
	return ret;
}

unsigned long hugetlb_get_unmapped_area(struct file *file, unsigned long addr, unsigned long len,
		unsigned long pgoff, unsigned long flags)
{
	struct vm_area_struct *vmm;

	if (len > RGN_MAP_LIMIT)
		return -ENOMEM;
	if (len & ~HPAGE_MASK)
		return -EINVAL;
	/* This code assumes that REGION_HPAGE != 0. */
	if ((REGION_NUMBER(addr) != REGION_HPAGE) || (addr & (HPAGE_SIZE - 1)))
		addr = TASK_HPAGE_BASE;
	else
		addr = COLOR_HALIGN(addr);
	for (vmm = find_vma(current->mm, addr); ; vmm = vmm->vm_next) {
		/* At this point:  (!vmm || addr < vmm->vm_end). */
		if (REGION_OFFSET(addr) + len > RGN_MAP_LIMIT)
			return -ENOMEM;
		if (!vmm || (addr + len) <= vmm->vm_start)
			return addr;
		addr = COLOR_HALIGN(vmm->vm_end);
	}
}
void update_and_free_page(struct page *page)
{
	int j;
	struct page *map;

	map = page;
	htlbzone_pages--;
	for (j = 0; j < (HPAGE_SIZE / PAGE_SIZE); j++) {
		map->flags &= ~(1 << PG_locked | 1 << PG_error | 1 << PG_referenced |
				1 << PG_dirty | 1 << PG_active | 1 << PG_reserved);
		set_page_count(map, 0);
		map++;
	}
	set_page_count(page, 1);
	__free_pages(page, HUGETLB_PAGE_ORDER);
}

int try_to_free_low(int count)
{
	struct list_head *p;
	struct page *page, *map;

	map = NULL;
	spin_lock(&htlbpage_lock);
	list_for_each(p, &htlbpage_freelist) {
		if (map) {
			list_del(&map->list);
			update_and_free_page(map);
			htlbpagemem--;
			map = NULL;
			if (++count == 0)
				break;
		}
		page = list_entry(p, struct page, list);
		if ((page_zone(page))->name[0] != 'H') //Look for non-Highmem zones.
			map = page;
	}
	if (map) {
		list_del(&map->list);
		update_and_free_page(map);
		htlbpagemem--;
		count++;
	}
	spin_unlock(&htlbpage_lock);
	return count;
}

int set_hugetlb_mem_size(int count)
{
	int j, lcount;
	struct page *page, *map;

	if (count < 0)
		lcount = count;
	else
		lcount = count - htlbzone_pages;

	if (lcount == 0)
		return (int)htlbzone_pages;
	if (lcount > 0) {	/* Increase the mem size. */
		while (lcount--) {
			page = alloc_pages(__GFP_HIGHMEM, HUGETLB_PAGE_ORDER);
			if (page == NULL)
				break;
			map = page;
			for (j = 0; j < (HPAGE_SIZE / PAGE_SIZE); j++) {
				SetPageReserved(map);
				map++;
			}
			spin_lock(&htlbpage_lock);
			list_add(&page->list, &htlbpage_freelist);
			htlbpagemem++;
			htlbzone_pages++;
			spin_unlock(&htlbpage_lock);
		}
		return (int) htlbzone_pages;
	}
	/* Shrink the memory size. */
	lcount = try_to_free_low(lcount);
	while (lcount++ < 0) {
		page = alloc_hugetlb_page();
		if (page == NULL)
			break;
		spin_lock(&htlbpage_lock);
		update_and_free_page(page);
		spin_unlock(&htlbpage_lock);
	}
	return (int) htlbzone_pages;
}

int hugetlb_sysctl_handler(ctl_table *table, int write, struct file *file, void *buffer, size_t *length)
{
	proc_dointvec(table, write, file, buffer, length);
	htlbpage_max = set_hugetlb_mem_size(htlbpage_max);
	return 0;
}

static int __init hugetlb_setup(char *s)
{
	if (sscanf(s, "%d", &htlbpage_max) <= 0)
		htlbpage_max = 0;
	return 1;
}
__setup("hugepages=", hugetlb_setup);

static int __init hugetlb_init(void)
{
	int i, j;
	struct page *page;

	for (i = 0; i < htlbpage_max; ++i) {
		page = alloc_pages(__GFP_HIGHMEM, HUGETLB_PAGE_ORDER);
		if (!page)
			break;
		for (j = 0; j < HPAGE_SIZE/PAGE_SIZE; ++j)
			SetPageReserved(&page[j]);
		spin_lock(&htlbpage_lock);
		list_add(&page->list, &htlbpage_freelist);
		spin_unlock(&htlbpage_lock);
	}
	htlbpage_max = htlbpagemem = htlbzone_pages = i;
	printk("Total HugeTLB memory allocated, %ld\n", htlbpagemem);
	return 0;
}
module_init(hugetlb_init);

int hugetlb_report_meminfo(char *buf)
{
	return sprintf(buf,
			"HugePages_Total: %5lu\n"
			"HugePages_Free:  %5lu\n"
			"Hugepagesize:    %5lu kB\n",
			htlbzone_pages,
			htlbpagemem,
			HPAGE_SIZE/1024);
}

int is_hugepage_mem_enough(size_t size)
{
	if (size > (htlbpagemem << HPAGE_SHIFT))
		return 0;
	return 1;
}

static struct page *hugetlb_nopage(struct vm_area_struct * area, unsigned long address, int unused)
{
	BUG();
	return NULL;
}

struct vm_operations_struct hugetlb_vm_ops = {
	.nopage =	hugetlb_nopage,
};
