/*	$NetBSD: cpufunc.c,v 1.65 2003/11/05 12:53:15 scw Exp $	*/

/*-
 * arm9 support code Copyright (C) 2001 ARM Ltd
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpufuncs.c
 *
 * C functions for supporting CPU / MMU / TLB specific operations.
 *
 * Created      : 30/01/97
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/disassem.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/uma.h>

#include <machine/cpuconf.h>
#include <machine/cpufunc.h>

#if defined(CPU_XSCALE_80321) || defined(CPU_XSCALE_80219)
#include <arm/xscale/i80321/i80321reg.h>
#include <arm/xscale/i80321/i80321var.h>
#endif

/*
 * Some definitions in i81342reg.h clash with i80321reg.h.
 * This only happens for the LINT kernel. As it happens,
 * we don't need anything from i81342reg.h that we already
 * got from somewhere else during a LINT compile.
 */
#if defined(CPU_XSCALE_81342) && !defined(COMPILING_LINT)
#include <arm/xscale/i8134x/i81342reg.h>
#endif

#ifdef CPU_XSCALE_IXP425
#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>
#endif

/* PRIMARY CACHE VARIABLES */
int	arm_picache_size;
int	arm_picache_line_size;
int	arm_picache_ways;

int	arm_pdcache_size;	/* and unified */
int	arm_pdcache_line_size;
int	arm_pdcache_ways;

int	arm_pcache_type;
int	arm_pcache_unified;

int	arm_dcache_align;
int	arm_dcache_align_mask;

u_int	arm_cache_level;
u_int	arm_cache_type[14];
u_int	arm_cache_loc;

int ctrl;

#ifdef CPU_ARM9
struct cpu_functions arm9_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	arm9_setttb,			/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	arm9_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	arm9_icache_sync_all,		/* icache_sync_all	*/
	arm9_icache_sync_range,		/* icache_sync_range	*/

	arm9_dcache_wbinv_all,		/* dcache_wbinv_all	*/
	arm9_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	arm9_dcache_inv_range,		/* dcache_inv_range	*/
	arm9_dcache_wb_range,		/* dcache_wb_range	*/

	armv4_idcache_inv_all,		/* idcache_inv_all	*/
	arm9_idcache_wbinv_all,		/* idcache_wbinv_all	*/
	arm9_idcache_wbinv_range,	/* idcache_wbinv_range	*/
	cpufunc_nullop,			/* l2cache_wbinv_all	*/
	(void *)cpufunc_nullop,		/* l2cache_wbinv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_inv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_wb_range	*/
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm9_context_switch,		/* context_switch	*/

	arm9_setup			/* cpu setup		*/

};
#endif /* CPU_ARM9 */

#if defined(CPU_ARM9E)
struct cpu_functions armv5_ec_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	armv5_ec_setttb,		/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	arm10_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	arm10_tlb_flushI_SE,		/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	armv5_ec_icache_sync_all,	/* icache_sync_all	*/
	armv5_ec_icache_sync_range,	/* icache_sync_range	*/

	armv5_ec_dcache_wbinv_all,	/* dcache_wbinv_all	*/
	armv5_ec_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	armv5_ec_dcache_inv_range,	/* dcache_inv_range	*/
	armv5_ec_dcache_wb_range,	/* dcache_wb_range	*/

	armv4_idcache_inv_all,		/* idcache_inv_all	*/
	armv5_ec_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	armv5_ec_idcache_wbinv_range,	/* idcache_wbinv_range	*/

	cpufunc_nullop,                 /* l2cache_wbinv_all    */
	(void *)cpufunc_nullop,         /* l2cache_wbinv_range  */
      	(void *)cpufunc_nullop,         /* l2cache_inv_range    */
	(void *)cpufunc_nullop,         /* l2cache_wb_range     */
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm10_context_switch,		/* context_switch	*/

	arm10_setup			/* cpu setup		*/

};

struct cpu_functions sheeva_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	sheeva_setttb,			/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	arm10_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	arm10_tlb_flushI_SE,		/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	armv5_ec_icache_sync_all,	/* icache_sync_all	*/
	armv5_ec_icache_sync_range,	/* icache_sync_range	*/

	armv5_ec_dcache_wbinv_all,	/* dcache_wbinv_all	*/
	sheeva_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	sheeva_dcache_inv_range,	/* dcache_inv_range	*/
	sheeva_dcache_wb_range,		/* dcache_wb_range	*/

	armv4_idcache_inv_all,		/* idcache_inv_all	*/
	armv5_ec_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	sheeva_idcache_wbinv_range,	/* idcache_wbinv_all	*/

	sheeva_l2cache_wbinv_all,	/* l2cache_wbinv_all    */
	sheeva_l2cache_wbinv_range,	/* l2cache_wbinv_range  */
	sheeva_l2cache_inv_range,	/* l2cache_inv_range    */
	sheeva_l2cache_wb_range,	/* l2cache_wb_range     */
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	sheeva_cpu_sleep,		/* sleep		*/

	/* Soft functions */

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	arm10_context_switch,		/* context_switch	*/

	arm10_setup			/* cpu setup		*/
};
#endif /* CPU_ARM9E */

