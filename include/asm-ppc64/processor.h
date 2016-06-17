#ifndef __ASM_PPC64_PROCESSOR_H
#define __ASM_PPC64_PROCESSOR_H

/*
 * Copyright (C) 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/stringify.h>
#ifndef __ASSEMBLY__
#include <asm/atomic.h>
#include <asm/ppcdebug.h>
#include <asm/a.out.h>
#endif
#include <asm/ptrace.h>
#include <asm/types.h>
#include <asm/delay.h>

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

/* Machine State Register (MSR) Fields */
#define MSR_SF_LG	63              /* Enable 64 bit mode */
#define MSR_ISF_LG	61              /* Interrupt 64b mode valid on 630 */
#define MSR_HV_LG 	60              /* Hypervisor state */
#define MSR_VEC_LG	25	        /* Enable AltiVec */
#define MSR_POW_LG	18		/* Enable Power Management */
#define MSR_WE_LG	18		/* Wait State Enable */
#define MSR_TGPR_LG	17		/* TLB Update registers in use */
#define MSR_CE_LG	17		/* Critical Interrupt Enable */
#define MSR_ILE_LG	16		/* Interrupt Little Endian */
#define MSR_EE_LG	15		/* External Interrupt Enable */
#define MSR_PR_LG	14		/* Problem State / Privilege Level */
#define MSR_FP_LG	13		/* Floating Point enable */
#define MSR_ME_LG	12		/* Machine Check Enable */
#define MSR_FE0_LG	11		/* Floating Exception mode 0 */
#define MSR_SE_LG	10		/* Single Step */
#define MSR_BE_LG	9		/* Branch Trace */
#define MSR_DE_LG	9 		/* Debug Exception Enable */
#define MSR_FE1_LG	8		/* Floating Exception mode 1 */
#define MSR_IP_LG	6		/* Exception prefix 0x000/0xFFF */
#define MSR_IR_LG	5 		/* Instruction Relocate */
#define MSR_DR_LG	4 		/* Data Relocate */
#define MSR_PE_LG	3		/* Protection Enable */
#define MSR_PX_LG	2		/* Protection Exclusive Mode */
#define MSR_RI_LG	1		/* Recoverable Exception */
#define MSR_LE_LG	0 		/* Little Endian */

#ifdef __ASSEMBLY__
#define __MASK(X)	(1<<(X))
#else
#define __MASK(X)	(1UL<<(X))
#endif

#define MSR_SF		__MASK(MSR_SF_LG)	/* Enable 64 bit mode */
#define MSR_ISF		__MASK(MSR_ISF_LG)	/* Interrupt 64b mode valid on 630 */
#define MSR_HV 		__MASK(MSR_HV_LG)	/* Hypervisor state */
#define MSR_VEC		__MASK(MSR_VEC_LG)	/* Enable AltiVec */
#define MSR_POW		__MASK(MSR_POW_LG)	/* Enable Power Management */
#define MSR_WE		__MASK(MSR_WE_LG)	/* Wait State Enable */
#define MSR_TGPR	__MASK(MSR_TGPR_LG)	/* TLB Update registers in use */
#define MSR_CE		__MASK(MSR_CE_LG)	/* Critical Interrupt Enable */
#define MSR_ILE		__MASK(MSR_ILE_LG)	/* Interrupt Little Endian */
#define MSR_EE		__MASK(MSR_EE_LG)	/* External Interrupt Enable */
#define MSR_PR		__MASK(MSR_PR_LG)	/* Problem State / Privilege Level */
#define MSR_FP		__MASK(MSR_FP_LG)	/* Floating Point enable */
#define MSR_ME		__MASK(MSR_ME_LG)	/* Machine Check Enable */
#define MSR_FE0		__MASK(MSR_FE0_LG)	/* Floating Exception mode 0 */
#define MSR_SE		__MASK(MSR_SE_LG)	/* Single Step */
#define MSR_BE		__MASK(MSR_BE_LG)	/* Branch Trace */
#define MSR_DE		__MASK(MSR_DE_LG)	/* Debug Exception Enable */
#define MSR_FE1		__MASK(MSR_FE1_LG)	/* Floating Exception mode 1 */
#define MSR_IP		__MASK(MSR_IP_LG)	/* Exception prefix 0x000/0xFFF */
#define MSR_IR		__MASK(MSR_IR_LG)	/* Instruction Relocate */
#define MSR_DR		__MASK(MSR_DR_LG)	/* Data Relocate */
#define MSR_PE		__MASK(MSR_PE_LG)	/* Protection Enable */
#define MSR_PX		__MASK(MSR_PX_LG)	/* Protection Exclusive Mode */
#define MSR_RI		__MASK(MSR_RI_LG)	/* Recoverable Exception */
#define MSR_LE		__MASK(MSR_LE_LG)	/* Little Endian */

#define MSR_		MSR_ME | MSR_RI | MSR_IR | MSR_DR | MSR_ISF
#define MSR_KERNEL      MSR_ | MSR_SF | MSR_HV

#define MSR_USER32	MSR_ | MSR_PR | MSR_EE
#define MSR_USER64	MSR_USER32 | MSR_SF

/* Floating Point Status and Control Register (FPSCR) Fields */

