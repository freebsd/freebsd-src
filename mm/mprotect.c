/*
 *	linux/mm/mprotect.c
 *
 *  (C) Copyright 1994 Linus Torvalds
 */
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/shm.h>
#include <linux/mman.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>

static inline void change_pte_range(pmd_t * pmd, unsigned long address,
	unsigned long size, pgprot_t newprot)
{
	pte_t * pte;
	unsigned long end;

	if (pmd_none(*pmd))
		return;
	if (pmd_bad(*pmd)) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		return;
	}
	pte = pte_offset(pmd, address);
	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		if (pte_present(*pte)) {
			pte_t entry;

			/* Avoid an SMP race with hardware updated dirty/clean
			 * bits by wiping the pte and then setting the new pte
			 * into place.
			 */
			entry = ptep_get_and_clear(pte);
			set_pte(pte, pte_modify(entry, newprot));
		}
		address += PAGE_SIZE;
		pte++;
	} while (address && (address < end));
}

static inline void change_pmd_range(pgd_t * pgd, unsigned long address,
	unsigned long size, pgprot_t newprot)
{
	pmd_t * pmd;
	unsigned long end;

	if (pgd_none(*pgd))
		return;
	if (pgd_bad(*pgd)) {
		pgd_ERROR(*pgd);
		pgd_clear(pgd);
		return;
	}
	pmd = pmd_offset(pgd, address);
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	do {
		change_pte_range(pmd, address, end - address, newprot);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
}

static void change_protection(unsigned long start, unsigned long end, pgprot_t newprot)
{
	pgd_t *dir;
	unsigned long beg = start;

	dir = pgd_offset(current->mm, start);
	flush_cache_range(current->mm, beg, end);
	if (start >= end)
		BUG();
	spin_lock(&current->mm->page_table_lock);
	do {
		change_pmd_range(dir, start, end - start, newprot);
		start = (start + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (start && (start < end));
	spin_unlock(&current->mm->page_table_lock);
	flush_tlb_range(current->mm, beg, end);
	return;
}

static inline int mprotect_fixup_all(struct vm_area_struct * vma, struct vm_area_struct ** pprev,
	int newflags, pgprot_t prot)
{
	struct vm_area_struct * prev = *pprev;
	struct mm_struct * mm = vma->vm_mm;

	if (prev && prev->vm_end == vma->vm_start && can_vma_merge(prev, newflags) &&
	    !vma->vm_file && !(vma->vm_flags & VM_SHARED)) {
		spin_lock(&mm->page_table_lock);
		prev->vm_end = vma->vm_end;
		__vma_unlink(mm, vma, prev);
		spin_unlock(&mm->page_table_lock);

		kmem_cache_free(vm_area_cachep, vma);
		mm->map_count--;

		return 0;
	}

	spin_lock(&mm->page_table_lock);
	vma->vm_flags = newflags;
	vma->vm_page_prot = prot;
	spin_unlock(&mm->page_table_lock);

	*pprev = vma;

	return 0;
}

static inline int mprotect_fixup_start(struct vm_area_struct * vma, struct vm_area_struct ** pprev,
	unsigned long end,
	int newflags, pgprot_t prot)
{
	struct vm_area_struct * n, * prev = *pprev;

	*pprev = vma;

	if (prev && prev->vm_end == vma->vm_start && can_vma_merge(prev, newflags) &&
	    !vma->vm_file && !(vma->vm_flags & VM_SHARED)) {
		spin_lock(&vma->vm_mm->page_table_lock);
		prev->vm_end = end;
		vma->vm_start = end;
		spin_unlock(&vma->vm_mm->page_table_lock);

		return 0;
	}
	n = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!n)
		return -ENOMEM;
	*n = *vma;
	n->vm_end = end;
	n->vm_flags = newflags;
	n->vm_raend = 0;
	n->vm_page_prot = prot;
	if (n->vm_file)
		get_file(n->vm_file);
	if (n->vm_ops && n->vm_ops->open)
		n->vm_ops->open(n);
	vma->vm_pgoff += (end - vma->vm_start) >> PAGE_SHIFT;
	lock_vma_mappings(vma);
	spin_lock(&vma->vm_mm->page_table_lock);
	vma->vm_start = end;
	__insert_vm_struct(current->mm, n);
	spin_unlock(&vma->vm_mm->page_table_lock);
	unlock_vma_mappings(vma);

	return 0;
}

static inline int mprotect_fixup_end(struct vm_area_struct * vma, struct vm_area_struct ** pprev,
	unsigned long start,
	int newflags, pgprot_t prot)
{
	struct vm_area_struct * n;

	n = kmem_cache_alloc(vm_area_cachep, GFP_KERNEL);
	if (!n)
		return -ENOMEM;
	*n = *vma;
	n->vm_start = start;
	n->vm_pgoff += (n->vm_start - vma->vm_start) >> PAGE_SHIFT;
	n->vm_flags = newflags;
	n->vm_raend = 0;
	n->vm_page_prot = prot;
	if (n->vm_file)
		get_file(n->vm_file);
	if (n->vm_ops && n->vm_ops->open)
		n->vm_ops->open(n);
	lock_vma_mappings(vma);
	spin_lock(&vma->vm_mm->page_table_lock);
	vma->vm_end = start;
	__insert_vm_struct(current->mm, n);
	spin_unlock(&vma->vm_mm->page_table_lock);
	unlock_vma_mappings(vma);

	*pprev = n;

	return 0;
}

static inline int mprotect_fixup_middle(struct vm_area_struct * vma, struct vm_area_struct ** pprev,
	unsigned long start, unsigned long end,
	int newflags, pgprot_t prot)
{
	struct vm_area_struct * left, * right;

	left = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!left)
		return -ENOMEM;
	right = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!right) {
		kmem_cache_free(vm_area_cachep, left);
		return -ENOMEM;
	}
	*left = *vma;
	*right = *vma;
	left->vm_end = start;
	right->vm_start = end;
	right->vm_pgoff += (right->vm_start - left->vm_start) >> PAGE_SHIFT;
	left->vm_raend = 0;
	right->vm_raend = 0;
	if (vma->vm_file)
		atomic_add(2,&vma->vm_file->f_count);
	if (vma->vm_ops && vma->vm_ops->open) {
		vma->vm_ops->open(left);
		vma->vm_ops->open(right);
	}
	vma->vm_pgoff += (start - vma->vm_start) >> PAGE_SHIFT;
	vma->vm_raend = 0;
	vma->vm_page_prot = prot;
	lock_vma_mappings(vma);
	spin_lock(&vma->vm_mm->page_table_lock);
	vma->vm_start = start;
	vma->vm_end = end;
	vma->vm_flags = newflags;
	__insert_vm_struct(current->mm, left);
	__insert_vm_struct(current->mm, right);
	spin_unlock(&vma->vm_mm->page_table_lock);
	unlock_vma_mappings(vma);

	*pprev = right;

	return 0;
}

