/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 *   [UNIT]  No hardware: AMD_PMC_TO_EVENTMASK splits a 12-bit EventSelect
 *           into config word bits 7:0 and 35:32.
 *
 *   [SMOKE] pmc_pmu_pmcallocate() lookup by name for well-known core
 *           events of the running Zen generation.  Verifies EventSelect
 *           bits 7:0 + 35:32, UnitMask bits 15:8, and OsUser bits 17:16
 *           in the returned config word.
 *
 *   [E2E]   Count ex_ret_instr on cpu0 across a known loop.
 *
 * Reference:
 *   AMD PPR for Family 19h Model 11h: PERF_CTL MSR bit layout:
 *     bits  7:0   EventSelect[7:0]
 *     bits 15:8   UnitMask[7:0]
 *     bit  16     USR (count in user mode)
 *     bit  17     OS  (count in OS/kernel mode)
 *     bits 35:32  EventSelect[11:8]
 *     bit  40     GUEST (count in guest mode)
 *     bit  41     HOST  (count in host mode)
 *
 *   hwpmc macros (sys/dev/hwpmc/hwpmc_amd.h):
 *     AMD_PMC_TO_EVENTMASK(x) = ((x) & 0xFF) | (((uint64_t)(x) & 0xF00) << 24)
 *     AMD_PMC_TO_UNITMASK(x)  = ((x) << 8) & 0xFF00
 *     AMD_PMC_USR              = (1 << 16)
 *     AMD_PMC_OS               = (1 << 17)
 */

#include <sys/param.h>
#include <sys/pmc.h>

#include <dev/hwpmc/hwpmc_amd.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "amd_pmc_test_common.h"

/* EventSelect[7:0] from config word bits 7:0 */
#define	EVTEST_EVSEL_LO(cfg)	((uint32_t)((cfg) & 0xFFU))
/* EventSelect[11:8] from config word bits 35:32 */
#define	EVTEST_EVSEL_HI(cfg)	((uint32_t)(((cfg) >> 24) & 0xF00U))
/* Full 12-bit EventSelect */
#define	EVTEST_EVSEL(cfg)	(EVTEST_EVSEL_LO(cfg) | EVTEST_EVSEL_HI(cfg))
/* UnitMask[7:0] from config word bits 15:8 */
#define	EVTEST_UMASK(cfg)	((uint32_t)(((cfg) >> 8) & 0xFFU))
/* USR bit (bit 16) */
#define	EVTEST_USR(cfg)		(((cfg) >> 16) & 1U)
/* OS bit (bit 17) */
#define	EVTEST_OS(cfg)		(((cfg) >> 17) & 1U)

/*
 * Core events and the EventCode/UMask the pmu-events tables in
 * lib/libpmc/pmu-events/arch/x86/amdzen* give for them.  umask 0 means
 * the table has no UMask for the event.  The tables differ between
 * generations (e.g. ls_mab_alloc.all_allocations is 0x7f on Zen 3/4,
 * 0x0f on Zen 5 and absent on Zen 1/2), so expectations are per table.
 */
struct evtest_event {
	const char	*name;
	uint32_t	event_code;
	uint32_t	umask;
};

/* amdzen1, amdzen2 */
static const struct evtest_event zen1_events[] = {
	{ "ex_ret_instr",			0xc0,	0x00 },
	{ "ls_not_halted_cyc",			0x76,	0x00 },
	{ "ex_ret_brn",				0xc2,	0x00 },
	{ "ex_ret_brn_misp",			0xc3,	0x00 },
	{ NULL,					0,	0 }
};

/* amdzen3 */
static const struct evtest_event zen3_events[] = {
	{ "ex_ret_instr",			0xc0,	0x00 },
	{ "ls_not_halted_cyc",			0x76,	0x00 },
	{ "ex_ret_brn",				0xc2,	0x00 },
	{ "ex_ret_brn_misp",			0xc3,	0x00 },
	{ "ls_mab_alloc.all_allocations",	0x41,	0x7f },
	{ "ex_ret_msprd_brnch_instr_dir_msmtch", 0x1c7,	0x00 },
	{ NULL,					0,	0 }
};

/* amdzen4 */
static const struct evtest_event zen4_events[] = {
	{ "ex_ret_instr",			0xc0,	0x00 },
	{ "ls_not_halted_cyc",			0x76,	0x00 },
	{ "ex_ret_brn",				0xc2,	0x00 },
	{ "ex_ret_brn_misp",			0xc3,	0x00 },
	{ "ls_mab_alloc.all_allocations",	0x41,	0x7f },
	{ "l2_request_g1.all",			0x60,	0xff },
	{ "ex_ret_msprd_brnch_instr_dir_msmtch", 0x1c7,	0x00 },
	{ NULL,					0,	0 }
};

