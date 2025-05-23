/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
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
 */

#include <sys/cdefs.h>
/* Routines for mapping device memory. */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/devmap.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>

#ifdef __arm__
#include <machine/pte.h>
#endif

#ifdef __HAVE_STATIC_DEVMAP
#define	DEVMAP_PADDR_NOTFOUND	((vm_paddr_t)(-1))

static const struct devmap_entry *devmap_table;
static boolean_t devmap_bootstrap_done = false;

/*
 * The allocated-kva (akva) devmap table and metadata.  Platforms can call
 * devmap_add_entry() to add static device mappings to this table using
 * automatically allocated virtual addresses carved out of the top of kva space.
 * Allocation begins immediately below the max kernel virtual address.
 */
#define	AKVA_DEVMAP_MAX_ENTRIES	32
static struct devmap_entry	akva_devmap_entries[AKVA_DEVMAP_MAX_ENTRIES];
static u_int			akva_devmap_idx;
#endif
static vm_offset_t		akva_devmap_vaddr = DEVMAP_MAX_VADDR;

#if defined(__aarch64__) || defined(__riscv)
extern int early_boot;
#endif

#ifdef __HAVE_STATIC_DEVMAP
/*
 * Print the contents of the static mapping table using the provided printf-like
 * output function (which will be either printf or db_printf).
 */
static void
devmap_dump_table(int (*prfunc)(const char *, ...))
{
	const struct devmap_entry *pd;

	if (devmap_table == NULL || devmap_table[0].pd_size == 0) {
		prfunc("No static device mappings.\n");
		return;
	}

	prfunc("Static device mappings:\n");
	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		prfunc("  0x%08jx - 0x%08jx mapped at VA 0x%08jx\n",
		    (uintmax_t)pd->pd_pa,
		    (uintmax_t)(pd->pd_pa + pd->pd_size - 1),
		    (uintmax_t)pd->pd_va);
	}
}

/*
 * Print the contents of the static mapping table.  Used for bootverbose.
 */
void
devmap_print_table(void)
{
	devmap_dump_table(printf);
}

/*
 * Return the "last" kva address used by the registered devmap table.  It's
 * actually the lowest address used by the static mappings, i.e., the address of
 * the first unusable byte of KVA.
 */
vm_offset_t
devmap_lastaddr(void)
{
	const struct devmap_entry *pd;
	vm_offset_t lowaddr;

	if (akva_devmap_idx > 0)
		return (akva_devmap_vaddr);

	lowaddr = DEVMAP_MAX_VADDR;
	for (pd = devmap_table; pd != NULL && pd->pd_size != 0; ++pd) {
		if (lowaddr > pd->pd_va)
			lowaddr = pd->pd_va;
	}

	return (lowaddr);
}

/*
 * Add an entry to the internal "akva" static devmap table using the given
 * physical address and size and a virtual address allocated from the top of
 * kva.  This automatically registers the akva table on the first call, so all a
 * platform has to do is call this routine to install as many mappings as it
 * needs and when the platform-specific init function calls devmap_bootstrap()
 * it will pick up all the entries in the akva table automatically.
 */
void
devmap_add_entry(vm_paddr_t pa, vm_size_t sz)
{
	struct devmap_entry *m;

	if (devmap_bootstrap_done)
		panic("devmap_add_entry() after devmap_bootstrap()");

	if (akva_devmap_idx == (AKVA_DEVMAP_MAX_ENTRIES - 1))
		panic("AKVA_DEVMAP_MAX_ENTRIES is too small");

	if (akva_devmap_idx == 0)
		devmap_register_table(akva_devmap_entries);

	 /* Allocate virtual address space from the top of kva downwards. */
	/*
	 * If the range being mapped is aligned and sized to 1MB boundaries then
	 * also align the virtual address to the next-lower 1MB boundary so that
	 * we end with a nice efficient section mapping.
	 */
	if ((pa & L1_S_OFFSET) == 0 && (sz & L1_S_OFFSET) == 0) {
		akva_devmap_vaddr = trunc_1mpage(akva_devmap_vaddr - sz);
	} else {
		akva_devmap_vaddr = trunc_page(akva_devmap_vaddr - sz);
	}
	m = &akva_devmap_entries[akva_devmap_idx++];
	m->pd_va    = akva_devmap_vaddr;
	m->pd_pa    = pa;
	m->pd_size  = sz;
}

/*
 * Register the given table as the one to use in devmap_bootstrap().
 */
void
devmap_register_table(const struct devmap_entry *table)
{

	devmap_table = table;
}

/*
 * Map all of the static regions in the devmap table, and remember the devmap
 * table so the mapdev, ptov, and vtop functions can do lookups later.
 */
