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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/systm.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/undefined.h>

static MALLOC_DEFINE(M_IDENTCPU, "CPU ID", "arm64 CPU identification memory");

struct cpu_desc;

static void print_cpu_midr(struct sbuf *sb, u_int cpu);
static void print_cpu_features(u_int cpu, struct cpu_desc *desc,
    struct cpu_desc *prev_desc);
static void print_cpu_caches(struct sbuf *sb, struct cpu_desc *desc);
#ifdef COMPAT_FREEBSD32
static u_long parse_cpu_features_hwcap32(void);
#endif

char machine[] = "arm64";

#ifdef SCTL_MASK32
extern int adaptive_machine_arch;
#endif

static SYSCTL_NODE(_machdep, OID_AUTO, cache, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Cache management tuning");

static int allow_dic = 1;
SYSCTL_INT(_machdep_cache, OID_AUTO, allow_dic, CTLFLAG_RDTUN, &allow_dic, 0,
    "Allow optimizations based on the DIC cache bit");

static int allow_idc = 1;
SYSCTL_INT(_machdep_cache, OID_AUTO, allow_idc, CTLFLAG_RDTUN, &allow_idc, 0,
    "Allow optimizations based on the IDC cache bit");

static void check_cpu_regs(u_int cpu, struct cpu_desc *desc,
    struct cpu_desc *prev_desc);

/*
 * The default implementation of I-cache sync assumes we have an
 * aliasing cache until we know otherwise.
 */
void (*arm64_icache_sync_range)(void *, vm_size_t) =
    &arm64_aliasing_icache_sync_range;

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
	 CTLFLAG_CAPRD | CTLFLAG_MPSAFE, NULL, 0, sysctl_hw_machine, "A",
	 "Machine class");

static char cpu_model[64];
SYSCTL_STRING(_hw, HW_MODEL, model, CTLFLAG_RD | CTLFLAG_CAPRD,
	cpu_model, sizeof(cpu_model), "Machine model");

#define	MAX_CACHES	8	/* Maximum number of caches supported
				   architecturally. */
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
	uint64_t	mpidr;
	uint64_t	id_aa64afr0;
	uint64_t	id_aa64afr1;
	uint64_t	id_aa64dfr0;
	uint64_t	id_aa64dfr1;
	uint64_t	id_aa64isar0;
	uint64_t	id_aa64isar1;
	uint64_t	id_aa64isar2;
	uint64_t	id_aa64mmfr0;
	uint64_t	id_aa64mmfr1;
	uint64_t	id_aa64mmfr2;
#ifdef NOTYET
	uint64_t	id_aa64mmfr3;
	uint64_t	id_aa64mmfr4;
#endif
	uint64_t	id_aa64pfr0;
	uint64_t	id_aa64pfr1;
#ifdef NOTYET
	uint64_t	id_aa64pfr2;
#endif
	uint64_t	id_aa64zfr0;
	uint64_t	ctr;
#ifdef COMPAT_FREEBSD32
	uint64_t	id_isar5;
	uint64_t	mvfr0;
	uint64_t	mvfr1;
#endif
	uint64_t	clidr;
	uint32_t	ccsidr[MAX_CACHES][2]; /* 2 possible types. */
	bool		have_sve;
};

static struct cpu_desc cpu_desc0;
static struct cpu_desc *cpu_desc;
static struct cpu_desc kern_cpu_desc;
static struct cpu_desc user_cpu_desc;
static struct cpu_desc l_user_cpu_desc;

static struct cpu_desc *
get_cpu_desc(u_int cpu)
{
	/* The cpu_desc for CPU 0 is used before the allocator is ready. */
	if (cpu == 0)
		return (&cpu_desc0);

	MPASS(cpu_desc != NULL);
	return (&cpu_desc[cpu - 1]);
}

struct cpu_parts {
	u_int		part_id;
	const char	*part_name;
};
#define	CPU_PART_NONE	{ 0, NULL }

struct cpu_implementers {
	u_int			impl_id;
	const char		*impl_name;
	/*
	 * Part number is implementation defined
	 * so each vendor will have its own set of values and names.
	 */
	const struct cpu_parts	*cpu_parts;
};
#define	CPU_IMPLEMENTER_NONE	{ 0, NULL, NULL }

/*
 * Per-implementer table of (PartNum, CPU Name) pairs.
 */
/* ARM Ltd. */
static const struct cpu_parts cpu_parts_arm[] = {
	{ CPU_PART_AEM_V8, "AEMv8" },
	{ CPU_PART_FOUNDATION, "Foundation-Model" },
	{ CPU_PART_CORTEX_A34, "Cortex-A34" },
	{ CPU_PART_CORTEX_A35, "Cortex-A35" },
	{ CPU_PART_CORTEX_A53, "Cortex-A53" },
	{ CPU_PART_CORTEX_A55, "Cortex-A55" },
	{ CPU_PART_CORTEX_A57, "Cortex-A57" },
	{ CPU_PART_CORTEX_A65, "Cortex-A65" },
	{ CPU_PART_CORTEX_A65AE, "Cortex-A65AE" },
	{ CPU_PART_CORTEX_A72, "Cortex-A72" },
	{ CPU_PART_CORTEX_A73, "Cortex-A73" },
	{ CPU_PART_CORTEX_A75, "Cortex-A75" },
	{ CPU_PART_CORTEX_A76, "Cortex-A76" },
	{ CPU_PART_CORTEX_A76AE, "Cortex-A76AE" },
	{ CPU_PART_CORTEX_A77, "Cortex-A77" },
	{ CPU_PART_CORTEX_A78, "Cortex-A78" },
	{ CPU_PART_CORTEX_A78C, "Cortex-A78C" },
	{ CPU_PART_CORTEX_A510, "Cortex-A510" },
	{ CPU_PART_CORTEX_A710, "Cortex-A710" },
	{ CPU_PART_CORTEX_A715, "Cortex-A715" },
	{ CPU_PART_CORTEX_X1, "Cortex-X1" },
	{ CPU_PART_CORTEX_X1C, "Cortex-X1C" },
	{ CPU_PART_CORTEX_X2, "Cortex-X2" },
	{ CPU_PART_CORTEX_X3, "Cortex-X3" },
	{ CPU_PART_NEOVERSE_E1, "Neoverse-E1" },
	{ CPU_PART_NEOVERSE_N1, "Neoverse-N1" },
	{ CPU_PART_NEOVERSE_N2, "Neoverse-N2" },
	{ CPU_PART_NEOVERSE_V1, "Neoverse-V1" },
	{ CPU_PART_NEOVERSE_V2, "Neoverse-V2" },
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

/* Qualcomm */
static const struct cpu_parts cpu_parts_qcom[] = {
	{ CPU_PART_KRYO400_GOLD, "Kryo 400 Gold" },
	{ CPU_PART_KRYO400_SILVER, "Kryo 400 Silver" },
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
	{ CPU_IMPL_AMPERE,	"Ampere",	cpu_parts_none },
	{ CPU_IMPL_APPLE,	"Apple",	cpu_parts_none },
	{ CPU_IMPL_APM,		"APM",		cpu_parts_apm },
	{ CPU_IMPL_ARM,		"ARM",		cpu_parts_arm },
	{ CPU_IMPL_BROADCOM,	"Broadcom",	cpu_parts_none },
	{ CPU_IMPL_CAVIUM,	"Cavium",	cpu_parts_cavium },
	{ CPU_IMPL_DEC,		"DEC",		cpu_parts_none },
	{ CPU_IMPL_FREESCALE,	"Freescale",	cpu_parts_none },
	{ CPU_IMPL_FUJITSU,	"Fujitsu",	cpu_parts_none },
	{ CPU_IMPL_INFINEON,	"IFX",		cpu_parts_none },
	{ CPU_IMPL_INTEL,	"Intel",	cpu_parts_none },
	{ CPU_IMPL_MARVELL,	"Marvell",	cpu_parts_none },
	{ CPU_IMPL_NVIDIA,	"NVIDIA",	cpu_parts_none },
	{ CPU_IMPL_QUALCOMM,	"Qualcomm",	cpu_parts_qcom },
	CPU_IMPLEMENTER_NONE,
};

#define	MRS_TYPE_MASK		0xf
#define	MRS_TYPE_FBSD_SHIFT	0
#define	MRS_TYPE_LNX_SHIFT	8
#define	MRS_INVALID		0
#define	MRS_EXACT		1
#define	MRS_EXACT_VAL(x)	(MRS_EXACT | ((x) << 4))
#define	MRS_EXACT_FIELD(x)	(((x) >> 4) & 0xf)
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

struct mrs_field_hwcap {
	uint64_t	min;
	u_long		hwcap_val;
	u_int		hwcap_id;
};

#define	MRS_HWCAP(_hwcap_id, _val, _min)			\
{								\
	.hwcap_id = (_hwcap_id),				\
	.hwcap_val = (_val),					\
	.min = (_min),						\
}

#define	MRS_HWCAP_END		{ .hwcap_id = 0 }

struct mrs_field {
	const char	*name;
	const struct mrs_field_value *values;
	const struct mrs_field_hwcap *hwcaps;
	uint64_t	mask;
	bool		sign;
	u_int		type;
	u_int		shift;
};

#define	MRS_FIELD_HWCAP_SPLIT(_register, _name, _sign, _fbsd_type,	\
    _lnx_type, _values, _hwcap)						\
	{								\
		.name = #_name,						\
		.sign = (_sign),					\
		.type = ((_fbsd_type) << MRS_TYPE_FBSD_SHIFT) |		\
		    ((_lnx_type) << MRS_TYPE_LNX_SHIFT),		\
		.shift = _register ## _ ## _name ## _SHIFT,		\
		.mask = _register ## _ ## _name ## _MASK,		\
		.values = (_values),					\
		.hwcaps = (_hwcap),					\
	}

#define	MRS_FIELD_HWCAP(_register, _name, _sign, _type, _values, _hwcap) \
	MRS_FIELD_HWCAP_SPLIT(_register, _name, _sign, _type, _type,	\
	    _values, _hwcap)

#define	MRS_FIELD(_register, _name, _sign, _type, _values)		\
	MRS_FIELD_HWCAP(_register, _name, _sign, _type, _values, NULL)

#define	MRS_FIELD_END	{ .type = MRS_INVALID, }

/* ID_AA64AFR0_EL1 */
static const struct mrs_field id_aa64afr0_fields[] = {
	MRS_FIELD_END,
};


/* ID_AA64AFR1_EL1 */
static const struct mrs_field id_aa64afr1_fields[] = {
	MRS_FIELD_END,
};


