/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */
/*-
 * Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: @(#)cache.c	8.2 (Berkeley) 10/30/93
 *	from: NetBSD: cache.c,v 1.5 2000/12/06 01:47:50 mrg Exp
 *
 * $FreeBSD$
 */

/*
 * Cache routines.
 *
 * UltraSPARCs have an virtually indexed, physically tagged (VIPT) level 1 data
 * cache (D$) and physically indexed, physically tagged (PIPT) level 1
 * instruction (I$) and Level 2 (E$) caches.
 * D$ is directly mapped, I$ is pseudo 2-way associative. The Level 2 cache (E$)
 * is documented to be directly mapped on the UltraSPARC IIi, but there are
 * apparently models (using the IIe version) that have a 4-way associative E$.
 *
 * D$ uses a write-through model, while E$ uses write-back and is
 * write-allocating. The lines present in D$ are forced to be a subset of those
 * in E$.
 * This means that lines that are present in D$ always have an identical
 * corresponding (sub-) line in E$.
 *
 * The term "main memory" is used in the following to refer to the non-cache
 * memory as well as to memory-mapped device i/o space.
 *
 * There are 3 documented ways to flush the D$ and E$ caches:
 * - displacement flushing (a sequence of loads of addresses that alias to
 *   to-be-flushed ones in the caches). This only works for directly mapped
 *   caches, and is recommended to flush D$ and E$ in the IIi manual. It is not
 *   used to flush E$ because of the aforementioned models that have a
 *   multiple-associative E$. Displacement flushing invalidates the cache
 *   entries and writes modified lines back to main memory.
 * - diagnostic acceses can be used to invalidate cache pages. All lines
 *   are discarded, which means that changes in D$/E$ that have not been
 *   committed to main memory are lost.
 * - block-commit stores. Those use the block transfer ASIs to load a
 *   64-byte block to a set of FP registers and store them back using a
 *   special ASI that will cause the data to be immediately committed to main
 *   memory. This method has the same properties as the first method, but
 *   (hopefully) works regardless of the associativity of the caches. It is
 *   expected to be slow.
 *
 * I$ can be handled using the flush instruction.
 *
 * Some usage guidelines:
 *
 * The inval functions are variants of the flush ones that discard modified
 * cache lines.
 * PCI DMA transactions are cache-coherent and do not require flushing
 * before DMA reads or after DMA writes. It is unclear from the manual
 * how far this applies to UPA transactions.
 *
 * icache_flush(): needed before code that has been written to memory is
 *	executed, because I$ is not necessarily consistent with D$, E$, or
 *	main memory. An exception is that I$ snoops DMA transfers, so no
 *	flush is required after to-be-executed data has been fetched this way.
 * icache_inval_phys(): has roughly same effect as icache_flush() since there
 *	are no writes to I$.
 *
 * dcache_flush(): required when a page mapping is changed from cacheable to
 *	noncacheable, or to resolve illegal aliases. Both cases should happen
 *	seldom. Mapping address changes do not require this, since D$ is VIPT.
 * dcache_inval(): has roughly same effect as dcache_flush() since D$ is
 *	write-through.
 * dcache_blast(): discards all lines in D$.
 *
 * ecache_flush(): needed to commit modified lines to main memory, and to make
 *	sure that no stale data is used when the main memory has changed without
 *	the cache controller noticing. This is e.g. needed for device i/o space.
 *	It is usually better to use a non-cacheable mapping in this case.
 *	ecache_flush() is guaranteed to also flush the relevant addresses out of
 *	D$.
 * ecache_inval_phys():  like ecache_flush(), but invalidates a physical range
 *	in the cache. This function is usually dangerous and should not be used.
 *
 * All operations have a line size granularity!
 *
 * All flush methods tend to be expensive, so unnecessary flushes should be
 * avoided.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cache.h>
#include <machine/cpufunc.h>
#include <machine/fp.h>
#include <machine/fsr.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/tte.h>
#include <machine/vmparam.h>

static struct cacheinfo cache;

#define	CDIAG_CLR(asi, addr) \
	__asm __volatile("stxa %%g0, [%0] %1" : : "r" (addr), "I" (asi))
/* Read to %g0, needed for E$ access. */
#define	CDIAG_RDG0(asi, addr) \
	__asm __volatile("ldxa [%0] %1, %%g0" : : "r" (addr), "I" (asi))