#ifdef CPU_MV_PJ4B
struct cpu_functions pj4bv7_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	armv7_drain_writebuf,		/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* Domain		*/
	armv7_setttb,			/* Setttb		*/
	cpufunc_faultstatus,		/* Faultstatus		*/
	cpufunc_faultaddress,		/* Faultaddress		*/

	/* TLB functions */

	armv7_tlb_flushID,		/* tlb_flushID		*/
	armv7_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv7_tlb_flushID,		/* tlb_flushI		*/
	armv7_tlb_flushID_SE,		/* tlb_flushI_SE	*/
	armv7_tlb_flushID,		/* tlb_flushD		*/
	armv7_tlb_flushID_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */
	armv7_idcache_wbinv_all,	/* icache_sync_all	*/
	armv7_icache_sync_range,	/* icache_sync_range	*/

	armv7_dcache_wbinv_all,		/* dcache_wbinv_all	*/
	armv7_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	armv7_dcache_inv_range,		/* dcache_inv_range	*/
	armv7_dcache_wb_range,		/* dcache_wb_range	*/

	armv7_idcache_inv_all,		/* idcache_inv_all	*/
	armv7_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	armv7_idcache_wbinv_range,	/* idcache_wbinv_all	*/

	(void *)cpufunc_nullop,		/* l2cache_wbinv_all	*/
	(void *)cpufunc_nullop,		/* l2cache_wbinv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_inv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_wb_range	*/
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv7_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	(void *)cpufunc_nullop,		/* sleep		*/

	/* Soft functions */

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	armv7_context_switch,		/* context_switch	*/

	pj4bv7_setup			/* cpu setup		*/
};
#endif /* CPU_MV_PJ4B */

#if defined(CPU_XSCALE_80321) || \
  defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425) || \
  defined(CPU_XSCALE_80219)

struct cpu_functions xscale_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	xscale_cpwait,			/* cpwait		*/

	/* MMU functions */

	xscale_control,			/* control		*/
	cpufunc_domains,		/* domain		*/
	xscale_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	xscale_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	xscale_cache_syncI,		/* icache_sync_all	*/
	xscale_cache_syncI_rng,		/* icache_sync_range	*/

	xscale_cache_purgeD,		/* dcache_wbinv_all	*/
	xscale_cache_purgeD_rng,	/* dcache_wbinv_range	*/
	xscale_cache_flushD_rng,	/* dcache_inv_range	*/
	xscale_cache_cleanD_rng,	/* dcache_wb_range	*/

	xscale_cache_flushID,		/* idcache_inv_all	*/
	xscale_cache_purgeID,		/* idcache_wbinv_all	*/
	xscale_cache_purgeID_rng,	/* idcache_wbinv_range	*/
	cpufunc_nullop,			/* l2cache_wbinv_all 	*/
	(void *)cpufunc_nullop,		/* l2cache_wbinv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_inv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_wb_range	*/
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	xscale_cpu_sleep,		/* sleep		*/

	/* Soft functions */

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	xscale_context_switch,		/* context_switch	*/

	xscale_setup			/* cpu setup		*/
};
#endif
/* CPU_XSCALE_80321 || CPU_XSCALE_PXA2X0 || CPU_XSCALE_IXP425
   CPU_XSCALE_80219 */

#ifdef CPU_XSCALE_81342
struct cpu_functions xscalec3_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	xscale_cpwait,			/* cpwait		*/

	/* MMU functions */

	xscale_control,			/* control		*/
	cpufunc_domains,		/* domain		*/
	xscalec3_setttb,		/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	xscale_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	(void *)armv4_tlb_flushI,	/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	xscalec3_cache_syncI,		/* icache_sync_all	*/
	xscalec3_cache_syncI_rng,	/* icache_sync_range	*/

	xscalec3_cache_purgeD,		/* dcache_wbinv_all	*/
	xscalec3_cache_purgeD_rng,	/* dcache_wbinv_range	*/
	xscale_cache_flushD_rng,	/* dcache_inv_range	*/
	xscalec3_cache_cleanD_rng,	/* dcache_wb_range	*/

	xscale_cache_flushID,		/* idcache_inv_all	*/
	xscalec3_cache_purgeID,		/* idcache_wbinv_all	*/
	xscalec3_cache_purgeID_rng,	/* idcache_wbinv_range	*/
	xscalec3_l2cache_purge,		/* l2cache_wbinv_all	*/
	xscalec3_l2cache_purge_rng,	/* l2cache_wbinv_range	*/
	xscalec3_l2cache_flush_rng,	/* l2cache_inv_range	*/
	xscalec3_l2cache_clean_rng,	/* l2cache_wb_range	*/
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	cpufunc_nullop,			/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	(void *)cpufunc_nullop,		/* flush_brnchtgt_E	*/

	xscale_cpu_sleep,		/* sleep		*/

	/* Soft functions */

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	xscalec3_context_switch,	/* context_switch	*/

	xscale_setup			/* cpu setup		*/
};
#endif /* CPU_XSCALE_81342 */


#if defined(CPU_FA526)
struct cpu_functions fa526_cpufuncs = {
	/* CPU functions */

	cpufunc_id,			/* id			*/
	cpufunc_nullop,			/* cpwait		*/

	/* MMU functions */

