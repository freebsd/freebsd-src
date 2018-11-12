/*-
 * Copyright (c) 2014 Andrew Turner
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Semihalf
 * under sponsorship of the FreeBSD Foundation.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>

static int ident_lock;

char machine[] = "arm64";

SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0,
    "Machine class");

/*
 * Per-CPU affinity as provided in MPIDR_EL1
 * Indexed by CPU number in logical order selected by the system.
 * Relevant fields can be extracted using CPU_AFFn macros,
 * Aff3.Aff2.Aff1.Aff0 construct a unique CPU address in the system.
 *
 * Fields used by us:
 * Aff1 - Cluster number
 * Aff0 - CPU number in Aff1 cluster
 */
uint64_t __cpu_affinity[MAXCPU];
static u_int cpu_aff_levels;

struct cpu_desc {
	u_int		cpu_impl;
	u_int		cpu_part_num;
	u_int		cpu_variant;
	u_int		cpu_revision;
	const char	*cpu_impl_name;
	const char	*cpu_part_name;

	uint64_t	mpidr;
	uint64_t	id_aa64afr0;
	uint64_t	id_aa64afr1;
	uint64_t	id_aa64dfr0;
	uint64_t	id_aa64dfr1;
	uint64_t	id_aa64isar0;
	uint64_t	id_aa64isar1;
	uint64_t	id_aa64mmfr0;
	uint64_t	id_aa64mmfr1;
	uint64_t	id_aa64pfr0;
	uint64_t	id_aa64pfr1;
};

struct cpu_desc cpu_desc[MAXCPU];
static u_int cpu_print_regs;
#define	PRINT_ID_AA64_AFR0	0x00000001
#define	PRINT_ID_AA64_AFR1	0x00000002
#define	PRINT_ID_AA64_DFR0	0x00000004
#define	PRINT_ID_AA64_DFR1	0x00000008
#define	PRINT_ID_AA64_ISAR0	0x00000010
#define	PRINT_ID_AA64_ISAR1	0x00000020
#define	PRINT_ID_AA64_MMFR0	0x00000040
#define	PRINT_ID_AA64_MMFR1	0x00000080
#define	PRINT_ID_AA64_PFR0	0x00000100
#define	PRINT_ID_AA64_PFR1	0x00000200

struct cpu_parts {
	u_int		part_id;
	const char	*part_name;
};
#define	CPU_PART_NONE	{ 0, "Unknown Processor" }

struct cpu_implementers {
	u_int			impl_id;
	const char		*impl_name;
	/*
	 * Part number is implementation defined
	 * so each vendor will have its own set of values and names.
	 */
	const struct cpu_parts	*cpu_parts;
};
#define	CPU_IMPLEMENTER_NONE	{ 0, "Unknown Implementer", cpu_parts_none }

/*
 * Per-implementer table of (PartNum, CPU Name) pairs.
 */
/* ARM Ltd. */
static const struct cpu_parts cpu_parts_arm[] = {
	{ CPU_PART_FOUNDATION, "Foundation-Model" },
	{ CPU_PART_CORTEX_A53, "Cortex-A53" },
	{ CPU_PART_CORTEX_A57, "Cortex-A57" },
	CPU_PART_NONE,
};
/* Cavium */
static const struct cpu_parts cpu_parts_cavium[] = {
	{ CPU_PART_THUNDER, "Thunder" },
	CPU_PART_NONE,
};

/* Unknown */
static const struct cpu_parts cpu_parts_none[] = {
	CPU_PART_NONE,
};

/*
 * Implementers table.
 */
