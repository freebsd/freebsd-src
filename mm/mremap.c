/*
 *	linux/mm/remap.c
 *
 *	(C) Copyright 1996 Linus Torvalds
 */

#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/swap.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

extern int vm_enough_memory(long pages);

static inline pte_t *get_one_pte(struct mm_struct *mm, unsigned long addr)
{
	pgd_t * pgd;
	pmd_t * pmd;
	pte_t * pte = NULL;

	pgd = pgd_offset(mm, addr);
	if (pgd_none(*pgd))
		goto end;
	if (pgd_bad(*pgd)) {
		pgd_ERROR(*pgd);
		pgd_clear(pgd);
		goto end;
	}

	pmd = pmd_offset(pgd, addr);
	if (pmd_none(*pmd))
		goto end;
	if (pmd_bad(*pmd)) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		goto end;
	}

	pte = pte_offset(pmd, addr);
	if (pte_none(*pte))
		pte = NULL;
end:
	return pte;
}

static inline pte_t *alloc_one_pte(struct mm_struct *mm, unsigned long addr)
{
	pmd_t * pmd;
	pte_t * pte = NULL;

	pmd = pmd_alloc(mm, pgd_offset(mm, addr), addr);
	if (pmd)
		pte = pte_alloc(mm, pmd, addr);
	return pte;
}

static inline int copy_one_pte(struct mm_struct *mm, pte_t * src, pte_t * dst)
{
	int error = 0;
	pte_t pte;

	if (!pte_none(*src)) {
		pte = ptep_get_and_clear(src);
		if (!dst) {
			/* No dest?  We must put it back. */
			dst = src;
			error++;
		}
		set_pte(dst, pte);
	}
	return error;
}

static int move_one_page(struct mm_struct *mm, unsigned long old_addr, unsigned long new_addr)
{
	int error = 0;
	pte_t * src, * dst;

	spin_lock(&mm->page_table_lock);
	src = get_one_pte(mm, old_addr);
	if (src) {
		dst = alloc_one_pte(mm, new_addr);
		src = get_one_pte(mm, old_addr);
		if (src) 
			error = copy_one_pte(mm, src, dst);
	}
	spin_unlock(&mm->page_table_lock);
	return error;
}

static int move_page_tables(struct mm_struct * mm,
	unsigned long new_addr, unsigned long old_addr, unsigned long len)
{
	unsigned long offset = len;

	flush_cache_range(mm, old_addr, old_addr + len);

	/*
	 * This is not the clever way to do this, but we're taking the
	 * easy way out on the assumption that most remappings will be
	 * only a few pages.. This also makes error recovery easier.
	 */
	while (offset) {
		offset -= PAGE_SIZE;
		if (move_one_page(mm, old_addr + offset, new_addr + offset))
			goto oops_we_failed;
	}
	flush_tlb_range(mm, old_addr, old_addr + len);
	return 0;

	/*
	 * Ok, the move failed because we didn't have enough pages for
	 * the new page table tree. This is unlikely, but we have to
	 * take the possibility into account. In that case we just move
	 * all the pages back (this will work, because we still have
	 * the old page tables)
	 */
oops_we_failed:
	flush_cache_range(mm, new_addr, new_addr + len);
	while ((offset += PAGE_SIZE) < len)
		move_one_page(mm, new_addr + offset, old_addr + offset);
	zap_page_range(mm, new_addr, len);
	return -1;
}

