/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/linker.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>

#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pc/bios.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>

#include "vmm_util.h"
#include "vmm_mem.h"

SYSCTL_DECL(_hw_vmm);

static u_long pages_allocated;
SYSCTL_ULONG(_hw_vmm, OID_AUTO, pages_allocated, CTLFLAG_RD,
	     &pages_allocated, 0, "4KB pages allocated");

static void
update_pages_allocated(int howmany)
{
	pages_allocated += howmany;	/* XXX locking? */
}

int
vmm_mem_init(void)
{

	return (0);
}

vm_paddr_t
vmm_mem_alloc(size_t size)
{
	int flags;
	vm_page_t m;
	vm_paddr_t pa;

	if (size != PAGE_SIZE)
		panic("vmm_mem_alloc: invalid allocation size %lu", size);

	flags = VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
		VM_ALLOC_ZERO;

	while (1) {
		/*
		 * XXX need policy to determine when to back off the allocation
		 */
		m = vm_page_alloc(NULL, 0, flags);
		if (m == NULL)
			VM_WAIT;
		else
			break;
	}

	pa = VM_PAGE_TO_PHYS(m);
	
	if ((m->flags & PG_ZERO) == 0)
		pagezero((void *)PHYS_TO_DMAP(pa));
	m->valid = VM_PAGE_BITS_ALL;

	update_pages_allocated(1);

	return (pa);
}

void
vmm_mem_free(vm_paddr_t base, size_t length)
{
	vm_page_t m;

	if (base & PAGE_MASK) {
		panic("vmm_mem_free: base 0x%0lx must be aligned on a "
		      "0x%0x boundary\n", base, PAGE_SIZE);
	}

	if (length != PAGE_SIZE)
		panic("vmm_mem_free: invalid length %lu", length);

	m = PHYS_TO_VM_PAGE(base);
	m->wire_count--;
	vm_page_free(m);
	atomic_subtract_int(&cnt.v_wire_count, 1);

	update_pages_allocated(-1);
}

vm_paddr_t
vmm_mem_maxaddr(void)
{

	return (ptoa(Maxmem));
}