#define FPSCR_FX	0x80000000	/* FPU exception summary */
#define FPSCR_FEX	0x40000000	/* FPU enabled exception summary */
#define FPSCR_VX	0x20000000	/* Invalid operation summary */
#define FPSCR_OX	0x10000000	/* Overflow exception summary */
#define FPSCR_UX	0x08000000	/* Underflow exception summary */
#define FPSCR_ZX	0x04000000	/* Zero-divide exception summary */
#define FPSCR_XX	0x02000000	/* Inexact exception summary */
#define FPSCR_VXSNAN	0x01000000	/* Invalid op for SNaN */
#define FPSCR_VXISI	0x00800000	/* Invalid op for Inv - Inv */
#define FPSCR_VXIDI	0x00400000	/* Invalid op for Inv / Inv */
#define FPSCR_VXZDZ	0x00200000	/* Invalid op for Zero / Zero */
#define FPSCR_VXIMZ	0x00100000	/* Invalid op for Inv * Zero */
#define FPSCR_VXVC	0x00080000	/* Invalid op for Compare */
#define FPSCR_FR	0x00040000	/* Fraction rounded */
#define FPSCR_FI	0x00020000	/* Fraction inexact */
#define FPSCR_FPRF	0x0001f000	/* FPU Result Flags */
#define FPSCR_FPCC	0x0000f000	/* FPU Condition Codes */
#define FPSCR_VXSOFT	0x00000400	/* Invalid op for software request */
#define FPSCR_VXSQRT	0x00000200	/* Invalid op for square root */
#define FPSCR_VXCVI	0x00000100	/* Invalid op for integer convert */
#define FPSCR_VE	0x00000080	/* Invalid op exception enable */
#define FPSCR_OE	0x00000040	/* IEEE overflow exception enable */
#define FPSCR_UE	0x00000020	/* IEEE underflow exception enable */
#define FPSCR_ZE	0x00000010	/* IEEE zero divide exception enable */
#define FPSCR_XE	0x00000008	/* FP inexact exception enable */
#define FPSCR_NI	0x00000004	/* FPU non IEEE-Mode */
#define FPSCR_RN	0x00000003	/* FPU rounding control */

/* Special Purpose Registers (SPRNs)*/

