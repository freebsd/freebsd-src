/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/wait.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "amd_pmc_test_common.h"

#define	CORE_EVENT	"unhalted-cycles"

struct row_snapshot {
	int		ncpu;
	int		npmc;
	enum pmc_disp	*disp;
};

static void
snapshot_take(struct row_snapshot *snap)
{
	struct pmc_pmcinfo *pmcinfo;
	int cpu, i;

	snap->ncpu = pmc_ncpu();
	snap->npmc = pmc_npmc(0);
	ATF_REQUIRE_MSG(snap->ncpu > 0 && snap->npmc > 0,
	    "pmc_ncpu/pmc_npmc failed: %s", strerror(errno));
	snap->disp = calloc((size_t)snap->ncpu * snap->npmc,
	    sizeof(*snap->disp));
	ATF_REQUIRE(snap->disp != NULL);
	for (cpu = 0; cpu < snap->ncpu; cpu++) {
		/* CPUs that are absent or halted have no PMC state. */
		if (pmc_pmcinfo(cpu, &pmcinfo) != 0)
			continue;
		for (i = 0; i < snap->npmc; i++)
			snap->disp[cpu * snap->npmc + i] =
			    pmcinfo->pm_pmcs[i].pm_rowdisp;
		free(pmcinfo);
	}
}

static void
snapshot_require_equal(struct row_snapshot *a, struct row_snapshot *b,
    const char *what)
{
	size_t i, n;

	ATF_REQUIRE(a->ncpu == b->ncpu && a->npmc == b->npmc);
	n = (size_t)a->ncpu * a->npmc;
	for (i = 0; i < n; i++)
		ATF_CHECK_MSG(a->disp[i] == b->disp[i],
		    "%s changed cpu%zu row %zu from %s to %s", what,
		    i / a->npmc, i % a->npmc,
		    pmc_name_of_disposition(a->disp[i]),
		    pmc_name_of_disposition(b->disp[i]));
}

static void
snapshot_free(struct row_snapshot *snap)
{
	free(snap->disp);
	snap->disp = NULL;
}

static void
skip_unless_ready(void)
{
	struct amd_test_cpu cpu;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();
	if (!pmc_pmu_enabled())
		atf_tc_skip("pmu-events not enabled");
}

static int
alloc_tc(pmc_id_t *id)
{
	*id = PMC_ID_INVALID;
	return (pmc_allocate(CORE_EVENT, PMC_MODE_TC, 0, PMC_CPU_ANY, id,
	    0) == 0 ? 0 : errno);
}

static size_t
fill_rows(pmc_id_t *ids, size_t max)
{
	size_t n;
	int error;

	for (n = 0; n < max; n++) {
		error = alloc_tc(&ids[n]);
		if (error == 0)
			continue;
		ATF_REQUIRE_MSG(n > 0 && error == EINVAL,
		    "allocation %zu failed with %d (%s), expected EINVAL "
		    "after at least one success", n, error, strerror(error));
		return (n);
	}
	atf_tc_fail("hwpmc granted %zu process-mode PMCs without refusing one",
	    max);
}

static void
release_all(pmc_id_t *ids, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		if (ids[i] != PMC_ID_INVALID)
			amd_test_release_pmc(ids[i]);
}

ATF_TC(alloc_reserves_distinct_rows);
ATF_TC_HEAD(alloc_reserves_distinct_rows, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Two process-mode core PMCs get two distinct rows, both reported "
	    "as THREAD");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(alloc_reserves_distinct_rows, tc)
{
	struct row_snapshot snap;
	pmc_id_t id[2];
	unsigned int r0, r1;
	int error, i;

	skip_unless_ready();
	for (i = 0; i < 2; i++) {
		error = alloc_tc(&id[i]);
		ATF_REQUIRE_MSG(error == 0, "allocation %d failed: %s", i,
		    strerror(error));
	}

	r0 = PMC_ID_TO_ROWINDEX(id[0]);
	r1 = PMC_ID_TO_ROWINDEX(id[1]);
	ATF_CHECK_MSG(r0 != r1, "both PMCs got row %u", r0);
	snapshot_take(&snap);
	ATF_CHECK_MSG(snap.disp[r0] == PMC_DISP_THREAD &&
	    snap.disp[r1] == PMC_DISP_THREAD,
	    "rows %u and %u are %s and %s, expected THREAD", r0, r1,
	    pmc_name_of_disposition(snap.disp[r0]),
	    pmc_name_of_disposition(snap.disp[r1]));
	snapshot_free(&snap);

	release_all(id, nitems(id));
}

