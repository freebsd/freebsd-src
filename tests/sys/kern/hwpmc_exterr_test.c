/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 */

#include <sys/param.h>
#include <sys/exterrvar.h>
#include <sys/module.h>
#include <sys/pmc.h>

#include <dev/hwpmc/hwpmc_amd.h>
#include <dev/hwpmc/hwpmc_ibs.h>

#include <atf-c.h>
#include <errno.h>
#include <exterr.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int
hwpmc_syscall_num(void)
{
	struct module_stat ms;
	int modid;

	modid = modfind(PMC_MODULE_NAME);
	if (modid < 0)
		return (-1);

	ms.version = sizeof(ms);
	if (modstat(modid, &ms) < 0)
		return (-1);

	return (ms.data.intval);
}

static int
hwpmc_call(int op, void *arg)
{
	int sc;

	sc = hwpmc_syscall_num();
	if (sc < 0)
		return (-1);

	return (syscall(sc, op, arg));
}

static void
clear_exterr(void)
{
	char exterr[UEXTERROR_MAXLEN];

	ATF_REQUIRE_ERRNO(EINVAL, exterrctl(EXTERRCTL_UD, 0, NULL) == -1);
	ATF_REQUIRE_EQ(0, uexterr_gettext(exterr, sizeof(exterr)));
	ATF_REQUIRE_STREQ("", exterr);
}

static void
require_exterr(const char *needle)
{
	char exterr[UEXTERROR_MAXLEN];

	ATF_REQUIRE_EQ(0, uexterr_gettext(exterr, sizeof(exterr)));
	ATF_REQUIRE_MSG(strstr(exterr, needle) != NULL,
	    "extended error \"%s\" does not contain \"%s\"", exterr, needle);
}

static void
require_hwpmc(void)
{

	if (pmc_init() == 0)
		return;

	atf_tc_skip("hwpmc is unavailable: %s", strerror(errno));
}

static bool
have_class(enum pmc_class class)
{
	const struct pmc_cpuinfo *pci;
	uint32_t i;

	ATF_REQUIRE_EQ(0, pmc_cpuinfo(&pci));
	for (i = 0; i < pci->pm_nclass; i++) {
		if (pci->pm_classes[i].pm_class == class)
			return (true);
	}
	return (false);
}

static void
require_class(enum pmc_class class, const char *name)
{

	if (have_class(class))
		return;

	atf_tc_skip("%s PMCs are unavailable on this system", name);
}

static void
allocate_soft_pmc(enum pmc_mode mode, int cpu, pmc_id_t *pmcid)
{

	ATF_REQUIRE_EQ(0, pmc_allocate("SOFT-CLOCK.HARD", mode, 0, cpu, pmcid,
	    1));
}

ATF_TC_WITHOUT_HEAD(pmcallocate_invalid_mode);
ATF_TC_BODY(pmcallocate_invalid_mode, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_class = PMC_CLASS_SOFT;
	pa.pm_mode = (enum pmc_mode)0xff;
	pa.pm_cpu = PMC_CPU_ANY;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("Invalid PMC mode");
}

ATF_TC_WITHOUT_HEAD(pmcallocate_invalid_cpu);
ATF_TC_BODY(pmcallocate_invalid_cpu, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_class = PMC_CLASS_SOFT;
	pa.pm_mode = PMC_MODE_TC;
	pa.pm_cpu = (uint32_t)pmc_ncpu();

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("Invalid PMC CPU");
}

ATF_TC_WITHOUT_HEAD(pmcallocate_invalid_flags);
ATF_TC_BODY(pmcallocate_invalid_flags, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_class = PMC_CLASS_SOFT;
	pa.pm_mode = PMC_MODE_TC;
	pa.pm_cpu = PMC_CPU_ANY;
	pa.pm_flags = 1U << 31;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("Invalid PMC flags");
}

ATF_TC_WITHOUT_HEAD(pmcattach_system_mode);
ATF_TC_BODY(pmcattach_system_mode, tc)
{
	struct pmc_op_pmcattach pa;
	pmc_id_t pmcid;

	require_hwpmc();
	clear_exterr();
	allocate_soft_pmc(PMC_MODE_SC, 0, &pmcid);

	memset(&pa, 0, sizeof(pa));
	pa.pm_pmc = pmcid;
	pa.pm_pid = getpid();

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCATTACH, &pa) == -1);
	require_exterr("Cannot attach a system-mode PMC");

	ATF_REQUIRE_EQ(0, pmc_release(pmcid));
}

