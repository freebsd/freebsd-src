/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <vm/vm.h> 
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_zone.h>

struct msgbuf *msgbufp;

vm_offset_t avail_start;
vm_offset_t avail_end;
vm_offset_t kernel_vm_end;
vm_offset_t phys_avail[10];
vm_offset_t virtual_avail;
vm_offset_t virtual_end;

struct pmap __kernel_pmap;

static boolean_t pmap_initialized = FALSE;

void
pmap_activate(struct proc *p)
{
	TODO;
}

vm_offset_t
pmap_addr_hint(vm_object_t object, vm_offset_t va, vm_size_t size)
{
	TODO;
	return (0);
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	TODO;
}

void
pmap_clear_modify(vm_page_t m)
{
	TODO;
}

void
pmap_collect(void)
{
	TODO;
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
	  vm_size_t len, vm_offset_t src_addr)
{
	TODO;
}

void
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
{
	TODO;
}

void
pmap_zero_page(vm_offset_t pa)
{
	TODO;
}

void
pmap_zero_page_area(vm_offset_t pa, int off, int size)
{
	TODO;
}

void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
	   boolean_t wired)
{
	TODO;
}

vm_offset_t
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	TODO;
	return (0);
}

void
pmap_growkernel(vm_offset_t addr)
{
	TODO;
}

void
pmap_init(vm_offset_t phys_start, vm_offset_t phys_end)
{
	TODO;
}

void
pmap_init2(void)
{
	TODO;
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	TODO;
	return (0);
}

void
pmap_clear_reference(vm_page_t m)
{
	TODO;
}

int
pmap_ts_referenced(vm_page_t m)
{
	TODO;
	return (0);
}

void
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	TODO;
}

vm_offset_t
pmap_kextract(vm_offset_t va)
{
	TODO;
	return (0);
}

void
pmap_kremove(vm_offset_t va)
{
	TODO;
}

vm_offset_t
pmap_map(vm_offset_t *va, vm_offset_t start, vm_offset_t end, int prot)
{
	TODO;
	return (0);
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{
	TODO;
	return (0);
}

void
pmap_new_proc(struct proc *p)
{
	TODO;
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
		    vm_pindex_t pindex, vm_size_t size, int limit)
{
	TODO;
}

void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	TODO;
}

void
pmap_pageable(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
	      boolean_t pageable)
{
	TODO;
}

boolean_t
pmap_page_exists(pmap_t pmap, vm_page_t m)
{
	TODO;
	return (0);
}

void
pmap_pinit(pmap_t pmap)
{
	TODO;
}

void
pmap_pinit0(pmap_t pmap)
{
	TODO;
}

void
pmap_pinit2(pmap_t pmap)
{
	TODO;
}

void
pmap_prefault(pmap_t pmap, vm_offset_t va, vm_map_entry_t entry)
{
	TODO;
}

void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	TODO;
}

vm_offset_t
pmap_phys_address(int ppn)
{
	TODO;
	return (0);
}

void
pmap_qenter(vm_offset_t va, vm_page_t *m, int count)
{
	TODO;
}

void
pmap_qremove(vm_offset_t va, int count)
{
	TODO;
}

void
pmap_reference(pmap_t pmap)
{
	TODO;
}

void
pmap_release(pmap_t pmap)
{
	TODO;
}

void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	TODO;
}

void
pmap_remove_pages(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	TODO;
}

void
pmap_swapin_proc(struct proc *p)
{
	TODO;
}

void
pmap_swapout_proc(struct proc *p)
{
	TODO;
}