#define	CDIAG_RD(asi, addr, r) \
	__asm __volatile("ldxa [%1] %2, %0" : "=r" (r) : "r" (addr), "I" (asi))
/* Sigh. I$ diagnostic registers want ldda. */
#define	ICDIAG_RD(asi, addr, r) \
	__asm __volatile("ldda [%1] %2, %%o4; mov %%o5, %0" : "=r" (r) : \
	    "r" (addr), "I" (asi) : "%o4", "%o5");

#define	OF_GET(h, n, v)	OF_getprop((h), (n), &(v), sizeof(v))

/*
 * Fill in the cache parameters using the cpu node.
 */
void
cache_init(phandle_t node)
{
	u_long set;

	if (OF_GET(node, "icache-size", cache.ic_size) == -1 ||
	    OF_GET(node, "icache-line-size", cache.ic_linesize) == -1 ||
	    OF_GET(node, "icache-associativity", cache.ic_assoc) == -1 ||
	    OF_GET(node, "dcache-size", cache.dc_size) == -1 ||
	    OF_GET(node, "dcache-line-size", cache.dc_linesize) == -1 ||
	    OF_GET(node, "dcache-associativity", cache.dc_assoc) == -1 ||
	    OF_GET(node, "ecache-size", cache.ec_size) == -1 ||
	    OF_GET(node, "ecache-line-size", cache.ec_linesize) == -1 ||
	    OF_GET(node, "ecache-associativity", cache.ec_assoc) == -1)
		panic("cache_init: could not retrieve cache parameters");

	set = cache.ic_size / cache.ic_assoc;
	cache.ic_l2set = ffs(set) - 1;
	if ((set & ~(1UL << cache.ic_l2set)) != 0)
		panic("cache_init: I$ set size not a power of 2");
	cache.dc_l2size = ffs(cache.dc_size) - 1;
	if ((cache.dc_size & ~(1UL << cache.dc_l2size)) != 0)
		panic("cache_init: D$ size not a power of 2");
	if (cache.dc_assoc != 1)
		panic("cache_init: D$ is not directly mapped!");
	set = cache.ec_size / cache.ec_assoc;
	cache.ec_l2set = ffs(set) - 1;
	if ((set & ~(1UL << cache.ec_l2set)) != 0)
		panic("cache_init: E$ set size not a power of 2");
	cache.c_enabled = 1; /* enable cache flushing */
}

/* Flush a range of addresses from I$ using the flush instruction. */
void
icache_flush(vm_offset_t start, vm_offset_t end)
{
	char *p, *ep;
	int ls;

	if (!cache.c_enabled)
		return;

	ls = cache.ic_linesize;
	ep = (char *)ulmin(end, start + cache.ic_size);
	for (p = (char *)start; p < ep; p += ls)
		flush(p);
}

/*
 * Blast a I$ physical range using diagnostic accesses.
 * NOTE: there is a race between checking the tag and invalidating it. It
 * cannot be closed by disabling interrupts, since the fetch for the next
 * instruction may be in that line, so we don't even bother.
 * Since blasting a line does not discard data, this has no ill effect except
 * a minor slowdown.
 */
void
icache_inval_phys(vm_offset_t start, vm_offset_t end)
{
	vm_offset_t addr, ica;
	u_long tag;
	u_long j;

	if (!cache.c_enabled)
		return;

	for (addr = start & ~(cache.ic_linesize - 1); addr <= end;
	     addr += cache.ic_linesize) {
		for (j = 0; j < 2; j++) {
			ica = (addr & (cache.ic_size - 1)) | ICDA_SET(j);
			ICDIAG_RD(ASI_ICACHE_TAG, ica, tag);
			if (ICDT_TAG(tag) != addr >> cache.ic_l2set)
				continue;
			CDIAG_CLR(ASI_ICACHE_TAG, ica);
		}
	}
}

/*
 * Flush a range of addresses from D$ using displacement flushes. This does
 * not necessarily flush E$, because we do not take care of flushing the
 * correct physical colors and E$ may not be directly mapped.
 */