#define	SPRN_CDBCR	0x3D7	/* Cache Debug Control Register */
#define	SPRN_CTR	0x009	/* Count Register */
#define	SPRN_DABR	0x3F5	/* Data Address Breakpoint Register */
#define	SPRN_DAC1	0x3F6	/* Data Address Compare 1 */
#define	SPRN_DAC2	0x3F7	/* Data Address Compare 2 */
#define	SPRN_DAR	0x013	/* Data Address Register */
#define	SPRN_DBAT0L	0x219	/* Data BAT 0 Lower Register */
#define	SPRN_DBAT0U	0x218	/* Data BAT 0 Upper Register */
#define	SPRN_DBAT1L	0x21B	/* Data BAT 1 Lower Register */
#define	SPRN_DBAT1U	0x21A	/* Data BAT 1 Upper Register */
#define	SPRN_DBAT2L	0x21D	/* Data BAT 2 Lower Register */
#define	SPRN_DBAT2U	0x21C	/* Data BAT 2 Upper Register */
#define	SPRN_DBAT3L	0x21F	/* Data BAT 3 Lower Register */
#define	SPRN_DBAT3U	0x21E	/* Data BAT 3 Upper Register */
#define	SPRN_DBCR	0x3F2	/* Debug Control Register */
#define	  DBCR_EDM	0x80000000
#define	  DBCR_IDM	0x40000000
#define	  DBCR_RST(x)	(((x) & 0x3) << 28)
#define	    DBCR_RST_NONE       	0
#define	    DBCR_RST_CORE       	1
#define	    DBCR_RST_CHIP       	2
#define	    DBCR_RST_SYSTEM		3
#define	  DBCR_IC	0x08000000	/* Instruction Completion Debug Evnt */
#define	  DBCR_BT	0x04000000	/* Branch Taken Debug Event */
#define	  DBCR_EDE	0x02000000	/* Exception Debug Event */
#define	  DBCR_TDE	0x01000000	/* TRAP Debug Event */
#define	  DBCR_FER	0x00F80000	/* First Events Remaining Mask */
#define	  DBCR_FT	0x00040000	/* Freeze Timers on Debug Event */
#define	  DBCR_IA1	0x00020000	/* Instr. Addr. Compare 1 Enable */
#define	  DBCR_IA2	0x00010000	/* Instr. Addr. Compare 2 Enable */
#define	  DBCR_D1R	0x00008000	/* Data Addr. Compare 1 Read Enable */
#define	  DBCR_D1W	0x00004000	/* Data Addr. Compare 1 Write Enable */
#define	  DBCR_D1S(x)	(((x) & 0x3) << 12)	/* Data Adrr. Compare 1 Size */
#define	    DAC_BYTE	0
#define	    DAC_HALF	1
#define	    DAC_WORD	2
#define	    DAC_QUAD	3
#define	  DBCR_D2R	0x00000800	/* Data Addr. Compare 2 Read Enable */
#define	  DBCR_D2W	0x00000400	/* Data Addr. Compare 2 Write Enable */
#define	  DBCR_D2S(x)	(((x) & 0x3) << 8)	/* Data Addr. Compare 2 Size */
#define	  DBCR_SBT	0x00000040	/* Second Branch Taken Debug Event */
#define	  DBCR_SED	0x00000020	/* Second Exception Debug Event */
#define	  DBCR_STD	0x00000010	/* Second Trap Debug Event */
#define	  DBCR_SIA	0x00000008	/* Second IAC Enable */
#define	  DBCR_SDA	0x00000004	/* Second DAC Enable */
#define	  DBCR_JOI	0x00000002	/* JTAG Serial Outbound Int. Enable */
#define	  DBCR_JII	0x00000001	/* JTAG Serial Inbound Int. Enable */
#define	SPRN_DBCR0	0x3F2	/* Debug Control Register 0 */
#define	SPRN_DBCR1	0x3BD	/* Debug Control Register 1 */
#define	SPRN_DBSR	0x3F0	/* Debug Status Register */
#define	SPRN_DCCR	0x3FA	/* Data Cache Cacheability Register */
#define	  DCCR_NOCACHE		0	/* Noncacheable */
#define	  DCCR_CACHE		1	/* Cacheable */
#define	SPRN_DCMP	0x3D1	/* Data TLB Compare Register */
#define	SPRN_DCWR	0x3BA	/* Data Cache Write-thru Register */
#define	  DCWR_COPY		0	/* Copy-back */
#define	  DCWR_WRITE		1	/* Write-through */
#define	SPRN_DEAR	0x3D5	/* Data Error Address Register */
#define	SPRN_DEC	0x016	/* Decrement Register */
#define	SPRN_DMISS	0x3D0	/* Data TLB Miss Register */
#define	SPRN_DSISR	0x012	/* Data Storage Interrupt Status Register */
#define	SPRN_EAR	0x11A	/* External Address Register */
#define	SPRN_ESR	0x3D4	/* Exception Syndrome Register */
#define	  ESR_IMCP	0x80000000	/* Instr. Machine Check - Protection */
#define	  ESR_IMCN	0x40000000	/* Instr. Machine Check - Non-config */
#define	  ESR_IMCB	0x20000000	/* Instr. Machine Check - Bus error */
#define	  ESR_IMCT	0x10000000	/* Instr. Machine Check - Timeout */
#define	  ESR_PIL	0x08000000	/* Program Exception - Illegal */
#define	  ESR_PPR	0x04000000	/* Program Exception - Privileged */
#define	  ESR_PTR	0x02000000	/* Program Exception - Trap */
#define	  ESR_DST	0x00800000	/* Storage Exception - Data miss */
#define	  ESR_DIZ	0x00400000	/* Storage Exception - Zone fault */
#define	SPRN_EVPR	0x3D6	/* Exception Vector Prefix Register */
#define	SPRN_HASH1	0x3D2	/* Primary Hash Address Register */
#define	SPRN_HASH2	0x3D3	/* Secondary Hash Address Register */
#define	SPRN_HID0	0x3F0	/* Hardware Implementation Register 0 */
#define	  HID0_EMCP	(1<<31)		/* Enable Machine Check pin */
#define	  HID0_EBA	(1<<29)		/* Enable Bus Address Parity */
#define	  HID0_EBD	(1<<28)		/* Enable Bus Data Parity */
#define	  HID0_SBCLK	(1<<27)
#define	  HID0_EICE	(1<<26)
#define	  HID0_ECLK	(1<<25)
#define	  HID0_PAR	(1<<24)
#define	  HID0_DOZE	(1<<23)
#define	  HID0_NAP	(1<<22)
#define	  HID0_SLEEP	(1<<21)
#define	  HID0_DPM	(1<<20)
#define	  HID0_ICE	(1<<15)		/* Instruction Cache Enable */
#define	  HID0_DCE	(1<<14)		/* Data Cache Enable */
#define	  HID0_ILOCK	(1<<13)		/* Instruction Cache Lock */
#define	  HID0_DLOCK	(1<<12)		/* Data Cache Lock */
#define	  HID0_ICFI	(1<<11)		/* Instr. Cache Flash Invalidate */
#define	  HID0_DCI	(1<<10)		/* Data Cache Invalidate */
#define   HID0_SPD	(1<<9)		/* Speculative disable */
#define   HID0_SGE	(1<<7)		/* Store Gathering Enable */
#define	  HID0_SIED	(1<<7)		/* Serial Instr. Execution [Disable] */
#define   HID0_BTIC	(1<<5)		/* Branch Target Instruction Cache Enable */
#define   HID0_ABE	(1<<3)		/* Address Broadcast Enable */
#define	  HID0_BHTE	(1<<2)		/* Branch History Table Enable */
#define	  HID0_BTCD	(1<<1)		/* Branch target cache disable */
#define	SPRN_MSRDORM	0x3F1	/* Hardware Implementation Register 1 */
#define	SPRN_IABR	0x3F2	/* Instruction Address Breakpoint Register */
#define	SPRN_NIADORM	0x3F3	/* Hardware Implementation Register 2 */
#define	SPRN_TSC 	0x3FD	/* Thread switch control */
#define	SPRN_TST 	0x3FC	/* Thread switch timeout */
#define	SPRN_IAC1	0x3F4	/* Instruction Address Compare 1 */
#define	SPRN_IAC2	0x3F5	/* Instruction Address Compare 2 */
#define	SPRN_IBAT0L	0x211	/* Instruction BAT 0 Lower Register */
#define	SPRN_IBAT0U	0x210	/* Instruction BAT 0 Upper Register */
#define	SPRN_IBAT1L	0x213	/* Instruction BAT 1 Lower Register */
#define	SPRN_IBAT1U	0x212	/* Instruction BAT 1 Upper Register */
#define	SPRN_IBAT2L	0x215	/* Instruction BAT 2 Lower Register */
#define	SPRN_IBAT2U	0x214	/* Instruction BAT 2 Upper Register */
#define	SPRN_IBAT3L	0x217	/* Instruction BAT 3 Lower Register */
#define	SPRN_IBAT3U	0x216	/* Instruction BAT 3 Upper Register */
#define	SPRN_ICCR	0x3FB	/* Instruction Cache Cacheability Register */
#define	  ICCR_NOCACHE		0	/* Noncacheable */
#define	  ICCR_CACHE		1	/* Cacheable */
#define	SPRN_ICDBDR	0x3D3	/* Instruction Cache Debug Data Register */
#define	SPRN_ICMP	0x3D5	/* Instruction TLB Compare Register */
#define	SPRN_ICTC	0x3FB	/* Instruction Cache Throttling Control Reg */
#define	SPRN_IMISS	0x3D4	/* Instruction TLB Miss Register */
#define	SPRN_IMMR	0x27E  	/* Internal Memory Map Register */
#define	SPRN_L2CR	0x3F9	/* Level 2 Cache Control Register */
#define	SPRN_LR		0x008	/* Link Register */
#define	SPRN_PBL1	0x3FC	/* Protection Bound Lower 1 */
#define	SPRN_PBL2	0x3FE	/* Protection Bound Lower 2 */
#define	SPRN_PBU1	0x3FD	/* Protection Bound Upper 1 */
#define	SPRN_PBU2	0x3FF	/* Protection Bound Upper 2 */
#define	SPRN_PID	0x3B1	/* Process ID */
#define	SPRN_PIR	0x3FF	/* Processor Identification Register */
#define	SPRN_PIT	0x3DB	/* Programmable Interval Timer */
#define	SPRN_PVR	0x11F	/* Processor Version Register */
#define	SPRN_RPA	0x3D6	/* Required Physical Address Register */
#define	SPRN_SDR1	0x019	/* MMU Hash Base Register */
#define	SPRN_SGR	0x3B9	/* Storage Guarded Register */
#define	  SGR_NORMAL		0
#define	  SGR_GUARDED		1
#define	SPRN_SPRG0	0x110	/* Special Purpose Register General 0 */
#define	SPRN_SPRG1	0x111	/* Special Purpose Register General 1 */
#define	SPRN_SPRG2	0x112	/* Special Purpose Register General 2 */
#define	SPRN_SPRG3	0x113	/* Special Purpose Register General 3 */
#define	SPRN_SRR0	0x01A	/* Save/Restore Register 0 */
#define	SPRN_SRR1	0x01B	/* Save/Restore Register 1 */
#define	SPRN_SRR2	0x3DE	/* Save/Restore Register 2 */
#define	SPRN_SRR3 	0x3DF	/* Save/Restore Register 3 */
#define	SPRN_TBHI	0x3DC	/* Time Base High */
#define	SPRN_TBHU	0x3CC	/* Time Base High User-mode */
#define	SPRN_TBLO	0x3DD	/* Time Base Low */
#define	SPRN_TBLU	0x3CD	/* Time Base Low User-mode */
#define	SPRN_TBRL	0x10D	/* Time Base Read Lower Register */
#define	SPRN_TBRU	0x10C	/* Time Base Read Upper Register */
#define	SPRN_TBWL	0x11D	/* Time Base Write Lower Register */
#define	SPRN_TBWU	0x11C	/* Time Base Write Upper Register */
#define	SPRN_TCR	0x3DA	/* Timer Control Register */
#define	  TCR_WP(x)		(((x)&0x3)<<30)	/* WDT Period */
#define	    WP_2_17		0		/* 2^17 clocks */
#define	    WP_2_21		1		/* 2^21 clocks */
#define	    WP_2_25		2		/* 2^25 clocks */
#define	    WP_2_29		3		/* 2^29 clocks */
#define	  TCR_WRC(x)		(((x)&0x3)<<28)	/* WDT Reset Control */
#define	    WRC_NONE		0		/* No reset will occur */
#define	    WRC_CORE		1		/* Core reset will occur */
#define	    WRC_CHIP		2		/* Chip reset will occur */
#define	    WRC_SYSTEM		3		/* System reset will occur */
#define	  TCR_WIE		0x08000000	/* WDT Interrupt Enable */
#define	  TCR_PIE		0x04000000	/* PIT Interrupt Enable */
#define	  TCR_FP(x)		(((x)&0x3)<<24)	/* FIT Period */
#define	    FP_2_9		0		/* 2^9 clocks */
#define	    FP_2_13		1		/* 2^13 clocks */
#define	    FP_2_17		2		/* 2^17 clocks */
#define	    FP_2_21		3		/* 2^21 clocks */
#define	  TCR_FIE		0x00800000	/* FIT Interrupt Enable */
#define	  TCR_ARE		0x00400000	/* Auto Reload Enable */
#define	SPRN_THRM1	0x3FC	/* Thermal Management Register 1 */
#define	  THRM1_TIN		(1<<0)
#define	  THRM1_TIV		(1<<1)
#define	  THRM1_THRES		(0x7f<<2)
#define	  THRM1_TID		(1<<29)
#define	  THRM1_TIE		(1<<30)
#define	  THRM1_V		(1<<31)
#define	SPRN_THRM2	0x3FD	/* Thermal Management Register 2 */
#define	SPRN_THRM3	0x3FE	/* Thermal Management Register 3 */
#define	  THRM3_E		(1<<31)
#define	SPRN_TSR	0x3D8	/* Timer Status Register */
#define	  TSR_ENW		0x80000000	/* Enable Next Watchdog */
#define	  TSR_WIS		0x40000000	/* WDT Interrupt Status */
#define	  TSR_WRS(x)		(((x)&0x3)<<28)	/* WDT Reset Status */
#define	    WRS_NONE		0		/* No WDT reset occurred */
#define	    WRS_CORE		1		/* WDT forced core reset */
#define	    WRS_CHIP		2		/* WDT forced chip reset */
#define	    WRS_SYSTEM		3		/* WDT forced system reset */
#define	  TSR_PIS		0x08000000	/* PIT Interrupt Status */
#define	  TSR_FIS		0x04000000	/* FIT Interrupt Status */
#define SPRN_VRSAVE	0x100	/* Vector Register Save Register */
#define	SPRN_XER	0x001	/* Fixed Point Exception Register */
#define	SPRN_ZPR	0x3B0	/* Zone Protection Register */

