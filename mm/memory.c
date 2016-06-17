/*
 *  linux/mm/memory.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */

/*
 * Real VM (paging to/from disk) started 18.12.91. Much more work and
 * thought has to go into this. Oh, well..
 * 19.12.91  -  works, somewhat. Sometimes I get faults, don't know why.
 *		Found it. Everything seems to work now.
 * 20.12.91  -  Ok, making the swap-device changeable like the root.
 */

/*
 * 05.04.94  -  Multi-page memory management added for v1.1.
 * 		Idea by Alex Bligh (alex@cconcepts.co.uk)
 *
 * 16.07.99  -  Support of BIGMEM added by Gerhard Wichert, Siemens AG
 *		(Gerhard.Wichert@pdb.siemens.de)
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

unsigned long max_mapnr;
unsigned long num_physpages;
unsigned long num_mappedpages;
void * high_memory;
struct page *highmem_start_page;

/*
 * We special-case the C-O-W ZERO_PAGE, because it's such
 * a common occurrence (no need to read the page to know
 * that it's zero - better for the cache and memory subsystem).
 */
static inline void copy_cow_page(struct page * from, struct page * to, unsigned long address)
{
	if (from == ZERO_PAGE(address)) {
		clear_user_highpage(to, address);
		return;
	}
	copy_user_highpage(to, from, address);
}

mem_map_t * mem_map;

/*
 * Called by TLB shootdown 
 */
void __free_pte(pte_t pte)
{
	struct page *page = pte_page(pte);
	if ((!VALID_PAGE(page)) || PageReserved(page))
		return;
	if (pte_dirty(pte))
		set_page_dirty(page);		
	free_page_and_swap_cache(page);
}


/*
 * Note: this doesn't free the actual pages themselves. That
 * has been handled earlier when unmapping all the memory regions.
 */
static inline void free_one_pmd(pmd_t * dir)
{
	pte_t * pte;

	if (pmd_none(*dir))
		return;
	if (pmd_bad(*dir)) {
		pmd_ERROR(*dir);
		pmd_clear(dir);
		return;
	}
	pte = pte_offset(dir, 0);
	pmd_clear(dir);
	pte_free(pte);
}

static inline void free_one_pgd(pgd_t * dir)
{
	int j;
	pmd_t * pmd;

	if (pgd_none(*dir))
		return;
	if (pgd_bad(*dir)) {
		pgd_ERROR(*dir);
		pgd_clear(dir);
		return;
	}
	pmd = pmd_offset(dir, 0);
	pgd_clear(dir);
	for (j = 0; j < PTRS_PER_PMD ; j++) {
		prefetchw(pmd+j+(PREFETCH_STRIDE/16));
		free_one_pmd(pmd+j);
	}
	pmd_free(pmd);
}

/* Low and high watermarks for page table cache.
   The system should try to have pgt_water[0] <= cache elements <= pgt_water[1]
 */
int pgt_cache_water[2] = { 25, 50 };

/* Returns the number of pages freed */
int check_pgt_cache(void)
{
	return do_check_pgt_cache(pgt_cache_water[0], pgt_cache_water[1]);
}


/*
 * This function clears all user-level page tables of a process - this
 * is needed by execve(), so that old pages aren't in the way.
 */
void clear_page_tables(struct mm_struct *mm, unsigned long first, int nr)
{
	pgd_t * page_dir = mm->pgd;

	spin_lock(&mm->page_table_lock);
	page_dir += first;
	do {
		free_one_pgd(page_dir);
		page_dir++;
	} while (--nr);
	spin_unlock(&mm->page_table_lock);

	/* keep the page table cache within bounds */
	check_pgt_cache();
}

#define PTE_TABLE_MASK	((PTRS_PER_PTE-1) * sizeof(pte_t))
#define PMD_TABLE_MASK	((PTRS_PER_PMD-1) * sizeof(pmd_t))

/*
 * copy one vm_area from one task to the other. Assumes the page tables
 * already present in the new task to be cleared in the whole range
 * covered by this vma.
 *
 * 08Jan98 Merged into one routine from several inline routines to reduce
 *         variable count and make things faster. -jj
 *
 * dst->page_table_lock is held on entry and exit,
 * but may be dropped within pmd_alloc() and pte_alloc().
 */
int copy_page_range(struct mm_struct *dst, struct mm_struct *src,
			struct vm_area_struct *vma)
{
	pgd_t * src_pgd, * dst_pgd;
	unsigned long address = vma->vm_start;
	unsigned long end = vma->vm_end;
	unsigned long cow = (vma->vm_flags & (VM_SHARED | VM_MAYWRITE)) == VM_MAYWRITE;

	src_pgd = pgd_offset(src, address)-1;
	dst_pgd = pgd_offset(dst, address)-1;

