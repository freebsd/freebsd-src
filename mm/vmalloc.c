/*
 *  linux/mm/vmalloc.c
 *
 *  Copyright (C) 1993  Linus Torvalds
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *  SMP-safe vmalloc/vfree/ioremap, Tigran Aivazian <tigran@veritas.com>, May 2000
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/highmem.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

rwlock_t vmlist_lock = RW_LOCK_UNLOCKED;
struct vm_struct * vmlist;

static inline void free_area_pte(pmd_t * pmd, unsigned long address, unsigned long size)
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
		pte_t page;
		page = ptep_get_and_clear(pte);
		address += PAGE_SIZE;
		pte++;
		if (pte_none(page))
			continue;
		if (pte_present(page)) {
			struct page *ptpage = pte_page(page);
			if (VALID_PAGE(ptpage) && (!PageReserved(ptpage)))
				__free_page(ptpage);
			continue;
		}
		printk(KERN_CRIT "Whee.. Swapped out page in kernel page table\n");
	} while (address < end);
}

static inline void free_area_pmd(pgd_t * dir, unsigned long address, unsigned long size)
{
	pmd_t * pmd;
	unsigned long end;

	if (pgd_none(*dir))
		return;
	if (pgd_bad(*dir)) {
		pgd_ERROR(*dir);
		pgd_clear(dir);
		return;
	}
	pmd = pmd_offset(dir, address);
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	do {
		free_area_pte(pmd, address, end - address);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);
}

void vmfree_area_pages(unsigned long address, unsigned long size)
{
	pgd_t * dir;
	unsigned long end = address + size;

	dir = pgd_offset_k(address);
	flush_cache_all();
	do {
		free_area_pmd(dir, address, end - address);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address && (address < end));
	flush_tlb_all();
}

static inline int alloc_area_pte (pte_t * pte, unsigned long address,
			unsigned long size, int gfp_mask,
			pgprot_t prot, struct page ***pages)
{
	unsigned long end;

	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;
	do {
		struct page * page;

		if (!pages) {
			spin_unlock(&init_mm.page_table_lock);
			page = alloc_page(gfp_mask);
			spin_lock(&init_mm.page_table_lock);
		} else {
			page = (**pages);
			(*pages)++;

			/* Add a reference to the page so we can free later */
			if (page)
				atomic_inc(&page->count);

		}
		if (!pte_none(*pte))
			printk(KERN_ERR "alloc_area_pte: page already exists\n");
		if (!page)
			return -ENOMEM;
		set_pte(pte, mk_pte(page, prot));
		address += PAGE_SIZE;
		pte++;
	} while (address < end);
	return 0;
}

static inline int alloc_area_pmd(pmd_t * pmd, unsigned long address,
			unsigned long size, int gfp_mask,
			pgprot_t prot, struct page ***pages)
{
	unsigned long end;

	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;
	do {
		pte_t * pte = pte_alloc(&init_mm, pmd, address);
		if (!pte)
			return -ENOMEM;
		if (alloc_area_pte(pte, address, end - address,
					gfp_mask, prot, pages))
			return -ENOMEM;
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);
	return 0;
}

static inline int __vmalloc_area_pages (unsigned long address,
					unsigned long size,
					int gfp_mask,
					pgprot_t prot,
					struct page ***pages)
{
	pgd_t * dir;
	unsigned long start = address;
	unsigned long end = address + size;

	dir = pgd_offset_k(address);
	spin_lock(&init_mm.page_table_lock);
	do {
		pmd_t *pmd;
		
		pmd = pmd_alloc(&init_mm, dir, address);
		if (!pmd)
			goto err;

		if (alloc_area_pmd(pmd, address, end - address, gfp_mask, prot, pages))
			goto err;	// The kernel NEVER reclaims pmds, so no need to undo pmd_alloc() here

		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	} while (address && (address < end));
	spin_unlock(&init_mm.page_table_lock);
	flush_cache_all();
	return 0;
err:
	spin_unlock(&init_mm.page_table_lock);
	flush_cache_all();
	if (address > start)
		vmfree_area_pages(start, address - start);
	return -ENOMEM;
}

int vmalloc_area_pages(unsigned long address, unsigned long size,
		       int gfp_mask, pgprot_t prot)
{
	return __vmalloc_area_pages(address, size, gfp_mask, prot, NULL);
}

struct vm_struct * get_vm_area(unsigned long size, unsigned long flags)
{
	unsigned long addr, next;
	struct vm_struct **p, *tmp, *area;

	area = (struct vm_struct *) kmalloc(sizeof(*area), GFP_KERNEL);
	if (!area)
		return NULL;

	size += PAGE_SIZE;
	if (!size) {
		kfree (area);
		return NULL;
	}

