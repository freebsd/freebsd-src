/*
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
 *	$Id: identcpu.c,v 1.7.2.6 1997/06/24 10:03:11 kato Exp $
 */

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/clock.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/md_var.h>

#include <i386/isa/isa_device.h>

/* XXX - should be in header file */
void	i486_bzero __P((void *buf, size_t len));

void printcpuinfo(void);		/* XXX should be in different header file */
void finishidentcpu(void);
void earlysetcpuclass(void);
void panicifcpuunsupported(void);
static void identifycyrix(void);

u_long	cyrix_did;		/* Device ID of Cyrix CPU */
int cpu_class = CPUCLASS_386;	/* least common denominator */
char machine[] = "i386";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static char cpu_model[128];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD, cpu_model, 0, "");

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
};

void
printcpuinfo(void)
{

	cpu_class = i386_cpus[cpu].cpu_class;
	printf("CPU: ");
	strncpy(cpu_model, i386_cpus[cpu].cpu_name, sizeof cpu_model);

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	if (strcmp(cpu_vendor,"GenuineIntel") == 0) {
		if ((cpu_id & 0xf00) > 3) {
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
				break;
			case 0x500:
				strcat(cpu_model, "Pentium"); /* nb no space */
				break;
			case 0x600:
				strcat(cpu_model, "Pentium Pro");
				break;
			default:
				strcat(cpu_model, "unknown");
				break;
			}

			switch (cpu_id & 0xff0) {
			case 0x400:
				strcat(cpu_model, "DX"); break;
			case 0x410:
				strcat(cpu_model, "DX"); break;
			case 0x420:
				strcat(cpu_model, "SX"); break;
			case 0x430:
				strcat(cpu_model, "DX2"); break;
			case 0x440:
				strcat(cpu_model, "SL"); break;
			case 0x450:
				strcat(cpu_model, "SX2"); break;
			case 0x470:
				strcat(cpu_model, "DX2 Write-Back Enhanced");
				break;
			case 0x480:
				strcat(cpu_model, "DX4"); break;
				break;
			}
		}
	} else if (strcmp(cpu_vendor,"AuthenticAMD") == 0) {
		/*
		 * Values taken from AMD Processor Recognition
		 * http://www.amd.com/html/products/pcd/techdocs/appnotes/20734c.pdf
		 */
		strcpy(cpu_model, "AMD ");
		switch (cpu_id & 0xFF0) {
		case 0x410:
			strcat(cpu_model, "Standard Am486DX");
			break;
		case 0x430:
			strcat(cpu_model, "Am486DX2/4 Write-Through");
			break;
		case 0x470:
			strcat(cpu_model, "Enhanced Am486DX4 Write-Back");
			break;
		case 0x480:
			strcat(cpu_model, "Enhanced Am486DX4 Write-Through");
			break;
		case 0x490:
			strcat(cpu_model, "Enhanced Am486DX4 Write-Back");
			break;
		case 0x4E0:
			strcat(cpu_model, "Am5x86 Write-Through");
			break;
		case 0x4F0:
			strcat(cpu_model, "Am5x86 Write-Back");
			break;
		case 0x500:
			strcat(cpu_model, "K5 model 0");
			break;
		case 0x510:
			strcat(cpu_model, "K5 model 1");
			break;
		case 0x520:
			strcat(cpu_model, "K5 PR166");
			break;
		case 0x530:
			strcat(cpu_model, "K5 PR200");
			break;
		case 0x560:
			strcat(cpu_model, "K6");
			break;
		default:
			strcat(cpu_model, "Unknown");
			break;
		}
	} else if (strcmp(cpu_vendor,"CyrixInstead") == 0) {
		strcpy(cpu_model, "Cyrix ");
		switch (cpu_id & 0xf00) {
		case 0x500:
			strcat(cpu_model, "6x86");
			break;
		case 0x600:
			strcat(cpu_model, "6x86MX");
			break;
		default:
			/* cpuid instruction is not supported */
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
				/* XXX */
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
	} else if (strcmp(cpu_vendor,"IBM") == 0)
		strcpy(cpu_model, "Blue Lightning CPU");
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
		       (i586_ctr_freq + 4999) / 1000000,
		       ((i586_ctr_freq + 4999) / 10000) % 100);
		printf("586");
		break;
#endif
#if defined(I686_CPU)
	case CPUCLASS_686:
		printf("%d.%02d-MHz ",
		       (i586_ctr_freq + 4999) / 1000000,
		       ((i586_ctr_freq + 4999) / 10000) % 100);
		printf("686");
		break;
#endif
	default:
		printf("unknown");	/* will panic below... */
	}
	printf("-class CPU)\n");