/* Short-hand versions for a number of the above SPRNs */

#define	CTR	SPRN_CTR	/* Counter Register */
#define	DAR	SPRN_DAR	/* Data Address Register */
#define	DABR	SPRN_DABR	/* Data Address Breakpoint Register */
#define	DBAT0L	SPRN_DBAT0L	/* Data BAT 0 Lower Register */
#define	DBAT0U	SPRN_DBAT0U	/* Data BAT 0 Upper Register */
#define	DBAT1L	SPRN_DBAT1L	/* Data BAT 1 Lower Register */
#define	DBAT1U	SPRN_DBAT1U	/* Data BAT 1 Upper Register */
#define	DBAT2L	SPRN_DBAT2L	/* Data BAT 2 Lower Register */
#define	DBAT2U	SPRN_DBAT2U	/* Data BAT 2 Upper Register */
#define	DBAT3L	SPRN_DBAT3L	/* Data BAT 3 Lower Register */
#define	DBAT3U	SPRN_DBAT3U	/* Data BAT 3 Upper Register */
#define	DCMP	SPRN_DCMP      	/* Data TLB Compare Register */
#define	DEC	SPRN_DEC       	/* Decrement Register */
#define	DMISS	SPRN_DMISS     	/* Data TLB Miss Register */
#define	DSISR	SPRN_DSISR	/* Data Storage Interrupt Status Register */
#define	EAR	SPRN_EAR       	/* External Address Register */
#define	HASH1	SPRN_HASH1	/* Primary Hash Address Register */
#define	HASH2	SPRN_HASH2	/* Secondary Hash Address Register */
#define	HID0	SPRN_HID0	/* Hardware Implementation Register 0 */
#define	MSRDORM	SPRN_MSRDORM	/* MSR Dormant Register */
#define	NIADORM	SPRN_NIADORM	/* NIA Dormant Register */
#define	TSC    	SPRN_TSC 	/* Thread switch control */
#define	TST    	SPRN_TST 	/* Thread switch timeout */
#define	IABR	SPRN_IABR      	/* Instruction Address Breakpoint Register */
#define	IBAT0L	SPRN_IBAT0L	/* Instruction BAT 0 Lower Register */
#define	IBAT0U	SPRN_IBAT0U	/* Instruction BAT 0 Upper Register */
#define	IBAT1L	SPRN_IBAT1L	/* Instruction BAT 1 Lower Register */
#define	IBAT1U	SPRN_IBAT1U	/* Instruction BAT 1 Upper Register */
#define	IBAT2L	SPRN_IBAT2L	/* Instruction BAT 2 Lower Register */
#define	IBAT2U	SPRN_IBAT2U	/* Instruction BAT 2 Upper Register */
#define	IBAT3L	SPRN_IBAT3L	/* Instruction BAT 3 Lower Register */
#define	IBAT3U	SPRN_IBAT3U	/* Instruction BAT 3 Upper Register */
#define	ICMP	SPRN_ICMP	/* Instruction TLB Compare Register */
#define	IMISS	SPRN_IMISS	/* Instruction TLB Miss Register */
#define	IMMR	SPRN_IMMR      	/* PPC 860/821 Internal Memory Map Register */
#define	L2CR	SPRN_L2CR    	/* PPC 750 L2 control register */
#define	LR	SPRN_LR
#define	PVR	SPRN_PVR	/* Processor Version */
#define	PIR	SPRN_PIR	/* Processor ID */
#define	RPA	SPRN_RPA	/* Required Physical Address Register */
#define	SDR1	SPRN_SDR1      	/* MMU hash base register */
#define	SPR0	SPRN_SPRG0	/* Supervisor Private Registers */
#define	SPR1	SPRN_SPRG1
#define	SPR2	SPRN_SPRG2
#define	SPR3	SPRN_SPRG3
#define	SPRG0   SPRN_SPRG0
#define	SPRG1   SPRN_SPRG1
#define	SPRG2   SPRN_SPRG2
#define	SPRG3   SPRN_SPRG3
#define	SRR0	SPRN_SRR0	/* Save and Restore Register 0 */
#define	SRR1	SPRN_SRR1	/* Save and Restore Register 1 */
#define	TBRL	SPRN_TBRL	/* Time Base Read Lower Register */
#define	TBRU	SPRN_TBRU	/* Time Base Read Upper Register */
#define	TBWL	SPRN_TBWL	/* Time Base Write Lower Register */
#define	TBWU	SPRN_TBWU	/* Time Base Write Upper Register */
#define ICTC	1019
#define	THRM1	SPRN_THRM1	/* Thermal Management Register 1 */
#define	THRM2	SPRN_THRM2	/* Thermal Management Register 2 */
#define	THRM3	SPRN_THRM3	/* Thermal Management Register 3 */
#define	XER	SPRN_XER
#define PMC1	0x313
#define PMC2	0x314
#define PMC3	0x315
#define PMC4	0x316
#define PMC5	0x317
#define PMC6	0x318
#define PMC7	0x319
#define PMC8	0x31a
#define MMCR0	0x31b
#define MMCR1	0x31e
#define MMCRA	0x312
#define SIAR	0x30c
#define SDAR	0x30d

