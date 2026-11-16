/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Core counter allocation: hwpmc exposes as many core counters as CPUID
 * reports, all of them can be held at once, one more allocation is refused
 * (hwpmc does not multiplex), and a released counter can be allocated
 * again.  Every counter uses ex_ret_instr, which all Zen event tables have.
 */

#include <sys/param.h>
#include <sys/pmc.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "amd_pmc_test_common.h"

#define	CORE_EVENT	"ex_ret_instr"
/* CPUID Fn8000_0022 EBX[3:0] reports at most 15 core counters. */
#define	CORE_PMCS_MAX	15

/* Skip unless the cases can run; return the number of core counters. */
static int
core_counters_or_skip(void)
{
	struct amd_test_cpu cpu;
	int n;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();
	if (!pmc_pmu_enabled())
		atf_tc_skip("pmu-events not enabled");
	n = amd_test_core_counters();
	if (n == 0)
		atf_tc_skip("CPUID reports no core counters");

	return (n);
}

static void
release_all(pmc_id_t *pmcids, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (pmcids[i] != PMC_ID_INVALID) {
			(void)pmc_release(pmcids[i]);
			pmcids[i] = PMC_ID_INVALID;
		}
	}
}

/*
 * Allocate n system-mode PMCs on cpu0.  Skips unless all core rows are free;
 * once they are, any failure to allocate is an error.
 */
static void
hold_core_slots(pmc_id_t *pmcids, int n)
{
	int slot;

	for (slot = 0; slot < n; slot++)
		pmcids[slot] = PMC_ID_INVALID;

	if (amd_test_count_core_rows(0, true) < n)
		atf_tc_skip("not all core counters on cpu0 are free");

	for (slot = 0; slot < n; slot++) {
		if (pmc_allocate(CORE_EVENT, PMC_MODE_SC, 0, 0, &pmcids[slot],
		    1) != 0) {
			release_all(pmcids, slot);
			atf_tc_fail("allocation %d of %d failed: %s", slot, n,
			    strerror(errno));
		}
	}
}

static void
spin(uint64_t iters)
{
	volatile uint64_t sink;
	uint64_t i;

	sink = 0;
	for (i = 0; i < iters; i++)
		sink += i * 3 + 7;
}

ATF_TC(pmu_mux_slot_count);
ATF_TC_HEAD(pmu_mux_slot_count, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_pmcinfo() reports as many core counter rows as CPUID");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_mux_slot_count, tc)
{
	int n, rows;

	n = core_counters_or_skip();

	rows = amd_test_count_core_rows(0, false);
	ATF_CHECK_MSG(rows == n,
	    "CPUID reports %d core counters, pmc_pmcinfo() has %d rows", n,
	    rows);
}

ATF_TC(pmu_mux_fill_all_slots);
ATF_TC_HEAD(pmu_mux_fill_all_slots, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "With every core counter held on cpu0, a held counter still "
	    "counts a known loop, and releasing them frees every row");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_mux_fill_all_slots, tc)
{
	pmc_id_t pmcids[CORE_PMCS_MAX];
	pmc_value_t before, after;
	int free_after, n, slot;

	n = core_counters_or_skip();
	amd_test_bind_cpu0();

	hold_core_slots(pmcids, n);

	for (slot = 0; slot < n; slot++) {
		if (pmc_start(pmcids[slot]) != 0) {
			release_all(pmcids, n);
			atf_tc_fail("pmc_start(slot %d) failed: %s",
			    slot, strerror(errno));
		}
	}

	before = after = 0;
	if (pmc_read(pmcids[0], &before) != 0) {
		release_all(pmcids, n);
		atf_tc_fail("pmc_read(before) failed: %s", strerror(errno));
	}
	spin(2000000);
	if (pmc_read(pmcids[0], &after) != 0) {
		release_all(pmcids, n);
		atf_tc_fail("pmc_read(after) failed: %s", strerror(errno));
	}

	for (slot = 0; slot < n; slot++) {
		if (pmc_stop(pmcids[slot]) != 0) {
			release_all(pmcids, n);
			atf_tc_fail("pmc_stop(slot %d) failed: %s",
			    slot, strerror(errno));
		}
	}
	release_all(pmcids, n);

	ATF_CHECK_MSG(after >= before && after - before >= 2000000,
	    "%s counted fewer than 2M instructions over a 2M-iteration loop "
	    "with all %d core counters held: before=%ju after=%ju",
	    CORE_EVENT, n, (uintmax_t)before, (uintmax_t)after);

	free_after = amd_test_count_core_rows(0, true);
	ATF_CHECK_MSG(free_after == n,
	    "core rows not freed: expected %d free, got %d", n, free_after);
}

