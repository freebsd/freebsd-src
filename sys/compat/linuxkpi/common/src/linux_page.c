/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2016 Matthew Macy (mmacy@mattmacy.io)
 * Copyright (c) 2017 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/memrange.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/vm_extern.h>

#include <vm/uma.h>
#include <vm/uma_int.h>

#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/preempt.h>
#include <linux/fs.h>
#include <linux/shmem_fs.h>
#include <linux/kernel.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/io-mapping.h>

#ifdef __i386__
DEFINE_IDR(mtrr_idr);
static MALLOC_DEFINE(M_LKMTRR, "idr", "Linux MTRR compat");
extern int pat_works;
#endif

void
si_meminfo(struct sysinfo *si)
{
	si->totalram = physmem;
	si->freeram = vm_free_count();
	si->totalhigh = 0;
	si->freehigh = 0;
	si->mem_unit = PAGE_SIZE;
}

void *
linux_page_address(const struct page *page)
{

	if (page->object != kernel_object) {
		return (PMAP_HAS_DMAP ?
		    ((void *)(uintptr_t)PHYS_TO_DMAP(page_to_phys(page))) :
		    NULL);
	}
	return ((void *)(uintptr_t)(VM_MIN_KERNEL_ADDRESS +
	    IDX_TO_OFF(page->pindex)));
}