/* Device Control Registers */

#define	DCRN_BEAR	0x090	/* Bus Error Address Register */
#define	DCRN_BESR	0x091	/* Bus Error Syndrome Register */
#define	  BESR_DSES    	0x80000000	/* Data-Side Error Status */
#define	  BESR_DMES	0x40000000	/* DMA Error Status */
#define	  BESR_RWS	0x20000000	/* Read/Write Status */
#define	  BESR_ETMASK	0x1C000000	/* Error Type */
#define	    ET_PROT	0
#define	    ET_PARITY	1
#define	    ET_NCFG	2
#define	    ET_BUSERR	4
#define	    ET_BUSTO	6
#define	DCRN_DMACC0	0x0C4	/* DMA Chained Count Register 0 */
#define	DCRN_DMACC1	0x0CC	/* DMA Chained Count Register 1 */
#define	DCRN_DMACC2	0x0D4	/* DMA Chained Count Register 2 */
#define	DCRN_DMACC3	0x0DC    /* DMA Chained Count Register 3 */
#define	DCRN_DMACR0	0x0C0    /* DMA Channel Control Register 0 */
#define	DCRN_DMACR1	0x0C8    /* DMA Channel Control Register 1 */
#define	DCRN_DMACR2	0x0D0    /* DMA Channel Control Register 2 */
#define	DCRN_DMACR3	0x0D8    /* DMA Channel Control Register 3 */
#define	DCRN_DMACT0	0x0C1    /* DMA Count Register 0 */
#define	DCRN_DMACT1	0x0C9    /* DMA Count Register 1 */
#define	DCRN_DMACT2	0x0D1    /* DMA Count Register 2 */
#define	DCRN_DMACT3	0x0D9    /* DMA Count Register 3 */
#define	DCRN_DMADA0	0x0C2    /* DMA Destination Address Register 0 */
#define	DCRN_DMADA1	0x0CA    /* DMA Destination Address Register 1 */
#define	DCRN_DMADA2	0x0D2    /* DMA Destination Address Register 2 */
#define	DCRN_DMADA3	0x0DA    /* DMA Destination Address Register 3 */
#define	DCRN_DMASA0	0x0C3    /* DMA Source Address Register 0 */
#define	DCRN_DMASA1	0x0CB    /* DMA Source Address Register 1 */
#define	DCRN_DMASA2	0x0D3    /* DMA Source Address Register 2 */
#define	DCRN_DMASA3	0x0DB    /* DMA Source Address Register 3 */
#define	DCRN_DMASR	0x0E0    /* DMA Status Register */
#define	DCRN_EXIER	0x042    /* External Interrupt Enable Register */
#define	  EXIER_CIE	0x80000000	/* Critical Interrupt Enable */
#define	  EXIER_SRIE	0x08000000	/* Serial Port Rx Int. Enable */
#define	  EXIER_STIE	0x04000000	/* Serial Port Tx Int. Enable */
#define	  EXIER_JRIE	0x02000000	/* JTAG Serial Port Rx Int. Enable */
#define	  EXIER_JTIE	0x01000000	/* JTAG Serial Port Tx Int. Enable */
#define	  EXIER_D0IE	0x00800000	/* DMA Channel 0 Interrupt Enable */
#define	  EXIER_D1IE	0x00400000	/* DMA Channel 1 Interrupt Enable */
#define	  EXIER_D2IE	0x00200000	/* DMA Channel 2 Interrupt Enable */
#define	  EXIER_D3IE	0x00100000	/* DMA Channel 3 Interrupt Enable */
#define	  EXIER_E0IE	0x00000010	/* External Interrupt 0 Enable */
#define	  EXIER_E1IE	0x00000008	/* External Interrupt 1 Enable */
#define	  EXIER_E2IE	0x00000004	/* External Interrupt 2 Enable */
#define	  EXIER_E3IE	0x00000002	/* External Interrupt 3 Enable */
#define	  EXIER_E4IE	0x00000001	/* External Interrupt 4 Enable */
#define	DCRN_EXISR	0x040    /* External Interrupt Status Register */
#define	DCRN_IOCR	0x0A0    /* Input/Output Configuration Register */
#define	  IOCR_E0TE	0x80000000
#define	  IOCR_E0LP	0x40000000
#define	  IOCR_E1TE	0x20000000
#define	  IOCR_E1LP	0x10000000
#define	  IOCR_E2TE	0x08000000
#define	  IOCR_E2LP	0x04000000
#define	  IOCR_E3TE	0x02000000
#define	  IOCR_E3LP	0x01000000
#define	  IOCR_E4TE	0x00800000
#define	  IOCR_E4LP	0x00400000
#define	  IOCR_EDT     	0x00080000
#define	  IOCR_SOR     	0x00040000
#define	  IOCR_EDO	0x00008000
#define	  IOCR_2XC	0x00004000
#define	  IOCR_ATC	0x00002000
#define	  IOCR_SPD	0x00001000
#define	  IOCR_BEM	0x00000800
#define	  IOCR_PTD	0x00000400
#define	  IOCR_ARE	0x00000080
#define	  IOCR_DRC	0x00000020
#define	  IOCR_RDM(x)	(((x) & 0x3) << 3)
#define	  IOCR_TCS	0x00000004
#define	  IOCR_SCS	0x00000002
#define	  IOCR_SPC	0x00000001