static inline unsigned long move_vma(struct vm_area_struct * vma,
	unsigned long addr, unsigned long old_len, unsigned long new_len,
	unsigned long new_addr)
{
	struct mm_struct * mm = vma->vm_mm;
	struct vm_area_struct * new_vma, * next, * prev;
	int allocated_vma;

	new_vma = NULL;
	next = find_vma_prev(mm, new_addr, &prev);
	if (next) {
		if (prev && prev->vm_end == new_addr &&
		    can_vma_merge(prev, vma->vm_flags) && !vma->vm_file && !(vma->vm_flags & VM_SHARED)) {
			spin_lock(&mm->page_table_lock);
			prev->vm_end = new_addr + new_len;
			spin_unlock(&mm->page_table_lock);
			new_vma = prev;
			if (next != prev->vm_next)
				BUG();
			if (prev->vm_end == next->vm_start && can_vma_merge(next, prev->vm_flags)) {
				spin_lock(&mm->page_table_lock);
				prev->vm_end = next->vm_end;
				__vma_unlink(mm, next, prev);
				spin_unlock(&mm->page_table_lock);

				mm->map_count--;
				kmem_cache_free(vm_area_cachep, next);
			}
		} else if (next->vm_start == new_addr + new_len &&
			   can_vma_merge(next, vma->vm_flags) && !vma->vm_file && !(vma->vm_flags & VM_SHARED)) {
			spin_lock(&mm->page_table_lock);
			next->vm_start = new_addr;
			spin_unlock(&mm->page_table_lock);
			new_vma = next;
		}
	} else {
		prev = find_vma(mm, new_addr-1);
		if (prev && prev->vm_end == new_addr &&
		    can_vma_merge(prev, vma->vm_flags) && !vma->vm_file && !(vma->vm_flags & VM_SHARED)) {
			spin_lock(&mm->page_table_lock);
			prev->vm_end = new_addr + new_len;
			spin_unlock(&mm->page_table_lock);
			new_vma = prev;
		}
	}

	allocated_vma = 0;
	if (!new_vma) {
		new_vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
		if (!new_vma)
			goto out;
		allocated_vma = 1;
	}

	if (!move_page_tables(current->mm, new_addr, addr, old_len)) {
		unsigned long vm_locked = vma->vm_flags & VM_LOCKED;

		if (allocated_vma) {
			*new_vma = *vma;
			new_vma->vm_start = new_addr;
			new_vma->vm_end = new_addr+new_len;
			new_vma->vm_pgoff += (addr-vma->vm_start) >> PAGE_SHIFT;
			new_vma->vm_raend = 0;
			if (new_vma->vm_file)
				get_file(new_vma->vm_file);
			if (new_vma->vm_ops && new_vma->vm_ops->open)
				new_vma->vm_ops->open(new_vma);
			insert_vm_struct(current->mm, new_vma);
		}

		do_munmap(current->mm, addr, old_len);

		current->mm->total_vm += new_len >> PAGE_SHIFT;
		if (vm_locked) {
			current->mm->locked_vm += new_len >> PAGE_SHIFT;
			if (new_len > old_len)
				make_pages_present(new_addr + old_len,
						   new_addr + new_len);
		}
		return new_addr;
	}
	if (allocated_vma)
		kmem_cache_free(vm_area_cachep, new_vma);
 out:
	return -ENOMEM;
}

/*
 * Expand (or shrink) an existing mapping, potentially moving it at the
 * same time (controlled by the MREMAP_MAYMOVE flag and available VM space)
 *
 * MREMAP_FIXED option added 5-Dec-1999 by Benjamin LaHaise
 * This option implies MREMAP_MAYMOVE.
 */