#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	if(*cpu_vendor)
		printf("  Origin = \"%s\"",cpu_vendor);
	if(cpu_id)
		printf("  Id = 0x%lx",cpu_id);

	if (strcmp(cpu_vendor, "GenuineIntel") == 0 ||
	    strcmp(cpu_vendor, "AuthenticAMD") == 0 ||
		((strcmp(cpu_vendor, "CyrixInstead") == 0) &&
		 (cpu_id & 0xf00 > 5))) {
		printf("  Stepping=%ld", cpu_id & 0xf);
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
			"\001FPU"
			"\002VME"
			"\003DE"
			"\004PSE"
			"\005TSC"
			"\006MSR"
			"\007PAE"
			"\010MCE"
			"\011CX8"
			"\012APIC"
			"\013<b10>"
			"\014<b11>"
			"\015MTRR"
			"\016PGE"
			"\017MCA"
			"\020CMOV"
			);
		}
	} else if (strcmp(cpu_vendor, "CyrixInstead") == 0) {
		printf("  Device ID = 0x%lx", cyrix_did);
		printf("  Stepping=%ld", (cyrix_did & 0xf000) >> 12);
		printf("  Revision=%ld", (cyrix_did & 0x0fff) >> 8);
#ifndef CYRIX_CACHE_REALLY_WORKS
		if (cpu == CPU_M1 && (cyrix_did & 0xff00) < 0x1700)
			printf("\n  CPU cache: write-through mode");
#endif
	}
	/* Avoid ugly blank lines: only print newline when we have to. */
	if (*cpu_vendor || cpu_id)
		printf("\n");
#endif
#ifdef I686_CPU
	/*
	 * XXX - Do PPro CPUID level=2 stuff here?
	 */
#endif
}

void
panicifcpuunsupported(void)
{

	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (cpu_class) {
	case CPUCLASS_286:	/* a 286 should not make it this far, anyway */
#if !defined(I386_CPU) && !defined(I486_CPU) && !defined(I586_CPU) && !defined(I686_CPU)
#error This kernel is not configured for one of the supported CPUs
#endif
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


static	volatile u_int trap_by_wrmsr;

/*
 * Special exception 16 handler.
 * The wrmsr instruction generates invalid opcodes fault on 486-class
 * Cyrix CPU.  Stacked eip register points the wrmsr instruction in the
 * function identblue() when this handler is called.  Stacked eip should
 * be advanced.
 */
inthand_t	bluetrap;
asm
("
	.text
_bluetrap:
	ss
	movl	$0xa8c1d, _trap_by_wrmsr  # Don't ask meaning of the number :-).
	addl	$2, (%esp)				  # I know wrmsr is a 2-bytes instruction.
	iret
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

	trap_by_wrmsr = 0;
	/* 
	 * Cyrix 486-class CPU does not support wrmsr instruction.
	 * The wrmsr instruction causes invalid opcode fault, and exception
	 * will be trapped by bluetrap() on Cyrix 486-class CPU.  The bluetrap()
	 * set the magic number to trap_by_wrmsr.
	 */
	setidt(6, bluetrap, SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	wrmsr(0x1002, 0x03000000LL);	/* Fault on Cyrix 486-class CPU. */

	if (trap_by_wrmsr == 0xa8c1d)
		return 0;					/* Cyrix CPU sets the magic number. */

	return 1;						/* IBM Blue Lightnig CPU */
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
	u_long	eflags;
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

	if (strcmp(cpu_vendor, "CyrixInstead") == 0) {
		if (cpu == CPU_486) {
			/*
			 * These conditions are equivalent to:
			 *     - CPU does not support cpuid instruction.
			 *     - Cyrix/IBM CPU is detected.
			 */
			if (identblue()) {
				strcpy(cpu_vendor, "IBM");
				cpu = CPU_BLUE;
				return;
			}
		}
		switch (cpu_id & 0xf00) {
		case 0x600:
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
				if ((cyrix_did & 0x00f0) < 8)
					cpu = CPU_M1;
				else
					cpu = CPU_M1SC;
				break;
			case 0x30:
				cpu = CPU_M1;
				break;
			case 0x40:
				cpu = CPU_M1SC;
				break;
			default:
				/* M2 and later CPUs are treated as M2. */
				cpu = CPU_M2;
				break;
			}
		}
	}
}

/*
 * This routine is called specifically to set up cpu_class before 
 * startrtclock() uses it.  Probably this should be rearranged so that
 * startrtclock() doesn't need to run until after identifycpu() has been
 * called.  Another alternative formulation would be for this routine
 * to do all the identification work, and make identifycpu() into a
 * printing-only routine.
 */
void
earlysetcpuclass(void)
{

	cpu_class = i386_cpus[cpu].cpu_class;
}
