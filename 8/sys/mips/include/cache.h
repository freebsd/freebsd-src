/*	$NetBSD: cache.h,v 1.6 2003/02/17 11:35:01 simonb Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Cache operations.
 *
 * We define the following primitives:
 *
 * --- Instruction cache synchronization (mandatory):
 *
 *	icache_sync_all		Synchronize I-cache
 *
 *	icache_sync_range	Synchronize I-cache range
 *
 *	icache_sync_range_index	(index ops)
 *
 * --- Primary data cache (mandatory):
 *
 *	pdcache_wbinv_all	Write-back Invalidate primary D-cache
 *
 *	pdcache_wbinv_range	Write-back Invalidate primary D-cache range
 *
 *	pdcache_wbinv_range_index (index ops)
 *
 *	pdcache_inv_range	Invalidate primary D-cache range
 *
 *	pdcache_wb_range	Write-back primary D-cache range
 *
 * --- Secondary data cache (optional):
 *
 *	sdcache_wbinv_all	Write-back Invalidate secondary D-cache
 *
 *	sdcache_wbinv_range	Write-back Invalidate secondary D-cache range
 *
 *	sdcache_wbinv_range_index (index ops)
 *
 *	sdcache_inv_range	Invalidate secondary D-cache range
 *
 *	sdcache_wb_range	Write-back secondary D-cache range
 *
 * There are some rules that must be followed:
 *
 *	I-cache Synch (all or range):
 *		The goal is to synchronize the instruction stream,
 *		so you may need to write-back dirty data cache
 *		blocks first.  If a range is requested, and you
 *		can't synchronize just a range, you have to hit
 *		the whole thing.
 *
 *	D-cache Write-back Invalidate range:
 *		If you can't WB-Inv a range, you must WB-Inv the
 *		entire D-cache.
 *
 *	D-cache Invalidate:
 *		If you can't Inv the D-cache without doing a
 *		Write-back, YOU MUST PANIC.  This is to catch
 *		errors in calling code.  Callers must be aware
 *		of this scenario, and must handle it appropriately
 *		(consider the bus_dma(9) operations).
 *
 *	D-cache Write-back:
 *		If you can't Write-back without doing an invalidate,
 *		that's fine.  Then treat this as a WB-Inv.  Skipping
 *		the invalidate is merely an optimization.
 *
 *	All operations:
 *		Valid virtual addresses must be passed to the
 *		cache operation.
 *
 * Finally, these primitives are grouped together in reasonable
 * ways.  For all operations described here, first the primary
 * cache is frobbed, then the secondary cache frobbed, if the
 * operation for the secondary cache exists.
 *
 *	mips_icache_sync_all	Synchronize I-cache
 *
 *	mips_icache_sync_range	Synchronize I-cache range
 *
 *	mips_icache_sync_range_index (index ops)
 *
 *	mips_dcache_wbinv_all	Write-back Invalidate D-cache
 *
 *	mips_dcache_wbinv_range	Write-back Invalidate D-cache range
 *
 *	mips_dcache_wbinv_range_index (index ops)
 *
 *	mips_dcache_inv_range	Invalidate D-cache range
 *
 *	mips_dcache_wb_range	Write-back D-cache range
 */

struct mips_cache_ops {
	void	(*mco_icache_sync_all)(void);
	void	(*mco_icache_sync_range)(vm_offset_t, vm_size_t);
	void	(*mco_icache_sync_range_index)(vm_offset_t, vm_size_t);

	void	(*mco_pdcache_wbinv_all)(void);
	void	(*mco_pdcache_wbinv_range)(vm_offset_t, vm_size_t);
	void	(*mco_pdcache_wbinv_range_index)(vm_offset_t, vm_size_t);
	void	(*mco_pdcache_inv_range)(vm_offset_t, vm_size_t);
	void	(*mco_pdcache_wb_range)(vm_offset_t, vm_size_t);

	/* These are called only by the (mipsNN) icache functions. */
	void    (*mco_intern_pdcache_wbinv_all)(void);
	void    (*mco_intern_pdcache_wbinv_range_index)(vm_offset_t, vm_size_t);
	void    (*mco_intern_pdcache_wb_range)(vm_offset_t, vm_size_t);

	void	(*mco_sdcache_wbinv_all)(void);
	void	(*mco_sdcache_wbinv_range)(vm_offset_t, vm_size_t);
	void	(*mco_sdcache_wbinv_range_index)(vm_offset_t, vm_size_t);
	void	(*mco_sdcache_inv_range)(vm_offset_t, vm_size_t);
	void	(*mco_sdcache_wb_range)(vm_offset_t, vm_size_t);