/*
 * amdzen5 and amdzen6.  mapfile.csv sends some Zen 5 models to amdzen6
 * (D0h-D7h, and 00h-0Fh in older versions), so only events with the same
 * values in both tables are listed.
 */
static const struct evtest_event zen5_events[] = {
	{ "ex_ret_instr",			0xc0,	0x00 },
	{ "ls_not_halted_cyc",			0x76,	0x00 },
	{ "ex_ret_brn",				0xc2,	0x00 },
	{ "ex_ret_brn_misp",			0xc3,	0x00 },
	{ "l2_request_g1.all",			0x60,	0xf7 },
	{ NULL,					0,	0 }
};

static const struct evtest_event *
evtest_events_for(enum amd_zen zen)
{
	switch (zen) {
	case AMD_ZEN_1:
	case AMD_ZEN_PLUS:
	case AMD_ZEN_2:
		return (zen1_events);
	case AMD_ZEN_3:
	case AMD_ZEN_3_PLUS:
		return (zen3_events);
	case AMD_ZEN_4:
		return (zen4_events);
	case AMD_ZEN_5:
	case AMD_ZEN_6:
		return (zen5_events);
	default:
		return (NULL);
	}
}

/*
 * Skip unless this is a Zen CPU with hwpmc and pmu-events available, and
 * return the expected core events for it.
 */
static const struct evtest_event *
evtest_skip_unless_ready(struct amd_test_cpu *cpu)
{
	const struct evtest_event *events;

	amd_test_skip_unless_zen(cpu);
	amd_test_skip_unless_hwpmc();

	events = evtest_events_for(cpu->zen);
	if (events == NULL)
		atf_tc_skip("no core event expectations for %s",
		    amd_test_zen_name(cpu->zen));
	if (!pmc_pmu_enabled())
		atf_tc_skip("pmu-events not enabled");
	return (events);
}

/* ================================================================
 * UNIT TESTS: no hardware, no pmc_init()
 * ================================================================ */

ATF_TC(pmu_event_unit_evsel_hi_encoding);
ATF_TC_HEAD(pmu_event_unit_evsel_hi_encoding, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[UNIT] Verify AMD_PMC_TO_EVENTMASK splits a 12-bit EventSelect "
	    "into config word bits 7:0 and 35:32.");
}

ATF_TC_BODY(pmu_event_unit_evsel_hi_encoding, tc)
{
	uint64_t cfg;

	cfg = AMD_PMC_TO_EVENTMASK(0x123);
	ATF_CHECK_MSG(EVTEST_EVSEL_LO(cfg) == 0x23,
	    "EventSelect[7:0] of 0x123 is 0x%02x, expected 0x23",
	    EVTEST_EVSEL_LO(cfg));
	ATF_CHECK_MSG(EVTEST_EVSEL_HI(cfg) == 0x100,
	    "EventSelect[11:8] of 0x123 is 0x%03x, expected 0x100",
	    EVTEST_EVSEL_HI(cfg));
	ATF_CHECK_MSG((cfg & ~(0xffULL | 0xfULL << 32)) == 0,
	    "0x123 sets bits outside 7:0 and 35:32 (config=0x%016jx)",
	    (uintmax_t)cfg);
}

/* ================================================================
 * SMOKE TESTS: require AMD CPU, hwpmc loaded, pmu-events JSON
 * ================================================================ */

ATF_TC(pmu_event_smoke_pmu_enabled);
ATF_TC_HEAD(pmu_event_smoke_pmu_enabled, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[SMOKE] Verify pmc_pmu_enabled() returns true on Zen CPUs, "
	    "which all have a pmu-events table in libpmc.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_event_smoke_pmu_enabled, tc)
{
	struct amd_test_cpu cpu;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();

	ATF_CHECK_MSG(pmc_pmu_enabled(),
	    "pmc_pmu_enabled() is false on %s (family %02xh model %02xh)",
	    amd_test_zen_name(cpu.zen), cpu.family, cpu.model);
}

ATF_TC(pmu_event_smoke_lookup_by_name);
ATF_TC_HEAD(pmu_event_smoke_lookup_by_name, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[SMOKE] Verify that every expected core event name for this "
	    "Zen generation resolves via pmc_pmu_pmcallocate().");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_event_smoke_lookup_by_name, tc)
{
	const struct evtest_event *events, *ev;
	struct pmc_op_pmcallocate cfg;
	struct amd_test_cpu cpu;
	int error;

	events = evtest_skip_unless_ready(&cpu);

	for (ev = events; ev->name != NULL; ev++) {
		memset(&cfg, 0, sizeof(cfg));
		error = pmc_pmu_pmcallocate(ev->name, &cfg);
		ATF_CHECK_MSG(error == 0,
		    "%s: pmc_pmu_pmcallocate() failed on %s: %s",
		    ev->name, amd_test_zen_name(cpu.zen), strerror(error));
	}
}

