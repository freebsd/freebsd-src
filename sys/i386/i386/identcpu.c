/*
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * Copyright (c) 1997 KATO Takenori.
 * Copyright (c) 2001 Tamotsu Hattori.
 * Copyright (c) 2001 Mitsuru IWASAKI.
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
 * $FreeBSD$
 */

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/asmacros.h>
#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/md_var.h>

#include <i386/isa/intr_machdep.h>

#define	IDENTBLUE_CYRIX486	0
#define	IDENTBLUE_IBMCPU	1
#define	IDENTBLUE_CYRIXM2	2

/* XXX - should be in header file: */
void printcpuinfo(void);
void finishidentcpu(void);
#if defined(I586_CPU) && defined(CPU_WT_ALLOC)
void	enable_K5_wt_alloc(void);
void	enable_K6_wt_alloc(void);
void	enable_K6_2_wt_alloc(void);
#endif
void panicifcpuunsupported(void);

static void identifycyrix(void);
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
static void print_AMD_features(void);
#endif
static void print_AMD_info(void);
static void print_AMD_assoc(int i);
static void print_transmeta_info(void);
static void setup_tmx86_longrun(void);

int	cpu_class = CPUCLASS_386;
u_int	cpu_exthigh;		/* Highest arg to extended CPUID */
u_int	cyrix_did;		/* Device ID of Cyrix CPU */
char machine[] = "i386";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, 
    machine, 0, "Machine class");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, 
    cpu_model, 0, "Machine model");

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

