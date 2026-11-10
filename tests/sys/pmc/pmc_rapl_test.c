/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Tests for the hwpmc RAPL energy class (hwpmc_rapl.c), which the AMD and
 * Intel backends both register.  The driver only adds a row when its MSR
 * responds, so every case skips when the class or the row it needs is
 * absent (VM, unsupported CPU).  Values are cumulative microjoules and
 * RAPL PMCs are system-scope only; see pmc.rapl(3).
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/pmc.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define	RAPL_EV_PKG	"rapl-energy-pkg"
#define	RAPL_EV_CORES	"rapl-energy-cores"

/* CPU the SC-mode RAPL PMCs are allocated on. */
#define	RAPL_CPU	0

/* Return the PMC_CLASS_RAPL classinfo, or NULL if it is not registered. */
static const struct pmc_classinfo *
rapl_classinfo(void)
{
	const struct pmc_cpuinfo *cpuinfo;
	uint32_t i;

	if (pmc_cpuinfo(&cpuinfo) != 0)
		return (NULL);
	for (i = 0; i < cpuinfo->pm_nclass; i++)
		if (cpuinfo->pm_classes[i].pm_class == PMC_CLASS_RAPL)
			return (&cpuinfo->pm_classes[i]);
	return (NULL);
}

/* Skip unless hwpmc is loaded and the RAPL class is registered. */
static void
skip_unless_rapl(void)
{
	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available: %s", strerror(errno));
	if (rapl_classinfo() == NULL)
		atf_tc_skip("PMC_CLASS_RAPL not registered: the RAPL energy "
		    "MSRs did not respond (VM or unsupported CPU)");
}

/* Return true if RAPL_CPU has a PMC row with the given name. */
static bool
rapl_has_row(const char *row)
{
	struct pmc_pmcinfo *pmcinfo;
	bool found;
	int i, npmc;

	ATF_REQUIRE_MSG(pmc_pmcinfo(RAPL_CPU, &pmcinfo) == 0,
	    "pmc_pmcinfo(%d) failed: %s", RAPL_CPU, strerror(errno));
	npmc = pmc_npmc(RAPL_CPU);
	found = false;
	for (i = 0; i < npmc; i++)
		if (strcmp(pmcinfo->pm_pmcs[i].pm_name, row) == 0)
			found = true;
	free(pmcinfo);
	return (found);
}

/* Busy-spin for at least ms milliseconds so the core consumes energy. */
static void
cpu_spin_ms(unsigned ms)
{
	struct timespec start, now;
	volatile uint64_t sink;
	uint64_t i;
	int64_t elapsed_ms;
	unsigned k;

	ATF_REQUIRE_MSG(clock_gettime(CLOCK_MONOTONIC, &start) == 0,
	    "clock_gettime failed: %s", strerror(errno));

	sink = 0;
	i = 0;
	for (;;) {
		for (k = 0; k < 200000; k++)
			sink += (i++ * 2654435761UL) ^ (sink >> 3);

		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
			break;
		elapsed_ms = (int64_t)(now.tv_sec - start.tv_sec) * 1000 +
		    (now.tv_nsec - start.tv_nsec) / 1000000;
		if (elapsed_ms >= (int64_t)ms)
			break;
	}
}

/*
 * Bind to RAPL_CPU and read the event of the given row across a 500 ms
 * spin there.  Skips if the driver did not register the row; any other
 * failure is an error.
 */
static void
rapl_delta_under_load(const char *event, const char *row,
    pmc_value_t *before, pmc_value_t *after)
{
	cpuset_t mask;
	pmc_id_t id;
	int error;

	if (!rapl_has_row(row))
		atf_tc_skip("the driver registered no %s row", row);

	CPU_ZERO(&mask);
	CPU_SET(RAPL_CPU, &mask);
	if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) != 0)
		atf_tc_skip("cannot bind to cpu%d: %s", RAPL_CPU,
		    strerror(errno));

	id = PMC_ID_INVALID;
	if (pmc_allocate(event, PMC_MODE_SC, 0, RAPL_CPU, &id, 1) != 0)
		atf_tc_fail("pmc_allocate(%s) failed: %s", event,
		    strerror(errno));

	error = 0;
	if (pmc_read(id, before) != 0) {
		error = errno;
	} else {
		cpu_spin_ms(500);
		if (pmc_read(id, after) != 0)
			error = errno;
	}
	ATF_REQUIRE_MSG(pmc_release(id) == 0, "pmc_release failed: %s",
	    strerror(errno));
	if (error != 0)
		atf_tc_fail("pmc_read(%s) failed: %s", event, strerror(error));
}