ATF_TC(pmu_event_smoke_evsel_encoding);
ATF_TC_HEAD(pmu_event_smoke_evsel_encoding, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[SMOKE] Verify EventSelect bits 7:0 and 35:32 in the hwpmc "
	    "config word match the pmu-events table event codes.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_event_smoke_evsel_encoding, tc)
{
	const struct evtest_event *events, *ev;
	struct pmc_op_pmcallocate cfg;
	struct amd_test_cpu cpu;
	uint64_t raw;
	bool any_checked;

	events = evtest_skip_unless_ready(&cpu);

	any_checked = false;
	for (ev = events; ev->name != NULL; ev++) {
		memset(&cfg, 0, sizeof(cfg));
		if (pmc_pmu_pmcallocate(ev->name, &cfg) != 0)
			continue;

		raw = cfg.pm_md.pm_amd.pm_amd_config;
		any_checked = true;
		ATF_CHECK_MSG(EVTEST_EVSEL(raw) == ev->event_code,
		    "%s: EventSelect mismatch: got 0x%03x expected 0x%03x "
		    "(config=0x%016jx)",
		    ev->name, EVTEST_EVSEL(raw), ev->event_code,
		    (uintmax_t)raw);
	}

	if (!any_checked)
		atf_tc_fail("no expected core event resolved on %s",
		    amd_test_zen_name(cpu.zen));
}

ATF_TC(pmu_event_smoke_umask_encoding);
ATF_TC_HEAD(pmu_event_smoke_umask_encoding, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[SMOKE] Verify UnitMask bits 15:8 in the hwpmc config word "
	    "match the pmu-events table unit mask values.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_event_smoke_umask_encoding, tc)
{
	const struct evtest_event *events, *ev;
	struct pmc_op_pmcallocate cfg;
	struct amd_test_cpu cpu;
	uint64_t raw;
	bool any_checked;

	events = evtest_skip_unless_ready(&cpu);

	any_checked = false;
	for (ev = events; ev->name != NULL; ev++) {
		/* Events without a UMask in the table have nothing to check. */
		if (ev->umask == 0x00)
			continue;

		memset(&cfg, 0, sizeof(cfg));
		if (pmc_pmu_pmcallocate(ev->name, &cfg) != 0)
			continue;

		raw = cfg.pm_md.pm_amd.pm_amd_config;
		any_checked = true;
		ATF_CHECK_MSG(EVTEST_UMASK(raw) == ev->umask,
		    "%s: UnitMask mismatch: got 0x%02x expected 0x%02x "
		    "(config=0x%016jx)",
		    ev->name, EVTEST_UMASK(raw), ev->umask, (uintmax_t)raw);
	}

	if (!any_checked)
		atf_tc_skip("no expected core event with a UnitMask on %s",
		    amd_test_zen_name(cpu.zen));
}

ATF_TC(pmu_event_smoke_osuser_bits);
ATF_TC_HEAD(pmu_event_smoke_osuser_bits, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[SMOKE] Verify OsUser bits 17:16 (AMD_PMC_OS | AMD_PMC_USR) "
	    "are set in the default config word from pmc_pmu_pmcallocate(). "
	    "libpmc sets both bits when no explicit mode is requested.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_event_smoke_osuser_bits, tc)
{
	struct pmc_op_pmcallocate cfg;
	struct amd_test_cpu cpu;
	uint64_t raw;
	int error;

	(void)evtest_skip_unless_ready(&cpu);

	memset(&cfg, 0, sizeof(cfg));
	error = pmc_pmu_pmcallocate("ex_ret_instr", &cfg);
	ATF_REQUIRE_MSG(error == 0, "ex_ret_instr not resolved: %s",
	    strerror(error));

	raw = cfg.pm_md.pm_amd.pm_amd_config;
	ATF_CHECK_MSG(EVTEST_USR(raw) != 0,
	    "USR bit (bit 16) not set in default pmc_pmu_pmcallocate() config "
	    "(config=0x%016jx)", (uintmax_t)raw);
	ATF_CHECK_MSG(EVTEST_OS(raw) != 0,
	    "OS bit (bit 17) not set in default pmc_pmu_pmcallocate() config "
	    "(config=0x%016jx)", (uintmax_t)raw);
}

ATF_TC(pmu_event_smoke_host_guest_bits_default);
ATF_TC_HEAD(pmu_event_smoke_host_guest_bits_default, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[SMOKE] Verify HOST/GUEST bits 41:40 are NOT automatically set "
	    "in the default config word from pmc_pmu_pmcallocate(). "
	    "libpmc does not set these bits without an explicit request.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_event_smoke_host_guest_bits_default, tc)
{
	struct pmc_op_pmcallocate cfg;
	struct amd_test_cpu cpu;
	uint64_t raw;
	int error;

	(void)evtest_skip_unless_ready(&cpu);

	memset(&cfg, 0, sizeof(cfg));
	error = pmc_pmu_pmcallocate("ex_ret_instr", &cfg);
	ATF_REQUIRE_MSG(error == 0, "ex_ret_instr not resolved: %s",
	    strerror(error));

	raw = cfg.pm_md.pm_amd.pm_amd_config;
	ATF_CHECK_MSG((raw & AMD_PMC_HOST) == 0,
	    "HOST bit (bit 41) unexpectedly set in default config "
	    "(config=0x%016jx)", (uintmax_t)raw);
	ATF_CHECK_MSG((raw & AMD_PMC_GUEST) == 0,
	    "GUEST bit (bit 40) unexpectedly set in default config "
	    "(config=0x%016jx)", (uintmax_t)raw);
}

/* ================================================================
 * E2E TESTS: allocate, start, count, stop, verify counter moved
 * ================================================================ */

ATF_TC(pmu_event_e2e_counter_counts);
ATF_TC_HEAD(pmu_event_e2e_counter_counts, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[E2E] Bind to cpu0, count ex_ret_instr with a system-mode PMC "
	    "on cpu0 across a 1M-iteration loop, and verify at least 1M "
	    "retired instructions: confirms the event encoding reaches "
	    "the hardware.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_event_e2e_counter_counts, tc)
{
	struct amd_test_cpu cpu;
	pmc_value_t before, after;
	pmc_id_t pmcid;
	volatile uint64_t sink;
	uint64_t i;

	(void)evtest_skip_unless_ready(&cpu);
	amd_test_bind_cpu0();

	/* hwpmc reports a full CPU as EINVAL, like a rejected encoding. */
	if (amd_test_count_core_rows(0, true) == 0)
		atf_tc_skip("no free core counter on cpu0");
	pmcid = PMC_ID_INVALID;
	if (pmc_allocate("ex_ret_instr", PMC_MODE_SC, 0, 0, &pmcid, 1) != 0)
		atf_tc_fail("pmc_allocate(ex_ret_instr, SC) failed: %s",
		    strerror(errno));

	if (pmc_start(pmcid) != 0) {
		amd_test_release_pmc(pmcid);
		atf_tc_fail("pmc_start() failed: %s", strerror(errno));
	}

	if (pmc_read(pmcid, &before) != 0) {
		(void)pmc_stop(pmcid);
		amd_test_release_pmc(pmcid);
		atf_tc_fail("pmc_read(before) failed: %s", strerror(errno));
	}

	sink = 0;
	for (i = 0; i < 1000000UL; i++)
		sink += i * 3 + 7;

	if (pmc_stop(pmcid) != 0) {
		amd_test_release_pmc(pmcid);
		atf_tc_fail("pmc_stop() failed: %s", strerror(errno));
	}

	if (pmc_read(pmcid, &after) != 0) {
		amd_test_release_pmc(pmcid);
		atf_tc_fail("pmc_read(after) failed: %s", strerror(errno));
	}

	amd_test_release_pmc(pmcid);

	ATF_CHECK_MSG(after >= before && after - before >= 1000000,
	    "ex_ret_instr counted fewer than 1M instructions over a "
	    "1M-iteration loop: before=%ju after=%ju",
	    (uintmax_t)before, (uintmax_t)after);
}

/* ================================================================
 * Test program entry
 * ================================================================ */

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pmu_event_unit_evsel_hi_encoding);

	/* Smoke tests: require AMD + hwpmc + pmu-events */
	ATF_TP_ADD_TC(tp, pmu_event_smoke_pmu_enabled);
	ATF_TP_ADD_TC(tp, pmu_event_smoke_lookup_by_name);
	ATF_TP_ADD_TC(tp, pmu_event_smoke_evsel_encoding);
	ATF_TP_ADD_TC(tp, pmu_event_smoke_umask_encoding);
	ATF_TP_ADD_TC(tp, pmu_event_smoke_osuser_bits);
	ATF_TP_ADD_TC(tp, pmu_event_smoke_host_guest_bits_default);

	/* E2E tests: allocate counter, count instructions, verify */
	ATF_TP_ADD_TC(tp, pmu_event_e2e_counter_counts);

	return (atf_no_error());
}