/* Processor Version Register (PVR) field extraction */

#define	PVR_VER(pvr)  (((pvr) >>  16) & 0xFFFF)	/* Version field */
#define	PVR_REV(pvr)  (((pvr) >>   0) & 0xFFFF)	/* Revison field */

/* Processor Version Numbers */
#define	PV_NORTHSTAR	0x0033
#define	PV_PULSAR	0x0034
#define	PV_POWER4	0x0035
#define	PV_ICESTAR	0x0036
#define	PV_SSTAR	0x0037
#define	PV_POWER4p	0x0038
#define PV_POWER4ul	0x0039
#define	PV_630        	0x0040
#define	PV_630p	        0x0041

/* Platforms supported by PPC64 */
#define PLATFORM_PSERIES      0x0100
#define PLATFORM_PSERIES_LPAR 0x0101
#define PLATFORM_ISERIES_LPAR 0x0201
#define PLATFORM_LPAR         0x0001
	
/*
 * List of interrupt controllers.
 */
#define IC_INVALID    0
#define IC_OPEN_PIC   1
#define IC_PPC_XIC    2

#define XGLUE(a,b) a##b
#define GLUE(a,b) XGLUE(a,b)

/*
 * Beginning of traceback info work for asm functions.
 */
#define TB_ASM		0x000C000000000000
#define TB_GLOBALLINK	0x0000800000000000
#define TB_IS_EPROL	0x0000400000000000
#define TB_HAS_TBOFF	0x0000200000000000
#define TB_INT_PROC	0x0000100000000000
#define TB_HAS_CTL	0x0000080000000000
#define TB_TOCLESS	0x0000040000000000
#define TB_FP_PRESENT	0x0000020000000000
#define TB_LOG_ABORT	0x0000010000000000
#define TB_INT_HNDL	0x0000008000000000
#define TB_NAME_PRESENT	0x0000004000000000
#define TB_SAVES_CR	0x0000000200000000
#define TB_SAVES_LR	0x0000000100000000
#define TB_STORES_BC	0x0000000080000000
#define TB_PARMINFO	0x000000000000FFFF
#define TB_DEFAULT	TB_ASM | TB_HAS_TBOFF | TB_NAME_PRESENT