ATF_TC(failed_alloc_leaves_no_trace);
ATF_TC_HEAD(failed_alloc_leaves_no_trace, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Once the core rows are full, a refused allocation leaves every "
	    "row disposition unchanged, and releasing restores the start "
	    "state");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(failed_alloc_leaves_no_trace, tc)
{
	struct row_snapshot after, before, full, initial;
	pmc_id_t extra, *ids;
	size_t max, n;
	int error;

	skip_unless_ready();
	max = (size_t)pmc_npmc(0) + 1;
	ids = calloc(max, sizeof(*ids));
	ATF_REQUIRE(ids != NULL);

	snapshot_take(&initial);
	n = fill_rows(ids, max);
	snapshot_take(&full);
	error = alloc_tc(&extra);
	if (error == 0)
		amd_test_release_pmc(extra);
	ATF_CHECK_MSG(error == EINVAL,
	    "an allocation beyond %zu rows returned %d (%s), expected EINVAL",
	    n, error, strerror(error));
	snapshot_take(&before);
	snapshot_require_equal(&full, &before, "a refused allocation");

	release_all(ids, n);
	snapshot_take(&after);
	snapshot_require_equal(&initial, &after, "allocate and release");

	snapshot_free(&after);
	snapshot_free(&before);
	snapshot_free(&full);
	snapshot_free(&initial);
	free(ids);
}

ATF_TC(thread_rows_refuse_system_pmc);
ATF_TC_HEAD(thread_rows_refuse_system_pmc, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "With every core row held by process-mode PMCs of another "
	    "process, a system-mode core PMC is refused with EINVAL and no "
	    "row disposition changes");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(thread_rows_refuse_system_pmc, tc)
{
	struct row_snapshot after, full;
	pmc_id_t id, sys;
	pid_t child;
	size_t max, n;
	int error, hold[2], report[2], status;
	char c;

	skip_unless_ready();
	max = (size_t)pmc_npmc(0) + 1;
	ATF_REQUIRE(pipe(hold) == 0);
	ATF_REQUIRE(pipe(report) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		/* Another owner holds the rows, not the parent. */
		close(hold[1]);
		close(report[0]);
		error = 0;
		for (n = 0; n < max && (error = alloc_tc(&id)) == 0; n++)
			;
		c = n > 0 && error == EINVAL ? 'f' : 'e';
		(void)write(report[1], &c, 1);
		(void)read(hold[0], &c, 1);
		_exit(0);
	}
	close(hold[0]);
	close(report[1]);
	if (read(report[0], &c, 1) != 1 || c != 'f') {
		close(hold[1]);
		(void)waitpid(child, NULL, 0);
		atf_tc_fail("the child could not fill the core rows");
	}

	snapshot_take(&full);
	sys = PMC_ID_INVALID;
	error = pmc_allocate(CORE_EVENT, PMC_MODE_SC, 0, 0, &sys, 0) == 0 ?
	    0 : errno;
	if (error == 0)
		amd_test_release_pmc(sys);
	snapshot_take(&after);
	close(hold[1]);
	close(report[0]);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);

	ATF_CHECK_MSG(error == EINVAL,
	    "system-mode allocation returned %d (%s), expected EINVAL", error,
	    strerror(error));
	snapshot_require_equal(&full, &after, "a refused system allocation");
	ATF_CHECK_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "child failed: status %#x", status);
	snapshot_free(&after);
	snapshot_free(&full);

	/* Positive control: the rows are free again. */
	error = pmc_allocate(CORE_EVENT, PMC_MODE_SC, 0, 0, &sys, 0) == 0 ?
	    0 : errno;
	ATF_REQUIRE_MSG(error == 0,
	    "system-mode allocation on free rows failed: %s", strerror(error));
	amd_test_release_pmc(sys);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, alloc_reserves_distinct_rows);
	ATF_TP_ADD_TC(tp, failed_alloc_leaves_no_trace);
	ATF_TP_ADD_TC(tp, thread_rows_refuse_system_pmc);

	return (atf_no_error());
}
