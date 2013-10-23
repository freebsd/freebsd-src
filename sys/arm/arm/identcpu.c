/*	$NetBSD: cpu.c,v 1.55 2004/02/13 11:36:10 wiz Exp $	*/

/*-
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpu.c
 *
 * Probing and configuration for the master CPU
 *
 * Created      : 10/10/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <machine/endian.h>

#include <machine/cpuconf.h>
#include <machine/md_var.h>

char machine[] = "arm";

SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD,
	machine, 0, "Machine class");

static const char * const generic_steppings[16] = {
	"rev 0",	"rev 1",	"rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const sa110_steppings[16] = {
	"rev 0",	"step J",	"step K",	"step S",
	"step T",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const sa1100_steppings[16] = {
	"rev 0",	"step B",	"step C",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"step D",	"step E",	"rev 10"	"step G",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const sa1110_steppings[16] = {
	"step A-0",	"rev 1",	"rev 2",	"rev 3",
	"step B-0",	"step B-1",	"step B-2",	"step B-3",
	"step B-4",	"step B-5",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const ixp12x0_steppings[16] = {
	"(IXP1200 step A)",		"(IXP1200 step B)",
	"rev 2",			"(IXP1200 step C)",
	"(IXP1200 step D)",		"(IXP1240/1250 step A)",
	"(IXP1240 step B)",		"(IXP1250 step B)",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const xscale_steppings[16] = {
	"step A-0",	"step A-1",	"step B-0",	"step C-0",
	"step D-0",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const i80219_steppings[16] = {
	"step A-0",	"rev 1",	"rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const i80321_steppings[16] = {
	"step A-0",	"step B-0",	"rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const i81342_steppings[16] = {
	"step A-0",	"rev 1",	"rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

/* Steppings for PXA2[15]0 */
static const char * const pxa2x0_steppings[16] = {
	"step A-0",	"step A-1",	"step B-0",	"step B-1",
	"step B-2",	"step C-0",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

/* Steppings for PXA255/26x.
 * rev 5: PXA26x B0, rev 6: PXA255 A0
 */
static const char * const pxa255_steppings[16] = {
	"rev 0",	"rev 1",	"rev 2",	"step A-0",
	"rev 4",	"step B-0",	"step A-0",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

/* Stepping for PXA27x */
static const char * const pxa27x_steppings[16] = {
	"step A-0",	"step A-1",	"step B-0",	"step B-1",
	"step C-0",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

static const char * const ixp425_steppings[16] = {
	"step 0 (A0)",	"rev 1 (ARMv5TE)", "rev 2",	"rev 3",
	"rev 4",	"rev 5",	"rev 6",	"rev 7",
	"rev 8",	"rev 9",	"rev 10",	"rev 11",
	"rev 12",	"rev 13",	"rev 14",	"rev 15",
};

struct cpuidtab {
	u_int32_t	cpuid;
	enum		cpu_class cpu_class;
	const char	*cpu_name;
	const char * const *cpu_steppings;
};

const struct cpuidtab cpuids[] = {
	{ CPU_ID_ARM2,		CPU_CLASS_ARM2,		"ARM2",
	  generic_steppings },
	{ CPU_ID_ARM250,	CPU_CLASS_ARM2AS,	"ARM250",
	  generic_steppings },

	{ CPU_ID_ARM3,		CPU_CLASS_ARM3,		"ARM3",
	  generic_steppings },

	{ CPU_ID_ARM600,	CPU_CLASS_ARM6,		"ARM600",
	  generic_steppings },
	{ CPU_ID_ARM610,	CPU_CLASS_ARM6,		"ARM610",
	  generic_steppings },
	{ CPU_ID_ARM620,	CPU_CLASS_ARM6,		"ARM620",
	  generic_steppings },

	{ CPU_ID_ARM700,	CPU_CLASS_ARM7,		"ARM700",
	  generic_steppings },
	{ CPU_ID_ARM710,	CPU_CLASS_ARM7,		"ARM710",
	  generic_steppings },
	{ CPU_ID_ARM7500,	CPU_CLASS_ARM7,		"ARM7500",
	  generic_steppings },
	{ CPU_ID_ARM710A,	CPU_CLASS_ARM7,		"ARM710a",
	  generic_steppings },
	{ CPU_ID_ARM7500FE,	CPU_CLASS_ARM7,		"ARM7500FE",
	  generic_steppings },
	{ CPU_ID_ARM710T,	CPU_CLASS_ARM7TDMI,	"ARM710T",
	  generic_steppings },
	{ CPU_ID_ARM720T,	CPU_CLASS_ARM7TDMI,	"ARM720T",
	  generic_steppings },
	{ CPU_ID_ARM740T8K,	CPU_CLASS_ARM7TDMI, "ARM740T (8 KB cache)",
	  generic_steppings },
	{ CPU_ID_ARM740T4K,	CPU_CLASS_ARM7TDMI, "ARM740T (4 KB cache)",
	  generic_steppings },

	{ CPU_ID_ARM810,	CPU_CLASS_ARM8,		"ARM810",
	  generic_steppings },

	{ CPU_ID_ARM920T,	CPU_CLASS_ARM9TDMI,	"ARM920T",
	  generic_steppings },
	{ CPU_ID_ARM920T_ALT,	CPU_CLASS_ARM9TDMI,	"ARM920T",
	  generic_steppings },
	{ CPU_ID_ARM922T,	CPU_CLASS_ARM9TDMI,	"ARM922T",
	  generic_steppings },
	{ CPU_ID_ARM926EJS,	CPU_CLASS_ARM9EJS,	"ARM926EJ-S",
	  generic_steppings },
	{ CPU_ID_ARM940T,	CPU_CLASS_ARM9TDMI,	"ARM940T",
	  generic_steppings },
	{ CPU_ID_ARM946ES,	CPU_CLASS_ARM9ES,	"ARM946E-S",
	  generic_steppings },
	{ CPU_ID_ARM966ES,	CPU_CLASS_ARM9ES,	"ARM966E-S",
	  generic_steppings },
	{ CPU_ID_ARM966ESR1,	CPU_CLASS_ARM9ES,	"ARM966E-S",
	  generic_steppings },
	{ CPU_ID_FA526,		CPU_CLASS_ARM9TDMI,	"FA526",
	  generic_steppings },
	{ CPU_ID_FA626TE,	CPU_CLASS_ARM9ES,	"FA626TE",
	  generic_steppings },

	{ CPU_ID_TI925T,	CPU_CLASS_ARM9TDMI,	"TI ARM925T",
	  generic_steppings },

	{ CPU_ID_ARM1020E,	CPU_CLASS_ARM10E,	"ARM1020E",
	  generic_steppings },
	{ CPU_ID_ARM1022ES,	CPU_CLASS_ARM10E,	"ARM1022E-S",
	  generic_steppings },
	{ CPU_ID_ARM1026EJS,	CPU_CLASS_ARM10EJ,	"ARM1026EJ-S",
	  generic_steppings },

	{ CPU_ID_CORTEXA8R1,	CPU_CLASS_CORTEXA,	"Cortex A8-r1",
	  generic_steppings },
	{ CPU_ID_CORTEXA8R2,	CPU_CLASS_CORTEXA,	"Cortex A8-r2",
	  generic_steppings },
	{ CPU_ID_CORTEXA8R3,	CPU_CLASS_CORTEXA,	"Cortex A8-r3",
	  generic_steppings },
	{ CPU_ID_CORTEXA9R1,	CPU_CLASS_CORTEXA,	"Cortex A9-r1",
	  generic_steppings },
	{ CPU_ID_CORTEXA9R2,	CPU_CLASS_CORTEXA,	"Cortex A9-r2",
	  generic_steppings },
	{ CPU_ID_CORTEXA9R3,	CPU_CLASS_CORTEXA,	"Cortex A9-r3",
	  generic_steppings },
	{ CPU_ID_CORTEXA15,     CPU_CLASS_CORTEXA,      "Cortex A15",
	  generic_steppings },

	{ CPU_ID_SA110,		CPU_CLASS_SA1,		"SA-110",
	  sa110_steppings },
	{ CPU_ID_SA1100,	CPU_CLASS_SA1,		"SA-1100",
	  sa1100_steppings },
	{ CPU_ID_SA1110,	CPU_CLASS_SA1,		"SA-1110",
	  sa1110_steppings },

	{ CPU_ID_IXP1200,	CPU_CLASS_SA1,		"IXP1200",
	  ixp12x0_steppings },

	{ CPU_ID_80200,		CPU_CLASS_XSCALE,	"i80200",
	  xscale_steppings },

	{ CPU_ID_80321_400,	CPU_CLASS_XSCALE,	"i80321 400MHz",
	  i80321_steppings },
	{ CPU_ID_80321_600,	CPU_CLASS_XSCALE,	"i80321 600MHz",
	  i80321_steppings },
	{ CPU_ID_80321_400_B0,	CPU_CLASS_XSCALE,	"i80321 400MHz",
	  i80321_steppings },
	{ CPU_ID_80321_600_B0,	CPU_CLASS_XSCALE,	"i80321 600MHz",
	  i80321_steppings },

	{ CPU_ID_81342,		CPU_CLASS_XSCALE,	"i81342",
	  i81342_steppings },

	{ CPU_ID_80219_400,	CPU_CLASS_XSCALE,	"i80219 400MHz",
	  i80219_steppings },
	{ CPU_ID_80219_600,	CPU_CLASS_XSCALE,	"i80219 600MHz",
	  i80219_steppings },

	{ CPU_ID_PXA27X,	CPU_CLASS_XSCALE,	"PXA27x",
	  pxa27x_steppings },
	{ CPU_ID_PXA250A,	CPU_CLASS_XSCALE,	"PXA250",
	  pxa2x0_steppings },
	{ CPU_ID_PXA210A,	CPU_CLASS_XSCALE,	"PXA210",
	  pxa2x0_steppings },
	{ CPU_ID_PXA250B,	CPU_CLASS_XSCALE,	"PXA250",
	  pxa2x0_steppings },
	{ CPU_ID_PXA210B,	CPU_CLASS_XSCALE,	"PXA210",
	  pxa2x0_steppings },
	{ CPU_ID_PXA250C, 	CPU_CLASS_XSCALE,	"PXA255",
	  pxa255_steppings },
	{ CPU_ID_PXA210C, 	CPU_CLASS_XSCALE,	"PXA210",
	  pxa2x0_steppings },

	{ CPU_ID_IXP425_533,	CPU_CLASS_XSCALE,	"IXP425 533MHz",
	  ixp425_steppings },
	{ CPU_ID_IXP425_400,	CPU_CLASS_XSCALE,	"IXP425 400MHz",
	  ixp425_steppings },
	{ CPU_ID_IXP425_266,	CPU_CLASS_XSCALE,	"IXP425 266MHz",
	  ixp425_steppings },

	/* XXX ixp435 steppings? */
	{ CPU_ID_IXP435,	CPU_CLASS_XSCALE,	"IXP435",
	  ixp425_steppings },

	{ CPU_ID_ARM1136JS,	CPU_CLASS_ARM11J,	"ARM1136J-S",
	  generic_steppings },
	{ CPU_ID_ARM1136JSR1,	CPU_CLASS_ARM11J,	"ARM1136J-S R1",
	  generic_steppings },
	{ CPU_ID_ARM1176JZS,	CPU_CLASS_ARM11J,	"ARM1176JZ-S",
	  generic_steppings },

	{ CPU_ID_MV88FR131,	CPU_CLASS_MARVELL,	"Feroceon 88FR131",
	  generic_steppings },

	{ CPU_ID_MV88FR571_VD,	CPU_CLASS_MARVELL,	"Feroceon 88FR571-VD",
	  generic_steppings },
	{ CPU_ID_MV88SV581X_V6,	CPU_CLASS_MARVELL,	"Sheeva 88SV581x",
	  generic_steppings },
	{ CPU_ID_ARM_88SV581X_V6, CPU_CLASS_MARVELL,	"Sheeva 88SV581x",
	  generic_steppings },
	{ CPU_ID_MV88SV581X_V7,	CPU_CLASS_MARVELL,	"Sheeva 88SV581x",
	  generic_steppings },
	{ CPU_ID_ARM_88SV581X_V7, CPU_CLASS_MARVELL,	"Sheeva 88SV581x",
	  generic_steppings },
	{ CPU_ID_MV88SV584X_V6,	CPU_CLASS_MARVELL,	"Sheeva 88SV584x",
	  generic_steppings },
	{ CPU_ID_ARM_88SV584X_V6, CPU_CLASS_MARVELL,	"Sheeva 88SV584x",
	  generic_steppings },
	{ CPU_ID_MV88SV584X_V7,	CPU_CLASS_MARVELL,	"Sheeva 88SV584x",
	  generic_steppings },

	{ 0, CPU_CLASS_NONE, NULL, NULL }
};

struct cpu_classtab {
	const char	*class_name;
	const char	*class_option;
};

const struct cpu_classtab cpu_classes[] = {
	{ "unknown",	NULL },			/* CPU_CLASS_NONE */
	{ "ARM2",	"CPU_ARM2" },		/* CPU_CLASS_ARM2 */
	{ "ARM2as",	"CPU_ARM250" },		/* CPU_CLASS_ARM2AS */
	{ "ARM3",	"CPU_ARM3" },		/* CPU_CLASS_ARM3 */
	{ "ARM6",	"CPU_ARM6" },		/* CPU_CLASS_ARM6 */
	{ "ARM7",	"CPU_ARM7" },		/* CPU_CLASS_ARM7 */
	{ "ARM7TDMI",	"CPU_ARM7TDMI" },	/* CPU_CLASS_ARM7TDMI */
	{ "ARM8",	"CPU_ARM8" },		/* CPU_CLASS_ARM8 */
	{ "ARM9TDMI",	"CPU_ARM9TDMI" },	/* CPU_CLASS_ARM9TDMI */
	{ "ARM9E-S",	"CPU_ARM9E" },		/* CPU_CLASS_ARM9ES */
	{ "ARM9EJ-S",	"CPU_ARM9E" },		/* CPU_CLASS_ARM9EJS */
	{ "ARM10E",	"CPU_ARM10" },		/* CPU_CLASS_ARM10E */
	{ "ARM10EJ",	"CPU_ARM10" },		/* CPU_CLASS_ARM10EJ */
	{ "Cortex-A",	"CPU_CORTEXA" },	/* CPU_CLASS_CORTEXA */
	{ "SA-1",	"CPU_SA110" },		/* CPU_CLASS_SA1 */
	{ "XScale",	"CPU_XSCALE_..." },	/* CPU_CLASS_XSCALE */
	{ "ARM11J",	"CPU_ARM11" },		/* CPU_CLASS_ARM11J */
	{ "Marvell",	"CPU_MARVELL" },	/* CPU_CLASS_MARVELL */
};

/*
 * Report the type of the specified arm processor. This uses the generic and
 * arm specific information in the cpu structure to identify the processor.
 * The remaining fields in the cpu structure are filled in appropriately.
 */

static const char * const wtnames[] = {
	"write-through",
	"write-back",
	"write-back",
	"**unknown 3**",
	"**unknown 4**",
	"write-back-locking",		/* XXX XScale-specific? */
	"write-back-locking-A",
	"write-back-locking-B",
	"**unknown 8**",
	"**unknown 9**",
	"**unknown 10**",
	"**unknown 11**",
	"**unknown 12**",
	"**unknown 13**",
	"write-back-locking-C",
	"**unknown 15**",
};

static void
print_enadis(int enadis, char *s)
{

	printf(" %s %sabled", s, (enadis == 0) ? "dis" : "en");
}

extern int ctrl;
enum cpu_class cpu_class = CPU_CLASS_NONE;

u_int cpu_pfr(int num)
{
	u_int feat;

	switch (num) {
	case 0:
		__asm __volatile("mrc p15, 0, %0, c0, c1, 0"
		    : "=r" (feat));
		break;
	case 1:
		__asm __volatile("mrc p15, 0, %0, c0, c1, 1"
		    : "=r" (feat));
		break;
	default:
		panic("Processor Feature Register %d not implemented", num);
		break;
	}

	return (feat);
}

static
void identify_armv7(void)
{
	u_int feature;

	printf("Supported features:");
	/* Get Processor Feature Register 0 */
	feature = cpu_pfr(0);

	if (feature & ARM_PFR0_ARM_ISA_MASK)
		printf(" ARM_ISA");

	if (feature & ARM_PFR0_THUMB2)
		printf(" THUMB2");
	else if (feature & ARM_PFR0_THUMB)
		printf(" THUMB");

	if (feature & ARM_PFR0_JAZELLE_MASK)
		printf(" JAZELLE");

	if (feature & ARM_PFR0_THUMBEE_MASK)
		printf(" THUMBEE");


	/* Get Processor Feature Register 1 */
	feature = cpu_pfr(1);

	if (feature & ARM_PFR1_ARMV4_MASK)
		printf(" ARMv4");

	if (feature & ARM_PFR1_SEC_EXT_MASK)
		printf(" Security_Ext");

	if (feature & ARM_PFR1_MICROCTRL_MASK)
		printf(" M_profile");

	printf("\n");
}

void
identify_arm_cpu(void)
{
	u_int cpuid, reg, size, sets, ways;
	u_int8_t type, linesize;
	int i;

	cpuid = cpu_id();

	if (cpuid == 0) {
		printf("Processor failed probe - no CPU ID\n");
		return;
	}

	for (i = 0; cpuids[i].cpuid != 0; i++)
		if (cpuids[i].cpuid == (cpuid & CPU_ID_CPU_MASK)) {
			cpu_class = cpuids[i].cpu_class;
			printf("CPU: %s %s (%s core)\n",
			    cpuids[i].cpu_name,
			    cpuids[i].cpu_steppings[cpuid &
			    CPU_ID_REVISION_MASK],
			    cpu_classes[cpu_class].class_name);
			break;
		}
	if (cpuids[i].cpuid == 0)
		printf("unknown CPU (ID = 0x%x)\n", cpuid);

	printf(" ");

	if ((cpuid & CPU_ID_ARCH_MASK) == CPU_ID_CPUID_SCHEME) {
		identify_armv7();
	} else {
		if (ctrl & CPU_CONTROL_BEND_ENABLE)
			printf(" Big-endian");
		else
			printf(" Little-endian");

		switch (cpu_class) {
		case CPU_CLASS_ARM6:
		case CPU_CLASS_ARM7:
		case CPU_CLASS_ARM7TDMI:
		case CPU_CLASS_ARM8:
			print_enadis(ctrl & CPU_CONTROL_IDC_ENABLE, "IDC");
			break;
		case CPU_CLASS_ARM9TDMI:
		case CPU_CLASS_ARM9ES:
		case CPU_CLASS_ARM9EJS:
		case CPU_CLASS_ARM10E:
		case CPU_CLASS_ARM10EJ:
		case CPU_CLASS_SA1:
		case CPU_CLASS_XSCALE:
		case CPU_CLASS_ARM11J:
		case CPU_CLASS_MARVELL:
			print_enadis(ctrl & CPU_CONTROL_DC_ENABLE, "DC");
			print_enadis(ctrl & CPU_CONTROL_IC_ENABLE, "IC");
#ifdef CPU_XSCALE_81342
			print_enadis(ctrl & CPU_CONTROL_L2_ENABLE, "L2");
#endif
#if defined(SOC_MV_KIRKWOOD) || defined(SOC_MV_DISCOVERY)
			i = sheeva_control_ext(0, 0);
			print_enadis(i & MV_WA_ENABLE, "WA");
			print_enadis(i & MV_DC_STREAM_ENABLE, "DC streaming");
			printf("\n ");
			print_enadis((i & MV_BTB_DISABLE) == 0, "BTB");
			print_enadis(i & MV_L2_ENABLE, "L2");
			print_enadis((i & MV_L2_PREFETCH_DISABLE) == 0,
			    "L2 prefetch");
			printf("\n ");
#endif
			break;
		default:
			break;
		}
	}

	print_enadis(ctrl & CPU_CONTROL_WBUF_ENABLE, "WB");
	if (ctrl & CPU_CONTROL_LABT_ENABLE)
		printf(" LABT");
	else
		printf(" EABT");

	print_enadis(ctrl & CPU_CONTROL_BPRD_ENABLE, "branch prediction");
	printf("\n");

	if (arm_cache_level) {
		printf("LoUU:%d LoC:%d LoUIS:%d \n", CPU_CLIDR_LOUU(arm_cache_level) + 1,
		    arm_cache_loc, CPU_CLIDR_LOUIS(arm_cache_level) + 1);
		i = 0;
		while (((type = CPU_CLIDR_CTYPE(arm_cache_level, i)) != 0) && i < 7) {
			printf("Cache level %d: \n", i + 1);
			if (type == CACHE_DCACHE || type == CACHE_UNI_CACHE ||
			    type == CACHE_SEP_CACHE) {
				reg = arm_cache_type[2 * i];
				ways = CPUV7_CT_xSIZE_ASSOC(reg) + 1;
				sets = CPUV7_CT_xSIZE_SET(reg) + 1;
				linesize = 1 << (CPUV7_CT_xSIZE_LEN(reg) + 4);
				size = (ways * sets * linesize) / 1024;

				if (type == CACHE_UNI_CACHE)
					printf(" %dKB/%dB %d-way unified cache", size, linesize,ways);
				else
					printf(" %dKB/%dB %d-way data cache", size, linesize, ways);
				if (reg & CPUV7_CT_CTYPE_WT)
					printf(" WT");
				if (reg & CPUV7_CT_CTYPE_WB)
					printf(" WB");
				if (reg & CPUV7_CT_CTYPE_RA)
					printf(" Read-Alloc");
				if (reg & CPUV7_CT_CTYPE_WA)
					printf(" Write-Alloc");
				printf("\n");
			}

			if (type == CACHE_ICACHE || type == CACHE_SEP_CACHE) {
				reg = arm_cache_type[(2 * i) + 1];

				ways = CPUV7_CT_xSIZE_ASSOC(reg) + 1;
				sets = CPUV7_CT_xSIZE_SET(reg) + 1;
				linesize = 1 << (CPUV7_CT_xSIZE_LEN(reg) + 4);
				size = (ways * sets * linesize) / 1024;

				printf(" %dKB/%dB %d-way instruction cache", size, linesize, ways);
				if (reg & CPUV7_CT_CTYPE_WT)
					printf(" WT");
				if (reg & CPUV7_CT_CTYPE_WB)
					printf(" WB");
				if (reg & CPUV7_CT_CTYPE_RA)
					printf(" Read-Alloc");
				if (reg & CPUV7_CT_CTYPE_WA)
					printf(" Write-Alloc");
				printf("\n");
			}
			i++;
		}
	} else {
		/* Print cache info. */
		if (arm_picache_line_size == 0 && arm_pdcache_line_size == 0)
			return;

		if (arm_pcache_unified) {
			printf("  %dKB/%dB %d-way %s unified cache\n",
			    arm_pdcache_size / 1024,
			    arm_pdcache_line_size, arm_pdcache_ways,
			    wtnames[arm_pcache_type]);
		} else {
			printf("  %dKB/%dB %d-way instruction cache\n",
			    arm_picache_size / 1024,
			    arm_picache_line_size, arm_picache_ways);
			printf("  %dKB/%dB %d-way %s data cache\n",
			    arm_pdcache_size / 1024,
			    arm_pdcache_line_size, arm_pdcache_ways,
			    wtnames[arm_pcache_type]);
		}
	}
}