	for (;;) {
		pmd_t * src_pmd, * dst_pmd;

		src_pgd++; dst_pgd++;
		
		/* copy_pmd_range */
		
		if (pgd_none(*src_pgd))
			goto skip_copy_pmd_range;
		if (pgd_bad(*src_pgd)) {
			pgd_ERROR(*src_pgd);
			pgd_clear(src_pgd);
skip_copy_pmd_range:	address = (address + PGDIR_SIZE) & PGDIR_MASK;
			if (!address || (address >= end))
				goto out;
			continue;
		}

		src_pmd = pmd_offset(src_pgd, address);
		dst_pmd = pmd_alloc(dst, dst_pgd, address);
		if (!dst_pmd)
			goto nomem;

		do {
			pte_t * src_pte, * dst_pte;
		
			/* copy_pte_range */
		
			if (pmd_none(*src_pmd))
				goto skip_copy_pte_range;
			if (pmd_bad(*src_pmd)) {
				pmd_ERROR(*src_pmd);
				pmd_clear(src_pmd);
skip_copy_pte_range:		address = (address + PMD_SIZE) & PMD_MASK;
				if (address >= end)
					goto out;
				goto cont_copy_pmd_range;
			}

			src_pte = pte_offset(src_pmd, address);
			dst_pte = pte_alloc(dst, dst_pmd, address);
			if (!dst_pte)
				goto nomem;

			spin_lock(&src->page_table_lock);			
			do {
				pte_t pte = *src_pte;
				struct page *ptepage;
				
				/* copy_one_pte */

				if (pte_none(pte))
					goto cont_copy_pte_range_noset;
				if (!pte_present(pte)) {
					swap_duplicate(pte_to_swp_entry(pte));
					goto cont_copy_pte_range;
				}
				ptepage = pte_page(pte);
				if ((!VALID_PAGE(ptepage)) || 
				    PageReserved(ptepage))
					goto cont_copy_pte_range;

				/* If it's a COW mapping, write protect it both in the parent and the child */
				if (cow && pte_write(pte)) {
					ptep_set_wrprotect(src_pte);
					pte = *src_pte;
				}

				/* If it's a shared mapping, mark it clean in the child */
				if (vma->vm_flags & VM_SHARED)
					pte = pte_mkclean(pte);
				pte = pte_mkold(pte);
				get_page(ptepage);
				dst->rss++;

cont_copy_pte_range:		set_pte(dst_pte, pte);
cont_copy_pte_range_noset:	address += PAGE_SIZE;
				if (address >= end)
					goto out_unlock;
				src_pte++;
				dst_pte++;
			} while ((unsigned long)src_pte & PTE_TABLE_MASK);
			spin_unlock(&src->page_table_lock);
		
cont_copy_pmd_range:	src_pmd++;
			dst_pmd++;
		} while ((unsigned long)src_pmd & PMD_TABLE_MASK);
	}
out_unlock:
	spin_unlock(&src->page_table_lock);
out:
	return 0;
nomem:
	return -ENOMEM;
}

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

static inline int zap_pte_range(mmu_gather_t *tlb, pmd_t * pmd, unsigned long address, unsigned long size)
{
	unsigned long offset;
	pte_t * ptep;
	int freed = 0;

	if (pmd_none(*pmd))
		return 0;
	if (pmd_bad(*pmd)) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		return 0;
	}
	ptep = pte_offset(pmd, address);
	offset = address & ~PMD_MASK;
	if (offset + size > PMD_SIZE)
		size = PMD_SIZE - offset;
	size &= PAGE_MASK;
	for (offset=0; offset < size; ptep++, offset += PAGE_SIZE) {
		pte_t pte = *ptep;
		if (pte_none(pte))
			continue;
		if (pte_present(pte)) {
			struct page *page = pte_page(pte);
			if (VALID_PAGE(page) && !PageReserved(page))
				freed ++;
			/* This will eventually call __free_pte on the pte. */
			tlb_remove_page(tlb, ptep, address + offset);
		} else {
			free_swap_and_cache(pte_to_swp_entry(pte));
			pte_clear(ptep);
		}
	}

	return freed;
}

static inline int zap_pmd_range(mmu_gather_t *tlb, pgd_t * dir, unsigned long address, unsigned long size)
{
	pmd_t * pmd;
	unsigned long end;
	int freed;

	if (pgd_none(*dir))
		return 0;
	if (pgd_bad(*dir)) {
		pgd_ERROR(*dir);
		pgd_clear(dir);
		return 0;
	}
	pmd = pmd_offset(dir, address);
	end = address + size;
	if (end > ((address + PGDIR_SIZE) & PGDIR_MASK))
		end = ((address + PGDIR_SIZE) & PGDIR_MASK);
	freed = 0;
	do {
		freed += zap_pte_range(tlb, pmd, address, end - address);
		address = (address + PMD_SIZE) & PMD_MASK; 
		pmd++;
	} while (address < end);
	return freed;
}

/*
 * remove user pages in a given range.
 */
