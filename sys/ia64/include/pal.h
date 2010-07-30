/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 *	$FreeBSD$
 */

#ifndef _MACHINE_PAL_H_
#define _MACHINE_PAL_H_

/*
 * Architected static calling convention procedures.
 */
#define PAL_CACHE_FLUSH		1
#define PAL_CACHE_INFO		2
#define PAL_CACHE_INIT		3
#define PAL_CACHE_SUMMARY	4
#define PAL_MEM_ATTRIB		5
#define PAL_PTCE_INFO		6
#define PAL_VM_INFO		7
#define PAL_VM_SUMMARY		8
#define PAL_BUS_GET_FEATURES	9
#define PAL_BUS_SET_FEATURES	10
#define PAL_DEBUG_INFO		11
#define PAL_FIXED_ADDR		12
#define PAL_FREQ_BASE		13
#define PAL_FREQ_RATIOS		14
#define PAL_PERF_MON_INFO	15
#define PAL_PLATFORM_ADDR	16
#define PAL_PROC_GET_FEATURE	17
#define PAL_PROC_SET_FEATURE	18
#define PAL_RSE_INFO		19
#define PAL_VERSION		20
#define PAL_MC_CLEAR_LOG	21
#define PAL_MC_DRAIN		22
#define	PAL_MC_EXPECTED		23
#define PAL_MC_DYNAMIC_STATE	24
#define PAL_MC_ERROR_INFO	25
#define	PAL_MC_RESUME		26
#define PAL_MC_REGISTER_MEM	27
#define PAL_HALT		28
#define PAL_HALT_LIGHT		29
#define PAL_COPY_INFO		30
#define PAL_CACHE_LINE_INIT	31
#define PAL_PMI_ENTRYPOINT	32
#define PAL_ENTER_IA_32_ENV	33
#define PAL_VM_PAGE_SIZE	34
#define	PAL_TEST_INFO		37
#define PAL_CACHE_PROT_INFO	38
#define PAL_REGISTER_INFO	39
#define PAL_PREFETCH_VISIBILITY	41
#define	PAL_LOGICAL_TO_PHYSICAL	42
#define	PAL_CACHE_SHARED_INFO	43
#define	PAL_PSTATE_INFO		44
#define	PAL_SHUTDOWN		45
#define	PAL_GET_HW_POLICY	48
#define	PAL_SET_HW_POLICY	49

/*
 * Architected stacked calling convention procedures.
 */
#define PAL_COPY_PAL		256
#define PAL_HALT_INFO		257
#define PAL_TEST_PROC		258
#define PAL_CACHE_READ		259
#define PAL_CACHE_WRITE		260
#define PAL_VM_TR_READ		261
#define	PAL_GET_PSTATE		262
#define	PAL_SET_PSTATE		263
#define	PAL_VP_CREATE		265
#define	PAL_VP_ENV_INFO		266
#define	PAL_VP_EXIT_ENV		267
#define	PAL_VP_INIT_ENV		268
#define	PAL_VP_REGISTER		269
#define	PAL_VP_RESTORE		270
#define	PAL_VP_SAVE		271
#define	PAL_VP_TERMINATE	272
#define	PAL_BRAND_INFO		274
#define	PAL_MC_ERROR_INJECT	276
#define	PAL_MEMORY_BUFFER	277

/*
 * Default physical address of the Processor Interrupt Block (PIB).
 * See also: IA-64 SDM, rev 1.1, volume 2, page 5-31.
 */
#define	PAL_PIB_DEFAULT_ADDR	0x00000000FEE00000L

struct ia64_pal_result {
	int64_t		pal_status;
	uint64_t	pal_result[3];
};

struct ia64_pal_result ia64_call_pal_static(uint64_t proc, uint64_t arg1,
    uint64_t arg2, uint64_t arg3);

struct ia64_pal_result ia64_call_pal_static_physical(uint64_t proc,
    uint64_t arg1, uint64_t arg2, uint64_t arg3);

struct ia64_pal_result ia64_call_pal_stacked(uint64_t proc, uint64_t arg1,
    uint64_t arg2, uint64_t arg3);

struct ia64_pal_result ia64_call_pal_stacked_physical(uint64_t proc,
    uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif /* _MACHINE_PAL_H_ */
