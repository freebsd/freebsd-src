/*-
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
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
 * $FreeBSD$
 */

#include "opt_user_ldt.h"

#include <machine/asmacros.h>
#include <machine/pmap.h>

#include "assym.s"

#ifdef SMP
	/*
	 * Define layout of per-cpu address space.
	 * This is "constructed" in locore.s on the BSP and in mp_machdep.c
	 * for each AP.  DO NOT REORDER THESE WITHOUT UPDATING THE REST!
	 */
	.globl	_SMP_prvspace, _lapic
	.set	_SMP_prvspace,(MPPTDI << PDRSHIFT)
	.set	_lapic,_SMP_prvspace + (NPTEPG-1) * PAGE_SIZE

	.globl  gd_idlestack,gd_idlestack_top
	.set    gd_idlestack,PS_IDLESTACK
	.set    gd_idlestack_top,PS_IDLESTACK_TOP
#endif

	/*
	 * Define layout of the global data.  On SMP this lives in
	 * the per-cpu address space, otherwise it's in the data segment.
	 */
	.globl	globaldata
#ifndef SMP
	.data
	ALIGN_DATA
globaldata:
	.space	GD_SIZEOF		/* in data segment */
#else
	.set	globaldata,0
#endif
	.globl	gd_curproc, gd_prevproc, gd_curpcb, gd_npxproc, gd_idleproc
	.globl	gd_astpending, gd_common_tss, gd_switchtime, gd_switchticks
	.globl	gd_intr_nesting_level
	.set	gd_curproc,globaldata + GD_CURPROC
	.set	gd_prevproc,globaldata + GD_PREVPROC
	.set	gd_astpending,globaldata + GD_ASTPENDING
	.set	gd_curpcb,globaldata + GD_CURPCB
	.set	gd_npxproc,globaldata + GD_NPXPROC
	.set	gd_idleproc,globaldata + GD_IDLEPROC
	.set	gd_common_tss,globaldata + GD_COMMON_TSS
	.set	gd_switchtime,globaldata + GD_SWITCHTIME
	.set	gd_switchticks,globaldata + GD_SWITCHTICKS
	.set	gd_intr_nesting_level,globaldata + GD_INTR_NESTING_LEVEL

	.globl	gd_common_tssd, gd_tss_gdt
	.set	gd_common_tssd,globaldata + GD_COMMON_TSSD
	.set	gd_tss_gdt,globaldata + GD_TSS_GDT

	.globl	gd_witness_spin_check
	.set	gd_witness_spin_check, globaldata + GD_WITNESS_SPIN_CHECK

#ifdef USER_LDT
	.globl	gd_currentldt
	.set	gd_currentldt,globaldata + GD_CURRENTLDT
#endif

/* XXX - doesn't work yet */
#ifdef KTR_PERCPU
	.globl	gd_ktr_idx, gd_ktr_buf, gd_ktr_buf_data
	.set	gd_ktr_idx,globaldata + GD_KTR_IDX
	.set	gd_ktr_buf,globaldata + GD_KTR_BUF
	.set	gd_ktr_buf_data,globaldata + GD_KTR_BUF_DATA
#endif

#ifndef SMP
	.globl	_curproc, _prevproc, _curpcb, _npxproc, _idleproc,
	.globl	_astpending, _common_tss, _switchtime, _switchticks
	.global	_intr_nesting_level
	.set	_curproc,globaldata + GD_CURPROC
	.set	_prevproc,globaldata + GD_PREVPROC
	.set	_astpending,globaldata + GD_ASTPENDING
	.set	_curpcb,globaldata + GD_CURPCB
	.set	_npxproc,globaldata + GD_NPXPROC
	.set	_idleproc,globaldata + GD_IDLEPROC
	.set	_common_tss,globaldata + GD_COMMON_TSS
	.set	_switchtime,globaldata + GD_SWITCHTIME
	.set	_switchticks,globaldata + GD_SWITCHTICKS
	.set	_intr_nesting_level,globaldata + GD_INTR_NESTING_LEVEL

	.globl	_common_tssd, _tss_gdt
	.set	_common_tssd,globaldata + GD_COMMON_TSSD
	.set	_tss_gdt,globaldata + GD_TSS_GDT

	.globl	_witness_spin_check
	.set	_witness_spin_check,globaldata + GD_WITNESS_SPIN_CHECK

#ifdef USER_LDT
	.globl	_currentldt
	.set	_currentldt,globaldata + GD_CURRENTLDT
#endif

/* XXX - doesn't work yet */
#ifdef KTR_PERCPU
	.globl	_ktr_idx, _ktr_buf, _ktr_buf_data
	.set	_ktr_idx,globaldata + GD_KTR_IDX
	.set	_ktr_buf,globaldata + GD_KTR_BUF
	.set	_ktr_buf_data,globaldata + GD_KTR_BUF_DATA
#endif
#endif

