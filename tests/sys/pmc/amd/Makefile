TESTSDIR=	${TESTSBASE}/sys/pmc/amd

ATF_TESTS_C+=	pmc_ibs_fetch_op_test
ATF_TESTS_C+=	pmc_ibs_test
ATF_TESTS_C+=	pmc_rowalloc_test
ATF_TESTS_C+=	pmc_start_msr_test
ATF_TESTS_C+=	pmu_detect_test
ATF_TESTS_C+=	pmu_event_neg_test
ATF_TESTS_C+=	pmu_event_test
ATF_TESTS_C+=	pmu_mux_test
ATF_TESTS_C+=	pmu_perfmonv2_test
ATF_TESTS_C+=	rapl_domains_test
ATF_TESTS_SH+=	pmc_frontend_test
ATF_TESTS_SH+=	pmcstat_ibs_test

# These need the counters of cpu0 to themselves.
TEST_METADATA.pmc_ibs_fetch_op_test+=	is_exclusive="true"
TEST_METADATA.pmc_ibs_test+=		is_exclusive="true"
TEST_METADATA.pmc_rowalloc_test+=	is_exclusive="true"
TEST_METADATA.pmc_start_msr_test+=	is_exclusive="true"
TEST_METADATA.pmu_event_neg_test+=	is_exclusive="true"
TEST_METADATA.pmu_event_test+=		is_exclusive="true"
TEST_METADATA.pmu_mux_test+=		is_exclusive="true"
TEST_METADATA.pmu_perfmonv2_test+=	is_exclusive="true"
TEST_METADATA.pmcstat_ibs_test+=	is_exclusive="true"
TEST_METADATA.rapl_domains_test+=	is_exclusive="true"

LIBADD+=	pmc

.include <bsd.test.mk>