	cpufunc_control,		/* control		*/
	cpufunc_domains,		/* domain		*/
	fa526_setttb,			/* setttb		*/
	cpufunc_faultstatus,		/* faultstatus		*/
	cpufunc_faultaddress,		/* faultaddress		*/

	/* TLB functions */

	armv4_tlb_flushID,		/* tlb_flushID		*/
	fa526_tlb_flushID_SE,		/* tlb_flushID_SE	*/
	armv4_tlb_flushI,		/* tlb_flushI		*/
	fa526_tlb_flushI_SE,		/* tlb_flushI_SE	*/
	armv4_tlb_flushD,		/* tlb_flushD		*/
	armv4_tlb_flushD_SE,		/* tlb_flushD_SE	*/

	/* Cache operations */

	fa526_icache_sync_all,		/* icache_sync_all	*/
	fa526_icache_sync_range,	/* icache_sync_range	*/

	fa526_dcache_wbinv_all,		/* dcache_wbinv_all	*/
	fa526_dcache_wbinv_range,	/* dcache_wbinv_range	*/
	fa526_dcache_inv_range,		/* dcache_inv_range	*/
	fa526_dcache_wb_range,		/* dcache_wb_range	*/

	armv4_idcache_inv_all,		/* idcache_inv_all	*/
	fa526_idcache_wbinv_all,	/* idcache_wbinv_all	*/
	fa526_idcache_wbinv_range,	/* idcache_wbinv_range	*/
	cpufunc_nullop,			/* l2cache_wbinv_all	*/
	(void *)cpufunc_nullop,		/* l2cache_wbinv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_inv_range	*/
	(void *)cpufunc_nullop,		/* l2cache_wb_range	*/
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	fa526_flush_prefetchbuf,	/* flush_prefetchbuf	*/
	armv4_drain_writebuf,		/* drain_writebuf	*/
	cpufunc_nullop,			/* flush_brnchtgt_C	*/
	fa526_flush_brnchtgt_E,		/* flush_brnchtgt_E	*/

	fa526_cpu_sleep,		/* sleep		*/

	/* Soft functions */

	cpufunc_null_fixup,		/* dataabt_fixup	*/
	cpufunc_null_fixup,		/* prefetchabt_fixup	*/

	fa526_context_switch,		/* context_switch	*/

	fa526_setup			/* cpu setup 		*/
};
#endif	/* CPU_FA526 */

#if defined(CPU_ARM1176)
struct cpu_functions arm1176_cpufuncs = {
	/* CPU functions */

	cpufunc_id,                     /* id                   */
	cpufunc_nullop,                 /* cpwait               */

	/* MMU functions */

	cpufunc_control,                /* control              */
	cpufunc_domains,                /* Domain               */
	arm11x6_setttb,                 /* Setttb               */
	cpufunc_faultstatus,            /* Faultstatus          */
	cpufunc_faultaddress,           /* Faultaddress         */

	/* TLB functions */

	arm11_tlb_flushID,              /* tlb_flushID          */
	arm11_tlb_flushID_SE,           /* tlb_flushID_SE       */
	arm11_tlb_flushI,               /* tlb_flushI           */
	arm11_tlb_flushI_SE,            /* tlb_flushI_SE        */
	arm11_tlb_flushD,               /* tlb_flushD           */
	arm11_tlb_flushD_SE,            /* tlb_flushD_SE        */

	/* Cache operations */

	arm11x6_icache_sync_all,        /* icache_sync_all      */
	arm11x6_icache_sync_range,      /* icache_sync_range    */

	arm11x6_dcache_wbinv_all,       /* dcache_wbinv_all     */
	armv6_dcache_wbinv_range,       /* dcache_wbinv_range   */
	armv6_dcache_inv_range,         /* dcache_inv_range     */
	armv6_dcache_wb_range,          /* dcache_wb_range      */

	armv6_idcache_inv_all,		/* idcache_inv_all	*/
	arm11x6_idcache_wbinv_all,      /* idcache_wbinv_all    */
	arm11x6_idcache_wbinv_range,    /* idcache_wbinv_range  */

	(void *)cpufunc_nullop,         /* l2cache_wbinv_all    */
	(void *)cpufunc_nullop,         /* l2cache_wbinv_range  */
	(void *)cpufunc_nullop,         /* l2cache_inv_range    */
	(void *)cpufunc_nullop,         /* l2cache_wb_range     */
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	arm11x6_flush_prefetchbuf,      /* flush_prefetchbuf    */
	arm11_drain_writebuf,           /* drain_writebuf       */
	cpufunc_nullop,                 /* flush_brnchtgt_C     */
	(void *)cpufunc_nullop,         /* flush_brnchtgt_E     */

	arm11x6_sleep,                  /* sleep                */

	/* Soft functions */

	cpufunc_null_fixup,             /* dataabt_fixup        */
	cpufunc_null_fixup,             /* prefetchabt_fixup    */

	arm11_context_switch,           /* context_switch       */

	arm11x6_setup                   /* cpu setup            */
};
#endif /*CPU_ARM1176 */

#if defined(CPU_CORTEXA) || defined(CPU_KRAIT)
struct cpu_functions cortexa_cpufuncs = {
	/* CPU functions */

