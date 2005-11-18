/*-
 * Copyright (c) 2005, Joseph Koshy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/pmc.h>
#include <sys/systm.h>

#include <machine/apicreg.h>
#include <machine/pmc_mdep.h>
#include <machine/md_var.h>

extern volatile lapic_t *lapic;

void
pmc_x86_lapic_enable_pmc_interrupt(void)
{
	uint32_t value;

	value =  lapic->lvt_pcint;
	value &= ~APIC_LVT_M;
	lapic->lvt_pcint = value;
}


static struct pmc_mdep *
pmc_intel_initialize(void)
{
	struct pmc_mdep *pmc_mdep;
	enum pmc_cputype cputype;
	int error, model;

	KASSERT(strcmp(cpu_vendor, "GenuineIntel") == 0,
	    ("[intel,%d] Initializing non-intel processor", __LINE__));

	PMCDBG(MDP,INI,0, "intel-initialize cpuid=0x%x", cpu_id);

	cputype = -1;

	switch (cpu_id & 0xF00) {
#if	defined(__i386__)
	case 0x500:		/* Pentium family processors */
		cputype = PMC_CPU_INTEL_P5;
		break;
	case 0x600:		/* Pentium Pro, Celeron, Pentium II & III */
		switch ((cpu_id & 0xF0) >> 4) { /* model number field */
		case 0x1:
			cputype = PMC_CPU_INTEL_P6;
			break;
		case 0x3: case 0x5:
			cputype = PMC_CPU_INTEL_PII;
			break;
		case 0x6:
			cputype = PMC_CPU_INTEL_CL;
			break;
		case 0x7: case 0x8: case 0xA: case 0xB:
			cputype = PMC_CPU_INTEL_PIII;
			break;
		case 0x9: case 0xD: case 0xE:
			cputype = PMC_CPU_INTEL_PM;
			break;
		}
		break;
#endif
#if	defined(__i386__) || defined(__amd64__)
	case 0xF00:		/* P4 */
		model = ((cpu_id & 0xF0000) >> 12) | ((cpu_id & 0xF0) >> 4);
		if (model >= 0 && model <= 4) /* known models */
			cputype = PMC_CPU_INTEL_PIV;
		break;
	}
#endif

	if ((int) cputype == -1) {
		printf("pmc: Unknown Intel CPU.\n");
		return NULL;
	}

	MALLOC(pmc_mdep, struct pmc_mdep *, sizeof(struct pmc_mdep),
	    M_PMC, M_WAITOK|M_ZERO);

	pmc_mdep->pmd_cputype 	    = cputype;
	pmc_mdep->pmd_nclass	    = 2;
	pmc_mdep->pmd_classes[0].pm_class    = PMC_CLASS_TSC;
	pmc_mdep->pmd_classes[0].pm_caps     = PMC_CAP_READ;
	pmc_mdep->pmd_classes[0].pm_width    = 64;
	pmc_mdep->pmd_nclasspmcs[0] = 1;

	error = 0;

	switch (cputype) {

#if	defined(__i386__) || defined(__amd64__)

		/*
		 * Intel Pentium 4 Processors, and P4/EMT64 processors.
		 */

	case PMC_CPU_INTEL_PIV:
		error = pmc_initialize_p4(pmc_mdep);
		break;
#endif

#if	defined(__i386__)
		/*
		 * P6 Family Processors
		 */

	case PMC_CPU_INTEL_P6:
	case PMC_CPU_INTEL_CL:
	case PMC_CPU_INTEL_PII:
	case PMC_CPU_INTEL_PIII:
	case PMC_CPU_INTEL_PM:

		error = pmc_initialize_p6(pmc_mdep);
		break;

		/*
		 * Intel Pentium PMCs.
		 */

	case PMC_CPU_INTEL_P5:
		error = pmc_initialize_p5(pmc_mdep);
		break;
#endif

	default:
		KASSERT(0,("[intel,%d] Unknown CPU type", __LINE__));
	}

	if (error) {
		FREE(pmc_mdep, M_PMC);
		pmc_mdep = NULL;
	}

	return pmc_mdep;
}


/*
 * Machine dependent initialization for x86 class platforms.
 */

struct pmc_mdep *
pmc_md_initialize()
{
	int i;
	struct pmc_mdep *md;

	/* determine the CPU kind */
	md = NULL;
	if (strcmp(cpu_vendor, "AuthenticAMD") == 0)
		md = pmc_amd_initialize();
	else if (strcmp(cpu_vendor, "GenuineIntel") == 0)
		md = pmc_intel_initialize();

	/* disallow sampling if we do not have an LAPIC */
	if (md != NULL && lapic == NULL)
		for (i = 1; i < md->pmd_nclass; i++)
			md->pmd_classes[i].pm_caps &= ~PMC_CAP_INTERRUPT;

	return md;
}
