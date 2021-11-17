/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
 * It is required that mutex_init() be called before pmap_bootstrap(), 
 * as the PMAP layer makes extensive use of mutexes.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/kerneldump.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>

#include <machine/dump.h>
#include <machine/ifunc.h>
#include <machine/md_var.h>
#include <machine/mmuvar.h>
#include <machine/smp.h>

mmu_t		mmu_obj;

/*
 * pmap globals
 */
struct pmap kernel_pmap_store;

vm_offset_t    msgbuf_phys;

vm_offset_t kernel_vm_end;
vm_offset_t virtual_avail;
vm_offset_t virtual_end;
caddr_t crashdumpmap;

int pmap_bootstrapped;
/* Default level 0 reservations consist of 512 pages (2MB superpage). */
int vm_level_0_order = 9;

SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD, 0, "VM/pmap parameters");

int superpages_enabled = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, superpages_enabled, CTLFLAG_RDTUN,
    &superpages_enabled, 0, "Enable support for transparent superpages");

#ifdef AIM
int
pvo_vaddr_compare(struct pvo_entry *a, struct pvo_entry *b)
{
	if (PVO_VADDR(a) < PVO_VADDR(b))
		return (-1);
	else if (PVO_VADDR(a) > PVO_VADDR(b))
		return (1);
	return (0);
}
RB_GENERATE(pvo_tree, pvo_entry, pvo_plink, pvo_vaddr_compare);
#endif

static int
pmap_nomethod(void)
{
	return (0);
}

