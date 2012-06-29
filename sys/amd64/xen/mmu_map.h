/* $FreeBSD$ */
/*-
 * Copyright (c) 2011-2012 Spectra Logic Corporation
 * All rights reserved.
 *
 * This software was developed by Cherry G. Mathew <cherry@FreeBSD.org>
 * under sponsorship from Spectra Logic Corporation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef _XEN_MMU_MAP_H_
#define _XEN_MMU_MAP_H_

#include <sys/types.h>

#include <machine/pmap.h>

/* 
 *
 * This API abstracts, in an MI fashion, the paging mechanism of an
 * arbitrary CPU architecture as an opaque FSM, which may then be
 * subject to inspection in MD ways.
 *
 * Use of this API can have the following effects on the VM system and
 * the kernel address space:
 *
 * - physical memory pages may be allocated.
 * - physical memory pages may be de-allocated.
 * - kernel virtual address space may be allocated.
 * - kernel virtual address space may be de-allocated.
 * - The page table hierarchy may be modified.
 * - TLB entries may be invalidated.
 *
 * The API is stateful, and designed around the following principles:
 * - Simplicity
 * - Object orientation
 * - Code reuse.
 */

/* 
 * We hide the page table structure behind an opaque "index" cookie
 * which acts as the "key" to a given va->pa mapping being inspected.
 */
typedef void * mmu_map_t;

/*
 * Memory backend types:
 * 
 * We provide a means to allocate ad-hoc memory/physical page
 * requirements to the paging mechanism by means of a "backend"
 * alloc function
 *
 * The memory backend is required to provide physical pages that are
 * at least temporarily mapped into the kernel VA space and whose
 * contents are thus accessible by a simple pointer indirection from
 * within the kernel. This requirement may be revoked after conclusion
 * of an instance of stateful usage of the API ( See:
 * mmu_map_t_fini() below ), at which point the backend
 * implementation is free to unmap any temporary mappings if so
 * desired. (XXX: review this for non-x86)
 *
 * Note: Only the mappings may be revoked - any physical pages
 * themselves allocated by the backend are considered allocated and
 * part of the paging mechanism.
 */

struct mmu_map_mbackend { /* Callbacks */

	vm_offset_t (*alloc)(void);
	void (*free)(vm_offset_t); /* May be NULL */

	/* 
	 * vtop()/ptov() conversion functions:
	 * These callbacks typically provide conversions for mapped
	 * pages allocated via the alloc()/free() callbacks (above).
	 * The API implementation is free to cache the mappings across
	 * multiple instances of use; ie; mappings may persist across 
	 * one pair of mmu_map_t_init()/.._finit() calls.
	 */
	vm_offset_t (*ptov)(vm_paddr_t);
	vm_paddr_t (*vtop)(vm_offset_t);
};

/* 
 * Return sizeof (mmu_map_t) as implemented within the api
 * This may then be used to allocate untyped memory for the cookie
 * which can then be operated on opaquely behind the API in a machine
 * specific manner.
 */
size_t mmu_map_t_size(void);

/*
 * Initialise the API state to use a specified memory backend 
 */
void mmu_map_t_init(mmu_map_t, struct mmu_map_mbackend *);

/* Conclude this instance of use of the API */
void mmu_map_t_fini(mmu_map_t);

/* Set "index" cookie state based on va lookup. This state may then be
 * inspected in MD ways ( See below ). Note that every call to the
 * following functions can change the state of the backing paging
 * mechanism FSM.
 */
bool mmu_map_inspect_va(struct pmap *, mmu_map_t, vm_offset_t);
/* 
 * Unconditionally allocate resources to setup and "inspect" (as
 * above) a given va->pa mapping 
 */
void mmu_map_hold_va(struct pmap *,  mmu_map_t, vm_offset_t);

/* Optionally release resources after tear down of a va->pa mapping */
void mmu_map_release_va(struct pmap *, mmu_map_t, vm_offset_t);

/* 
 * Machine dependant "view" into the page table hierarchy FSM.
 * On amd64, there are four tables that are consulted for a va->pa
 * translation. This information may be extracted by the MD functions
 * below and is only considered valid between a successful call to
 * mmu_map_inspect_va() or mmu_map_hold_va() and a subsequent
 * call to mmu_map_release_va()
 */
pd_entry_t * mmu_map_pml4t(mmu_map_t); /* Page Map Level 4 Table */
pd_entry_t * mmu_map_pdpt(mmu_map_t);  /* Page Directory Pointer Table */
pd_entry_t * mmu_map_pdt(mmu_map_t);   /* Page Directory Table */
pd_entry_t * mmu_map_pt(mmu_map_t);    /* Page Table */

#endif /*  !_XEN_MMU_MAP_H_ */
