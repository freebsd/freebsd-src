/*-
 * Copyright (c) 2006 Fill this file and put your name here
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/cpuinfo.h>
#include <machine/cache.h>

struct mips_cache_ops mips_cache_ops;

void
mips_config_cache(struct mips_cpuinfo * cpuinfo)
{
	switch (cpuinfo->l1.ic_linesize) {
	case 16:
		mips_cache_ops.mco_icache_sync_all = mipsNN_icache_sync_all_16;
		mips_cache_ops.mco_icache_sync_range =
		    mipsNN_icache_sync_range_16;
		mips_cache_ops.mco_icache_sync_range_index =
		    mipsNN_icache_sync_range_index_16;
		break;
	case 32:
		mips_cache_ops.mco_icache_sync_all = mipsNN_icache_sync_all_32;
		mips_cache_ops.mco_icache_sync_range =
		    mipsNN_icache_sync_range_32;
		mips_cache_ops.mco_icache_sync_range_index =
		    mipsNN_icache_sync_range_index_32;
		break;
#ifdef TARGET_OCTEON
	case 128:
		mips_cache_ops.mco_icache_sync_all = mipsNN_icache_sync_all_128;
		mips_cache_ops.mco_icache_sync_range =
		    mipsNN_icache_sync_range_128;
		mips_cache_ops.mco_icache_sync_range_index =
		    mipsNN_icache_sync_range_index_128;
		break;
#endif

#ifdef MIPS_DISABLE_L1_CACHE
	case 0:
		mips_cache_ops.mco_icache_sync_all = cache_noop;
		mips_cache_ops.mco_icache_sync_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_icache_sync_range_index =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		break;
#endif
	default:
		panic("no Icache ops for %d byte lines",
		    cpuinfo->l1.ic_linesize);
	}

	switch (cpuinfo->l1.dc_linesize) {
	case 16:
		mips_cache_ops.mco_pdcache_wbinv_all =
		    mips_cache_ops.mco_intern_pdcache_wbinv_all =
		    mipsNN_pdcache_wbinv_all_16;
		mips_cache_ops.mco_pdcache_wbinv_range =
		    mipsNN_pdcache_wbinv_range_16;
		mips_cache_ops.mco_pdcache_wbinv_range_index =
		    mips_cache_ops.mco_intern_pdcache_wbinv_range_index =
		    mipsNN_pdcache_wbinv_range_index_16;
		mips_cache_ops.mco_pdcache_inv_range =
		    mipsNN_pdcache_inv_range_16;
		mips_cache_ops.mco_pdcache_wb_range =
		    mips_cache_ops.mco_intern_pdcache_wb_range =
		    mipsNN_pdcache_wb_range_16;
		break;
	case 32:
		mips_cache_ops.mco_pdcache_wbinv_all =
		    mips_cache_ops.mco_intern_pdcache_wbinv_all =
		    mipsNN_pdcache_wbinv_all_32;
		mips_cache_ops.mco_pdcache_wbinv_range =
		    mipsNN_pdcache_wbinv_range_32;
		mips_cache_ops.mco_pdcache_wbinv_range_index =
		    mips_cache_ops.mco_intern_pdcache_wbinv_range_index =
		    mipsNN_pdcache_wbinv_range_index_32;
		mips_cache_ops.mco_pdcache_inv_range =
		    mipsNN_pdcache_inv_range_32;
		mips_cache_ops.mco_pdcache_wb_range =
		    mips_cache_ops.mco_intern_pdcache_wb_range =
		    mipsNN_pdcache_wb_range_32;
		break;
#ifdef TARGET_OCTEON
	case 128:
		mips_cache_ops.mco_pdcache_wbinv_all =
		    mips_cache_ops.mco_intern_pdcache_wbinv_all =
		    mipsNN_pdcache_wbinv_all_128;
		mips_cache_ops.mco_pdcache_wbinv_range =
		    mipsNN_pdcache_wbinv_range_128;
		mips_cache_ops.mco_pdcache_wbinv_range_index =
		    mips_cache_ops.mco_intern_pdcache_wbinv_range_index =
		    mipsNN_pdcache_wbinv_range_index_128;
		mips_cache_ops.mco_pdcache_inv_range =
		    mipsNN_pdcache_inv_range_128;
		mips_cache_ops.mco_pdcache_wb_range =
		    mips_cache_ops.mco_intern_pdcache_wb_range =
		    mipsNN_pdcache_wb_range_128;
		break;
#endif		
#ifdef MIPS_DISABLE_L1_CACHE
	case 0:
		mips_cache_ops.mco_pdcache_wbinv_all = cache_noop;
		mips_cache_ops.mco_intern_pdcache_wbinv_all = cache_noop;
		mips_cache_ops.mco_pdcache_wbinv_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_pdcache_wbinv_range_index =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_intern_pdcache_wbinv_range_index =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_pdcache_inv_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_pdcache_wb_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_intern_pdcache_wb_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		break;
#endif
	default:
		panic("no Dcache ops for %d byte lines",
		    cpuinfo->l1.dc_linesize);
	}

	mipsNN_cache_init(cpuinfo);

#if 0
	if (mips_cpu_flags &
	    (CPU_MIPS_D_CACHE_COHERENT | CPU_MIPS_I_D_CACHE_COHERENT)) {
#ifdef CACHE_DEBUG
		printf("  Dcache is coherent\n");
#endif
		mips_cache_ops.mco_pdcache_wbinv_all = cache_noop;
		mips_cache_ops.mco_pdcache_wbinv_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_pdcache_wbinv_range_index =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_pdcache_inv_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_pdcache_wb_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
	}
	if (mips_cpu_flags & CPU_MIPS_I_D_CACHE_COHERENT) {
#ifdef CACHE_DEBUG
		printf("  Icache is coherent against Dcache\n");
#endif
		mips_cache_ops.mco_intern_pdcache_wbinv_all =
		    cache_noop;
		mips_cache_ops.mco_intern_pdcache_wbinv_range_index =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
		mips_cache_ops.mco_intern_pdcache_wb_range =
		    (void (*)(vaddr_t, vsize_t))cache_noop;
	}
#endif

	/* Check that all cache ops are set up. */
	if (mips_picache_size || 1) {   /* XXX- must have primary Icache */
		if (!mips_cache_ops.mco_icache_sync_all)
			panic("no icache_sync_all cache op");
		if (!mips_cache_ops.mco_icache_sync_range)
			panic("no icache_sync_range cache op");
		if (!mips_cache_ops.mco_icache_sync_range_index)
			panic("no icache_sync_range_index cache op");
	}
	if (mips_pdcache_size || 1) {   /* XXX- must have primary Icache */
		if (!mips_cache_ops.mco_pdcache_wbinv_all)
			panic("no pdcache_wbinv_all");
		if (!mips_cache_ops.mco_pdcache_wbinv_range)
			panic("no pdcache_wbinv_range");
		if (!mips_cache_ops.mco_pdcache_wbinv_range_index)
			panic("no pdcache_wbinv_range_index");
		if (!mips_cache_ops.mco_pdcache_inv_range)
			panic("no pdcache_inv_range");
		if (!mips_cache_ops.mco_pdcache_wb_range)
			panic("no pdcache_wb_range");
	}

	/* XXXMIPS: No secondary cache handlers yet */
#ifdef notyet
	if (mips_sdcache_size) {
		if (!mips_cache_ops.mco_sdcache_wbinv_all)
			panic("no sdcache_wbinv_all");
		if (!mips_cache_ops.mco_sdcache_wbinv_range)
			panic("no sdcache_wbinv_range");
		if (!mips_cache_ops.mco_sdcache_wbinv_range_index)
			panic("no sdcache_wbinv_range_index");
		if (!mips_cache_ops.mco_sdcache_inv_range)
			panic("no sdcache_inv_range");
		if (!mips_cache_ops.mco_sdcache_wb_range)
			panic("no sdcache_wb_range");
	}
#endif
}