unsigned long do_mremap(unsigned long addr,
	unsigned long old_len, unsigned long new_len,
	unsigned long flags, unsigned long new_addr)
{
	struct vm_area_struct *vma;
	unsigned long ret = -EINVAL;

	if (flags & ~(MREMAP_FIXED | MREMAP_MAYMOVE))
		goto out;

	if (addr & ~PAGE_MASK)
		goto out;

	old_len = PAGE_ALIGN(old_len);
	new_len = PAGE_ALIGN(new_len);

	/* new_addr is only valid if MREMAP_FIXED is specified */
	if (flags & MREMAP_FIXED) {
		if (new_addr & ~PAGE_MASK)
			goto out;
		if (!(flags & MREMAP_MAYMOVE))
			goto out;

		if (new_len > TASK_SIZE || new_addr > TASK_SIZE - new_len)
			goto out;
		/*
		 * Allow new_len == 0 only if new_addr == addr
		 * to preserve truncation in place (that was working
		 * safe and some app may depend on it).
		 */
		if (unlikely(!new_len && new_addr != addr))
			goto out;

		/* Check if the location we're moving into overlaps the
		 * old location at all, and fail if it does.
		 */
		if ((new_addr <= addr) && (new_addr+new_len) > addr)
			goto out;

		if ((addr <= new_addr) && (addr+old_len) > new_addr)
			goto out;

		ret = do_munmap(current->mm, new_addr, new_len);
		if (ret && new_len)
			goto out;
	}

	/*
	 * Always allow a shrinking remap: that just unmaps
	 * the unnecessary pages..
	 */
	if (old_len >= new_len) {
		ret = do_munmap(current->mm, addr+new_len, old_len - new_len);
		if (ret && old_len != new_len)
			goto out;
		ret = addr;
		if (!(flags & MREMAP_FIXED) || (new_addr == addr))
			goto out;
	}

	/*
	 * Ok, we need to grow..  or relocate.
	 */
	ret = -EFAULT;
	vma = find_vma(current->mm, addr);
	if (!vma || vma->vm_start > addr)
		goto out;
	/* We can't remap across vm area boundaries */
	if (old_len > vma->vm_end - addr)
		goto out;
	if (vma->vm_flags & VM_DONTEXPAND) {
		if (new_len > old_len)
			goto out;
	}
	if (vma->vm_flags & VM_LOCKED) {
		unsigned long locked = current->mm->locked_vm << PAGE_SHIFT;
		locked += new_len - old_len;
		ret = -EAGAIN;
		if (locked > current->rlim[RLIMIT_MEMLOCK].rlim_cur)
			goto out;
	}
	ret = -ENOMEM;
	if ((current->mm->total_vm << PAGE_SHIFT) + (new_len - old_len)
	    > current->rlim[RLIMIT_AS].rlim_cur)
		goto out;
	/* Private writable mapping? Check memory availability.. */
	if ((vma->vm_flags & (VM_SHARED | VM_WRITE)) == VM_WRITE &&
	    !(flags & MAP_NORESERVE)				 &&
	    !vm_enough_memory((new_len - old_len) >> PAGE_SHIFT))
		goto out;

	/* old_len exactly to the end of the area..
	 * And we're not relocating the area.
	 */
	if (old_len == vma->vm_end - addr &&
	    !((flags & MREMAP_FIXED) && (addr != new_addr)) &&
	    (old_len != new_len || !(flags & MREMAP_MAYMOVE))) {
		unsigned long max_addr = TASK_SIZE;
		if (vma->vm_next)
			max_addr = vma->vm_next->vm_start;
		/* can we just expand the current mapping? */
		if (max_addr - addr >= new_len) {
			int pages = (new_len - old_len) >> PAGE_SHIFT;
			spin_lock(&vma->vm_mm->page_table_lock);
			vma->vm_end = addr + new_len;
			spin_unlock(&vma->vm_mm->page_table_lock);
			current->mm->total_vm += pages;
			if (vma->vm_flags & VM_LOCKED) {
				current->mm->locked_vm += pages;
				make_pages_present(addr + old_len,
						   addr + new_len);
			}
			ret = addr;
			goto out;
		}
	}

	/*
	 * We weren't able to just expand or shrink the area,
	 * we need to create a new one and move it..
	 */
	ret = -ENOMEM;
	if (flags & MREMAP_MAYMOVE) {
		if (!(flags & MREMAP_FIXED)) {
			unsigned long map_flags = 0;
			if (vma->vm_flags & VM_SHARED)
				map_flags |= MAP_SHARED;

			new_addr = get_unmapped_area(vma->vm_file, 0, new_len, vma->vm_pgoff, map_flags);
			ret = new_addr;
			if (new_addr & ~PAGE_MASK)
				goto out;
		}
		ret = move_vma(vma, addr, old_len, new_len, new_addr);
	}
out:
	return ret;
}

asmlinkage unsigned long sys_mremap(unsigned long addr,
	unsigned long old_len, unsigned long new_len,
	unsigned long flags, unsigned long new_addr)
{
	unsigned long ret;

	down_write(&current->mm->mmap_sem);
	ret = do_mremap(addr, old_len, new_len, flags, new_addr);
	up_write(&current->mm->mmap_sem);
	return ret;
}