	cpufunc_id,                     /* id                   */
	cpufunc_nullop,                 /* cpwait               */

	/* MMU functions */

	cpufunc_control,                /* control              */
	cpufunc_domains,                /* Domain               */
	armv7_setttb,                   /* Setttb               */
	cpufunc_faultstatus,            /* Faultstatus          */
	cpufunc_faultaddress,           /* Faultaddress         */

	/*
	 * TLB functions.  ARMv7 does all TLB ops based on a unified TLB model
	 * whether the hardware implements separate I+D or not, so we use the
	 * same 'ID' functions for all 3 variations.
	 */

	armv7_tlb_flushID,              /* tlb_flushID          */
	armv7_tlb_flushID_SE,           /* tlb_flushID_SE       */
	armv7_tlb_flushID,              /* tlb_flushI           */
	armv7_tlb_flushID_SE,           /* tlb_flushI_SE        */
	armv7_tlb_flushID,              /* tlb_flushD           */
	armv7_tlb_flushID_SE,           /* tlb_flushD_SE        */

	/* Cache operations */

	armv7_icache_sync_all, 	        /* icache_sync_all      */
	armv7_icache_sync_range,        /* icache_sync_range    */

	armv7_dcache_wbinv_all,         /* dcache_wbinv_all     */
	armv7_dcache_wbinv_range,       /* dcache_wbinv_range   */
	armv7_dcache_inv_range,         /* dcache_inv_range     */
	armv7_dcache_wb_range,          /* dcache_wb_range      */

	armv7_idcache_inv_all,		/* idcache_inv_all	*/
	armv7_idcache_wbinv_all,        /* idcache_wbinv_all    */
	armv7_idcache_wbinv_range,      /* idcache_wbinv_range  */

	/*
	 * Note: For CPUs using the PL310 the L2 ops are filled in when the
	 * L2 cache controller is actually enabled.
	 */
	cpufunc_nullop,                 /* l2cache_wbinv_all    */
	(void *)cpufunc_nullop,         /* l2cache_wbinv_range  */
	(void *)cpufunc_nullop,         /* l2cache_inv_range    */
	(void *)cpufunc_nullop,         /* l2cache_wb_range     */
	(void *)cpufunc_nullop,         /* l2cache_drain_writebuf */

	/* Other functions */

	cpufunc_nullop,                 /* flush_prefetchbuf    */
	armv7_drain_writebuf,           /* drain_writebuf       */
	cpufunc_nullop,                 /* flush_brnchtgt_C     */
	(void *)cpufunc_nullop,         /* flush_brnchtgt_E     */

	armv7_cpu_sleep,                /* sleep                */

	/* Soft functions */

	cpufunc_null_fixup,             /* dataabt_fixup        */
	cpufunc_null_fixup,             /* prefetchabt_fixup    */

	armv7_context_switch,           /* context_switch       */

	cortexa_setup                     /* cpu setup            */
};
#endif /* CPU_CORTEXA */

/*
 * Global constants also used by locore.s
 */

struct cpu_functions cpufuncs;
u_int cputype;
u_int cpu_reset_needs_v4_MMU_disable;	/* flag used in locore.s */

#if defined(CPU_ARM9) ||	\
  defined (CPU_ARM9E) ||	\
  defined(CPU_ARM1176) || defined(CPU_XSCALE_80321) ||		\
  defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425) ||		\
  defined(CPU_FA526) || defined(CPU_MV_PJ4B) ||			\
  defined(CPU_XSCALE_80219) || defined(CPU_XSCALE_81342) || \
  defined(CPU_CORTEXA) || defined(CPU_KRAIT)

/* Global cache line sizes, use 32 as default */
int	arm_dcache_min_line_size = 32;
int	arm_icache_min_line_size = 32;
int	arm_idcache_min_line_size = 32;

static void get_cachetype_cp15(void);

/* Additional cache information local to this file.  Log2 of some of the
   above numbers.  */
static int	arm_dcache_l2_nsets;
static int	arm_dcache_l2_assoc;
static int	arm_dcache_l2_linesize;

