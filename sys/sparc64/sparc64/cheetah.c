/*-
 * Copyright (c) 2003 Jake Burkholder.
 * Copyright (c) 2005, 2008, 2010 Marius Strobl <marius@FreeBSD.org>
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

#include <machine/asi.h>
#include <machine/cache.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/dcr.h>
#include <machine/lsu.h>
#include <machine/mcntl.h>
#include <machine/smp.h>
#include <machine/tlb.h>
#include <machine/ver.h>
#include <machine/vmparam.h>

#define	CHEETAH_ICACHE_TAG_LOWER	0x30

/*
 * CPU-specific initialization - this is used for both the Sun Cheetah and
 * later as well as the Fujitsu Zeus and later CPUs.
 */
void
cheetah_init(u_int cpu_impl)
{
	u_long val;

	/* Ensure the TSB Extension Registers hold 0 as TSB_Base. */

	stxa(AA_DMMU_TSB_PEXT_REG, ASI_DMMU, 0);
	stxa(AA_IMMU_TSB_PEXT_REG, ASI_IMMU, 0);
	membar(Sync);

	stxa(AA_DMMU_TSB_SEXT_REG, ASI_DMMU, 0);
	/*
	 * NB: the secondary context was removed from the iMMU.
	 */
	membar(Sync);

	stxa(AA_DMMU_TSB_NEXT_REG, ASI_DMMU, 0);
	stxa(AA_IMMU_TSB_NEXT_REG, ASI_IMMU, 0);
	membar(Sync);

	if (cpu_impl == CPU_IMPL_SPARC64V) {
		/* Ensure MCNTL_JPS1_TSBP is 0. */
		val = ldxa(AA_MCNTL, ASI_MCNTL);
		val &= ~MCNTL_JPS1_TSBP;
		stxa(AA_MCNTL, ASI_MCNTL, val);
		return;
	}

	/*
	 * Configure the first large dTLB to hold 4MB pages (e.g. for direct
	 * mappings) for all three contexts and ensure the second one is set
	 * up to hold 8k pages for them.  Note that this is constraint by
	 * US-IV+, whose large dTLBs can only hold entries of certain page
	 * sizes each.
	 * For US-IV+, additionally ensure that the large iTLB is set up to
	 * hold 8k pages for nucleus and primary context (still no secondary
	 * iMMU context.
	 * NB: according to documentation, changing the page size of the same
	 * context requires a context demap before changing the corresponding
	 * page size, but we hardly can flush our locked pages here, so we use
	 * a demap all instead.
	 */
	stxa(TLB_DEMAP_ALL, ASI_DMMU_DEMAP, 0);
	membar(Sync);
	val = (TS_4M << TLB_PCXR_N_PGSZ0_SHIFT) |
	    (TS_8K << TLB_PCXR_N_PGSZ1_SHIFT) |
	    (TS_4M << TLB_PCXR_P_PGSZ0_SHIFT) |
	    (TS_8K << TLB_PCXR_P_PGSZ1_SHIFT);
	if (cpu_impl == CPU_IMPL_ULTRASPARCIVp)
		val |= (TS_8K << TLB_PCXR_N_PGSZ_I_SHIFT) |
		    (TS_8K << TLB_PCXR_P_PGSZ_I_SHIFT);
	stxa(AA_DMMU_PCXR, ASI_DMMU, val);
	val = (TS_4M << TLB_SCXR_S_PGSZ0_SHIFT) |
	    (TS_8K << TLB_SCXR_S_PGSZ1_SHIFT);
	stxa(AA_DMMU_SCXR, ASI_DMMU, val);
	flush(KERNBASE);

	/*
	 * Ensure DCR_IFPOE is disabled as long as we haven't implemented
	 * support for it (if ever) as most if not all firmware versions
	 * apparently turn it on.  Not making use of DCR_IFPOE should also
	 * avoid Cheetah erratum #109.
	 */
	val = rd(asr18) & ~DCR_IFPOE;
	if (cpu_impl == CPU_IMPL_ULTRASPARCIVp) {
		/*
		 * Ensure the branch prediction mode is set to PC indexing
		 * in order to work around US-IV+ erratum #2.
		 */
		val = (val & ~DCR_BPM_MASK) | DCR_BPM_PC;
		/*
		 * XXX disable dTLB parity error reporting as otherwise we
		 * get seemingly false positives when copying in the user
		 * window by simulating a fill trap on return to usermode in
		 * case single issue is disabled, which thus appears to be
		 * a CPU bug.
		 */
		val &= ~DCR_DTPE;
	}
	wr(asr18, val, 0);
}

