/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/pmc.h>

#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define	WORK_ITERATIONS	1000000

static const struct pmc_classinfo *
perf_classinfo(void)
{
	const struct pmc_cpuinfo *ci;
	uint32_t i;

	ATF_REQUIRE_MSG(pmc_cpuinfo(&ci) == 0, "pmc_cpuinfo() failed: %s",
	    strerror(errno));
	for (i = 0; i < ci->pm_nclass; i++)
		if (ci->pm_classes[i].pm_class == PMC_CLASS_PERF)
			return (&ci->pm_classes[i]);
	return (NULL);
}

static int
perf_setup(void)
{
	cpuset_t mask;
	int cpu;

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not loaded: %s", strerror(errno));
	if (perf_classinfo() == NULL)
		atf_tc_skip("hwpmc does not expose the PERF class");

	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0);
	cpu = CPU_FFS(&mask) - 1;
	ATF_REQUIRE(cpu >= 0);
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0);
	return (cpu);
}

static bool
perf_has_row(int cpu, const char *name)
{
	struct pmc_pmcinfo *info;
	bool found;
	int i, n;

	n = pmc_npmc(cpu);
	ATF_REQUIRE(n >= 0);
	ATF_REQUIRE(pmc_pmcinfo(cpu, &info) == 0);
	found = false;
	for (i = 0; i < n; i++)
		if (info->pm_pmcs[i].pm_class == PMC_CLASS_PERF &&
		    strcmp(info->pm_pmcs[i].pm_name, name) == 0)
			found = true;
	free(info);
	return (found);
}

static void
work(void)
{
	volatile uint64_t x;
	int i;

	x = 1;
	for (i = 0; i < WORK_ITERATIONS; i++)
		x = x * 1664525U + 1013904223U;
}

/* PERF MSRs are free-running: reads advance even while stopped. */
static void
check_counter(const char *row, const char *name, const char *alias)
{
	const char *names[2] = { name, alias };
	pmc_value_t before, previous, running, stopped;
	pmc_id_t id;
	int cpu, i;

	cpu = perf_setup();
	if (!perf_has_row(cpu, row))
		atf_tc_skip("cpu%d has no %s row", cpu, row);

	previous = 0;
	for (i = 0; i < 2; i++) {
		id = PMC_ID_INVALID;
		ATF_REQUIRE_MSG(pmc_allocate(names[i], PMC_MODE_SC, 0, cpu,
		    &id, 0) == 0, "pmc_allocate(%s) failed: %s", names[i],
		    strerror(errno));
		ATF_CHECK_EQ(PMC_ID_TO_CLASS(id), PMC_CLASS_PERF);
		ATF_REQUIRE(pmc_read(id, &before) == 0);
		ATF_CHECK_MSG(before > previous, "%s went back across the "
		    "release: %ju, then %ju", row, (uintmax_t)previous,
		    (uintmax_t)before);
		ATF_REQUIRE(pmc_start(id) == 0);
		work();
		ATF_REQUIRE(pmc_read(id, &running) == 0);
		ATF_REQUIRE(pmc_stop(id) == 0);
		work();
		ATF_REQUIRE(pmc_read(id, &stopped) == 0);
		ATF_CHECK_MSG(running > before && stopped > running,
		    "%s did not advance: %ju, %ju started, %ju stopped", row,
		    (uintmax_t)before, (uintmax_t)running, (uintmax_t)stopped);
		ATF_REQUIRE(pmc_release(id) == 0);
		errno = 0;
		ATF_CHECK_MSG(pmc_read(id, &before) == -1 && errno == ESRCH,
		    "reading the released %s id: errno %d", row, errno);
		previous = stopped;
	}
}

ATF_TC(class_rows);
ATF_TC_HEAD(class_rows, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "The PERF class is 64 bits wide and read-only; it has MPERF and "
	    "APERF rows only if CPUID reports them, an IRPERF row only if "
	    "CPUID reports it, and libpmc names every row");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(class_rows, tc)
{
	static const char *const rows[] = { "MPERF", "APERF", "IRPERF" };
	const struct pmc_classinfo *ci;
	const char **names;
	u_int regs[4];
	bool aperfmperf, irperf, listed;
	size_t r;
	int cpu, i, n, nrows;

	cpu = perf_setup();
	ci = perf_classinfo();
	ATF_CHECK_EQ_MSG(ci->pm_width, 64, "PERF width is %u", ci->pm_width);
	ATF_CHECK_EQ_MSG(ci->pm_caps, PMC_CAP_READ, "PERF caps are %#x",
	    ci->pm_caps);

	do_cpuid(0, regs);
	aperfmperf = false;
	if (regs[0] >= 6) {
		do_cpuid(6, regs);
		aperfmperf = (regs[2] & CPUID_PERF_STAT) != 0;
	}
	do_cpuid(0x80000000, regs);
	irperf = false;
	if (regs[0] >= 0x80000008) {
		do_cpuid(0x80000008, regs);
		irperf = (regs[1] & AMDFEID_IRPERF) != 0;
	}

	ATF_REQUIRE(pmc_event_names_of_class(PMC_CLASS_PERF, &names, &n) == 0);
	nrows = 0;
	for (r = 0; r < nitems(rows); r++) {
		if (!perf_has_row(cpu, rows[r]))
			continue;
		nrows++;
		ATF_CHECK_MSG(r == 2 ? irperf : aperfmperf,
		    "cpu%d has a %s row that CPUID does not report", cpu,
		    rows[r]);
		listed = false;
		for (i = 0; i < n; i++)
			listed |= strcasecmp(names[i], rows[r]) == 0;
		ATF_CHECK_MSG(listed, "libpmc does not name %s", rows[r]);
	}
	free(names);
	ATF_CHECK_MSG(perf_has_row(cpu, "MPERF") == aperfmperf &&
	    perf_has_row(cpu, "APERF") == aperfmperf,
	    "MPERF and APERF rows do not match CPUID");
	ATF_CHECK_EQ_MSG((uint32_t)nrows, ci->pm_num,
	    "%d PERF rows on cpu%d, the class reports %u", nrows, cpu,
	    ci->pm_num);
}

