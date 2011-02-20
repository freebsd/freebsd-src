/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * Copyright (c) 1997 KATO Takenori.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *
 *	from: Id: machdep.c,v 1.193 1996/06/18 01:22:04 bde Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/power.h>

#include <machine/asmacros.h>
#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <machine/specialreg.h>

#define	IDENTBLUE_CYRIX486	0
#define	IDENTBLUE_IBMCPU	1
#define	IDENTBLUE_CYRIXM2	2

/* XXX - should be in header file: */
void printcpuinfo(void);
void finishidentcpu(void);
void earlysetcpuclass(void);
#if defined(I586_CPU) && defined(CPU_WT_ALLOC)
void	enable_K5_wt_alloc(void);
void	enable_K6_wt_alloc(void);
void	enable_K6_2_wt_alloc(void);
#endif
void panicifcpuunsupported(void);

static void identifycyrix(void);
static void init_exthigh(void);
static u_int find_cpu_vendor_id(void);
static void print_AMD_info(void);
static void print_INTEL_info(void);
static void print_INTEL_TLB(u_int data);
static void print_AMD_assoc(int i);
static void print_transmeta_info(void);
static void print_via_padlock_info(void);

int	cpu_class;
u_int	cpu_exthigh;		/* Highest arg to extended CPUID */
u_int	cyrix_did;		/* Device ID of Cyrix CPU */
char machine[] = MACHINE;
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, 
    machine, 0, "Machine class");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, 
    cpu_model, 0, "Machine model");

static int hw_clockrate;
SYSCTL_INT(_hw, OID_AUTO, clockrate, CTLFLAG_RD, 
    &hw_clockrate, 0, "CPU instruction clock rate");

static eventhandler_tag tsc_post_tag;

static char cpu_brand[48];

#define	MAX_BRAND_INDEX	8

static const char *cpu_brandtable[MAX_BRAND_INDEX + 1] = {
	NULL,			/* No brand */
	"Intel Celeron",
	"Intel Pentium III",
	"Intel Pentium III Xeon",
	NULL,
	NULL,
	NULL,
	NULL,
	"Intel Pentium 4"
};

static struct {
	char	*cpu_name;
	int	cpu_class;
} i386_cpus[] = {
	{ "Intel 80286",	CPUCLASS_286 },		/* CPU_286   */
	{ "i386SX",		CPUCLASS_386 },		/* CPU_386SX */
	{ "i386DX",		CPUCLASS_386 },		/* CPU_386   */
	{ "i486SX",		CPUCLASS_486 },		/* CPU_486SX */
	{ "i486DX",		CPUCLASS_486 },		/* CPU_486   */
	{ "Pentium",		CPUCLASS_586 },		/* CPU_586   */
	{ "Cyrix 486",		CPUCLASS_486 },		/* CPU_486DLC */
	{ "Pentium Pro",	CPUCLASS_686 },		/* CPU_686 */
	{ "Cyrix 5x86",		CPUCLASS_486 },		/* CPU_M1SC */
	{ "Cyrix 6x86",		CPUCLASS_486 },		/* CPU_M1 */
	{ "Blue Lightning",	CPUCLASS_486 },		/* CPU_BLUE */
	{ "Cyrix 6x86MX",	CPUCLASS_686 },		/* CPU_M2 */
	{ "NexGen 586",		CPUCLASS_386 },		/* CPU_NX586 (XXX) */
	{ "Cyrix 486S/DX",	CPUCLASS_486 },		/* CPU_CY486DX */
	{ "Pentium II",		CPUCLASS_686 },		/* CPU_PII */
	{ "Pentium III",	CPUCLASS_686 },		/* CPU_PIII */
	{ "Pentium 4",		CPUCLASS_686 },		/* CPU_P4 */
};

static struct {
	char	*vendor;
	u_int	vendor_id;
} cpu_vendors[] = {
	{ INTEL_VENDOR_ID,	CPU_VENDOR_INTEL },	/* GenuineIntel */
	{ AMD_VENDOR_ID,	CPU_VENDOR_AMD },	/* AuthenticAMD */
	{ CENTAUR_VENDOR_ID,	CPU_VENDOR_CENTAUR },	/* CentaurHauls */
	{ NSC_VENDOR_ID,	CPU_VENDOR_NSC },	/* Geode by NSC */
	{ CYRIX_VENDOR_ID,	CPU_VENDOR_CYRIX },	/* CyrixInstead */
	{ TRANSMETA_VENDOR_ID,	CPU_VENDOR_TRANSMETA },	/* GenuineTMx86 */
	{ SIS_VENDOR_ID,	CPU_VENDOR_SIS },	/* SiS SiS SiS  */
	{ UMC_VENDOR_ID,	CPU_VENDOR_UMC },	/* UMC UMC UMC  */
	{ NEXGEN_VENDOR_ID,	CPU_VENDOR_NEXGEN },	/* NexGenDriven */
	{ RISE_VENDOR_ID,	CPU_VENDOR_RISE },	/* RiseRiseRise */
#if 0
	/* XXX CPUID 8000_0000h and 8086_0000h, not 0000_0000h */
	{ "TransmetaCPU",	CPU_VENDOR_TRANSMETA },
#endif
};

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
int has_f00f_bug = 0;		/* Initialized so that it can be patched. */
#endif

static void
init_exthigh(void)
{
	static int done = 0;
	u_int regs[4];

	if (done == 0) {
		if (cpu_high > 0 &&
		    (cpu_vendor_id == CPU_VENDOR_INTEL ||
		    cpu_vendor_id == CPU_VENDOR_AMD ||
		    cpu_vendor_id == CPU_VENDOR_TRANSMETA ||
		    cpu_vendor_id == CPU_VENDOR_CENTAUR ||
		    cpu_vendor_id == CPU_VENDOR_NSC)) {
			do_cpuid(0x80000000, regs);
			if (regs[0] >= 0x80000000)
				cpu_exthigh = regs[0];
		}

		done = 1;
	}
}