void
dcache_flush(vm_offset_t start, vm_offset_t end)
{
	int j;
	vm_offset_t baseoff;
	u_long i, mask;
	char *kp;

	if (!cache.c_enabled)
		return;

	mask = cache.dc_size - 1;
	/* No need to flush lines more than once. */
	baseoff = start & mask;
	/*
	 * Use a locked page for flushing. D$ should be smaller than 4M, which
	 * is somewhat likely...
	 */
	kp = (char *)KERNBASE;
	j = 0;
	for (i = 0; i <= ulmin((end - start), cache.dc_size);
	     i += cache.dc_linesize)
		j += kp[(baseoff + i) & mask];
}

/*
 * Blast a D$ range using diagnostic accesses.
 * This has the same (harmless) races as icache_blast().
 * Assumes a page in the kernel map.
 */
void
dcache_inval(pmap_t pmap, vm_offset_t start, vm_offset_t end)
{
	vm_offset_t va, pa, offs, dca;
	u_long tag;

	if (!cache.c_enabled)
		return;
	for (va = start & ~(cache.dc_linesize - 1); va <= end;
	     va = (va + PAGE_SIZE_MIN) & ~PAGE_MASK_MIN) {
		if ((pa = pmap_extract(pmap, va)) == 0)
			continue;
		for (offs = start & PAGE_MASK_MIN;
		     offs < ulmin(PAGE_SIZE_MIN, end - va + 1);
		     offs += cache.dc_linesize) {
			dca = (va + offs) & (cache.dc_size - 1);
			CDIAG_RD(ASI_DCACHE_TAG, dca, tag);
			if (DCDT_TAG(tag) != (pa + offs) >> PAGE_SHIFT_MIN)
				continue;
			CDIAG_CLR(ASI_DCACHE_TAG, dca);
		}
	}
}

/* Discard all lines in D$. */
void
dcache_blast()
{
	vm_offset_t dca;

	if (!cache.c_enabled)
		return;
	for (dca = 0; dca < cache.dc_size; dca += cache.dc_linesize)
		CDIAG_CLR(ASI_DCACHE_TAG, dca);
}

/* Flush a E$ physical range using block commit stores. */
void
ecache_flush(vm_offset_t start, vm_offset_t end)
{
	vm_offset_t addr;

	if (!cache.c_enabled)
		return;

	/* XXX: not needed in all cases, provide a wrapper in fp.c */
	savefpctx(&curthread->td_pcb->pcb_fpstate);
	wr(fprs, 0, FPRS_FEF);

	for (addr = start & ~(cache.ec_linesize - 1); addr <= end;
	     addr += cache.ec_linesize) {
		__asm __volatile("ldda [%0] %1, %%f0; membar #Sync; "
		    "stda %%f0, [%0] %2" : : "r" (addr), "I" (ASI_BLK_S),
		    "I" (ASI_BLK_COMMIT_S));
	}
	membar(Sync);

	restorefpctx(&curthread->td_pcb->pcb_fpstate);
}

#if 0
/*
 * Blast a E$ range using diagnostic accesses.
 * This is disabled: it suffers from the same races as dcache_blast() and
 * icache_blast_phys(), but they may be fatal here because blasting an E$ line
 * can discard modified data.
 * There is no really use for this anyway.
 */
void
ecache_inval_phys(vm_offset_t start, vm_offset_t end)
{
	vm_offset_t addr, eca;
	critical_t c;
	u_long tag, j;

	if (!cache.c_enabled)
		return;

	for (addr = start & ~(cache.ec_linesize - 1); addr <= end;
	     addr += cache.ec_linesize) {
		for (j = 0; j < cache.ec_assoc; j++) {
			/* XXX: guesswork... */
			eca = (addr & (cache.ec_size - 1)) |
			    (j << (cache.ec_l2set));
			c = critical_enter();
			/*
			 * Retrieve the tag:
			 * A read from the appropriate VA in ASI_ECACHE_R
			 * will transfer the tag from the tag RAM to the
			 * data register (ASI_ECACHE_TAG_DATA, VA 0).
			 */
			CDIAG_RDG0(ASI_ECACHE_R, ECDA_TAG | eca);
			CDIAG_RD(ASI_ECACHE_TAG_DATA, 0, tag);
			if ((addr & ~cache.ec_size) >> cache.ec_l2set ==
			    (tag & ECDT_TAG_MASK)) {
				/*
				 * Clear. Works like retrieving the tag, but
				 * the other way round.
				 */
				CDIAG_CLR(ASI_ECACHE_TAG_DATA, 0);
				CDIAG_CLR(ASI_ECACHE_W, ECDA_TAG | eca);
			}
		}
	}
}
#endif