ATF_TC(rapl_class_present);
ATF_TC_HEAD(rapl_class_present, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "PMC_CLASS_RAPL is registered with 64-bit counters.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(rapl_class_present, tc)
{
	const struct pmc_classinfo *ci;

	skip_unless_rapl();

	ci = rapl_classinfo();
	ATF_REQUIRE(ci != NULL);
	ATF_CHECK_MSG(ci->pm_width == 64,
	    "RAPL counter width expected 64, got %u", ci->pm_width);
}

ATF_TC(rapl_reject_process_mode);
ATF_TC_HEAD(rapl_reject_process_mode, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Allocating a RAPL event in process mode (PMC_MODE_TC) fails "
	    "with EINVAL: RAPL is system-scope only.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(rapl_reject_process_mode, tc)
{
	pmc_id_t id;

	skip_unless_rapl();
	if (!rapl_has_row("RAPL_ENERGY_PKG"))
		atf_tc_skip("the driver registered no RAPL_ENERGY_PKG row");

	id = PMC_ID_INVALID;
	if (pmc_allocate(RAPL_EV_PKG, PMC_MODE_TC, 0, PMC_CPU_ANY, &id,
	    1) == 0) {
		(void)pmc_release(id);
		atf_tc_fail("pmc_allocate(%s, PMC_MODE_TC) succeeded",
		    RAPL_EV_PKG);
	}
	ATF_CHECK_MSG(errno == EINVAL,
	    "expected EINVAL for a process-mode RAPL PMC, got %d (%s)",
	    errno, strerror(errno));
}

ATF_TC(rapl_pkg_delta_under_load);
ATF_TC_HEAD(rapl_pkg_delta_under_load, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "rapl-energy-pkg increases across a 500 ms CPU workload.");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "30");
}

ATF_TC_BODY(rapl_pkg_delta_under_load, tc)
{
	pmc_value_t before, after;

	skip_unless_rapl();

	before = after = 0;
	rapl_delta_under_load(RAPL_EV_PKG, "RAPL_ENERGY_PKG", &before,
	    &after);
	ATF_CHECK_MSG(after > before,
	    "package energy did not increase under load: before=%ju "
	    "after=%ju", (uintmax_t)before, (uintmax_t)after);
}

ATF_TC(rapl_cores_delta_under_load);
ATF_TC_HEAD(rapl_cores_delta_under_load, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "rapl-energy-cores on cpu0 increases across a 500 ms workload "
	    "running on cpu0.");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "30");
}

ATF_TC_BODY(rapl_cores_delta_under_load, tc)
{
	pmc_value_t before, after;

	skip_unless_rapl();

	before = after = 0;
	rapl_delta_under_load(RAPL_EV_CORES, "RAPL_ENERGY_CORES", &before,
	    &after);
	/* Some Intel server parts register a PP0 row that never counts. */
	if (before == 0 && after == 0)
		atf_tc_skip("RAPL_ENERGY_CORES reads 0: the domain is not "
		    "implemented");
	ATF_CHECK_MSG(after > before,
	    "core energy did not increase under load on cpu0: before=%ju "
	    "after=%ju", (uintmax_t)before, (uintmax_t)after);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rapl_class_present);
	ATF_TP_ADD_TC(tp, rapl_reject_process_mode);
	ATF_TP_ADD_TC(tp, rapl_pkg_delta_under_load);
	ATF_TP_ADD_TC(tp, rapl_cores_delta_under_load);

	return (atf_no_error());
}
