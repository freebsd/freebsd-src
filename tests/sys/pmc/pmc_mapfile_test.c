/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * libpmc picks its pmu-events table by matching kern.hwpmc.cpuid against
 * mapfile.csv.  hwpmc prints the model with two digits, so Turin (Family
 * 1Ah, model 02h) is "AuthenticAMD-26-02-<stepping>".  For each cpuid
 * string, look up an event that only the expected table has.  No PMC
 * hardware is needed.
 */

#include <sys/param.h>

#include <string.h>

#include <atf-c.h>
#include <pmc.h>

/*
 * _pmc_cpuid_strip_model_zero() is an internal libpmc interface that
 * pmu_events_map_get() uses to undo hwpmc's zero-padding of the AMD/Hygon
 * model field before matching it against mapfile.csv.
 *
 * It is not part of the installed pmc.h, so declare it here to exercise it
 * directly: this is the only way to check, byte for byte, that cpuid strings
 * outside its AMD/Hygon vendor-prefixed shape come back completely unmodified.
 */
void	_pmc_cpuid_strip_model_zero(char *);

#define	ZEN1_ONLY_EVENT	"bp_l1_tlb_miss_l2_miss"
#define	ZEN2_ONLY_EVENT	"bp_l1_tlb_miss_l2_tlb_miss"
#define	ZEN3_ONLY_EVENT	"bp_l1_btb_correct"
#define	ZEN4_ONLY_EVENT	"bp_l1_tlb_fetch_hit.all"
#define	ZEN5_ONLY_EVENT	"bp_l2_btb_correct"
#define	ZEN6_ONLY_EVENT	"bp_fe_redir.all"

static const char *const zen5_cpuids[] = {
	"AuthenticAMD-26-02-0",
	"AuthenticAMD-26-0F-0",
	"AuthenticAMD-26-11-0",
	"AuthenticAMD-26-2F-0",
	"AuthenticAMD-26-44-0",
	"AuthenticAMD-26-70-0",
	"AuthenticAMD-26-7F-0",
};

static const char *const zen6_cpuids[] = {
	"AuthenticAMD-26-50-0",
	"AuthenticAMD-26-80-0",
	"AuthenticAMD-26-C0-0",
	"AuthenticAMD-26-D8-0",
	"AuthenticAMD-26-EF-0",
};

static void
check_table(const char *cpuid, const char *has, const char *lacks,
    const char *table)
{
	ATF_CHECK_MSG(pmc_pmu_idx_get_by_event(cpuid, has) >= 0 &&
	    pmc_pmu_idx_get_by_event(cpuid, lacks) < 0,
	    "%s does not select %s", cpuid, table);
}

ATF_TC(zen5_models);
ATF_TC_HEAD(zen5_models, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Zen 5 cpuid strings, including models 00h-0Fh, select amdzen5");
}
ATF_TC_BODY(zen5_models, tc)
{
	size_t i;

	for (i = 0; i < nitems(zen5_cpuids); i++)
		check_table(zen5_cpuids[i], ZEN5_ONLY_EVENT, ZEN6_ONLY_EVENT,
		    "amdzen5");
}

ATF_TC(zen6_models);
ATF_TC_HEAD(zen6_models, tc)
{
	atf_tc_set_md_var(tc, "descr", "Zen 6 cpuid strings select amdzen6");
}
ATF_TC_BODY(zen6_models, tc)
{
	size_t i;

	for (i = 0; i < nitems(zen6_cpuids); i++)
		check_table(zen6_cpuids[i], ZEN6_ONLY_EVENT, ZEN5_ONLY_EVENT,
		    "amdzen6");
}

ATF_TC(low_models);
ATF_TC_HEAD(low_models, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Models below 10h, with or without the leading zero");
}
ATF_TC_BODY(low_models, tc)
{
	check_table("AuthenticAMD-23-01-0", ZEN1_ONLY_EVENT, ZEN2_ONLY_EVENT,
	    "amdzen1");
	check_table("HygonGenuine-24-00-0", ZEN1_ONLY_EVENT, ZEN2_ONLY_EVENT,
	    "amdzen1");
	check_table("AuthenticAMD-25-01-0", ZEN3_ONLY_EVENT, ZEN4_ONLY_EVENT,
	    "amdzen3");
	check_table("AuthenticAMD-26-2-0", ZEN5_ONLY_EVENT, ZEN6_ONLY_EVENT,
	    "amdzen5");
}

/*
 * Only AuthenticAMD/HygonGenuine cpuid strings use the
 * "<vendor>-<family>-<model>[-<stepping>]" shape that needs the model's
 * leading zero stripped.  Every other cpuid shape must come back unchanged, in
 * particular arm64's bare "0x<MIDR>" and powerpc's bare "%08x" PVR.  Also,
 * check a string that merely happens to contain two dashes and a zero, to
 * confirm the vendor-prefix check (not "has a dash") is what actually gates
 * the strip.
 */
static const char *const non_amd_cpuids[] = {
	"0x00000000410fd020",	/* arm64 MIDR, cortex-a34 */
	"0x00000000410fd030",	/* arm64 MIDR, cortex-a53 */
	"00390202",		/* powerpc PVR */
	"NotAVendor-1-00-1",	/* two dashes, but not AuthenticAMD/HygonGenuine */
};

ATF_TC(non_amd_cpuid_unchanged);
ATF_TC_HEAD(non_amd_cpuid_unchanged, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Non-AMD/Hygon cpuid strings are never touched by the "
	    "model zero-strip, e.g. arm64's bare 0x<MIDR>");
}
ATF_TC_BODY(non_amd_cpuid_unchanged, tc)
{
	char buf[64];
	size_t i;

	for (i = 0; i < nitems(non_amd_cpuids); i++) {
		strlcpy(buf, non_amd_cpuids[i], sizeof(buf));
		_pmc_cpuid_strip_model_zero(buf);
		ATF_CHECK_MSG(strcmp(buf, non_amd_cpuids[i]) == 0,
		    "%s was modified to %s", non_amd_cpuids[i], buf);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, zen5_models);
	ATF_TP_ADD_TC(tp, zen6_models);
	ATF_TP_ADD_TC(tp, low_models);
	ATF_TP_ADD_TC(tp, non_amd_cpuid_unchanged);

	return (atf_no_error());
}