static void
get_cachetype_cp15()
{
	u_int ctype, isize, dsize, cpuid;
	u_int clevel, csize, i, sel;
	u_int multiplier;
	u_char type;

	__asm __volatile("mrc p15, 0, %0, c0, c0, 1"
		: "=r" (ctype));

	cpuid = cpufunc_id();
	/*
	 * ...and thus spake the ARM ARM:
	 *
	 * If an <opcode2> value corresponding to an unimplemented or
	 * reserved ID register is encountered, the System Control
	 * processor returns the value of the main ID register.
	 */
	if (ctype == cpuid)
		goto out;

	if (CPU_CT_FORMAT(ctype) == CPU_CT_ARMV7) {
		/* Resolve minimal cache line sizes */
		arm_dcache_min_line_size = 1 << (CPU_CT_DMINLINE(ctype) + 2);
		arm_icache_min_line_size = 1 << (CPU_CT_IMINLINE(ctype) + 2);
		arm_idcache_min_line_size =
		    min(arm_icache_min_line_size, arm_dcache_min_line_size);

		__asm __volatile("mrc p15, 1, %0, c0, c0, 1"
		    : "=r" (clevel));
		arm_cache_level = clevel;
		arm_cache_loc = CPU_CLIDR_LOC(arm_cache_level);
		i = 0;
		while ((type = (clevel & 0x7)) && i < 7) {
			if (type == CACHE_DCACHE || type == CACHE_UNI_CACHE ||
			    type == CACHE_SEP_CACHE) {
				sel = i << 1;
				__asm __volatile("mcr p15, 2, %0, c0, c0, 0"
				    : : "r" (sel));
				__asm __volatile("mrc p15, 1, %0, c0, c0, 0"
				    : "=r" (csize));
				arm_cache_type[sel] = csize;
				arm_dcache_align = 1 <<
				    (CPUV7_CT_xSIZE_LEN(csize) + 4);
				arm_dcache_align_mask = arm_dcache_align - 1;
			}
			if (type == CACHE_ICACHE || type == CACHE_SEP_CACHE) {
				sel = (i << 1) | 1;
				__asm __volatile("mcr p15, 2, %0, c0, c0, 0"
				    : : "r" (sel));
				__asm __volatile("mrc p15, 1, %0, c0, c0, 0"
				    : "=r" (csize));
				arm_cache_type[sel] = csize;
			}
			i++;
			clevel >>= 3;
		}
	} else {
		if ((ctype & CPU_CT_S) == 0)
			arm_pcache_unified = 1;

		/*
		 * If you want to know how this code works, go read the ARM ARM.
		 */

		arm_pcache_type = CPU_CT_CTYPE(ctype);

		if (arm_pcache_unified == 0) {
			isize = CPU_CT_ISIZE(ctype);
			multiplier = (isize & CPU_CT_xSIZE_M) ? 3 : 2;
			arm_picache_line_size = 1U << (CPU_CT_xSIZE_LEN(isize) + 3);
			if (CPU_CT_xSIZE_ASSOC(isize) == 0) {
				if (isize & CPU_CT_xSIZE_M)
					arm_picache_line_size = 0; /* not present */
				else
					arm_picache_ways = 1;
			} else {
				arm_picache_ways = multiplier <<
				    (CPU_CT_xSIZE_ASSOC(isize) - 1);
			}
			arm_picache_size = multiplier << (CPU_CT_xSIZE_SIZE(isize) + 8);
		}

		dsize = CPU_CT_DSIZE(ctype);
		multiplier = (dsize & CPU_CT_xSIZE_M) ? 3 : 2;
		arm_pdcache_line_size = 1U << (CPU_CT_xSIZE_LEN(dsize) + 3);
		if (CPU_CT_xSIZE_ASSOC(dsize) == 0) {
			if (dsize & CPU_CT_xSIZE_M)
				arm_pdcache_line_size = 0; /* not present */
			else
				arm_pdcache_ways = 1;
		} else {
			arm_pdcache_ways = multiplier <<
			    (CPU_CT_xSIZE_ASSOC(dsize) - 1);
		}
		arm_pdcache_size = multiplier << (CPU_CT_xSIZE_SIZE(dsize) + 8);

		arm_dcache_align = arm_pdcache_line_size;

		arm_dcache_l2_assoc = CPU_CT_xSIZE_ASSOC(dsize) + multiplier - 2;
		arm_dcache_l2_linesize = CPU_CT_xSIZE_LEN(dsize) + 3;
		arm_dcache_l2_nsets = 6 + CPU_CT_xSIZE_SIZE(dsize) -
		    CPU_CT_xSIZE_ASSOC(dsize) - CPU_CT_xSIZE_LEN(dsize);

	out:
		arm_dcache_align_mask = arm_dcache_align - 1;
	}
}
#endif /* ARM9 || XSCALE */

/*
 * Cannot panic here as we may not have a console yet ...
 */