#ifdef __ASSEMBLY__

#define _GLOBAL(name) \
	.section ".text"; \
	.align 2 ; \
	.globl name; \
	.globl GLUE(.,name); \
	.section ".opd","aw"; \
name: \
	.quad GLUE(.,name); \
	.quad .TOC.@tocbase; \
	.quad 0; \
	.previous; \
	.type GLUE(.,name),@function; \
GLUE(.,name):

#define _STATIC(name) \
	.section ".text"; \
	.align 2 ; \
	.section ".opd","aw"; \
name: \
	.quad GLUE(.,name); \
	.quad .TOC.@tocbase; \
	.quad 0; \
	.previous; \
	.type GLUE(.,name),@function; \
GLUE(.,name):

#define _TRACEBACK(NAME) \
GLUE(.LT,NAME): ;\
	.long 0 ;\
	.llong TB_DEFAULT ;\
	.long GLUE(.LT,NAME)-GLUE(.,NAME) ;\
	.short GLUE(GLUE(.LT,NAME),_procname_end)-GLUE(GLUE(.LT,NAME),_procname_start) ;\
GLUE(GLUE(.LT,NAME),_procname_start): ;\
	.ascii __stringify(NAME) ;\
GLUE(GLUE(.LT,NAME),_procname_end):

#endif /* __ASSEMBLY__ */


/* Macros for setting and retrieving special purpose registers */

#define mfmsr()		({unsigned long rval; \
			asm volatile("mfmsr %0" : "=r" (rval)); rval;})

#define mtmsrd(v)	asm volatile("mtmsrd %0" : : "r" (v))

#define mfspr(rn)	({unsigned long rval; \
			asm volatile("mfspr %0," __stringify(rn) \
				     : "=r" (rval)); rval;})
#define mtspr(rn, v)	asm volatile("mtspr " __stringify(rn) ",%0" : : "r" (v))

#define mftb()		({unsigned long rval;	\
			asm volatile("mftb %0" : "=r" (rval)); rval;})

/* iSeries CTRL register (for runlatch) */

#define CTRLT		0x098
#define CTRLF		0x088
#define RUNLATCH	0x0001
#define RUN_FLAG	0x0002

/* Size of an exception stack frame contained in the paca. */
#define EXC_FRAME_SIZE 64

#define mfasr()		({unsigned long rval; \
			asm volatile("mfasr %0" : "=r" (rval)); rval;})

#ifndef __ASSEMBLY__
extern int have_of;

struct task_struct;
void start_thread(struct pt_regs *regs, unsigned long fdptr, unsigned long sp);
void release_thread(struct task_struct *);

/*
 * Create a new kernel thread.
 */
extern long arch_kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

/*
 * Bus types
 */
#define EISA_bus 0
#define EISA_bus__is_a_macro /* for versions in ksyms.c */
#define MCA_bus 0
#define MCA_bus__is_a_macro /* for versions in ksyms.c */

#ifndef CONFIG_SMP
/* Lazy FPU handling on uni-processor */
extern struct task_struct *last_task_used_math;
extern struct task_struct *last_task_used_altivec;
#endif /* CONFIG_SMP */


