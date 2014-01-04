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
__FBSDID("$FreeBSD$");

/*
 * Routines for mapping device memory.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <machine/armreg.h>
#include <machine/devmap.h>

static const struct arm_devmap_entry *devmap_table;
static boolean_t devmap_bootstrap_done = false;

/*
 * The allocated-kva (akva) devmap table and metadata.  Platforms can call
 * arm_devmap_add_entry() to add static device mappings to this table using
 * automatically allocated virtual addresses carved out of the top of kva space.
 * Allocation begins immediately below the ARM_VECTORS_HIGH address.
 */
#define	AKVA_DEVMAP_MAX_ENTRIES	32
static struct arm_devmap_entry	akva_devmap_entries[AKVA_DEVMAP_MAX_ENTRIES];
static u_int			akva_devmap_idx;
static vm_offset_t		akva_devmap_vaddr = ARM_VECTORS_HIGH;

/*
 * Return the "last" kva address used by the registered devmap table.  It's
 * actually the lowest address used by the static mappings, i.e., the address of
 * the first unusable byte of KVA.
 */
vm_offset_t
arm_devmap_lastaddr()
{
	const struct arm_devmap_entry *pd;
	vm_offset_t lowaddr;

	if (akva_devmap_idx > 0)
		return (akva_devmap_vaddr);

	if (devmap_table == NULL)
		panic("arm_devmap_lastaddr(): No devmap table registered.");

	lowaddr = ARM_VECTORS_HIGH;
	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
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
 * needs and when initarm() calls arm_devmap_bootstrap() it will pick up all the
 * entries in the akva table automatically.
 */
void
arm_devmap_add_entry(vm_paddr_t pa, vm_size_t sz)
{
	struct arm_devmap_entry *m;

	if (devmap_bootstrap_done)
		panic("arm_devmap_add_entry() after arm_devmap_bootstrap()");

	if (akva_devmap_idx == (AKVA_DEVMAP_MAX_ENTRIES - 1))
		panic("AKVA_DEVMAP_MAX_ENTRIES is too small");

	if (akva_devmap_idx == 0)
		arm_devmap_register_table(akva_devmap_entries);

	/*
	 * Allocate virtual address space from the top of kva downwards.  If the
	 * range being mapped is aligned and sized to 1MB boundaries then also
	 * align the virtual address to the next-lower 1MB boundary so that we
	 * end up with a nice efficient section mapping.
	 */
	if ((pa & 0x000fffff) == 0 && (sz & 0x000fffff) == 0) {
		akva_devmap_vaddr = trunc_1mpage(akva_devmap_vaddr - sz);
	} else {
		akva_devmap_vaddr = trunc_page(akva_devmap_vaddr - sz);
	}
	m = &akva_devmap_entries[akva_devmap_idx++];
	m->pd_va    = akva_devmap_vaddr;
	m->pd_pa    = pa;
	m->pd_size  = sz;
	m->pd_prot  = VM_PROT_READ | VM_PROT_WRITE;
	m->pd_cache = PTE_DEVICE;
}

/*
 * Register the given table as the one to use in arm_devmap_bootstrap().
 */
void
arm_devmap_register_table(const struct arm_devmap_entry *table)
{

	devmap_table = table;
}

/*
 * Map all of the static regions in the devmap table, and remember the devmap
 * table so the mapdev, ptov, and vtop functions can do lookups later.
 *
 * If a non-NULL table pointer is given it is used unconditionally, otherwise
 * the previously-registered table is used.  This smooths transition from legacy
 * code that fills in a local table then calls this function passing that table,
 * and newer code that uses arm_devmap_register_table() in platform-specific
 * code, then lets the common initarm() call this function with a NULL pointer.
 */
void
arm_devmap_bootstrap(vm_offset_t l1pt, const struct arm_devmap_entry *table)
{
	const struct arm_devmap_entry *pd;

	/*
	 * If given a table pointer, use it, else ensure a table was previously
	 * registered.  This happens early in boot, and there's a good chance
	 * the panic message won't be seen, but there's not much we can do.
	 */
	if (table != NULL)
		devmap_table = table;
	else if (devmap_table == NULL)
		panic("arm_devmap_bootstrap(): No devmap table registered");

	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		pmap_map_chunk(l1pt, pd->pd_va, pd->pd_pa, pd->pd_size,
		    pd->pd_prot,pd->pd_cache);
	}

	devmap_bootstrap_done = true;
}

/*
 * Look up the given physical address in the static mapping data and return the
 * corresponding virtual address, or NULL if not found.
 */
void *
arm_devmap_ptov(vm_paddr_t pa, vm_size_t size)
{
	const struct arm_devmap_entry *pd;

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
vm_paddr_t
arm_devmap_vtop(void * vpva, vm_size_t size)
{
	const struct arm_devmap_entry *pd;
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

/*
 * Map a set of physical memory pages into the kernel virtual address space.
 * Return a pointer to where it is mapped.
 *
 * This uses a pre-established static mapping if one exists for the requested
 * range, otherwise it allocates kva space and maps the physical pages into it.
 *
 * This routine is intended to be used for mapping device memory, NOT real
 * memory; the mapping type is inherently PTE_DEVICE in pmap_kenter_device().
 */
void *
pmap_mapdev(vm_offset_t pa, vm_size_t size)
{
	vm_offset_t va, tmpva, offset;
	void * rva;

	/* First look in the static mapping table. */
	if ((rva = arm_devmap_ptov(pa, size)) != NULL)
		return (rva);
	
	offset = pa & PAGE_MASK;
	pa = trunc_page(pa);
	size = round_page(size + offset);
	
	va = kva_alloc(size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");

	for (tmpva = va; size > 0;) {
		pmap_kenter_device(tmpva, pa);
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	
	return ((void *)(va + offset));
}

/*
 * Unmap device memory and free the kva space.
 */
void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	vm_offset_t tmpva, offset;
	vm_size_t origsize;

	/* Nothing to do if we find the mapping in the static table. */
	if (arm_devmap_vtop((void*)va, size) != DEVMAP_PADDR_NOTFOUND)
		return;

	origsize = size;
	offset = va & PAGE_MASK;
	va = trunc_page(va);
	size = round_page(size + offset);

	for (tmpva = va; size > 0;) {
		pmap_kremove(tmpva);
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
	}

	kva_free(va, origsize);
}

