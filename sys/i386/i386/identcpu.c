/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
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
 *	$Id: identcpu.c,v 1.8 1996/11/11 20:38:50 bde Exp $
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

/* XXX - should be in header file */
void	i486_bzero __P((void *buf, size_t len));

void identifycpu(void);		/* XXX should be in different header file */
void earlysetcpuclass(void);

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
	{ "Cy486DLC",		CPUCLASS_486 },		/* CPU_486DLC */
	{ "Pentium Pro",	CPUCLASS_686 },		/* CPU_686 */
};

void
identifycpu(void)
{
	cpu_class = i386_cpus[cpu].cpu_class;
	printf("CPU: ");
	strncpy(cpu_model, i386_cpus[cpu].cpu_name, sizeof cpu_model);

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	if (!strcmp(cpu_vendor,"GenuineIntel")) {
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
	} else if (!strcmp(cpu_vendor,"AuthenticAMD")) {
		cpu_model[0] = '\0';
		strcpy(cpu_model, "AMD ");
		switch (cpu_id & 0xF0) {
		case 0xE0:
			strcat(cpu_model, "Am5x86 Write-Through");
			break;
		case 0xF0:
			strcat(cpu_model, "Am5x86 Write-Back");
			break;
		default:
			strcat(cpu_model, "Unknown");
			break;
		}
	}
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

	if (!strcmp(cpu_vendor, "GenuineIntel")) {
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
