/*-
 * Copyright (c) 2003 The FreeBSD Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_dumpset.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <machine/md_var.h>

static int hw_uma_mdpages;
SYSCTL_INT(_hw, OID_AUTO, uma_mdpages, CTLFLAG_RD, &hw_uma_mdpages, 0,
	   "UMA MD pages in use");

void *
uma_small_alloc(uma_zone_t zone, vm_size_t bytes, int domain, u_int8_t *flags,
    int wait)
{
	void *va;
	vm_paddr_t pa;
	vm_page_t m;

	*flags = UMA_SLAB_PRIV;

	m = vm_page_alloc_noobj_domain(domain, malloc2vm_flags(wait) |
	    VM_ALLOC_WIRED);
	if (m == NULL) 
		return (NULL);

	pa = VM_PAGE_TO_PHYS(m);
#ifdef __powerpc64__
	if ((wait & M_NODUMP) == 0)
		dump_add_page(pa);
#endif

	if (!hw_direct_map) {
		pmap_kenter(pa, pa);
		va = (void *)(vm_offset_t)pa;
	} else {
		va = (void *)(vm_offset_t)PHYS_TO_DMAP(pa);
	}
	atomic_add_int(&hw_uma_mdpages, 1);

	return (va);
}

void
uma_small_free(void *mem, vm_size_t size, u_int8_t flags)
{
	vm_page_t m;

	if (hw_direct_map)
		m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)mem));
	else {
		m = PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)mem));
		pmap_kremove((vm_offset_t)mem);
	}

	KASSERT(m != NULL,
	    ("Freeing UMA block at %p with no associated page", mem));
#ifdef __powerpc64__
	dump_drop_page(VM_PAGE_TO_PHYS(m));
#endif
	vm_page_unwire_noq(m);
	vm_page_free(m);
	atomic_subtract_int(&hw_uma_mdpages, 1);
}