void zap_page_range(struct mm_struct *mm, unsigned long address, unsigned long size)
{
	mmu_gather_t *tlb;
	pgd_t * dir;
	unsigned long start = address, end = address + size;
	int freed = 0;

	dir = pgd_offset(mm, address);

	/*
	 * This is a long-lived spinlock. That's fine.
	 * There's no contention, because the page table
	 * lock only protects against kswapd anyway, and
	 * even if kswapd happened to be looking at this
	 * process we _want_ it to get stuck.
	 */
	if (address >= end)
		BUG();
	spin_lock(&mm->page_table_lock);
	flush_cache_range(mm, address, end);
	tlb = tlb_gather_mmu(mm);

	do {
		freed += zap_pmd_range(tlb, dir, address, end - address);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address && (address < end));

	/* this will flush any remaining tlb entries */
	tlb_finish_mmu(tlb, start, end);

	/*
	 * Update rss for the mm_struct (not necessarily current->mm)
	 * Notice that rss is an unsigned long.
	 */
	if (mm->rss > freed)
		mm->rss -= freed;
	else
		mm->rss = 0;
	spin_unlock(&mm->page_table_lock);
}

/*
 * Do a quick page-table lookup for a single page. 
 */
static struct page * follow_page(struct mm_struct *mm, unsigned long address, int write) 
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *ptep, pte;

	pgd = pgd_offset(mm, address);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		goto out;

	pmd = pmd_offset(pgd, address);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		goto out;

	ptep = pte_offset(pmd, address);
	if (!ptep)
		goto out;

	pte = *ptep;
	if (pte_present(pte)) {
		if (!write ||
		    (pte_write(pte) && pte_dirty(pte)))
			return pte_page(pte);
	}

out:
	return 0;
}

/* 
 * Given a physical address, is there a useful struct page pointing to
 * it?  This may become more complex in the future if we start dealing
 * with IO-aperture pages in kiobufs.
 */

static inline struct page * get_page_map(struct page *page)
{
	if (!VALID_PAGE(page))
		return 0;
	return page;
}

/*
 * Please read Documentation/cachetlb.txt before using this function,
 * accessing foreign memory spaces can cause cache coherency problems.
 *
 * Accessing a VM_IO area is even more dangerous, therefore the function
 * fails if pages is != NULL and a VM_IO area is found.
 */
int get_user_pages(struct task_struct *tsk, struct mm_struct *mm, unsigned long start,
		int len, int write, int force, struct page **pages, struct vm_area_struct **vmas)
{
	int i;
	unsigned int flags;

	/*
	 * Require read or write permissions.
	 * If 'force' is set, we only require the "MAY" flags.
	 */
	flags = write ? (VM_WRITE | VM_MAYWRITE) : (VM_READ | VM_MAYREAD);
	flags &= force ? (VM_MAYREAD | VM_MAYWRITE) : (VM_READ | VM_WRITE);
	i = 0;

	do {
		struct vm_area_struct *	vma;

		vma = find_extend_vma(mm, start);

		if ( !vma || (pages && vma->vm_flags & VM_IO) || !(flags & vma->vm_flags) )
			return i ? : -EFAULT;

		spin_lock(&mm->page_table_lock);
		do {
			struct page *map;
			while (!(map = follow_page(mm, start, write))) {
				spin_unlock(&mm->page_table_lock);
				switch (handle_mm_fault(mm, vma, start, write)) {
				case 1:
					tsk->min_flt++;
					break;
				case 2:
					tsk->maj_flt++;
					break;
				case 0:
					if (i) return i;
					return -EFAULT;
				default:
					if (i) return i;
					return -ENOMEM;
				}
				spin_lock(&mm->page_table_lock);
			}
			if (pages) {
				pages[i] = get_page_map(map);
				/* FIXME: call the correct function,
				 * depending on the type of the found page
				 */
				if (!pages[i])
					goto bad_page;
				page_cache_get(pages[i]);
			}
			if (vmas)
				vmas[i] = vma;
			i++;
			start += PAGE_SIZE;
			len--;
		} while(len && start < vma->vm_end);
		spin_unlock(&mm->page_table_lock);
	} while(len);
out:
	return i;

	/*
	 * We found an invalid page in the VMA.  Release all we have
	 * so far and fail.
	 */
bad_page:
	spin_unlock(&mm->page_table_lock);
	while (i--)
		page_cache_release(pages[i]);
	i = -EFAULT;
	goto out;
}

EXPORT_SYMBOL(get_user_pages);

/*
 * Force in an entire range of pages from the current process's user VA,
 * and pin them in physical memory.  
 */
#define dprintk(x...)

int map_user_kiobuf(int rw, struct kiobuf *iobuf, unsigned long va, size_t len)
{
	int pgcount, err;
	struct mm_struct *	mm;
	
	/* Make sure the iobuf is not already mapped somewhere. */
	if (iobuf->nr_pages)
		return -EINVAL;

	mm = current->mm;
	dprintk ("map_user_kiobuf: begin\n");
	
	pgcount = (va + len + PAGE_SIZE - 1)/PAGE_SIZE - va/PAGE_SIZE;
	/* mapping 0 bytes is not permitted */
	if (!pgcount) BUG();
	err = expand_kiobuf(iobuf, pgcount);
	if (err)
		return err;

	iobuf->locked = 0;
	iobuf->offset = va & (PAGE_SIZE-1);
	iobuf->length = len;
	
	/* Try to fault in all of the necessary pages */
	down_read(&mm->mmap_sem);
	/* rw==READ means read from disk, write into memory area */
	err = get_user_pages(current, mm, va, pgcount,
			(rw==READ), 0, iobuf->maplist, NULL);
	up_read(&mm->mmap_sem);
	if (err < 0) {
		unmap_kiobuf(iobuf);
		dprintk ("map_user_kiobuf: end %d\n", err);
		return err;
	}
	iobuf->nr_pages = err;
	while (pgcount--) {
		/* FIXME: flush superflous for rw==READ,
		 * probably wrong function for rw==WRITE
		 */
		flush_dcache_page(iobuf->maplist[pgcount]);
	}
	dprintk ("map_user_kiobuf: end OK\n");
	return 0;
}

