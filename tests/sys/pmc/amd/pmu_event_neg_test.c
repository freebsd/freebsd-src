/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * These tests issue PMC_OP_PMCALLOCATE directly, as libpmc does, with
 * config words that libpmc would never build, to exercise the config
 * validation in hwpmc_amd.c.
 *
 * Kernel validation path, amd_can_assign_pmc():
 *	config = a->pm_md.pm_amd.pm_amd_config;
 *	if ((config & ~amd_config_mask(subclass, caps)) != 0)
 *		return (EINVAL);
 *
 * For the CORE sub-class amd_config_mask() is AMD_VALID_BITS (CounterMask,
 * Invert, Enable, Int, PC, Edge, OS, USR, UnitMask, EventMask) on every
 * family, plus PRECISERETIRE (bit 43) only with PMC_CAP_PRECISE, plus the
 * kern.hwpmc.amd_core_extra_mask tunable.  HOST (bit 41), GUEST (bit 40)
 * and bits 44 and up must therefore be rejected.
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/pmc.h>
#include <sys/sysctl.h>

#include <dev/hwpmc/hwpmc_amd.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "amd_pmc_test_common.h"

/* Find the hwpmc syscall number the same way pmc_init() does. */
static int
neg_get_pmc_syscall(void)
{
	struct module_stat ms;
	int mod_id;

	mod_id = modfind(PMC_MODULE_NAME);
	if (mod_id < 0)
		return (-1);

	ms.version = sizeof(ms);
	if (modstat(mod_id, &ms) < 0)
		return (-1);

	return (ms.data.intval);
}

static int
neg_raw_pmcallocate(int pmc_sc, struct pmc_op_pmcallocate *op)
{
	return (syscall(pmc_sc, PMC_OP_PMCALLOCATE, op));
}

static void
neg_raw_pmcrelease(int pmc_sc, pmc_id_t pmcid)
{
	struct pmc_op_simple op;

	memset(&op, 0, sizeof(op));
	op.pm_pmcid = pmcid;
	(void)syscall(pmc_sc, PMC_OP_PMCRELEASE, &op);
}

/*
 * Skip unless the raw allocation can be tested, and return the syscall
 * number together with a valid system-mode request for ex_ret_instr on
 * cpu0 (the event is in every amdzen* table).
 */
static void
neg_setup(int *sc, struct pmc_op_pmcallocate *op)
{
	struct amd_test_cpu cpu;
	uint64_t extra;
	size_t len;
	int error;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();

	len = sizeof(extra);
	if (sysctlbyname("kern.hwpmc.amd_core_extra_mask", &extra, &len,
	    NULL, 0) == 0 && extra != 0)
		atf_tc_skip("kern.hwpmc.amd_core_extra_mask=%#jx widens the "
		    "accepted CORE bits", (uintmax_t)extra);
	if (!pmc_pmu_enabled())
		atf_tc_skip("pmu-events not enabled");

	*sc = neg_get_pmc_syscall();
	if (*sc < 0)
		atf_tc_skip("cannot find the hwpmc syscall: %s",
		    strerror(errno));

	memset(op, 0, sizeof(*op));
	error = pmc_pmu_pmcallocate("ex_ret_instr", op);
	if (error != 0)
		atf_tc_skip("ex_ret_instr not resolvable: %s",
		    strerror(error));
	op->pm_mode = PMC_MODE_SC;
	op->pm_flags = PMC_F_EV_PMU;
	op->pm_cpu = 0;
	op->pm_pmcid = PMC_ID_INVALID;
}

/*
 * Check that the request with bits added to its config word is refused
 * with EINVAL.  hwpmc also answers EINVAL when no row is free, so the
 * unmodified request must be accepted first.
 */