const struct cpu_implementers cpu_implementers[] = {
	{ CPU_IMPL_ARM,		"ARM",		cpu_parts_arm },
	{ CPU_IMPL_BROADCOM,	"Broadcom",	cpu_parts_none },
	{ CPU_IMPL_CAVIUM,	"Cavium",	cpu_parts_cavium },
	{ CPU_IMPL_DEC,		"DEC",		cpu_parts_none },
	{ CPU_IMPL_INFINEON,	"IFX",		cpu_parts_none },
	{ CPU_IMPL_FREESCALE,	"Freescale",	cpu_parts_none },
	{ CPU_IMPL_NVIDIA,	"NVIDIA",	cpu_parts_none },
	{ CPU_IMPL_APM,		"APM",		cpu_parts_none },
	{ CPU_IMPL_QUALCOMM,	"Qualcomm",	cpu_parts_none },
	{ CPU_IMPL_MARVELL,	"Marvell",	cpu_parts_none },
	{ CPU_IMPL_INTEL,	"Intel",	cpu_parts_none },
	CPU_IMPLEMENTER_NONE,
};

void
print_cpu_features(u_int cpu)
{
	int printed;

	printf("CPU%3d: %s %s r%dp%d", cpu, cpu_desc[cpu].cpu_impl_name,
	    cpu_desc[cpu].cpu_part_name, cpu_desc[cpu].cpu_variant,
	    cpu_desc[cpu].cpu_revision);

	printf(" affinity:");
	switch(cpu_aff_levels) {
	default:
	case 4:
		printf(" %2d", CPU_AFF3(cpu_desc[cpu].mpidr));
		/* FALLTHROUGH */
	case 3:
		printf(" %2d", CPU_AFF2(cpu_desc[cpu].mpidr));
		/* FALLTHROUGH */
	case 2:
		printf(" %2d", CPU_AFF1(cpu_desc[cpu].mpidr));
		/* FALLTHROUGH */
	case 1:
	case 0: /* On UP this will be zero */
		printf(" %2d", CPU_AFF0(cpu_desc[cpu].mpidr));
		break;
	}
	printf("\n");

	/*
	 * There is a hardware errata where, if one CPU is performing a TLB
	 * invalidation while another is performing a store-exclusive the
	 * store-exclusive may return the wrong status. A workaround seems
	 * to be to use an IPI to invalidate on each CPU, however given the
	 * limited number of affected units (pass 1.1 is the evaluation
	 * hardware revision), and the lack of information from Cavium
	 * this has not been implemented.
	 *
	 * At the time of writing this the only information is from:
	 * https://lkml.org/lkml/2016/8/4/722
	 */
	/*
	 * XXX: CPU_MATCH_ERRATA_CAVIUM_THUNDER_1_1 on it's own also
	 * triggers on pass 2.0+.
	 */
	if (cpu == 0 && CPU_VAR(PCPU_GET(midr)) == 0 &&
	    CPU_MATCH_ERRATA_CAVIUM_THUNDER_1_1)
		printf("WARNING: ThunderX Pass 1.1 detected.\nThis has known "
		    "hardware bugs that may cause the incorrect operation of "
		    "atomic operations.\n");

	if (cpu != 0 && cpu_print_regs == 0)
		return;

#define SEP_STR	((printed++) == 0) ? "" : ","

	/* AArch64 Instruction Set Attribute Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_ISAR0) != 0) {
		printed = 0;
		printf(" Instruction Set Attributes 0 = <");

		switch (ID_AA64ISAR0_RDM(cpu_desc[cpu].id_aa64isar0)) {
		case ID_AA64ISAR0_RDM_NONE:
			break;
		case ID_AA64ISAR0_RDM_IMPL:
			printf("%sRDM", SEP_STR);
			break;
		default:
			printf("%sUnknown RDM", SEP_STR);
		}

		switch (ID_AA64ISAR0_ATOMIC(cpu_desc[cpu].id_aa64isar0)) {
		case ID_AA64ISAR0_ATOMIC_NONE:
			break;
		case ID_AA64ISAR0_ATOMIC_IMPL:
			printf("%sAtomic", SEP_STR);
			break;
		default:
			printf("%sUnknown Atomic", SEP_STR);
		}

		switch (ID_AA64ISAR0_AES(cpu_desc[cpu].id_aa64isar0)) {
		case ID_AA64ISAR0_AES_NONE:
			break;
		case ID_AA64ISAR0_AES_BASE:
			printf("%sAES", SEP_STR);
			break;
		case ID_AA64ISAR0_AES_PMULL:
			printf("%sAES+PMULL", SEP_STR);
			break;
		default:
			printf("%sUnknown AES", SEP_STR);
			break;
		}

		switch (ID_AA64ISAR0_SHA1(cpu_desc[cpu].id_aa64isar0)) {
		case ID_AA64ISAR0_SHA1_NONE:
			break;
		case ID_AA64ISAR0_SHA1_BASE:
			printf("%sSHA1", SEP_STR);
			break;
		default:
			printf("%sUnknown SHA1", SEP_STR);
			break;
		}

		switch (ID_AA64ISAR0_SHA2(cpu_desc[cpu].id_aa64isar0)) {
		case ID_AA64ISAR0_SHA2_NONE:
			break;
		case ID_AA64ISAR0_SHA2_BASE:
			printf("%sSHA2", SEP_STR);
			break;
		default:
			printf("%sUnknown SHA2", SEP_STR);
			break;
		}

		switch (ID_AA64ISAR0_CRC32(cpu_desc[cpu].id_aa64isar0)) {
		case ID_AA64ISAR0_CRC32_NONE:
			break;
		case ID_AA64ISAR0_CRC32_BASE:
			printf("%sCRC32", SEP_STR);
			break;
		default:
			printf("%sUnknown CRC32", SEP_STR);
			break;
		}

		if ((cpu_desc[cpu].id_aa64isar0 & ~ID_AA64ISAR0_MASK) != 0)
			printf("%s%#lx", SEP_STR,
			    cpu_desc[cpu].id_aa64isar0 & ~ID_AA64ISAR0_MASK);

		printf(">\n");
	}

	/* AArch64 Instruction Set Attribute Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_ISAR1) != 0) {
		printf(" Instruction Set Attributes 1 = <%#lx>\n",
		    cpu_desc[cpu].id_aa64isar1);
	}

	/* AArch64 Processor Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_PFR0) != 0) {
		printed = 0;
		printf("         Processor Features 0 = <");
		switch (ID_AA64PFR0_GIC(cpu_desc[cpu].id_aa64pfr0)) {
		case ID_AA64PFR0_GIC_CPUIF_NONE:
			break;
		case ID_AA64PFR0_GIC_CPUIF_EN:
			printf("%sGIC", SEP_STR);
			break;
		default:
			printf("%sUnknown GIC interface", SEP_STR);
			break;
		}

		switch (ID_AA64PFR0_ADV_SIMD(cpu_desc[cpu].id_aa64pfr0)) {
		case ID_AA64PFR0_ADV_SIMD_NONE:
			break;
		case ID_AA64PFR0_ADV_SIMD_IMPL:
			printf("%sAdvSIMD", SEP_STR);
			break;
		default:
			printf("%sUnknown AdvSIMD", SEP_STR);
			break;
		}

		switch (ID_AA64PFR0_FP(cpu_desc[cpu].id_aa64pfr0)) {
		case ID_AA64PFR0_FP_NONE:
			break;
		case ID_AA64PFR0_FP_IMPL:
			printf("%sFloat", SEP_STR);
			break;
		default:
			printf("%sUnknown Float", SEP_STR);
			break;
		}

		switch (ID_AA64PFR0_EL3(cpu_desc[cpu].id_aa64pfr0)) {
		case ID_AA64PFR0_EL3_NONE:
			printf("%sNo EL3", SEP_STR);
			break;
		case ID_AA64PFR0_EL3_64:
			printf("%sEL3", SEP_STR);
			break;
		case ID_AA64PFR0_EL3_64_32:
			printf("%sEL3 32", SEP_STR);
			break;
		default:
			printf("%sUnknown EL3", SEP_STR);
			break;
		}

		switch (ID_AA64PFR0_EL2(cpu_desc[cpu].id_aa64pfr0)) {
		case ID_AA64PFR0_EL2_NONE:
			printf("%sNo EL2", SEP_STR);
			break;
		case ID_AA64PFR0_EL2_64:
			printf("%sEL2", SEP_STR);
			break;
		case ID_AA64PFR0_EL2_64_32:
			printf("%sEL2 32", SEP_STR);
			break;
		default:
			printf("%sUnknown EL2", SEP_STR);
			break;
		}

		switch (ID_AA64PFR0_EL1(cpu_desc[cpu].id_aa64pfr0)) {
		case ID_AA64PFR0_EL1_64:
			printf("%sEL1", SEP_STR);
			break;
		case ID_AA64PFR0_EL1_64_32:
			printf("%sEL1 32", SEP_STR);
			break;
		default:
			printf("%sUnknown EL1", SEP_STR);
			break;
		}

		switch (ID_AA64PFR0_EL0(cpu_desc[cpu].id_aa64pfr0)) {
		case ID_AA64PFR0_EL0_64:
			printf("%sEL0", SEP_STR);
			break;
		case ID_AA64PFR0_EL0_64_32:
			printf("%sEL0 32", SEP_STR);
			break;
		default:
			printf("%sUnknown EL0", SEP_STR);
			break;
		}

		if ((cpu_desc[cpu].id_aa64pfr0 & ~ID_AA64PFR0_MASK) != 0)
			printf("%s%#lx", SEP_STR,
			    cpu_desc[cpu].id_aa64pfr0 & ~ID_AA64PFR0_MASK);

		printf(">\n");
	}

	/* AArch64 Processor Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_PFR1) != 0) {
		printf("         Processor Features 1 = <%#lx>\n",
		    cpu_desc[cpu].id_aa64pfr1);
	}

	/* AArch64 Memory Model Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_MMFR0) != 0) {
		printed = 0;
		printf("      Memory Model Features 0 = <");
		switch (ID_AA64MMFR0_TGRAN4(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_TGRAN4_NONE:
			break;
		case ID_AA64MMFR0_TGRAN4_IMPL:
			printf("%s4k Granule", SEP_STR);
			break;
		default:
			printf("%sUnknown 4k Granule", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR0_TGRAN16(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_TGRAN16_NONE:
			break;
		case ID_AA64MMFR0_TGRAN16_IMPL:
			printf("%s16k Granule", SEP_STR);
			break;
		default:
			printf("%sUnknown 16k Granule", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR0_TGRAN64(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_TGRAN64_NONE:
			break;
		case ID_AA64MMFR0_TGRAN64_IMPL:
			printf("%s64k Granule", SEP_STR);
			break;
		default:
			printf("%sUnknown 64k Granule", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR0_BIGEND(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_BIGEND_FIXED:
			break;
		case ID_AA64MMFR0_BIGEND_MIXED:
			printf("%sMixedEndian", SEP_STR);
			break;
		default:
			printf("%sUnknown Endian switching", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR0_BIGEND_EL0(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_BIGEND_EL0_FIXED:
			break;
		case ID_AA64MMFR0_BIGEND_EL0_MIXED:
			printf("%sEL0 MixEndian", SEP_STR);
			break;
		default:
			printf("%sUnknown EL0 Endian switching", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR0_S_NS_MEM(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_S_NS_MEM_NONE:
			break;
		case ID_AA64MMFR0_S_NS_MEM_DISTINCT:
			printf("%sS/NS Mem", SEP_STR);
			break;
		default:
			printf("%sUnknown S/NS Mem", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR0_ASID_BITS(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_ASID_BITS_8:
			printf("%s8bit ASID", SEP_STR);
			break;
		case ID_AA64MMFR0_ASID_BITS_16:
			printf("%s16bit ASID", SEP_STR);
			break;
		default:
			printf("%sUnknown ASID", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR0_PA_RANGE(cpu_desc[cpu].id_aa64mmfr0)) {
		case ID_AA64MMFR0_PA_RANGE_4G:
			printf("%s4GB PA", SEP_STR);
			break;
		case ID_AA64MMFR0_PA_RANGE_64G:
			printf("%s64GB PA", SEP_STR);
			break;
		case ID_AA64MMFR0_PA_RANGE_1T:
			printf("%s1TB PA", SEP_STR);
			break;
		case ID_AA64MMFR0_PA_RANGE_4T:
			printf("%s4TB PA", SEP_STR);
			break;
		case ID_AA64MMFR0_PA_RANGE_16T:
			printf("%s16TB PA", SEP_STR);
			break;
		case ID_AA64MMFR0_PA_RANGE_256T:
			printf("%s256TB PA", SEP_STR);
			break;
		default:
			printf("%sUnknown PA Range", SEP_STR);
			break;
		}

		if ((cpu_desc[cpu].id_aa64mmfr0 & ~ID_AA64MMFR0_MASK) != 0)
			printf("%s%#lx", SEP_STR,
			    cpu_desc[cpu].id_aa64mmfr0 & ~ID_AA64MMFR0_MASK);
		printf(">\n");
	}

	/* AArch64 Memory Model Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_MMFR1) != 0) {
		printed = 0;
		printf("      Memory Model Features 1 = <");

		switch (ID_AA64MMFR1_PAN(cpu_desc[cpu].id_aa64mmfr1)) {
		case ID_AA64MMFR1_PAN_NONE:
			break;
		case ID_AA64MMFR1_PAN_IMPL:
			printf("%sPAN", SEP_STR);
			break;
		default:
			printf("%sUnknown PAN", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR1_LO(cpu_desc[cpu].id_aa64mmfr1)) {
		case ID_AA64MMFR1_LO_NONE:
			break;
		case ID_AA64MMFR1_LO_IMPL:
			printf("%sLO", SEP_STR);
			break;
		default:
			printf("%sUnknown LO", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR1_HPDS(cpu_desc[cpu].id_aa64mmfr1)) {
		case ID_AA64MMFR1_HPDS_NONE:
			break;
		case ID_AA64MMFR1_HPDS_IMPL:
			printf("%sHPDS", SEP_STR);
			break;
		default:
			printf("%sUnknown HPDS", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR1_VH(cpu_desc[cpu].id_aa64mmfr1)) {
		case ID_AA64MMFR1_VH_NONE:
			break;
		case ID_AA64MMFR1_VH_IMPL:
			printf("%sVHE", SEP_STR);
			break;
		default:
			printf("%sUnknown VHE", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR1_VMIDBITS(cpu_desc[cpu].id_aa64mmfr1)) {
		case ID_AA64MMFR1_VMIDBITS_8:
			break;
		case ID_AA64MMFR1_VMIDBITS_16:
			printf("%s16 VMID bits", SEP_STR);
			break;
		default:
			printf("%sUnknown VMID bits", SEP_STR);
			break;
		}

		switch (ID_AA64MMFR1_HAFDBS(cpu_desc[cpu].id_aa64mmfr1)) {
		case ID_AA64MMFR1_HAFDBS_NONE:
			break;
		case ID_AA64MMFR1_HAFDBS_AF:
			printf("%sAF", SEP_STR);
			break;
		case ID_AA64MMFR1_HAFDBS_AF_DBS:
			printf("%sAF+DBS", SEP_STR);
			break;
		default:
			printf("%sUnknown Hardware update AF/DBS", SEP_STR);
			break;
		}

		if ((cpu_desc[cpu].id_aa64mmfr1 & ~ID_AA64MMFR1_MASK) != 0)
			printf("%s%#lx", SEP_STR,
			    cpu_desc[cpu].id_aa64mmfr1 & ~ID_AA64MMFR1_MASK);
		printf(">\n");
	}

	/* AArch64 Debug Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_DFR0) != 0) {
		printed = 0;
		printf("             Debug Features 0 = <");
		printf("%s%lu CTX Breakpoints", SEP_STR,
		    ID_AA64DFR0_CTX_CMPS(cpu_desc[cpu].id_aa64dfr0));

		printf("%s%lu Watchpoints", SEP_STR,
		    ID_AA64DFR0_WRPS(cpu_desc[cpu].id_aa64dfr0));

		printf("%s%lu Breakpoints", SEP_STR,
		    ID_AA64DFR0_BRPS(cpu_desc[cpu].id_aa64dfr0));

		switch (ID_AA64DFR0_PMU_VER(cpu_desc[cpu].id_aa64dfr0)) {
		case ID_AA64DFR0_PMU_VER_NONE:
			break;
		case ID_AA64DFR0_PMU_VER_3:
			printf("%sPMUv3", SEP_STR);
			break;
		case ID_AA64DFR0_PMU_VER_3_1:
			printf("%sPMUv3+16 bit evtCount", SEP_STR);
			break;
		case ID_AA64DFR0_PMU_VER_IMPL:
			printf("%sImplementation defined PMU", SEP_STR);
			break;
		default:
			printf("%sUnknown PMU", SEP_STR);
			break;
		}

		switch (ID_AA64DFR0_TRACE_VER(cpu_desc[cpu].id_aa64dfr0)) {
		case ID_AA64DFR0_TRACE_VER_NONE:
			break;
		case ID_AA64DFR0_TRACE_VER_IMPL:
			printf("%sTrace", SEP_STR);
			break;
		default:
			printf("%sUnknown Trace", SEP_STR);
			break;
		}

		switch (ID_AA64DFR0_DEBUG_VER(cpu_desc[cpu].id_aa64dfr0)) {
		case ID_AA64DFR0_DEBUG_VER_8:
			printf("%sDebug v8", SEP_STR);
			break;
		case ID_AA64DFR0_DEBUG_VER_8_VHE:
			printf("%sDebug v8+VHE", SEP_STR);
			break;
		default:
			printf("%sUnknown Debug", SEP_STR);
			break;
		}

		if (cpu_desc[cpu].id_aa64dfr0 & ~ID_AA64DFR0_MASK)
			printf("%s%#lx", SEP_STR,
			    cpu_desc[cpu].id_aa64dfr0 & ~ID_AA64DFR0_MASK);
		printf(">\n");
	}

	/* AArch64 Memory Model Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_DFR1) != 0) {
		printf("             Debug Features 1 = <%#lx>\n",
		    cpu_desc[cpu].id_aa64dfr1);
	}

	/* AArch64 Auxiliary Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_AFR0) != 0) {
		printf("         Auxiliary Features 0 = <%#lx>\n",
		    cpu_desc[cpu].id_aa64afr0);
	}

	/* AArch64 Auxiliary Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_AFR1) != 0) {
		printf("         Auxiliary Features 1 = <%#lx>\n",
		    cpu_desc[cpu].id_aa64afr1);
	}

#undef SEP_STR
}

void
identify_cpu(void)
{
	u_int midr;
	u_int impl_id;
	u_int part_id;
	u_int cpu;
	size_t i;
	const struct cpu_parts *cpu_partsp = NULL;

	cpu = PCPU_GET(cpuid);
	midr = get_midr();

	/*
	 * Store midr to pcpu to allow fast reading
	 * from EL0, EL1 and assembly code.
	 */
	PCPU_SET(midr, midr);

	impl_id = CPU_IMPL(midr);
	for (i = 0; i < nitems(cpu_implementers); i++) {
		if (impl_id == cpu_implementers[i].impl_id ||
		    cpu_implementers[i].impl_id == 0) {
			cpu_desc[cpu].cpu_impl = impl_id;
			cpu_desc[cpu].cpu_impl_name = cpu_implementers[i].impl_name;
			cpu_partsp = cpu_implementers[i].cpu_parts;
			break;
		}
	}

	part_id = CPU_PART(midr);
	for (i = 0; &cpu_partsp[i] != NULL; i++) {
		if (part_id == cpu_partsp[i].part_id ||
		    cpu_partsp[i].part_id == 0) {
			cpu_desc[cpu].cpu_part_num = part_id;
			cpu_desc[cpu].cpu_part_name = cpu_partsp[i].part_name;
			break;
		}
	}

	cpu_desc[cpu].cpu_revision = CPU_REV(midr);
	cpu_desc[cpu].cpu_variant = CPU_VAR(midr);

	/* Save affinity for current CPU */
	cpu_desc[cpu].mpidr = get_mpidr();
	CPU_AFFINITY(cpu) = cpu_desc[cpu].mpidr & CPU_AFF_MASK;

	cpu_desc[cpu].id_aa64dfr0 = READ_SPECIALREG(id_aa64dfr0_el1);
	cpu_desc[cpu].id_aa64dfr1 = READ_SPECIALREG(id_aa64dfr1_el1);
	cpu_desc[cpu].id_aa64isar0 = READ_SPECIALREG(id_aa64isar0_el1);
	cpu_desc[cpu].id_aa64isar1 = READ_SPECIALREG(id_aa64isar1_el1);
	cpu_desc[cpu].id_aa64mmfr0 = READ_SPECIALREG(id_aa64mmfr0_el1);
	cpu_desc[cpu].id_aa64mmfr1 = READ_SPECIALREG(id_aa64mmfr1_el1);
	cpu_desc[cpu].id_aa64pfr0 = READ_SPECIALREG(id_aa64pfr0_el1);
	cpu_desc[cpu].id_aa64pfr1 = READ_SPECIALREG(id_aa64pfr1_el1);

	if (cpu != 0) {
		/*
		 * This code must run on one cpu at a time, but we are
		 * not scheduling on the current core so implement a
		 * simple spinlock.
		 */
		while (atomic_cmpset_acq_int(&ident_lock, 0, 1) == 0)
			__asm __volatile("wfe" ::: "memory");

		switch (cpu_aff_levels) {
		case 0:
			if (CPU_AFF0(cpu_desc[cpu].mpidr) !=
			    CPU_AFF0(cpu_desc[0].mpidr))
				cpu_aff_levels = 1;
			/* FALLTHROUGH */
		case 1:
			if (CPU_AFF1(cpu_desc[cpu].mpidr) !=
			    CPU_AFF1(cpu_desc[0].mpidr))
				cpu_aff_levels = 2;
			/* FALLTHROUGH */
		case 2:
			if (CPU_AFF2(cpu_desc[cpu].mpidr) !=
			    CPU_AFF2(cpu_desc[0].mpidr))
				cpu_aff_levels = 3;
			/* FALLTHROUGH */
		case 3:
			if (CPU_AFF3(cpu_desc[cpu].mpidr) !=
			    CPU_AFF3(cpu_desc[0].mpidr))
				cpu_aff_levels = 4;
			break;
		}

		if (cpu_desc[cpu].id_aa64afr0 != cpu_desc[0].id_aa64afr0)
			cpu_print_regs |= PRINT_ID_AA64_AFR0;
		if (cpu_desc[cpu].id_aa64afr1 != cpu_desc[0].id_aa64afr1)
			cpu_print_regs |= PRINT_ID_AA64_AFR1;

		if (cpu_desc[cpu].id_aa64dfr0 != cpu_desc[0].id_aa64dfr0)
			cpu_print_regs |= PRINT_ID_AA64_DFR0;
		if (cpu_desc[cpu].id_aa64dfr1 != cpu_desc[0].id_aa64dfr1)
			cpu_print_regs |= PRINT_ID_AA64_DFR1;

		if (cpu_desc[cpu].id_aa64isar0 != cpu_desc[0].id_aa64isar0)
			cpu_print_regs |= PRINT_ID_AA64_ISAR0;
		if (cpu_desc[cpu].id_aa64isar1 != cpu_desc[0].id_aa64isar1)
			cpu_print_regs |= PRINT_ID_AA64_ISAR1;

		if (cpu_desc[cpu].id_aa64mmfr0 != cpu_desc[0].id_aa64mmfr0)
			cpu_print_regs |= PRINT_ID_AA64_MMFR0;
		if (cpu_desc[cpu].id_aa64mmfr1 != cpu_desc[0].id_aa64mmfr1)
			cpu_print_regs |= PRINT_ID_AA64_MMFR1;

		if (cpu_desc[cpu].id_aa64pfr0 != cpu_desc[0].id_aa64pfr0)
			cpu_print_regs |= PRINT_ID_AA64_PFR0;
		if (cpu_desc[cpu].id_aa64pfr1 != cpu_desc[0].id_aa64pfr1)
			cpu_print_regs |= PRINT_ID_AA64_PFR1;

		/* Wake up the other CPUs */
		atomic_store_rel_int(&ident_lock, 0);
		__asm __volatile("sev" ::: "memory");
	}
}