/*
 * Mark all of the pages in a kiobuf as dirty 
 *
 * We need to be able to deal with short reads from disk: if an IO error
 * occurs, the number of bytes read into memory may be less than the
 * size of the kiobuf, so we have to stop marking pages dirty once the
 * requested byte count has been reached.
 *
 * Must be called from process context - set_page_dirty() takes VFS locks.
 */

void mark_dirty_kiobuf(struct kiobuf *iobuf, int bytes)
{
	int index, offset, remaining;
	struct page *page;
	
	index = iobuf->offset >> PAGE_SHIFT;
	offset = iobuf->offset & ~PAGE_MASK;
	remaining = bytes;
	if (remaining > iobuf->length)
		remaining = iobuf->length;
	
	while (remaining > 0 && index < iobuf->nr_pages) {
		page = iobuf->maplist[index];
		
		if (!PageReserved(page))
			set_page_dirty(page);

		remaining -= (PAGE_SIZE - offset);
		offset = 0;
		index++;
	}
}

/*
 * Unmap all of the pages referenced by a kiobuf.  We release the pages,
 * and unlock them if they were locked. 
 */

void unmap_kiobuf (struct kiobuf *iobuf) 
{
	int i;
	struct page *map;
	
	for (i = 0; i < iobuf->nr_pages; i++) {
		map = iobuf->maplist[i];
		if (map) {
			if (iobuf->locked)
				UnlockPage(map);
			/* FIXME: cache flush missing for rw==READ
			 * FIXME: call the correct reference counting function
			 */
			page_cache_release(map);
		}
	}
	
	iobuf->nr_pages = 0;
	iobuf->locked = 0;
}


/*
 * Lock down all of the pages of a kiovec for IO.
 *
 * If any page is mapped twice in the kiovec, we return the error -EINVAL.
 *
 * The optional wait parameter causes the lock call to block until all
 * pages can be locked if set.  If wait==0, the lock operation is
 * aborted if any locked pages are found and -EAGAIN is returned.
 */

int lock_kiovec(int nr, struct kiobuf *iovec[], int wait)
{
	struct kiobuf *iobuf;
	int i, j;
	struct page *page, **ppage;
	int doublepage = 0;
	int repeat = 0;
	
 repeat:
	
	for (i = 0; i < nr; i++) {
		iobuf = iovec[i];

		if (iobuf->locked)
			continue;

		ppage = iobuf->maplist;
		for (j = 0; j < iobuf->nr_pages; ppage++, j++) {
			page = *ppage;
			if (!page)
				continue;
			
			if (TryLockPage(page)) {
				while (j--) {
					struct page *tmp = *--ppage;
					if (tmp)
						UnlockPage(tmp);
				}
				goto retry;
			}
		}
		iobuf->locked = 1;
	}

	return 0;
	
 retry:
	
	/* 
	 * We couldn't lock one of the pages.  Undo the locking so far,
	 * wait on the page we got to, and try again.  
	 */
	
	unlock_kiovec(nr, iovec);
	if (!wait)
		return -EAGAIN;
	
	/* 
	 * Did the release also unlock the page we got stuck on?
	 */
	if (!PageLocked(page)) {
		/* 
		 * If so, we may well have the page mapped twice
		 * in the IO address range.  Bad news.  Of
		 * course, it _might_ just be a coincidence,
		 * but if it happens more than once, chances
		 * are we have a double-mapped page. 
		 */
		if (++doublepage >= 3) 
			return -EINVAL;
		
		/* Try again...  */
		wait_on_page(page);
	}
	
	if (++repeat < 16)
		goto repeat;
	return -EAGAIN;
}

/*
 * Unlock all of the pages of a kiovec after IO.
 */

int unlock_kiovec(int nr, struct kiobuf *iovec[])
{
	struct kiobuf *iobuf;
	int i, j;
	struct page *page, **ppage;
	
	for (i = 0; i < nr; i++) {
		iobuf = iovec[i];

		if (!iobuf->locked)
			continue;
		iobuf->locked = 0;
		
		ppage = iobuf->maplist;
		for (j = 0; j < iobuf->nr_pages; ppage++, j++) {
			page = *ppage;
			if (!page)
				continue;
			UnlockPage(page);
		}
	}
	return 0;
}

static inline void zeromap_pte_range(pte_t * pte, unsigned long address,
                                     unsigned long size, pgprot_t prot)
{
	unsigned long end;

	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		pte_t zero_pte = pte_wrprotect(mk_pte(ZERO_PAGE(address), prot));
		pte_t oldpage = ptep_get_and_clear(pte);
		set_pte(pte, zero_pte);
		forget_pte(oldpage);
		address += PAGE_SIZE;
		pte++;
	} while (address && (address < end));
}

