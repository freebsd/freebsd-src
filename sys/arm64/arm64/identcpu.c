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
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/undefined.h>
#include <machine/elf.h>

static int ident_lock;
static void print_cpu_features(u_int cpu);
static u_long parse_cpu_features_hwcap(u_int cpu);

char machine[] = "arm64";

#ifdef SCTL_MASK32
extern int adaptive_machine_arch;
#endif

static int
sysctl_hw_machine(SYSCTL_HANDLER_ARGS)
{
#ifdef SCTL_MASK32
	static const char machine32[] = "arm";
#endif
	int error;
#ifdef SCTL_MASK32
	if ((req->flags & SCTL_MASK32) != 0 && adaptive_machine_arch)
		error = SYSCTL_OUT(req, machine32, sizeof(machine32));
	else
#endif
		error = SYSCTL_OUT(req, machine, sizeof(machine));
	return (error);
}

SYSCTL_PROC(_hw, HW_MACHINE, machine, CTLTYPE_STRING | CTLFLAG_RD |
	CTLFLAG_MPSAFE, NULL, 0, sysctl_hw_machine, "A", "Machine class");

static char cpu_model[64];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD,
	cpu_model, sizeof(cpu_model), "Machine model");

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
	uint64_t	id_aa64mmfr2;
	uint64_t	id_aa64pfr0;
	uint64_t	id_aa64pfr1;
};

struct cpu_desc cpu_desc[MAXCPU];
struct cpu_desc user_cpu_desc;
static u_int cpu_print_regs;
#define	PRINT_ID_AA64_AFR0	0x00000001
#define	PRINT_ID_AA64_AFR1	0x00000002
#define	PRINT_ID_AA64_DFR0	0x00000010
#define	PRINT_ID_AA64_DFR1	0x00000020
#define	PRINT_ID_AA64_ISAR0	0x00000100
#define	PRINT_ID_AA64_ISAR1	0x00000200
#define	PRINT_ID_AA64_MMFR0	0x00001000
#define	PRINT_ID_AA64_MMFR1	0x00002000
#define	PRINT_ID_AA64_MMFR2	0x00004000
#define	PRINT_ID_AA64_PFR0	0x00010000
#define	PRINT_ID_AA64_PFR1	0x00020000

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
	{ CPU_PART_CORTEX_A35, "Cortex-A35" },
	{ CPU_PART_CORTEX_A53, "Cortex-A53" },
	{ CPU_PART_CORTEX_A55, "Cortex-A55" },
	{ CPU_PART_CORTEX_A57, "Cortex-A57" },
	{ CPU_PART_CORTEX_A65, "Cortex-A65" },
	{ CPU_PART_CORTEX_A72, "Cortex-A72" },
	{ CPU_PART_CORTEX_A73, "Cortex-A73" },
	{ CPU_PART_CORTEX_A75, "Cortex-A75" },
	{ CPU_PART_CORTEX_A76, "Cortex-A76" },
	{ CPU_PART_CORTEX_A76AE, "Cortex-A76AE" },
	{ CPU_PART_CORTEX_A77, "Cortex-A77" },
	{ CPU_PART_NEOVERSE_N1, "Neoverse-N1" },
	CPU_PART_NONE,
};
/* Cavium */
static const struct cpu_parts cpu_parts_cavium[] = {
	{ CPU_PART_THUNDERX, "ThunderX" },
	{ CPU_PART_THUNDERX2, "ThunderX2" },
	CPU_PART_NONE,
};

/* APM / Ampere */
static const struct cpu_parts cpu_parts_apm[] = {
	{ CPU_PART_EMAG8180, "eMAG 8180" },
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
	{ CPU_IMPL_APM,		"APM",		cpu_parts_apm },
	{ CPU_IMPL_QUALCOMM,	"Qualcomm",	cpu_parts_none },
	{ CPU_IMPL_MARVELL,	"Marvell",	cpu_parts_none },
	{ CPU_IMPL_INTEL,	"Intel",	cpu_parts_none },
	CPU_IMPLEMENTER_NONE,
};

#define	MRS_TYPE_MASK		0xf
#define	MRS_INVALID		0
#define	MRS_EXACT		1
#define	MRS_EXACT_VAL(x)	(MRS_EXACT | ((x) << 4))
#define	MRS_EXACT_FIELD(x)	((x) >> 4)
#define	MRS_LOWER		2

struct mrs_field_value {
	uint64_t	value;
	const char	*desc;
};

#define	MRS_FIELD_VALUE(_value, _desc)					\
	{								\
		.value = (_value),					\
		.desc = (_desc),					\
	}