ATF_TC(pmu_mux_over_allocation_rejected);
ATF_TC_HEAD(pmu_mux_over_allocation_rejected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "With every core counter held, one more allocation is refused "
	    "with EINVAL and the held counters still start and stop");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_mux_over_allocation_rejected, tc)
{
	pmc_id_t pmcids[CORE_PMCS_MAX];
	pmc_id_t extra;
	int n, slot;

	n = core_counters_or_skip();
	hold_core_slots(pmcids, n);

	extra = PMC_ID_INVALID;
	if (pmc_allocate(CORE_EVENT, PMC_MODE_SC, 0, 0, &extra, 1) == 0) {
		(void)pmc_release(extra);
		atf_tc_fail_nonfatal("one more core counter was allocated "
		    "while all %d were held", n);
	} else {
		/* hwpmc reports "no PMC row accepted the request" as EINVAL. */
		ATF_CHECK_MSG(errno == EINVAL,
		    "the extra allocation failed with unexpected errno %d (%s)",
		    errno, strerror(errno));
	}

	for (slot = 0; slot < n; slot++) {
		ATF_CHECK_MSG(pmc_start(pmcids[slot]) == 0,
		    "held slot %d failed pmc_start: %s",
		    slot, strerror(errno));
		ATF_CHECK_MSG(pmc_stop(pmcids[slot]) == 0,
		    "held slot %d failed pmc_stop: %s",
		    slot, strerror(errno));
	}
	release_all(pmcids, n);
}

ATF_TC(pmu_mux_sequential_reuse);
ATF_TC_HEAD(pmu_mux_sequential_reuse, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "With every core counter held, a released counter can be "
	    "allocated again and counts a known loop");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_mux_sequential_reuse, tc)
{
	pmc_id_t pmcids[CORE_PMCS_MAX];
	pmc_id_t reused;
	pmc_value_t val;
	int n;

	n = core_counters_or_skip();
	amd_test_bind_cpu0();

	hold_core_slots(pmcids, n);

	ATF_REQUIRE_MSG(pmc_release(pmcids[0]) == 0,
	    "pmc_release(slot 0) failed: %s", strerror(errno));
	pmcids[0] = PMC_ID_INVALID;

	reused = PMC_ID_INVALID;
	if (pmc_allocate(CORE_EVENT, PMC_MODE_SC, 0, 0, &reused, 1) != 0) {
		release_all(pmcids, n);
		atf_tc_fail("reallocating the released counter failed: %s",
		    strerror(errno));
	}

	val = 0;
	if (pmc_start(reused) != 0) {
		(void)pmc_release(reused);
		release_all(pmcids, n);
		atf_tc_fail("pmc_start(reused) failed: %s", strerror(errno));
	}
	spin(1000000);
	if (pmc_stop(reused) != 0 || pmc_read(reused, &val) != 0) {
		(void)pmc_release(reused);
		release_all(pmcids, n);
		atf_tc_fail("stop/read of the reused counter failed: %s",
		    strerror(errno));
	}

	(void)pmc_release(reused);
	release_all(pmcids, n);

	ATF_CHECK_MSG(val >= 1000000,
	    "reused counter counted %ju instructions over a 1M-iteration "
	    "loop", (uintmax_t)val);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pmu_mux_slot_count);
	ATF_TP_ADD_TC(tp, pmu_mux_fill_all_slots);
	ATF_TP_ADD_TC(tp, pmu_mux_over_allocation_rejected);
	ATF_TP_ADD_TC(tp, pmu_mux_sequential_reuse);
	return (atf_no_error());
}
