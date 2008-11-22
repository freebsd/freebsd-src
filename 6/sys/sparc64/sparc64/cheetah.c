/*-
 * Copyright (c) 2003 Jake Burkholder.
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

#include "opt_pmap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cache.h>
#include <machine/cpufunc.h>
#include <machine/smp.h>
#include <machine/tlb.h>

/*
 * Enable level 1 caches.
 */
void
cheetah_cache_enable(void)
{

}

/*
 * Flush all lines from the level 1 caches.
 */
void
cheetah_cache_flush(void)
{

}

/*
 * Flush a physical page from the data cache.
 */
void
cheetah_dcache_page_inval(vm_paddr_t spa)
{
	vm_paddr_t pa;
	void *cookie;

	KASSERT((spa & PAGE_MASK) == 0, ("%s: pa not page aligned", __func__));
	cookie = ipi_dcache_page_inval(tl_ipi_cheetah_dcache_page_inval, spa);
	for (pa = spa; pa < spa + PAGE_SIZE; pa += cache.dc_linesize)
		stxa_sync(pa, ASI_DCACHE_INVALIDATE, 0);
	ipi_wait(cookie);
}

/*
 * Flush a physical page from the intsruction cache.  Instruction cache
 * consistency is maintained by hardware.
 */
void
cheetah_icache_page_inval(vm_paddr_t pa)
{

}

/*
 * Flush all non-locked mappings from the TLB.
 */
void
cheetah_tlb_flush_nonlocked(void)
{

	panic("cheetah_tlb_flush_nonlocked");
}

/*
 * Flush all user mappings from the TLB.
 */
void
cheetah_tlb_flush_user(void)
{

	panic("cheetah_tlb_flush_user");
}