void
printcpuinfo(void)
{
	u_int regs[4], i;
	char *brand;

	cpu_class = i386_cpus[cpu].cpu_class;
	printf("CPU: ");
	strncpy(cpu_model, i386_cpus[cpu].cpu_name, sizeof (cpu_model));

	/* Check for extended CPUID information and a processor name. */
	init_exthigh();
	if (cpu_exthigh >= 0x80000004) {
		brand = cpu_brand;
		for (i = 0x80000002; i < 0x80000005; i++) {
			do_cpuid(i, regs);
			memcpy(brand, regs, sizeof(regs));
			brand += sizeof(regs);
		}
	}

	if (cpu_vendor_id == CPU_VENDOR_INTEL) {
		if ((cpu_id & 0xf00) > 0x300) {
			u_int brand_index;

			cpu_model[0] = '\0';

			switch (cpu_id & 0x3000) {
			case 0x1000:
				strcpy(cpu_model, "Overdrive ");
				break;
			case 0x2000:
				strcpy(cpu_model, "Dual ");
				break;
			}

			switch (cpu_id & 0xf00) {
			case 0x400:
				strcat(cpu_model, "i486 ");
			        /* Check the particular flavor of 486 */
				switch (cpu_id & 0xf0) {
				case 0x00:
				case 0x10:
					strcat(cpu_model, "DX");
					break;
				case 0x20:
					strcat(cpu_model, "SX");
					break;
				case 0x30:
					strcat(cpu_model, "DX2");
					break;
				case 0x40:
					strcat(cpu_model, "SL");
					break;
				case 0x50:
					strcat(cpu_model, "SX2");
					break;
				case 0x70:
					strcat(cpu_model,
					    "DX2 Write-Back Enhanced");
					break;
				case 0x80:
					strcat(cpu_model, "DX4");
					break;
				}
				break;
			case 0x500:
			        /* Check the particular flavor of 586 */
			        strcat(cpu_model, "Pentium");
			        switch (cpu_id & 0xf0) {
				case 0x00:
				        strcat(cpu_model, " A-step");
					break;
				case 0x10:
				        strcat(cpu_model, "/P5");
					break;
				case 0x20:
				        strcat(cpu_model, "/P54C");
					break;
				case 0x30:
				        strcat(cpu_model, "/P24T");
					break;
				case 0x40:
				        strcat(cpu_model, "/P55C");
					break;
				case 0x70:
				        strcat(cpu_model, "/P54C");
					break;
				case 0x80:
				        strcat(cpu_model, "/P55C (quarter-micron)");
					break;
				default:
				        /* nothing */
					break;
				}
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
				/*
				 * XXX - If/when Intel fixes the bug, this
				 * should also check the version of the
				 * CPU, not just that it's a Pentium.
				 */
				has_f00f_bug = 1;
#endif
				break;
			case 0x600:
			        /* Check the particular flavor of 686 */
  			        switch (cpu_id & 0xf0) {
				case 0x00:
				        strcat(cpu_model, "Pentium Pro A-step");
					break;
				case 0x10:
				        strcat(cpu_model, "Pentium Pro");
					break;
				case 0x30:
				case 0x50:
				case 0x60:
				        strcat(cpu_model,
				"Pentium II/Pentium II Xeon/Celeron");
					cpu = CPU_PII;
					break;
				case 0x70:
				case 0x80:
				case 0xa0:
				case 0xb0:
				        strcat(cpu_model,
					"Pentium III/Pentium III Xeon/Celeron");
					cpu = CPU_PIII;
					break;
				default:
				        strcat(cpu_model, "Unknown 80686");
					break;
				}
				break;
			case 0xf00:
				strcat(cpu_model, "Pentium 4");
				cpu = CPU_P4;
				break;
			default:
				strcat(cpu_model, "unknown");
				break;
			}

			/*
			 * If we didn't get a brand name from the extended
			 * CPUID, try to look it up in the brand table.
			 */
			if (cpu_high > 0 && *cpu_brand == '\0') {
				brand_index = cpu_procinfo & CPUID_BRAND_INDEX;
				if (brand_index <= MAX_BRAND_INDEX &&
				    cpu_brandtable[brand_index] != NULL)
					strcpy(cpu_brand,
					    cpu_brandtable[brand_index]);
			}
		}
	} else if (cpu_vendor_id == CPU_VENDOR_AMD) {
		/*
		 * Values taken from AMD Processor Recognition
		 * http://www.amd.com/K6/k6docs/pdf/20734g.pdf
		 * (also describes ``Features'' encodings.
		 */
		strcpy(cpu_model, "AMD ");
		switch (cpu_id & 0xFF0) {
		case 0x410:
			strcat(cpu_model, "Standard Am486DX");
			break;
		case 0x430:
			strcat(cpu_model, "Enhanced Am486DX2 Write-Through");
			break;
		case 0x470:
			strcat(cpu_model, "Enhanced Am486DX2 Write-Back");
			break;
		case 0x480:
			strcat(cpu_model, "Enhanced Am486DX4/Am5x86 Write-Through");
			break;
		case 0x490:
			strcat(cpu_model, "Enhanced Am486DX4/Am5x86 Write-Back");
			break;
		case 0x4E0:
			strcat(cpu_model, "Am5x86 Write-Through");
			break;
		case 0x4F0:
			strcat(cpu_model, "Am5x86 Write-Back");
			break;
		case 0x500:
			strcat(cpu_model, "K5 model 0");
			tsc_is_broken = 1;
			break;
		case 0x510:
			strcat(cpu_model, "K5 model 1");
			break;
		case 0x520:
			strcat(cpu_model, "K5 PR166 (model 2)");
			break;
		case 0x530:
			strcat(cpu_model, "K5 PR200 (model 3)");
			break;
		case 0x560:
			strcat(cpu_model, "K6");
			break;
		case 0x570:
			strcat(cpu_model, "K6 266 (model 1)");
			break;
		case 0x580:
			strcat(cpu_model, "K6-2");
			break;
		case 0x590:
			strcat(cpu_model, "K6-III");
			break;
		case 0x5a0:
			strcat(cpu_model, "Geode LX");
			/*
			 * Make sure the TSC runs through suspension,
			 * otherwise we can't use it as timecounter
			 */
			wrmsr(0x1900, rdmsr(0x1900) | 0x20ULL);
			break;
		default:
			strcat(cpu_model, "Unknown");
			break;
		}
#if defined(I586_CPU) && defined(CPU_WT_ALLOC)
		if ((cpu_id & 0xf00) == 0x500) {
			if (((cpu_id & 0x0f0) > 0)
			    && ((cpu_id & 0x0f0) < 0x60)
			    && ((cpu_id & 0x00f) > 3))
				enable_K5_wt_alloc();
			else if (((cpu_id & 0x0f0) > 0x80)
				 || (((cpu_id & 0x0f0) == 0x80)
				     && (cpu_id & 0x00f) > 0x07))
				enable_K6_2_wt_alloc();
			else if ((cpu_id & 0x0f0) > 0x50)
				enable_K6_wt_alloc();
		}
#endif
	} else if (cpu_vendor_id == CPU_VENDOR_CYRIX) {
		strcpy(cpu_model, "Cyrix ");
		switch (cpu_id & 0xff0) {
		case 0x440:
			strcat(cpu_model, "MediaGX");
			break;
		case 0x520:
			strcat(cpu_model, "6x86");
			break;
		case 0x540:
			cpu_class = CPUCLASS_586;
			strcat(cpu_model, "GXm");
			break;
		case 0x600:
			strcat(cpu_model, "6x86MX");
			break;
		default:
			/*
			 * Even though CPU supports the cpuid
			 * instruction, it can be disabled.
			 * Therefore, this routine supports all Cyrix
			 * CPUs.
			 */
			switch (cyrix_did & 0xf0) {
			case 0x00:
				switch (cyrix_did & 0x0f) {
				case 0x00:
					strcat(cpu_model, "486SLC");
					break;
				case 0x01:
					strcat(cpu_model, "486DLC");
					break;
				case 0x02:
					strcat(cpu_model, "486SLC2");
					break;
				case 0x03:
					strcat(cpu_model, "486DLC2");
					break;
				case 0x04:
					strcat(cpu_model, "486SRx");
					break;
				case 0x05:
					strcat(cpu_model, "486DRx");
					break;
				case 0x06:
					strcat(cpu_model, "486SRx2");
					break;
				case 0x07:
					strcat(cpu_model, "486DRx2");
					break;
				case 0x08:
					strcat(cpu_model, "486SRu");
					break;
				case 0x09:
					strcat(cpu_model, "486DRu");
					break;
				case 0x0a:
					strcat(cpu_model, "486SRu2");
					break;
				case 0x0b:
					strcat(cpu_model, "486DRu2");
					break;
				default:
					strcat(cpu_model, "Unknown");
					break;
				}
				break;
			case 0x10:
				switch (cyrix_did & 0x0f) {
				case 0x00:
					strcat(cpu_model, "486S");
					break;
				case 0x01:
					strcat(cpu_model, "486S2");
					break;
				case 0x02:
					strcat(cpu_model, "486Se");
					break;
				case 0x03:
					strcat(cpu_model, "486S2e");
					break;
				case 0x0a:
					strcat(cpu_model, "486DX");
					break;
				case 0x0b:
					strcat(cpu_model, "486DX2");
					break;
				case 0x0f:
					strcat(cpu_model, "486DX4");
					break;
				default:
					strcat(cpu_model, "Unknown");
					break;
				}
				break;
			case 0x20:
				if ((cyrix_did & 0x0f) < 8)
					strcat(cpu_model, "6x86");	/* Where did you get it? */
				else
					strcat(cpu_model, "5x86");
				break;
			case 0x30:
				strcat(cpu_model, "6x86");
				break;
			case 0x40:
				if ((cyrix_did & 0xf000) == 0x3000) {
					cpu_class = CPUCLASS_586;
					strcat(cpu_model, "GXm");
				} else
					strcat(cpu_model, "MediaGX");
				break;
			case 0x50:
				strcat(cpu_model, "6x86MX");
				break;
			case 0xf0:
				switch (cyrix_did & 0x0f) {
				case 0x0d:
					strcat(cpu_model, "Overdrive CPU");
					break;
				case 0x0e:
					strcpy(cpu_model, "Texas Instruments 486SXL");
					break;
				case 0x0f:
					strcat(cpu_model, "486SLC/DLC");
					break;
				default:
					strcat(cpu_model, "Unknown");
					break;
				}
				break;
			default:
				strcat(cpu_model, "Unknown");
				break;
			}
			break;
		}
	} else if (cpu_vendor_id == CPU_VENDOR_RISE) {
		strcpy(cpu_model, "Rise ");
		switch (cpu_id & 0xff0) {
		case 0x500:
			strcat(cpu_model, "mP6");
			break;
		default:
			strcat(cpu_model, "Unknown");
		}
	} else if (cpu_vendor_id == CPU_VENDOR_CENTAUR) {
		switch (cpu_id & 0xff0) {
		case 0x540:
			strcpy(cpu_model, "IDT WinChip C6");
			tsc_is_broken = 1;
			break;
		case 0x580:
			strcpy(cpu_model, "IDT WinChip 2");
			break;
		case 0x660:
			strcpy(cpu_model, "VIA C3 Samuel");
			break;
		case 0x670:
			if (cpu_id & 0x8)
				strcpy(cpu_model, "VIA C3 Ezra");
			else
				strcpy(cpu_model, "VIA C3 Samuel 2");
			break;
		case 0x680:
			strcpy(cpu_model, "VIA C3 Ezra-T");
			break;
		case 0x690:
			strcpy(cpu_model, "VIA C3 Nehemiah");
			break;
		case 0x6a0:
		case 0x6d0:
			strcpy(cpu_model, "VIA C7 Esther");
			break;
		case 0x6f0:
			strcpy(cpu_model, "VIA Nano");
			break;
		default:
			strcpy(cpu_model, "VIA/IDT Unknown");
		}
	} else if (cpu_vendor_id == CPU_VENDOR_IBM) {
		strcpy(cpu_model, "Blue Lightning CPU");
	} else if (cpu_vendor_id == CPU_VENDOR_NSC) {
		switch (cpu_id & 0xfff) {
		case 0x540:
			strcpy(cpu_model, "Geode SC1100");
			cpu = CPU_GEODE1100;
			tsc_is_broken = 1;
			break;
		default:
			strcpy(cpu_model, "Geode/NSC unknown");
			break;
		}
	}

	/*
	 * Replace cpu_model with cpu_brand minus leading spaces if
	 * we have one.
	 */
	brand = cpu_brand;
	while (*brand == ' ')
		++brand;
	if (*brand != '\0')
		strcpy(cpu_model, brand);

	printf("%s (", cpu_model);
	switch(cpu_class) {
	case CPUCLASS_286:
		printf("286");
		break;
	case CPUCLASS_386:
		printf("386");
		break;
#if defined(I486_CPU)
	case CPUCLASS_486:
		printf("486");
		break;
#endif
#if defined(I586_CPU)
	case CPUCLASS_586:
		hw_clockrate = (tsc_freq + 5000) / 1000000;
		printf("%jd.%02d-MHz ",
		       (intmax_t)(tsc_freq + 4999) / 1000000,
		       (u_int)((tsc_freq + 4999) / 10000) % 100);
		printf("586");
		break;
#endif
#if defined(I686_CPU)
	case CPUCLASS_686:
		hw_clockrate = (tsc_freq + 5000) / 1000000;
		printf("%jd.%02d-MHz ",
		       (intmax_t)(tsc_freq + 4999) / 1000000,
		       (u_int)((tsc_freq + 4999) / 10000) % 100);
		printf("686");
		break;
#endif
	default:
		printf("Unknown");	/* will panic below... */
	}
	printf("-class CPU)\n");
	if(*cpu_vendor)
		printf("  Origin = \"%s\"",cpu_vendor);
	if(cpu_id)
		printf("  Id = 0x%x", cpu_id);

	if (cpu_vendor_id == CPU_VENDOR_INTEL ||
	    cpu_vendor_id == CPU_VENDOR_AMD ||
	    cpu_vendor_id == CPU_VENDOR_TRANSMETA ||
	    cpu_vendor_id == CPU_VENDOR_RISE ||
	    cpu_vendor_id == CPU_VENDOR_CENTAUR ||
	    cpu_vendor_id == CPU_VENDOR_NSC ||
		(cpu_vendor_id == CPU_VENDOR_CYRIX &&
		 ((cpu_id & 0xf00) > 0x500))) {
		printf("  Family = %x", CPUID_TO_FAMILY(cpu_id));
		printf("  Model = %x", CPUID_TO_MODEL(cpu_id));
		printf("  Stepping = %u", cpu_id & CPUID_STEPPING);
		if (cpu_vendor_id == CPU_VENDOR_CYRIX)
			printf("\n  DIR=0x%04x", cyrix_did);
		if (cpu_high > 0) {

			/*
			 * Here we should probably set up flags indicating
			 * whether or not various features are available.
			 * The interesting ones are probably VME, PSE, PAE,
			 * and PGE.  The code already assumes without bothering
			 * to check that all CPUs >= Pentium have a TSC and
			 * MSRs.
			 */
			printf("\n  Features=0x%b", cpu_feature,
			"\020"
			"\001FPU"	/* Integral FPU */
			"\002VME"	/* Extended VM86 mode support */
			"\003DE"	/* Debugging Extensions (CR4.DE) */
			"\004PSE"	/* 4MByte page tables */
			"\005TSC"	/* Timestamp counter */
			"\006MSR"	/* Machine specific registers */
			"\007PAE"	/* Physical address extension */
			"\010MCE"	/* Machine Check support */
			"\011CX8"	/* CMPEXCH8 instruction */
			"\012APIC"	/* SMP local APIC */
			"\013oldMTRR"	/* Previous implementation of MTRR */
			"\014SEP"	/* Fast System Call */
			"\015MTRR"	/* Memory Type Range Registers */
			"\016PGE"	/* PG_G (global bit) support */
			"\017MCA"	/* Machine Check Architecture */
			"\020CMOV"	/* CMOV instruction */
			"\021PAT"	/* Page attributes table */
			"\022PSE36"	/* 36 bit address space support */
			"\023PN"	/* Processor Serial number */
			"\024CLFLUSH"	/* Has the CLFLUSH instruction */
			"\025<b20>"
			"\026DTS"	/* Debug Trace Store */
			"\027ACPI"	/* ACPI support */
			"\030MMX"	/* MMX instructions */
			"\031FXSR"	/* FXSAVE/FXRSTOR */
			"\032SSE"	/* Streaming SIMD Extensions */
			"\033SSE2"	/* Streaming SIMD Extensions #2 */
			"\034SS"	/* Self snoop */
			"\035HTT"	/* Hyperthreading (see EBX bit 16-23) */
			"\036TM"	/* Thermal Monitor clock slowdown */
			"\037IA64"	/* CPU can execute IA64 instructions */
			"\040PBE"	/* Pending Break Enable */
			);

			if (cpu_feature2 != 0) {
				printf("\n  Features2=0x%b", cpu_feature2,
				"\020"
				"\001SSE3"	/* SSE3 */
				"\002PCLMULQDQ"	/* Carry-Less Mul Quadword */
				"\003DTES64"	/* 64-bit Debug Trace */
				"\004MON"	/* MONITOR/MWAIT Instructions */
				"\005DS_CPL"	/* CPL Qualified Debug Store */
				"\006VMX"	/* Virtual Machine Extensions */
				"\007SMX"	/* Safer Mode Extensions */
				"\010EST"	/* Enhanced SpeedStep */
				"\011TM2"	/* Thermal Monitor 2 */
				"\012SSSE3"	/* SSSE3 */
				"\013CNXT-ID"	/* L1 context ID available */
				"\014<b11>"
				"\015<b12>"
				"\016CX16"	/* CMPXCHG16B Instruction */
				"\017xTPR"	/* Send Task Priority Messages*/
				"\020PDCM"	/* Perf/Debug Capability MSR */
				"\021<b16>"
				"\022PCID"	/* Process-context Identifiers */
				"\023DCA"	/* Direct Cache Access */
				"\024SSE4.1"
				"\025SSE4.2"
				"\026x2APIC"	/* xAPIC Extensions */
				"\027MOVBE"
				"\030POPCNT"
				"\031<b24>"
				"\032AESNI"	/* AES Crypto*/
				"\033XSAVE"
				"\034OSXSAVE"
				"\035<b28>"
				"\036<b29>"
				"\037<b30>"
				"\040<b31>"
				);
			}

			/*
			 * AMD64 Architecture Programmer's Manual Volume 3:
			 * General-Purpose and System Instructions
			 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/24594.pdf
			 *
			 * IA-32 Intel Architecture Software Developer's Manual,
			 * Volume 2A: Instruction Set Reference, A-M
			 * ftp://download.intel.com/design/Pentium4/manuals/25366617.pdf
			 */
			if (amd_feature != 0) {
				printf("\n  AMD Features=0x%b", amd_feature,
				"\020"		/* in hex */
				"\001<s0>"	/* Same */
				"\002<s1>"	/* Same */
				"\003<s2>"	/* Same */
				"\004<s3>"	/* Same */
				"\005<s4>"	/* Same */
				"\006<s5>"	/* Same */
				"\007<s6>"	/* Same */
				"\010<s7>"	/* Same */
				"\011<s8>"	/* Same */
				"\012<s9>"	/* Same */
				"\013<b10>"	/* Undefined */
				"\014SYSCALL"	/* Have SYSCALL/SYSRET */
				"\015<s12>"	/* Same */
				"\016<s13>"	/* Same */
				"\017<s14>"	/* Same */
				"\020<s15>"	/* Same */
				"\021<s16>"	/* Same */
				"\022<s17>"	/* Same */
				"\023<b18>"	/* Reserved, unknown */
				"\024MP"	/* Multiprocessor Capable */
				"\025NX"	/* Has EFER.NXE, NX */
				"\026<b21>"	/* Undefined */
				"\027MMX+"	/* AMD MMX Extensions */
				"\030<s23>"	/* Same */
				"\031<s24>"	/* Same */
				"\032FFXSR"	/* Fast FXSAVE/FXRSTOR */
				"\033Page1GB"	/* 1-GB large page support */
				"\034RDTSCP"	/* RDTSCP */
				"\035<b28>"	/* Undefined */
				"\036LM"	/* 64 bit long mode */
				"\0373DNow!+"	/* AMD 3DNow! Extensions */
				"\0403DNow!"	/* AMD 3DNow! */
				);
			}

			if (amd_feature2 != 0) {
				printf("\n  AMD Features2=0x%b", amd_feature2,
				"\020"
				"\001LAHF"	/* LAHF/SAHF in long mode */
				"\002CMP"	/* CMP legacy */
				"\003SVM"	/* Secure Virtual Mode */
				"\004ExtAPIC"	/* Extended APIC register */
				"\005CR8"	/* CR8 in legacy mode */
				"\006ABM"	/* LZCNT instruction */
				"\007SSE4A"	/* SSE4A */
				"\010MAS"	/* Misaligned SSE mode */
				"\011Prefetch"	/* 3DNow! Prefetch/PrefetchW */
				"\012OSVW"	/* OS visible workaround */
				"\013IBS"	/* Instruction based sampling */
				"\014SSE5"	/* SSE5 */
				"\015SKINIT"	/* SKINIT/STGI */
				"\016WDT"	/* Watchdog timer */
				"\017<b14>"
				"\020<b15>"
				"\021<b16>"
				"\022<b17>"
				"\023<b18>"
				"\024<b19>"
				"\025<b20>"
				"\026<b21>"
				"\027<b22>"
				"\030<b23>"
				"\031<b24>"
				"\032<b25>"
				"\033<b26>"
				"\034<b27>"
				"\035<b28>"
				"\036<b29>"
				"\037<b30>"
				"\040<b31>"
				);
			}

			if (cpu_vendor_id == CPU_VENDOR_CENTAUR)
				print_via_padlock_info();

			if ((cpu_feature & CPUID_HTT) &&
			    cpu_vendor_id == CPU_VENDOR_AMD)
				cpu_feature &= ~CPUID_HTT;

			/*
			 * If this CPU supports P-state invariant TSC then
			 * mention the capability.
			 */
			if (tsc_is_invariant)
				printf("\n  TSC: P-state invariant");

		}
	} else if (cpu_vendor_id == CPU_VENDOR_CYRIX) {
		printf("  DIR=0x%04x", cyrix_did);
		printf("  Stepping=%u", (cyrix_did & 0xf000) >> 12);
		printf("  Revision=%u", (cyrix_did & 0x0f00) >> 8);
#ifndef CYRIX_CACHE_REALLY_WORKS
		if (cpu == CPU_M1 && (cyrix_did & 0xff00) < 0x1700)
			printf("\n  CPU cache: write-through mode");
#endif
	}

	/* Avoid ugly blank lines: only print newline when we have to. */
	if (*cpu_vendor || cpu_id)
		printf("\n");

	if (!bootverbose)
		return;

	if (cpu_vendor_id == CPU_VENDOR_AMD)
		print_AMD_info();
	else if (cpu_vendor_id == CPU_VENDOR_INTEL)
		print_INTEL_info();
	else if (cpu_vendor_id == CPU_VENDOR_TRANSMETA)
		print_transmeta_info();
}