#define	MRS_FIELD_VALUE_NONE_IMPL(_reg, _field, _none, _impl)		\
	MRS_FIELD_VALUE(_reg ## _ ## _field ## _ ## _none, ""),		\
	MRS_FIELD_VALUE(_reg ## _ ## _field ## _ ## _impl, #_field)

#define	MRS_FIELD_VALUE_COUNT(_reg, _field, _desc)			\
	MRS_FIELD_VALUE(0ul << _reg ## _ ## _field ## _SHIFT, "1 " _desc), \
	MRS_FIELD_VALUE(1ul << _reg ## _ ## _field ## _SHIFT, "2 " _desc "s"), \
	MRS_FIELD_VALUE(2ul << _reg ## _ ## _field ## _SHIFT, "3 " _desc "s"), \
	MRS_FIELD_VALUE(3ul << _reg ## _ ## _field ## _SHIFT, "4 " _desc "s"), \
	MRS_FIELD_VALUE(4ul << _reg ## _ ## _field ## _SHIFT, "5 " _desc "s"), \
	MRS_FIELD_VALUE(5ul << _reg ## _ ## _field ## _SHIFT, "6 " _desc "s"), \
	MRS_FIELD_VALUE(6ul << _reg ## _ ## _field ## _SHIFT, "7 " _desc "s"), \
	MRS_FIELD_VALUE(7ul << _reg ## _ ## _field ## _SHIFT, "8 " _desc "s"), \
	MRS_FIELD_VALUE(8ul << _reg ## _ ## _field ## _SHIFT, "9 " _desc "s"), \
	MRS_FIELD_VALUE(9ul << _reg ## _ ## _field ## _SHIFT, "10 "_desc "s"), \
	MRS_FIELD_VALUE(10ul<< _reg ## _ ## _field ## _SHIFT, "11 "_desc "s"), \
	MRS_FIELD_VALUE(11ul<< _reg ## _ ## _field ## _SHIFT, "12 "_desc "s"), \
	MRS_FIELD_VALUE(12ul<< _reg ## _ ## _field ## _SHIFT, "13 "_desc "s"), \
	MRS_FIELD_VALUE(13ul<< _reg ## _ ## _field ## _SHIFT, "14 "_desc "s"), \
	MRS_FIELD_VALUE(14ul<< _reg ## _ ## _field ## _SHIFT, "15 "_desc "s"), \
	MRS_FIELD_VALUE(15ul<< _reg ## _ ## _field ## _SHIFT, "16 "_desc "s")

#define	MRS_FIELD_VALUE_END	{ .desc = NULL }

struct mrs_field {
	const char	*name;
	struct mrs_field_value *values;
	uint64_t	mask;
	bool		sign;
	u_int		type;
	u_int		shift;
};

#define	MRS_FIELD(_register, _name, _sign, _type, _values)		\
	{								\
		.name = #_name,						\
		.sign = (_sign),					\
		.type = (_type),					\
		.shift = _register ## _ ## _name ## _SHIFT,		\
		.mask = _register ## _ ## _name ## _MASK,		\
		.values = (_values),					\
	}

#define	MRS_FIELD_END	{ .type = MRS_INVALID, }

/* ID_AA64AFR0_EL1 */
static struct mrs_field id_aa64afr0_fields[] = {
	MRS_FIELD_END,
};


/* ID_AA64AFR1_EL1 */
static struct mrs_field id_aa64afr1_fields[] = {
	MRS_FIELD_END,
};


/* ID_AA64DFR0_EL1 */
static struct mrs_field_value id_aa64dfr0_pmsver[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_PMSVer_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMSVer_V1, "SPE"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64dfr0_ctx_cmps[] = {
	MRS_FIELD_VALUE_COUNT(ID_AA64DFR0, CTX_CMPs, "CTX BKPT"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64dfr0_wrps[] = {
	MRS_FIELD_VALUE_COUNT(ID_AA64DFR0, WRPs, "Watchpoint"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64dfr0_brps[] = {
	MRS_FIELD_VALUE_COUNT(ID_AA64DFR0, BRPs, "Breakpoint"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64dfr0_pmuver[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3, "PMUv3"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3_1, "PMUv3+16 bit evtCount"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_IMPL, "IMPL PMU"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64dfr0_tracever[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_TraceVer_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64DFR0_TraceVer_IMPL, "Trace"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64dfr0_debugver[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8, "Debugv8"),
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8_VHE, "Debugv8_VHE"),
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8_2, "Debugv8.2"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field id_aa64dfr0_fields[] = {
	MRS_FIELD(ID_AA64DFR0, PMSVer, false, MRS_EXACT, id_aa64dfr0_pmsver),
	MRS_FIELD(ID_AA64DFR0, CTX_CMPs, false, MRS_EXACT,
	    id_aa64dfr0_ctx_cmps),
	MRS_FIELD(ID_AA64DFR0, WRPs, false, MRS_EXACT, id_aa64dfr0_wrps),
	MRS_FIELD(ID_AA64DFR0, BRPs, false, MRS_LOWER, id_aa64dfr0_brps),
	MRS_FIELD(ID_AA64DFR0, PMUVer, false, MRS_EXACT, id_aa64dfr0_pmuver),
	MRS_FIELD(ID_AA64DFR0, TraceVer, false, MRS_EXACT,
	    id_aa64dfr0_tracever),
	MRS_FIELD(ID_AA64DFR0, DebugVer, false, MRS_EXACT_VAL(0x6),
	    id_aa64dfr0_debugver),
	MRS_FIELD_END,
};


/* ID_AA64DFR1 */
static struct mrs_field id_aa64dfr1_fields[] = {
	MRS_FIELD_END,
};


/* ID_AA64ISAR0_EL1 */
static struct mrs_field_value id_aa64isar0_dp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, DP, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_sm4[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SM4, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_sm3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SM3, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_sha3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SHA3, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_rdm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, RDM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_atomic[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, Atomic, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_crc32[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, CRC32, NONE, BASE),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_sha2[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SHA2, NONE, BASE),
	MRS_FIELD_VALUE(ID_AA64ISAR0_SHA2_512, "SHA2+SHA512"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_sha1[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SHA1, NONE, BASE),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar0_aes[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, AES, NONE, BASE),
	MRS_FIELD_VALUE(ID_AA64ISAR0_AES_PMULL, "AES+PMULL"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field id_aa64isar0_fields[] = {
	MRS_FIELD(ID_AA64ISAR0, DP, false, MRS_LOWER, id_aa64isar0_dp),
	MRS_FIELD(ID_AA64ISAR0, SM4, false, MRS_LOWER, id_aa64isar0_sm4),
	MRS_FIELD(ID_AA64ISAR0, SM3, false, MRS_LOWER, id_aa64isar0_sm3),
	MRS_FIELD(ID_AA64ISAR0, SHA3, false, MRS_LOWER, id_aa64isar0_sha3),
	MRS_FIELD(ID_AA64ISAR0, RDM, false, MRS_LOWER, id_aa64isar0_rdm),
	MRS_FIELD(ID_AA64ISAR0, Atomic, false, MRS_LOWER, id_aa64isar0_atomic),
	MRS_FIELD(ID_AA64ISAR0, CRC32, false, MRS_LOWER, id_aa64isar0_crc32),
	MRS_FIELD(ID_AA64ISAR0, SHA2, false, MRS_LOWER, id_aa64isar0_sha2),
	MRS_FIELD(ID_AA64ISAR0, SHA1, false, MRS_LOWER, id_aa64isar0_sha1),
	MRS_FIELD(ID_AA64ISAR0, AES, false, MRS_LOWER, id_aa64isar0_aes),
	MRS_FIELD_END,
};


/* ID_AA64ISAR1_EL1 */
static struct mrs_field_value id_aa64isar1_gpi[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, GPI, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar1_gpa[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, GPA, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar1_lrcpc[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, LRCPC, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar1_fcma[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, FCMA, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar1_jscvt[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, JSCVT, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar1_api[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, API, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar1_apa[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, GPA, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64isar1_dpb[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, DPB, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field id_aa64isar1_fields[] = {
	MRS_FIELD(ID_AA64ISAR1, GPI, false, MRS_EXACT, id_aa64isar1_gpi),
	MRS_FIELD(ID_AA64ISAR1, GPA, false, MRS_EXACT, id_aa64isar1_gpa),
	MRS_FIELD(ID_AA64ISAR1, LRCPC, false, MRS_LOWER, id_aa64isar1_lrcpc),
	MRS_FIELD(ID_AA64ISAR1, FCMA, false, MRS_LOWER, id_aa64isar1_fcma),
	MRS_FIELD(ID_AA64ISAR1, JSCVT, false, MRS_LOWER, id_aa64isar1_jscvt),
	MRS_FIELD(ID_AA64ISAR1, API, false, MRS_EXACT, id_aa64isar1_api),
	MRS_FIELD(ID_AA64ISAR1, APA, false, MRS_EXACT, id_aa64isar1_apa),
	MRS_FIELD(ID_AA64ISAR1, DPB, false, MRS_LOWER, id_aa64isar1_dpb),
	MRS_FIELD_END,
};


/* ID_AA64MMFR0_EL1 */
static struct mrs_field_value id_aa64mmfr0_tgran4[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, TGran4, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr0_tgran64[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, TGran64, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr0_tgran16[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, TGran16, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr0_bigend_el0[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, BigEndEL0, FIXED, MIXED),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr0_snsmem[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, SNSMem, NONE, DISTINCT),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr0_bigend[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, BigEnd, FIXED, MIXED),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr0_asid_bits[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR0_ASIDBits_8, "8bit ASID"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_ASIDBits_16, "16bit ASID"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr0_parange[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_4G, "4GB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_64G, "64GB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_1T, "1TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_4T, "4TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_16T, "16TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_256T, "256TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_4P, "4PB PA"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field id_aa64mmfr0_fields[] = {
	MRS_FIELD(ID_AA64MMFR0, TGran4, false, MRS_EXACT, id_aa64mmfr0_tgran4),
	MRS_FIELD(ID_AA64MMFR0, TGran64, false, MRS_EXACT,
	    id_aa64mmfr0_tgran64),
	MRS_FIELD(ID_AA64MMFR0, TGran16, false, MRS_EXACT,
	    id_aa64mmfr0_tgran16),
	MRS_FIELD(ID_AA64MMFR0, BigEndEL0, false, MRS_EXACT,
	    id_aa64mmfr0_bigend_el0),
	MRS_FIELD(ID_AA64MMFR0, SNSMem, false, MRS_EXACT, id_aa64mmfr0_snsmem),
	MRS_FIELD(ID_AA64MMFR0, BigEnd, false, MRS_EXACT, id_aa64mmfr0_bigend),
	MRS_FIELD(ID_AA64MMFR0, ASIDBits, false, MRS_EXACT,
	    id_aa64mmfr0_asid_bits),
	MRS_FIELD(ID_AA64MMFR0, PARange, false, MRS_EXACT,
	    id_aa64mmfr0_parange),
	MRS_FIELD_END,
};


/* ID_AA64MMFR1_EL1 */
static struct mrs_field_value id_aa64mmfr1_xnx[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, XNX, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr1_specsei[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, SpecSEI, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr1_pan[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, PAN, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64MMFR1_PAN_ATS1E1, "PAN+ATS1E1"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr1_lo[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, LO, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr1_hpds[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR1_HPDS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HPDS_HPD, "HPD"),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HPDS_TTPBHA, "HPD+TTPBHA"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr1_vh[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, VH, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr1_vmidbits[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR1_VMIDBits_8, "8bit VMID"),
	MRS_FIELD_VALUE(ID_AA64MMFR1_VMIDBits_16, "16bit VMID"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr1_hafdbs[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR1_HAFDBS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HAFDBS_AF, "HAF"),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HAFDBS_AF_DBS, "HAF+DS"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field id_aa64mmfr1_fields[] = {
	MRS_FIELD(ID_AA64MMFR1, XNX, false, MRS_EXACT, id_aa64mmfr1_xnx),
	MRS_FIELD(ID_AA64MMFR1, SpecSEI, false, MRS_EXACT,
	    id_aa64mmfr1_specsei),
	MRS_FIELD(ID_AA64MMFR1, PAN, false, MRS_EXACT, id_aa64mmfr1_pan),
	MRS_FIELD(ID_AA64MMFR1, LO, false, MRS_EXACT, id_aa64mmfr1_lo),
	MRS_FIELD(ID_AA64MMFR1, HPDS, false, MRS_EXACT, id_aa64mmfr1_hpds),
	MRS_FIELD(ID_AA64MMFR1, VH, false, MRS_EXACT, id_aa64mmfr1_vh),
	MRS_FIELD(ID_AA64MMFR1, VMIDBits, false, MRS_EXACT,
	    id_aa64mmfr1_vmidbits),
	MRS_FIELD(ID_AA64MMFR1, HAFDBS, false, MRS_EXACT, id_aa64mmfr1_hafdbs),
	MRS_FIELD_END,
};


/* ID_AA64MMFR2_EL1 */
static struct mrs_field_value id_aa64mmfr2_nv[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, NV, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr2_ccidx[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR2_CCIDX_32, "32bit CCIDX"),
	MRS_FIELD_VALUE(ID_AA64MMFR2_CCIDX_64, "32bit CCIDX"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr2_varange[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR2_VARange_48, "48bit VA"),
	MRS_FIELD_VALUE(ID_AA64MMFR2_VARange_52, "52bit VA"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr2_iesb[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, IESB, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr2_lsm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, LSM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr2_uao[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, UAO, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64mmfr2_cnp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, CnP, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field id_aa64mmfr2_fields[] = {
	MRS_FIELD(ID_AA64MMFR2, NV, false, MRS_EXACT, id_aa64mmfr2_nv),
	MRS_FIELD(ID_AA64MMFR2, CCIDX, false, MRS_EXACT, id_aa64mmfr2_ccidx),
	MRS_FIELD(ID_AA64MMFR2, VARange, false, MRS_EXACT,
	    id_aa64mmfr2_varange),
	MRS_FIELD(ID_AA64MMFR2, IESB, false, MRS_EXACT, id_aa64mmfr2_iesb),
	MRS_FIELD(ID_AA64MMFR2, LSM, false, MRS_EXACT, id_aa64mmfr2_lsm),
	MRS_FIELD(ID_AA64MMFR2, UAO, false, MRS_EXACT, id_aa64mmfr2_uao),
	MRS_FIELD(ID_AA64MMFR2, CnP, false, MRS_EXACT, id_aa64mmfr2_cnp),
	MRS_FIELD_END,
};


/* ID_AA64PFR0_EL1 */
static struct mrs_field_value id_aa64pfr0_sve[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, SVE, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_ras[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_RAS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR0_RAS_V1, "RASv1"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_gic[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, GIC, CPUIF_NONE, CPUIF_EN),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_advsimd[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, AdvSIMD, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64PFR0_AdvSIMD_HP, "AdvSIMD+HP"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_fp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, FP, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64PFR0_FP_HP, "FP+HP"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_el3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, EL3, NONE, 64),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL3_64_32, "EL3 32"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_el2[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, EL2, NONE, 64),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL2_64_32, "EL2 32"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_el1[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_EL1_64, "EL1"),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL1_64_32, "EL1 32"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field_value id_aa64pfr0_el0[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_EL0_64, "EL0"),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL0_64_32, "EL0 32"),
	MRS_FIELD_VALUE_END,
};

static struct mrs_field id_aa64pfr0_fields[] = {
	MRS_FIELD(ID_AA64PFR0, SVE, false, MRS_EXACT, id_aa64pfr0_sve),
	MRS_FIELD(ID_AA64PFR0, RAS, false, MRS_EXACT, id_aa64pfr0_ras),
	MRS_FIELD(ID_AA64PFR0, GIC, false, MRS_EXACT, id_aa64pfr0_gic),
	MRS_FIELD(ID_AA64PFR0, AdvSIMD, true, MRS_LOWER, id_aa64pfr0_advsimd),
	MRS_FIELD(ID_AA64PFR0, FP, true,  MRS_LOWER, id_aa64pfr0_fp),
	MRS_FIELD(ID_AA64PFR0, EL3, false, MRS_EXACT, id_aa64pfr0_el3),
	MRS_FIELD(ID_AA64PFR0, EL2, false, MRS_EXACT, id_aa64pfr0_el2),
	MRS_FIELD(ID_AA64PFR0, EL1, false, MRS_LOWER, id_aa64pfr0_el1),
	MRS_FIELD(ID_AA64PFR0, EL0, false, MRS_LOWER, id_aa64pfr0_el0),
	MRS_FIELD_END,
};


/* ID_AA64PFR1_EL1 */
static struct mrs_field id_aa64pfr1_fields[] = {
	MRS_FIELD_END,
};

struct mrs_user_reg {
	u_int		reg;
	u_int		CRm;
	u_int		Op2;
	size_t		offset;
	struct mrs_field *fields;
};

static struct mrs_user_reg user_regs[] = {
	{	/* id_aa64isar0_el1 */
		.reg = ID_AA64ISAR0_EL1,
		.CRm = 6,
		.Op2 = 0,
		.offset = __offsetof(struct cpu_desc, id_aa64isar0),
		.fields = id_aa64isar0_fields,
	},
	{	/* id_aa64isar1_el1 */
		.reg = ID_AA64ISAR1_EL1,
		.CRm = 6,
		.Op2 = 1,
		.offset = __offsetof(struct cpu_desc, id_aa64isar1),
		.fields = id_aa64isar1_fields,
	},
	{	/* id_aa64pfr0_el1 */
		.reg = ID_AA64PFR0_EL1,
		.CRm = 4,
		.Op2 = 0,
		.offset = __offsetof(struct cpu_desc, id_aa64pfr0),
		.fields = id_aa64pfr0_fields,
	},
	{	/* id_aa64dfr0_el1 */
		.reg = ID_AA64DFR0_EL1,
		.CRm = 5,
		.Op2 = 0,
		.offset = __offsetof(struct cpu_desc, id_aa64dfr0),
		.fields = id_aa64dfr0_fields,
	},
};

#define	CPU_DESC_FIELD(desc, idx)					\
    *(uint64_t *)((char *)&(desc) + user_regs[(idx)].offset)

static int
user_mrs_handler(vm_offset_t va, uint32_t insn, struct trapframe *frame,
    uint32_t esr)
{
	uint64_t value;
	int CRm, Op2, i, reg;

	if ((insn & MRS_MASK) != MRS_VALUE)
		return (0);

	/*
	 * We only emulate Op0 == 3, Op1 == 0, CRn == 0, CRm == {0, 4-7}.
	 * These are in the EL1 CPU identification space.
	 * CRm == 0 holds MIDR_EL1, MPIDR_EL1, and REVID_EL1.
	 * CRm == {4-7} holds the ID_AA64 registers.
	 *
	 * For full details see the ARMv8 ARM (ARM DDI 0487C.a)
	 * Table D9-2 System instruction encodings for non-Debug System
	 * register accesses.
	 */
	if (mrs_Op0(insn) != 3 || mrs_Op1(insn) != 0 || mrs_CRn(insn) != 0)
		return (0);

	CRm = mrs_CRm(insn);
	if (CRm > 7 || (CRm < 4 && CRm != 0))
		return (0);

	Op2 = mrs_Op2(insn);
	value = 0;

	for (i = 0; i < nitems(user_regs); i++) {
		if (user_regs[i].CRm == CRm && user_regs[i].Op2 == Op2) {
			value = CPU_DESC_FIELD(user_cpu_desc, i);
			break;
		}
	}

	if (CRm == 0) {
		switch (Op2) {
		case 0:
			value = READ_SPECIALREG(midr_el1);
			break;
		case 5:
			value = READ_SPECIALREG(mpidr_el1);
			break;
		case 6:
			value = READ_SPECIALREG(revidr_el1);
			break;
		default:
			return (0);
		}
	}

	/*
	 * We will handle this instruction, move to the next so we
	 * don't trap here again.
	 */
	frame->tf_elr += INSN_SIZE;

	reg = MRS_REGISTER(insn);
	/* If reg is 31 then write to xzr, i.e. do nothing */
	if (reg == 31)
		return (1);

	if (reg < nitems(frame->tf_x))
		frame->tf_x[reg] = value;
	else if (reg == 30)
		frame->tf_lr = value;

	return (1);
}

bool
extract_user_id_field(u_int reg, u_int field_shift, uint8_t *val)
{
	uint64_t value;
	int i;

	for (i = 0; i < nitems(user_regs); i++) {
		if (user_regs[i].reg == reg) {
			value = CPU_DESC_FIELD(user_cpu_desc, i);
			*val = value >> field_shift;
			return (true);
		}
	}

	return (false);
}

static void
update_user_regs(u_int cpu)
{
	struct mrs_field *fields;
	uint64_t cur, value;
	int i, j, cur_field, new_field;

	for (i = 0; i < nitems(user_regs); i++) {
		value = CPU_DESC_FIELD(cpu_desc[cpu], i);
		if (cpu == 0)
			cur = value;
		else
			cur = CPU_DESC_FIELD(user_cpu_desc, i);

		fields = user_regs[i].fields;
		for (j = 0; fields[j].type != 0; j++) {
			switch (fields[j].type & MRS_TYPE_MASK) {
			case MRS_EXACT:
				cur &= ~(0xfu << fields[j].shift);
				cur |=
				    (uint64_t)MRS_EXACT_FIELD(fields[j].type) <<
				    fields[j].shift;
				break;
			case MRS_LOWER:
				new_field = (value >> fields[j].shift) & 0xf;
				cur_field = (cur >> fields[j].shift) & 0xf;
				if ((fields[j].sign &&
				     (int)new_field < (int)cur_field) ||
				    (!fields[j].sign &&
				     (u_int)new_field < (u_int)cur_field)) {
					cur &= ~(0xfu << fields[j].shift);
					cur |= new_field << fields[j].shift;
				}
				break;
			default:
				panic("Invalid field type: %d", fields[j].type);
			}
		}

		CPU_DESC_FIELD(user_cpu_desc, i) = cur;
	}
}

/* HWCAP */
extern u_long elf_hwcap;

static void
identify_cpu_sysinit(void *dummy __unused)
{
	int cpu;
	u_long hwcap;

	/* Create a user visible cpu description with safe values */
	memset(&user_cpu_desc, 0, sizeof(user_cpu_desc));
	/* Safe values for these registers */
	user_cpu_desc.id_aa64pfr0 = ID_AA64PFR0_AdvSIMD_NONE |
	    ID_AA64PFR0_FP_NONE | ID_AA64PFR0_EL1_64 | ID_AA64PFR0_EL0_64;
	user_cpu_desc.id_aa64dfr0 = ID_AA64DFR0_DebugVer_8;


	CPU_FOREACH(cpu) {
		print_cpu_features(cpu);
		hwcap = parse_cpu_features_hwcap(cpu);
		if (elf_hwcap == 0)
			elf_hwcap = hwcap;
		else
			elf_hwcap &= hwcap;
		update_user_regs(cpu);
	}

	install_undef_handler(true, user_mrs_handler);
}
SYSINIT(idenrity_cpu, SI_SUB_SMP, SI_ORDER_ANY, identify_cpu_sysinit, NULL);

static u_long
parse_cpu_features_hwcap(u_int cpu)
{
	u_long hwcap = 0;

	if (ID_AA64ISAR0_DP_VAL(cpu_desc[cpu].id_aa64isar0) == ID_AA64ISAR0_DP_IMPL)
		hwcap |= HWCAP_ASIMDDP;

	if (ID_AA64ISAR0_SM4_VAL(cpu_desc[cpu].id_aa64isar0) == ID_AA64ISAR0_SM4_IMPL)
		hwcap |= HWCAP_SM4;

	if (ID_AA64ISAR0_SM3_VAL(cpu_desc[cpu].id_aa64isar0) == ID_AA64ISAR0_SM3_IMPL)
		hwcap |= HWCAP_SM3;

	if (ID_AA64ISAR0_RDM_VAL(cpu_desc[cpu].id_aa64isar0) == ID_AA64ISAR0_RDM_IMPL)
		hwcap |= HWCAP_ASIMDRDM;

	if (ID_AA64ISAR0_Atomic_VAL(cpu_desc[cpu].id_aa64isar0) == ID_AA64ISAR0_Atomic_IMPL)
		hwcap |= HWCAP_ATOMICS;

	if (ID_AA64ISAR0_CRC32_VAL(cpu_desc[cpu].id_aa64isar0) == ID_AA64ISAR0_CRC32_BASE)
		hwcap |= HWCAP_CRC32;

	switch (ID_AA64ISAR0_SHA2_VAL(cpu_desc[cpu].id_aa64isar0)) {
		case ID_AA64ISAR0_SHA2_BASE:
			hwcap |= HWCAP_SHA2;
			break;
		case ID_AA64ISAR0_SHA2_512:
			hwcap |= HWCAP_SHA2 | HWCAP_SHA512;
			break;
	default:
		break;
	}

	if (ID_AA64ISAR0_SHA1_VAL(cpu_desc[cpu].id_aa64isar0))
		hwcap |= HWCAP_SHA1;

	switch (ID_AA64ISAR0_AES_VAL(cpu_desc[cpu].id_aa64isar0)) {
	case ID_AA64ISAR0_AES_BASE:
		hwcap |= HWCAP_AES;
		break;
	case ID_AA64ISAR0_AES_PMULL:
		hwcap |= HWCAP_PMULL | HWCAP_AES;
		break;
	default:
		break;
	}

	if (ID_AA64ISAR1_LRCPC_VAL(cpu_desc[cpu].id_aa64isar1) == ID_AA64ISAR1_LRCPC_IMPL)
		hwcap |= HWCAP_LRCPC;

	if (ID_AA64ISAR1_FCMA_VAL(cpu_desc[cpu].id_aa64isar1) == ID_AA64ISAR1_FCMA_IMPL)
		hwcap |= HWCAP_FCMA;

	if (ID_AA64ISAR1_JSCVT_VAL(cpu_desc[cpu].id_aa64isar1) == ID_AA64ISAR1_JSCVT_IMPL)
		hwcap |= HWCAP_JSCVT;

	if (ID_AA64ISAR1_DPB_VAL(cpu_desc[cpu].id_aa64isar1) == ID_AA64ISAR1_DPB_IMPL)
		hwcap |= HWCAP_DCPOP;

	if (ID_AA64PFR0_SVE_VAL(cpu_desc[cpu].id_aa64pfr0) == ID_AA64PFR0_SVE_IMPL)
		hwcap |= HWCAP_SVE;

	switch (ID_AA64PFR0_AdvSIMD_VAL(cpu_desc[cpu].id_aa64pfr0)) {
	case ID_AA64PFR0_AdvSIMD_IMPL:
		hwcap |= HWCAP_ASIMD;
		break;
	case ID_AA64PFR0_AdvSIMD_HP:
		hwcap |= HWCAP_ASIMD | HWCAP_ASIMDDP;
		break;
	default:
		break;
	}

	switch (ID_AA64PFR0_FP_VAL(cpu_desc[cpu].id_aa64pfr0)) {
	case ID_AA64PFR0_FP_IMPL:
		hwcap |= HWCAP_FP;
		break;
	case ID_AA64PFR0_FP_HP:
		hwcap |= HWCAP_FP | HWCAP_FPHP;
		break;
	default:
		break;
	}

	return (hwcap);
}

static void
print_id_register(struct sbuf *sb, const char *reg_name, uint64_t reg,
    struct mrs_field *fields)
{
	struct mrs_field_value *fv;
	int field, i, j, printed;

	sbuf_printf(sb, "%29s = <", reg_name);

#define SEP_STR	((printed++) == 0) ? "" : ","
	printed = 0;
	for (i = 0; fields[i].type != 0; i++) {
		fv = fields[i].values;

		/* TODO: Handle with an unknown message */
		if (fv == NULL)
			continue;

		field = (reg & fields[i].mask) >> fields[i].shift;
		for (j = 0; fv[j].desc != NULL; j++) {
			if ((fv[j].value >> fields[i].shift) != field)
				continue;

			if (fv[j].desc[0] != '\0')
				sbuf_printf(sb, "%s%s", SEP_STR, fv[j].desc);
				break;
		}
		if (fv[j].desc == NULL)
			sbuf_printf(sb, "%sUnknown %s(%x)", SEP_STR,
			    fields[i].name, field);

		reg &= ~(0xful << fields[i].shift);
	}

	if (reg != 0)
		sbuf_printf(sb, "%s%#lx", SEP_STR, reg);
#undef SEP_STR

	sbuf_finish(sb);
	printf("%s>\n", sbuf_data(sb));
	sbuf_clear(sb);
}

static void
print_cpu_features(u_int cpu)
{
	struct sbuf *sb;

	sb = sbuf_new_auto();
	sbuf_printf(sb, "CPU%3d: %s %s r%dp%d", cpu,
	    cpu_desc[cpu].cpu_impl_name, cpu_desc[cpu].cpu_part_name,
	    cpu_desc[cpu].cpu_variant, cpu_desc[cpu].cpu_revision);

	sbuf_cat(sb, " affinity:");
	switch(cpu_aff_levels) {
	default:
	case 4:
		sbuf_printf(sb, " %2d", CPU_AFF3(cpu_desc[cpu].mpidr));
		/* FALLTHROUGH */
	case 3:
		sbuf_printf(sb, " %2d", CPU_AFF2(cpu_desc[cpu].mpidr));
		/* FALLTHROUGH */
	case 2:
		sbuf_printf(sb, " %2d", CPU_AFF1(cpu_desc[cpu].mpidr));
		/* FALLTHROUGH */
	case 1:
	case 0: /* On UP this will be zero */
		sbuf_printf(sb, " %2d", CPU_AFF0(cpu_desc[cpu].mpidr));
		break;
	}
	sbuf_finish(sb);
	printf("%s\n", sbuf_data(sb));
	sbuf_clear(sb);

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
	 * XXX: CPU_MATCH_ERRATA_CAVIUM_THUNDERX_1_1 on its own also
	 * triggers on pass 2.0+.
	 */
	if (cpu == 0 && CPU_VAR(PCPU_GET(midr)) == 0 &&
	    CPU_MATCH_ERRATA_CAVIUM_THUNDERX_1_1)
		printf("WARNING: ThunderX Pass 1.1 detected.\nThis has known "
		    "hardware bugs that may cause the incorrect operation of "
		    "atomic operations.\n");

	/* AArch64 Instruction Set Attribute Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_ISAR0) != 0)
		print_id_register(sb, "Instruction Set Attributes 0",
		    cpu_desc[cpu].id_aa64isar0, id_aa64isar0_fields);

	/* AArch64 Instruction Set Attribute Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_ISAR1) != 0)
		print_id_register(sb, "Instruction Set Attributes 1",
		    cpu_desc[cpu].id_aa64isar1, id_aa64isar1_fields);

	/* AArch64 Processor Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_PFR0) != 0)
		print_id_register(sb, "Processor Features 0",
		    cpu_desc[cpu].id_aa64pfr0, id_aa64pfr0_fields);

	/* AArch64 Processor Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_PFR1) != 0)
		print_id_register(sb, "Processor Features 1",
		    cpu_desc[cpu].id_aa64pfr1, id_aa64pfr1_fields);

	/* AArch64 Memory Model Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_MMFR0) != 0)
		print_id_register(sb, "Memory Model Features 0",
		    cpu_desc[cpu].id_aa64mmfr0, id_aa64mmfr0_fields);

	/* AArch64 Memory Model Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_MMFR1) != 0)
		print_id_register(sb, "Memory Model Features 1",
		    cpu_desc[cpu].id_aa64mmfr1, id_aa64mmfr1_fields);

	/* AArch64 Memory Model Feature Register 2 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_MMFR2) != 0)
		print_id_register(sb, "Memory Model Features 2",
		    cpu_desc[cpu].id_aa64mmfr2, id_aa64mmfr2_fields);

	/* AArch64 Debug Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_DFR0) != 0)
		print_id_register(sb, "Debug Features 0",
		    cpu_desc[cpu].id_aa64dfr0, id_aa64dfr0_fields);

	/* AArch64 Memory Model Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_DFR1) != 0)
		print_id_register(sb, "Debug Features 1",
		    cpu_desc[cpu].id_aa64dfr1, id_aa64dfr1_fields);

	/* AArch64 Auxiliary Feature Register 0 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_AFR0) != 0)
		print_id_register(sb, "Auxiliary Features 0",
		    cpu_desc[cpu].id_aa64afr0, id_aa64afr0_fields);

	/* AArch64 Auxiliary Feature Register 1 */
	if (cpu == 0 || (cpu_print_regs & PRINT_ID_AA64_AFR1) != 0)
		print_id_register(sb, "Auxiliary Features 1",
		    cpu_desc[cpu].id_aa64afr1, id_aa64afr1_fields);

	sbuf_delete(sb);
	sb = NULL;
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

	snprintf(cpu_model, sizeof(cpu_model), "%s %s r%dp%d",
	    cpu_desc[cpu].cpu_impl_name, cpu_desc[cpu].cpu_part_name,
	    cpu_desc[cpu].cpu_variant, cpu_desc[cpu].cpu_revision);

	/* Save affinity for current CPU */
	cpu_desc[cpu].mpidr = get_mpidr();
	CPU_AFFINITY(cpu) = cpu_desc[cpu].mpidr & CPU_AFF_MASK;

	cpu_desc[cpu].id_aa64dfr0 = READ_SPECIALREG(id_aa64dfr0_el1);
	cpu_desc[cpu].id_aa64dfr1 = READ_SPECIALREG(id_aa64dfr1_el1);
	cpu_desc[cpu].id_aa64isar0 = READ_SPECIALREG(id_aa64isar0_el1);
	cpu_desc[cpu].id_aa64isar1 = READ_SPECIALREG(id_aa64isar1_el1);
	cpu_desc[cpu].id_aa64mmfr0 = READ_SPECIALREG(id_aa64mmfr0_el1);
	cpu_desc[cpu].id_aa64mmfr1 = READ_SPECIALREG(id_aa64mmfr1_el1);
	cpu_desc[cpu].id_aa64mmfr2 = READ_SPECIALREG(id_aa64mmfr2_el1);
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
		if (cpu_desc[cpu].id_aa64mmfr2 != cpu_desc[0].id_aa64mmfr2)
			cpu_print_regs |= PRINT_ID_AA64_MMFR2;

		if (cpu_desc[cpu].id_aa64pfr0 != cpu_desc[0].id_aa64pfr0)
			cpu_print_regs |= PRINT_ID_AA64_PFR0;
		if (cpu_desc[cpu].id_aa64pfr1 != cpu_desc[0].id_aa64pfr1)
			cpu_print_regs |= PRINT_ID_AA64_PFR1;

		/* Wake up the other CPUs */
		atomic_store_rel_int(&ident_lock, 0);
		__asm __volatile("sev" ::: "memory");
	}
}
