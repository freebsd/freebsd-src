/*
 * Copyright (c) KATO Takenori, 1997, 1998.
 * 
 * All rights reserved.  Unpublished rights reserved under the copyright
 * laws of Japan.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
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
 *
 * $FreeBSD$
 */

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

void initializecpu(void);
#if defined(I586_CPU) && defined(CPU_WT_ALLOC)
void	enable_K5_wt_alloc(void);
void	enable_K6_wt_alloc(void);
void	enable_K6_2_wt_alloc(void);
#endif

#ifdef I486_CPU
static void init_5x86(void);
static void init_bluelightning(void);
static void init_486dlc(void);
static void init_cy486dx(void);
#ifdef CPU_I486_ON_386
static void init_i486_on_386(void);
#endif
static void init_6x86(void);
#endif /* I486_CPU */

#ifdef I686_CPU
static void	init_6x86MX(void);
static void	init_ppro(void);
static void	init_mendocino(void);
#endif
void	enable_sse(void);

int	hw_instruction_sse = 0;
SYSCTL_INT(_hw, OID_AUTO, instruction_sse, CTLFLAG_RD,
	   &hw_instruction_sse, 0,
	   "SIMD/MMX2 instructions available in CPU");

/* Must *NOT* be BSS or locore will bzero these after setting them */
int	cpu = 0;		/* Are we 386, 386sx, 486, etc? */
u_int	cpu_id = 0;		/* Stepping ID */
u_int	cpu_feature = 0;	/* Feature flags */
u_int	cpu_high = 0;		/* Highest arg to CPUID */
#ifdef CPU_ENABLE_SSE
u_int	cpu_fxsr = 0;		/* SSE enabled */
#endif
char	cpu_vendor[20] = "";	/* CPU Origin code */

#ifdef I486_CPU
/*
 * IBM Blue Lightning
 */
static void
init_bluelightning(void)
{
	u_long	eflags;

#if defined(PC98) && !defined(CPU_UPGRADE_HW_CACHE)
	need_post_dma_flush = 1;
#endif

	eflags = read_eflags();
	disable_intr();

	load_cr0(rcr0() | CR0_CD | CR0_NW);
	invd();

#ifdef CPU_BLUELIGHTNING_FPU_OP_CACHE
	wrmsr(0x1000, 0x9c92LL);	/* FP operand can be cacheable on Cyrix FPU */
#else
	wrmsr(0x1000, 0x1c92LL);	/* Intel FPU */
#endif
	/* Enables 13MB and 0-640KB cache. */
	wrmsr(0x1001, (0xd0LL << 32) | 0x3ff);
#ifdef CPU_BLUELIGHTNING_3X
	wrmsr(0x1002, 0x04000000LL);	/* Enables triple-clock mode. */
#else
	wrmsr(0x1002, 0x03000000LL);	/* Enables double-clock mode. */
#endif

	/* Enable caching in CR0. */
	load_cr0(rcr0() & ~(CR0_CD | CR0_NW));	/* CD = 0 and NW = 0 */
	invd();
	write_eflags(eflags);
}

/*
 * Cyrix 486SLC/DLC/SR/DR series
 */
static void
init_486dlc(void)
{
	u_long	eflags;
	u_char	ccr0;

	eflags = read_eflags();
	disable_intr();
	invd();

	ccr0 = read_cyrix_reg(CCR0);
#ifndef CYRIX_CACHE_WORKS
	ccr0 |= CCR0_NC1 | CCR0_BARB;
	write_cyrix_reg(CCR0, ccr0);
	invd();
#else
	ccr0 &= ~CCR0_NC0;
#ifndef CYRIX_CACHE_REALLY_WORKS
	ccr0 |= CCR0_NC1 | CCR0_BARB;
#else
	ccr0 |= CCR0_NC1;
#endif
#ifdef CPU_DIRECT_MAPPED_CACHE
	ccr0 |= CCR0_CO;			/* Direct mapped mode. */
#endif
	write_cyrix_reg(CCR0, ccr0);

	/* Clear non-cacheable region. */
	write_cyrix_reg(NCR1+2, NCR_SIZE_0K);
	write_cyrix_reg(NCR2+2, NCR_SIZE_0K);
	write_cyrix_reg(NCR3+2, NCR_SIZE_0K);
	write_cyrix_reg(NCR4+2, NCR_SIZE_0K);

	write_cyrix_reg(0, 0);	/* dummy write */

	/* Enable caching in CR0. */
	load_cr0(rcr0() & ~(CR0_CD | CR0_NW));	/* CD = 0 and NW = 0 */
	invd();
#endif /* !CYRIX_CACHE_WORKS */
	write_eflags(eflags);
}