int
set_cpufuncs()
{
	cputype = cpufunc_id();
	cputype &= CPU_ID_CPU_MASK;

#ifdef CPU_ARM9
	if (((cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_ARM_LTD ||
	     (cputype & CPU_ID_IMPLEMENTOR_MASK) == CPU_ID_TI) &&
	    (cputype & 0x0000f000) == 0x00009000) {
		cpufuncs = arm9_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* V4 or higher */
		get_cachetype_cp15();
		arm9_dcache_sets_inc = 1U << arm_dcache_l2_linesize;
		arm9_dcache_sets_max = (1U << (arm_dcache_l2_linesize +
		    arm_dcache_l2_nsets)) - arm9_dcache_sets_inc;
		arm9_dcache_index_inc = 1U << (32 - arm_dcache_l2_assoc);
		arm9_dcache_index_max = 0U - arm9_dcache_index_inc;
		pmap_pte_init_generic();
		goto out;
	}
#endif /* CPU_ARM9 */
#if defined(CPU_ARM9E)
	if (cputype == CPU_ID_MV88FR131 || cputype == CPU_ID_MV88FR571_VD ||
	    cputype == CPU_ID_MV88FR571_41) {
		uint32_t sheeva_ctrl;

		sheeva_ctrl = (MV_DC_STREAM_ENABLE | MV_BTB_DISABLE |
		    MV_L2_ENABLE);
		/*
		 * Workaround for Marvell MV78100 CPU: Cache prefetch
		 * mechanism may affect the cache coherency validity,
		 * so it needs to be disabled.
		 *
		 * Refer to errata document MV-S501058-00C.pdf (p. 3.1
		 * L2 Prefetching Mechanism) for details.
		 */
		if (cputype == CPU_ID_MV88FR571_VD ||
		    cputype == CPU_ID_MV88FR571_41)
			sheeva_ctrl |= MV_L2_PREFETCH_DISABLE;

		sheeva_control_ext(0xffffffff & ~MV_WA_ENABLE, sheeva_ctrl);

		cpufuncs = sheeva_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_generic();
		goto out;
	} else if (cputype == CPU_ID_ARM926EJS) {
		cpufuncs = armv5_ec_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_generic();
		goto out;
	}
#endif /* CPU_ARM9E */
#if defined(CPU_ARM1176)
	if (cputype == CPU_ID_ARM1176JZS) {
		cpufuncs = arm1176_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;     /* V4 or higher */
		get_cachetype_cp15();

		pmap_pte_init_mmu_v6();

		goto out;
	}
#endif /* CPU_ARM1176 */
#if defined(CPU_CORTEXA) || defined(CPU_KRAIT)
	if (cputype == CPU_ID_CORTEXA5 ||
	    cputype == CPU_ID_CORTEXA7 ||
	    cputype == CPU_ID_CORTEXA8R1 ||
	    cputype == CPU_ID_CORTEXA8R2 ||
	    cputype == CPU_ID_CORTEXA8R3 ||
	    cputype == CPU_ID_CORTEXA9R1 ||
	    cputype == CPU_ID_CORTEXA9R2 ||
	    cputype == CPU_ID_CORTEXA9R3 ||
	    cputype == CPU_ID_CORTEXA9R4 ||
	    cputype == CPU_ID_CORTEXA12R0 ||
	    cputype == CPU_ID_CORTEXA15R0 ||
	    cputype == CPU_ID_CORTEXA15R1 ||
	    cputype == CPU_ID_CORTEXA15R2 ||
	    cputype == CPU_ID_CORTEXA15R3 ||
	    cputype == CPU_ID_KRAIT300R0 ||
	    cputype == CPU_ID_KRAIT300R1 ) {
		cpufuncs = cortexa_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;     /* V4 or higher */
		get_cachetype_cp15();

		pmap_pte_init_mmu_v6();
		goto out;
	}
#endif /* CPU_CORTEXA */

#if defined(CPU_MV_PJ4B)
	if (cputype == CPU_ID_MV88SV581X_V7 ||
	    cputype == CPU_ID_MV88SV584X_V7 ||
	    cputype == CPU_ID_ARM_88SV581X_V7) {
		cpufuncs = pj4bv7_cpufuncs;
		get_cachetype_cp15();
		pmap_pte_init_mmu_v6();
		goto out;
	}
#endif /* CPU_MV_PJ4B */

#if defined(CPU_FA526)
	if (cputype == CPU_ID_FA526 || cputype == CPU_ID_FA626TE) {
		cpufuncs = fa526_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* SA needs it	*/
		get_cachetype_cp15();
		pmap_pte_init_generic();

		goto out;
	}
#endif	/* CPU_FA526 */

#if defined(CPU_XSCALE_80321) || defined(CPU_XSCALE_80219)
	if (cputype == CPU_ID_80321_400 || cputype == CPU_ID_80321_600 ||
	    cputype == CPU_ID_80321_400_B0 || cputype == CPU_ID_80321_600_B0 ||
	    cputype == CPU_ID_80219_400 || cputype == CPU_ID_80219_600) {
		cpufuncs = xscale_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();
		goto out;
	}
#endif /* CPU_XSCALE_80321 */

#if defined(CPU_XSCALE_81342)
	if (cputype == CPU_ID_81342) {
		cpufuncs = xscalec3_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();
		goto out;
	}
#endif /* CPU_XSCALE_81342 */
#ifdef CPU_XSCALE_PXA2X0
	/* ignore core revision to test PXA2xx CPUs */
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA250 ||
	    (cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X ||
	    (cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA210) {

		cpufuncs = xscale_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();

		goto out;
	}
#endif /* CPU_XSCALE_PXA2X0 */
#ifdef CPU_XSCALE_IXP425
	if (cputype == CPU_ID_IXP425_533 || cputype == CPU_ID_IXP425_400 ||
            cputype == CPU_ID_IXP425_266 || cputype == CPU_ID_IXP435) {

		cpufuncs = xscale_cpufuncs;
		cpu_reset_needs_v4_MMU_disable = 1;	/* XScale needs it */
		get_cachetype_cp15();
		pmap_pte_init_xscale();

		goto out;
	}
#endif /* CPU_XSCALE_IXP425 */
	/*
	 * Bzzzz. And the answer was ...
	 */
	panic("No support for this CPU type (%08x) in kernel", cputype);
	return(ARCHITECTURE_NOT_PRESENT);
out:
	uma_set_align(arm_dcache_align_mask);
	return (0);
}

/*
 * Fixup routines for data and prefetch aborts.
 *
 * Several compile time symbols are used
 *
 * DEBUG_FAULT_CORRECTION - Print debugging information during the
 * correction of registers after a fault.
 */


/*
 * Null abort fixup routine.
 * For use when no fixup is required.
 */
int
cpufunc_null_fixup(arg)
	void *arg;
{
	return(ABORT_FIXUP_OK);
}

/*
 * CPU Setup code
 */

#ifdef CPU_ARM9
void
arm9_setup(void)
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
	    | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE |
	    CPU_CONTROL_ROUNDROBIN;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_VECRELOC
		 | CPU_CONTROL_ROUNDROBIN;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	cpu_control(cpuctrlmask, cpuctrl);
	ctrl = cpuctrl;

}
#endif	/* CPU_ARM9 */

#if defined(CPU_ARM9E)
void
arm10_setup(void)
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_BPRD_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_SYST_ENABLE
	    | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
	    | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
	    | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
	    | CPU_CONTROL_BPRD_ENABLE
	    | CPU_CONTROL_ROUNDROBIN | CPU_CONTROL_CPCLK;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm __volatile ("mcr\tp15, 0, r0, c7, c7, 0" : : );

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Set the control register */
	ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
}
#endif	/* CPU_ARM9E || CPU_ARM10 */