#ifdef SMP
	/*
	 * The BSP version of these get setup in locore.s and pmap.c, while
	 * the AP versions are setup in mp_machdep.c.
	 */
	.globl  gd_cpuid, gd_cpu_lockid, gd_other_cpus
	.globl	gd_ss_eflags, gd_inside_intr
	.globl  gd_prv_CMAP1, gd_prv_CMAP2, gd_prv_CMAP3, gd_prv_PMAP1
	.globl  gd_prv_CADDR1, gd_prv_CADDR2, gd_prv_CADDR3, gd_prv_PADDR1

	.set    gd_cpuid,globaldata + GD_CPUID
	.set    gd_cpu_lockid,globaldata + GD_CPU_LOCKID
	.set    gd_other_cpus,globaldata + GD_OTHER_CPUS
	.set    gd_ss_eflags,globaldata + GD_SS_EFLAGS
	.set    gd_inside_intr,globaldata + GD_INSIDE_INTR
	.set    gd_prv_CMAP1,globaldata + GD_PRV_CMAP1
	.set    gd_prv_CMAP2,globaldata + GD_PRV_CMAP2
	.set    gd_prv_CMAP3,globaldata + GD_PRV_CMAP3
	.set    gd_prv_PMAP1,globaldata + GD_PRV_PMAP1
	.set    gd_prv_CADDR1,globaldata + GD_PRV_CADDR1
	.set    gd_prv_CADDR2,globaldata + GD_PRV_CADDR2
	.set    gd_prv_CADDR3,globaldata + GD_PRV_CADDR3
	.set    gd_prv_PADDR1,globaldata + GD_PRV_PADDR1
#endif

#if defined(SMP) || defined(APIC_IO)
	.globl	lapic_eoi, lapic_svr, lapic_tpr, lapic_irr1, lapic_ver
	.globl	lapic_icr_lo,lapic_icr_hi,lapic_isr1
/*
 * Do not clutter our namespace with these unless we need them in other
 * assembler code.  The C code uses different definitions.
 */
#if 0
	.globl	lapic_id,lapic_ver,lapic_tpr,lapic_apr,lapic_ppr,lapic_eoi
	.globl	lapic_ldr,lapic_dfr,lapic_svr,lapic_isr,lapic_isr0
	.globl	lapic_isr2,lapic_isr3,lapic_isr4,lapic_isr5,lapic_isr6
	.globl	lapic_isr7,lapic_tmr,lapic_tmr0,lapic_tmr1,lapic_tmr2
	.globl	lapic_tmr3,lapic_tmr4,lapic_tmr5,lapic_tmr6,lapic_tmr7
	.globl	lapic_irr,lapic_irr0,lapic_irr1,lapic_irr2,lapic_irr3
	.globl	lapic_irr4,lapic_irr5,lapic_irr6,lapic_irr7,lapic_esr
	.globl	lapic_lvtt,lapic_pcint,lapic_lvt1
	.globl	lapic_lvt2,lapic_lvt3,lapic_ticr,lapic_tccr,lapic_tdcr
#endif
	.set	lapic_id,	_lapic + 0x020
	.set	lapic_ver,	_lapic + 0x030
	.set	lapic_tpr,	_lapic + 0x080
	.set	lapic_apr,	_lapic + 0x090
	.set	lapic_ppr,	_lapic + 0x0a0
	.set	lapic_eoi,	_lapic + 0x0b0
	.set	lapic_ldr,	_lapic + 0x0d0
	.set	lapic_dfr,	_lapic + 0x0e0
	.set	lapic_svr,	_lapic + 0x0f0
	.set	lapic_isr,	_lapic + 0x100
	.set	lapic_isr0,	_lapic + 0x100
	.set	lapic_isr1,	_lapic + 0x110
	.set	lapic_isr2,	_lapic + 0x120
	.set	lapic_isr3,	_lapic + 0x130
	.set	lapic_isr4,	_lapic + 0x140
	.set	lapic_isr5,	_lapic + 0x150
	.set	lapic_isr6,	_lapic + 0x160
	.set	lapic_isr7,	_lapic + 0x170
	.set	lapic_tmr,	_lapic + 0x180
	.set	lapic_tmr0,	_lapic + 0x180
	.set	lapic_tmr1,	_lapic + 0x190
	.set	lapic_tmr2,	_lapic + 0x1a0
	.set	lapic_tmr3,	_lapic + 0x1b0
	.set	lapic_tmr4,	_lapic + 0x1c0
	.set	lapic_tmr5,	_lapic + 0x1d0
	.set	lapic_tmr6,	_lapic + 0x1e0
	.set	lapic_tmr7,	_lapic + 0x1f0
	.set	lapic_irr,	_lapic + 0x200
	.set	lapic_irr0,	_lapic + 0x200
	.set	lapic_irr1,	_lapic + 0x210
	.set	lapic_irr2,	_lapic + 0x220
	.set	lapic_irr3,	_lapic + 0x230
	.set	lapic_irr4,	_lapic + 0x240
	.set	lapic_irr5,	_lapic + 0x250
	.set	lapic_irr6,	_lapic + 0x260
	.set	lapic_irr7,	_lapic + 0x270
	.set	lapic_esr,	_lapic + 0x280
	.set	lapic_icr_lo,	_lapic + 0x300
	.set	lapic_icr_hi,	_lapic + 0x310
	.set	lapic_lvtt,	_lapic + 0x320
	.set	lapic_pcint,	_lapic + 0x340
	.set	lapic_lvt1,	_lapic + 0x350
	.set	lapic_lvt2,	_lapic + 0x360
	.set	lapic_lvt3,	_lapic + 0x370
	.set	lapic_ticr,	_lapic + 0x380
	.set	lapic_tccr,	_lapic + 0x390
	.set	lapic_tdcr,	_lapic + 0x3e0
#endif