/*
 * Cyrix 486S/DX series
 */
static void
init_cy486dx(void)
{
	u_long	eflags;
	u_char	ccr2;

	eflags = read_eflags();
	disable_intr();
	invd();

	ccr2 = read_cyrix_reg(CCR2);
#ifdef CPU_SUSP_HLT
	ccr2 |= CCR2_SUSP_HLT;
#endif

#ifdef PC98
	/* Enables WB cache interface pin and Lock NW bit in CR0. */
	ccr2 |= CCR2_WB | CCR2_LOCK_NW;
	/* Unlock NW bit in CR0. */
	write_cyrix_reg(CCR2, ccr2 & ~CCR2_LOCK_NW);
	load_cr0((rcr0() & ~CR0_CD) | CR0_NW);	/* CD = 0, NW = 1 */
#endif

	write_cyrix_reg(CCR2, ccr2);
	write_eflags(eflags);
}


/*
 * Cyrix 5x86
 */
static void
init_5x86(void)
{
	u_long	eflags;
	u_char	ccr2, ccr3, ccr4, pcr0;

	eflags = read_eflags();
	disable_intr();

	load_cr0(rcr0() | CR0_CD | CR0_NW);
	wbinvd();

	(void)read_cyrix_reg(CCR3);		/* dummy */

	/* Initialize CCR2. */
	ccr2 = read_cyrix_reg(CCR2);
	ccr2 |= CCR2_WB;
#ifdef CPU_SUSP_HLT
	ccr2 |= CCR2_SUSP_HLT;
#else
	ccr2 &= ~CCR2_SUSP_HLT;
#endif
	ccr2 |= CCR2_WT1;
	write_cyrix_reg(CCR2, ccr2);

	/* Initialize CCR4. */
	ccr3 = read_cyrix_reg(CCR3);
	write_cyrix_reg(CCR3, CCR3_MAPEN0);

	ccr4 = read_cyrix_reg(CCR4);
	ccr4 |= CCR4_DTE;
	ccr4 |= CCR4_MEM;
#ifdef CPU_FASTER_5X86_FPU
	ccr4 |= CCR4_FASTFPE;
#else
	ccr4 &= ~CCR4_FASTFPE;
#endif
	ccr4 &= ~CCR4_IOMASK;
	/********************************************************************
	 * WARNING: The "BIOS Writers Guide" mentions that I/O recovery time
	 * should be 0 for errata fix.
	 ********************************************************************/
#ifdef CPU_IORT
	ccr4 |= CPU_IORT & CCR4_IOMASK;
#endif
	write_cyrix_reg(CCR4, ccr4);

	/* Initialize PCR0. */
	/****************************************************************
	 * WARNING: RSTK_EN and LOOP_EN could make your system unstable.
	 * BTB_EN might make your system unstable.
	 ****************************************************************/
	pcr0 = read_cyrix_reg(PCR0);
#ifdef CPU_RSTK_EN
	pcr0 |= PCR0_RSTK;
#else
	pcr0 &= ~PCR0_RSTK;
#endif
#ifdef CPU_BTB_EN
	pcr0 |= PCR0_BTB;
#else
	pcr0 &= ~PCR0_BTB;
#endif
#ifdef CPU_LOOP_EN
	pcr0 |= PCR0_LOOP;
#else
	pcr0 &= ~PCR0_LOOP;
#endif

	/****************************************************************
	 * WARNING: if you use a memory mapped I/O device, don't use
	 * DISABLE_5X86_LSSER option, which may reorder memory mapped
	 * I/O access.
	 * IF YOUR MOTHERBOARD HAS PCI BUS, DON'T DISABLE LSSER.
	 ****************************************************************/
#ifdef CPU_DISABLE_5X86_LSSER
	pcr0 &= ~PCR0_LSSER;
#else
	pcr0 |= PCR0_LSSER;
#endif
	write_cyrix_reg(PCR0, pcr0);

	/* Restore CCR3. */
	write_cyrix_reg(CCR3, ccr3);

	(void)read_cyrix_reg(0x80);		/* dummy */

	/* Unlock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) & ~CCR2_LOCK_NW);
	load_cr0((rcr0() & ~CR0_CD) | CR0_NW);	/* CD = 0, NW = 1 */
	/* Lock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) | CCR2_LOCK_NW);

	write_eflags(eflags);
}

#ifdef CPU_I486_ON_386
/*
 * There are i486 based upgrade products for i386 machines.
 * In this case, BIOS doesn't enables CPU cache.
 */
void
init_i486_on_386(void)
{
	u_long	eflags;

#if defined(PC98) && !defined(CPU_UPGRADE_HW_CACHE)
	need_post_dma_flush = 1;
#endif

	eflags = read_eflags();
	disable_intr();

	load_cr0(rcr0() & ~(CR0_CD | CR0_NW));	/* CD = 0, NW = 0 */

	write_eflags(eflags);
}
#endif

/*
 * Cyrix 6x86
 *
 * XXX - What should I do here?  Please let me know.
 */
static void
init_6x86(void)
{
	u_long	eflags;
	u_char	ccr3, ccr4;

	eflags = read_eflags();
	disable_intr();

	load_cr0(rcr0() | CR0_CD | CR0_NW);
	wbinvd();

	/* Initialize CCR0. */
	write_cyrix_reg(CCR0, read_cyrix_reg(CCR0) | CCR0_NC1);

	/* Initialize CCR1. */
#ifdef CPU_CYRIX_NO_LOCK
	write_cyrix_reg(CCR1, read_cyrix_reg(CCR1) | CCR1_NO_LOCK);
#else
	write_cyrix_reg(CCR1, read_cyrix_reg(CCR1) & ~CCR1_NO_LOCK);
#endif

	/* Initialize CCR2. */
#ifdef CPU_SUSP_HLT
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) | CCR2_SUSP_HLT);
#else
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) & ~CCR2_SUSP_HLT);
#endif

	ccr3 = read_cyrix_reg(CCR3);
	write_cyrix_reg(CCR3, CCR3_MAPEN0);

	/* Initialize CCR4. */
	ccr4 = read_cyrix_reg(CCR4);
	ccr4 |= CCR4_DTE;
	ccr4 &= ~CCR4_IOMASK;
