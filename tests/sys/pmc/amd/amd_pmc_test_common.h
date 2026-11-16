/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 */

/*
 * Common helpers for the AMD hwpmc tests.
 */

#ifndef _AMD_PMC_TEST_COMMON_H_
#define	_AMD_PMC_TEST_COMMON_H_

#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define	AMD_CPUID_VENDOR_EBX	0x68747541U	/* "Auth" */
#define	AMD_CPUID_VENDOR_EDX	0x69746e65U	/* "enti" */
#define	AMD_CPUID_VENDOR_ECX	0x444d4163U	/* "cAMD" */

/* Ordered oldest to newest so that generations can be compared. */
enum amd_zen {
	AMD_ZEN_PRE_ZEN = 0,
	AMD_ZEN_1,
	AMD_ZEN_PLUS,
	AMD_ZEN_2,
	AMD_ZEN_3,
	AMD_ZEN_3_PLUS,
	AMD_ZEN_4,
	AMD_ZEN_5,
	AMD_ZEN_6,
	AMD_ZEN_FUTURE,
	AMD_ZEN_UNKNOWN
};

struct amd_test_cpu {
	uint32_t	family;
	uint32_t	model;
	enum amd_zen	zen;
};

static inline const char *
amd_test_zen_name(enum amd_zen zen)
{
	switch (zen) {
	case AMD_ZEN_PRE_ZEN:
		return ("pre-Zen");
	case AMD_ZEN_1:
		return ("Zen 1");
	case AMD_ZEN_PLUS:
		return ("Zen+");
	case AMD_ZEN_2:
		return ("Zen 2");
	case AMD_ZEN_3:
		return ("Zen 3");
	case AMD_ZEN_3_PLUS:
		return ("Zen 3+");
	case AMD_ZEN_4:
		return ("Zen 4");
	case AMD_ZEN_5:
		return ("Zen 5");
	case AMD_ZEN_6:
		return ("Zen 6");
	case AMD_ZEN_FUTURE:
		return ("post-Zen 6");
	default:
		return ("unknown");
	}
}

static inline enum amd_zen
amd_test_map_zen(uint32_t family, uint32_t model)
{
	if (family < 0x17)
		return (AMD_ZEN_PRE_ZEN);
	if (family > 0x1a)
		return (AMD_ZEN_FUTURE);

	switch (family) {
	case 0x17:
		if (model >= 0x08 && model <= 0x0f)	/* Pinnacle Ridge */
			return (AMD_ZEN_PLUS);
		if ((model >= 0x01 && model <= 0x07) ||	/* Summit Ridge */
		    (model >= 0x10 && model <= 0x17) ||	/* Raven Ridge */
		    (model >= 0x20 && model <= 0x2f))	/* Dali */
			return (AMD_ZEN_1);
		if (model >= 0x18 && model <= 0x1f)	/* Picasso */
			return (AMD_ZEN_PLUS);
		if ((model >= 0x31 && model <= 0x3f) ||	/* Rome */
		    (model >= 0x60 && model <= 0x6f) ||	/* Renoir */
		    (model >= 0x71 && model <= 0x7f) ||	/* Matisse */
		    (model >= 0x90 && model <= 0x9f) ||	/* Van Gogh */
		    (model >= 0xa0 && model <= 0xaf))	/* Mendocino */
			return (AMD_ZEN_2);
		break;
	case 0x19:
		if ((model >= 0x10 && model <= 0x1f) ||	/* Genoa, Storm Peak */
		    (model >= 0x60 && model <= 0x7f) ||	/* Raphael, Phoenix */
		    (model >= 0xa0 && model <= 0xaf))	/* Bergamo, Siena */
			return (AMD_ZEN_4);
		if (model >= 0x40 && model <= 0x4f)	/* Rembrandt */
			return (AMD_ZEN_3_PLUS);
		if (model <= 0x0f ||				/* Milan */
		    (model >= 0x20 && model <= 0x2f) ||	/* Vermeer */
		    (model >= 0x50 && model <= 0x5f))	/* Cezanne */
			return (AMD_ZEN_3);
		break;
	case 0x1a:
		if ((model >= 0x50 && model <= 0x5f) ||	/* Medusa */
		    (model >= 0x80 && model <= 0xaf) ||
		    (model >= 0xc0 && model <= 0xcf) ||
		    (model >= 0xd8 && model <= 0xef))
			return (AMD_ZEN_6);
		/* Turin, Strix, Krackan, Fire Range */
		if (model <= 0x2f || (model >= 0x40 && model <= 0x4f) ||
		    (model >= 0x60 && model <= 0x7f) ||
		    (model >= 0xd0 && model <= 0xd7))
			return (AMD_ZEN_5);
		break;
	}

	return (AMD_ZEN_UNKNOWN);
}

static inline bool
amd_test_is_amd(void)
{
	u_int regs[4];

	do_cpuid(0, regs);
	return (regs[1] == AMD_CPUID_VENDOR_EBX &&
	    regs[3] == AMD_CPUID_VENDOR_EDX &&
	    regs[2] == AMD_CPUID_VENDOR_ECX);
}

static inline void
amd_test_skip_unless_amd(void)
{
	if (!amd_test_is_amd())
		atf_tc_skip("CPU vendor is not AuthenticAMD");
}