static inline int zeromap_pmd_range(struct mm_struct *mm, pmd_t * pmd, unsigned long address,
                                    unsigned long size, pgprot_t prot)
{
	unsigned long end;

	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	do {
		pte_t * pte = pte_alloc(mm, pmd, address);
		if (!pte)
			return -ENOMEM;
		zeromap_pte_range(pte, address, end - address, prot);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
	return 0;
}

int zeromap_page_range(unsigned long address, unsigned long size, pgprot_t prot)
{
	int error = 0;
	pgd_t * dir;
	unsigned long beg = address;
	unsigned long end = address + size;
	struct mm_struct *mm = current->mm;

	dir = pgd_offset(mm, address);
	flush_cache_range(mm, beg, end);
	if (address >= end)
		BUG();

	spin_lock(&mm->page_table_lock);
	do {
		pmd_t *pmd = pmd_alloc(mm, dir, address);
		error = -ENOMEM;
		if (!pmd)
			break;
		error = zeromap_pmd_range(mm, pmd, address, end - address, prot);
		if (error)
			break;
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address && (address < end));
	spin_unlock(&mm->page_table_lock);
	flush_tlb_range(mm, beg, end);
	return error;
}

/*
 * maps a range of physical memory into the requested pages. the old
 * mappings are removed. any references to nonexistent pages results
 * in null mappings (currently treated as "copy-on-access")
 */
static inline void remap_pte_range(pte_t * pte, unsigned long address, unsigned long size,
	unsigned long phys_addr, pgprot_t prot)
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
	unsigned long phys_addr, pgprot_t prot)
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