ATF_TC_WITHOUT_HEAD(pmcattach_running_pmc);
ATF_TC_BODY(pmcattach_running_pmc, tc)
{
	struct pmc_op_pmcattach pa;
	pmc_id_t pmcid;

	require_hwpmc();
	clear_exterr();
	allocate_soft_pmc(PMC_MODE_TC, PMC_CPU_ANY, &pmcid);
	ATF_REQUIRE_EQ(0, pmc_start(pmcid));

	memset(&pa, 0, sizeof(pa));
	pa.pm_pmc = pmcid;
	pa.pm_pid = getpid();

	ATF_REQUIRE_ERRNO(EBUSY, hwpmc_call(PMC_OP_PMCATTACH, &pa) == -1);
	require_exterr("PMC must be stopped before attach");

	ATF_REQUIRE_EQ(0, pmc_stop(pmcid));
	ATF_REQUIRE_EQ(0, pmc_release(pmcid));
}

ATF_TC_WITHOUT_HEAD(pmcrw_no_flags);
ATF_TC_BODY(pmcrw_no_flags, tc)
{
	struct pmc_op_pmcrw prw;
	pmc_id_t pmcid;

	require_hwpmc();
	clear_exterr();
	allocate_soft_pmc(PMC_MODE_TC, PMC_CPU_ANY, &pmcid);

	memset(&prw, 0, sizeof(prw));
	prw.pm_pmcid = pmcid;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCRW, &prw) == -1);
	require_exterr("PMCRW requires OLDVALUE");

	ATF_REQUIRE_EQ(0, pmc_release(pmcid));
}

ATF_TC_WITHOUT_HEAD(pmcrw_write_running);
ATF_TC_BODY(pmcrw_write_running, tc)
{
	struct pmc_op_pmcrw prw;
	pmc_id_t pmcid;

	require_hwpmc();
	clear_exterr();
	allocate_soft_pmc(PMC_MODE_TC, PMC_CPU_ANY, &pmcid);
	ATF_REQUIRE_EQ(0, pmc_start(pmcid));

	memset(&prw, 0, sizeof(prw));
	prw.pm_flags = PMC_F_NEWVALUE;
	prw.pm_pmcid = pmcid;
	prw.pm_value = 1;

	ATF_REQUIRE_ERRNO(EBUSY, hwpmc_call(PMC_OP_PMCRW, &prw) == -1);
	require_exterr("Cannot write a PMC while it is running");

	ATF_REQUIRE_EQ(0, pmc_stop(pmcid));
	ATF_REQUIRE_EQ(0, pmc_release(pmcid));
}

ATF_TC_WITHOUT_HEAD(amd_missing_pmu_flag);
ATF_TC_BODY(amd_missing_pmu_flag, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	require_class(PMC_CLASS_K8, "AMD");
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_class = PMC_CLASS_K8;
	pa.pm_mode = PMC_MODE_TC;
	pa.pm_cpu = PMC_CPU_ANY;
	pa.pm_md.pm_amd.pm_amd_sub_class = PMC_AMD_SUB_CLASS_CORE;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("AMD PMCs require PMC_F_EV_PMU");
}

ATF_TC_WITHOUT_HEAD(amd_invalid_subclass);
ATF_TC_BODY(amd_invalid_subclass, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	require_class(PMC_CLASS_K8, "AMD");
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_class = PMC_CLASS_K8;
	pa.pm_mode = PMC_MODE_TC;
	pa.pm_cpu = PMC_CPU_ANY;
	pa.pm_flags = PMC_F_EV_PMU;
	pa.pm_md.pm_amd.pm_amd_sub_class = UINT32_MAX;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("AMD subclass");
}