void
panicifcpuunsupported(void)
{

#if !defined(lint)
#if !defined(I486_CPU) && !defined(I586_CPU) && !defined(I686_CPU)
#error This kernel is not configured for one of the supported CPUs
#endif
#else /* lint */
#endif /* lint */
	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
	case CPUCLASS_286:	/* a 286 should not make it this far, anyway */
	case CPUCLASS_386:
#if !defined(I486_CPU)
	case CPUCLASS_486:
#endif
#if !defined(I586_CPU)
	case CPUCLASS_586:
#endif
#if !defined(I686_CPU)
	case CPUCLASS_686:
#endif
		panic("CPU class not configured");
	default:
		break;
	}
}


static	volatile u_int trap_by_rdmsr;

/*
 * Special exception 6 handler.
 * The rdmsr instruction generates invalid opcodes fault on 486-class
 * Cyrix CPU.  Stacked eip register points the rdmsr instruction in the
 * function identblue() when this handler is called.  Stacked eip should
 * be advanced.
 */
inthand_t	bluetrap6;
#ifdef __GNUCLIKE_ASM
__asm
("									\n\
	.text								\n\
	.p2align 2,0x90							\n\
	.type	" __XSTRING(CNAME(bluetrap6)) ",@function		\n\
" __XSTRING(CNAME(bluetrap6)) ":					\n\
	ss								\n\
	movl	$0xa8c1d," __XSTRING(CNAME(trap_by_rdmsr)) "		\n\
	addl	$2, (%esp)	/* rdmsr is a 2-byte instruction */	\n\
	iret								\n\
");
#endif

