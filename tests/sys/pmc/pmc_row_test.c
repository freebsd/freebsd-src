/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/pmc.h>

#include <errno.h>
#include <pmc.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

/* The software class lets these run on any CPU. */
#define	SOFT_EVENT	"SOFT-CLOCK.HARD"

static void
require_hwpmc(void)
{
	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available: %s", strerror(errno));
}

static enum pmc_disp
row_disposition(pmc_id_t pmcid)
{
	struct pmc_pmcinfo *pmcinfo;
	enum pmc_disp disp;
	unsigned int row;
	int npmc;

	row = PMC_ID_TO_ROWINDEX(pmcid);
	npmc = pmc_npmc(0);
	ATF_REQUIRE_MSG(npmc > 0, "pmc_npmc(0) failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(row < (unsigned int)npmc,
	    "row %u is beyond the %d rows of cpu0", row, npmc);
	ATF_REQUIRE_MSG(pmc_pmcinfo(0, &pmcinfo) == 0,
	    "pmc_pmcinfo(0) failed: %s", strerror(errno));
	disp = pmcinfo->pm_pmcs[row].pm_rowdisp;
	free(pmcinfo);
	return (disp);
}

ATF_TC(row_disposition_follows_mode);
ATF_TC_HEAD(row_disposition_follows_mode, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_pmcinfo() reports the row of a process-mode PMC as THREAD "
	    "and the row of a system-mode PMC as STANDALONE");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(row_disposition_follows_mode, tc)
{
	enum pmc_disp disp;
	pmc_id_t id;

	require_hwpmc();

	ATF_REQUIRE_MSG(pmc_allocate(SOFT_EVENT, PMC_MODE_TC, 0, PMC_CPU_ANY,
	    &id, 0) == 0, "pmc_allocate(%s, TC) failed: %s", SOFT_EVENT,
	    strerror(errno));
	disp = row_disposition(id);
	ATF_CHECK_MSG(disp == PMC_DISP_THREAD,
	    "process-mode row is %s, expected %s",
	    pmc_name_of_disposition(disp),
	    pmc_name_of_disposition(PMC_DISP_THREAD));
	ATF_REQUIRE(pmc_release(id) == 0);

	ATF_REQUIRE_MSG(pmc_allocate(SOFT_EVENT, PMC_MODE_SC, 0, 0, &id,
	    0) == 0, "pmc_allocate(%s, SC) failed: %s", SOFT_EVENT,
	    strerror(errno));
	disp = row_disposition(id);
	ATF_CHECK_MSG(disp == PMC_DISP_STANDALONE,
	    "system-mode row is %s, expected %s",
	    pmc_name_of_disposition(disp),
	    pmc_name_of_disposition(PMC_DISP_STANDALONE));
	ATF_REQUIRE(pmc_release(id) == 0);
}

ATF_TC(system_sampling_needs_logfile);
ATF_TC_HEAD(system_sampling_needs_logfile, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Starting a system sampling PMC without a configured log file "
	    "fails with EDOOFUS");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(system_sampling_needs_logfile, tc)
{
	pmc_id_t id;
	int error;

	require_hwpmc();

	ATF_REQUIRE_MSG(pmc_allocate(SOFT_EVENT, PMC_MODE_SS, 0, 0, &id,
	    10000) == 0, "pmc_allocate(%s, SS) failed: %s", SOFT_EVENT,
	    strerror(errno));
	errno = 0;
	error = pmc_start(id);
	if (error == 0) {
		/* pmc_release() stops a running PMC itself. */
		(void)pmc_release(id);
		atf_tc_fail("pmc_start() succeeded without a log file");
	}
	ATF_CHECK_MSG(errno == EDOOFUS,
	    "pmc_start() failed with %d (%s), expected EDOOFUS", errno,
	    strerror(errno));
	ATF_REQUIRE(pmc_release(id) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, row_disposition_follows_mode);
	ATF_TP_ADD_TC(tp, system_sampling_needs_logfile);

	return (atf_no_error());
}