static void
neg_expect_rejected(uint64_t bits, const char *what)
{
	struct pmc_op_pmcallocate op, valid;
	int sc;

	neg_setup(&sc, &valid);

	op = valid;
	if (neg_raw_pmcallocate(sc, &op) != 0)
		atf_tc_skip("the unmodified request is refused: %s",
		    strerror(errno));
	neg_raw_pmcrelease(sc, op.pm_pmcid);

	op = valid;
	op.pm_md.pm_amd.pm_amd_config |= bits;
	if (neg_raw_pmcallocate(sc, &op) == 0) {
		neg_raw_pmcrelease(sc, op.pm_pmcid);
		atf_tc_fail("config word with %s (0x%016jx) was accepted",
		    what, (uintmax_t)op.pm_md.pm_amd.pm_amd_config);
	}
	ATF_CHECK_MSG(errno == EINVAL,
	    "config word with %s: expected EINVAL, got %d (%s)",
	    what, errno, strerror(errno));
}

ATF_TC(pmu_neg_valid_config_accepted);
ATF_TC_HEAD(pmu_neg_valid_config_accepted, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[NEG/SANITY] The unmodified ex_ret_instr request is accepted by "
	    "the raw PMC_OP_PMCALLOCATE path used by the rejection tests.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_neg_valid_config_accepted, tc)
{
	struct pmc_op_pmcallocate op;
	int sc;

	neg_setup(&sc, &op);
	if (amd_test_count_core_rows(0, true) == 0)
		atf_tc_skip("no free core counter on cpu0");

	ATF_REQUIRE_MSG(neg_raw_pmcallocate(sc, &op) == 0,
	    "valid config word 0x%016jx was refused: %s",
	    (uintmax_t)op.pm_md.pm_amd.pm_amd_config, strerror(errno));
	neg_raw_pmcrelease(sc, op.pm_pmcid);
}

ATF_TC(pmu_neg_host_bit_rejected);
ATF_TC_HEAD(pmu_neg_host_bit_rejected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[NEG] A CORE config word with HOST (bit 41) set is refused "
	    "with EINVAL.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_neg_host_bit_rejected, tc)
{
	neg_expect_rejected(AMD_PMC_HOST, "HOST (bit 41)");
}

ATF_TC(pmu_neg_guest_bit_rejected);
ATF_TC_HEAD(pmu_neg_guest_bit_rejected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[NEG] A CORE config word with GUEST (bit 40) set is refused "
	    "with EINVAL.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_neg_guest_bit_rejected, tc)
{
	neg_expect_rejected(AMD_PMC_GUEST, "GUEST (bit 40)");
}

ATF_TC(pmu_neg_host_and_guest_bits_rejected);
ATF_TC_HEAD(pmu_neg_host_and_guest_bits_rejected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[NEG] A CORE config word with both HOST and GUEST set is "
	    "refused with EINVAL.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_neg_host_and_guest_bits_rejected, tc)
{
	neg_expect_rejected(AMD_PMC_HOST | AMD_PMC_GUEST, "HOST|GUEST");
}

ATF_TC(pmu_neg_reserved_bits_rejected);
ATF_TC_HEAD(pmu_neg_reserved_bits_rejected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[NEG] A CORE config word with reserved bit 44 set is refused "
	    "with EINVAL.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_neg_reserved_bits_rejected, tc)
{
	neg_expect_rejected(1ULL << 44, "reserved bit 44");
}

ATF_TC(pmu_neg_preciseretire_without_cap_rejected);
ATF_TC_HEAD(pmu_neg_preciseretire_without_cap_rejected, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "[NEG] A CORE config word with PRECISERETIRE (bit 43) set is "
	    "refused with EINVAL when PMC_CAP_PRECISE is not requested.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_neg_preciseretire_without_cap_rejected, tc)
{
	/* libpmc requests only PMC_CAP_READ | PMC_CAP_WRITE. */
	neg_expect_rejected(AMD_PMC_PRECISERETIRE,
	    "PRECISERETIRE (bit 43) without PMC_CAP_PRECISE");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pmu_neg_valid_config_accepted);
	ATF_TP_ADD_TC(tp, pmu_neg_host_bit_rejected);
	ATF_TP_ADD_TC(tp, pmu_neg_guest_bit_rejected);
	ATF_TP_ADD_TC(tp, pmu_neg_host_and_guest_bits_rejected);
	ATF_TP_ADD_TC(tp, pmu_neg_reserved_bits_rejected);
	ATF_TP_ADD_TC(tp, pmu_neg_preciseretire_without_cap_rejected);

	return (atf_no_error());
}