/*  Note: this is only safe if the mm semaphore is held when called. */
int remap_page_range(unsigned long from, unsigned long phys_addr, unsigned long size, pgprot_t prot)
{
	int error = 0;
	pgd_t * dir;
	unsigned long beg = from;
	unsigned long end = from + size;
	struct mm_struct *mm = current->mm;

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

/*
 * Establish a new mapping:
 *  - flush the old one
 *  - update the page tables
 *  - inform the TLB about the new one
 *
 * We hold the mm semaphore for reading and vma->vm_mm->page_table_lock
 */
static inline void establish_pte(struct vm_area_struct * vma, unsigned long address, pte_t *page_table, pte_t entry)
{
	set_pte(page_table, entry);
	flush_tlb_page(vma, address);
	update_mmu_cache(vma, address, entry);
}

/*
 * We hold the mm semaphore for reading and vma->vm_mm->page_table_lock
 */
static inline void break_cow(struct vm_area_struct * vma, struct page * new_page, unsigned long address, 
		pte_t *page_table)
{
	flush_page_to_ram(new_page);
	flush_cache_page(vma, address);
	establish_pte(vma, address, page_table, pte_mkwrite(pte_mkdirty(mk_pte(new_page, vma->vm_page_prot))));
}

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * Goto-purists beware: the only reason for goto's here is that it results
 * in better assembly code.. The "default" path will see no jumps at all.
 *
 * Note that this routine assumes that the protection checks have been
 * done by the caller (the low-level page fault routine in most cases).
 * Thus we can safely just mark it writable once we've done any necessary
 * COW.
 *
 * We also mark the page dirty at this point even though the page will
 * change only once the write actually happens. This avoids a few races,
 * and potentially makes it more efficient.
 *
 * We hold the mm semaphore and the page_table_lock on entry and exit
 * with the page_table_lock released.
 */
static int do_wp_page(struct mm_struct *mm, struct vm_area_struct * vma,
	unsigned long address, pte_t *page_table, pte_t pte)
{
	struct page *old_page, *new_page;

	old_page = pte_page(pte);
	if (!VALID_PAGE(old_page))
		goto bad_wp_page;

	if (!TryLockPage(old_page)) {
		int reuse = can_share_swap_page(old_page);
		unlock_page(old_page);
		if (reuse) {
			flush_cache_page(vma, address);
			establish_pte(vma, address, page_table, pte_mkyoung(pte_mkdirty(pte_mkwrite(pte))));
			spin_unlock(&mm->page_table_lock);
			return 1;	/* Minor fault */
		}
	}

	/*
	 * Ok, we need to copy. Oh, well..
	 */
	page_cache_get(old_page);
	spin_unlock(&mm->page_table_lock);

	new_page = alloc_page(GFP_HIGHUSER);
	if (!new_page)
		goto no_mem;
	copy_cow_page(old_page,new_page,address);

	/*
	 * Re-check the pte - we dropped the lock
	 */
	spin_lock(&mm->page_table_lock);
	if (pte_same(*page_table, pte)) {
		if (PageReserved(old_page))
			++mm->rss;
		break_cow(vma, new_page, address, page_table);
		lru_cache_add(new_page);

		/* Free the old page.. */
		new_page = old_page;
	}
	spin_unlock(&mm->page_table_lock);
	page_cache_release(new_page);
	page_cache_release(old_page);
	return 1;	/* Minor fault */

bad_wp_page:
	spin_unlock(&mm->page_table_lock);
	printk("do_wp_page: bogus page at address %08lx (page 0x%lx)\n",address,(unsigned long)old_page);
	return -1;
no_mem:
	page_cache_release(old_page);
	return -1;
}

static void vmtruncate_list(struct vm_area_struct *mpnt, unsigned long pgoff)
{
	do {
		struct mm_struct *mm = mpnt->vm_mm;
		unsigned long start = mpnt->vm_start;
		unsigned long end = mpnt->vm_end;
		unsigned long len = end - start;
		unsigned long diff;

		/* mapping wholly truncated? */
		if (mpnt->vm_pgoff >= pgoff) {
			zap_page_range(mm, start, len);
			continue;
		}

		/* mapping wholly unaffected? */
		len = len >> PAGE_SHIFT;
		diff = pgoff - mpnt->vm_pgoff;
		if (diff >= len)
			continue;

		/* Ok, partially affected.. */
		start += diff << PAGE_SHIFT;
		len = (len - diff) << PAGE_SHIFT;
		zap_page_range(mm, start, len);
	} while ((mpnt = mpnt->vm_next_share) != NULL);
}

/*
 * Handle all mappings that got truncated by a "truncate()"
 * system call.
 *
 * NOTE! We have to be ready to update the memory sharing
 * between the file and the memory map for a potential last
 * incomplete page.  Ugly, but necessary.
 */
int vmtruncate(struct inode * inode, loff_t offset)
{
	unsigned long pgoff;
	struct address_space *mapping = inode->i_mapping;
	unsigned long limit;

	if (inode->i_size < offset)
		goto do_expand;
	inode->i_size = offset;
	spin_lock(&mapping->i_shared_lock);
	if (!mapping->i_mmap && !mapping->i_mmap_shared)
		goto out_unlock;

	pgoff = (offset + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	if (mapping->i_mmap != NULL)
		vmtruncate_list(mapping->i_mmap, pgoff);
	if (mapping->i_mmap_shared != NULL)
		vmtruncate_list(mapping->i_mmap_shared, pgoff);

out_unlock:
	spin_unlock(&mapping->i_shared_lock);
	truncate_inode_pages(mapping, offset);
	goto out_truncate;

do_expand:
	limit = current->rlim[RLIMIT_FSIZE].rlim_cur;
	if (limit != RLIM_INFINITY && offset > limit)
		goto out_sig;
	if (offset > inode->i_sb->s_maxbytes)
		goto out;
	inode->i_size = offset;

out_truncate:
	if (inode->i_op && inode->i_op->truncate) {
		lock_kernel();
		inode->i_op->truncate(inode);
		unlock_kernel();
	}
	return 0;
out_sig:
	send_sig(SIGXFSZ, current, 0);
out:
	return -EFBIG;
}

/* 
 * Primitive swap readahead code. We simply read an aligned block of
 * (1 << page_cluster) entries in the swap area. This method is chosen
 * because it doesn't cost us any seek time.  We also make sure to queue
 * the 'original' request together with the readahead ones...  
 */
void swapin_readahead(swp_entry_t entry)
{
	int i, num;
	struct page *new_page;
	unsigned long offset;

	/*
	 * Get the number of handles we should do readahead io to.
	 */
	num = valid_swaphandles(entry, &offset);
	for (i = 0; i < num; offset++, i++) {
		/* Ok, do the async read-ahead now */
		new_page = read_swap_cache_async(SWP_ENTRY(SWP_TYPE(entry), offset));
		if (!new_page)
			break;
		page_cache_release(new_page);
	}
	return;
}

/*
 * We hold the mm semaphore and the page_table_lock on entry and
 * should release the pagetable lock on exit..
 */
static int do_swap_page(struct mm_struct * mm,
	struct vm_area_struct * vma, unsigned long address,
	pte_t * page_table, pte_t orig_pte, int write_access)
{
	struct page *page;
	swp_entry_t entry = pte_to_swp_entry(orig_pte);
	pte_t pte;
	int ret = 1;

	spin_unlock(&mm->page_table_lock);
	page = lookup_swap_cache(entry);
	if (!page) {
		swapin_readahead(entry);
		page = read_swap_cache_async(entry);
		if (!page) {
			/*
			 * Back out if somebody else faulted in this pte while
			 * we released the page table lock.
			 */
			int retval;
			spin_lock(&mm->page_table_lock);
			retval = pte_same(*page_table, orig_pte) ? -1 : 1;
			spin_unlock(&mm->page_table_lock);
			return retval;
		}

		/* Had to read the page from swap area: Major fault */
		ret = 2;
	}

	mark_page_accessed(page);

	lock_page(page);

	/*
	 * Back out if somebody else faulted in this pte while we
	 * released the page table lock.
	 */
	spin_lock(&mm->page_table_lock);
	if (!pte_same(*page_table, orig_pte)) {
		spin_unlock(&mm->page_table_lock);
		unlock_page(page);
		page_cache_release(page);
		return 1;
	}

	/* The page isn't present yet, go ahead with the fault. */
		
	swap_free(entry);
	if (vm_swap_full())
		remove_exclusive_swap_page(page);

	mm->rss++;
	pte = mk_pte(page, vma->vm_page_prot);
	if (write_access && can_share_swap_page(page))
		pte = pte_mkdirty(pte_mkwrite(pte));
	unlock_page(page);

	flush_page_to_ram(page);
	flush_icache_page(vma, page);
	set_pte(page_table, pte);

	/* No need to invalidate - it was non-present before */
	update_mmu_cache(vma, address, pte);
	spin_unlock(&mm->page_table_lock);
	return ret;
}

/*
 * We are called with the MM semaphore and page_table_lock
 * spinlock held to protect against concurrent faults in
 * multithreaded programs. 
 */
static int do_anonymous_page(struct mm_struct * mm, struct vm_area_struct * vma, pte_t *page_table, int write_access, unsigned long addr)
{
	pte_t entry;

	/* Read-only mapping of ZERO_PAGE. */
	entry = pte_wrprotect(mk_pte(ZERO_PAGE(addr), vma->vm_page_prot));

	/* ..except if it's a write access */
	if (write_access) {
		struct page *page;

		/* Allocate our own private page. */
		spin_unlock(&mm->page_table_lock);

		page = alloc_page(GFP_HIGHUSER);
		if (!page)
			goto no_mem;
		clear_user_highpage(page, addr);

		spin_lock(&mm->page_table_lock);
		if (!pte_none(*page_table)) {
			page_cache_release(page);
			spin_unlock(&mm->page_table_lock);
			return 1;
		}
		mm->rss++;
		flush_page_to_ram(page);
		entry = pte_mkwrite(pte_mkdirty(mk_pte(page, vma->vm_page_prot)));
		lru_cache_add(page);
		mark_page_accessed(page);
	}

	set_pte(page_table, entry);

	/* No need to invalidate - it was non-present before */
	update_mmu_cache(vma, addr, entry);
	spin_unlock(&mm->page_table_lock);
	return 1;	/* Minor fault */

no_mem:
	return -1;
}

/*
 * do_no_page() tries to create a new page mapping. It aggressively
 * tries to share with existing pages, but makes a separate copy if
 * the "write_access" parameter is true in order to avoid the next
 * page fault.
 *
 * As this is called only for pages that do not currently exist, we
 * do not need to flush old virtual caches or the TLB.
 *
 * This is called with the MM semaphore held and the page table
 * spinlock held. Exit with the spinlock released.
 */
static int do_no_page(struct mm_struct * mm, struct vm_area_struct * vma,
	unsigned long address, int write_access, pte_t *page_table)
{
	struct page * new_page;
	pte_t entry;

	if (!vma->vm_ops || !vma->vm_ops->nopage)
		return do_anonymous_page(mm, vma, page_table, write_access, address);
	spin_unlock(&mm->page_table_lock);

	new_page = vma->vm_ops->nopage(vma, address & PAGE_MASK, 0);

	if (new_page == NULL)	/* no page was available -- SIGBUS */
		return 0;
	if (new_page == NOPAGE_OOM)
		return -1;

	/*
	 * Should we do an early C-O-W break?
	 */
	if (write_access && !(vma->vm_flags & VM_SHARED)) {
		struct page * page = alloc_page(GFP_HIGHUSER);
		if (!page) {
			page_cache_release(new_page);
			return -1;
		}
		copy_user_highpage(page, new_page, address);
		page_cache_release(new_page);
		lru_cache_add(page);
		new_page = page;
	}

	spin_lock(&mm->page_table_lock);
	/*
	 * This silly early PAGE_DIRTY setting removes a race
	 * due to the bad i386 page protection. But it's valid
	 * for other architectures too.
	 *
	 * Note that if write_access is true, we either now have
	 * an exclusive copy of the page, or this is a shared mapping,
	 * so we can make it writable and dirty to avoid having to
	 * handle that later.
	 */
	/* Only go through if we didn't race with anybody else... */
	if (pte_none(*page_table)) {
		if (!PageReserved(new_page))
			++mm->rss;
		flush_page_to_ram(new_page);
		flush_icache_page(vma, new_page);
		entry = mk_pte(new_page, vma->vm_page_prot);
		if (write_access)
			entry = pte_mkwrite(pte_mkdirty(entry));
		set_pte(page_table, entry);
	} else {
		/* One of our sibling threads was faster, back out. */
		page_cache_release(new_page);
		spin_unlock(&mm->page_table_lock);
		return 1;
	}

	/* no need to invalidate: a not-present page shouldn't be cached */
	update_mmu_cache(vma, address, entry);
	spin_unlock(&mm->page_table_lock);
	return 2;	/* Major fault */
}

/*
 * These routines also need to handle stuff like marking pages dirty
 * and/or accessed for architectures that don't do it in hardware (most
 * RISC architectures).  The early dirtying is also good on the i386.
 *
 * There is also a hook called "update_mmu_cache()" that architectures
 * with external mmu caches can use to update those (ie the Sparc or
 * PowerPC hashed page tables that act as extended TLBs).
 *
 * Note the "page_table_lock". It is to protect against kswapd removing
 * pages from under us. Note that kswapd only ever _removes_ pages, never
 * adds them. As such, once we have noticed that the page is not present,
 * we can drop the lock early.
 *
 * The adding of pages is protected by the MM semaphore (which we hold),
 * so we don't need to worry about a page being suddenly been added into
 * our VM.
 *
 * We enter with the pagetable spinlock held, we are supposed to
 * release it when done.
 */
static inline int handle_pte_fault(struct mm_struct *mm,
	struct vm_area_struct * vma, unsigned long address,
	int write_access, pte_t * pte)
{
	pte_t entry;

	entry = *pte;
	if (!pte_present(entry)) {
		/*
		 * If it truly wasn't present, we know that kswapd
		 * and the PTE updates will not touch it later. So
		 * drop the lock.
		 */
		if (pte_none(entry))
			return do_no_page(mm, vma, address, write_access, pte);
		return do_swap_page(mm, vma, address, pte, entry, write_access);
	}

	if (write_access) {
		if (!pte_write(entry))
			return do_wp_page(mm, vma, address, pte, entry);

		entry = pte_mkdirty(entry);
	}
	entry = pte_mkyoung(entry);
	establish_pte(vma, address, pte, entry);
	spin_unlock(&mm->page_table_lock);
	return 1;
}

/*
 * By the time we get here, we already hold the mm semaphore
 */
int handle_mm_fault(struct mm_struct *mm, struct vm_area_struct * vma,
	unsigned long address, int write_access)
{
	pgd_t *pgd;
	pmd_t *pmd;

	current->state = TASK_RUNNING;
	pgd = pgd_offset(mm, address);

	/*
	 * We need the page table lock to synchronize with kswapd
	 * and the SMP-safe atomic PTE updates.
	 */
	spin_lock(&mm->page_table_lock);
	pmd = pmd_alloc(mm, pgd, address);

	if (pmd) {
		pte_t * pte = pte_alloc(mm, pmd, address);
		if (pte)
			return handle_pte_fault(mm, vma, address, write_access, pte);
	}
	spin_unlock(&mm->page_table_lock);
	return -1;
}

/*
 * Allocate page middle directory.
 *
 * We've already handled the fast-path in-line, and we own the
 * page table lock.
 *
 * On a two-level page table, this ends up actually being entirely
 * optimized away.
 */
pmd_t *__pmd_alloc(struct mm_struct *mm, pgd_t *pgd, unsigned long address)
{
	pmd_t *new;

	/* "fast" allocation can happen without dropping the lock.. */
	new = pmd_alloc_one_fast(mm, address);
	if (!new) {
		spin_unlock(&mm->page_table_lock);
		new = pmd_alloc_one(mm, address);
		spin_lock(&mm->page_table_lock);
		if (!new)
			return NULL;

		/*
		 * Because we dropped the lock, we should re-check the
		 * entry, as somebody else could have populated it..
		 */
		if (!pgd_none(*pgd)) {
			pmd_free(new);
			check_pgt_cache();
			goto out;
		}
	}
	pgd_populate(mm, pgd, new);
out:
	return pmd_offset(pgd, address);
}

/*
 * Allocate the page table directory.
 *
 * We've already handled the fast-path in-line, and we own the
 * page table lock.
 */
pte_t *pte_alloc(struct mm_struct *mm, pmd_t *pmd, unsigned long address)
{
	if (pmd_none(*pmd)) {
		pte_t *new;

		/* "fast" allocation can happen without dropping the lock.. */
		new = pte_alloc_one_fast(mm, address);
		if (!new) {
			spin_unlock(&mm->page_table_lock);
			new = pte_alloc_one(mm, address);
			spin_lock(&mm->page_table_lock);
			if (!new)
				return NULL;

			/*
			 * Because we dropped the lock, we should re-check the
			 * entry, as somebody else could have populated it..
			 */
			if (!pmd_none(*pmd)) {
				pte_free(new);
				check_pgt_cache();
				goto out;
			}
		}
		pmd_populate(mm, pmd, new);
	}
out:
	return pte_offset(pmd, address);
}

int make_pages_present(unsigned long addr, unsigned long end)
{
	int ret, len, write;
	struct vm_area_struct * vma;

	vma = find_vma(current->mm, addr);
	write = (vma->vm_flags & VM_WRITE) != 0;
	if (addr >= end)
		BUG();
	if (end > vma->vm_end)
		BUG();
	len = (end+PAGE_SIZE-1)/PAGE_SIZE-addr/PAGE_SIZE;
	ret = get_user_pages(current, current->mm, addr,
			len, write, 0, NULL, NULL);
	return ret == len ? 0 : -1;
}

struct page * vmalloc_to_page(void * vmalloc_addr)
{
	unsigned long addr = (unsigned long) vmalloc_addr;
	struct page *page = NULL;
	pmd_t *pmd;
	pte_t *pte;
	pgd_t *pgd;
	
	pgd = pgd_offset_k(addr);
	if (!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, addr);
		if (!pmd_none(*pmd)) {
			pte = pte_offset(pmd, addr);
			if (pte_present(*pte)) {
				page = pte_page(*pte);
			}
		}
	}
	return page;
}