#ifdef CPU_IORT
	write_cyrix_reg(CCR4, ccr4 | (CPU_IORT & CCR4_IOMASK));
#else
	write_cyrix_reg(CCR4, ccr4 | 7);
#endif

	/* Initialize CCR5. */
#ifdef CPU_WT_ALLOC
	write_cyrix_reg(CCR5, read_cyrix_reg(CCR5) | CCR5_WT_ALLOC);
#endif

	/* Restore CCR3. */
	write_cyrix_reg(CCR3, ccr3);

	/* Unlock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) & ~CCR2_LOCK_NW);

	/*
	 * Earlier revision of the 6x86 CPU could crash the system if
	 * L1 cache is in write-back mode.
	 */
	if ((cyrix_did & 0xff00) > 0x1600)
		load_cr0(rcr0() & ~(CR0_CD | CR0_NW));	/* CD = 0 and NW = 0 */
	else {
		/* Revision 2.6 and lower. */
#ifdef CYRIX_CACHE_REALLY_WORKS
		load_cr0(rcr0() & ~(CR0_CD | CR0_NW));	/* CD = 0 and NW = 0 */
#else
		load_cr0((rcr0() & ~CR0_CD) | CR0_NW);	/* CD = 0 and NW = 1 */
#endif
	}

	/* Lock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) | CCR2_LOCK_NW);

	write_eflags(eflags);
}
#endif /* I486_CPU */