static int mprotect_fixup(struct vm_area_struct * vma, struct vm_area_struct ** pprev,
	unsigned long start, unsigned long end, unsigned int newflags)
{
	pgprot_t newprot;
	int error;

	if (newflags == vma->vm_flags) {
		*pprev = vma;
		return 0;
	}
	newprot = protection_map[newflags & 0xf];
	if (start == vma->vm_start) {
		if (end == vma->vm_end)
			error = mprotect_fixup_all(vma, pprev, newflags, newprot);
		else
			error = mprotect_fixup_start(vma, pprev, end, newflags, newprot);
	} else if (end == vma->vm_end)
		error = mprotect_fixup_end(vma, pprev, start, newflags, newprot);
	else
		error = mprotect_fixup_middle(vma, pprev, start, end, newflags, newprot);

	if (error)
		return error;

	change_protection(start, end, newprot);
	return 0;
}

asmlinkage long sys_mprotect(unsigned long start, size_t len, unsigned long prot)
{
	unsigned long nstart, end, tmp;
	struct vm_area_struct * vma, * next, * prev;
	int error = -EINVAL;

	if (start & ~PAGE_MASK)
		return -EINVAL;
	len = PAGE_ALIGN(len);
	end = start + len;
	if (end < start)
		return -ENOMEM;
	if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return -EINVAL;
	if (end == start)
		return 0;

	down_write(&current->mm->mmap_sem);

	vma = find_vma_prev(current->mm, start, &prev);
	error = -ENOMEM;
	if (!vma || vma->vm_start > start)
		goto out;

	for (nstart = start ; ; ) {
		unsigned int newflags;
		int last = 0;

		/* Here we know that  vma->vm_start <= nstart < vma->vm_end. */

		newflags = prot | (vma->vm_flags & ~(PROT_READ | PROT_WRITE | PROT_EXEC));
		if ((newflags & ~(newflags >> 4)) & 0xf) {
			error = -EACCES;
			goto out;
		}

		if (vma->vm_end > end) {
			error = mprotect_fixup(vma, &prev, nstart, end, newflags);
			goto out;
		}
		if (vma->vm_end == end)
			last = 1;

		tmp = vma->vm_end;
		next = vma->vm_next;
		error = mprotect_fixup(vma, &prev, nstart, tmp, newflags);
		if (error)
			goto out;
		if (last)
			break;
		nstart = tmp;
		vma = next;
		if (!vma || vma->vm_start != nstart) {
			error = -ENOMEM;
			goto out;
		}
	}
	if (next && prev->vm_end == next->vm_start && can_vma_merge(next, prev->vm_flags) &&
	    !prev->vm_file && !(prev->vm_flags & VM_SHARED)) {
		spin_lock(&prev->vm_mm->page_table_lock);
		prev->vm_end = next->vm_end;
		__vma_unlink(prev->vm_mm, next, prev);
		spin_unlock(&prev->vm_mm->page_table_lock);

		kmem_cache_free(vm_area_cachep, next);
		prev->vm_mm->map_count--;
	}
out:
	up_write(&current->mm->mmap_sem);
	return error;
}
