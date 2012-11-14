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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <machine/vmparam.h>

void *
uma_small_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	void *va;
	vm_page_t m;
	int pflags;

	*flags = UMA_SLAB_PRIV;
	pflags = malloc2vm_flags(wait) | VM_ALLOC_WIRED;

	for (;;) {
		m = vm_page_alloc(NULL, 0, pflags | VM_ALLOC_NOOBJ);
		if (m == NULL) {
			if (wait & M_NOWAIT)
				return (NULL);
			VM_WAIT;
		} else
			break;
	}

	va = (void *)IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(m));
	if ((wait & M_ZERO) && (m->flags & PG_ZERO) == 0)
		bzero(va, PAGE_SIZE);
	return (va);
}

void
uma_small_free(void *mem, int size, u_int8_t flags)
{
	vm_page_t m;

	m = PHYS_TO_VM_PAGE(IA64_RR_MASK((u_int64_t)mem));
	m->wire_count--;
	vm_page_free(m);
	atomic_subtract_int(&cnt.v_wire_count, 1);
}