#ifdef I686_CPU
/*
 * Cyrix 6x86MX (code-named M2)
 *
 * XXX - What should I do here?  Please let me know.
 */
static void
init_6x86MX(void)
{
	u_long	eflags;
	u_char	ccr3, ccr4;

	eflags = read_eflags();
	disable_intr();

	load_cr0(rcr0() | CR0_CD | CR0_NW);
	wbinvd();

	/* Initialize CCR0. */
	write_cyrix_reg(CCR0, read_cyrix_reg(CCR0) | CCR0_NC1);

	/* Initialize CCR1. */
#ifdef CPU_CYRIX_NO_LOCK
	write_cyrix_reg(CCR1, read_cyrix_reg(CCR1) | CCR1_NO_LOCK);
#else
	write_cyrix_reg(CCR1, read_cyrix_reg(CCR1) & ~CCR1_NO_LOCK);
#endif

	/* Initialize CCR2. */
#ifdef CPU_SUSP_HLT
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) | CCR2_SUSP_HLT);
#else
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) & ~CCR2_SUSP_HLT);
#endif

	ccr3 = read_cyrix_reg(CCR3);
	write_cyrix_reg(CCR3, CCR3_MAPEN0);

	/* Initialize CCR4. */
	ccr4 = read_cyrix_reg(CCR4);
	ccr4 &= ~CCR4_IOMASK;
#ifdef CPU_IORT
	write_cyrix_reg(CCR4, ccr4 | (CPU_IORT & CCR4_IOMASK));
#else
	write_cyrix_reg(CCR4, ccr4 | 7);
#endif

	/* Initialize CCR5. */
#ifdef CPU_WT_ALLOC
	write_cyrix_reg(CCR5, read_cyrix_reg(CCR5) | CCR5_WT_ALLOC);
#endif

	/* Restore CCR3. */
	write_cyrix_reg(CCR3, ccr3);

	/* Unlock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) & ~CCR2_LOCK_NW);

	load_cr0(rcr0() & ~(CR0_CD | CR0_NW));	/* CD = 0 and NW = 0 */

	/* Lock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) | CCR2_LOCK_NW);

	write_eflags(eflags);
}

static void
init_ppro(void)
{
#ifndef SMP
	u_int64_t	apicbase;

	/*
	 * Local APIC should be diabled in UP kernel.
	 */
	apicbase = rdmsr(0x1b);
	apicbase &= ~0x800LL;
	wrmsr(0x1b, apicbase);
#endif
}

/*
 * Initialize BBL_CR_CTL3 (Control register 3: used to configure the
 * L2 cache).
 */
void
init_mendocino(void)
{
#ifdef CPU_PPRO2CELERON
	u_long	eflags;
	u_int64_t	bbl_cr_ctl3;

	eflags = read_eflags();
	disable_intr();

	load_cr0(rcr0() | CR0_CD | CR0_NW);
	wbinvd();

	bbl_cr_ctl3 = rdmsr(0x11e);

	/* If the L2 cache is configured, do nothing. */
	if (!(bbl_cr_ctl3 & 1)) {
		bbl_cr_ctl3 = 0x134052bLL;

		/* Set L2 Cache Latency (Default: 5). */
#ifdef	CPU_CELERON_L2_LATENCY
#if CPU_L2_LATENCY > 15
#error invalid CPU_L2_LATENCY.
#endif
		bbl_cr_ctl3 |= CPU_L2_LATENCY << 1;
#else
		bbl_cr_ctl3 |= 5 << 1;
#endif
		wrmsr(0x11e, bbl_cr_ctl3);
	}

	load_cr0(rcr0() & ~(CR0_CD | CR0_NW));
	write_eflags(eflags);
#endif /* CPU_PPRO2CELERON */
}