#define DEFINE_PMAP_IFUNC(ret, func, args) 				\
	DEFINE_IFUNC(, ret, pmap_##func, args) {			\
		pmap_##func##_t f;					\
		f = PMAP_RESOLVE_FUNC(func);				\
		return (f != NULL ? f : (pmap_##func##_t)pmap_nomethod);\
	}
#define DEFINE_DUMPSYS_IFUNC(ret, func, args) 				\
	DEFINE_IFUNC(, ret, dumpsys_##func, args) {			\
		pmap_dumpsys_##func##_t f;				\
		f = PMAP_RESOLVE_FUNC(dumpsys_##func);			\
		return (f != NULL ? f : (pmap_dumpsys_##func##_t)pmap_nomethod);\
	}

DEFINE_PMAP_IFUNC(void, activate, (struct thread *));
DEFINE_PMAP_IFUNC(void, advise, (pmap_t, vm_offset_t, vm_offset_t, int));
DEFINE_PMAP_IFUNC(void, align_superpage, (vm_object_t, vm_ooffset_t,
	vm_offset_t *, vm_size_t));
DEFINE_PMAP_IFUNC(void, clear_modify, (vm_page_t));
DEFINE_PMAP_IFUNC(void, copy, (pmap_t, pmap_t, vm_offset_t, vm_size_t, vm_offset_t));
DEFINE_PMAP_IFUNC(int, enter, (pmap_t, vm_offset_t, vm_page_t, vm_prot_t, u_int, int8_t));
DEFINE_PMAP_IFUNC(void, enter_quick, (pmap_t, vm_offset_t, vm_page_t, vm_prot_t));
DEFINE_PMAP_IFUNC(void, enter_object, (pmap_t, vm_offset_t, vm_offset_t, vm_page_t,
	vm_prot_t));
DEFINE_PMAP_IFUNC(vm_paddr_t, extract, (pmap_t, vm_offset_t));
DEFINE_PMAP_IFUNC(vm_page_t, extract_and_hold, (pmap_t, vm_offset_t, vm_prot_t));
DEFINE_PMAP_IFUNC(void, kenter, (vm_offset_t, vm_paddr_t));
DEFINE_PMAP_IFUNC(void, kenter_attr, (vm_offset_t, vm_paddr_t, vm_memattr_t));
DEFINE_PMAP_IFUNC(vm_paddr_t, kextract, (vm_offset_t));
DEFINE_PMAP_IFUNC(void, kremove, (vm_offset_t));
DEFINE_PMAP_IFUNC(void, object_init_pt, (pmap_t, vm_offset_t, vm_object_t, vm_pindex_t,
	vm_size_t));
DEFINE_PMAP_IFUNC(boolean_t, is_modified, (vm_page_t));
DEFINE_PMAP_IFUNC(boolean_t, is_prefaultable, (pmap_t, vm_offset_t));
DEFINE_PMAP_IFUNC(boolean_t, is_referenced, (vm_page_t));
DEFINE_PMAP_IFUNC(boolean_t, page_exists_quick, (pmap_t, vm_page_t));
DEFINE_PMAP_IFUNC(void, page_init, (vm_page_t));
DEFINE_PMAP_IFUNC(boolean_t, page_is_mapped, (vm_page_t));
DEFINE_PMAP_IFUNC(int, page_wired_mappings, (vm_page_t));
DEFINE_PMAP_IFUNC(void, protect, (pmap_t, vm_offset_t, vm_offset_t, vm_prot_t));
DEFINE_PMAP_IFUNC(bool, ps_enabled, (pmap_t));
DEFINE_PMAP_IFUNC(void, qenter, (vm_offset_t, vm_page_t *, int));
DEFINE_PMAP_IFUNC(void, qremove, (vm_offset_t, int));
DEFINE_PMAP_IFUNC(vm_offset_t, quick_enter_page, (vm_page_t));
DEFINE_PMAP_IFUNC(void, quick_remove_page, (vm_offset_t));
DEFINE_PMAP_IFUNC(boolean_t, ts_referenced, (vm_page_t));
DEFINE_PMAP_IFUNC(void, release, (pmap_t));
DEFINE_PMAP_IFUNC(void, remove, (pmap_t, vm_offset_t, vm_offset_t));
DEFINE_PMAP_IFUNC(void, remove_all, (vm_page_t));
DEFINE_PMAP_IFUNC(void, remove_pages, (pmap_t));
DEFINE_PMAP_IFUNC(void, remove_write, (vm_page_t));
DEFINE_PMAP_IFUNC(void, unwire, (pmap_t, vm_offset_t, vm_offset_t));
DEFINE_PMAP_IFUNC(void, zero_page, (vm_page_t));
DEFINE_PMAP_IFUNC(void, zero_page_area, (vm_page_t, int, int));
DEFINE_PMAP_IFUNC(void, copy_page, (vm_page_t, vm_page_t));
DEFINE_PMAP_IFUNC(void, copy_pages,
    (vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize));
DEFINE_PMAP_IFUNC(void, growkernel, (vm_offset_t));
DEFINE_PMAP_IFUNC(void, init, (void));
DEFINE_PMAP_IFUNC(vm_offset_t, map, (vm_offset_t *, vm_paddr_t, vm_paddr_t, int));
DEFINE_PMAP_IFUNC(int, pinit, (pmap_t));
DEFINE_PMAP_IFUNC(void, pinit0, (pmap_t));
DEFINE_PMAP_IFUNC(int, mincore, (pmap_t, vm_offset_t, vm_paddr_t *));
DEFINE_PMAP_IFUNC(void, deactivate, (struct thread *));
DEFINE_PMAP_IFUNC(void, bootstrap, (vm_offset_t, vm_offset_t));
DEFINE_PMAP_IFUNC(void, cpu_bootstrap, (int));
DEFINE_PMAP_IFUNC(void *, mapdev, (vm_paddr_t, vm_size_t));
DEFINE_PMAP_IFUNC(void *, mapdev_attr, (vm_paddr_t, vm_size_t, vm_memattr_t));
DEFINE_PMAP_IFUNC(void, page_set_memattr, (vm_page_t, vm_memattr_t));
DEFINE_PMAP_IFUNC(void, unmapdev, (vm_offset_t, vm_size_t));
DEFINE_PMAP_IFUNC(int, map_user_ptr,
    (pmap_t, volatile const void *, void **, size_t, size_t *));
DEFINE_PMAP_IFUNC(int, decode_kernel_ptr, (vm_offset_t, int *, vm_offset_t *));
DEFINE_PMAP_IFUNC(boolean_t, dev_direct_mapped, (vm_paddr_t, vm_size_t));
DEFINE_PMAP_IFUNC(void, sync_icache, (pmap_t, vm_offset_t, vm_size_t));
DEFINE_PMAP_IFUNC(int, change_attr, (vm_offset_t, vm_size_t, vm_memattr_t));
DEFINE_PMAP_IFUNC(void, page_array_startup, (long));
DEFINE_PMAP_IFUNC(void, tlbie_all, (void));

DEFINE_DUMPSYS_IFUNC(void, map_chunk, (vm_paddr_t, size_t, void **));
DEFINE_DUMPSYS_IFUNC(void, unmap_chunk, (vm_paddr_t, size_t, void *));
DEFINE_DUMPSYS_IFUNC(void, pa_init, (void));
DEFINE_DUMPSYS_IFUNC(size_t, scan_pmap, (struct bitset *));
DEFINE_DUMPSYS_IFUNC(void *, dump_pmap_init, (unsigned));
DEFINE_DUMPSYS_IFUNC(void *, dump_pmap, (void *, void *, u_long *));

/*
 * MMU install routines. Highest priority wins, equal priority also
 * overrides allowing last-set to win.
 */
SET_DECLARE(mmu_set, struct mmu_kobj);

boolean_t
pmap_mmu_install(char *name, int prio)
{
	mmu_t	*mmupp, mmup;
	static int	curr_prio = 0;

	/*
	 * Try and locate the MMU kobj corresponding to the name
	 */
	SET_FOREACH(mmupp, mmu_set) {
		mmup = *mmupp;

		if (mmup->name &&
		    !strcmp(mmup->name, name) &&
		    (prio >= curr_prio || mmu_obj == NULL)) {
			curr_prio = prio;
			mmu_obj = mmup;
			return (TRUE);
		}
	}

	return (FALSE);
}

/* MMU "pre-bootstrap" init, used to install extra resolvers, etc. */
void
pmap_mmu_init()
{
	if (mmu_obj->funcs->install != NULL)
		(mmu_obj->funcs->install)();
}

const char *
pmap_mmu_name(void)
{
	return (mmu_obj->name);
}

int unmapped_buf_allowed;

boolean_t
pmap_is_valid_memattr(pmap_t pmap __unused, vm_memattr_t mode)
{

	switch (mode) {
	case VM_MEMATTR_DEFAULT:
	case VM_MEMATTR_UNCACHEABLE:
	case VM_MEMATTR_CACHEABLE:
	case VM_MEMATTR_WRITE_COMBINING:
	case VM_MEMATTR_WRITE_BACK:
	case VM_MEMATTR_WRITE_THROUGH:
	case VM_MEMATTR_PREFETCHABLE:
		return (TRUE);
	default:
		return (FALSE);
	}
}
