/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * AMD RAPL domain layout: AMD has package and core energy MSRs
 * (MSR_AMD_PKG_ENERGY_STATUS, MSR_AMD_CORE_ENERGY_STATUS) but no DRAM
 * domain, so hwpmc_rapl.c registers at most the PKG and CORES rows.  The
 * vendor-neutral RAPL tests are in ../pmc_rapl_test.c.
 */

#include <sys/param.h>
#include <sys/pmc.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdint.h>
#include <string.h>

#include "amd_pmc_test_common.h"

/* CPU the SC-mode RAPL PMCs are allocated on. */
#define	RAPL_CPU	0

/* Return the PMC_CLASS_RAPL classinfo, or NULL if it is not registered. */
static const struct pmc_classinfo *
rapl_classinfo(void)
{
	const struct pmc_cpuinfo *cpuinfo;

	if (pmc_cpuinfo(&cpuinfo) != 0)
		return (NULL);

	return (amd_test_find_classinfo(cpuinfo, PMC_CLASS_RAPL));
}

/* Skip unless this is AMD with hwpmc and a registered RAPL class. */
static void
skip_unless_rapl(void)
{
	amd_test_skip_unless_amd();
	amd_test_skip_unless_hwpmc();

	if (rapl_classinfo() == NULL)
		atf_tc_skip("PMC_CLASS_RAPL not registered: the RAPL energy "
		    "MSRs did not respond (VM or unsupported CPU)");
}

ATF_TC(rapl_metadata_names);
ATF_TC_HEAD(rapl_metadata_names, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "On AMD the RAPL class has at most two rows, PKG and CORES "
	    "(there is no DRAM domain), all with the RAPL prefix.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(rapl_metadata_names, tc)
{
	const struct pmc_classinfo *ci;
	size_t nrows;

	skip_unless_rapl();

	ci = rapl_classinfo();
	ATF_REQUIRE(ci != NULL);
	/* The driver registers only the rows whose MSR responds. */
	ATF_CHECK_MSG(ci->pm_num >= 1 && ci->pm_num <= 2,
	    "AMD RAPL expected 1 or 2 rows (PKG, CORES), got %u",
	    ci->pm_num);

	nrows = amd_test_count_pmc_rows_with_prefix(RAPL_CPU, "RAPL");
	ATF_CHECK_MSG(nrows == (size_t)ci->pm_num,
	    "expected %u RAPL-prefixed rows in pmc_pmcinfo, got %zu",
	    ci->pm_num, nrows);
}

ATF_TC(rapl_no_dram_on_amd);
ATF_TC_HEAD(rapl_no_dram_on_amd, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "AMD has no RAPL DRAM domain: allocating rapl-energy-dram fails "
	    "while rapl-energy-pkg can be allocated.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(rapl_no_dram_on_amd, tc)
{
	pmc_id_t id;

	skip_unless_rapl();
	if (amd_test_count_pmc_rows_with_prefix(RAPL_CPU,
	    "RAPL_ENERGY_PKG") == 0)
		atf_tc_skip("the driver registered no RAPL_ENERGY_PKG row");

	/* Show that RAPL allocation works before expecting a refusal. */
	id = PMC_ID_INVALID;
	if (pmc_allocate("rapl-energy-pkg", PMC_MODE_SC, 0, RAPL_CPU, &id,
	    1) != 0)
		atf_tc_fail("pmc_allocate(rapl-energy-pkg) failed: %s",
		    strerror(errno));
	amd_test_release_pmc(id);

	/*
	 * Either libpmc (no DRAM row on AMD) or the driver (wrong event for
	 * the row) may refuse the request; only the refusal is asserted.
	 */
	id = PMC_ID_INVALID;
	if (pmc_allocate("rapl-energy-dram", PMC_MODE_SC, 0, RAPL_CPU, &id,
	    1) == 0) {
		amd_test_release_pmc(id);
		atf_tc_fail("pmc_allocate(rapl-energy-dram) succeeded on AMD");
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rapl_metadata_names);
	ATF_TP_ADD_TC(tp, rapl_no_dram_on_amd);

	return (atf_no_error());
}