#endif /* I686_CPU */

/*
 * Initialize CR4 (Control register 4) to enable SSE instructions.
 */
void
enable_sse(void)
{
#if defined(CPU_ENABLE_SSE)
	if ((cpu_feature & CPUID_XMM) && (cpu_feature & CPUID_FXSR)) {
		load_cr4(rcr4() | CR4_FXSR | CR4_XMM);
		cpu_fxsr = hw_instruction_sse = 1;
	}
#endif
}

void
initializecpu(void)
{

	switch (cpu) {
#ifdef I486_CPU
	case CPU_BLUE:
		init_bluelightning();
		break;
	case CPU_486DLC:
		init_486dlc();
		break;
	case CPU_CY486DX:
		init_cy486dx();
		break;
	case CPU_M1SC:
		init_5x86();
		break;
#ifdef CPU_I486_ON_386
	case CPU_486:
		init_i486_on_386();
		break;
#endif
	case CPU_M1:
		init_6x86();
		break;
#endif /* I486_CPU */
#ifdef I686_CPU
	case CPU_M2:
		init_6x86MX();
		break;
	case CPU_686:
		if (strcmp(cpu_vendor, "GenuineIntel") == 0) {
			switch (cpu_id & 0xff0) {
			case 0x610:
				init_ppro();
				break;
			case 0x660:
				init_mendocino();
				break;
			}
		} else if (strcmp(cpu_vendor, "AuthenticAMD") == 0) {
#if defined(I686_CPU) && defined(CPU_ATHLON_SSE_HACK)
			/*
			 * Sometimes the BIOS doesn't enable SSE instructions.
			 * According to AMD document 20734, the mobile
			 * Duron, the (mobile) Athlon 4 and the Athlon MP
			 * support SSE. These correspond to cpu_id 0x66X
			 * or 0x67X.
			 */
			if ((cpu_feature & CPUID_XMM) == 0 &&
			    ((cpu_id & ~0xf) == 0x660 ||
			     (cpu_id & ~0xf) == 0x670)) {
				u_int regs[4];
				wrmsr(0xC0010015, rdmsr(0xC0010015) & ~0x08000);
				do_cpuid(1, regs);
				cpu_feature = regs[3];
			}
#endif
		}
		break;
#endif
	default:
		break;
	}
	enable_sse();

#if defined(PC98) && !defined(CPU_UPGRADE_HW_CACHE)
	/*
	 * OS should flush L1 cache by itself because no PC-98 supports
	 * non-Intel CPUs.  Use wbinvd instruction before DMA transfer
	 * when need_pre_dma_flush = 1, use invd instruction after DMA
	 * transfer when need_post_dma_flush = 1.  If your CPU upgrade
	 * product supports hardware cache control, you can add the
	 * CPU_UPGRADE_HW_CACHE option in your kernel configuration file.
	 * This option eliminates unneeded cache flush instruction(s).
	 */
	if (strcmp(cpu_vendor, "CyrixInstead") == 0) {
		switch (cpu) {
#ifdef I486_CPU
		case CPU_486DLC:
			need_post_dma_flush = 1;
			break;
		case CPU_M1SC:
			need_pre_dma_flush = 1;
			break;
		case CPU_CY486DX:
			need_pre_dma_flush = 1;
#ifdef CPU_I486_ON_386
			need_post_dma_flush = 1;
#endif
			break;
#endif
		default:
			break;
		}
	} else if (strcmp(cpu_vendor, "AuthenticAMD") == 0) {
		switch (cpu_id & 0xFF0) {
		case 0x470:		/* Enhanced Am486DX2 WB */
		case 0x490:		/* Enhanced Am486DX4 WB */
		case 0x4F0:		/* Am5x86 WB */
			need_pre_dma_flush = 1;
			break;
		}
	} else if (strcmp(cpu_vendor, "IBM") == 0) {
		need_post_dma_flush = 1;
	} else {
#ifdef CPU_I486_ON_386
		need_pre_dma_flush = 1;
#endif
	}
#endif /* PC98 && !CPU_UPGRADE_HW_CACHE */
}

