/*-
 * Copyright (c) 2003 Alan L. Cox <alc@cs.rice.edu>
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

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

void *
uma_small_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	static vm_pindex_t colour;
	vm_page_t m;
	void *va;
	int pflags;

	*flags = UMA_SLAB_PRIV;
	if ((wait & (M_NOWAIT|M_USE_RESERVE)) == M_NOWAIT)
		pflags = VM_ALLOC_INTERRUPT;
	else
		pflags = VM_ALLOC_SYSTEM;
	if (wait & M_ZERO)
		pflags |= VM_ALLOC_ZERO;
	for (;;) {
		m = vm_page_alloc(NULL, colour++, pflags | VM_ALLOC_NOOBJ);
		if (m == NULL) {
			if (wait & M_NOWAIT)
				return (NULL);
			else
				VM_WAIT;
		} else
			break;
	}
	va = (void *)PHYS_TO_DMAP(m->phys_addr);
	if ((wait & M_ZERO) && (m->flags & PG_ZERO) == 0)
		pagezero(va);
	return (va);
}

void
uma_small_free(void *mem, int size, u_int8_t flags)
{
	vm_page_t m;

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)mem));
	vm_page_lock_queues();
	vm_page_free(m);
	vm_page_unlock_queues();
}