/* ID_AA64DFR0_EL1 */
static const struct mrs_field_value id_aa64dfr0_hpmn0[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64DFR0, HPMN0, CONSTR, DEFINED),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_brbe[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64DFR0, BRBE, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64DFR0_BRBE_EL3, "BRBE EL3"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_mtpmu[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64DFR0, MTPMU, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64DFR0_MTPMU_NONE_MT_RES0, "MTPMU res0"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_tracebuffer[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64DFR0, TraceBuffer, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_tracefilt[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_TraceFilt_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64DFR0_TraceFilt_8_4, "Trace v8.4"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_doublelock[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_DoubleLock_IMPL, "DoubleLock"),
	MRS_FIELD_VALUE(ID_AA64DFR0_DoubleLock_NONE, ""),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_pmsver[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_PMSVer_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMSVer_SPE, "SPE"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMSVer_SPE_1_1, "SPEv1p1"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMSVer_SPE_1_2, "SPEv1p2"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMSVer_SPE_1_3, "SPEv1p3"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_ctx_cmps[] = {
	MRS_FIELD_VALUE_COUNT(ID_AA64DFR0, CTX_CMPs, "CTX BKPT"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_wrps[] = {
	MRS_FIELD_VALUE_COUNT(ID_AA64DFR0, WRPs, "Watchpoint"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_brps[] = {
	MRS_FIELD_VALUE_COUNT(ID_AA64DFR0, BRPs, "Breakpoint"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_pmuver[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3, "PMUv3"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3_1, "PMUv3p1"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3_4, "PMUv3p4"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3_5, "PMUv3p5"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3_7, "PMUv3p7"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_3_8, "PMUv3p8"),
	MRS_FIELD_VALUE(ID_AA64DFR0_PMUVer_IMPL, "IMPL PMU"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_tracever[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_TraceVer_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64DFR0_TraceVer_IMPL, "Trace"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64dfr0_debugver[] = {
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8, "Debugv8"),
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8_VHE, "Debugv8_VHE"),
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8_2, "Debugv8p2"),
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8_4, "Debugv8p4"),
	MRS_FIELD_VALUE(ID_AA64DFR0_DebugVer_8_8, "Debugv8p8"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64dfr0_fields[] = {
	MRS_FIELD(ID_AA64DFR0, HPMN0, false, MRS_EXACT, id_aa64dfr0_hpmn0),
	MRS_FIELD(ID_AA64DFR0, BRBE, false, MRS_EXACT, id_aa64dfr0_brbe),
	MRS_FIELD(ID_AA64DFR0, MTPMU, true, MRS_EXACT, id_aa64dfr0_mtpmu),
	MRS_FIELD(ID_AA64DFR0, TraceBuffer, false, MRS_EXACT,
	    id_aa64dfr0_tracebuffer),
	MRS_FIELD(ID_AA64DFR0, TraceFilt, false, MRS_EXACT,
	    id_aa64dfr0_tracefilt),
	MRS_FIELD(ID_AA64DFR0, DoubleLock, false, MRS_EXACT,
	    id_aa64dfr0_doublelock),
	MRS_FIELD(ID_AA64DFR0, PMSVer, false, MRS_EXACT, id_aa64dfr0_pmsver),
	MRS_FIELD(ID_AA64DFR0, CTX_CMPs, false, MRS_EXACT,
	    id_aa64dfr0_ctx_cmps),
	MRS_FIELD(ID_AA64DFR0, WRPs, false, MRS_LOWER, id_aa64dfr0_wrps),
	MRS_FIELD(ID_AA64DFR0, BRPs, false, MRS_LOWER, id_aa64dfr0_brps),
	MRS_FIELD(ID_AA64DFR0, PMUVer, false, MRS_EXACT, id_aa64dfr0_pmuver),
	MRS_FIELD(ID_AA64DFR0, TraceVer, false, MRS_EXACT,
	    id_aa64dfr0_tracever),
	MRS_FIELD(ID_AA64DFR0, DebugVer, false, MRS_EXACT_VAL(0x6),
	    id_aa64dfr0_debugver),
	MRS_FIELD_END,
};


/* ID_AA64DFR1_EL1 */
static const struct mrs_field id_aa64dfr1_fields[] = {
	MRS_FIELD_END,
};


/* ID_AA64ISAR0_EL1 */
static const struct mrs_field_value id_aa64isar0_rndr[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR0_RNDR_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR0_RNDR_IMPL, "RNG"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_rndr_caps[] = {
	MRS_HWCAP(2, HWCAP2_RNG, ID_AA64ISAR0_RNDR_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_tlb[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR0_TLB_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR0_TLB_TLBIOS, "TLBI-OS"),
	MRS_FIELD_VALUE(ID_AA64ISAR0_TLB_TLBIOSR, "TLBI-OSR"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar0_ts[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR0_TS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR0_TS_CondM_8_4, "CondM-8.4"),
	MRS_FIELD_VALUE(ID_AA64ISAR0_TS_CondM_8_5, "CondM-8.5"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_ts_caps[] = {
	MRS_HWCAP(1, HWCAP_FLAGM, ID_AA64ISAR0_TS_CondM_8_4),
	MRS_HWCAP(2, HWCAP2_FLAGM2, ID_AA64ISAR0_TS_CondM_8_5),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_fhm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, FHM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_fhm_caps[] = {
	MRS_HWCAP(1, HWCAP_ASIMDFHM, ID_AA64ISAR0_FHM_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_dp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, DP, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_dp_caps[] = {
	MRS_HWCAP(1, HWCAP_ASIMDDP, ID_AA64ISAR0_DP_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_sm4[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SM4, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_sm4_caps[] = {
	MRS_HWCAP(1, HWCAP_SM4, ID_AA64ISAR0_SM4_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_sm3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SM3, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_sm3_caps[] = {
	MRS_HWCAP(1, HWCAP_SM3, ID_AA64ISAR0_SM3_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_sha3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SHA3, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_sha3_caps[] = {
	MRS_HWCAP(1, HWCAP_SHA3, ID_AA64ISAR0_SHA3_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_rdm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, RDM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_rdm_caps[] = {
	MRS_HWCAP(1, HWCAP_ASIMDRDM, ID_AA64ISAR0_RDM_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_tme[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, TME, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar0_atomic[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, Atomic, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_atomic_caps[] = {
	MRS_HWCAP(1, HWCAP_ATOMICS, ID_AA64ISAR0_Atomic_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_crc32[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, CRC32, NONE, BASE),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_crc32_caps[] = {
	MRS_HWCAP(1, HWCAP_CRC32, ID_AA64ISAR0_CRC32_BASE),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_sha2[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SHA2, NONE, BASE),
	MRS_FIELD_VALUE(ID_AA64ISAR0_SHA2_512, "SHA2+SHA512"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_sha2_caps[] = {
	MRS_HWCAP(1, HWCAP_SHA2, ID_AA64ISAR0_SHA2_BASE),
	MRS_HWCAP(1, HWCAP_SHA512, ID_AA64ISAR0_SHA2_512),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_sha1[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, SHA1, NONE, BASE),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_sha1_caps[] = {
	MRS_HWCAP(1, HWCAP_SHA1, ID_AA64ISAR0_SHA1_BASE),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar0_aes[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR0, AES, NONE, BASE),
	MRS_FIELD_VALUE(ID_AA64ISAR0_AES_PMULL, "AES+PMULL"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar0_aes_caps[] = {
	MRS_HWCAP(1, HWCAP_AES, ID_AA64ISAR0_AES_BASE),
	MRS_HWCAP(1, HWCAP_PMULL, ID_AA64ISAR0_AES_PMULL),
	MRS_HWCAP_END
};

static const struct mrs_field id_aa64isar0_fields[] = {
	MRS_FIELD_HWCAP(ID_AA64ISAR0, RNDR, false, MRS_LOWER,
	    id_aa64isar0_rndr, id_aa64isar0_rndr_caps),
	MRS_FIELD(ID_AA64ISAR0, TLB, false, MRS_EXACT, id_aa64isar0_tlb),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, TS, false, MRS_LOWER, id_aa64isar0_ts,
	    id_aa64isar0_ts_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, FHM, false, MRS_LOWER, id_aa64isar0_fhm,
	    id_aa64isar0_fhm_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, DP, false, MRS_LOWER, id_aa64isar0_dp,
	    id_aa64isar0_dp_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, SM4, false, MRS_LOWER, id_aa64isar0_sm4,
	    id_aa64isar0_sm4_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, SM3, false, MRS_LOWER, id_aa64isar0_sm3,
	    id_aa64isar0_sm3_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, SHA3, false, MRS_LOWER, id_aa64isar0_sha3,
	    id_aa64isar0_sha3_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, RDM, false, MRS_LOWER, id_aa64isar0_rdm,
	    id_aa64isar0_rdm_caps),
	MRS_FIELD(ID_AA64ISAR0, TME, false, MRS_EXACT, id_aa64isar0_tme),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, Atomic, false, MRS_LOWER,
	    id_aa64isar0_atomic, id_aa64isar0_atomic_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, CRC32, false, MRS_LOWER,
	    id_aa64isar0_crc32, id_aa64isar0_crc32_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, SHA2, false, MRS_LOWER, id_aa64isar0_sha2,
	    id_aa64isar0_sha2_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, SHA1, false, MRS_LOWER,
	    id_aa64isar0_sha1, id_aa64isar0_sha1_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR0, AES, false, MRS_LOWER, id_aa64isar0_aes,
	    id_aa64isar0_aes_caps),
	MRS_FIELD_END,
};


/* ID_AA64ISAR1_EL1 */
static const struct mrs_field_value id_aa64isar1_ls64[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, LS64, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64ISAR1_LS64_V, "LS64v"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_LS64_ACCDATA, "LS64+ACCDATA"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar1_xs[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, XS, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar1_i8mm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, I8MM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_i8mm_caps[] = {
	MRS_HWCAP(2, HWCAP2_I8MM, ID_AA64ISAR1_I8MM_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_dgh[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, DGH, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_dgh_caps[] = {
	MRS_HWCAP(2, HWCAP2_DGH, ID_AA64ISAR1_DGH_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_bf16[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, BF16, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64ISAR1_BF16_EBF, "EBF16"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_bf16_caps[] = {
	MRS_HWCAP(2, HWCAP2_BF16, ID_AA64ISAR1_BF16_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_specres[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR1_SPECRES_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR1_SPECRES_IMPL, "PredInv"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar1_sb[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, SB, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_sb_caps[] = {
	MRS_HWCAP(1, HWCAP_SB, ID_AA64ISAR1_SB_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_frintts[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, FRINTTS, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_frintts_caps[] = {
	MRS_HWCAP(2, HWCAP2_FRINT, ID_AA64ISAR1_FRINTTS_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_gpi[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, GPI, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_gpi_caps[] = {
	MRS_HWCAP(1, HWCAP_PACG, ID_AA64ISAR1_GPI_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_gpa[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, GPA, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_gpa_caps[] = {
	MRS_HWCAP(1, HWCAP_PACG, ID_AA64ISAR1_GPA_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_lrcpc[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR1_LRCPC_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR1_LRCPC_RCPC_8_3, "RCPC-8.3"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_LRCPC_RCPC_8_4, "RCPC-8.4"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_lrcpc_caps[] = {
	MRS_HWCAP(1, HWCAP_LRCPC, ID_AA64ISAR1_LRCPC_RCPC_8_3),
	MRS_HWCAP(1, HWCAP_ILRCPC, ID_AA64ISAR1_LRCPC_RCPC_8_4),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_fcma[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, FCMA, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_fcma_caps[] = {
	MRS_HWCAP(1, HWCAP_FCMA, ID_AA64ISAR1_FCMA_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_jscvt[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR1, JSCVT, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_jscvt_caps[] = {
	MRS_HWCAP(1, HWCAP_JSCVT, ID_AA64ISAR1_JSCVT_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_api[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR1_API_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR1_API_PAC, "API PAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_API_EPAC, "API EPAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_API_EPAC2, "Impl PAuth+EPAC2"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_API_FPAC, "Impl PAuth+FPAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_API_FPAC_COMBINED,
	    "Impl PAuth+FPAC+Combined"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_api_caps[] = {
	MRS_HWCAP(1, HWCAP_PACA, ID_AA64ISAR1_API_PAC),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_apa[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR1_APA_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR1_APA_PAC, "APA PAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_APA_EPAC, "APA EPAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_APA_EPAC2, "APA EPAC2"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_APA_FPAC, "APA FPAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_APA_FPAC_COMBINED,
	    "APA FPAC+Combined"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_apa_caps[] = {
	MRS_HWCAP(1, HWCAP_PACA, ID_AA64ISAR1_APA_PAC),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar1_dpb[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR1_DPB_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR1_DPB_DCCVAP, "DCPoP"),
	MRS_FIELD_VALUE(ID_AA64ISAR1_DPB_DCCVADP, "DCCVADP"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar1_dpb_caps[] = {
	MRS_HWCAP(1, HWCAP_DCPOP, ID_AA64ISAR1_DPB_DCCVAP),
	MRS_HWCAP(2, HWCAP2_DCPODP, ID_AA64ISAR1_DPB_DCCVADP),
	MRS_HWCAP_END
};

static const struct mrs_field id_aa64isar1_fields[] = {
	MRS_FIELD(ID_AA64ISAR1, LS64, false, MRS_EXACT, id_aa64isar1_ls64),
	MRS_FIELD(ID_AA64ISAR1, XS, false, MRS_EXACT, id_aa64isar1_xs),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, I8MM, false, MRS_LOWER,
	    id_aa64isar1_i8mm, id_aa64isar1_i8mm_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, DGH, false, MRS_LOWER, id_aa64isar1_dgh,
	    id_aa64isar1_dgh_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, BF16, false, MRS_LOWER,
	    id_aa64isar1_bf16, id_aa64isar1_bf16_caps),
	MRS_FIELD(ID_AA64ISAR1, SPECRES, false, MRS_EXACT,
	    id_aa64isar1_specres),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, SB, false, MRS_LOWER, id_aa64isar1_sb,
	    id_aa64isar1_sb_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, FRINTTS, false, MRS_LOWER,
	    id_aa64isar1_frintts, id_aa64isar1_frintts_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, GPI, false, MRS_EXACT, id_aa64isar1_gpi,
	    id_aa64isar1_gpi_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, GPA, false, MRS_EXACT, id_aa64isar1_gpa,
	    id_aa64isar1_gpa_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, LRCPC, false, MRS_LOWER,
	    id_aa64isar1_lrcpc, id_aa64isar1_lrcpc_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, FCMA, false, MRS_LOWER,
	    id_aa64isar1_fcma, id_aa64isar1_fcma_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, JSCVT, false, MRS_LOWER,
	    id_aa64isar1_jscvt, id_aa64isar1_jscvt_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, API, false, MRS_EXACT, id_aa64isar1_api,
	    id_aa64isar1_api_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, APA, false, MRS_EXACT, id_aa64isar1_apa,
	    id_aa64isar1_apa_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR1, DPB, false, MRS_LOWER, id_aa64isar1_dpb,
	    id_aa64isar1_dpb_caps),
	MRS_FIELD_END,
};


/* ID_AA64ISAR2_EL1 */
static const struct mrs_field_value id_aa64isar2_pac_frac[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR2, PAC_frac, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar2_bc[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR2, BC, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar2_mops[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR2, MOPS, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar2_apa3[] = {
	MRS_FIELD_VALUE(ID_AA64ISAR2_APA3_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64ISAR2_APA3_PAC, "APA3 PAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR2_APA3_EPAC, "APA3 EPAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR2_APA3_EPAC2, "APA3 EPAC2"),
	MRS_FIELD_VALUE(ID_AA64ISAR2_APA3_FPAC, "APA3 FPAC"),
	MRS_FIELD_VALUE(ID_AA64ISAR2_APA3_FPAC_COMBINED,
	    "APA3 FPAC+Combined"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar2_apa3_caps[] = {
	MRS_HWCAP(1, HWCAP_PACA, ID_AA64ISAR2_APA3_PAC),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar2_gpa3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR2, GPA3, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64isar2_gpa3_caps[] = {
	MRS_HWCAP(1, HWCAP_PACG, ID_AA64ISAR2_GPA3_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64isar2_rpres[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR2, RPRES, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64isar2_wfxt[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ISAR2, WFxT, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64isar2_fields[] = {
	MRS_FIELD(ID_AA64ISAR2, PAC_frac, false, MRS_EXACT,
	    id_aa64isar2_pac_frac),
	MRS_FIELD(ID_AA64ISAR2, BC, false, MRS_EXACT, id_aa64isar2_bc),
	MRS_FIELD(ID_AA64ISAR2, MOPS, false, MRS_EXACT, id_aa64isar2_mops),
	MRS_FIELD_HWCAP(ID_AA64ISAR2, APA3, false, MRS_EXACT,
	    id_aa64isar2_apa3, id_aa64isar2_apa3_caps),
	MRS_FIELD_HWCAP(ID_AA64ISAR2, GPA3, false, MRS_EXACT,
	    id_aa64isar2_gpa3, id_aa64isar2_gpa3_caps),
	MRS_FIELD(ID_AA64ISAR2, RPRES, false, MRS_EXACT, id_aa64isar2_rpres),
	MRS_FIELD(ID_AA64ISAR2, WFxT, false, MRS_EXACT, id_aa64isar2_wfxt),
	MRS_FIELD_END,
};


/* ID_AA64MMFR0_EL1 */
static const struct mrs_field_value id_aa64mmfr0_ecv[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, ECV, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64MMFR0_ECV_CNTHCTL, "ECV+CNTHCTL"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_fgt[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, FGT, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_exs[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, ExS, ALL, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_tgran4_2[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran4_2_TGran4, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran4_2_NONE, "No S2 TGran4"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran4_2_IMPL, "S2 TGran4"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran4_2_LPA2, "S2 TGran4+LPA2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_tgran64_2[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran64_2_TGran64, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran64_2_NONE, "No S2 TGran64"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran64_2_IMPL, "S2 TGran64"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_tgran16_2[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran16_2_TGran16, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran16_2_NONE, "No S2 TGran16"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran16_2_IMPL, "S2 TGran16"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran16_2_LPA2, "S2 TGran16+LPA2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_tgran4[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, TGran4, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran4_LPA2, "TGran4+LPA2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_tgran64[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, TGran64, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_tgran16[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, TGran16, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64MMFR0_TGran16_LPA2, "TGran16+LPA2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_bigendel0[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, BigEndEL0, FIXED, MIXED),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_snsmem[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, SNSMem, NONE, DISTINCT),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_bigend[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR0, BigEnd, FIXED, MIXED),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_asidbits[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR0_ASIDBits_8, "8bit ASID"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_ASIDBits_16, "16bit ASID"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr0_parange[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_4G, "4GB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_64G, "64GB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_1T, "1TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_4T, "4TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_16T, "16TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_256T, "256TB PA"),
	MRS_FIELD_VALUE(ID_AA64MMFR0_PARange_4P, "4PB PA"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64mmfr0_fields[] = {
	MRS_FIELD(ID_AA64MMFR0, ECV, false, MRS_EXACT, id_aa64mmfr0_ecv),
	MRS_FIELD(ID_AA64MMFR0, FGT, false, MRS_EXACT, id_aa64mmfr0_fgt),
	MRS_FIELD(ID_AA64MMFR0, ExS, false, MRS_EXACT, id_aa64mmfr0_exs),
	MRS_FIELD(ID_AA64MMFR0, TGran4_2, false, MRS_EXACT,
	    id_aa64mmfr0_tgran4_2),
	MRS_FIELD(ID_AA64MMFR0, TGran64_2, false, MRS_EXACT,
	    id_aa64mmfr0_tgran64_2),
	MRS_FIELD(ID_AA64MMFR0, TGran16_2, false, MRS_EXACT,
	    id_aa64mmfr0_tgran16_2),
	MRS_FIELD(ID_AA64MMFR0, TGran4, false, MRS_EXACT, id_aa64mmfr0_tgran4),
	MRS_FIELD(ID_AA64MMFR0, TGran64, false, MRS_EXACT,
	    id_aa64mmfr0_tgran64),
	MRS_FIELD(ID_AA64MMFR0, TGran16, false, MRS_EXACT,
	    id_aa64mmfr0_tgran16),
	MRS_FIELD(ID_AA64MMFR0, BigEndEL0, false, MRS_EXACT,
	    id_aa64mmfr0_bigendel0),
	MRS_FIELD(ID_AA64MMFR0, SNSMem, false, MRS_EXACT, id_aa64mmfr0_snsmem),
	MRS_FIELD(ID_AA64MMFR0, BigEnd, false, MRS_EXACT, id_aa64mmfr0_bigend),
	MRS_FIELD(ID_AA64MMFR0, ASIDBits, false, MRS_EXACT,
	    id_aa64mmfr0_asidbits),
	MRS_FIELD(ID_AA64MMFR0, PARange, false, MRS_EXACT,
	    id_aa64mmfr0_parange),
	MRS_FIELD_END,
};


/* ID_AA64MMFR1_EL1 */
static const struct mrs_field_value id_aa64mmfr1_cmovw[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, CMOVW, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_tidcp1[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, TIDCP1, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_ntlbpa[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, nTLBPA, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_afp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, AFP, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_hcx[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, HCX, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_ets[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, ETS, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_twed[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, TWED, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_xnx[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, XNX, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_specsei[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, SpecSEI, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_pan[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, PAN, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64MMFR1_PAN_ATS1E1, "PAN+ATS1E1"),
	MRS_FIELD_VALUE(ID_AA64MMFR1_PAN_EPAN, "EPAN"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_lo[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, LO, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_hpds[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR1_HPDS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HPDS_HPD, "HPD"),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HPDS_TTPBHA, "HPD+TTPBHA"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_vh[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR1, VH, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_vmidbits[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR1_VMIDBits_8, "8bit VMID"),
	MRS_FIELD_VALUE(ID_AA64MMFR1_VMIDBits_16, "16bit VMID"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr1_hafdbs[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR1_HAFDBS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HAFDBS_AF, "HAF"),
	MRS_FIELD_VALUE(ID_AA64MMFR1_HAFDBS_AF_DBS, "HAF+DS"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64mmfr1_fields[] = {
	MRS_FIELD(ID_AA64MMFR1, CMOVW, false, MRS_EXACT, id_aa64mmfr1_cmovw),
	MRS_FIELD(ID_AA64MMFR1, TIDCP1, false, MRS_EXACT, id_aa64mmfr1_tidcp1),
	MRS_FIELD(ID_AA64MMFR1, nTLBPA, false, MRS_EXACT, id_aa64mmfr1_ntlbpa),
	MRS_FIELD(ID_AA64MMFR1, AFP, false, MRS_EXACT, id_aa64mmfr1_afp),
	MRS_FIELD(ID_AA64MMFR1, HCX, false, MRS_EXACT, id_aa64mmfr1_hcx),
	MRS_FIELD(ID_AA64MMFR1, ETS, false, MRS_EXACT, id_aa64mmfr1_ets),
	MRS_FIELD(ID_AA64MMFR1, TWED, false, MRS_EXACT, id_aa64mmfr1_twed),
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
static const struct mrs_field_value id_aa64mmfr2_e0pd[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, E0PD, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_evt[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR2_EVT_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR2_EVT_8_2, "EVT-8.2"),
	MRS_FIELD_VALUE(ID_AA64MMFR2_EVT_8_5, "EVT-8.5"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_bbm[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR2_BBM_LEVEL0, ""),
	MRS_FIELD_VALUE(ID_AA64MMFR2_BBM_LEVEL1, "BBM level 1"),
	MRS_FIELD_VALUE(ID_AA64MMFR2_BBM_LEVEL2, "BBM level 2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_ttl[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, TTL, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_fwb[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, FWB, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_ids[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, IDS, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_at[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, AT, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64mmfr2_at_caps[] = {
	MRS_HWCAP(1, HWCAP_USCAT, ID_AA64MMFR2_AT_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64mmfr2_st[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, ST, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_nv[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, NV, NONE, 8_3),
	MRS_FIELD_VALUE(ID_AA64MMFR2_NV_8_4, "NV v8.4"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_ccidx[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR2_CCIDX_32, "32bit CCIDX"),
	MRS_FIELD_VALUE(ID_AA64MMFR2_CCIDX_64, "64bit CCIDX"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_varange[] = {
	MRS_FIELD_VALUE(ID_AA64MMFR2_VARange_48, "48bit VA"),
	MRS_FIELD_VALUE(ID_AA64MMFR2_VARange_52, "52bit VA"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_iesb[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, IESB, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_lsm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, LSM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_uao[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, UAO, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr2_cnp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR2, CnP, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64mmfr2_fields[] = {
	MRS_FIELD(ID_AA64MMFR2, E0PD, false, MRS_EXACT, id_aa64mmfr2_e0pd),
	MRS_FIELD(ID_AA64MMFR2, EVT, false, MRS_EXACT, id_aa64mmfr2_evt),
	MRS_FIELD(ID_AA64MMFR2, BBM, false, MRS_EXACT, id_aa64mmfr2_bbm),
	MRS_FIELD(ID_AA64MMFR2, TTL, false, MRS_EXACT, id_aa64mmfr2_ttl),
	MRS_FIELD(ID_AA64MMFR2, FWB, false, MRS_EXACT, id_aa64mmfr2_fwb),
	MRS_FIELD(ID_AA64MMFR2, IDS, false, MRS_EXACT, id_aa64mmfr2_ids),
	MRS_FIELD_HWCAP(ID_AA64MMFR2, AT, false, MRS_LOWER, id_aa64mmfr2_at,
	    id_aa64mmfr2_at_caps),
	MRS_FIELD(ID_AA64MMFR2, ST, false, MRS_EXACT, id_aa64mmfr2_st),
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


#ifdef NOTYET
/* ID_AA64MMFR2_EL1 */
static const struct mrs_field_value id_aa64mmfr3_spec_fpacc[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR3, Spec_FPACC, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr3_mec[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR3, MEC, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr3_sctlrx[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR3, SCTLRX, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64mmfr3_tcrx[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64MMFR3, TCRX, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64mmfr3_fields[] = {
	MRS_FIELD(ID_AA64MMFR3, Spec_FPACC, false, MRS_EXACT,
	    id_aa64mmfr3_spec_fpacc),
	MRS_FIELD(ID_AA64MMFR3, MEC, false, MRS_EXACT, id_aa64mmfr3_mec),
	MRS_FIELD(ID_AA64MMFR3, SCTLRX, false, MRS_EXACT, id_aa64mmfr3_sctlrx),
	MRS_FIELD(ID_AA64MMFR3, TCRX, false, MRS_EXACT, id_aa64mmfr3_tcrx),
	MRS_FIELD_END,
};


/* ID_AA64MMFR4_EL1 */
static const struct mrs_field id_aa64mmfr4_fields[] = {
	MRS_FIELD_END,
};
#endif


/* ID_AA64PFR0_EL1 */
static const struct mrs_field_value id_aa64pfr0_csv3[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_CSV3_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR0_CSV3_ISOLATED, "CSV3"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_csv2[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_CSV2_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR0_CSV2_ISOLATED, "CSV2"),
	MRS_FIELD_VALUE(ID_AA64PFR0_CSV2_SCXTNUM, "CSV2_2"),
	MRS_FIELD_VALUE(ID_AA64PFR0_CSV2_3, "CSV2_3"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_rme[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, RME, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_dit[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_DIT_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR0_DIT_PSTATE, "PSTATE.DIT"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64pfr0_dit_caps[] = {
	MRS_HWCAP(1, HWCAP_DIT, ID_AA64PFR0_DIT_PSTATE),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64pfr0_amu[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_AMU_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR0_AMU_V1, "AMUv1"),
	MRS_FIELD_VALUE(ID_AA64PFR0_AMU_V1_1, "AMUv1p1"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_mpam[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, MPAM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_sel2[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, SEL2, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_sve[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, SVE, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

#if 0
/* Enable when we add SVE support */
static const struct mrs_field_hwcap id_aa64pfr0_sve_caps[] = {
	MRS_HWCAP(1, HWCAP_SVE, ID_AA64PFR0_SVE_IMPL),
	MRS_HWCAP_END
};
#endif

static const struct mrs_field_value id_aa64pfr0_ras[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_RAS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR0_RAS_IMPL, "RAS"),
	MRS_FIELD_VALUE(ID_AA64PFR0_RAS_8_4, "RAS v8.4"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_gic[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, GIC, CPUIF_NONE, CPUIF_EN),
	MRS_FIELD_VALUE(ID_AA64PFR0_GIC_CPUIF_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR0_GIC_CPUIF_EN, "GIC"),
	MRS_FIELD_VALUE(ID_AA64PFR0_GIC_CPUIF_4_1, "GIC 4.1"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_advsimd[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, AdvSIMD, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64PFR0_AdvSIMD_HP, "AdvSIMD+HP"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64pfr0_advsimd_caps[] = {
	MRS_HWCAP(1, HWCAP_ASIMD, ID_AA64PFR0_AdvSIMD_IMPL),
	MRS_HWCAP(1, HWCAP_ASIMDHP, ID_AA64PFR0_AdvSIMD_HP),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64pfr0_fp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, FP, NONE, IMPL),
	MRS_FIELD_VALUE(ID_AA64PFR0_FP_HP, "FP+HP"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64pfr0_fp_caps[] = {
	MRS_HWCAP(1, HWCAP_FP, ID_AA64PFR0_FP_IMPL),
	MRS_HWCAP(1, HWCAP_FPHP, ID_AA64PFR0_FP_HP),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64pfr0_el3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, EL3, NONE, 64),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL3_64_32, "EL3 32"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_el2[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR0, EL2, NONE, 64),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL2_64_32, "EL2 32"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_el1[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_EL1_64, "EL1"),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL1_64_32, "EL1 32"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr0_el0[] = {
	MRS_FIELD_VALUE(ID_AA64PFR0_EL0_64, "EL0"),
	MRS_FIELD_VALUE(ID_AA64PFR0_EL0_64_32, "EL0 32"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64pfr0_fields[] = {
	MRS_FIELD(ID_AA64PFR0, CSV3, false, MRS_EXACT, id_aa64pfr0_csv3),
	MRS_FIELD(ID_AA64PFR0, CSV2, false, MRS_EXACT, id_aa64pfr0_csv2),
	MRS_FIELD(ID_AA64PFR0, RME, false, MRS_EXACT, id_aa64pfr0_rme),
	MRS_FIELD_HWCAP(ID_AA64PFR0, DIT, false, MRS_LOWER, id_aa64pfr0_dit,
	    id_aa64pfr0_dit_caps),
	MRS_FIELD(ID_AA64PFR0, AMU, false, MRS_EXACT, id_aa64pfr0_amu),
	MRS_FIELD(ID_AA64PFR0, MPAM, false, MRS_EXACT, id_aa64pfr0_mpam),
	MRS_FIELD(ID_AA64PFR0, SEL2, false, MRS_EXACT, id_aa64pfr0_sel2),
	MRS_FIELD(ID_AA64PFR0, SVE, false, MRS_EXACT, id_aa64pfr0_sve),
	MRS_FIELD(ID_AA64PFR0, RAS, false, MRS_EXACT, id_aa64pfr0_ras),
	MRS_FIELD(ID_AA64PFR0, GIC, false, MRS_EXACT, id_aa64pfr0_gic),
	MRS_FIELD_HWCAP(ID_AA64PFR0, AdvSIMD, true, MRS_LOWER,
	    id_aa64pfr0_advsimd, id_aa64pfr0_advsimd_caps),
	MRS_FIELD_HWCAP(ID_AA64PFR0, FP, true,  MRS_LOWER, id_aa64pfr0_fp,
	    id_aa64pfr0_fp_caps),
	MRS_FIELD(ID_AA64PFR0, EL3, false, MRS_EXACT, id_aa64pfr0_el3),
	MRS_FIELD(ID_AA64PFR0, EL2, false, MRS_EXACT, id_aa64pfr0_el2),
	MRS_FIELD(ID_AA64PFR0, EL1, false, MRS_LOWER, id_aa64pfr0_el1),
	MRS_FIELD(ID_AA64PFR0, EL0, false, MRS_LOWER, id_aa64pfr0_el0),
	MRS_FIELD_END,
};


/* ID_AA64PFR1_EL1 */
static const struct mrs_field_value id_aa64pfr1_nmi[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR1, NMI, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr1_csv2_frac[] = {
	MRS_FIELD_VALUE(ID_AA64PFR1_CSV2_frac_p0, ""),
	MRS_FIELD_VALUE(ID_AA64PFR1_CSV2_frac_p1, "CSV2 p1"),
	MRS_FIELD_VALUE(ID_AA64PFR1_CSV2_frac_p2, "CSV2 p2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr1_rndr_trap[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64PFR1, RNDR_trap, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr1_sme[] = {
	MRS_FIELD_VALUE(ID_AA64PFR1_SME_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR1_SME_SME, "SME"),
	MRS_FIELD_VALUE(ID_AA64PFR1_SME_SME2, "SME2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr1_mpam_frac[] = {
	MRS_FIELD_VALUE(ID_AA64PFR1_MPAM_frac_p0, ""),
	MRS_FIELD_VALUE(ID_AA64PFR1_MPAM_frac_p1, "MPAM p1"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr1_ras_frac[] = {
	MRS_FIELD_VALUE(ID_AA64PFR1_RAS_frac_p0, ""),
	MRS_FIELD_VALUE(ID_AA64PFR1_RAS_frac_p1, "RAS p1"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr1_mte[] = {
	MRS_FIELD_VALUE(ID_AA64PFR1_MTE_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR1_MTE_MTE, "MTE"),
	MRS_FIELD_VALUE(ID_AA64PFR1_MTE_MTE2, "MTE2"),
	MRS_FIELD_VALUE(ID_AA64PFR1_MTE_MTE3, "MTE3"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64pfr1_ssbs[] = {
	MRS_FIELD_VALUE(ID_AA64PFR1_SSBS_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR1_SSBS_PSTATE, "PSTATE.SSBS"),
	MRS_FIELD_VALUE(ID_AA64PFR1_SSBS_PSTATE_MSR, "PSTATE.SSBS MSR"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64pfr1_ssbs_caps[] = {
	MRS_HWCAP(1, HWCAP_SSBS, ID_AA64PFR1_SSBS_PSTATE),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_aa64pfr1_bt[] = {
	MRS_FIELD_VALUE(ID_AA64PFR1_BT_NONE, ""),
	MRS_FIELD_VALUE(ID_AA64PFR1_BT_IMPL, "BTI"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_aa64pfr1_bt_caps[] = {
	MRS_HWCAP(2, HWCAP2_BTI, ID_AA64PFR1_BT_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field id_aa64pfr1_fields[] = {
	MRS_FIELD(ID_AA64PFR1, NMI, false, MRS_EXACT, id_aa64pfr1_nmi),
	MRS_FIELD(ID_AA64PFR1, CSV2_frac, false, MRS_EXACT,
	    id_aa64pfr1_csv2_frac),
	MRS_FIELD(ID_AA64PFR1, RNDR_trap, false, MRS_EXACT,
	    id_aa64pfr1_rndr_trap),
	MRS_FIELD(ID_AA64PFR1, SME, false, MRS_EXACT, id_aa64pfr1_sme),
	MRS_FIELD(ID_AA64PFR1, MPAM_frac, false, MRS_EXACT,
	    id_aa64pfr1_mpam_frac),
	MRS_FIELD(ID_AA64PFR1, RAS_frac, false, MRS_EXACT,
	    id_aa64pfr1_ras_frac),
	MRS_FIELD(ID_AA64PFR1, MTE, false, MRS_EXACT, id_aa64pfr1_mte),
	MRS_FIELD_HWCAP(ID_AA64PFR1, SSBS, false, MRS_LOWER, id_aa64pfr1_ssbs,
	    id_aa64pfr1_ssbs_caps),
	MRS_FIELD_HWCAP_SPLIT(ID_AA64PFR1, BT, false, MRS_LOWER, MRS_EXACT,
	    id_aa64pfr1_bt, id_aa64pfr1_bt_caps),
	MRS_FIELD_END,
};


#ifdef NOTYET
/* ID_AA64PFR2_EL1 */
static const struct mrs_field id_aa64pfr2_fields[] = {
	MRS_FIELD_END,
};
#endif


/* ID_AA64ZFR0_EL1 */
static const struct mrs_field_value id_aa64zfr0_f64mm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, F64MM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_f32mm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, F32MM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_i8mm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, I8MM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_sm4[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, SM4, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_sha3[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, SHA3, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_bf16[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, BF16, NONE, BASE),
	MRS_FIELD_VALUE(ID_AA64ZFR0_BF16_EBF, "BF16+EBF"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_bitperm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, BitPerm, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_aes[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_AA64ZFR0, AES, NONE, BASE),
	MRS_FIELD_VALUE(ID_AA64ZFR0_AES_PMULL, "AES+PMULL"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_aa64zfr0_svever[] = {
	MRS_FIELD_VALUE(ID_AA64ZFR0_SVEver_SVE1, "SVE1"),
	MRS_FIELD_VALUE(ID_AA64ZFR0_SVEver_SVE2, "SVE2"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_aa64zfr0_fields[] = {
	MRS_FIELD(ID_AA64ZFR0, F64MM, false, MRS_EXACT, id_aa64zfr0_f64mm),
	MRS_FIELD(ID_AA64ZFR0, F32MM, false, MRS_EXACT, id_aa64zfr0_f32mm),
	MRS_FIELD(ID_AA64ZFR0, I8MM, false, MRS_EXACT, id_aa64zfr0_i8mm),
	MRS_FIELD(ID_AA64ZFR0, SM4, false, MRS_EXACT, id_aa64zfr0_sm4),
	MRS_FIELD(ID_AA64ZFR0, SHA3, false, MRS_EXACT, id_aa64zfr0_sha3),
	MRS_FIELD(ID_AA64ZFR0, BF16, false, MRS_EXACT, id_aa64zfr0_bf16),
	MRS_FIELD(ID_AA64ZFR0, BitPerm, false, MRS_EXACT, id_aa64zfr0_bitperm),
	MRS_FIELD(ID_AA64ZFR0, AES, false, MRS_EXACT, id_aa64zfr0_aes),
	MRS_FIELD(ID_AA64ZFR0, SVEver, false, MRS_EXACT, id_aa64zfr0_svever),
	MRS_FIELD_END,
};


#ifdef COMPAT_FREEBSD32
/* ID_ISAR5_EL1 */
static const struct mrs_field_value id_isar5_vcma[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_ISAR5, VCMA, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_isar5_rdm[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_ISAR5, RDM, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value id_isar5_crc32[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_ISAR5, CRC32, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_isar5_crc32_caps[] = {
	MRS_HWCAP(2, HWCAP32_2_CRC32, ID_ISAR5_CRC32_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_isar5_sha2[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_ISAR5, SHA2, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_isar5_sha2_caps[] = {
	MRS_HWCAP(2, HWCAP32_2_SHA2, ID_ISAR5_SHA2_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_isar5_sha1[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_ISAR5, SHA1, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_isar5_sha1_caps[] = {
	MRS_HWCAP(2, HWCAP32_2_SHA1, ID_ISAR5_SHA1_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_isar5_aes[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_ISAR5, AES, NONE, BASE),
	MRS_FIELD_VALUE(ID_ISAR5_AES_VMULL, "AES+VMULL"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap id_isar5_aes_caps[] = {
	MRS_HWCAP(2, HWCAP32_2_AES, ID_ISAR5_AES_BASE),
	MRS_HWCAP(2, HWCAP32_2_PMULL, ID_ISAR5_AES_VMULL),
	MRS_HWCAP_END
};

static const struct mrs_field_value id_isar5_sevl[] = {
	MRS_FIELD_VALUE_NONE_IMPL(ID_ISAR5, SEVL, NOP, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field id_isar5_fields[] = {
	MRS_FIELD(ID_ISAR5, VCMA, false, MRS_LOWER, id_isar5_vcma),
	MRS_FIELD(ID_ISAR5, RDM, false, MRS_LOWER, id_isar5_rdm),
	MRS_FIELD_HWCAP(ID_ISAR5, CRC32, false, MRS_LOWER, id_isar5_crc32,
	    id_isar5_crc32_caps),
	MRS_FIELD_HWCAP(ID_ISAR5, SHA2, false, MRS_LOWER, id_isar5_sha2,
	    id_isar5_sha2_caps),
	MRS_FIELD_HWCAP(ID_ISAR5, SHA1, false, MRS_LOWER, id_isar5_sha1,
	    id_isar5_sha1_caps),
	MRS_FIELD_HWCAP(ID_ISAR5, AES, false, MRS_LOWER, id_isar5_aes,
	    id_isar5_aes_caps),
	MRS_FIELD(ID_ISAR5, SEVL, false, MRS_LOWER, id_isar5_sevl),
	MRS_FIELD_END,
};

/* MVFR0 */
static const struct mrs_field_value mvfr0_fpround[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR0, FPRound, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr0_fpsqrt[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR0, FPSqrt, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr0_fpdivide[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR0, FPDivide, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr0_fptrap[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR0, FPTrap, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr0_fpdp[] = {
	MRS_FIELD_VALUE(MVFR0_FPDP_NONE, ""),
	MRS_FIELD_VALUE(MVFR0_FPDP_VFP_v2, "DP VFPv2"),
	MRS_FIELD_VALUE(MVFR0_FPDP_VFP_v3_v4, "DP VFPv3+v4"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap mvfr0_fpdp_caps[] = {
	MRS_HWCAP(1, HWCAP32_VFP, MVFR0_FPDP_VFP_v2),
	MRS_HWCAP(1, HWCAP32_VFPv3, MVFR0_FPDP_VFP_v3_v4),
	MRS_HWCAP_END
};

static const struct mrs_field_value mvfr0_fpsp[] = {
	MRS_FIELD_VALUE(MVFR0_FPSP_NONE, ""),
	MRS_FIELD_VALUE(MVFR0_FPSP_VFP_v2, "SP VFPv2"),
	MRS_FIELD_VALUE(MVFR0_FPSP_VFP_v3_v4, "SP VFPv3+v4"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr0_simdreg[] = {
	MRS_FIELD_VALUE(MVFR0_SIMDReg_NONE, ""),
	MRS_FIELD_VALUE(MVFR0_SIMDReg_FP, "FP 16x64"),
	MRS_FIELD_VALUE(MVFR0_SIMDReg_AdvSIMD, "AdvSIMD"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field mvfr0_fields[] = {
	MRS_FIELD(MVFR0, FPRound, false, MRS_LOWER, mvfr0_fpround),
	MRS_FIELD(MVFR0, FPSqrt, false, MRS_LOWER, mvfr0_fpsqrt),
	MRS_FIELD(MVFR0, FPDivide, false, MRS_LOWER, mvfr0_fpdivide),
	MRS_FIELD(MVFR0, FPTrap, false, MRS_LOWER, mvfr0_fptrap),
	MRS_FIELD_HWCAP(MVFR0, FPDP, false, MRS_LOWER, mvfr0_fpdp,
	    mvfr0_fpdp_caps),
	MRS_FIELD(MVFR0, FPSP, false, MRS_LOWER, mvfr0_fpsp),
	MRS_FIELD(MVFR0, SIMDReg, false, MRS_LOWER, mvfr0_simdreg),
	MRS_FIELD_END,
};

/* MVFR1 */
static const struct mrs_field_value mvfr1_simdfmac[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR1, SIMDFMAC, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap mvfr1_simdfmac_caps[] = {
	MRS_HWCAP(1, HWCAP32_VFPv4, MVFR1_SIMDFMAC_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value mvfr1_fphp[] = {
	MRS_FIELD_VALUE(MVFR1_FPHP_NONE, ""),
	MRS_FIELD_VALUE(MVFR1_FPHP_CONV_SP, "FPHP SP Conv"),
	MRS_FIELD_VALUE(MVFR1_FPHP_CONV_DP, "FPHP DP Conv"),
	MRS_FIELD_VALUE(MVFR1_FPHP_ARITH, "FPHP Arith"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr1_simdhp[] = {
	MRS_FIELD_VALUE(MVFR1_SIMDHP_NONE, ""),
	MRS_FIELD_VALUE(MVFR1_SIMDHP_CONV_SP, "SIMDHP SP Conv"),
	MRS_FIELD_VALUE(MVFR1_SIMDHP_ARITH, "SIMDHP Arith"),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr1_simdsp[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR1, SIMDSP, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr1_simdint[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR1, SIMDInt, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr1_simdls[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR1, SIMDLS, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_hwcap mvfr1_simdls_caps[] = {
	MRS_HWCAP(1, HWCAP32_VFPv4, MVFR1_SIMDFMAC_IMPL),
	MRS_HWCAP_END
};

static const struct mrs_field_value mvfr1_fpdnan[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR1, FPDNaN, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field_value mvfr1_fpftz[] = {
	MRS_FIELD_VALUE_NONE_IMPL(MVFR1, FPFtZ, NONE, IMPL),
	MRS_FIELD_VALUE_END,
};

static const struct mrs_field mvfr1_fields[] = {
	MRS_FIELD_HWCAP(MVFR1, SIMDFMAC, false, MRS_LOWER, mvfr1_simdfmac,
	    mvfr1_simdfmac_caps),
	MRS_FIELD(MVFR1, FPHP, false, MRS_LOWER, mvfr1_fphp),
	MRS_FIELD(MVFR1, SIMDHP, false, MRS_LOWER, mvfr1_simdhp),
	MRS_FIELD(MVFR1, SIMDSP, false, MRS_LOWER, mvfr1_simdsp),
	MRS_FIELD(MVFR1, SIMDInt, false, MRS_LOWER, mvfr1_simdint),
	MRS_FIELD_HWCAP(MVFR1, SIMDLS, false, MRS_LOWER, mvfr1_simdls,
	    mvfr1_simdls_caps),
	MRS_FIELD(MVFR1, FPDNaN, false, MRS_LOWER, mvfr1_fpdnan),
	MRS_FIELD(MVFR1, FPFtZ, false, MRS_LOWER, mvfr1_fpftz),
	MRS_FIELD_END,
};
#endif /* COMPAT_FREEBSD32 */

struct mrs_user_reg {
	u_int		reg;
	u_int		CRm;
	u_int		Op2;
	bool		is64bit;
	size_t		offset;
	const struct mrs_field *fields;
};

#define	USER_REG(name, field_name, _is64bit)				\
	{								\
		.reg = name,						\
		.CRm = name##_CRm,					\
		.Op2 = name##_op2,					\
		.offset = __offsetof(struct cpu_desc, field_name),	\
		.fields = field_name##_fields,				\
		.is64bit = _is64bit,					\
	}
static const struct mrs_user_reg user_regs[] = {
	USER_REG(ID_AA64AFR0_EL1, id_aa64afr0, true),
	USER_REG(ID_AA64AFR1_EL1, id_aa64afr1, true),

	USER_REG(ID_AA64DFR0_EL1, id_aa64dfr0, true),
	USER_REG(ID_AA64DFR1_EL1, id_aa64dfr1, true),

	USER_REG(ID_AA64ISAR0_EL1, id_aa64isar0, true),
	USER_REG(ID_AA64ISAR1_EL1, id_aa64isar1, true),
	USER_REG(ID_AA64ISAR2_EL1, id_aa64isar2, true),

	USER_REG(ID_AA64MMFR0_EL1, id_aa64mmfr0, true),
	USER_REG(ID_AA64MMFR1_EL1, id_aa64mmfr1, true),
	USER_REG(ID_AA64MMFR2_EL1, id_aa64mmfr2, true),
#ifdef NOTYET
	USER_REG(ID_AA64MMFR3_EL1, id_aa64mmfr3, true),
	USER_REG(ID_AA64MMFR4_EL1, id_aa64mmfr4, true),
#endif

	USER_REG(ID_AA64PFR0_EL1, id_aa64pfr0, true),
	USER_REG(ID_AA64PFR1_EL1, id_aa64pfr1, true),
#ifdef NOTYET
	USER_REG(ID_AA64PFR2_EL1, id_aa64pfr2, true),
#endif

	USER_REG(ID_AA64ZFR0_EL1, id_aa64zfr0, true),

#ifdef COMPAT_FREEBSD32
	USER_REG(ID_ISAR5_EL1, id_isar5, false),

	USER_REG(MVFR0_EL1, mvfr0, false),
	USER_REG(MVFR1_EL1, mvfr1, false),
#endif /* COMPAT_FREEBSD32 */
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
			if (SV_CURPROC_ABI() == SV_ABI_FREEBSD)
				value = CPU_DESC_FIELD(user_cpu_desc, i);
			else
				value = CPU_DESC_FIELD(l_user_cpu_desc, i);
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

/*
 * Compares two field values that may be signed or unsigned.
 * Returns:
 *  < 0 when a is less than b
 *  = 0 when a equals b
 *  > 0 when a is greater than b
 */
static int
mrs_field_cmp(uint64_t a, uint64_t b, u_int shift, int width, bool sign)
{
	uint64_t mask;

	KASSERT(width > 0 && width < 64, ("%s: Invalid width %d", __func__,
	    width));

	mask = (1ul << width) - 1;
	/* Move the field to the lower bits */
	a = (a >> shift) & mask;
	b = (b >> shift) & mask;

	if (sign) {
		/*
		 * The field is signed. Toggle the upper bit so the comparison
		 * works on unsigned values as this makes positive numbers,
		 * i.e. those with a 0 bit, larger than negative numbers,
		 * i.e. those with a 1 bit, in an unsigned comparison.
		 */
		a ^= 1ul << (width - 1);
		b ^= 1ul << (width - 1);
	}

	return (a - b);
}

static uint64_t
update_lower_register(uint64_t val, uint64_t new_val, u_int shift,
    int width, bool sign)
{
	uint64_t mask;

	KASSERT(width > 0 && width < 64, ("%s: Invalid width %d", __func__,
	    width));

	/*
	 * If the new value is less than the existing value update it.
	 */
	if (mrs_field_cmp(new_val, val, shift, width, sign) < 0) {
		mask = (1ul << width) - 1;
		val &= ~(mask << shift);
		val |= new_val & (mask << shift);
	}

	return (val);
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

bool
get_kernel_reg(u_int reg, uint64_t *val)
{
	int i;

	for (i = 0; i < nitems(user_regs); i++) {
		if (user_regs[i].reg == reg) {
			*val = CPU_DESC_FIELD(kern_cpu_desc, i);
			return (true);
		}
	}

	return (false);
}

/*
 * Fetch the specified register's value, ensuring that individual field values
 * do not exceed those in the mask.
 */
bool
get_kernel_reg_masked(u_int reg, uint64_t *valp, uint64_t mask)
{
	const struct mrs_field *fields;
	uint64_t val;

	for (int i = 0; i < nitems(user_regs); i++) {
		if (user_regs[i].reg == reg) {
			val = CPU_DESC_FIELD(kern_cpu_desc, i);
			fields = user_regs[i].fields;
			for (int j = 0; fields[j].type != 0; j++) {
				mask = update_lower_register(mask, val,
				    fields[j].shift, 4, fields[j].sign);
			}
			*valp = mask;
			return (true);
		}
	}

	return (false);
}

static uint64_t
update_special_reg_field(uint64_t user_reg, u_int type, uint64_t value,
    u_int shift, bool sign)
{
	switch (type & MRS_TYPE_MASK) {
	case MRS_EXACT:
		user_reg &= ~(0xful << shift);
		user_reg |= (uint64_t)MRS_EXACT_FIELD(type) << shift;
		break;
	case MRS_LOWER:
		user_reg = update_lower_register(user_reg, value, shift, 4,
		    sign);
		break;
	default:
		panic("Invalid field type: %d", type);
	}

	return (user_reg);
}

void
update_special_regs(u_int cpu)
{
	struct cpu_desc *desc;
	const struct mrs_field *fields;
	uint64_t l_user_reg, user_reg, kern_reg, value;
	int i, j;

	if (cpu == 0) {
		/* Create a user visible cpu description with safe values */
		memset(&user_cpu_desc, 0, sizeof(user_cpu_desc));
		/* Safe values for these registers */
		user_cpu_desc.id_aa64pfr0 = ID_AA64PFR0_AdvSIMD_NONE |
		    ID_AA64PFR0_FP_NONE | ID_AA64PFR0_EL1_64 |
		    ID_AA64PFR0_EL0_64;
		user_cpu_desc.id_aa64dfr0 = ID_AA64DFR0_DebugVer_8;
		/* Create the Linux user visible cpu description */
		memcpy(&l_user_cpu_desc, &user_cpu_desc, sizeof(user_cpu_desc));
	}

	desc = get_cpu_desc(cpu);
	for (i = 0; i < nitems(user_regs); i++) {
		value = CPU_DESC_FIELD(*desc, i);
		if (cpu == 0) {
			kern_reg = value;
			user_reg = value;
			l_user_reg = value;
		} else {
			kern_reg = CPU_DESC_FIELD(kern_cpu_desc, i);
			user_reg = CPU_DESC_FIELD(user_cpu_desc, i);
			l_user_reg = CPU_DESC_FIELD(l_user_cpu_desc, i);
		}

		fields = user_regs[i].fields;
		for (j = 0; fields[j].type != 0; j++) {
			/* Update the FreeBSD userspace ID register view */
			user_reg = update_special_reg_field(user_reg,
			    fields[j].type >> MRS_TYPE_FBSD_SHIFT, value,
			    fields[j].shift, fields[j].sign);

			/* Update the Linux userspace ID register view */
			l_user_reg = update_special_reg_field(l_user_reg,
			    fields[j].type >> MRS_TYPE_LNX_SHIFT, value,
			    fields[j].shift, fields[j].sign);

			/* Update the kernel ID register view */
			kern_reg = update_lower_register(kern_reg, value,
			    fields[j].shift, 4, fields[j].sign);
		}

		CPU_DESC_FIELD(kern_cpu_desc, i) = kern_reg;
		CPU_DESC_FIELD(user_cpu_desc, i) = user_reg;
		CPU_DESC_FIELD(l_user_cpu_desc, i) = l_user_reg;
	}
}

void
cpu_desc_init(void)
{
	if (mp_ncpus == 1)
		return;

	/*
	 * Allocate memory for the non-boot CPUs to store their registers.
	 * As this is indexed by CPU ID we need to allocate space for CPUs
	 * 1 to mp_maxid. Because of this mp_maxid is already the correct
	 * number of elements.
	 */
	cpu_desc = mallocarray(mp_maxid, sizeof(*cpu_desc), M_IDENTCPU,
	    M_ZERO | M_WAITOK);
}

/* HWCAP */
bool __read_frequently lse_supported = false;

bool __read_frequently icache_aliasing = false;
bool __read_frequently icache_vmid = false;

int64_t dcache_line_size;	/* The minimum D cache line size */
int64_t icache_line_size;	/* The minimum I cache line size */
int64_t idcache_line_size;	/* The minimum cache line size */

/*
 * Find the values to export to userspace as AT_HWCAP and AT_HWCAP2.
 */
static void
parse_cpu_features(bool is64bit, struct cpu_desc *cpu_desc, u_long *hwcap,
    u_long *hwcap2)
{
	const struct mrs_field_hwcap *hwcaps;
	const struct mrs_field *fields;
	uint64_t min, reg;
	u_long *cur_hwcap;
	int i, j, k;

	for (i = 0; i < nitems(user_regs); i++) {
		if (user_regs[i].is64bit != is64bit)
			continue;

		reg = CPU_DESC_FIELD(*cpu_desc, i);
		fields = user_regs[i].fields;
		for (j = 0; fields[j].type != 0; j++) {
			hwcaps = fields[j].hwcaps;
			if (hwcaps == NULL)
				continue;

			for (k = 0; hwcaps[k].hwcap_id != 0; k++) {
				KASSERT(hwcaps[k].hwcap_id == 1 ||
				    hwcaps[k].hwcap_id == 2,
				    ("%s: Invalid HWCAP ID %d", __func__,
				    hwcaps[k].hwcap_id));

				cur_hwcap = hwcaps[k].hwcap_id == 1 ?
				    hwcap : hwcap2;
				min = hwcaps[k].min;

				/*
				 * If the field is greater than the minimum
				 * value we can set the hwcap;
				 */
				if (mrs_field_cmp(reg, min, fields[j].shift,
				    4, fields[j].sign) >= 0) {
					*cur_hwcap |= hwcaps[k].hwcap_val;
				}
			}
		}
	}
}

static void
identify_cpu_sysinit(void *dummy __unused)
{
	struct cpu_desc *desc, *prev_desc;
	int cpu;
	bool dic, idc;

	dic = (allow_dic != 0);
	idc = (allow_idc != 0);

	prev_desc = NULL;
	CPU_FOREACH(cpu) {
		desc = get_cpu_desc(cpu);
		if (cpu != 0) {
			check_cpu_regs(cpu, desc, prev_desc);
			update_special_regs(cpu);
		}

		if (CTR_DIC_VAL(desc->ctr) == 0)
			dic = false;
		if (CTR_IDC_VAL(desc->ctr) == 0)
			idc = false;
		prev_desc = desc;
	}

	/* Find the values to export to userspace as AT_HWCAP and AT_HWCAP2 */
	parse_cpu_features(true, &user_cpu_desc, &elf_hwcap, &elf_hwcap2);
	parse_cpu_features(true, &l_user_cpu_desc, &linux_elf_hwcap,
	    &linux_elf_hwcap2);
#ifdef COMPAT_FREEBSD32
	parse_cpu_features(false, &user_cpu_desc, &elf32_hwcap, &elf32_hwcap2);
#endif

	/* We export the CPUID registers */
	elf_hwcap |= HWCAP_CPUID;
	linux_elf_hwcap |= HWCAP_CPUID;

#ifdef COMPAT_FREEBSD32
	/* Set the default caps and any that need to check multiple fields */
	elf32_hwcap |= parse_cpu_features_hwcap32();
#endif

	if (dic && idc) {
		arm64_icache_sync_range = &arm64_dic_idc_icache_sync_range;
		if (bootverbose)
			printf("Enabling DIC & IDC ICache sync\n");
	} else if (idc) {
		arm64_icache_sync_range = &arm64_idc_aliasing_icache_sync_range;
		if (bootverbose)
			printf("Enabling IDC ICache sync\n");
	}

	if ((elf_hwcap & HWCAP_ATOMICS) != 0) {
		lse_supported = true;
		if (bootverbose)
			printf("Enabling LSE atomics in the kernel\n");
	}
#ifdef LSE_ATOMICS
	if (!lse_supported)
		panic("CPU does not support LSE atomic instructions");
#endif

	install_undef_handler(true, user_mrs_handler);
}
SYSINIT(identify_cpu, SI_SUB_CPU, SI_ORDER_MIDDLE, identify_cpu_sysinit, NULL);

static void
cpu_features_sysinit(void *dummy __unused)
{
	struct sbuf sb;
	struct cpu_desc *desc, *prev_desc;
	u_int cpu;

	prev_desc = NULL;
	CPU_FOREACH(cpu) {
		desc = get_cpu_desc(cpu);
		print_cpu_features(cpu, desc, prev_desc);
		prev_desc = desc;
	}

	/* Fill in cpu_model for the hw.model sysctl */
	sbuf_new(&sb, cpu_model, sizeof(cpu_model), SBUF_FIXEDLEN);
	print_cpu_midr(&sb, 0);

	sbuf_finish(&sb);
	sbuf_delete(&sb);

	free(cpu_desc, M_IDENTCPU);
}
/* Log features before APs are released and start printing to the dmesg. */
SYSINIT(cpu_features, SI_SUB_SMP - 1, SI_ORDER_ANY, cpu_features_sysinit, NULL);

static void
tcr_set_e0pd1(void *arg __unused)
{
	uint64_t tcr;

	tcr = READ_SPECIALREG(tcr_el1);
	tcr |= TCR_E0PD1;
	WRITE_SPECIALREG(tcr_el1, tcr);
	isb();
}

/* Enable support for more recent architecture features */
static void
cpu_feat_support(void *arg __unused)
{
	/*
	 * If FEAT_E0PD is supported use it to cause faults without a page
	 * table walk if userspace tries to access kernel memory.
	 */
	if (ID_AA64MMFR2_E0PD_VAL(kern_cpu_desc.id_aa64mmfr2) !=
	    ID_AA64MMFR2_E0PD_NONE)
		smp_rendezvous(NULL, tcr_set_e0pd1, NULL, NULL);
}
SYSINIT(cpu_feat_support, SI_SUB_SMP, SI_ORDER_ANY, cpu_feat_support, NULL);

#ifdef COMPAT_FREEBSD32
static u_long
parse_cpu_features_hwcap32(void)
{
	u_long hwcap = HWCAP32_DEFAULT;

	if ((MVFR1_SIMDLS_VAL(user_cpu_desc.mvfr1) >=
	     MVFR1_SIMDLS_IMPL) &&
	    (MVFR1_SIMDInt_VAL(user_cpu_desc.mvfr1) >=
	     MVFR1_SIMDInt_IMPL) &&
	    (MVFR1_SIMDSP_VAL(user_cpu_desc.mvfr1) >=
	     MVFR1_SIMDSP_IMPL))
		hwcap |= HWCAP32_NEON;

	return (hwcap);
}
#endif /* COMPAT_FREEBSD32 */

static void
print_ctr_fields(struct sbuf *sb, uint64_t reg, const void *arg __unused)
{

	sbuf_printf(sb, "%u byte D-cacheline,", CTR_DLINE_SIZE(reg));
	sbuf_printf(sb, "%u byte I-cacheline,", CTR_ILINE_SIZE(reg));
	reg &= ~(CTR_DLINE_MASK | CTR_ILINE_MASK);

	switch(CTR_L1IP_VAL(reg)) {
	case CTR_L1IP_VPIPT:
		sbuf_printf(sb, "VPIPT");
		break;
	case CTR_L1IP_AIVIVT:
		sbuf_printf(sb, "AIVIVT");
		break;
	case CTR_L1IP_VIPT:
		sbuf_printf(sb, "VIPT");
		break;
	case CTR_L1IP_PIPT:
		sbuf_printf(sb, "PIPT");
		break;
	}
	sbuf_printf(sb, " ICache,");
	reg &= ~CTR_L1IP_MASK;

	sbuf_printf(sb, "%d byte ERG,", CTR_ERG_SIZE(reg));
	sbuf_printf(sb, "%d byte CWG", CTR_CWG_SIZE(reg));
	reg &= ~(CTR_ERG_MASK | CTR_CWG_MASK);

	if (CTR_IDC_VAL(reg) != 0)
		sbuf_printf(sb, ",IDC");
	if (CTR_DIC_VAL(reg) != 0)
		sbuf_printf(sb, ",DIC");
	reg &= ~(CTR_IDC_MASK | CTR_DIC_MASK);
	reg &= ~CTR_RES1;

	if (reg != 0)
		sbuf_printf(sb, ",%lx", reg);
}

static void
print_register(struct sbuf *sb, const char *reg_name, uint64_t reg,
    void (*print_fields)(struct sbuf *, uint64_t, const void *),
    const void *arg)
{

	sbuf_printf(sb, "%29s = <", reg_name);

	print_fields(sb, reg, arg);

	sbuf_finish(sb);
	printf("%s>\n", sbuf_data(sb));
	sbuf_clear(sb);
}

static void
print_id_fields(struct sbuf *sb, uint64_t reg, const void *arg)
{
	const struct mrs_field *fields = arg;
	const struct mrs_field_value *fv;
	int field, i, j, printed;

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
}

static void
print_id_register(struct sbuf *sb, const char *reg_name, uint64_t reg,
    const struct mrs_field *fields)
{

	print_register(sb, reg_name, reg, print_id_fields, fields);
}

static void
print_cpu_midr(struct sbuf *sb, u_int cpu)
{
	const struct cpu_parts *cpu_partsp;
	const char *cpu_impl_name;
	const char *cpu_part_name;
	u_int midr;
	u_int impl_id;
	u_int part_id;

	midr = pcpu_find(cpu)->pc_midr;

	cpu_impl_name = NULL;
	cpu_partsp = NULL;
	impl_id = CPU_IMPL(midr);
	for (int i = 0; cpu_implementers[i].impl_name != NULL; i++) {
		if (impl_id == cpu_implementers[i].impl_id) {
			cpu_impl_name = cpu_implementers[i].impl_name;
			cpu_partsp = cpu_implementers[i].cpu_parts;
			break;
		}
	}
	/* Unknown implementer, so unknown part */
	if (cpu_impl_name == NULL) {
		sbuf_printf(sb, "Unknown Implementer (midr: %08x)", midr);
		return;
	}

	KASSERT(cpu_partsp != NULL, ("%s: No parts table for implementer %s",
	    __func__, cpu_impl_name));

	cpu_part_name = NULL;
	part_id = CPU_PART(midr);
	for (int i = 0; cpu_partsp[i].part_name != NULL; i++) {
		if (part_id == cpu_partsp[i].part_id) {
			cpu_part_name = cpu_partsp[i].part_name;
			break;
		}
	}
	/* Known Implementer, Unknown part */
	if (cpu_part_name == NULL) {
		sbuf_printf(sb, "%s Unknown CPU r%dp%d (midr: %08x)",
		    cpu_impl_name, CPU_VAR(midr), CPU_REV(midr), midr);
		return;
	}

	sbuf_printf(sb, "%s %s r%dp%d", cpu_impl_name,
	    cpu_part_name, CPU_VAR(midr), CPU_REV(midr));
}

static void
print_cpu_cache(struct cpu_desc *desc, struct sbuf *sb, uint64_t ccs,
    bool icache, bool unified)
{
	size_t cache_size;
	size_t line_size;

	/* LineSize is Log2(S) - 4. */
	line_size = 1 << ((ccs & CCSIDR_LineSize_MASK) + 4);
	/*
	 * Calculate cache size (sets * ways * line size).  There are different
	 * formats depending on the FEAT_CCIDX bit in ID_AA64MMFR2 feature
	 * register.
	 */
	if ((desc->id_aa64mmfr2 & ID_AA64MMFR2_CCIDX_64))
		cache_size = (CCSIDR_NSETS_64(ccs) + 1) *
		    (CCSIDR_ASSOC_64(ccs) + 1);
	else
		cache_size = (CCSIDR_NSETS(ccs) + 1) * (CCSIDR_ASSOC(ccs) + 1);

	cache_size *= line_size;
	sbuf_printf(sb, "%zuKB (%s)", cache_size / 1024,
	    icache ? "instruction" : unified ? "unified" : "data");
}

static void
print_cpu_caches(struct sbuf *sb, struct cpu_desc *desc)
{
	/* Print out each cache combination */
	uint64_t clidr;
	int i = 1;
	clidr = desc->clidr;

	for (i = 0; (clidr & CLIDR_CTYPE_MASK) != 0; i++, clidr >>= 3) {
		int j = 0;
		int ctype_m = (clidr & CLIDR_CTYPE_MASK);

		sbuf_printf(sb, " L%d cache: ", i + 1);
		if ((clidr & CLIDR_CTYPE_IO)) {
			print_cpu_cache(desc, sb, desc->ccsidr[i][j++], true,
			    false);
			/* If there's more, add to the line. */
			if ((ctype_m & ~CLIDR_CTYPE_IO) != 0)
				sbuf_printf(sb, ", ");
		}
		if ((ctype_m & ~CLIDR_CTYPE_IO) != 0) {
			print_cpu_cache(desc, sb, desc->ccsidr[i][j], false,
			    (clidr & CLIDR_CTYPE_UNIFIED));
		}
		sbuf_printf(sb, "\n");

	}
	sbuf_finish(sb);
	printf("%s", sbuf_data(sb));
}

static void
print_cpu_features(u_int cpu, struct cpu_desc *desc,
    struct cpu_desc *prev_desc)
{
	struct sbuf *sb;

	sb = sbuf_new_auto();
	sbuf_printf(sb, "CPU%3u: ", cpu);
	print_cpu_midr(sb, cpu);

	sbuf_cat(sb, " affinity:");
	switch(cpu_aff_levels) {
	default:
	case 4:
		sbuf_printf(sb, " %2d", CPU_AFF3(desc->mpidr));
		/* FALLTHROUGH */
	case 3:
		sbuf_printf(sb, " %2d", CPU_AFF2(desc->mpidr));
		/* FALLTHROUGH */
	case 2:
		sbuf_printf(sb, " %2d", CPU_AFF1(desc->mpidr));
		/* FALLTHROUGH */
	case 1:
	case 0: /* On UP this will be zero */
		sbuf_printf(sb, " %2d", CPU_AFF0(desc->mpidr));
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

#define	SHOULD_PRINT_REG(_reg)						\
    (prev_desc == NULL || desc->_reg != prev_desc->_reg)

	/* Cache Type Register */
	if (SHOULD_PRINT_REG(ctr)) {
		print_register(sb, "Cache Type",
		    desc->ctr, print_ctr_fields, NULL);
	}

	/* AArch64 Instruction Set Attribute Register 0 */
	if (SHOULD_PRINT_REG(id_aa64isar0))
		print_id_register(sb, "Instruction Set Attributes 0",
		    desc->id_aa64isar0, id_aa64isar0_fields);

	/* AArch64 Instruction Set Attribute Register 1 */
	if (SHOULD_PRINT_REG(id_aa64isar1))
		print_id_register(sb, "Instruction Set Attributes 1",
		    desc->id_aa64isar1, id_aa64isar1_fields);

	/* AArch64 Instruction Set Attribute Register 2 */
	if (SHOULD_PRINT_REG(id_aa64isar2))
		print_id_register(sb, "Instruction Set Attributes 2",
		    desc->id_aa64isar2, id_aa64isar2_fields);

	/* AArch64 Processor Feature Register 0 */
	if (SHOULD_PRINT_REG(id_aa64pfr0))
		print_id_register(sb, "Processor Features 0",
		    desc->id_aa64pfr0, id_aa64pfr0_fields);

	/* AArch64 Processor Feature Register 1 */
	if (SHOULD_PRINT_REG(id_aa64pfr1))
		print_id_register(sb, "Processor Features 1",
		    desc->id_aa64pfr1, id_aa64pfr1_fields);

#ifdef NOTYET
	/* AArch64 Processor Feature Register 2 */
	if (SHOULD_PRINT_REG(id_aa64pfr2))
		print_id_register(sb, "Processor Features 2",
		    desc->id_aa64pfr2, id_aa64pfr2_fields);
#endif

	/* AArch64 Memory Model Feature Register 0 */
	if (SHOULD_PRINT_REG(id_aa64mmfr0))
		print_id_register(sb, "Memory Model Features 0",
		    desc->id_aa64mmfr0, id_aa64mmfr0_fields);

	/* AArch64 Memory Model Feature Register 1 */
	if (SHOULD_PRINT_REG(id_aa64mmfr1))
		print_id_register(sb, "Memory Model Features 1",
		    desc->id_aa64mmfr1, id_aa64mmfr1_fields);

	/* AArch64 Memory Model Feature Register 2 */
	if (SHOULD_PRINT_REG(id_aa64mmfr2))
		print_id_register(sb, "Memory Model Features 2",
		    desc->id_aa64mmfr2, id_aa64mmfr2_fields);

#ifdef NOTYET
	/* AArch64 Memory Model Feature Register 3 */
	if (SHOULD_PRINT_REG(id_aa64mmfr3))
		print_id_register(sb, "Memory Model Features 3",
		    desc->id_aa64mmfr3, id_aa64mmfr3_fields);

	/* AArch64 Memory Model Feature Register 4 */
	if (SHOULD_PRINT_REG(id_aa64mmfr4))
		print_id_register(sb, "Memory Model Features 4",
		    desc->id_aa64mmfr4, id_aa64mmfr4_fields);
#endif

	/* AArch64 Debug Feature Register 0 */
	if (SHOULD_PRINT_REG(id_aa64dfr0))
		print_id_register(sb, "Debug Features 0",
		    desc->id_aa64dfr0, id_aa64dfr0_fields);

	/* AArch64 Memory Model Feature Register 1 */
	if (SHOULD_PRINT_REG(id_aa64dfr1))
		print_id_register(sb, "Debug Features 1",
		    desc->id_aa64dfr1, id_aa64dfr1_fields);

	/* AArch64 Auxiliary Feature Register 0 */
	if (SHOULD_PRINT_REG(id_aa64afr0))
		print_id_register(sb, "Auxiliary Features 0",
		    desc->id_aa64afr0, id_aa64afr0_fields);

	/* AArch64 Auxiliary Feature Register 1 */
	if (SHOULD_PRINT_REG(id_aa64afr1))
		print_id_register(sb, "Auxiliary Features 1",
		    desc->id_aa64afr1, id_aa64afr1_fields);

	/* AArch64 SVE Feature Register 0 */
	if (desc->have_sve) {
		if (SHOULD_PRINT_REG(id_aa64zfr0) ||
		    !prev_desc->have_sve) {
			print_id_register(sb, "SVE Features 0",
			    desc->id_aa64zfr0, id_aa64zfr0_fields);
		}
	}

#ifdef COMPAT_FREEBSD32
	/* AArch32 Instruction Set Attribute Register 5 */
	if (SHOULD_PRINT_REG(id_isar5))
		print_id_register(sb, "AArch32 Instruction Set Attributes 5",
		     desc->id_isar5, id_isar5_fields);

	/* AArch32 Media and VFP Feature Register 0 */
	if (SHOULD_PRINT_REG(mvfr0))
		print_id_register(sb, "AArch32 Media and VFP Features 0",
		     desc->mvfr0, mvfr0_fields);

	/* AArch32 Media and VFP Feature Register 1 */
	if (SHOULD_PRINT_REG(mvfr1))
		print_id_register(sb, "AArch32 Media and VFP Features 1",
		     desc->mvfr1, mvfr1_fields);
#endif
	if (bootverbose)
		print_cpu_caches(sb, desc);

	sbuf_delete(sb);
	sb = NULL;
#undef SHOULD_PRINT_REG
#undef SEP_STR
}

void
identify_cache(uint64_t ctr)
{

	/* Identify the L1 cache type */
	switch (CTR_L1IP_VAL(ctr)) {
	case CTR_L1IP_PIPT:
		break;
	case CTR_L1IP_VPIPT:
		icache_vmid = true;
		break;
	default:
	case CTR_L1IP_VIPT:
		icache_aliasing = true;
		break;
	}

	if (dcache_line_size == 0) {
		KASSERT(icache_line_size == 0, ("%s: i-cacheline size set: %ld",
		    __func__, icache_line_size));

		/* Get the D cache line size */
		dcache_line_size = CTR_DLINE_SIZE(ctr);
		/* And the same for the I cache */
		icache_line_size = CTR_ILINE_SIZE(ctr);

		idcache_line_size = MIN(dcache_line_size, icache_line_size);
	}

	if (dcache_line_size != CTR_DLINE_SIZE(ctr)) {
		printf("WARNING: D-cacheline size mismatch %ld != %d\n",
		    dcache_line_size, CTR_DLINE_SIZE(ctr));
	}

	if (icache_line_size != CTR_ILINE_SIZE(ctr)) {
		printf("WARNING: I-cacheline size mismatch %ld != %d\n",
		    icache_line_size, CTR_ILINE_SIZE(ctr));
	}
}

void
identify_cpu(u_int cpu)
{
	struct cpu_desc *desc;
	uint64_t clidr;

	desc = get_cpu_desc(cpu);
	/* Save affinity for current CPU */
	desc->mpidr = get_mpidr();
	CPU_AFFINITY(cpu) = desc->mpidr & CPU_AFF_MASK;

	desc->ctr = READ_SPECIALREG(ctr_el0);
	desc->id_aa64dfr0 = READ_SPECIALREG(id_aa64dfr0_el1);
	desc->id_aa64dfr1 = READ_SPECIALREG(id_aa64dfr1_el1);
	desc->id_aa64isar0 = READ_SPECIALREG(id_aa64isar0_el1);
	desc->id_aa64isar1 = READ_SPECIALREG(id_aa64isar1_el1);
	desc->id_aa64isar2 = READ_SPECIALREG(id_aa64isar2_el1);
	desc->id_aa64mmfr0 = READ_SPECIALREG(id_aa64mmfr0_el1);
	desc->id_aa64mmfr1 = READ_SPECIALREG(id_aa64mmfr1_el1);
	desc->id_aa64mmfr2 = READ_SPECIALREG(id_aa64mmfr2_el1);
#ifdef NOTYET
	desc->id_aa64mmfr3 = READ_SPECIALREG(id_aa64mmfr3_el1);
	desc->id_aa64mmfr4 = READ_SPECIALREG(id_aa64mmfr4_el1);
#endif
	desc->id_aa64pfr0 = READ_SPECIALREG(id_aa64pfr0_el1);
	desc->id_aa64pfr1 = READ_SPECIALREG(id_aa64pfr1_el1);
#ifdef NOTYET
	desc->id_aa64pfr2 = READ_SPECIALREG(id_aa64pfr2_el1);
#endif

	/*
	 * ID_AA64ZFR0_EL1 is only valid when at least one of:
	 *  - ID_AA64PFR0_EL1.SVE is non-zero
	 *  - ID_AA64PFR1_EL1.SME is non-zero
	 * In other cases it is zero, but still safe to read
	 */
	desc->have_sve =
	    (ID_AA64PFR0_SVE_VAL(desc->id_aa64pfr0) != 0);
	desc->id_aa64zfr0 = READ_SPECIALREG(ID_AA64ZFR0_EL1_REG);

	desc->clidr = READ_SPECIALREG(clidr_el1);

	clidr = desc->clidr;

	for (int i = 0; (clidr & CLIDR_CTYPE_MASK) != 0; i++, clidr >>= 3) {
		int j = 0;
		if ((clidr & CLIDR_CTYPE_IO)) {
			WRITE_SPECIALREG(csselr_el1,
			    CSSELR_Level(i) | CSSELR_InD);
			desc->ccsidr[i][j++] =
			    READ_SPECIALREG(ccsidr_el1);
		}
		if ((clidr & ~CLIDR_CTYPE_IO) == 0)
			continue;
		WRITE_SPECIALREG(csselr_el1, CSSELR_Level(i));
		desc->ccsidr[i][j] = READ_SPECIALREG(ccsidr_el1);
	}

#ifdef COMPAT_FREEBSD32
	/* Only read aarch32 SRs if EL0-32 is available */
	if (ID_AA64PFR0_EL0_VAL(desc->id_aa64pfr0) == ID_AA64PFR0_EL0_64_32) {
		desc->id_isar5 = READ_SPECIALREG(id_isar5_el1);
		desc->mvfr0 = READ_SPECIALREG(mvfr0_el1);
		desc->mvfr1 = READ_SPECIALREG(mvfr1_el1);
	}
#endif
}

static void
check_cpu_regs(u_int cpu, struct cpu_desc *desc, struct cpu_desc *prev_desc)
{
	switch (cpu_aff_levels) {
	case 0:
		if (CPU_AFF0(desc->mpidr) != CPU_AFF0(prev_desc->mpidr))
			cpu_aff_levels = 1;
		/* FALLTHROUGH */
	case 1:
		if (CPU_AFF1(desc->mpidr) != CPU_AFF1(prev_desc->mpidr))
			cpu_aff_levels = 2;
		/* FALLTHROUGH */
	case 2:
		if (CPU_AFF2(desc->mpidr) != CPU_AFF2(prev_desc->mpidr))
			cpu_aff_levels = 3;
		/* FALLTHROUGH */
	case 3:
		if (CPU_AFF3(desc->mpidr) != CPU_AFF3(prev_desc->mpidr))
			cpu_aff_levels = 4;
		break;
	}

	if (desc->ctr != prev_desc->ctr) {
		/*
		 * If the cache type register is different we may
		 * have a different l1 cache type.
		 */
		identify_cache(desc->ctr);
	}
}