/*
 * Special exception 13 handler.
 * Accessing non-existent MSR generates general protection fault.
 */
inthand_t	bluetrap13;
#ifdef __GNUCLIKE_ASM
__asm
("									\n\
	.text								\n\
	.p2align 2,0x90							\n\
	.type	" __XSTRING(CNAME(bluetrap13)) ",@function		\n\
" __XSTRING(CNAME(bluetrap13)) ":					\n\
	ss								\n\
	movl	$0xa89c4," __XSTRING(CNAME(trap_by_rdmsr)) "		\n\
	popl	%eax		/* discard error code */		\n\
	addl	$2, (%esp)	/* rdmsr is a 2-byte instruction */	\n\
	iret								\n\
");
#endif

/*
 * Distinguish IBM Blue Lightning CPU from Cyrix CPUs that does not
 * support cpuid instruction.  This function should be called after
 * loading interrupt descriptor table register.
 *
 * I don't like this method that handles fault, but I couldn't get
 * information for any other methods.  Does blue giant know?
 */
static int
identblue(void)
{

	trap_by_rdmsr = 0;

	/*
	 * Cyrix 486-class CPU does not support rdmsr instruction.
	 * The rdmsr instruction generates invalid opcode fault, and exception
	 * will be trapped by bluetrap6() on Cyrix 486-class CPU.  The
	 * bluetrap6() set the magic number to trap_by_rdmsr.
	 */
	setidt(IDT_UD, bluetrap6, SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	/*
	 * Certain BIOS disables cpuid instruction of Cyrix 6x86MX CPU.
	 * In this case, rdmsr generates general protection fault, and
	 * exception will be trapped by bluetrap13().
	 */
	setidt(IDT_GP, bluetrap13, SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	rdmsr(0x1002);		/* Cyrix CPU generates fault. */

	if (trap_by_rdmsr == 0xa8c1d)
		return IDENTBLUE_CYRIX486;
	else if (trap_by_rdmsr == 0xa89c4)
		return IDENTBLUE_CYRIXM2;
	return IDENTBLUE_IBMCPU;
}


/*
 * identifycyrix() set lower 16 bits of cyrix_did as follows:
 *
 *  F E D C B A 9 8 7 6 5 4 3 2 1 0
 * +-------+-------+---------------+
 * |  SID  |  RID  |   Device ID   |
 * |    (DIR 1)    |    (DIR 0)    |
 * +-------+-------+---------------+
 */
static void
identifycyrix(void)
{
	register_t saveintr;
	int	ccr2_test = 0, dir_test = 0;
	u_char	ccr2, ccr3;

	saveintr = intr_disable();

	ccr2 = read_cyrix_reg(CCR2);
	write_cyrix_reg(CCR2, ccr2 ^ CCR2_LOCK_NW);
	read_cyrix_reg(CCR2);
	if (read_cyrix_reg(CCR2) != ccr2)
		ccr2_test = 1;
	write_cyrix_reg(CCR2, ccr2);

	ccr3 = read_cyrix_reg(CCR3);
	write_cyrix_reg(CCR3, ccr3 ^ CCR3_MAPEN3);
	read_cyrix_reg(CCR3);
	if (read_cyrix_reg(CCR3) != ccr3)
		dir_test = 1;					/* CPU supports DIRs. */
	write_cyrix_reg(CCR3, ccr3);

	if (dir_test) {
		/* Device ID registers are available. */
		cyrix_did = read_cyrix_reg(DIR1) << 8;
		cyrix_did += read_cyrix_reg(DIR0);
	} else if (ccr2_test)
		cyrix_did = 0x0010;		/* 486S A-step */
	else
		cyrix_did = 0x00ff;		/* Old 486SLC/DLC and TI486SXLC/SXL */

	intr_restore(saveintr);
}

/* Update TSC freq with the value indicated by the caller. */
static void
tsc_freq_changed(void *arg __unused, const struct cf_level *level, int status)
{

	/* If there was an error during the transition, don't do anything. */
	if (status != 0)
		return;

	/* Total setting for this level gives the new frequency in MHz. */
	hw_clockrate = level->total_set.freq;
}

static void
hook_tsc_freq(void *arg __unused)
{

	if (tsc_is_invariant)
		return;

	tsc_post_tag = EVENTHANDLER_REGISTER(cpufreq_post_change,
	    tsc_freq_changed, NULL, EVENTHANDLER_PRI_ANY);
}

SYSINIT(hook_tsc_freq, SI_SUB_CONFIGURE, SI_ORDER_ANY, hook_tsc_freq, NULL);

/*
 * Final stage of CPU identification. -- Should I check TI?
 */
void
finishidentcpu(void)
{
	int	isblue = 0;
	u_char	ccr3;
	u_int	regs[4];

	cpu_vendor_id = find_cpu_vendor_id();

	/*
	 * Clear "Limit CPUID Maxval" bit and get the largest standard CPUID
	 * function number again if it is set from BIOS.  It is necessary
	 * for probing correct CPU topology later.
	 * XXX This is only done on the BSP package.
	 */
	if (cpu_vendor_id == CPU_VENDOR_INTEL && cpu_high > 0 && cpu_high < 4 &&
	    ((CPUID_TO_FAMILY(cpu_id) == 0xf && CPUID_TO_MODEL(cpu_id) >= 0x3) ||
	    (CPUID_TO_FAMILY(cpu_id) == 0x6 && CPUID_TO_MODEL(cpu_id) >= 0xe))) {
		uint64_t msr;
		msr = rdmsr(MSR_IA32_MISC_ENABLE);
		if ((msr & 0x400000ULL) != 0) {
			wrmsr(MSR_IA32_MISC_ENABLE, msr & ~0x400000ULL);
			do_cpuid(0, regs);
			cpu_high = regs[0];
		}
	}

	/* Detect AMD features (PTE no-execute bit, 3dnow, 64 bit mode etc) */
	if (cpu_vendor_id == CPU_VENDOR_INTEL ||
	    cpu_vendor_id == CPU_VENDOR_AMD) {
		init_exthigh();
		if (cpu_exthigh >= 0x80000001) {
			do_cpuid(0x80000001, regs);
			amd_feature = regs[3] & ~(cpu_feature & 0x0183f3ff);
			amd_feature2 = regs[2];
		}
		if (cpu_exthigh >= 0x80000007) {
			do_cpuid(0x80000007, regs);
			amd_pminfo = regs[3];
		}
		if (cpu_exthigh >= 0x80000008) {
			do_cpuid(0x80000008, regs);
			cpu_procinfo2 = regs[2];
		}
	} else if (cpu_vendor_id == CPU_VENDOR_CYRIX) {
		if (cpu == CPU_486) {
			/*
			 * These conditions are equivalent to:
			 *     - CPU does not support cpuid instruction.
			 *     - Cyrix/IBM CPU is detected.
			 */
			isblue = identblue();
			if (isblue == IDENTBLUE_IBMCPU) {
				strcpy(cpu_vendor, "IBM");
				cpu_vendor_id = CPU_VENDOR_IBM;
				cpu = CPU_BLUE;
				return;
			}
		}
		switch (cpu_id & 0xf00) {
		case 0x600:
			/*
			 * Cyrix's datasheet does not describe DIRs.
			 * Therefor, I assume it does not have them
			 * and use the result of the cpuid instruction.
			 * XXX they seem to have it for now at least. -Peter
			 */
			identifycyrix();
			cpu = CPU_M2;
			break;
		default:
			identifycyrix();
			/*
			 * This routine contains a trick.
			 * Don't check (cpu_id & 0x00f0) == 0x50 to detect M2, now.
			 */
			switch (cyrix_did & 0x00f0) {
			case 0x00:
			case 0xf0:
				cpu = CPU_486DLC;
				break;
			case 0x10:
				cpu = CPU_CY486DX;
				break;
			case 0x20:
				if ((cyrix_did & 0x000f) < 8)
					cpu = CPU_M1;
				else
					cpu = CPU_M1SC;
				break;
			case 0x30:
				cpu = CPU_M1;
				break;
			case 0x40:
				/* MediaGX CPU */
				cpu = CPU_M1SC;
				break;
			default:
				/* M2 and later CPUs are treated as M2. */
				cpu = CPU_M2;

				/*
				 * enable cpuid instruction.
				 */
				ccr3 = read_cyrix_reg(CCR3);
				write_cyrix_reg(CCR3, CCR3_MAPEN0);
				write_cyrix_reg(CCR4, read_cyrix_reg(CCR4) | CCR4_CPUID);
				write_cyrix_reg(CCR3, ccr3);

				do_cpuid(0, regs);
				cpu_high = regs[0];	/* eax */
				do_cpuid(1, regs);
				cpu_id = regs[0];	/* eax */
				cpu_feature = regs[3];	/* edx */
				break;
			}
		}
	} else if (cpu == CPU_486 && *cpu_vendor == '\0') {
		/*
		 * There are BlueLightning CPUs that do not change
		 * undefined flags by dividing 5 by 2.  In this case,
		 * the CPU identification routine in locore.s leaves
		 * cpu_vendor null string and puts CPU_486 into the
		 * cpu.
		 */
		isblue = identblue();
		if (isblue == IDENTBLUE_IBMCPU) {
			strcpy(cpu_vendor, "IBM");
			cpu_vendor_id = CPU_VENDOR_IBM;
			cpu = CPU_BLUE;
			return;
		}
	}
}

static u_int
find_cpu_vendor_id(void)
{
	int	i;

	for (i = 0; i < sizeof(cpu_vendors) / sizeof(cpu_vendors[0]); i++)
		if (strcmp(cpu_vendor, cpu_vendors[i].vendor) == 0)
			return (cpu_vendors[i].vendor_id);
	return (0);
}

static void
print_AMD_assoc(int i)
{
	if (i == 255)
		printf(", fully associative\n");
	else
		printf(", %d-way associative\n", i);
}

static void
print_AMD_info(void)
{
	quad_t amd_whcr;

	if (cpu_exthigh >= 0x80000005) {
		u_int regs[4];

		do_cpuid(0x80000005, regs);
		printf("Data TLB: %d entries", (regs[1] >> 16) & 0xff);
		print_AMD_assoc(regs[1] >> 24);
		printf("Instruction TLB: %d entries", regs[1] & 0xff);
		print_AMD_assoc((regs[1] >> 8) & 0xff);
		printf("L1 data cache: %d kbytes", regs[2] >> 24);
		printf(", %d bytes/line", regs[2] & 0xff);
		printf(", %d lines/tag", (regs[2] >> 8) & 0xff);
		print_AMD_assoc((regs[2] >> 16) & 0xff);
		printf("L1 instruction cache: %d kbytes", regs[3] >> 24);
		printf(", %d bytes/line", regs[3] & 0xff);
		printf(", %d lines/tag", (regs[3] >> 8) & 0xff);
		print_AMD_assoc((regs[3] >> 16) & 0xff);
		if (cpu_exthigh >= 0x80000006) {	/* K6-III only */
			do_cpuid(0x80000006, regs);
			printf("L2 internal cache: %d kbytes", regs[2] >> 16);
			printf(", %d bytes/line", regs[2] & 0xff);
			printf(", %d lines/tag", (regs[2] >> 8) & 0x0f);
			print_AMD_assoc((regs[2] >> 12) & 0x0f);	
		}
	}
	if (((cpu_id & 0xf00) == 0x500)
	    && (((cpu_id & 0x0f0) > 0x80)
		|| (((cpu_id & 0x0f0) == 0x80)
		    && (cpu_id & 0x00f) > 0x07))) {
		/* K6-2(new core [Stepping 8-F]), K6-III or later */
		amd_whcr = rdmsr(0xc0000082);
		if (!(amd_whcr & (0x3ff << 22))) {
			printf("Write Allocate Disable\n");
		} else {
			printf("Write Allocate Enable Limit: %dM bytes\n",
			    (u_int32_t)((amd_whcr & (0x3ff << 22)) >> 22) * 4);
			printf("Write Allocate 15-16M bytes: %s\n",
			    (amd_whcr & (1 << 16)) ? "Enable" : "Disable");
		}
	} else if (((cpu_id & 0xf00) == 0x500)
		   && ((cpu_id & 0x0f0) > 0x50)) {
		/* K6, K6-2(old core) */
		amd_whcr = rdmsr(0xc0000082);
		if (!(amd_whcr & (0x7f << 1))) {
			printf("Write Allocate Disable\n");
		} else {
			printf("Write Allocate Enable Limit: %dM bytes\n",
			    (u_int32_t)((amd_whcr & (0x7f << 1)) >> 1) * 4);
			printf("Write Allocate 15-16M bytes: %s\n",
			    (amd_whcr & 0x0001) ? "Enable" : "Disable");
			printf("Hardware Write Allocate Control: %s\n",
			    (amd_whcr & 0x0100) ? "Enable" : "Disable");
		}
	}

	/*
	 * Opteron Rev E shows a bug as in very rare occasions a read memory
	 * barrier is not performed as expected if it is followed by a
	 * non-atomic read-modify-write instruction.
	 * As long as that bug pops up very rarely (intensive machine usage
	 * on other operating systems generally generates one unexplainable
	 * crash any 2 months) and as long as a model specific fix would be
	 * impratical at this stage, print out a warning string if the broken
	 * model and family are identified.
	 */
	if (CPUID_TO_FAMILY(cpu_id) == 0xf && CPUID_TO_MODEL(cpu_id) >= 0x20 &&
	    CPUID_TO_MODEL(cpu_id) <= 0x3f)
		printf("WARNING: This architecture revision has known SMP "
		    "hardware bugs which may cause random instability\n");
}

static void
print_INTEL_info(void)
{
	u_int regs[4];
	u_int rounds, regnum;
	u_int nwaycode, nway;

	if (cpu_high >= 2) {
		rounds = 0;
		do {
			do_cpuid(0x2, regs);
			if (rounds == 0 && (rounds = (regs[0] & 0xff)) == 0)
				break;	/* we have a buggy CPU */

			for (regnum = 0; regnum <= 3; ++regnum) {
				if (regs[regnum] & (1<<31))
					continue;
				if (regnum != 0)
					print_INTEL_TLB(regs[regnum] & 0xff);
				print_INTEL_TLB((regs[regnum] >> 8) & 0xff);
				print_INTEL_TLB((regs[regnum] >> 16) & 0xff);
				print_INTEL_TLB((regs[regnum] >> 24) & 0xff);
			}
		} while (--rounds > 0);
	}

	if (cpu_exthigh >= 0x80000006) {
		do_cpuid(0x80000006, regs);
		nwaycode = (regs[2] >> 12) & 0x0f;
		if (nwaycode >= 0x02 && nwaycode <= 0x08)
			nway = 1 << (nwaycode / 2);
		else
			nway = 0;
		printf("\nL2 cache: %u kbytes, %u-way associative, %u bytes/line",
		    (regs[2] >> 16) & 0xffff, nway, regs[2] & 0xff);
	}

	printf("\n");
}

static void
print_INTEL_TLB(u_int data)
{
	switch (data) {
	case 0x0:
	case 0x40:
	default:
		break;
	case 0x1:
		printf("\nInstruction TLB: 4 KB pages, 4-way set associative, 32 entries");
		break;
	case 0x2:
		printf("\nInstruction TLB: 4 MB pages, fully associative, 2 entries");
		break;
	case 0x3:
		printf("\nData TLB: 4 KB pages, 4-way set associative, 64 entries");
		break;
	case 0x4:
		printf("\nData TLB: 4 MB Pages, 4-way set associative, 8 entries");
		break;
	case 0x6:
		printf("\n1st-level instruction cache: 8 KB, 4-way set associative, 32 byte line size");
		break;
	case 0x8:
		printf("\n1st-level instruction cache: 16 KB, 4-way set associative, 32 byte line size");
		break;
	case 0xa:
		printf("\n1st-level data cache: 8 KB, 2-way set associative, 32 byte line size");
		break;
	case 0xc:
		printf("\n1st-level data cache: 16 KB, 4-way set associative, 32 byte line size");
		break;
	case 0x22:
		printf("\n3rd-level cache: 512 KB, 4-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x23:
		printf("\n3rd-level cache: 1 MB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x25:
		printf("\n3rd-level cache: 2 MB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x29:
		printf("\n3rd-level cache: 4 MB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x2c:
		printf("\n1st-level data cache: 32 KB, 8-way set associative, 64 byte line size");
		break;
	case 0x30:
		printf("\n1st-level instruction cache: 32 KB, 8-way set associative, 64 byte line size");
		break;
	case 0x39:
		printf("\n2nd-level cache: 128 KB, 4-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x3b:
		printf("\n2nd-level cache: 128 KB, 2-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x3c:
		printf("\n2nd-level cache: 256 KB, 4-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x41:
		printf("\n2nd-level cache: 128 KB, 4-way set associative, 32 byte line size");
		break;
	case 0x42:
		printf("\n2nd-level cache: 256 KB, 4-way set associative, 32 byte line size");
		break;
	case 0x43:
		printf("\n2nd-level cache: 512 KB, 4-way set associative, 32 byte line size");
		break;
	case 0x44:
		printf("\n2nd-level cache: 1 MB, 4-way set associative, 32 byte line size");
		break;
	case 0x45:
		printf("\n2nd-level cache: 2 MB, 4-way set associative, 32 byte line size");
		break;
	case 0x46:
		printf("\n3rd-level cache: 4 MB, 4-way set associative, 64 byte line size");
		break;
	case 0x47:
		printf("\n3rd-level cache: 8 MB, 8-way set associative, 64 byte line size");
		break;
	case 0x50:
		printf("\nInstruction TLB: 4 KB, 2 MB or 4 MB pages, fully associative, 64 entries");
		break;
	case 0x51:
		printf("\nInstruction TLB: 4 KB, 2 MB or 4 MB pages, fully associative, 128 entries");
		break;
	case 0x52:
		printf("\nInstruction TLB: 4 KB, 2 MB or 4 MB pages, fully associative, 256 entries");
		break;
	case 0x5b:
		printf("\nData TLB: 4 KB or 4 MB pages, fully associative, 64 entries");
		break;
	case 0x5c:
		printf("\nData TLB: 4 KB or 4 MB pages, fully associative, 128 entries");
		break;
	case 0x5d:
		printf("\nData TLB: 4 KB or 4 MB pages, fully associative, 256 entries");
		break;
	case 0x60:
		printf("\n1st-level data cache: 16 KB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x66:
		printf("\n1st-level data cache: 8 KB, 4-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x67:
		printf("\n1st-level data cache: 16 KB, 4-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x68:
		printf("\n1st-level data cache: 32 KB, 4 way set associative, sectored cache, 64 byte line size");
		break;
	case 0x70:
		printf("\nTrace cache: 12K-uops, 8-way set associative");
		break;
	case 0x71:
		printf("\nTrace cache: 16K-uops, 8-way set associative");
		break;
	case 0x72:
		printf("\nTrace cache: 32K-uops, 8-way set associative");
		break;
	case 0x78:
		printf("\n2nd-level cache: 1 MB, 4-way set associative, 64-byte line size");
		break;
	case 0x79:
		printf("\n2nd-level cache: 128 KB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x7a:
		printf("\n2nd-level cache: 256 KB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x7b:
		printf("\n2nd-level cache: 512 KB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x7c:
		printf("\n2nd-level cache: 1 MB, 8-way set associative, sectored cache, 64 byte line size");
		break;
	case 0x7d:
		printf("\n2nd-level cache: 2-MB, 8-way set associative, 64-byte line size");
		break;
	case 0x7f:
		printf("\n2nd-level cache: 512-KB, 2-way set associative, 64-byte line size");
		break;
	case 0x82:
		printf("\n2nd-level cache: 256 KB, 8-way set associative, 32 byte line size");
		break;
	case 0x83:
		printf("\n2nd-level cache: 512 KB, 8-way set associative, 32 byte line size");
		break;
	case 0x84:
		printf("\n2nd-level cache: 1 MB, 8-way set associative, 32 byte line size");
		break;
	case 0x85:
		printf("\n2nd-level cache: 2 MB, 8-way set associative, 32 byte line size");
		break;
	case 0x86:
		printf("\n2nd-level cache: 512 KB, 4-way set associative, 64 byte line size");
		break;
	case 0x87:
		printf("\n2nd-level cache: 1 MB, 8-way set associative, 64 byte line size");
		break;
	case 0xb0:
		printf("\nInstruction TLB: 4 KB Pages, 4-way set associative, 128 entries");
		break;
	case 0xb3:
		printf("\nData TLB: 4 KB Pages, 4-way set associative, 128 entries");
		break;
	}
}

static void
print_transmeta_info(void)
{
	u_int regs[4], nreg = 0;

	do_cpuid(0x80860000, regs);
	nreg = regs[0];
	if (nreg >= 0x80860001) {
		do_cpuid(0x80860001, regs);
		printf("  Processor revision %u.%u.%u.%u\n",
		       (regs[1] >> 24) & 0xff,
		       (regs[1] >> 16) & 0xff,
		       (regs[1] >> 8) & 0xff,
		       regs[1] & 0xff);
	}
	if (nreg >= 0x80860002) {
		do_cpuid(0x80860002, regs);
		printf("  Code Morphing Software revision %u.%u.%u-%u-%u\n",
		       (regs[1] >> 24) & 0xff,
		       (regs[1] >> 16) & 0xff,
		       (regs[1] >> 8) & 0xff,
		       regs[1] & 0xff,
		       regs[2]);
	}
	if (nreg >= 0x80860006) {
		char info[65];
		do_cpuid(0x80860003, (u_int*) &info[0]);
		do_cpuid(0x80860004, (u_int*) &info[16]);
		do_cpuid(0x80860005, (u_int*) &info[32]);
		do_cpuid(0x80860006, (u_int*) &info[48]);
		info[64] = 0;
		printf("  %s\n", info);
	}
}

static void
print_via_padlock_info(void)
{
	u_int regs[4];

	/* Check for supported models. */
	switch (cpu_id & 0xff0) {
	case 0x690:
		if ((cpu_id & 0xf) < 3)
			return;
	case 0x6a0:
	case 0x6d0:
	case 0x6f0:
		break;
	default:
		return;
	}
	
	do_cpuid(0xc0000000, regs);
	if (regs[0] >= 0xc0000001)
		do_cpuid(0xc0000001, regs);
	else
		return;

	printf("\n  VIA Padlock Features=0x%b", regs[3],
	"\020"
	"\003RNG"		/* RNG */
	"\007AES"		/* ACE */
	"\011AES-CTR"		/* ACE2 */
	"\013SHA1,SHA256"	/* PHE */
	"\015RSA"		/* PMM */
	);
}