#if defined(CPU_ARM1176) \
 || defined(CPU_MV_PJ4B) \
 || defined(CPU_CORTEXA) || defined(CPU_KRAIT)
static __inline void
cpu_scc_setup_ccnt(void)
{
/* This is how you give userland access to the CCNT and PMCn
 * registers.
 * BEWARE! This gives write access also, which may not be what
 * you want!
 */
#ifdef _PMC_USER_READ_WRITE_
	/* Set PMUSERENR[0] to allow userland access */
	cp15_pmuserenr_set(1);
#endif
#if defined(CPU_ARM1176)
	/* Set PMCR[2,0] to enable counters and reset CCNT */
	cp15_pmcr_set(5);
#else
	/* Set up the PMCCNTR register as a cyclecounter:
	 * Set PMINTENCLR to 0xFFFFFFFF to block interrupts
	 * Set PMCR[2,0] to enable counters and reset CCNT
	 * Set PMCNTENSET to 0x80000000 to enable CCNT */
	cp15_pminten_clr(0xFFFFFFFF);
	cp15_pmcr_set(5);
	cp15_pmcnten_set(0x80000000);
#endif
}
#endif

#if defined(CPU_ARM1176)
void
arm11x6_setup(void)
{
	int cpuctrl, cpuctrl_wax;
	uint32_t auxctrl, auxctrl_wax;
	uint32_t tmp, tmp2;
	uint32_t sbz=0;
	uint32_t cpuid;

	cpuid = cpufunc_id();

	cpuctrl =
		CPU_CONTROL_MMU_ENABLE  |
		CPU_CONTROL_DC_ENABLE   |
		CPU_CONTROL_WBUF_ENABLE |
		CPU_CONTROL_32BP_ENABLE |
		CPU_CONTROL_32BD_ENABLE |
		CPU_CONTROL_LABT_ENABLE |
		CPU_CONTROL_SYST_ENABLE |
		CPU_CONTROL_IC_ENABLE   |
		CPU_CONTROL_UNAL_ENABLE;

	/*
	 * "write as existing" bits
	 * inverse of this is mask
	 */
	cpuctrl_wax =
		(3 << 30) | /* SBZ */
		(1 << 29) | /* FA */
		(1 << 28) | /* TR */
		(3 << 26) | /* SBZ */
		(3 << 19) | /* SBZ */
		(1 << 17);  /* SBZ */

	cpuctrl |= CPU_CONTROL_BPRD_ENABLE;
	cpuctrl |= CPU_CONTROL_V6_EXTPAGE;

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	auxctrl = 0;
	auxctrl_wax = ~0;

	/*
	 * Enable an errata workaround
	 */
	if ((cpuid & CPU_ID_CPU_MASK) == CPU_ID_ARM1176JZS) { /* ARM1176JZSr0 */
		auxctrl = ARM1176_AUXCTL_PHD;
		auxctrl_wax = ~ARM1176_AUXCTL_PHD;
	}

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Now really make sure they are clean.  */
	__asm volatile ("mcr\tp15, 0, %0, c7, c7, 0" : : "r"(sbz));

	/* Allow detection code to find the VFP if it's fitted.  */
	cp15_cpacr_set(0x0fffffff);

	/* Set the control register */
	ctrl = cpuctrl;
	cpu_control(~cpuctrl_wax, cpuctrl);

	tmp = cp15_actlr_get();
	tmp2 = tmp;
	tmp &= auxctrl_wax;
	tmp |= auxctrl;
	if (tmp != tmp2)
		cp15_actlr_set(tmp);

	/* And again. */
	cpu_idcache_wbinv_all();

	cpu_scc_setup_ccnt();
}
#endif  /* CPU_ARM1176 */