struct page *
linux_alloc_pages(gfp_t flags, unsigned int order)
{
	struct page *page;

	if (PMAP_HAS_DMAP) {
		unsigned long npages = 1UL << order;
		int req = VM_ALLOC_WIRED;

		if ((flags & M_ZERO) != 0)
			req |= VM_ALLOC_ZERO;

		if (order == 0 && (flags & GFP_DMA32) == 0) {
			page = vm_page_alloc_noobj(req);
			if (page == NULL)
				return (NULL);
		} else {
			vm_paddr_t pmax = (flags & GFP_DMA32) ?
			    BUS_SPACE_MAXADDR_32BIT : BUS_SPACE_MAXADDR;

			if ((flags & __GFP_NORETRY) != 0)
				req |= VM_ALLOC_NORECLAIM;

		retry:
			page = vm_page_alloc_noobj_contig(req, npages, 0, pmax,
			    PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
			if (page == NULL) {
				if ((flags & (M_WAITOK | __GFP_NORETRY)) ==
				    M_WAITOK) {
					int err = vm_page_reclaim_contig(req,
					    npages, 0, pmax, PAGE_SIZE, 0);
					if (err == ENOMEM)
						vm_wait(NULL);
					else if (err != 0)
						return (NULL);
					flags &= ~M_WAITOK;
					goto retry;
				}
				return (NULL);
			}
		}
	} else {
		vm_offset_t vaddr;

		vaddr = linux_alloc_kmem(flags, order);
		if (vaddr == 0)
			return (NULL);

		page = virt_to_page((void *)vaddr);

		KASSERT(vaddr == (vm_offset_t)page_address(page),
		    ("Page address mismatch"));
	}

	return (page);
}

static void
_linux_free_kmem(vm_offset_t addr, unsigned int order)
{
	size_t size = ((size_t)PAGE_SIZE) << order;

	kmem_free((void *)addr, size);
}

void
linux_free_pages(struct page *page, unsigned int order)
{
	if (PMAP_HAS_DMAP) {
		unsigned long npages = 1UL << order;
		unsigned long x;

		for (x = 0; x != npages; x++) {
			vm_page_t pgo = page + x;

			/*
			 * The "free page" function is used in several
			 * contexts.
			 *
			 * Some pages are allocated by `linux_alloc_pages()`
			 * above, but not all of them are. For instance in the
			 * DRM drivers, some pages come from
			 * `shmem_read_mapping_page_gfp()`.
			 *
			 * That's why we need to check if the page is managed
			 * or not here.
			 */
			if ((pgo->oflags & VPO_UNMANAGED) == 0) {
				vm_page_unwire(pgo, PQ_ACTIVE);
			} else {
				if (vm_page_unwire_noq(pgo))
					vm_page_free(pgo);
			}
		}
	} else {
		vm_offset_t vaddr;

		vaddr = (vm_offset_t)page_address(page);

		_linux_free_kmem(vaddr, order);
	}
}

void
linux_release_pages(release_pages_arg arg, int nr)
{
	int i;

	CTASSERT(offsetof(struct folio, page) == 0);

	for (i = 0; i < nr; i++)
		__free_page(arg.pages[i]);
}

vm_offset_t
linux_alloc_kmem(gfp_t flags, unsigned int order)
{
	size_t size = ((size_t)PAGE_SIZE) << order;
	void *addr;

	addr = kmem_alloc_contig(size, flags & GFP_NATIVE_MASK, 0,
	    ((flags & GFP_DMA32) == 0) ? -1UL : BUS_SPACE_MAXADDR_32BIT,
	    PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);

	return ((vm_offset_t)addr);
}

void
linux_free_kmem(vm_offset_t addr, unsigned int order)
{
	KASSERT((addr & ~PAGE_MASK) == 0,
	    ("%s: addr %p is not page aligned", __func__, (void *)addr));

	if (addr >= VM_MIN_KERNEL_ADDRESS && addr < VM_MAX_KERNEL_ADDRESS) {
		_linux_free_kmem(addr, order);
	} else {
		vm_page_t page;

		page = PHYS_TO_VM_PAGE(DMAP_TO_PHYS(addr));
		linux_free_pages(page, order);
	}
}

static int
linux_get_user_pages_internal(vm_map_t map, unsigned long start, int nr_pages,
    int write, struct page **pages)
{
	vm_prot_t prot;
	size_t len;
	int count;

	prot = write ? (VM_PROT_READ | VM_PROT_WRITE) : VM_PROT_READ;
	len = ptoa((vm_offset_t)nr_pages);
	count = vm_fault_quick_hold_pages(map, start, len, prot, pages, nr_pages);
	return (count == -1 ? -EFAULT : nr_pages);
}

int
__get_user_pages_fast(unsigned long start, int nr_pages, int write,
    struct page **pages)
{
	vm_map_t map;
	vm_page_t *mp;
	vm_offset_t va;
	vm_offset_t end;
	vm_prot_t prot;
	int count;

	if (nr_pages == 0 || in_interrupt())
		return (0);

	MPASS(pages != NULL);
	map = &curthread->td_proc->p_vmspace->vm_map;
	end = start + ptoa((vm_offset_t)nr_pages);
	if (!vm_map_range_valid(map, start, end))
		return (-EINVAL);
	prot = write ? (VM_PROT_READ | VM_PROT_WRITE) : VM_PROT_READ;
	for (count = 0, mp = pages, va = start; va < end;
	    mp++, va += PAGE_SIZE, count++) {
		*mp = pmap_extract_and_hold(map->pmap, va, prot);
		if (*mp == NULL)
			break;

		if ((prot & VM_PROT_WRITE) != 0 &&
		    (*mp)->dirty != VM_PAGE_BITS_ALL) {
			/*
			 * Explicitly dirty the physical page.  Otherwise, the
			 * caller's changes may go unnoticed because they are
			 * performed through an unmanaged mapping or by a DMA
			 * operation.
			 *
			 * The object lock is not held here.
			 * See vm_page_clear_dirty_mask().
			 */
			vm_page_dirty(*mp);
		}
	}
	return (count);
}

long
get_user_pages_remote(struct task_struct *task, struct mm_struct *mm,
    unsigned long start, unsigned long nr_pages, unsigned int gup_flags,
    struct page **pages, struct vm_area_struct **vmas)
{
	vm_map_t map;

	map = &task->task_thread->td_proc->p_vmspace->vm_map;
	return (linux_get_user_pages_internal(map, start, nr_pages,
	    !!(gup_flags & FOLL_WRITE), pages));
}

long
lkpi_get_user_pages(unsigned long start, unsigned long nr_pages,
    unsigned int gup_flags, struct page **pages)
{
	vm_map_t map;

	map = &curthread->td_proc->p_vmspace->vm_map;
	return (linux_get_user_pages_internal(map, start, nr_pages,
	    !!(gup_flags & FOLL_WRITE), pages));
}

int
is_vmalloc_addr(const void *addr)
{
	return (vtoslab((vm_offset_t)addr & ~UMA_SLAB_MASK) != NULL);
}

vm_fault_t
lkpi_vmf_insert_pfn_prot_locked(struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, pgprot_t prot)
{
	struct pctrie_iter pages;
	vm_object_t vm_obj = vma->vm_obj;
	vm_object_t tmp_obj;
	vm_page_t page;
	vm_pindex_t pindex;

	VM_OBJECT_ASSERT_WLOCKED(vm_obj);
	vm_page_iter_init(&pages, vm_obj);
	pindex = OFF_TO_IDX(addr - vma->vm_start);
	if (vma->vm_pfn_count == 0)
		vma->vm_pfn_first = pindex;
	MPASS(pindex <= OFF_TO_IDX(vma->vm_end));

retry:
	page = vm_page_grab_iter(vm_obj, pindex, VM_ALLOC_NOCREAT, &pages);
	if (page == NULL) {
		page = PHYS_TO_VM_PAGE(IDX_TO_OFF(pfn));
		if (!vm_page_busy_acquire(page, VM_ALLOC_WAITFAIL)) {
			pctrie_iter_reset(&pages);
			goto retry;
		}
		if (page->object != NULL) {
			tmp_obj = page->object;
			vm_page_xunbusy(page);
			VM_OBJECT_WUNLOCK(vm_obj);
			VM_OBJECT_WLOCK(tmp_obj);
			if (page->object == tmp_obj &&
			    vm_page_busy_acquire(page, VM_ALLOC_WAITFAIL)) {
				KASSERT(page->object == tmp_obj,
				    ("page has changed identity"));
				KASSERT((page->oflags & VPO_UNMANAGED) == 0,
				    ("page does not belong to shmem"));
				vm_pager_page_unswapped(page);
				if (pmap_page_is_mapped(page)) {
					vm_page_xunbusy(page);
					VM_OBJECT_WUNLOCK(tmp_obj);
					printf("%s: page rename failed: page "
					    "is mapped\n", __func__);
					VM_OBJECT_WLOCK(vm_obj);
					return (VM_FAULT_NOPAGE);
				}
				vm_page_remove(page);
			}
			VM_OBJECT_WUNLOCK(tmp_obj);
			pctrie_iter_reset(&pages);
			VM_OBJECT_WLOCK(vm_obj);
			goto retry;
		}
		if (vm_page_iter_insert(page, vm_obj, pindex, &pages) != 0) {
			vm_page_xunbusy(page);
			return (VM_FAULT_OOM);
		}
		vm_page_valid(page);
	}
	pmap_page_set_memattr(page, pgprot2cachemode(prot));
	vma->vm_pfn_count++;

	return (VM_FAULT_NOPAGE);
}

int
lkpi_remap_pfn_range(struct vm_area_struct *vma, unsigned long start_addr,
    unsigned long start_pfn, unsigned long size, pgprot_t prot)
{
	vm_object_t vm_obj;
	unsigned long addr, pfn;
	int err = 0;

	vm_obj = vma->vm_obj;

	VM_OBJECT_WLOCK(vm_obj);
	for (addr = start_addr, pfn = start_pfn;
	    addr < start_addr + size;
	    addr += PAGE_SIZE) {
		vm_fault_t ret;
retry:
		ret = lkpi_vmf_insert_pfn_prot_locked(vma, addr, pfn, prot);

		if ((ret & VM_FAULT_OOM) != 0) {
			VM_OBJECT_WUNLOCK(vm_obj);
			vm_wait(NULL);
			VM_OBJECT_WLOCK(vm_obj);
			goto retry;
		}

		if ((ret & VM_FAULT_ERROR) != 0) {
			err = -EFAULT;
			break;
		}

		pfn++;
	}
	VM_OBJECT_WUNLOCK(vm_obj);

	if (unlikely(err)) {
		zap_vma_ptes(vma, start_addr,
		    (pfn - start_pfn) << PAGE_SHIFT);
		return (err);
	}

	return (0);
}

int
lkpi_io_mapping_map_user(struct io_mapping *iomap,
    struct vm_area_struct *vma, unsigned long addr,
    unsigned long pfn, unsigned long size)
{
	pgprot_t prot;
	int ret;

	prot = cachemode2protval(iomap->attr);
	ret = lkpi_remap_pfn_range(vma, addr, pfn, size, prot);

	return (ret);
}

/*
 * Although FreeBSD version of unmap_mapping_range has semantics and types of
 * parameters compatible with Linux version, the values passed in are different
 * @obj should match to vm_private_data field of vm_area_struct returned by
 *      mmap file operation handler, see linux_file_mmap_single() sources
 * @holelen should match to size of area to be munmapped.
 */
void
lkpi_unmap_mapping_range(void *obj, loff_t const holebegin __unused,
    loff_t const holelen __unused, int even_cows __unused)
{
	vm_object_t devobj;

	devobj = cdev_pager_lookup(obj);
	if (devobj != NULL) {
		cdev_mgtdev_pager_free_pages(devobj);
		vm_object_deallocate(devobj);
	}
}

int
lkpi_arch_phys_wc_add(unsigned long base, unsigned long size)
{
#ifdef __i386__
	struct mem_range_desc *mrdesc;
	int error, id, act;

	/* If PAT is available, do nothing */
	if (pat_works)
		return (0);

	mrdesc = malloc(sizeof(*mrdesc), M_LKMTRR, M_WAITOK);
	mrdesc->mr_base = base;
	mrdesc->mr_len = size;
	mrdesc->mr_flags = MDF_WRITECOMBINE;
	strlcpy(mrdesc->mr_owner, "drm", sizeof(mrdesc->mr_owner));
	act = MEMRANGE_SET_UPDATE;
	error = mem_range_attr_set(mrdesc, &act);
	if (error == 0) {
		error = idr_get_new(&mtrr_idr, mrdesc, &id);
		MPASS(idr_find(&mtrr_idr, id) == mrdesc);
		if (error != 0) {
			act = MEMRANGE_SET_REMOVE;
			mem_range_attr_set(mrdesc, &act);
		}
	}
	if (error != 0) {
		free(mrdesc, M_LKMTRR);
		pr_warn(
		    "Failed to add WC MTRR for [%p-%p]: %d; "
		    "performance may suffer\n",
		    (void *)base, (void *)(base + size - 1), error);
	} else
		pr_warn("Successfully added WC MTRR for [%p-%p]\n",
		    (void *)base, (void *)(base + size - 1));

	return (error != 0 ? -error : id + __MTRR_ID_BASE);
#else
	return (0);
#endif
}

void
lkpi_arch_phys_wc_del(int reg)
{
#ifdef __i386__
	struct mem_range_desc *mrdesc;
	int act;

	/* Check if arch_phys_wc_add() failed. */
	if (reg < __MTRR_ID_BASE)
		return;

	mrdesc = idr_find(&mtrr_idr, reg - __MTRR_ID_BASE);
	MPASS(mrdesc != NULL);
	idr_remove(&mtrr_idr, reg - __MTRR_ID_BASE);
	act = MEMRANGE_SET_REMOVE;
	mem_range_attr_set(mrdesc, &act);
	free(mrdesc, M_LKMTRR);
#endif
}

/*
 * This is a highly simplified version of the Linux page_frag_cache.
 * We only support up-to 1 single page as fragment size and we will
 * always return a full page.  This may be wasteful on small objects
 * but the only known consumer (mt76) is either asking for a half-page
 * or a full page.  If this was to become a problem we can implement
 * a more elaborate version.
 */
void *
linuxkpi_page_frag_alloc(struct page_frag_cache *pfc,
    size_t fragsz, gfp_t gfp)
{
	vm_page_t pages;

	if (fragsz == 0)
		return (NULL);

	KASSERT(fragsz <= PAGE_SIZE, ("%s: fragsz %zu > PAGE_SIZE not yet "
	    "supported", __func__, fragsz));

	pages = alloc_pages(gfp, flsl(howmany(fragsz, PAGE_SIZE) - 1));
	if (pages == NULL)
		return (NULL);
	pfc->va = linux_page_address(pages);

	/* Passed in as "count" to __page_frag_cache_drain(). Unused by us. */
	pfc->pagecnt_bias = 0;

	return (pfc->va);
}

void
linuxkpi_page_frag_free(void *addr)
{
	vm_page_t page;

	page = virt_to_page(addr);
	linux_free_pages(page, 0);
}

void
linuxkpi__page_frag_cache_drain(struct page *page, size_t count __unused)
{

	linux_free_pages(page, 0);
}
