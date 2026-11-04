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
 * 1Ah, model 02h) is "AuthenticAMD-26-02-<stepping>".  For Zen 5 and Zen 6
 * cpuid strings, look up an event that only one of amdzen5 and amdzen6 has.
 * No PMC hardware is needed.
 */

#include <sys/param.h>

#include <atf-c.h>
#include <pmc.h>

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

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, zen5_models);
	ATF_TP_ADD_TC(tp, zen6_models);

	return (atf_no_error());
}