ATF_TC_WITHOUT_HEAD(amd_invalid_config_bits);
ATF_TC_BODY(amd_invalid_config_bits, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	require_class(PMC_CLASS_K8, "AMD");
	if (!pmc_pmu_enabled())
		atf_tc_skip("AMD PMU raw allocation path is unavailable");
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_class = PMC_CLASS_K8;
	pa.pm_mode = PMC_MODE_TC;
	pa.pm_cpu = PMC_CPU_ANY;
	pa.pm_flags = PMC_F_EV_PMU;
	pa.pm_md.pm_amd.pm_amd_sub_class = PMC_AMD_SUB_CLASS_CORE;
	pa.pm_md.pm_amd.pm_amd_config = ~0ULL;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("AMD PMU config has unsupported bits");
}

ATF_TC_WITHOUT_HEAD(ibs_missing_system_capability);
ATF_TC_BODY(ibs_missing_system_capability, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	require_class(PMC_CLASS_IBS, "IBS");
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_class = PMC_CLASS_IBS;
	pa.pm_mode = PMC_MODE_SS;
	pa.pm_cpu = 0;
	pa.pm_md.pm_ibs.ibs_type = IBS_PMC_FETCH;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("IBS requires SYSTEM capability");
}

ATF_TC_WITHOUT_HEAD(ibs_invalid_type);
ATF_TC_BODY(ibs_invalid_type, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	require_class(PMC_CLASS_IBS, "IBS");
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_caps = PMC_CAP_SYSTEM;
	pa.pm_class = PMC_CLASS_IBS;
	pa.pm_mode = PMC_MODE_SS;
	pa.pm_cpu = 0;
	pa.pm_md.pm_ibs.ibs_type = UINT32_MAX;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("IBS type");
}

ATF_TC_WITHOUT_HEAD(ibs_invalid_config_bits);
ATF_TC_BODY(ibs_invalid_config_bits, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	require_class(PMC_CLASS_IBS, "IBS");
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_caps = PMC_CAP_SYSTEM;
	pa.pm_class = PMC_CLASS_IBS;
	pa.pm_mode = PMC_MODE_SS;
	pa.pm_cpu = 0;
	pa.pm_md.pm_ibs.ibs_type = IBS_PMC_FETCH;
	pa.pm_md.pm_ibs.ibs_ctl = ~0ULL;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("Unsupported IBS fetch control bits");
}

ATF_TC_WITHOUT_HEAD(ibs_nonsampling_mode);
ATF_TC_BODY(ibs_nonsampling_mode, tc)
{
	struct pmc_op_pmcallocate pa;

	require_hwpmc();
	require_class(PMC_CLASS_IBS, "IBS");
	clear_exterr();

	memset(&pa, 0, sizeof(pa));
	pa.pm_caps = PMC_CAP_SYSTEM;
	pa.pm_class = PMC_CLASS_IBS;
	pa.pm_mode = PMC_MODE_SC;
	pa.pm_cpu = 0;
	pa.pm_md.pm_ibs.ibs_type = IBS_PMC_FETCH;

	ATF_REQUIRE_ERRNO(EINVAL, hwpmc_call(PMC_OP_PMCALLOCATE, &pa) == -1);
	require_exterr("IBS only supports sampling mode");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pmcallocate_invalid_mode);
	ATF_TP_ADD_TC(tp, pmcallocate_invalid_cpu);
	ATF_TP_ADD_TC(tp, pmcallocate_invalid_flags);
	ATF_TP_ADD_TC(tp, pmcattach_system_mode);
	ATF_TP_ADD_TC(tp, pmcattach_running_pmc);
	ATF_TP_ADD_TC(tp, pmcrw_no_flags);
	ATF_TP_ADD_TC(tp, pmcrw_write_running);
	ATF_TP_ADD_TC(tp, amd_missing_pmu_flag);
	ATF_TP_ADD_TC(tp, amd_invalid_subclass);
	ATF_TP_ADD_TC(tp, amd_invalid_config_bits);
	ATF_TP_ADD_TC(tp, ibs_missing_system_capability);
	ATF_TP_ADD_TC(tp, ibs_invalid_type);
	ATF_TP_ADD_TC(tp, ibs_invalid_config_bits);
	ATF_TP_ADD_TC(tp, ibs_nonsampling_mode);

	return (atf_no_error());
}