ATF_TC(mperf_counts);
ATF_TC_HEAD(mperf_counts, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "MPERF advances under load when started and when stopped, and "
	    "continues across a release");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(mperf_counts, tc)
{
	check_counter("MPERF", "perf-mperf", "mperf");
}

ATF_TC(aperf_counts);
ATF_TC_HEAD(aperf_counts, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "APERF advances under load when started and when stopped, and "
	    "continues across a release");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(aperf_counts, tc)
{
	check_counter("APERF", "perf-aperf", "aperf");
}

ATF_TC(irperf_counts);
ATF_TC_HEAD(irperf_counts, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "IRPERF advances under load when started and when stopped, and "
	    "continues across a release");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(irperf_counts, tc)
{
	check_counter("IRPERF", "perf-irperf", "irperf");
}

ATF_TC(irperf_matches_instructions);
ATF_TC_HEAD(irperf_matches_instructions, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Over the same loop on one CPU, IRPERF and a system-mode "
	    "instructions PMC advance by amounts within 10% of each other");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(irperf_matches_instructions, tc)
{
	pmc_value_t i0, i1, r0, r1;
	pmc_id_t instr, irperf;
	uint64_t di, dr;
	int cpu;

	cpu = perf_setup();
	if (!perf_has_row(cpu, "IRPERF"))
		atf_tc_skip("cpu%d has no IRPERF row", cpu);
	irperf = instr = PMC_ID_INVALID;
	ATF_REQUIRE(pmc_allocate("perf-irperf", PMC_MODE_SC, 0, cpu, &irperf,
	    0) == 0);
	if (pmc_allocate("instructions", PMC_MODE_SC, 0, cpu, &instr, 0) != 0)
		atf_tc_skip("no instructions event: %s", strerror(errno));
	ATF_REQUIRE(pmc_start(irperf) == 0);
	ATF_REQUIRE(pmc_start(instr) == 0);

	/* The IRPERF reads bracket the instructions reads. */
	ATF_REQUIRE(pmc_read(irperf, &r0) == 0);
	ATF_REQUIRE(pmc_read(instr, &i0) == 0);
	work();
	work();
	ATF_REQUIRE(pmc_read(instr, &i1) == 0);
	ATF_REQUIRE(pmc_read(irperf, &r1) == 0);
	ATF_CHECK(pmc_stop(instr) == 0);
	ATF_CHECK(pmc_release(instr) == 0);
	ATF_CHECK(pmc_stop(irperf) == 0);
	ATF_CHECK(pmc_release(irperf) == 0);

	di = i1 - i0;
	dr = r1 - r0;
	ATF_CHECK_MSG(di > 0 && dr * 10 >= di * 9 && dr * 10 <= di * 11,
	    "IRPERF advanced by %ju, instructions by %ju", (uintmax_t)dr,
	    (uintmax_t)di);
}

ATF_TC(mode_refusals);
ATF_TC_HEAD(mode_refusals, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A PERF counter is refused in process counting mode (EINVAL), in "
	    "either sampling mode (EOPNOTSUPP) and with a qualifier (EINVAL)");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(mode_refusals, tc)
{
	static const struct {
		const char	*event;
		enum pmc_mode	mode;
		int		error;
	} cases[] = {
		{ "perf-mperf",		PMC_MODE_TC,	EINVAL },
		{ "perf-mperf",		PMC_MODE_SS,	EOPNOTSUPP },
		{ "perf-mperf",		PMC_MODE_TS,	EOPNOTSUPP },
		{ "perf-mperf,usr",	PMC_MODE_SC,	EINVAL },
	};
	pmc_id_t id;
	size_t i;
	int cpu, rc;

	cpu = perf_setup();
	if (!perf_has_row(cpu, "MPERF"))
		atf_tc_skip("cpu%d has no MPERF row", cpu);
	for (i = 0; i < nitems(cases); i++) {
		id = PMC_ID_INVALID;
		errno = 0;
		rc = pmc_allocate(cases[i].event, cases[i].mode, 0,
		    PMC_IS_SYSTEM_MODE(cases[i].mode) ? cpu : PMC_CPU_ANY,
		    &id, 0);
		if (rc == 0)
			ATF_CHECK(pmc_release(id) == 0);
		ATF_CHECK_MSG(rc == -1 && errno == cases[i].error,
		    "%s in mode %d: rc %d, errno %d, expected %d",
		    cases[i].event, cases[i].mode, rc, errno, cases[i].error);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, class_rows);
	ATF_TP_ADD_TC(tp, mperf_counts);
	ATF_TP_ADD_TC(tp, aperf_counts);
	ATF_TP_ADD_TC(tp, irperf_counts);
	ATF_TP_ADD_TC(tp, irperf_matches_instructions);
	ATF_TP_ADD_TC(tp, mode_refusals);

	return (atf_no_error());
}
