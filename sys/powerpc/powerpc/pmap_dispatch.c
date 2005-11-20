/*-
 * Copyright (c) 2005 Peter Grehan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Dispatch MI pmap calls to the appropriate MMU implementation
 * through a previously registered kernel object.
 *
 * Before pmap_bootstrap() can be called, a CPU module must have
 * called pmap_mmu_install(). This may be called multiple times:
 * the highest priority call will be installed as the default
 * MMU handler when pmap_bootstrap() is called.
 *
 * It is required that kobj_machdep_init() be called before
 * pmap_bootstrap() to allow the kobj subsystem to initialise. This
 * in turn requires that mutex_init() has been called.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/mmuvar.h>

#include "mmu_if.h"

static mmu_def_t	*mmu_def_impl;
static mmu_t		mmu_obj;
static struct mmu_kobj	mmu_kernel_obj;
static struct kobj_ops	mmu_kernel_kops;

/*
 * pmap globals
 */
struct pmap kernel_pmap_store;

struct msgbuf *msgbufp;
vm_offset_t    msgbuf_phys;

vm_offset_t avail_start;
vm_offset_t avail_end;
vm_offset_t kernel_vm_end;
vm_offset_t phys_avail[PHYS_AVAIL_SZ];
vm_offset_t virtual_avail;
vm_offset_t virtual_end;

int pmap_bootstrapped;

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	MMU_CHANGE_WIRING(mmu_obj, pmap, va, wired);
}

void
pmap_clear_modify(vm_page_t m)
{
	MMU_CLEAR_MODIFY(mmu_obj, m);
}

void
pmap_clear_reference(vm_page_t m)
{
	MMU_CLEAR_REFERENCE(mmu_obj, m);
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
    vm_size_t len, vm_offset_t src_addr)
{
	MMU_COPY(mmu_obj, dst_pmap, src_pmap, dst_addr, len, src_addr);
}

void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	MMU_COPY_PAGE(mmu_obj, src, dst);
}

void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t p, vm_prot_t prot,
    boolean_t wired)
{
	MMU_ENTER(mmu_obj, pmap, va, p, prot, wired);
}

vm_page_t
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    vm_page_t mpte)
{
	return (MMU_ENTER_QUICK(mmu_obj, pmap, va, m, prot, mpte));
}

vm_paddr_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	return (MMU_EXTRACT(mmu_obj, pmap, va));
}

vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	return (MMU_EXTRACT_AND_HOLD(mmu_obj, pmap, va, prot));
}

void
pmap_growkernel(vm_offset_t va)
{
	MMU_GROWKERNEL(mmu_obj, va);
}

void
pmap_init(void)
{
	MMU_INIT(mmu_obj);
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	return (MMU_IS_MODIFIED(mmu_obj, m));
}

boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t va)
{
	return (MMU_IS_PREFAULTABLE(mmu_obj, pmap, va));
}

boolean_t
pmap_ts_referenced(vm_page_t m)
{
	return (MMU_TS_REFERENCED(mmu_obj, m));
}

vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{
	return (MMU_MAP(mmu_obj, virt, start, end, prot));
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{
	MMU_OBJECT_INIT_PT(mmu_obj, pmap, addr, object, pindex, size);
}

boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	return (MMU_PAGE_EXISTS_QUICK(mmu_obj, pmap, m));
}

void
pmap_page_init(vm_page_t m)
{
	MMU_PAGE_INIT(mmu_obj, m);
}

void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	MMU_PAGE_PROTECT(mmu_obj, m, prot);
}

void
pmap_pinit(pmap_t pmap)
{
	MMU_PINIT(mmu_obj, pmap);
}

void
pmap_pinit0(pmap_t pmap)
{
	MMU_PINIT0(mmu_obj, pmap);
}

void
pmap_protect(pmap_t pmap, vm_offset_t start, vm_offset_t end, vm_prot_t prot)
{
	MMU_PROTECT(mmu_obj, pmap, start, end, prot);
}

void
pmap_qenter(vm_offset_t start, vm_page_t *m, int count)
{
	MMU_QENTER(mmu_obj, start, m, count);
}

void
pmap_qremove(vm_offset_t start, int count)
{
	MMU_QREMOVE(mmu_obj, start, count);
}

void
pmap_release(pmap_t pmap)
{
	MMU_RELEASE(mmu_obj, pmap);
}

void
pmap_remove(pmap_t pmap, vm_offset_t start, vm_offset_t end)
{
	MMU_REMOVE(mmu_obj, pmap, start, end);
}

void
pmap_remove_all(vm_page_t m)
{
	MMU_REMOVE_ALL(mmu_obj, m);
}

void
pmap_remove_pages(pmap_t pmap, vm_offset_t start, vm_offset_t end)
{
	MMU_REMOVE_PAGES(mmu_obj, pmap, start, end);
}

void
pmap_zero_page(vm_page_t m)
{
	MMU_ZERO_PAGE(mmu_obj, m);
}

void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	MMU_ZERO_PAGE_AREA(mmu_obj, m, off, size);
}

void
pmap_zero_page_idle(vm_page_t m)
{
	MMU_ZERO_PAGE_IDLE(mmu_obj, m);
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{
	return (MMU_MINCORE(mmu_obj, pmap, addr));
}

void
pmap_activate(struct thread *td)
{
	MMU_ACTIVATE(mmu_obj, td);
}

void
pmap_deactivate(struct thread *td)
{
	MMU_DEACTIVATE(mmu_obj, td);
}

vm_offset_t
pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size)
{
	return (MMU_ADDR_HINT(mmu_obj, obj, addr, size));
}



/*
 * Routines used in machine-dependent code
 */
void
pmap_bootstrap(vm_offset_t start, vm_offset_t end)
{
	mmu_obj = &mmu_kernel_obj;

	/*
	 * Take care of compiling the selected class, and
	 * then statically initialise the MMU object
	 */
	kobj_class_compile_static(mmu_def_impl, &mmu_kernel_kops);
	kobj_init((kobj_t)mmu_obj, mmu_def_impl);

	MMU_BOOTSTRAP(mmu_obj, start, end);
}

void *
pmap_mapdev(vm_offset_t pa, vm_size_t size)
{
	return (MMU_MAPDEV(mmu_obj, pa, size));
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	MMU_UNMAPDEV(mmu_obj, va, size);
}

vm_offset_t
pmap_kextract(vm_offset_t va)
{
	return (MMU_KEXTRACT(mmu_obj, va));
}

void
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	MMU_KENTER(mmu_obj, va, pa);
}

boolean_t
pmap_dev_direct_mapped(vm_offset_t pa, vm_size_t size)
{
	return (MMU_DEV_DIRECT_MAPPED(mmu_obj, pa, size));
}


/*
 * MMU install routines. Highest priority wins, equal priority also
 * overrides allowing last-set to win.
 */
SET_DECLARE(mmu_set, mmu_def_t);

boolean_t
pmap_mmu_install(char *name, int prio)
{
	mmu_def_t	**mmupp, *mmup;
	static int	curr_prio = 0;

	/*
	 * Try and locate the MMU kobj corresponding to the name
	 */
	SET_FOREACH(mmupp, mmu_set) {
		mmup = *mmupp;

		if (mmup->name &&
		    !strcmp(mmup->name, name) &&
		    prio >= curr_prio) {
			curr_prio = prio;
			mmu_def_impl = mmup;
			return (TRUE);
		}
	}

	return (FALSE);
}