#ifdef __KERNEL__
/* 64-bit user address space is 41-bits (2TBs user VM) */
/* Subtract PGDIR_SIZE to work around a bug in free_pgtables */ 
#define TASK_SIZE_USER64 (0x0000020000000000UL - PGDIR_SIZE)

/* 
 * 32-bit user address space is 4GB - 1 page 
 * (this 1 page is needed so referencing of 0xFFFFFFFF generates EFAULT
 */
#define TASK_SIZE_USER32 (0x0000000100000000UL - (1*PAGE_SIZE))

#define TASK_SIZE ((current->thread.flags & PPC_FLAG_32BIT) ? \
		TASK_SIZE_USER32 : TASK_SIZE_USER64)
#endif /* __KERNEL__ */


/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE_USER32 PAGE_ALIGN(STACK_TOP_USER32 / 4)
#define TASK_UNMAPPED_BASE_USER64 PAGE_ALIGN(STACK_TOP_USER64 / 4)

#define TASK_UNMAPPED_BASE (((current->thread.flags & PPC_FLAG_32BIT)||(ppcdebugset(PPCDBG_BINFMT_32ADDR))) ? \
		TASK_UNMAPPED_BASE_USER32 : TASK_UNMAPPED_BASE_USER64 )

typedef struct {
	unsigned long seg;
} mm_segment_t;

struct thread_struct {
	unsigned long	ksp;		/* Kernel stack pointer */
	struct pt_regs	*regs;		/* Pointer to saved register state */
	mm_segment_t	fs;		/* for get_fs() validation */
	void		*pgdir;		/* root of page-table tree */
	signed long     last_syscall;
	unsigned long	flags;
	double		fpr[32];	/* Complete floating point set */
	unsigned long	fpscr;		/* Floating point status (plus pad) */
	unsigned long	fpexc_mode;	/* Floating-point exception mode */
	unsigned long	saved_msr;	/* Save MSR across signal handlers */
	unsigned long	saved_softe;	/* Ditto for Soft Enable/Disable */
#ifdef CONFIG_ALTIVEC
	vector128	vr[32];		/* Complete AltiVec set */
	vector128	vscr;		/* AltiVec status */
	u32             vrsave[2];      /* 32 bit vrsave is in vrsave[1] */
#endif /* CONFIG_ALTIVEC */
};

#define PPC_FLAG_32BIT		0x01
#define PPC_FLAG_RUN_LIGHT	RUN_FLAG

#ifdef CONFIG_SHARED_MEMORY_ADDRESSING
#define PPC_FLAG_SHARED   	0x4UL
#endif

#define INIT_SP		(sizeof(init_stack) + (unsigned long) &init_stack)

#define INIT_THREAD  { \
	INIT_SP, /* ksp */ \
	(struct pt_regs *)INIT_SP - 1, /* regs */ \
	KERNEL_DS, /*fs*/ \
	swapper_pg_dir, /* pgdir */ \
	0, /* last_syscall */ \
	PPC_FLAG_RUN_LIGHT, /* flags */ \
	{0}, /* fprs */ \
	0, /* fpscr */ \
	MSR_FE0|MSR_FE1, /* fpexc_mode */ \
}

/*
 * Note: the vm_start and vm_end fields here should *not*
 * be in kernel space.  (Could vm_end == vm_start perhaps?)
 */
#define IOREMAP_MMAP { &ioremap_mm, 0, 0x1000, NULL, \
		    PAGE_SHARED, VM_READ | VM_WRITE | VM_EXEC, \
		    1, NULL, NULL }

extern struct mm_struct ioremap_mm;

/*
 * Return saved PC of a blocked thread. For now, this is the "user" PC
 */
static inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	return (t->regs) ? t->regs->nip : 0;
}

#define copy_segments(tsk, mm)		do { } while (0)
#define release_segments(mm)		do { } while (0)
#define forget_segments()		do { } while (0)

unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->nip: 0)
#define KSTK_ESP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->gpr[1]: 0)

/* Get/set floating-point exception mode */
#define GET_FPEXC_CTL(tsk, adr)	get_fpexc_mode((tsk), (adr))
#define SET_FPEXC_CTL(tsk, val)	set_fpexc_mode((tsk), (val))

extern int get_fpexc_mode(struct task_struct *tsk, unsigned long adr);
extern int set_fpexc_mode(struct task_struct *tsk, unsigned int val);

static inline unsigned int __unpack_fe01(unsigned long msr_bits)
{
	return ((msr_bits & MSR_FE0) >> 10) | ((msr_bits & MSR_FE1) >> 8);
}

static inline unsigned long __pack_fe01(unsigned int fpmode)
{
	return ((fpmode << 10) & MSR_FE0) | ((fpmode << 8) & MSR_FE1);
}


/*
 * NOTE! The task struct and the stack go together
 */
#define THREAD_SIZE (4*PAGE_SIZE)
struct task_struct * alloc_task_struct(void);
        
void free_task_struct(struct task_struct *);
#define get_task_struct(tsk)	atomic_inc(&virt_to_page(tsk)->count)

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

#define cpu_relax()     udelay(1)

/*
 * Prefetch macros.
 */
#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
#define ARCH_HAS_SPINLOCK_PREFETCH

static inline void prefetch(const void *x)
{
	__asm__ __volatile__ ("dcbt 0,%0" : : "r" (x));
}

static inline void prefetchw(const void *x)
{
	__asm__ __volatile__ ("dcbtst 0,%0" : : "r" (x));
}

#define spin_lock_prefetch(x)	prefetchw(x)

#endif /* ASSEMBLY */

#endif /* __ASM_PPC64_PROCESSOR_H */