	addr = VMALLOC_START;
	write_lock(&vmlist_lock);
	for (p = &vmlist; (tmp = *p) ; p = &tmp->next) {
		if ((size + addr) < addr)
			goto out;
		if (size + addr <= (unsigned long) tmp->addr)
			break;
		next = tmp->size + (unsigned long) tmp->addr;
		if (next > addr) 
			addr = next;
		if (addr > VMALLOC_END-size)
			goto out;
	}
	area->flags = flags;
	area->addr = (void *)addr;
	area->size = size;
	area->next = *p;
	*p = area;
	write_unlock(&vmlist_lock);
	return area;

out:
	write_unlock(&vmlist_lock);
	kfree(area);
	return NULL;
}

void __vfree(void * addr, int free_area_pages)
{
	struct vm_struct **p, *tmp;

	if (!addr)
		return;
	if ((PAGE_SIZE-1) & (unsigned long) addr) {
		printk(KERN_ERR "Trying to vfree() bad address (%p)\n", addr);
		return;
	}
	write_lock(&vmlist_lock);
	for (p = &vmlist ; (tmp = *p) ; p = &tmp->next) {
		if (tmp->addr == addr) {
			*p = tmp->next;
			if (free_area_pages)
				vmfree_area_pages(VMALLOC_VMADDR(tmp->addr), tmp->size);
			write_unlock(&vmlist_lock);
			kfree(tmp);
			return;
		}
	}
	write_unlock(&vmlist_lock);
	printk(KERN_ERR "Trying to vfree() nonexistent vm area (%p)\n", addr);
}

void vfree(void * addr)
{
	__vfree(addr,1);
}

void * __vmalloc (unsigned long size, int gfp_mask, pgprot_t prot)
{
	void * addr;
	struct vm_struct *area;

	size = PAGE_ALIGN(size);
	if (!size || (size >> PAGE_SHIFT) > num_physpages)
		return NULL;
	area = get_vm_area(size, VM_ALLOC);
	if (!area)
		return NULL;
	addr = area->addr;
	if (__vmalloc_area_pages(VMALLOC_VMADDR(addr), size, gfp_mask,
				 prot, NULL)) {
		__vfree(addr, 0);
		return NULL;
	}
	return addr;
}

void * vmap(struct page **pages, int count,
	    unsigned long flags, pgprot_t prot)
{
	void * addr;
	struct vm_struct *area;
	unsigned long size = count << PAGE_SHIFT;

	if (!size || size > (max_mapnr << PAGE_SHIFT))
		return NULL;
	area = get_vm_area(size, flags);
	if (!area) {
		return NULL;
	}
	addr = area->addr;
	if (__vmalloc_area_pages(VMALLOC_VMADDR(addr), size, 0,
				 prot, &pages)) {
		__vfree(addr, 0);
		return NULL;
	}
	return addr;
}

long vread(char *buf, char *addr, unsigned long count)
{
	struct vm_struct *tmp;
	char *vaddr, *buf_start = buf;
	unsigned long n;

	/* Don't allow overflow */
	if ((unsigned long) addr + count < count)
		count = -(unsigned long) addr;

	read_lock(&vmlist_lock);
	for (tmp = vmlist; tmp; tmp = tmp->next) {
		vaddr = (char *) tmp->addr;
		if (addr >= vaddr + tmp->size - PAGE_SIZE)
			continue;
		while (addr < vaddr) {
			if (count == 0)
				goto finished;
			*buf = '\0';
			buf++;
			addr++;
			count--;
		}
		n = vaddr + tmp->size - PAGE_SIZE - addr;
		do {
			if (count == 0)
				goto finished;
			*buf = *addr;
			buf++;
			addr++;
			count--;
		} while (--n > 0);
	}
finished:
	read_unlock(&vmlist_lock);
	return buf - buf_start;
}

long vwrite(char *buf, char *addr, unsigned long count)
{
	struct vm_struct *tmp;
	char *vaddr, *buf_start = buf;
	unsigned long n;

	/* Don't allow overflow */
	if ((unsigned long) addr + count < count)
		count = -(unsigned long) addr;

	read_lock(&vmlist_lock);
	for (tmp = vmlist; tmp; tmp = tmp->next) {
		vaddr = (char *) tmp->addr;
		if (addr >= vaddr + tmp->size - PAGE_SIZE)
			continue;
		while (addr < vaddr) {
			if (count == 0)
				goto finished;
			buf++;
			addr++;
			count--;
		}
		n = vaddr + tmp->size - PAGE_SIZE - addr;
		do {
			if (count == 0)
				goto finished;
			*addr = *buf;
			buf++;
			addr++;
			count--;
		} while (--n > 0);
	}
finished:
	read_unlock(&vmlist_lock);
	return buf - buf_start;
}