void
devmap_bootstrap(void)
{
	const struct devmap_entry *pd;

	devmap_bootstrap_done = true;

	/*
	 * If a table was previously registered, use it.  Otherwise, no work to
	 * do.
	 */
	if (devmap_table == NULL)
		return;

	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		pmap_preboot_map_attr(pd->pd_pa, pd->pd_va, pd->pd_size,
		    VM_PROT_READ | VM_PROT_WRITE, VM_MEMATTR_DEVICE);
	}
}

/*
 * Look up the given physical address in the static mapping data and return the
 * corresponding virtual address, or NULL if not found.
 */
static void *
devmap_ptov(vm_paddr_t pa, vm_size_t size)
{
	const struct devmap_entry *pd;

	if (devmap_table == NULL)
		return (NULL);

	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		if (pa >= pd->pd_pa && pa + size <= pd->pd_pa + pd->pd_size)
			return ((void *)(pd->pd_va + (pa - pd->pd_pa)));
	}

	return (NULL);
}

/*
 * Look up the given virtual address in the static mapping data and return the
 * corresponding physical address, or DEVMAP_PADDR_NOTFOUND if not found.
 */
static vm_paddr_t
devmap_vtop(void * vpva, vm_size_t size)
{
	const struct devmap_entry *pd;
	vm_offset_t va;

	if (devmap_table == NULL)
		return (DEVMAP_PADDR_NOTFOUND);

	va = (vm_offset_t)vpva;
	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		if (va >= pd->pd_va && va + size <= pd->pd_va + pd->pd_size)
			return ((vm_paddr_t)(pd->pd_pa + (va - pd->pd_va)));
	}

	return (DEVMAP_PADDR_NOTFOUND);
}
#endif

/*
 * Map a set of physical memory pages into the kernel virtual address space.
 * Return a pointer to where it is mapped.
 *
 * This uses a pre-established static mapping if one exists for the requested
 * range, otherwise it allocates kva space and maps the physical pages into it.
 *
 * This routine is intended to be used for mapping device memory, NOT real
 * memory; the mapping type is inherently VM_MEMATTR_DEVICE in
 * pmap_kenter_device().
 */
void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{
	return (pmap_mapdev_attr(pa, size, VM_MEMATTR_DEVICE));
}

void *
pmap_mapdev_attr(vm_paddr_t pa, vm_size_t size, vm_memattr_t ma)
{
	vm_offset_t va, offset;
#ifdef __HAVE_STATIC_DEVMAP
	void * rva;

	/*
	 * First look in the static mapping table. These are all mapped
	 * as device memory, so only use the devmap for VM_MEMATTR_DEVICE.
	 */
	if ((rva = devmap_ptov(pa, size)) != NULL) {
		KASSERT(ma == VM_MEMATTR_DEVICE,
		    ("%s: Non-device mapping for pa %jx (type %x)", __func__,
		    (uintmax_t)pa, ma));
		return (rva);
	}
#endif

	offset = pa & PAGE_MASK;
	pa = trunc_page(pa);
	size = round_page(size + offset);

#ifdef PMAP_MAPDEV_EARLY_SIZE
	if (early_boot) {
		akva_devmap_vaddr = trunc_page(akva_devmap_vaddr - size);
		va = akva_devmap_vaddr;
		KASSERT(va >= (VM_MAX_KERNEL_ADDRESS - PMAP_MAPDEV_EARLY_SIZE),
		    ("%s: Too many early devmap mappings", __func__));
	} else
#endif
#ifdef __aarch64__
	if (size >= L2_SIZE && (pa & L2_OFFSET) == 0)
		va = kva_alloc_aligned(size, L2_SIZE);
	else if (size >= L3C_SIZE && (pa & L3C_OFFSET) == 0)
		va = kva_alloc_aligned(size, L3C_SIZE);
	else
#endif
		va = kva_alloc(size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");

	pmap_kenter(va, size, pa, ma);

	return ((void *)(va + offset));
}

/*
 * Unmap device memory and free the kva space.
 */
void
pmap_unmapdev(void *p, vm_size_t size)
{
	vm_offset_t offset, va;

#ifdef __HAVE_STATIC_DEVMAP
	/* Nothing to do if we find the mapping in the static table. */
	if (devmap_vtop(p, size) != DEVMAP_PADDR_NOTFOUND)
		return;
#endif

	va = (vm_offset_t)p;
	offset = va & PAGE_MASK;
	va = trunc_page(va);
	size = round_page(size + offset);

	pmap_kremove_device(va, size);
	kva_free(va, size);
}

#ifdef DDB
#ifdef __HAVE_STATIC_DEVMAP
#include <ddb/ddb.h>

DB_SHOW_COMMAND_FLAGS(devmap, db_show_devmap, DB_CMD_MEMSAFE)
{
	devmap_dump_table(db_printf);
}

#endif
#endif /* DDB */