/* Skip unless this is an AMD CPU of a known Zen generation. */
static inline void
amd_test_skip_unless_zen(struct amd_test_cpu *cpu)
{
	u_int regs[4];

	amd_test_skip_unless_amd();

	do_cpuid(1, regs);
	cpu->family = (regs[0] >> 8) & 0x0f;
	cpu->model = (regs[0] >> 4) & 0x0f;
	if (cpu->family == 0x0f) {
		cpu->family += (regs[0] >> 20) & 0xff;
		cpu->model |= ((regs[0] >> 16) & 0x0f) << 4;
	}
	cpu->zen = amd_test_map_zen(cpu->family, cpu->model);

	if (cpu->zen == AMD_ZEN_PRE_ZEN)
		atf_tc_skip("AMD family %02xh predates Zen", cpu->family);
	if (cpu->zen == AMD_ZEN_FUTURE || cpu->zen == AMD_ZEN_UNKNOWN)
		atf_tc_skip("AMD family %02xh model %02xh is not in the "
		    "Zen map", cpu->family, cpu->model);
}

/* Without PerfCtrExtCore (e.g. some guests) hwpmc exposes 4 core counters. */
static inline void
amd_test_skip_unless_perfctr_extcore(void)
{
	u_int regs[4];

	do_cpuid(0x80000001, regs);
	if ((regs[2] & AMDID2_PCXC) == 0)
		atf_tc_skip("CPU lacks PerfCtrExtCore; only 4 core counters");
}

/*
 * The number of core counters, computed as hwpmc_amd.c does: Fn8000_0022
 * EBX[3:0] when that leaf reports counters, otherwise 6 with PerfCtrExtCore
 * and 4 without.
 */
static inline int
amd_test_core_counters(void)
{
	u_int regs[4];
	int n;

	do_cpuid(0x80000001, regs);
	n = (regs[2] & AMDID2_PCXC) != 0 ? 6 : 4;
	do_cpuid(0x80000000, regs);
	if (regs[0] >= 0x80000022) {
		do_cpuid(0x80000022, regs);
		if (regs[1] != 0)
			n = regs[1] & 0x0f;
	}
	return (n);
}

static inline void
amd_test_skip_unless_hwpmc(void)
{
	if (pmc_init() < 0)
		atf_tc_skip("pmc_init() failed: %s", strerror(errno));
}

/* Bind the calling thread to cpu0, where the system-mode PMCs count. */
static inline void
amd_test_bind_cpu0(void)
{
	cpuset_t mask;

	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) != 0)
		atf_tc_skip("cannot bind to cpu0: %s", strerror(errno));
}

static inline const struct pmc_classinfo *
amd_test_find_classinfo(const struct pmc_cpuinfo *cpuinfo, enum pmc_class cl)
{
	uint32_t i;

	for (i = 0; i < cpuinfo->pm_nclass; i++)
		if (cpuinfo->pm_classes[i].pm_class == cl)
			return (&cpuinfo->pm_classes[i]);

	return (NULL);
}

static inline size_t
amd_test_count_pmc_rows_with_prefix(int cpu, const char *prefix)
{
	struct pmc_pmcinfo *pmcinfo;
	size_t count;
	int i, npmc;

	count = 0;
	ATF_REQUIRE_MSG(pmc_pmcinfo(cpu, &pmcinfo) == 0,
	    "pmc_pmcinfo(%d) failed: %s", cpu, strerror(errno));

	npmc = pmc_npmc(cpu);
	if (npmc < 0) {
		free(pmcinfo);
		atf_tc_fail("pmc_npmc(%d) failed: %s", cpu, strerror(errno));
	}

	for (i = 0; i < npmc; i++) {
		if (strncmp(pmcinfo->pm_pmcs[i].pm_name, prefix,
		    strlen(prefix)) == 0)
			count++;
	}

	free(pmcinfo);
	return (count);
}

/*
 * Count the AMD core counter rows ("K8-<n>") on a CPU.  The L3, DF and UMC
 * rows share the K8 class and prefix ("K8-L3-", "K8-DF-", "K8-UMC") and are
 * not core counters.  With free_only, count only rows that are free.
 */
static inline int
amd_test_count_core_rows(int cpu, bool free_only)
{
	struct pmc_pmcinfo *pmcinfo;
	const struct pmc_info *p;
	int count, i, npmc;

	ATF_REQUIRE_MSG(pmc_pmcinfo(cpu, &pmcinfo) == 0,
	    "pmc_pmcinfo(%d) failed: %s", cpu, strerror(errno));

	npmc = pmc_npmc(cpu);
	if (npmc < 0) {
		free(pmcinfo);
		atf_tc_fail("pmc_npmc(%d) failed: %s", cpu, strerror(errno));
	}

	count = 0;
	for (i = 0; i < npmc; i++) {
		p = &pmcinfo->pm_pmcs[i];
		if (strncmp(p->pm_name, "K8-", 3) != 0 ||
		    p->pm_name[3] < '0' || p->pm_name[3] > '9')
			continue;
		if (free_only && p->pm_rowdisp != PMC_DISP_FREE)
			continue;
		count++;
	}

	free(pmcinfo);
	return (count);
}

static inline void
amd_test_release_pmc(pmc_id_t pmcid)
{
	if (pmcid != PMC_ID_INVALID)
		ATF_REQUIRE_MSG(pmc_release(pmcid) == 0,
		    "pmc_release(%u) failed: %s", pmcid, strerror(errno));
}

#endif /* _AMD_PMC_TEST_COMMON_H_ */