	/* These are called only by the (mipsNN) icache functions. */
	void    (*mco_intern_sdcache_wbinv_all)(void);
	void    (*mco_intern_sdcache_wbinv_range_index)(vm_offset_t, vm_size_t);
	void    (*mco_intern_sdcache_wb_range)(vm_offset_t, vm_size_t);
};

extern struct mips_cache_ops mips_cache_ops;

/* PRIMARY CACHE VARIABLES */
extern u_int mips_picache_size;
extern u_int mips_picache_line_size;
extern u_int mips_picache_ways;
extern u_int mips_picache_way_size;
extern u_int mips_picache_way_mask;

extern u_int mips_pdcache_size;		/* and unified */
extern u_int mips_pdcache_line_size;
extern u_int mips_pdcache_ways;
extern u_int mips_pdcache_way_size;
extern u_int mips_pdcache_way_mask;
extern int mips_pdcache_write_through;

extern int mips_pcache_unified;

/* SECONDARY CACHE VARIABLES */
extern u_int mips_sicache_size;
extern u_int mips_sicache_line_size;
extern u_int mips_sicache_ways;
extern u_int mips_sicache_way_size;
extern u_int mips_sicache_way_mask;

extern u_int mips_sdcache_size;		/* and unified */
extern u_int mips_sdcache_line_size;
extern u_int mips_sdcache_ways;
extern u_int mips_sdcache_way_size;
extern u_int mips_sdcache_way_mask;
extern int mips_sdcache_write_through;

extern int mips_scache_unified;

/* TERTIARY CACHE VARIABLES */
extern u_int mips_tcache_size;		/* always unified */
extern u_int mips_tcache_line_size;
extern u_int mips_tcache_ways;
extern u_int mips_tcache_way_size;
extern u_int mips_tcache_way_mask;
extern int mips_tcache_write_through;

extern u_int mips_dcache_align;
extern u_int mips_dcache_align_mask;

extern u_int mips_cache_alias_mask;
extern u_int mips_cache_prefer_mask;

#define	__mco_noargs(prefix, x)						\
do {									\
	(*mips_cache_ops.mco_ ## prefix ## p ## x )();			\
	if (*mips_cache_ops.mco_ ## prefix ## s ## x )			\
		(*mips_cache_ops.mco_ ## prefix ## s ## x )();		\
} while (/*CONSTCOND*/0)

#define	__mco_2args(prefix, x, a, b)					\
do {									\
	(*mips_cache_ops.mco_ ## prefix ## p ## x )((a), (b));		\
	if (*mips_cache_ops.mco_ ## prefix ## s ## x )			\
		(*mips_cache_ops.mco_ ## prefix ## s ## x )((a), (b));	\
} while (/*CONSTCOND*/0)

#define	mips_icache_sync_all()						\
	(*mips_cache_ops.mco_icache_sync_all)()

#define	mips_icache_sync_range(v, s)					\
	(*mips_cache_ops.mco_icache_sync_range)((v), (s))

#define	mips_icache_sync_range_index(v, s)				\
	(*mips_cache_ops.mco_icache_sync_range_index)((v), (s))

#define	mips_dcache_wbinv_all()						\
	__mco_noargs(, dcache_wbinv_all)

#define	mips_dcache_wbinv_range(v, s)					\
	__mco_2args(, dcache_wbinv_range, (v), (s))

#define	mips_dcache_wbinv_range_index(v, s)				\
	__mco_2args(, dcache_wbinv_range_index, (v), (s))

#define	mips_dcache_inv_range(v, s)					\
	__mco_2args(, dcache_inv_range, (v), (s))

#define	mips_dcache_wb_range(v, s)					\
	__mco_2args(, dcache_wb_range, (v), (s))

/*
 * Private D-cache functions only called from (currently only the
 * mipsNN) I-cache functions.
 */
#define mips_intern_dcache_wbinv_all()					\
	__mco_noargs(intern_, dcache_wbinv_all)

#define mips_intern_dcache_wbinv_range_index(v, s)			\
	__mco_2args(intern_, dcache_wbinv_range_index, (v), (s))

#define mips_intern_dcache_wb_range(v, s)				\
	__mco_2args(intern_, dcache_wb_range, (v), (s))

/* forward declaration */
struct mips_cpuinfo;

void    mips_config_cache(struct mips_cpuinfo *);
void    mips_dcache_compute_align(void);

#include <machine/cache_mipsNN.h>
