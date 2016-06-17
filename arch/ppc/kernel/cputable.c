/*
 *  arch/ppc/kernel/cputable.c
 *
 *  Copyright (C) 2001 Ben. Herrenschmidt (benh@kernel.crashing.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/threads.h>
#include <linux/init.h>
#include <asm/cputable.h>

struct cpu_spec* cur_cpu_spec[NR_CPUS];

extern void __setup_cpu_601(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_603(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_604(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_750(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_750cx(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_750fx(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_7400(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_7410(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_745x(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_power3(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_power4(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_ppc970(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_8xx(unsigned long offset, int cpu_nr, struct cpu_spec* spec);
extern void __setup_cpu_generic(unsigned long offset, int cpu_nr, struct cpu_spec* spec);

#define CLASSIC_PPC (!defined(CONFIG_8xx) && !defined(CONFIG_4xx) && \
		     !defined(CONFIG_POWER3) && !defined(CONFIG_POWER4))

/* This table only contains "desktop" CPUs, it need to be filled with embedded
 * ones as well...
 */
#define COMMON_PPC	(PPC_FEATURE_32 | PPC_FEATURE_HAS_FPU | \
			 PPC_FEATURE_HAS_MMU)

/* We only set the altivec features if the kernel was compiled with altivec
 * support
 */
#ifdef CONFIG_ALTIVEC
#define CPU_FTR_ALTIVEC_COMP	CPU_FTR_ALTIVEC
#else
#define CPU_FTR_ALTIVEC_COMP	0
#endif