static struct cpu_nameclass i386_cpus[] = {
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

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
int has_f00f_bug = 0;		/* Initialized so that it can be patched. */
#endif

void
printcpuinfo(void)
{
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	u_int regs[4], i;
#endif
	char *brand;

	cpu_class = i386_cpus[cpu].cpu_class;
	printf("CPU: ");
	strncpy(cpu_model, i386_cpus[cpu].cpu_name, sizeof (cpu_model));

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	/* Check for extended CPUID information and a processor name. */
	if (cpu_high > 0 &&
	    (strcmp(cpu_vendor, "GenuineIntel") == 0 ||
	    strcmp(cpu_vendor, "AuthenticAMD") == 0 ||
	    strcmp(cpu_vendor, "GenuineTMx86") == 0 ||
	    strcmp(cpu_vendor, "TransmetaCPU") == 0)) {
		do_cpuid(0x80000000, regs);
		if (regs[0] >= 0x80000000) {
			cpu_exthigh = regs[0];
			if (cpu_exthigh >= 0x80000004) {
				brand = cpu_brand;
				for (i = 0x80000002; i < 0x80000005; i++) {
					do_cpuid(i, regs);
					memcpy(brand, regs, sizeof(regs));
					brand += sizeof(regs);
				}
			}
		}
	}

	if (strcmp(cpu_vendor, "GenuineIntel") == 0) {
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
				        strcat(cpu_model, "/P54T Overdrive");
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
	} else if (strcmp(cpu_vendor, "AuthenticAMD") == 0) {
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
	} else if (strcmp(cpu_vendor, "CyrixInstead") == 0) {
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
	} else if (strcmp(cpu_vendor, "RiseRiseRise") == 0) {
		strcpy(cpu_model, "Rise ");
		switch (cpu_id & 0xff0) {
		case 0x500:
			strcat(cpu_model, "mP6");
			break;
		default:
			strcat(cpu_model, "Unknown");
		}
	} else if (strcmp(cpu_vendor, "CentaurHauls") == 0) {
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
		default:
			strcpy(cpu_model, "VIA/IDT Unknown");
		}
	} else if (strcmp(cpu_vendor, "IBM") == 0) {
		strcpy(cpu_model, "Blue Lightning CPU");
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

#endif

	printf("%s (", cpu_model);
	switch(cpu_class) {
	case CPUCLASS_286:
		printf("286");
		break;
#if defined(I386_CPU)
	case CPUCLASS_386:
		printf("386");
		break;
#endif
#if defined(I486_CPU)
	case CPUCLASS_486:
		printf("486");
		bzero = i486_bzero;
		break;
#endif
#if defined(I586_CPU)
	case CPUCLASS_586:
		printf("%d.%02d-MHz ",
		       (tsc_freq + 4999) / 1000000,
		       ((tsc_freq + 4999) / 10000) % 100);
		printf("586");
		break;
#endif
#if defined(I686_CPU)
	case CPUCLASS_686:
		printf("%d.%02d-MHz ",
		       (tsc_freq + 4999) / 1000000,
		       ((tsc_freq + 4999) / 10000) % 100);
		printf("686");
		break;
#endif
	default:
		printf("Unknown");	/* will panic below... */
	}
	printf("-class CPU)\n");
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	if(*cpu_vendor)
		printf("  Origin = \"%s\"",cpu_vendor);
	if(cpu_id)
		printf("  Id = 0x%x", cpu_id);

	if (strcmp(cpu_vendor, "GenuineIntel") == 0 ||
	    strcmp(cpu_vendor, "AuthenticAMD") == 0 ||
	    strcmp(cpu_vendor, "RiseRiseRise") == 0 ||
	    strcmp(cpu_vendor, "CentaurHauls") == 0 ||
		((strcmp(cpu_vendor, "CyrixInstead") == 0) &&
		 ((cpu_id & 0xf00) > 0x500))) {
		printf("  Stepping = %u", cpu_id & 0xf);
		if (strcmp(cpu_vendor, "CyrixInstead") == 0)
			printf("  DIR=0x%04x", cyrix_did);
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

			/*
			 * If this CPU supports hyperthreading then mention
			 * the number of logical CPU's it contains.
			 */
			if (cpu_feature & CPUID_HTT &&
			    (cpu_procinfo & CPUID_HTT_CORES) >> 16 > 1)
				printf("\n  Hyperthreading: %d logical CPUs",
				    (cpu_procinfo & CPUID_HTT_CORES) >> 16);
		}
		if (strcmp(cpu_vendor, "AuthenticAMD") == 0 &&
		    cpu_exthigh >= 0x80000001)
			print_AMD_features();
	} else if (strcmp(cpu_vendor, "CyrixInstead") == 0) {
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

#endif
	if (strcmp(cpu_vendor, "GenuineTMx86") == 0 ||
	    strcmp(cpu_vendor, "TransmetaCPU") == 0) {
		setup_tmx86_longrun();
	}

	if (!bootverbose)
		return;

	if (strcmp(cpu_vendor, "AuthenticAMD") == 0)
		print_AMD_info();
	else if (strcmp(cpu_vendor, "GenuineTMx86") == 0 ||
		 strcmp(cpu_vendor, "TransmetaCPU") == 0)
		print_transmeta_info();

#ifdef I686_CPU
	/*
	 * XXX - Do PPro CPUID level=2 stuff here?
	 *
	 * No, but maybe in a print_Intel_info() function called from here.
	 */
#endif
}

void
panicifcpuunsupported(void)
{

#if !defined(I386_CPU) && !defined(I486_CPU) && !defined(I586_CPU) && !defined(I686_CPU)
#error This kernel is not configured for one of the supported CPUs
#endif
	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
	case CPUCLASS_286:	/* a 286 should not make it this far, anyway */
#if !defined(I386_CPU)
	case CPUCLASS_386:
#endif
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

/*
 * Special exception 13 handler.
 * Accessing non-existent MSR generates general protection fault.
 */
inthand_t	bluetrap13;
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
	setidt(6, bluetrap6, SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/*
	 * Certain BIOS disables cpuid instruction of Cyrix 6x86MX CPU.
	 * In this case, rdmsr generates general protection fault, and
	 * exception will be trapped by bluetrap13().
	 */
	setidt(13, bluetrap13, SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

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
	u_int	eflags;
	int	ccr2_test = 0, dir_test = 0;
	u_char	ccr2, ccr3;

	eflags = read_eflags();
	disable_intr();

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

	write_eflags(eflags);
}

/*
 * Final stage of CPU identification. -- Should I check TI?
 */
void
finishidentcpu(void)
{
	int	isblue = 0;
	u_char	ccr3;
	u_int	regs[4];

	if (strcmp(cpu_vendor, "CyrixInstead") == 0) {
		if (cpu == CPU_486) {
			/*
			 * These conditions are equivalent to:
			 *     - CPU does not support cpuid instruction.
			 *     - Cyrix/IBM CPU is detected.
			 */
			isblue = identblue();
			if (isblue == IDENTBLUE_IBMCPU) {
				strcpy(cpu_vendor, "IBM");
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
			cpu = CPU_BLUE;
			return;
		}
	}
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
}

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
static void
print_AMD_features(void)
{
	u_int regs[4];

	/*
	 * Values taken from AMD Processor Recognition
	 * http://www.amd.com/products/cpg/athlon/techdocs/pdf/20734.pdf
	 */
	do_cpuid(0x80000001, regs);
	printf("\n  AMD Features=0x%b", regs[3] &~ cpu_feature,
		"\020"		/* in hex */
		"\001FPU"	/* Integral FPU */
		"\002VME"	/* Extended VM86 mode support */
		"\003DE"	/* Debug extensions */
		"\004PSE"	/* 4MByte page tables */
		"\005TSC"	/* Timestamp counter */
		"\006MSR"	/* Machine specific registers */
		"\007PAE"	/* Physical address extension */
		"\010MCE"	/* Machine Check support */
		"\011CX8"	/* CMPEXCH8 instruction */
		"\012APIC"	/* SMP local APIC */
		"\013<b10>"
		"\014SYSCALL"	/* SYSENTER/SYSEXIT instructions */
		"\015MTRR"	/* Memory Type Range Registers */
		"\016PGE"	/* PG_G (global bit) support */
		"\017MCA"	/* Machine Check Architecture */
		"\020ICMOV"	/* CMOV instruction */
		"\021PAT"	/* Page attributes table */
		"\022PGE36"	/* 36 bit address space support */
		"\023RSVD"	/* Reserved, unknown */
		"\024MP"	/* Multiprocessor Capable */
		"\025<b20>"
		"\026<b21>"
		"\027AMIE"	/* AMD MMX Instruction Extensions */
		"\030MMX"
		"\031FXSAVE"	/* FXSAVE/FXRSTOR */
		"\032<b25>"
		"\033<b26>"
		"\034<b27>"
		"\035<b28>"
		"\036<b29>"
		"\037DSP"	/* AMD 3DNow! Instruction Extensions */
		"\0403DNow!"
		);
}
#endif

/*
 * Transmeta Crusoe LongRun Support by Tamotsu Hattori. 
 */

#define MSR_TMx86_LONGRUN		0x80868010
#define MSR_TMx86_LONGRUN_FLAGS		0x80868011

#define LONGRUN_MODE_MASK(x)		((x) & 0x000000007f)
#define LONGRUN_MODE_RESERVED(x)	((x) & 0xffffff80)
#define LONGRUN_MODE_WRITE(x, y)	(LONGRUN_MODE_RESERVED(x) | LONGRUN_MODE_MASK(y))

#define LONGRUN_MODE_MINFREQUENCY	0x00
#define LONGRUN_MODE_ECONOMY		0x01
#define LONGRUN_MODE_PERFORMANCE	0x02
#define LONGRUN_MODE_MAXFREQUENCY	0x03
#define LONGRUN_MODE_UNKNOWN		0x04
#define LONGRUN_MODE_MAX		0x04

union msrinfo {
	u_int64_t	msr;
	u_int32_t	regs[2];
};

u_int32_t longrun_modes[LONGRUN_MODE_MAX][3] = {
	/*  MSR low, MSR high, flags bit0 */
	{	  0,	  0,		0},	/* LONGRUN_MODE_MINFREQUENCY */
	{	  0,	100,		0},	/* LONGRUN_MODE_ECONOMY */
	{	  0,	100,		1},	/* LONGRUN_MODE_PERFORMANCE */
	{	100,	100,		1},	/* LONGRUN_MODE_MAXFREQUENCY */
};

static u_int 
tmx86_get_longrun_mode(void)
{
	u_long		eflags;
	union msrinfo	msrinfo;
	u_int		low, high, flags, mode;

	eflags = read_eflags();
	disable_intr();

	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN);
	low = LONGRUN_MODE_MASK(msrinfo.regs[0]);
	high = LONGRUN_MODE_MASK(msrinfo.regs[1]);
	flags = rdmsr(MSR_TMx86_LONGRUN_FLAGS) & 0x01;

	for (mode = 0; mode < LONGRUN_MODE_MAX; mode++) {
		if (low   == longrun_modes[mode][0] &&
		    high  == longrun_modes[mode][1] &&
		    flags == longrun_modes[mode][2]) {
			goto out;
		}
	}
	mode = LONGRUN_MODE_UNKNOWN;
out:
	write_eflags(eflags);
	return (mode);
}

static u_int 
tmx86_get_longrun_status(u_int * frequency, u_int * voltage, u_int * percentage)
{
	u_long		eflags;
	u_int		regs[4];

	eflags = read_eflags();
	disable_intr();

	do_cpuid(0x80860007, regs);
	*frequency = regs[0];
	*voltage = regs[1];
	*percentage = regs[2];

	write_eflags(eflags);
	return (1);
}

static u_int 
tmx86_set_longrun_mode(u_int mode)
{
	u_long		eflags;
	union msrinfo	msrinfo;

	if (mode >= LONGRUN_MODE_UNKNOWN) {
		return (0);
	}

	eflags = read_eflags();
	disable_intr();

	/* Write LongRun mode values to Model Specific Register. */
	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN);
	msrinfo.regs[0] = LONGRUN_MODE_WRITE(msrinfo.regs[0],
					     longrun_modes[mode][0]);
	msrinfo.regs[1] = LONGRUN_MODE_WRITE(msrinfo.regs[1],
					     longrun_modes[mode][1]);
	wrmsr(MSR_TMx86_LONGRUN, msrinfo.msr);

	/* Write LongRun mode flags to Model Specific Register. */
	msrinfo.msr = rdmsr(MSR_TMx86_LONGRUN_FLAGS);
	msrinfo.regs[0] = (msrinfo.regs[0] & ~0x01) | longrun_modes[mode][2];
	wrmsr(MSR_TMx86_LONGRUN_FLAGS, msrinfo.msr);

	write_eflags(eflags);
	return (1);
}

static u_int			 crusoe_longrun;
static u_int			 crusoe_frequency;
static u_int	 		 crusoe_voltage;
static u_int	 		 crusoe_percentage;
static struct sysctl_ctx_list	 crusoe_sysctl_ctx;
static struct sysctl_oid	*crusoe_sysctl_tree;

static int
tmx86_longrun_sysctl(SYSCTL_HANDLER_ARGS)
{
	u_int	mode;
	int	error;

	crusoe_longrun = tmx86_get_longrun_mode();
	mode = crusoe_longrun;
	error = sysctl_handle_int(oidp, &mode, 0, req);
	if (error || !req->newptr) {
		return (error);
	}
	if (mode >= LONGRUN_MODE_UNKNOWN) {
		error = EINVAL;
		return (error);
	}
	if (crusoe_longrun != mode) {
		crusoe_longrun = mode;
		tmx86_set_longrun_mode(crusoe_longrun);
	}

	return (error);
}

static int
tmx86_status_sysctl(SYSCTL_HANDLER_ARGS)
{
	u_int	val;
	int	error;

	tmx86_get_longrun_status(&crusoe_frequency,
				 &crusoe_voltage, &crusoe_percentage);
	val = *(u_int *)oidp->oid_arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	return (error);
}

static void
setup_tmx86_longrun(void)
{
	static int	done = 0;

	if (done)
		return;
	done++;

	sysctl_ctx_init(&crusoe_sysctl_ctx);
	crusoe_sysctl_tree = SYSCTL_ADD_NODE(&crusoe_sysctl_ctx,
				SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
				"crusoe", CTLFLAG_RD, 0,
				"Transmeta Crusoe LongRun support");
	SYSCTL_ADD_PROC(&crusoe_sysctl_ctx, SYSCTL_CHILDREN(crusoe_sysctl_tree),
		OID_AUTO, "longrun", CTLTYPE_INT | CTLFLAG_RW,
		&crusoe_longrun, 0, tmx86_longrun_sysctl, "I",
		"LongRun mode [0-3]");
	SYSCTL_ADD_PROC(&crusoe_sysctl_ctx, SYSCTL_CHILDREN(crusoe_sysctl_tree),
		OID_AUTO, "frequency", CTLTYPE_INT | CTLFLAG_RD,
		&crusoe_frequency, 0, tmx86_status_sysctl, "I",
		"Current frequency (MHz)");
	SYSCTL_ADD_PROC(&crusoe_sysctl_ctx, SYSCTL_CHILDREN(crusoe_sysctl_tree),
		OID_AUTO, "voltage", CTLTYPE_INT | CTLFLAG_RD,
		&crusoe_voltage, 0, tmx86_status_sysctl, "I",
		"Current voltage (mV)");
	SYSCTL_ADD_PROC(&crusoe_sysctl_ctx, SYSCTL_CHILDREN(crusoe_sysctl_tree),
		OID_AUTO, "percentage", CTLTYPE_INT | CTLFLAG_RD,
		&crusoe_percentage, 0, tmx86_status_sysctl, "I",
		"Processing performance (%)");
}

static void
print_transmeta_info()
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

	crusoe_longrun = tmx86_get_longrun_mode();
	tmx86_get_longrun_status(&crusoe_frequency,
				 &crusoe_voltage, &crusoe_percentage);
	printf("  LongRun mode: %d  <%dMHz %dmV %d%%>\n", crusoe_longrun,
	       crusoe_frequency, crusoe_voltage, crusoe_percentage);
}