#ifdef CPU_MV_PJ4B
void
pj4bv7_setup(void)
{
	int cpuctrl;

	pj4b_config();

	cpuctrl = CPU_CONTROL_MMU_ENABLE;
#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif
	cpuctrl |= CPU_CONTROL_DC_ENABLE;
	cpuctrl |= (0xf << 3);
	cpuctrl |= CPU_CONTROL_BPRD_ENABLE;
	cpuctrl |= CPU_CONTROL_IC_ENABLE;
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
	cpuctrl |= (0x5 << 16) | (1 < 22);
	cpuctrl |= CPU_CONTROL_V6_EXTPAGE;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	ctrl = cpuctrl;
	cpu_control(0xFFFFFFFF, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();

	cpu_scc_setup_ccnt();
}
#endif /* CPU_MV_PJ4B */

#if defined(CPU_CORTEXA) || defined(CPU_KRAIT)

void
cortexa_setup(void)
{
	int cpuctrl, cpuctrlmask;

	cpuctrlmask = CPU_CONTROL_MMU_ENABLE |     /* MMU enable         [0] */
	    CPU_CONTROL_AFLT_ENABLE |    /* Alignment fault    [1] */
	    CPU_CONTROL_DC_ENABLE |      /* DCache enable      [2] */
	    CPU_CONTROL_BPRD_ENABLE |    /* Branch prediction [11] */
	    CPU_CONTROL_IC_ENABLE |      /* ICache enable     [12] */
	    CPU_CONTROL_VECRELOC;        /* Vector relocation [13] */

	cpuctrl = CPU_CONTROL_MMU_ENABLE |
	    CPU_CONTROL_IC_ENABLE |
	    CPU_CONTROL_DC_ENABLE |
	    CPU_CONTROL_BPRD_ENABLE;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

	/* Switch to big endian */
#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	/* Check if the vector page is at the high address (0xffff0000) */
	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	ctrl = cpuctrl;
	cpu_control(cpuctrlmask, cpuctrl);

	/* And again. */
	cpu_idcache_wbinv_all();
#if defined(SMP) && !defined(ARM_NEW_PMAP)
	armv7_auxctrl((1 << 6) | (1 << 0), (1 << 6) | (1 << 0)); /* Enable SMP + TLB broadcasting  */
#endif

	cpu_scc_setup_ccnt();
}
#endif  /* CPU_CORTEXA */

#if defined(CPU_FA526)
void
fa526_setup(void)
{
	int cpuctrl, cpuctrlmask;

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE
		| CPU_CONTROL_BPRD_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_VECRELOC;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/* Set the control register */
	ctrl = cpuctrl;
	cpu_control(0xffffffff, cpuctrl);
}
#endif	/* CPU_FA526 */

#if defined(CPU_XSCALE_80321) || \
  defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425) || \
  defined(CPU_XSCALE_80219) || defined(CPU_XSCALE_81342)
void
xscale_setup(void)
{
	uint32_t auxctl;
	int cpuctrl, cpuctrlmask;

	/*
	 * The XScale Write Buffer is always enabled.  Our option
	 * is to enable/disable coalescing.  Note that bits 6:3
	 * must always be enabled.
	 */

	cpuctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_LABT_ENABLE
		 | CPU_CONTROL_BPRD_ENABLE;
	cpuctrlmask = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		 | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE
		 | CPU_CONTROL_IC_ENABLE | CPU_CONTROL_DC_ENABLE
		 | CPU_CONTROL_WBUF_ENABLE | CPU_CONTROL_ROM_ENABLE
		 | CPU_CONTROL_BEND_ENABLE | CPU_CONTROL_AFLT_ENABLE
		 | CPU_CONTROL_LABT_ENABLE | CPU_CONTROL_BPRD_ENABLE
		 | CPU_CONTROL_CPCLK | CPU_CONTROL_VECRELOC | \
		 CPU_CONTROL_L2_ENABLE;

#ifndef ARM32_DISABLE_ALIGNMENT_FAULTS
	cpuctrl |= CPU_CONTROL_AFLT_ENABLE;
#endif

#ifdef __ARMEB__
	cpuctrl |= CPU_CONTROL_BEND_ENABLE;
#endif

	if (vector_page == ARM_VECTORS_HIGH)
		cpuctrl |= CPU_CONTROL_VECRELOC;
#ifdef CPU_XSCALE_CORE3
	cpuctrl |= CPU_CONTROL_L2_ENABLE;
#endif

	/* Clear out the cache */
	cpu_idcache_wbinv_all();

	/*
	 * Set the control register.  Note that bits 6:3 must always
	 * be set to 1.
	 */
	ctrl = cpuctrl;
/*	cpu_control(cpuctrlmask, cpuctrl);*/
	cpu_control(0xffffffff, cpuctrl);

	/* Make sure write coalescing is turned on */
	__asm __volatile("mrc p15, 0, %0, c1, c0, 1"
		: "=r" (auxctl));
#ifdef XSCALE_NO_COALESCE_WRITES
	auxctl |= XSCALE_AUXCTL_K;
#else
	auxctl &= ~XSCALE_AUXCTL_K;
#endif
#ifdef CPU_XSCALE_CORE3
	auxctl |= XSCALE_AUXCTL_LLR;
	auxctl |= XSCALE_AUXCTL_MD_MASK;
#endif
	__asm __volatile("mcr p15, 0, %0, c1, c0, 1"
		: : "r" (auxctl));
}
#endif	/* CPU_XSCALE_80321 || CPU_XSCALE_PXA2X0 || CPU_XSCALE_IXP425
	   CPU_XSCALE_80219 */