/*
 * Enable level 1 caches.
 */
void
cheetah_cache_enable(u_int cpu_impl)
{
	u_long lsu;

	lsu = ldxa(0, ASI_LSU_CTL_REG);
	if (cpu_impl == CPU_IMPL_ULTRASPARCIII) {
		/* Disable P$ due to US-III erratum #18. */
		lsu &= ~LSU_PE;
	}
	stxa(0, ASI_LSU_CTL_REG, lsu | LSU_IC | LSU_DC);
	flush(KERNBASE);
}

/*
 * Flush all lines from the level 1 caches.
 */
void
cheetah_cache_flush(void)
{
	u_long addr, lsu;
	register_t s;

	s = intr_disable();
	for (addr = 0; addr < PCPU_GET(cache.dc_size);
	    addr += PCPU_GET(cache.dc_linesize))
		/*
		 * Note that US-IV+ additionally require a membar #Sync before
		 * a load or store to ASI_DCACHE_TAG.
		 */
		__asm __volatile(
		    "membar #Sync;"
		    "stxa %%g0, [%0] %1;"
		    "membar #Sync"
		    : : "r" (addr), "n" (ASI_DCACHE_TAG));

	/* The I$ must be disabled when flushing it so ensure it's off. */
	lsu = ldxa(0, ASI_LSU_CTL_REG);
	stxa(0, ASI_LSU_CTL_REG, lsu & ~(LSU_IC));
	flush(KERNBASE);
	for (addr = CHEETAH_ICACHE_TAG_LOWER;
	    addr < PCPU_GET(cache.ic_size) * 2;
	    addr += PCPU_GET(cache.ic_linesize) * 2)
		__asm __volatile(
		    "stxa %%g0, [%0] %1;"
		    "membar #Sync"
		    : : "r" (addr), "n" (ASI_ICACHE_TAG));
	stxa(0, ASI_LSU_CTL_REG, lsu);
	flush(KERNBASE);
	intr_restore(s);
}

/*
 * Flush a physical page from the data cache.
 */
void
cheetah_dcache_page_inval(vm_paddr_t spa)
{
	vm_paddr_t pa;
	void *cookie;

	KASSERT((spa & PAGE_MASK) == 0,
	    ("%s: pa not page aligned", __func__));
	cookie = ipi_dcache_page_inval(tl_ipi_cheetah_dcache_page_inval, spa);
	for (pa = spa; pa < spa + PAGE_SIZE;
	    pa += PCPU_GET(cache.dc_linesize))
		stxa_sync(pa, ASI_DCACHE_INVALIDATE, 0);
	ipi_wait(cookie);
}

/*
 * Flush a physical page from the intsruction cache.  Instruction cache
 * consistency is maintained by hardware.
 */
void
cheetah_icache_page_inval(vm_paddr_t pa __unused)
{

}

#define	cheetah_dmap_all() do {						\
	stxa(TLB_DEMAP_ALL, ASI_DMMU_DEMAP, 0);				\
	stxa(TLB_DEMAP_ALL, ASI_IMMU_DEMAP, 0);				\
	flush(KERNBASE);						\
} while (0)

/*
 * Flush all non-locked mappings from the TLB.
 */
void
cheetah_tlb_flush_nonlocked(void)
{

	cheetah_dmap_all();
}

/*
 * Flush all user mappings from the TLB.
 */
void
cheetah_tlb_flush_user()
{

	/*
	 * Just use cheetah_dmap_all() and accept somes TLB misses
	 * rather than searching all 1040 D-TLB and 144 I-TLB slots
	 * for non-kernel mappings.
	 */
	cheetah_dmap_all();
}
