/*
 * Copyright (c) KATO Takenori, 1997.
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
 *		$Id: initcpu.c,v 1.5.2.2 1997/06/28 07:56:12 kato Exp $
 */

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

void initializecpu(void);
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
#endif

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
#ifdef SUSP_HLT
	ccr2 |= CCR2_SUSP_HTL;
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

	/* Restore CCR3. */
	write_cyrix_reg(CCR3, ccr3);

	/* Unlock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) & ~CCR2_LOCK_NW);

	load_cr0(rcr0() & ~(CR0_CD | CR0_NW));	/* CD = 0 and NW = 0 */

	/* Lock NW bit in CR0. */
	write_cyrix_reg(CCR2, read_cyrix_reg(CCR2) | CCR2_LOCK_NW);

	write_eflags(eflags);
}
#endif /* I686_CPU */

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
#endif
	default:
		break;
	}

#if defined(PC98) && !defined(CPU_UPGRADE_HW_CACHE)
	/*
	 * OS should flush L1 cahce by itself because no PC-98 supports
	 * non-Intel CPUs.  Use wbinvd instruction before DMA transfer
	 * when need_pre_dma_flush = 1, use invd instruction after DMA
	 * transfer when need_post_dma_flush = 1.  If your CPU upgrade
	 * product support hardware cache control, you can add
	 * UPGRADE_CPU_HW_CACHE option in your kernel configuration file.
	 * This option elminate unneeded cache flush instruction.
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
#endif /* PC98 && !UPGRADE_CPU_HW_CACHE */
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(cyrixreg, cyrixreg)
{
	u_long	eflags;
	u_int	cr0;
	u_char	ccr0, ccr1, ccr2, ccr3, ccr4, ccr5, pcr0;

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
		if ((cpu == CPU_M1SC) || (cpu == CPU_M1)) {
			write_cyrix_reg(CCR3, CCR3_MAPEN0);
			ccr4 = read_cyrix_reg(CCR4);
			if (cpu == CPU_M1)
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
		if ((cpu == CPU_M1SC) || (cpu == CPU_M1)) {
			printf(", CCR4=%x, ", (u_int)ccr4);
			if (cpu == CPU_M1)
				printf("CCR5=%x\n", ccr5);
			else
				printf("PCR0=%x\n", pcr0);
		}
	}
	printf("CR0=%x\n", cr0);
}
#endif /* DDB */