struct cpu_spec	cpu_specs[] = {
#if CLASSIC_PPC
    { 	/* 601 */
	0xffff0000, 0x00010000, "601",
	CPU_FTR_601 | CPU_FTR_HPTE_TABLE,
	COMMON_PPC | PPC_FEATURE_601_INSTR | PPC_FEATURE_UNIFIED_CACHE,
	32, 32,
	__setup_cpu_601
    },
    {	/* 603 */
    	0xffff0000, 0x00030000, "603",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
    	CPU_FTR_CAN_NAP,
	COMMON_PPC,
    	32, 32,
	__setup_cpu_603
    },
    {	/* 603e */
    	0xffff0000, 0x00060000, "603e",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
    	CPU_FTR_CAN_NAP,
	COMMON_PPC,
	32, 32,
	__setup_cpu_603
    },
    {	/* 603ev */
    	0xffff0000, 0x00070000, "603ev",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
    	CPU_FTR_CAN_NAP,
	COMMON_PPC,
	32, 32,
	__setup_cpu_603
    },
    {	/* 604 */
    	0xffff0000, 0x00040000, "604",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_604_PERF_MON |
	CPU_FTR_HPTE_TABLE,
	COMMON_PPC,
	32, 32,
	__setup_cpu_604
    },
    {	/* 604e */
    	0xfffff000, 0x00090000, "604e",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_604_PERF_MON |
	CPU_FTR_HPTE_TABLE,
	COMMON_PPC,
	32, 32,
	__setup_cpu_604
    },
    {	/* 604r */
    	0xffff0000, 0x00090000, "604r",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_604_PERF_MON |
	CPU_FTR_HPTE_TABLE,
	COMMON_PPC,
	32, 32,
	__setup_cpu_604
    },
    {	/* 604ev */
    	0xffff0000, 0x000a0000, "604ev",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_604_PERF_MON |
	CPU_FTR_HPTE_TABLE,
	COMMON_PPC,
	32, 32,
	__setup_cpu_604
    },
    {	/* 740/750 (0x4202, don't support TAU ?) */
    	0xffffffff, 0x00084202, "740/750",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_HPTE_TABLE | CPU_FTR_CAN_NAP,
	COMMON_PPC,
	32, 32,
	__setup_cpu_750
    },
    {	/* 745/755 */
    	0xfffff000, 0x00083000, "745/755",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_HPTE_TABLE | CPU_FTR_CAN_NAP,
	COMMON_PPC,
	32, 32,
	__setup_cpu_750
    },
    {	/* 750CX */
    	0xffffff00, 0x00082200, "750CX",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_HPTE_TABLE | CPU_FTR_CAN_NAP,
	COMMON_PPC,
	32, 32,
	__setup_cpu_750cx
    },
    {	/* 750FX rev 1.x */
    	0xffffff00, 0x70000100, "750FX",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_HPTE_TABLE | CPU_FTR_CAN_NAP |
	CPU_FTR_750FX | CPU_FTR_NO_DPM,
	COMMON_PPC,
	32, 32,
	__setup_cpu_750
    },
    {	/* 750FX rev 2.0 must disable HID0[DPM] */
    	0xffffffff, 0x70000200, "750FX",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_HPTE_TABLE | CPU_FTR_CAN_NAP |
	CPU_FTR_750FX | CPU_FTR_HAS_HIGH_BATS | CPU_FTR_NO_DPM,
	COMMON_PPC,
	32, 32,
	__setup_cpu_750
    },
    {	/* 750FX (All revs > 2.0) */
    	0xffff0000, 0x70000000, "750FX",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_HPTE_TABLE | CPU_FTR_CAN_NAP |
	CPU_FTR_750FX | CPU_FTR_HAS_HIGH_BATS,
	COMMON_PPC,
	32, 32,
	__setup_cpu_750fx
    },
    {	/* 740/750 (L2CR bit need fixup for 740) */
    	0xffff0000, 0x00080000, "740/750",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_HPTE_TABLE | CPU_FTR_CAN_NAP,
	COMMON_PPC,
	32, 32,
	__setup_cpu_750
    },
    {	/* 7400 rev 1.1 ? (no TAU) */
    	0xffffffff, 0x000c1101, "7400 (1.1)",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_HPTE_TABLE |
	CPU_FTR_CAN_NAP,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_7400
    },
    {	/* 7400 */
    	0xffff0000, 0x000c0000, "7400",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_ALTIVEC_COMP | CPU_FTR_HPTE_TABLE |
	CPU_FTR_CAN_NAP,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_7400
    },
    {	/* 7410 */
    	0xffff0000, 0x800c0000, "7410",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_TAU | CPU_FTR_ALTIVEC_COMP | CPU_FTR_HPTE_TABLE |
	CPU_FTR_CAN_NAP,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_7410
    },
    {	/* 7450 1.x - no doze/nap */
    	0xffffff00, 0x80000100, "7450",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 7450 2.0 - no doze/nap */
    	0xffffffff, 0x80000200, "7450",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 7450 2.1 */
    	0xffffffff, 0x80000201, "7450",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_CAN_NAP |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450 | CPU_FTR_NAP_DISABLE_L2_PR |
	CPU_FTR_L3_DISABLE_NAP,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 7450 2.3 and newer */
    	0xffff0000, 0x80000000, "7450",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_CAN_NAP |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450 | CPU_FTR_NAP_DISABLE_L2_PR,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 7455 rev 1.x */
    	0xffffff00, 0x80010100, "7455",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450 | CPU_FTR_HAS_HIGH_BATS,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 7455 rev 2.0 */
    	0xffffffff, 0x80010200, "7455",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_CAN_NAP |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450 | CPU_FTR_NAP_DISABLE_L2_PR |
	CPU_FTR_L3_DISABLE_NAP | CPU_FTR_HAS_HIGH_BATS,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 7455 others */
    	0xffff0000, 0x80010000, "7455",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_CAN_NAP |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450 | CPU_FTR_NAP_DISABLE_L2_PR |
	CPU_FTR_HAS_HIGH_BATS,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 7457 */
    	0xffff0000, 0x80020000, "7457",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_CAN_NAP |
	CPU_FTR_L2CR | CPU_FTR_ALTIVEC_COMP | CPU_FTR_L3CR |
	CPU_FTR_HPTE_TABLE | CPU_FTR_SPEC7450 | CPU_FTR_NAP_DISABLE_L2_PR |
	CPU_FTR_HAS_HIGH_BATS,
	COMMON_PPC | PPC_FEATURE_HAS_ALTIVEC,
	32, 32,
	__setup_cpu_745x
    },
    {	/* 82xx (8240, 8245, 8260 are all 603e cores) */
	0x7fff0000, 0x00810000, "82xx",
	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB,
	COMMON_PPC,
	32, 32,
	__setup_cpu_603
    },
    {	/* 8280 is a G2_LE (603e core, plus some) */
	0x7fff0000, 0x00820000, "8280",
	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_CAN_DOZE | CPU_FTR_USE_TB |
	CPU_FTR_CAN_NAP | CPU_FTR_HAS_HIGH_BATS,
	COMMON_PPC,
	32, 32,
	__setup_cpu_603
    },
    {	/* default match, we assume split I/D cache & TB (non-601)... */
    	0x00000000, 0x00000000, "(generic PPC)",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_HPTE_TABLE,
	COMMON_PPC,
	32, 32,
	__setup_cpu_generic
    },
#endif /* CLASSIC_PPC */
#ifdef CONFIG_PPC64BRIDGE
    {	/* Power3 */
    	0xffff0000, 0x00400000, "Power3 (630)",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_HPTE_TABLE,
    	COMMON_PPC | PPC_FEATURE_64,
	128, 128,
	__setup_cpu_power3
    },
    {	/* Power3+ */
    	0xffff0000, 0x00410000, "Power3 (630+)",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_HPTE_TABLE,
    	COMMON_PPC | PPC_FEATURE_64,
	128, 128,
	__setup_cpu_power3
    },
	{	/* I-star */
		0xffff0000, 0x00360000, "I-star",
		CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_HPTE_TABLE,
		COMMON_PPC | PPC_FEATURE_64,
		128, 128,
		__setup_cpu_power3
	},
	{	/* S-star */
		0xffff0000, 0x00370000, "S-star",
		CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_HPTE_TABLE,
		COMMON_PPC | PPC_FEATURE_64,
		128, 128,
		__setup_cpu_power3
	},
#endif /* CONFIG_PPC64BRIDGE */
#ifdef CONFIG_POWER4
    {	/* Power4 */
    	0xffff0000, 0x00350000, "Power4",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_HPTE_TABLE,
    	COMMON_PPC | PPC_FEATURE_64,
	128, 128,
	__setup_cpu_power4
    },
    {	/* PPC970 */
	0xffff0000, 0x00390000, "PPC970",
	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB | CPU_FTR_HPTE_TABLE |
	CPU_FTR_ALTIVEC_COMP | CPU_FTR_CAN_NAP,
	COMMON_PPC | PPC_FEATURE_64 | PPC_FEATURE_HAS_ALTIVEC,
	128, 128,
	__setup_cpu_ppc970
    },
#endif /* CONFIG_POWER4 */
#ifdef CONFIG_8xx
    {	/* 8xx */
    	0xffff0000, 0x00500000, "8xx",
		/* CPU_FTR_CAN_DOZE is possible, if the 8xx code is there.... */
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
    	PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
 	16, 16,
	__setup_cpu_8xx	/* Empty */
    },
#endif /* CONFIG_8xx */
#ifdef CONFIG_40x
    {	/* 403GC */
    	0xffffff00, 0x00200200, "403GC",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
    	PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
	16, 16,
	0, /*__setup_cpu_403 */
    },
    {	/* 403GCX */
    	0xffffff00, 0x00201400, "403GCX",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
    	PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
	16, 16,
	0, /*__setup_cpu_403 */
    },
    {	/* 403G ?? */
    	0xffff0000, 0x00200000, "403G ??",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
    	PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
	16, 16,
	0, /*__setup_cpu_403 */
    },
    {	/* 405GP */
    	0xffff0000, 0x40110000, "405GP",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
    	PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU | PPC_FEATURE_HAS_4xxMAC,
	32, 32,
	0, /*__setup_cpu_405 */
    },
    {	/* STB 03xxx */
    	0xffff0000, 0x40130000, "STB03xxx",
    	CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
    	PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU | PPC_FEATURE_HAS_4xxMAC,
	32, 32,
	0, /*__setup_cpu_405 */
    },
#endif /* CONFIG_4xx */
#ifdef CONFIG_44x
    { /* 440GP Rev. B */
        0xf0000fff, 0x40000440, "440GP Rev. B",
        CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
        PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
        32, 32,
        0, /*__setup_cpu_440 */
    },
    { /* 440GP Rev. C */
        0xf0000fff, 0x40000481, "440GP Rev. C",
        CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
        PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
        32, 32,
        0, /*__setup_cpu_440 */
    },
    { /* 440GX Rev. A */
        0xf0000fff, 0x50000850, "440GX Rev. A",
        CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
        PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
        32, 32,
        0, /*__setup_cpu_440 */
    },
    { /* 440GX Rev. B */
        0xf0000fff, 0x50000851, "440GX Rev. B",
        CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
        PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
        32, 32,
        0, /*__setup_cpu_440 */
    },
    { /* 440GX Rev. B1 (2.1) */
        0xf0000fff, 0x50000852, "440GX Rev. B1 (2.1)",
        CPU_FTR_SPLIT_ID_CACHE | CPU_FTR_USE_TB,
        PPC_FEATURE_32 | PPC_FEATURE_HAS_MMU,
        32, 32,
        0, /*__setup_cpu_440 */
    },
#endif /* CONFIG_44x */
#if !CLASSIC_PPC
    {	/* default match */
    	0x00000000, 0x00000000, "(generic PPC)",
    	0,
    	PPC_FEATURE_32,
	32, 32,
	0,
    }
#endif /* !CLASSIC_PPC */
};