#if defined(I586_CPU) && defined(CPU_WT_ALLOC)
/*
 * Enable write allocate feature of AMD processors.
 * Following two functions require the Maxmem variable being set.
 */
void
enable_K5_wt_alloc(void)
{
	u_int64_t	msr;
	register_t	savecrit;

	/*
	 * Write allocate is supported only on models 1, 2, and 3, with
	 * a stepping of 4 or greater.
	 */
	if (((cpu_id & 0xf0) > 0) && ((cpu_id & 0x0f) > 3)) {
		savecrit = intr_disable();
		msr = rdmsr(0x83);		/* HWCR */
		wrmsr(0x83, msr & !(0x10));

		/*
		 * We have to tell the chip where the top of memory is,
		 * since video cards could have frame bufferes there,
		 * memory-mapped I/O could be there, etc.
		 */
		if(Maxmem > 0)
		  msr = Maxmem / 16;
		else
		  msr = 0;
		msr |= AMD_WT_ALLOC_TME | AMD_WT_ALLOC_FRE;
#ifdef PC98
		if (!(inb(0x43b) & 4)) {
			wrmsr(0x86, 0x0ff00f0);
			msr |= AMD_WT_ALLOC_PRE;
		}
#else
		/*
		 * There is no way to know wheter 15-16M hole exists or not. 
		 * Therefore, we disable write allocate for this range.
		 */
			wrmsr(0x86, 0x0ff00f0);
			msr |= AMD_WT_ALLOC_PRE;
#endif
		wrmsr(0x85, msr);

		msr=rdmsr(0x83);
		wrmsr(0x83, msr|0x10); /* enable write allocate */
		intr_restore(savecrit);
	}
}

void
enable_K6_wt_alloc(void)
{
	quad_t	size;
	u_int64_t	whcr;
	u_long	eflags;

	eflags = read_eflags();
	disable_intr();
	wbinvd();

#ifdef CPU_DISABLE_CACHE
	/*
	 * Certain K6-2 box becomes unstable when write allocation is
	 * enabled.
	 */
	/*
	 * The AMD-K6 processer provides the 64-bit Test Register 12(TR12),
	 * but only the Cache Inhibit(CI) (bit 3 of TR12) is suppported.
	 * All other bits in TR12 have no effect on the processer's operation.
	 * The I/O Trap Restart function (bit 9 of TR12) is always enabled
	 * on the AMD-K6.
	 */
	wrmsr(0x0000000e, (u_int64_t)0x0008);
#endif
	/* Don't assume that memory size is aligned with 4M. */
	if (Maxmem > 0)
	  size = ((Maxmem >> 8) + 3) >> 2;
	else
	  size = 0;

	/* Limit is 508M bytes. */
	if (size > 0x7f)
		size = 0x7f;
	whcr = (rdmsr(0xc0000082) & ~(0x7fLL << 1)) | (size << 1);

#if defined(PC98) || defined(NO_MEMORY_HOLE)
	if (whcr & (0x7fLL << 1)) {
#ifdef PC98
		/*
		 * If bit 2 of port 0x43b is 0, disable wrte allocate for the
		 * 15-16M range.
		 */
		if (!(inb(0x43b) & 4))
			whcr &= ~0x0001LL;
		else
#endif
			whcr |=  0x0001LL;
	}
#else
	/*
	 * There is no way to know wheter 15-16M hole exists or not. 
	 * Therefore, we disable write allocate for this range.
	 */
	whcr &= ~0x0001LL;
#endif
	wrmsr(0x0c0000082, whcr);

	write_eflags(eflags);
}

void
enable_K6_2_wt_alloc(void)
{
	quad_t	size;
	u_int64_t	whcr;
	u_long	eflags;

	eflags = read_eflags();
	disable_intr();
	wbinvd();

#ifdef CPU_DISABLE_CACHE
	/*
	 * Certain K6-2 box becomes unstable when write allocation is
	 * enabled.
	 */
	/*
	 * The AMD-K6 processer provides the 64-bit Test Register 12(TR12),
	 * but only the Cache Inhibit(CI) (bit 3 of TR12) is suppported.
	 * All other bits in TR12 have no effect on the processer's operation.
	 * The I/O Trap Restart function (bit 9 of TR12) is always enabled
	 * on the AMD-K6.
	 */
	wrmsr(0x0000000e, (u_int64_t)0x0008);
#endif
	/* Don't assume that memory size is aligned with 4M. */
	if (Maxmem > 0)
	  size = ((Maxmem >> 8) + 3) >> 2;
	else
	  size = 0;

	/* Limit is 4092M bytes. */
	if (size > 0x3fff)
		size = 0x3ff;
	whcr = (rdmsr(0xc0000082) & ~(0x3ffLL << 22)) | (size << 22);

#if defined(PC98) || defined(NO_MEMORY_HOLE)
	if (whcr & (0x3ffLL << 22)) {
#ifdef PC98
		/*
		 * If bit 2 of port 0x43b is 0, disable wrte allocate for the
		 * 15-16M range.
		 */
		if (!(inb(0x43b) & 4))
			whcr &= ~(1LL << 16);
		else
#endif
			whcr |=  1LL << 16;
	}
#else
	/*
	 * There is no way to know wheter 15-16M hole exists or not. 
	 * Therefore, we disable write allocate for this range.
	 */
	whcr &= ~(1LL << 16);
#endif
	wrmsr(0x0c0000082, whcr);

	write_eflags(eflags);
}
#endif /* I585_CPU && CPU_WT_ALLOC */

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(cyrixreg, cyrixreg)
{
	u_long	eflags;
	u_int	cr0;
	u_char	ccr1, ccr2, ccr3;
	u_char	ccr0 = 0, ccr4 = 0, ccr5 = 0, pcr0 = 0;

	cr0 = rcr0();
	if (strcmp(cpu_vendor,"CyrixInstead") == 0) {
		eflags = read_eflags();
		disable_intr();


		if ((cpu != CPU_M1SC) && (cpu != CPU_CY486DX)) {
			ccr0 = read_cyrix_reg(CCR0);
		}
		ccr1 = read_cyrix_reg(CCR1);
		ccr2 = read_cyrix_reg(CCR2);
		ccr3 = read_cyrix_reg(CCR3);
		if ((cpu == CPU_M1SC) || (cpu == CPU_M1) || (cpu == CPU_M2)) {
			write_cyrix_reg(CCR3, CCR3_MAPEN0);
			ccr4 = read_cyrix_reg(CCR4);
			if ((cpu == CPU_M1) || (cpu == CPU_M2))
				ccr5 = read_cyrix_reg(CCR5);
			else
				pcr0 = read_cyrix_reg(PCR0);
			write_cyrix_reg(CCR3, ccr3);		/* Restore CCR3. */
		}
		write_eflags(eflags);

		if ((cpu != CPU_M1SC) && (cpu != CPU_CY486DX))
			printf("CCR0=%x, ", (u_int)ccr0);

		printf("CCR1=%x, CCR2=%x, CCR3=%x",
			(u_int)ccr1, (u_int)ccr2, (u_int)ccr3);
		if ((cpu == CPU_M1SC) || (cpu == CPU_M1) || (cpu == CPU_M2)) {
			printf(", CCR4=%x, ", (u_int)ccr4);
			if (cpu == CPU_M1SC)
				printf("PCR0=%x\n", pcr0);
			else
				printf("CCR5=%x\n", ccr5);
		}
	}
	printf("CR0=%x\n", cr0);
}
#endif /* DDB */
